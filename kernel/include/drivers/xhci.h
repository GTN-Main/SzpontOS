/*
 * SzpontOS - xHCI (Extensible Host Controller Interface) USB 3.0 Driver
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef SZPONTOS_DRIVERS_XHCI_H
#define SZPONTOS_DRIVERS_XHCI_H

#include <drivers/pci.h>
#include <drivers/usb.h>
#include <kernel/types.h>
#include <stdbool.h>

/* xHCI Capability Registers (Off Base) */
#define XHCI_CAP_CAPLENGTH 0x00
#define XHCI_CAP_HCIVERSION 0x02
#define XHCI_CAP_HCSPARAMS1 0x04
#define XHCI_CAP_HCSPARAMS2 0x08
#define XHCI_CAP_HCSPARAMS3 0x0C
#define XHCI_CAP_HCCPARAMS1 0x10
#define XHCI_CAP_DBOFF 0x14
#define XHCI_CAP_RTSOFF 0x18
#define XHCI_CAP_HCCPARAMS2 0x1C

/* xHCI Operational Registers (Off OpBase) */
#define XHCI_OP_USBCMD 0x00
#define XHCI_OP_USBSTS 0x04
#define XHCI_OP_PAGESIZE 0x08
#define XHCI_OP_DNCTRL 0x14
#define XHCI_OP_CRCR 0x18
#define XHCI_OP_DCBAAP 0x30
#define XHCI_OP_CONFIG 0x38
#define XHCI_OP_PORTSC_BASE 0x400

/* USBCMD Bits */
#define XHCI_CMD_RS (1U << 0)    /* Run/Stop */
#define XHCI_CMD_HCRST (1U << 1) /* Host Controller Reset */
#define XHCI_CMD_INTE (1U << 2)  /* Interrupter Enable */
#define XHCI_CMD_HSEE (1U << 3)  /* Host System Error Enable */

/* USBSTS Bits */
#define XHCI_STS_HCH (1U << 0)  /* HC Halted */
#define XHCI_STS_HSE (1U << 2)  /* Host System Error */
#define XHCI_STS_EINT (1U << 3) /* Event Interrupt */
#define XHCI_STS_PCD (1U << 4)  /* Port Change Detect */
#define XHCI_STS_SSS (1U << 8)  /* Save/Restore State Status */
#define XHCI_STS_RSS (1U << 9)  /* Restore State Status */
#define XHCI_STS_SRE (1U << 10) /* Save/Restore Error */
#define XHCI_STS_CNR (1U << 11) /* Controller Not Ready */
#define XHCI_STS_HCE (1U << 12) /* Host Controller Error */

/* PORTSC Bits */
#define XHCI_PORT_CCS (1U << 0)           /* Current Connect Status */
#define XHCI_PORT_PED (1U << 1)           /* Port Enabled/Disabled */
#define XHCI_PORT_OCA (1U << 3)           /* Over-current Active */
#define XHCI_PORT_PR (1U << 4)            /* Port Reset */
#define XHCI_PORT_PLS_MASK (0xFU << 5)    /* Port Link State */
#define XHCI_PORT_PP (1U << 9)            /* Port Power */
#define XHCI_PORT_SPEED_MASK (0xFU << 10) /* Port Speed */
#define XHCI_PORT_CSC (1U << 17)          /* Connect Status Change (W1C) */
#define XHCI_PORT_PEC (1U << 18)          /* Port Enabled/Disabled Change */
#define XHCI_PORT_WRC (1U << 19)          /* Warm Port Reset Change */
#define XHCI_PORT_OCC (1U << 20)          /* Over-current Change */
#define XHCI_PORT_PRC (1U << 21)          /* Port Reset Change */
#define XHCI_PORT_PLC (1U << 22)          /* Port Link State Change */
#define XHCI_PORT_CEC (1U << 23)          /* Port Config Error Change */

#define XHCI_PORT_W1C_ALL                                                                                              \
    (XHCI_PORT_CSC | XHCI_PORT_PEC | XHCI_PORT_WRC | XHCI_PORT_OCC | XHCI_PORT_PRC | XHCI_PORT_PLC | XHCI_PORT_CEC)

/* Extended Capability IDs */
#define XHCI_EXT_CAP_LEGSUP 0x01
#define XHCI_EXT_CAP_PROTOCOL 0x02

/* USBLEGSUP Register Bits */
#define XHCI_LEGSUP_BIOS_OWNED (1U << 16)
#define XHCI_LEGSUP_OS_OWNED (1U << 24)

