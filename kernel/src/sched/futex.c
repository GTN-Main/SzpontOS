/*
 * SzpontOS - Fast Userspace Mutex (Futex) Subsystem
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <sched/futex.h>
#include <sched/sched.h>
#include <sched/process.h>
#include <mm/vmm.h>
#include <mm/heap.h>
#include <kernel/spinlock.h>
#include <kernel/kprint.h>
#include <mm/usercopy.h>
#include <arch/x86_64/pit.h>

#define FUTEX_HASH_SIZE 64
#define FUTEX_HASH(addr) (((uintptr_t)(addr) >> 2) % FUTEX_HASH_SIZE)

typedef struct futex_bucket {
    spinlock_t lock;
    list_node_t waiters;
} futex_bucket_t;

static futex_bucket_t g_futex_buckets[FUTEX_HASH_SIZE];

void futex_init(void) {
    for (size_t i = 0; i < FUTEX_HASH_SIZE; i++) {
        spinlock_init(&g_futex_buckets[i].lock);
        list_init(&g_futex_buckets[i].waiters);
    }
    klog_info("Futex Subsystem initialized (64 hash buckets)");
}

int futex_wait(uintptr_t uaddr, int val, const struct timespec *timeout) {
    process_t *proc = sched_get_current_process();
    thread_t *curr = sched_get_current_thread();
    if (!proc || !curr || uaddr == 0)
        return -1;

    /* Verify that uaddr is mapped and accessible in user space */
    if (!vmm_virt_to_phys(proc->pagemap, uaddr)) {
        return -14; /* -EFAULT */
    }

    uint32_t bucket_idx = FUTEX_HASH(uaddr);
    futex_bucket_t *bucket = &g_futex_buckets[bucket_idx];

    spinlock_acquire(&bucket->lock);

    /* Atomic check: read current value at uaddr using usercopy */
    int current_val = 0;
    if (!copy_from_user(&current_val, uaddr, sizeof(int))) {
        spinlock_release(&bucket->lock);
        return -14; /* -EFAULT */
    }

    if (current_val != val) {
        spinlock_release(&bucket->lock);
        return -11; /* -EAGAIN */
    }

    /* Block current thread and add to futex wait queue */
    curr->state = THREAD_BLOCKED;
    curr->futex_uaddr = uaddr;
    curr->futex_proc = proc;
    list_add_tail(&bucket->waiters, &curr->futex_node);

    spinlock_release(&bucket->lock);

    if (timeout) {
        struct timespec ktimeout;
        if (!copy_from_user(&ktimeout, (uintptr_t)timeout, sizeof(struct timespec))) {
            spinlock_acquire(&bucket->lock);
            if (curr->futex_uaddr != 0) {
                list_remove(&curr->futex_node);
                curr->futex_uaddr = 0;
                curr->futex_proc = NULL;
                curr->state = THREAD_READY;
            }
            spinlock_release(&bucket->lock);
            return -14; /* -EFAULT */
        }

        uint64_t wait_ms = (uint64_t)ktimeout.tv_sec * 1000 + (uint64_t)(ktimeout.tv_nsec / 1000000);
        if (wait_ms == 0 && ktimeout.tv_nsec > 0)
            wait_ms = 1;

        uint64_t start_ticks = pit_get_ticks();
        uint32_t freq = pit_get_frequency();
        if (freq == 0) freq = 1000;
        uint64_t duration_ticks = (wait_ms * freq) / 1000;
        if (duration_ticks == 0 && wait_ms > 0) duration_ticks = 1;
        uint64_t expire_ticks = start_ticks + duration_ticks;

        while (curr->futex_uaddr != 0) {
            if (pit_get_ticks() >= expire_ticks) {
                /* Expiration check */
                spinlock_acquire(&bucket->lock);
                if (curr->futex_uaddr != 0) {
                    list_remove(&curr->futex_node);
                    curr->futex_uaddr = 0;
                    curr->futex_proc = NULL;
                    curr->state = THREAD_READY;
                    spinlock_release(&bucket->lock);
                    return -110; /* -ETIMEDOUT */
                }
                spinlock_release(&bucket->lock);
                break;
            }
            thread_sleep(1);
        }
    } else {
        /* Yield CPU until awakened by futex_wake */
        sched_yield();
    }

    return 0;
}

