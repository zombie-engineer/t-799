/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Broadcom BM2835 V4L2 driver
 *
 * Copyright © 2013 Raspberry Pi (Trading) Ltd.
 *
 * Authors: Vincent Sanders @ Collabora
 *          Dave Stevenson @ Broadcom
 *    (now dave.stevenson@raspberrypi.org)
 *          Simon Mellor @ Broadcom
 *          Luke Diamand @ Broadcom
 *
 * MMAL interface to VCHIQ message passing
 */

#pragma once

#include "mmal-common.h"
#include "mmal-msg-format.h"
#include <mutex.h>
#include <spinlock.h>
#include <atomic.h>


/* Maximum size of the format extradata. */
#define MMAL_FORMAT_EXTRADATA_MAX_SIZE 128

enum vchiq_mmal_es_type {
  MMAL_ES_TYPE_UNKNOWN,     /**< Unknown elementary stream type */
  MMAL_ES_TYPE_CONTROL,     /**< Elementary stream of control commands */
  MMAL_ES_TYPE_AUDIO,       /**< Audio elementary stream */
  MMAL_ES_TYPE_VIDEO,       /**< Video elementary stream */
  MMAL_ES_TYPE_SUBPICTURE   /**< Sub-picture elementary stream */
};
