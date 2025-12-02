#include <vc/service_mmal.h>
#include <vc/vchiq.h>
#include <vc/service_smem.h>
#include <vc/service_mmal_param.h>
#include <list.h>
#include <task.h>
#include <memory_map.h>
#include <string.h>
#include <printf.h>
#include <list_fifo.h>
#include <event.h>
#include <os_api.h>
#include <errcode.h>
#include <bitops.h>
#include <kmalloc.h>
#include <write_stream_buffer.h>
#include <os_msgq.h>

#define MODULE_UNIT_TAG "mmal"
#include <module_common.h>

#define GEN_SERVICE_ID(a, b, c, d) ((a) | (b << 8) | (c << 16) | (d << 24))
#define MMAL_MAGIC GEN_SERVICE_ID('m', 'm', 'a', 'l')

#define VC_MMAL_VER 15
#define VC_MMAL_MIN_VER 10

#define MMAL_IO_BUF_UNKNOWN  0
#define MMAL_IO_BUF_READY    1
#define MMAL_IO_BUF_CONSUMED 2

#define MMAL_IO_MSGQ_MAX_COUNT 32
#define MMAL_IO_MSGQ_BUF_SIZE \
  (sizeof(struct mmal_msgq_msg) * MMAL_IO_MSGQ_MAX_COUNT)

struct mmal_msgq_msg {
  uint64_t id;
  struct mmal_port *port;
  struct mmal_buffer *buffer;
} /* PACKED */;

struct mmal_msg_context {
  union {
    struct {
      int msg_type;
      struct event completion_waitflag;
      struct mmal_msg *rmsg;
      int rmsg_size;
    } sync;
    struct {
      struct mmal_port *port;
    } bulk;
  } u;
};

struct mmal_port_param_stats {
  /* Total number of buffers processed */
  uint32_t buffer_count;

  /* Total number of frames processed */
  uint32_t frame_count;

  /* Number of frames without expected PTS based on frame rate */
  uint32_t frames_skipped;

  /* Number of frames discarded */
  uint32_t frames_discarded;

  /* Set if the end of stream has been reached */
  uint32_t eos_seen;

  /* Maximum frame size in bytes */
  uint32_t maximum_frame_bytes;

  /* Total number of bytes processed */
  int64_t  total_bytes;

  /* Number of corrupt macroblocks in the stream */
  uint32_t corrupt_macroblocks;
};

struct mmal_port_param_core_stats {
  /* Total buffer count on this port */
  uint32_t buffer_count;

  /* Time (us) of first buffer seen on this port */
  uint32_t first_buffer_time;

  /* Time (us) of most recently buffer on this port */
  uint32_t last_buffer_time;

  /* Max delay (us) between buffers, ignoring first few frames */
  uint32_t max_delay;
};

static const char *const mmal_msg_type_names[] = {
  "UNKNOWN",
  "QUIT",
  "SERVICE_CLOSED",
  "GET_VERSION",
  "COMPONENT_CREATE",
  "COMPONENT_DESTROY",
  "COMPONENT_ENABLE",
  "COMPONENT_DISABLE",
  "PORT_INFO_GET",
  "PORT_INFO_SET",
  "PORT_ACTION",
  "BUFFER_FROM_HOST",
  "BUFFER_TO_HOST",
  "GET_STATS",
  "PORT_PARAMETER_SET",
  "PORT_PARAMETER_GET",
  "EVENT_TO_HOST",
  "GET_CORE_STATS_FOR_PORT",
  "OPAQUE_ALLOCATOR",
  "CONSUME_MEM",
  "LMK",
  "OPAQUE_ALLOCATOR_DESC",
  "DRM_GET_LHS32",
  "DRM_GET_TIME",
  "BUFFER_FROM_HOST_ZEROLEN",
  "PORT_FLUSH",
  "HOST_LOG",
};

static struct vchiq_service *service_mmal = NULL;

static uint8_t mmal_io_msgq_buffer[MMAL_IO_MSGQ_BUF_SIZE];

static struct os_msgq mmal_io_msgq;

static mmal_io_buffer_ready_cb_t mmal_io_buffer_ready_cb = NULL;

static struct mmal_msg_context *mmal_msg_context_from_handle(uint32_t handle)
{
  return (struct mmal_msg_context *)(uint64_t)(handle | 0xffff000000000000);
}

static uint32_t mmal_msg_context_to_handle(const struct mmal_msg_context *ctx)
{
  return (uint32_t)((uint64_t)ctx & ~0xffff000000000000);
}

static void mmal_msg_fill_header(struct vchiq_service *service,
  int mmal_msg_type, struct mmal_msg *msg, struct mmal_msg_context *ctx)
{
  msg->h.magic = MMAL_MAGIC;
  msg->h.context = mmal_msg_context_to_handle(ctx);
  msg->h.control_service = vchiq_service_to_handle(service);
  msg->h.status = 0;
  msg->h.padding = 0;
  msg->h.type = mmal_msg_type;
}

static inline void *mmal_msg_check_reply(struct mmal_msg *rmsg,
  int msg_type)
{
  if (rmsg->h.type != msg_type) {
    MODULE_ERR("mmal msg expected type %s, received: %s",
      mmal_msg_type_names[msg_type],
      mmal_msg_type_names[rmsg->h.type]);
    return NULL;
  }
  return &rmsg->u;
}

