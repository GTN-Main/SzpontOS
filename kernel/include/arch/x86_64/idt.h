#ifndef SZPONTOS_ARCH_X86_64_IDT_H
#define SZPONTOS_ARCH_X86_64_IDT_H

#include <kernel/types.h>

#define IDT_ENTRIES 256

#define IRQ0 32
#define IRQ1 33
#define IRQ2 34
#define IRQ3 35
#define IRQ4 36
#define IRQ5 37
#define IRQ6 38
#define IRQ7 39
#define IRQ8 40
#define IRQ9 41
#define IRQ10 42
#define IRQ11 43
#define IRQ12 44
#define IRQ13 45
#define IRQ14 46
#define IRQ15 47

struct __attribute__((packed)) interrupt_frame {
    /* Pushed by isr_common */
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;

    /* Interrupt vector & error code */
    uint64_t int_no;
    uint64_t err_code;

    /* Pushed automatically by CPU */
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

typedef struct interrupt_frame interrupt_frame_t;
typedef void (*isr_handler_t)(interrupt_frame_t *frame);

void idt_init(void);
void idt_set_gate(uint8_t vector, void *handler, uint8_t flags);
void isr_register_handler(uint8_t vector, isr_handler_t handler);

#endif /* SZPONTOS_ARCH_X86_64_IDT_H */
