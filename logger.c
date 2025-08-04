#include <logger.h>
#include <task.h>
#include <errcode.h>
#include <printf.h>
#include <sched.h>
#include <os_api.h>
#include <compiler.h>
#include <bitmap.h>
#include <mutex.h>
#include <common.h>

#define LOG_ENTRIES_COUNT 256

/*
 * Logger data structure explained:
 * - there is a contignous array of entries, that form a circular buffer
 * - each entry has a fix-sized buffer, that can be filled with log message
 *   with sprintf
 * - entry is in one of 3 states: FREE,BUSY,READY
 * - because sprintf can be time consuming, in addition to standard 'push' and
 *   'pop' circular buffer has one extra operation 'take'
 * - a thread wishing to log a message, performs 'take' operation to occupy
 *   next free entry in a circular buffer. If there is are no free entries in
 *   a circular, the oldest entry is dropped and given to the caller.
 * - The owning thread now has to sprintf a text buffer in the entry and upon
 *   completion the entry as 'ready' and notify the logging thread.
 * - If multiple threads 'take' several entries in a row, it may take different
 *   amount of time to sprintf each buffer and they might perform 'mark_ready'
 *   in different order than they did the 'take' operation. In that case there
 *   will be a situation when logger thread will be notified that new entry
 *   is ready, but will not be able to progress until the earliest entry
 *   becomes ready. Examples when logger thread wants to write from position 0:

 *   | 0:ready | 1:ready | 2:ready |   <- logger thread will write entries
 *   0,1,2 as soon as it wakes up
 *
 *   | 0:busy | 1:ready | 2:busy | 3:ready | <- logger thread will
 *   will wake up, see that entry 0 is not ready and will sleep until 0
 *   becomes ready
 * - If all entries are busy, logger thread will block or skip logging 
 *   completely on 'take' operation. Number of skipped and dropped entries is
 *   tracked and output to log
 *
 * Suppose example entries array size is 3 items, initially tail and head
 * point to the start of array, number of items, we also have to
 * ready for writeout is 0
 * 
 * entries array: | 0 | 1 | 2 | nr_ready = 0, start_from=0
 *   ^tail
 *   ^head
 * 
 */

