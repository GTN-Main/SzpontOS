#include <sched/sched.h>
#include <arch/x86_64/gdt.h>
#include <arch/x86_64/pit.h>
#include <arch/x86_64/io.h>
#include <mm/vmm.h>
#include <kernel/kprint.h>
#include <kernel/spinlock.h>

static list_node_t g_ready_queue = LIST_HEAD_INIT(g_ready_queue);
static list_node_t g_sleeping_queue = LIST_HEAD_INIT(g_sleeping_queue);
static thread_t *g_current_thread = NULL;
static thread_t *g_idle_thread = NULL;
static spinlock_t g_sched_lock = SPINLOCK_INIT;
static bool g_sched_started = false;

extern void arch_switch_context(uintptr_t *old_rsp, uintptr_t new_rsp);

static void idle_thread_func(void) {
    while (1) {
        __asm__ volatile ("sti; hlt; cli");
    }
}

void sched_init(void) {
    spinlock_init(&g_sched_lock);
    list_init(&g_ready_queue);
    list_init(&g_sleeping_queue);

    /* Create Idle Thread / Process */
    process_t *idle_proc = process_create("idle");
    g_idle_thread = thread_create(idle_proc, idle_thread_func, false);
    list_remove(&g_idle_thread->sched_node); /* Not in normal ready queue */

    klog_info("Preemptive Scheduler initialized (Round-Robin)");
}

void sched_add_thread(thread_t *thread) {
    if (!thread) return;
    spinlock_acquire(&g_sched_lock);
    thread->state = THREAD_READY;
    list_add_tail(&g_ready_queue, &thread->sched_node);
    spinlock_release(&g_sched_lock);
}

void sched_remove_thread(thread_t *thread) {
    if (!thread) return;
    spinlock_acquire(&g_sched_lock);
    list_remove(&thread->sched_node);
    spinlock_release(&g_sched_lock);
}

thread_t *sched_get_current_thread(void) {
    return g_current_thread;
}

process_t *sched_get_current_process(void) {
    return g_current_thread ? g_current_thread->process : NULL;
}

void sched_yield(void) {
    if (!g_sched_started) return;

    spinlock_acquire(&g_sched_lock);

    thread_t *prev = g_current_thread;
    if (prev && prev != g_idle_thread && prev->state == THREAD_RUNNING) {
        prev->state = THREAD_READY;
        list_add_tail(&g_ready_queue, &prev->sched_node);
    }

    /* Wake sleeping threads */
    uint64_t ticks = pit_get_ticks();
    list_node_t *pos, *n;
    list_for_each_safe(pos, n, &g_sleeping_queue) {
        thread_t *t = container_of(pos, thread_t, sched_node);
        if (ticks >= t->sleep_until_tick) {
            list_remove(&t->sched_node);
            t->state = THREAD_READY;
            list_add_tail(&g_ready_queue, &t->sched_node);
        }
    }

    /* Pick next thread from ready queue */
    thread_t *next = NULL;
    if (!list_is_empty(&g_ready_queue)) {
        list_node_t *head = g_ready_queue.next;
        list_remove(head);
        next = container_of(head, thread_t, sched_node);
    } else {
        next = g_idle_thread;
    }

    next->state = THREAD_RUNNING;
    g_current_thread = next;

    /* Update TSS kernel stack for next thread */
    gdt_set_kernel_stack(next->kernel_stack_top);

    /* Update TLS FS_BASE (MSR 0xC0000100) */
    wrmsr(0xC0000100, next->fs_base);

    /* Switch address space if necessary */
    if (next->process && next->process->pagemap) {
        vmm_switch_address_space(next->process->pagemap);
    }

    if (prev != next) {
        spinlock_release(&g_sched_lock);
        static uintptr_t boot_rsp = 0;
        uintptr_t *old_rsp_ptr = prev ? &prev->rsp : &boot_rsp;
        arch_switch_context(old_rsp_ptr, next->rsp);
    } else {
        spinlock_release(&g_sched_lock);
    }
}

void thread_sleep(uint32_t ms) {
    thread_t *curr = sched_get_current_thread();
    if (!curr) return;

    if (ms == 0) {
        sched_yield();
        return;
    }

    uint32_t freq = pit_get_frequency();
    uint64_t ticks_to_sleep = ((uint64_t)ms * freq + 999) / 1000;
    if (ticks_to_sleep == 0) ticks_to_sleep = 1;

    spinlock_acquire(&g_sched_lock);
    curr->state = THREAD_SLEEPING;
    curr->sleep_until_tick = pit_get_ticks() + ticks_to_sleep;
    list_add_tail(&g_sleeping_queue, &curr->sched_node);
    spinlock_release(&g_sched_lock);

    sched_yield();
}

void sched_tick(void) {
    extern void e1000_poll(void);
    e1000_poll();
    if (!g_sched_started) return;
    sched_yield();
}

void sched_start(void) {
    g_sched_started = true;
    sched_yield();
}
