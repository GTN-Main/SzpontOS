#ifndef SZPONTOS_KERNEL_PANIC_H
#define SZPONTOS_KERNEL_PANIC_H

#include <kernel/types.h>

struct interrupt_frame;

__attribute__((noreturn)) void panic(const char *fmt, ...);
__attribute__((noreturn)) void panic_with_frame(struct interrupt_frame *frame, const char *fmt, ...);

#define KASSERT(expr) \
    do { \
        if (!(expr)) { \
            panic("Assertion failed: %s at %s:%d in %s()", #expr, __FILE__, __LINE__, __func__); \
        } \
    } while (0)

#endif /* SZPONTOS_KERNEL_PANIC_H */
