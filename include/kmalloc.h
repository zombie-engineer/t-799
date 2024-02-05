#pragma once
#include <stddef.h>
#include <stdint.h>

void kmalloc_init(void);
void *kmalloc(size_t sz);
void *kzalloc(size_t sz);
void *dma_alloc(size_t sz);
void kfree(void *);
void dma_memory_init(void);
void *dma_alloc(size_t sz);
void dma_free(void *);

uint64_t dma_memory_get_start_addr(void);
uint64_t dma_memory_get_end_addr(void);
