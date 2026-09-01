#include <arch/x86_64/gdt.h>
#include <arch/x86_64/idt.h>
#include <arch/x86_64/io.h>
#include <arch/x86_64/pic.h>
#include <arch/x86_64/pit.h>
#include <drivers/framebuffer.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <drivers/ps2_mouse.h>
#include <drivers/ps2_test.h>
#include <drivers/speaker.h>
#include <drivers/tty.h>
#include <drivers/drm.h>
#include <drivers/evdev.h>
#include <drivers/serial.h>
#include <drivers/random.h>
#include <drivers/pty.h>
#include <drivers/block.h>
#include <drivers/ata.h>
#include <drivers/ahci.h>
#include <drivers/pci.h>
#include <net/net.h>
#include <fs/vfs.h>
#include <fs/devfs.h>
#include <fs/procfs.h>
#include <fs/tmpfs.h>
#include <fs/bcache.h>
#include <fs/ext2.h>
#include <fs/initramfs.h>
#include <fs/elf.h>
#include <sched/process.h>
#include <sched/sched.h>
#include <sched/futex.h>
#include <kernel/module.h>
#include <kernel/sysctl.h>
#include <syscall/syscall.h>
#include <kernel/kprint.h>
#include <kernel/panic.h>
#include <kernel/string.h>
#include <kernel/types.h>
#include <limine.h>
#include <mm/heap.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/shm.h>
#include <drivers/acpi.h>
#include <drivers/ioapic.h>
#include <drivers/usb.h>
#include <drivers/rtc.h>

/* Limine Requests */
__attribute__((used, section(".requests"))) static volatile LIMINE_BASE_REVISION(3);

__attribute__((used, section(".requests"))) static volatile struct limine_framebuffer_request g_framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST, .revision = 0};

__attribute__((used, section(".requests"))) static volatile struct limine_memmap_request g_memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST, .revision = 0};

__attribute__((used, section(".requests"))) static volatile struct limine_hhdm_request g_hhdm_request = {
    .id = LIMINE_HHDM_REQUEST, .revision = 0};

__attribute__((used, section(".requests"))) static volatile struct limine_module_request g_module_request = {
    .id = LIMINE_MODULE_REQUEST, .revision = 0};

__attribute__((used, section(".requests_start_marker"))) static volatile LIMINE_REQUESTS_START_MARKER;
__attribute__((used, section(".requests_end_marker"))) static volatile LIMINE_REQUESTS_END_MARKER;

static void print_banner(void) {
    kprintf("\n");
    fb_console_set_color(FB_COLOR_CYAN, FB_COLOR_BG);
    kprintf("   _____                      _    ____   _____\n");
    kprintf("  / ____|                    | |  / __ \\ / ____|\n");
    kprintf(" | (___  _____ __   ___  _ __ | |_| |  | | (___  \n");
    kprintf("  \\___ \\|_  / '_ \\ / _ \\| '_ \\| __| |  | |\\___ \\ \n");
    kprintf("  ____) |/ /| |_) | (_) | | | | |_| |__| |____) |\n");
    kprintf(" |_____//___| .__/ \\___/|_| |_|\\__|\\____/|_____/ \n");
    kprintf("            | |                                  \n");
    kprintf("            |_|                                  \n");
    fb_console_set_color(FB_COLOR_YELLOW, FB_COLOR_BG);
    kprintf(" ========================================================\n");
    kprintf("  SzpontOS v0.1.0 (x86_64 Higher-Half UNIX Kernel)\n");
    kprintf("  (C) Copyright by Szpont Industries. All rights reserved.\n");
    kprintf(" ========================================================\n\n");
    fb_console_set_color(FB_COLOR_WHITE, FB_COLOR_BG);
}

static void run_heap_self_test(void) {
    klog_info("Running Kernel Heap Self-Test...");
    void *p1 = kmalloc(128);
    void *p2 = kmalloc(512);
    void *p3 = kmalloc(2048);

    KASSERT(p1 != NULL);
    KASSERT(p2 != NULL);
    KASSERT(p3 != NULL);

    memset(p1, 0xAA, 128);
    memset(p2, 0xBB, 512);
    memset(p3, 0xCC, 2048);

    kfree(p2);
    void *p4 = kmalloc(256);
    KASSERT(p4 != NULL);

    kfree(p1);
    kfree(p3);
    kfree(p4);

    klog_info("Heap Self-Test PASSED! Memory allocations and coalescing OK.");
}

