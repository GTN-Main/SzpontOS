#include <arch/x86_64/io.h>
#include <arch/x86_64/gdt.h>
#include <kernel/kprint.h>

#define MSR_EFER    0xC0000080
#define MSR_STAR    0xC0000081
#define MSR_LSTAR   0xC0000082
#define MSR_SFMASK  0xC0000084

#define EFER_SCE    (1 << 0) /* System Call Extensions */

extern void syscall_entry(void);

void syscall_arch_init(void) {
    /* 1. Enable SCE in EFER */
    uint64_t efer = rdmsr(MSR_EFER);
    efer |= EFER_SCE;
    wrmsr(MSR_EFER, efer);

    /* 2. Configure STAR MSR
     * Bits [47:32] = Kernel CS (0x08), Kernel SS = 0x10
     * Bits [63:48] = User CS base for sysret (0x10 -> User SS = 0x18 | 3, User CS = 0x20 | 3)
     */
    uint64_t star = ((uint64_t)0x0010 << 48) | ((uint64_t)0x0008 << 32);
    wrmsr(MSR_STAR, star);

    /* 3. Configure LSTAR with syscall_entry handler */
    wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);

    /* 4. Configure SFMASK to clear IF (0x200), DF (0x400), TF (0x100) on syscall entry */
    wrmsr(MSR_SFMASK, 0x200 | 0x400 | 0x100);

    klog_info("x86_64 Fast Syscall MSRs configured (SYSCALL / SYSRET enabled)");
}
