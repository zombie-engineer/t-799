#pragma once
#include <block_device.h>
#include <bcm2835/bcm2835_emmc.h>

int bcm2835_emmc_block_device_init(struct block_device *);
