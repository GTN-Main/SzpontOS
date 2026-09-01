/*
 * SzpontOS - Realtek RTL8139 PCI Fast Ethernet Driver
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <drivers/rtl8139.h>
#include <drivers/pci.h>
#include <net/net.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/heap.h>
#include <arch/x86_64/io.h>
#include <kernel/kprint.h>
#include <kernel/string.h>

#define RTL8139_VENDOR_ID 0x10EC
#define RTL8139_DEVICE_ID 0x8139

/* Register Offsets */
#define RTL_REG_MAC0 0x00
#define RTL_REG_MAR0 0x08
#define RTL_REG_TSD0 0x10
#define RTL_REG_TSAD0 0x20
#define RTL_REG_RBSTART 0x30
#define RTL_REG_CMD 0x37
#define RTL_REG_CAPR 0x38
#define RTL_REG_CBR 0x3A
#define RTL_REG_IMR 0x3C
#define RTL_REG_ISR 0x3E
#define RTL_REG_TCR 0x40
#define RTL_REG_RCR 0x44
#define RTL_REG_CONFIG1 0x52

/* Command Register Bits */
#define RTL_CMD_BUFFER_EMPTY 0x01
#define RTL_CMD_TE 0x04  /* Transmitter Enable */
#define RTL_CMD_RE 0x08  /* Receiver Enable */
#define RTL_CMD_RST 0x10 /* Software Reset */

/* RCR Flags: Accept Broadcast, Multicast, Physical match, All with error, Wrap */
#define RTL_RCR_AAP (1 << 0)
#define RTL_RCR_APM (1 << 1)
#define RTL_RCR_AM (1 << 2)
#define RTL_RCR_AB (1 << 3)
#define RTL_RCR_WRAP (1 << 7)

#define RX_BUFFER_SIZE (8192 + 16 + 1536) /* 8K ring + 16-byte header + wrap space */
#define TX_BUFFER_SIZE 2048
#define NUM_TX_DESCRIPTORS 4

static uint16_t g_rtl_io_base = 0;
static uint8_t *g_rx_buffer = NULL;
static uintptr_t g_rx_buffer_phys = 0;

static uint8_t *g_tx_buffers[NUM_TX_DESCRIPTORS];
static uintptr_t g_tx_buffers_phys[NUM_TX_DESCRIPTORS];
static uint8_t g_tx_cur = 0;
static uint16_t g_rx_offset = 0;

static netif_t g_rtl8139_netif;
static bool g_rtl8139_active = false;

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

static int rtl8139_send(netif_t *netif, net_buf_t *buf) {
    if (!buf || buf->len == 0 || !g_rtl8139_active || buf->len > TX_BUFFER_SIZE)
        return -1;

    uint8_t tx_idx = g_tx_cur;
    g_tx_cur = (g_tx_cur + 1) % NUM_TX_DESCRIPTORS;

    memcpy(g_tx_buffers[tx_idx], buf->data + buf->offset, buf->len);

    /* Write physical address to TSAD */
    outl(g_rtl_io_base + RTL_REG_TSAD0 + (tx_idx * 4), (uint32_t)g_tx_buffers_phys[tx_idx]);

    /* Write length and clear OWN bit to start transmit */
    outl(g_rtl_io_base + RTL_REG_TSD0 + (tx_idx * 4), (uint32_t)(buf->len & 0x1FFF));

    if (netif) {
        netif->tx_packets++;
        netif->tx_bytes += buf->len;
    }

    return 0;
}

void rtl8139_poll(void) {
    if (!g_rtl8139_active)
        return;

    /* Check if receive buffer has data */
    while (!(inb(g_rtl_io_base + RTL_REG_CMD) & RTL_CMD_BUFFER_EMPTY)) {
        uint8_t *rx_ptr = g_rx_buffer + g_rx_offset;
        uint16_t status = *(uint16_t *)rx_ptr;
        uint16_t length = *(uint16_t *)(rx_ptr + 2);

        /* Packet OK check (bit 0 of status) */
        if (!(status & 1) || length == 0 || length > 1536) {
            /* Reset buffer on error */
            outw(g_rtl_io_base + RTL_REG_CAPR, 0);
            g_rx_offset = 0;
            break;
        }

        uint8_t *packet_data = rx_ptr + 4;
        size_t eth_len = length - 4; /* Strip 4-byte CRC */

        net_buf_t *buf = net_buf_alloc();
        if (buf) {
            memcpy(buf->data, packet_data, eth_len);
            buf->len = eth_len;
            buf->offset = 0;

            g_rtl8139_netif.rx_packets++;
            g_rtl8139_netif.rx_bytes += eth_len;

            netif_input(&g_rtl8139_netif, buf);
        }

        /* Advance RX offset (aligned to 4 bytes) */
        g_rx_offset = (uint16_t)((g_rx_offset + length + 4 + 3) & ~3);
        if (g_rx_offset >= 8192) {
            g_rx_offset -= 8192;
        }

        outw(g_rtl_io_base + RTL_REG_CAPR, g_rx_offset - 16);
    }

    /* Acknowledge interrupts in ISR */
    outw(g_rtl_io_base + RTL_REG_ISR, inw(g_rtl_io_base + RTL_REG_ISR));
}

