/*
 * Because major VideoCore services on Raspberry Pi are left undocumented, all
 * of the VCHIQ, MMAL related code is derived from drivers found in linux
 * kernel. Some structures, fields and constants are taken as-is with only
 * minor changes to minimize code base and keep code-style consistent to the
 * rest of the kernel code.
 */
#include <vc/vchiq.h>
#include <string.h>
#include <kmalloc.h>
#include <errcode.h>
#include <os_api.h>
#include <mbox_props.h>
#include <task.h>
#include "vchiq_priv.h"
#include "vchiq_doorbell.h"

#define MODULE_UNIT_TAG "vchiq"
#define MODULE_LOG_LEVEL vchiq_log_level
#include <module_common.h>

static bool vchiq_ready = false;

static struct vchiq_state vchiq_state;

static struct vchiq_service *vchiq_service_map[10];

static inline void vchiq_event_init(struct vchiq_event *ev)
{
  ev->armed = 0;
  ev->event = (uint32_t)(uint64_t)ev;
}

static struct vchiq_slot_zero *vchiq_slots_init(void *mem_base, int mem_size)
{
  int mem_align = (VCHIQ_SLOT_SIZE - (int)(uint64_t)mem_base) & VCHIQ_SLOT_MASK;
  struct vchiq_slot_zero *slot_zero = (void *)((char *)mem_base + mem_align);
  int num_slots = (mem_size - mem_align)/VCHIQ_SLOT_SIZE;
  int first_data_slot = VCHIQ_SLOT_ZERO_SLOTS;

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

static void vchiq_state_init(struct vchiq_slot_zero *slot_zero)
{
  struct vchiq_state * const s = &vchiq_state;
  struct vchiq_shared_state *local;
  struct vchiq_shared_state *remote;
  struct vchiq_header *h;
  int i;

  /* Check the input configuration */
  local = &slot_zero->slave;
  remote = &slot_zero->master;

  memset(s, 0, sizeof(*s));

  /* initialize shared state pointers */
  s->local = local;
  s->remote = remote;
  s->slots = (struct vchiq_slot *)slot_zero;

  os_event_init(&s->ev_trigger);
  os_event_init(&s->ev_recycle);
  os_event_init(&s->ev_sync_trigger);
  os_event_init(&s->ev_sync_release);
  os_event_init(&s->ev_ack);
  INIT_LIST_HEAD(&s->components);
  s->slot_queue_avail = 0;

  for (i = local->slot_first; i <= local->slot_last; i++)
    local->slot_queue[s->slot_queue_avail++] = i;

  vchiq_event_init(&local->trigger);
  local->tx_pos = 0;

  vchiq_event_init(&local->recycle);
  local->slot_queue_recycle = s->slot_queue_avail;

  vchiq_event_init(&local->sync_trigger);
  vchiq_event_init(&local->sync_release);

  /* At start-of-day, the slot is empty and available */
  h = (void *)SLOT_DATA_FROM_INDEX(s, local->slot_sync);
  h->msgid = VCHIQ_MSGID_PADDING;

  local->sync_release.armed = 0;

  local->debug[DEBUG_ENTRIES] = DEBUG_MAX;

  /* Indicate readiness to the other side */
  local->initialised = 1;
}

static inline struct vchiq_fragment *fragment_get_next(
  struct vchiq_fragment *f, size_t fragment_size)
{
  return (void *)((char *)f + fragment_size);
}

static inline void vchiq_init_fragments(void *baseaddr, int num_fragments,
  size_t fragment_size)
{
  int i;
  struct vchiq_fragment *f = baseaddr;

  for (i = 0; i < num_fragments - 1; i++) {
    f->next = fragment_get_next(f, fragment_size);
    f = f->next;
  }
  f->next = NULL;
}

static inline void vchiq_event_check_isr(struct event *ev,
  struct vchiq_event *e)
{
  vchiq_dsb();

