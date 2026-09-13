/*
 * SzpontOS - x86_64 MTRR (Memory Type Range Register) Driver
 * (C) Copyright by Szpont Industries. All rights reserved.
 *
 * Configures hardware Write-Combining (WC) on x86 processors for discrete
 * GPUs, PCIe VRAM buffers, and Legacy BIOS framebuffers to achieve maximum
 * PCIe burst throughput.
 */

#include <arch/x86_64/mtrr.h>
#include <arch/x86_64/io.h>
#include <kernel/spinlock.h>
#include <kernel/kprint.h>
#include <mm/vmm.h>

#define IA32_MTRRCAP_MSR       0x0FE
#define IA32_MTRR_DEF_TYPE_MSR 0x2FF
#define IA32_MTRR_PHYSBASE0    0x200
#define IA32_MTRR_PHYSMASK0    0x201

#define MTRRCAP_WC_BIT         (1ULL << 10)
#define MTRR_DEF_TYPE_ENABLE   (1ULL << 11)
#define MTRR_PHYSMASK_VALID    (1ULL << 11)

static uint64_t mtrr_get_phys_mask(void) {
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000000), "c"(0));
    uint32_t phys_bits = 36; /* Standard safe x86_64 fallback */
    if (eax >= 0x80000008) {
        __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000008), "c"(0));
        uint32_t bits = eax & 0xFF;
        if (bits >= 32 && bits <= 52) {
            phys_bits = bits;
        }
    }
    uint64_t mask = (phys_bits >= 64) ? ~0ULL : ((1ULL << phys_bits) - 1ULL);
    return mask & ~0xFFFULL;
}

bool mtrr_is_supported(void) {
    uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    __asm__ volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1), "c"(0));
    /* Check CPUID.1:EDX[bit 12] = MTRR */
    if (!(edx & (1U << 12))) {
        return false;
    }

    uint64_t cap = rdmsr(IA32_MTRRCAP_MSR);
    /* Check if Write-Combining (WC) is supported */
    return (cap & MTRRCAP_WC_BIT) != 0;
}

bool mtrr_set_write_combining(uintptr_t base, size_t size) {
    if (!mtrr_is_supported()) {
        klog_warn("MTRR: CPU does not support MTRR Write-Combining");
        return false;
    }

    uint64_t cap = rdmsr(IA32_MTRRCAP_MSR);
    uint8_t vcnt = (uint8_t)(cap & 0xFF);
    if (vcnt == 0) {
        klog_warn("MTRR: No variable MTRRs available");
        return false;
    }

    uint64_t phys_mask = mtrr_get_phys_mask();

    /* Round size up to nearest power of 2 (minimum 1 MiB) */
    size_t rounded_size = 0x100000;
    while (rounded_size < size) {
        rounded_size <<= 1;
    }

    uintptr_t aligned_base = base & ~((uintptr_t)rounded_size - 1);

    /* 1. Check if already covered by an active MTRR */
    for (uint8_t i = 0; i < vcnt; i++) {
        uint64_t mask_val = rdmsr(IA32_MTRR_PHYSMASK0 + 2 * i);
        if (mask_val & MTRR_PHYSMASK_VALID) {
            uint64_t base_val = rdmsr(IA32_MTRR_PHYSBASE0 + 2 * i);
            uintptr_t cur_base = (uintptr_t)(base_val & phys_mask);
            uint8_t cur_type = (uint8_t)(base_val & 0xFF);
            if (cur_base == aligned_base && cur_type == MTRR_TYPE_WC) {
                klog_info("MTRR: Phys 0x%lx already configured as Write-Combining (slot %u)",
                          base, i);
                return true;
            }
        }
    }

    /* 2. Find an empty variable MTRR slot */
    int slot = -1;
    for (uint8_t i = 0; i < vcnt; i++) {
        uint64_t mask_val = rdmsr(IA32_MTRR_PHYSMASK0 + 2 * i);
        if (!(mask_val & MTRR_PHYSMASK_VALID)) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        klog_warn("MTRR: All %u variable MTRR slots are occupied", vcnt);
        return false;
    }

    uint64_t new_base = (aligned_base & phys_mask) | MTRR_TYPE_WC;
    uint64_t new_mask = ((~((uint64_t)rounded_size - 1ULL)) & phys_mask) | MTRR_PHYSMASK_VALID;

    /* 3. Safe MTRR Update Sequence (Intel SDM Vol 3A, Section 11.11.4.1) */
    uint64_t rflags = spinlock_irqsave();

    /* Disable caches: CR0.CD = 1, CR0.NW = 0 */
    uint64_t cr0 = read_cr0();
    write_cr0((cr0 | (1ULL << 30)) & ~(1ULL << 29));
    __asm__ volatile("wbinvd" ::: "memory");

    /* Disable MTRRs globally */
    uint64_t def_type = rdmsr(IA32_MTRR_DEF_TYPE_MSR);
    wrmsr(IA32_MTRR_DEF_TYPE_MSR, def_type & ~MTRR_DEF_TYPE_ENABLE);

    /* Write new Variable MTRR Base and Mask */
    wrmsr(IA32_MTRR_PHYSBASE0 + 2 * slot, new_base);
    wrmsr(IA32_MTRR_PHYSMASK0 + 2 * slot, new_mask);

    /* Flush caches again and re-enable MTRRs */
    __asm__ volatile("wbinvd" ::: "memory");
    wrmsr(IA32_MTRR_DEF_TYPE_MSR, def_type | MTRR_DEF_TYPE_ENABLE);

    /* Restore CR0 (re-enable caches) and flush TLB */
    write_cr0(cr0);
    write_cr3(read_cr3());

    spinlock_irqrestore(rflags);

    klog_info("MTRR: Configured Write-Combining for 0x%lx (size %zu MiB) in slot %d",
              aligned_base, rounded_size / (1024 * 1024), slot);
    return true;
}
