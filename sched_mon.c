#include <sched_mon.h>

#if defined(CONFIG_SCHED_MON)
#include <stdint.h>
#include <cpu.h>
#include <printf.h>

struct sched_mon_record {
  uint64_t schedule_time;
  int task_id;
};

extern uint64_t __last_restore_ctx_time;

static bool sched_mon_enabled = false;
static struct sched_mon_record sched_mon_data[256];
static int sched_mon_idx = 0;

static inline uint64_t sched_get_last_restore_ctx_time(void)
{
  uint64_t ret;
  asm volatile (
    "ldr %0, =__last_restore_ctx_time\n"
    "ldr %0, [%0]\n"
    :"=r"(ret));

  return ret;
}

void sched_mon_start(void)
{
  int irqflags;
  disable_irq_save_flags(irqflags);
  sched_mon_idx = 0;
  sched_mon_enabled = true;
  asm volatile("dsb st");
  restore_irq_flags(irqflags);
}

void sched_mon_stop(const char *tag, uint64_t ts0, uint64_t ts1)
{
  int i;
  int task_id;
  int irqflags;
  uint64_t last_restore_ctx_time;
  uint64_t sched_ts;

  disable_irq_save_flags(irqflags);
  sched_mon_enabled = false;
  last_restore_ctx_time = sched_get_last_restore_ctx_time();

  printf("%s: %lu %lu %d [", tag, ts0, ts1, sched_mon_idx);

  for (i = 0; i < sched_mon_idx; ++i)
    printf(" %d:%lu", sched_mon_data[i].task_id,
      sched_mon_data[i].schedule_time);

  printf("] %lu\r\n", last_restore_ctx_time);
  restore_irq_flags(irqflags);
}

void sched_mon_schedule(int task_id)
{
  if (!sched_mon_enabled)
    return;

  sched_mon_data[sched_mon_idx].task_id = task_id;
  sched_mon_data[sched_mon_idx].schedule_time = arm_timer_get_count();
  sched_mon_idx++;
  if (sched_mon_idx > 255)
    sched_mon_idx = 0;
}
#endif
