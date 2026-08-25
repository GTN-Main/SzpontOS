/*
 * SzpontOS - CSPRNG Kernel Driver & /dev/urandom Implementation
 * Inspired by FreeBSD sys/dev/random/
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <drivers/random.h>
#include <fs/devfs.h>
#include <fs/vfs.h>
#include <drivers/rtc.h>
#include <arch/x86_64/pit.h>
#include <mm/heap.h>
#include <kernel/spinlock.h>
#include <kernel/kprint.h>
#include <kernel/string.h>

static uint64_t s[4];
static spinlock_t g_random_lock = SPINLOCK_INIT;
static bool g_random_initialized = false;

static inline uint64_t rotl(const uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}

static inline uint64_t rdtsc_pure(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static uint64_t splitmix64(uint64_t *state) {
    uint64_t z = (*state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

void random_add_entropy(uint64_t data) {
    spinlock_acquire(&g_random_lock);
    s[0] ^= data ^ rdtsc_pure();
    s[1] ^= rotl(data, 19);
    s[2] ^= (uint64_t)pit_get_ticks();
    s[3] ^= rotl(data, 37);
    spinlock_release(&g_random_lock);
}

uint64_t random_get_u64(void) {
    spinlock_acquire(&g_random_lock);
    const uint64_t result = rotl(s[1] * 5, 7) * 9;
    const uint64_t t = s[1] << 17;

    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];
    s[2] ^= t;
    s[3] = rotl(s[3], 45);

    /* Mix in continuous TSC entropy */
    s[0] ^= rdtsc_pure();
    spinlock_release(&g_random_lock);
    return result;
}

uint32_t random_get_u32(void) {
    return (uint32_t)random_get_u64();
}

size_t random_get_bytes(void *buf, size_t len) {
    if (!buf || len == 0)
        return 0;
    uint8_t *p = (uint8_t *)buf;
    size_t i = 0;

    while (i + 8 <= len) {
        uint64_t val = random_get_u64();
        memcpy(p + i, &val, 8);
        i += 8;
    }

    if (i < len) {
        uint64_t val = random_get_u64();
        memcpy(p + i, &val, len - i);
        i = len;
    }

    return len;
}

/* =========================================================================
 * DevFS Device Operations for /dev/random & /dev/urandom
 * ========================================================================= */

static ssize_t dev_random_read(vfs_node_t *node, off_t offset, size_t size, void *buffer) {
    (void)node;
    (void)offset;
    return (ssize_t)random_get_bytes(buffer, size);
}

static ssize_t dev_random_write(vfs_node_t *node, off_t offset, size_t size, const void *buffer) {
    (void)node;
    (void)offset;
    if (buffer && size >= 8) {
        uint64_t data;
        memcpy(&data, buffer, sizeof(data));
        random_add_entropy(data);
    }
    return (ssize_t)size;
}

static vfs_ops_t g_random_ops = {.read = dev_random_read, .write = dev_random_write};

void random_init(void) {
    if (g_random_initialized)
        return;

    uint64_t seed = rdtsc_pure() ^ ((uint64_t)rtc_get_current_epoch() << 32) ^ (uint64_t)&g_random_ops;
    s[0] = splitmix64(&seed);
    s[1] = splitmix64(&seed);
    s[2] = splitmix64(&seed);
    s[3] = splitmix64(&seed);

    g_random_initialized = true;

    /* Register devices in /dev */
    vfs_node_t *urandom_node = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
    urandom_node->flags = VFS_TYPE_CHARDEVICE;
    urandom_node->permissions = 0666;
    urandom_node->ops = &g_random_ops;
    devfs_register_device("urandom", urandom_node);

    vfs_node_t *random_node = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
    random_node->flags = VFS_TYPE_CHARDEVICE;
    random_node->permissions = 0666;
    random_node->ops = &g_random_ops;
    devfs_register_device("random", random_node);

    klog_info("CSPRNG: /dev/urandom & /dev/random registered");
}
