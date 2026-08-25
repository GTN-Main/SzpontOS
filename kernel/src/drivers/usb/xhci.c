/*
 * SzpontOS - Complete xHCI (Extensible Host Controller Interface) USB 3.0 Driver
 * Full hardware enumeration, DMA rings, BIOS handover, Evaluate Context, and USB HID Keyboard support.
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <drivers/xhci.h>
#include <drivers/hid.h>
#include <drivers/pci.h>
#include <mm/vmm.h>
#include <mm/heap.h>
#include <mm/pmm.h>
#include <arch/x86_64/io.h>
#include <kernel/kprint.h>
#include <kernel/string.h>

#define XHCI_MAX_CONTROLLERS 4
static xhci_controller_t g_xhci_controllers[XHCI_MAX_CONTROLLERS];
static size_t g_xhci_count = 0;

static inline uint32_t xhci_read32(uintptr_t addr) {
    return *(volatile uint32_t *)addr;
}

static inline void xhci_write32(uintptr_t addr, uint32_t val) {
    *(volatile uint32_t *)addr = val;
}

static inline uint64_t xhci_read64(uintptr_t addr) {
    uint32_t lo = *(volatile uint32_t *)addr;
    uint32_t hi = *(volatile uint32_t *)(addr + 4);
    return ((uint64_t)hi << 32) | lo;
}

static inline void xhci_write64(uintptr_t addr, uint64_t val) {
    *(volatile uint32_t *)addr = (uint32_t)(val & 0xFFFFFFFF);
    *(volatile uint32_t *)(addr + 4) = (uint32_t)(val >> 32);
}

/* ==============================================================================
 * Ring Management Primitives
 * ============================================================================== */
static bool xhci_alloc_ring(xhci_ring_t *ring, size_t size) {
    if (!ring || size < 16)
        return false;

    uintptr_t phys = pmm_alloc_page();
    if (!phys)
        return false;

    memset((void *)PHYS_TO_VIRT(phys), 0, PAGE_SIZE);

    ring->trbs = (xhci_trb_t *)PHYS_TO_VIRT(phys);
    ring->trbs_phys = phys;
    ring->size = size;
    ring->enqueue_ptr = 0;
    ring->dequeue_ptr = 0;
    ring->cycle_state = 1;

    /* Setup Link TRB at the end of the ring */
    size_t last = size - 1;
    ring->trbs[last].parameter = phys;
    ring->trbs[last].status = 0;
    ring->trbs[last].control = (XHCI_TRB_LINK << 10) | (1 << 1) /* Toggle Cycle */ | 1 /* Cycle bit */;

    return true;
}

static void xhci_enqueue_trb(xhci_ring_t *ring, uint64_t param, uint32_t status, uint32_t control) {
    size_t cur = ring->enqueue_ptr;
    ring->trbs[cur].parameter = param;
    ring->trbs[cur].status = status;
    ring->trbs[cur].control = (control & ~1) | ring->cycle_state;

    ring->enqueue_ptr++;
    if (ring->enqueue_ptr == ring->size - 1) {
        /* Update link TRB cycle bit and wrap */
        size_t last = ring->size - 1;
        ring->trbs[last].control = (XHCI_TRB_LINK << 10) | (1 << 1) | ring->cycle_state;
        ring->cycle_state ^= 1;
        ring->enqueue_ptr = 0;
    }
}

/* ==============================================================================
 * Event Ring & Command Execution Engine
 * ============================================================================== */
static void xhci_update_erdp(xhci_controller_t *hc) {
    uintptr_t erdp_phys = hc->event_ring_phys + hc->event_ring_dequeue * sizeof(xhci_trb_t);
    xhci_write64(hc->rt_base + 0x20 + 0x18, erdp_phys | (1 << 3) /* EHB */);
}

static int xhci_wait_cmd_completion(xhci_controller_t *hc, uint8_t *out_slot_id, uint32_t timeout_ms) {
    uint32_t elapsed = 0;
    while (elapsed < timeout_ms * 100) {
        xhci_trb_t *trb = &hc->event_ring[hc->event_ring_dequeue];
        uint8_t cycle = trb->control & 1;

        if (cycle == hc->event_ring_cycle) {
            uint8_t trb_type = (trb->control >> 10) & 0x3F;
            uint8_t comp_code = (trb->status >> 24) & 0xFF;
            uint8_t slot_id = (trb->control >> 24) & 0xFF;

            hc->event_ring_dequeue = (hc->event_ring_dequeue + 1) % XHCI_RING_SIZE;
            if (hc->event_ring_dequeue == 0) {
                hc->event_ring_cycle ^= 1;
            }
            xhci_update_erdp(hc);

            if (trb_type == XHCI_TRB_CMD_COMPLETION) {
                if (out_slot_id)
                    *out_slot_id = slot_id;
                return (comp_code == 1) ? 0 : -1;
            }
        }

        udelay(10);
        elapsed++;
    }
    return -1; /* Timeout */
}

