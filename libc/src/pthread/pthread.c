/*
 * SzpontOS - POSIX Threads (pthreads) Core Implementation
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sched.h>
#include <sys/syscall.h>

#define ARCH_SET_FS 0x1002
#define DEFAULT_STACK_SIZE (64 * 1024)
#define MAX_PTHREAD_KEYS 64

extern int __clone(int (*fn)(void *), void *child_stack, int flags, void *arg, int *ptid, void *tls, int *ctid);
extern int64_t __syscall1(int64_t num, int64_t a1);
extern int64_t __syscall4(int64_t num, int64_t a1, int64_t a2, int64_t a3, int64_t a4);

static inline int futex_wait(volatile int *addr, int val, const struct timespec *to) {
    return (int)__syscall4(SYS_futex, (int64_t)addr, 0 /* FUTEX_WAIT */, val, (int64_t)to);
}

static inline int futex_wake(volatile int *addr, int count) {
    return (int)__syscall4(SYS_futex, (int64_t)addr, 1 /* FUTEX_WAKE */, count, 0);
}

typedef struct pthread_key_entry {
    int in_use;
    void (*destructor)(void *);
} pthread_key_entry_t;

static pthread_key_entry_t g_keys[MAX_PTHREAD_KEYS];
static pthread_spinlock_t g_key_lock = 0;

typedef struct __pthread_internal {
    struct __pthread_internal *self; /* Offset 0: %fs:0 points to self! */
    pthread_t tid;
    void *(*start_routine)(void *);
    void *arg;
    void *retval;
    int detached;
    volatile int clear_child_tid;
    void *stack_base;
    size_t stack_size;
    void *tsd[MAX_PTHREAD_KEYS];
    struct __pthread_internal *next;
} pthread_internal_t;

static pthread_internal_t *g_thread_list = NULL;
static pthread_spinlock_t g_list_lock = 0;
static pthread_internal_t g_main_thread;
static int g_pthreads_initialized = 0;

static void pthread_init_main_thread(void) {
    if (g_pthreads_initialized)
        return;
    memset(&g_main_thread, 0, sizeof(pthread_internal_t));
    g_main_thread.tid = (pthread_t)getpid();
    g_main_thread.self = &g_main_thread;
    g_main_thread.clear_child_tid = (int)g_main_thread.tid;
    g_thread_list = &g_main_thread;

    /* Set FS_BASE for main thread */
    arch_prctl(ARCH_SET_FS, (unsigned long)&g_main_thread);
    g_pthreads_initialized = 1;
}

static inline pthread_internal_t *get_current_thread(void) {
    if (!g_pthreads_initialized) {
        pthread_init_main_thread();
    }
    pthread_internal_t *self = NULL;
    __asm__ volatile("movq %%fs:0, %0" : "=r"(self));
    if (!self || (uintptr_t)self < 4096) {
        return &g_main_thread;
    }
    return self;
}

static int __pthread_trampoline(void *arg) {
    pthread_internal_t *t = (pthread_internal_t *)arg;

    /* Set TLS base pointer (%fs:0) */
    arch_prctl(ARCH_SET_FS, (unsigned long)t);

    void *ret = t->start_routine(t->arg);
    pthread_exit(ret);
    return 0;
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg) {
    if (!thread || !start_routine)
        return EINVAL;
    pthread_init_main_thread();

    size_t stack_size = (attr && attr->stacksize) ? attr->stacksize : DEFAULT_STACK_SIZE;
    void *stack_base = malloc(stack_size);
    if (!stack_base)
        return ENOMEM;

    pthread_internal_t *t = (pthread_internal_t *)malloc(sizeof(pthread_internal_t));
    if (!t) {
        free(stack_base);
        return ENOMEM;
    }
    memset(t, 0, sizeof(pthread_internal_t));
    t->start_routine = start_routine;
    t->arg = arg;
    t->detached = (attr && attr->detachstate == PTHREAD_CREATE_DETACHED) ? 1 : 0;
    t->stack_base = stack_base;
    t->stack_size = stack_size;
    t->self = t;

    /* Align stack pointer to 16 bytes */
    uintptr_t stack_top = (uintptr_t)stack_base + stack_size - 16;
    stack_top &= ~15ULL;

    int flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD | CLONE_SETTLS | CLONE_PARENT_SETTID |
                CLONE_CHILD_CLEARTID;

    int child_tid = 0;
    int res = __clone(__pthread_trampoline, (void *)stack_top, flags, t, &child_tid, t, (int *)&t->clear_child_tid);
    if (res < 0) {
        free(t);
        free(stack_base);
        return EAGAIN;
    }

    t->tid = (pthread_t)child_tid;
    t->clear_child_tid = child_tid;
    *thread = t->tid;

    /* Register thread in global list */
    while (__sync_lock_test_and_set(&g_list_lock, 1)) {
        __builtin_ia32_pause();
    }
    t->next = g_thread_list;
    g_thread_list = t;
    __sync_lock_release(&g_list_lock);

    return 0;
}

