#include <app/app_main.h>
#include <app/camera.h>
#include <vc/vchiq.h>
#include <vc/service_smem.h>
#include <vc/service_mmal.h>
#include <block_device.h>
#include <printf.h>
#include <os_api.h>
#include <fs/fs.h>
#include <drivers/display/display_ili9341.h>
#include <drivers/sd/sdhc_test.h>
#include <errcode.h>
#include <logger.h>
#include <drivers/sd/sdhost_bcm2835.h>
#include <kmalloc.h>
#include <drivers/spi.h>
#include <stat_fifo.h>
#include "ui/ui.h"

#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 240
#define FRAME_WIDTH 1920
#define FRAME_HEIGHT 1080
#define PREVIEW_WIDTH 320
#define PREVIEW_HEIGHT 180
#define PREVIEW_ENABLE false
#define CAMERA_BIT_RATE (4 * 1000 * 1000)
#define CAMERA_FRAME_RATE 30

#define GPIO_BLK   19
#define GPIO_DC    13
#define GPIO_RESET 6

#define GAP_X 2

#define BPS_TEXT_POS_X 0
#define BPS_TEXT_POS_Y 0
#define BPS_TEXT_SIZE_X 60
#define BPS_TEXT_SIZE_Y 20

#define BPS_DIAGRAM_POS_X (BPS_TEXT_POS_X + BPS_TEXT_SIZE_X + GAP_X)
#define BPS_DIAGRAM_POS_Y BPS_TEXT_POS_Y
#define BPS_DIAGRAM_WIDTH (DISPLAY_WIDTH - BPS_DIAGRAM_POS_X)
#define BPS_DIAGRAM_HEIGHT (BPS_TEXT_SIZE_Y + 1)

#define FPS_TEXT_POS_X 0
#define FPS_TEXT_POS_Y 20
#define FPS_TEXT_SIZE_X 60
#define FPS_TEXT_SIZE_Y 20

#define FPS_DIAGRAM_POS_X (FPS_TEXT_POS_X + FPS_TEXT_SIZE_X + GAP_X)
#define FPS_DIAGRAM_POS_Y FPS_TEXT_POS_Y
#define FPS_DIAGRAM_WIDTH (DISPLAY_WIDTH - FPS_DIAGRAM_POS_X)
#define FPS_DIAGRAM_HEIGHT (FPS_TEXT_SIZE_Y + 1)

static struct block_device bdev_sdcard;
static struct block_device *bdev_partition;
static struct sdhc sdhc;
static struct ili9341_drawframe drawframes[2];

const struct ui_conf ui_conf = {
  .diagram_bps = {
     .text_pos_x = BPS_TEXT_POS_X,
     .text_pos_y = BPS_TEXT_POS_Y,
     .text_size_x = BPS_TEXT_SIZE_X,
     .text_size_y = BPS_TEXT_SIZE_Y,
     .graph_pos_x = BPS_DIAGRAM_POS_X,
     .graph_pos_y = BPS_DIAGRAM_POS_Y,
     .graph_size_x = BPS_DIAGRAM_WIDTH,
     .graph_size_y = BPS_DIAGRAM_HEIGHT,
     .max_value = 30000000
  },
  .diagram_fps = {
     .text_pos_x = FPS_TEXT_POS_X,
     .text_pos_y = FPS_TEXT_POS_Y,
     .text_size_x = FPS_TEXT_SIZE_X,
     .text_size_y = FPS_TEXT_SIZE_Y,
     .graph_pos_x = FPS_DIAGRAM_POS_X,
     .graph_pos_y = FPS_DIAGRAM_POS_Y,
     .graph_size_x = FPS_DIAGRAM_WIDTH,
     .graph_size_y = FPS_DIAGRAM_HEIGHT,
     .max_value = CAMERA_FRAME_RATE * 1.2
  },
  .canvas_size_x = DISPLAY_WIDTH,
  .canvas_size_y = DISPLAY_HEIGHT - PREVIEW_HEIGHT,
};

extern void fetch_clear_stats(uint32_t *out_num_h264_frames,
  uint32_t *out_total_bytes, uint32_t *out_num_frames_done);

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

struct ili9341_per_frame_dma dmabuf_camera[1];

struct ili9341_per_frame_dma dmabuf_app;

