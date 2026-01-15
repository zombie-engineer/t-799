#pragma once
#include <irq.h>
#include <ioreg.h>
#include <memory_map.h>
#include <drivers/intc/intc_bcm2835.h>

/*
 * ARM doorbells are used by VCHIQ as a way of  interprocessor communicaiton
 * between VideoCore GPU and ARM CPU. Communication protocol and specific bits
 * of doorbell registers are lefy undocumented and are just taken AS-IS from
 * Linux Kernel source code.
 * How this works?
 * 
 * ARM_DOORBELL_0 is a clear-on-read register,
 * By experiment if you read it twice after VCHIQ interrupt, first time it will
 * return 5, next time it will return 0.
 * Bit 3 (1<<3 == 4) means the doorbell was activated on VideoCore's side.
 *
 * This thread discusses the documentation for this register:
 * https://forums.raspberrypi.com/viewtopic.php?t=280271
 * The thread ends with the understanding that this register is very specific
 * to VCHIQ and is not documented anywhere else.
 *
 * Architecture of event handling is this:
 * There are situations when VideoCore GPU's firmware will notify ARM CPU of
 * some new event in VCHIQ. Possible situations:
 * 1. VideoCore has consumed a message from ARM CPU and notifies it that the
 *    message can be "recycled" to free up space in the message queue.
 * 2. VideoCore has written new message for ARM CPU in the message queue, and
 *    wants to notify ARM CPU about it.
 * Mesage passing is done via special events in the zero slot.
 * For recycling 'recycle' event is used.
 * For messages 'trigger' event is used.
 * Its not clear how sync_event is used, might be for other kind of messages.
 * To signal one of these events VideoCore sets event->fired = 1, and then
 * writes some value to ARM_DOORBELL_0 (On PI 3B+ address is 0x3f00b840).
 * Writing to this address triggers VCHIQ interrupt in one of the CPU cores,
 * which one depends on where peripheral interrupts are routed. IRQ will
 * only get triggered if ARM_DOORBELL_0 iterrupt is enabled in bcm interrupt
 * controller, the one on 0x3f00b200. To enable it set bit 2 in base interrupt
 * enable register 0x3f00b218.
 * ARM_DOORBELL_0 has irq number 66 in irq_table. The meaning of 66 is this:
 * there are 64 GPU interrupts that are in 2 32bit registers:
 * pending(0x204,0x208), enable(0x210,0x214), etc.
 * so they have numbers 0-63.
 * next number 64 is basic pending register bit 0, 65, is basic bit 1, and
 * the doorbell 0 is basic bit 2, thus 64+2 = 66.
 */

#define ARM_DOORBELL_BASE (BCM2835_MEM_PERIPH_BASE + 0xb840)

#define ARM_DOORBELL_0 (ioreg32_t)(ARM_DOORBELL_BASE + 0x00)
#define ARM_DOORBELL_1 (ioreg32_t)(ARM_DOORBELL_BASE + 0x04)
#define ARM_DOORBELL_2 (ioreg32_t)(ARM_DOORBELL_BASE + 0x08)
#define ARM_DOORBELL_3 (ioreg32_t)(ARM_DOORBELL_BASE + 0x0c)

static inline void vchiq_doorbell_0_irq_enable(irq_func func)
{
  irq_set(BCM2835_IRQNR_ARM_DOORBELL_0, func);
  bcm2835_ic_enable_irq(BCM2835_IRQNR_ARM_DOORBELL_0);
}

static inline bool vchiq_doorbell_is_triggered(void)
{
  return ioreg32_read(ARM_DOORBELL_0) & 4;
}

static void vchiq_doorbell_trigger(void)
{
  ioreg32_write(ARM_DOORBELL_2, 0);
}
