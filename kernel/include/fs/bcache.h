#ifndef SZPONTOS_FS_BCACHE_H
#define SZPONTOS_FS_BCACHE_H

#include <kernel/types.h>
#include <drivers/block.h>

#define BCACHE_MAX_BLOCK_SIZE 4096

typedef struct buffer {
    block_device_t *dev;
    uint32_t block_no;
    size_t block_size;
    bool dirty;
    bool valid;
    uint32_t refcount;
    uint8_t data[BCACHE_MAX_BLOCK_SIZE];
} buffer_t;

void bcache_init(void);
buffer_t *bread(block_device_t *dev, uint32_t block_no, size_t block_size);
int bwrite(buffer_t *buf);
void brelse(buffer_t *buf);
void bflush(block_device_t *dev);

#endif /* SZPONTOS_FS_BCACHE_H */
