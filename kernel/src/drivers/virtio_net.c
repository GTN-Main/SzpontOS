/*
 * SzpontOS - VirtIO PCI Network Device Driver (virtio_net.c)
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <drivers/virtio_net.h>
#include <drivers/pci.h>
#include <net/net.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/heap.h>
#include <arch/x86_64/io.h>
#include <kernel/kprint.h>
#include <kernel/string.h>

#define VIRTIO_NET_QUEUE_RX 0
#define VIRTIO_NET_QUEUE_TX 1
#define VIRTIO_NET_NUM_BUFFERS 64
#define VIRTIO_NET_BUF_SIZE 2048

static uint16_t g_vnet_io_base = 0;
static pci_device_t *g_vnet_pci = NULL;
static netif_t g_vnet_netif;

/* RX Virtqueue */
static uint16_t g_rx_qsize = 0;
static vring_desc_t *g_rx_desc = NULL;
static vring_avail_t *g_rx_avail = NULL;
static vring_used_t *g_rx_used = NULL;
static uint8_t *g_rx_bufs[VIRTIO_NET_NUM_BUFFERS];
static uintptr_t g_rx_bufs_phys[VIRTIO_NET_NUM_BUFFERS];
static uint16_t g_rx_last_used_idx = 0;

/* TX Virtqueue */
static uint16_t g_tx_qsize = 0;
static vring_desc_t *g_tx_desc = NULL;
static vring_avail_t *g_tx_avail = NULL;
static vring_used_t *g_tx_used = NULL;
static uint8_t *g_tx_buf = NULL;
static uintptr_t g_tx_buf_phys = 0;
static uint16_t g_tx_cur = 0;

static void *alloc_dma_zero(size_t size, uintptr_t *phys_out) {
    size_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    uintptr_t phys = (uintptr_t)pmm_alloc_pages(num_pages);
    if (!phys)
        return NULL;
    *phys_out = phys;
    void *virt = (void *)PHYS_TO_VIRT(phys);
    memset(virt, 0, num_pages * PAGE_SIZE);
    return virt;
}

static int virtio_net_send(netif_t *netif, net_buf_t *buf) {
    if (!g_vnet_io_base || !buf || buf->len == 0 || buf->len > (VIRTIO_NET_BUF_SIZE - sizeof(virtio_net_hdr_t)))
        return -1;

    /* Build VirtIO net header (all zeroes for standard frame) */
    virtio_net_hdr_t *hdr = (virtio_net_hdr_t *)g_tx_buf;
    memset(hdr, 0, sizeof(virtio_net_hdr_t));

    /* Copy packet data */
    memcpy(g_tx_buf + sizeof(virtio_net_hdr_t), buf->data + buf->offset, buf->len);

    uint32_t total_len = (uint32_t)(sizeof(virtio_net_hdr_t) + buf->len);

    /* Fill TX descriptor */
    g_tx_desc[0].addr = (uint64_t)g_tx_buf_phys;
    g_tx_desc[0].len = total_len;
    g_tx_desc[0].flags = 0;
    g_tx_desc[0].next = 0;

    /* Put descriptor in TX available ring */
    uint16_t avail_idx = g_tx_avail->idx;
    g_tx_avail->ring[avail_idx % g_tx_qsize] = 0;
    __asm__ volatile ("" : : : "memory");
    g_tx_avail->idx = avail_idx + 1;

    /* Notify host of TX */
    outw(g_vnet_io_base + VIRTIO_REG_QUEUE_NOTIFY, VIRTIO_NET_QUEUE_TX);

    if (netif) {
        netif->tx_packets++;
        netif->tx_bytes += buf->len;
    }

    return 0;
}

void virtio_net_poll(netif_t *netif) {
    if (!g_vnet_io_base || !g_rx_used)
        return;

    while (g_rx_last_used_idx != g_rx_used->idx) {
        uint16_t used_elem_idx = g_rx_last_used_idx % g_rx_qsize;
        uint32_t desc_id = g_rx_used->ring[used_elem_idx].id;
        uint32_t len = g_rx_used->ring[used_elem_idx].len;

        if (desc_id < VIRTIO_NET_NUM_BUFFERS && len > sizeof(virtio_net_hdr_t)) {
            uint32_t pkt_len = len - (uint32_t)sizeof(virtio_net_hdr_t);
            uint8_t *pkt_data = g_rx_bufs[desc_id] + sizeof(virtio_net_hdr_t);

            if (pkt_len <= NET_MAX_PACKET_SIZE) {
                net_buf_t *buf = net_buf_alloc();
                if (buf) {
                    memcpy(buf->data, pkt_data, pkt_len);
                    buf->len = pkt_len;
                    buf->offset = 0;

                    if (netif) {
                        netif->rx_packets++;
                        netif->rx_bytes += pkt_len;
                        netif_input(netif, buf);
                    } else {
                        net_buf_free(buf);
                    }
                }
            }

            /* Recycle buffer back into available ring */
            uint16_t avail_idx = g_rx_avail->idx;
            g_rx_avail->ring[avail_idx % g_rx_qsize] = (uint16_t)desc_id;
            __asm__ volatile ("" : : : "memory");
            g_rx_avail->idx = avail_idx + 1;
        }

        g_rx_last_used_idx++;
    }
}

