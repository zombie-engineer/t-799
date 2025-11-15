#pragma once
#include <stdint.h>

#define MMAL_PARAM_CAM_INFO_MAX_CAMERAS 4
#define MMAL_PARAM_CAM_INFO_MAX_FLASHES 2
#define MMAL_PARAM_CAM_INFO_MAX_STR_LEN 16

#define MMAL_MAX_IMAGEFX_PARAMETERS 5

/* Common parameter ID group, used with many types of component. */
#define MMAL_PARAM_GROUP_COMMON    (0 << 16)

/* Camera-specific parameter ID group. */
#define MMAL_PARAM_GROUP_CAMERA    (1 << 16)

/* Video-specific parameter ID group. */
#define MMAL_PARAM_GROUP_VIDEO    (2 << 16)

/* Audio-specific parameter ID group. */
#define MMAL_PARAM_GROUP_AUDIO    (3 << 16)

/* Clock-specific parameter ID group. */
#define MMAL_PARAM_GROUP_CLOCK    (4 << 16)

/* Miracast-specific parameter ID group. */
#define MMAL_PARAM_GROUP_MIRACAST    (5 << 16)

typedef enum {
  /* Always timestamp frames as 0 */
  MMAL_PARAM_TIMESTAMP_MODE_ZERO = 0,
  /* Use the raw STC value for the frame timestamp */
  MMAL_PARAM_TIMESTAMP_MODE_RAW_STC,
  /*
   * Use the STC timestamp but subtract the timestamp of the first frame sent
   * to give a zero based timestamp.
   */
  MMAL_PARAM_TIMESTAMP_MODE_RESET_STC,
} mmal_param_timestamp_mode_t;

enum mmal_param_mirror {
  MMAL_PARAM_MIRROR_NONE,
  MMAL_PARAM_MIRROR_VERTICAL,
  MMAL_PARAM_MIRROR_HORIZONTAL,
  MMAL_PARAM_MIRROR_BOTH,
};

enum mmal_param_displaytransform {
  MMAL_DISPLAY_ROT0 = 0,
  MMAL_DISPLAY_MIRROR_ROT0 = 1,
  MMAL_DISPLAY_MIRROR_ROT180 = 2,
  MMAL_DISPLAY_ROT180 = 3,
  MMAL_DISPLAY_MIRROR_ROT90 = 4,
  MMAL_DISPLAY_ROT270 = 5,
  MMAL_DISPLAY_ROT90 = 6,
  MMAL_DISPLAY_MIRROR_ROT270 = 7,
};

enum mmal_param_displaymode {
  MMAL_DISPLAY_MODE_FILL = 0,
  MMAL_DISPLAY_MODE_LETTERBOX = 1,
};

enum mmal_param_displayset {
  MMAL_DISPLAY_SET_NONE = 0,
  MMAL_DISPLAY_SET_NUM = 1,
  MMAL_DISPLAY_SET_FULLSCREEN = 2,
  MMAL_DISPLAY_SET_TRANSFORM = 4,
  MMAL_DISPLAY_SET_DEST_RECT = 8,
  MMAL_DISPLAY_SET_SRC_RECT = 0x10,
  MMAL_DISPLAY_SET_MODE = 0x20,
  MMAL_DISPLAY_SET_PIXEL = 0x40,
  MMAL_DISPLAY_SET_NOASPECT = 0x80,
  MMAL_DISPLAY_SET_LAYER = 0x100,
  MMAL_DISPLAY_SET_COPYPROTECT = 0x200,
  MMAL_DISPLAY_SET_ALPHA = 0x400,
};

