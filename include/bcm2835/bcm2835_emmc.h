#pragma once
#include <stddef.h>

#define BCM2835_EMMC_BLOCK_SIZE 1024

#define BCM2835_EMMC_WAIT_TIMEOUT_USEC 1000000

int bcm2835_emmc_init(void);
void bcm2835_emmc_report(void);
int bcm2835_emmc_read(size_t blocknum, size_t numblocks, char *buf);
int bcm2835_emmc_write(size_t blocknum, size_t numblocks, const char *buf);
