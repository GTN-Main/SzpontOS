#ifndef SZPONTOS_MM_HEAP_H
#define SZPONTOS_MM_HEAP_H

#include <kernel/types.h>

void heap_init(size_t initial_size);

void *kmalloc(size_t size);
void *kzalloc(size_t size);
void *kcalloc(size_t num, size_t size);
void *krealloc(void *ptr, size_t new_size);
void kfree(void *ptr);

void heap_dump_stats(void);

#endif /* SZPONTOS_MM_HEAP_H */
