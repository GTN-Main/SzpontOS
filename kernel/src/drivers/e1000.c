/*
 * SzpontOS - Intel E1000 Gigabit Ethernet Network Driver
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <drivers/e1000.h>
#include <drivers/pci.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/heap.h>
#include <kernel/kprint.h>
#include <kernel/string.h>

static pci_device_t *g_e1000_pci = NULL;
static uintptr_t g_e1000_mmio_base = 0;
static netif_t g_e1000_netif;

static e1000_rx_desc_t *g_rx_descs = NULL;
static e1000_tx_desc_t *g_tx_descs = NULL;
static uint8_t *g_rx_buffers[E1000_NUM_RX_DESC];
static uint8_t *g_tx_buffers[E1000_NUM_TX_DESC];

static uint16_t g_rx_cur = 0;
static uint16_t g_tx_cur = 0;
static bool g_e1000_has_eeprom = false;

static inline void write_cmd(uint16_t addr, uint32_t val) {
    *(volatile uint32_t *)(g_e1000_mmio_base + addr) = val;
}

static inline uint32_t read_cmd(uint16_t addr) {
    return *(volatile uint32_t *)(g_e1000_mmio_base + addr);
}

static uint16_t e1000_read_eeprom(uint8_t addr) {
    write_cmd(REG_EEPROM, 1 | ((uint32_t)addr << 8));
    uint32_t tmp = 0;
    while (!((tmp = read_cmd(REG_EEPROM)) & (1 << 4))) {
        /* Wait for EEPROM read completion */
    }
    return (uint16_t)((tmp >> 16) & 0xFFFF);
}

static bool e1000_detect_eeprom(void) {
    write_cmd(REG_EEPROM, 1);
    for (int i = 0; i < 1000; i++) {
        uint32_t val = read_cmd(REG_EEPROM);
        if (val & (1 << 4)) {
            g_e1000_has_eeprom = true;
            return true;
        }
    }
    g_e1000_has_eeprom = false;
    return false;
}

static void e1000_read_mac(uint8_t *mac) {
    if (e1000_detect_eeprom()) {
        uint16_t val;
        val = e1000_read_eeprom(0);
        mac[0] = val & 0xFF;
        mac[1] = val >> 8;
        val = e1000_read_eeprom(1);
        mac[2] = val & 0xFF;
        mac[3] = val >> 8;
        val = e1000_read_eeprom(2);
        mac[4] = val & 0xFF;
        mac[5] = val >> 8;
    } else {
        uint32_t ral = read_cmd(REG_RAL);
        uint32_t rah = read_cmd(REG_RAH);
        mac[0] = ral & 0xFF;
        mac[1] = (ral >> 8) & 0xFF;
        mac[2] = (ral >> 16) & 0xFF;
        mac[3] = (ral >> 24) & 0xFF;
        mac[4] = rah & 0xFF;
        mac[5] = (rah >> 8) & 0xFF;
    }
}

static void e1000_rx_init(void) {
    uintptr_t rx_phys = pmm_alloc_page();
    g_rx_descs = (e1000_rx_desc_t *)PHYS_TO_VIRT(rx_phys);
    memset(g_rx_descs, 0, PAGE_SIZE);

    for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
        uintptr_t buf_phys = pmm_alloc_page();
        g_rx_buffers[i] = (uint8_t *)PHYS_TO_VIRT(buf_phys);
        g_rx_descs[i].address = (uint64_t)buf_phys;
        g_rx_descs[i].status = 0;
    }

    write_cmd(REG_RDBAH, (uint32_t)(rx_phys >> 32));
    write_cmd(REG_RDBAL, (uint32_t)(rx_phys & 0xFFFFFFFF));
    write_cmd(REG_RDLEN, E1000_NUM_RX_DESC * sizeof(e1000_rx_desc_t));
    write_cmd(REG_RDH, 0);
    write_cmd(REG_RDT, E1000_NUM_RX_DESC - 1);

    /* RCTL: Enable, Strip CRC, 2048-byte buffer, Broadcast accept */
    write_cmd(REG_RCTL, (1 << 1) | (1 << 26) | (1 << 15) | (0 << 16));
    g_rx_cur = 0;
}

static void e1000_tx_init(void) {
    uintptr_t tx_phys = pmm_alloc_page();
    g_tx_descs = (e1000_tx_desc_t *)PHYS_TO_VIRT(tx_phys);
    memset(g_tx_descs, 0, PAGE_SIZE);

    for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
        uintptr_t buf_phys = pmm_alloc_page();
        g_tx_buffers[i] = (uint8_t *)PHYS_TO_VIRT(buf_phys);
        g_tx_descs[i].address = (uint64_t)buf_phys;
        g_tx_descs[i].cmd = 0;
        g_tx_descs[i].status = 1; /* Mark as empty/done */
    }

    write_cmd(REG_TDBAH, (uint32_t)(tx_phys >> 32));
    write_cmd(REG_TDBAL, (uint32_t)(tx_phys & 0xFFFFFFFF));
    write_cmd(REG_TDLEN, E1000_NUM_TX_DESC * sizeof(e1000_tx_desc_t));
    write_cmd(REG_TDH, 0);
    write_cmd(REG_TDT, 0);

    /* TCTL: Enable, Pad Short Packets, Collision Threshold 15, Collision Distance 64 */
    write_cmd(REG_TCTL, (1 << 1) | (1 << 3) | (15 << 4) | (64 << 12));
    g_tx_cur = 0;
}