#define VCHIQ_MMAL_MSG_COMMUNICATE_ASYNC() \
  os_event_init(&ctx.u.sync.completion_waitflag); \
  mmal_msg_fill_header(_ms, ctx.u.sync.msg_type, &msg, &ctx); \
  vchiq_msg_prep(VCHIQ_MSG_DATA, _ms->localport, \
    _ms->remoteport, &msg, sizeof(struct mmal_msg_header) + sizeof(*m)); \
  vchiq_event_signal_trigger();

#define VCHIQ_MMAL_MSG_COMMUNICATE_SYNC() \
  VCHIQ_MMAL_MSG_COMMUNICATE_ASYNC(); \
  os_event_wait(&ctx.u.sync.completion_waitflag); \
  os_event_clear(&ctx.u.sync.completion_waitflag); \
  r = mmal_msg_check_reply(ctx.u.sync.rmsg, ctx.u.sync.msg_type); \
  if (!r) { \
    MODULE_ERR("invalid reply");\
    return ERR_GENERIC; \
  } \
  if (r->status != MMAL_MSG_STATUS_SUCCESS) { \
    MODULE_ERR("status not success: %d", r->status); \
    return ERR_GENERIC; \
  }

#define MMAL_MSG_CONTEXT_INIT_SYNC(__msg_type) \
{ \
  .u.sync = { \
    .msg_type = MMAL_MSG_TYPE_ ## __msg_type,\
    .completion_waitflag = EVENT_INIT,\
    .rmsg = NULL \
  }\
}

#define VCHIQ_MMAL_MSG_DECL_ASYNC(__ms, __msg_type, __msg_u) \
  struct mmal_msg_context ctx = MMAL_MSG_CONTEXT_INIT_SYNC(__msg_type); \
  struct mmal_msg msg = { 0 }; \
  struct vchiq_service *_ms = __ms; \
  struct mmal_msg_ ## __msg_u *m = &msg.u. __msg_u

#define VCHIQ_MMAL_MSG_DECL(__ms, __mmal_msg_type, __msg_u, __msg_u_reply) \
  VCHIQ_MMAL_MSG_DECL_ASYNC(__ms, __mmal_msg_type, __msg_u); \
  struct mmal_msg_ ## __msg_u_reply *r;


static void mmal_format_print(const char *action, const char *name,
  const struct mmal_es_format_local *f)
{
  const char *src0 = (const char *)&f->encoding;
  const char *src1 = (const char *)&f->encoding_variant;

  char str_enc[] = { src0[0], src0[1], src0[2], src0[3], 0 };
  char str_enc_var[] = { src1[0], src1[1], src1[2], src1[3], 0 };

  MODULE_INFO("%s:%s:tp:%d %s/%s,bitrate:%d,%dx%d,f:%02x", action, name,
    f->type, str_enc,
    f->encoding_variant ? str_enc_var : "0",
    f->bitrate, f->es->video.width,
    f->es->video.height, f->flags);
}

static int mmal_send_msg_buffer_from_host(struct mmal_port *p,
  struct mmal_buffer *b)
{
  /*
   * Kernel code in bcm2835-camera.c states this is only possible for enabled
   * port
   */
  if (!p->enabled)
    return ERR_INVAL;

  VCHIQ_MMAL_MSG_DECL_ASYNC(p->component->ms, BUFFER_FROM_HOST,
    buffer_from_host);

  memset(m, 0, sizeof(*m));

  m->drvbuf.magic = MMAL_MAGIC;
  m->drvbuf.component_handle = p->component->handle;
  m->drvbuf.port_handle = p->handle;
  m->drvbuf.client_context = (uint32_t)((uint64_t)b & ~0xffff000000000000);

  m->is_zero_copy = p->zero_copy;
  m->buffer_header.next = 0;
  m->buffer_header.priv = 0;
  m->buffer_header.cmd = 0;
  m->buffer_header.user_data = NARROW_PTR(b);
  if (p->zero_copy)
    m->buffer_header.data = b->vcsm_handle;
  else
    m->buffer_header.data = RAM_PHY_TO_BUS_UNCACHED(b->buffer);
  m->buffer_header.alloc_size = b->buffer_size;

  if (p->type == MMAL_PORT_TYPE_OUTPUT) {
    m->buffer_header.length = 0;
    m->buffer_header.offset = 0;
    m->buffer_header.flags = 0;
    m->buffer_header.pts = MMAL_TIME_UNKNOWN;
    m->buffer_header.dts = MMAL_TIME_UNKNOWN;
  } else {
    m->buffer_header.length = b->length;
    m->buffer_header.offset = 0;
    m->buffer_header.flags = b->flags;
    m->buffer_header.pts = b->pts;
    m->buffer_header.dts = b->dts;
  }

  memset(&m->buffer_header_type_specific, 0,
    sizeof(m->buffer_header_type_specific));
  m->payload_in_message = 0;

  VCHIQ_MMAL_MSG_COMMUNICATE_ASYNC();
  return SUCCESS;
}