void rtl8139_init(void) {
    pci_device_t *pci_dev = pci_find_device(RTL8139_VENDOR_ID, RTL8139_DEVICE_ID);
    if (!pci_dev) {
        return;
    }

    pci_enable_bus_mastering(pci_dev);

    /* BAR0 contains I/O port address */
    g_rtl_io_base = (uint16_t)(pci_dev->bar[0] & ~0x3);
    if (!g_rtl_io_base)
        return;

    klog_info("RTL8139: Found Realtek RTL8139 Fast Ethernet at I/O 0x%04x", g_rtl_io_base);

    /* 1. Power on device (Config_1 = 0x00) */
    outb(g_rtl_io_base + RTL_REG_CONFIG1, 0x00);

    /* 2. Software Reset */
    outb(g_rtl_io_base + RTL_REG_CMD, RTL_CMD_RST);
    int timeout = 50000;
    while ((inb(g_rtl_io_base + RTL_REG_CMD) & RTL_CMD_RST) && --timeout) {
        io_wait();
    }

    /* 3. Read MAC Address */
    uint8_t mac[6];
    for (int i = 0; i < 6; i++) {
        mac[i] = inb(g_rtl_io_base + RTL_REG_MAC0 + i);
    }

    /* 4. Allocate RX Buffer DMA */
    g_rx_buffer = (uint8_t *)alloc_dma_zero(RX_BUFFER_SIZE, &g_rx_buffer_phys);
    outl(g_rtl_io_base + RTL_REG_RBSTART, (uint32_t)g_rx_buffer_phys);

    /* 5. Allocate TX Buffers */
    for (int i = 0; i < NUM_TX_DESCRIPTORS; i++) {
        g_tx_buffers[i] = (uint8_t *)alloc_dma_zero(TX_BUFFER_SIZE, &g_tx_buffers_phys[i]);
    }
    g_tx_cur = 0;
    g_rx_offset = 0;

    /* 6. Configure Interrupt Mask (ROK | TOK | RER | TER) */
    outw(g_rtl_io_base + RTL_REG_IMR, 0x0005);

    /* 7. Configure Receive Configuration (Accept broadcast, multicast, physical, wrap) */
    outl(g_rtl_io_base + RTL_REG_RCR, RTL_RCR_AB | RTL_RCR_AM | RTL_RCR_APM | RTL_RCR_AAP | RTL_RCR_WRAP);

    /* 8. Enable RX and TX */
    outb(g_rtl_io_base + RTL_REG_CMD, RTL_CMD_RE | RTL_CMD_TE);

    /* 9. Register Network Interface */
    memset(&g_rtl8139_netif, 0, sizeof(netif_t));
    ksnprintf(g_rtl8139_netif.name, sizeof(g_rtl8139_netif.name), "eth1");
    memcpy(g_rtl8139_netif.mac, mac, 6);

    g_rtl8139_netif.ip = (10) | (0 << 8) | (2 << 16) | (16 << 24);
    g_rtl8139_netif.netmask = (255) | (255 << 8) | (255 << 16) | (0 << 24);
    g_rtl8139_netif.gateway = (10) | (0 << 8) | (2 << 16) | (2 << 24);
    g_rtl8139_netif.flags = NETIF_FLAG_UP | NETIF_FLAG_RUNNING;
    g_rtl8139_netif.send = rtl8139_send;
    g_rtl8139_netif.poll = (void (*)(netif_t *))rtl8139_poll;

    netif_register(&g_rtl8139_netif);

    g_rtl8139_active = true;
    klog_info("RTL8139: Realtek NIC initialized: eth1, MAC %02x:%02x:%02x:%02x:%02x:%02x (IP 10.0.2.16/24)", mac[0],
              mac[1], mac[2], mac[3], mac[4], mac[5]);
}
