#pragma once
#include <log.h>

extern int mmal_log_level;

#define MMAL_UNIT_TAG "mmal"
#define MMAL_INFO(__fmt, ...) \
  LOG(mmal_log_level, INFO, MMAL_UNIT_TAG, __fmt, ##__VA_ARGS__)

#define MMAL_DEBUG(__fmt, ...) \
  LOG(mmal_log_level, DEBUG, MMAL_UNIT_TAG, __fmt, ##__VA_ARGS__)

#define MMAL_DEBUG2(__fmt, ...) \
  LOG(mmal_log_level, DEBUG2, MMAL_UNIT_TAG, __fmt, ##__VA_ARGS__)

#define MMAL_ERR(__fmt, ...) \
  LOG(mmal_log_level, ERR, MMAL_UNIT_TAG, "%s:" __fmt __endline, \
  __func__, ##__VA_ARGS__)