static int mmal_port_info_get(struct mmal_port *p)
{
  VCHIQ_MMAL_MSG_DECL(p->component->ms, PORT_INFO_GET, port_info_get,
    port_info_get_reply);

  m->component_handle = p->component->handle;
  m->index = p->index;
  m->port_type = p->type;

  VCHIQ_MMAL_MSG_COMMUNICATE_SYNC();

  p->enabled = r->port.is_enabled ? 1 : 0;
  p->handle = r->port_handle;
  p->type = r->port_type;
  p->index = r->port_index;

  p->minimum_buffer.num = r->port.buffer_num_min;
  p->minimum_buffer.size = r->port.buffer_size_min;
  p->minimum_buffer.alignment = r->port.buffer_alignment_min;

  p->recommended_buffer.alignment = r->port.buffer_alignment_min;
  p->recommended_buffer.num = r->port.buffer_num_recommended;
  p->recommended_buffer.size = r->port.buffer_size_recommended;

  p->current_buffer.num = r->port.buffer_num;
  p->current_buffer.size = r->port.buffer_size;

  /* stream format */
  p->format.type = r->format.type;
  p->format.encoding = r->format.encoding;
  p->format.encoding_variant = r->format.encoding_variant;
  p->format.bitrate = r->format.bitrate;
  p->format.flags = r->format.flags;

  /* elementary stream format */
  memcpy(&p->es, &r->es, sizeof(union mmal_es_specific_format));
  p->format.es = &p->es;

  p->format.extradata_size = r->format.extradata_size;
  memcpy(p->format.extradata, r->extradata, p->format.extradata_size);

  mmal_format_print("actual", p->name, &p->format);
  MODULE_INFO(" ena:%d,min:%dx%d,rec:%dx%d,curr:%dx%d", p->enabled,
    p->minimum_buffer.num, p->minimum_buffer.size,
    p->recommended_buffer.num, p->recommended_buffer.size,
    p->current_buffer.num, p->current_buffer.size);

  return SUCCESS;
}


int OPTIMIZED mmal_port_buffers_to_remote(struct mmal_port *p,
  struct list_head *l, bool report_result)
{
  int irqflags;
  int err = SUCCESS;
  struct mmal_buffer *b;

  disable_irq_save_flags(irqflags);
  while (!list_empty(l)) {
    b = list_first_entry(l, struct mmal_buffer, list);
    list_del(&b->list);
    list_add_tail(&b->list, &p->bufs.remote_side);
    restore_irq_flags(irqflags);

    err = mmal_send_msg_buffer_from_host(p, b);
    CHECK_ERR("Failed to submit buffer");
    disable_irq_save_flags(irqflags);
  }

  if (report_result) {
    err = mmal_port_info_get(p);
    CHECK_ERR("failed to get port info");
    MODULE_INFO("port buf applied: nr:%d, size:%d",
      p->current_buffer.num, p->current_buffer.size);
  }

out_err:
  restore_irq_flags(irqflags);
  return err;
}

int mmal_port_set_zero_copy(struct mmal_port *p)
{
  uint32_t zero_copy = 1;

  return mmal_port_parameter_set(p, MMAL_PARAM_ZERO_COPY, &zero_copy,
    sizeof(zero_copy));
}

static inline struct mmal_buffer *mmal_port_get_buffer_from_header(
  struct mmal_port *p, struct list_head *buffers, uint32_t handle)
{
  int irqflags;
  struct mmal_buffer *b;

  disable_irq_save_flags(irqflags);
  list_for_each_entry(b, buffers, list) {
    if (handle == b->vcsm_handle) {
      restore_irq_flags(irqflags);
      return b;
    }
  }

  restore_irq_flags(irqflags);
  MODULE_ERR("buffer not found for handle: %08x on port:%s", handle, p->name);
  return NULL;
}

static inline void mmal_print_supported_encodings(uint32_t *encodings, int num)
{
  int i, ii;
  char buf[5];
  buf[4] = 0;
  char *ptr = (char *)encodings;
  for (i = 0; i < num; ++i) {
    for (ii = 0; ii < 4; ++ii) {
      buf[ii] = ptr[ii];
    }
    ptr += 4;
    MODULE_INFO("supported_encoding: %s", buf);
  }
}

int mmal_port_get_supp_encodings(struct mmal_port *p, uint32_t *encodings,
  int max_encodings, int *num_encodings)
{
  int err;
  uint32_t param_size;

  param_size = max_encodings * sizeof(*encodings);
  err = mmal_port_parameter_get(p, MMAL_PARAM_SUPPORTED_ENCODINGS,
    encodings, &param_size);

  CHECK_ERR("Failed to get supported_encodings");
  *num_encodings = param_size / sizeof(*encodings);
  mmal_print_supported_encodings(encodings, *num_encodings);
  return SUCCESS;
out_err:
  return err;
}

void mmal_format_set(struct mmal_es_format_local *f, int encoding,
  int encoding_variant, int width, int height, int frame_rate, int bitrate)
{
  f->encoding = encoding;
  f->encoding_variant = encoding_variant;
  f->es->video.width = width;
  f->es->video.height = height;
  f->es->video.crop.x = 0;
  f->es->video.crop.y = 0;
  f->es->video.crop.width = width;
  f->es->video.crop.height = height;

  /* frame rate taken from RaspiVid.c */
  f->es->video.frame_rate.num = frame_rate;
  f->es->video.frame_rate.den = 1;

  f->bitrate = bitrate;
}

