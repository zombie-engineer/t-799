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
#include <ili9341.h>
#include <bitmap.h>
#include <fs/fat32.h>
#include <block_device.h>

#define BELL0 ((ioreg32_t)(0x3f00b840))
#define BELL2 ((ioreg32_t)(0x3f00b848))

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

#define VCHIQ_MAX_SLOTS          128
#define VCHIQ_MAX_SLOTS_PER_SIDE 64

#define VCHIQ_NUM_CURRENT_BULKS 32
#define VCHIQ_NUM_SERVICE_BULKS 4

#define VCHIQ_ENABLE_DEBUG 1

#define MAX_FRAGMENTS (VCHIQ_NUM_CURRENT_BULKS * 2)

#define VCHIQ_PLATFORM_FRAGMENTS_OFFSET_IDX 0
#define VCHIQ_PLATFORM_FRAGMENTS_COUNT_IDX  1

enum {
  CAM_PORT_PREVIEW = 0,
  CAM_PORT_VIDEO,
  CAM_PORT_CAPTURE,
  CAM_PORT_COUNT
};

#define MAX_SUPPORTED_ENCODINGS 20

struct vchiq_open_payload {
  int fourcc;
  int client_id;
  short version;
  short version_min;
} PACKED;

struct vchiq_mmal_port_buffer {
  /* number of buffers */
  unsigned int num;
  /* size of buffers */
  uint32_t size;
  /* alignment of buffers */
  uint32_t alignment;
};

struct vchiq_mmal_port {
  char name[32];
  uint32_t enabled : 1;
  uint32_t zero_copy : 1;
  uint32_t handle;
  /* port type, cached to use on port info set */
  uint32_t type;
  /* port index, cached to use on port info set */
  uint32_t index;

  /* component port belongs to, allows simple deref */
  struct vchiq_mmal_component *component;

  /* port conencted to */
  struct vchiq_mmal_port *connected;

  /* buffer info */
  struct vchiq_mmal_port_buffer minimum_buffer;
  struct vchiq_mmal_port_buffer recommended_buffer;
  struct vchiq_mmal_port_buffer current_buffer;

  /* stream format */
  struct mmal_es_format_local format;

  /* elementary stream format */
  union mmal_es_specific_format es;

  /*
   * Preallocated free buffers on our side, they are not used but if required
   * we can release them to the remote side (vcos)
   */
  struct list_head buffers_free;

  /*
   * Buffers that are released to remote side (vcos). At some point they will
   * be sent to us by BUFFER_TO_HOST message
   */
  struct list_head buffers_busy;

  /*
   * Buffers that are send to use by BUFFER_TO_HOST and are currently being
   * processed by us. After processing is done these will be sent to
   * buffers_free
   */
  struct list_head buffers_in_process;

  /* callback context */
  void *cb_ctx;
};

#define MAX_PORT_COUNT 4
struct vchiq_mmal_component {
  struct list_head list;
  char name[32];

  uint32_t in_use:1;
  uint32_t enabled:1;
  /* VideoCore handle for component */
  uint32_t handle;
  /* Number of input ports */
  uint32_t inputs;
  /* Number of output ports */
  uint32_t outputs;
  /* Number of clock ports */
  uint32_t clocks;
  /* control port */
  struct vchiq_mmal_port control;
  /* input ports */
  struct vchiq_mmal_port input[MAX_PORT_COUNT];
  /* output ports */
  struct vchiq_mmal_port output[MAX_PORT_COUNT];
  /* clock ports */
  struct vchiq_mmal_port clock[MAX_PORT_COUNT];
  /* Used to ref back to client struct */
  uint32_t client_component;
  struct vchiq_service_common *ms;
};


static int vc_trans_id = 0;
static int frame_num = 0;
static size_t frame_offset = 0;
static struct block_device *bdev = NULL;
void vchiq_set_blockdev(struct block_device *bd)
{
  bdev = bd;
}

static struct vchiq_state vchiq_state;

static struct list_head mmal_io_work_list;
static struct event mmal_io_work_waitflag;

typedef int (*mmal_io_fn)(struct vchiq_mmal_component *,
  struct vchiq_mmal_port *, struct mmal_buffer_header *);

int mmal_log_level = LOG_LEVEL_INFO;

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
  VCHIQ_SERVICE_OPENED,        /* service, -, -             */
  VCHIQ_SERVICE_CLOSED,        /* service, -, -             */
  VCHIQ_MESSAGE_AVAILABLE,     /* service, header, -        */
  VCHIQ_BULK_TRANSMIT_DONE,    /* service, -, bulk_userdata */
  VCHIQ_BULK_RECEIVE_DONE,     /* service, -, bulk_userdata */
  VCHIQ_BULK_TRANSMIT_ABORTED, /* service, -, bulk_userdata */
  VCHIQ_BULK_RECEIVE_ABORTED   /* service, -, bulk_userdata */
} vchiq_reason_t;

struct vchiq_header {
  /* The message identifier - opaque to applications. */
  int msgid;

  /* Size of message data */
  unsigned int size;

  /* message */
  char data[0];
};

/*
 * Payload is aligned to header size, so that header would always start at
 * granule of its size
 */
#define VCHIQ_MSG_HDR_SZ_MASK 7
#define VCHIQ_MSG_HDR_SZ_INV_MASK 0xfffffff8

#define VCHIQ_MSG_TOTAL_SIZE(__payload_sz) \
  ((__payload_sz + sizeof(struct vchiq_header) + VCHIQ_MSG_HDR_SZ_MASK) \
    & VCHIQ_MSG_HDR_SZ_INV_MASK)

typedef void (*vchiq_userdata_term_t)(void *userdata);

typedef uint32_t vchi_mem_handle_t;

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

  struct event trigger_waitflag;
  struct event recycle_waitflag;
  struct event sync_trigger_waitflag;
  struct event sync_release_waitflag;
  struct event state_waitflag;

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
  int slot_queue_available;

  struct list_head components;
};

struct mmal_io_work {
  struct list_head list;
  struct mmal_buffer_header *buffer_header;
  struct vchiq_mmal_component *component;
  struct vchiq_mmal_port *port;
  mmal_io_fn fn;
  int idx;
};

#define MAX_MMAL_WORKS 12

struct mmal_state {
  struct mmal_io_work work_array[MAX_MMAL_WORKS];
  struct bitmap work_bitmap;
  uint64_t work_bitmap_data[BITMAP_NUM_WORDS(MAX_MMAL_WORKS)];
};

static struct vchiq_state vchiq_state;
static struct mmal_state mmal_state;

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

#define SLOT_DATA_FROM_INDEX(state, index) (state->slots + (index))

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

  /* initialize shared state pointers */
  state->local = local;
  state->remote = remote;
  state->slots = (struct vchiq_slot *)slot_zero;

  os_event_init(&state->trigger_waitflag);
  os_event_init(&state->recycle_waitflag);
  os_event_init(&state->sync_trigger_waitflag);
  os_event_init(&state->sync_release_waitflag);
  os_event_init(&state->state_waitflag);
  INIT_LIST_HEAD(&state->components);
  state->slot_queue_available = 0;

  for (i = local->slot_first; i <= local->slot_last; i++)
    local->slot_queue[state->slot_queue_available++] = i;

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

