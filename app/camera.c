#include <app/camera.h>
#include <block_device.h>
#include <ili9341.h>
#include <event.h>
#include <list.h>
#include <list_fifo.h>
#include <errcode.h>
#include <os_api.h>
#include <kmalloc.h>
#include <vc/service_mmal.h>
#include <vc/service_mmal_param.h>
#include <vc/service_mmal_encoding.h>
#include <write_stream_buffer.h>
#include <memory_map.h>

#define MODULE_LOG_LEVEL camera_log_level
#define MODULE_UNIT_TAG "cam"
#include <module_common.h>

#define MAX_SUPPORTED_ENCODINGS 20
#define IO_MIN_SECTORS 8192
#define H264BUF_SIZE (IO_MIN_SECTORS * 512)

enum {
  CAM_PORT_PREVIEW = 0,
  CAM_PORT_VIDEO,
  CAM_PORT_CAPTURE,
  CAM_PORT_COUNT
};

struct camera_component_splitter {
  struct mmal_port *in;
  struct mmal_port *out0;
  struct mmal_port *out1;
};

struct camera_component_camera {
  struct mmal_port *video;
  struct mmal_port *still;
  struct mmal_port *preview;
};

struct camera_component_resizer {
  struct mmal_port *in;
  struct mmal_port *out;
};

struct camera_component_encoder {
  struct mmal_port *in;
  struct mmal_port *out;
};

struct camera {
  struct block_device *bdev;

  struct camera_component_camera camera;
  struct camera_component_resizer resizer;
  struct camera_component_encoder encoder;
  struct camera_component_splitter splitter;
  struct mmal_port *port_preview_stream;
  struct mmal_port *port_h264_stream;

  int stat_frames;
};

struct encoder_h264_params {
  uint32_t q_initial;
  uint32_t q_min;
  uint32_t q_max;
  uint32_t intraperiod;
  uint32_t bitrate;
  enum mmal_video_profile profile;
  enum mmal_video_level level;
};

static struct camera cam = { 0 };

static inline struct mmal_buffer *mmal_buf_fifo_pop(struct list_head *l)
{
  struct list_head *node;
  node = list_fifo_pop_head(l);
  return node ? list_entry(node, struct mmal_buffer, list) : NULL;
}

static OPTIMIZED int camera_on_preview_buffer_ready(struct mmal_buffer *b)
{
  int err;
  int irqflags;
  bool should_draw;
  struct mmal_buffer *b2;
  struct mmal_port *p = cam.port_preview_stream;
  b2 = mmal_buf_fifo_pop(&p->bufs.os_side_consumable);

  if (b != b2)
    return ERR_INVAL;

  if (!(b->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END))
    return SUCCESS;

  should_draw = false;

  disable_irq_save_flags(irqflags);
  while(1) {
    list_del(&b->list);
    if (list_empty(&p->bufs.os_side_consumable))
      break;

    list_add_tail(&b->list, &p->bufs.os_side_free);
    b = list_first_entry(&p->bufs.os_side_consumable,
      struct mmal_buffer, list);
  }
  restore_irq_flags(irqflags);

  disable_irq_save_flags(irqflags);
  should_draw = list_empty(&p->bufs.os_side_in_process);
  if (should_draw)
    list_add_tail(&b->list, &p->bufs.os_side_in_process);

  restore_irq_flags(irqflags);

  if (!should_draw)
    return SUCCESS;

  err = ili9341_draw_dma_buf(b->user_handle);
  return err;
}

