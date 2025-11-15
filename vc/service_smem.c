#include <vc/service_smem.h>
#include <vc/service_smem_helpers.h>
#include <vc/vchiq.h>
#include <errcode.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <event.h>
#include <common.h>
#include <os_api.h>

#define VC_SM_VER  1
#define VC_SM_MIN_VER 0

#define SMEM_CTX_POOL_SIZE 4

#define VC_SM_RESOURCE_NAME_DEFAULT "sm-host-resource"

#define VC_SM_RESOURCE_NAME 32

/* Type of memory to be allocated */
enum vc_sm_alloc_type_t {
  VC_SM_ALLOC_CACHED,
  VC_SM_ALLOC_NON_CACHED,
};

enum vc_sm_msg_type {
  /* Message types supported for HOST->VC direction */

  /* Allocate shared memory block */
  VC_SM_MSG_TYPE_ALLOC,
  /* Lock allocated shared memory block */
  VC_SM_MSG_TYPE_LOCK,
  /* Unlock allocated shared memory block */
  VC_SM_MSG_TYPE_UNLOCK,
  /* Unlock allocated shared memory block, do not answer command */
  VC_SM_MSG_TYPE_UNLOCK_NOANS,
  /* Free shared memory block */
  VC_SM_MSG_TYPE_FREE,
  /* Resize a shared memory block */
  VC_SM_MSG_TYPE_RESIZE,
  /* Walk the allocated shared memory block(s) */
  VC_SM_MSG_TYPE_WALK_ALLOC,

  /* A previously applied action will need to be reverted */
  VC_SM_MSG_TYPE_ACTION_CLEAN,

  /*
   * Import a physical address and wrap into a MEM_HANDLE_T.
   * Release with VC_SM_MSG_TYPE_FREE.
   */
  VC_SM_MSG_TYPE_IMPORT,
  /*
   *Tells VC the protocol version supported by this client.
   * 2 supports the async/cmd messages from the VPU for final release
   * of memory, and for VC allocations.
   */
  VC_SM_MSG_TYPE_CLIENT_VERSION,
  /* Response to VC request for memory */
  VC_SM_MSG_TYPE_VC_MEM_REQUEST_REPLY,

  /*
   * Asynchronous/cmd messages supported for VC->HOST direction.
   * Signalled by setting the top bit in vc_sm_result_t trans_id.
   */

  /*
   * VC has finished with an imported memory allocation.
   * Release any Linux reference counts on the underlying block.
   */
  VC_SM_MSG_TYPE_RELEASED,
  /* VC request for memory */
  VC_SM_MSG_TYPE_VC_MEM_REQUEST,

  VC_SM_MSG_TYPE_MAX
};

/* Message header for all messages in HOST->VC direction */
/* Request to free a previously allocated memory (HOST->VC) */
struct vc_sm_msg_hdr_t {
  uint32_t type;
  uint32_t trans_id;
  uint8_t body[0];
};

/* Generic result for a request (VC->HOST) */
struct vc_sm_result_t {
  /* Transaction identifier */
  uint32_t trans_id;
  int32_t success;
};

/* Request to import memory (HOST->VC) */
struct vc_sm_import {
  /* type of memory to allocate */
  enum vc_sm_alloc_type_t type;
  /* pointer to the VC (ie physical) address of the allocated memory */
  uint32_t addr;
  /* size of buffer */
  uint32_t size;
  /* opaque handle returned in RELEASED messages */
  uint32_t kernel_id;
  /* Allocator identifier */
  uint32_t allocator;
  /* resource name (for easier tracking on vc side) */
  char     name[VC_SM_RESOURCE_NAME];
};

struct smem_msg_ctx {
  bool active;
  uint32_t trans_id;
  struct event ev_done;
  void *data;
  int data_size;
};

/* Result of a requested memory import (VC->HOST) */
struct vc_sm_import_result {
  /* Transaction identifier */
  uint32_t trans_id;
  /* Resource handle */
  uint32_t res_handle;
};

static struct vchiq_service *service_smem = NULL;

static int vc_trans_id = 0;

static uint32_t mems_transaction_counter = 0;

static struct smem_msg_ctx smem_msg_ctx_pool[SMEM_CTX_POOL_SIZE] = { 0 };

static struct smem_msg_ctx *smem_msg_ctx_alloc(void)
{
  int i;
  struct smem_msg_ctx *ctx;

  for (i = 0; i < ARRAY_SIZE(smem_msg_ctx_pool); ++i) {
    ctx = &smem_msg_ctx_pool[i];
    if (!ctx->active) {
      ctx->active = true;
      ctx->data = NULL;
      ctx->trans_id = mems_transaction_counter++;
      os_event_init(&ctx->ev_done);
      return ctx;
    }
  }
  return NULL;
}

