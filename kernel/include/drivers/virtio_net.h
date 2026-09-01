/*
 * SzpontOS - VirtIO Network Driver Header (<drivers/virtio_net.h>)
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef SZPONTOS_DRIVERS_VIRTIO_NET_H
#define SZPONTOS_DRIVERS_VIRTIO_NET_H

#include <kernel/types.h>
#include <drivers/pci.h>
#include <net/net.h>

#define VIRTIO_NET_VENDOR_ID  0x1AF4
#define VIRTIO_NET_DEVICE_ID  0x1000

/* VirtIO PCI Status Register Bits */
#define VIRTIO_STATUS_ACKNOWLEDGE 1
#define VIRTIO_STATUS_DRIVER      2
#define VIRTIO_STATUS_DRIVER_OK   4
#define VIRTIO_STATUS_FAILED      128

/* VirtIO PCI Register Offsets */
#define VIRTIO_REG_DEVICE_FEATURES 0x00
#define VIRTIO_REG_GUEST_FEATURES  0x04
#define VIRTIO_REG_QUEUE_PFN       0x08
#define VIRTIO_REG_QUEUE_NUM       0x0C
#define VIRTIO_REG_QUEUE_SEL       0x0E
#define VIRTIO_REG_QUEUE_NOTIFY    0x10
#define VIRTIO_REG_DEVICE_STATUS   0x12
#define VIRTIO_REG_ISR_STATUS      0x13
#define VIRTIO_REG_MAC             0x14

/* VirtIO Network Feature Bits */
#define VIRTIO_NET_F_MAC (1 << 5)

/* VirtIO Net Header */
typedef struct __attribute__((packed)) {
    uint8_t flags;
    uint8_t gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
} virtio_net_hdr_t;

/* Virtqueue Descriptor */
typedef struct __attribute__((packed)) {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} vring_desc_t;

#define VRING_DESC_F_NEXT     1
#define VRING_DESC_F_WRITE    2
#define VRING_DESC_F_INDIRECT 4

/* Virtqueue Available Ring */
typedef struct __attribute__((packed)) {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
} vring_avail_t;

/* Virtqueue Used Element */
typedef struct __attribute__((packed)) {
    uint32_t id;
    uint32_t len;
} vring_used_elem_t;

/* Virtqueue Used Ring */
typedef struct __attribute__((packed)) {
    uint16_t flags;
    uint16_t idx;
    vring_used_elem_t ring[];
} vring_used_t;

bool virtio_net_init(pci_device_t *pci_dev);
void virtio_net_poll(netif_t *netif);

#endif /* SZPONTOS_DRIVERS_VIRTIO_NET_H */
