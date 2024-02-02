#pragma once
#include <stddef.h>

void kmalloc_init(void);
void *kmalloc(size_t sz);
void *kzalloc(size_t sz);
void *dma_alloc(size_t sz);
void kfree(void *);
