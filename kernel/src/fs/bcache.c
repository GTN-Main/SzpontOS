#include <fs/bcache.h>
#include <mm/heap.h>
#include <kernel/string.h>
#include <kernel/kprint.h>
#include <kernel/spinlock.h>

#define BCACHE_ENTRIES 64

static buffer_t g_bcache_pool[BCACHE_ENTRIES];
static spinlock_t g_bcache_lock = SPINLOCK_INIT;

void bcache_init(void) {
    spinlock_init(&g_bcache_lock);
    memset(g_bcache_pool, 0, sizeof(g_bcache_pool));
    klog_info("Buffer Cache initialized (%d entries)", BCACHE_ENTRIES);
}

buffer_t *bread(block_device_t *dev, uint32_t block_no, size_t block_size) {
    if (!dev || block_size > BCACHE_MAX_BLOCK_SIZE || block_size == 0)
        return NULL;

    spinlock_acquire(&g_bcache_lock);

    /* 1. Look for existing cached buffer */
    for (size_t i = 0; i < BCACHE_ENTRIES; i++) {
        if (g_bcache_pool[i].valid && g_bcache_pool[i].dev == dev && g_bcache_pool[i].block_no == block_no &&
            g_bcache_pool[i].block_size == block_size) {
            g_bcache_pool[i].refcount++;
            spinlock_release(&g_bcache_lock);
            return &g_bcache_pool[i];
        }
    }

    /* 2. Find a free or victim buffer */
    buffer_t *victim = NULL;
    for (size_t i = 0; i < BCACHE_ENTRIES; i++) {
        if (g_bcache_pool[i].refcount == 0) {
            victim = &g_bcache_pool[i];
            break;
        }
    }

    if (!victim) {
        klog_warn("bcache: Out of free buffers!");
        spinlock_release(&g_bcache_lock);
        return NULL;
    }

    /* 3. If dirty, flush victim to disk */
    if (victim->valid && victim->dirty && victim->dev) {
        uint64_t v_lba = (uint64_t)victim->block_no * (victim->block_size / victim->dev->sector_size);
        uint32_t v_count = (uint32_t)(victim->block_size / victim->dev->sector_size);
        victim->dev->write_blocks(victim->dev, v_lba, v_count, victim->data);
        victim->dirty = false;
    }

    /* 4. Load new block from disk */
    victim->dev = dev;
    victim->block_no = block_no;
    victim->block_size = block_size;
    victim->refcount = 1;
    victim->dirty = false;
    victim->valid = false;

    uint64_t lba = (uint64_t)block_no * (block_size / dev->sector_size);
    uint32_t count = (uint32_t)(block_size / dev->sector_size);

    if (dev->read_blocks(dev, lba, count, victim->data) == 0) {
        victim->valid = true;
    } else {
        victim->valid = false;
        victim->refcount = 0;
        victim = NULL;
    }

    spinlock_release(&g_bcache_lock);
    return victim;
}

int bwrite(buffer_t *buf) {
    if (!buf || !buf->dev)
        return -1;

    spinlock_acquire(&g_bcache_lock);
    uint64_t lba = (uint64_t)buf->block_no * (buf->block_size / buf->dev->sector_size);
    uint32_t count = (uint32_t)(buf->block_size / buf->dev->sector_size);

    int res = buf->dev->write_blocks(buf->dev, lba, count, buf->data);
    if (res == 0) {
        buf->dirty = false;
    }
    spinlock_release(&g_bcache_lock);
    return res;
}

void brelse(buffer_t *buf) {
    if (!buf)
        return;
    spinlock_acquire(&g_bcache_lock);
    if (buf->refcount > 0) {
        buf->refcount--;
    }
    spinlock_release(&g_bcache_lock);
}

void bflush(block_device_t *dev) {
    spinlock_acquire(&g_bcache_lock);
    for (size_t i = 0; i < BCACHE_ENTRIES; i++) {
        if (g_bcache_pool[i].valid && g_bcache_pool[i].dirty && (!dev || g_bcache_pool[i].dev == dev)) {
            uint64_t lba =
                (uint64_t)g_bcache_pool[i].block_no * (g_bcache_pool[i].block_size / g_bcache_pool[i].dev->sector_size);
            uint32_t count = (uint32_t)(g_bcache_pool[i].block_size / g_bcache_pool[i].dev->sector_size);
            g_bcache_pool[i].dev->write_blocks(g_bcache_pool[i].dev, lba, count, g_bcache_pool[i].data);
            g_bcache_pool[i].dirty = false;
        }
    }
    spinlock_release(&g_bcache_lock);
}
