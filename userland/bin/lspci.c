/*
 * SzpontOS - lspci (List PCI Devices)
 * Standard Unix utility to display detailed PCI bus & device information.
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>

typedef struct {
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    uint16_t class_id;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t rev;
    uint8_t prog_if;
    uint8_t irq;
    char bars[256];
} pci_info_t;

static const char *get_class_name(uint16_t class_id, uint8_t prog_if) {
    uint8_t base_class = (class_id >> 8) & 0xFF;
    uint8_t sub_class = class_id & 0xFF;

    switch (base_class) {
    case 0x00:
        if (sub_class == 0x01) return "VGA compatible unclassified device";
        return "Unclassified device";
    case 0x01:
        switch (sub_class) {
        case 0x00: return "SCSI storage controller";
        case 0x01: return "IDE interface";
        case 0x02: return "Floppy disk controller";
        case 0x05: return "RAID bus controller";
        case 0x06:
            if (prog_if == 0x01) return "SATA controller (AHCI 1.0)";
            return "SATA controller";
        case 0x08: return "Non-Volatile memory controller (NVMe)";
        default: return "Mass storage controller";
        }
    case 0x02:
        switch (sub_class) {
        case 0x00: return "Ethernet controller";
        case 0x80: return "Network controller";
        default: return "Network controller";
        }
    case 0x03:
        switch (sub_class) {
        case 0x00: return "VGA compatible controller";
        case 0x02: return "3D controller";
        default: return "Display controller";
        }
    case 0x04:
        switch (sub_class) {
        case 0x00: return "Multimedia video controller";
        case 0x01: return "Multimedia audio controller";
        case 0x03: return "Audio device (High Definition Audio)";
        default: return "Multimedia controller";
        }
    case 0x05:
        return "Memory controller";
    case 0x06:
        switch (sub_class) {
        case 0x00: return "Host bridge";
        case 0x01: return "ISA bridge";
        case 0x04: return "PCI bridge";
        default: return "Bridge";
        }
    case 0x07:
        return "Communication controller";
    case 0x08:
        return "Generic system peripheral";
    case 0x09:
        return "Input device controller";
    case 0x0A:
        return "Docking station";
    case 0x0B:
        return "Processor";
    case 0x0C:
        switch (sub_class) {
        case 0x00: return "FireWire (IEEE 1394)";
        case 0x03:
            if (prog_if == 0x00) return "USB controller (UHCI)";
            if (prog_if == 0x10) return "USB controller (OHCI)";
            if (prog_if == 0x20) return "USB controller (EHCI)";
            if (prog_if == 0x30) return "USB controller (xHCI)";
            return "USB controller";
        case 0x05: return "SMBus";
        default: return "Serial bus controller";
        }
    default:
        return "Unknown class";
    }
}

static const char *get_vendor_name(uint16_t vendor_id) {
    switch (vendor_id) {
    case 0x8086: return "Intel Corporation";
    case 0x1af4: return "Red Hat, Inc. / Virtio";
    case 0x1b36: return "Red Hat, Inc.";
    case 0x10ec: return "Realtek Semiconductor Co., Ltd.";
    case 0x1002: return "Advanced Micro Devices, Inc. [AMD/ATI]";
    case 0x10de: return "NVIDIA Corporation";
    case 0x1234: return "Bochs / QEMU";
    case 0x15ad: return "VMware";
    case 0x1022: return "Advanced Micro Devices, Inc. [AMD]";
    case 0x104c: return "Texas Instruments";
    default: return "Unknown Vendor";
    }
}

static const char *get_device_name(uint16_t vendor_id, uint16_t device_id) {
    if (vendor_id == 0x8086) {
        switch (device_id) {
        case 0x29c0: return "82G33/G31 Express DRAM Controller";
        case 0x2918: return "82801IB (ICH9) LPC Interface Controller";
        case 0x2922: return "82801IR/IO/IH (ICH9R/DO/DH) 6 port SATA Controller [AHCI mode]";
        case 0x2930: return "82801I (ICH9 Family) SMBus Controller";
        case 0x100e: return "82540EM Gigabit Ethernet Controller";
        case 0x1237: return "440FX - 82441FX PMC [Natoma]";
        case 0x7000: return "82371SB PIIX3 ISA [Natoma/Triton II]";
        case 0x7010: return "82371SB PIIX3 IDE [Natoma/Triton II]";
        case 0x7113: return "82371AB/EB/MB PIIX4 ACPI";
        default: break;
        }
    } else if (vendor_id == 0x1af4) {
        switch (device_id) {
        case 0x1000: return "Virtio network device";
        case 0x1001: return "Virtio block device";
        case 0x1002: return "Virtio memory balloon";
        case 0x1003: return "Virtio console";
        case 0x1004: return "Virtio RNG";
        case 0x1050: return "Virtio GPU";
        default: break;
        }
    } else if (vendor_id == 0x1b36) {
        switch (device_id) {
        case 0x000d: return "QEMU USB 3.0 xHCI Controller";
        case 0x0008: return "QEMU PCIe Root Port";
        case 0x0001: return "PCI-PCI bridge (redhat-pci-bridge)";
        default: break;
        }
    } else if (vendor_id == 0x10ec) {
        switch (device_id) {
        case 0x8139: return "RTL-8129/8130/8139 PCI Fast Ethernet Adapter";
        case 0x8168: return "RTL8111/8168/8411 PCI Express Gigabit Ethernet Controller";
        default: break;
        }
    } else if (vendor_id == 0x1234) {
        if (device_id == 0x1111) return "VGA Extension";
    }

    return NULL;
}

static void print_help(void) {
    printf("Usage: lspci [options]\n\n");
    printf("Basic display modes:\n");
    printf("  -v            Be verbose (show detailed BARs & IRQs)\n");
    printf("  -n            Show numeric IDs (vendor:device hex codes)\n");
    printf("  -s [[<bus>]:][<slot>][.[<func>]]  Show only devices in selected slots\n");
    printf("  -h, --help    Show this help\n");
    printf("  -V, --version Show version\n");
}

int main(int argc, char *argv[]) {
    bool opt_verbose = false;
    bool opt_numeric = false;
    int filter_bus = -1;
    int filter_slot = -1;
    int filter_func = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            opt_verbose = true;
        } else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--numeric") == 0) {
            opt_numeric = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help();
            return 0;
        } else if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("lspci (SzpontOS pciutils) 0.1.0\n");
            return 0;
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            const char *s = argv[++i];
            /* Parse bus:slot.func */
            char *colon = strchr(s, ':');
            char *dot = strchr(s, '.');
            if (colon) {
                filter_bus = (int)strtol(s, NULL, 16);
                filter_slot = (int)strtol(colon + 1, NULL, 16);
            } else {
                filter_slot = (int)strtol(s, NULL, 16);
            }
            if (dot) {
                filter_func = (int)strtol(dot + 1, NULL, 16);
            }
        }
    }

    FILE *f = fopen("/proc/pci", "r");
    if (!f) {
        fprintf(stderr, "lspci: Cannot open /proc/pci (is procfs mounted?)\n");
        return 1;
    }

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '\0' || line[0] == '\n')
            continue;

        pci_info_t dev;
        memset(&dev, 0, sizeof(dev));

        unsigned int bus = 0, slot = 0, func = 0, class_id = 0, vid = 0, did = 0, prog_if = 0, irq = 0;
        char bars[256] = {0};

        int matched = sscanf(line, "%x:%x.%x %x: %x:%x (rev %*x, prog-if %x) IRQ %u %[^\n]",
                             &bus, &slot, &func, &class_id, &vid, &did, &prog_if, &irq, bars);

        if (matched < 6) {
            continue;
        }

        dev.bus = (uint8_t)bus;
        dev.slot = (uint8_t)slot;
        dev.func = (uint8_t)func;
        dev.class_id = (uint16_t)class_id;
        dev.vendor_id = (uint16_t)vid;
        dev.device_id = (uint16_t)did;
        dev.prog_if = (uint8_t)prog_if;
        dev.irq = (uint8_t)irq;
        strncpy(dev.bars, bars, sizeof(dev.bars) - 1);

        if (filter_bus >= 0 && dev.bus != filter_bus) continue;
        if (filter_slot >= 0 && dev.slot != filter_slot) continue;
        if (filter_func >= 0 && dev.func != filter_func) continue;

        if (opt_numeric) {
            printf("%02x:%02x.%x %04x: %04x:%04x (rev 00)\n",
                   dev.bus, dev.slot, dev.func, dev.class_id, dev.vendor_id, dev.device_id);
        } else {
            const char *class_str = get_class_name(dev.class_id, dev.prog_if);
            const char *vendor_str = get_vendor_name(dev.vendor_id);
            const char *dev_str = get_device_name(dev.vendor_id, dev.device_id);

            if (dev_str) {
                printf("%02x:%02x.%x %s: %s %s\n",
                       dev.bus, dev.slot, dev.func, class_str, vendor_str, dev_str);
            } else {
                printf("%02x:%02x.%x %s: %s Device %04x\n",
                       dev.bus, dev.slot, dev.func, class_str, vendor_str, dev.device_id);
            }
        }

        if (opt_verbose) {
            if (dev.irq > 0) {
                printf("\tFlags: bus master, IRQ %u\n", dev.irq);
            } else {
                printf("\tFlags: bus master\n");
            }
            if (strlen(dev.bars) > 0) {
                printf("\tMemory: %s\n", dev.bars);
            }
        }
    }

    fclose(f);
    return 0;
}
