#include <drivers/ata.h>
#include <fs/devfs.h>
#include <arch/x86_64/io.h>
#include <mm/heap.h>
#include <kernel/string.h>
#include <kernel/kprint.h>
#include <kernel/spinlock.h>

#define MAX_BLOCK_DEVS 16
static block_device_t *g_block_devices[MAX_BLOCK_DEVS];
static size_t g_block_dev_count = 0;
static spinlock_t g_block_dev_lock = SPINLOCK_INIT;

static ata_drive_t g_ata_drives[4];
static spinlock_t g_ata_lock = SPINLOCK_INIT;

void block_device_init(void) {
    spinlock_init(&g_block_dev_lock);
    memset(g_block_devices, 0, sizeof(g_block_devices));
    g_block_dev_count = 0;
}

int block_device_register(block_device_t *dev) {
    if (!dev)
        return -1;
    spinlock_acquire(&g_block_dev_lock);
    if (g_block_dev_count >= MAX_BLOCK_DEVS) {
        spinlock_release(&g_block_dev_lock);
        return -1;
    }
    g_block_devices[g_block_dev_count++] = dev;
    klog_info("BlockDev: Registered '%s' (%lu sectors, %lu MiB)", dev->name, dev->sector_count,
              (dev->sector_count * dev->sector_size) / (1024 * 1024));
    spinlock_release(&g_block_dev_lock);
    return 0;
}

block_device_t *block_device_get(const char *name) {
    if (!name)
        return NULL;
    for (size_t i = 0; i < g_block_dev_count; i++) {
        if (g_block_devices[i] && strcmp(g_block_devices[i]->name, name) == 0) {
            return g_block_devices[i];
        }
    }
    return NULL;
}

size_t block_device_get_count(void) {
    return g_block_dev_count;
}

block_device_t *block_device_get_by_index(size_t index) {
    if (index >= g_block_dev_count)
        return NULL;
    return g_block_devices[index];
}

/* 400ns delay on ATA bus */
static void ata_delay(uint16_t io_base) {
    inb(io_base + ATA_REG_STATUS);
    inb(io_base + ATA_REG_STATUS);
    inb(io_base + ATA_REG_STATUS);
    inb(io_base + ATA_REG_STATUS);
}

static int ata_wait_ready(uint16_t io_base) {
    for (int timeout = 0; timeout < 100000; timeout++) {
        uint8_t status = inb(io_base + ATA_REG_STATUS);
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) {
            return 0;
        }
        if (status & ATA_SR_ERR) {
            return -1;
        }
    }
    return -1;
}

static int ata_wait_not_busy(uint16_t io_base) {
    for (int timeout = 0; timeout < 100000; timeout++) {
        uint8_t status = inb(io_base + ATA_REG_STATUS);
        if (!(status & ATA_SR_BSY)) {
            return 0;
        }
    }
    return -1;
}

static int ata_block_dev_read(block_device_t *dev, uint64_t lba, uint32_t count, void *buf) {
    ata_drive_t *drive = (ata_drive_t *)dev->driver_data;
    if (!drive || !buf)
        return -1;
    return ata_read_sectors(drive, (uint32_t)lba, (uint8_t)count, buf);
}

static int ata_block_dev_write(block_device_t *dev, uint64_t lba, uint32_t count, const void *buf) {
    ata_drive_t *drive = (ata_drive_t *)dev->driver_data;
    if (!drive || !buf)
        return -1;
    return ata_write_sectors(drive, (uint32_t)lba, (uint8_t)count, buf);
}

