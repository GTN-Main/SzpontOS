/*
 * SzpontOS - POSIX Semaphores Implementation
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <semaphore.h>
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

int sem_init(sem_t *sem, int pshared, unsigned int value) {
    (void)pshared;
    if (!sem)
        return EINVAL;
    sem->val = (int)value;
    sem->waiters = 0;
    return 0;
}

int sem_destroy(sem_t *sem) {
    if (!sem)
        return EINVAL;
    if (sem->waiters > 0)
        return EBUSY;
    return 0;
}

int sem_wait(sem_t *sem) {
    if (!sem)
        return EINVAL;

    while (1) {
        int val = sem->val;
        if (val > 0) {
            if (__sync_bool_compare_and_swap(&sem->val, val, val - 1)) {
                return 0;
            }
        } else {
            __sync_fetch_and_add(&sem->waiters, 1);
            futex_wait(&sem->val, 0, NULL);
            __sync_fetch_and_sub(&sem->waiters, 1);
        }
    }
}

int sem_trywait(sem_t *sem) {
    if (!sem)
        return EINVAL;

    while (1) {
        int val = sem->val;
        if (val <= 0)
            return EAGAIN;
        if (__sync_bool_compare_and_swap(&sem->val, val, val - 1)) {
            return 0;
        }
    }
}

int sem_timedwait(sem_t *sem, const struct timespec *abs_timeout) {
    if (!sem)
        return EINVAL;

    while (1) {
        int val = sem->val;
        if (val > 0) {
            if (__sync_bool_compare_and_swap(&sem->val, val, val - 1)) {
                return 0;
            }
        } else {
            __sync_fetch_and_add(&sem->waiters, 1);
            int res = futex_wait(&sem->val, 0, abs_timeout);
            __sync_fetch_and_sub(&sem->waiters, 1);
            if (res == -110 /* -ETIMEDOUT */)
                return ETIMEDOUT;
        }
    }
}

int sem_post(sem_t *sem) {
    if (!sem)
        return EINVAL;

    __sync_fetch_and_add(&sem->val, 1);
    if (sem->waiters > 0) {
        futex_wake(&sem->val, 1);
    }
    return 0;
}

int sem_getvalue(sem_t *sem, int *sval) {
    if (!sem || !sval)
        return EINVAL;
    *sval = sem->val;
    return 0;
}
