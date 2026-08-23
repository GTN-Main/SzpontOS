#ifndef SZPONTOS_DRIVERS_ATA_H
#define SZPONTOS_DRIVERS_ATA_H

#include <kernel/types.h>
#include <drivers/block.h>

/* Primary & Secondary Channel I/O Ports */
#define ATA_PRIMARY_IO      0x1F0
#define ATA_PRIMARY_CTRL    0x3F6
#define ATA_SECONDARY_IO    0x170
#define ATA_SECONDARY_CTRL  0x376

/* ATA Registers (Offsets from Base I/O) */
#define ATA_REG_DATA        0x00
#define ATA_REG_ERROR       0x01
#define ATA_REG_FEATURES    0x01
#define ATA_REG_SECCOUNT    0x02
#define ATA_REG_LBA_LO      0x03
#define ATA_REG_LBA_MID     0x04
#define ATA_REG_LBA_HI      0x05
#define ATA_REG_DRIVE_SEL   0x06
#define ATA_REG_STATUS      0x07
#define ATA_REG_COMMAND     0x07

/* ATA Status Register Bits */
#define ATA_SR_ERR          0x01    /* Error */
#define ATA_SR_IDX          0x02    /* Index */
#define ATA_SR_CORR         0x04    /* Corrected data */
#define ATA_SR_DRQ          0x08    /* Data Request */
#define ATA_SR_DSC          0x10    /* Drive Seek Complete */
#define ATA_SR_DF           0x20    /* Drive Fault */
#define ATA_SR_DRDY         0x40    /* Drive Ready */
#define ATA_SR_BSY          0x80    /* Busy */

/* ATA Commands */
#define ATA_CMD_READ_SECTORS  0x20
#define ATA_CMD_WRITE_SECTORS 0x30
#define ATA_CMD_IDENTIFY      0xEC

typedef struct ata_drive {
    bool present;
    bool is_slave;
    uint16_t io_base;
    uint16_t ctrl_base;
    uint64_t sectors;
    char model[41];
    block_device_t block_dev;
} ata_drive_t;

void ata_init(void);
int ata_read_sectors(ata_drive_t *drive, uint32_t lba, uint8_t count, void *buf);
int ata_write_sectors(ata_drive_t *drive, uint32_t lba, uint8_t count, const void *buf);

#endif /* SZPONTOS_DRIVERS_ATA_H */
