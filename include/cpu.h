#pragma once
#include <stdint.h>
#include <stdbool.h>

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

static inline bool irq_is_enabled(void)
{
  bool ret;

  asm volatile (
    /*
     * Move DAIF to x0, x0 will have bits set as  D | A | I | F | RES0,
     * Where RES0 is bits 0,1,2,3,4,5,
     * F - is bit 6,
     * I - is bit 7
     * Bit I == 1 (set) means IRQ exception /interrupt is masked (disabled),
     *      == 0 (clear) means IRQ exception / interrupt is not masked(enabled)
     */
    "mrs %0, daif\n"
    /* Invert bits in X0 to have 1 for interrupt enabled, 0 for disabled */
    "mvn %0, %0\n"
    /* Shift bit I (IRQ masked) to bit 0 */
    "lsr %0, %0, #7\n"
    /* Return 0 for IRQ disabled, 1 for IRQ enabled */
    "and %0, %0, #1\n"
    : "=r"(ret)
    :
    : "x0"
  );
  return ret;
}

#define aarch64_get_far_el1(__dst) \
  asm volatile(\
      "mrs %0, far_el1\n"\
      : "=r"(__dst)\
      :\
      : "memory")

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