void pthread_exit(void *retval) {
    pthread_internal_t *self = (pthread_internal_t *)pthread_self();
    if (!self || (uintptr_t)self < 4096) {
        self = &g_main_thread;
    }

    self->retval = retval;

    /* Run thread key destructors */
    for (int i = 0; i < MAX_PTHREAD_KEYS; i++) {
        if (g_keys[i].in_use && g_keys[i].destructor && self->tsd[i]) {
            void *val = self->tsd[i];
            self->tsd[i] = NULL;
            g_keys[i].destructor(val);
        }
    }

    if (self->detached) {
        if (self->stack_base)
            free(self->stack_base);
        free(self);
    }

    /* Terminate calling thread (SYS_exit clears clear_child_tid and futex-wakes joiners) */
    __syscall1(SYS_exit, 0);
    while (1) {
        sched_yield();
    }
}

int pthread_join(pthread_t thread, void **retval) {
    pthread_init_main_thread();

    while (__sync_lock_test_and_set(&g_list_lock, 1)) {
        __builtin_ia32_pause();
    }
    pthread_internal_t *curr = g_thread_list;
    while (curr && curr->tid != thread) {
        curr = curr->next;
    }
    __sync_lock_release(&g_list_lock);

    if (!curr)
        return ESRCH;
    if (curr->detached)
        return EINVAL;

    /* Wait on clear_child_tid futex until the child thread exits */
    while (curr->clear_child_tid != 0) {
        futex_wait(&curr->clear_child_tid, curr->clear_child_tid, NULL);
    }

    if (retval) {
        *retval = curr->retval;
    }

    /* Unlink from thread list */
    while (__sync_lock_test_and_set(&g_list_lock, 1)) {
        __builtin_ia32_pause();
    }
    if (g_thread_list == curr) {
        g_thread_list = curr->next;
    } else {
        pthread_internal_t *prev = g_thread_list;
        while (prev && prev->next != curr)
            prev = prev->next;
        if (prev)
            prev->next = curr->next;
    }
    __sync_lock_release(&g_list_lock);

    if (curr->stack_base)
        free(curr->stack_base);
    free(curr);

    return 0;
}

int pthread_detach(pthread_t thread) {
    pthread_init_main_thread();

    while (__sync_lock_test_and_set(&g_list_lock, 1)) {
        __builtin_ia32_pause();
    }
    pthread_internal_t *curr = g_thread_list;
    while (curr && curr->tid != thread) {
        curr = curr->next;
    }
    if (curr) {
        curr->detached = 1;
    }
    __sync_lock_release(&g_list_lock);

    return curr ? 0 : ESRCH;
}

pthread_t pthread_self(void) {
    return get_current_thread()->tid;
}

int pthread_equal(pthread_t t1, pthread_t t2) {
    return t1 == t2;
}

int pthread_yield(void) {
    return sched_yield();
}

int pthread_once(pthread_once_t *once_control, void (*init_routine)(void)) {
    if (!once_control || !init_routine)
        return EINVAL;

    if (*once_control == 2)
        return 0; /* Already initialized */

    if (__sync_bool_compare_and_swap(once_control, 0, 1)) {
        init_routine();
        __sync_synchronize();
        *once_control = 2;
    } else {
        while (*once_control != 2) {
            sched_yield();
        }
    }
    return 0;
}

