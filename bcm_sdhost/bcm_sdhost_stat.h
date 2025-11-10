#pragma once

struct bcm_sdhost_stat {
  int num_blocks;
  uint64_t cmd_start;
  int num_block_irqs;
  uint64_t block_irq[128];
  uint64_t dma_end;
  uint64_t wait_start;
  uint64_t wait_end;
};

static struct bcm_sdhost_stat bcm_sdhost_stats[128] = { 0 };
static int bcm_sdhost_stat_current_idx = 0;
static struct bcm_sdhost_stat *bcm_sdhost_stat_current = NULL;
static int bcm_sdhost_num_irq = 0;

static void bcm_sdhost_stat_new(int num_blocks)
{
  if (bcm_sdhost_stat_current_idx == 128)
    return;

  struct bcm_sdhost_stat *c = &bcm_sdhost_stats[bcm_sdhost_stat_current_idx++];
  c->num_blocks = num_blocks;
  c->cmd_start = arm_timer_get_count();

  bcm_sdhost_stat_current = c;
}

static void bcm_sdhost_stat_block_irq(void)
{
  struct bcm_sdhost_stat *c = bcm_sdhost_stat_current;
  if (!c)
    return;

  if (c->num_block_irqs >= ARRAY_SIZE(c->block_irq))
    return;

  c->block_irq[c->num_block_irqs++] = arm_timer_get_count();
}

static void bcm_sdhost_stat_dma_end(void)
{
  struct bcm_sdhost_stat *c = bcm_sdhost_stat_current;
  if (!c)
    return;

  c->dma_end = arm_timer_get_count();
}

static void bcm_sdhost_stat_wait_start(void)
{
  struct bcm_sdhost_stat *c = bcm_sdhost_stat_current;
  if (!c)
    return;

  c->wait_start = arm_timer_get_count();
}

static void bcm_sdhost_stat_wait_end(void)
{
  struct bcm_sdhost_stat *c = bcm_sdhost_stat_current;
  if (!c)
    return;

  c->wait_end = arm_timer_get_count();
  bcm_sdhost_stat_current = NULL;
  if (bcm_sdhost_stat_current_idx == 128) {
    for (int i = 0; i < 128; ++i) {
      c = &bcm_sdhost_stats[i];
      printf("%d %ld %d %d ", i, c->cmd_start, c->num_blocks, c->num_block_irqs);
      for (int ii = 0; ii < c->num_block_irqs; ++ii) {
        printf(" %ld", c->block_irq[ii]);
      }
      printf(" %ld %ld %ld\r\n", c->dma_end, c->wait_start, c->wait_end);
    }
  }
}
