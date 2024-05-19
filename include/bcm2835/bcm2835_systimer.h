#pragma once
#include <stdint.h>
#include <timer.h>

void bcm2835_systimer_init(void);

void bcm2835_systimer_start_periodic(uint32_t usec, timer_callback_t cb,
  void *cb_arg);

void bcm2835_systimer_start_oneshot(uint32_t usec, timer_callback_t cb,
  void *cb_arg);

void bcm2835_systimer_clear_irq(int timer_idx);

uint64_t bcm2835_systimer_get_time_us(void);
uint64_t bcm2835_systimer_get_time_us_locked(void);