static int xhci_send_cmd(xhci_controller_t *hc, uint64_t param, uint32_t status, uint32_t control, uint8_t *out_slot_id) {
    xhci_enqueue_trb(&hc->cmd_ring, param, status, control);
    /* Ring Doorbell 0 */
    xhci_write32(hc->db_base, 0);
    return xhci_wait_cmd_completion(hc, out_slot_id, 1000);
}

static int xhci_wait_transfer_event(xhci_controller_t *hc, uint8_t slot_id, uint8_t dci, uint32_t timeout_ms) {
    uint32_t elapsed = 0;
    while (elapsed < timeout_ms * 100) {
        xhci_trb_t *trb = &hc->event_ring[hc->event_ring_dequeue];
        uint8_t cycle = trb->control & 1;

        if (cycle == hc->event_ring_cycle) {
            uint8_t trb_type = (trb->control >> 10) & 0x3F;
            uint8_t comp_code = (trb->status >> 24) & 0xFF;
            uint8_t ev_slot = (trb->control >> 24) & 0xFF;
            uint8_t ev_dci = (trb->control >> 16) & 0x1F;

            hc->event_ring_dequeue = (hc->event_ring_dequeue + 1) % XHCI_RING_SIZE;
            if (hc->event_ring_dequeue == 0) {
                hc->event_ring_cycle ^= 1;
            }
            xhci_update_erdp(hc);

            if (trb_type == XHCI_TRB_TRANSFER_EVENT && ev_slot == slot_id && ev_dci == dci) {
                return (comp_code == 1 || comp_code == 13 /* Short packet */) ? 0 : -1;
            }
        }

        udelay(10);
        elapsed++;
    }
    return -1;
}

/* ==============================================================================
 * USB Control Transfers (EP0)
 * ============================================================================== */
static int xhci_control_transfer(xhci_controller_t *hc, xhci_device_t *dev, uint8_t req_type, uint8_t req,
                                uint16_t val, uint16_t idx, uint16_t len, void *data, uintptr_t data_phys) {
    (void)data;
    uint8_t trt = (len == 0) ? 0 : ((req_type & 0x80) ? 3 : 2);
    uint64_t setup_param = (uint64_t)req_type | ((uint64_t)req << 8) | ((uint64_t)val << 16) |
                           ((uint64_t)idx << 32) | ((uint64_t)len << 48);

    /* 1. Setup Stage TRB */
    xhci_enqueue_trb(&dev->ep0_ring, setup_param, 8,
                     (XHCI_TRB_SETUP_STAGE << 10) | (1 << 6) /* IDT */ | (trt << 16));

    /* 2. Data Stage TRB (if len > 0) */
    if (len > 0 && data_phys) {
        uint32_t dir = (req_type & 0x80) ? 1 : 0;
        xhci_enqueue_trb(&dev->ep0_ring, data_phys, len,
                         (XHCI_TRB_DATA_STAGE << 10) | (dir << 16));
    }

    /* 3. Status Stage TRB */
    uint32_t status_dir = (len == 0 || !(req_type & 0x80)) ? 1 : 0;
    xhci_enqueue_trb(&dev->ep0_ring, 0, 0,
                     (XHCI_TRB_STATUS_STAGE << 10) | (status_dir << 16) | (1 << 5) /* IOC */);

    /* Ring Doorbell for Slot, EP0 (target = 1) */
    xhci_write32(hc->db_base + dev->slot_id * 4, 1);

    return xhci_wait_transfer_event(hc, dev->slot_id, 1, 1000);
}

/* ==============================================================================
 * BIOS / OS Handover (USBLEGSUP Extended Capability)
 * ============================================================================== */
static void xhci_bios_handover(xhci_controller_t *hc) {
    uint32_t hccparams1 = xhci_read32(hc->cap_base + XHCI_CAP_HCCPARAMS1);
    uint32_t xecp = (hccparams1 >> 16) & 0xFFFF;
    if (!xecp)
        return;

    uintptr_t ext_offset = xecp << 2;
    while (ext_offset) {
        uint32_t cap = xhci_read32(hc->cap_base + ext_offset);
        uint8_t cap_id = cap & 0xFF;
        uint8_t next = (cap >> 8) & 0xFF;

        if (cap_id == XHCI_EXT_CAP_LEGSUP) {
            if (cap & XHCI_LEGSUP_BIOS_OWNED) {
                klog_info("xHCI: Requesting OS ownership from BIOS...");
                xhci_write32(hc->cap_base + ext_offset, cap | XHCI_LEGSUP_OS_OWNED);

                /* Wait up to 1000ms for BIOS to release ownership */
                for (int i = 0; i < 100; i++) {
                    uint32_t cur = xhci_read32(hc->cap_base + ext_offset);
                    if (!(cur & XHCI_LEGSUP_BIOS_OWNED))
                        break;
                    mdelay(10);
                }
            }

            /* Clear SMI enables in USBLEGCTLSTS */
            xhci_write32(hc->cap_base + ext_offset + 4, 0);
            klog_info("xHCI: BIOS handover complete (OS owns controller)");
            break;
        }

        if (!next)
            break;
        ext_offset += (next << 2);
    }
}