struct mmal_io_work *mmal_io_work_pop(void)
{
  int irqflags;
  struct mmal_io_work *w = NULL;
  if (list_empty(&mmal_io_work_list))
    return NULL;

  w = list_first_entry(&mmal_io_work_list, typeof(*w), list);
  list_del_init(&w->list);
  wmb();
  return w;
}

static struct mmal_io_work *mmal_io_work_alloc(void)
{
  struct mmal_io_work *w;

  int idx = bitmap_set_next_free(&mmal_state.work_bitmap);
  if (idx != -1) {
    w = &mmal_state.work_array[idx];
    w->idx = idx;
  }

  return w;
}

static void mmal_io_work_free(struct mmal_io_work *w)
{
  bitmap_clear_entry(&mmal_state.work_bitmap, w->idx);
}

static void vchiq_io_thread(void)
{
  struct mmal_io_work *w;
  while(1) {
    os_event_wait(&mmal_io_work_waitflag);
    os_event_clear(&mmal_io_work_waitflag);
    w = mmal_io_work_pop();
    if (w) {
      w->fn(w->component, w->port, w->buffer_header);
      mmal_io_work_free(w);
    }
  }
}

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
  bell_reg = ioreg32_read(BELL0);
  if (bell_reg & 4)
    vchiq_check_local_events();
}

void vchiq_ring_bell(void)
{
  ioreg32_write(BELL2, 0);
}

static int vchiq_tx_header_to_slot_idx(struct vchiq_state *s)
{
  return s->remote->slot_queue[(s->rx_pos / VCHIQ_SLOT_SIZE) & VCHIQ_SLOT_QUEUE_MASK];
}

struct vchiq_header *vchiq_get_next_header_rx(struct vchiq_state *state)
{
  struct vchiq_header *h;
  int rx_pos, slot_queue_index, slot_index;

next_header:
  slot_queue_index = ((int)((unsigned int)(state->rx_pos) / VCHIQ_SLOT_SIZE));
  slot_index = state->remote->slot_queue[slot_queue_index & VCHIQ_SLOT_QUEUE_MASK];
  state->rx_data = (char *)(state->slots + slot_index);
  rx_pos = state->rx_pos;
  h = (struct vchiq_header *)&state->rx_data[rx_pos & VCHIQ_SLOT_MASK];
  state->rx_pos += VCHIQ_MSG_TOTAL_SIZE(h->size);
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
      s->is_connected = true;
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
      VCHIQ_MSG_TOTAL_SIZE(h->size));
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
  char *slots;
  struct vchiq_header *h;

  slot_queue_available = s->slot_queue_available;

  mb();

  while (slot_queue_available != s->local->slot_queue_recycle) {
    slot_index = s->local->slot_queue[slot_queue_available & VCHIQ_SLOT_QUEUE_MASK];
    slot_queue_available++;
    slots = (char *)&s->slots[slot_index];

    rmb();

    pos = 0;
    while (pos < VCHIQ_SLOT_SIZE) {
      h = (struct vchiq_header *)(slots + pos);

      pos += VCHIQ_MSG_TOTAL_SIZE(h->size);
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
  }
}

