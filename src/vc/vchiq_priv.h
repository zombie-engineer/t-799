#pragma once

/*
 * Because major VideoCore services on Raspberry Pi are left undocumented, all
 * of the VCHIQ, MMAL related code is derived from drivers found in linux
 * kernel. Some structures, fields and constants are taken as-is with only
 * minor changes to minimize code base and keep code-style consistent to the
 * rest of the kernel code.
 */
#include <list.h>
#include <stdbool.h>

#define VCHIQ_ENABLE_DEBUG 1

#define VCHIQ_MAX_SLOTS          128
#define VCHIQ_MAX_SLOTS_PER_SIDE 64

/*
 * Payload is aligned to header size, so that header would always start at
 * granule of its size
 */
#define VCHIQ_MSG_HDR_SZ_MASK 7
#define VCHIQ_MSG_HDR_SZ_INV_MASK 0xfffffff8

#define VCHIQ_MSG_TOTAL_SIZE(__payload_sz) \
  ((__payload_sz + sizeof(struct vchiq_header) + VCHIQ_MSG_HDR_SZ_MASK) \
    & VCHIQ_MSG_HDR_SZ_INV_MASK)

#define VCHIQ_MAKE_MSG(type, srcport, dstport) \
  ((type<<24) | (srcport<<12) | (dstport<<0))

#define VCHIQ_MSG_TYPE(msgid) \
  ((unsigned int)msgid >> 24)

#define VCHIQ_MSG_SRCPORT(msgid) \
  (unsigned short)(((unsigned int)msgid >> 12) & 0xfff)

#define VCHIQ_MSG_DSTPORT(msgid) \
  ((unsigned short)msgid & 0xfff)

#define VCHIQ_MSGID_PADDING VCHIQ_MAKE_MSG(VCHIQ_MSG_PADDING, 0, 0)

#define VCHIQ_MAKE_FOURCC(x0, x1, x2, x3) \
  (((x0) << 24) | ((x1) << 16) | ((x2) << 8) | (x3))

#define VCHIQ_MAGIC VCHIQ_MAKE_FOURCC('V', 'C', 'H', 'I')

/* The version of VCHIQ - change with any non-trivial change */
#define VCHIQ_VERSION 8

/*
 * The minimum compatible version - update to match VCHIQ_VERSION with any
 * incompatible change
 */
#define VCHIQ_VERSION_MIN 3

#define VCHIQ_SLOT_SIZE 4096

#define VCHIQ_NUM_CURRENT_BULKS 32
#define VCHIQ_NUM_SERVICE_BULKS 4

#define VCHIQ_MAX_FRAGMENTS (VCHIQ_NUM_CURRENT_BULKS * 2)

#define VCHIQ_PLATFORM_FRAGMENTS_OFFSET_IDX 0
#define VCHIQ_PLATFORM_FRAGMENTS_COUNT_IDX  1

#define VCHIQ_SLOT_MASK (VCHIQ_SLOT_SIZE - 1)
#define VCHIQ_SLOT_QUEUE_MASK  (VCHIQ_MAX_SLOTS_PER_SIDE - 1)
#define VCHIQ_SLOT_ZERO_SLOTS  ((sizeof(struct vchiq_slot_zero) + \
  VCHIQ_SLOT_SIZE - 1) / VCHIQ_SLOT_SIZE)

#if 0
typedef enum {
  VCHIQ_SERVICE_OPENED,        /* service, -, -             */
  VCHIQ_SERVICE_CLOSED,        /* service, -, -             */
  VCHIQ_MESSAGE_AVAILABLE,     /* service, header, -        */
  VCHIQ_BULK_TRANSMIT_DONE,    /* service, -, bulk_userdata */
  VCHIQ_BULK_RECEIVE_DONE,     /* service, -, bulk_userdata */
  VCHIQ_BULK_TRANSMIT_ABORTED, /* service, -, bulk_userdata */
  VCHIQ_BULK_RECEIVE_ABORTED   /* service, -, bulk_userdata */
} vchiq_reason_t;
#endif

