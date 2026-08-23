#ifndef SZPONTOS_SCHED_SCHED_H
#define SZPONTOS_SCHED_SCHED_H

#include <sched/process.h>
#include <sched/thread.h>
#include <arch/x86_64/idt.h>

void sched_init(void);
void sched_start(void);
void sched_yield(void);
void sched_tick(void);

thread_t *sched_get_current_thread(void);
process_t *sched_get_current_process(void);

void sched_add_thread(thread_t *thread);
void sched_remove_thread(thread_t *thread);
void sched_block_current_thread(void);
void sched_unblock_thread(thread_t *thread);

#endif /* SZPONTOS_SCHED_SCHED_H */