/* ==============================================================================
 * Device Enumeration & USB HID Keyboard Setup
 * ============================================================================== */
static void xhci_enumerate_device_on_port(xhci_controller_t *hc, uint8_t port, uint8_t speed) {
    if (hc->device_count >= 16)
        return;

    /* 1. Enable Slot Command */
    uint8_t slot_id = 0;
    if (xhci_send_cmd(hc, 0, 0, (XHCI_TRB_ENABLE_SLOT_CMD << 10), &slot_id) != 0 || slot_id == 0) {
        klog_warn("xHCI: Enable slot failed for port %u", port);
        return;
    }

    xhci_device_t *dev = &hc->devices[hc->device_count];
    memset(dev, 0, sizeof(xhci_device_t));
    dev->slot_id = slot_id;
    dev->port_num = port;
    dev->speed = speed;

    /* 2. Allocate Contexts */
    uintptr_t in_ctx_phys = pmm_alloc_page();
    uintptr_t dev_ctx_phys = pmm_alloc_page();
    if (!in_ctx_phys || !dev_ctx_phys)
        return;

    memset((void *)PHYS_TO_VIRT(in_ctx_phys), 0, PAGE_SIZE);
    memset((void *)PHYS_TO_VIRT(dev_ctx_phys), 0, PAGE_SIZE);

    dev->input_ctx = (void *)PHYS_TO_VIRT(in_ctx_phys);
    dev->input_ctx_phys = in_ctx_phys;
    dev->device_ctx = (void *)PHYS_TO_VIRT(dev_ctx_phys);
    dev->device_ctx_phys = dev_ctx_phys;

    /* Set DCBAA entry */
    hc->dcbaa[slot_id] = dev_ctx_phys;

    /* 3. Allocate EP0 Transfer Ring */
    if (!xhci_alloc_ring(&dev->ep0_ring, XHCI_RING_SIZE))
        return;

    /* 4. Setup Input Control Context */
    xhci_input_ctrl_ctx_t *ctrl_ctx = (xhci_input_ctrl_ctx_t *)dev->input_ctx;
    ctrl_ctx->add_flags = (1 << 0) | (1 << 1); /* Slot + EP0 */

    /* Setup Slot Context */
    uint8_t *slot_ctx_raw = (uint8_t *)dev->input_ctx + hc->ctx_size;
    xhci_slot_ctx_t *slot_ctx = (xhci_slot_ctx_t *)slot_ctx_raw;
    slot_ctx->info1 = (speed << 20) | (1 << 27); /* Context entries = 1 (EP0) */
    slot_ctx->info2 = (port << 16);               /* Root Hub Port Number */

    /* Setup EP0 Context: initial max packet size per xHCI spec 4.3.3 */
    uint8_t *ep0_ctx_raw = (uint8_t *)dev->input_ctx + (2 * hc->ctx_size);
    xhci_ep_ctx_t *ep0_ctx = (xhci_ep_ctx_t *)ep0_ctx_raw;
    uint16_t initial_max_packet = (speed == USB_SPEED_SUPER) ? 512 : ((speed == USB_SPEED_HIGH) ? 64 : 8);
    dev->max_packet_size = initial_max_packet;

    ep0_ctx->info2 = (3 << 1) /* CErr = 3 */ | (4 << 3) /* EP Type 4 = Control */ | (initial_max_packet << 16);
    ep0_ctx->tr_dequeue_ptr = dev->ep0_ring.trbs_phys | 1;
    ep0_ctx->avg_trb_len = 8;

    /* 5. Address Device Command */
    if (xhci_send_cmd(hc, in_ctx_phys, 0, (XHCI_TRB_ADDRESS_DEV_CMD << 10) | ((uint32_t)slot_id << 24), NULL) != 0) {
        klog_warn("xHCI: Address device failed on port %u (slot %u)", port, slot_id);
        return;
    }

    /* 6. Allocate DMA buffer for USB control transfers */
    uintptr_t dma_page = pmm_alloc_page();
    if (!dma_page)
        return;
    uint8_t *dma_buf = (uint8_t *)PHYS_TO_VIRT(dma_page);
    memset(dma_buf, 0, PAGE_SIZE);

    /* 7. Get Device Descriptor (18 bytes) */
    if (xhci_control_transfer(hc, dev, 0x80, USB_REQ_GET_DESCRIPTOR, (USB_DESC_DEVICE << 8), 0, 18, dma_buf, dma_page) != 0) {
        klog_warn("xHCI: Get Device Descriptor failed on slot %u", slot_id);
        return;
    }

    usb_dev_desc_t *dev_desc = (usb_dev_desc_t *)dma_buf;
    dev->vendor_id = dev_desc->idVendor;
    dev->product_id = dev_desc->idProduct;
    uint8_t actual_ep0_max = dev_desc->bMaxPacketSize0;

    /* If Full-Speed device max packet size differs from initial (8), issue Evaluate Context */
    if (speed == USB_SPEED_FULL && actual_ep0_max != 8 && actual_ep0_max >= 8) {
        memset(dev->input_ctx, 0, PAGE_SIZE);
        ctrl_ctx = (xhci_input_ctrl_ctx_t *)dev->input_ctx;
        ctrl_ctx->add_flags = (1 << 1); /* EP0 only */

        ep0_ctx_raw = (uint8_t *)dev->input_ctx + (2 * hc->ctx_size);
        ep0_ctx = (xhci_ep_ctx_t *)ep0_ctx_raw;
        ep0_ctx->info2 = (3 << 1) | (4 << 3) | ((uint32_t)actual_ep0_max << 16);

        if (xhci_send_cmd(hc, in_ctx_phys, 0, (XHCI_TRB_EVAL_CTX_CMD << 10) | ((uint32_t)slot_id << 24), NULL) == 0) {
            dev->max_packet_size = actual_ep0_max;
        }
    }

    /* 8. Get Configuration Descriptor */
    memset(dma_buf, 0, PAGE_SIZE);
    if (xhci_control_transfer(hc, dev, 0x80, USB_REQ_GET_DESCRIPTOR, (USB_DESC_CONFIGURATION << 8), 0, 9, dma_buf, dma_page) != 0) {
        return;
    }

    usb_cfg_desc_t *cfg_desc = (usb_cfg_desc_t *)dma_buf;
    uint16_t total_len = cfg_desc->wTotalLength;
    if (total_len > 1024)
        total_len = 1024;

    memset(dma_buf, 0, PAGE_SIZE);
    if (xhci_control_transfer(hc, dev, 0x80, USB_REQ_GET_DESCRIPTOR, (USB_DESC_CONFIGURATION << 8), 0, total_len, dma_buf, dma_page) != 0) {
        return;
    }

    /* 9. Parse Configuration Descriptors for HID Keyboard */
    uint8_t *ptr = dma_buf;
    uint8_t *end = dma_buf + total_len;
    bool found_kbd = false;
    uint8_t ep_addr = 0;
    uint16_t ep_max_packet = 8;
    uint8_t ep_interval = 10;
    uint8_t iface_num = 0;

    while (ptr < end) {
        uint8_t len = ptr[0];
        uint8_t type = ptr[1];
        if (len == 0)
            break;

        if (type == USB_DESC_INTERFACE) {
            usb_if_desc_t *if_desc = (usb_if_desc_t *)ptr;
            iface_num = if_desc->bInterfaceNumber;
            if (if_desc->bInterfaceClass == USB_CLASS_HID) {
                found_kbd = true;
            }
        } else if (type == USB_DESC_ENDPOINT && found_kbd) {
            usb_ep_desc_t *ep_desc = (usb_ep_desc_t *)ptr;
            if (ep_desc->bEndpointAddress & 0x80) { /* IN Endpoint */
                ep_addr = ep_desc->bEndpointAddress;
                ep_max_packet = ep_desc->wMaxPacketSize;
                ep_interval = ep_desc->bInterval;
                break;
            }
        }
        ptr += len;
    }

    if (!found_kbd || !ep_addr) {
        klog_info("xHCI: Device on Port %u, Slot %u (VID: %04x, PID: %04x) initialized", port, slot_id, dev->vendor_id, dev->product_id);
        hc->device_count++;
        return;
    }

    /* 10. Configure Interrupt IN Endpoint for HID Keyboard */
    uint8_t ep_num = ep_addr & 0x0F;
    uint8_t dci = (ep_num * 2) + 1; /* For EP 1 IN -> DCI 3 */
    dev->ep_in_addr = ep_addr;
    dev->ep_in_ctx_idx = dci;

    if (!xhci_alloc_ring(&dev->ep_in_ring, XHCI_RING_SIZE))
        return;

    /* Setup Input Control Context */
    memset(dev->input_ctx, 0, PAGE_SIZE);
    ctrl_ctx = (xhci_input_ctrl_ctx_t *)dev->input_ctx;
    ctrl_ctx->add_flags = (1 << 0) | (1 << dci); /* Slot + EP IN */

    /* Update Slot Context */
    slot_ctx_raw = (uint8_t *)dev->input_ctx + hc->ctx_size;
    slot_ctx = (xhci_slot_ctx_t *)slot_ctx_raw;
    slot_ctx->info1 = (speed << 20) | ((uint32_t)dci << 27); /* Context entries = DCI */
    slot_ctx->info2 = (port << 16);

    /* Setup EP IN Context */
    uint8_t *ep_in_ctx_raw = (uint8_t *)dev->input_ctx + ((dci + 1) * hc->ctx_size);
    xhci_ep_ctx_t *ep_in_ctx = (xhci_ep_ctx_t *)ep_in_ctx_raw;

    /* Calculate interval power-of-2 for xHCI */
    uint8_t interval_val = 3; /* ~8ms default */
    if (speed == USB_SPEED_LOW || speed == USB_SPEED_FULL) {
        if (ep_interval > 0) {
            uint8_t count = 0;
            uint8_t temp = ep_interval;
            while (temp > 1) {
                temp >>= 1;
                count++;
            }
            interval_val = count + 3;
        }
    }

    ep_in_ctx->info1 = (interval_val << 16);
    ep_in_ctx->info2 = (3 << 1) /* CErr = 3 */ | (7 << 3) /* EP Type 7 = Interrupt IN */ | (ep_max_packet << 16);
    ep_in_ctx->tr_dequeue_ptr = dev->ep_in_ring.trbs_phys | 1;
    ep_in_ctx->avg_trb_len = ep_max_packet;

    /* Issue Configure Endpoint Command */
    if (xhci_send_cmd(hc, in_ctx_phys, 0, (XHCI_TRB_CONFIG_EP_CMD << 10) | ((uint32_t)slot_id << 24), NULL) != 0) {
        klog_warn("xHCI: Configure endpoint failed on slot %u", slot_id);
        return;
    }

    /* 11. Set Configuration (1) */
    xhci_control_transfer(hc, dev, 0x00, USB_REQ_SET_CONFIGURATION, 1, 0, 0, NULL, 0);

    /* 12. Set Boot Protocol (0 = Boot Protocol) */
    xhci_control_transfer(hc, dev, 0x21, USB_HID_REQ_SET_PROTOCOL, 0, iface_num, 0, NULL, 0);

    /* 13. Set Idle (Duration = 0) */
    xhci_control_transfer(hc, dev, 0x21, USB_HID_REQ_SET_IDLE, 0, iface_num, 0, NULL, 0);

    /* 14. Setup HID Report Buffer & Enqueue First Interrupt IN TRB */
    uintptr_t rep_page = pmm_alloc_page();
    if (!rep_page)
        return;
    dev->report_buffer_phys = rep_page;

    /* Enqueue Normal TRB */
    xhci_enqueue_trb(&dev->ep_in_ring, rep_page, 8,
                     (XHCI_TRB_NORMAL << 10) | (1 << 5) /* IOC */ | (1 << 2) /* ISP */);

    /* Ring Doorbell for Slot, target = DCI */
    xhci_write32(hc->db_base + slot_id * 4, dci);

    dev->is_keyboard = true;
    hc->device_count++;

    klog_info("xHCI: USB Keyboard attached successfully on Port %u, Slot %u (VID: %04x, PID: %04x)",
              port, slot_id, dev->vendor_id, dev->product_id);
}

