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
#define DISPLAY_PREVIEW_WIDTH 320
#define DISPLAY_PREVIEW_HEIGHT 180

#define CAM_VIDEO_FRAME_WIDTH 1920
#define CAM_VIDEO_FRAME_HEIGHT 1080
#define CAM_VIDEO_BUFFERS_NUM 128
#define CAM_VIDEO_BUFFER_SIZE (256 * 1024)
#define CAM_PREVIEW_WIDTH DISPLAY_PREVIEW_WIDTH
#define CAM_PREVIEW_HEIGHT DISPLAY_PREVIEW_HEIGHT
#define CAM_ENABLE_PREVIEW true
#define CAM_VIDEO_BITRATE (4 * 1000 * 1000)
#define CAM_VIDEO_FRAMES_PER_SEC 30

#define CAM_H264_Q_MIN 10
#define CAM_H264_Q_MAX 22
#define CAM_H264_Q_INIT 18
#define CAM_H264_PROFILE MMAL_VIDEO_PROFILE_H264_HIGH
#define CAM_H264_LEVEL MMAL_VIDEO_LEVEL_H264_4

#include "ui_config.h"

#define GPIO_BLK   19
#define GPIO_DC    13
#define GPIO_RESET 6

static struct block_device bdev_sdcard;
static struct block_device *bdev_partition;
static struct sdhc sdhc;
static struct ili9341_drawframe drawframes[2];

static struct ui_data ui_data = { 0 };

static struct canvas_plot_with_value_text ui_plots[] = {
  CANVAS_PLOT_WITH_TEXT_INIT(H264_BPS, &ui_data.h264_bps     , "bps", 1024),
  CANVAS_PLOT_WITH_TEXT_INIT(H264_FPS, &ui_data.h264_fps     , "fps", 1000),
  CANVAS_PLOT_WITH_TEXT_INIT(SD_WR_BPS, &ui_data.sd_write_bps, "bps", 1024),
};

static struct ui ui = {
  .canvas_size.x = DISPLAY_WIDTH,
  .canvas_size.y = DISPLAY_HEIGHT - DISPLAY_PREVIEW_HEIGHT,
  .plots = ui_plots,
  .num_plots = ARRAY_SIZE(ui_plots),
  .h264_num_bufs = {
    .pos = { .x = H264_NUM_BUFS_X, .y = H264_NUM_BUFS_Y },
    .size = { .x = H264_NUM_BUFS_SIZE_X, .y = H264_NUM_BUFS_SIZE_Y },
  }
};

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

static int init_display_region_configs(
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
    DISPLAY_PREVIEW_WIDTH, DISPLAY_PREVIEW_HEIGHT,
    DISPLAY_WIDTH, DISPLAY_HEIGHT);
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

  const struct camera_config camera_config = {
    .block_device = bdev_partition,
    .frame_size_x = CAM_VIDEO_FRAME_WIDTH,
    .frame_size_y = CAM_VIDEO_FRAME_HEIGHT,
    .frames_per_sec = CAM_VIDEO_FRAMES_PER_SEC,
    .bit_rate = CAM_VIDEO_BITRATE,
    .enable_preview = CAM_ENABLE_PREVIEW,
    .preview_drawframe = &drawframes[0],
    .preview_size_x = CAM_PREVIEW_WIDTH,
    .preview_size_y = CAM_PREVIEW_HEIGHT,
    .h264_buffers_num = CAM_VIDEO_BUFFERS_NUM,
    .h264_buffer_size = CAM_VIDEO_BUFFER_SIZE,
    .h264_encoder = {
      .q_initial = CAM_H264_Q_INIT,
      .q_min = CAM_H264_Q_MIN,
      .q_max = CAM_H264_Q_MAX,
      .profile = CAM_H264_PROFILE,
      .level = CAM_H264_LEVEL,
      .intraperiod = CAM_VIDEO_FRAMES_PER_SEC * 2,
      .bitrate = CAM_VIDEO_BITRATE
    }
  };

  err = camera_init(&camera_config);
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
  uint32_t prev_bytes_from_vc;
  uint32_t prev_frame_end_from_vc;
  struct ili9341_drawframe *df = &drawframes[1];
  struct ili9341_per_frame_dma *dma_io = &df->bufs[0];
  struct sdhc_iostats sd_iostats;
  struct cam_port_stats cam_h264_stats;

  memset(dma_io->buf, 0, dma_io->buf_size);
  ui_init(&ui, dma_io->buf, dma_io->buf_size);

  ili9341_drawframe_set_irq(df, app_ui_draw_done_isr);

  os_semaphore_init(&sem_ui_draw, 0, 10);

  ts_next_wkup = time_now_ms() + 1000;

  while (1) {
    ts = time_now_ms();
    ts_next_wkup = ts + 1000;
    cam_port_stats_fetch(&cam_h264_stats, CAM_PORT_STATS_H264);
    sdhc_iostats_fetch_clear(&sd_iostats);

    ui_data.h264_bps = (cam_h264_stats.bytes_from_vc - prev_bytes_from_vc) * 8;
    ui_data.h264_fps = cam_h264_stats.frame_end_from_vc -
      prev_frame_end_from_vc;

    ui_data.h264_bufs_on_arm = cam_h264_stats.bufs_on_arm;
    ui_data.h264_bufs_from_vc = cam_h264_stats.bufs_from_vc;
    ui_data.h264_bufs_to_vc = cam_h264_stats.bufs_to_vc;

    ui_data.sd_write_bps = sd_iostats.num_bytes_written;

    prev_frame_end_from_vc = cam_h264_stats.frame_end_from_vc;
    prev_bytes_from_vc = cam_h264_stats.bytes_from_vc;

    ui_redraw(&ui_data);
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
