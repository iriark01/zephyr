/*
 * Copyright (c) 2018 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_INCLUDE_SPINLOCK_H_
#define ZEPHYR_INCLUDE_SPINLOCK_H_

#include <sys/atomic.h>
#include <kernel_structs.h>

/* There's a spinlock validation framework available when asserts are
 * enabled.  It adds a relatively hefty overhead (about 3k or so) to
 * kernel code size, don't use on platforms known to be small.
 */
#ifdef CONFIG_SPIN_VALIDATE
#include <sys/__assert.h>
#include <stdbool.h>
struct k_spinlock;
bool z_spin_lock_valid(struct k_spinlock *l);
bool z_spin_unlock_valid(struct k_spinlock *l);
void z_spin_lock_set_owner(struct k_spinlock *l);
BUILD_ASSERT(CONFIG_MP_NUM_CPUS < 4, "Too many CPUs for mask");
#endif /* CONFIG_SPIN_VALIDATE */

struct k_spinlock_key {
	int key;
};

typedef struct k_spinlock_key k_spinlock_key_t;

static ALWAYS_INLINE k_spinlock_key_t k_spin_lock(struct k_spinlock *l)
{
	ARG_UNUSED(l);
	k_spinlock_key_t k;

	/* Note that we need to use the underlying arch-specific lock
	 * implementation.  The "irq_lock()" API in SMP context is
	 * actually a wrapper for a global spinlock!
	 */
	k.key = arch_irq_lock();

#ifdef CONFIG_SPIN_VALIDATE
	__ASSERT(z_spin_lock_valid(l), "Recursive spinlock %p", l);
#endif

#ifdef CONFIG_SMP
	while (!atomic_cas(&l->locked, 0, 1)) {
	}
#endif

#ifdef CONFIG_SPIN_VALIDATE
	z_spin_lock_set_owner(l);
#endif
	return k;
}

static ALWAYS_INLINE void k_spin_unlock(struct k_spinlock *l,
					k_spinlock_key_t key)
{
	ARG_UNUSED(l);
#ifdef CONFIG_SPIN_VALIDATE
	__ASSERT(z_spin_unlock_valid(l), "Not my spinlock %p", l);
#endif

#ifdef CONFIG_SMP
	/* Strictly we don't need atomic_clear() here (which is an
	 * exchange operation that returns the old value).  We are always
	 * setting a zero and (because we hold the lock) know the existing
	 * state won't change due to a race.  But some architectures need
	 * a memory barrier when used like this, and we don't have a
	 * Zephyr framework for that.
	 */
	atomic_clear(&l->locked);
#endif
	arch_irq_unlock(key.key);
}

/* Internal function: releases the lock, but leaves local interrupts
 * disabled
 */
static ALWAYS_INLINE void k_spin_release(struct k_spinlock *l)
{
	ARG_UNUSED(l);
#ifdef CONFIG_SPIN_VALIDATE
	__ASSERT(z_spin_unlock_valid(l), "Not my spinlock %p", l);
#endif
#ifdef CONFIG_SMP
	atomic_clear(&l->locked);
#endif
}


#endif /* ZEPHYR_INCLUDE_SPINLOCK_H_ */
