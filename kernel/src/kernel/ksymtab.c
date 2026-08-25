/*
 * SzpontOS - Kernel Symbol Table (ksymtab) & Export Subsystem
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <kernel/module.h>
#include <kernel/kprint.h>
#include <kernel/string.h>
#include <kernel/spinlock.h>
#include <mm/heap.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <fs/vfs.h>
#include <fs/devfs.h>
#include <fs/procfs.h>
#include <drivers/block.h>
#include <arch/x86_64/idt.h>
#include <arch/x86_64/pic.h>
#include <arch/x86_64/pit.h>
#include <arch/x86_64/io.h>
#include <sched/sched.h>
#include <sched/process.h>
#include <sched/futex.h>

#define MAX_DYNAMIC_SYMS 256

/* Dynamic symbols exported by loaded modules */
static kernel_symbol_t g_dynamic_syms[MAX_DYNAMIC_SYMS];
static size_t g_dynamic_syms_count = 0;
static spinlock_t g_ksym_lock = SPINLOCK_INIT;

/* Static table of built-in kernel symbols */
static const kernel_symbol_t g_builtin_ksymtab[] = {
    /* Logging & Output */
    {"kprintf", (uintptr_t)&kprintf},
    {"kvprintf", (uintptr_t)&kvprintf},
    {"ksnprintf", (uintptr_t)&ksnprintf},
    {"kvsnprintf", (uintptr_t)&kvsnprintf},
    {"klog", (uintptr_t)&klog},

    /* String & Memory Functions */
    {"strcmp", (uintptr_t)&strcmp},
    {"strncmp", (uintptr_t)&strncmp},
    {"strlen", (uintptr_t)&strlen},
    {"strnlen", (uintptr_t)&strnlen},
    {"strcpy", (uintptr_t)&strcpy},
    {"strncpy", (uintptr_t)&strncpy},
    {"memcpy", (uintptr_t)&memcpy},
    {"memset", (uintptr_t)&memset},
    {"memmove", (uintptr_t)&memmove},
    {"memcmp", (uintptr_t)&memcmp},
    {"strchr", (uintptr_t)&strchr},
    {"strrchr", (uintptr_t)&strrchr},
    {"strcat", (uintptr_t)&strcat},
    {"strdup", (uintptr_t)&strdup},

    /* Kernel Memory Allocators */
    {"kmalloc", (uintptr_t)&kmalloc},
    {"kzalloc", (uintptr_t)&kzalloc},
    {"kfree", (uintptr_t)&kfree},
    {"pmm_alloc_pages", (uintptr_t)&pmm_alloc_pages},
    {"pmm_free_pages", (uintptr_t)&pmm_free_pages},
    {"pmm_alloc_page", (uintptr_t)&pmm_alloc_page},
    {"pmm_free_page", (uintptr_t)&pmm_free_page},
    {"vmm_map_page", (uintptr_t)&vmm_map_page},
    {"vmm_unmap_page", (uintptr_t)&vmm_unmap_page},
    {"vmm_virt_to_phys", (uintptr_t)&vmm_virt_to_phys},

    /* Virtual File System & DevFS */
    {"vfs_mount", (uintptr_t)&vfs_mount},
    {"vfs_lookup", (uintptr_t)&vfs_lookup},
    {"vfs_mkdir", (uintptr_t)&vfs_mkdir},
    {"devfs_register_device", (uintptr_t)&devfs_register_device},
    {"devfs_unregister_device", (uintptr_t)&devfs_unregister_device},

    /* Block Devices */
    {"block_device_register", (uintptr_t)&block_device_register},
    {"block_device_get", (uintptr_t)&block_device_get},
    {"block_device_get_count", (uintptr_t)&block_device_get_count},

    /* Interrupts, PIC, PIT & IO Ports */
    {"isr_register_handler", (uintptr_t)&isr_register_handler},
    {"pic_clear_mask", (uintptr_t)&pic_clear_mask},
    {"pic_set_mask", (uintptr_t)&pic_set_mask},
    {"pit_get_ticks", (uintptr_t)&pit_get_ticks},
    {"thread_sleep", (uintptr_t)&thread_sleep},
    {"inb", (uintptr_t)&inb},
    {"outb", (uintptr_t)&outb},
    {"inw", (uintptr_t)&inw},
    {"outw", (uintptr_t)&outw},
    {"inl", (uintptr_t)&inl},
    {"outl", (uintptr_t)&outl},

    /* Multitasking, Scheduling & Synchronization */
    {"sched_add_thread", (uintptr_t)&sched_add_thread},
    {"sched_yield", (uintptr_t)&sched_yield},
    {"sched_get_current_process", (uintptr_t)&sched_get_current_process},
    {"sched_get_current_thread", (uintptr_t)&sched_get_current_thread},
    {"process_create", (uintptr_t)&process_create},
    {"process_exit", (uintptr_t)&process_exit},
    {"spinlock_init", (uintptr_t)&spinlock_init},
    {"spinlock_acquire", (uintptr_t)&spinlock_acquire},
    {"spinlock_release", (uintptr_t)&spinlock_release},
    {"futex_wait", (uintptr_t)&futex_wait},
    {"futex_wake", (uintptr_t)&futex_wake},
    {"futex_requeue", (uintptr_t)&futex_requeue},

    /* Module Management Interop */
    {"ksym_lookup", (uintptr_t)&ksym_lookup},
    {"ksym_register", (uintptr_t)&ksym_register},
    {"module_find", (uintptr_t)&module_find},

    {NULL, 0}};

uintptr_t ksym_lookup(const char *name) {
    if (!name)
        return 0;

    /* 1. Search built-in kernel symbols */
    for (size_t i = 0; g_builtin_ksymtab[i].name != NULL; i++) {
        if (strcmp(g_builtin_ksymtab[i].name, name) == 0) {
            return g_builtin_ksymtab[i].addr;
        }
    }

    /* 2. Search dynamic module-exported symbols */
    spinlock_acquire(&g_ksym_lock);
    for (size_t i = 0; i < g_dynamic_syms_count; i++) {
        if (g_dynamic_syms[i].name && strcmp(g_dynamic_syms[i].name, name) == 0) {
            uintptr_t addr = g_dynamic_syms[i].addr;
            spinlock_release(&g_ksym_lock);
            return addr;
        }
    }
    spinlock_release(&g_ksym_lock);

    return 0;
}

int ksym_register(const char *name, uintptr_t addr) {
    if (!name || addr == 0)
        return -1;

    spinlock_acquire(&g_ksym_lock);
    if (g_dynamic_syms_count >= MAX_DYNAMIC_SYMS) {
        spinlock_release(&g_ksym_lock);
        return -1;
    }

    g_dynamic_syms[g_dynamic_syms_count].name = strdup(name);
    g_dynamic_syms[g_dynamic_syms_count].addr = addr;
    g_dynamic_syms_count++;
    spinlock_release(&g_ksym_lock);

    return 0;
}

void ksym_unregister_range(uintptr_t base, size_t size) {
    spinlock_acquire(&g_ksym_lock);
    size_t i = 0;
    while (i < g_dynamic_syms_count) {
        uintptr_t addr = g_dynamic_syms[i].addr;
        if (addr >= base && addr < (base + size)) {
            if (g_dynamic_syms[i].name) {
                kfree((void *)g_dynamic_syms[i].name);
            }
            g_dynamic_syms[i] = g_dynamic_syms[g_dynamic_syms_count - 1];
            g_dynamic_syms_count--;
        } else {
            i++;
        }
    }
    spinlock_release(&g_ksym_lock);
}
