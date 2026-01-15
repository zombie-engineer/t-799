#pragma once
#include <block_device.h>
#include <drivers/sd/sdhc.h>

void sdhc_perf_measure(struct block_device *bdev);
bool sdhc_run_self_test(struct sdhc *s, struct block_device *bdev, bool stream);
