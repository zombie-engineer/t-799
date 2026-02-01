#include <app/camera.h>
#include <block_device.h>
#include <drivers/display/display_ili9341.h>
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
#include <printf.h>

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

struct camera_stats {
  struct cam_port_stats h264;
};

struct camera {
  struct block_device *bdev;

  struct camera_component_camera camera;
  struct camera_component_resizer resizer;
  struct camera_component_encoder encoder;
  struct camera_component_splitter splitter;
  struct mmal_port *port_preview_stream;
  struct mmal_port *port_h264_stream;

  /* size = 2 is hardcoded but might be extended */
  struct ili9341_per_frame_dma *preview_dma_bufs[2];

  struct camera_stats stats;
};

static struct camera cam = { 0 };

static bool cam_preview_buf_drawn = false;

static bool cam_h264_config_received = false;


static inline struct mmal_buffer *mmal_buf_fifo_pop(struct list_head *l)
{
  struct list_head *node;
  node = list_fifo_pop_head(l);
  return node ? list_entry(node, struct mmal_buffer, list) : NULL;
}

/*
 * Callback that draws camera preview buffer to display in response to signal
 * from VideoCore that preview buffer is ready.
 *
 * IMPORTANT DETAILS:
 *
 * This function implements an optimized ownership management for the preview
 * buffer which is OPPOSITE TO NAIVE EXPECTED pipeline.
 *
 * How expected ownership of preview buffer should work (not used here):
 * 1. Buffer is allocated, initialized  Owned by this kernel.
 * 2. Buffer is passed to MMAL stack to VideoCore with 'BUFFER_FROM_HOST' API.
 *    Owned by VideoCore now.
 * 3. VideoCore's MMAL fills the buffer with pixels and sends it with
 *    'BUFFER_TO_HOST' API. Owned by this kernel now.
 * 4. Kernel puts the buffer to display stack (DMA + SPI chains) to draw the
 *    buffer on display. Still owned by this kernel.
 * 5. Display finishes drawing the buffer and sends "done" callback from isr.
 *    Kernel being signalled about that sends the buffer back to VideoCore
 *    with 'BUFFER_FROM_HOST' API. Now the buffer is owned by VideoCore again.
 *
 * Issues with the above pipeline.
 * - On VideoCore MMAL's side preview stream (or pipeline) is stalled for the
 *   whole time from 4 to 5. VideoCore sees there are no free buffers for the
 *   preview output port, so the port is marked as stalled or throttled for
 *   this time.
 * - (Assumed to be so) as the input to preview port itself is sourced form the
 *   ISR souces node that is common to both the preview port and the video
 *   port, ISR is considered throttled in response to preview port marked
 *   throttled and by consequence video port output receives input pixel at a
 *   slower pace.
 * - As a result, the sub-graph ISR->camera.video port ->h264 encoder port
 *   has a reduced FPS (last observed is 22 instead of 30) and obvious frame
 *   drops are present in saved video
 *
 * OPTIMIZED PIPELINE (SHORTCUT):
 * 1. Buffer is allocated, initialized  Owned by this kernel.
 * 2. Buffer is passed to MMAL stack to VideoCore with 'BUFFER_FROM_HOST' API.
 *    Owned by VideoCore now.
 * 3. VideoCore's MMAL fills the buffer with pixels and sends it with
 *    'BUFFER_TO_HOST' API. Owned by this kernel now.
 * 4. Kernel requests redraw from the display stack (DMA + SPI chains) if
 *    display is not currently drawing. Immediately kernel returns the buffer
 *    back to VideoCore by 'BUFFER_FROM_HOST' without waiting for the "done"
 *    callback from the display stack. Buffer is then owned by VideoCore but
 *    is also used by the display stack.
 * So, at 4. the buffer is shared between a producer and consumer. How safe is
 * this in practice:
 * - Buffer is allocated once and its address and size is always known to the
 *   kernel.
 * - When VideoCore finishes drawing it sends the buffer, for the kernel it is
 *   useful as a signal that currently buffer has a frame that can be drawn and
 *   can start drawing it.
 * - But while this frame is being drawn, VideoCore is already owning the
 *   buffer and probalby is in process of copying pixels of a next frame.
 * - So the potential risk is that while display stack's DMA is copying bytes
 *   this could become next frame in the middle of the process.
 * - Also, because BUFFER_TO_HOST is immediately followed by BUFFER_FROM_HOST,
 *   there is the risk that as VideoCore can also be fast to fill in the buffer
 *   multiple times during a single DMA copy process on the display stack,
 *   visually that might look as if the frame on the display could belong to
 *   several different "torn" frames.
 * - This is the only identified risk so far. Decision is in favor of h264
 *   video output FPS, so we can live with potential frame tearing.
 */
