#pragma once

#include <log.h>

extern int bcm2835_emmc_log_level;

#define BCM2835_LOG_COMMON(__level, __fmt, ...) \
  LOG(bcm2835_emmc_log_level, __level, "bcm2835_emmc", __fmt, ##__VA_ARGS__)

#define BCM2835_EMMC_LOG(__fmt, ...) \
  BCM2835_LOG_COMMON(INFO, __fmt, ## __VA_ARGS__)

#define BCM2835_EMMC_DBG(__fmt, ...) \
  BCM2835_LOG_COMMON(DEBUG, __fmt, ## __VA_ARGS__)

#define BCM2835_EMMC_DBG2(__fmt, ...) \
  BCM2835_LOG_COMMON(DEBUG2, __fmt, ## __VA_ARGS__)

#define BCM2835_EMMC_WARN(__fmt, ...) \
  BCM2835_LOG_COMMON(WARN, __fmt, ## __VA_ARGS__)

#define BCM2835_EMMC_CRIT(__fmt, ...) \
  BCM2835_LOG_COMMON(CRIT, __fmt, ## __VA_ARGS__)

#define BCM2835_EMMC_ERR(__fmt, ...) \
  BCM2835_LOG_COMMON(ERR, __fmt, ## __VA_ARGS__)

