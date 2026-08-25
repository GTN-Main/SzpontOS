/*
 * SzpontOS - ACPI Power Management & Hardware Reset Implementation
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <drivers/power.h>
#include <arch/x86_64/io.h>
#include <fs/bcache.h>
#include <sched/process.h>
#include <sched/sched.h>
#include <kernel/kprint.h>

void power_shutdown(void) {
    klog_info("System is shutting down...");
    bflush(NULL);

    /* 1. QEMU ACPI poweroff port */
    outw(0x604, 0x2000);
    io_wait();

    /* 2. QEMU isa-debug-exit port */
    outb(0x501, 0x31);
    io_wait();

    /* 3. VirtualBox ACPI poweroff */
    outw(0x4004, 0x3400);
    io_wait();

    /* 4. Bochs / QEMU older ACPI */
    outw(0xB004, 0x2000);
    io_wait();

    /* 5. Fallback CPU halt */
    cli();
    while (1) {
        hlt();
    }
}

void power_reboot(void) {
    klog_info("System is rebooting...");
    bflush(NULL);
    cli();

    /*
     * Reset sequence modeled after FreeBSD sys/x86/x86/cpu_machdep.c cpu_reset_real().
     */

    /* 1. 8042 Keyboard Controller reset pulse (outb 0x64, 0xFE) */
    for (int i = 0; i < 1000; i++) {
        if ((inb(0x64) & 0x02) == 0)
            break;
        io_wait();
    }
    outb(0x64, 0xFE);
    for (volatile int d = 0; d < 5000000; d++) {
    } /* delay ~500ms */

    /* 2. PCI Reset Control Register (Port 0xCF9) — hard reset */
    outb(0xCF9, 0x02);
    io_wait();
    outb(0xCF9, 0x06);
    for (volatile int d = 0; d < 5000000; d++) {
    }

    /* 3. Fast A20 / Init register (Port 0x92) — assert INIT# */
    {
        uint8_t b = inb(0x92);
        if (b != 0xFF) {
            if ((b & 0x01) != 0)
                outb(0x92, b & 0xFE);
            outb(0x92, b | 0x01);
        }
    }
    for (volatile int d = 0; d < 5000000; d++) {
    }

    /* 4. Triple Fault — load NULL IDT and trigger undefined instruction */
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) null_idt = {0, 0};

    __asm__ volatile("lidt %0; ud2" ::"m"(null_idt));

    /* NOTREACHED */
    while (1) {
        hlt();
    }
}

int sys_reboot(int magic1, int magic2, int cmd, void *arg) {
    (void)arg;
    process_t *proc = sched_get_current_process();
    if (!proc || proc->euid != 0) {
        return -1; /* EPERM */
    }

    /* Verify reboot magic or permissive execution */
    if (magic1 != (int)REBOOT_MAGIC1 && magic1 != 0) {
        return -1; /* EINVAL */
    }

    switch ((uint32_t)cmd) {
    case REBOOT_CMD_POWER_OFF:
    case REBOOT_CMD_HALT:
        power_shutdown();
        return 0;

    case REBOOT_CMD_RESTART:
    default:
        power_reboot();
        return 0;
    }
}