static void smem_msg_ctx_free(struct smem_msg_ctx *ctx)
{
  const struct smem_msg_ctx *start = &smem_msg_ctx_pool[0];
  const struct smem_msg_ctx *end = &smem_msg_ctx_pool[SMEM_CTX_POOL_SIZE];
  if (ctx < start || ctx >= end)
    panic_with_log("mems context free error");

  ctx->active = 0;
}

static int vc_sm_cma_vchi_send_msg(enum vc_sm_msg_type msg_id, const void *msg,
  uint32_t msg_size, void *result, uint32_t result_size,
  uint32_t *cur_trans_id)
{
  int err;
  char buf[256];
  struct vc_sm_msg_hdr_t *hdr = (void *)buf;
  struct smem_msg_ctx *ctx;

  if (!service_smem)
    return ERR_INVAL;

  ctx = smem_msg_ctx_alloc();
  if (!ctx) {
    err = ERR_GENERIC;
    SMEM_ERR("Failed to allocate mems message context");
    return err;
  }

  hdr->type = msg_id;
  hdr->trans_id = vc_trans_id++;

  if (msg_size)
    memcpy(hdr->body, msg, msg_size);

  vchiq_msg_prep(
    VCHIQ_MSG_DATA,
    service_smem->localport,
    service_smem->remoteport,
    hdr,
    msg_size + sizeof(*hdr));

  vchiq_event_signal_trigger();
  os_event_wait(&ctx->ev_done);
  os_event_clear(&ctx->ev_done);

  memcpy(result, ctx->data, ctx->data_size);
  smem_msg_ctx_free(ctx);
  return SUCCESS;
}

static struct smem_msg_ctx *smem_msg_ctx_from_trans_id(uint32_t trans_id)
{
  int i;
  struct smem_msg_ctx *ctx;

  for (i = 0; i < ARRAY_SIZE(smem_msg_ctx_pool); ++i) {
    ctx = &smem_msg_ctx_pool[i];
    if (ctx->trans_id == trans_id) {
      if (!ctx->active) {
        SMEM_ERR("mems data callback for inactive message");
        return NULL;
      }
      return ctx;
    }
  }
  SMEM_ERR("mems data callback for non-existent context");
  return NULL;
}

static int smem_service_cb(struct vchiq_service *s, struct vchiq_header *h)
{
  struct vc_sm_result_t *r;
  struct smem_msg_ctx *ctx;

  BUG_IF(h->size < 4, "mems message less than 4");
  r = (struct vc_sm_result_t *)&h->data[0];
  ctx = smem_msg_ctx_from_trans_id(r->trans_id);
  BUG_IF(!ctx, "mem transaction ctx problem");
  ctx->data = h->data;
  ctx->data_size = h->size;
  os_event_notify(&ctx->ev_done);
  return SUCCESS;
}

int smem_import_dmabuf(void *addr, uint32_t size, uint32_t *vcsm_handle)
{
  int err;
  struct vc_sm_import msg;
  struct vc_sm_import_result result;
  uint32_t cur_trans_id = 0;

  if (!service_smem)
    return ERR_INVAL;

  msg.type = VC_SM_ALLOC_NON_CACHED;
  msg.allocator = 0x66aa;
  msg.addr = (uint32_t)(uint64_t)addr | 0xc0000000;
  msg.size = size;
  msg.kernel_id = 0x6677;

  memcpy(msg.name, VC_SM_RESOURCE_NAME_DEFAULT,
    sizeof(VC_SM_RESOURCE_NAME_DEFAULT));

  err = vc_sm_cma_vchi_send_msg(VC_SM_MSG_TYPE_IMPORT,
    &msg, sizeof(msg),
    &result, sizeof(result),
    &cur_trans_id);
  SMEM_CHECK_ERR("Failed to import buffer to vc");

  SMEM_DEBUG("imported_dmabuf: addr:%08x, size: %d, trans_id: %08x,"
    " res.trans_id: %08x, res.handle: %08x",
    msg.addr, msg.size, cur_trans_id, result.trans_id,
    result.res_handle);

  *vcsm_handle = result.res_handle;

  return SUCCESS;

out_err:
  return err;
}

int smem_init(void)
{
  service_smem = vchiq_service_open(VCHIQ_SERVICE_NAME_TO_ID("SMEM"),
    VC_SM_MIN_VER, VC_SM_VER, smem_service_cb);
  return service_smem ? SUCCESS : ERR_GENERIC;
}
