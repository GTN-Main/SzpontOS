#ifndef SZPONTOS_KERNEL_SPINLOCK_H
#define SZPONTOS_KERNEL_SPINLOCK_H

#include <kernel/types.h>

typedef struct {
    volatile uint32_t lock;
} spinlock_t;

#define SPINLOCK_INIT ((spinlock_t){.lock = 0})

static inline void spinlock_init(spinlock_t *sl) {
    sl->lock = 0;
}

static inline void spinlock_acquire(spinlock_t *sl) {
    while (__atomic_exchange_n(&sl->lock, 1, __ATOMIC_ACQUIRE)) {
#if defined(__x86_64__)
        __builtin_ia32_pause();
#endif
    }
}

static inline bool spinlock_try_acquire(spinlock_t *sl) {
    return (__atomic_exchange_n(&sl->lock, 1, __ATOMIC_ACQUIRE) == 0);
}

static inline void spinlock_release(spinlock_t *sl) {
    __atomic_store_n(&sl->lock, 0, __ATOMIC_RELEASE);
}

#endif /* SZPONTOS_KERNEL_SPINLOCK_H */