static int OPTIMIZED camera_on_h264_buffer_ready(void)
{
  int irqflags;
  int err;
  struct mmal_buffer *b;
  struct list_head iobufs = LIST_HEAD_INIT(iobufs);
  struct list_head *node;
  struct write_stream_buf *iobuf;
  struct write_stream_buf *iobuf_tmp;
  struct mmal_port *p = cam.port_h264_stream;

  b = mmal_buf_fifo_pop(&p->bufs.os_side_consumable);
  node = list_fifo_pop_head(&p->iobufs);

  MODULE_ASSERT(b, "Unexpected buffer not found");
  MODULE_ASSERT(b->buffer, "buffer ptr is NULL");
  MODULE_ASSERT(node, "Not enough io bufs to continue");

  iobuf = list_entry(node, struct write_stream_buf, list);
  iobuf->paddr = NARROW_PTR(b->buffer);
  iobuf->size = ALIGN_UP_4(b->length);
  iobuf->io_offset = 0;
  iobuf->priv = b;
  list_fifo_push_tail(&iobufs, &iobuf->list);

  err = cam.bdev->ops.write_stream_push(cam.bdev, &iobufs);
  if (err) {
    MODULE_ERR("failed to write to sd card stream");
  }

  /* iobufs after steam_push hold list of released buffers */
  list_for_each_entry_safe(iobuf, iobuf_tmp, &iobufs, list) {
    disable_irq_save_flags(irqflags);
    list_del_init(&iobuf->list);
    list_fifo_push_tail_locked(&p->iobufs, &iobuf->list);
    b = iobuf->priv;
    list_del(&b->list);
    list_add_tail(&b->list, &p->bufs.os_side_free);
    restore_irq_flags(irqflags);
  }

#if 0
  if (b->flags & MMAL_BUFFER_HEADER_FLAG_KEYFRAME) {}
  if (b->flags & MMAL_BUFFER_HEADER_FLAG_CONFIG) {}
#endif
  
  if (!(cam.stat_frames % 25))
    os_log(".%d\r\n", cam.stat_frames);
  cam.stat_frames++;
  err = mmal_port_buffer_submit_list(p, &p->bufs.os_side_free);
  if (err != SUCCESS) {
    MODULE_ERR("failed to submit buffer list, err: %d", err);
  }
  return SUCCESS;
}

static int OPTIMIZED camera_on_mmal_buffer_ready(struct mmal_port *p,
  struct mmal_buffer *b)
{
  if (p == cam.port_h264_stream) {
    return camera_on_h264_buffer_ready();
  } if (p == cam.port_preview_stream) {
    return camera_on_preview_buffer_ready(b);
  }
  return ERR_INVAL;
}

static int mmal_set_camera_parameters(struct mmal_component *c,
  struct mmal_param_cam_info_cam *cam_info, int width, int height)
{
  int ret;
  uint32_t config_size;
  struct mmal_param_cam_config config = {
    .max_stills_w = width,
    .max_stills_h = height,
    .stills_yuv422 = 0,
    .one_shot_stills = 0,
    .max_preview_video_w = width,
    .max_preview_video_h = height,
    .num_preview_video_frames = 3,
    .stills_capture_circular_buffer_height = 0,
    .fast_preview_resume = 0,
    .use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RAW_STC
  };

  struct mmal_param_cam_config new_config = { 0 };

  config_size = sizeof(new_config);

  ret = mmal_port_parameter_set(&c->control,
    MMAL_PARAM_CAMERA_CONFIG, &config, sizeof(config));

  config_size = sizeof(new_config);
  ret = mmal_port_parameter_get(&c->control,
    MMAL_PARAM_CAMERA_CONFIG, &new_config, &config_size);
  return ret;
}

#if 0
static int mmal_apply_display_buffers(struct mmal_port *p,
  struct ili9341_buffer_info *buffers, size_t num_buffers)
{
  size_t i;
  int err;

  for (i = 0; i < num_buffers; ++i) {
    err = mmal_port_add_buffer(p, buffers[i].buffer,
      buffers[i].buffer_size, buffers[i].handle);
    CHECK_ERR("Failed to import display buffer #%d", i);
    MODULE_INFO("mmal_apply_display_buffers: port:%s, ptr:%08x, size:%d"
      " handle:%d, port_enabled:%s",
      p->name,
      buffers[i].buffer,
      buffers[i].buffer_size, buffers[i].handle,
      p->enabled ? "yes" : "no");
  }

  err = mmal_port_buffer_send_all(p);
  CHECK_ERR("Failed to send buffers to port");
out_err:
  return err;
}
#endif


static void OPTIMIZED camera_on_preview_data_consumed_isr(uint32_t buf_handle)
{
  struct mmal_port *p = cam.port_preview_stream;
  struct mmal_buffer *b = NULL, *tmp;

  list_for_each_entry_safe(b, tmp, &p->bufs.os_side_in_process, list) {
    if (buf_handle == b->user_handle) {
      list_move_tail(&b->list, &p->bufs.os_side_free);
      mmal_port_buffer_consumed_isr(p, b);
      return;
    }
  }

  MODULE_ERR("Failed to find display buffer\r\n");
}

