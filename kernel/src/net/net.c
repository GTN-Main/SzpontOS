/*
 * SzpontOS - Network Core Subsystem Initializer
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <net/net.h>
#include <net/socket.h>
#include <drivers/pci.h>
#include <drivers/e1000.h>
#include <drivers/rtl8139.h>
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

    /* Probe and Initialize all PCI Network Controllers */
    netif_init_drivers();

    klog_info("NET: TCP/IP Stack & Berkeley Sockets ready!");
}