/* ==============================================================================
 * Controller Initialization & Port Enumeration
 * ============================================================================== */
static void xhci_init_controller(pci_device_t *pci_dev) {
    if (g_xhci_count >= XHCI_MAX_CONTROLLERS)
        return;

    pci_enable_bus_mastering(pci_dev);

    uintptr_t mmio_phys = pci_dev->bar[0] & ~0xF;
    if (!mmio_phys)
        return;

    /* Map 256 KiB MMIO space with cache disabled */
    for (size_t p = 0; p < 64; p++) {
        uintptr_t v = (uintptr_t)PHYS_TO_VIRT(mmio_phys + p * PAGE_SIZE);
        vmm_map_page(&g_kernel_pagemap, v, mmio_phys + p * PAGE_SIZE,
                     VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | VMM_FLAG_CACHE_DISABLE);
    }

    uintptr_t mmio_virt = (uintptr_t)PHYS_TO_VIRT(mmio_phys);
    xhci_controller_t *hc = &g_xhci_controllers[g_xhci_count];
    memset(hc, 0, sizeof(xhci_controller_t));

    hc->pci_dev = pci_dev;
    hc->mmio_base = mmio_virt;
    hc->cap_base = mmio_virt;

    uint8_t caplength = *(volatile uint8_t *)hc->cap_base;
    uint16_t hciversion = *(volatile uint16_t *)(hc->cap_base + XHCI_CAP_HCIVERSION);
    uint32_t hcsparams1 = xhci_read32(hc->cap_base + XHCI_CAP_HCSPARAMS1);
    uint32_t hccparams1 = xhci_read32(hc->cap_base + XHCI_CAP_HCCPARAMS1);
    uint32_t dboff = xhci_read32(hc->cap_base + XHCI_CAP_DBOFF);
    uint32_t rtsoff = xhci_read32(hc->cap_base + XHCI_CAP_RTSOFF);

    hc->op_base = hc->cap_base + caplength;
    hc->db_base = hc->cap_base + (dboff & ~0x3);
    hc->rt_base = hc->cap_base + (rtsoff & ~0x1F);

    hc->max_slots = hcsparams1 & 0xFF;
    hc->max_ports = (hcsparams1 >> 24) & 0xFF;
    hc->ctx_size = (hccparams1 & 4) ? 64 : 32; /* CSZ bit */

    klog_info("xHCI: Initializing USB 3.0 Controller v%x.%x (Slots: %u, Ports: %u, CtxSize: %u bytes)",
              hciversion >> 8, hciversion & 0xFF, hc->max_slots, hc->max_ports, hc->ctx_size);

    /* 1. Perform BIOS to OS Handover */
    xhci_bios_handover(hc);

    /* 2. Wait for Controller Ready (CNR == 0) */
    while (xhci_read32(hc->op_base + XHCI_OP_USBSTS) & XHCI_STS_CNR) {
        udelay(10);
    }

    /* 3. Stop Controller (RS = 0) */
    uint32_t usbcmd = xhci_read32(hc->op_base + XHCI_OP_USBCMD);
    usbcmd &= ~XHCI_CMD_RS;
    xhci_write32(hc->op_base + XHCI_OP_USBCMD, usbcmd);

    while (!(xhci_read32(hc->op_base + XHCI_OP_USBSTS) & XHCI_STS_HCH)) {
        udelay(10);
    }

    /* 4. Reset Controller (HCRST = 1) */
    xhci_write32(hc->op_base + XHCI_OP_USBCMD, XHCI_CMD_HCRST);
    while (xhci_read32(hc->op_base + XHCI_OP_USBCMD) & XHCI_CMD_HCRST) {
        udelay(10);
    }
    while (xhci_read32(hc->op_base + XHCI_OP_USBSTS) & XHCI_STS_CNR) {
        udelay(10);
    }

    /* 5. Set Max Slots Enabled */
    uint8_t max_slots_en = (hc->max_slots > 16) ? 16 : hc->max_slots;
    xhci_write32(hc->op_base + XHCI_OP_CONFIG, max_slots_en);

    /* 6. Allocate DCBAA (Device Context Base Address Array) */
    uintptr_t dcbaa_phys = pmm_alloc_page();
    if (!dcbaa_phys)
        return;
    memset((void *)PHYS_TO_VIRT(dcbaa_phys), 0, PAGE_SIZE);
    hc->dcbaa = (uint64_t *)PHYS_TO_VIRT(dcbaa_phys);
    hc->dcbaa_phys = dcbaa_phys;

    /* Allocate Scratchpad Buffers if required by hardware (Intel/AMD xHCI spec 4.2.2 & 6.1) */
    uint32_t hcsparams2 = xhci_read32(hc->cap_base + XHCI_CAP_HCSPARAMS2);
    uint32_t max_scratchpad = (((hcsparams2 >> 21) & 0x1F) << 5) | ((hcsparams2 >> 27) & 0x1F);
    if (max_scratchpad > 0) {
        uintptr_t sp_array_phys = pmm_alloc_page();
        if (sp_array_phys) {
            uint64_t *sp_array = (uint64_t *)PHYS_TO_VIRT(sp_array_phys);
            memset(sp_array, 0, PAGE_SIZE);
            for (uint32_t i = 0; i < max_scratchpad; i++) {
                uintptr_t sp_buf = pmm_alloc_page();
                if (sp_buf) {
                    memset((void *)PHYS_TO_VIRT(sp_buf), 0, PAGE_SIZE);
                    sp_array[i] = (uint64_t)sp_buf;
                }
            }
            hc->dcbaa[0] = (uint64_t)sp_array_phys;
            klog_info("xHCI: Allocated %u scratchpad buffers for hardware DMA", max_scratchpad);
        }
    }

    xhci_write64(hc->op_base + XHCI_OP_DCBAAP, dcbaa_phys);

    /* 7. Allocate Command Ring */
    if (!xhci_alloc_ring(&hc->cmd_ring, XHCI_RING_SIZE))
        return;
    xhci_write64(hc->op_base + XHCI_OP_CRCR, hc->cmd_ring.trbs_phys | 1 /* RCS */);

    /* 8. Allocate Event Ring & Event Ring Segment Table (ERST) */
    uintptr_t erst_phys = pmm_alloc_page();
    uintptr_t ev_phys = pmm_alloc_page();
    if (!erst_phys || !ev_phys)
        return;

    memset((void *)PHYS_TO_VIRT(erst_phys), 0, PAGE_SIZE);
    memset((void *)PHYS_TO_VIRT(ev_phys), 0, PAGE_SIZE);

    hc->erst = (xhci_erst_entry_t *)PHYS_TO_VIRT(erst_phys);
    hc->erst_phys = erst_phys;
    hc->event_ring = (xhci_trb_t *)PHYS_TO_VIRT(ev_phys);
    hc->event_ring_phys = ev_phys;
    hc->event_ring_dequeue = 0;
    hc->event_ring_cycle = 1;

    hc->erst[0].ring_segment_base_address = ev_phys;
    hc->erst[0].ring_segment_size = XHCI_RING_SIZE;

    /* 9. Configure Primary Interrupter */
    xhci_write32(hc->rt_base + 0x20 + 0x08, 1);                                      /* ERSTSZ = 1 */
    xhci_write64(hc->rt_base + 0x20 + 0x10, erst_phys);                              /* ERSTBA */
    xhci_write64(hc->rt_base + 0x20 + 0x18, ev_phys | (1 << 3) /* EHB */);           /* ERDP */
    xhci_write32(hc->rt_base + 0x20 + 0x00, 2);                                      /* IMAN IE */

    /* 10. Start Controller (RS = 1, INTE = 1) */
    xhci_write32(hc->op_base + XHCI_OP_USBCMD, XHCI_CMD_RS | XHCI_CMD_INTE);
    while (xhci_read32(hc->op_base + XHCI_OP_USBSTS) & XHCI_STS_HCH) {
        udelay(10);
    }

    hc->initialized = true;
    g_xhci_count++;
    klog_info("xHCI: Host Controller running. Scanning %u root hub ports...", hc->max_ports);

#define XHCI_PORT_W1C_MASK ((1U << 1) | (1U << 17) | (1U << 18) | (1U << 19) | (1U << 20) | (1U << 21) | (1U << 22) | (1U << 23))

    /* 11. Power all ports and wait for link stabilization */
    for (uint8_t port = 1; port <= hc->max_ports; port++) {
        uintptr_t portsc_addr = hc->op_base + XHCI_OP_PORTSC_BASE + (port - 1) * 0x10;
        uint32_t portsc = xhci_read32(portsc_addr);
        if (!(portsc & XHCI_PORT_PP)) {
            xhci_write32(portsc_addr, (portsc & ~XHCI_PORT_W1C_MASK) | XHCI_PORT_PP);
        }
    }
    mdelay(50);

    /* 12. Enumerate connected ports */
    for (uint8_t port = 1; port <= hc->max_ports; port++) {
        uintptr_t portsc_addr = hc->op_base + XHCI_OP_PORTSC_BASE + (port - 1) * 0x10;
        uint32_t portsc = xhci_read32(portsc_addr);

        if (portsc & XHCI_PORT_CCS) {
            /* Issue Port Reset if not enabled */
            if (!(portsc & XHCI_PORT_PED)) {
                xhci_write32(portsc_addr, (portsc & ~XHCI_PORT_W1C_MASK) | XHCI_PORT_PP | XHCI_PORT_PR);
                for (int t = 0; t < 50; t++) {
                    mdelay(10);
                    portsc = xhci_read32(portsc_addr);
                    if (!(portsc & XHCI_PORT_PR) && (portsc & XHCI_PORT_PED))
                        break;
                }
                /* Clear status change bits */
                xhci_write32(portsc_addr, (portsc & ~XHCI_PORT_W1C_MASK) | XHCI_PORT_PP | (portsc & (XHCI_PORT_CSC | XHCI_PORT_PRC)));
                portsc = xhci_read32(portsc_addr);
            }

            uint8_t speed = (portsc >> 10) & 0xF;
            if (speed == 0)
                speed = USB_SPEED_FULL;

            klog_info("xHCI: Port %u connected (Speed class: %u, PORTSC: 0x%08x)", port, speed, portsc);
            xhci_enumerate_device_on_port(hc, port, speed);
        }
    }
}