enum mmal_video_profile {
  MMAL_VIDEO_PROFILE_H263_BASELINE,
  MMAL_VIDEO_PROFILE_H263_H320CODING,
  MMAL_VIDEO_PROFILE_H263_BACKWARDCOMPATIBLE,
  MMAL_VIDEO_PROFILE_H263_ISWV2,
  MMAL_VIDEO_PROFILE_H263_ISWV3,
  MMAL_VIDEO_PROFILE_H263_HIGHCOMPRESSION,
  MMAL_VIDEO_PROFILE_H263_INTERNET,
  MMAL_VIDEO_PROFILE_H263_INTERLACE,
  MMAL_VIDEO_PROFILE_H263_HIGHLATENCY,
  MMAL_VIDEO_PROFILE_MP4V_SIMPLE,
  MMAL_VIDEO_PROFILE_MP4V_SIMPLESCALABLE,
  MMAL_VIDEO_PROFILE_MP4V_CORE,
  MMAL_VIDEO_PROFILE_MP4V_MAIN,
  MMAL_VIDEO_PROFILE_MP4V_NBIT,
  MMAL_VIDEO_PROFILE_MP4V_SCALABLETEXTURE,
  MMAL_VIDEO_PROFILE_MP4V_SIMPLEFACE,
  MMAL_VIDEO_PROFILE_MP4V_SIMPLEFBA,
  MMAL_VIDEO_PROFILE_MP4V_BASICANIMATED,
  MMAL_VIDEO_PROFILE_MP4V_HYBRID,
  MMAL_VIDEO_PROFILE_MP4V_ADVANCEDREALTIME,
  MMAL_VIDEO_PROFILE_MP4V_CORESCALABLE,
  MMAL_VIDEO_PROFILE_MP4V_ADVANCEDCODING,
  MMAL_VIDEO_PROFILE_MP4V_ADVANCEDCORE,
  MMAL_VIDEO_PROFILE_MP4V_ADVANCEDSCALABLE,
  MMAL_VIDEO_PROFILE_MP4V_ADVANCEDSIMPLE,
  MMAL_VIDEO_PROFILE_H264_BASELINE,
  MMAL_VIDEO_PROFILE_H264_MAIN,
  MMAL_VIDEO_PROFILE_H264_EXTENDED,
  MMAL_VIDEO_PROFILE_H264_HIGH,
  MMAL_VIDEO_PROFILE_H264_HIGH10,
  MMAL_VIDEO_PROFILE_H264_HIGH422,
  MMAL_VIDEO_PROFILE_H264_HIGH444,
  MMAL_VIDEO_PROFILE_H264_CONSTRAINED_BASELINE,
  MMAL_VIDEO_PROFILE_DUMMY = 0x7FFFFFFF
};

enum mmal_video_level {
  MMAL_VIDEO_LEVEL_H263_10,
  MMAL_VIDEO_LEVEL_H263_20,
  MMAL_VIDEO_LEVEL_H263_30,
  MMAL_VIDEO_LEVEL_H263_40,
  MMAL_VIDEO_LEVEL_H263_45,
  MMAL_VIDEO_LEVEL_H263_50,
  MMAL_VIDEO_LEVEL_H263_60,
  MMAL_VIDEO_LEVEL_H263_70,
  MMAL_VIDEO_LEVEL_MP4V_0,
  MMAL_VIDEO_LEVEL_MP4V_0b,
  MMAL_VIDEO_LEVEL_MP4V_1,
  MMAL_VIDEO_LEVEL_MP4V_2,
  MMAL_VIDEO_LEVEL_MP4V_3,
  MMAL_VIDEO_LEVEL_MP4V_4,
  MMAL_VIDEO_LEVEL_MP4V_4a,
  MMAL_VIDEO_LEVEL_MP4V_5,
  MMAL_VIDEO_LEVEL_MP4V_6,
  MMAL_VIDEO_LEVEL_H264_1,
  MMAL_VIDEO_LEVEL_H264_1b,
  MMAL_VIDEO_LEVEL_H264_11,
  MMAL_VIDEO_LEVEL_H264_12,
  MMAL_VIDEO_LEVEL_H264_13,
  MMAL_VIDEO_LEVEL_H264_2,
  MMAL_VIDEO_LEVEL_H264_21,
  MMAL_VIDEO_LEVEL_H264_22,
  MMAL_VIDEO_LEVEL_H264_3,
  MMAL_VIDEO_LEVEL_H264_31,
  MMAL_VIDEO_LEVEL_H264_32,
  MMAL_VIDEO_LEVEL_H264_4,
  MMAL_VIDEO_LEVEL_H264_41,
  MMAL_VIDEO_LEVEL_H264_42,
  MMAL_VIDEO_LEVEL_H264_5,
  MMAL_VIDEO_LEVEL_H264_51,
  MMAL_VIDEO_LEVEL_DUMMY = 0x7FFFFFFF
};

struct mmal_param_video_profile {
  enum mmal_video_profile profile;
  enum mmal_video_level level;
};

