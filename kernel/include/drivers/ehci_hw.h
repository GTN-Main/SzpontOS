/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 * (C) Copyright by Szpont Industries. All rights reserved.
 *
 * Complete EHCI (Enhanced Host Controller Interface) Hardware Specification
 * Direct FreeBSD-derived register and descriptor definitions for SzpontOS.
 */

#ifndef SZPONTOS_DRIVERS_EHCI_HW_H
#define SZPONTOS_DRIVERS_EHCI_HW_H

#include <kernel/types.h>
#include <stdbool.h>

/* ==============================================================================
 * EHCI PCI Configuration Registers
 * ============================================================================== */
#define EHCI_PCI_SBRN             0x60   /* Serial Bus Release Number */
#define EHCI_PCI_FLADJ            0x61   /* Frame Length Adjustment */
#define EHCI_PCI_PORTWAKECAP      0x62   /* Port Wake Capabilities */
#define EHCI_PCI_USBLEGSUP        0x00   /* Offset in EECP: Legacy Support */
#define EHCI_PCI_USBLEGCTLSTS     0x04   /* Offset in EECP: Legacy Control/Status */

#define EHCI_USBLEGSUP_BIOS_OWNED (1 << 16)
#define EHCI_USBLEGSUP_OS_OWNED   (1 << 24)

/* ==============================================================================
 * EHCI Capability Registers (Relative to CAPBASE)
 * ============================================================================== */
#define EHCI_CAPLENGTH            0x00   /* Capability Register Length (1 byte) */
#define EHCI_HCIVERSION           0x02   /* Host Controller Interface Version (2 bytes) */
#define EHCI_HCSPARAMS            0x04   /* Structural Parameters (4 bytes) */
#define EHCI_HCCPARAMS            0x08   /* Capability Parameters (4 bytes) */
#define EHCI_HCSP_PORTROUTE       0x0C   /* Companion Port Route Description */

#define EHCI_HCS_N_PORTS(p)       (((p) >> 0) & 0x0F)
#define EHCI_HCS_PPC(p)           (((p) >> 4) & 0x01)  /* Port Power Control */
#define EHCI_HCS_N_PCC(p)         (((p) >> 8) & 0x0F)  /* Number of Companion Controllers */
#define EHCI_HCS_N_CC(p)          (((p) >> 12) & 0x0F) /* Number of Ports per CC */
#define EHCI_HCS_P_INDICATOR(p)   (((p) >> 16) & 0x01) /* Port Indicators */
#define EHCI_HCS_DP_ROUTING(p)    (((p) >> 20) & 0x01) /* Direct Port Routing */

#define EHCI_HCC_64BIT(p)         (((p) >> 0) & 0x01)  /* 64-bit Addressing Capability */
#define EHCI_HCC_PFL(p)           (((p) >> 1) & 0x01)  /* Programmable Frame List Flag */
#define EHCI_HCC_ASPC(p)          (((p) >> 2) & 0x01)  /* Asynchronous Schedule Park Cap */
#define EHCI_HCC_IST(p)           (((p) >> 4) & 0x0F)  /* Isochronous Scheduling Threshold */
#define EHCI_HCC_EECP(p)          (((p) >> 8) & 0xFF)  /* EHCI Extended Capabilities Pointer */

/* ==============================================================================
 * EHCI Operational Registers (Relative to OPBASE = CAPBASE + CAPLENGTH)
 * ============================================================================== */
#define EHCI_USBCMD               0x00   /* USB Command Register */
#define EHCI_USBSTS               0x04   /* USB Status Register */
#define EHCI_USBINTR              0x08   /* USB Interrupt Enable Register */
#define EHCI_FRINDEX              0x0C   /* USB Frame Index Register */
#define EHCI_CTRLDSSEGMENT        0x10   /* 4G Segment Selector Register */
#define EHCI_PERIODICLISTBASE     0x14   /* Frame List Base Address Register */
#define EHCI_ASYNCLISTADDR        0x18   /* Next Asynchronous List Address Register */
#define EHCI_CONFIGFLAG           0x40   /* Configured Flag Register */
#define EHCI_PORTSC_BASE          0x44   /* Port Status/Control Register Base */

