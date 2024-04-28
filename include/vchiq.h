#pragma once

struct block_device;

void vchiq_init(void);
void vchiq_set_blockdev(struct block_device *bd);