static void mmal_port_to_msg(struct mmal_port *port, struct mmal_port_msg *p)
{
  /* todo do readonly fields need setting at all? */
  p->type = port->type;
  p->index = port->index;
  p->index_all = 0;
  p->is_enabled = port->enabled;
  p->buffer_num_min = port->minimum_buffer.num;
  p->buffer_size_min = port->minimum_buffer.size;
  p->buffer_alignment_min = port->minimum_buffer.alignment;
  p->buffer_num_recommended = port->recommended_buffer.num;
  p->buffer_size_recommended = port->recommended_buffer.size;

  /* only three writable fields in a port */
  p->buffer_num = port->current_buffer.num;
  p->buffer_size = port->current_buffer.size;
  p->userdata = (uint32_t)(unsigned long)port;
}

static int mmal_port_info_set(struct mmal_port *p)
{
  VCHIQ_MMAL_MSG_DECL(p->component->ms, PORT_INFO_SET, port_info_set,
    port_info_set_reply);

  m->component_handle = p->component->handle;
  m->port_type = p->type;
  m->port_index = p->index;

  mmal_port_to_msg(p, &m->port);
  m->format.type = p->format.type;
  m->format.encoding = p->format.encoding;
  m->format.encoding_variant = p->format.encoding_variant;
  m->format.bitrate = p->format.bitrate;
  m->format.flags = p->format.flags;

  memcpy(&m->es, &p->es, sizeof(union mmal_es_specific_format));

  m->format.extradata_size = p->format.extradata_size;
  memcpy(&m->extradata, p->format.extradata, p->format.extradata_size);
  VCHIQ_MMAL_MSG_COMMUNICATE_SYNC();
  return SUCCESS;
}

int mmal_port_set_format(struct mmal_port *p)
{
  int err;

  mmal_format_print("requested", p->name, &p->format);
  err = mmal_port_info_set(p);
  CHECK_ERR("failed to set port info");

  err = mmal_port_info_get(p);
  CHECK_ERR("failed to get port info");
out_err:
  return err;
}

void mmal_port_dump(const char *tag, const struct mmal_port *p)
{
  printf("%s:handle:%d type:%d,encoding:%08x,variant:%08x,%dx%d,f:%d/%d,"
    "aspect:%d/%d,color:%d\r\n",
    tag, p->handle, p->format.type, p->format.encoding,
    p->format.encoding_variant, p->format.es->video.width,
    p->format.es->video.height,
    p->format.es->video.frame_rate.num,
    p->format.es->video.frame_rate.den,
    p->format.es->video.par.num,
    p->format.es->video.par.den,
    p->format.es->video.color_space);

  printf("%s: minbuf: %dx%ld, recommended:%dx%d, current:%dx%ld\r\n",
    tag,
    p->minimum_buffer.num,
    p->minimum_buffer.size,
    p->recommended_buffer.num,
    p->recommended_buffer.size,
    p->current_buffer.num,
    p->current_buffer.size);
  printf("%s: es:%p\r\n", tag);
}

static inline void mmal_buf_fifo_push(struct list_head *l,
  struct mmal_buffer *b)
{
  list_fifo_push_tail(l, &b->list);
}

static struct mmal_port *mmal_port_get_by_handle(struct mmal_component *c,
  uint32_t handle)
{
  size_t i;

  if (c->control.handle == handle)
    return &c->control;

  for (i = 0; i < c->inputs; ++i) {
    if (c->input[i].handle == handle)
      return &c->input[i];
  }

  for (i = 0; i < c->outputs; ++i) {
    if (c->output[i].handle == handle)
      return &c->output[i];
  }
  return NULL;
}

static void mmal_port_report_buffer_ready(struct mmal_port *p,
  struct mmal_buffer *b)
{
  struct mmal_msgq_msg m = {
    .id = MMAL_IO_BUF_READY,
    .port = p,
    .buffer = b
  };

  os_msgq_put(&mmal_io_msgq, &m);
}

/*
 * take buffer_to_host message and find associated component, port and buffer
 * and return port and buffer
 */
static inline int mmal_buffer_to_host_msg_to_port(
  const struct mmal_msg_buffer_from_host *m,
  struct mmal_port **out_p, struct mmal_buffer **out_b)
{
  struct mmal_component *c;
  struct list_head *components = vchiq_get_components_list();
  struct mmal_port *p;
  struct mmal_buffer *b;

  list_for_each_entry(c, components, list) {
    if (c->handle == m->drvbuf.component_handle)
      break;
  }

  if (c->handle != m->drvbuf.component_handle) {
    MODULE_ERR("Failed to find component for buffer");
    return ERR_NOT_FOUND;
  }

  p = mmal_port_get_by_handle(c, m->drvbuf.port_handle);
  if (!p) {
    MODULE_ERR("Failed to find component for buffer");
    return ERR_NOT_FOUND;
  }

  b = mmal_port_get_buffer_from_header(p, &p->bufs.remote_side,
    m->buffer_header.data);

  if (!b || !b->buffer) {
    os_log("Bad buffer %p->%p\r\n", b, b ? b->buffer : NULL);
    while(1)
      asm volatile ("wfe");
  }

  *out_p = p;
  *out_b = b;

  return SUCCESS;
}

