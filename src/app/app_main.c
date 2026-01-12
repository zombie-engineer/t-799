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
#include <spi.h>

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


static struct block_device bdev_sdcard;
static struct block_device *bdev_partition;
static struct sdhc sdhc;
static struct ili9341_drawframe drawframes[2];

extern void fetch_clear_stats(int *out_num_buffers, int *out_total_bytes, int *out_num_bufs_on_vc);
struct font_glyph {
  uint8_t data[8];
};

/*
 * 8x8 monospace debug font
 * ASCII indexed (32..90 partially filled)
 */
static const struct font_glyph font8x8[] = {
    /* ' ' (space) ASCII 32 */
    { { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 } },

    /* '!' */
    { { 0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00 } },

    /* '"' */
    { { 0x36,0x36,0x24,0x00,0x00,0x00,0x00,0x00 } },

    /* '#' */
    { { 0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00 } },

    /* '$' */
    { { 0x18,0x3E,0x58,0x3C,0x1A,0x7C,0x18,0x00 } },

    /* '%' */
    { { 0x62,0x64,0x08,0x10,0x26,0x46,0x00,0x00 } },

    /* '&' */
    { { 0x30,0x48,0x30,0x4A,0x44,0x3A,0x00,0x00 } },

    /* ''' */
    { { 0x18,0x18,0x10,0x00,0x00,0x00,0x00,0x00 } },

    /* '(' */
    { { 0x0C,0x10,0x20,0x20,0x20,0x10,0x0C,0x00 } },

    /* ')' */
    { { 0x30,0x08,0x04,0x04,0x04,0x08,0x30,0x00 } },

    /* '*' */
    { { 0x00,0x24,0x18,0x7E,0x18,0x24,0x00,0x00 } },

    /* '+' */
    { { 0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00 } },

    /* ',' */
    { { 0x00,0x00,0x00,0x00,0x18,0x18,0x10,0x00 } },

    /* '-' */
    { { 0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00 } },

    /* '.' */
    { { 0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00 } },

    /* '/' */
    { { 0x02,0x04,0x08,0x10,0x20,0x40,0x00,0x00 } },

    /* '0' */
    { { 0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00 } },

    /* '1' */
    { { 0x18,0x38,0x18,0x18,0x18,0x18,0x3C,0x00 } },

    /* '2' */
    { { 0x3C,0x66,0x06,0x1C,0x30,0x66,0x7E,0x00 } },

    /* '3' */
    { { 0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00 } },

    /* '4' */
    { { 0x0C,0x1C,0x2C,0x4C,0x7E,0x0C,0x0C,0x00 } },

    /* '5' */
    { { 0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00 } },

    /* '6' */
    { { 0x1C,0x30,0x60,0x7C,0x66,0x66,0x3C,0x00 } },

    /* '7' */
    { { 0x7E,0x66,0x06,0x0C,0x18,0x18,0x18,0x00 } },

    /* '8' */
    { { 0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00 } },

    /* '9' */
    { { 0x3C,0x66,0x66,0x3E,0x06,0x0C,0x38,0x00 } },

    /* ':' */
    { { 0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00 } },

    /* 'A' */
    { { 0x18,0x24,0x42,0x7E,0x42,0x42,0x42,0x00 } },

    /* 'B' */
    { { 0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00 } },

    /* 'C' */
    { { 0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00 } },

    /* 'D' */
    { { 0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00 } },

    /* 'E' */
    { { 0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0x00 } },

    /* 'F' */
    { { 0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0x00 } },

    /* 'G' */
    { { 0x3C,0x66,0x60,0x6E,0x66,0x66,0x3C,0x00 } },

    /* 'H' */
    { { 0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00 } },

    /* 'I' */
    { { 0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00 } },

    /* 'J' */
    { { 0x1E,0x0C,0x0C,0x0C,0x0C,0x6C,0x38,0x00 } },

    /* 'K' */
    { { 0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00 } },

    /* 'L' */
    { { 0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00 } },

    /* 'M' */
    { { 0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00 } },

    /* 'N' */
    { { 0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0x00 } },

    /* 'O' */
    { { 0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00 } },

    /* 'P' */
    { { 0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00 } },

    /* 'Q' */
    { { 0x3C,0x66,0x66,0x66,0x6E,0x3C,0x0E,0x00 } },

    /* 'R' */
    { { 0x7C,0x66,0x66,0x7C,0x78,0x6C,0x66,0x00 } },

    /* 'S' */
    { { 0x3C,0x66,0x30,0x18,0x0C,0x66,0x3C,0x00 } },

    /* 'T' */
    { { 0x7E,0x5A,0x18,0x18,0x18,0x18,0x3C,0x00 } },

    /* 'U' */
    { { 0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00 } },

    /* 'V' */
    { { 0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00 } },

    /* 'W' */
    { { 0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00 } },

    /* 'X' */
    { { 0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00 } },

    /* 'Y' */
    { { 0x66,0x66,0x66,0x3C,0x18,0x18,0x3C,0x00 } },

    /* 'Z' */
    { { 0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00 } },
};