static void xhci_check_ports(xhci_controller_t *hc) {
    for (uint8_t port = 1; port <= hc->max_ports; port++) {
        uintptr_t portsc_addr = hc->op_base + XHCI_OP_PORTSC_BASE + (port - 1) * 0x10;
        uint32_t portsc = xhci_read32(portsc_addr);

        /* Only check on actual hardware Port Status Change events */
        if (portsc & (XHCI_PORT_CSC | XHCI_PORT_PRC)) {
            /* Acknowledge change bits immediately */
            xhci_write32(portsc_addr, (portsc & ~XHCI_PORT_W1C_MASK) | XHCI_PORT_PP | (portsc & (XHCI_PORT_CSC | XHCI_PORT_PRC)));

            if (portsc & XHCI_PORT_CCS) {
                bool exists = false;
                for (size_t d = 0; d < hc->device_count; d++) {
                    if (hc->devices[d].port_num == port) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    if (!(portsc & XHCI_PORT_PED)) {
                        xhci_write32(portsc_addr, (portsc & ~XHCI_PORT_W1C_MASK) | XHCI_PORT_PP | XHCI_PORT_PR);
                        for (int t = 0; t < 20; t++) {
                            udelay(500);
                            portsc = xhci_read32(portsc_addr);
                            if (!(portsc & XHCI_PORT_PR) && (portsc & XHCI_PORT_PED))
                                break;
                        }
                        xhci_write32(portsc_addr, (portsc & ~XHCI_PORT_W1C_MASK) | XHCI_PORT_PP | (portsc & (XHCI_PORT_CSC | XHCI_PORT_PRC)));
                        portsc = xhci_read32(portsc_addr);
                    }
                    uint8_t speed = (portsc >> 10) & 0xF;
                    if (speed == 0)
                        speed = USB_SPEED_FULL;
                    xhci_enumerate_device_on_port(hc, port, speed);
                }
            }
        }
    }
}

