#include <fs/bcache.h>
#include <mm/heap.h>
#include <kernel/string.h>
#include <kernel/kprint.h>
#include <kernel/spinlock.h>

#define BCACHE_ENTRIES 512
#define BCACHE_HASH_BUCKETS 256
#define BCACHE_HASH_MASK (BCACHE_HASH_BUCKETS - 1)

static buffer_t g_bcache_pool[BCACHE_ENTRIES];
static buffer_t *g_bcache_hash[BCACHE_HASH_BUCKETS];
static buffer_t *g_lru_head = NULL; /* Oldest (Least Recently Used) */
static buffer_t *g_lru_tail = NULL; /* Newest (Most Recently Used) */
static spinlock_t g_bcache_lock = SPINLOCK_INIT;

static inline uint32_t bcache_calc_hash(block_device_t *dev, uint32_t block_no) {
    uintptr_t d = (uintptr_t)dev;
    return (uint32_t)(((d >> 4) ^ block_no ^ (block_no >> 8)) & BCACHE_HASH_MASK);
}

static void lru_unlink(buffer_t *buf) {
    if (!buf)
        return;
    if (buf->lru_prev) {
        buf->lru_prev->lru_next = buf->lru_next;
    } else {
        g_lru_head = buf->lru_next;
    }
    if (buf->lru_next) {
        buf->lru_next->lru_prev = buf->lru_prev;
    } else {
        g_lru_tail = buf->lru_prev;
    }
    buf->lru_next = NULL;
    buf->lru_prev = NULL;
}

static void lru_insert_tail(buffer_t *buf) {
    if (!buf)
        return;
    buf->lru_prev = g_lru_tail;
    buf->lru_next = NULL;
    if (g_lru_tail) {
        g_lru_tail->lru_next = buf;
    } else {
        g_lru_head = buf;
    }
    g_lru_tail = buf;
}

static void lru_touch(buffer_t *buf) {
    lru_unlink(buf);
    lru_insert_tail(buf);
}

static void hash_insert(buffer_t *buf) {
    if (!buf || !buf->dev)
        return;
    uint32_t h = bcache_calc_hash(buf->dev, buf->block_no);
    buf->hash_next = g_bcache_hash[h];
    buf->hash_prev = NULL;
    if (g_bcache_hash[h]) {
        g_bcache_hash[h]->hash_prev = buf;
    }
    g_bcache_hash[h] = buf;
}

static void hash_unlink(buffer_t *buf) {
    if (!buf || !buf->dev)
        return;
    uint32_t h = bcache_calc_hash(buf->dev, buf->block_no);
    if (buf->hash_prev) {
        buf->hash_prev->hash_next = buf->hash_next;
    } else if (g_bcache_hash[h] == buf) {
        g_bcache_hash[h] = buf->hash_next;
    }
    if (buf->hash_next) {
        buf->hash_next->hash_prev = buf->hash_prev;
    }
    buf->hash_next = NULL;
    buf->hash_prev = NULL;
}

void bcache_init(void) {
    spinlock_init(&g_bcache_lock);
    memset(g_bcache_pool, 0, sizeof(g_bcache_pool));
    memset(g_bcache_hash, 0, sizeof(g_bcache_hash));

    g_lru_head = NULL;
    g_lru_tail = NULL;

    /* Initialize LRU list linking all entries in initial order */
    for (size_t i = 0; i < BCACHE_ENTRIES; i++) {
        buffer_t *buf = &g_bcache_pool[i];
        lru_insert_tail(buf);
    }

    klog_info("Buffer Cache initialized (%d entries, %d hash buckets, LRU eviction)", BCACHE_ENTRIES,
              BCACHE_HASH_BUCKETS);
}

buffer_t *bread(block_device_t *dev, uint32_t block_no, size_t block_size) {
    if (!dev || block_size > BCACHE_MAX_BLOCK_SIZE || block_size == 0 || dev->sector_size == 0)
        return NULL;

    spinlock_acquire(&g_bcache_lock);

    /* 1. Fast O(1) hash table lookup */
    uint32_t h = bcache_calc_hash(dev, block_no);
    buffer_t *buf = g_bcache_hash[h];
    while (buf) {
        if (buf->valid && buf->dev == dev && buf->block_no == block_no && buf->block_size == block_size) {
            buf->refcount++;
            lru_touch(buf);
            spinlock_release(&g_bcache_lock);
            return buf;
        }
        buf = buf->hash_next;
    }

    /* 2. Cache miss: find an unreferenced victim starting from least recently used (g_lru_head) */
    buffer_t *victim = g_lru_head;
    while (victim) {
        if (victim->refcount == 0)
            break;
        victim = victim->lru_next;
    }

    if (!victim) {
        klog_warn("bcache: Out of free buffers!");
        spinlock_release(&g_bcache_lock);
        return NULL;
    }

    /* 3. If victim held valid data, write back if dirty and unlink from its hash bucket */
    if (victim->valid && victim->dev) {
        if (victim->dirty && victim->dev->write_blocks) {
            uint64_t v_lba = (uint64_t)victim->block_no * (victim->block_size / victim->dev->sector_size);
            uint32_t v_count = (uint32_t)(victim->block_size / victim->dev->sector_size);
            victim->dev->write_blocks(victim->dev, v_lba, v_count, victim->data);
            victim->dirty = false;
        }
        hash_unlink(victim);
    }

    /* 4. Configure buffer for new block and register in hash table and LRU tail */
    victim->dev = dev;
    victim->block_no = block_no;
    victim->block_size = block_size;
    victim->refcount = 1;
    victim->dirty = false;
    victim->valid = false;

    hash_insert(victim);
    lru_touch(victim);

    uint64_t lba = (uint64_t)block_no * (block_size / dev->sector_size);
    uint32_t count = (uint32_t)(block_size / dev->sector_size);

    if (dev->read_blocks(dev, lba, count, victim->data) == 0) {
        victim->valid = true;
    } else {
        victim->valid = false;
        victim->refcount = 0;
        hash_unlink(victim);
        victim = NULL;
    }

    spinlock_release(&g_bcache_lock);
    return victim;
}

int bwrite(buffer_t *buf) {
    if (!buf || !buf->dev || !buf->dev->write_blocks || buf->dev->sector_size == 0)
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
        buffer_t *buf = &g_bcache_pool[i];
        if (buf->valid && buf->dirty && (!dev || buf->dev == dev) && buf->dev && buf->dev->write_blocks &&
            buf->dev->sector_size > 0) {
            uint64_t lba = (uint64_t)buf->block_no * (buf->block_size / buf->dev->sector_size);
            uint32_t count = (uint32_t)(buf->block_size / buf->dev->sector_size);
            buf->dev->write_blocks(buf->dev, lba, count, buf->data);
            buf->dirty = false;
        }
    }
    spinlock_release(&g_bcache_lock);
}