static inline void camera_display_buffers_dump(
  const struct ili9341_buffer_info *b,
  size_t num_buffers)
{
  for (size_t i = 0; i < num_buffers; ++i)
    MODULE_INFO("display buffer: %08x(size %ld),handle:%d", b[i].buffer,
      b[i].buffer_size,
      b[i].handle);
}

static inline int camera_cam_info_request(struct mmal_component *c,
  struct mmal_param_cam_info *cam_info)
{
  int err;
  uint32_t param_size;

  param_size = sizeof(*cam_info);
  err = mmal_port_parameter_get(&c->control, MMAL_PARAM_CAM_INFO, cam_info,
    &param_size);
  return err;
}

static void camera_cam_info_dump(const struct mmal_param_cam_info *cam_info)
{
  int i;
  const struct mmal_param_cam_info_cam *c;

  for (i = 0; i < cam_info->num_cameras; ++i) {
    c = &cam_info->cameras[i];
    MODULE_INFO("cam %d: name:%s, W x H: %dx%d\r\n",
      i, c->camera_name, c->max_width, c->max_height);
  }
}

static int camera_get_camera_info(struct mmal_param_cam_info *cam_info)
{
  int err;
  struct mmal_component *c;
  struct mmal_param_logging l = { .set = 0x1, .clear = 0 };

  c = mmal_component_create("camera_info");

  err = mmal_port_parameter_set(&c->control, MMAL_PARAM_LOGGING, &l,
    sizeof(l));

  err = camera_cam_info_request(c, cam_info);
  CHECK_ERR("Failed to get camera info");

  camera_cam_info_dump(cam_info);
  err = mmal_component_disable(c);
  CHECK_ERR("Failed to disable 'camera info' component");

  err = mmal_component_destroy(c);
  CHECK_ERR("Failed to destroy 'camera info' component");
  return SUCCESS;

out_err:
  return err;
}