static int OPTIMIZED camera_on_preview_buffer_ready(struct mmal_buffer *b)
{
  int err;
  int irqflags;
  bool should_draw;
  struct mmal_buffer *b2;
  struct mmal_port *p = cam.port_preview_stream;
  struct ili9341_per_frame_dma *dma_buf;

  disable_irq_save_flags(irqflags);
  b2 = mmal_buf_fifo_pop(&p->bufs.os_side_consumable);
  if (b != b2) {
    restore_irq_flags(irqflags);
    return ERR_INVAL;
  }

  if (!(b->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END)) {
    restore_irq_flags(irqflags);
    goto to_remote;
  }

  should_draw = cam_preview_buf_drawn;
  if (cam_preview_buf_drawn)
    cam_preview_buf_drawn = false;

  restore_irq_flags(irqflags);

  dma_buf = NULL;
  for (int i = 0; i < ARRAY_SIZE(cam.preview_dma_bufs); ++i) {
    if (cam.preview_dma_bufs[i]->buf == b->buffer)
      dma_buf = cam.preview_dma_bufs[i];
  }

  if (!dma_buf) {
    MODULE_ERR("Failed to find matching dma buffer");
    return ERR_INVAL;
  }

  if (should_draw)
    ili9341_draw_dma_buf(dma_buf);

to_remote:
  err = mmal_port_buffer_to_remote(p, b);
  if (err != SUCCESS) {
    MODULE_ERR("Failed to return unused display buffer to remode");
  }
  return err;
}

void cam_port_stats_fetch(struct cam_port_stats *dst, int stats_id)
{
  int irqflags;
  struct cam_port_stats *stats;

  if (stats_id != CAM_PORT_STATS_H264)
    return;
  stats = &cam.stats.h264;
  disable_irq_save_flags(irqflags);
  *dst = *stats;
  restore_irq_flags(irqflags);
}

static inline void cam_port_stats_on_buffer_from_vc(struct cam_port_stats *s,
  const struct mmal_buffer *b)
{
  int irqflags;
  struct cam_port_stats *stats = &cam.stats.h264;

  disable_irq_save_flags(irqflags);

  stats->bufs_from_vc++;
  stats->bufs_on_arm++;
  stats->bytes_from_vc += b->length;

  if (b->flags & MMAL_BUFFER_HEADER_FLAG_CONFIG)
    stats->configs_from_vc++;

  if (b->flags & MMAL_BUFFER_HEADER_FLAG_KEYFRAME)
    stats->keyframes_from_vc++;

  if (b->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_START)
    stats->frame_start_from_vc++;

  if (b->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END)
    stats->frame_end_from_vc++;

  if (b->flags & MMAL_BUFFER_HEADER_FLAG_NAL_END)
    stats->nal_end_from_vc++;

  restore_irq_flags(irqflags);
}

static inline void cam_port_stats_on_buffer_to_vc(struct cam_port_stats *s,
  const struct mmal_buffer *b)
{
  int irqflags;
  struct cam_port_stats *stats = &cam.stats.h264;
  disable_irq_save_flags(irqflags);
  stats->bufs_to_vc++;
  stats->bufs_on_arm--;
  restore_irq_flags(irqflags);
}

static int OPTIMIZED camera_on_h264_buffer_ready(struct mmal_buffer *b)
{
  int irqflags;
  int err;
  struct mmal_buffer *b2;
  struct list_head iobufs = LIST_HEAD_INIT(iobufs);
  struct list_head *node;
  struct write_stream_buf *iobuf;
  struct write_stream_buf *iobuf_tmp;
  struct mmal_port *p = cam.port_h264_stream;
  struct cam_port_stats *stats = &cam.stats.h264;

  cam_port_stats_on_buffer_from_vc(stats, b);

  if (!(stats->keyframes_from_vc % 25))
    os_log(".%d\r\n", stats->keyframes_from_vc);

  disable_irq_save_flags(irqflags);
  b2 = mmal_buf_fifo_pop(&p->bufs.os_side_consumable);
  if (b != b2) {
    restore_irq_flags(irqflags);
    return ERR_INVAL;
  }

  if (b->flags & MMAL_BUFFER_HEADER_FLAG_CONFIG) {
    if (!cam_h264_config_received)
      cam_h264_config_received = true;
  }

  if (!cam_h264_config_received) {
    disable_irq_save_flags(irqflags);
    list_del(&b->list);
    list_add_tail(&b->list, &p->bufs.os_side_free);
    restore_irq_flags(irqflags);
    cam_port_stats_on_buffer_to_vc(stats, b);
    goto second_half;
  }

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
    cam_port_stats_on_buffer_to_vc(stats, b);
  }

second_half:
  err = mmal_port_buffers_to_remote(p, &p->bufs.os_side_free, false);
  if (err != SUCCESS) {
    MODULE_ERR("failed to submit buffer list, err: %d", err);
  }
  return SUCCESS;
}

static int OPTIMIZED camera_on_mmal_buffer_ready(struct mmal_port *p,
  struct mmal_buffer *b)
{
  if (p == cam.port_h264_stream)
    return camera_on_h264_buffer_ready(b);
  if (p == cam.port_preview_stream)
    return camera_on_preview_buffer_ready(b);
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

static void OPTIMIZED camera_on_preview_data_consumed_isr(
  struct ili9341_per_frame_dma *buf)
{
  cam_preview_buf_drawn = true;
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
  const struct camera_video_port_config_h264 *p)
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