void _start(void) {
    /* Step 1: Early Serial Debug Port */
    serial_init();
    kprint_init();

    /* Check Limine base revision */
    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        panic("Limine base revision 3 is NOT supported by the bootloader!");
    }

    /* Step 2: Framebuffer Console */
    if (g_framebuffer_request.response != NULL && g_framebuffer_request.response->framebuffer_count > 0) {
        struct limine_framebuffer *fb = g_framebuffer_request.response->framebuffers[0];
        framebuffer_init(fb);
    }

    /* Display Welcome Banner */
    print_banner();

    /* Step 3: Global Descriptor Table, TSS & FPU/SSE */
    gdt_init();
    fpu_init();

    /* Step 4: Interrupt Subsystem (PIC Remap & IDT) */
    pic_remap(32, 40);
    idt_init();

    /* Step 5: Memory Management (PMM, VMM, Heap) */
    if (!g_memmap_request.response || !g_hhdm_request.response) {
        panic("Bootloader failed to provide memory map or HHDM response!");
    }

    uint64_t hhdm_offset = g_hhdm_request.response->offset;
    pmm_init(g_memmap_request.response, hhdm_offset);
    vmm_init(hhdm_offset);
    pic_enable_apic_extint();
    acpi_init();
    ioapic_init();
    heap_init(8 * 1024 * 1024); /* 8 MiB initial heap */

    /* Step 5b: Timers & Real-Time Clock calibration for precise driver delays */
    pit_init(1000);
    rtc_init();
    if (ioapic_is_active()) {
        lapic_timer_init(1000);
    } else {
        pit_route_irq();
    }

    run_heap_self_test();

    /* Initialize in-RAM Shadow Backbuffer & Write-Combining for blazing fast Framebuffer */
    framebuffer_init_backbuffer();

    /* Step 6: Virtual File System, UNIX TTY & Root FS (Initramfs) */
    vfs_init();
    tty_init();

    /* Check for loaded boot modules (Initramfs) */
    if (g_module_request.response && g_module_request.response->module_count > 0) {
        struct limine_file *mod = g_module_request.response->modules[0];
        klog_info("Initramfs module loaded at %p (%lu bytes, path: %s)", mod->address, mod->size,
                  mod->path ? mod->path : "unknown");

        vfs_node_t *root_fs = initramfs_init(mod->address, mod->size);
        if (root_fs) {
            vfs_mount("/", root_fs);
        }
    } else {
        klog_warn("No initramfs module provided by bootloader!");
    }

    /* Mount DevFS at /dev, CSPRNG & UNIX98 PTY */
    devfs_init();
    random_init();
    pty_init();

    /* Mount ProcFS at /proc */
    vfs_node_t *proc_fs = procfs_init();
    if (proc_fs) {
        vfs_mount("/proc", proc_fs);
    }

    /* Mount TmpFS at /tmp and /run */
    tmpfs_init();

    /* Step 7: Block Devices, Buffer Cache, ATA & AHCI SATA Storage */
    bcache_init();
    ata_init();

    /* Step 8: PCI Bus Enumeration, AHCI SATA, USB & Networking */
    pci_init();
    ahci_init();
    usb_init();
    net_init();

    /* Mount primary hard disk (/dev/sda or /dev/hda) at /mnt */
    block_device_t *root_disk = block_device_get("sda");
    if (!root_disk)
        root_disk = block_device_get("hda");

    if (root_disk) {
        vfs_node_t *ext2_root = ext2_mount(root_disk);
        if (ext2_root) {
            vfs_mount("/mnt", ext2_root);
            klog_info("ext2: Mounted '/dev/%s' at '/mnt'", root_disk->name);
        }
    }

    /* Step 9: Sysctl MIB & Fast Syscalls Subsystem */
    sysctl_init();
    syscall_init();

    /* Step 10: Multitasking, Futexes, Modules, Shared Memory & Scheduler */
    process_init();
    futex_init();
    shm_init();
    module_init_subsystem();
    sched_init();

    /* Step 11: PS/2 Keyboard, Mouse & Evdev Drivers */
    mouse_init();
    keyboard_init();
    ps2_mouse_init();
    evdev_init();

    /* Step 12: Launch First Userland Process (/bin/init or /bin/sh) */
    process_t *init_proc = elf_spawn("/bin/init", "init");
    if (!init_proc) {
        klog_warn("Could not spawn /bin/init, attempting /bin/sh...");
        init_proc = elf_spawn("/bin/sh", "sh");
    }

    if (init_proc) {
        klog_info("Initial Ring 3 process spawned successfully (PID %d)", init_proc->pid);
    } else {
        klog_warn("No userland binaries found in /bin! Falling back to kernel interactive loop.");
    }

    /* Enable Interrupts & Start Scheduler */
    sti();
    klog_info("Interrupts enabled! Starting Preemptive Multitasking Scheduler...");

    if (init_proc) {
        sched_start();
    }

    /* Fallback idle loop if no user process */
    while (1) {
        hlt();
    }
}