/* USBCMD Register Bits */
#define EHCI_CMD_RS               (1 << 0)   /* Run / Stop (1 = Run, 0 = Stop) */
#define EHCI_CMD_HCRESET          (1 << 1)   /* Host Controller Reset */
#define EHCI_CMD_FLS_1024         (0 << 2)   /* Frame List Size: 1024 entries (4096 bytes) */
#define EHCI_CMD_FLS_512          (1 << 2)   /* Frame List Size: 512 entries (2048 bytes) */
#define EHCI_CMD_FLS_256          (2 << 2)   /* Frame List Size: 256 entries (1024 bytes) */
#define EHCI_CMD_FLS_MASK         (3 << 2)
#define EHCI_CMD_PSE              (1 << 4)   /* Periodic Schedule Enable */
#define EHCI_CMD_ASE              (1 << 5)   /* Asynchronous Schedule Enable */
#define EHCI_CMD_IAAD             (1 << 6)   /* Interrupt on Async Advance Doorbell */
#define EHCI_CMD_LHCR             (1 << 7)   /* Light Host Controller Reset */
#define EHCI_CMD_ITC_IMM          (0x00 << 16) /* Interrupt Threshold: Immediate (0) */
#define EHCI_CMD_ITC_1            (0x01 << 16) /* Interrupt Threshold: 1 microframe */
#define EHCI_CMD_ITC_2            (0x02 << 16) /* Interrupt Threshold: 2 microframes */
#define EHCI_CMD_ITC_4            (0x04 << 16) /* Interrupt Threshold: 4 microframes */
#define EHCI_CMD_ITC_8            (0x08 << 16) /* Interrupt Threshold: 8 microframes (1 ms default) */

/* USBSTS Register Bits */
#define EHCI_STS_USBINT           (1 << 0)   /* USB Interrupt (Transfer complete with IOC) */
#define EHCI_STS_USBERRINT        (1 << 1)   /* USB Error Interrupt */
#define EHCI_STS_PCD              (1 << 2)   /* Port Change Detect */
#define EHCI_STS_FLR              (1 << 3)   /* Frame List Rollover */
#define EHCI_STS_HSE              (1 << 4)   /* Host System Error (PCI parity, fatal DMA) */
#define EHCI_STS_IAA              (1 << 5)   /* Interrupt on Async Advance */
#define EHCI_STS_HCH              (1 << 12)  /* Host Controller Halted */
#define EHCI_STS_REC              (1 << 13)  /* Reclamation */
#define EHCI_STS_PSS              (1 << 14)  /* Periodic Schedule Status */
#define EHCI_STS_ASS              (1 << 15)  /* Asynchronous Schedule Status */

/* USBINTR Register Bits */
#define EHCI_INTR_USBINT          (1 << 0)
#define EHCI_INTR_USBERRINT       (1 << 1)
#define EHCI_INTR_PCD             (1 << 2)
#define EHCI_INTR_FLR             (1 << 3)
#define EHCI_INTR_HSE             (1 << 4)
#define EHCI_INTR_IAA             (1 << 5)

