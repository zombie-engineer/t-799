#pragma once

struct fat32_fs;

void vchiq_init(void);
void vchiq_set_fs(struct fat32_fs *fs);