/* ==============================================================================
 * Universal xHCI Polling & Event Loop
 * ============================================================================== */
void xhci_poll(void) {
    for (size_t c = 0; c < g_xhci_count; c++) {
        xhci_controller_t *hc = &g_xhci_controllers[c];
        if (!hc->initialized)
            continue;

        xhci_check_ports(hc);

        while (1) {
            xhci_trb_t *trb = &hc->event_ring[hc->event_ring_dequeue];
            uint8_t cycle = trb->control & 1;

            if (cycle != hc->event_ring_cycle)
                break;

            uint8_t trb_type = (trb->control >> 10) & 0x3F;
            uint8_t slot_id = (trb->control >> 24) & 0xFF;
            uint8_t dci = (trb->control >> 16) & 0x1F;

            hc->event_ring_dequeue = (hc->event_ring_dequeue + 1) % XHCI_RING_SIZE;
            if (hc->event_ring_dequeue == 0) {
                hc->event_ring_cycle ^= 1;
            }
            xhci_update_erdp(hc);

            if (trb_type == XHCI_TRB_TRANSFER_EVENT) {
                for (size_t d = 0; d < hc->device_count; d++) {
                    xhci_device_t *dev = &hc->devices[d];
                    if (dev->slot_id == slot_id && dev->is_keyboard && dci == dev->ep_in_ctx_idx) {
                        /* Copy and process 8-byte HID report */
                        uint8_t report[8];
                        memcpy(report, (void *)PHYS_TO_VIRT(dev->report_buffer_phys), 8);
                        hid_process_keyboard_report(report);

                        /* Re-enqueue Normal TRB */
                        xhci_enqueue_trb(&dev->ep_in_ring, dev->report_buffer_phys, 8,
                                         (XHCI_TRB_NORMAL << 10) | (1 << 5) /* IOC */ | (1 << 2) /* ISP */);
                        /* Ring Doorbell */
                        xhci_write32(hc->db_base + slot_id * 4, dci);
                        break;
                    }
                }
            }
        }
    }
}

