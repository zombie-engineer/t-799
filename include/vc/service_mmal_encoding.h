#pragma once

#define MAKE_FOURCC(x) \
  ((int32_t)( (x[0] << 24) | (x[1] << 16) | (x[2] << 8) | x[3] ))

#define MMAL_ENCODING_OPAQUE           MMAL_FOURCC('O', 'P', 'Q', 'V')
#define MMAL_ENCODING_H264             MMAL_FOURCC('H', '2', '6', '4')
#define MMAL_ENCODING_I420             MMAL_FOURCC('I', '4', '2', '0')
#define MMAL_ENCODING_RGB24            MMAL_FOURCC('R', 'G', 'B', '3')
