/*
 * SzpontOS - Symmetric Multiprocessing (SMP) Subsystem
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef SZPONTOS_KERNEL_SMP_H
#define SZPONTOS_KERNEL_SMP_H

#include <kernel/types.h>
#include <arch/x86_64/gdt.h>

#define SMP_MAX_CPUS 64

struct thread;

/*
 * Per-CPU architectural and scheduler control block.
 *
 * ATTENTION: Offsets within this structure are directly addressed by assembly:
 *   Offset  0: self (pointer to this cpu_t, for %gs:0 fast resolution)
 *   Offset  8: cpu_id (uint32_t)
 *   Offset 12: lapic_id (uint32_t)
 *   Offset 16: kernel_stack (uintptr_t, loaded into RSP during syscall)
 *   Offset 24: user_rsp (uintptr_t, saved user RSP during syscall)
 *   Offset 32: current_thread (struct thread *)
 *   Offset 40: idle_thread (struct thread *)
 *   Offset 48: boot_rsp (uintptr_t)
 *   Offset 56: online (volatile uint8_t)
 *   Offset 57: is_bsp (uint8_t)
 */
typedef struct cpu {
    struct cpu *self;             /* 0x00 */
    uint32_t cpu_id;              /* 0x08 */
    uint32_t lapic_id;            /* 0x0C */
    uintptr_t kernel_stack;       /* 0x10 */
    uintptr_t user_rsp;           /* 0x18 */
    struct thread *current_thread;/* 0x20 */
    struct thread *idle_thread;   /* 0x28 */
    uintptr_t boot_rsp;           /* 0x30 */
    volatile uint8_t online;      /* 0x38 */
    uint8_t is_bsp;               /* 0x39 */
    uint8_t reserved[6];          /* 0x3A */

    tss_entry_t tss;              /* Per-CPU Task State Segment */
    gdt_table_t gdt;              /* Per-CPU Global Descriptor Table */
    gdt_descriptor_t gdt_desc;    /* Per-CPU GDTR value */
    uint64_t ticks;               /* Per-CPU timer ticks */
} __attribute__((aligned(64))) cpu_t;

void smp_init(void);
cpu_t *smp_get_cpu(uint32_t cpu_id);
uint32_t smp_get_cpu_count(void);
bool smp_is_bsp(void);
void smp_send_ipi(uint32_t lapic_id, uint8_t vector);
void smp_prepare_user_mode(void);

/* Fast per-CPU resolution in Ring 0 via GS segment base */
static inline cpu_t *smp_current_cpu(void) {
    cpu_t *cpu;
    __asm__ volatile("movq %%gs:0, %0" : "=r"(cpu));
    return cpu;
}

#endif /* SZPONTOS_KERNEL_SMP_H */
