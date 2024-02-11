#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <compiler.h>
#include <list.h>
#include <atomic.h>
#include <kmalloc.h>
#include <printf.h>
#include <errcode.h>
#include <sched.h>
#include <os_api.h>
#include <common.h>
#include <mbox_props.h>
#include <task.h>
#include <irq.h>
#include <ioreg.h>
#include <bcm2835/bcm2835_ic.h>
#include "mmal-msg.h"
#include "mmal-log.h"
#include "mmal-parameters.h"
#include "mmal-encodings.h"
#include "vc_sm_defs.h"

int mmal_log_level = LOG_LEVEL_INFO;

static inline void mb(void)
{
  asm volatile("dsb sy");
}

static inline void dsb(void)
{
  asm volatile("dsb sy");
}

static inline void rmb(void)
{
  asm volatile("dsb sy");
}

static inline void wmb(void)
{
  asm volatile("dsb sy");
}

#define CHECK_ERR(__fmt, ...) \
  if (err != SUCCESS) { \
    MMAL_ERR("err: %d, " __fmt, err, ##__VA_ARGS__); \
    goto out_err; \
  }

#define CHECK_ERR_PTR(__ptr, __fmt, ...) \
  if (!(__ptr)) { \
    MMAL_ERR("err: %d, " __fmt, err, ##__VA_ARGS__); \
    goto out_err; \
  }

#define VC_SM_RESOURCE_NAME_DEFAULT       "sm-host-resource"

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

#define VCHIQ_MAX_SLOTS_PER_SIDE 64
#define VCHIQ_SLOT_SIZE 4096

#define VCHIQ_MAKE_MSG(type, srcport, dstport) \
  ((type<<24) | (srcport<<12) | (dstport<<0))
#define VCHIQ_MSG_TYPE(msgid)          ((unsigned int)msgid >> 24)
#define VCHIQ_MSG_SRCPORT(msgid) \
  (unsigned short)(((unsigned int)msgid >> 12) & 0xfff)
#define VCHIQ_MSG_DSTPORT(msgid) \
  ((unsigned short)msgid & 0xfff)

#define VCHIQ_MSGID_PADDING            VCHIQ_MAKE_MSG(VCHIQ_MSG_PADDING, 0, 0)
#define VCHIQ_MSGID_CLAIMED            0x40000000

#define VCHIQ_FOURCC_INVALID           0x00000000
#define VCHIQ_FOURCC_IS_LEGAL(fourcc)  (fourcc != VCHIQ_FOURCC_INVALID)

#define VCHIQ_BULK_ACTUAL_ABORTED -1

#define VCHIQ_MAKE_FOURCC(x0, x1, x2, x3) \
  (((x0) << 24) | ((x1) << 16) | ((x2) << 8) | (x3))

#define VCHIQ_MAGIC VCHIQ_MAKE_FOURCC('V', 'C', 'H', 'I')

/* The version of VCHIQ - change with any non-trivial change */
#define VCHIQ_VERSION            8
/* The minimum compatible version - update to match VCHIQ_VERSION with any
** incompatible change */
#define VCHIQ_VERSION_MIN        3

/* The version that introduced the VCHIQ_IOC_LIB_VERSION ioctl */
#define VCHIQ_VERSION_LIB_VERSION 7

/* The version that introduced the VCHIQ_IOC_CLOSE_DELIVERED ioctl */
#define VCHIQ_VERSION_CLOSE_DELIVERED 7

/* The version that made it safe to use SYNCHRONOUS mode */
#define VCHIQ_VERSION_SYNCHRONOUS_MODE 8

#define VCHIQ_MAX_STATES         1
#define VCHIQ_MAX_SERVICES       4096
#define VCHIQ_MAX_SLOTS          128
#define VCHIQ_MAX_SLOTS_PER_SIDE 64

#define VCHIQ_NUM_CURRENT_BULKS        32
#define VCHIQ_NUM_SERVICE_BULKS        4

#ifndef VCHIQ_ENABLE_DEBUG
#define VCHIQ_ENABLE_DEBUG             1
#endif

#ifndef VCHIQ_ENABLE_STATS
#define VCHIQ_ENABLE_STATS             1
#endif

#define BITSET_SIZE(b)        ((b + 31) >> 5)

#define MAX_FRAGMENTS (VCHIQ_NUM_CURRENT_BULKS * 2)

#define VCHIQ_PLATFORM_FRAGMENTS_OFFSET_IDX 0
#define VCHIQ_PLATFORM_FRAGMENTS_COUNT_IDX  1

struct vchiq_open_payload {
  int fourcc;
  int client_id;
  short version;
  short version_min;
} PACKED;

static int vc_trans_id = 0;

static struct vchiq_state vchiq_state ALIGNED(64);

static struct list_head mmal_io_work_list;
static struct event mmal_io_work_waitflag;
static struct spinlock mmal_io_work_list_lock;

typedef int (*mmal_io_fn)(struct vchiq_mmal_component *,
  struct vchiq_mmal_port *, struct mmal_buffer_header *);

typedef uint32_t vchiq_service_handle_t;

typedef enum {
  VCHIQ_CONNSTATE_DISCONNECTED,
  VCHIQ_CONNSTATE_CONNECTING,
  VCHIQ_CONNSTATE_CONNECTED,
  VCHIQ_CONNSTATE_PAUSING,
  VCHIQ_CONNSTATE_PAUSE_SENT,
  VCHIQ_CONNSTATE_PAUSED,
  VCHIQ_CONNSTATE_RESUMING,
  VCHIQ_CONNSTATE_PAUSE_TIMEOUT,
  VCHIQ_CONNSTATE_RESUME_TIMEOUT
} vchiq_connstate_t;

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

struct vchiq_remote_event {
  int armed;
  int fired;
  unsigned event;
};

static inline void remote_event_init(struct vchiq_remote_event *ev)
{
  ev->armed = 0;
  ev->event = (uint32_t)(uint64_t)ev;
}

struct vchiq_slot {
  char data[VCHIQ_SLOT_SIZE];
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
  struct vchiq_remote_event trigger;

  /*
   * Indicates the byte position within the stream where the next message
   * will be written. The least significant bits are an index into the
   * slot. The next bits are the index of the slot in slot_queue.
   */
  int tx_pos;

  /* This event should be signalled when a slot is recycled. */
  struct vchiq_remote_event recycle;

  /* The slot_queue index where the next recycled slot will be written. */
  int slot_queue_recycle;

  /* This event should be signalled when a synchronous message is sent. */
  struct vchiq_remote_event sync_trigger;

  /*
   * This event should be signalled when a synchronous message has been
   * released.
   */
  struct vchiq_remote_event sync_release;

  /* A circular buffer of slot indexes. */
  int slot_queue[VCHIQ_MAX_SLOTS_PER_SIDE];

  /* Debugging state */
  int debug[DEBUG_MAX];
};

struct vchiq_slot_info {
  /* Use two counters rather than one to avoid the need for a mutex. */
  short use_count;
  short release_count;
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

typedef enum {
  VCHIQ_SERVICE_OPENED,         /* service, -, -             */
  VCHIQ_SERVICE_CLOSED,         /* service, -, -             */
  VCHIQ_MESSAGE_AVAILABLE,      /* service, header, -        */
  VCHIQ_BULK_TRANSMIT_DONE,     /* service, -, bulk_userdata */
  VCHIQ_BULK_RECEIVE_DONE,      /* service, -, bulk_userdata */
  VCHIQ_BULK_TRANSMIT_ABORTED,  /* service, -, bulk_userdata */
  VCHIQ_BULK_RECEIVE_ABORTED    /* service, -, bulk_userdata */
} vchiq_reason_t;

struct vchiq_header {
  /* The message identifier - opaque to applications. */
  int msgid;

  /* Size of message data. */
  unsigned int size;

  char data[0];           /* message */
};

typedef enum {
  VCHIQ_ERROR   = -1,
  VCHIQ_SUCCESS = 0,
  VCHIQ_RETRY   = 1
} vchiq_status_t;

typedef vchiq_status_t (*vchiq_callback_t)(vchiq_reason_t,
  struct vchiq_header*,
  vchiq_service_handle_t, void *);

struct vchiq_service_base {
  int fourcc;
  vchiq_callback_t callback;
  void *userdata;
};

typedef void (*vchiq_userdata_term_t)(void *userdata);

typedef uint32_t vchi_mem_handle_t;

struct vchiq_bulk {
  short mode;
  short dir;
  void *userdata;
  vchi_mem_handle_t handle;
  void *data;
  int size;
  void *remote_data;
  int remote_size;
  int actual;
};

struct vchiq_bulk_queue {
  int local_insert;  /* Where to insert the next local bulk */
  int remote_insert; /* Where to insert the next remote bulk (master) */
  int process;       /* Bulk to transfer next */
  int remote_notify; /* Bulk to notify the remote client of next (mstr) */
  int remove;        /* Bulk to notify the local client of, and remove,
                      ** next */
  struct vchiq_bulk bulks[VCHIQ_NUM_SERVICE_BULKS];
};

struct vchiq_service_common;
typedef int (*vchiq_data_callback_t)(struct vchiq_service_common *,
  struct vchiq_header *);

struct vchiq_service_common {
  uint32_t fourcc;
  bool opened;
  int remoteport;
  int localport;
  struct vchiq_state *s;
  vchiq_data_callback_t data_callback;
};

struct vchiq_service {
  struct vchiq_service_base base;
  vchiq_service_handle_t handle;
  unsigned int ref_count;
  int srvstate;
  vchiq_userdata_term_t userdata_term;
  unsigned int localport;
  unsigned int remoteport;
  int public_fourcc;
  int client_id;
  char auto_close;
  char sync;
  char closing;
  atomic_t poll_flags;
  short version;
  short version_min;
  short peer_version;

  struct vchiq_state *state;

  int service_use_count;

  struct vchiq_bulk_queue bulk_tx;
  struct vchiq_bulk_queue bulk_rx;

  struct service_stats {
    int quota_stalls;
    int slot_stalls;
    int bulk_stalls;
    int error_count;
    int ctrl_tx_count;
    int ctrl_rx_count;
    int bulk_tx_count;
    int bulk_rx_count;
    int bulk_aborted_count;
    uint64_t ctrl_tx_bytes;
    uint64_t ctrl_rx_bytes;
    uint64_t bulk_tx_bytes;
    uint64_t bulk_rx_bytes;
  } stats;
};

struct vchiq_state {
  int initialised;
  vchiq_connstate_t conn_state;
  int is_master;
  short version_common;

  struct vchiq_shared_state *local;
  struct vchiq_shared_state *remote;
  struct vchiq_slot *slot_data;

  /* Processes incoming messages */
  struct task *slot_handler_thread;

  /* Processes recycled slots */
  struct task *recycle_thread;

  /* Processes synchronous messages */
  struct task *sync_thread;

  struct event trigger_waitflag;
  struct event recycle_waitflag;
  struct event sync_trigger_waitflag;
  struct event sync_release_waitflag;
  struct event state_waitflag;

  char *tx_data;
  char *rx_data;
  struct vchiq_slot_info *rx_info;

  /* Indicates the byte position within the stream from where the next
  ** message will be read. The least significant bits are an index into
  ** the slot.The next bits are the index of the slot in
  ** remote->slot_queue. */
  int rx_pos;

  /* A cached copy of local->tx_pos. Only write to local->tx_pos, and read
    from remote->tx_pos. */
  int local_tx_pos;

  /* The slot_queue index of the slot to become available next. */
  int slot_queue_available;

  /* A flag to indicate if any poll has been requested */
  int poll_needed;

  /* Ths index of the previous slot used for data messages. */
  int previous_data_index;

  /* The number of slots occupied by data messages. */
  unsigned short data_use_count;

  /* The maximum number of slots to be occupied by data messages. */
  unsigned short data_quota;

  /* Incremented when there are bulk transfers which cannot be processed
   * whilst paused and must be processed on resume */
  int deferred_bulks;

  struct state_stats_struct {
    int slot_stalls;
    int data_stalls;
    int ctrl_tx_count;
    int ctrl_rx_count;
    int error_count;
  } stats;

  struct vchiq_service *services[VCHIQ_MAX_SERVICES];
};

static struct vchiq_state vchiq_state ALIGNED(64);

#define VCHIQ_SLOT_MASK (VCHIQ_SLOT_SIZE - 1)
#define VCHIQ_SLOT_QUEUE_MASK  (VCHIQ_MAX_SLOTS_PER_SIDE - 1)
#define VCHIQ_SLOT_ZERO_SLOTS  ((sizeof(struct vchiq_slot_zero) + \
  VCHIQ_SLOT_SIZE - 1) / VCHIQ_SLOT_SIZE)
static struct vchiq_slot_zero *vslotz = NULL;
static struct vchiq_slot *vslot = NULL;

static struct vchiq_slot_zero *vchiq_init_slots(void *mem_base, int mem_size)
{
  int mem_align = (VCHIQ_SLOT_SIZE - (int)(uint64_t)mem_base) & VCHIQ_SLOT_MASK;
  struct vchiq_slot_zero *slot_zero = (struct vchiq_slot_zero *)((char *)mem_base + mem_align);
  int num_slots = (mem_size - mem_align)/VCHIQ_SLOT_SIZE;
  int first_data_slot = VCHIQ_SLOT_ZERO_SLOTS;

  vslotz = mem_base;
  vslot = mem_base;

  /* Ensure there is enough memory to run an absolutely minimum system */
  num_slots -= first_data_slot;

  if (num_slots < 4)
    return NULL;

  memset(slot_zero, 0, sizeof(*slot_zero));

  slot_zero->magic = VCHIQ_MAGIC;
  slot_zero->version = VCHIQ_VERSION;
  slot_zero->version_min = VCHIQ_VERSION_MIN;
  slot_zero->slot_zero_size = sizeof(*slot_zero);
  slot_zero->slot_size = VCHIQ_SLOT_SIZE;
  slot_zero->max_slots = VCHIQ_MAX_SLOTS;
  slot_zero->max_slots_per_side = VCHIQ_MAX_SLOTS_PER_SIDE;

  slot_zero->master.slot_sync = first_data_slot;
  slot_zero->master.slot_first = first_data_slot + 1;
  slot_zero->master.slot_last = first_data_slot + (num_slots / 2) - 1;
  slot_zero->slave.slot_sync = first_data_slot + (num_slots / 2);
  slot_zero->slave.slot_first = first_data_slot + (num_slots / 2) + 1;
  slot_zero->slave.slot_last = first_data_slot + num_slots - 1;

  return slot_zero;
}

#define SLOT_DATA_FROM_INDEX(state, index) (state->slot_data + (index))

static inline void
remote_event_create(struct vchiq_remote_event *event)
{
  event->armed = 0;
  event->event = (uint32_t)(uint64_t)event;
}

static inline void
remote_event_signal_local(struct vchiq_remote_event *event)
{
  event->armed = 0;
}

static void vchiq_init_state(struct vchiq_state *state,
  struct vchiq_slot_zero *slot_zero)
{
  struct vchiq_shared_state *local;
  struct vchiq_shared_state *remote;
  int i;

  /* Check the input configuration */
  local = &slot_zero->slave;
  remote = &slot_zero->master;

  memset(state, 0, sizeof(*state));
  state->is_master = 0;

  /* initialize shared state pointers */
  state->local = local;
  state->remote = remote;
  state->slot_data = (struct vchiq_slot *)slot_zero;

  os_event_init(&state->trigger_waitflag);
  os_event_init(&state->recycle_waitflag);
  os_event_init(&state->sync_trigger_waitflag);
  os_event_init(&state->sync_release_waitflag);
  os_event_init(&state->state_waitflag);

  state->slot_queue_available = 0;

  for (i = local->slot_first; i <= local->slot_last; i++)
    local->slot_queue[state->slot_queue_available++] = i;

  state->previous_data_index = -1;
  state->data_use_count = 0;
  state->data_quota = state->slot_queue_available - 1;

  remote_event_init(&local->trigger);
  local->tx_pos = 0;

  remote_event_init(&local->recycle);
  local->slot_queue_recycle = state->slot_queue_available;

  remote_event_init(&local->sync_trigger);
  remote_event_init(&local->sync_release);

  /* At start-of-day, the slot is empty and available */
  ((struct vchiq_header *)SLOT_DATA_FROM_INDEX(state, local->slot_sync))->msgid
    = VCHIQ_MSGID_PADDING;

  remote_event_signal_local(&local->sync_release);

  local->debug[DEBUG_ENTRIES] = DEBUG_MAX;

  /* Indicate readiness to the other side */
  local->initialised = 1;
}

struct fragment {
  struct fragment *next;
  char data[];
};

static inline struct fragment *fragment_get_next(struct fragment *f,
  size_t fragment_size)
{
  return (void *)((char *)f + fragment_size);
}

static inline void vchiq_init_fragments(void *baseaddr, int num_fragments,
  size_t fragment_size)
{
  int i;
  struct fragment *f = baseaddr;

  for (i = 0; i < num_fragments - 1; i++) {
    f->next = fragment_get_next(f, fragment_size);
    f = f->next;
  }
  f->next = NULL;
}

struct mmal_io_work {
  struct list_head list;
  struct mmal_buffer_header *b;
  struct vchiq_mmal_component *c;
  struct vchiq_mmal_port *p;
  mmal_io_fn fn;
};

struct mmal_io_work *mmal_io_work_pop(void)
{
  int irqflags;
  struct mmal_io_work *w = NULL;
  // spinlock_lock_disable_irq(&mmal_io_work_list_lock, irqflags);
  if (list_empty(&mmal_io_work_list))
    goto out_unlock;

  w = list_first_entry(&mmal_io_work_list, typeof(*w), list);
  list_del_init(&w->list);
  wmb();

out_unlock:
  // spinlock_unlock_restore_irq(&mmal_io_work_list_lock, irqflags);
  return w;
}

static void vchiq_io_thread(void)
{
  struct mmal_io_work *w;
  while(1) {
    os_event_wait(&mmal_io_work_waitflag);
    os_event_clear(&mmal_io_work_waitflag);
    // MMAL_INFO("io_thread: woke up");
    w = mmal_io_work_pop();
    if (w) {
      // MMAL_INFO("io_thread: have new work: %p", w);
      w->fn(w->c, w->p, w->b);
    }
  }
}

static uint64_t vchiq_events ALIGNED(64) = 0xfffffffffffffff4;

static inline void vchiq_event_check(struct event *ev, struct vchiq_remote_event *e)
{
  dsb();
  if (e->armed && e->fired)
    os_event_notify(ev);
}

static void vchiq_check_local_events()
{
  struct vchiq_shared_state *local;
  local = vchiq_state.local;

  vchiq_event_check(&vchiq_state.trigger_waitflag, &local->trigger);
  vchiq_event_check(&vchiq_state.recycle_waitflag, &local->recycle);
  vchiq_event_check(&vchiq_state.sync_trigger_waitflag, &local->sync_trigger);
  vchiq_event_check(&vchiq_state.sync_release_waitflag, &local->sync_release);
}

static void vchiq_irq_handler(void)
{
  uint32_t bell_reg;
  vchiq_events++;

  /*
   * This is a very specific undocumented part of vchiq IRQ
   * handling code, taken as is from linux kernel.
   * BELL0 register is obviously a clear-on-read, once we
   * read the word at this address, the second read shows
   * zero, thus this is a clear-on-read.
   * Mostly in this code path bell_reg will read 0x5 in the
   * next line. And bit 3 (1<<3 == 4) means the bell was rung
   * on the VC side.
   * This thread discusses the documentation for this register:
   * https://forums.raspberrypi.com/viewtopic.php?t=280271
   * The thread ends with the understanding that this register is very
   * specific to VCHIQ and is not documented anywhere, so waste your time on
   * your own risk.
   *
   * Anyways, the architecture of event handling is this:
   * VideoCore has a couple of situations when it wants to send signals to us:
   * 1. It needs us to recycle some old messages that waste queue space.
   * 2. It wants to send us some message.
   * All of which is done via special events in the zero slot.
   * For recycling 'recycle_event' is used.
   * For messages 'trigger_event' is used.
   * I don't know how sync_event is used, might be for other kind of messages.
   * To signal one of these events VideoCore sets event->fired = 1, and then
   * writes something to BELL0, which is 0x3f00b840 on Raspberry PI 3B+.
   * Writing to this address triggers IRQ Exception in one of the CPU cores,
   * which one depends on where peripheral interrupts are routed. IRQ will
   * only get triggered if DOORBELL0 iterrupts are enabled in bcm interrupt
   * controller, the one on 0x3f00b200. To enable it set bit 2 in
   * base interrupt enable register 0x3f00b218.
   * DOORBELL 0 has irq number 66 in irq_table. The meaning of 66 is this:
   * there are 64 GPU interrupts that are in 2 32bit registers:
   * pending(0x204,0x208), enable(0x210,0x214), etc.
   * so they have numbers 0-63.
   * next number 64 is basic pending register bit 0, 65, is basic bit 1, and
   * the doorbell 0 is basic bit 2, thus 64+2 = 66.
   */
#define BELL0 ((ioreg32_t)(0x3f00b840))
  bell_reg = ioreg32_read(BELL0);
  if (bell_reg & 4)
    vchiq_check_local_events();
}

#define BELL2 ((ioreg32_t)(0x3f00b848))
void vchiq_ring_bell(void)
{
  ioreg32_write(BELL2, 0);
}

static int vchiq_tx_header_to_slot_idx(struct vchiq_state *s)
{
  return s->remote->slot_queue[(s->rx_pos / VCHIQ_SLOT_SIZE) & VCHIQ_SLOT_QUEUE_MASK];
}

static int vchiq_calc_stride(int size)
{
  size += sizeof(struct vchiq_header);
  return (size + sizeof(struct vchiq_header) - 1) & ~(sizeof(struct vchiq_header) - 1);
}

struct vchiq_header *vchiq_get_next_header_rx(struct vchiq_state *state)
{
  struct vchiq_header *h;
  int rx_pos, slot_queue_index, slot_index;

next_header:
  slot_queue_index = ((int)((unsigned int)(state->rx_pos) / VCHIQ_SLOT_SIZE));
  slot_index = state->remote->slot_queue[slot_queue_index & VCHIQ_SLOT_QUEUE_MASK];
  state->rx_data = (char *)(state->slot_data + slot_index);
  rx_pos = state->rx_pos;
  h = (struct vchiq_header *)&state->rx_data[rx_pos & VCHIQ_SLOT_MASK];
  state->rx_pos += vchiq_calc_stride(h->size);
  if (h->msgid == VCHIQ_MSGID_PADDING)
    goto next_header;

  return h;
}

static struct vchiq_service_common *vchiq_service_map[10];

static struct vchiq_service_common *vchiq_alloc_service(void)
{
  struct vchiq_service_common *service;

  int i;

  for (i = 0; i < ARRAY_SIZE(vchiq_service_map); ++i) {
    if (!vchiq_service_map[i])
      break;
  }

  if (i == ARRAY_SIZE(vchiq_service_map)) {
    MMAL_ERR("No free service port");
    return NULL;
  }

  service = kzalloc(sizeof(*service));
  if (!service) {
    MMAL_ERR("Failed to alloce memory for service");
    return NULL;
  }

  /* ser localport to non zero value or vchiq will assign something */
  service->localport = i + 1;
  wmb();
  vchiq_service_map[i] = service;
  return service;
}

static struct vchiq_service_common *vchiq_service_find_by_localport(
  int localport)
{
  int i;

  for (i = 0; i < ARRAY_SIZE(vchiq_service_map); ++i) {
    if (vchiq_service_map[i] && vchiq_service_map[i]->localport == localport)
      return vchiq_service_map[i];
  }
  return NULL;
}

static int vchiq_parse_msg_openack(struct vchiq_state *s,
  int localport, int remoteport)
{
  struct vchiq_service_common *service = NULL;

  service = vchiq_service_find_by_localport(localport);
  BUG_IF(!service, "OPENACK with no service");
  service->opened = true;
  service->remoteport = remoteport;
  wmb();
  os_event_notify(&s->state_waitflag);
  return SUCCESS;
}

static int vchiq_parse_msg_data(struct vchiq_state *s, int localport,
  int remoteport, struct vchiq_header *h)
{
  struct vchiq_service_common *service = NULL;

  service = vchiq_service_find_by_localport(localport);
  BUG_IF(!service, "MSG_DATA with no service");
  service->data_callback(service, h);
  return SUCCESS;
}


static inline int vchiq_parse_rx_dispatch(struct vchiq_state *s,
  struct vchiq_header *h)
{
  int err;
  int msg_type, localport, remoteport;
  msg_type = VCHIQ_MSG_TYPE(h->msgid);
  localport = VCHIQ_MSG_DSTPORT(h->msgid);
  remoteport = VCHIQ_MSG_SRCPORT(h->msgid);
  switch(msg_type) {
    case VCHIQ_MSG_CONNECT:
      s->conn_state = VCHIQ_CONNSTATE_CONNECTED;
      os_event_notify(&s->state_waitflag);
      break;
    case VCHIQ_MSG_OPENACK:
      err = vchiq_parse_msg_openack(s, localport, remoteport);
      break;
    case VCHIQ_MSG_DATA:
      err = vchiq_parse_msg_data(s, localport, remoteport, h);
      break;
    default:
      err = ERR_INVAL;
      break;
  }
  return err;
}

void vchiq_event_signal(struct vchiq_remote_event *event)
{
  wmb();
  event->fired = 1;
  dsb();
  if (event->armed)
    vchiq_ring_bell();
}

static void vchiq_release_slot(struct vchiq_state *s, int slot_index)
{
  int slot_queue_recycle;

  slot_queue_recycle = s->remote->slot_queue_recycle;
  rmb();
  s->remote->slot_queue[slot_queue_recycle & VCHIQ_SLOT_QUEUE_MASK] = slot_index;
  s->remote->slot_queue_recycle = slot_queue_recycle + 1;
  vchiq_event_signal(&s->remote->recycle);
}

static int vchiq_parse_rx(struct vchiq_state *s)
{
  int err = SUCCESS;
  int rx_slot;
  struct vchiq_header *h;
  int prev_rx_slot = vchiq_tx_header_to_slot_idx(s);

  while(s->rx_pos != s->remote->tx_pos) {
    int old_rx_pos = s->rx_pos;
    h = vchiq_get_next_header_rx(s);
    MMAL_DEBUG2("msg received: %p, rx_pos: %d, size: %d", h, old_rx_pos,
      vchiq_calc_stride(h->size));
    err = vchiq_parse_rx_dispatch(s, h);
    CHECK_ERR("failed to parse message from remote");

    rx_slot = vchiq_tx_header_to_slot_idx(s);
    if (rx_slot != prev_rx_slot) {
      vchiq_release_slot(s, prev_rx_slot);
      prev_rx_slot = rx_slot;
    }
  }
  return SUCCESS;
out_err:
  return err;
}

void vchiq_event_wait(struct event *ev, struct vchiq_remote_event *event)
{
  if (!event->fired) {
    event->armed = 1;
    dsb();
    os_event_wait(ev);
    os_event_clear(ev);
    event->armed = 0;
    wmb();
  }
  event->fired = 0;
  rmb();
}

/*
 * Called in the context of recycle thread when remote (VC) is
 * done with some particular slot of our messages and signals
 * recylce event to us, so that we can reuse these slots.
 *
 * In original code there is a lot of bookkeeping in this func
 * but it seems it's not needed critically right now.
 */
static void vchiq_process_free_queue(struct vchiq_state *s)
{
  int slot_queue_available, pos, slot_index;
  char *slot_data;
  struct vchiq_header *h;

  slot_queue_available = s->slot_queue_available;

  mb();

  while (slot_queue_available != s->local->slot_queue_recycle) {
    slot_index = s->local->slot_queue[slot_queue_available & VCHIQ_SLOT_QUEUE_MASK];
    slot_queue_available++;
    slot_data = (char *)&s->slot_data[slot_index];

    rmb();

    pos = 0;
    while (pos < VCHIQ_SLOT_SIZE) {
      h = (struct vchiq_header *)(slot_data + pos);

      pos += vchiq_calc_stride(h->size);
      BUG_IF(pos > VCHIQ_SLOT_SIZE, "some");
    }

    mb();

    s->slot_queue_available = slot_queue_available;
  }
}

static void vchiq_recycle_thread(void)
{
  struct vchiq_state *s;

  s = &vchiq_state;
  while (1) {
    vchiq_event_wait(&s->recycle_waitflag, &s->local->recycle);
    vchiq_process_free_queue(s);
    os_yield();
  }
}

static void vchiq_loop_thread(void)
{
  struct vchiq_state *s;

  s = &vchiq_state;
  while(1) {
    vchiq_event_wait(&s->trigger_waitflag, &s->local->trigger);
    BUG_IF(vchiq_parse_rx(s) != SUCCESS,
      "failed to parse incoming vchiq messages");
  }
}

static int vchiq_start_thread(struct vchiq_state *s)
{
  int err;
  struct task *t;

  INIT_LIST_HEAD(&mmal_io_work_list);
  os_event_init(&mmal_io_work_waitflag);
  // spinlock_init(&mmal_io_work_list_lock);

  t = task_create(vchiq_io_thread, "vchiq_io_thread");
  CHECK_ERR_PTR(t, "Failed to start vchiq_thread");
  os_schedule_task(t);

  irq_set(BCM2835_IRQNR_ARM_DOORBELL_0, vchiq_irq_handler);
  bcm2835_ic_enable_irq(BCM2835_IRQNR_ARM_DOORBELL_0);
  t = task_create(vchiq_loop_thread, "vchiq_loop_thread");
  CHECK_ERR_PTR(t, "Failed to start vchiq_thread");
  os_schedule_task(t);
  os_yield();

  t = task_create(vchiq_recycle_thread, "vchiq_recycle_thread");
  CHECK_ERR_PTR(t, "Failed to start vchiq_thread");
  os_schedule_task(t);

  return SUCCESS;

out_err:
  bcm2835_ic_disable_irq(BCM2835_IRQNR_ARM_DOORBELL_0);
  irq_set(BCM2835_IRQNR_ARM_DOORBELL_0, 0);
  return err;
}

static struct vchiq_header *vchiq_prep_next_header_tx(struct vchiq_state *s,
  int msg_size)
{
  int tx_pos, slot_queue_index, slot_index;
  struct vchiq_header *h;
  int slot_space;
  int stride;

  /* Recall last position for tx */
  tx_pos = s->local_tx_pos;
  stride = vchiq_calc_stride(msg_size);

  /*
   * If message can not passed in one chunk within current slot,
   * we have to add padding header to this last free slot space and
   * allocate start of header already in the next slot
   *
   * |----------SLOT 1----------------|----------SLOT 2----------------|
   * |XXXXXXXXXXXXXXXXXXXXXXXXX0000000|00000000000000000000000000000000|
   * |                         ^                                       |
   * |                           \start of header                      |
   * |                         |============| <- message overlaps      |
   * |                                           slot boundary         |
   * |                         |pppppp|============| <- messsage moved |
   * |                        /                         to next slot   |
   * |         padding message added                                   |
   */
  slot_space = VCHIQ_SLOT_SIZE - (tx_pos & VCHIQ_SLOT_MASK);
  if (slot_space < stride) {
    slot_queue_index = ((int)((unsigned int)(tx_pos) / VCHIQ_SLOT_SIZE));
    slot_index = s->local->slot_queue[slot_queue_index & VCHIQ_SLOT_QUEUE_MASK];

    s->tx_data = (char*)&s->slot_data[slot_index];

    h = (struct vchiq_header *)(s->tx_data + (tx_pos & VCHIQ_SLOT_MASK));
    h->msgid = VCHIQ_MSGID_PADDING;
    h->size = slot_space - sizeof(*h);
    s->local_tx_pos += slot_space;
    tx_pos += slot_space;
  }

  slot_queue_index = ((int)((unsigned int)(tx_pos) / VCHIQ_SLOT_SIZE));
  slot_index = s->local->slot_queue[slot_queue_index & VCHIQ_SLOT_QUEUE_MASK];

  s->tx_data = (char*)&s->slot_data[slot_index];
  s->local_tx_pos += stride;
  s->local->tx_pos = s->local_tx_pos;

  h = (struct vchiq_header *)(s->tx_data + (tx_pos & VCHIQ_SLOT_MASK));
  h->size = msg_size;
  return h;
}

static void vchiq_handmade_prep_msg(struct vchiq_state *s, int msgid,
  int srcport, int dstport, void *payload, int payload_sz)
{
  struct vchiq_header *h;
  int old_tx_pos = s->local->tx_pos;

  h = vchiq_prep_next_header_tx(s, payload_sz);
  MMAL_DEBUG2("msg sent: %p, tx_pos: %d, size: %d", h, old_tx_pos,
    vchiq_calc_stride(h->size));

  h->msgid = VCHIQ_MAKE_MSG(msgid, srcport, dstport);
  memcpy(h->data, payload, payload_sz);
}


static void vchiq_open_service(struct vchiq_state *state,
  struct vchiq_service_common *service, uint32_t fourcc, short version,
  short version_min)
{
  /* Service open payload */
  struct vchiq_open_payload open_payload;

  /* Open "mmal" service */
  open_payload.fourcc = fourcc;
  open_payload.client_id = 0;
  open_payload.version = version;
  open_payload.version_min = version_min;

  service->opened = false;
  service->fourcc = fourcc;

  vchiq_handmade_prep_msg(state, VCHIQ_MSG_OPEN, service->localport, 0,
    &open_payload, sizeof(open_payload));

  vchiq_event_signal(&state->remote->trigger);
  os_event_wait(&state->state_waitflag);
  os_event_clear(&state->state_waitflag);
  BUG_IF(!service->opened, "Service still not opened");
  MMAL_INFO("opened service %08x, localport: %d, remoteport: %d",
    service->fourcc,
    service->localport,
    service->remoteport);
}

static struct vchiq_service_common *vchiq_alloc_open_service(
  struct vchiq_state *state, uint32_t fourcc, short version,
  short version_min, vchiq_data_callback_t data_callback)
{
  struct vchiq_service_common *service;

  service = vchiq_alloc_service();
  if (service)
  {
    vchiq_open_service(state, service, fourcc, version, version_min);
    service->s = state;
    service->data_callback = data_callback;
  }
  return service;
}

static int mmal_io_work_push(struct vchiq_mmal_component *c,
  struct vchiq_mmal_port *p, struct mmal_buffer_header *b, mmal_io_fn fn)
{
  int irqflags;
  struct mmal_io_work *w;
  w = kzalloc(sizeof(*w));
  if (!w)
    return ERR_GENERIC;

  w->c = c;
  w->p = p;
  w->b = b;
  w->fn = fn;
  wmb();
  // spinlock_lock_disable_irq(&mmal_io_work_list_lock, irqflags);
  list_add_tail(&w->list, &mmal_io_work_list);
  os_event_notify(&mmal_io_work_waitflag);
  // spinlock_unlock_restore_irq(&mmal_io_work_list_lock, irqflags);
  return SUCCESS;
}

static inline struct mmal_buffer *mmal_port_get_buffer_from_header(
  struct vchiq_mmal_port *p, struct mmal_buffer_header *h)
{
  struct mmal_buffer *b;
  uint32_t buffer_data;

  list_for_each_entry(b, &p->buffers, list) {
    if (p->zero_copy)
      buffer_data = (uint32_t)(uint64_t)b->vcsm_handle;
    else
      buffer_data = ((uint32_t)(uint64_t)b->buffer) | 0xc0000000;
    if (buffer_data == h->data)
      return b;
  }

  MMAL_ERR("buffer not found for data: %08x", h->data);
  return NULL;
}

struct mmal_msg_context {
  union {
    struct {
      int msg_type;
      struct event completion_waitflag;
      struct mmal_msg *rmsg;
      int rmsg_size;
    } sync;
    struct {
      struct vchiq_mmal_port *port;
    } bulk;
  } u;
};

#define MMAL_MSG_CONTEXT_INIT_SYNC(__msg_type) \
{ \
  .u.sync = { \
    .msg_type = MMAL_MSG_TYPE_ ## __msg_type,\
    .completion_waitflag = EVENT_INIT,\
    .rmsg = NULL \
  }\
}

#define VCHIQ_MMAL_MSG_DECL_ASYNC(__ms, __msg_type, __msg_u) \
  struct mmal_msg_context ctx = MMAL_MSG_CONTEXT_INIT_SYNC(__msg_type); \
  struct mmal_msg msg = { 0 }; \
  struct vchiq_service_common *_ms = __ms; \
  struct mmal_msg_ ## __msg_u *m = &msg.u. __msg_u

#define VCHIQ_MMAL_MSG_DECL(__ms, __mmal_msg_type, __msg_u, __msg_u_reply) \
  VCHIQ_MMAL_MSG_DECL_ASYNC(__ms, __mmal_msg_type, __msg_u); \
  struct mmal_msg_ ## __msg_u_reply *r;

static struct mmal_msg_context *mmal_context_buffer[12] = { 0 };

static uint32_t mmal_msg_context_to_handle(struct mmal_msg_context *ctx)
{
  for (int i = 0; i < ARRAY_SIZE(mmal_context_buffer); ++i) {
    if (!mmal_context_buffer[i]) {
      mmal_context_buffer[i] = ctx;
      return i;
    }
  }
  return UINT32_MAX;
}

static struct mmal_msg_context *mmal_msg_context_from_handle(uint32_t handle)
{
  struct mmal_msg_context *result;

  if (handle >= ARRAY_SIZE(mmal_context_buffer)
    || !mmal_context_buffer[handle])
    return NULL;

  result = mmal_context_buffer[handle];
  mmal_context_buffer[handle] = NULL;
  return result;
}

static uint32_t vchiq_service_to_handle(struct vchiq_service_common *s)
{
  return s->localport;
}

static void vchiq_mmal_fill_header(struct vchiq_service_common *service,
  int mmal_msg_type, struct mmal_msg *msg, struct mmal_msg_context *ctx)
{
  msg->h.magic = MMAL_MAGIC;
  msg->h.context = mmal_msg_context_to_handle(ctx);
  msg->h.control_service = vchiq_service_to_handle(service);
  msg->h.status = 0;
  msg->h.padding = 0;
  msg->h.type = mmal_msg_type;
}

#define VCHIQ_MMAL_MSG_COMMUNICATE_ASYNC() \
  os_event_init(&ctx.u.sync.completion_waitflag); \
  vchiq_mmal_fill_header(_ms, ctx.u.sync.msg_type, &msg, &ctx); \
  vchiq_handmade_prep_msg(_ms->s, VCHIQ_MSG_DATA, _ms->localport, \
    _ms->remoteport, &msg, sizeof(struct mmal_msg_header) + sizeof(*m)); \
  vchiq_event_signal(&_ms->s->remote->trigger);

static const char *const mmal_msg_type_names[] = {
  "UNKNOWN",
  "QUIT",
  "SERVICE_CLOSED",
  "GET_VERSION",
  "COMPONENT_CREATE",
  "COMPONENT_DESTROY",
  "COMPONENT_ENABLE",
  "COMPONENT_DISABLE",
  "PORT_INFO_GET",
  "PORT_INFO_SET",
  "PORT_ACTION",
  "BUFFER_FROM_HOST",
  "BUFFER_TO_HOST",
  "GET_STATS",
  "PORT_PARAMETER_SET",
  "PORT_PARAMETER_GET",
  "EVENT_TO_HOST",
  "GET_CORE_STATS_FOR_PORT",
  "OPAQUE_ALLOCATOR",
  "CONSUME_MEM",
  "LMK",
  "OPAQUE_ALLOCATOR_DESC",
  "DRM_GET_LHS32",
  "DRM_GET_TIME",
  "BUFFER_FROM_HOST_ZEROLEN",
  "PORT_FLUSH",
  "HOST_LOG",
};

static inline void *mmal_check_reply_msg(struct mmal_msg *rmsg, int msg_type)
{
  if (rmsg->h.type != msg_type) {
    MMAL_ERR("mmal msg expected type %s, received: %s",
      mmal_msg_type_names[msg_type],
      mmal_msg_type_names[rmsg->h.type]);
    return NULL;
  }
  return &rmsg->u;
}

#define VCHIQ_MMAL_MSG_COMMUNICATE_SYNC() \
  VCHIQ_MMAL_MSG_COMMUNICATE_ASYNC(); \
  os_event_wait(&ctx.u.sync.completion_waitflag); \
  os_event_clear(&ctx.u.sync.completion_waitflag); \
  r = mmal_check_reply_msg(ctx.u.sync.rmsg, ctx.u.sync.msg_type); \
  if (!r) { \
    MMAL_ERR("invalid reply");\
    return ERR_GENERIC; \
  } \
  if (r->status != MMAL_MSG_STATUS_SUCCESS) { \
    MMAL_ERR("status not success: %d", r->status); \
    return ERR_GENERIC; \
  }

static int vchiq_mmal_buffer_from_host(struct vchiq_mmal_port *p,
  struct mmal_buffer *b)
{
  VCHIQ_MMAL_MSG_DECL_ASYNC(p->component->ms, BUFFER_FROM_HOST,
    buffer_from_host);

  memset(m, 0xbc, sizeof(*m));

  m->drvbuf.magic = MMAL_MAGIC;
  m->drvbuf.component_handle = p->component->handle;
  m->drvbuf.port_handle = p->handle;
  m->drvbuf.client_context = (uint32_t)((uint64_t)p & ~0xffff000000000000);

  m->is_zero_copy = p->zero_copy;
  m->buffer_header.next = 0;
  m->buffer_header.priv = 0;
  m->buffer_header.cmd = 0;
  m->buffer_header.user_data =(uint32_t)(uint64_t)b;
  if (p->zero_copy)
    m->buffer_header.data = (uint32_t)(uint64_t)b->vcsm_handle;
  else
    m->buffer_header.data = ((uint32_t)(uint64_t)b->buffer) | 0xc0000000;
  m->buffer_header.alloc_size = b->buffer_size;

  if (p->type == MMAL_PORT_TYPE_OUTPUT) {
    m->buffer_header.length = 0;
    m->buffer_header.offset = 0;
    m->buffer_header.flags = 0;
    m->buffer_header.pts = MMAL_TIME_UNKNOWN;
    m->buffer_header.dts = MMAL_TIME_UNKNOWN;
  } else {
    m->buffer_header.length = b->length;
    m->buffer_header.offset = 0;
    m->buffer_header.flags = b->mmal_flags;
    m->buffer_header.pts = b->pts;
    m->buffer_header.dts = b->dts;
  }

  memset(&m->buffer_header_type_specific, 0,
    sizeof(m->buffer_header_type_specific));
  m->payload_in_message = 0;

  VCHIQ_MMAL_MSG_COMMUNICATE_ASYNC();
  return SUCCESS;
}

static int mmal_port_buffer_send_one(struct vchiq_mmal_port *p,
  struct mmal_buffer *b)
{
  int err;
  struct mmal_buffer *pb;

  list_for_each_entry(pb, &p->buffers, list) {
    if (pb == b) {
      err = vchiq_mmal_buffer_from_host(p, pb);
      if (err) {
        MMAL_ERR("failed to submit port buffer to VC");
        return err;
      }
      return SUCCESS;
    }
  }

  MMAL_ERR("Invalid buffer in request: %p: %08x", b, b->buffer);
  return ERR_INVAL;
}

static int vchiq_mmal_port_parameter_set(struct vchiq_mmal_component *c,
  struct vchiq_mmal_port *p, uint32_t parameter_id, void *value,
  int value_size)
{
  VCHIQ_MMAL_MSG_DECL(c->ms, PORT_PARAMETER_SET, port_parameter_set, port_parameter_set_reply);

  /* GET PARAMETER CAMERA INFO */
  m->component_handle = c->handle;
  m->port_handle = p->handle;
  m->id = parameter_id;
  m->size = 2 * sizeof(uint32_t) + value_size;
  memcpy(&m->value, value, value_size);

  VCHIQ_MMAL_MSG_COMMUNICATE_SYNC();
  if (parameter_id == MMAL_PARAMETER_ZERO_COPY)
    p->zero_copy = 1;
  return SUCCESS;
}

static int mmal_camera_capture_frames(struct vchiq_mmal_component *cam,
  struct vchiq_mmal_port *capture_port)
{
  uint32_t frame_count = 1;

  return vchiq_mmal_port_parameter_set(cam, capture_port,
    MMAL_PARAMETER_CAPTURE, &frame_count, sizeof(frame_count));
}

static int mmal_port_buffer_io_work(struct vchiq_mmal_component *c,
  struct vchiq_mmal_port *p, struct mmal_buffer_header *h)
{
  int err;
  struct mmal_buffer *b;

  /*
   * Find the buffer in a list of buffers bound to port
   */
  b = mmal_port_get_buffer_from_header(p, h);
  BUG_IF(!b, "failed to find buffer");

  /*
   * Buffer payload
   */
  if (h->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END) {
    /* MMAL_INFO("Received non-EOS, pushing to display"); */
    // tft_lcd_print_data(b->buffer, h->length);
  }

  err = mmal_port_buffer_send_one(p, b);
  CHECK_ERR("Failed to submit buffer");

  if (h->flags & MMAL_BUFFER_HEADER_FLAG_EOS) {
    // MMAL_INFO("EOS received, sending CAPTURE command");
    err = mmal_camera_capture_frames(c, p);
    CHECK_ERR("Failed to initiate frame capture");
  }

  return SUCCESS;
out_err:
  return err;
}

static inline void mmal_buffer_header_make_flags_string(
  struct mmal_buffer_header *h, char *buf, int bufsz)
{
  int n = 0;

#define CHECK_FLAG(__name) \
  if (h->flags & MMAL_BUFFER_HEADER_FLAG_ ## __name) { \
    if (n != 0 && (bufsz - n >= 2)) { \
      buf[n++] = ','; \
      buf[n++] = ' '; \
    } \
    strncpy(buf + n, #__name, MIN(sizeof(#__name), bufsz - n));\
  }

  CHECK_FLAG(EOS);
  CHECK_FLAG(FRAME_START);
  CHECK_FLAG(FRAME_END);
  CHECK_FLAG(KEYFRAME);
  CHECK_FLAG(DISCONTINUITY);
  CHECK_FLAG(CONFIG);
  CHECK_FLAG(ENCRYPTED);
  CHECK_FLAG(CODECSIDEINFO);
  CHECK_FLAG(SNAPSHOT);
  CHECK_FLAG(CORRUPTED);
  CHECK_FLAG(TRANSMISSION_FAILED);
#undef CHECK_FLAG
}

static inline void mmal_buffer_print_meta(struct mmal_buffer_header *h)
{
  char flagsbuf[256];
  mmal_buffer_header_make_flags_string(h, flagsbuf, sizeof(flagsbuf));
  MMAL_DEBUG2("buffer_header: %p,hdl:%08x,addr:%08x,sz:%d/%d,%s", h,
    h->data, h->user_data, h->alloc_size, h->length, flagsbuf);
}

static int mmal_buffer_to_host_cb(struct mmal_msg *rmsg)
{
  int err;
  struct vchiq_mmal_port *p;

  struct mmal_msg_buffer_from_host *r;

  r = (struct mmal_msg_buffer_from_host *)&rmsg->u;
  p = (struct vchiq_mmal_port *)(uint64_t)(r->drvbuf.client_context | 0xffff000000000000);
  mmal_buffer_print_meta(&r->buffer_header);

  err = mmal_io_work_push(p->component, p, &r->buffer_header,
    mmal_port_buffer_io_work);

  CHECK_ERR("Failed to schedule mmal io work");
out_err:
  return err;
}

static int mmal_event_to_host_cb(const struct mmal_msg *rmsg)
{
  return SUCCESS;
}

static int mmal_service_data_callback(struct vchiq_service_common *s,
  struct vchiq_header *h)
{
  struct mmal_msg *rmsg;
  struct mmal_msg_context *msg_ctx;

  rmsg = (struct mmal_msg *)h->data;

  if (rmsg->h.type == MMAL_MSG_TYPE_BUFFER_TO_HOST)
    return mmal_buffer_to_host_cb(rmsg);

  if (rmsg->h.type == MMAL_MSG_TYPE_EVENT_TO_HOST)
    return mmal_event_to_host_cb(rmsg);

  msg_ctx = mmal_msg_context_from_handle(rmsg->h.context);

  if (msg_ctx->u.sync.msg_type != rmsg->h.type) {
    MMAL_ERR("mmal msg expected type %s, received: %s",
      mmal_msg_type_names[rmsg->h.type],
      mmal_msg_type_names[msg_ctx->u.sync.msg_type]);
    return ERR_INVAL;
  }
  msg_ctx->u.sync.rmsg = rmsg;
  msg_ctx->u.sync.rmsg_size = h->size;
  os_event_notify(&msg_ctx->u.sync.completion_waitflag);

  return SUCCESS;
}

#define VCHIQ_MAKE_FOURCC(x0, x1, x2, x3) \
  (((x0) << 24) | ((x1) << 16) | ((x2) << 8) | (x3))

#define VC_MMAL_VER 15
#define VC_MMAL_MIN_VER 10

#define VC_SM_VER  1
#define VC_SM_MIN_VER 0

static inline struct vchiq_service_common *vchiq_open_mmal_service(
  struct vchiq_state *state)
{
  return vchiq_alloc_open_service(state, MAKE_FOURCC("mmal"), VC_MMAL_VER,
    VC_MMAL_MIN_VER, mmal_service_data_callback);
}

struct mems_msg_context {
  bool active;
  uint32_t trans_id;
  struct event completion_waitflag;
  void *data;
  int data_size;
};

static uint32_t mems_transaction_counter = 0;

#define MEMS_CONTEXT_POOL_SIZE 4
static
struct mems_msg_context mems_msg_context_pool[MEMS_CONTEXT_POOL_SIZE] = { 0 };

struct mems_msg_context *mems_msg_context_from_trans_id(uint32_t trans_id)
{
  int i;
  struct mems_msg_context *ctx;
  for (i = 0; i < ARRAY_SIZE(mems_msg_context_pool); ++i) {
    ctx = &mems_msg_context_pool[i];
    if (ctx->trans_id == trans_id) {
      if (!ctx->active) {
        MMAL_ERR("mems data callback for inactive message");
        return NULL;
      }
      return ctx;
    }
  }
  MMAL_ERR("mems data callback for non-existent context");
  return NULL;
}

int mems_service_data_callback(struct vchiq_service_common *s,
  struct vchiq_header *h)
{
  struct vc_sm_result_t *r;
  struct mems_msg_context *ctx;
  BUG_IF(h->size < 4, "mems message less than 4");
  r = (struct vc_sm_result_t *)&h->data[0];
  ctx = mems_msg_context_from_trans_id(r->trans_id);
  BUG_IF(!ctx, "mem transaction ctx problem");
  ctx->data = h->data;
  ctx->data_size = h->size;
  os_event_notify(&ctx->completion_waitflag);
  return SUCCESS;
}

static inline struct vchiq_service_common *vchiq_open_smem_service(
  struct vchiq_state *state)
{
  return vchiq_alloc_open_service(state, MAKE_FOURCC("SMEM"), VC_SM_VER,
    VC_SM_MIN_VER, mems_service_data_callback);
}

#define VCHI_VERSION(v_) { v_, v_ }
#define VCHI_VERSION_EX(v_, m_) { v_, m_ }

enum {
  COMP_CAMERA = 0,
  COMP_PREVIEW,
  COMP_IMAGE_ENCODE,
  COMP_VIDEO_ENCODE,
  COMP_COUNT
};

enum {
  CAM_PORT_PREVIEW = 0,
  CAM_PORT_VIDEO,
  CAM_PORT_CAPTURE,
  CAM_PORT_COUNT
};

#define MAX_SUPPORTED_ENCODINGS 20

static int vchiq_mmal_port_info_get(struct vchiq_mmal_component *c,
  struct vchiq_mmal_port *p)
{
  VCHIQ_MMAL_MSG_DECL(c->ms, PORT_INFO_GET, port_info_get, port_info_get_reply);

  m->component_handle = c->handle;
  m->index = p->index;
  m->port_type = p->type;

  VCHIQ_MMAL_MSG_COMMUNICATE_SYNC();
  if (r->port.is_enabled)
    p->enabled = 1;
  else
    p->enabled = 0;

  p->handle = r->port_handle;
  p->type = r->port_type;
  p->index = r->port_index;

  p->minimum_buffer.num = r->port.buffer_num_min;
  p->minimum_buffer.size = r->port.buffer_size_min;
  p->minimum_buffer.alignment = r->port.buffer_alignment_min;

  p->recommended_buffer.alignment = r->port.buffer_alignment_min;
  p->recommended_buffer.num = r->port.buffer_num_recommended;

  p->current_buffer.num = r->port.buffer_num;
  p->current_buffer.size = r->port.buffer_size;

  /* stream format */
  p->format.type = r->format.type;
  p->format.encoding = r->format.encoding;
  p->format.encoding_variant = r->format.encoding_variant;
  p->format.bitrate = r->format.bitrate;
  p->format.flags = r->format.flags;

  /* elementary stream format */
  memcpy(&p->es, &r->es, sizeof(union mmal_es_specific_format));
  p->format.es = &p->es;

  p->format.extradata_size = r->format.extradata_size;
  memcpy(p->format.extradata, r->extradata, p->format.extradata_size);
  p->component = c;
  return SUCCESS;
}

static int mmal_port_create(struct vchiq_mmal_component *c,
  struct vchiq_mmal_port *p, enum mmal_port_type type, int index)
{
  int err;

  /* Type and index are needed for port info get */
  p->type = type;
  p->index = index;

  err = vchiq_mmal_port_info_get(c, p);
  CHECK_ERR("Failed to get port info");

  INIT_LIST_HEAD(&p->buffers);
  p->component = c;
  return SUCCESS;
out_err:
  return err;
}

static int mmal_get_version(struct vchiq_service_common *mmal_service)
{
  struct mmal_msg_context ctx = {
    .u.sync = {
      .msg_type = MMAL_MSG_TYPE_GET_VERSION,
      .completion_waitflag = EVENT_INIT,
      .rmsg = NULL
    }
  };

  struct mmal_msg msg = { 0 };

  vchiq_mmal_fill_header(mmal_service, ctx.u.sync.msg_type, &msg, &ctx);

  vchiq_handmade_prep_msg(mmal_service->s, VCHIQ_MSG_DATA,
    mmal_service->localport, mmal_service->remoteport,
    &msg, sizeof(struct mmal_msg_header));

  vchiq_event_signal(&mmal_service->s->remote->trigger);

  os_event_wait(&ctx.u.sync.completion_waitflag);
  os_event_clear(&ctx.u.sync.completion_waitflag);
#if 0
  //r = mmal_check_reply_msg(ctx.u.sync.rmsg, ctx.u.sync.msg_type);
  if (!r) {
    MMAL_ERR("invalid reply");
    return ERR_GENERIC;
  }
  if (r->status != MMAL_MSG_STATUS_SUCCESS) {
    MMAL_ERR("status not success: %d", r->status);
    return ERR_GENERIC;
  }
#endif
  return 0;
}

static int mmal_component_create(struct vchiq_service_common *mmal_service,
  const char *name, int component_idx, struct vchiq_mmal_component *c)
{
  VCHIQ_MMAL_MSG_DECL(mmal_service, COMPONENT_CREATE, component_create,
    component_create_reply);

  m->client_component = component_idx;
  strncpy(m->name, name, sizeof(m->name));

  VCHIQ_MMAL_MSG_COMMUNICATE_SYNC();

  c->handle = r->component_handle;
  c->ms = mmal_service;
  c->enabled = true;
  c->inputs = r->input_num;
  c->outputs = r->output_num;
  c->clocks = r->clock_num;
  return SUCCESS;
}

static int mmal_component_destroy(struct vchiq_service_common *mmal_service,
  struct vchiq_mmal_component *c)
{
  return SUCCESS;
}

struct vchiq_mmal_component *component_create(
  struct vchiq_service_common *mmal_service, const char *name)
{
  int err, i;
  struct vchiq_mmal_component *c;
  c = kzalloc(sizeof(*c));
  if (!c) {
    MMAL_ERR("failed to allocate memory for mmal_component");
    return NULL;
  }

  if (mmal_component_create(mmal_service, name, 0, c) != SUCCESS) {
    MMAL_ERR("Component not created '%s'", name);
    return NULL;
  }

  strncpy(c->name, name, sizeof(c->name));

  MMAL_INFO("vchiq component created name:%s: handle: %d, input: %d,"
    " output: %d, clock: %d", c->name, c->handle, c->inputs, c->outputs,
    c->clocks);

  err = mmal_port_create(c, &c->control, MMAL_PORT_TYPE_CONTROL, 0);
  if (err != SUCCESS)
    goto err_component_destroy;

  for (i = 0; i < c->inputs; ++i) {
    err = mmal_port_create(c, &c->input[i], MMAL_PORT_TYPE_INPUT, i);
    if (err != SUCCESS)
      goto err_component_destroy;
  }

  for (i = 0; i < c->outputs; ++i) {
    err = mmal_port_create(c, &c->output[i], MMAL_PORT_TYPE_OUTPUT, i);
    if (err != SUCCESS)
      goto err_component_destroy;
  }

  return c;
err_component_destroy:
  mmal_component_destroy(mmal_service, c);

err_component_free:
  kfree(c);
  return NULL;
}

static inline struct vchiq_mmal_component *vchiq_mmal_create_camera_info(
  struct vchiq_service_common *mmal_service)
{
  return component_create(mmal_service, "camera_info");
}

static int vchiq_mmal_port_parameter_get(struct vchiq_mmal_component *c,
  struct vchiq_mmal_port *port, int parameter_id, void *value,
  uint32_t *value_size)
{
  VCHIQ_MMAL_MSG_DECL(c->ms, PORT_PARAMETER_GET, port_parameter_get,
    port_parameter_get_reply);

  m->component_handle = c->handle;
  m->port_handle = port->handle;
  m->id = parameter_id;
  m->size = 2 * sizeof(uint32_t) + *value_size;

  VCHIQ_MMAL_MSG_COMMUNICATE_SYNC();

  memcpy(value, r->value, MIN(r->size, *value_size));
  *value_size = r->size;
  return SUCCESS;
}

static inline int vchiq_mmal_get_camera_info(struct vchiq_service_common *ms,
  struct vchiq_mmal_component *c, struct mmal_cam_info *cam_info)
{
  int err;
  uint32_t param_size;

  param_size = sizeof(*cam_info);
  err = vchiq_mmal_port_parameter_get(c, &c->control,
    MMAL_PARAMETER_CAMERA_INFO, cam_info, &param_size);
  return err;
}

static void vchiq_mmal_cam_info_print(struct mmal_cam_info *cam_info)
{
  int i;
  struct mmal_parameter_camera_info_camera_t *c;

  for (i = 0; i < cam_info->num_cameras; ++i) {
    c = &cam_info->cameras[i];
    MMAL_INFO("cam %d: name:%s, W x H: %dx%x\r\n",
      i, c->camera_name, c->max_width, c->max_height);
  }
}

static int vchiq_mmal_handmade_component_disable(
  struct vchiq_mmal_component *c)
{
  VCHIQ_MMAL_MSG_DECL(c->ms, COMPONENT_DISABLE, component_disable,
    component_disable_reply);

  BUG_IF(!c->enabled,
    "trying to disable mmal component, which is already disabled");
  m->component_handle = c->handle;

  VCHIQ_MMAL_MSG_COMMUNICATE_SYNC();
  c->enabled = false;
  MMAL_INFO("vchiq_mmal_handmade_component_disable, name:%s, handle:%d",
    c->name, c->handle);
  return SUCCESS;
}

static int vchiq_mmal_handmade_component_destroy(
  struct vchiq_service_common *ms, struct vchiq_mmal_component *c)
{
  VCHIQ_MMAL_MSG_DECL(c->ms, COMPONENT_DESTROY, component_destroy,
    component_destroy_reply);

  BUG_IF(c->enabled,
    "trying to destroy mmal component, which is not disabled first");
  m->component_handle = c->handle;

  VCHIQ_MMAL_MSG_COMMUNICATE_SYNC();
  MMAL_INFO("vchiq_mmal_handmade_component_destroy, name:%s, handle:%d",
    c->name, c->handle);
  kfree(c);
  return SUCCESS;
}

static int vchiq_mmal_get_cam_info(struct vchiq_service_common *ms,
  struct mmal_cam_info *cam_info)
{
  int err;
  struct vchiq_mmal_component *camera_info;

  camera_info = vchiq_mmal_create_camera_info(ms);
  err = vchiq_mmal_get_camera_info(ms, camera_info, cam_info);
  CHECK_ERR("Failed to get camera info");

  vchiq_mmal_cam_info_print(cam_info);
  err = vchiq_mmal_handmade_component_disable(camera_info);
  CHECK_ERR("Failed to disable 'camera info' component");

  err = vchiq_mmal_handmade_component_destroy(ms, camera_info);
  CHECK_ERR("Failed to destroy 'camera info' component");
  return SUCCESS;

out_err:
  return err;
}

static int mmal_set_camera_parameters(struct vchiq_mmal_component *c,
  struct mmal_parameter_camera_info_camera_t *cam_info)
{
  int ret;
  uint32_t config_size;
  struct mmal_parameter_camera_config config = {
    .max_stills_w = cam_info->max_width,
    .max_stills_h = cam_info->max_height,
    .stills_yuv422 = 1,
    .one_shot_stills = 1,
    .max_preview_video_w = 320,
    .max_preview_video_h = 240,
    .num_preview_video_frames = 3,
    .stills_capture_circular_buffer_height = 0,
    .fast_preview_resume = 0,
    .use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RAW_STC
  };

  struct mmal_parameter_camera_config new_config = { 0 };

  config_size = sizeof(new_config);

  ret = vchiq_mmal_port_parameter_set(c, &c->control,
    MMAL_PARAMETER_CAMERA_CONFIG, &config, sizeof(config));
  config_size = sizeof(new_config);
  ret = vchiq_mmal_port_parameter_get(c, &c->control,
    MMAL_PARAMETER_CAMERA_CONFIG, &new_config, &config_size);
  return SUCCESS;
}

static inline void mmal_print_supported_encodings(uint32_t *encodings, int num)
{
  int i, ii;
  char buf[5];
  buf[4] = 0;
  char *ptr = (char *)encodings;
  for (i = 0; i < num; ++i) {
    for (ii = 0; ii < 4; ++ii) {
      buf[ii] = ptr[ii];
    }
    ptr += 4;
    MMAL_INFO("supported_encoding: %s", buf);
  }
}

static int mmal_port_get_supp_encodings(struct vchiq_mmal_port *p,
  uint32_t *encodings, int max_encodings, int *num_encodings)
{
  int err;
  uint32_t param_size;

  param_size = max_encodings * sizeof(*encodings);
  err = vchiq_mmal_port_parameter_get(p->component, p,
    MMAL_PARAMETER_SUPPORTED_ENCODINGS, encodings, &param_size);
  CHECK_ERR("Failed to get supported_encodings");
  *num_encodings = param_size / sizeof(*encodings);
  mmal_print_supported_encodings(encodings, *num_encodings);
  return SUCCESS;
out_err:
  return err;
}

static void vchiq_mmal_local_format_fill(struct mmal_es_format_local *f, int encoding, int encoding_variant, int width, int height)
{
  f->encoding = encoding;
  f->encoding_variant = encoding_variant;
  f->es->video.width = width;
  f->es->video.height = height;
  f->es->video.crop.x = 0;
  f->es->video.crop.y = 0;
  f->es->video.crop.width = width;
  f->es->video.crop.height = height;
  f->es->video.frame_rate.num = 1;
  f->es->video.frame_rate.den = 0;
}

static void port_to_mmal_msg(struct vchiq_mmal_port *port, struct mmal_port *p)
{
  /* todo do readonly fields need setting at all? */
  p->type = port->type;
  p->index = port->index;
  p->index_all = 0;
  p->is_enabled = port->enabled;
  p->buffer_num_min = port->minimum_buffer.num;
  p->buffer_size_min = port->minimum_buffer.size;
  p->buffer_alignment_min = port->minimum_buffer.alignment;
  p->buffer_num_recommended = port->recommended_buffer.num;
  p->buffer_size_recommended = port->recommended_buffer.size;

  /* only three writable fields in a port */
  p->buffer_num = port->current_buffer.num;
  p->buffer_size = port->current_buffer.size;
  p->userdata = (uint32_t)(unsigned long)port;
}

static int vchiq_mmal_port_info_set(struct vchiq_mmal_component *c,
  struct vchiq_mmal_port *p)
{
  VCHIQ_MMAL_MSG_DECL(c->ms, PORT_INFO_SET, port_info_set,
    port_info_set_reply);

  m->component_handle = c->handle;
  m->port_type = p->type;
  m->port_index = p->index;

  port_to_mmal_msg(p, &m->port);
  m->format.type = p->format.type;
  m->format.encoding = p->format.encoding;
  m->format.encoding_variant = p->format.encoding_variant;
  m->format.bitrate = p->format.bitrate;
  m->format.flags = p->format.flags;

  memcpy(&m->es, &p->es, sizeof(union mmal_es_specific_format));

  m->format.extradata_size = p->format.extradata_size;
  memcpy(&m->extradata, p->format.extradata, p->format.extradata_size);
  VCHIQ_MMAL_MSG_COMMUNICATE_SYNC();
  return SUCCESS;
}

static int vchiq_mmal_port_set_format(struct vchiq_mmal_component *c,
  struct vchiq_mmal_port *port)
{
  int err;

  err = vchiq_mmal_port_info_set(c, port);
  CHECK_ERR("failed to set port info");

  err = vchiq_mmal_port_info_get(c, port);
  CHECK_ERR("failed to get port info");

out_err:
  return err;
}

static inline int mmal_port_set_zero_copy(struct vchiq_mmal_component *c,
  struct vchiq_mmal_port *p)
{
  uint32_t zero_copy = 1;
  return vchiq_mmal_port_parameter_set(c, p, MMAL_PARAMETER_ZERO_COPY,
    &zero_copy, sizeof(zero_copy));
}

static int vchiq_mmal_component_enable(struct vchiq_mmal_component *c)
{
  VCHIQ_MMAL_MSG_DECL(c->ms, COMPONENT_ENABLE, component_enable,
    component_enable_reply);

  m->component_handle = c->handle;
  VCHIQ_MMAL_MSG_COMMUNICATE_SYNC();
  return SUCCESS;
}

static int mmal_camera_enable(struct vchiq_mmal_component *cam)
{
  int err;
  err = vchiq_mmal_component_enable(cam);
  os_wait_ms(300);
  return SUCCESS;

out_err:
  return err;
}

static int vchiq_mmal_port_action_port(struct vchiq_mmal_component *c,
  struct vchiq_mmal_port *p, int action)
{
  VCHIQ_MMAL_MSG_DECL(c->ms, PORT_ACTION, port_action_port, port_action_reply);

  m->component_handle = c->handle;
  m->port_handle = p->handle;
  m->action = action;
  port_to_mmal_msg(p, &m->port);

  VCHIQ_MMAL_MSG_COMMUNICATE_SYNC();
  return SUCCESS;
}

static int vchiq_mmal_port_enable(struct vchiq_mmal_port *p)
{
  int err;

  if (p->enabled) {
    MMAL_INFO("skipping port already enabled");
    return SUCCESS;
  }
  err = vchiq_mmal_port_action_port(p->component, p, MMAL_MSG_PORT_ACTION_TYPE_ENABLE);
  CHECK_ERR("port action type enable failed");
  p->enabled = 1;

out_err:
  return vchiq_mmal_port_info_get(p->component, p);
}

static struct mems_msg_context *mems_msg_context_alloc(void)
{
  int i;
  struct mems_msg_context *ctx;

  for (i = 0; i < ARRAY_SIZE(mems_msg_context_pool); ++i) {
    ctx = &mems_msg_context_pool[i];
    if (!ctx->active) {
      ctx->active = true;
      ctx->data = NULL;
      ctx->trans_id = mems_transaction_counter++;
      os_event_init(&ctx->completion_waitflag);
      return ctx;
    }
  }
  return NULL;
}

static void mems_msg_context_free(struct mems_msg_context *ctx)
{
  if (ctx > &mems_msg_context_pool[MEMS_CONTEXT_POOL_SIZE]
    || ctx < &mems_msg_context_pool[0])
    panic_with_log("mems context free error");
  ctx->active = 0;
}

static int vc_sm_cma_vchi_send_msg(struct vchiq_service_common *mems_service,
  enum vc_sm_msg_type msg_id, void *msg,
  uint32_t msg_size, void *result, uint32_t result_size,
  uint32_t *cur_trans_id, uint8_t wait_reply)
{
  int err;
  char buf[256];
  struct vc_sm_msg_hdr_t *hdr = (struct vc_sm_msg_hdr_t *)buf;
  struct mems_msg_context *ctx;
  ctx = mems_msg_context_alloc();
  if (!ctx) {
    err = ERR_GENERIC;
    MMAL_ERR("Failed to allocate mems message context");
    return err;
  }
  // struct sm_cmd_rsp_blk *cmd;
  hdr->type = msg_id;
  hdr->trans_id = vc_trans_id++;

  if (msg_size)
    memcpy(hdr->body, msg, msg_size);

  // cmd = vc_vchi_cmd_create(msg_id, msg, msg_size, 1);
  // cmd->sent = 1;

  vchiq_handmade_prep_msg(
    mems_service->s,
    VCHIQ_MSG_DATA,
    mems_service->localport,
    mems_service->remoteport,
    hdr,
    msg_size + sizeof(*hdr));

  vchiq_event_signal(&mems_service->s->remote->trigger);
  os_event_wait(&ctx->completion_waitflag);
  os_event_clear(&ctx->completion_waitflag);

  memcpy(result, ctx->data, ctx->data_size);
  mems_msg_context_free(ctx);
  return SUCCESS;
}

static int vc_sm_cma_vchi_import(struct vchiq_service_common *mems_service,
  struct vc_sm_import *msg, struct vc_sm_import_result *result,
  uint32_t *cur_trans_id)
{
  return vc_sm_cma_vchi_send_msg(mems_service, VC_SM_MSG_TYPE_IMPORT, msg,
   sizeof(*msg), result, sizeof(*result), cur_trans_id, 1);
}

static int vc_sm_cma_import_dmabuf(struct vchiq_service_common *mems_service,
  struct mmal_buffer *b, void **vcsm_handle)
{
  int err;
  struct vc_sm_import import;
  struct vc_sm_import_result result;
  uint32_t cur_trans_id = 0;
  import.type = VC_SM_ALLOC_NON_CACHED;
  import.allocator = 0x66aa;
  import.addr = (uint32_t)(uint64_t)b->buffer | 0xc0000000;
  import.size = b->buffer_size;
  import.kernel_id = 0x6677;

  memcpy(import.name, VC_SM_RESOURCE_NAME_DEFAULT,
    sizeof(VC_SM_RESOURCE_NAME_DEFAULT));

  err = vc_sm_cma_vchi_import(mems_service, &import, &result, &cur_trans_id);
  CHECK_ERR("Failed to import buffer to vc");

  MMAL_INFO("imported_dmabuf: addr:%08x, size: %d, trans_id: %08x,"
    " res.trans_id: %08x, res.handle: %08x",
    import.addr, import.size, cur_trans_id, result.trans_id,
    result.res_handle);

  *vcsm_handle = (void*)(uint64_t)result.res_handle;

  return SUCCESS;

out_err:
  return err;
}

static int mmal_alloc_port_buffers(struct vchiq_service_common *mems_service,
  struct vchiq_mmal_port *p)
{
  int err;
  int i;
  struct mmal_buffer *buf;

  for (i = 0; i < p->minimum_buffer.num * 2; ++i) {
    buf = kzalloc(sizeof(*buf));
    buf->buffer_size = p->minimum_buffer.size;
    buf->buffer = dma_alloc(buf->buffer_size);;
    MMAL_INFO("min_num: %d, min_sz:%d, min_al:%d, port_enabled:%s, buf:%08x",
      p->minimum_buffer.num,
      p->minimum_buffer.size,
      p->minimum_buffer.alignment,
      p->enabled ? "yes" : "no",
      buf->buffer);

    if (p->zero_copy) {
      err = vc_sm_cma_import_dmabuf(mems_service, buf, &buf->vcsm_handle);
      CHECK_ERR("failed to import dmabuf");
    }
    list_add_tail(&buf->list, &p->buffers);
  }
  return SUCCESS;

out_err:
  return err;
}

static int mmal_port_buffer_send_all(struct vchiq_mmal_port *p)
{
  int err;
  struct mmal_buffer *b;

  if (list_empty(&p->buffers)) {
    MMAL_ERR("port buffer list is empty");
    return ERR_RESOURCE;
  }

  list_for_each_entry(b, &p->buffers, list) {
    err = vchiq_mmal_buffer_from_host(p, b);
    if (err) {
      MMAL_ERR("failed to submit port buffer to VC");
      return err;
    }
  }

  return SUCCESS;
}

static int mmal_port_buffer_receive(struct vchiq_mmal_port *p)
{
  while(1) {
    asm volatile ("wfe");
    os_yield();
  }
  return SUCCESS;
}

static int vchiq_camera_run(struct vchiq_service_common *mmal_service,
  struct vchiq_service_common *mems_service,
  int frame_width, int frame_height)
{
  int err;
  struct mmal_cam_info cam_info = {0};
  struct vchiq_mmal_component *cam;
  struct vchiq_mmal_port *still_port, *video_port, *preview_port;
  uint32_t supported_encodings[MAX_SUPPORTED_ENCODINGS];
  int num_encodings;
  uint32_t param;

  err = vchiq_mmal_get_cam_info(mmal_service, &cam_info);
  CHECK_ERR("Failed to get num cameras");

  /* mmal_init start */
  cam = component_create(mmal_service, "ril.camera");
  CHECK_ERR_PTR(cam, "Failed to create component 'ril.camera'");

  param = 0;
  err = vchiq_mmal_port_parameter_set(cam, &cam->control,
    MMAL_PARAMETER_CAMERA_NUM, &param, sizeof(param));

  err = vchiq_mmal_port_parameter_set(cam, &cam->control,
    MMAL_PARAMETER_CAMERA_CUSTOM_SENSOR_CONFIG, &param, sizeof(param));

  err = vchiq_mmal_port_enable(&cam->control);

  err =  mmal_set_camera_parameters(cam, &cam_info.cameras[0]);
  CHECK_ERR("Failed to set parameters to component 'ril.camera'");

  still_port = &cam->output[CAM_PORT_CAPTURE];
  video_port = &cam->output[CAM_PORT_VIDEO];
  preview_port = &cam->output[CAM_PORT_PREVIEW];

  err = mmal_port_get_supp_encodings(still_port, supported_encodings,
    sizeof(supported_encodings), &num_encodings);

  CHECK_ERR("Failed to retrieve supported encodings from port");

  vchiq_mmal_local_format_fill(&still_port->format,
    /* MMAL_ENCODING_I420 */ MMAL_ENCODING_RGB24, 0, frame_width,
    frame_height);

  err = vchiq_mmal_port_set_format(cam, still_port);
  CHECK_ERR("Failed to set format for still capture port");

  vchiq_mmal_local_format_fill(&video_port->format, MMAL_ENCODING_OPAQUE,
    0, frame_width, frame_height);

  err = vchiq_mmal_port_set_format(cam, video_port);
  CHECK_ERR("Failed to set format for video capture port");

  vchiq_mmal_local_format_fill(&preview_port->format, MMAL_ENCODING_OPAQUE,
    0, frame_width, frame_height);

  err = vchiq_mmal_port_set_format(cam, preview_port);
  CHECK_ERR("Failed to set format for preview capture port");

  err = mmal_port_set_zero_copy(cam, still_port);
  CHECK_ERR("Failed to set zero copy param for camera port");
  err = mmal_camera_enable(cam);
  CHECK_ERR("Failed to enable component camera");

  err = vchiq_mmal_port_enable(still_port);
  CHECK_ERR("Failed to enable video_port");

  err = mmal_alloc_port_buffers(mems_service, still_port);
  CHECK_ERR("Failed to prepare buffer for still port");

  while(1) {
    err = mmal_port_buffer_send_all(still_port);
    CHECK_ERR("Failed to send buffer to port");
    mmal_camera_capture_frames(cam, still_port);
    CHECK_ERR("Failed to initiate frame capture");
    err = mmal_port_buffer_receive(still_port);
    CHECK_ERR("Failed to receive buffer from VC");
  }
  return SUCCESS;

out_err:
  return err;
}

static int vchiq_handmade_connect(struct vchiq_state *s)
{
  struct vchiq_header *h;
  int msg_size;
  int err;

  /* Send CONNECT MESSAGE */
  msg_size = 0;
  h = vchiq_prep_next_header_tx(s, msg_size);

  h->msgid = VCHIQ_MAKE_MSG(VCHIQ_MSG_CONNECT, 0, 0);
  h->size = msg_size;

  wmb();
  /* Make the new tx_pos visible to the peer. */
  s->local->tx_pos = s->local_tx_pos;
  wmb();

  vchiq_event_signal(&s->remote->trigger);

  if (s->conn_state != VCHIQ_CONNSTATE_CONNECTED) {
    err = ERR_GENERIC;
    goto out_err;
  }

  return SUCCESS;

out_err:
  return err;
}

static void vchiq_handmade(struct vchiq_state *s, struct vchiq_slot_zero *z)
{
  int err;
  struct vchiq_service_common *smem_service, *mmal_service;

  err = vchiq_start_thread(s);
  CHECK_ERR("failed to start vchiq async primitives");
  os_event_wait(&s->state_waitflag);
  os_event_clear(&s->state_waitflag);
  err = vchiq_handmade_connect(s);

  mmal_service = vchiq_open_mmal_service(s);
  BUG_IF(!mmal_service, "failed to open mmal service");

  smem_service = vchiq_open_smem_service(s);
  BUG_IF(!smem_service, "failed at open smem service");

  err = vchiq_camera_run(mmal_service, smem_service, 320, 240);
  CHECK_ERR("failed to run camera");

out_err:
  while(1)
    asm volatile("wfe");
}

int vchiq_init(void)
{
  struct vchiq_slot_zero *vchiq_slot_zero;
  void *slot_mem;
  uint32_t slot_phys;
  uint32_t channelbase;
  int slot_mem_size, frag_mem_size;
  int err;
  uint32_t fragment_size;
  struct vchiq_state *s = &vchiq_state;
  char *fragments_baseaddr;

  /* fragment size granulariry is cache line size */
  fragment_size = 2 * 64;
  /* Allocate space for the channels in coherent memory */
  /* TOTAL_SLOTS */
  slot_mem_size = 80 * VCHIQ_SLOT_SIZE;
  frag_mem_size = fragment_size * MAX_FRAGMENTS;

  slot_mem = dma_alloc(slot_mem_size + frag_mem_size);
  slot_phys = (uint32_t)(uint64_t)slot_mem;
  if (!slot_mem) {
    printf("failed to allocate DMA memory");
   return ERR_MEMALLOC;
  }

  vchiq_slot_zero = vchiq_init_slots(slot_mem, slot_mem_size);
  if (!vchiq_slot_zero)
    return ERR_INVAL;

  vchiq_init_state(s, vchiq_slot_zero);

  vchiq_slot_zero->platform_data[VCHIQ_PLATFORM_FRAGMENTS_OFFSET_IDX]
    = ((uint32_t)slot_phys) + slot_mem_size;

  vchiq_slot_zero->platform_data[VCHIQ_PLATFORM_FRAGMENTS_COUNT_IDX]
    = MAX_FRAGMENTS;

  fragments_baseaddr = (char *)slot_mem + slot_mem_size;
  vchiq_init_fragments(fragments_baseaddr, MAX_FRAGMENTS, fragment_size);

  asm volatile("dsb sy");
  /* Send the base address of the slots to VideoCore */
  channelbase = 0xc0000000 | slot_phys;

  if (!mbox_init_vchiq(&channelbase)) {
    LOG(0, ERR, "vchiq", "failed to set channelbase");
    return ERR_GENERIC;
  }

  asm volatile("dsb sy");

  s->conn_state = VCHIQ_CONNSTATE_DISCONNECTED;
  vchiq_handmade(s, vchiq_slot_zero);

  return SUCCESS;
}
