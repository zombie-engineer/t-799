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