static inline void mmal_port_buffer_to_host_cb(struct mmal_port *p,
  const struct mmal_msg_buffer_from_host *m,
  struct mmal_buffer *b)
{
  int irqflags;
  disable_irq_save_flags(irqflags);
  list_del_init(&b->list);
  b->length = m->buffer_header.length;
  b->flags = m->buffer_header.flags;
  mmal_buf_fifo_push(&p->bufs.os_side_consumable, b);
  restore_irq_flags(irqflags);
}

int OPTIMIZED  mmal_port_buffer_to_remote(struct mmal_port *p,
  struct mmal_buffer *b)
{
  int irqflags;

  disable_irq_save_flags(irqflags);
  list_del_init(&b->list);
  list_add_tail(&b->list, &p->bufs.remote_side);
  restore_irq_flags(irqflags);
  return mmal_send_msg_buffer_from_host(p, b);
}

static int OPTIMIZED mmal_buffer_to_host_cb(const struct mmal_msg *m)
{
  int err;
  struct mmal_port *p;
  struct mmal_buffer *b;

  err = mmal_buffer_to_host_msg_to_port(&m->u.buffer_from_host, &p, &b);
  if (p->bufs.acks_count < p->bufs.total_count) {
    if (b->length || b->flags) {
      MODULE_ERR("non-0 buf during ack %d/%d", p->bufs.acks_count,
        p->bufs.total_count);
    }
    p->bufs.acks_count++;
    err = mmal_port_buffer_to_remote(p, b);
    CHECK_ERR("Failed to re-send acknowledged buffer to vc");
    return SUCCESS;
  }

  CHECK_ERR("Failed to find matching port and buffer");
  mmal_port_buffer_to_host_cb(p, &m->u.buffer_from_host, b);
  mmal_port_report_buffer_ready(p, b);
  err = SUCCESS;

out_err:
  return err;
}

static int mmal_port_action_port(struct mmal_component *c, struct mmal_port *p,
  int action)
{
  VCHIQ_MMAL_MSG_DECL(c->ms, PORT_ACTION, port_action_port, port_action_reply);

  m->component_handle = c->handle;
  m->port_handle = p->handle;
  m->action = action;
  mmal_port_to_msg(p, &m->port);

  VCHIQ_MMAL_MSG_COMMUNICATE_SYNC();
  return SUCCESS;
}

static __attribute__((unused)) int mmal_port_get_systime(struct mmal_port *p,
  uint64_t *time)
{
  int err;
  uint64_t systime;
  uint32_t size;

  size = sizeof(systime);
  err = mmal_port_parameter_get(p,
    MMAL_PARAM_SYSTEM_TIME, &systime, &size);
  CHECK_ERR("Failed to get port systime");
  *time = systime;

  // MODULE_INFO("system_time:%d.%dms", systime / 1000, systime % 1000);
out_err:
  return err;

  return SUCCESS;
}

static __attribute__((unused)) int mmal_port_get_stats(struct mmal_port *p)
{
  int err;
  struct mmal_port_param_stats stats;
  uint32_t size;
  // struct mmal_port_param_core_stats core_stats;
  // uint32_t pool_mem_alloc_size;

  size = sizeof(stats);
  err = mmal_port_parameter_get(p,
    MMAL_PARAM_STATISTICS, &stats, &size);
  CHECK_ERR("Failed to get port stats");

  MODULE_INFO("b:%d,fski:%d,fdis:%d,eos:%d,maxb:%d,tot:%d,cmb:%d",
   stats.buffer_count, stats.frame_count,
   stats.frames_skipped,
   stats.frames_discarded,
   stats.frames_discarded,
   stats.eos_seen,
   stats.maximum_frame_bytes,
   stats.total_bytes,
   stats.corrupt_macroblocks
   );

#if 0
  size = sizeof(core_stats);
  err = mmal_port_parameter_get(p->component, p,
    MMAL_PARAM_CORE_STATISTICS, &core_stats, &size);
  CHECK_ERR("Failed to get port core stats");

  size = sizeof(pool_mem_alloc_size);
  err = mmal_port_parameter_get(p->component, p,
    MMAL_PARAM_MEM_USAGE, &pool_mem_alloc_size, &size);
  CHECK_ERR("Failed to get port mem usage");

#endif

out_err:
  return err;

  return SUCCESS;
}

int mmal_port_parameter_set(struct mmal_port *p, uint32_t parameter_id,
  const void *value, int value_size)
{
  VCHIQ_MMAL_MSG_DECL(p->component->ms, PORT_PARAMETER_SET, port_parameter_set,
    port_parameter_set_reply);

  m->component_handle = p->component->handle;
  m->port_handle = p->handle;
  m->id = parameter_id;
  m->size = 2 * sizeof(uint32_t) + value_size;
  memcpy(&m->value, value, value_size);

  VCHIQ_MMAL_MSG_COMMUNICATE_SYNC();
  if (parameter_id == MMAL_PARAM_ZERO_COPY)
    p->zero_copy = 1;
  return SUCCESS;
}

int mmal_port_parameter_get(struct mmal_port *p, int parameter_id, void *value,
  uint32_t *value_size)
{
  VCHIQ_MMAL_MSG_DECL(p->component->ms, PORT_PARAMETER_GET, port_parameter_get,
    port_parameter_get_reply);

  m->component_handle = p->component->handle;
  m->port_handle = p->handle;
  m->id = parameter_id;
  m->size = 2 * sizeof(uint32_t) + *value_size;

  VCHIQ_MMAL_MSG_COMMUNICATE_SYNC();
  MODULE_INFO("port_parameter_get: id:%d,resp:status:%d,id:%d,size:%d\n",
    parameter_id, r->status, r->id, r->size);

  memcpy(value, r->value, MIN(r->size, *value_size));
  *value_size = r->size;
  return SUCCESS;
}


