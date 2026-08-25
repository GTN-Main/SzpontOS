/*
 * SzpontOS - AHCI (Advanced Host Controller Interface) SATA Driver
 * Modern Bare Metal SATA Storage Subsystem
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <drivers/ahci.h>
#include <drivers/pci.h>
#include <drivers/block.h>
#include <mm/vmm.h>
#include <mm/heap.h>
#include <mm/pmm.h>
#include <arch/x86_64/io.h>
#include <kernel/kprint.h>
#include <kernel/string.h>

#define AHCI_PCI_CLASS_STORAGE 0x01
#define AHCI_PCI_SUBCLASS_SATA 0x06
#define AHCI_PCI_PROGIF_AHCI 0x01

#define HBA_GHC_AE (1U << 31) /* AHCI Enable */
#define HBA_GHC_HR (1U << 0)  /* HBA Reset */

#define HBA_PORT_CMD_ST (1U << 0)  /* Start */
#define HBA_PORT_CMD_FRE (1U << 4) /* FIS Receive Enable */
#define HBA_PORT_CMD_FR (1U << 14) /* FIS Receive Running */
#define HBA_PORT_CMD_CR (1U << 15) /* Command List Running */

#define HBA_PORT_TFD_ERR (1U << 0)
#define HBA_PORT_TFD_DRQ (1U << 3)
#define HBA_PORT_TFD_BSY (1U << 7)

#define SATA_SIG_ATA 0x00000101   /* SATA drive */
#define SATA_SIG_ATAPI 0xEB140101 /* SATAPI optical drive */
#define SATA_SIG_SEMB 0xC33C0101  /* Enclosure management */
#define SATA_SIG_PM 0x96690101    /* Port multiplier */

#define FIS_TYPE_REG_H2D 0x27

#define ATA_CMD_READ_DMA_EXT 0x25
#define ATA_CMD_WRITE_DMA_EXT 0x35
#define ATA_CMD_IDENTIFY 0xEC

/* AHCI Port Memory Structure */
typedef struct hba_port {
    volatile uint32_t clb;
    volatile uint32_t clbu;
    volatile uint32_t fb;
    volatile uint32_t fbu;
    volatile uint32_t is;
    volatile uint32_t ie;
    volatile uint32_t cmd;
    volatile uint32_t reserved0;
    volatile uint32_t tfd;
    volatile uint32_t sig;
    volatile uint32_t ssts;
    volatile uint32_t sctl;
    volatile uint32_t serr;
    volatile uint32_t sact;
    volatile uint32_t ci;
    volatile uint32_t sntf;
    volatile uint32_t fbs;
    volatile uint32_t reserved1[11];
    volatile uint32_t vendor[4];
} __attribute__((packed)) hba_port_t;

/* Generic Host Control Register Map */
typedef struct hba_mem {
    volatile uint32_t cap;
    volatile uint32_t ghc;
    volatile uint32_t is;
    volatile uint32_t pi;
    volatile uint32_t vs;
    volatile uint32_t ccc_ctl;
    volatile uint32_t ccc_pts;
    volatile uint32_t em_loc;
    volatile uint32_t em_ctl;
    volatile uint32_t cap2;
    volatile uint32_t bohc;
    volatile uint8_t reserved[0xA0 - 0x2C];
    volatile uint8_t vendor[0x100 - 0xA0];
    hba_port_t ports[32];
} __attribute__((packed)) hba_mem_t;

/* Command Header */
typedef struct hba_cmd_header {
    uint8_t cfl : 5; /* Command FIS length in DWORDS, 2-16 */
    uint8_t a : 1;   /* ATAPI */
    uint8_t w : 1;   /* Write, 1: H2D, 0: D2H */
    uint8_t p : 1;   /* Prefetchable */
    uint8_t r : 1;   /* Reset */
    uint8_t b : 1;   /* BIST */
    uint8_t c : 1;   /* Clear busy upon R_OK */
    uint8_t reserved0 : 1;
    uint8_t pmp : 4;         /* Port multiplier port */
    uint16_t prdtl;          /* Physical Region Descriptor Table length in entries */
    volatile uint32_t prdbc; /* PRD Byte Count transferred */
    uint32_t ctba;           /* Command Table Descriptor Base Address */
    uint32_t ctbau;          /* Command Table Descriptor Base Address Upper */
    uint32_t reserved1[4];
} __attribute__((packed)) hba_cmd_header_t;

