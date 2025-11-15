#pragma once

#define MMAL_FORMAT_EXTRADATA_MAX_SIZE 128
#define MMAL_MSG_MAX_PAYLOAD 488
#define MMAL_TIME_UNKNOWN (1ull << 63)

/* Size of space reserved in a buffer message for short messages. */
#define MMAL_VC_SHORT_DATA 128

/* Signals that the current payload is the end of the stream of data */
#define MMAL_BUFFER_HEADER_FLAG_EOS                    BIT(0)
/* Signals that the start of the current payload starts a frame */
#define MMAL_BUFFER_HEADER_FLAG_FRAME_START            BIT(1)
/* Signals that the end of the current payload ends a frame */
#define MMAL_BUFFER_HEADER_FLAG_FRAME_END              BIT(2)
/* Signals that the current payload contains only complete frames (>1) */
#define MMAL_BUFFER_HEADER_FLAG_FRAME                  \
  (MMAL_BUFFER_HEADER_FLAG_FRAME_START | \
   MMAL_BUFFER_HEADER_FLAG_FRAME_END)
/* Signals that the current payload is a keyframe (i.e. self decodable) */
#define MMAL_BUFFER_HEADER_FLAG_KEYFRAME               BIT(3)
/*
 * Signals a discontinuity in the stream of data (e.g. after a seek).
 * Can be used for instance by a decoder to reset its state
 */
#define MMAL_BUFFER_HEADER_FLAG_DISCONTINUITY          BIT(4)
/*
 * Signals a buffer containing some kind of config data for the component
 * (e.g. codec config data)
 */
#define MMAL_BUFFER_HEADER_FLAG_CONFIG                 BIT(5)
/* Signals an encrypted payload */
#define MMAL_BUFFER_HEADER_FLAG_ENCRYPTED              BIT(6)
/* Signals a buffer containing side information */
#define MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO          BIT(7)
/*
 * Signals a buffer which is the snapshot/postview image from a stills
 * capture
 */
#define MMAL_BUFFER_HEADER_FLAG_SNAPSHOT               BIT(8)
/* Signals a buffer which contains data known to be corrupted */
#define MMAL_BUFFER_HEADER_FLAG_CORRUPTED              BIT(9)
/* Signals that a buffer failed to be transmitted */
#define MMAL_BUFFER_HEADER_FLAG_TRANSMISSION_FAILED    BIT(10)
/** Signals the output buffer won't be used, just update reference frames */
#define MMAL_BUFFER_HEADER_FLAG_DECODEONLY             (1<<11)
/** Signals that the end of the current payload ends a NAL */
#define MMAL_BUFFER_HEADER_FLAG_NAL_END                (1<<12)

#define MMAL_WORKER_PORT_PARAMETER_SPACE      96

/* event messages */
#define MMAL_WORKER_EVENT_SPACE 256

/* Four CC's for events */
#define MMAL_FOURCC(a, b, c, d) ((a) | (b << 8) | (c << 16) | (d << 24))

#define MMAL_EVENT_ERROR    MMAL_FOURCC('E', 'R', 'R', 'O')
#define MMAL_EVENT_EOS      MMAL_FOURCC('E', 'E', 'O', 'S')
#define MMAL_EVENT_FORMAT_CHANGED  MMAL_FOURCC('E', 'F', 'C', 'H')
#define MMAL_EVENT_PARAMETER_CHANGED  MMAL_FOURCC('E', 'P', 'C', 'H')

enum mmal_port_type {
  MMAL_PORT_TYPE_UNKNOWN = 0,
  MMAL_PORT_TYPE_CONTROL,
  MMAL_PORT_TYPE_INPUT,
  MMAL_PORT_TYPE_OUTPUT,
  MMAL_PORT_TYPE_CLOCK,
};

