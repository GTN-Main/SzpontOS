#ifndef SZPONTOS_ARCH_X86_64_GDT_H
#define SZPONTOS_ARCH_X86_64_GDT_H

#include <kernel/types.h>

#define GDT_KERNEL_CODE_SEL 0x08
#define GDT_KERNEL_DATA_SEL 0x10
#define GDT_USER_DATA_SEL (0x18 | 3)
#define GDT_USER_CODE_SEL (0x20 | 3)
#define GDT_TSS_SEL 0x28

struct __attribute__((packed)) gdt_descriptor {
    uint16_t limit;
    uint64_t base;
};
typedef struct gdt_descriptor gdt_descriptor_t;

struct __attribute__((packed)) gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
};
typedef struct gdt_entry gdt_entry_t;

struct __attribute__((packed)) gdt_tss_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
    uint32_t base_upper;
    uint32_t reserved;
};
typedef struct gdt_tss_entry gdt_tss_entry_t;

struct __attribute__((packed)) tss_entry {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
};
typedef struct tss_entry tss_entry_t;

struct __attribute__((packed)) gdt_table {
    struct gdt_entry null_entry;    /* 0x00 */
    struct gdt_entry kernel_code;   /* 0x08 */
    struct gdt_entry kernel_data;   /* 0x10 */
    struct gdt_entry user_data;     /* 0x18 */
    struct gdt_entry user_code;     /* 0x20 */
    struct gdt_tss_entry tss_entry; /* 0x28 (16 bytes) */
};
typedef struct gdt_table gdt_table_t;

void gdt_init(void);
void gdt_init_cpu(uint32_t cpu_id, gdt_table_t *gdt, gdt_descriptor_t *gdt_desc, tss_entry_t *tss);
void gdt_set_kernel_stack(uintptr_t stack);
void gdt_set_cpu_kernel_stack(uint32_t cpu_id, uintptr_t stack);
void fpu_init(void);
void fpu_init_cpu(void);
extern uint8_t g_default_fpu_state[512] __attribute__((aligned(16)));

#endif /* SZPONTOS_ARCH_X86_64_GDT_H */