/* PRDT Entry */
typedef struct hba_prdt_entry {
    uint32_t dba;  /* Data Base Address */
    uint32_t dbau; /* Data Base Address Upper */
    uint32_t reserved0;
    uint32_t dbc : 22; /* Byte count (0-indexed, up to 4MB) */
    uint32_t reserved1 : 9;
    uint32_t i : 1; /* Interrupt on completion */
} __attribute__((packed)) hba_prdt_entry_t;

/* Command FIS Structure */
typedef struct fis_reg_h2d {
    uint8_t fis_type;   /* FIS_TYPE_REG_H2D */
    uint8_t pmport : 4; /* Port multiplier */
    uint8_t reserved0 : 3;
    uint8_t c : 1;    /* 1: Command, 0: Control */
    uint8_t command;  /* ATA Command */
    uint8_t featurel; /* Feature Low */
    uint8_t lba0;     /* LBA Low */
    uint8_t lba1;     /* LBA Mid */
    uint8_t lba2;     /* LBA High */
    uint8_t device;   /* Device Register */
    uint8_t lba3;     /* LBA3 */
    uint8_t lba4;     /* LBA4 */
    uint8_t lba5;     /* LBA5 */
    uint8_t featureh; /* Feature High */
    uint8_t countl;   /* Count Low */
    uint8_t counth;   /* Count High */
    uint8_t icc;      /* Isochronous Command Completion */
    uint8_t control;  /* Control Register */
    uint8_t reserved1[4];
} __attribute__((packed)) fis_reg_h2d_t;

/* Command Table */
typedef struct hba_cmd_tbl {
    uint8_t cfis[64];
    uint8_t acmd[16];
    uint8_t reserved[48];
    hba_prdt_entry_t prdt_entries[1];
} __attribute__((packed)) hba_cmd_tbl_t;

typedef struct ahci_device {
    hba_port_t *port;
    int port_num;
    uint64_t sector_count;
    char name[16];
    hba_cmd_header_t *cmd_headers;
    uintptr_t cmd_headers_phys;
    hba_cmd_tbl_t *cmd_tbl;
    uintptr_t cmd_tbl_phys;
    void *received_fis;
    uintptr_t received_fis_phys;
} ahci_device_t;

#define MAX_AHCI_DEVICES 8
static ahci_device_t g_sata_devices[MAX_AHCI_DEVICES];
static size_t g_sata_dev_count = 0;
static bool g_ahci_active = false;

static void *alloc_dma_zero(size_t bytes, uintptr_t *out_phys) {
    size_t pages = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    uintptr_t phys = pmm_alloc_pages(pages);
    if (!phys)
        return NULL;

    void *virt = PHYS_TO_VIRT(phys);
    memset(virt, 0, pages * PAGE_SIZE);
    if (out_phys)
        *out_phys = phys;
    return virt;
}

static void port_stop_cmd(hba_port_t *port) {
    port->cmd &= ~HBA_PORT_CMD_ST;
    port->cmd &= ~HBA_PORT_CMD_FRE;

    int timeout = 50000;
    while (--timeout) {
        if (port->cmd & HBA_PORT_CMD_FR)
            continue;
        if (port->cmd & HBA_PORT_CMD_CR)
            continue;
        break;
    }
}

static void port_start_cmd(hba_port_t *port) {
    while (port->cmd & HBA_PORT_CMD_CR)
        ;
    port->cmd |= HBA_PORT_CMD_FRE;
    port->cmd |= HBA_PORT_CMD_ST;
}