int e1000_send(netif_t *netif, net_buf_t *buf) {
    if (!buf || buf->len == 0 || !g_tx_descs)
        return -1;
    (void)netif;

    uint16_t cur = g_tx_cur;
    memcpy(g_tx_buffers[cur], buf->data + buf->offset, buf->len);

    g_tx_descs[cur].length = (uint16_t)buf->len;
    g_tx_descs[cur].cmd = (1 << 0) | (1 << 1) | (1 << 3); /* EOP | IFCS | RS */
    g_tx_descs[cur].status = 0;

    g_tx_cur = (g_tx_cur + 1) % E1000_NUM_TX_DESC;
    write_cmd(REG_TDT, g_tx_cur);

    if (netif) {
        netif->tx_packets++;
        netif->tx_bytes += buf->len;
    }

    return 0;
}

void e1000_poll(void) {
    if (!g_rx_descs)
        return;

    while (g_rx_descs[g_rx_cur].status & 0x01) { /* DD - Descriptor Done */
        uint8_t *pkt_data = g_rx_buffers[g_rx_cur];
        uint16_t pkt_len = g_rx_descs[g_rx_cur].length;

        if (pkt_len > 0 && pkt_len <= NET_MAX_PACKET_SIZE) {
            net_buf_t *buf = net_buf_alloc();
            if (buf) {
                memcpy(buf->data, pkt_data, pkt_len);
                buf->len = pkt_len;
                buf->offset = 0;

                g_e1000_netif.rx_packets++;
                g_e1000_netif.rx_bytes += pkt_len;

                netif_input(&g_e1000_netif, buf);
            }
        }

        g_rx_descs[g_rx_cur].status = 0;
        uint16_t old_cur = g_rx_cur;
        g_rx_cur = (g_rx_cur + 1) % E1000_NUM_RX_DESC;
        write_cmd(REG_RDT, old_cur);
    }
}

bool e1000_init(pci_device_t *pci_dev) {
    if (!pci_dev)
        return false;

    g_e1000_pci = pci_dev;
    pci_enable_bus_mastering(pci_dev);

    /* BAR0 contains the MMIO base address */
    uintptr_t mmio_phys = pci_dev->bar[0] & ~0xFULL;
    g_e1000_mmio_base = (uintptr_t)PHYS_TO_VIRT(mmio_phys);

    /* Map 128 KB MMIO space in kernel pagemap */
    extern pagemap_t g_kernel_pagemap;
    for (size_t i = 0; i < 32; i++) {
        vmm_map_page(&g_kernel_pagemap, g_e1000_mmio_base + (i * PAGE_SIZE), mmio_phys + (i * PAGE_SIZE),
                     VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | VMM_FLAG_CACHE_DISABLE);
    }

    /* Initialize device link control: Set Auto-Speed Detection & Link Up */
    write_cmd(REG_CTRL, read_cmd(REG_CTRL) | (1 << 5) | (1 << 6));

    /* Read MAC Address */
    uint8_t mac[6];
    e1000_read_mac(mac);

    /* Initialize Multicast Table Array (Clear all entries) */
    for (int i = 0; i < 128; i++) {
        write_cmd(REG_MTA + (i * 4), 0);
    }

    /* Initialize RX and TX Rings */
    e1000_rx_init();
    e1000_tx_init();

    /* Clear and Disable all Interrupts (using polling / thread dispatcher) */
    write_cmd(REG_IMS, 0);
    read_cmd(REG_ICR);

    /* Setup network interface */
    memset(&g_e1000_netif, 0, sizeof(netif_t));
    ksnprintf(g_e1000_netif.name, sizeof(g_e1000_netif.name), "eth0");
    memcpy(g_e1000_netif.mac, mac, 6);

    /* Default QEMU NAT IPv4 Configuration (10.0.2.15/24, GW: 10.0.2.2) */
    g_e1000_netif.ip = (10) | (0 << 8) | (2 << 16) | (15 << 24);
    g_e1000_netif.netmask = (255) | (255 << 8) | (255 << 16) | (0 << 24);
    g_e1000_netif.gateway = (10) | (0 << 8) | (2 << 16) | (2 << 24);
    g_e1000_netif.flags = NETIF_FLAG_UP | NETIF_FLAG_RUNNING;
    g_e1000_netif.send = e1000_send;
    g_e1000_netif.poll = (void (*)(netif_t *))e1000_poll;

    netif_register(&g_e1000_netif);

    klog_info("E1000: Intel 8254x NIC initialized: eth0, MAC %02x:%02x:%02x:%02x:%02x:%02x (IP 10.0.2.15/24)", mac[0],
              mac[1], mac[2], mac[3], mac[4], mac[5]);
    return true;
}
