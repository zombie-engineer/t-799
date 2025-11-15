#pragma once
#include <stdint.h>
#include <stdbool.h>

#define VCHIQ_SERVICE_NAME_TO_ID(__n) \
  ((uint32_t)((__n[0] << 24)|(__n[1] << 16)|(__n[2] <<  8)|__n[3]))

#define VCHIQ_MSG_PADDING            0  /* -                                 */
#define VCHIQ_MSG_CONNECT            1  /* -                                 */
#define VCHIQ_MSG_OPEN               2  /* + (srcport, -), fourcc, client_id */
#define VCHIQ_MSG_OPENACK            3  /* + (srcport, dstport)              */
#define VCHIQ_MSG_CLOSE              4  /* + (srcport, dstport)              */
#define VCHIQ_MSG_DATA               5  /* + (srcport, dstport)              */
#define VCHIQ_MSG_BULK_RX            6  /* + (srcport, dstport), data, size  */
#define VCHIQ_MSG_BULK_TX            7  /* + (srcport, dstport), data, size  */
#define VCHIQ_MSG_BULK_RX_DONE       8  /* + (srcport, dstport), actual      */
#define VCHIQ_MSG_BULK_TX_DONE       9  /* + (srcport, dstport), actual      */
#define VCHIQ_MSG_PAUSE             10  /* -                                 */
#define VCHIQ_MSG_RESUME            11  /* -                                 */
#define VCHIQ_MSG_REMOTE_USE        12  /* -                                 */
#define VCHIQ_MSG_REMOTE_RELEASE    13  /* -                                 */
#define VCHIQ_MSG_REMOTE_USE_ACTIVE 14  /* -                                 */


struct vchiq_state;
struct vchiq_service;

struct vchiq_header {
  /* Message identifier, opaque to applications */
  int msgid;

  /* Size of message data */
  unsigned int size;

  /* message */
  char data[0];
};

typedef int (*vchiq_service_cb_t)(struct vchiq_service *,
  struct vchiq_header *);

struct vchiq_service {
  uint32_t service_id;
  bool opened;
  int remoteport;
  int localport;
  struct vchiq_state *s;
  vchiq_service_cb_t cb;
};

void vchiq_msg_prep(int msgid, int srcport, int dstport, void *payload,
  int payload_sz);

int vchiq_init(void);

struct vchiq_service *vchiq_service_open(uint32_t service_id, int version_min,
  int version_max, vchiq_service_cb_t service_cb);

static inline uint32_t vchiq_service_to_handle(const struct vchiq_service *s)
{
  return s->localport;
}

struct list_head *vchiq_get_components_list(void);

void vchiq_event_signal_trigger(void);
