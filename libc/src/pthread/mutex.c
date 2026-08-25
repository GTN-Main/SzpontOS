/*
 * SzpontOS - POSIX Threads Mutex, RWLock, Spinlock & Barrier
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>

extern int64_t __syscall4(int64_t num, int64_t a1, int64_t a2, int64_t a3, int64_t a4);

static inline int futex_wait(volatile int *addr, int val, const struct timespec *to) {
    return (int)__syscall4(SYS_futex, (int64_t)addr, 0 /* FUTEX_WAIT */, val, (int64_t)to);
}

static inline int futex_wake(volatile int *addr, int count) {
    return (int)__syscall4(SYS_futex, (int64_t)addr, 1 /* FUTEX_WAKE */, count, 0);
}

/* =========================================================================
 * Mutexes
 * ========================================================================= */

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
    if (!mutex)
        return EINVAL;
    mutex->lock = 0;
    mutex->type = attr ? attr->type : PTHREAD_MUTEX_NORMAL;
    mutex->owner = 0;
    mutex->count = 0;
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex) {
    if (!mutex)
        return EINVAL;
    if (mutex->lock != 0)
        return EBUSY;
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex) {
    if (!mutex)
        return EINVAL;
    pthread_t me = pthread_self();

    if (mutex->type == PTHREAD_MUTEX_RECURSIVE) {
        if (mutex->owner == (int)me) {
            mutex->count++;
            return 0;
        }
    }

    /* Fast path: 0 -> 1 */
    int c = __sync_val_compare_and_swap(&mutex->lock, 0, 1);
    if (c != 0) {
        if (c != 2) {
            c = __sync_lock_test_and_set(&mutex->lock, 2);
        }
        while (c != 0) {
            futex_wait(&mutex->lock, 2, NULL);
            c = __sync_lock_test_and_set(&mutex->lock, 2);
        }
    }

    mutex->owner = (int)me;
    mutex->count = 1;
    return 0;
}

int pthread_mutex_trylock(pthread_mutex_t *mutex) {
    if (!mutex)
        return EINVAL;
    pthread_t me = pthread_self();

    if (mutex->type == PTHREAD_MUTEX_RECURSIVE && mutex->owner == (int)me) {
        mutex->count++;
        return 0;
    }

    if (__sync_bool_compare_and_swap(&mutex->lock, 0, 1)) {
        mutex->owner = (int)me;
        mutex->count = 1;
        return 0;
    }

    return EBUSY;
}

int pthread_mutex_timedlock(pthread_mutex_t *mutex, const struct timespec *abstime) {
    if (!mutex)
        return EINVAL;
    pthread_t me = pthread_self();

    if (mutex->type == PTHREAD_MUTEX_RECURSIVE && mutex->owner == (int)me) {
        mutex->count++;
        return 0;
    }

    int c = __sync_val_compare_and_swap(&mutex->lock, 0, 1);
    if (c != 0) {
        if (c != 2)
            c = __sync_lock_test_and_set(&mutex->lock, 2);
        while (c != 0) {
            int res = futex_wait(&mutex->lock, 2, abstime);
            if (res == -110 /* -ETIMEDOUT */)
                return ETIMEDOUT;
            c = __sync_lock_test_and_set(&mutex->lock, 2);
        }
    }

    mutex->owner = (int)me;
    mutex->count = 1;
    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    if (!mutex)
        return EINVAL;

    if (mutex->type == PTHREAD_MUTEX_RECURSIVE) {
        pthread_t me = pthread_self();
        if (mutex->owner != (int)me)
            return EPERM;
        mutex->count--;
        if (mutex->count > 0)
            return 0;
    }

    mutex->owner = 0;
    mutex->count = 0;

    /* Fast path: 1 -> 0 */
    if (__sync_fetch_and_sub(&mutex->lock, 1) != 1) {
        mutex->lock = 0;
        futex_wake(&mutex->lock, 1);
    }

    return 0;
}

int pthread_mutexattr_init(pthread_mutexattr_t *attr) {
    if (!attr)
        return EINVAL;
    attr->type = PTHREAD_MUTEX_NORMAL;
    attr->pshared = PTHREAD_PROCESS_PRIVATE;
    return 0;
}

int pthread_mutexattr_destroy(pthread_mutexattr_t *attr) {
    (void)attr;
    return 0;
}

int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type) {
    if (!attr || (type != PTHREAD_MUTEX_NORMAL && type != PTHREAD_MUTEX_RECURSIVE && type != PTHREAD_MUTEX_ERRORCHECK))
        return EINVAL;
    attr->type = type;
    return 0;
}

int pthread_mutexattr_gettype(const pthread_mutexattr_t *attr, int *type) {
    if (!attr || !type)
        return EINVAL;
    *type = attr->type;
    return 0;
}

/* =========================================================================
 * Read-Write Locks (rwlock)
 * ========================================================================= */

int pthread_rwlock_init(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr) {
    (void)attr;
    if (!rwlock)
        return EINVAL;
    rwlock->lock = 0;
    rwlock->writers = 0;
    rwlock->readers = 0;
    rwlock->writer_tid = 0;
    return 0;
}

int pthread_rwlock_destroy(pthread_rwlock_t *rwlock) {
    if (!rwlock)
        return EINVAL;
    if (rwlock->readers > 0 || rwlock->writers > 0)
        return EBUSY;
    return 0;
}