/* PORTSC Register Bits */
#define EHCI_PORT_CCS             (1 << 0)   /* Current Connect Status (1 = device connected) */
#define EHCI_PORT_CSC             (1 << 1)   /* Connect Status Change (W1C) */
#define EHCI_PORT_PE              (1 << 2)   /* Port Enabled / Disabled */
#define EHCI_PORT_PEC             (1 << 3)   /* Port Enable/Disable Change (W1C) */
#define EHCI_PORT_OCA             (1 << 4)   /* Over-current Active */
#define EHCI_PORT_OCC             (1 << 5)   /* Over-current Change (W1C) */
#define EHCI_PORT_FPR             (1 << 6)   /* Force Port Resume */
#define EHCI_PORT_SUSP            (1 << 7)   /* Port Suspend */
#define EHCI_PORT_PR              (1 << 8)   /* Port Reset (1 = in reset) */
#define EHCI_PORT_HSP             (1 << 9)   /* High-Speed Status (RO) */
#define EHCI_PORT_LINE_STATUS     (3 << 10)  /* Line Status: D+ (bit 10), D- (bit 11) */
#define EHCI_PORT_PP              (1 << 12)  /* Port Power (1 = powered) */
#define EHCI_PORT_PO              (1 << 13)  /* Port Owner (1 = Companion UHCI/OHCI) */
#define EHCI_PORT_PIC_MASK        (3 << 14)  /* Port Indicator Control */
#define EHCI_PORT_PTC_MASK        (15 << 16) /* Port Test Control */
#define EHCI_PORT_WKCNNT_E        (1 << 20)  /* Wake on Connect Enable */
#define EHCI_PORT_WKDSCNNT_E      (1 << 21)  /* Wake on Disconnect Enable */
#define EHCI_PORT_WKOC_E          (1 << 22)  /* Wake on Over-current Enable */

/* Port W1C Mask: Writing 1 to these bits clears status changes */
#define EHCI_PORT_W1C_MASK        (EHCI_PORT_CSC | EHCI_PORT_PEC | EHCI_PORT_OCC)

/* CONFIGFLAG Register */
#define EHCI_CONFIGFLAG_ROUTE     (1 << 0)   /* 1 = Route all ports to EHCI */

/* ==============================================================================
 * EHCI Physical Link Pointers
 * ============================================================================== */
#define EHCI_LINK_TERMINATE       (1 << 0)   /* 1 = Terminate / End of chain */
#define EHCI_LINK_TYPE_ITD        (0 << 1)   /* Isochronous Transfer Descriptor */
#define EHCI_LINK_TYPE_QH         (1 << 1)   /* Queue Head */
#define EHCI_LINK_TYPE_SITD       (2 << 1)   /* Split Isochronous Transfer Descriptor */
#define EHCI_LINK_TYPE_FSTN       (3 << 1)   /* Frame Span Traversal Node */

/* ==============================================================================
 * EHCI Queue Transfer Descriptor (qTD) - 32 bytes minimum, 32-byte aligned
 * ============================================================================== */
typedef struct __attribute__((packed, aligned(32))) ehci_qtd {
    uint32_t qtd_next;          /* Next qTD physical address | Terminate */
    uint32_t qtd_altnext;       /* Alternate Next qTD physical address (on error/short) */
    uint32_t qtd_status;        /* Token: Status, PID, CERR, Bytes, Toggle, IOC */
    uint32_t qtd_buffer[5];     /* 5 Page Buffer pointers (Up to 20 KiB per qTD) */
    uint32_t qtd_buffer_hi[5];  /* 64-bit addressing high dwords (if 64-bit capable) */

    /* Software tracking fields */
    uint32_t qtd_self_phys;
    void *buf_virt;
    size_t length;
    bool active_in_hw;
} ehci_qtd_t;

/* qTD Token Status Field Definitions */
#define EHCI_QTD_PING_ERR         (1 << 0)   /* Ping State / Out Token Error */
#define EHCI_QTD_SPLIT_ERR        (1 << 1)   /* Split Transaction State Error */
#define EHCI_QTD_MMF              (1 << 2)   /* Missed Micro-Frame */
#define EHCI_QTD_XACT_ERR         (1 << 3)   /* Transaction Error (CRC, Timeout, Bad PID) */
#define EHCI_QTD_BABBLE           (1 << 4)   /* Babble Detected */
#define EHCI_QTD_DATA_BUFF_ERR    (1 << 5)   /* Data Buffer Error (Host System Over/Underrun) */
#define EHCI_QTD_HALTED           (1 << 6)   /* Halted (Fatal transfer error) */
#define EHCI_QTD_ACTIVE           (1 << 7)   /* Active (1 = Executing in HW DMA) */

#define EHCI_QTD_PID_OUT          (0 << 8)   /* OUT PID (00) */
#define EHCI_QTD_PID_IN           (1 << 8)   /* IN PID (01) */
#define EHCI_QTD_PID_SETUP        (2 << 8)   /* SETUP PID (10) */

