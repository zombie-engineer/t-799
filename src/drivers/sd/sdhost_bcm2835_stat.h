#pragma once
#include <stdint.h>

struct sdhost_bcm2835_write_stat {
  int num_blocks;
  uint64_t cmd_start;
  int num_block_irqs;
  uint64_t block_irq[128];
  uint64_t dma_end;
  uint64_t wait_start;
  uint64_t wait_end;
};

extern uint64_t sdhost_wait_start;
extern uint64_t sdhost_wait_cycles;


#if 0
struct bcm_sdhost_stat {
  uint32_t num_irqs;
  uint32_t num_waits;
  uint64_t wait_rdy_start_time;
  uint64_t wait_rdy_end_time;
  uint64_t total_wait_time;
};

static struct bcm_sdhost_stat bcm_sdhost_stat = { 0 };
static struct sdhost_bcm2835_write_stat sdhost_bcm2835_write_stats[128] = { 0 };
static int sdhost_bcm2835_write_stat_current_idx = 0;
static struct sdhost_bcm2835_write_stat *sdhost_bcm2835_write_stat_current = NULL;

static inline void bcm_sdhost_stat_irq(void)
{
  bcm_sdhost_stat.num_irqs++;
}

static inline void bcm_sdhost_stat_wait_rdy_start(void)
{
  bcm_sdhost_stat.wait_rdy_start_time = arm_timer_get_count();
}

static inline void bcm_sdhost_stat_wait_rdy_iter(void)
{
  bcm_sdhost_stat.num_waits++;
}

static inline void bcm_sdhost_stat_wait_rdy_end(void)
{
  bcm_sdhost_stat.wait_rdy_end_time = arm_timer_get_count();
  bcm_sdhost_stat.total_wait_time += bcm_sdhost_stat.wait_rdy_end_time 
    - bcm_sdhost_stat.wait_rdy_start_time;
}

static void sdhost_bcm2835_write_stat_new(int num_blocks)
{
  struct sdhost_bcm2835_write_stat *c;

  if (sdhost_bcm2835_write_stat_current_idx == 128)
    return;

  c = &sdhost_bcm2835_write_stats[sdhost_bcm2835_write_stat_current_idx++];
  c->num_blocks = num_blocks;
  c->cmd_start = arm_timer_get_count();

  sdhost_bcm2835_write_stat_current = c;
}

#if 0
static void sdhost_bcm2835_write_stat_block_irq(void)
{
  struct sdhost_bcm2835_write_stat *c = sdhost_bcm2835_write_stat_current;
  if (!c)
    return;

  if (c->num_block_irqs >= ARRAY_SIZE(c->block_irq))
    return;

  c->block_irq[c->num_block_irqs++] = arm_timer_get_count();
}
#endif

static void sdhost_bcm2835_write_stat_dma_end(void)
{
  struct sdhost_bcm2835_write_stat *c = sdhost_bcm2835_write_stat_current;
  if (!c)
    return;

  c->dma_end = arm_timer_get_count();
}

static void sdhost_bcm2835_write_stat_wait_start(void)
{
  struct sdhost_bcm2835_write_stat *c = sdhost_bcm2835_write_stat_current;
  if (!c)
    return;

  c->wait_start = arm_timer_get_count();
}

static void sdhost_bcm2835_write_stat_wait_end(void)
{
  struct sdhost_bcm2835_write_stat *c = sdhost_bcm2835_write_stat_current;
  if (!c)
    return;

  c->wait_end = arm_timer_get_count();
  sdhost_bcm2835_write_stat_current = NULL;
  if (sdhost_bcm2835_write_stat_current_idx == 128) {
    for (int i = 0; i < 128; ++i) {
      c = &sdhost_bcm2835_write_stats[i];
      printf("%d %ld %d %d ", i, c->cmd_start, c->num_blocks, c->num_block_irqs);
      for (int ii = 0; ii < c->num_block_irqs; ++ii) {
        printf(" %ld", c->block_irq[ii]);
      }
      printf(" %ld %ld %ld\r\n", c->dma_end, c->wait_start, c->wait_end);
    }
  }
}
#endif
