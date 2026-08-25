/*
 * SzpontOS - PCI Bus Enumerator & Device Manager
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <drivers/pci.h>
#include <arch/x86_64/io.h>
#include <mm/heap.h>
#include <kernel/kprint.h>
#include <kernel/string.h>

static pci_device_t *g_pci_devices = NULL;

static uint32_t pci_config_addr(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    return (uint32_t)((1U << 31) | ((uint32_t)bus << 16) | ((uint32_t)(slot & 0x1F) << 11) |
                      ((uint32_t)(func & 0x07) << 8) | (offset & 0xFC));
}

uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    outl(PCI_CONFIG_ADDRESS, pci_config_addr(bus, slot, func, offset));
    return inl(PCI_CONFIG_DATA);
}

uint16_t pci_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    outl(PCI_CONFIG_ADDRESS, pci_config_addr(bus, slot, func, offset));
    return (uint16_t)((inl(PCI_CONFIG_DATA) >> ((offset & 2) * 8)) & 0xFFFF);
}

uint8_t pci_read8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    outl(PCI_CONFIG_ADDRESS, pci_config_addr(bus, slot, func, offset));
    return (uint8_t)((inl(PCI_CONFIG_DATA) >> ((offset & 3) * 8)) & 0xFF);
}

void pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t val) {
    outl(PCI_CONFIG_ADDRESS, pci_config_addr(bus, slot, func, offset));
    outl(PCI_CONFIG_DATA, val);
}

void pci_write16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t val) {
    outl(PCI_CONFIG_ADDRESS, pci_config_addr(bus, slot, func, offset));
    outw((uint16_t)(PCI_CONFIG_DATA + (offset & 2)), val);
}

void pci_write8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t val) {
    outl(PCI_CONFIG_ADDRESS, pci_config_addr(bus, slot, func, offset));
    outb((uint16_t)(PCI_CONFIG_DATA + (offset & 3)), val);
}

void pci_enable_bus_mastering(pci_device_t *dev) {
    if (!dev)
        return;
    uint16_t cmd = pci_read16(dev->bus, dev->slot, dev->func, PCI_COMMAND);
    cmd |= (PCI_COMMAND_MASTER | PCI_COMMAND_MMIO | PCI_COMMAND_IO);
    pci_write16(dev->bus, dev->slot, dev->func, PCI_COMMAND, cmd);
}

static void pci_scan_function(uint8_t bus, uint8_t slot, uint8_t func) {
    uint16_t vendor_id = pci_read16(bus, slot, func, PCI_VENDOR_ID);
    if (vendor_id == 0xFFFF || vendor_id == 0x0000)
        return;

    uint16_t device_id = pci_read16(bus, slot, func, PCI_DEVICE_ID);
    uint8_t class_code = pci_read8(bus, slot, func, PCI_CLASS);
    uint8_t subclass = pci_read8(bus, slot, func, PCI_SUBCLASS);
    uint8_t prog_if = pci_read8(bus, slot, func, PCI_PROG_IF);
    uint8_t irq = pci_read8(bus, slot, func, PCI_INTERRUPT_LINE);

    pci_device_t *dev = (pci_device_t *)kmalloc(sizeof(pci_device_t));
    if (!dev)
        return;
    memset(dev, 0, sizeof(pci_device_t));

    dev->bus = bus;
    dev->slot = slot;
    dev->func = func;
    dev->vendor_id = vendor_id;
    dev->device_id = device_id;
    dev->class_code = class_code;
    dev->subclass = subclass;
    dev->prog_if = prog_if;
    dev->irq = irq;

    /* Probe BARs (handling 64-bit Memory BARs correctly) */
    for (int b = 0; b < 6; b++) {
        uint8_t bar_offset = (uint8_t)(PCI_BAR0 + b * 4);
        uint32_t bar_val = pci_read32(bus, slot, func, bar_offset);
        if (!bar_val) {
            dev->bar[b] = 0;
            dev->bar_size[b] = 0;
            dev->bar_is_io[b] = false;
            continue;
        }

        if (bar_val & 1) {
            /* I/O Space BAR (32-bit) */
            dev->bar[b] = bar_val & ~0x3;
            dev->bar_is_io[b] = true;
        } else {
            /* Memory Space BAR */
            uintptr_t bar_addr = bar_val & ~0xF;
            if ((bar_val & 0x06) == 0x04 && b < 5) {
                /* 64-bit Memory BAR */
                uint32_t bar_high = pci_read32(bus, slot, func, (uint8_t)(PCI_BAR0 + (b + 1) * 4));
                bar_addr |= ((uint64_t)bar_high << 32);
                dev->bar[b] = bar_addr;
                dev->bar_is_io[b] = false;
                dev->bar_size[b] = 0;
                b++; /* Consume next 32-bit BAR */
                dev->bar[b] = 0;
                dev->bar_is_io[b] = false;
                dev->bar_size[b] = 0;
                continue;
            }
            dev->bar[b] = bar_addr;
            dev->bar_is_io[b] = false;
        }
        dev->bar_size[b] = 0;
    }

    dev->next = g_pci_devices;
    g_pci_devices = dev;

    klog_info("PCI: %02x:%02x.%d [0x%04x:0x%04x] class 0x%02x:0x%02x (IRQ %d, BAR0: 0x%lx)", bus, slot, func, vendor_id,
              device_id, class_code, subclass, irq, dev->bar[0]);
}

static void pci_scan_bus(uint8_t bus) {
    for (uint8_t slot = 0; slot < 32; slot++) {
        uint16_t vendor_id = pci_read16(bus, slot, 0, PCI_VENDOR_ID);
        if (vendor_id == 0xFFFF || vendor_id == 0x0000)
            continue;

        uint8_t header_type = pci_read8(bus, slot, 0, PCI_HEADER_TYPE);
        pci_scan_function(bus, slot, 0);

        if (header_type & 0x80) {
            /* Multi-function device */
            for (uint8_t func = 1; func < 8; func++) {
                if (pci_read16(bus, slot, func, PCI_VENDOR_ID) != 0xFFFF) {
                    pci_scan_function(bus, slot, func);
                }
            }
        }
    }
}

void pci_init(void) {
    g_pci_devices = NULL;
    klog_info("PCI: Enumerating PCI bus devices...");

    for (uint16_t bus = 0; bus < 256; bus++) {
        pci_scan_bus((uint8_t)bus);
    }
}

pci_device_t *pci_find_device(uint16_t vendor_id, uint16_t device_id) {
    for (pci_device_t *cur = g_pci_devices; cur != NULL; cur = cur->next) {
        if (cur->vendor_id == vendor_id && (device_id == 0xFFFF || cur->device_id == device_id)) {
            return cur;
        }
    }
    return NULL;
}

pci_device_t *pci_find_class(uint8_t class_code, uint8_t subclass) {
    for (pci_device_t *cur = g_pci_devices; cur != NULL; cur = cur->next) {
        if (cur->class_code == class_code && (subclass == 0xFF || cur->subclass == subclass)) {
            return cur;
        }
    }
    return NULL;
}

pci_device_t *pci_get_device_list(void) {
    return g_pci_devices;
}