#define EHCI_QTD_CERR(n)          (((n) & 3) << 10) /* Error Counter (3 = max retries) */
#define EHCI_QTD_CERR_MAX         EHCI_QTD_CERR(3)
#define EHCI_QTD_GET_CERR(t)      (((t) >> 10) & 3)

#define EHCI_QTD_CPAGE(n)         (((n) & 7) << 12) /* Current Page Index (0..4) */
#define EHCI_QTD_IOC              (1 << 15)  /* Interrupt On Complete */
#define EHCI_QTD_BYTES(n)         (((n) & 0x7FFF) << 16) /* Total Bytes to Transfer */
#define EHCI_QTD_GET_BYTES(t)     (((t) >> 16) & 0x7FFF)
#define EHCI_QTD_TOGGLE           (1U << 31) /* Data Toggle (0 = DATA0, 1 = DATA1) */

/* ==============================================================================
 * EHCI Queue Head (QH) - 68 bytes minimum, 64-byte aligned
 * ============================================================================== */
typedef struct __attribute__((packed, aligned(64))) ehci_qh {
    uint32_t qh_link;           /* Horizontal Link Pointer (Next QH physical address) */
    uint32_t qh_endp;           /* Endpoint Characteristics */
    uint32_t qh_endphub;        /* Endpoint Capabilities (Split transaction parameters) */
    uint32_t qh_curqtd;         /* Current executing qTD physical address (RO by HW) */

    /* Overlay Area (Hardware copies qTD here during active processing) */
    uint32_t qtd_next;
    uint32_t qtd_altnext;
    uint32_t qtd_status;
    uint32_t qtd_buffer[5];
    uint32_t qtd_buffer_hi[5];

    /* Software tracking fields */
    uint32_t qh_self_phys;
    struct ehci_qh *next_qh_virt;
    ehci_qtd_t *first_qtd;
    bool in_periodic_schedule;
} ehci_qh_t;

/* Endpoint Characteristics (qh_endp) Bits */
#define EHCI_QH_DEV_ADDR(a)       (((a) & 0x7F) << 0)
#define EHCI_QH_INACTIVATE        (1 << 7)   /* Inactivate on next transaction */
#define EHCI_QH_EP_NUM(e)         (((e) & 0x0F) << 8)
#define EHCI_QH_SPEED_FULL        (0 << 12)  /* Full-Speed (12 Mb/s) */
#define EHCI_QH_SPEED_LOW         (1 << 12)  /* Low-Speed (1.5 Mb/s) */
#define EHCI_QH_SPEED_HIGH        (2 << 12)  /* High-Speed (480 Mb/s) */
#define EHCI_QH_DTC               (1 << 14)  /* Data Toggle Control (1 = Take from qTD) */
#define EHCI_QH_HRECL             (1 << 15)  /* Head of Reclamation List Flag (Async head) */
#define EHCI_QH_MAX_PACKET(s)     (((s) & 0x7FF) << 16)
#define EHCI_QH_CONTROL_EP        (1 << 27)  /* Control Endpoint Flag (Full/Low speed) */
#define EHCI_QH_RL(n)             (((n) & 0x0F) << 28) /* Nak Counter Reload */

/* Endpoint Capabilities (qh_endphub) Bits */
#define EHCI_QH_SMASK(m)          (((m) & 0xFF) << 0)  /* Interrupt Schedule Mask (uFrame 0 = 0x01) */
#define EHCI_QH_CMASK(m)          (((m) & 0xFF) << 8)  /* Split Completion Mask (uFrames 2..4 = 0x1C) */
#define EHCI_QH_HUB_ADDR(a)       (((a) & 0x7F) << 16) /* Hub Device Address for Split */
#define EHCI_QH_HUB_PORT(p)       (((p) & 0x7F) << 23) /* Hub Downstream Port Number */
#define EHCI_QH_MULT(n)           (((n) & 0x03) << 30) /* High-Bandwidth Pipe Multiplier (1..3) */

#endif /* SZPONTOS_DRIVERS_EHCI_HW_H */
