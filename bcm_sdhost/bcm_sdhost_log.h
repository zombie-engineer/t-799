#pragma once
#include <log.h>

extern int bcm_sdhost_log_level;

#define BCM_SDHOST_LOG_COMMON(__level, __fmt, ...) \
  LOG(bcm_sdhost_log_level, __level, "bcm_sdhost", __fmt, ##__VA_ARGS__)

#define BCM_SDHOST_LOG_INFO(__fmt, ...) \
  BCM_SDHOST_LOG_COMMON(INFO, __fmt, ## __VA_ARGS__)

#define BCM_SDHOST_LOG_DBG(__fmt, ...) \
  BCM_SDHOST_LOG_COMMON(DEBUG, __fmt, ## __VA_ARGS__)

#define BCM_SDHOST_LOG_DBG2(__fmt, ...) \
  BCM_SDHOST_LOG_COMMON(DEBUG2, __fmt, ## __VA_ARGS__)

#define BCM_SDHOST_LOG_WARN(__fmt, ...) \
  BCM_SDHOST_LOG_COMMON(WARN, __fmt, ## __VA_ARGS__)

#define BCM_SDHOST_LOG_CRIT(__fmt, ...) \
  BCM_SDHOST_LOG_COMMON(CRIT, __fmt, ## __VA_ARGS__)

#define BCM_SDHOST_LOG_ERR(__fmt, ...) \
  BCM_SDHOST_LOG_COMMON(ERR, __fmt, ## __VA_ARGS__)

