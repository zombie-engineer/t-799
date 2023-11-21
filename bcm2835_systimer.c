#include <timer.h>
#include <bcm2835/bcm2835_systimer.h>
#include <bcm2835/bcm2835_ic.h>
#include <memory_map.h>
#include <ioreg.h>
#include <string.h>
#include <irq.h>
#include <common.h>

#define BCM2835_SYSTIMER_BASE   (uint64_t)(BCM2835_MEM_PERIPH_BASE + 0x3000)
#define BCM2835_SYSTIMER_CS     (ioreg32_t)(BCM2835_SYSTIMER_BASE + 0x00)
#define BCM2835_SYSTIMER_CLO    (ioreg32_t)(BCM2835_SYSTIMER_BASE + 0x04)
#define BCM2835_SYSTIMER_CHI    (ioreg32_t)(BCM2835_SYSTIMER_BASE + 0x08)
#define BCM2835_SYSTIMER_C0     (ioreg32_t)(BCM2835_SYSTIMER_BASE + 0x0c)
#define BCM2835_SYSTIMER_C1     (ioreg32_t)(BCM2835_SYSTIMER_BASE + 0x10)
#define BCM2835_SYSTIMER_C2     (ioreg32_t)(BCM2835_SYSTIMER_BASE + 0x14)
#define BCM2835_SYSTIMER_C3     (ioreg32_t)(BCM2835_SYSTIMER_BASE + 0x18)

struct bcm2835_systimer {
  timer_callback_t cb;
  void *cb_arg;
  uint32_t period;
};

static struct bcm2835_systimer systimer1;

static void bcm2835_systimer_clear_irq_1()
{
  ioreg32_write(BCM2835_SYSTIMER_CS, (1<<1));
}

void bcm2835_systimer_clear_irq(int timer_idx)
{
  if (timer_idx > 3)
    panic();
  
  ioreg32_write(BCM2835_SYSTIMER_CS, 1 << timer_idx);
}

static void bcm2835_systimer_start(uint32_t usec)
{
  uint32_t clo;

  clo = ioreg32_read(BCM2835_SYSTIMER_CLO);
  ioreg32_write(BCM2835_SYSTIMER_C1, clo + usec);
}

static void bcm2835_systimer_info_reset(struct bcm2835_systimer *t)
{
  memset(t, 0, sizeof(*t));
}

static void bcm2835_systimer_info_set(struct bcm2835_systimer *t,
  timer_callback_t cb, void *cb_arg, uint32_t period)
{
  t->cb = cb;
  t->cb_arg = cb_arg;
  t->period = period;
}

static void bcm2835_systimer_run_cb_1(void)
{
  if (systimer1.cb)
    systimer1.cb(systimer1.cb_arg);
}

static void bcm2835_systimer_cb_periodic_timer_1(void)
{
  bcm2835_systimer_clear_irq_1();
  bcm2835_systimer_run_cb_1();
}

void bcm2835_systimer_start_periodic(uint32_t usec, timer_callback_t cb,
  void *cb_arg)
{
  bcm2835_systimer_info_set(&systimer1, cb, cb_arg, usec);
  bcm2835_systimer_start(usec);
}

void bcm2835_systimer_start_oneshot(uint32_t usec, timer_callback_t cb,
  void *cb_arg)
{
  bcm2835_systimer_info_set(&systimer1, cb, cb_arg, 0);
  bcm2835_systimer_start(usec);
}

/* Count maximum cycles for execution of bcm2835_systimer_start function */
static uint32_t bcm2835_systimer_get_min_set_time(void)
{
  uint32_t t1, t2;
  uint32_t max_delta;
  const unsigned num_samples = 8;
  unsigned i;
  if (ioreg32_read(BCM2835_SYSTIMER_CS))
    panic_with_log("bcm2835_systimer_get_min_set_time should not "
      "be called after timer is set\n");

  max_delta = 0;
  // While loop here to calculate time range only when
  // non overflowed values
  for (i = 0; i < num_samples; ++i) {
    while(1) {
      t1 = ioreg32_read(BCM2835_SYSTIMER_CLO);
      bcm2835_systimer_start(10);
      t2 = ioreg32_read(BCM2835_SYSTIMER_CLO);
      ioreg32_write(BCM2835_SYSTIMER_CS, 0);
      if (t2 > t1) {
        if (t2 - t1 > max_delta)
          max_delta = t2 - t1;
        break;
      }
    }
  }
  return max_delta;
}

void __irq_routine bcm2835_systimer_irq_handler(void)
{
  bcm2835_systimer_clear_irq_1();
  if (systimer1.cb)
    systimer1.cb(systimer1.cb_arg);
}

void bcm2835_systimer_init(void)
{
  bcm2835_systimer_info_reset(&systimer1);
  irq_set(BCM2835_IRQNR_SYSTIMER_1, bcm2835_systimer_irq_handler);
  bcm2835_ic_enable_irq(BCM2835_IRQNR_SYSTIMER_1);
}

void bcm2835_systimer_get(void)
{
}
