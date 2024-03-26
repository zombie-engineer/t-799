#pragma once

#define BCM2835_EMMC_BLOCK_SIZE 1024

#define BCM2835_EMMC_WAIT_TIMEOUT_USEC 1000000

int bcm2835_emmc_init(void);
void bcm2835_emmc_report(void);
int bcm2835_emmc_read(int blocknum, int numblocks, char *buf, int bufsz);
int bcm2835_emmc_write(int blocknum, int numblocks, const char *buf,
  int bufsz);