bool xhci_is_active(void) {
    return g_xhci_count > 0;
}

size_t xhci_get_device_list_info(char *buf, size_t max_len) {
    if (!buf || max_len == 0)
        return 0;

    size_t pos = 0;
    for (size_t c = 0; c < g_xhci_count && pos < max_len; c++) {
        xhci_controller_t *hc = &g_xhci_controllers[c];
        if (!hc->initialized)
            continue;

        uint16_t hc_vid = hc->pci_dev ? hc->pci_dev->vendor_id : 0x1b36;
        uint16_t hc_pid = hc->pci_dev ? hc->pci_dev->device_id : 0x000d;
        pos += ksnprintf(buf + pos, max_len - pos,
                         "Bus %03zu Device 001: ID %04x:%04x QEMU xHCI USB 3.0 Host Controller (Ports: %u, Slots: %u)\n",
                         c + 1, hc_vid, hc_pid, hc->max_ports, hc->max_slots);

        for (size_t d = 0; d < hc->device_count && pos < max_len; d++) {
            xhci_device_t *dev = &hc->devices[d];
            const char *speed_str = "Full";
            if (dev->speed == USB_SPEED_LOW)
                speed_str = "Low";
            else if (dev->speed == USB_SPEED_HIGH)
                speed_str = "High";
            else if (dev->speed == USB_SPEED_SUPER)
                speed_str = "Super";
            else if (dev->speed == USB_SPEED_SUPER_PLUS)
                speed_str = "Super+";

            const char *type_str = dev->is_keyboard ? "USB Keyboard (HID)" : "USB Device";
            pos += ksnprintf(buf + pos, max_len - pos,
                             "Bus %03zu Device %03u: ID %04x:%04x %s (Port %u, Speed %s, Slot %u)\n",
                             c + 1, (unsigned int)(d + 2), dev->vendor_id, dev->product_id,
                             type_str, dev->port_num, speed_str, dev->slot_id);
        }
    }

    if (pos == 0) {
        pos += ksnprintf(buf + pos, max_len - pos, "No active USB host controllers or devices found.\n");
    }

    return pos;
}

void xhci_init(void) {
    /* Scan registered PCI devices for xHCI controllers */
    for (pci_device_t *dev = pci_get_device_list(); dev != NULL; dev = dev->next) {
        if (dev->class_code == 0x0C && dev->subclass == 0x03 && dev->prog_if == 0x30) {
            xhci_init_controller(dev);
        }
    }

    if (g_xhci_count == 0) {
        klog_info("xHCI: No xHCI USB 3.0 controllers found on PCI bus");
    }
}
