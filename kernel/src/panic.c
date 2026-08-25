#include <kernel/panic.h>
#include <kernel/kprint.h>
#include <drivers/serial.h>
#include <drivers/framebuffer.h>
#include <arch/x86_64/idt.h>
#include <arch/x86_64/io.h>

static void dump_stack(uint64_t *rbp) {
    kprintf("Stack backtrace:\n");
    for (int i = 0; i < 10 && rbp; i++) {
        uint64_t rip = rbp[1];
        uint64_t next_rbp = rbp[0];
        if (rip == 0)
            break;
        kprintf("  [%d] RIP: 0x%016lx, RBP: 0x%016lx\n", i, rip, (uint64_t)rbp);
        if (next_rbp <= (uint64_t)rbp)
            break;
        rbp = (uint64_t *)next_rbp;
    }
}

void panic(const char *fmt, ...) {
    cli();

    char buf[1024];
    va_list args;
    va_start(args, fmt);
    kvsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    serial_puts("\n\n!!!!!!!!!!!!!!!! KERNEL PANIC !!!!!!!!!!!!!!!!\n");
    serial_puts(buf);
    serial_puts("\nSystem halted.\n");

    fb_console_set_color(FB_COLOR_WHITE, FB_COLOR_RED);
    fb_console_clear();
    fb_console_puts("\n  =======================================================\n");
    fb_console_puts("  !!!                  KERNEL PANIC                   !!!\n");
    fb_console_puts("  =======================================================\n\n");
    fb_console_puts("  Reason: ");
    fb_console_puts(buf);
    fb_console_puts("\n\n");

    uint64_t *rbp;
    __asm__ volatile("mov %%rbp, %0" : "=r"(rbp));
    dump_stack(rbp);

    fb_console_puts("\n  System halted. Please reboot the machine.\n");

    for (;;) {
        hlt();
    }
}

void panic_with_frame(struct interrupt_frame *frame, const char *fmt, ...) {
    cli();

    char buf[1024];
    va_list args;
    va_start(args, fmt);
    kvsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    serial_puts("\n\n!!!!!!!!!!!!!!!! KERNEL PANIC (CPU EXCEPTION) !!!!!!!!!!!!!!!!\n");
    serial_puts(buf);
    serial_puts("\nSystem halted.\n");

    fb_console_set_color(FB_COLOR_WHITE, FB_COLOR_RED);
    fb_console_clear();
    fb_console_puts("\n  =======================================================\n");
    fb_console_puts("  !!!            KERNEL PANIC: CPU EXCEPTION          !!!\n");
    fb_console_puts("  =======================================================\n\n");
    fb_console_puts("  Reason: ");
    fb_console_puts(buf);
    fb_console_puts("\n\n");

    if (frame) {
        kprintf("  RIP: 0x%016lx  CS: 0x%04lx  RFLAGS: 0x%016lx\n", frame->rip, frame->cs, frame->rflags);
        kprintf("  RSP: 0x%016lx  SS: 0x%04lx  RBP:    0x%016lx\n", frame->rsp, frame->ss, frame->rbp);
        kprintf("  RAX: 0x%016lx  RBX: 0x%016lx  RCX: 0x%016lx\n", frame->rax, frame->rbx, frame->rcx);
        kprintf("  RDX: 0x%016lx  RSI: 0x%016lx  RDI: 0x%016lx\n", frame->rdx, frame->rsi, frame->rdi);
        kprintf("  R8:  0x%016lx  R9:  0x%016lx  R10: 0x%016lx\n", frame->r8, frame->r9, frame->r10);
        kprintf("  R11: 0x%016lx  R12: 0x%016lx  R13: 0x%016lx\n", frame->r11, frame->r12, frame->r13);
        kprintf("  R14: 0x%016lx  R15: 0x%016lx\n\n", frame->r14, frame->r15);
    }

    fb_console_puts("  System halted. Please reboot the machine.\n");

    for (;;) {
        hlt();
    }
}
