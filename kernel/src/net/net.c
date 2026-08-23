/*
 * SzpontOS - Network Core Subsystem Initializer
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <net/net.h>
#include <net/socket.h>
#include <drivers/pci.h>
#include <drivers/e1000.h>
#include <kernel/kprint.h>

extern void loopback_init(void);

void net_init(void) {
    klog_info("NET: Initializing TCP/IP Network Stack & Berkeley Sockets...");

    arp_init();
    ipv4_init();
    udp_init();
    tcp_init();
    socket_subsystem_init();

    /* Initialize Loopback (127.0.0.1) */
    loopback_init();

    /* Scan PCI for Intel E1000 Ethernet Cards */
    pci_device_t *nic = pci_find_device(E1000_VENDOR_INTEL, E1000_DEV_82540EM);
    if (!nic) nic = pci_find_device(E1000_VENDOR_INTEL, E1000_DEV_82545EM);
    if (!nic) nic = pci_find_device(E1000_VENDOR_INTEL, E1000_DEV_82574L);
    if (!nic) nic = pci_find_device(E1000_VENDOR_INTEL, 0x1004);
    if (!nic) nic = pci_find_device(E1000_VENDOR_INTEL, 0x1539);

    if (nic) {
        e1000_init(nic);
    } else {
        klog_info("NET: No physical PCI Ethernet NIC detected, operating in Loopback mode");
    }

    klog_info("NET: TCP/IP Stack & Berkeley Sockets ready!");
}
