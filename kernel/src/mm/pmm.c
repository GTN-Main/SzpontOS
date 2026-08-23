#include <mm/pmm.h>
#include <kernel/string.h>
#include <kernel/kprint.h>
#include <kernel/panic.h>
#include <kernel/spinlock.h>

static uint8_t *g_pmm_bitmap = NULL;
static size_t g_total_pages = 0;
static size_t g_used_pages = 0;
static size_t g_last_page_idx = 0;
static uint64_t g_hhdm_offset = 0;
static spinlock_t g_pmm_lock = SPINLOCK_INIT;

static inline void bitmap_set(size_t bit) {
    g_pmm_bitmap[bit / 8] |= (1 << (bit % 8));
}

static inline void bitmap_clear(size_t bit) {
    g_pmm_bitmap[bit / 8] &= ~(1 << (bit % 8));
}

static inline bool bitmap_test(size_t bit) {
    return (g_pmm_bitmap[bit / 8] & (1 << (bit % 8))) != 0;
}

void pmm_init(struct limine_memmap_response *memmap, uint64_t hhdm_offset) {
    if (!memmap || memmap->entry_count == 0) {
        panic("PMM: Invalid or empty memory map provided by bootloader!");
    }

    g_hhdm_offset = hhdm_offset;
    uintptr_t max_addr = 0;

    /* Find highest usable physical address */
    for (size_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            uintptr_t top = entry->base + entry->length;
            if (top > max_addr) {
                max_addr = top;
            }
        }
    }

    g_total_pages = max_addr / PAGE_SIZE;
    size_t bitmap_size = ALIGN_UP(g_total_pages / 8, PAGE_SIZE);

    /* Find a usable memory region large enough to hold our bitmap */
    uintptr_t bitmap_phys = 0;
    for (size_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE && entry->length >= bitmap_size) {
            bitmap_phys = entry->base;
            break;
        }
    }

    if (bitmap_phys == 0) {
        panic("PMM: Could not find contiguous physical memory for allocation bitmap (%lu bytes required)!", bitmap_size);
    }

    /* Access bitmap via HHDM virtual offset */
    g_pmm_bitmap = (uint8_t *)(bitmap_phys + g_hhdm_offset);

    /* Initially mark all pages as used (1) */
    memset(g_pmm_bitmap, 0xFF, bitmap_size);
    g_used_pages = g_total_pages;

    /* Mark all USABLE regions as free (0) in the bitmap */
    for (size_t i = 0; i < memmap->entry_count; i++) {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            size_t start_page = entry->base / PAGE_SIZE;
            size_t page_count = entry->length / PAGE_SIZE;

            for (size_t p = 0; p < page_count; p++) {
                if (start_page + p < g_total_pages) {
                    bitmap_clear(start_page + p);
                    g_used_pages--;
                }
            }
        }
    }

    /* Mark the bitmap itself and low 1MB as used */
    size_t bitmap_start_page = bitmap_phys / PAGE_SIZE;
    size_t bitmap_page_count = bitmap_size / PAGE_SIZE;
    for (size_t p = 0; p < bitmap_page_count; p++) {
        bitmap_set(bitmap_start_page + p);
        g_used_pages++;
    }

    /* Lock low 1MiB (BIOS/hardware structures) */
    for (size_t p = 0; p < 256; p++) {
        if (!bitmap_test(p)) {
            bitmap_set(p);
            g_used_pages++;
        }
    }

    klog_info("PMM initialized: %lu MiB total, %lu MiB free, %lu MiB used",
              (g_total_pages * PAGE_SIZE) / (1024 * 1024),
              ((g_total_pages - g_used_pages) * PAGE_SIZE) / (1024 * 1024),
              (g_used_pages * PAGE_SIZE) / (1024 * 1024));
}

uintptr_t pmm_alloc_page(void) {
    spinlock_acquire(&g_pmm_lock);

    for (size_t i = 0; i < g_total_pages; i++) {
        size_t idx = (g_last_page_idx + i) % g_total_pages;
        if (!bitmap_test(idx)) {
            bitmap_set(idx);
            g_used_pages++;
            g_last_page_idx = idx + 1;
            spinlock_release(&g_pmm_lock);
            return (uintptr_t)(idx * PAGE_SIZE);
        }
    }

    spinlock_release(&g_pmm_lock);
    panic("PMM: Out of physical memory!");
}

uintptr_t pmm_alloc_pages(size_t count) {
    if (count == 0) return 0;
    if (count == 1) return pmm_alloc_page();

    spinlock_acquire(&g_pmm_lock);

    size_t consecutive = 0;
    size_t start_idx = 0;

    for (size_t i = 0; i < g_total_pages; i++) {
        if (!bitmap_test(i)) {
            if (consecutive == 0) start_idx = i;
            consecutive++;
            if (consecutive == count) {
                for (size_t p = 0; p < count; p++) {
                    bitmap_set(start_idx + p);
                }
                g_used_pages += count;
                spinlock_release(&g_pmm_lock);
                return (uintptr_t)(start_idx * PAGE_SIZE);
            }
        } else {
            consecutive = 0;
        }
    }

    spinlock_release(&g_pmm_lock);
    panic("PMM: Out of contiguous physical memory for %lu pages!", count);
}

void pmm_free_page(uintptr_t phys_addr) {
    size_t page_idx = phys_addr / PAGE_SIZE;
    if (page_idx >= g_total_pages) return;

    spinlock_acquire(&g_pmm_lock);
    if (bitmap_test(page_idx)) {
        bitmap_clear(page_idx);
        g_used_pages--;
    }
    spinlock_release(&g_pmm_lock);
}

void pmm_free_pages(uintptr_t phys_addr, size_t count) {
    for (size_t i = 0; i < count; i++) {
        pmm_free_page(phys_addr + i * PAGE_SIZE);
    }
}

size_t pmm_get_total_memory(void) {
    return g_total_pages * PAGE_SIZE;
}

size_t pmm_get_free_memory(void) {
    return (g_total_pages - g_used_pages) * PAGE_SIZE;
}

size_t pmm_get_used_memory(void) {
    return g_used_pages * PAGE_SIZE;
}