enum mmal_param_video_type {
  MMAL_PARAM_DISPLAYREGION = MMAL_PARAM_GROUP_VIDEO,
  MMAL_PARAM_SUPPORTED_PROFILES,
  MMAL_PARAM_PROFILE,
  MMAL_PARAM_INTRAPERIOD, /* uint32_t */
  MMAL_PARAM_RATECONTROL,
  MMAL_PARAM_NALUNITFORMAT,
  MMAL_PARAM_MINIMISE_FRAGMENTATION, /* bool */
  /* Setting the value to zero resets to the default (one slice per frame).*/
  MMAL_PARAM_MB_ROWS_PER_SLICE, /* uint32_t */
  MMAL_PARAM_VIDEO_LEVEL_EXTENSION,
  MMAL_PARAM_VIDEO_EEDE_ENABLE,
  MMAL_PARAM_VIDEO_EEDE_LOSSRATE,
  MMAL_PARAM_VIDEO_REQUEST_I_FRAME, /* bool */
  MMAL_PARAM_VIDEO_INTRA_REFRESH,
  MMAL_PARAM_VIDEO_IMMUTABLE_INPUT, /* bool */
  /* Run-time bit rate control */
  MMAL_PARAM_VIDEO_BIT_RATE, /* bool */
  MMAL_PARAM_VIDEO_FRAME_RATE,
  MMAL_PARAM_VIDEO_ENCODE_MIN_QUANT, /* uint32_t */
  MMAL_PARAM_VIDEO_ENCODE_MAX_QUANT, /* uint32_t */
  MMAL_PARAM_VIDEO_ENCODE_RC_MODEL,
  MMAL_PARAM_EXTRA_BUFFERS, /* uint32_t */
  /*
   * Changing this parameter from the default can reduce frame rate
   * because image buffers need to be re-pitched.
   */
  MMAL_PARAM_VIDEO_ALIGN_HORIZ, /* uint32_t */

  /*
   * Changing this parameter from the default can reduce frame rate
   * because image buffers need to be re-pitched.
   */
  MMAL_PARAM_VIDEO_ALIGN_VERT, /* uint32_t */
  MMAL_PARAM_VIDEO_DROPPABLE_PFRAMES, /* bool */
  MMAL_PARAM_VIDEO_ENCODE_INITIAL_QUANT, /* uint32_t */
  MMAL_PARAM_VIDEO_ENCODE_QP_P, /* uint32_t */
  MMAL_PARAM_VIDEO_ENCODE_RC_SLICE_DQUANT, /* uint32_t */
  MMAL_PARAM_VIDEO_ENCODE_FRAME_LIMIT_BITS, /* uint32_t */
  MMAL_PARAM_VIDEO_ENCODE_PEAK_RATE, /* uint32_t */
  /* H264 specific parameters */
  MMAL_PARAM_VIDEO_ENCODE_H264_DISABLE_CABAC, /* bool */
  MMAL_PARAM_VIDEO_ENCODE_H264_LOW_LATENCY,   /* bool */
  MMAL_PARAM_VIDEO_ENCODE_H264_AU_DELIMITERS, /* bool */
  MMAL_PARAM_VIDEO_ENCODE_H264_DEBLOCK_IDC,   /* uint32_t */
  MMAL_PARAM_VIDEO_ENCODE_H264_MB_INTRA_MODE,
  MMAL_PARAM_VIDEO_ENCODE_HEADER_ON_OPEN,     /* bool */
  MMAL_PARAM_VIDEO_ENCODE_PRECODE_FOR_QP,     /* bool */
  MMAL_PARAM_VIDEO_DRM_INIT_INFO,
  MMAL_PARAM_VIDEO_TIMESTAMP_FIFO,            /* bool */
  MMAL_PARAM_VIDEO_DECODE_ERROR_CONCEALMENT,  /* bool */
  MMAL_PARAM_VIDEO_DRM_PROTECT_BUFFER,
  MMAL_PARAM_VIDEO_DECODE_CONFIG_VD3,
  MMAL_PARAM_VIDEO_ENCODE_H264_VCL_HRD_PARAMETERS, /* bool */
  MMAL_PARAM_VIDEO_ENCODE_H264_LOW_DELAY_HRD_FLAG, /* bool */
  MMAL_PARAM_VIDEO_ENCODE_INLINE_HEADER,           /* bool */
  MMAL_PARAM_VIDEO_ENCODE_SEI_ENABLE,              /* bool */
  MMAL_PARAM_VIDEO_ENCODE_INLINE_VECTORS,          /* bool */
  MMAL_PARAM_VIDEO_RENDER_STATS,
  MMAL_PARAM_VIDEO_INTERLACE_TYPE,
  MMAL_PARAM_VIDEO_INTERPOLATE_TIMESTAMPS,         /* bool */
  MMAL_PARAM_VIDEO_ENCODE_SPS_TIMING,              /* bool */
  MMAL_PARAM_VIDEO_MAX_NUM_CALLBACKS,              /* uint32_t */
  MMAL_PARAM_VIDEO_SOURCE_PATTERN,
  MMAL_PARAM_VIDEO_ENCODE_SEPARATE_NAL_BUFS,       /* bool */
  MMAL_PARAM_VIDEO_DROPPABLE_PFRAME_LENGTH,        /* uint32_t */
};