enum mmal_msg_type {
  MMAL_MSG_TYPE_QUIT = 1,
  MMAL_MSG_TYPE_SERVICE_CLOSED,
  MMAL_MSG_TYPE_GET_VERSION,
  MMAL_MSG_TYPE_COMPONENT_CREATE,
  MMAL_MSG_TYPE_COMPONENT_DESTROY,
  MMAL_MSG_TYPE_COMPONENT_ENABLE,
  MMAL_MSG_TYPE_COMPONENT_DISABLE,
  MMAL_MSG_TYPE_PORT_INFO_GET,
  MMAL_MSG_TYPE_PORT_INFO_SET,
  MMAL_MSG_TYPE_PORT_ACTION,
  MMAL_MSG_TYPE_BUFFER_FROM_HOST,
  MMAL_MSG_TYPE_BUFFER_TO_HOST,
  MMAL_MSG_TYPE_GET_STATS,
  MMAL_MSG_TYPE_PORT_PARAMETER_SET,
  MMAL_MSG_TYPE_PORT_PARAMETER_GET,
  MMAL_MSG_TYPE_EVENT_TO_HOST,
  MMAL_MSG_TYPE_GET_CORE_STATS_FOR_PORT,
  MMAL_MSG_TYPE_OPAQUE_ALLOCATOR,
  MMAL_MSG_TYPE_CONSUME_MEM,
  MMAL_MSG_TYPE_LMK,
  MMAL_MSG_TYPE_OPAQUE_ALLOCATOR_DESC,
  MMAL_MSG_TYPE_DRM_GET_LHS32,
  MMAL_MSG_TYPE_DRM_GET_TIME,
  MMAL_MSG_TYPE_BUFFER_FROM_HOST_ZEROLEN,
  MMAL_MSG_TYPE_PORT_FLUSH,
  MMAL_MSG_TYPE_HOST_LOG,
  MMAL_MSG_TYPE_MSG_LAST
};

enum mmal_msg_port_action_type {
  MMAL_MSG_PORT_ACTION_TYPE_UNKNOWN = 0,
  MMAL_MSG_PORT_ACTION_TYPE_ENABLE,
  MMAL_MSG_PORT_ACTION_TYPE_DISABLE,
  MMAL_MSG_PORT_ACTION_TYPE_FLUSH,
  MMAL_MSG_PORT_ACTION_TYPE_CONNECT,
  MMAL_MSG_PORT_ACTION_TYPE_DISCONNECT,
  MMAL_MSG_PORT_ACTION_TYPE_SET_REQUIREMENTS,
};

struct mmal_rect {
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;
};

struct mmal_rational {
  /* Numerator */
  int32_t num;
  /* Denominator */
  int32_t den;
};

struct mmal_audio_format {
  /* Number of audio channels */
  uint32_t channels;

  /* Sample rate */
  uint32_t sample_rate;

  /* Bits per sample */
  uint32_t bits_per_sample;

  /* Size of a block of data */
  uint32_t block_align;
};

struct mmal_video_format {
  /* Width of frame in pixels */
  uint32_t width;

  /* Height of frame in rows of pixels */
  uint32_t height;

  /* Visible region of the frame */
  struct mmal_rect crop;

  /* Frame rate */
  struct mmal_rational frame_rate;

  /* Pixel aspect ratio */
  struct mmal_rational par;

  /*
   * FourCC specifying the color space of the video stream. See the
   * MmalColorSpace "pre-defined color spaces" for some examples.
   */
  uint32_t color_space;
};

struct mmal_subpicture_format {
  uint32_t x_offset;
  uint32_t y_offset;
};

union mmal_es_specific_format {
  struct mmal_audio_format audio;
  struct mmal_video_format video;
  struct mmal_subpicture_format subpicture;
};

struct mmal_port_buffer {
  /* number of buffers */
  unsigned int num;
  /* size of buffers */
  uint32_t size;
  /* alignment of buffers */
  uint32_t alignment;
};

struct mmal_es_format_local {
  /* enum mmal_es_type */
  uint32_t type;
  /* FourCC specifying encoding of the elementary stream.  */
  uint32_t encoding;

  /*
   * FourCC specifying the specific encoding variant of the elementary
   * stream.
   */
  uint32_t encoding_variant;

  /* Type specifiv information for the elementary stream */
  union mmal_es_specific_format *es;

  /* Bitrate in bits per second */
  uint32_t bitrate;

  /* Flags describing properties of the elementary stream.  */
  uint32_t flags;

  /* Size of the codec specific data */
  uint32_t extradata_size;

