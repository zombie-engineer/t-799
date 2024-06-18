#include <block_device.h>
#include <printf.h>
#include <errcode.h>
#include <cpu.h>
#include <os_api.h>
#include <log.h>
#include <common.h>

struct blockdev_scheduler {
  struct blockdev_io io_entries[64];
  uint64_t nr_requests;
  uint64_t nr_done;
  struct event req_event;
  struct event done_event;
};

static struct blockdev_scheduler blockdev_sched = { 0 };

void blockdev_scheduler_push_io(struct blockdev_io *io)
{
  irq_disable();

  if (blockdev_sched.nr_requests - blockdev_sched.nr_done >= 
    ARRAY_SIZE(blockdev_sched.io_entries))
  {
    os_event_clear(&blockdev_sched.done_event);
    irq_enable();
    os_event_wait(&blockdev_sched.done_event);
    irq_disable();
  }

  blockdev_sched.io_entries[
    blockdev_sched.nr_requests % ARRAY_SIZE(blockdev_sched.io_entries)] = 
      *io;

  blockdev_sched.nr_requests++;
  irq_enable();
  os_event_notify(&blockdev_sched.req_event);
}

static inline void blockdev_scheduler_run_io(struct blockdev_io *io)
{
  int err;

  if (io->is_write) {
    err = io->dev->ops.block_erase(io->dev, io->start_sector, io->num_sectors);
    os_wait_ms(300);
    if (err != SUCCESS)
      os_log("Block device error: block erase failed %d\r\n", err);
    else
      err = io->dev->ops.write(io->dev, io->addr, io->start_sector,
        io->num_sectors);
  }
  else
    err = io->dev->ops.read(io->dev, io->addr, io->start_sector,
      io->num_sectors);

  if (err != SUCCESS)
    printf("Block device io failed: %d", err);

  io->cb(err);
}

void blockdev_scheduler_fn(void)
{
  struct blockdev_io *io;
  int next_req = 0;

  irq_disable();
  os_event_clear(&blockdev_sched.req_event);
  irq_enable();
  os_event_wait(&blockdev_sched.req_event);

  while(1) {
    irq_disable();
    if (blockdev_sched.nr_requests == blockdev_sched.nr_done) {
      // next_req = blockdev_sched.nr_requests + 1;
      os_event_clear(&blockdev_sched.req_event);
      irq_enable();
      os_event_wait(&blockdev_sched.req_event);
      irq_disable();
    }

    io = &blockdev_sched.io_entries[
      next_req % ARRAY_SIZE(blockdev_sched.io_entries)];

    irq_enable();
    blockdev_scheduler_run_io(io);

    irq_disable();
    blockdev_sched.nr_done++;
    irq_enable();
    // printf("IO %d done\r\n", next_req);
    os_event_notify(&blockdev_sched.done_event);
    next_req++;
  }
}

void blockdev_scheduler_init(void)
{
  blockdev_sched.nr_requests = 0;
  blockdev_sched.nr_done = 0;

  os_event_init(&blockdev_sched.req_event);
  os_event_init(&blockdev_sched.done_event);
}
