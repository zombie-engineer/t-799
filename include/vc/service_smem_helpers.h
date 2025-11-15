#pragma once

#include <log.h>

int smem_log_level;

#define SMEM_UNIT_TAG "smem"
#define SMEM_INFO(__fmt, ...) \
  LOG(smem_log_level, INFO, SMEM_UNIT_TAG, __fmt, ##__VA_ARGS__)

#define SMEM_DEBUG(__fmt, ...) \
  LOG(smem_log_level, DEBUG, SMEM_UNIT_TAG, __fmt, ##__VA_ARGS__)

#define SMEM_DEBUG2(__fmt, ...) \
  LOG(smem_log_level, DEBUG2, SMEM_UNIT_TAG, __fmt, ##__VA_ARGS__)

#define SMEM_ERR(__fmt, ...) \
  LOG(smem_log_level, ERR, SMEM_UNIT_TAG, "%s:" __fmt __endline, \
  __func__, ##__VA_ARGS__)

#define SMEM_CHECK_ERR(__fmt, ...) \
  if (err != SUCCESS) { \
    SMEM_ERR("err:%d " __fmt, err, ##__VA_ARGS__); \
    goto out_err; \
  }

