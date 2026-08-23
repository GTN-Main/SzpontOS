#ifndef SZPONTOS_MM_PMM_H
#define SZPONTOS_MM_PMM_H

#include <kernel/types.h>
#include <limine.h>

void pmm_init(struct limine_memmap_response *memmap, uint64_t hhdm_offset);
uintptr_t pmm_alloc_page(void);
uintptr_t pmm_alloc_pages(size_t count);
void pmm_free_page(uintptr_t phys_addr);
void pmm_free_pages(uintptr_t phys_addr, size_t count);

size_t pmm_get_total_memory(void);
size_t pmm_get_free_memory(void);
size_t pmm_get_used_memory(void);

#endif /* SZPONTOS_MM_PMM_H */