int mmal_port_connect(struct mmal_port *src, struct mmal_port *dst)
{
  VCHIQ_MMAL_MSG_DECL(src->component->ms, PORT_ACTION, port_action_handle,
    port_action_reply);
  m->component_handle = src->component->handle;
  m->port_handle = src->handle;
  m->action = MMAL_MSG_PORT_ACTION_TYPE_CONNECT;
  m->connect_component_handle = dst->component->handle;
  m->connect_port_handle = dst->handle;
  VCHIQ_MMAL_MSG_COMMUNICATE_SYNC();
  return SUCCESS;
}

int mmal_port_enable(struct mmal_port *p)
{
  int err;

  if (p->enabled) {
    MODULE_INFO("skipping port already enabled");
    return SUCCESS;
  }
  err = mmal_port_action_port(p->component, p,
    MMAL_MSG_PORT_ACTION_TYPE_ENABLE);

  CHECK_ERR("port action type enable failed");

  err = mmal_port_info_get(p);
  CHECK_ERR("port action type enable failed");
  if (!p->enabled) {
    MODULE_ERR("port still not enabled");
    err = ERR_GENERIC;
  }
out_err:
  return err;
}

static __attribute__((unused)) int create_port_connection(
  struct mmal_port *in, struct mmal_port *out,
  struct mmal_port *out2)
{
  int err;
  out->format.encoding = in->format.encoding;
  out->format.encoding_variant = in->format.encoding_variant;
  out->es.video.width = in->es.video.width;
  out->es.video.height = in->es.video.height;
  out->es.video.crop.x = in->es.video.crop.x;
  out->es.video.crop.y = in->es.video.crop.y;
  out->es.video.crop.width = in->es.video.crop.width;
  out->es.video.crop.height = in->es.video.crop.height;
  out->es.video.frame_rate.num = in->es.video.frame_rate.num;
  out->es.video.frame_rate.den = in->es.video.frame_rate.den;
  err = mmal_port_set_format(out);
  CHECK_ERR("Failed to change destination port format");
  if (out->recommended_buffer.num != in->recommended_buffer.num) {
    MODULE_ERR("Buffer info mismatch on connection recommended num %d vs %d",
      out->recommended_buffer.num, in->recommended_buffer.num);
    return ERR_GENERIC;
  }

  if (out->recommended_buffer.size != in->recommended_buffer.size) {
    MODULE_ERR("Buffer info mismatch on connection recommended size %d vs %d",
      out->recommended_buffer.size, in->recommended_buffer.size);
    return ERR_GENERIC;
  }

  err = mmal_port_connect(in, out);
  CHECK_ERR("Failed to connect ports");

  err = mmal_port_enable(in);
  CHECK_ERR("Failed to enable source port");

out_err:
  return err;
}

int mmal_port_add_buffer(struct mmal_port *p, void *dma_buf,
  size_t dma_buf_size, uint32_t user_handle)
{
  int err;
  struct mmal_buffer *buf;

  buf = kzalloc(sizeof(*buf));
  if (!buf) {
    MODULE_ERR("Failed to allocate buffer");
    return ERR_RESOURCE;
  }

  buf->buffer = dma_buf;
  buf->buffer_size = dma_buf_size;
  // MODULE_INFO("--- buf:%p, buffer:%p, sz:(%d)", buf,  buf->buffer);
  buf->user_handle = user_handle;
  err = smem_import_dmabuf(buf->buffer, buf->buffer_size, &buf->vcsm_handle);
  CHECK_ERR("failed to import dmabuf");
  list_add_tail(&buf->list, &p->bufs.os_side_free);

out_err:
  return err;
}

static int mmal_port_alloc_buffers(struct mmal_port *p, size_t min_buffers)
{
  int err;
  size_t i;
  size_t num_buffers = p->current_buffer.num;
  size_t dma_buf_size = p->current_buffer.size;
  struct write_stream_buf *iobuf;
  void *dma_buf;

  if (num_buffers < min_buffers)
    num_buffers = min_buffers;

  MODULE_INFO("port %p (%s): allocating %d buffers (%d bytes) to port %s", p,
    p->name, num_buffers, p->minimum_buffer.size, p->name);

  for (i = 0; i < num_buffers; ++i) {
    dma_buf = dma_alloc(dma_buf_size, 0);
    if (!dma_buf) {
      MODULE_ERR("Failed to allocate dma buffer");
      return ERR_RESOURCE;
    }

    iobuf = kmalloc(sizeof(struct write_stream_buf));
    if (!iobuf) {
      MODULE_ERR("Failed to allocate iobuf");
      return ERR_RESOURCE;
    }

    INIT_LIST_HEAD(&iobuf->list);
    list_fifo_push_tail_locked(&p->iobufs, &iobuf->list);
    err = mmal_port_add_buffer(p, dma_buf, dma_buf_size, 0);
    CHECK_ERR("failed to import dmabuf");
  }

  p->bufs.total_count = num_buffers;
  return SUCCESS;

out_err:
  return err;
}