  /* Codec specific data */
  uint8_t *extradata;
};

struct mmal_es_format {
  /* enum mmal_es_type */
  uint32_t type;

  /* FourCC specifying encoding of the elementary stream.  */
  uint32_t encoding;
  /* FourCC specifying the specific encoding variant of the elementary stream.*/
  uint32_t encoding_variant;

  /* Type specific information for the elementary stream */
  uint32_t es;

  /* Bitrate in bits per second */
  uint32_t bitrate;

  /* Flags describing properties of the elementary stream.  */
  uint32_t flags;

  /* Size of the codec specific data */
  uint32_t extradata_size;
  /* Codec specific data */
  uint32_t extradata;
};

struct mmal_msg_header {
  uint32_t magic;
  /* enum mmal_msg_type */
  uint32_t type;

  /* Opaque handle to the control service */
  uint32_t control_service;

  uint32_t context;  /* a uint32_t per message context */
  uint32_t status;  /* The status of the vchiq operation */
  uint32_t padding;
};

/* Send from VC to host to report version */
struct mmal_msg_version {
  uint32_t flags;
  uint32_t major;
  uint32_t minor;
  uint32_t minimum;
};

/* request to VC to create component */
struct mmal_msg_component_create {
  uint32_t client_component;  /* component context */
  char name[128];
  uint32_t pid;    /* For debug */
};

/* reply from VC to component creation request */
struct mmal_msg_component_create_reply {
  uint32_t status;  /* enum mmal_msg_status - how does this differ to
       * the one in the header?
       */
  uint32_t component_handle; /* VideoCore handle for component */
  uint32_t input_num;        /* Number of input ports */
  uint32_t output_num;       /* Number of output ports */
  uint32_t clock_num;        /* Number of clock ports */
};

/* request to VC to destroy a component */
struct mmal_msg_component_destroy {
  uint32_t component_handle;
};

struct mmal_msg_component_destroy_reply {
  uint32_t status; /* The component destruction status */
};

/* request and reply to VC to enable a component */
struct mmal_msg_component_enable {
  uint32_t component_handle;
};

struct mmal_msg_component_enable_reply {
  uint32_t status; /* The component enable status */
};

/* request and reply to VC to disable a component */
struct mmal_msg_component_disable {
  uint32_t component_handle;
};

struct mmal_msg_component_disable_reply {
  uint32_t status; /* The component disable status */
};

/* request to VC to get port information */
struct mmal_msg_port_info_get {
  uint32_t component_handle;  /* component handle port is associated with */
  uint32_t port_type;         /* enum mmal_msg_port_type */
  uint32_t index;             /* port index to query */
};

struct mmal_port_msg {
  uint32_t priv;  /* Private member used by the framework */
  uint32_t name;  /* Port name. Used for debugging purposes (RO) */

  uint32_t type;  /* Type of the port (RO) enum mmal_port_type */
  uint16_t index;  /* Index of the port in its type list (RO) */
  uint16_t index_all;  /* Index of the port in the list of all ports (RO) */

  uint32_t is_enabled;  /* Indicates whether the port is enabled or not (RO) */
  uint32_t format;  /* Format of the elementary stream */

  uint32_t buffer_num_min;  /* Minimum number of buffers the port
         *   requires (RO).  This is set by the
         *   component.
         */

  uint32_t buffer_size_min;  /* Minimum size of buffers the port
         * requires (RO).  This is set by the
         * component.
         */

  uint32_t buffer_alignment_min;/* Minimum alignment requirement for
          * the buffers (RO).  A value of
          * zero means no special alignment
          * requirements.  This is set by the
          * component.
          */

  uint32_t buffer_num_recommended;  /* Number of buffers the port
           * recommends for optimal
           * performance (RO).  A value of
           * zero means no special
           * recommendation.  This is set
           * by the component.
           */

  uint32_t buffer_size_recommended;  /* Size of buffers the port
           * recommends for optimal
           * performance (RO).  A value of
           * zero means no special
           * recommendation.  This is set
           * by the component.
           */

  uint32_t buffer_num;  /* Actual number of buffers the port will use.
       * This is set by the client.
       */

