#pragma once
#include <stdint.h>

/*
 * dcache_clean_and_invalidate_rng - cleans and invalidates
 * all data cache lines that form whole virtual address range
 * given by the arguments.
 * vaddr_start - virtual address of first byte in range
 * vaddr_end   - virtual address of end of range
 */
void dcache_clean_and_invalidate_rng(uint64_t vaddr_start, uint64_t vaddr_end);

void dcache_invalidate_rng(uint64_t vaddr_start, uint64_t vaddr_end);

#define dcache_flush(addr, size)\
  dcache_clean_and_invalidate_rng((uint64_t)(addr), (uint64_t)(addr) + (size))

#define dcache_invalidate(addr, size)\
  dcache_invalidate_rng((uint64_t)(addr), (uint64_t)(addr) + (size))

/*
 * Returns system clock frequency in Hz
 */
uint64_t get_cpu_counter_64_freq(void);

/*
 *  reads cpu-specific generic 64 bit counter
 */
uint64_t read_cpu_counter_64(void);

static inline void irq_enable(void)
{
  asm volatile ("msr daifclr, #(1 << 1)");
}

static inline void irq_disable(void)
{
  asm volatile ("msr daifset, #(1 << 1)");
}

#define disable_irq_save_flags(__flags)\
  asm volatile(\
      "mrs %0, daif\n"\
      "msr daifset, #2\n"\
      : "=r"(__flags)\
      :\
      : "memory")

#define restore_irq_flags(__flags)\
  asm volatile("msr daif, %0\n"\
      :\
      : "r"(__flags)\
      : "memory")

static inline uint64_t get_boottime_msec(void)
{
  /*
   *  19200000 counts = 1000 millisec
   *  19200    counts = 1    millisec
   *  CNT counts      = X millisec
   *
   *  X = CNT / 192000
   *  X = (cnt * 1000) / (freq)
   */
  uint64_t freq, cnt;
  asm volatile(
    "mrs %0, cntfrq_el0\n"
    "ubfx %0, %0, #0, #32\n"
    "mrs %1, cntpct_el0\n"
    :"=r"(freq), "=r"(cnt)
    :
    :"memory");
  return (cnt * 1000) / freq;
}