enum mmal_param_camera_type {
  MMAL_PARAM_THUMBNAIL_CONFIGURATION = MMAL_PARAM_GROUP_CAMERA,
  MMAL_PARAM_CAPTURE_QUALITY,
  MMAL_PARAM_ROTATION,     /* int32_t */
  MMAL_PARAM_EXIF_DISABLE, /* bool */
  MMAL_PARAM_EXIF,
  MMAL_PARAM_AWB_MODE,
  MMAL_PARAM_IMAGE_EFFECT,
  MMAL_PARAM_COLOUR_EFFECT,
  MMAL_PARAM_FLICKER_AVOID,
  MMAL_PARAM_FLASH,
  MMAL_PARAM_REDEYE,
  MMAL_PARAM_FOCUS,
  MMAL_PARAM_FOCAL_LENGTHS, /* int32_t */
  MMAL_PARAM_EXPOSURE_COMP,
  MMAL_PARAM_ZOOM,
  MMAL_PARAM_MIRROR,
  MMAL_PARAM_CAMERA_NUM, /* uint32_t */
  MMAL_PARAM_CAPTURE, /* bool */
  MMAL_PARAM_EXPOSURE_MODE,
  MMAL_PARAM_EXP_METERING_MODE,
  MMAL_PARAM_FOCUS_STATUS,
  MMAL_PARAM_CAMERA_CONFIG,
  MMAL_PARAM_CAPTURE_STATUS,
  MMAL_PARAM_FACE_TRACK,
  MMAL_PARAM_DRAW_BOX_FACES_AND_FOCUS, /* bool */
  MMAL_PARAM_JPEG_Q_FACTOR,            /* uint32_t */
  MMAL_PARAM_FRAME_RATE,
  MMAL_PARAM_USE_STC,
  MMAL_PARAM_CAM_INFO,
  MMAL_PARAM_VIDEO_STABILISATION, /* bool */
  MMAL_PARAM_FACE_TRACK_RESULTS,
  MMAL_PARAM_ENABLE_RAW_CAPTURE, /* bool */
  MMAL_PARAM_DPF_FILE,
  MMAL_PARAM_ENABLE_DPF_FILE,   /* bool */
  MMAL_PARAM_DPF_FAIL_IS_FATAL, /* bool */
  MMAL_PARAM_CAPTURE_MODE,
  MMAL_PARAM_FOCUS_REGIONS,
  MMAL_PARAM_INPUT_CROP,
  MMAL_PARAM_SENSOR_INFORMATION,
  MMAL_PARAM_FLASH_SELECT,
  MMAL_PARAM_FIELD_OF_VIEW,
  MMAL_PARAM_HIGH_DYNAMIC_RANGE, /* bool */
  MMAL_PARAM_DYNAMIC_RANGE_COMPRESSION,
  MMAL_PARAM_ALGORITHM_CONTROL,
  MMAL_PARAM_SHARPNESS,  /* mmal_param_rational */
  MMAL_PARAM_CONTRAST,   /* mmal_param_rational */
  MMAL_PARAM_BRIGHTNESS, /* mmal_param_rational */
  MMAL_PARAM_SATURATION, /* mmal_param_rational */
  MMAL_PARAM_ISO,                         /* uint32_t */
  MMAL_PARAM_ANTISHAKE,                   /* bool */
  MMAL_PARAM_IMAGE_EFFECT_PARAMETERS,
  MMAL_PARAM_CAMERA_BURST_CAPTURE,        /* bool */
  MMAL_PARAM_CAMERA_MIN_ISO,              /* uint32_t */
  MMAL_PARAM_CAMERA_USE_CASE,
  MMAL_PARAM_CAPTURE_STATS_PASS,          /* bool */
  MMAL_PARAM_CAMERA_CUSTOM_SENSOR_CONFIG, /* uint32_t */
  MMAL_PARAM_ENABLE_REGISTER_FILE,        /* bool */
  MMAL_PARAM_REGISTER_FAIL_IS_FATAL,      /* bool */
  MMAL_PARAM_CONFIGFILE_REGISTERS,
  MMAL_PARAM_CONFIGFILE_CHUNK_REGISTERS,
  MMAL_PARAM_JPEG_ATTACH_LOG,       /* bool */
  MMAL_PARAM_ZERO_SHUTTER_LAG,
  MMAL_PARAM_FPS_RANGE,
  MMAL_PARAM_CAPTURE_EXPOSURE_COMP, /* int32_t */
  MMAL_PARAM_SW_SHARPEN_DISABLE,    /* bool */
  MMAL_PARAM_FLASH_REQUIRED,        /* bool */
  MMAL_PARAM_SW_SATURATION_DISABLE, /* bool */
  MMAL_PARAM_SHUTTER_SPEED,         /* uint32_t */
  MMAL_PARAM_CUSTOM_AWB_GAINS,
};

