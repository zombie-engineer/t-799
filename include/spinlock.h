#pragma once
#include <compiler.h>
#include <cpu.h>

struct spinlock {
  uint64_t lock ALIGNED(64);
};

extern void __armv8_spinlock_init(void *);
extern void __armv8_spinlock_lock(void *);
extern void __armv8_spinlock_unlock(void *);

#define __spinlock_init __armv8_spinlock_init
#define __spinlock_lock __armv8_spinlock_lock
#define __spinlock_unlock __armv8_spinlock_unlock

/*
 * spinlock. Used when scheduling and MMU are both enabled
 *
 * spinlock_init               - initialize spinlock
 * spinlock_lock_irq           - lock spinlock during interrupt when irq is disabled
 * spinlock_unlock_irq         - unlock spinlock during interrupt when irq is disabled
 * spinlock_lock_disable_irq   - lock spinlock and ensure irq disabled,
 *                               also save irq flags to restore to them later
 * spinlock_unlock_restore_irq - unlock spinlock and restore irq flags, saved during
 *                               'spinlock_lock_disable_irq'
 */
#define spinlock_init(__spinlock)\
  __spinlock_init(&(__spinlock)->lock)

#define spinlock_lock_irq(__spinlock)\
  __spinlock_lock(&(__spinlock)->lock)

#define spinlock_unlock_irq(__spinlock)\
  __spinlock_unlock(&(__spinlock)->lock)

#define spinlock_lock_disable_irq(__spinlock, __irqflags)\
  do {\
    disable_irq_save_flags(__irqflags);\
    spinlock_lock_irq(__spinlock);\
  } while(0)

#define spinlock_unlock_restore_irq(__spinlock, __irqflags)\
  do {\
    spinlock_unlock_irq(__spinlock);\
    restore_irq_flags(__irqflags);\
  } while(0)