  if (e->armed && e->fired)
    os_event_notify_isr(ev);
}

static void vchiq_check_local_events_isr(void)
{
  struct vchiq_shared_state *local;
  local = vchiq_state.local;

  vchiq_event_check_isr(&vchiq_state.ev_trigger, &local->trigger);
  vchiq_event_check_isr(&vchiq_state.ev_recycle, &local->recycle);
  vchiq_event_check_isr(&vchiq_state.ev_sync_trigger, &local->sync_trigger);
  vchiq_event_check_isr(&vchiq_state.ev_sync_release, &local->sync_release);
}

static void vchiq_irq_handler(void)
{
  if (vchiq_doorbell_is_triggered())
    vchiq_check_local_events_isr();
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

static struct vchiq_service *vchiq_service_alloc(void)
{
  struct vchiq_service *service;

  int i;

  for (i = 0; i < ARRAY_SIZE(vchiq_service_map); ++i) {
    if (!vchiq_service_map[i])
      break;
  }

  if (i == ARRAY_SIZE(vchiq_service_map)) {
    MODULE_ERR("No free service port");
    return NULL;
  }

  service = kzalloc(sizeof(*service));
  if (!service) {
    MODULE_ERR("Failed to alloce memory for service");
    return NULL;
  }

  /* ser localport to non zero value or vchiq will assign something */
  service->localport = i + 1;
  vchiq_wmb();
  vchiq_service_map[i] = service;
  return service;
}

static void vchiq_service_free(struct vchiq_service *service)
{
  int i;

  for (i = 0; i < ARRAY_SIZE(vchiq_service_map); ++i) {
    if (vchiq_service_map[i] == service) {
      vchiq_service_map[i] = NULL;
      vchiq_wmb();
      kfree(service);
      break;
    }
  }
}

static struct vchiq_service *vchiq_service_find_by_localport(int localport)
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
  struct vchiq_service *service = NULL;

  service = vchiq_service_find_by_localport(localport);
  BUG_IF(!service, "OPENACK with no service");
  service->opened = true;
  service->remoteport = remoteport;
  vchiq_wmb();
  os_event_notify(&s->ev_ack);
  return SUCCESS;
}

static OPTIMIZED int vchiq_parse_msg_data(struct vchiq_state *s, int localport,
  int remoteport, struct vchiq_header *h)
{
  struct vchiq_service *service = NULL;

  service = vchiq_service_find_by_localport(localport);
  BUG_IF(!service, "MSG_DATA with no service");
  service->cb(service, h);
  return SUCCESS;
}

static inline int OPTIMIZED vchiq_parse_rx_dispatch(struct vchiq_state *s,
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
      os_event_notify(&s->ev_ack);
      err = SUCCESS;
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

static void vchiq_event_signal(struct vchiq_event *event)
{
  vchiq_wmb();
  event->fired = 1;
  vchiq_dsb();
  if (event->armed)
    vchiq_doorbell_trigger();
}

static void vchiq_release_slot(struct vchiq_state *s, int slot_index)
{
  int slot_queue_recycle;

  slot_queue_recycle = s->remote->slot_queue_recycle;
  vchiq_rmb();
  s->remote->slot_queue[slot_queue_recycle & VCHIQ_SLOT_QUEUE_MASK] = slot_index;
  s->remote->slot_queue_recycle = slot_queue_recycle + 1;
  vchiq_event_signal(&s->remote->recycle);
}

static int OPTIMIZED vchiq_parse_rx(struct vchiq_state *s)
{
  int err = SUCCESS;
  int rx_slot;
  struct vchiq_header *h;
  int prev_rx_slot = vchiq_tx_header_to_slot_idx(s);

  while(s->rx_pos != s->remote->tx_pos) {
    int old_rx_pos = s->rx_pos;
    h = vchiq_get_next_header_rx(s);
    MODULE_DEBUG2("msg received: %p, rx_pos: %d, size: %d", h, old_rx_pos,
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

static void vchiq_event_wait(struct event *ev, struct vchiq_event *event)
{
  if (!event->fired) {
    event->armed = 1;
    vchiq_dsb();
    os_event_wait(ev);
    os_event_clear(ev);
    event->armed = 0;
    vchiq_wmb();
  }
  event->fired = 0;
  vchiq_rmb();
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
  int slot_queue_avail, pos, slot_index;
  char *slots;
  struct vchiq_header *h;

  slot_queue_avail = s->slot_queue_avail;

  vchiq_mb();

  while (slot_queue_avail != s->local->slot_queue_recycle) {
    slot_index = s->local->slot_queue[slot_queue_avail & VCHIQ_SLOT_QUEUE_MASK];
    slot_queue_avail++;
    slots = (char *)&s->slots[slot_index];

    vchiq_rmb();

    pos = 0;
    while (pos < VCHIQ_SLOT_SIZE) {
      h = (struct vchiq_header *)(slots + pos);

      pos += VCHIQ_MSG_TOTAL_SIZE(h->size);
      BUG_IF(pos > VCHIQ_SLOT_SIZE, "wrong vchiq header");
    }

    vchiq_mb();

    s->slot_queue_avail = slot_queue_avail;
  }
}

static void vchiq_recycle_thread(void)
{
  struct vchiq_state *s;

  s = &vchiq_state;
  while (1) {
    vchiq_event_wait(&s->ev_recycle, &s->local->recycle);
    vchiq_process_free_queue(s);
  }
}

static void vchiq_sync_release_thread(void)
{
  struct vchiq_state *s;

  s = &vchiq_state;
  while (1) {
    vchiq_event_wait(&s->ev_sync_release, &s->local->sync_release);
    // vchiq_process_free_queue(s);
  }
}

static void OPTIMIZED vchiq_loop_thread(void)
{
  struct vchiq_state *s;

  s = &vchiq_state;
  while(1) {
    vchiq_event_wait(&s->ev_trigger, &s->local->trigger);
    BUG_IF(vchiq_parse_rx(s) != SUCCESS,
      "failed to parse incoming vchiq messages");
  }
}

static int vchiq_start_runtime(void)
{
  int err;
  struct task *t;

  vchiq_doorbell_0_irq_enable(vchiq_irq_handler);

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

static struct vchiq_header *vchiq_msg_prep_next_header_tx(size_t msg_size)
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
  tx_pos = vchiq_state.local->tx_pos;
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

    slot_index = vchiq_state.local->slot_queue[slot_queue_index & VCHIQ_SLOT_QUEUE_MASK];
    slot = (char *)&vchiq_state.slots[slot_index];
    h = (struct vchiq_header *)(slot + (tx_pos & VCHIQ_SLOT_MASK));
    h->msgid = VCHIQ_MSGID_PADDING;
    h->size = slot_space - sizeof(*h);
    tx_pos += slot_space;
    slot_queue_index++;
  }

  slot_index = vchiq_state.local->slot_queue[slot_queue_index & VCHIQ_SLOT_QUEUE_MASK];
  slot = (char *)&vchiq_state.slots[slot_index];

  /* just prepare the header, all the filling is done outside */
  h = (struct vchiq_header *)(slot + (tx_pos & VCHIQ_SLOT_MASK));

  tx_pos += msg_full_size;

  MODULE_DEBUG("prep_next_msg:%d(%d:%d)->%d(%d:%d)",
    vchiq_state.local->tx_pos,
    vchiq_state.local->slot_queue[(vchiq_state.local->tx_pos / VCHIQ_SLOT_SIZE) & VCHIQ_SLOT_QUEUE_MASK],
    vchiq_state.local_tx_pos & VCHIQ_SLOT_MASK,
    tx_pos, slot_index, tx_pos & VCHIQ_SLOT_MASK);

  vchiq_state.local_tx_pos = tx_pos;
  return h;
}

static bool vchiq_send_msg_open_service(struct vchiq_service *service,
  uint32_t service_id, int version_min, int version_max)
{
  struct vchiq_msg_open_service msg = {
    .service_id = service_id,
    .client_id = 0,
    .version = version_max,
    .version_min = version_min,
  };

  service->opened = false;
  service->service_id = service_id;

  vchiq_msg_prep(VCHIQ_MSG_OPEN, service->localport, 0, &msg, sizeof(msg));

  vchiq_event_signal(&vchiq_state.remote->trigger);
  os_event_wait(&vchiq_state.ev_ack);
  os_event_clear(&vchiq_state.ev_ack);

  if (service->opened) {
    MODULE_INFO("opened service %08x, localport: %d, remoteport: %d",
      service->service_id,
      service->localport,
      service->remoteport);
  }
  return service->opened;
}

struct list_head *vchiq_get_components_list(void)
{
  return &vchiq_state.components;
}

static struct vchiq_service *vchiq_open_service(uint32_t service_id,
  int version_min, int version_max, vchiq_service_cb_t service_cb)
{
  struct vchiq_service *service = vchiq_service_alloc();
  if (!service)
    return NULL;

  if (!vchiq_send_msg_open_service(service, service_id, version_min,
    version_max)) {
    vchiq_service_free(service);
    os_log("Failed to open service %08x\n", service_id);
    return NULL;
  }

  service->s = &vchiq_state;
  service->cb = service_cb;
  return service;
}

static int vchiq_connect(void)
{
  struct vchiq_header *h;
  int msg_size;
  int err;
  struct vchiq_state *s = &vchiq_state;

  os_event_wait(&s->ev_ack);
  os_event_clear(&s->ev_ack);

  /* Send CONNECT MESSAGE */
  msg_size = 0;
  h = vchiq_msg_prep_next_header_tx(msg_size);

  h->msgid = VCHIQ_MAKE_MSG(VCHIQ_MSG_CONNECT, 0, 0);
  h->size = msg_size;

  vchiq_wmb();
  /* Make the new tx_pos visible to the peer. */
  s->local->tx_pos = s->local_tx_pos;
  vchiq_wmb();

  vchiq_event_signal(&s->remote->trigger);

  if (!s->is_connected) {
    err = ERR_GENERIC;
    goto out_err;
  }

  return SUCCESS;

out_err:
  return err;
}

struct vchiq_service *vchiq_service_open(uint32_t service_id, int version_min,
  int version_max, vchiq_service_cb_t service_cb)
{
  if (!vchiq_ready) {
    os_log("vchiq not ready");
    return NULL;
  }

  return vchiq_open_service(service_id, version_max, version_min, service_cb);
}

void vchiq_msg_prep(int msgid, int srcport, int dstport, void *payload,
  int payload_sz)
{
  struct vchiq_header *h;
  int old_tx_pos = vchiq_state.local->tx_pos;

  h = vchiq_msg_prep_next_header_tx(payload_sz);
  h->msgid = VCHIQ_MAKE_MSG(msgid, srcport, dstport);
  h->size = payload_sz;
  memcpy(h->data, payload, payload_sz);
  vchiq_wmb();

  /* Make the new tx_pos visible to the peer. */
  vchiq_state.local->tx_pos = vchiq_state.local_tx_pos;
  vchiq_wmb();
  MODULE_DEBUG("msg sent: %p, tx_pos: %d, size: %d", h, old_tx_pos,
    VCHIQ_MSG_TOTAL_SIZE(h->size));
}

void vchiq_event_signal_trigger(void)
{
  vchiq_event_signal(&vchiq_state.remote->trigger);
}

int vchiq_init(void)
{
  int err;
  struct vchiq_slot_zero *vchiq_slot_zero;
  void *slot_mem;
  uint32_t slot_phys;
  uint32_t channelbase;
  int slot_mem_size, frag_mem_size;
  uint32_t fragment_size;
  char *fragments_baseaddr;

  if (vchiq_ready) {
    os_log("Attempt to double init vchiq");
    return ERR_INVAL;
  }

  /* fragment size granulariry is cache line size */
  fragment_size = 2 * 64;
  /* Allocate space for the channels in coherent memory */
  /* TOTAL_SLOTS */
  slot_mem_size = 16 * VCHIQ_SLOT_SIZE;
  frag_mem_size = fragment_size * VCHIQ_MAX_FRAGMENTS;

  slot_mem = dma_alloc(slot_mem_size + frag_mem_size, 1);
  slot_phys = NARROW_PTR(slot_mem);
  if (!slot_mem) {
    MODULE_ERR("failed to allocate DMA memory");
    return ERR_MEMALLOC;
  }

  vchiq_slot_zero = vchiq_slots_init(slot_mem, slot_mem_size);
  if (!vchiq_slot_zero)
    return ERR_MEMALLOC;

  vchiq_state_init(vchiq_slot_zero);

  vchiq_slot_zero->platform_data[VCHIQ_PLATFORM_FRAGMENTS_OFFSET_IDX]
    = slot_phys + slot_mem_size;

  vchiq_slot_zero->platform_data[VCHIQ_PLATFORM_FRAGMENTS_COUNT_IDX]
    = VCHIQ_MAX_FRAGMENTS;

  fragments_baseaddr = (char *)slot_mem + slot_mem_size;
  vchiq_init_fragments(fragments_baseaddr, VCHIQ_MAX_FRAGMENTS, fragment_size);

  vchiq_mb();
  /* Send the base address of the slots to VideoCore */
  channelbase = RAM_PHY_TO_BUS_UNCACHED(slot_mem);

  if (!mbox_init_vchiq(&channelbase)) {
    LOG(0, ERR, "vchiq", "failed to set channelbase");
    return ERR_IO;
  }

  vchiq_mb();

  vchiq_state.is_connected = false;
  err = vchiq_start_runtime();
  BUG_IF(err != SUCCESS, "failed to start vchiq async primitives");
  err = vchiq_connect();
  BUG_IF(err != SUCCESS, "failed to connect to VideoCore VCHIQ");
  vchiq_ready = true;
  return SUCCESS;
}