typedef enum {
  MMAL_PARAM_UNUSED = MMAL_PARAM_GROUP_COMMON,
  MMAL_PARAM_SUPPORTED_ENCODINGS,
  MMAL_PARAM_URI,
  MMAL_PARAM_CHANGE_EVENT_REQUEST,
  MMAL_PARAM_ZERO_COPY,       /* bool */
  MMAL_PARAM_BUFFER_REQUIREMENTS,
  MMAL_PARAM_STATISTICS,
  MMAL_PARAM_CORE_STATISTICS,
  MMAL_PARAM_MEM_USAGE,
  MMAL_PARAM_BUFFER_FLAG_FILTER, /* uint32_t */
  MMAL_PARAM_SEEK,
  MMAL_PARAM_POWERMON_ENABLE,  /* bool */
  MMAL_PARAM_LOGGING,          /* mmal_param_logging */
  MMAL_PARAM_SYSTEM_TIME,      /* uint64_t */
  MMAL_PARAM_NO_IMAGE_PADDING, /* bool */
} mmal_param_common_t;

struct mmal_param_rational {
  int32_t num;
  int32_t den;
};

struct mmal_param_rect {
  int32_t x;
  int32_t y;
  int32_t width;
  int32_t height;
};

struct mmal_param_displayregion {
  /* Bitfield that indicates which fields are set and should be
   * used. All other fields will maintain their current value.
   * \ref MMAL_DISPLAYSET_T defines the bits that can be
   * combined.
   */
  uint32_t set;

  /* Describes the display output device, with 0 typically
   * being a directly connected LCD display.  The actual values
   * will depend on the hardware.  Code using hard-wired numbers
   * (e.g. 2) is certain to fail.
   */

  uint32_t display_num;
  /* Indicates that we are using the full device screen area,
   * rather than a window of the display.  If zero, then
   * dest_rect is used to specify a region of the display to
   * use.
   */

  int32_t fullscreen;
  /* Indicates any rotation or flipping used to map frames onto
   * the natural display orientation.
   */
  uint32_t transform; /* enum mmal_param_displaytransform */

  /* Where to display the frame within the screen, if
   * fullscreen is zero.
   */
  struct mmal_param_rect dest_rect;

  /* Indicates which area of the frame to display. If all
   * values are zero, the whole frame will be used.
   */
  struct mmal_param_rect src_rect;

  /* If set to non-zero, indicates that any display scaling
   * should disregard the aspect ratio of the frame region being
   * displayed.
   */
  int32_t noaspect;

  /* Indicates how the image should be scaled to fit the
   * display. \code MMAL_DISPLAY_MODE_FILL \endcode indicates
   * that the image should fill the screen by potentially
   * cropping the frames.  Setting \code mode \endcode to \code
   * MMAL_DISPLAY_MODE_LETTERBOX \endcode indicates that all the
   * source region should be displayed and black bars added if
   * necessary.
   */
  uint32_t mode; /* enum mmal_param_displaymode */