static void u32_to_str(uint32_t v, char *out) {
    char tmp[11];
    int i = 0;

    if (v == 0) {
        out[0] = '0';
        out[1] = 0;
        return;
    }

    while (v) {
        tmp[i++] = '0' + (v % 10);
        v /= 10;
    }

    int j = 0;
    while (i--) {
        out[j++] = tmp[i];
    }
    out[j] = 0;
}

struct canvas {
  uint16_t size_x;
  uint16_t size_y;

  /* pixels */
  uint8_t *data;

  /* foreground color */
  uint8_t fg_r;
  uint8_t fg_g;
  uint8_t fg_b;

  /* background color */
  uint8_t bg_r;
  uint8_t bg_g;
  uint8_t bg_b;
};

static inline void fb_set_pixel_color(struct canvas *c,
    unsigned x, unsigned y,
    uint8_t r, uint8_t g, uint8_t b)
{
    if (x >= c->size_x|| y >= c->size_y)
        return;

    uint8_t *p = c->data + (y * c->size_x + x) * 3;
    p[0] = r;
    p[1] = g;
    p[2] = b;
}

void fb_draw_char(struct canvas *c, int x, int y, char ch)
{
  const uint8_t *glyph;
  int row, col;
  uint8_t bits;

  if (ch < 32 || ch > 127) {
    printf("char: %02x, not drawing\r\n", ch);
    return;
  }

  glyph = font8x8[ch - 32].data;

  for (row = 0; row < 8; row++) {
    bits = glyph[row];
    for (col = 0; col < 8; col++) {
      if (bits & (1 << (7 - col))) {
        fb_set_pixel_color(c, x + col, y + row, c->fg_r, c->fg_g,
          c->fg_b);
      }
      else
        fb_set_pixel_color(c, x + col, y + row, c->bg_r, c->bg_g,
          c->bg_b);
     }
  }
}

void fb_draw_text(struct canvas *c, int x, int y, const char *s)
{
  int cx = x;

  while (*s) {
    fb_draw_char(c, cx, y, *s);
    cx += 8;
    s++;
  }
}

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

static bool app_ui_draw_done = true;

static uint32_t time_now_ms(void)
{
  return arm_timer_get_count() / (19200000.0f / 1000);
}

static void app_ui_draw_done_isr(struct ili9341_per_frame_dma *buf)
{
  app_ui_draw_done = true;
}

static NOOPT void app_ui_loop(void)
{
  char num_buf[32];
  uint64_t ts_next_wkup, ts;
  int num_buffers;
  int total_bytes;
  int num_bufs_on_vc;
  struct ili9341_drawframe *df = &drawframes[1];
  struct ili9341_per_frame_dma *dma_io = &df->bufs[0];

  struct canvas c = {
    .size_x = DISPLAY_WIDTH,
    .size_y = DISPLAY_HEIGHT - PREVIEW_HEIGHT,
    .data = dma_io->buf,
    .fg_r = 255,
    .fg_g = 0,
    .fg_b = 0,
    .bg_r = 0,
    .bg_g = 0,
    .bg_b = 0,
  };

  ili9341_drawframe_set_irq(df, app_ui_draw_done_isr);

  memset(dma_io->buf, 0, dma_io->buf_size);
  ts_next_wkup = time_now_ms() + 250;

  while (1) {
    ts = time_now_ms();
    if (ts_next_wkup > ts) {
      printf("waiting %dms\r\n", ts_next_wkup - ts);
      os_wait_ms(ts_next_wkup - ts);
    }

    ts_next_wkup = time_now_ms() + 250;

    fetch_clear_stats(&num_buffers, &total_bytes, &num_bufs_on_vc);
    if (!app_ui_draw_done) {
      continue;
    }

    app_ui_draw_done = false;

    num_buf[0] = '!';
    u32_to_str(num_buffers, num_buf + 1);
    fb_draw_text(&c, 0, 0, num_buf);
    u32_to_str(total_bytes, num_buf + 1);
    fb_draw_text(&c, 0, 20, num_buf);
    u32_to_str(num_bufs_on_vc, num_buf + 1);
    fb_draw_text(&c, 0, 30, num_buf);
    dcache_flush(dma_io->buf, dma_io->buf_size);
    ili9341_draw_dma_buf(dma_io);
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