/* =========================================================================
 * Thread Attributes
 * ========================================================================= */

int pthread_attr_init(pthread_attr_t *attr) {
    if (!attr)
        return EINVAL;
    attr->detachstate = PTHREAD_CREATE_JOINABLE;
    attr->stacksize = DEFAULT_STACK_SIZE;
    attr->stackaddr = NULL;
    return 0;
}

int pthread_attr_destroy(pthread_attr_t *attr) {
    if (!attr)
        return EINVAL;
    memset(attr, 0, sizeof(pthread_attr_t));
    return 0;
}

int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize) {
    if (!attr || stacksize < 4096)
        return EINVAL;
    attr->stacksize = stacksize;
    return 0;
}

int pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize) {
    if (!attr || !stacksize)
        return EINVAL;
    *stacksize = attr->stacksize;
    return 0;
}

int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate) {
    if (!attr || (detachstate != PTHREAD_CREATE_JOINABLE && detachstate != PTHREAD_CREATE_DETACHED))
        return EINVAL;
    attr->detachstate = detachstate;
    return 0;
}

int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate) {
    if (!attr || !detachstate)
        return EINVAL;
    *detachstate = attr->detachstate;
    return 0;
}

/* =========================================================================
 * Thread-Specific Data (TSD / Keys)
 * ========================================================================= */

int pthread_key_create(pthread_key_t *key, void (*destructor)(void *)) {
    if (!key)
        return EINVAL;

    while (__sync_lock_test_and_set(&g_key_lock, 1)) {
        __builtin_ia32_pause();
    }

    int found = -1;
    for (int i = 0; i < MAX_PTHREAD_KEYS; i++) {
        if (!g_keys[i].in_use) {
            g_keys[i].in_use = 1;
            g_keys[i].destructor = destructor;
            found = i;
            break;
        }
    }

    __sync_lock_release(&g_key_lock);

    if (found < 0)
        return EAGAIN;
    *key = (pthread_key_t)found;
    return 0;
}

int pthread_key_delete(pthread_key_t key) {
    if (key >= MAX_PTHREAD_KEYS)
        return EINVAL;

    while (__sync_lock_test_and_set(&g_key_lock, 1)) {
        __builtin_ia32_pause();
    }
    g_keys[key].in_use = 0;
    g_keys[key].destructor = NULL;
    __sync_lock_release(&g_key_lock);
    return 0;
}

void *pthread_getspecific(pthread_key_t key) {
    if (key >= MAX_PTHREAD_KEYS)
        return NULL;
    pthread_internal_t *self = get_current_thread();
    return self->tsd[key];
}

int pthread_setspecific(pthread_key_t key, const void *value) {
    if (key >= MAX_PTHREAD_KEYS)
        return EINVAL;
    pthread_internal_t *self = get_current_thread();
    self->tsd[key] = (void *)value;
    return 0;
}

int pthread_sigmask(int how, const sigset_t *set, sigset_t *oldset) {
    return sigprocmask(how, set, oldset);
}

int pthread_setcancelstate(int state, int *oldstate) {
    if (oldstate)
        *oldstate = PTHREAD_CANCEL_ENABLE;
    (void)state;
    return 0;
}

int pthread_setcanceltype(int type, int *oldtype) {
    if (oldtype)
        *oldtype = PTHREAD_CANCEL_DEFERRED;
    (void)type;
    return 0;
}

int pthread_cancel(pthread_t thread) {
    (void)thread;
    return 0;
}

void pthread_testcancel(void) {
}

typedef struct {
    unsigned long ti_module;
    unsigned long ti_offset;
} tls_index;

void *__tls_get_addr(tls_index *ti) {
    if (!g_pthreads_initialized) {
        pthread_init_main_thread();
    }
    pthread_internal_t *t = get_current_thread();
    return (void *)((uintptr_t)t + (ti ? ti->ti_offset : 0));
}