  /* If non-zero, defines the width of a source pixel relative
   * to \code pixel_y \endcode.  If zero, then pixels default to
   * being square.
   */
  uint32_t pixel_x;

  /* If non-zero, defines the height of a source pixel relative
   * to \code pixel_x \endcode.  If zero, then pixels default to
   * being square.
   */
  uint32_t pixel_y;

  /* Sets the relative depth of the images, with greater values
   * being in front of smaller values.
   */
  uint32_t layer;

  /* Set to non-zero to ensure copy protection is used on
   * output.
   */
  int32_t copyprotect_required;

  /* Level of opacity of the layer, where zero is fully
   * transparent and 255 is fully opaque.
   */
  uint32_t alpha;
};

struct mmal_param_fps_range {
  /* Low end of the permitted framerate range */
  struct mmal_param_rational  fps_low;
  /* High end of the permitted framerate range */
  struct mmal_param_rational  fps_high;
};

/* camera configuration parameter */
enum mmal_param_exposuremode {
  MMAL_PARAM_EXPOSUREMODE_OFF,
  MMAL_PARAM_EXPOSUREMODE_AUTO,
  MMAL_PARAM_EXPOSUREMODE_NIGHT,
  MMAL_PARAM_EXPOSUREMODE_NIGHTPREVIEW,
  MMAL_PARAM_EXPOSUREMODE_BACKLIGHT,
  MMAL_PARAM_EXPOSUREMODE_SPOTLIGHT,
  MMAL_PARAM_EXPOSUREMODE_SPORTS,
  MMAL_PARAM_EXPOSUREMODE_SNOW,
  MMAL_PARAM_EXPOSUREMODE_BEACH,
  MMAL_PARAM_EXPOSUREMODE_VERYLONG,
  MMAL_PARAM_EXPOSUREMODE_FIXEDFPS,
  MMAL_PARAM_EXPOSUREMODE_ANTISHAKE,
  MMAL_PARAM_EXPOSUREMODE_FIREWORKS,
};

enum mmal_param_exposuremeteringmode {
  MMAL_PARAM_EXPOSUREMETERINGMODE_AVERAGE,
  MMAL_PARAM_EXPOSUREMETERINGMODE_SPOT,
  MMAL_PARAM_EXPOSUREMETERINGMODE_BACKLIT,
  MMAL_PARAM_EXPOSUREMETERINGMODE_MATRIX,
};

enum mmal_param_awbmode {
  MMAL_PARAM_AWBMODE_OFF,
  MMAL_PARAM_AWBMODE_AUTO,
  MMAL_PARAM_AWBMODE_SUNLIGHT,
  MMAL_PARAM_AWBMODE_CLOUDY,
  MMAL_PARAM_AWBMODE_SHADE,
  MMAL_PARAM_AWBMODE_TUNGSTEN,
  MMAL_PARAM_AWBMODE_FLUORESCENT,
  MMAL_PARAM_AWBMODE_INCANDESCENT,
  MMAL_PARAM_AWBMODE_FLASH,
  MMAL_PARAM_AWBMODE_HORIZON,
  MMAL_PARAM_AWBMODE_GREYWORLD,
};

enum mmal_param_imagefx_effect {
  MMAL_PARAM_IMAGEFX_NONE,
  MMAL_PARAM_IMAGEFX_NEGATIVE,
  MMAL_PARAM_IMAGEFX_SOLARIZE,
  MMAL_PARAM_IMAGEFX_POSTERIZE,
  MMAL_PARAM_IMAGEFX_WHITEBOARD,
  MMAL_PARAM_IMAGEFX_BLACKBOARD,
  MMAL_PARAM_IMAGEFX_SKETCH,
  MMAL_PARAM_IMAGEFX_DENOISE,
  MMAL_PARAM_IMAGEFX_EMBOSS,
  MMAL_PARAM_IMAGEFX_OILPAINT,
  MMAL_PARAM_IMAGEFX_HATCH,
  MMAL_PARAM_IMAGEFX_GPEN,
  MMAL_PARAM_IMAGEFX_PASTEL,
  MMAL_PARAM_IMAGEFX_WATERCOLOUR,
  MMAL_PARAM_IMAGEFX_FILM,
  MMAL_PARAM_IMAGEFX_BLUR,
  MMAL_PARAM_IMAGEFX_SATURATION,
  MMAL_PARAM_IMAGEFX_COLOURSWAP,
  MMAL_PARAM_IMAGEFX_WASHEDOUT,
  MMAL_PARAM_IMAGEFX_POSTERISE,
  MMAL_PARAM_IMAGEFX_COLOURPOINT,
  MMAL_PARAM_IMAGEFX_COLOURBALANCE,
  MMAL_PARAM_IMAGEFX_CARTOON,
};

