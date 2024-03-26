#pragma once
#include <stdbool.h>
#include <stdint.h>

int bcm2835_emmc_reset(bool blocking, uint32_t *rca, uint32_t *device_id);