  err = mmal_port_parameter_set(e, MMAL_PARAM_PROFILE,
    &video_profile, sizeof(video_profile));
  CHECK_ERR("Failed to set h264 MMAL_PARAM_PROFILE param");

  /* INLINE VECTORS gives a stream that ffplay does not support */
  MMAL_PARAM_SET32(VIDEO_ENCODE_INLINE_HEADER, 1);
#if 0
  MMAL_PARAM_SET32(VIDEO_ENCODE_INLINE_VECTORS, 1);
#endif


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

static int camera_make_resizer(int in_width, int in_height, int out_width,
  int out_height, int num_buffers)
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
  c->output[0].current_buffer.num = num_buffers;
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

static int camera_make_encoder(unsigned int width, unsigned int height,
  unsigned int frames_per_sec, unsigned int bit_rate,
  size_t requested_num_buffers, size_t requested_buf_size,
  const struct camera_video_port_config_h264 *encoder_config)
{
  int err = SUCCESS;
  struct mmal_component *encoder;

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
    height, frames_per_sec, bit_rate);

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

  err = camera_encoder_params_set(&encoder->output[0], encoder_config);
  if (err)
    goto out_err;

  err = camera_encoder_params_dump(&encoder->output[0]);

  cam.encoder.in = &encoder->input[0];
  cam.encoder.out = &encoder->output[0];

out_err:
  return err;
}


static int camera_make_camera(unsigned int frame_width,
  unsigned int frame_height, unsigned int frames_per_sec,
  struct mmal_param_cam_info_cam *cam_info)
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
    MMAL_ENCODING_I420, frame_width, frame_height, frames_per_sec, 0);
  err = mmal_port_set_format(preview);
  CHECK_ERR("Failed to set format for preview capture port");

  mmal_format_set(&video->format, MMAL_ENCODING_OPAQUE,
    0, frame_width, frame_height, frames_per_sec, 0);

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

  os_log("Starting capture...\r\n");
  return mmal_port_parameter_set(cam.camera.video, MMAL_PARAM_CAPTURE,
    &frame_count, sizeof(frame_count));
}

int camera_video_stop(void)
{
  uint32_t frame_count = 0;

  return mmal_port_parameter_set(cam.camera.video, MMAL_PARAM_CAPTURE,
    &frame_count, sizeof(frame_count));
}

static int camera_init_preview_buffers(struct mmal_port *p,
  struct ili9341_drawframe *drawframe)
{
  size_t i;
  int err;
  struct ili9341_per_frame_dma *buf;

  for (i = 0; i < drawframe->num_bufs; ++i) {
    buf = &drawframe->bufs[i];
    if (i >= ARRAY_SIZE(cam.preview_dma_bufs)) {
      MODULE_ERR("Too many buffers provided");
      return ERR_INVAL;
    }
    cam.preview_dma_bufs[i] = buf;
    err = mmal_port_add_buffer(p, buf->buf, buf->buf_size, buf->handle);
    CHECK_ERR("Failed to import display buffer #%d", i);
    MODULE_INFO("mmal_apply_display_buffers: port:%s, ptr:%08x, size:%d"
      " handle:%d, port_enabled:%s",
      p->name,
      buf->buf,
      buf->buf_size, buf->handle,
      p->enabled ? "yes" : "no");
  }

  err = mmal_port_buffers_to_remote(p, &p->bufs.os_side_free, true);
  CHECK_ERR("Failed to send buffers to port");
out_err:
  return err;
}

static int camera_preview_init(struct ili9341_drawframe *drawframe,
  int input_width, int input_height, int preview_width, int preview_height,
  bool start)
{
  int err;

  err = ili9341_drawframe_set_irq(drawframe,
    camera_on_preview_data_consumed_isr);
  cam_preview_buf_drawn = true;

  err = camera_make_resizer(input_width, input_height, preview_width,
    preview_height, drawframe->num_bufs);

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

  err = camera_init_preview_buffers(cam.resizer.out, drawframe);
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

int camera_init(const struct camera_config *conf)
{
  int err;
  struct mmal_param_cam_info cam_info = {0};
  cam.bdev = conf->block_device;
  cam.port_h264_stream = NULL;
  cam.port_preview_stream = NULL;

  mmal_register_io_cb(camera_on_mmal_buffer_ready);

  err = camera_get_camera_info(&cam_info);
  CHECK_ERR("Failed to get num cameras");

  err = camera_make_camera(conf->frame_size_x, conf->frame_size_y,
    conf->frames_per_sec, &cam_info.cameras[0]);
  CHECK_ERR("Failed to create camera component");

  err = camera_make_encoder(conf->frame_size_x, conf->frame_size_y,
    conf->frames_per_sec, conf->bit_rate, conf->h264_buffers_num,
    conf->h264_buffer_size, &conf->h264_encoder);

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

  if (conf->enable_preview) {
    err = camera_preview_init(conf->preview_drawframe, conf->frame_size_x,
      conf->frame_size_y, conf->preview_size_x, conf->preview_size_y, true);
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
