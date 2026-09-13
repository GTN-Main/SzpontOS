#ifndef SZPONTOS_ARCH_X86_64_MTRR_H
#define SZPONTOS_ARCH_X86_64_MTRR_H

#include <kernel/types.h>

#define MTRR_TYPE_UC  0
#define MTRR_TYPE_WC  1
#define MTRR_TYPE_WT  4
#define MTRR_TYPE_WP  5
#define MTRR_TYPE_WB  6

bool mtrr_is_supported(void);
bool mtrr_set_write_combining(uintptr_t base, size_t size);

#endif /* SZPONTOS_ARCH_X86_64_MTRR_H */