static __attribute__((unused)) void mmal_format_copy(
  struct mmal_es_format_local *to, const struct mmal_es_format_local *from)
{
  void *tmp = to->es;
  *to->es = *from->es;
  *to = *from;
  to->es = tmp;
  to->extradata_size = 0;
  to->extradata = NULL;
}

int mmal_port_init_buffers(struct mmal_port *p, size_t min_buffers)
{
  int err;

  err = mmal_port_alloc_buffers(p, min_buffers);
  p->bufs.acks_count = 0;
  CHECK_ERR("Failed to alloc buffers for port");
  err = mmal_port_buffers_to_remote(p, &p->bufs.os_side_free, true);
  CHECK_ERR("Failed to send buffers to port");
out_err:
  return err;
}

static int mmal_event_to_host_cb(const struct mmal_msg *rmsg)
{
  const struct mmal_msg_event_to_host *m = &rmsg->u.event_to_host;
  MODULE_INFO("event_to_host: comp:%d,port type:%05x,num:%d,cmd:%08x,length:%d",
    m->client_component, m->port_type, m->port_num, m->cmd, m->length);
  for (size_t i = 0; i < m->length; ++i) {
    printf(" %02x", m->data[i]);
    if (i & ((i % 8) == 0))
      printf("\r\n");
  }

  return SUCCESS;
}

static OPTIMIZED int mmal_service_cb(struct vchiq_service *s,
  struct vchiq_header *h)
{
  struct mmal_msg *rmsg;
  struct mmal_msg_context *msg_ctx;

  rmsg = (struct mmal_msg *)h->data;

  if (rmsg->h.type == MMAL_MSG_TYPE_BUFFER_TO_HOST)
    return mmal_buffer_to_host_cb(rmsg);

  if (rmsg->h.type == MMAL_MSG_TYPE_EVENT_TO_HOST)
    return mmal_event_to_host_cb(rmsg);

  msg_ctx = mmal_msg_context_from_handle(rmsg->h.context);

  if (msg_ctx->u.sync.msg_type != rmsg->h.type) {
    MODULE_ERR("mmal msg expected type %s, received: %s",
      mmal_msg_type_names[rmsg->h.type],
      mmal_msg_type_names[msg_ctx->u.sync.msg_type]);
    return ERR_INVAL;
  }

  msg_ctx->u.sync.rmsg = rmsg;
  msg_ctx->u.sync.rmsg_size = h->size;
  os_event_notify(&msg_ctx->u.sync.completion_waitflag);

  return SUCCESS;
}

static int mmal_msg_component_create(struct vchiq_service *mmal_service,
  const char *name, int component_idx, struct mmal_component *c)
{
  VCHIQ_MMAL_MSG_DECL(mmal_service, COMPONENT_CREATE, component_create,
    component_create_reply);

  m->client_component = component_idx;
  strncpy(m->name, name, sizeof(m->name));

  VCHIQ_MMAL_MSG_COMMUNICATE_SYNC();

  c->handle = r->component_handle;
  c->ms = mmal_service;
  c->enabled = true;
  c->inputs = r->input_num;
  c->outputs = r->output_num;
  c->clocks = r->clock_num;
  return SUCCESS;
}

static int mmal_msg_component_enable(struct mmal_component *c)
{
  VCHIQ_MMAL_MSG_DECL(c->ms, COMPONENT_ENABLE, component_enable,
    component_enable_reply);

  m->component_handle = c->handle;
  VCHIQ_MMAL_MSG_COMMUNICATE_SYNC();
  return SUCCESS;
}

static int mmal_port_create(struct mmal_component *c,
  struct mmal_port *p, enum mmal_port_type type, int index)
{
  int err;

  /* Type and index are needed for port info get */
  p->component = c;
  p->type = type;
  p->index = index;
  snprintf(p->name, sizeof(p->name), "%s.%d%d", c->name, (int)type, index);

  err = mmal_port_info_get(p);
  CHECK_ERR("Failed to get port info");

  INIT_LIST_HEAD(&p->iobufs);
  INIT_LIST_HEAD(&p->bufs.os_side_free);
  INIT_LIST_HEAD(&p->bufs.os_side_consumable);
  INIT_LIST_HEAD(&p->bufs.os_side_in_process);
  INIT_LIST_HEAD(&p->bufs.remote_side);

  return SUCCESS;
out_err:
  return err;
}

struct mmal_component *mmal_component_create(const char *name)
{
  int err, i;
  struct mmal_component *c;
  struct list_head *components_list;

  if (!service_mmal)
    return NULL;

  components_list = vchiq_get_components_list();

  c = kzalloc(sizeof(*c));
  if (!c) {
    MODULE_ERR("failed to allocate memory for mmal_component");
    return NULL;
  }

  if (mmal_msg_component_create(service_mmal, name, 0, c) != SUCCESS) {
    MODULE_ERR("Component not created '%s'", name);
    return NULL;
  }

  strncpy(c->name, name, sizeof(c->name));
  list_add_tail(&c->list, components_list);

  MODULE_INFO("vchiq component created name:%s: handle: %d, input: %d,"
    " output: %d, clock: %d", c->name, c->handle, c->inputs, c->outputs,
    c->clocks);