static int ahci_port_read_write_sectors(ahci_device_t *dev, uint64_t lba, uint32_t count, void *buf, bool is_write) {
    hba_port_t *port = dev->port;
    port->is = (uint32_t)-1; /* Clear interrupt status */

    /* Setup Command Header (Slot 0) */
    hba_cmd_header_t *cmd_hdr = &dev->cmd_headers[0];
    cmd_hdr->cfl = sizeof(fis_reg_h2d_t) / sizeof(uint32_t);
    cmd_hdr->w = is_write ? 1 : 0;
    cmd_hdr->prdtl = 1;
    cmd_hdr->ctba = (uint32_t)(dev->cmd_tbl_phys & 0xFFFFFFFF);
    cmd_hdr->ctbau = (uint32_t)((dev->cmd_tbl_phys >> 32) & 0xFFFFFFFF);

    /* Allocate or map DMA buffer for the transfer */
    uintptr_t buf_phys = 0;
    void *dma_buf = alloc_dma_zero(count * 512, &buf_phys);
    if (!dma_buf)
        return -1;

    if (is_write) {
        memcpy(dma_buf, buf, count * 512);
    }

    /* Setup PRDT Entry 0 */
    hba_prdt_entry_t *prdt = &dev->cmd_tbl->prdt_entries[0];
    prdt->dba = (uint32_t)(buf_phys & 0xFFFFFFFF);
    prdt->dbau = (uint32_t)((buf_phys >> 32) & 0xFFFFFFFF);
    prdt->dbc = (count * 512) - 1; /* 512 bytes per sector, 0-indexed */
    prdt->i = 1;

    /* Setup Command FIS */
    fis_reg_h2d_t *cmdfis = (fis_reg_h2d_t *)dev->cmd_tbl->cfis;
    memset(cmdfis, 0, sizeof(fis_reg_h2d_t));
    cmdfis->fis_type = FIS_TYPE_REG_H2D;
    cmdfis->c = 1; /* Command */
    cmdfis->command = is_write ? ATA_CMD_WRITE_DMA_EXT : ATA_CMD_READ_DMA_EXT;

    cmdfis->lba0 = (uint8_t)lba;
    cmdfis->lba1 = (uint8_t)(lba >> 8);
    cmdfis->lba2 = (uint8_t)(lba >> 16);
    cmdfis->device = 1 << 6; /* LBA mode */

    cmdfis->lba3 = (uint8_t)(lba >> 24);
    cmdfis->lba4 = (uint8_t)(lba >> 32);
    cmdfis->lba5 = (uint8_t)(lba >> 40);

    cmdfis->countl = (uint8_t)(count & 0xFF);
    cmdfis->counth = (uint8_t)((count >> 8) & 0xFF);

    /* Issue Command to Slot 0 */
    port->ci = 1 << 0;

    /* Wait for completion */
    int timeout = 1000000;
    while (--timeout) {
        if ((port->ci & (1 << 0)) == 0) {
            break;
        }
        if (port->tfd & HBA_PORT_TFD_ERR) {
            klog_error("AHCI: Disk error on port %d (TFD: 0x%08x)", dev->port_num, port->tfd);
            pmm_free_pages(buf_phys, (count * 512 + PAGE_SIZE - 1) / PAGE_SIZE);
            return -1;
        }
        io_wait();
    }

    if (timeout == 0) {
        klog_error("AHCI: Command timed out on port %d", dev->port_num);
        pmm_free_pages(buf_phys, (count * 512 + PAGE_SIZE - 1) / PAGE_SIZE);
        return -1;
    }

    if (!is_write) {
        memcpy(buf, dma_buf, count * 512);
    }

    pmm_free_pages(buf_phys, (count * 512 + PAGE_SIZE - 1) / PAGE_SIZE);
    return 0;
}

static int ahci_block_read(block_device_t *bdev, uint64_t sector, uint32_t count, void *buf) {
    ahci_device_t *dev = (ahci_device_t *)bdev->driver_data;
    if (!dev)
        return -1;
    return ahci_port_read_write_sectors(dev, sector, count, buf, false);
}

static int ahci_block_write(block_device_t *bdev, uint64_t sector, uint32_t count, const void *buf) {
    ahci_device_t *dev = (ahci_device_t *)bdev->driver_data;
    if (!dev)
        return -1;
    return ahci_port_read_write_sectors(dev, sector, count, (void *)buf, true);
}

