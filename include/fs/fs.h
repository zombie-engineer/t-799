#pragma once
struct block_device;
void fs_probe_early(void);

int write_image_to_sd(const char *filename, char *image_start,
  char *image_end);

int fs_init(struct block_device *b);
