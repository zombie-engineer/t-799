#include <mbox.h>
// #include <compiler.h>
#include <cpu.h>


#define VIDEOCORE_MBOX (BCM2835_MEM_PERIPH_BASE + 0xb880)
#define MBOX_READ   ((volatile unsigned int*)(VIDEOCORE_MBOX + 0x00))
#define MBOX_POLL   ((volatile unsigned int*)(VIDEOCORE_MBOX + 0x10))
#define MBOX_SENDER ((volatile unsigned int*)(VIDEOCORE_MBOX + 0x14))
#define MBOX_STATUS ((volatile unsigned int*)(VIDEOCORE_MBOX + 0x18))
#define MBOX_CONFIG ((volatile unsigned int*)(VIDEOCORE_MBOX + 0x1c))
#define MBOX_WRITE  ((volatile unsigned int*)(VIDEOCORE_MBOX + 0x20))
#define MBOX_RESPONSE 0x80000000
#define MBOX_FULL     0x80000000
#define MBOX_EMPTY    0x40000000

#define mbox_flush_dcache()\
  dcache_flush(mbox_buffer, sizeof(mbox_buffer));

int mbox_prop_call()
{
  int ret;

  mbox_flush_dcache();
  ret = mbox_call_blocking(MBOX_CH_PROP);
  mbox_flush_dcache();

  return ret;
}

void mbox_init(void)
{
}