static void ahci_init_port(hba_mem_t *abar, int port_num) {
    hba_port_t *port = &abar->ports[port_num];

    uint32_t ssts = port->ssts;
    uint8_t ipm = (ssts >> 8) & 0x0F;
    uint8_t det = ssts & 0x0F;

    if (det != 3 || ipm != 1) {
        return; /* No active SATA PHY connection */
    }

    uint32_t sig = port->sig;
    if (sig != SATA_SIG_ATA) {
        /* Not an ATA Hard Disk (e.g. ATAPI CDROM or SEMB) */
        return;
    }

    klog_info("AHCI: SATA hard disk found on Port %d (DET: 0x%x, SIG: 0x%08x)", port_num, det, sig);

    port_stop_cmd(port);

    ahci_device_t *dev = &g_sata_devices[g_sata_dev_count];
    memset(dev, 0, sizeof(ahci_device_t));
    dev->port = port;
    dev->port_num = port_num;

    /* 1. Allocate Command List (1 KiB, 32 entries) */
    dev->cmd_headers = (hba_cmd_header_t *)alloc_dma_zero(sizeof(hba_cmd_header_t) * 32, &dev->cmd_headers_phys);
    port->clb = (uint32_t)(dev->cmd_headers_phys & 0xFFFFFFFF);
    port->clbu = (uint32_t)((dev->cmd_headers_phys >> 32) & 0xFFFFFFFF);

    /* 2. Allocate Received FIS (256 bytes) */
    dev->received_fis = alloc_dma_zero(256, &dev->received_fis_phys);
    port->fb = (uint32_t)(dev->received_fis_phys & 0xFFFFFFFF);
    port->fbu = (uint32_t)((dev->received_fis_phys >> 32) & 0xFFFFFFFF);

    /* 3. Allocate Command Table for Slot 0 */
    dev->cmd_tbl = (hba_cmd_tbl_t *)alloc_dma_zero(sizeof(hba_cmd_tbl_t), &dev->cmd_tbl_phys);

    port_start_cmd(port);

    /* Assign disk name (sda, sdb, sdc, ...) */
    dev->name[0] = 's';
    dev->name[1] = 'd';
    dev->name[2] = (char)('a' + g_sata_dev_count);
    dev->name[3] = '\0';

    dev->sector_count = 2097152; /* Default 1 GiB capacity if not identified */

    /* Register with Block Device Layer */
    block_device_t *bdev = (block_device_t *)kmalloc(sizeof(block_device_t));
    if (bdev) {
        memset(bdev, 0, sizeof(block_device_t));
        strncpy(bdev->name, dev->name, sizeof(bdev->name) - 1);
        bdev->sector_size = 512;
        bdev->sector_count = dev->sector_count;
        bdev->driver_data = dev;
        bdev->read_blocks = ahci_block_read;
        bdev->write_blocks = ahci_block_write;
        block_device_register(bdev);
    }

    klog_info("AHCI: Registered SATA block device '/dev/%s' (%lu MiB)", dev->name,
              (dev->sector_count * 512) / (1024 * 1024));

    g_sata_dev_count++;
}

void ahci_init(void) {
    pci_device_t *pci_dev = pci_find_class(AHCI_PCI_CLASS_STORAGE, AHCI_PCI_SUBCLASS_SATA);
    if (!pci_dev) {
        klog_info("AHCI: No SATA AHCI controller found on PCI bus");
        return;
    }

    pci_enable_bus_mastering(pci_dev);

    uintptr_t abar_phys = pci_dev->bar[5] & ~0xFFF;
    if (!abar_phys) {
        klog_warn("AHCI: Controller BAR5 (ABAR) is invalid");
        return;
    }

    /* Map AHCI MMIO Space */
    for (size_t p = 0; p < 8; p++) {
        uintptr_t v = (uintptr_t)PHYS_TO_VIRT(abar_phys + p * PAGE_SIZE);
        vmm_map_page(&g_kernel_pagemap, v, abar_phys + p * PAGE_SIZE,
                     VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | VMM_FLAG_CACHE_DISABLE);
    }

    hba_mem_t *abar = (hba_mem_t *)PHYS_TO_VIRT(abar_phys);

    /* Enable AHCI Mode in Generic Host Control */
    abar->ghc |= HBA_GHC_AE;

    uint32_t pi = abar->pi;
    uint32_t vs = abar->vs;
    klog_info("AHCI: AHCI Controller v%x.%x initialized (Ports Implemented: 0x%08x)", (vs >> 16), vs & 0xFFFF, pi);

    /* Enumerate SATA Ports */
    for (int p = 0; p < 32; p++) {
        if (pi & (1 << p)) {
            ahci_init_port(abar, p);
        }
    }

    if (g_sata_dev_count > 0) {
        g_ahci_active = true;
    }
}

bool ahci_is_active(void) {
    return g_ahci_active;
}
