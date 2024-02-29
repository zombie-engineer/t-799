#pragma once
#include <stdint.h>
#include <stdbool.h>

void mmu_print_va(uint64_t addr, int verbose);
void mmu_init(uint64_t dma_memory_start, uint64_t dma_memory_end);
bool mmu_get_pddr(uint64_t va, uint64_t *pa);