static int ata_identify(ata_drive_t *drive) {
    uint16_t io = drive->io_base;
    uint8_t drive_select = drive->is_slave ? 0xB0 : 0xA0;

    outb(io + ATA_REG_DRIVE_SEL, drive_select);
    ata_delay(io);

    outb(io + ATA_REG_SECCOUNT, 0);
    outb(io + ATA_REG_LBA_LO, 0);
    outb(io + ATA_REG_LBA_MID, 0);
    outb(io + ATA_REG_LBA_HI, 0);

    outb(io + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    ata_delay(io);

    uint8_t status = inb(io + ATA_REG_STATUS);
    if (status == 0) {
        return -1; /* Drive does not exist */
    }

    if (ata_wait_not_busy(io) != 0) {
        return -1;
    }

    uint8_t lba_mid = inb(io + ATA_REG_LBA_MID);
    uint8_t lba_hi = inb(io + ATA_REG_LBA_HI);
    if (lba_mid == 0x14 && lba_hi == 0xEB) {
        /* ATAPI Device (e.g. CD-ROM) */
        return -1;
    }

    if (ata_wait_ready(io) != 0) {
        return -1;
    }

    uint16_t info[256];
    for (int i = 0; i < 256; i++) {
        info[i] = inw(io + ATA_REG_DATA);
    }

    /* Model string (words 27..46) with byte-swapping */
    for (int i = 0; i < 20; i++) {
        drive->model[i * 2] = (char)(info[27 + i] >> 8);
        drive->model[i * 2 + 1] = (char)(info[27 + i] & 0xFF);
    }
    drive->model[40] = '\0';

    /* Trim trailing spaces from model */
    for (int i = 39; i >= 0 && drive->model[i] == ' '; i--) {
        drive->model[i] = '\0';
    }

    /* Total 28-bit LBA sectors (words 60-61) */
    drive->sectors = ((uint32_t)info[61] << 16) | info[60];
    if (drive->sectors == 0) {
        return -1;
    }

    drive->present = true;
    return 0;
}

int ata_read_sectors(ata_drive_t *drive, uint32_t lba, uint8_t count, void *buf) {
    if (!drive || !drive->present || !buf || count == 0)
        return -1;

    spinlock_acquire(&g_ata_lock);
    uint16_t io = drive->io_base;

    if (ata_wait_not_busy(io) != 0) {
        spinlock_release(&g_ata_lock);
        return -1;
    }

    uint8_t drive_select = 0xE0 | (drive->is_slave ? 0x10 : 0x00) | ((lba >> 24) & 0x0F);
    outb(io + ATA_REG_DRIVE_SEL, drive_select);
    ata_delay(io);

    outb(io + ATA_REG_FEATURES, 0x00);
    outb(io + ATA_REG_SECCOUNT, count);
    outb(io + ATA_REG_LBA_LO, (uint8_t)(lba & 0xFF));
    outb(io + ATA_REG_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(io + ATA_REG_LBA_HI, (uint8_t)((lba >> 16) & 0xFF));

    outb(io + ATA_REG_COMMAND, ATA_CMD_READ_SECTORS);

    uint16_t *ptr = (uint16_t *)buf;
    for (int s = 0; s < count; s++) {
        if (ata_wait_ready(io) != 0) {
            spinlock_release(&g_ata_lock);
            return -1;
        }

        for (int i = 0; i < 256; i++) {
            *ptr++ = inw(io + ATA_REG_DATA);
        }
    }

    spinlock_release(&g_ata_lock);
    return 0;
}

int ata_write_sectors(ata_drive_t *drive, uint32_t lba, uint8_t count, const void *buf) {
    if (!drive || !drive->present || !buf || count == 0)
        return -1;

    spinlock_acquire(&g_ata_lock);
    uint16_t io = drive->io_base;

    if (ata_wait_not_busy(io) != 0) {
        spinlock_release(&g_ata_lock);
        return -1;
    }

    uint8_t drive_select = 0xE0 | (drive->is_slave ? 0x10 : 0x00) | ((lba >> 24) & 0x0F);
    outb(io + ATA_REG_DRIVE_SEL, drive_select);
    ata_delay(io);

    outb(io + ATA_REG_FEATURES, 0x00);
    outb(io + ATA_REG_SECCOUNT, count);
    outb(io + ATA_REG_LBA_LO, (uint8_t)(lba & 0xFF));
    outb(io + ATA_REG_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(io + ATA_REG_LBA_HI, (uint8_t)((lba >> 16) & 0xFF));

    outb(io + ATA_REG_COMMAND, ATA_CMD_WRITE_SECTORS);

    const uint16_t *ptr = (const uint16_t *)buf;
    for (int s = 0; s < count; s++) {
        if (ata_wait_ready(io) != 0) {
            spinlock_release(&g_ata_lock);
            return -1;
        }

        for (int i = 0; i < 256; i++) {
            outw(io + ATA_REG_DATA, *ptr++);
        }
    }

    /* Cache flush */
    outb(io + ATA_REG_COMMAND, 0xE7);
    ata_wait_not_busy(io);

    spinlock_release(&g_ata_lock);
    return 0;
}

void ata_init(void) {
    block_device_init();
    spinlock_init(&g_ata_lock);

    struct {
        uint16_t io_base;
        uint16_t ctrl_base;
        bool is_slave;
        const char *name;
    } configs[4] = {
        {ATA_PRIMARY_IO, ATA_PRIMARY_CTRL, false, "hda"},
        {ATA_PRIMARY_IO, ATA_PRIMARY_CTRL, true, "hdb"},
        {ATA_SECONDARY_IO, ATA_SECONDARY_CTRL, false, "hdc"},
        {ATA_SECONDARY_IO, ATA_SECONDARY_CTRL, true, "hdd"},
    };

    for (int i = 0; i < 4; i++) {
        g_ata_drives[i].present = false;
        g_ata_drives[i].io_base = configs[i].io_base;
        g_ata_drives[i].ctrl_base = configs[i].ctrl_base;
        g_ata_drives[i].is_slave = configs[i].is_slave;

        if (ata_identify(&g_ata_drives[i]) == 0) {
            klog_info("ATA: Found drive '/dev/%s' (Model: %s, Sectors: %lu, Size: %lu MiB)", configs[i].name,
                      g_ata_drives[i].model, g_ata_drives[i].sectors, (g_ata_drives[i].sectors * 512) / (1024 * 1024));

            strncpy(g_ata_drives[i].block_dev.name, configs[i].name, BLOCK_DEV_NAME_LEN - 1);
            g_ata_drives[i].block_dev.sector_size = 512;
            g_ata_drives[i].block_dev.sector_count = g_ata_drives[i].sectors;
            g_ata_drives[i].block_dev.driver_data = &g_ata_drives[i];
            g_ata_drives[i].block_dev.read_blocks = ata_block_dev_read;
            g_ata_drives[i].block_dev.write_blocks = ata_block_dev_write;

            block_device_register(&g_ata_drives[i].block_dev);
            devfs_register_block_device(&g_ata_drives[i].block_dev);
        }
    }
}