enum {
  DEBUG_ENTRIES,
#if VCHIQ_ENABLE_DEBUG
  DEBUG_SLOT_HANDLER_COUNT,
  DEBUG_SLOT_HANDLER_LINE,
  DEBUG_PARSE_LINE,
  DEBUG_PARSE_HEADER,
  DEBUG_PARSE_MSGID,
  DEBUG_AWAIT_COMPLETION_LINE,
  DEBUG_DEQUEUE_MESSAGE_LINE,
  DEBUG_SERVICE_CALLBACK_LINE,
  DEBUG_MSG_QUEUE_FULL_COUNT,
  DEBUG_COMPLETION_QUEUE_FULL_COUNT,
#endif
  DEBUG_MAX
};

struct vchiq_event {
  int armed;
  int fired;
  unsigned event;
};

struct vchiq_slot {
  char data[VCHIQ_SLOT_SIZE];
};

struct vchiq_slot_info {
  /* Use two counters rather than one to avoid the need for a mutex. */
  short use_count;
  short release_count;
};

struct vchiq_shared_state {
  /* A non-zero value here indicates that the content is valid.*/
  int initialised;

  /* The first and last (inclusive) slots allocated to the owner */
  int slot_first;
  int slot_last;

  /* The slot allocated to synchronous messages from the owner */
  int slot_sync;

  /*
   * Signalling this event indicates that owner's slot handler thread
   * should run.
   */
  struct vchiq_event trigger;

  /*
   * Indicates the byte position within the stream where the next message
   * will be written. The least significant bits are an index into the
   * slot. The next bits are the index of the slot in slot_queue.
   */
  int tx_pos;

  /* This event should be signalled when a slot is recycled. */
  struct vchiq_event recycle;

  /* The slot_queue index where the next recycled slot will be written. */
  int slot_queue_recycle;

  /* This event should be signalled when a synchronous message is sent. */
  struct vchiq_event sync_trigger;

  /*
   * This event should be signalled when a synchronous message has been
   * released.
   */
  struct vchiq_event sync_release;

  /* A circular buffer of slot indexes. */
  int slot_queue[VCHIQ_MAX_SLOTS_PER_SIDE];

  /* Debugging state */
  int debug[DEBUG_MAX];
};

struct vchiq_slot_zero {
  int magic;
  short version;
  short version_min;
  int slot_zero_size;
  int slot_size;
  int max_slots;
  int max_slots_per_side;
  int platform_data[2];
  struct vchiq_shared_state master;
  struct vchiq_shared_state slave;
  struct vchiq_slot_info slots[VCHIQ_MAX_SLOTS];
};

struct vchiq_state {
  bool is_connected;

  struct vchiq_shared_state *local;
  struct vchiq_shared_state *remote;
  struct vchiq_slot *slots;

  /* Processes incoming messages */
  struct task *slot_handler_thread;

  /* Processes recycled slots */
  struct task *recycle_thread;

  /* Processes synchronous messages */
  struct task *sync_thread;

  struct event ev_trigger;
  struct event ev_recycle;
  struct event ev_sync_trigger;
  struct event ev_sync_release;
  struct event ev_ack;

  char *rx_data;

  /* Indicates the byte position within the stream from where the next
  ** message will be read. The least significant bits are an index into
  ** the slot.The next bits are the index of the slot in
  ** remote->slot_queue. */
  int rx_pos;

  /* A cached copy of local->tx_pos. Only write to local->tx_pos, and read
    from remote->tx_pos. */
  int local_tx_pos;

  /* The slot_queue index of the slot to become available next. */
  int slot_queue_avail;

  struct list_head components;
};

#define SLOT_DATA_FROM_INDEX(state, index) (state->slots + (index))

struct vchiq_msg_open_service {
  int service_id;
  int client_id;
  short version;
  short version_min;
} PACKED;

struct vchiq_fragment {
  struct vchiq_fragment *next;
  char data[];
};

/* CPU level synchronization functions ported from vchiq linux driver */
static inline void vchiq_mb(void)
{
  asm volatile("dsb sy");
}

static inline void vchiq_dsb(void)
{
  asm volatile("dsb sy");
}

static inline void vchiq_rmb(void)
{
  asm volatile("dsb sy");
}

static inline void vchiq_wmb(void)
{
  asm volatile("dsb sy");
}