int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock) {
    if (!rwlock)
        return EINVAL;
    while (1) {
        while (rwlock->writers > 0) {
            futex_wait(&rwlock->lock, rwlock->lock, NULL);
        }
        __sync_fetch_and_add(&rwlock->readers, 1);
        if (rwlock->writers == 0)
            break;
        __sync_fetch_and_sub(&rwlock->readers, 1);
    }
    return 0;
}

int pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock) {
    if (!rwlock)
        return EINVAL;
    if (rwlock->writers > 0)
        return EBUSY;
    __sync_fetch_and_add(&rwlock->readers, 1);
    if (rwlock->writers > 0) {
        __sync_fetch_and_sub(&rwlock->readers, 1);
        return EBUSY;
    }
    return 0;
}

int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock) {
    if (!rwlock)
        return EINVAL;
    pthread_t me = pthread_self();
    while (__sync_lock_test_and_set(&rwlock->writers, 1)) {
        futex_wait(&rwlock->writers, 1, NULL);
    }
    rwlock->writer_tid = (int)me;
    while (rwlock->readers > 0) {
        futex_wait(&rwlock->readers, rwlock->readers, NULL);
    }
    return 0;
}

int pthread_rwlock_trywrlock(pthread_rwlock_t *rwlock) {
    if (!rwlock)
        return EINVAL;
    pthread_t me = pthread_self();
    if (__sync_bool_compare_and_swap(&rwlock->writers, 0, 1)) {
        if (rwlock->readers == 0) {
            rwlock->writer_tid = (int)me;
            return 0;
        }
        rwlock->writers = 0;
    }
    return EBUSY;
}

int pthread_rwlock_unlock(pthread_rwlock_t *rwlock) {
    if (!rwlock)
        return EINVAL;
    pthread_t me = pthread_self();
    if (rwlock->writer_tid == (int)me) {
        rwlock->writer_tid = 0;
        rwlock->writers = 0;
        __sync_synchronize();
        futex_wake(&rwlock->writers, 1);
        futex_wake(&rwlock->lock, 64);
    } else if (rwlock->readers > 0) {
        int r = __sync_sub_and_fetch(&rwlock->readers, 1);
        if (r == 0) {
            futex_wake(&rwlock->readers, 1);
            futex_wake(&rwlock->writers, 1);
        }
    }
    return 0;
}

int pthread_rwlockattr_init(pthread_rwlockattr_t *attr) {
    if (!attr)
        return EINVAL;
    attr->pshared = PTHREAD_PROCESS_PRIVATE;
    return 0;
}

int pthread_rwlockattr_destroy(pthread_rwlockattr_t *attr) {
    (void)attr;
    return 0;
}

/* =========================================================================
 * Spinlocks
 * ========================================================================= */

int pthread_spin_init(pthread_spinlock_t *lock, int pshared) {
    (void)pshared;
    if (!lock)
        return EINVAL;
    *lock = 0;
    return 0;
}

int pthread_spin_destroy(pthread_spinlock_t *lock) {
    if (!lock)
        return EINVAL;
    return 0;
}

int pthread_spin_lock(pthread_spinlock_t *lock) {
    if (!lock)
        return EINVAL;
    while (__sync_lock_test_and_set(lock, 1)) {
        while (*lock) {
            __builtin_ia32_pause();
        }
    }
    return 0;
}

int pthread_spin_trylock(pthread_spinlock_t *lock) {
    if (!lock)
        return EINVAL;
    if (__sync_lock_test_and_set(lock, 1))
        return EBUSY;
    return 0;
}

int pthread_spin_unlock(pthread_spinlock_t *lock) {
    if (!lock)
        return EINVAL;
    __sync_lock_release(lock);
    return 0;
}

/* =========================================================================
 * Barriers
 * ========================================================================= */

int pthread_barrier_init(pthread_barrier_t *barrier, const pthread_barrierattr_t *attr, unsigned int count) {
    (void)attr;
    if (!barrier || count == 0)
        return EINVAL;
    barrier->count = 0;
    barrier->total = (int)count;
    barrier->cycle = 0;
    return 0;
}

int pthread_barrier_destroy(pthread_barrier_t *barrier) {
    if (!barrier)
        return EINVAL;
    if (barrier->count > 0)
        return EBUSY;
    return 0;
}

int pthread_barrier_wait(pthread_barrier_t *barrier) {
    if (!barrier)
        return EINVAL;
    int cycle = barrier->cycle;

    if (__sync_add_and_fetch(&barrier->count, 1) == barrier->total) {
        barrier->count = 0;
        barrier->cycle++;
        futex_wake(&barrier->cycle, barrier->total);
        return PTHREAD_BARRIER_SERIAL_THREAD;
    }

    while (barrier->cycle == cycle) {
        futex_wait(&barrier->cycle, cycle, NULL);
    }
    return 0;
}

int pthread_barrierattr_init(pthread_barrierattr_t *attr) {
    if (!attr)
        return EINVAL;
    attr->pshared = PTHREAD_PROCESS_PRIVATE;
    return 0;
}

int pthread_barrierattr_destroy(pthread_barrierattr_t *attr) {
    (void)attr;
    return 0;
}
