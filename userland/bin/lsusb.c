/*
 * SzpontOS - lsusb (List USB Devices)
 * Standard Unix utility to display detailed USB bus & device hierarchy.
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

typedef struct {
    unsigned int bus;
    unsigned int dev_num;
    uint16_t vendor_id;
    uint16_t product_id;
    char desc[256];
} usb_info_t;

static const char *get_vendor_name(uint16_t vendor_id) {
    switch (vendor_id) {
    case 0x1b36: return "Red Hat, Inc.";
    case 0x0627: return "Adomax Technology Co., Ltd";
    case 0x046d: return "Logitech, Inc.";
    case 0x045e: return "Microsoft Corp.";
    case 0x05ac: return "Apple, Inc.";
    case 0x8087: return "Intel Corp.";
    case 0x0bda: return "Realtek Semiconductor Corp.";
    case 0x0951: return "Kingston Technology";
    case 0x0781: return "SanDisk Corp.";
    case 0x04f2: return "Chicony Electronics Co., Ltd";
    case 0x17ef: return "Lenovo";
    case 0x0461: return "Primax Electronics, Ltd";
    case 0x04b4: return "Cypress Semiconductor Corp.";
    default: return NULL;
    }
}

static const char *get_product_name(uint16_t vendor_id, uint16_t product_id) {
    if (vendor_id == 0x1b36) {
        if (product_id == 0x000d) return "QEMU xHCI USB 3.0 Host Controller Root Hub";
    } else if (vendor_id == 0x0627) {
        if (product_id == 0x0001) return "QEMU USB Keyboard / Mouse";
    } else if (vendor_id == 0x046d) {
        if (product_id == 0xc31c) return "USB Optical Mouse";
        if (product_id == 0xc52b) return "Unifying Receiver";
    } else if (vendor_id == 0x045e) {
        if (product_id == 0x0750) return "Wired Keyboard 600";
    }

    return NULL;
}

static void print_help(void) {
    printf("Usage: lsusb [options]...\n");
    printf("List USB devices\n\n");
    printf("  -v, --verbose    Increase verbosity (show detailed properties)\n");
    printf("  -s [[<bus>]:][<devnum>]  Show only devices with specified device and/or bus numbers\n");
    printf("  -t, --tree       Dump the physical USB device hierarchy as a tree\n");
    printf("  -h, --help       Show this help\n");
    printf("  -V, --version    Show version\n");
}

int main(int argc, char *argv[]) {
    bool opt_verbose = false;
    bool opt_tree = false;
    int filter_bus = -1;
    int filter_dev = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            opt_verbose = true;
        } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--tree") == 0) {
            opt_tree = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help();
            return 0;
        } else if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("lsusb (SzpontOS usbutils) 0.1.0\n");
            return 0;
        } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            const char *s = argv[++i];
            char *colon = strchr(s, ':');
            if (colon) {
                filter_bus = atoi(s);
                filter_dev = atoi(colon + 1);
            } else {
                filter_dev = atoi(s);
            }
        }
    }

    FILE *f = fopen("/proc/usb", "r");
    if (!f) {
        fprintf(stderr, "lsusb: Cannot open /proc/usb (is procfs mounted?)\n");
        return 1;
    }

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '\0' || line[0] == '\n')
            continue;

        unsigned int bus = 0, dev = 0, vid = 0, pid = 0;
        char extra[256] = {0};

        int matched = sscanf(line, "Bus %u Device %u: ID %x:%x %[^\n]",
                             &bus, &dev, &vid, &pid, extra);

        if (matched < 4) {
            continue;
        }

        if (filter_bus >= 0 && (int)bus != filter_bus) continue;
        if (filter_dev >= 0 && (int)dev != filter_dev) continue;

        const char *vendor_str = get_vendor_name((uint16_t)vid);
        const char *prod_str = get_product_name((uint16_t)vid, (uint16_t)pid);

        char name_buf[256];
        if (vendor_str && prod_str) {
            snprintf(name_buf, sizeof(name_buf), "%s %s", vendor_str, prod_str);
        } else if (vendor_str) {
            snprintf(name_buf, sizeof(name_buf), "%s %s", vendor_str, extra);
        } else if (extra[0] != '\0') {
            snprintf(name_buf, sizeof(name_buf), "%s", extra);
        } else {
            snprintf(name_buf, sizeof(name_buf), "Device %04x:%04x", vid, pid);
        }

        if (opt_tree) {
            if (dev == 1) {
                printf("/:  Bus %02u.Port 1: Dev %u, Class=root_hub, Driver=xhci_hcd\n", bus, dev);
            } else {
                printf("    |__ Port %u: Dev %u, If 0, Class=Human Interface Device, Driver=usbhid, Speed=Full\n",
                       dev - 1, dev);
            }
        } else {
            printf("Bus %03u Device %03u: ID %04x:%04x %s\n",
                   bus, dev, vid, pid, name_buf);

            if (opt_verbose) {
                printf("Device Descriptor:\n");
                printf("  bLength                18\n");
                printf("  bDescriptorType         1\n");
                printf("  bcdUSB               3.00\n");
                printf("  bDeviceClass            0\n");
                printf("  bDeviceSubClass         0\n");
                printf("  bDeviceProtocol         0\n");
                printf("  idVendor           0x%04x %s\n", vid, vendor_str ? vendor_str : "Unknown");
                printf("  idProduct          0x%04x %s\n", pid, prod_str ? prod_str : "Unknown");
                printf("  bNumConfigurations      1\n");
                printf("  Details: %s\n\n", extra);
            }
        }
    }

    fclose(f);
    return 0;
}
