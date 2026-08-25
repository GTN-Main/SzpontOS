/*
 * SzpontOS - POSIX Threads Condition Variables
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

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr) {
    (void)attr;
    if (!cond)
        return EINVAL;
    cond->seq = 0;
    cond->waiters = 0;
    return 0;
}

int pthread_cond_destroy(pthread_cond_t *cond) {
    if (!cond)
        return EINVAL;
    if (cond->waiters > 0)
        return EBUSY;
    return 0;
}

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
    if (!cond || !mutex)
        return EINVAL;

    int seq = cond->seq;
    __sync_fetch_and_add(&cond->waiters, 1);

    pthread_mutex_unlock(mutex);

    while (cond->seq == seq) {
        futex_wait(&cond->seq, seq, NULL);
    }

    __sync_fetch_and_sub(&cond->waiters, 1);
    pthread_mutex_lock(mutex);
    return 0;
}

int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime) {
    if (!cond || !mutex)
        return EINVAL;

    int seq = cond->seq;
    __sync_fetch_and_add(&cond->waiters, 1);

    pthread_mutex_unlock(mutex);

    int err = 0;
    while (cond->seq == seq) {
        int res = futex_wait(&cond->seq, seq, abstime);
        if (res == -110 /* -ETIMEDOUT */) {
            err = ETIMEDOUT;
            break;
        }
    }

    __sync_fetch_and_sub(&cond->waiters, 1);
    pthread_mutex_lock(mutex);
    return err;
}

int pthread_cond_signal(pthread_cond_t *cond) {
    if (!cond)
        return EINVAL;
    __sync_fetch_and_add(&cond->seq, 1);
    futex_wake(&cond->seq, 1);
    return 0;
}

int pthread_cond_broadcast(pthread_cond_t *cond) {
    if (!cond)
        return EINVAL;
    __sync_fetch_and_add(&cond->seq, 1);
    futex_wake(&cond->seq, 1024);
    return 0;
}

int pthread_condattr_init(pthread_condattr_t *attr) {
    if (!attr)
        return EINVAL;
    attr->pshared = PTHREAD_PROCESS_PRIVATE;
    return 0;
}

int pthread_condattr_destroy(pthread_condattr_t *attr) {
    (void)attr;
    return 0;
}
