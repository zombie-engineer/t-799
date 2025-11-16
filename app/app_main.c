#include <app/app_main.h>
#include <app/camera.h>
#include <vc/vchiq.h>
#include <vc/service_smem.h>
#include <vc/service_mmal.h>
#include <block_device.h>
#include <printf.h>
#include <os_api.h>
#include <fs/fs.h>
#include <ili9341.h>
#include <sdhc_test.h>
#include <errcode.h>
#include <logger.h>
#include <bcm2835/bcm_sdhost.h>

static struct block_device bdev_sdcard;
static struct block_device *bdev_partition;
static struct sdhc sdhc;

void app_main(void)
{
  int err;

  const uint32_t camera_bit_rate = 4 * 1000 * 1000;
  const int camera_frame_rate = 30;

  os_log("Application main\r\n");

  err = sdhc_init(&bdev_sdcard, &sdhc, &bcm_sdhost_ops);
  if (err != SUCCESS) {
    printf("Failed to init sdhc, %d\r\n", err);
    goto out;
  }

  os_log("SDHC intialized\r\n");

  if (!sdhc_run_self_test(&sdhc, &bdev_sdcard, false))
    goto out;

  err = sdhc_set_io_mode(&sdhc, SDHC_IO_MODE_IT_DMA, false);
  if (err != SUCCESS) {
    os_log("Failed to set sdhc io mode IT_DMA\r\n");
    goto out;
  }

  // blockdev_scheduler_init();
  err = fs_init(&bdev_sdcard, &bdev_partition);
  if (err != SUCCESS) {
    os_log("Failed to init fs block device, err: %d\r\n", err);
    goto out;
  }

  // sdhc_perf_measure(bdev_partition);

  if (!sdhc_run_self_test(&sdhc, &bdev_sdcard, true))
    goto out;

  err = ili9341_init();
  if (err != SUCCESS) {
    printf("Failed to init ili9341 display, err: %d\r\n", err);
    goto out;
  }

  fs_dump_partition(bdev_partition);

  err = vchiq_init();
  BUG_IF(err != SUCCESS, "failed to init VideoCore \"VCHIQ\"");
  err = mmal_init();
  BUG_IF(err != SUCCESS, "failed to init VideoCore service \"MMAL\"");
  err = smem_init();
  BUG_IF(err != SUCCESS, "failed to init VideoCore service \"SMEM\"");
  err = camera_init(bdev_partition, 1920, 1080, camera_frame_rate,
    camera_bit_rate);
  BUG_IF(err != SUCCESS, "failed to init camera");

  while (1) {
    asm volatile ("wfe");
  }

out:
  os_exit_current_task();
}
