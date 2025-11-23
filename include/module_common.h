#pragma once

#include <log.h>

static int MODULE_LOG_LEVEL = LOG_LEVEL_CRIT;

#define MODULE_INFO(__fmt, ...) \
  LOG(MODULE_LOG_LEVEL, INFO, MODULE_UNIT_TAG, __fmt, ##__VA_ARGS__)

#define MODULE_DEBUG(__fmt, ...) \
  LOG(MODULE_LOG_LEVEL, DEBUG, MODULE_UNIT_TAG, __fmt, ##__VA_ARGS__)

#define MODULE_DEBUG2(__fmt, ...) \
  LOG(MODULE_LOG_LEVEL, DEBUG2, MODULE_UNIT_TAG, __fmt, ##__VA_ARGS__)

#define MODULE_ERR(__fmt, ...) \
  LOG(MODULE_LOG_LEVEL, ERR, MODULE_UNIT_TAG, "%s:" __fmt __endline, \
  __func__, ##__VA_ARGS__)

#define CHECK_ERR(__fmt, ...) \
  if (err != SUCCESS) { \
    MODULE_ERR("err:%d " __fmt, err, ##__VA_ARGS__); \
    goto out_err; \
  }

#define CHECK_ERR_PTR(__ptr, __fmt, ...) \
  if (!(__ptr)) { \
    err = ERR_GENERIC; \
    MODULE_ERR("err:%d " __fmt, err, ##__VA_ARGS__); \
    goto out_err; \
  }

#define MODULE_ASSERT(__expr, __msg) \
  if (!(__expr)) { \
    MODULE_ERR(__msg); \
    while(1) \
      asm volatile("wfe"); \
  }
