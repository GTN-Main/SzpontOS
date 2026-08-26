/*
 * SzpontOS - EHCI (Enhanced Host Controller Interface - USB 2.0) Driver
 * High-Level Subsystem Structures and Public API Definitions.
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef SZPONTOS_DRIVERS_EHCI_H
#define SZPONTOS_DRIVERS_EHCI_H

#include <drivers/ehci_hw.h>
#include <drivers/pci.h>
#include <drivers/usb.h>
#include <kernel/spinlock.h>

#define EHCI_MAX_DEVICES          16
#define EHCI_FRAMELIST_COUNT      1024
#define EHCI_FRAMELIST_SIZE       (EHCI_FRAMELIST_COUNT * sizeof(uint32_t))

/* EHCI Logical Device / Endpoint Pipe Descriptor */
typedef struct ehci_device {
    bool active;
    uint8_t address;
    uint8_t port_num;
    uint8_t speed;
    uint16_t vendor_id;
    uint16_t product_id;
    uint8_t device_class;
    bool is_keyboard;
    bool is_mouse;
    bool is_hub;

    /* Upstream Hub Information (for Split Transactions) */
    uint8_t parent_hub_addr;
    uint8_t parent_hub_port;

    /* Control Pipe Attributes */
    uint16_t ep0_max_packet;

    /* Interrupt IN Endpoint (for Keyboards/Mice) */
    uint8_t ep_in_num;
    uint16_t ep_in_max_packet;
    uint8_t ep_in_interval;
    uint8_t ep_in_toggle;

    /* DMA Hardware Structures */
    ehci_qh_t *intr_qh;
    uint32_t intr_qh_phys;

    ehci_qtd_t *intr_qtd;
    uint32_t intr_qtd_phys;

    uintptr_t report_buf_phys;
    uint8_t *report_buf_virt;
    size_t report_len;
} ehci_device_t;

/* EHCI Host Controller Software State (softc) */
typedef struct ehci_controller {
    pci_device_t *pci_dev;
    uintptr_t cap_base;
    uintptr_t op_base;
    uint8_t cap_length;
    uint16_t version;
    uint8_t max_ports;
    bool has_64bit;
    bool initialized;

    /* Periodic Schedule Frame List (1024 frames = 4096 bytes) */
    uint32_t *periodic_frame_list;
    uintptr_t periodic_frame_list_phys;

    /* Asynchronous Schedule Ring (Circular QH List) */
    ehci_qh_t *async_head_qh;
    uintptr_t async_head_qh_phys;

    /* Control Transfer Temporary QH/qTD DMA Pool */
    ehci_qh_t *ctrl_qh;
    uintptr_t ctrl_qh_phys;
    ehci_qtd_t *ctrl_qtd_pool;
    uintptr_t ctrl_qtd_pool_phys;
    uint8_t *ctrl_data_buf;
    uintptr_t ctrl_data_buf_phys;

    /* Enumerated Devices Pool */
    ehci_device_t devices[EHCI_MAX_DEVICES];
    size_t device_count;
    uint8_t next_address;

    /* Intel Rate Matching Hub (RMH) Subsystem State */
    bool has_rmh_hub;
    uint8_t rmh_hub_addr;
    uint8_t rmh_hub_ports;
    uint8_t rmh_hub_speed;
    uint32_t pnp_poll_counter;

    spinlock_t lock;
} ehci_controller_t;

/* Public Subsystem API */
void ehci_init(void);
void ehci_poll(void);
bool ehci_is_active(void);
size_t ehci_get_device_list_info(char *buf, size_t max_len);

#endif /* SZPONTOS_DRIVERS_EHCI_H */