bool virtio_net_init(pci_device_t *pci_dev) {
    if (!pci_dev)
        return false;

    g_vnet_pci = pci_dev;
    pci_enable_bus_mastering(pci_dev);

    /* BAR0 contains I/O port base address */
    g_vnet_io_base = (uint16_t)(pci_dev->bar[0] & ~0x3);
    if (!g_vnet_io_base)
        return false;

    /* 1. Reset device */
    outb(g_vnet_io_base + VIRTIO_REG_DEVICE_STATUS, 0);

    /* 2. Acknowledge and Driver status */
    outb(g_vnet_io_base + VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);

    /* 3. Read features and negotiate */
    uint32_t host_features = inl(g_vnet_io_base + VIRTIO_REG_DEVICE_FEATURES);
    uint32_t guest_features = host_features & VIRTIO_NET_F_MAC;
    outl(g_vnet_io_base + VIRTIO_REG_GUEST_FEATURES, guest_features);

    /* 4. Read MAC address */
    uint8_t mac[6];
    for (int i = 0; i < 6; i++) {
        mac[i] = inb(g_vnet_io_base + VIRTIO_REG_MAC + i);
    }

    /* 5. Setup Queue 0 (RX) */
    outw(g_vnet_io_base + VIRTIO_REG_QUEUE_SEL, VIRTIO_NET_QUEUE_RX);
    g_rx_qsize = inw(g_vnet_io_base + VIRTIO_REG_QUEUE_NUM);
    if (g_rx_qsize == 0)
        return false;

    uintptr_t rx_ring_phys = 0;
    size_t rx_ring_size = (sizeof(vring_desc_t) * g_rx_qsize) +
                          (sizeof(uint16_t) * (3 + g_rx_qsize)) + 4096 +
                          (sizeof(vring_used_elem_t) * g_rx_qsize) + 32;
    void *rx_ring_virt = alloc_dma_zero(rx_ring_size, &rx_ring_phys);

    g_rx_desc = (vring_desc_t *)rx_ring_virt;
    g_rx_avail = (vring_avail_t *)((uintptr_t)g_rx_desc + (sizeof(vring_desc_t) * g_rx_qsize));
    uintptr_t used_offset = ((uintptr_t)g_rx_avail + sizeof(uint16_t) * (3 + g_rx_qsize) + 4095) & ~4095ULL;
    g_rx_used = (vring_used_t *)used_offset;

    outl(g_vnet_io_base + VIRTIO_REG_QUEUE_PFN, (uint32_t)(rx_ring_phys >> 12));

    /* Populate RX buffers */
    for (uint16_t i = 0; i < VIRTIO_NET_NUM_BUFFERS && i < g_rx_qsize; i++) {
        g_rx_bufs[i] = (uint8_t *)alloc_dma_zero(VIRTIO_NET_BUF_SIZE, &g_rx_bufs_phys[i]);
        g_rx_desc[i].addr = (uint64_t)g_rx_bufs_phys[i];
        g_rx_desc[i].len = VIRTIO_NET_BUF_SIZE;
        g_rx_desc[i].flags = VRING_DESC_F_WRITE;
        g_rx_desc[i].next = 0;

        g_rx_avail->ring[i] = i;
    }
    g_rx_avail->idx = VIRTIO_NET_NUM_BUFFERS < g_rx_qsize ? VIRTIO_NET_NUM_BUFFERS : g_rx_qsize;
    g_rx_last_used_idx = 0;

    /* 6. Setup Queue 1 (TX) */
    outw(g_vnet_io_base + VIRTIO_REG_QUEUE_SEL, VIRTIO_NET_QUEUE_TX);
    g_tx_qsize = inw(g_vnet_io_base + VIRTIO_REG_QUEUE_NUM);

    uintptr_t tx_ring_phys = 0;
    size_t tx_ring_size = (sizeof(vring_desc_t) * g_tx_qsize) +
                          (sizeof(uint16_t) * (3 + g_tx_qsize)) + 4096 +
                          (sizeof(vring_used_elem_t) * g_tx_qsize) + 32;
    void *tx_ring_virt = alloc_dma_zero(tx_ring_size, &tx_ring_phys);

    g_tx_desc = (vring_desc_t *)tx_ring_virt;
    g_tx_avail = (vring_avail_t *)((uintptr_t)g_tx_desc + (sizeof(vring_desc_t) * g_tx_qsize));
    used_offset = ((uintptr_t)g_tx_avail + sizeof(uint16_t) * (3 + g_tx_qsize) + 4095) & ~4095ULL;
    g_tx_used = (vring_used_t *)used_offset;

    outl(g_vnet_io_base + VIRTIO_REG_QUEUE_PFN, (uint32_t)(tx_ring_phys >> 12));

    g_tx_buf = (uint8_t *)alloc_dma_zero(VIRTIO_NET_BUF_SIZE, &g_tx_buf_phys);

    /* 7. Mark Driver OK */
    outb(g_vnet_io_base + VIRTIO_REG_DEVICE_STATUS,
         VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_DRIVER_OK);

    /* 8. Setup Network Interface */
    memset(&g_vnet_netif, 0, sizeof(netif_t));
    ksnprintf(g_vnet_netif.name, sizeof(g_vnet_netif.name), "vtnet0");
    memcpy(g_vnet_netif.mac, mac, 6);

    g_vnet_netif.ip = (10) | (0 << 8) | (2 << 16) | (17 << 24);
    g_vnet_netif.netmask = (255) | (255 << 8) | (255 << 16) | (0 << 24);
    g_vnet_netif.gateway = (10) | (0 << 8) | (2 << 16) | (2 << 24);
    g_vnet_netif.flags = NETIF_FLAG_UP | NETIF_FLAG_RUNNING;
    g_vnet_netif.send = virtio_net_send;
    g_vnet_netif.poll = virtio_net_poll;

    netif_register(&g_vnet_netif);

    klog_info("VirtIO-Net: Initialized interface vtnet0, MAC %02x:%02x:%02x:%02x:%02x:%02x (IP 10.0.2.17/24)",
              mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return true;
}
