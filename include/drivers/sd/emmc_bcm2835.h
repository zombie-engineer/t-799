#pragma once
#include <stddef.h>
#include <block_device.h>
#include <stdint.h>

#define BCM2835_EMMC_BLOCK_SIZE 1024

#define BCM2835_EMMC_WAIT_TIMEOUT_USEC 1000000

int bcm2835_emmc_init(void);
int bcm2835_emmc_set_interrupt_mode(void);
void bcm2835_emmc_report(void);
int bcm2835_emmc_block_device_init(struct block_device *);
int bcm2835_emmc_set_clock(uint32_t clock_div);