  err = mmal_port_create(c, &c->control, MMAL_PORT_TYPE_CONTROL, 0);
  if (err != SUCCESS)
    goto err_component_destroy;

  for (i = 0; i < c->inputs; ++i) {
    err = mmal_port_create(c, &c->input[i], MMAL_PORT_TYPE_INPUT, i);
    if (err != SUCCESS)
      goto err_component_destroy;
  }

  for (i = 0; i < c->outputs; ++i) {
    err = mmal_port_create(c, &c->output[i], MMAL_PORT_TYPE_OUTPUT, i);
    if (err != SUCCESS)
      goto err_component_destroy;
  }

  return c;
err_component_destroy:
  mmal_component_destroy(c);
  kfree(c);
  return NULL;
}

void mmal_port_buffer_consumed_isr(struct mmal_port *p, struct mmal_buffer *b)
{
  struct mmal_msgq_msg m = {
    .id = MMAL_IO_BUF_CONSUMED,
    .port = p,
    .buffer = b
  };

  os_msgq_put_isr(&mmal_io_msgq, &m);
}

static void mmal_io_loop(void)
{
  int err;

  struct mmal_msgq_msg m;

  while(1) {
    os_msgq_get(&mmal_io_msgq, &m);
    if (m.id == MMAL_IO_BUF_READY) {
      err = mmal_io_buffer_ready_cb(m.port, m.buffer);
      if (err != SUCCESS) {
        printf("PROBLEM\r\n");
      }
    }
    else if (m.id == MMAL_IO_BUF_CONSUMED) {
      err = mmal_port_buffer_to_remote(m.port, m.buffer);
      if (err != SUCCESS) {
        MODULE_ERR("Failed to submit buffer");
      }
    }
  }
}

int mmal_get_version(void)
{
  struct mmal_msg_context ctx = {
    .u.sync = {
      .msg_type = MMAL_MSG_TYPE_GET_VERSION,
      .completion_waitflag = EVENT_INIT,
      .rmsg = NULL
    }
  };

  struct mmal_msg msg = { 0 };

  if (!service_mmal)
    return ERR_INVAL;

  mmal_msg_fill_header(service_mmal, ctx.u.sync.msg_type, &msg, &ctx);

  vchiq_msg_prep(VCHIQ_MSG_DATA, service_mmal->localport,
    service_mmal->remoteport,
    &msg, sizeof(struct mmal_msg_header));

  vchiq_event_signal_trigger();

  os_event_wait(&ctx.u.sync.completion_waitflag);
  os_event_clear(&ctx.u.sync.completion_waitflag);
#if 0
  //r = mmal_msg_check_reply(ctx.u.sync.rmsg, ctx.u.sync.msg_type);
  if (!r) {
    MODULE_ERR("invalid reply");
    return ERR_GENERIC;
  }
  if (r->status != MMAL_MSG_STATUS_SUCCESS) {
    MODULE_ERR("status not success: %d", r->status);
    return ERR_GENERIC;
  }
#endif
  return SUCCESS;
}

int mmal_component_enable(struct mmal_component *c)
{
  int err;
  err = mmal_msg_component_enable(c);
  os_wait_ms(300);
  return err;
}

int mmal_component_disable(struct mmal_component *c)
{
  VCHIQ_MMAL_MSG_DECL(c->ms, COMPONENT_DISABLE, component_disable,
    component_disable_reply);

  BUG_IF(!c->enabled,
    "trying to disable mmal component, which is already disabled");
  m->component_handle = c->handle;

  VCHIQ_MMAL_MSG_COMMUNICATE_SYNC();
  c->enabled = false;
  MODULE_INFO("mmal_component_disable, name:%s, handle:%d",
    c->name, c->handle);
  return SUCCESS;
}

int mmal_component_destroy(struct mmal_component *c)
{
  VCHIQ_MMAL_MSG_DECL(c->ms, COMPONENT_DESTROY, component_destroy,
    component_destroy_reply);

  BUG_IF(c->enabled,
    "trying to destroy mmal component, which is not disabled first");
  m->component_handle = c->handle;

  VCHIQ_MMAL_MSG_COMMUNICATE_SYNC();
  MODULE_INFO("vchiq_mmal_handmade_component_destroy, name:%s, handle:%d",
    c->name, c->handle);
  kfree(c);
  return SUCCESS;
}

int mmal_init(void)
{
  struct task *t;
  uint32_t service_id = VCHIQ_SERVICE_NAME_TO_ID("mmal");

  os_msgq_init(&mmal_io_msgq, mmal_io_msgq_buffer,
    sizeof(struct mmal_msgq_msg), MMAL_IO_MSGQ_MAX_COUNT);

  service_mmal = vchiq_service_open(service_id, VC_MMAL_MIN_VER, VC_MMAL_VER,
    mmal_service_cb);

  if (!service_mmal) {
    MODULE_ERR("failed to open mmal service");
    return ERR_GENERIC;
  }

  t = task_create(mmal_io_loop, "mmal_io_loop");
  if (!t) {
    MODULE_ERR("Failed to start vchiq_thread");
    return ERR_GENERIC;
  }

  os_schedule_task(t);

  return SUCCESS;
}

void mmal_register_io_cb(mmal_io_buffer_ready_cb_t cb)
{
  mmal_io_buffer_ready_cb = cb;
}
