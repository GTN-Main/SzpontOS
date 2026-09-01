/*
 * SzpontOS - Network Interface Management & Packet Buffer Subsystem
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <net/net.h>
#include <mm/heap.h>
#include <kernel/kprint.h>
#include <kernel/string.h>

static netif_t *g_netif_list = NULL;
static spinlock_t g_netif_lock = SPINLOCK_INIT;

net_buf_t *net_buf_alloc(void) {
    net_buf_t *buf = (net_buf_t *)kmalloc(sizeof(net_buf_t));
    if (buf) {
        memset(buf, 0, sizeof(net_buf_t));
    }
    return buf;
}

void net_buf_free(net_buf_t *buf) {
    if (buf) {
        kfree(buf);
    }
}

void netif_register(netif_t *nif) {
    if (!nif)
        return;

    spinlock_acquire(&g_netif_lock);
    nif->next = g_netif_list;
    g_netif_list = nif;
    spinlock_release(&g_netif_lock);

    klog_info("NET: Registered interface '%s'", nif->name);
}

netif_t *netif_get_default(void) {
    spinlock_acquire(&g_netif_lock);
    for (netif_t *cur = g_netif_list; cur != NULL; cur = cur->next) {
        if (!(cur->flags & NETIF_FLAG_LOOPBACK) && (cur->flags & NETIF_FLAG_UP)) {
            spinlock_release(&g_netif_lock);
            return cur;
        }
    }
    netif_t *lo = netif_get_loopback();
    spinlock_release(&g_netif_lock);
    return lo;
}

netif_t *netif_get_loopback(void) {
    for (netif_t *cur = g_netif_list; cur != NULL; cur = cur->next) {
        if (cur->flags & NETIF_FLAG_LOOPBACK) {
            return cur;
        }
    }
    return NULL;
}

netif_t *netif_find_by_name(const char *name) {
    if (!name)
        return NULL;
    spinlock_acquire(&g_netif_lock);
    for (netif_t *cur = g_netif_list; cur != NULL; cur = cur->next) {
        if (strcmp(cur->name, name) == 0) {
            spinlock_release(&g_netif_lock);
            return cur;
        }
    }
    spinlock_release(&g_netif_lock);
    return NULL;
}

netif_t *netif_find_by_ip(uint32_t ip) {
    spinlock_acquire(&g_netif_lock);
    for (netif_t *cur = g_netif_list; cur != NULL; cur = cur->next) {
        if (cur->ip == ip) {
            spinlock_release(&g_netif_lock);
            return cur;
        }
    }
    spinlock_release(&g_netif_lock);
    return NULL;
}

netif_t *netif_get_list(void) {
    return g_netif_list;
}

void netif_input(netif_t *netif, net_buf_t *buf) {
    if (!netif || !buf || buf->len < sizeof(eth_hdr_t)) {
        if (buf)
            net_buf_free(buf);
        return;
    }

    eth_hdr_t *eth = (eth_hdr_t *)(buf->data + buf->offset);
    uint16_t eth_type = ntohs(eth->type);

    /* Advance buffer offset past Ethernet header */
    buf->offset += sizeof(eth_hdr_t);
    buf->len -= sizeof(eth_hdr_t);

    if (eth_type == ETH_P_ARP) {
        arp_input(netif, buf);
    } else if (eth_type == ETH_P_IP) {
        ipv4_input(netif, buf);
    } else {
        net_buf_free(buf);
    }
}

int netif_output(netif_t *netif, net_buf_t *buf) {
    if (!netif || !buf || !netif->send) {
        if (buf)
            net_buf_free(buf);
        return -1;
    }
    int res = netif->send(netif, buf);
    net_buf_free(buf);
    return res;
}

void netif_poll_all(void) {
    netif_t *poll_list[8];
    size_t count = 0;

    spinlock_acquire(&g_netif_lock);
    for (netif_t *cur = g_netif_list; cur != NULL && count < 8; cur = cur->next) {
        if (cur->poll && (cur->flags & NETIF_FLAG_UP)) {
            poll_list[count++] = cur;
        }
    }
    spinlock_release(&g_netif_lock);

    for (size_t i = 0; i < count; i++) {
        poll_list[i]->poll(poll_list[i]);
    }
}

#include <drivers/pci.h>
extern bool e1000_init(pci_device_t *pci_dev);
extern bool virtio_net_init(pci_device_t *pci_dev);
extern void rtl8139_init(void);

void netif_init_drivers(void) {
    pci_device_t *pci_list = pci_get_device_list();
    bool found_nic = false;

    for (pci_device_t *dev = pci_list; dev != NULL; dev = dev->next) {
        if (dev->class_code == 0x02 /* PCI_CLASS_NETWORK */) {
            if (dev->vendor_id == 0x8086) {
                /* Intel E1000 Gigabit Network Cards (82540EM, 82545EM, 82574L, etc.) */
                if (dev->device_id == 0x100e || dev->device_id == 0x1004 || dev->device_id == 0x100f ||
                    dev->device_id == 0x10d3 || dev->device_id == 0x1079 || dev->device_id == 0x107c ||
                    dev->device_id == 0x1539) {
                    klog_info("NET: Initializing Intel Gigabit NIC [0x%04x:0x%04x] on PCI %02x:%02x.%d",
                              dev->vendor_id, dev->device_id, dev->bus, dev->slot, dev->func);
                    if (e1000_init(dev)) {
                        found_nic = true;
                    }
                }
            } else if (dev->vendor_id == 0x1af4) {
                /* VirtIO Network Adapter */
                if (dev->device_id == 0x1000 || dev->device_id == 0x1041) {
                    klog_info("NET: Initializing VirtIO-Net NIC [0x%04x:0x%04x] on PCI %02x:%02x.%d",
                              dev->vendor_id, dev->device_id, dev->bus, dev->slot, dev->func);
                    if (virtio_net_init(dev)) {
                        found_nic = true;
                    }
                }
            } else if (dev->vendor_id == 0x10ec) {
                /* Realtek RTL8139 10/100 Ethernet */
                if (dev->device_id == 0x8139) {
                    klog_info("NET: Initializing Realtek RTL8139 NIC [0x%04x:0x%04x] on PCI %02x:%02x.%d",
                              dev->vendor_id, dev->device_id, dev->bus, dev->slot, dev->func);
                    rtl8139_init();
                    found_nic = true;
                }
            }
        }
    }

    if (!found_nic) {
        /* Fallback probe for older emulated configurations */
        pci_device_t *e1000_dev = pci_find_device(0x8086, 0x100e);
        if (e1000_dev) {
            e1000_init(e1000_dev);
            found_nic = true;
        }
    }

    if (!found_nic) {
        klog_info("NET: No physical PCI Ethernet NIC detected, operating in Loopback mode");
    }
}

