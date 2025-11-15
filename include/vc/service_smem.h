#pragma once
#include <stdint.h>

int smem_init(void);
int smem_import_dmabuf(void *addr, uint32_t size, uint32_t *vcsm_handle);
