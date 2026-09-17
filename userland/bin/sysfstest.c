/*
 * SzpontOS Userland - SysFS (/sys) Test Suite
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>

#define TEST_PASS(name) printf("  [\033[32mPASS\033[0m] %s\n", name)
#define TEST_FAIL(name, msg) do { printf("  [\033[31mFAIL\033[0m] %s: %s (errno=%d)\n", name, msg, errno); exit(1); } while (0)

static void read_and_print_file(const char *path, const char *label) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        TEST_FAIL(path, "Failed to open sysfs file");
    }
    char buf[128];
    memset(buf, 0, sizeof(buf));
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n < 0) {
        TEST_FAIL(path, "Failed to read sysfs file");
    }
    /* strip trailing newline for display */
    if (n > 0 && buf[n - 1] == '\n') buf[n - 1] = '\0';
    printf("     -> %s: %s\n", label, buf);
    TEST_PASS(path);
}

int main(void) {
    printf("=== Starting SzpontOS SysFS (/sys) Test Suite ===\n");

    /* Test 1: Check root /sys */
    DIR *d = opendir("/sys");
    if (!d) {
        TEST_FAIL("/sys", "Failed to opendir /sys");
    }
    closedir(d);
    TEST_PASS("opendir /sys succeeded");

    /* Test 2: Check CPU topology */
    read_and_print_file("/sys/devices/system/cpu/online", "Online CPUs");
    read_and_print_file("/sys/devices/system/cpu/cpu0/topology/core_id", "CPU 0 Core ID");

    /* Test 3: Check DRM class */
    read_and_print_file("/sys/class/drm/card0/dev", "DRM Card0 Major:Minor");

    /* Test 4: Check Network class */
    d = opendir("/sys/class/net");
    if (!d) {
        TEST_FAIL("/sys/class/net", "Failed to opendir /sys/class/net");
    }
    struct dirent *de;
    int netif_count = 0;
    while ((de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;
        netif_count++;
        char path[128];
        snprintf(path, sizeof(path), "/sys/class/net/%s/address", de->d_name);
        char label[64];
        snprintf(label, sizeof(label), "Netif '%s' MAC", de->d_name);
        read_and_print_file(path, label);
    }
    closedir(d);
    if (netif_count == 0) {
        TEST_FAIL("net interfaces", "No network interfaces found in /sys/class/net");
    }
    TEST_PASS("Enumerated /sys/class/net network interfaces");

    /* Test 5: Check PCI bus devices */
    d = opendir("/sys/bus/pci/devices");
    if (!d) {
        TEST_FAIL("/sys/bus/pci/devices", "Failed to opendir /sys/bus/pci/devices");
    }
    int pci_count = 0;
    while ((de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;
        pci_count++;
        char path[128];
        snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/vendor", de->d_name);
        char label[64];
        snprintf(label, sizeof(label), "PCI %s Vendor", de->d_name);
        read_and_print_file(path, label);
    }
    closedir(d);
    if (pci_count == 0) {
        TEST_FAIL("PCI devices", "No PCI devices found in /sys/bus/pci/devices");
    }
    TEST_PASS("Enumerated /sys/bus/pci/devices hardware buses");

    printf("=== All SysFS (/sys) tests PASSED successfully! ===\n");
    return 0;
}
