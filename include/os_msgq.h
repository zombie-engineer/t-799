#pragma once
#include <list.h>
#include <cpu.h>
#include <os_api.h>
#include <errcode.h>
#include <stringlib.h>

/*
 * Generic message queue backed by a circular buffer
 * - uses all memory available for queue
 *   |0:RW|1:  |2:  |3:  | nmsg = 0
 *   |0:R |1: W|2:  |3:  | nmsg = 1
 *   |0:R |1:  |2: W|3:  | nmsg = 2
 *   |0:R |1:  |2:  |3: W| nmsg = 3
 *   |0:  |1:R |2:  |3: W| nmsg = 2
 *   |0:  |1:  |2:R |3: W| nmsg = 1
 *   |0:  |1:  |2:  |3:RW| nmsg = 0
 *   |0: W|1:  |2:  |3:R | nmsg = 1
 *   |0:  |1: W|2:  |3:R | nmsg = 2
 *   |0:  |1:  |2: W|3:R | nmsg = 3
 *   |0:R |1:  |2: W|3:  | nmsg = 2
 *   |0:  |1:R |2: W|3:  | nmsg = 1
 *   |0:  |1:  |2:RW|3:  | nmsg = 0
 */

struct os_msgq {
  /* address of memory area allocated for this queue */
  uint8_t *queue;

  /* size of element of the queue */
  size_t msg_size;

  /* size of memory area, assigned to queue in number of messages */
  unsigned int queue_size;

  /* index of a queue slot where next message could be put /written to */
  unsigned int wr;

  /* index of a queue slot from where next message could be read */
  unsigned int rd;

  /* list of tasks that are sleeping, waiting available message */
  struct list_head wait_list;
};

static inline void os_msgq_init(struct os_msgq *q, void *queue_addr,
  size_t msg_size, size_t num_msg)
{
  q->queue = queue_addr;
  q->queue_size = num_msg;
  q->msg_size = msg_size;
  q->wr = 0;
  q->rd = 0;
  INIT_LIST_HEAD(&q->wait_list);
}

static inline int os_msgq_put_isr(struct os_msgq *q, const void *m)
{
  uint8_t *slot;

  if (q->wr < q->rd && q->rd - q->wr == 1)
    return ERR_BUSY;

  if (!q->rd && q->wr == q->queue_size - 1)
    return ERR_BUSY;

  slot = q->queue + q->wr * q->msg_size;

  q->wr++;
  if (q->wr == q->queue_size)
    q->wr = 0;

  memcpy(slot, m, q->msg_size);

  if (!list_empty(&q->wait_list))
    sched_wait_list_wake_one_isr(&q->wait_list);

  return SUCCESS;
}

static inline void os_msgq_put(struct os_msgq *q, const void *m)
{
  os_api_syscall2(SYSCALL_MSGQ_PUT, q, m);
}

static inline void os_msgq_get(struct os_msgq *q, void *m)
{
  int irq;
  const uint8_t *slot;

  if (q->wr == q->rd)
    os_api_put_to_wait_list(&q->wait_list);

  slot = q->queue + q->rd * q->msg_size;
  q->rd++;
  if (q->rd == q->msg_size)
    q->rd = 0;

  disable_irq_save_flags(irq);
  memcpy(m, slot, q->msg_size);
  restore_irq_flags(irq);
}
