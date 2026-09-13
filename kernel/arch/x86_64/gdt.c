#include <arch/x86_64/gdt.h>
#include <arch/x86_64/io.h>
#include <kernel/smp.h>
#include <kernel/string.h>
#include <kernel/kprint.h>

static gdt_table_t g_boot_gdt;
static gdt_descriptor_t g_boot_gdt_desc;
static tss_entry_t g_boot_tss;

uintptr_t g_current_kernel_stack = 0;
uint8_t g_default_fpu_state[512] __attribute__((aligned(16)));

static void gdt_populate_table(gdt_table_t *table, tss_entry_t *tss) {
    memset(table, 0, sizeof(gdt_table_t));

    /* Kernel Code 64-bit: Ring 0, Exec/Read, Conforming=0, 64-bit=1 */
    table->kernel_code.limit_low = 0xFFFF;
    table->kernel_code.access = 0x9A;      /* Present, Ring 0, Executable, Readable */
    table->kernel_code.granularity = 0x20; /* 64-bit long mode flag (L=1, D=0) */

    /* Kernel Data 64-bit: Ring 0, Read/Write */
    table->kernel_data.limit_low = 0xFFFF;
    table->kernel_data.access = 0x92;      /* Present, Ring 0, Writable */
    table->kernel_data.granularity = 0x00;

    /* User Data 64-bit: Ring 3, Read/Write */
    table->user_data.limit_low = 0xFFFF;
    table->user_data.access = 0xF2;        /* Present, Ring 3, Writable */
    table->user_data.granularity = 0x00;

    /* User Code 64-bit: Ring 3, Exec/Read */
    table->user_code.limit_low = 0xFFFF;
    table->user_code.access = 0xFA;        /* Present, Ring 3, Executable, Readable */
    table->user_code.granularity = 0x20;   /* 64-bit flag */

    /* TSS Setup */
    uintptr_t tss_base = (uintptr_t)tss;
    uint32_t tss_limit = sizeof(tss_entry_t) - 1;

    table->tss_entry.limit_low = (uint16_t)(tss_limit & 0xFFFF);
    table->tss_entry.base_low = (uint16_t)(tss_base & 0xFFFF);
    table->tss_entry.base_mid = (uint8_t)((tss_base >> 16) & 0xFF);
    table->tss_entry.access = 0x89;        /* Present, Ring 0, Available 64-bit TSS */
    table->tss_entry.granularity = (uint8_t)((tss_limit >> 16) & 0x0F);
    table->tss_entry.base_high = (uint8_t)((tss_base >> 24) & 0xFF);
    table->tss_entry.base_upper = (uint32_t)(tss_base >> 32);
    table->tss_entry.reserved = 0;

    tss->iomap_base = sizeof(tss_entry_t);
}

static void gdt_flush_and_load_tss(const gdt_descriptor_t *desc) {
    __asm__ volatile("lgdt %0\n\t"
                     "mov $0x10, %%ax\n\t"
                     "mov %%ax, %%ds\n\t"
                     "mov %%ax, %%es\n\t"
                     "mov %%ax, %%ss\n\t"
                     "pushq $0x08\n\t"
                     "leaq 1f(%%rip), %%rax\n\t"
                     "pushq %%rax\n\t"
                     "lretq\n\t"
                     "1:\n\t"
                     "mov $0x28, %%ax\n\t"
                     "ltr %%ax\n\t"
                     :
                     : "m"(*desc)
                     : "rax", "memory");
}

void gdt_init_cpu(uint32_t cpu_id, gdt_table_t *gdt, gdt_descriptor_t *gdt_desc, tss_entry_t *tss) {
    (void)cpu_id;
    memset(tss, 0, sizeof(tss_entry_t));
    gdt_populate_table(gdt, tss);
    gdt_desc->limit = sizeof(gdt_table_t) - 1;
    gdt_desc->base = (uint64_t)gdt;
    gdt_flush_and_load_tss(gdt_desc);
}

void gdt_init(void) {
    gdt_init_cpu(0, &g_boot_gdt, &g_boot_gdt_desc, &g_boot_tss);
    cpu_t *bsp = smp_get_cpu(0);
    wrmsr(0xC0000101, (uint64_t)bsp);
    wrmsr(0xC0000102, (uint64_t)bsp);
    klog_info("GDT & TSS initialized successfully (CS=0x08, DS=0x10, TSS=0x28)");
}


void gdt_set_cpu_kernel_stack(uint32_t cpu_id, uintptr_t stack) {
    cpu_t *cpu = smp_get_cpu(cpu_id);
    if (cpu) {
        cpu->tss.rsp0 = stack;
        cpu->kernel_stack = stack;
    }
    if (cpu_id == 0) {
        g_boot_tss.rsp0 = stack;
        g_current_kernel_stack = stack;
    }
}

void gdt_set_kernel_stack(uintptr_t stack) {
    cpu_t *cpu = smp_current_cpu();
    uint32_t cid = cpu ? cpu->cpu_id : 0;
    gdt_set_cpu_kernel_stack(cid, stack);
}

void fpu_init_cpu(void) {
    uint64_t cr0 = read_cr0();
    cr0 &= ~(1ULL << 2); /* Clear EM */
    cr0 |= (1ULL << 1);  /* Set MP */
    cr0 |= (1ULL << 5);  /* Set NE */
    write_cr0(cr0);

    uint64_t cr4 = read_cr4();
    cr4 |= (1ULL << 9);  /* Set OSFXSR (SSE enable) */
    cr4 |= (1ULL << 10); /* Set OSXMMEXCPT */
    write_cr4(cr4);

    __asm__ volatile("fninit");
    uint32_t mxcsr = 0x1F80;
    __asm__ volatile("ldmxcsr %0" : : "m"(mxcsr));
}

void fpu_init(void) {
    fpu_init_cpu();
    __asm__ volatile("fxsave64 %0" : "=m"(g_default_fpu_state));
    klog_info("FPU & SSE / AVX SIMD instructions enabled (CR0 & CR4 configured)");
}