  uint32_t buffer_size; /* Actual maximum size of the buffers that
        * will be sent to the port. This is set by
        * the client.
        */

  uint32_t component;  /* Component this port belongs to (Read Only) */

  uint32_t userdata;  /* Field reserved for use by the client */

  uint32_t capabilities;  /* Flags describing the capabilities of a
         * port (RO).  Bitwise combination of \ref
         * portcapabilities "Port capabilities"
         * values.
         */
};


/* reply from VC to get port info request */
struct mmal_msg_port_info_get_reply {
  uint32_t status;    /* enum mmal_msg_status */
  uint32_t component_handle;  /* component handle port is associated with */
  uint32_t port_type;    /* enum mmal_msg_port_type */
  uint32_t port_index;    /* port indexed in query */
  int32_t found;    /* unused */
  uint32_t port_handle;  /* Handle to use for this port */
  struct mmal_port_msg port;
  struct mmal_es_format format; /* elementary stream format */
  union mmal_es_specific_format es; /* es type specific data */
  uint8_t extradata[MMAL_FORMAT_EXTRADATA_MAX_SIZE]; /* es extra data */
};

/* request to VC to set port information */
struct mmal_msg_port_info_set {
  uint32_t component_handle;
  uint32_t port_type;    /* enum mmal_msg_port_type */
  uint32_t port_index;    /* port indexed in query */
  struct mmal_port_msg port;
  struct mmal_es_format format;
  union mmal_es_specific_format es;
  uint8_t extradata[MMAL_FORMAT_EXTRADATA_MAX_SIZE];
};

/* reply from VC to port info set request */
struct mmal_msg_port_info_set_reply {
  uint32_t status;
  uint32_t component_handle;  /* component handle port is associated with */
  uint32_t port_type;    /* enum mmal_msg_port_type */
  uint32_t index;    /* port indexed in query */
  int32_t found;    /* unused */
  uint32_t port_handle;  /* Handle to use for this port */
  struct mmal_port_msg port;
  struct mmal_es_format format;
  union mmal_es_specific_format es;
  uint8_t extradata[MMAL_FORMAT_EXTRADATA_MAX_SIZE];
};

/* port action requests that take a mmal_port_msg as a parameter */
struct mmal_msg_port_action_port {
  uint32_t component_handle;
  uint32_t port_handle;
  uint32_t action;    /* enum mmal_msg_port_action_type */
  struct mmal_port_msg port;
};

/* port action requests that take handles as a parameter */
struct mmal_msg_port_action_handle {
  uint32_t component_handle;
  uint32_t port_handle;
  uint32_t action;    /* enum mmal_msg_port_action_type */
  uint32_t connect_component_handle;
  uint32_t connect_port_handle;
};

struct mmal_msg_port_action_reply {
  uint32_t status;  /* The port action operation status */
};

struct mmal_driver_buffer {
  uint32_t magic;
  uint32_t component_handle;
  uint32_t port_handle;
  uint32_t client_context;
};

/* buffer header */
struct mmal_buffer_header {
  uint32_t next;  /* next header */
  uint32_t priv;  /* framework private data */
  uint32_t cmd;
  uint32_t data;
  uint32_t alloc_size;
  uint32_t length;
  uint32_t offset;
  uint32_t flags;
  int64_t pts;
  int64_t dts;
  uint32_t type;
  uint32_t user_data;
};

struct mmal_buffer_header_type_specific {
  union {
    struct {
    uint32_t planes;
    uint32_t offset[4];
    uint32_t pitch[4];
    uint32_t flags;
    } video;
  } u;
};

struct mmal_msg_buffer_from_host {
  /*
   *The front 32 bytes of the buffer header are copied
   * back to us in the reply to allow for context. This
   * area is used to store two mmal_driver_buffer structures to
   * allow for multiple concurrent service users.
   */
  /* control data */
  struct mmal_driver_buffer drvbuf;

  /* referenced control data for passthrough buffer management */
  struct mmal_driver_buffer drvbuf_ref;
  struct mmal_buffer_header buffer_header; /* buffer header itself */
  struct mmal_buffer_header_type_specific buffer_header_type_specific;
  int32_t is_zero_copy;
  int32_t has_reference;

