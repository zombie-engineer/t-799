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
#include <kmalloc.h>

#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 240
#define FRAME_WIDTH 1920
#define FRAME_HEIGHT 1080
#define PREVIEW_WIDTH 320
#define PREVIEW_HEIGHT 180
#define PREVIEW_ENABLE true
#define CAMERA_BIT_RATE (4 * 1000 * 1000)
#define CAMERA_FRAME_RATE 30

#define GPIO_BLK   19
#define GPIO_DC    13
#define GPIO_RESET 6


static struct block_device bdev_sdcard;
static struct block_device *bdev_partition;
static struct sdhc sdhc;

static inline int app_init_sdcard(void)
{
  int err;

  err = sdhc_init(&bdev_sdcard, &sdhc, &bcm_sdhost_ops);
  if (err != SUCCESS) {
    os_log("Failed to init sdhc, %d", err);
    return err;
  }

  os_log("SDHC intialized\r\n");

#if 0
  if (!sdhc_run_self_test(&sdhc, &bdev_sdcard, false))
    goto out;
#endif

  err = sdhc_set_io_mode(&sdhc, SDHC_IO_MODE_IT_DMA, false);
  if (err != SUCCESS) {
    os_log("Failed to set sdhc io mode IT_DMA");
    return err;
  }

  // blockdev_scheduler_init();
  err = fs_init(&bdev_sdcard, &bdev_partition);
  if (err != SUCCESS) {
    os_log("Failed to init fs block device, err: %d", err);
    return err;
  }

  // sdhc_perf_measure(bdev_partition);

#if 0
  if (!sdhc_run_self_test(&sdhc, &bdev_sdcard, true))
    goto out;
#endif

  fs_dump_partition(bdev_partition);
  return SUCCESS;
}

static inline int init_display_region_configs(struct ili9341_region_cfg *c,
  int preview_width, int preview_height,
  int display_width, int display_height)
{
  int i;
  uint8_t *cam_dma_bufs[2];
  uint8_t *app_dma_buf;
  size_t buf_size;
  struct ili9341_region_cfg *cfg_cam = &c[0];
  struct ili9341_region_cfg *cfg_app = &c[1];

  cfg_cam->frame.pos.x = 0;
  cfg_cam->frame.pos.y = 0;
  cfg_cam->frame.size.x = preview_width;
  cfg_cam->frame.size.y = preview_height;
  cfg_app->frame.pos.x = 0;
  cfg_app->frame.pos.y = cfg_cam->frame.size.y;
  cfg_app->frame.size.x = display_width;
  cfg_app->frame.size.y = display_height - cfg_cam->frame.size.y;

  buf_size = ili9341_get_frame_byte_size(cfg_cam->frame.size.x,
    cfg_cam->frame.size.y);

  for (i = 0; i < ARRAY_SIZE(cam_dma_bufs); ++i) {
    cam_dma_bufs[i] = dma_alloc(buf_size, false);
    if (!cam_dma_bufs[i]) {
      os_log("Failed to alloc display buf#%d for cam preview\r\n", i);
      return ERR_RESOURCE;
    }
    os_log("Allocated %d bytes dma buf#%d for camera\r\n", buf_size, i);
  }

  app_dma_buf = dma_alloc(buf_size, false);
  if (!app_dma_buf) {
    os_log("Failed to alloc display buf for app");
    return ERR_RESOURCE;
  }
  os_log("Allocated %d bytes dma buf#%d for app\r\n", buf_size);

  buf_size = ili9341_get_frame_byte_size(cfg_cam->frame.size.x,
    cfg_cam->frame.size.y);

  cfg_cam->dma_buffers = cam_dma_bufs;
  cfg_cam->num_dma_buffers = ARRAY_SIZE(cam_dma_bufs);

  cfg_app->dma_buffers = &app_dma_buf;
  cfg_app->num_dma_buffers = 1;

  return SUCCESS;
}

static inline int app_init_display(void)
{
  int err;

  struct ili9341_region_cfg regions[2];

  err = ili9341_init(GPIO_BLK, GPIO_DC, GPIO_RESET);
  if (err != SUCCESS) {
    os_log("Failed to init ili9341 display, err: %d", err);
    return err;
  }

  err = init_display_region_configs(regions, PREVIEW_WIDTH, PREVIEW_HEIGHT,
    DISPLAY_WIDTH, DISPLAY_HEIGHT);
  if (err) {
    os_log("Failed to init ili9341 region config for camera, err: %d", err);
    return err;
  }

  err = ili9341_regions_init(regions, ARRAY_SIZE(regions));
  if (err != SUCCESS) {
    printf("Failed to init ili9341 frames, err: %d\r\n", err);
    return err;
  }

  return SUCCESS;
}

static inline void app_init_camera(void)
{
  int err;

  err = vchiq_init();
  BUG_IF(err != SUCCESS, "failed to init VideoCore \"VCHIQ\"");
  err = mmal_init();
  BUG_IF(err != SUCCESS, "failed to init VideoCore service \"MMAL\"");
  err = smem_init();
  BUG_IF(err != SUCCESS, "failed to init VideoCore service \"SMEM\"");

  err = camera_init(bdev_partition,
    FRAME_WIDTH, FRAME_HEIGHT,
    CAMERA_FRAME_RATE,
    CAMERA_BIT_RATE,
    PREVIEW_ENABLE, PREVIEW_WIDTH,
    PREVIEW_HEIGHT);
  BUG_IF(err != SUCCESS, "failed to init camera");

#if 0
  os_wait_ms(1000 * 10);
  printf("Started capture");
  camera_video_start();
#endif
}

void app_main(void)
{
  os_log("Application main");

  if (app_init_sdcard() != SUCCESS)
    goto out;

  if (app_init_display() != SUCCESS)
    goto out;

  app_init_camera();

out:
  while (1) {
    asm volatile ("wfe");
  }
}