/* TRB Types */
#define XHCI_TRB_NORMAL 1
#define XHCI_TRB_SETUP_STAGE 2
#define XHCI_TRB_DATA_STAGE 3
#define XHCI_TRB_STATUS_STAGE 4
#define XHCI_TRB_ISOCH 5
#define XHCI_TRB_LINK 6
#define XHCI_TRB_EVENT_DATA 7
#define XHCI_TRB_NOOP 8
#define XHCI_TRB_ENABLE_SLOT_CMD 9
#define XHCI_TRB_DISABLE_SLOT_CMD 10
#define XHCI_TRB_ADDRESS_DEV_CMD 11
#define XHCI_TRB_CONFIG_EP_CMD 12
#define XHCI_TRB_EVAL_CTX_CMD 13
#define XHCI_TRB_RESET_EP_CMD 14
#define XHCI_TRB_STOP_EP_CMD 15
#define XHCI_TRB_SET_TR_DEQUEUE_CMD 16
#define XHCI_TRB_RESET_DEV_CMD 17
#define XHCI_TRB_NOOP_CMD 23
#define XHCI_TRB_TRANSFER_EVENT 32
#define XHCI_TRB_CMD_COMPLETION 33
#define XHCI_TRB_PORT_STATUS_CHANGE 34
#define XHCI_TRB_HOST_CTRL_EVENT 37
#define XHCI_TRB_DEVICE_NOTIFY_EVENT 38
#define XHCI_TRB_MFINDEX_WRAP_EVENT 39

/* TRB Structure (16 bytes) */
typedef struct __attribute__((packed, aligned(16))) {
    uint64_t parameter;
    uint32_t status;
    uint32_t control;
} xhci_trb_t;

/* Event Ring Segment Table Entry (16 bytes) */
typedef struct __attribute__((packed, aligned(16))) {
    uint64_t ring_segment_base_address;
    uint16_t ring_segment_size;
    uint16_t reserved1;
    uint32_t reserved2;
} xhci_erst_entry_t;

/* Slot Context (32 bytes) */
typedef struct __attribute__((packed, aligned(32))) {
    uint32_t info1; /* Route String, Speed, MTT, Hub, Context Entries */
    uint32_t info2; /* Max Exit Latency, Root Hub Port Number, Number of Ports */
    uint32_t tt;    /* TT Hub Slot ID, TT Port Number, Interrupter Target */
    uint32_t state; /* Device Address, Slot State */
    uint32_t rsvd[4];
} xhci_slot_ctx_t;

/* Endpoint Context (32 bytes) */
typedef struct __attribute__((packed, aligned(32))) {
    uint32_t info1;          /* EP State, Mult, MaxPStreams, LSA, Interval */
    uint32_t info2;          /* Force Event, CErr, EP Type, Max Burst Size, Max Packet Size */
    uint64_t tr_dequeue_ptr; /* DCS, TR Dequeue Pointer */
    uint32_t avg_trb_len;    /* Average TRB Length, Max ESIT Payload */
    uint32_t rsvd[3];
} xhci_ep_ctx_t;

/* Input Control Context (32 bytes) */
typedef struct __attribute__((packed, aligned(32))) {
    uint32_t drop_flags;
    uint32_t add_flags;
    uint32_t rsvd[6];
} xhci_input_ctrl_ctx_t;

#define XHCI_RING_SIZE 64

typedef struct {
    xhci_trb_t *trbs;
    uintptr_t trbs_phys;
    size_t enqueue_ptr;
    size_t dequeue_ptr;
    uint8_t cycle_state;
    size_t size;
} xhci_ring_t;

/* xHCI Device State */
typedef struct {
    uint8_t slot_id;
    uint8_t port_num;
    uint8_t speed;
    uint16_t vendor_id;
    uint16_t product_id;
    bool is_keyboard;
    uint8_t ep_in_addr;
    uint8_t ep_in_ctx_idx;
    uint16_t max_packet_size;
    xhci_ring_t ep0_ring;
    xhci_ring_t ep_in_ring;
    void *input_ctx;
    uintptr_t input_ctx_phys;
    void *device_ctx;
    uintptr_t device_ctx_phys;
    uint8_t report_buffer[8];
    uintptr_t report_buffer_phys;
} xhci_device_t;

/* xHCI Controller State */
typedef struct {
    pci_device_t *pci_dev;
    uintptr_t mmio_base;
    uintptr_t cap_base;
    uintptr_t op_base;
    uintptr_t rt_base;
    uintptr_t db_base;
    uint8_t max_slots;
    uint8_t max_ports;
    uint8_t ctx_size; /* 32 or 64 bytes (CSZ bit) */
    uint32_t pagesize;

    /* DCBAA (Device Context Base Address Array) */
    uint64_t *dcbaa;
    uintptr_t dcbaa_phys;

    /* Command Ring */
    xhci_ring_t cmd_ring;

    /* Event Ring & ERST */
    xhci_trb_t *event_ring;
    uintptr_t event_ring_phys;
    size_t event_ring_dequeue;
    uint8_t event_ring_cycle;
    xhci_erst_entry_t *erst;
    uintptr_t erst_phys;

    /* Devices */
    xhci_device_t devices[16];
    size_t device_count;

    bool initialized;
} xhci_controller_t;

void xhci_init(void);
void xhci_poll(void);
bool xhci_is_active(void);
size_t xhci_get_device_list_info(char *buf, size_t max_len);

#endif /* SZPONTOS_DRIVERS_XHCI_H */
