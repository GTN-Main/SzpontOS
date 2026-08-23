#ifndef SZPONTOS_DRIVERS_BLOCK_H
#define SZPONTOS_DRIVERS_BLOCK_H

#include <kernel/types.h>

#define BLOCK_DEV_NAME_LEN 32

typedef struct block_device {
    char name[BLOCK_DEV_NAME_LEN];
    size_t sector_size;      /* Standard 512 bytes */
    uint64_t sector_count;    /* Total sectors on device */
    void *driver_data;        /* Driver private context */

    int (*read_blocks)(struct block_device *dev, uint64_t lba, uint32_t count, void *buffer);
    int (*write_blocks)(struct block_device *dev, uint64_t lba, uint32_t count, const void *buffer);
} block_device_t;

void block_device_init(void);
int block_device_register(block_device_t *dev);
block_device_t *block_device_get(const char *name);
size_t block_device_get_count(void);
block_device_t *block_device_get_by_index(size_t index);

#endif /* SZPONTOS_DRIVERS_BLOCK_H */
