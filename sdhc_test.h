#pragma once
#include <block_device.h>
#include <sdhc.h>

void sdhc_perf_measure(struct block_device *bdev);
int sdhc_run_self_test(struct sdhc *s, struct block_device *bdev);