static void vchiq_sync_release_thread(void)
{
  struct vchiq_state *s;

  s = &vchiq_state;
  while (1) {
    vchiq_event_wait(&s->sync_release_waitflag, &s->local->sync_release);
    // vchiq_process_free_queue(s);
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

  t = task_create(vchiq_sync_release_thread, "vchiq_sync_release");
  CHECK_ERR_PTR(t, "Failed to start vchiq_sync_release");
  os_schedule_task(t);

  return SUCCESS;

out_err:
  bcm2835_ic_disable_irq(BCM2835_IRQNR_ARM_DOORBELL_0);
  irq_set(BCM2835_IRQNR_ARM_DOORBELL_0, 0);
  return err;
}

static struct vchiq_header *vchiq_prep_next_header_tx(struct vchiq_state *s,
  size_t msg_size)
{
  int tx_pos, slot_queue_index, slot_index;
  struct vchiq_header *h;
  size_t slot_space;
  size_t msg_full_size;

  char *slot;

  /*
   * Recall last position for tx
   * Answer to question why there is s->local_tx_pos and s->local->tx_pos and
   * why we don't update s->local->tx_pos in this function:
   * s->local->tx_pos is a shared field, that can be read by VideoCore firmware
   * at any moment. When it detects 'tx_pos' incremented it is assumes message
   * is ready.
   * In this function message is not ready, before message, including header
   * and payload are both fully written s->local->tx_pos should not be
   * incremented.
   * Because we already have calculated the value for the new tx_pos here,
   * we cache in s->local_tx_pos, the caller of this function will fill the
   * message and update tx_pos from this s->local_tx_pos
   */
  tx_pos = s->local->tx_pos;
  msg_full_size = VCHIQ_MSG_TOTAL_SIZE(msg_size);

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
  slot_queue_index = tx_pos / VCHIQ_SLOT_SIZE;
  if (slot_space < msg_full_size) {
    BUG_IF(slot_space < sizeof(*h),
      "Trailing slot space less than header size");

    slot_index = s->local->slot_queue[slot_queue_index & VCHIQ_SLOT_QUEUE_MASK];
    slot = (char *)&s->slots[slot_index];
    h = (struct vchiq_header *)(slot + (tx_pos & VCHIQ_SLOT_MASK));
    h->msgid = VCHIQ_MSGID_PADDING;
    h->size = slot_space - sizeof(*h);
    tx_pos += slot_space;
    slot_queue_index++;
  }

  slot_index = s->local->slot_queue[slot_queue_index & VCHIQ_SLOT_QUEUE_MASK];
  slot = (char *)&s->slots[slot_index];

  /* just prepare the header, all the filling is done outside */
  h = (struct vchiq_header *)(slot + (tx_pos & VCHIQ_SLOT_MASK));

  tx_pos += msg_full_size;

  MMAL_DEBUG("prep_next_msg:%d(%d:%d)->%d(%d:%d)",
    s->local->tx_pos,
    s->local->slot_queue[(s->local->tx_pos / VCHIQ_SLOT_SIZE) & VCHIQ_SLOT_QUEUE_MASK],
    s->local_tx_pos & VCHIQ_SLOT_MASK,
    tx_pos, slot_index, tx_pos & VCHIQ_SLOT_MASK);

  s->local_tx_pos = tx_pos;
  return h;
}

static void vchiq_handmade_prep_msg(struct vchiq_state *s, int msgid,
  int srcport, int dstport, void *payload, int payload_sz)
{
  struct vchiq_header *h;
  int old_tx_pos = s->local->tx_pos;

  h = vchiq_prep_next_header_tx(s, payload_sz);
  h->msgid = VCHIQ_MAKE_MSG(msgid, srcport, dstport);
  h->size = payload_sz;
  memcpy(h->data, payload, payload_sz);
  wmb();
  /* Make the new tx_pos visible to the peer. */
  s->local->tx_pos = s->local_tx_pos;
  wmb();
  MMAL_DEBUG("msg sent: %p, tx_pos: %d, size: %d", h, old_tx_pos,
    VCHIQ_MSG_TOTAL_SIZE(h->size));
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

  w = mmal_io_work_alloc();
  if (!w)
    return ERR_GENERIC;

  w->component = c;
  w->port = p;
  w->buffer_header = b;
  w->fn = fn;
  wmb();
  list_add_tail(&w->list, &mmal_io_work_list);
  os_event_notify(&mmal_io_work_waitflag);
  return SUCCESS;
}

#if 0
static inline struct mmal_buffer *mmal_port_get_buffer_from_header(
  struct vchiq_mmal_port *p, struct mmal_buffer_header *h)
{
  struct mmal_buffer *b;
  uint32_t buffer_data;

  list_for_each_entry(b, &p->buffers_free, list) {
    if (p->zero_copy)
      buffer_data = (uint32_t)(uint64_t)b->vcsm_handle;
    else
      buffer_data = (uint32_t)(uint64_t)b->buffer;
    if (buffer_data == h->user_data)
      return b;
  }

  MMAL_ERR("buffer not found for data: %08x", h->data);
  return NULL;
}
#endif

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
  return (uint32_t)((uint64_t)ctx & ~0xffff000000000000);
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
  return (struct mmal_msg_context *)(uint64_t)(handle | 0xffff000000000000);
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
  /*
   * Kernel code in bcm2835-camera.c states this is only possible for enabled
   * port
   */
  if (!p->enabled)
    return ERR_INVAL;

  VCHIQ_MMAL_MSG_DECL_ASYNC(p->component->ms, BUFFER_FROM_HOST,
    buffer_from_host);

  memset(m, 0, sizeof(*m));

  m->drvbuf.magic = MMAL_MAGIC;
  m->drvbuf.component_handle = p->component->handle;
  m->drvbuf.port_handle = p->handle;
  m->drvbuf.client_context = (uint32_t)((uint64_t)b & ~0xffff000000000000);

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

struct mmal_port_param_stats {
   uint32_t buffer_count;           /**< Total number of buffers processed */
   uint32_t frame_count;            /**< Total number of frames processed */
   uint32_t frames_skipped;         /**< Number of frames without expected PTS based on frame rate */
   uint32_t frames_discarded;       /**< Number of frames discarded */
   uint32_t eos_seen;               /**< Set if the end of stream has been reached */
   uint32_t maximum_frame_bytes;    /**< Maximum frame size in bytes */
   int64_t  total_bytes;            /**< Total number of bytes processed */
   uint32_t corrupt_macroblocks;    /**< Number of corrupt macroblocks in the stream */
};

struct mmal_port_param_core_stats {
   uint32_t buffer_count;        /**< Total buffer count on this port */
   uint32_t first_buffer_time;   /**< Time (us) of first buffer seen on this port */
   uint32_t last_buffer_time;    /**< Time (us) of most recently buffer on this port */
   uint32_t max_delay;           /**< Max delay (us) between buffers, ignoring first few frames */
};

static int vchiq_mmal_port_parameter_get(struct vchiq_mmal_component *c,
  struct vchiq_mmal_port *port, int parameter_id, void *value,
  uint32_t *value_size);

static int mmal_port_get_stats(struct vchiq_mmal_port *p)
{
  int err;
  struct mmal_port_param_stats stats;
  uint32_t size;
  struct mmal_port_param_core_stats core_stats;
  uint32_t pool_mem_alloc_size;

  size = sizeof(stats);
  err = vchiq_mmal_port_parameter_get(p->component, p,
    MMAL_PARAMETER_STATISTICS, &stats, &size);
  CHECK_ERR("Failed to get port stats");

  MMAL_INFO("b:%d,fski:%d,fdis:%d,eos:%d,maxb:%d,tot:%d,cmb:%d",
   stats.buffer_count, stats.frame_count,
   stats.frames_skipped,
   stats.frames_discarded,
   stats.frames_discarded,
   stats.eos_seen,
   stats.maximum_frame_bytes,
   stats.total_bytes,
   stats.corrupt_macroblocks
   );

#if 0
  size = sizeof(core_stats);
  err = vchiq_mmal_port_parameter_get(p->component, p,
    MMAL_PARAMETER_CORE_STATISTICS, &core_stats, &size);
  CHECK_ERR("Failed to get port core stats");

  size = sizeof(pool_mem_alloc_size);
  err = vchiq_mmal_port_parameter_get(p->component, p,
    MMAL_PARAMETER_MEM_USAGE, &pool_mem_alloc_size, &size);
  CHECK_ERR("Failed to get port mem usage");

#endif

out_err:
  return err;

  return SUCCESS;
}

static int mmal_port_buffer_send_one(struct vchiq_mmal_port *p,
  struct mmal_buffer *b)
{
  int err;
  err = vchiq_mmal_buffer_from_host(p, b);
  if (err) {
    MMAL_ERR("failed to submit port buffer to VC");
    return err;
  }
  return SUCCESS;

#if 0
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
#endif
  return ERR_INVAL;
}

static int vchiq_mmal_port_parameter_set(struct vchiq_mmal_port *p,
  uint32_t parameter_id, void *value, int value_size)
{
  VCHIQ_MMAL_MSG_DECL(p->component->ms, PORT_PARAMETER_SET, port_parameter_set,
    port_parameter_set_reply);

  /* GET PARAMETER CAMERA INFO */
  m->component_handle = p->component->handle;
  m->port_handle = p->handle;
  m->id = parameter_id;
  m->size = 2 * sizeof(uint32_t) + value_size;
  memcpy(&m->value, value, value_size);

  VCHIQ_MMAL_MSG_COMMUNICATE_SYNC();
  if (parameter_id == MMAL_PARAMETER_ZERO_COPY)
    p->zero_copy = 1;
  return SUCCESS;
}

static int num_capture_frames = 0;

static int mmal_camera_capture_frames(struct vchiq_mmal_port *p)
{
  uint32_t frame_count = 1;

  // MMAL_INFO("capture_frames %d", num_capture_frames++);
  return vchiq_mmal_port_parameter_set(p, MMAL_PARAMETER_CAPTURE,
    &frame_count, sizeof(frame_count));
}

#define IO_MIN_SECTORS 8192
#define H264BUF_SIZE (IO_MIN_SECTORS * 512)

struct camera {
  char *current_buf;
  char *current_buf_ptr;
  int write_sector_offset;
  char *io_buffers[2];
  struct event next_buf_avail;
};

static struct camera cam = {
  0
};

static void h264_sectors_write_complete(int err)
{
  os_event_notify(&cam.next_buf_avail);
}

static int mmal_port_buffer_io_work(struct vchiq_mmal_component *c,
  struct vchiq_mmal_port *p, struct mmal_buffer_header *h)
{
  int err;
  struct mmal_buffer *b;

  b = (void *)(uint64_t)(h->user_data | 0xffff000000000000);
  if (!p->zero_copy) {
    goto buffer_return;
  }

  size_t bytes_left = h->length;
  const char *src = b->buffer;

  while (bytes_left) {
    size_t buffer_left = cam.current_buf + H264BUF_SIZE - cam.current_buf_ptr;
    size_t io_sz = MIN(bytes_left, buffer_left);

#if 0
    MMAL_INFO("%d +%d bytes %08x %08x", cam.current_buf_ptr - cam.current_buf,
      io_sz, ((uint32_t *)b->buffer)[0],
      ((uint32_t *)b->buffer)[1]);
#endif

    memcpy(cam.current_buf_ptr, src, io_sz);
    cam.current_buf_ptr += io_sz;
    bytes_left -= io_sz;
    src += io_sz;

    if (cam.current_buf_ptr == cam.current_buf + H264BUF_SIZE) {

      struct blockdev_io io = {
        .dev = bdev,
        .is_write = true,
        .addr = cam.current_buf,
        .start_sector = cam.write_sector_offset,
        .num_sectors = IO_MIN_SECTORS,
        .cb = h264_sectors_write_complete
      };

      if (cam.current_buf == cam.io_buffers[0])
        cam.current_buf = cam.io_buffers[1];
      else
        cam.current_buf = cam.io_buffers[0];

      cam.current_buf_ptr = cam.current_buf;

      os_event_wait(&cam.next_buf_avail);
      blockdev_scheduler_push_io(&io);
      os_event_clear(&cam.next_buf_avail);
      cam.write_sector_offset += IO_MIN_SECTORS;
    }
  }

  if (h->flags & MMAL_BUFFER_HEADER_FLAG_KEYFRAME) {
  }

  if (h->flags & MMAL_BUFFER_HEADER_FLAG_CONFIG) {
    h->length = 0;
    goto buffer_return;
  }
  /*
   * Buffer payload
   */
  if (h->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END) {
    int err;
    frame_num++;
    if (frame_num % 20)
      mmal_port_get_stats(p);
#if 0
    MMAL_INFO("Done frame %d", frame_num);
    ili9341_draw_bitmap(b->buffer, h->length);
#endif
  }

buffer_return:
  list_del(&b->list);
  list_add_tail(&b->list, &p->buffers_free);
  if (h->flags & MMAL_BUFFER_HEADER_FLAG_EOS) {
    MMAL_DEBUG2("EOS received, sending CAPTURE command");
    err = mmal_camera_capture_frames(p);
    CHECK_ERR("Failed to initiate frame capture");
  }

  return SUCCESS;
out_err:
  return err;
}

static inline void mmal_buffer_flags_to_string(
  struct mmal_buffer_header *h, char *buf, int bufsz)
{
  int n = 0;

  if (!bufsz)
    return;

  /* No chance to write something meaningful */
  if (bufsz < 5)
    goto out;

#define CHECK_FLAG(__name) \
  if (h->flags & MMAL_BUFFER_HEADER_FLAG_ ## __name) { \
    if (n && (bufsz - n >= 2)) { \
      buf[n++] = '|'; \
    } \
    size_t len = MIN(sizeof(#__name) - 1, bufsz - n); \
    strncpy(buf + n, #__name, len);\
    n += len; \
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
  CHECK_FLAG(DECODEONLY);
  CHECK_FLAG(NAL_END);
#undef CHECK_FLAG
  if (n >= bufsz)
    n = bufsz - 1;

out:
  buf[n] = 0;
}

static inline void mmal_buffer_print_meta(struct mmal_buffer_header *h)
{
  char flagsbuf[256];
  mmal_buffer_flags_to_string(h, flagsbuf, sizeof(flagsbuf));
  MMAL_INFO("buffer_header: %p,hdl:%08x,addr:%08x,sz:%d/%d,flags:%0x,'%s'", h,
    h->data, h->user_data, h->alloc_size, h->length, h->flags, flagsbuf);
}

static struct vchiq_mmal_port *mmal_port_get_by_handle(
  struct vchiq_mmal_component *c, uint32_t handle)
{
  size_t i;

  if (c->control.handle == handle)
    return &c->control;

  for (i = 0; i < c->inputs; ++i) {
    if (c->input[i].handle == handle)
      return &c->input[i];
  }

  for (i = 0; i < c->outputs; ++i) {
    if (c->output[i].handle == handle)
      return &c->output[i];
  }
  return NULL;
}

static int mmal_buffer_to_host_cb(const struct mmal_msg *rmsg)
{
  int err;
  struct vchiq_mmal_port *p = NULL;
  struct vchiq_mmal_component *c;

  struct mmal_msg_buffer_from_host *r;

  r = (struct mmal_msg_buffer_from_host *)&rmsg->u;
  list_for_each_entry(c, &vchiq_state.components, list) {
    if (c->handle == r->drvbuf.component_handle)
      break;
  }

  if (c->handle != r->drvbuf.component_handle) {
    MMAL_ERR("Failed to find component for buffer");
    return ERR_NOT_FOUND;
  }

  p = mmal_port_get_by_handle(c, r->drvbuf.port_handle);
  if (!p) {
    MMAL_ERR("Failed to find component for buffer");
    return ERR_NOT_FOUND;
  }

  struct mmal_buffer *b = list_first_entry(&p->buffers_busy,
    struct mmal_buffer, list);

  list_del(&b->list);
  list_add_tail(&b->list, &p->buffers_in_process);

  r->buffer_header.user_data =(uint32_t)(uint64_t)b;

  mmal_buffer_print_meta(&r->buffer_header);

  err = mmal_io_work_push(p->component, p, &r->buffer_header,
    mmal_port_buffer_io_work);

  b = list_first_entry(&p->buffers_free, struct mmal_buffer, list);
  list_del(&b->list);
  list_add_tail(&b->list, &p->buffers_busy);
  err = mmal_port_buffer_send_one(p, b);
  CHECK_ERR("Failed to submit buffer");
out_err:
  return err;
}

static int mmal_event_to_host_cb(const struct mmal_msg *rmsg)
{
  const struct mmal_msg_event_to_host *m = &rmsg->u.event_to_host;
  MMAL_INFO("event_to_host: comp:%d,port type:%05x,num:%d,cmd:%08x,length:%d",
    m->client_component, m->port_type, m->port_num, m->cmd, m->length);
  for (size_t i = 0; i < m->length; ++i) {
    printf(" %02x", m->data[i]);
    if (i & ((i % 8) == 0))
      printf("\r\n");
  }

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

static void mmal_format_print(const char *action, const char *name,
  const struct mmal_es_format_local *f)
{
  const char *src0 = (const char *)&f->encoding;
  const char *src1 = (const char *)&f->encoding_variant;
  char str_enc[] = { src0[0], src0[1], src0[2], src0[3], 0 };
  char str_enc_var[] = { src1[0], src1[1], src1[2], src1[3], 0 };

  MMAL_INFO("%s:%s:tp:%d %s/%s,bitrate:%d,%dx%d,f:%02x", action, name,
    f->type, str_enc, str_enc_var, f->bitrate, f->es->video.width,
    f->es->video.height, f->flags);
}

static int vchiq_mmal_port_info_get(struct vchiq_mmal_port *p)
{
  VCHIQ_MMAL_MSG_DECL(p->component->ms, PORT_INFO_GET, port_info_get,
    port_info_get_reply);

  m->component_handle = p->component->handle;
  m->index = p->index;
  m->port_type = p->type;

  VCHIQ_MMAL_MSG_COMMUNICATE_SYNC();

  p->enabled = r->port.is_enabled ? 1 : 0;
  p->handle = r->port_handle;
  p->type = r->port_type;
  p->index = r->port_index;

  p->minimum_buffer.num = r->port.buffer_num_min;
  p->minimum_buffer.size = r->port.buffer_size_min;
  p->minimum_buffer.alignment = r->port.buffer_alignment_min;

  p->recommended_buffer.alignment = r->port.buffer_alignment_min;
  p->recommended_buffer.num = r->port.buffer_num_recommended;
  p->recommended_buffer.size = r->port.buffer_size_recommended;

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

  mmal_format_print("actual", p->name, &p->format);
  MMAL_INFO(" ena:%d,min:%dx%d,rec:%dx%d,curr:%dx%d", p->enabled,
    p->minimum_buffer.num, p->minimum_buffer.size,
    p->recommended_buffer.num, p->recommended_buffer.size,
    p->current_buffer.num, p->current_buffer.size);

  return SUCCESS;
}

static int mmal_port_create(struct vchiq_mmal_component *c,
  struct vchiq_mmal_port *p, enum mmal_port_type type, int index)
{
  int err;

  /* Type and index are needed for port info get */
  p->component = c;
  p->type = type;
  p->index = index;
  snprintf(p->name, sizeof(p->name), "%s.%d%d", c->name, (int)type, index);

  err = vchiq_mmal_port_info_get(p);
  CHECK_ERR("Failed to get port info");

  INIT_LIST_HEAD(&p->buffers_free);
  INIT_LIST_HEAD(&p->buffers_busy);
  INIT_LIST_HEAD(&p->buffers_in_process);

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
  list_add_tail(&c->list, &vchiq_state.components);

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

struct mmal_parameter_logging
{
   uint32_t set;     /**< Logging bits to set */
   uint32_t clear;   /**< Logging bits to clear */
};

static int vchiq_mmal_get_cam_info(struct vchiq_service_common *ms,
  struct mmal_cam_info *cam_info)
{
  int err;
  struct vchiq_mmal_component *camera_info;
  struct vchiq_mmal_component *cam;
  struct mmal_parameter_logging l = { .set = 0x1, .clear = 0 };

  camera_info = vchiq_mmal_create_camera_info(ms);

  err = vchiq_mmal_port_parameter_set(&camera_info->control,
    MMAL_PARAMETER_LOGGING, &l, sizeof(l));

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
  struct mmal_parameter_camera_info_camera_t *cam_info, int width, int height)
{
  int ret;
  uint32_t config_size;
  struct mmal_parameter_camera_config config = {
    .max_stills_w = width,
    .max_stills_h = height,
    .stills_yuv422 = 0,
    .one_shot_stills = 0,
    .max_preview_video_w = width,
    .max_preview_video_h = height,
    .num_preview_video_frames = 3,
    .stills_capture_circular_buffer_height = 0,
    .fast_preview_resume = 0,
    .use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RAW_STC
  };

  struct mmal_parameter_camera_config new_config = { 0 };

  config_size = sizeof(new_config);

  ret = vchiq_mmal_port_parameter_set(&c->control,
    MMAL_PARAMETER_CAMERA_CONFIG, &config, sizeof(config));

  config_size = sizeof(new_config);
  ret = vchiq_mmal_port_parameter_get(c, &c->control,
    MMAL_PARAMETER_CAMERA_CONFIG, &new_config, &config_size);
  return ret;
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

static void mmal_format_set(struct mmal_es_format_local *f,
  int encoding, int encoding_variant, int width, int height, int framerate,
  int bitrate)
{
  f->encoding = encoding;
  f->encoding_variant = encoding_variant;
  f->es->video.width = width;
  f->es->video.height = height;
  f->es->video.crop.x = 0;
  f->es->video.crop.y = 0;
  f->es->video.crop.width = width;
  f->es->video.crop.height = height;

  /* frame rate taken from RaspiVid.c */
  f->es->video.frame_rate.num = framerate;
  f->es->video.frame_rate.den = 1;

  f->bitrate = bitrate;
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

static int vchiq_mmal_port_info_set(struct vchiq_mmal_port *p)
{
  VCHIQ_MMAL_MSG_DECL(p->component->ms, PORT_INFO_SET, port_info_set,
    port_info_set_reply);

  m->component_handle = p->component->handle;
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

static int mmal_port_set_format(struct vchiq_mmal_port *p)
{
  int err;

  mmal_format_print("requested", p->name, &p->format);
  err = vchiq_mmal_port_info_set(p);
  CHECK_ERR("failed to set port info");

  err = vchiq_mmal_port_info_get(p);
  CHECK_ERR("failed to get port info");
out_err:
  return err;
}

static inline int mmal_port_set_zero_copy(struct vchiq_mmal_port *p)
{
  uint32_t zero_copy = 1;
  return vchiq_mmal_port_parameter_set(p, MMAL_PARAMETER_ZERO_COPY,
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

static int mmal_component_enable(struct vchiq_mmal_component *cam)
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

static int vchiq_mmal_port_connect(struct vchiq_mmal_port *src,
  struct vchiq_mmal_port *dst)
{
  VCHIQ_MMAL_MSG_DECL(src->component->ms, PORT_ACTION, port_action_handle,
    port_action_reply);
  m->component_handle = src->component->handle;
  m->port_handle = src->handle;
  m->action = MMAL_MSG_PORT_ACTION_TYPE_CONNECT;
  m->connect_component_handle = dst->component->handle;
  m->connect_port_handle = dst->handle;
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
  err = vchiq_mmal_port_action_port(p->component, p,
    MMAL_MSG_PORT_ACTION_TYPE_ENABLE);

  CHECK_ERR("port action type enable failed");

  err = vchiq_mmal_port_info_get(p);
  CHECK_ERR("port action type enable failed");
  if (!p->enabled) {
    MMAL_ERR("port still not enabled");
    err = ERR_GENERIC;
  }
out_err:
  return err;
}

static int create_port_connection(struct vchiq_mmal_port *in,
  struct vchiq_mmal_port *out, struct vchiq_mmal_port *out2)
{
  int err;
  out->format.encoding = in->format.encoding;
  out->format.encoding_variant = in->format.encoding_variant;
  out->es.video.width = in->es.video.width;
  out->es.video.height = in->es.video.height;
  out->es.video.crop.x = in->es.video.crop.x;
  out->es.video.crop.y = in->es.video.crop.y;
  out->es.video.crop.width = in->es.video.crop.width;
  out->es.video.crop.height = in->es.video.crop.height;
  out->es.video.frame_rate.num = in->es.video.frame_rate.num;
  out->es.video.frame_rate.den = in->es.video.frame_rate.den;
  err = mmal_port_set_format(out);
  CHECK_ERR("Failed to change destination port format");
  if (out->recommended_buffer.num != in->recommended_buffer.num) {
    MMAL_ERR("Buffer info mismatch on connection recommended num %d vs %d",
      out->recommended_buffer.num, in->recommended_buffer.num);
    return ERR_GENERIC;
  }

  if (out->recommended_buffer.size != in->recommended_buffer.size) {
    MMAL_ERR("Buffer info mismatch on connection recommended size %d vs %d",
      out->recommended_buffer.size, in->recommended_buffer.size);
    return ERR_GENERIC;
  }

  err = vchiq_mmal_port_connect(in, out);
  CHECK_ERR("Failed to connect ports");

  err = vchiq_mmal_port_enable(in);
  CHECK_ERR("Failed to enable source port");

out_err:
  return err;
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
  struct vchiq_mmal_port *p, size_t min_buffers)
{
  int err;
  size_t i;
  struct mmal_buffer *buf;
  size_t num_buffers = p->minimum_buffer.num;
  if (num_buffers < min_buffers)
    num_buffers = min_buffers;

  MMAL_INFO("%s: allocating buffers %dx%d to port %s", p->name, num_buffers,
    p->minimum_buffer.size, p->name);

  for (i = 0; i < num_buffers + 1; ++i) {
    buf = dma_alloc(sizeof(*buf));
    buf->buffer_size = p->minimum_buffer.size;
    buf->buffer = dma_alloc(buf->buffer_size);;

    if (p->zero_copy) {
      err = vc_sm_cma_import_dmabuf(mems_service, buf, &buf->vcsm_handle);
      CHECK_ERR("failed to import dmabuf");
    }
    list_add_tail(&buf->list, &p->buffers_free);
  }

  MMAL_INFO("%s: port buf alloc: nr:%d, size:%d, align:%d, port_enabled:%s, %08x",
    p->name,
    p->minimum_buffer.num,
    p->minimum_buffer.size,
    p->minimum_buffer.alignment,
    p->enabled ? "yes" : "no",
    buf->buffer);

  return SUCCESS;

out_err:
  return err;
}

static int mmal_port_buffer_send_all(struct vchiq_mmal_port *p)
{
  int err;
  struct mmal_buffer *b;
  struct list_head *node, *tmp;
  int to_send = p->minimum_buffer.num;

  list_for_each_safe(node, tmp, &p->buffers_free) {
    b = container_of(node, struct mmal_buffer, list);
    list_del(node);
    list_add_tail(node, &p->buffers_busy);
    err = vchiq_mmal_buffer_from_host(p, b);
    CHECK_ERR("failed to submit port buffer to VC");
    to_send--;

    if (!to_send)
      break;
  }

  if (to_send) {
    MMAL_ERR("failed to send all required buffers");
    return ERR_RESOURCE;
  }

  err = vchiq_mmal_port_info_get(p);
  CHECK_ERR("failed to get port info");
  MMAL_INFO("port buf applied: nr:%d, size:%d",
    p->current_buffer.num, p->current_buffer.size);

out_err:
  return err;
}

static void mmal_format_copy(struct mmal_es_format_local *to,
  const struct mmal_es_format_local *from)
{
  void *tmp = to->es;
  *to->es = *from->es;
  *to = *from;
  to->es = tmp;
  to->extradata_size = 0;
  to->extradata = NULL;
}

static int mmal_apply_buffers(struct vchiq_service_common *mems_service,
  struct vchiq_mmal_port *p, bool zero_copy, size_t min_buffers)
{
  int err;

  if (zero_copy) {
    err = mmal_port_set_zero_copy(p);
    CHECK_ERR("Failed to set 'zero copy' for port");
  }

  err = mmal_alloc_port_buffers(mems_service, p, min_buffers);
  CHECK_ERR("Failed to alloc buffers for port");
  err = mmal_port_buffer_send_all(p);
  CHECK_ERR("Failed to send buffers to port");
out_err:
  return err;
}

static int create_resizer(struct vchiq_service_common *mmal_service,
  struct vchiq_service_common *mems_service,
  struct vchiq_mmal_port **resizer_input,
  struct vchiq_mmal_port **resizer_output, int width, int height)
{
  bool bool_arg;
  int err = SUCCESS;
  struct vchiq_mmal_component *resizer;
  struct mmal_parameter_video_profile video_profile;
 
  resizer = component_create(mmal_service, "ril.resize");

  CHECK_ERR_PTR(resizer, "Failed to create component 'ril.video_encode'");
  if (!resizer->inputs || !resizer->outputs) {
    MMAL_ERR("err: resizer has 0 outputs or inputs");
    return ERR_GENERIC;
  }

  err = mmal_component_enable(resizer);
  CHECK_ERR("Failed to enable resizer");

  *resizer_input = &resizer->input[0];
  *resizer_output = &resizer->output[0];

  resizer->input[0].format.encoding = MMAL_ENCODING_RGB24;
  resizer->input[0].format.encoding_variant = 0;
  resizer->input[0].format.bitrate = 0;
  resizer->input[0].format.es->video.frame_rate.num = 0;
  resizer->input[0].format.es->video.frame_rate.den = 0;
  resizer->input[0].format.es->video.width = width;
  resizer->input[0].format.es->video.height = height;
  resizer->input[0].format.es->video.crop.x = 0;
  resizer->input[0].format.es->video.crop.y = 0;
  resizer->input[0].format.es->video.crop.width = width;
  resizer->input[0].format.es->video.crop.height = height;

  err = mmal_port_set_format(&resizer->input[0]);
  CHECK_ERR("Failed to set format for resizer input port");

  resizer->output[0].format.encoding = MMAL_ENCODING_RGB24;
  resizer->output[0].format.encoding_variant = 0;
  resizer->output[0].format.bitrate = 0;
  resizer->output[0].format.es->video.frame_rate.num = 0;
  resizer->output[0].format.es->video.frame_rate.den = 0;
  resizer->output[0].format.es->video.width = width;
  resizer->output[0].format.es->video.height = height;
  resizer->output[0].format.es->video.crop.x = 0;
  resizer->output[0].format.es->video.crop.y = 0;
  resizer->output[0].format.es->video.crop.width = width;
  resizer->output[0].format.es->video.crop.height = height;

  err = mmal_port_set_format(&resizer->output[0]);
  CHECK_ERR("Failed to set format for resizer output port");

out_err:
  return err;
}

struct encoder_h264_params {
  uint32_t quantization;
};

static int create_encoder_component(struct vchiq_service_common *mmal_service,
  struct vchiq_service_common *mems_service,
  struct vchiq_mmal_port **encoder_input,
  struct vchiq_mmal_port **encoder_output, int width, int height)
{
  bool bool_arg;
  uint32_t uint32_arg;
  int err = SUCCESS;
  struct vchiq_mmal_component *encoder;
  struct mmal_parameter_video_profile video_profile;
  struct encoder_h264_params p = {
    .quantization = 0
  };
 
  encoder = component_create(mmal_service, "ril.video_encode");
  CHECK_ERR_PTR(encoder, "Failed to create component 'ril.video_encode'");
  if (!encoder->inputs || !encoder->outputs) {
    MMAL_ERR("err: encoder has 0 outputs or inputs");
    return ERR_GENERIC;
  }

  err = mmal_component_enable(encoder);
  CHECK_ERR("Failed to enable encoder");

  *encoder_input = &encoder->input[0];
  *encoder_output = &encoder->output[0];

  err = vchiq_mmal_port_enable(&encoder->control);
  CHECK_ERR("failed to enable control port");

  // mmal_format_copy(&encoder->output[0].format, &encoder->input[0].format);

  mmal_format_set(&encoder->output[0].format, MMAL_ENCODING_H264, 0, width,
    height, 0, 25000000);
  err = mmal_port_set_format(&encoder->output[0]);
  CHECK_ERR("Failed to set format for preview capture port");

  mmal_format_set(&encoder->input[0].format, MMAL_ENCODING_OPAQUE,
    MMAL_ENCODING_I420, width, height, 0, 0);
  err = mmal_port_set_format(&encoder->input[0]);
  CHECK_ERR("Failed to set format for preview capture port");

  video_profile.profile = MMAL_VIDEO_PROFILE_H264_HIGH;
  video_profile.level = MMAL_VIDEO_LEVEL_H264_4;

  err = vchiq_mmal_port_parameter_set(&encoder->output[0],
    MMAL_PARAMETER_PROFILE, &video_profile, sizeof(video_profile));
  CHECK_ERR("Failed to set h264 MMAL_PARAMETER_PROFILE param");

  if (p.quantization) {
    uint32_arg = p.quantization;
    err = vchiq_mmal_port_parameter_set(&encoder->output[0],
      MMAL_PARAMETER_VIDEO_ENCODE_INITIAL_QUANT, &uint32_arg,
      sizeof(uint32_arg));
    CHECK_ERR("Failed to set MMAL_PARAMETER_VIDEO_ENCODE_INITIAL_QUANT param");

    err = vchiq_mmal_port_parameter_set(&encoder->output[0],
      MMAL_PARAMETER_VIDEO_ENCODE_MIN_QUANT, &uint32_arg, sizeof(uint32_arg));
    CHECK_ERR("Failed to set MMAL_PARAMETER_VIDEO_ENCODE_INITIAL_QUANT param");

    err = vchiq_mmal_port_parameter_set(&encoder->output[0],
      MMAL_PARAMETER_VIDEO_ENCODE_MAX_QUANT, &bool_arg, sizeof(bool_arg));
    CHECK_ERR("Failed to set MMAL_PARAMETER_VIDEO_IMMUTABLE_INPUT param");
  }

#if 0
  bool_arg = false;
  err = vchiq_mmal_port_parameter_set(&encoder->output[0],
    MMAL_PARAMETER_VIDEO_ENCODE_INLINE_HEADER, &bool_arg, sizeof(bool_arg));
  CHECK_ERR("Failed to set h264 encoder parameter");

  err = vchiq_mmal_port_parameter_set(&encoder->input[0],
    MMAL_PARAMETER_VIDEO_ENCODE_SPS_TIMING, &bool_arg, sizeof(bool_arg));
  CHECK_ERR("Failed to set h264 encoder param 'sps timing'");

  err = vchiq_mmal_port_parameter_set(&encoder->input[0],
    MMAL_PARAMETER_VIDEO_ENCODE_INLINE_VECTORS, &bool_arg, sizeof(bool_arg));
  CHECK_ERR("Failed to set h264 encoder param 'inline vectors'");

  err = vchiq_mmal_port_parameter_set(&encoder->input[0],
    MMAL_PARAMETER_VIDEO_ENCODE_SPS_TIMING, &bool_arg, sizeof(bool_arg));
  CHECK_ERR("Failed to set h264 encoder param 'inline vectors'");
#endif

out_err:
  return err;
}

static int create_splitter_component(struct vchiq_service_common *mmal_service,
  const struct mmal_es_format_local *input_format,
  struct vchiq_mmal_port **splitter_in,
  struct vchiq_mmal_port **splitter_out_1,
  struct vchiq_mmal_port **splitter_out_2)
{
  int err = SUCCESS;
  struct vchiq_mmal_component *splitter;
 
  splitter = component_create(mmal_service, "ril.video_splitter");
  CHECK_ERR_PTR(splitter, "Failed to create component 'ril.video_encode'");
  if (!splitter->inputs) {
    MMAL_ERR("err: splitter has no input");
    return ERR_GENERIC;
  }

  if (splitter->outputs < 2) {
    MMAL_ERR("err: splitter has wrong number of outputs %d",
      splitter->outputs);
    return ERR_GENERIC;
  }

  *splitter_in    = &splitter->input[0];
  *splitter_out_1 = &splitter->output[0];
  *splitter_out_2 = &splitter->output[1];

  mmal_format_copy(&splitter->input[0].format, input_format);

  err = mmal_port_set_format(&splitter->input[0]);
  CHECK_ERR("Failed to set splitter input format");

  for (int i = 0; i < 2; ++i) {
    mmal_format_copy(&splitter->output[i].format, &splitter->input[0].format);
    err = mmal_port_set_format(&splitter->output[i]);
    CHECK_ERR("Failed to set format for preview capture port");
  }

  err = mmal_component_enable(splitter);
  CHECK_ERR("Failed to enable splitter");

out_err:
  return err;
}

static int create_camera_component(struct vchiq_service_common *mmal_service,
  int frame_width ,int frame_height,
  struct mmal_parameter_camera_info_camera_t *cam_info,
  struct vchiq_mmal_port **out_preview, struct vchiq_mmal_port **out_video,
  struct vchiq_mmal_port **out_still)
{
  uint32_t param;
  uint32_t supported_encodings[MAX_SUPPORTED_ENCODINGS];
  int num_encodings;
  struct vchiq_mmal_port *still, *video, *preview;
  int err;
  /* mmal_init start */

  struct vchiq_mmal_component *cam = component_create(mmal_service,
    "ril.camera");

  CHECK_ERR_PTR(cam, "Failed to create component 'ril.camera'");

  param = 0;
  err = vchiq_mmal_port_parameter_set(&cam->control, MMAL_PARAMETER_CAMERA_NUM,
    &param, sizeof(param));

  err = vchiq_mmal_port_parameter_set(&cam->control,
    MMAL_PARAMETER_CAMERA_CUSTOM_SENSOR_CONFIG, &param, sizeof(param));

  err = vchiq_mmal_port_enable(&cam->control);

  err = mmal_set_camera_parameters(cam, cam_info, frame_width,
    frame_height);

  CHECK_ERR("Failed to set parameters to component 'ril.camera'");

  preview = &cam->output[CAM_PORT_PREVIEW];
  video = &cam->output[CAM_PORT_VIDEO];
  still = &cam->output[CAM_PORT_CAPTURE];

  err = mmal_port_get_supp_encodings(still, supported_encodings,
    sizeof(supported_encodings), &num_encodings);

  CHECK_ERR("Failed to retrieve supported encodings from port");

  mmal_format_set(&preview->format, MMAL_ENCODING_OPAQUE,
    MMAL_ENCODING_I420, frame_width, frame_height, 3, 0);

  err = mmal_port_set_format(preview);
  CHECK_ERR("Failed to set format for preview capture port");

#if 0
  err = mmal_port_set_zero_copy(video);
  CHECK_ERR("Failed to set zero copy param for camera port");
#endif

  mmal_format_set(&video->format, MMAL_ENCODING_OPAQUE,
    0, frame_width, frame_height, 3, 0);

  err = mmal_port_set_format(video);
  CHECK_ERR("Failed to set format for video capture port");

  mmal_format_set(&still->format, MMAL_ENCODING_OPAQUE,
    MMAL_ENCODING_I420, frame_width, frame_height, 0, 0);

  err = mmal_port_set_format(still);
  CHECK_ERR("Failed to set format for still capture port");

  err = mmal_component_enable(cam);
  CHECK_ERR("Failed to enable component camera");

  *out_preview = preview;
  *out_video = video;
  *out_still = still;
out_err:
  return err;
}

static int vchiq_startup_camera(struct vchiq_service_common *mmal_service,
  struct vchiq_service_common *mems_service,
  int frame_width, int frame_height)
{
  int err;
  struct mmal_cam_info cam_info = {0};
  struct vchiq_mmal_port *cam_preview, *cam_video, *cam_still;
  struct vchiq_mmal_port *encoder_input, *encoder_output;
  struct vchiq_mmal_port *resizer_input, *resizer_output;
  struct vchiq_mmal_port *splitter_in, *splitter_out_1,
    *splitter_out_2;

  cam.io_buffers[0] = dma_alloc(H264BUF_SIZE);
  cam.io_buffers[1] = dma_alloc(H264BUF_SIZE);

  cam.current_buf = cam.io_buffers[0];
  cam.current_buf_ptr = cam.current_buf;
  os_event_init(&cam.next_buf_avail);
  os_event_notify(&cam.next_buf_avail);

  cam.write_sector_offset = 0;
  err = vchiq_mmal_get_cam_info(mmal_service, &cam_info);
  CHECK_ERR("Failed to get num cameras");

  err = create_camera_component(mmal_service, frame_width, frame_height,
    &cam_info.cameras[0], &cam_preview, &cam_video, &cam_still);
  CHECK_ERR("Failed to create camera component");

  err = create_encoder_component(mmal_service, mems_service, &encoder_input,
    &encoder_output, frame_width, frame_height);
  CHECK_ERR("Failed to create encoder component");

  err = vchiq_mmal_port_connect(cam_video, encoder_input);
  CHECK_ERR("Failed to connect camera to encoder");

  err = mmal_port_set_zero_copy(encoder_output);
  if (err != SUCCESS)
    goto out_err;

  err = mmal_port_set_zero_copy(encoder_input);
  if (err != SUCCESS)
    goto out_err;

  err = mmal_port_set_zero_copy(cam_video);
  if (err != SUCCESS)
    goto out_err;

  err = vchiq_mmal_port_enable(encoder_output);
  if (err != SUCCESS)
    goto out_err;

  err = vchiq_mmal_port_enable(encoder_input);
  if (err != SUCCESS)
    goto out_err;

  encoder_output->minimum_buffer.num = 1;
  encoder_output->minimum_buffer.size = 512 * 1024;

  err = mmal_apply_buffers(mems_service, encoder_output, true, 1);
  CHECK_ERR("Failed to add buffers to encoder output");
  err = mmal_camera_capture_frames(cam_video);
  CHECK_ERR("Failed to start camera capture");

#if 0
  err = create_splitter_component(mmal_service, &cam_video->format,
    &splitter_in, &splitter_out_1, &splitter_out_2);
  CHECK_ERR("Failed to create splitter component");
#endif

  while(1)
    asm volatile ("wfe");

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

  if (!s->is_connected) {
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
  BUG_IF(err != SUCCESS, "failed to start vchiq async primitives");
  os_event_wait(&s->state_waitflag);
  os_event_clear(&s->state_waitflag);
  err = vchiq_handmade_connect(s);

  mmal_service = vchiq_open_mmal_service(s);
  BUG_IF(!mmal_service, "failed to open mmal service");

  smem_service = vchiq_open_smem_service(s);
  BUG_IF(!smem_service, "failed at open smem service");

  err = vchiq_startup_camera(mmal_service, smem_service, 1920, 1088);
  BUG_IF(err != SUCCESS, "failed to run camera");
}

static void mmal_init_state(void)
{
  mmal_state.work_bitmap.data = mmal_state.work_bitmap_data;
  mmal_state.work_bitmap.num_entries = MAX_MMAL_WORKS;
  bitmap_clear_all(&mmal_state.work_bitmap);
}

void vchiq_init(void)
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
  frame_num = 0;

  /* fragment size granulariry is cache line size */
  fragment_size = 2 * 64;
  /* Allocate space for the channels in coherent memory */
  /* TOTAL_SLOTS */
  slot_mem_size = 16 * VCHIQ_SLOT_SIZE;
  frag_mem_size = fragment_size * MAX_FRAGMENTS;

  slot_mem = dma_alloc(slot_mem_size + frag_mem_size);
  slot_phys = (uint32_t)(uint64_t)slot_mem;
  if (!slot_mem) {
    printf("failed to allocate DMA memory");
    return;
  }

  vchiq_slot_zero = vchiq_init_slots(slot_mem, slot_mem_size);
  if (!vchiq_slot_zero)
    return;

  vchiq_init_state(s, vchiq_slot_zero);
  mmal_init_state();

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
    return;
  }

  asm volatile("dsb sy");

  s->is_connected = false;
  vchiq_handmade(s, vchiq_slot_zero);
}