static int camera_encoder_params_dump(struct mmal_port *e)
{
  int err = SUCCESS;

  uint32_t v32;
  uint32_t param_size;

#define MMAL_PARAM_LOG32(__p) \
  param_size = sizeof(v32); \
  err = mmal_port_parameter_get(e, MMAL_PARAM_ ## __p, &v32, \
    &param_size); \
    CHECK_ERR("Failed to get param MMAL_PARAM_ "#__p); \
  MODULE_INFO(#__p" (%d): %d", param_size, v32)

  MMAL_PARAM_LOG32(VIDEO_ENCODE_INITIAL_QUANT);
  MMAL_PARAM_LOG32(VIDEO_ENCODE_MIN_QUANT);
  MMAL_PARAM_LOG32(VIDEO_ENCODE_MAX_QUANT);
  MMAL_PARAM_LOG32(INTRAPERIOD);
  MMAL_PARAM_LOG32(VIDEO_BIT_RATE);
  MMAL_PARAM_LOG32(VIDEO_ENCODE_INLINE_HEADER);
out_err:
  return err;
}

static int camera_encoder_params_set(struct mmal_port *e,
  const struct encoder_h264_params *p)
{
  int err = SUCCESS;
  uint32_t v32;

  const struct mmal_param_video_profile video_profile = {
    .profile = p->profile,
    .level = p->level
  };

#define MMAL_PARAM_SET32(__p, __v) \
  v32 = __v; \
  err = mmal_port_parameter_set(e, MMAL_PARAM_ ## __p, &v32, \
    sizeof(v32)); \
    CHECK_ERR("Failed to set param MMAL_PARAM_ "#__p); \
  MODULE_INFO(#__p" set to %d", v32)

  MMAL_PARAM_SET32(VIDEO_ENCODE_INITIAL_QUANT, p->q_initial);
  MMAL_PARAM_SET32(VIDEO_ENCODE_MIN_QUANT, p->q_min);
  MMAL_PARAM_SET32(VIDEO_ENCODE_MAX_QUANT, p->q_max);
  MMAL_PARAM_SET32(INTRAPERIOD, p->intraperiod);
  MMAL_PARAM_SET32(VIDEO_BIT_RATE, p->bitrate);
  MMAL_PARAM_SET32(VIDEO_ENCODE_INLINE_HEADER, 1);

  /* INLINE VECTORS gives a stream that ffplay does not support */
#if 0
  MMAL_PARAM_SET32(VIDEO_ENCODE_INLINE_VECTORS, 1);
#endif

  MMAL_PARAM_SET32(VIDEO_ENCODE_SPS_TIMING, 1);

  err = mmal_port_parameter_set(e, MMAL_PARAM_PROFILE,
    &video_profile, sizeof(video_profile));
  CHECK_ERR("Failed to set h264 MMAL_PARAM_PROFILE param");

#if 0
  v32 = 1;
  err = mmal_port_parameter_set(e,
    MMAL_PARAM_VIDEO_ENCODE_INLINE_HEADER, &v32, sizeof(v32));
  CHECK_ERR("Failed to set h264 encoder parameter");
#endif

#if 0
  err = mmal_port_parameter_set(&encoder->input[0],
    MMAL_PARAM_VIDEO_ENCODE_SPS_TIMING, &bool_arg, sizeof(bool_arg));
  CHECK_ERR("Failed to set h264 encoder param 'sps timing'");

  err = mmal_port_parameter_set(&encoder->input[0],
    MMAL_PARAM_VIDEO_ENCODE_INLINE_VECTORS, &bool_arg, sizeof(bool_arg));
  CHECK_ERR("Failed to set h264 encoder param 'inline vectors'");

  err = mmal_port_parameter_set(&encoder->input[0],
    MMAL_PARAM_VIDEO_ENCODE_SPS_TIMING, &bool_arg, sizeof(bool_arg));
  CHECK_ERR("Failed to set h264 encoder param 'inline vectors'");
#endif
out_err:
  return err;
}

static int camera_make_resizer_port(int in_width, int in_height,
  int out_width, int out_height)
{
  int err = SUCCESS;
  struct mmal_component *c;
  uint32_t supported_encodings[MAX_SUPPORTED_ENCODINGS];
  int num_encodings;
 
  c = mmal_component_create("ril.isp");
  CHECK_ERR_PTR(c, "Failed to create component 'ril.isp'");

  if (!c->inputs || !c->outputs) {
    MODULE_ERR("err: ril.isp has 0 outputs or inputs");
    return ERR_GENERIC;
  }

  err = mmal_port_get_supp_encodings(&c->output[0], supported_encodings,
    sizeof(supported_encodings), &num_encodings);

  CHECK_ERR("Failed to retrieve supported encodings from port");

  err = mmal_component_enable(c);
  CHECK_ERR("Failed to enable resizer");

  mmal_port_dump("resizer.IN", &c->input[0]);
  mmal_port_dump("resizer.OUT", &c->output[0]);

  c->input[0].format.encoding = MMAL_ENCODING_OPAQUE;
  c->input[0].format.encoding_variant = MMAL_ENCODING_I420;
  c->input[0].format.es->video.width = in_width;
  c->input[0].format.es->video.height = in_height;
  c->input[0].format.es->video.crop.width = in_width;
  c->input[0].format.es->video.crop.height = in_height;
  err = mmal_port_set_format(&c->input[0]);
  CHECK_ERR("Failed to set format for resizer.IN port");

  c->output[0].format.encoding = MMAL_ENCODING_RGB24;
  c->output[0].format.encoding_variant = 0;
  c->output[0].format.es->video.width = out_width;
  c->output[0].format.es->video.height = out_height;
  c->output[0].format.es->video.crop.width = out_width;
  c->output[0].format.es->video.crop.height = out_height;
  err = mmal_port_set_format(&c->output[0]);
  CHECK_ERR("Failed to set format for resizer.OUT port");

  cam.resizer.in = &c->input[0];
  cam.resizer.out = &c->output[0];

out_err:
  return err;
}

static __attribute__((unused)) int camera_make_splitter_port(int width,
  int height)
{
  int err = SUCCESS;
  struct mmal_component *c;
 
  c = mmal_component_create("ril.video_splitter");
  CHECK_ERR_PTR(c, "Failed to create component 'ril.video_encode'");
  if (!c->inputs) {
    MODULE_ERR("err: splitter has no input");
    return ERR_GENERIC;
  }

  if (c->outputs < 2) {
    MODULE_ERR("err: splitter has wrong number of outputs %d", c->outputs);
    return ERR_GENERIC;
  }

  err = mmal_component_enable(c);
  CHECK_ERR("Failed to enable splitter component");

  err = mmal_port_enable(&c->control);
  CHECK_ERR("failed to enable splitter control port");

  mmal_format_set(&c->input[0].format, MMAL_ENCODING_OPAQUE,
    MMAL_ENCODING_I420, width, height, 0, 0);
  err = mmal_port_set_format(&c->input[0]);

  /* Output 0 will go to encoder */
  mmal_format_set(&c->output[0].format, MMAL_ENCODING_OPAQUE,
    MMAL_ENCODING_I420, width, height, 0, 0);
  err = mmal_port_set_format(&c->output[0]);

  /* Output 1 will go to preview display */
  mmal_format_set(&c->output[1].format, MMAL_ENCODING_OPAQUE,
    MMAL_ENCODING_I420, width, height, 0, 0);
  err = mmal_port_set_format(&c->output[1]);
  CHECK_ERR("Failed to set splitter input format");

  cam.splitter.in = &c->input[0];
  cam.splitter.out0 = &c->output[0];
  cam.splitter.out1 = &c->output[1];

out_err:
  return err;
}

static int camera_make_encoder(int width, int height, int frame_rate,
  uint32_t bit_rate)
{
  int err = SUCCESS;
  struct mmal_component *encoder;

  const struct encoder_h264_params encoder_params = {
    .q_initial = 18,
    .q_min = 10,
    .q_max = 22,
    .profile = MMAL_VIDEO_PROFILE_H264_HIGH,
    .level = MMAL_VIDEO_LEVEL_H264_4,
    .intraperiod = frame_rate * 2,
    .bitrate = bit_rate
  };
 
  encoder = mmal_component_create("ril.video_encode");
  CHECK_ERR_PTR(encoder, "Failed to create component 'ril.video_encode'");
  if (!encoder->inputs || !encoder->outputs) {
    MODULE_ERR("err: encoder has 0 outputs or inputs");
    return ERR_GENERIC;
  }

  err = mmal_component_enable(encoder);
  CHECK_ERR("Failed to enable encoder");

  err = mmal_port_enable(&encoder->control);
  CHECK_ERR("failed to enable control port");

  mmal_format_set(&encoder->output[0].format, MMAL_ENCODING_H264, 0, width,
    height, frame_rate, bit_rate);

  size_t requested_buf_size = 256 * 1024;
  size_t requested_num_buffers = 128;

  encoder->output[0].current_buffer.num = requested_num_buffers;
  encoder->output[0].current_buffer.size = requested_buf_size;

  err = mmal_port_set_format(&encoder->output[0]);
  CHECK_ERR("Failed to set format for preview capture port");

  if (encoder->output[0].current_buffer.num != requested_num_buffers)
  {
    MODULE_ERR("Backend refused to set number of buffers to %d, (got %d)",
      requested_num_buffers, encoder->output[0].current_buffer.num);
    return ERR_GENERIC;
  }
  if (encoder->output[0].current_buffer.size != requested_buf_size) {
    MODULE_ERR("Backend refused to set number of buffers to %d, (got %d)",
      requested_buf_size, encoder->output[0].current_buffer.size);
    return ERR_GENERIC;
  }

  mmal_format_set(&encoder->input[0].format, MMAL_ENCODING_OPAQUE,
    MMAL_ENCODING_I420, width, height, 0, 0);
  err = mmal_port_set_format(&encoder->input[0]);
  CHECK_ERR("Failed to set format for preview capture port");

  err = camera_encoder_params_dump(&encoder->output[0]);
  if (err)
    goto out_err;

  err = camera_encoder_params_set(&encoder->output[0], &encoder_params);
  if (err)
    goto out_err;

  err = camera_encoder_params_dump(&encoder->output[0]);

  cam.encoder.in = &encoder->input[0];
  cam.encoder.out = &encoder->output[0];

out_err:
  return err;
}


static int camera_make_camera(int frame_width ,int frame_height,
  int frame_rate, struct mmal_param_cam_info_cam *cam_info)
{
  struct mmal_component *c;
  struct mmal_port *preview, *still, *video;
  uint32_t param;
  uint32_t supported_encodings[MAX_SUPPORTED_ENCODINGS];
  int num_encodings;
  int err;
  /* mmal_init start */

  struct mmal_param_rational cam_brightness = {
    .num = 3,
    .den = 5
  };

  c = mmal_component_create("ril.camera");
  CHECK_ERR_PTR(c, "'ril.camera': failed to create component");

  param = 0;
  err = mmal_port_parameter_set(&c->control, MMAL_PARAM_CAMERA_NUM,
    &param, sizeof(param));
  CHECK_ERR("'ril.camera': failed to set param CAMERA_NUM");

  err = mmal_port_parameter_set(&c->control,
    MMAL_PARAM_CAMERA_CUSTOM_SENSOR_CONFIG, &param, sizeof(param));
  CHECK_ERR("'ril.camera': failed to set param SENSOR_CONFIG ");

  err = mmal_port_parameter_set(&c->control, MMAL_PARAM_BRIGHTNESS,
    &cam_brightness, sizeof(cam_brightness));
  CHECK_ERR("Failed to set param MMAL_PARAM_BRIGHTNESS");
  MODULE_INFO("BRIGHTNESS set");

  err = mmal_port_enable(&c->control);
  CHECK_ERR("'ril.camera': failed to enable port");

  err = mmal_set_camera_parameters(c, cam_info, frame_width, frame_height);
  CHECK_ERR("Failed to set parameters to component 'ril.camera'");

  preview = &c->output[CAM_PORT_PREVIEW];
  video = &c->output[CAM_PORT_VIDEO];
  still = &c->output[CAM_PORT_CAPTURE];

  err = mmal_port_get_supp_encodings(still, supported_encodings,
    sizeof(supported_encodings), &num_encodings);

  CHECK_ERR("Failed to retrieve supported encodings from port");

  mmal_format_set(&preview->format, MMAL_ENCODING_OPAQUE,
    MMAL_ENCODING_I420, frame_width, frame_height, frame_rate, 0);
  err = mmal_port_set_format(preview);
  CHECK_ERR("Failed to set format for preview capture port");

  mmal_format_set(&video->format, MMAL_ENCODING_OPAQUE,
    0, frame_width, frame_height, frame_rate, 0);

  err = mmal_port_set_format(video);
  CHECK_ERR("Failed to set format for video capture port");

  mmal_format_set(&still->format, MMAL_ENCODING_OPAQUE,
    MMAL_ENCODING_I420, frame_width, frame_height, 0, 0);

  err = mmal_port_set_format(still);
  CHECK_ERR("Failed to set format for still capture port");

  err = mmal_component_enable(c);
  CHECK_ERR("Failed to enable component camera");

  cam.camera.preview = preview;
  cam.camera.video = video;
  cam.camera.still = still;
out_err:
  return err;
}

int camera_video_start(void)
{
  uint32_t frame_count = 1;

  cam.bdev->ops.write_stream_open(cam.bdev, 0);

  return mmal_port_parameter_set(cam.camera.video, MMAL_PARAM_CAPTURE,
    &frame_count, sizeof(frame_count));
}

int camera_video_stop(void)
{
  uint32_t frame_count = 0;

  return mmal_port_parameter_set(cam.camera.video, MMAL_PARAM_CAPTURE,
    &frame_count, sizeof(frame_count));
}

static int mmal_apply_display_buffers(struct mmal_port *p,
  struct ili9341_buffer_info *buffers, size_t num_buffers)
{
  size_t i;
  int err;

  for (i = 0; i < num_buffers; ++i) {
    err = mmal_port_add_buffer(p, buffers[i].buffer,
      buffers[i].buffer_size, buffers[i].handle);
    CHECK_ERR("Failed to import display buffer #%d", i);
    MODULE_INFO("mmal_apply_display_buffers: port:%s, ptr:%08x, size:%d"
      " handle:%d, port_enabled:%s",
      p->name,
      buffers[i].buffer,
      buffers[i].buffer_size, buffers[i].handle,
      p->enabled ? "yes" : "no");
  }

  err = mmal_port_buffer_send_all(p);
  CHECK_ERR("Failed to send buffers to port");
out_err:
  return err;
}

static int camera_preview_init(int input_width, int input_height,
    int preview_width, int preview_height, bool start)
{
  int err;

  struct ili9341_buffer_info display_buffers[2];
  err = ili9341_nonstop_refresh_init(camera_on_preview_data_consumed_isr);
  CHECK_ERR("Failed to init display in non-stop refresh mode");
  err = ili9341_nonstop_refresh_get_buffers(display_buffers,
    ARRAY_SIZE(display_buffers));
  CHECK_ERR("Failed to get display dma buffers");

  camera_display_buffers_dump(display_buffers, ARRAY_SIZE(display_buffers));

  err = camera_make_resizer_port(input_width, input_height, preview_width,
    preview_height);

  CHECK_ERR("Failed to make resizer port");

  err = mmal_port_set_zero_copy(cam.camera.preview);
  CHECK_ERR("Failed to set zero copy to preview.OUT");
  err = mmal_port_set_zero_copy(cam.resizer.in);
  CHECK_ERR("Failed to set zero copy to resizer.IN");
  err = mmal_port_set_zero_copy(cam.resizer.out);
  CHECK_ERR("Failed to set zero copy to resizer.OUT");

  err = mmal_port_connect(cam.camera.preview, cam.resizer.in);
  CHECK_ERR("Failed to connect camera video.OUT to encoder.IN");
  cam.port_preview_stream = cam.resizer.out;

  /* Enable resizer ports */
  err = mmal_port_enable(cam.resizer.out);
  CHECK_ERR("Failed to enable resizer.OUT");

  err = mmal_apply_display_buffers(cam.resizer.out, display_buffers,
    ARRAY_SIZE(display_buffers));
  CHECK_ERR("Failed to add display buffers");

  if (start) {
    /*
     * Preview frames will start coming indefinitely as soon as
     * - preview port is enabled
     * - buffer is provided to the output of MMAL component graph, to where
     *   preview port is connected, in our case this is 'ril.isp.OUT' node of
     *   graph 'ril.camera.PREVIEW -> ril.isp.IN -> ril.isp.OUT'
     * - var name for ril.isp nodes are named as resizer_X
     */
    err = mmal_port_enable(cam.camera.preview);
    CHECK_ERR("Failed to enable preview.OUT");
    MODULE_INFO("Preview port enabled %p", cam.camera.preview);
  }
  return SUCCESS;

out_err:
  return err;
}

int camera_init(struct block_device *bdev, int frame_width, int frame_height,
  int frame_rate, uint32_t bit_rate, bool preview_enable, int preview_width,
  int preview_height)
{
  int err;
  struct mmal_param_cam_info cam_info = {0};
  cam.bdev = bdev;

  mmal_register_io_cb(camera_on_mmal_buffer_ready);

  err = camera_get_camera_info(&cam_info);
  CHECK_ERR("Failed to get num cameras");

  err = camera_make_camera(frame_width, frame_height, frame_rate,
    &cam_info.cameras[0]);
  CHECK_ERR("Failed to create camera component");

  err = camera_make_encoder(frame_width, frame_height, frame_rate,
    bit_rate);
  CHECK_ERR("Failed to create encoder component");

  err = mmal_port_connect(cam.camera.video, cam.encoder.in);
  CHECK_ERR("Failed to connect camera video.OUT to encoder.IN");

  err = mmal_port_set_zero_copy(cam.encoder.out);
  CHECK_ERR("Failed to set zero copy to encoder.OUT");
  err = mmal_port_set_zero_copy(cam.encoder.in);
  CHECK_ERR("Failed to set zero copy to encoder.IN");
  err = mmal_port_set_zero_copy(cam.camera.video);
  CHECK_ERR("Failed to set zero copy to video.OUT");
  cam.port_h264_stream = cam.encoder.out;

  if (preview_enable) {
    err = camera_preview_init(frame_width, frame_height, preview_width,
      preview_height, true);
    CHECK_ERR("Failed to enable preview graph when requested");
  }

  /*
   * Enable encoder ports.
   * NOTE: port must be enabled before queing the buffers
   */
  err = mmal_port_enable(cam.encoder.out);
  CHECK_ERR("Failed to enable encoder.OUT");

  err = mmal_port_enable(cam.encoder.in);
  CHECK_ERR("Failed to enable encoder.IN");

  err = mmal_port_init_buffers(cam.port_h264_stream, 1);
  CHECK_ERR("Failed to add buffers to encoder output");

  err = camera_video_start();
  CHECK_ERR("Failed to start camera capture");
  MODULE_INFO("Started video capture, err = %d", err);
  return SUCCESS;

out_err:
  return err;
}