  /* allows short data to be xfered in control message */
  uint32_t payload_in_message;
  uint8_t short_data[MMAL_VC_SHORT_DATA];
};

/* port parameter setting */

struct mmal_msg_port_parameter_set {
  uint32_t component_handle;  /* component */
  uint32_t port_handle;  /* port */
  uint32_t id;      /* Parameter ID  */
  uint32_t size;    /* Parameter size */
  uint32_t value[MMAL_WORKER_PORT_PARAMETER_SPACE];
};

struct mmal_msg_port_parameter_set_reply {
  uint32_t status;  /* enum mmal_msg_status todo: how does this
       * differ to the one in the header?
       */
};

/* port parameter getting */

struct mmal_msg_port_parameter_get {
  uint32_t component_handle;  /* component */
  uint32_t port_handle;  /* port */
  uint32_t id;      /* Parameter ID  */
  uint32_t size;    /* Parameter size */
};

struct mmal_msg_port_parameter_get_reply {
  uint32_t status;    /* Status of mmal_port_parameter_get call */
  uint32_t id;      /* Parameter ID  */
  uint32_t size;    /* Parameter size */
  uint32_t value[MMAL_WORKER_PORT_PARAMETER_SPACE];
};

/* Structs for each of the event message payloads */
struct mmal_msg_event_eos {
  uint32_t port_type;  /**< Type of port that received the end of stream */
  uint32_t port_index;  /**< Index of port that received the end of stream */
};

/** Format changed event data. */
struct mmal_msg_event_format_changed {
  /* Minimum size of buffers the port requires */
  uint32_t buffer_size_min;
  /* Minimum number of buffers the port requires */
  uint32_t buffer_num_min;
  /* Size of buffers the port recommends for optimal performance.
   * A value of zero means no special recommendation.
   */
  uint32_t buffer_size_recommended;
  /* Number of buffers the port recommends for optimal
   * performance. A value of zero means no special recommendation.
   */
  uint32_t buffer_num_recommended;

  uint32_t es_ptr;
  struct mmal_es_format format;
  union mmal_es_specific_format es;
  uint8_t extradata[MMAL_FORMAT_EXTRADATA_MAX_SIZE];
};

struct mmal_msg_event_to_host {
  uint32_t client_component;  /* component context */

  uint32_t port_type;
  uint32_t port_num;

  uint32_t cmd;
  uint32_t length;
  uint8_t data[MMAL_WORKER_EVENT_SPACE];
  uint32_t delayed_buffer;
};

/* all mmal messages are serialised through this structure */
struct mmal_msg {
  /* header */
  struct mmal_msg_header h;
  /* payload */
  union {
    struct mmal_msg_version version;

    struct mmal_msg_component_create component_create;
    struct mmal_msg_component_create_reply component_create_reply;

    struct mmal_msg_component_destroy component_destroy;
    struct mmal_msg_component_destroy_reply component_destroy_reply;

    struct mmal_msg_component_enable component_enable;
    struct mmal_msg_component_enable_reply component_enable_reply;

    struct mmal_msg_component_disable component_disable;
    struct mmal_msg_component_disable_reply component_disable_reply;

    struct mmal_msg_port_info_get port_info_get;
    struct mmal_msg_port_info_get_reply port_info_get_reply;

    struct mmal_msg_port_info_set port_info_set;
    struct mmal_msg_port_info_set_reply port_info_set_reply;

    struct mmal_msg_port_action_port port_action_port;
    struct mmal_msg_port_action_handle port_action_handle;
    struct mmal_msg_port_action_reply port_action_reply;

    struct mmal_msg_buffer_from_host buffer_from_host;

    struct mmal_msg_port_parameter_set port_parameter_set;
    struct mmal_msg_port_parameter_set_reply
      port_parameter_set_reply;
    struct mmal_msg_port_parameter_get
      port_parameter_get;
    struct mmal_msg_port_parameter_get_reply
      port_parameter_get_reply;

    struct mmal_msg_event_to_host event_to_host;

    uint8_t payload[MMAL_MSG_MAX_PAYLOAD];
  } u;
};
