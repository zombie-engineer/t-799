#include <sched_mon.h>

#if defined(CONFIG_SCHED_MON)
#include <stdint.h>
#include <cpu.h>
#include <printf.h>
#include <common.h>

struct sched_mon_record {
  uint64_t time;
  uint32_t ev;
  uint32_t value;
} __attribute__((packed));

static bool sched_mon_enabled = false;
static struct sched_mon_record sched_mon_data[256];
static int sched_mon_idx = 0;
static int sched_mon_num_events = 0;

void sched_mon_start(void)
{
  struct sched_mon_record *r;
  int irqflags;
  int i;
  disable_irq_save_flags(irqflags);
  sched_mon_idx = 0;

  for (i = 0; i < ARRAY_SIZE(sched_mon_data); ++i) {
    r = &sched_mon_data[i];
    r->ev = SCHED_MON_EV_NOT_YET;
    r->time = 0;
    r->value = 0;
  }

  sched_mon_enabled = true;
  asm volatile("dsb st");
  restore_irq_flags(irqflags);
}

void sched_mon_stop(void)
{
  int i;
  int irqflags;
  struct sched_mon_record *r;

  disable_irq_save_flags(irqflags);
  sched_mon_enabled = false;

#define DUMP_ITEM(__idx) \
  do { \
    r = &sched_mon_data[i]; \
    printf("%lu %u %u\r\n", r->time, r->ev, r->value); \
  } while(0)

  if (sched_mon_num_events >= ARRAY_SIZE(sched_mon_data)) {
    for (i = sched_mon_idx; i < ARRAY_SIZE(sched_mon_data); ++i)
      DUMP_ITEM(i);
  }

  for (i = 0; i < sched_mon_idx; ++i)
    DUMP_ITEM(i);

  restore_irq_flags(irqflags);
}

void sched_mon_event(sched_mon_event_t ev, int value)
{
  struct sched_mon_record *r;

  if (!sched_mon_enabled)
    return;

  r = &sched_mon_data[sched_mon_idx++];
  sched_mon_num_events++;

  if (sched_mon_idx > 255)
    sched_mon_idx = 0;

  r->ev = ev;
  r->value = value;
  r->time = arm_timer_get_count();
}

void sched_mon_save_ctx(void)
{
  sched_mon_event(SCHED_MON_EV_SAVE_CTX, 0);
}
#endif