enum MMAL_PARAM_FLICKERAVOID_T {
  MMAL_PARAM_FLICKERAVOID_OFF,
  MMAL_PARAM_FLICKERAVOID_AUTO,
  MMAL_PARAM_FLICKERAVOID_50HZ,
  MMAL_PARAM_FLICKERAVOID_60HZ,
  MMAL_PARAM_FLICKERAVOID_MAX = 0x7FFFFFFF
};

struct mmal_param_awbgains {
  struct mmal_param_rational r_gain;
  struct mmal_param_rational b_gain;
};

/* Manner of video rate control */
enum mmal_param_rate_control_mode {
  MMAL_VIDEO_RATECONTROL_DEFAULT,
  MMAL_VIDEO_RATECONTROL_VARIABLE,
  MMAL_VIDEO_RATECONTROL_CONSTANT,
  MMAL_VIDEO_RATECONTROL_VARIABLE_SKIP_FRAMES,
  MMAL_VIDEO_RATECONTROL_CONSTANT_SKIP_FRAMES
};

struct mmal_param_logging {
   /* Logging bits to set */
   uint32_t set;

   /* Logging bits to clear */
   uint32_t clear;
};

struct mmal_param_cam_info_cam {
  uint32_t port_id;
  uint32_t max_width;
  uint32_t max_height;
  uint32_t lens_present;
  uint8_t  camera_name[MMAL_PARAM_CAM_INFO_MAX_STR_LEN];
};

typedef enum {
  /* Make values explicit to ensure they match values in config ini */
  MMAL_PARAM_CAMERA_INFO_FLASH_TYPE_XENON = 0,
  MMAL_PARAM_CAMERA_INFO_FLASH_TYPE_LED   = 1,
  MMAL_PARAM_CAMERA_INFO_FLASH_TYPE_OTHER = 2,
  MMAL_PARAM_CAMERA_INFO_FLASH_TYPE_MAX = 0x7fffffff
} mmal_param_cam_info_flash_type_t;

struct mmal_param_cam_info_flash {
  mmal_param_cam_info_flash_type_t flash_type;
};

struct mmal_param_cam_info {
  uint32_t num_cameras;
  uint32_t num_flashes;
  struct mmal_param_cam_info_cam cameras[MMAL_PARAM_CAM_INFO_MAX_CAMERAS];
  struct mmal_param_cam_info_flash flashes[MMAL_PARAM_CAM_INFO_MAX_FLASHES];
};

struct mmal_param_cam_config {
  /* Max size of stills capture */
  uint32_t max_stills_w;
  uint32_t max_stills_h;

  /* Allow YUV422 stills capture */
  uint32_t stills_yuv422;

  /* Continuous or one shot stills captures. */
  uint32_t one_shot_stills;

  /* Max size of the preview or video capture frames */
  uint32_t max_preview_video_w;
  uint32_t max_preview_video_h;
  uint32_t num_preview_video_frames;

  /* Sets the height of the circular buffer for stills capture. */
  uint32_t stills_capture_circular_buffer_height;

  /*
   * Allows preview/encode to resume as fast as possible after the stills
   * input frame has been received, and then processes the still frame in
   * the background whilst preview/encode has resumed.
   * Actual mode is controlled by MMAL_PARAM_CAPTURE_MODE.
   */
  uint32_t fast_preview_resume;

  /*
   * Selects algorithm for timestamping frames if there is no clock component 
   * connected, see mmal_param_timestamp_mode_t
   */
  int32_t use_stc_timestamp;
};

struct mmal_param_imagefx {
  enum mmal_param_imagefx_effect effect;
  uint32_t num_effect_params;
  uint32_t effect_parameter[MMAL_MAX_IMAGEFX_PARAMETERS];
};