static __attribute__((optimize("O0"))) int init_display_region_configs(
  struct ili9341_drawframe *df_cam,
  struct ili9341_drawframe *df_app,
  int preview_width, int preview_height,
  int display_width, int display_height)
{
  int i;
  size_t buf_size;

  df_cam->frame.pos.x = 0;
  df_cam->frame.pos.y = 0;
  df_cam->frame.size.x = preview_width;
  df_cam->frame.size.y = preview_height;
  df_app->frame.pos.x = 0;
  df_app->frame.pos.y = df_cam->frame.size.y;
  df_app->frame.size.x = display_width;
  df_app->frame.size.y = display_height - df_cam->frame.size.y;

  buf_size = ili9341_get_frame_byte_size(df_cam->frame.size.x,
    df_cam->frame.size.y);

  for (i = 0; i < ARRAY_SIZE(dmabuf_camera); ++i) {
    INIT_LIST_HEAD(&dmabuf_camera[i].draw_tasks);
    dmabuf_camera[i].buf = dma_alloc(buf_size, false);
    if (!dmabuf_camera[i].buf) {
      os_log("Failed to alloc display buf#%d for cam preview\r\n", i);
      return ERR_RESOURCE;
    }
    os_log("Allocated %d bytes dma buf#%d for camera\r\n", buf_size, i);
    dmabuf_camera[i].buf_size = buf_size;
    dmabuf_camera[i].handle = i;
  }

  df_cam->bufs = dmabuf_camera;
  df_cam->num_bufs = ARRAY_SIZE(dmabuf_camera);

  INIT_LIST_HEAD(&dmabuf_app.draw_tasks);
  buf_size = ili9341_get_frame_byte_size(df_app->frame.size.x,
    df_app->frame.size.y);
  dmabuf_app.buf = dma_alloc(buf_size, false);
  if (!dmabuf_app.buf) {
    os_log("Failed to alloc display buf for app");
    return ERR_RESOURCE;
  }
  os_log("Allocated %d bytes dma buf#%d for app\r\n", buf_size);
  dmabuf_app.buf_size = buf_size;

  df_app->bufs = &dmabuf_app;
  df_app->num_bufs = 1;

  buf_size = ili9341_get_frame_byte_size(df_cam->frame.size.x,
    df_cam->frame.size.y);

  df_app->bufs = &dmabuf_app;
  df_app->num_bufs = 1;

  return SUCCESS;
}

static int app_init_display(void)
{
  int err;

  err = init_display_region_configs(&drawframes[0], &drawframes[1],
    PREVIEW_WIDTH, PREVIEW_HEIGHT, DISPLAY_WIDTH, DISPLAY_HEIGHT);
  if (err) {
    os_log("Failed to init ili9341 region config for camera, err: %d", err);
    return err;
  }

  err = ili9341_init(GPIO_BLK, GPIO_DC, GPIO_RESET, drawframes,
    ARRAY_SIZE(drawframes));
  if (err != SUCCESS) {
    os_log("Failed to init ili9341 display, err: %d", err);
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
    PREVIEW_ENABLE, &drawframes[0], PREVIEW_WIDTH,
    PREVIEW_HEIGHT);
  BUG_IF(err != SUCCESS, "failed to init camera");

#if 0
  os_wait_ms(1000 * 10);
  printf("Started capture");
  camera_video_start();
#endif
}

struct semaphore sem_ui_draw;

static uint32_t time_now_ms(void)
{
  return arm_timer_get_count() / (19200000.0f / 1000);
}

static void app_ui_draw_done_isr(struct ili9341_per_frame_dma *buf)
{
  os_semaphore_give_isr(&sem_ui_draw);
}

static NOOPT void app_ui_loop(void)
{
  uint64_t ts_next_wkup;
  uint64_t ts;
  uint32_t num_h264_frames;
  uint32_t total_bytes;
  uint32_t num_frames_done;
  struct ili9341_drawframe *df = &drawframes[1];
  struct ili9341_per_frame_dma *dma_io = &df->bufs[0];
  struct ui_data d;

  memset(dma_io->buf, 0, dma_io->buf_size);
  ui_init(&ui_conf, dma_io->buf, dma_io->buf_size);

  ili9341_drawframe_set_irq(df, app_ui_draw_done_isr);

  os_semaphore_init(&sem_ui_draw, 0, 10);

  ts_next_wkup = time_now_ms() + 1000;

  while (1) {
    ts = time_now_ms();
    ts_next_wkup = ts + 1000;
    fetch_clear_stats(&num_h264_frames, &total_bytes, &num_frames_done);
    d.bitrate = total_bytes * 8;
    d.fps = num_h264_frames;
    ui_redraw(&d);
    ili9341_draw_dma_buf(dma_io);
    os_semaphore_take(&sem_ui_draw);
    ts = time_now_ms();
    if (ts < ts_next_wkup) {
      os_wait_ms(ts_next_wkup - ts);
    }
  }
}

void NOOPT app_main(void)
{
  os_log("Application main");

  spi_init();

  if (app_init_sdcard() != SUCCESS)
    goto out;

  if (app_init_display() != SUCCESS)
    goto out;

  app_init_camera();
  app_ui_loop();

out:
  while (1) {

    asm volatile ("wfe");
  }
}
