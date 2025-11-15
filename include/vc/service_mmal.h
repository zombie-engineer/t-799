#pragma once
#include <list.h>
#include <stdint.h>
#include <vc/service_mmal_protocol.h>

#define MMAL_COMPONENT_MAX_PORTS 4

enum mmal_msg_status {
  MMAL_MSG_STATUS_SUCCESS = 0, /* Success */
  MMAL_MSG_STATUS_ENOMEM,      /* Out of memory */
  MMAL_MSG_STATUS_ENOSPC,      /* Out of resources other than memory */
  MMAL_MSG_STATUS_EINVAL,      /* Argument is invalid */
  MMAL_MSG_STATUS_ENOSYS,      /* Function not implemented */
  MMAL_MSG_STATUS_ENOENT,      /* No such file or directory */
  MMAL_MSG_STATUS_ENXIO,       /* No such device or address */
  MMAL_MSG_STATUS_EIO,         /* I/O error */
  MMAL_MSG_STATUS_ESPIPE,      /* Illegal seek */
  MMAL_MSG_STATUS_ECORRUPT,    /* Data is corrupt \attention */
  MMAL_MSG_STATUS_ENOTREADY,   /* Component is not ready */
  MMAL_MSG_STATUS_ECONFIG,     /* Component is not configured */
  MMAL_MSG_STATUS_EISCONN,     /* Port is already connected */
  MMAL_MSG_STATUS_ENOTCONN,    /* Port is disconnected */
  MMAL_MSG_STATUS_EAGAIN,      /* Resource temporarily unavailable. */
  MMAL_MSG_STATUS_EFAULT,      /* Bad address */
};

/* buffer for a signle video frame */
struct mmal_buffer {
  /* list of buffers available */
  struct list_head list;

  /* raw data */
  void *buffer;

  /* size of allocated buffer */
  unsigned long buffer_size;

  /* VCSM handle having imported the dmabuf */
  uint32_t vcsm_handle;

  uint32_t user_handle;

  unsigned long length;
  uint32_t flags;
  int64_t dts;
  int64_t pts;
};


struct mmal_port {
  char name[32];
  uint32_t enabled : 1;
  uint32_t zero_copy : 1;
  uint32_t handle;
  /* port type, cached to use on port info set */
  uint32_t type;
  /* port index, cached to use on port info set */
  uint32_t index;

  /* component port belongs to, allows simple deref */
  struct mmal_component *component;

  /* port conencted to */
  struct mmal_port *connected;

  /* buffer info */
  struct mmal_port_buffer minimum_buffer;
  struct mmal_port_buffer recommended_buffer;
  struct mmal_port_buffer current_buffer;

  /* stream format */
  struct mmal_es_format_local format;

  /* elementary stream format */
  union mmal_es_specific_format es;

  /*
   * Preallocated free buffers on our side, they are not used but if required
   * we can release them to the remote side (vcos)
   */
  struct list_head buffers_free;
  size_t nr_free;

  /*
   * Buffers that are released to remote side (vcos). At some point they will
   * be sent to us by BUFFER_TO_HOST message
   */
  struct list_head buffers_busy;
  size_t nr_busy;

  struct list_head buffers_pending;
  size_t nr_pending;

  /*
   * Buffers that are send to use by BUFFER_TO_HOST and are currently being
   * processed by us. After processing is done these will be sent to
   * buffers_free
   */
  struct list_head buffers_in_process;
  size_t nr_in_process;


  /* callback context */
  void *cb_ctx;
  struct list_head iobufs;
};

struct mmal_component {
  struct list_head list;
  char name[32];

  uint32_t in_use : 1;
  uint32_t enabled : 1;
  /* VideoCore handle for component */
  uint32_t handle;
  /* Number of input ports */
  uint32_t inputs;
  /* Number of output ports */
  uint32_t outputs;
  /* Number of clock ports */
  uint32_t clocks;
  /* control port */
  struct mmal_port control;
  /* input ports */
  struct mmal_port input[MMAL_COMPONENT_MAX_PORTS];
  /* output ports */
  struct mmal_port output[MMAL_COMPONENT_MAX_PORTS];
  /* clock ports */
  struct mmal_port clock[MMAL_COMPONENT_MAX_PORTS];
  /* Used to ref back to client struct */
  uint32_t client_component;
  struct vchiq_service *ms;
};

int mmal_init(void);

int mmal_get_version(void);

int mmal_component_enable(struct mmal_component *c);
int mmal_component_disable(struct mmal_component *c);
int mmal_component_destroy(struct mmal_component *c);
struct mmal_component *mmal_component_create(const char *name);

int mmal_port_enable(struct mmal_port *p);
int mmal_port_set_format(struct mmal_port *p);
int mmal_port_apply_buffers(struct mmal_port *p, size_t min_buffers);
int mmal_port_connect(struct mmal_port *src, struct mmal_port *dst);
int mmal_port_set_zero_copy(struct mmal_port *p);
int mmal_port_get_supp_encodings(struct mmal_port *p, uint32_t *encodings,
  int max_encodings, int *num_encodings);
int mmal_port_buffer_submit_list(struct mmal_port *p, struct list_head *l);
int mmal_port_parameter_set(struct mmal_port *p,
  uint32_t parameter_id, const void *value, int value_size);
int mmal_port_parameter_get(struct mmal_port *p, int parameter_id, void *value,
  uint32_t *value_size);
void mmal_port_dump(const char *tag, const struct mmal_port *p);

void mmal_register_io_cb(void (*cb)(void));

void mmal_format_set(struct mmal_es_format_local *f, int encoding,
  int encoding_variant, int width, int height, int frame_rate, int bitrate);

