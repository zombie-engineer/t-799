#pragma once
#include <sdhc.h>

extern struct sdhc_ops bcm_sdhost_ops;
void bcm_sdhost_set_log_level(int l);