int futex_wake(uintptr_t uaddr, int count) {
    if (uaddr == 0 || count <= 0)
        return 0;
    process_t *proc = sched_get_current_process();
    if (!proc)
        return 0;

    uint32_t bucket_idx = FUTEX_HASH(uaddr);
    futex_bucket_t *bucket = &g_futex_buckets[bucket_idx];

    spinlock_acquire(&bucket->lock);

    int woken = 0;
    list_node_t *pos, *n;
    list_for_each_safe(pos, n, &bucket->waiters) {
        if (woken >= count)
            break;

        thread_t *t = container_of(pos, thread_t, futex_node);
        if (t->futex_uaddr == uaddr && (t->futex_proc == proc || t->futex_proc->pagemap == proc->pagemap)) {
            list_remove(pos);
            t->futex_uaddr = 0;
            t->futex_proc = NULL;
            t->state = THREAD_READY;
            sched_add_thread(t);
            woken++;
        }
    }

    spinlock_release(&bucket->lock);
    return woken;
}

/* Removes a thread from whatever futex bucket it's blocked in, if any.
 * Must be called before a blocked thread is force-transitioned to
 * THREAD_ZOMBIE (e.g. process_exit(), a fatal signal) — otherwise its
 * futex_node stays linked into g_futex_buckets[] and a later futex_wake()
 * on the same address can resurrect the (possibly already-freed) thread
 * into the ready queue. */
void futex_remove_thread(thread_t *t) {
    if (!t || !t->futex_proc)
        return;

    uint32_t bucket_idx = FUTEX_HASH(t->futex_uaddr);
    futex_bucket_t *bucket = &g_futex_buckets[bucket_idx];

    spinlock_acquire(&bucket->lock);
    if (t->futex_proc) {
        list_remove(&t->futex_node);
        t->futex_uaddr = 0;
        t->futex_proc = NULL;
    }
    spinlock_release(&bucket->lock);
}

int futex_requeue(uintptr_t uaddr1, int wake_count, uintptr_t uaddr2, int requeue_count) {
    if (uaddr1 == 0 || uaddr2 == 0)
        return -1;
    process_t *proc = sched_get_current_process();
    if (!proc)
        return -1;

    uint32_t b1_idx = FUTEX_HASH(uaddr1);
    uint32_t b2_idx = FUTEX_HASH(uaddr2);

    futex_bucket_t *b1 = &g_futex_buckets[b1_idx];
    futex_bucket_t *b2 = &g_futex_buckets[b2_idx];

    /* Always acquire locks in bucket index order to prevent deadlock */
    if (b1_idx < b2_idx) {
        spinlock_acquire(&b1->lock);
        spinlock_acquire(&b2->lock);
    } else if (b1_idx > b2_idx) {
        spinlock_acquire(&b2->lock);
        spinlock_acquire(&b1->lock);
    } else {
        spinlock_acquire(&b1->lock);
    }

    int woken = 0;
    int requeued = 0;

    list_node_t *pos, *n;
    list_for_each_safe(pos, n, &b1->waiters) {
        thread_t *t = container_of(pos, thread_t, futex_node);
        if (t->futex_uaddr == uaddr1 && (t->futex_proc == proc || t->futex_proc->pagemap == proc->pagemap)) {
            if (woken < wake_count) {
                list_remove(pos);
                t->futex_uaddr = 0;
                t->futex_proc = NULL;
                t->state = THREAD_READY;
                sched_add_thread(t);
                woken++;
            } else if (requeued < requeue_count) {
                list_remove(pos);
                t->futex_uaddr = uaddr2;
                list_add_tail(&b2->waiters, &t->futex_node);
                requeued++;
            } else {
                break;
            }
        }
    }

    if (b1_idx != b2_idx) {
        spinlock_release(&b2->lock);
    }
    spinlock_release(&b1->lock);

    return woken + requeued;
}