#if 0
#define LOGGER_PRINTF(__fmt, ...) \
  printf(__fmt, ##__VA_ARGS__)

#define LOGGER_PUTS(__msg) \
  puts(__msg)
#else
#define LOGGER_PRINTF(__fmt, ...) ;

#define LOGGER_PUTS(__msg) ;
#endif

struct logger {
  struct event entry_is_ready_event;

  struct mutex lock;

  struct logger_entry *tail;
  struct logger_entry *head;
  struct logger_entry *array;
  struct logger_entry *array_end;
  size_t nr_ready;
  size_t nr_busy;
  size_t nr_entries;
  size_t nr_dropped;
  size_t nr_dropped_takes;
  size_t offset;
  bool thread_active;
};

static struct logger logger = { 0 };

static struct logger_entry *logger_take(void)
{
  struct logger_entry *e;
  int irqflag;

  disable_irq_save_flags(irqflag);
  LOGGER_PUTS("[logger_take]\r\n");

  /*
   * Case when all entries are busy - taken by other threads, almost
   * impossible case
   */
  if (logger.nr_busy == logger.nr_entries) {
    logger.nr_dropped++;
    LOGGER_PUTS("[logger_take end rejected]\r\n");
    restore_irq_flags(irqflag);
    return NULL;
  }

  /*
   * Case when there are no free entries, but not all busy, so READY can be
   * dropped if it is the next to be taken, otherwise it is BUSY, then we can
   * only wait or skip the current message
   */
  if (logger.nr_busy + logger.nr_ready == logger.nr_entries) {
    if (logger.tail->state == LOGGER_ENT_STATE_BUSY) {
      logger.nr_dropped_takes++;
      restore_irq_flags(irqflag);
      LOGGER_PUTS("[logger_take end tail busy]\r\n");
      return NULL;
    }

    LOGGER_PUTS("[logger_take last msg dropped]\r\n");
    logger.nr_ready--;
    logger.nr_dropped++;
    logger.tail->state = LOGGER_ENT_STATE_FREE;
    logger.tail++;
    if (logger.tail == logger.array_end)
      logger.tail = logger.array;
    if (logger.tail > logger.array_end) {
      LOGGER_PUTS("[logger_take last msg dropped ERROR]\r\n");
      while(1)
        asm volatile("wfe");
    }
  }

  e = logger.head++;
  if (logger.head == logger.array_end)
    logger.head = logger.array;
  logger.nr_busy++;
  e->state = LOGGER_ENT_STATE_BUSY;
  LOGGER_PRINTF("[logger_take end good] %p\r\n", e);
  restore_irq_flags(irqflag);
  return e;
}

static void logger_mark_ready(struct logger_entry *e)
{
  int irqflag;
  disable_irq_save_flags(irqflag);
  LOGGER_PRINTF("[logger_mark_ready e:%p]\r\n", e);
  logger.nr_ready++;
  logger.nr_busy--;
  e->state = LOGGER_ENT_STATE_READY;
  os_event_notify(&logger.entry_is_ready_event);
  LOGGER_PUTS("[logger_mark_ready end]\r\n");
  restore_irq_flags(irqflag);
}

#define STACK_CANARY 0xff112277
void __os_log(const char *fmt, __builtin_va_list *args)
{
  int irqflags;
  struct logger_entry *e;

  if (!logger.thread_active) {
    disable_irq_save_flags(irqflags);
    vsnprintf(printfbuf, printfbuf_size, fmt, args);
    uart_pl011_send(printfbuf, 0);
    restore_irq_flags(irqflags);
    return;
  }

  e = logger_take();
  if (e) {
    vsnprintf(e->buf, sizeof(e->buf), fmt, args);
    logger_mark_ready(e);
  }
}

static struct logger_entry *logger_pop_next_ready(void)
{
  struct logger_entry *e;
  int irqflag;
  volatile int canary = STACK_CANARY;

  disable_irq_save_flags(irqflag);
  LOGGER_PUTS("[logger_pop_next_ready]\r\n");
  while (1) {
    if (logger.tail->state == LOGGER_ENT_STATE_READY)
      break;

    os_event_clear(&logger.entry_is_ready_event);
    LOGGER_PUTS("[logger_pop_next_ready wait]\r\n");
    restore_irq_flags(irqflag);
    os_event_wait(&logger.entry_is_ready_event);
    LOGGER_PRINTF("[logger_pop_next_ready wait end] %p, state:%d\r\n", logger.tail, logger.tail->state);
    disable_irq_save_flags(irqflag);
  }

  e = logger.tail++;
  if (logger.tail == logger.array_end)
    logger.tail = logger.array;
  if (logger.tail > logger.array_end) {
    printf("!!!!\r\n");
    while(1)
    asm volatile("wfe");
  }

  logger.nr_ready--;
  logger.nr_busy++;
  e->state = LOGGER_ENT_STATE_BUSY;
  LOGGER_PUTS("[logger_pop_next_ready end]\r\n");
  if (canary != STACK_CANARY) {
    printf("!!!%p,%p,%p\r\n", logger.array, logger.array_end, e);
    while(1);
  }
  restore_irq_flags(irqflag);
  return e;
}

static void logger_mark_free(struct logger_entry *e)
{
  int irqflag;

  disable_irq_save_flags(irqflag);
  e->state = LOGGER_ENT_STATE_FREE;
  logger.nr_busy--;
  restore_irq_flags(irqflag);
  /*
   * Design idea - we can notify threads that want to log if they want to block
   * while waiting for next free entry
   */
  // os_event_notify(&logger.entry_is_free_event);
}


#define BITMAP_ARR_LEN ROUND_UP(LOG_ENTRIES_COUNT, sizeof(uint64_t))

static struct logger_entry logger_entries[LOG_ENTRIES_COUNT];

static void logger_thread(void)
{
  struct logger_entry *e;
  logger.thread_active = true;

  while(1) {
    e = logger_pop_next_ready();
    if (logger.nr_dropped) {
      printf("log entries dropped: %d\r\n", logger.nr_dropped);
      logger.nr_dropped = 0;
    }
    if (logger.nr_dropped_takes) {
      printf("log requests dropped: %d\r\n", logger.nr_dropped_takes);
      logger.nr_dropped_takes = 0;
    }

    puts(e->buf);
    logger_mark_free(e);
  }
}

int logger_init(void)
{
  struct task *t;

  logger.head = logger.tail = logger.array = logger_entries;
  logger.nr_entries = ARRAY_SIZE(logger_entries);
  logger.array_end = logger.array + logger.nr_entries;
  logger.nr_dropped = 0;
  logger.nr_dropped_takes = 0;
  logger.nr_busy = 0;
  logger.nr_ready = 0;
  logger.thread_active = false;

  t = task_create(logger_thread, "logger");
  if (!t) {
    printf("Failed to create logger task\r\n");
    return ERR_GENERIC;
  }

  if (!sched_run_task_isr(t)) {
    printf("Failed to schedule logger task\r\n");
    return ERR_GENERIC;
  }

  os_event_init(&logger.entry_is_ready_event);
  return SUCCESS;
}
