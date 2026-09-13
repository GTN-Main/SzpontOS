/*
 * SzpontOS - Symmetric Multiprocessing (SMP) Implementation
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <kernel/smp.h>
#include <kernel/kprint.h>
#include <kernel/string.h>
#include <kernel/panic.h>
#include <arch/x86_64/gdt.h>
#include <arch/x86_64/idt.h>
#include <arch/x86_64/io.h>
#include <drivers/ioapic.h>
#include <drivers/rtc.h>
#include <mm/vmm.h>
#include <mm/heap.h>
#include <sched/sched.h>
#include <limine.h>

__attribute__((used, section(".requests"))) static volatile struct limine_smp_request g_smp_request = {
    .id = LIMINE_SMP_REQUEST,
    .revision = 0,
    .response = NULL,
    .flags = 0,
};

static cpu_t g_cpus[SMP_MAX_CPUS] = {
    [0] = {
        .self = &g_cpus[0],
        .cpu_id = 0,
        .is_bsp = 1,
        .online = 1,
    }
};
static uint32_t g_cpu_count = 1;
static volatile bool g_smp_initialized = false;

extern void syscall_arch_init(void);

cpu_t *smp_get_cpu(uint32_t cpu_id) {
    if (cpu_id < g_cpu_count) {
        return &g_cpus[cpu_id];
    }
    return &g_cpus[0];
}

uint32_t smp_get_cpu_count(void) {
    return g_cpu_count;
}

bool smp_is_bsp(void) {
    cpu_t *cpu = smp_current_cpu();
    return cpu ? (bool)cpu->is_bsp : true;
}

void smp_send_ipi(uint32_t lapic_id, uint8_t vector) {
    lapic_send_ipi(lapic_id, vector);
}

void smp_prepare_user_mode(void) {
    cpu_t *cpu = smp_current_cpu();
    if (!cpu) {
        cpu = smp_get_cpu(0);
    }
    if (cpu->current_thread && cpu->current_thread->kernel_stack_top) {
        cpu->kernel_stack = cpu->current_thread->kernel_stack_top;
        cpu->tss.rsp0 = cpu->current_thread->kernel_stack_top;
    }
    wrmsr(0xC0000102, (uint64_t)cpu);
}

/* AP (Application Processor) core entry point jumped to by Limine */
static void smp_ap_entry(struct limine_smp_info *info) {
    cpu_t *cpu = (cpu_t *)info->extra_argument;

    /* Switch to kernel address space */
    vmm_switch_address_space(&g_kernel_pagemap);

    /* Load per-CPU GDT & TSS (LTR 0x28) */
    gdt_init_cpu(cpu->cpu_id, &cpu->gdt, &cpu->gdt_desc, &cpu->tss);

    /* Set GS_BASE and KERNEL_GS_BASE to point to this core's cpu_t */
    wrmsr(0xC0000101, (uint64_t)cpu);
    wrmsr(0xC0000102, (uint64_t)cpu);

    /* Load IDT on this core */
    idt_load_cpu();

    /* Enable FPU & SSE/AVX */
    fpu_init_cpu();

    /* Initialize IA32_PAT (MSR 0x277) to enable Write-Combining on PA1 */
    wrmsr(0x277, 0x0007010600070106ULL);

    /* Configure SYSCALL / SYSRET MSRs */
    syscall_arch_init();

    /* Initialize Local APIC on this core */
    lapic_init_cpu();

    /* Initialize LAPIC Timer */
    lapic_timer_init_ap();

    /* Mark this AP core as online */
    __atomic_store_n(&cpu->online, 1, __ATOMIC_RELEASE);

    /* Wait for scheduler to be started by BSP */
    while (!sched_is_started()) {
        __builtin_ia32_pause();
    }

    /* Enable interrupts and enter the scheduler */
    sti();
    sched_ap_start();

    while (1) {
        hlt();
    }
}

void smp_init(void) {
    /* Setup BSP (CPU 0) first */
    cpu_t *bsp = &g_cpus[0];
    bsp->self = bsp;
    bsp->cpu_id = 0;
    bsp->is_bsp = 1;
    bsp->online = 1;

    /* Reinitialize GDT and TSS to per-CPU storage */
    gdt_init_cpu(0, &bsp->gdt, &bsp->gdt_desc, &bsp->tss);

    /* Set GS_BASE and KERNEL_GS_BASE for BSP */
    wrmsr(0xC0000101, (uint64_t)bsp);
    wrmsr(0xC0000102, (uint64_t)bsp);

    struct limine_smp_response *resp = g_smp_request.response;
    if (!resp) {
        klog_warn("SMP: Limine MP response not received, running in uniprocessor mode");
        g_cpu_count = 1;
        g_smp_initialized = true;
        return;
    }

    bsp->lapic_id = resp->bsp_lapic_id;
    uint32_t total_cpus = (uint32_t)resp->cpu_count;
    if (total_cpus > SMP_MAX_CPUS) {
        total_cpus = SMP_MAX_CPUS;
    }
    g_cpu_count = total_cpus;

    klog_info("SMP: Limine detected %u CPU(s) (BSP LAPIC ID: %u)", total_cpus, resp->bsp_lapic_id);

    if (total_cpus <= 1) {
        g_smp_initialized = true;
        return;
    }

    uint32_t next_cpu_id = 1;
    for (uint64_t i = 0; i < resp->cpu_count; i++) {
        struct limine_smp_info *smp_info = resp->cpus[i];
        if (smp_info->lapic_id == resp->bsp_lapic_id) {
            continue; /* Skip BSP */
        }
        if (next_cpu_id >= SMP_MAX_CPUS) {
            break;
        }

        uint32_t cpu_id = next_cpu_id++;
        cpu_t *cpu = &g_cpus[cpu_id];
        cpu->self = cpu;
        cpu->cpu_id = cpu_id;
        cpu->lapic_id = smp_info->lapic_id;
        cpu->is_bsp = 0;
        cpu->online = 0;

        /* Allocate 16 KiB kernel boot stack for AP */
        void *stack = kmalloc(16384);
        if (!stack) {
            panic("SMP: Failed to allocate kernel stack for AP #%u", cpu_id);
        }
        uintptr_t stack_top = (uintptr_t)stack + 16384;
        cpu->boot_rsp = stack_top;
        cpu->kernel_stack = stack_top;

        smp_info->extra_argument = (uint64_t)cpu;
        __atomic_store_n(&smp_info->goto_address, smp_ap_entry, __ATOMIC_SEQ_CST);

        /* Wait for AP to become online with timeout */
        uint64_t start_tsc = rdtsc();
        uint64_t timeout_cycles = (g_tsc_freq_hz ? g_tsc_freq_hz : 2400000000ULL); /* 1 sec */
        while (__atomic_load_n(&cpu->online, __ATOMIC_ACQUIRE) == 0) {
            if ((rdtsc() - start_tsc) > timeout_cycles) {
                klog_error("SMP: Timeout waiting for CPU #%u (LAPIC ID %u) to boot!", cpu_id, cpu->lapic_id);
                break;
            }
            __builtin_ia32_pause();
        }

        if (__atomic_load_n(&cpu->online, __ATOMIC_ACQUIRE)) {
            klog_info("SMP: CPU #%u (LAPIC ID %u) is online", cpu_id, cpu->lapic_id);
        }
    }

    klog_info("SMP: Multi-core subsystem online (%u active CPU cores)", g_cpu_count);
    g_smp_initialized = true;
}
