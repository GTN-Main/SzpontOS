/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 * (C) Copyright by Szpont Industries. All rights reserved.
 *
 * SzpontOS - Complete EHCI (Enhanced Host Controller Interface - USB 2.0) Driver
 * Direct FreeBSD/Linux-derived full implementation with Intel Rate Matching Hub (RMH),
 * 1024-entry Periodic Frame List, Split Transactions, and Dynamic Plug-and-Play Hotplug.
 */

#include <drivers/ehci.h>
#include <drivers/hid.h>
#include <drivers/pci.h>
#include <drivers/usb.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/heap.h>
#include <kernel/kprint.h>
#include <kernel/string.h>
#include <arch/x86_64/io.h>

#define MAX_EHCI_CONTROLLERS      4
#define EHCI_CTRL_TIMEOUT_US      500000 /* 500ms transfer timeout */

static ehci_controller_t g_ehci_controllers[MAX_EHCI_CONTROLLERS];
static size_t g_ehci_count = 0;

/* MMIO 32-bit register read/write helpers */
static inline uint32_t ehci_read32(uintptr_t addr) {
    return *(volatile uint32_t *)addr;
}

static inline void ehci_write32(uintptr_t addr, uint32_t val) {
    *(volatile uint32_t *)addr = val;
}

/* ==============================================================================
 * Section 1: BIOS-to-OS Handover (USBLEGSUP & SMI Disable)
 * ============================================================================== */
static void ehci_bios_handover(ehci_controller_t *hc) {
    uint32_t hccparams = ehci_read32(hc->cap_base + EHCI_HCCPARAMS);
    uint8_t eecp = EHCI_HCC_EECP(hccparams);

    if (eecp >= 0x40 && hc->pci_dev) {
        uint32_t legsup = pci_read32(hc->pci_dev->bus, hc->pci_dev->slot, hc->pci_dev->func, eecp);

        if (legsup & EHCI_USBLEGSUP_BIOS_OWNED) {
            klog_info("EHCI: Requesting OS ownership from BIOS (PCI %02x:%02x.%u, EECP 0x%02x)",
                      hc->pci_dev->bus, hc->pci_dev->slot, hc->pci_dev->func, eecp);

            pci_write32(hc->pci_dev->bus, hc->pci_dev->slot, hc->pci_dev->func, eecp,
                        legsup | EHCI_USBLEGSUP_OS_OWNED);

            int timeout = 500;
            while (timeout > 0) {
                legsup = pci_read32(hc->pci_dev->bus, hc->pci_dev->slot, hc->pci_dev->func, eecp);
                if (!(legsup & EHCI_USBLEGSUP_BIOS_OWNED)) {
                    break;
                }
                udelay(100);
                timeout--;
            }

            if (legsup & EHCI_USBLEGSUP_BIOS_OWNED) {
                klog_warn("EHCI: BIOS did not release ownership cleanly, forcing OS ownership");
                pci_write32(hc->pci_dev->bus, hc->pci_dev->slot, hc->pci_dev->func, eecp, EHCI_USBLEGSUP_OS_OWNED);
            }
        }

        /* Disable BIOS SMI interrupts completely (Offset EECP + 4) */
        pci_write32(hc->pci_dev->bus, hc->pci_dev->slot, hc->pci_dev->func, eecp + 4, 0);
    }
}

/* ==============================================================================
 * Section 2: Low-Level DMA Descriptor Management (QH & qTD)
 * ============================================================================== */
static void ehci_init_qh(ehci_qh_t *qh, uint32_t qh_phys, uint8_t dev_addr, uint8_t ep_num, uint8_t speed,
                         uint16_t max_packet, uint8_t hub_addr, uint8_t hub_port) {
    memset(qh, 0, sizeof(ehci_qh_t));
    qh->qh_self_phys = qh_phys;

    uint32_t ep_speed_val = (speed == USB_SPEED_HIGH) ? EHCI_QH_SPEED_HIGH :
                            ((speed == USB_SPEED_LOW) ? EHCI_QH_SPEED_LOW : EHCI_QH_SPEED_FULL);

    uint32_t ep_char = EHCI_QH_DEV_ADDR(dev_addr) |
                       EHCI_QH_EP_NUM(ep_num) |
                       ep_speed_val |
                       EHCI_QH_DTC |
                       EHCI_QH_MAX_PACKET(max_packet) |
                       EHCI_QH_RL(4);

    if (ep_num == 0 && speed != USB_SPEED_HIGH) {
        ep_char |= EHCI_QH_CONTROL_EP;
    }
    qh->qh_endp = ep_char;

    /* Endpoint Capabilities for Split Transactions & Periodic Scheduling */
    uint32_t ep_cap = EHCI_QH_MULT(1);
    if (hub_addr > 0 && speed != USB_SPEED_HIGH) {
        ep_cap = EHCI_QH_SMASK(0x01) |       /* Start-Split in uFrame 0 */
                 EHCI_QH_CMASK(0x1C) |       /* Complete-Split in uFrames 2, 3, 4 */
                 EHCI_QH_HUB_ADDR(hub_addr) |
                 EHCI_QH_HUB_PORT(hub_port) |
                 EHCI_QH_MULT(1);
    } else if (ep_num > 0 || speed != USB_SPEED_HIGH) {
        /* High-speed interrupt endpoint or direct root interrupt */
        ep_cap = EHCI_QH_SMASK(0x01) | EHCI_QH_MULT(1);
    }
    qh->qh_endphub = ep_cap;

    /* Initialize overlay area to terminate */
    qh->qh_curqtd = 0;
    qh->qtd_next = EHCI_LINK_TERMINATE;
    qh->qtd_altnext = EHCI_LINK_TERMINATE;
    qh->qtd_status = 0;
}

static void ehci_init_qtd(ehci_qtd_t *qtd, uint32_t qtd_phys, uint32_t pid, void *buf_virt, uintptr_t buf_phys,
                         size_t len, bool toggle, bool ioc) {
    memset(qtd, 0, sizeof(ehci_qtd_t));
    qtd->qtd_self_phys = qtd_phys;
    qtd->buf_virt = buf_virt;
    qtd->length = len;

    qtd->qtd_next = EHCI_LINK_TERMINATE;
    qtd->qtd_altnext = EHCI_LINK_TERMINATE;

    uint32_t token = EHCI_QTD_ACTIVE |
                     pid |
                     EHCI_QTD_CERR_MAX |
                     EHCI_QTD_BYTES(len);
    if (ioc) {
        token |= EHCI_QTD_IOC;
    }
    if (toggle) {
        token |= EHCI_QTD_TOGGLE;
    }
    qtd->qtd_status = token;

    if (len > 0 && buf_phys) {
        qtd->qtd_buffer[0] = (uint32_t)buf_phys;
        qtd->qtd_buffer[1] = (uint32_t)((buf_phys + 0x1000) & ~0xFFF);
        qtd->qtd_buffer[2] = (uint32_t)((buf_phys + 0x2000) & ~0xFFF);
        qtd->qtd_buffer[3] = (uint32_t)((buf_phys + 0x3000) & ~0xFFF);
        qtd->qtd_buffer[4] = (uint32_t)((buf_phys + 0x4000) & ~0xFFF);
    }
}

/* ==============================================================================
 * Section 3: Synchronous Control Transfer Engine
 * ============================================================================== */
static bool ehci_exec_control_transfer(ehci_controller_t *hc, uint8_t dev_addr, uint8_t speed, uint16_t max_packet,
                                       uint8_t hub_addr, uint8_t hub_port,
                                       const usb_setup_pkt_t *setup, void *data_buf, size_t data_len, bool dir_in) {
    if (!hc->ctrl_qh || !hc->ctrl_qtd_pool || !hc->async_head_qh)
        return false;

    spinlock_acquire(&hc->lock);

    ehci_qh_t *head_qh = hc->async_head_qh;
    ehci_qh_t *ctrl_qh = hc->ctrl_qh;
    uint32_t ctrl_qh_phys = (uint32_t)hc->ctrl_qh_phys;

    ehci_init_qh(ctrl_qh, ctrl_qh_phys, dev_addr, 0, speed, max_packet, hub_addr, hub_port);

    /* Prepare Setup Buffer in DMA Memory */
    usb_setup_pkt_t *setup_dma = (usb_setup_pkt_t *)hc->ctrl_data_buf;
    uintptr_t setup_dma_phys = hc->ctrl_data_buf_phys;
    memcpy(setup_dma, setup, sizeof(usb_setup_pkt_t));

    uint8_t *data_dma = hc->ctrl_data_buf + 64;
    uintptr_t data_dma_phys = hc->ctrl_data_buf_phys + 64;
    if (data_len > 0 && !dir_in && data_buf) {
        memcpy(data_dma, data_buf, data_len);
    }

    ehci_qtd_t *setup_qtd = &hc->ctrl_qtd_pool[0];
    uint32_t setup_qtd_phys = (uint32_t)hc->ctrl_qtd_pool_phys;

    ehci_qtd_t *data_qtd = &hc->ctrl_qtd_pool[1];
    uint32_t data_qtd_phys = (uint32_t)(hc->ctrl_qtd_pool_phys + sizeof(ehci_qtd_t));

    ehci_qtd_t *status_qtd = &hc->ctrl_qtd_pool[2];
    uint32_t status_qtd_phys = (uint32_t)(hc->ctrl_qtd_pool_phys + 2 * sizeof(ehci_qtd_t));

    /* 1. Setup Stage qTD */
    ehci_init_qtd(setup_qtd, setup_qtd_phys, EHCI_QTD_PID_SETUP, setup_dma, setup_dma_phys, sizeof(usb_setup_pkt_t), false, false);

    /* 2. Data Stage qTD (Optional) */
    if (data_len > 0) {
        setup_qtd->qtd_next = data_qtd_phys;
        ehci_init_qtd(data_qtd, data_qtd_phys, dir_in ? EHCI_QTD_PID_IN : EHCI_QTD_PID_OUT, data_dma, data_dma_phys, data_len, true, false);
        data_qtd->qtd_next = status_qtd_phys;
    } else {
        setup_qtd->qtd_next = status_qtd_phys;
    }

    /* 3. Status Stage qTD */
    uint32_t status_pid = (data_len > 0 && dir_in) ? EHCI_QTD_PID_OUT : EHCI_QTD_PID_IN;
    ehci_init_qtd(status_qtd, status_qtd_phys, status_pid, NULL, 0, 0, true, true);

    /* Link qTD chain to Control QH */
    ctrl_qh->qh_curqtd = 0;
    ctrl_qh->qtd_next = setup_qtd_phys;
    ctrl_qh->qtd_altnext = EHCI_LINK_TERMINATE;
    ctrl_qh->qtd_status = 0;

    /* Insert Control QH into circular Async Schedule */
    uint32_t saved_link = head_qh->qh_link;
    ctrl_qh->qh_link = saved_link;
    head_qh->qh_link = ctrl_qh_phys | EHCI_LINK_TYPE_QH;

    /* Wait for transfer completion */
    int timeout = EHCI_CTRL_TIMEOUT_US / 100;
    bool success = false;

    while (timeout > 0) {
        uint32_t st_token = status_qtd->qtd_status;
        if (!(st_token & EHCI_QTD_ACTIVE)) {
            if (!(st_token & (EHCI_QTD_HALTED | EHCI_QTD_XACT_ERR | EHCI_QTD_BABBLE))) {
                success = true;
            }
            break;
        }

        if (setup_qtd->qtd_status & EHCI_QTD_HALTED || (data_len > 0 && (data_qtd->qtd_status & EHCI_QTD_HALTED))) {
            break;
        }

        udelay(100);
        timeout--;
    }

    /* Restore Async Schedule chain */
    head_qh->qh_link = saved_link;

    /* Copy received data */
    if (success && data_len > 0 && dir_in && data_buf) {
        memcpy(data_buf, data_dma, data_len);
    }

    spinlock_release(&hc->lock);
    return success;
}

/* Forward declarations */
static void ehci_enumerate_device(ehci_controller_t *hc, uint8_t port_num, uint8_t speed, uint8_t hub_addr, uint8_t hub_port);
static void ehci_enumerate_hub_ports(ehci_controller_t *hc, uint8_t hub_addr, uint8_t speed, uint16_t ep0_max_packet);

/* ==============================================================================
 * Section 4: USB Hub (Intel Rate Matching Hub) Subsystem
 * ============================================================================== */
static void ehci_enumerate_hub_ports(ehci_controller_t *hc, uint8_t hub_addr, uint8_t speed, uint16_t ep0_max_packet) {
    /* 1. Read Hub Descriptor (Type 0x29) */
    usb_setup_pkt_t req;
    req.bmRequestType = 0xA0;
    req.bRequest = USB_REQ_GET_DESCRIPTOR;
    req.wValue = (0x29 << 8);
    req.wIndex = 0;
    req.wLength = 8;

    uint8_t hub_desc[8] = {0};
    if (!ehci_exec_control_transfer(hc, hub_addr, speed, ep0_max_packet, 0, 0, &req, hub_desc, 8, true)) {
        klog_warn("EHCI: Failed to read Hub Descriptor for Addr %u", hub_addr);
        return;
    }

    uint8_t num_ports = hub_desc[2];
    if (num_ports == 0 || num_ports > 16) {
        num_ports = 8;
    }

    hc->has_rmh_hub = true;
    hc->rmh_hub_addr = hub_addr;
    hc->rmh_hub_ports = num_ports;
    hc->rmh_hub_speed = speed;

    klog_info("EHCI: Intel RMH USB Hub active (Addr %u, %u downstream physical ports)", hub_addr, num_ports);

    /* 2. Power on all downstream ports (PORT_POWER = 8) */
    for (uint8_t p = 1; p <= num_ports; p++) {
        req.bmRequestType = 0x23;
        req.bRequest = USB_REQ_SET_FEATURE;
        req.wValue = 8; /* PORT_POWER */
        req.wIndex = p;
        req.wLength = 0;
        ehci_exec_control_transfer(hc, hub_addr, speed, ep0_max_packet, 0, 0, &req, NULL, 0, false);
    }

    udelay(150000); /* 150ms power-on stabilization delay */

    /* 3. Probe and enumerate connected downstream ports */
    for (uint8_t p = 1; p <= num_ports; p++) {
        req.bmRequestType = 0xA3;
        req.bRequest = USB_REQ_GET_STATUS;
        req.wValue = 0;
        req.wIndex = p;
        req.wLength = 4;

        uint32_t port_status = 0;
        if (ehci_exec_control_transfer(hc, hub_addr, speed, ep0_max_packet, 0, 0, &req, &port_status, 4, true)) {
            if (port_status & 1) { /* Device is connected on Hub Port p */
                klog_info("EHCI: Device detected on Hub Port %u (Status 0x%08x)", p, port_status);

                /* Reset Hub Port (PORT_RESET = 4) */
                req.bmRequestType = 0x23;
                req.bRequest = USB_REQ_SET_FEATURE;
                req.wValue = 4; /* PORT_RESET */
                req.wIndex = p;
                req.wLength = 0;
                ehci_exec_control_transfer(hc, hub_addr, speed, ep0_max_packet, 0, 0, &req, NULL, 0, false);

                udelay(50000); /* 50ms Port Reset */

                /* Clear Port Reset Change (C_PORT_RESET = 20) */
                req.bmRequestType = 0x23;
                req.bRequest = USB_REQ_CLEAR_FEATURE;
                req.wValue = 20; /* C_PORT_RESET */
                req.wIndex = p;
                req.wLength = 0;
                ehci_exec_control_transfer(hc, hub_addr, speed, ep0_max_packet, 0, 0, &req, NULL, 0, false);

                udelay(20000); /* 20ms Recovery */

                /* Query Speed */
                req.bmRequestType = 0xA3;
                req.bRequest = USB_REQ_GET_STATUS;
                req.wValue = 0;
                req.wIndex = p;
                req.wLength = 4;
                port_status = 0;
                ehci_exec_control_transfer(hc, hub_addr, speed, ep0_max_packet, 0, 0, &req, &port_status, 4, true);

                uint8_t dev_speed = USB_SPEED_FULL;
                if (port_status & (1 << 9)) {
                    dev_speed = USB_SPEED_LOW;
                } else if (port_status & (1 << 10)) {
                    dev_speed = USB_SPEED_HIGH;
                }

                ehci_enumerate_device(hc, p, dev_speed, hub_addr, p);
            }
        }
    }
}

/* ==============================================================================
 * Section 5: Device Enumeration & UKBD HID Attachment
 * ============================================================================== */
static void ehci_enumerate_device(ehci_controller_t *hc, uint8_t port_num, uint8_t speed, uint8_t hub_addr, uint8_t hub_port) {
    if (hc->device_count >= EHCI_MAX_DEVICES)
        return;

    uint8_t dev_addr = hc->next_address++;
    uint16_t ep0_max_packet = (speed == USB_SPEED_HIGH) ? 64 : 8;

    /* 1. GET_DESCRIPTOR (Device) - First 8 bytes to discover EP0 MaxPacketSize */
    usb_setup_pkt_t req;
    req.bmRequestType = 0x80;
    req.bRequest = USB_REQ_GET_DESCRIPTOR;
    req.wValue = (USB_DESC_DEVICE << 8);
    req.wIndex = 0;
    req.wLength = 8;

    usb_dev_desc_t dev_desc;
    memset(&dev_desc, 0, sizeof(dev_desc));

    if (!ehci_exec_control_transfer(hc, 0, speed, ep0_max_packet, hub_addr, hub_port, &req, &dev_desc, 8, true)) {
        klog_warn("EHCI: Failed to read initial Device Descriptor (Port %u, Hub %u)", port_num, hub_addr);
        return;
    }

    if (dev_desc.bMaxPacketSize0 >= 8) {
        ep0_max_packet = dev_desc.bMaxPacketSize0;
    }

    /* 2. SET_ADDRESS */
    req.bmRequestType = 0x00;
    req.bRequest = USB_REQ_SET_ADDRESS;
    req.wValue = dev_addr;
    req.wIndex = 0;
    req.wLength = 0;

    if (!ehci_exec_control_transfer(hc, 0, speed, ep0_max_packet, hub_addr, hub_port, &req, NULL, 0, false)) {
        klog_warn("EHCI: Failed to set address %u for device on Port %u", dev_addr, port_num);
        return;
    }
    udelay(10000); /* 10ms recovery delay */

    /* 3. GET_DESCRIPTOR (Device) - Full 18 bytes */
    req.bmRequestType = 0x80;
    req.bRequest = USB_REQ_GET_DESCRIPTOR;
    req.wValue = (USB_DESC_DEVICE << 8);
    req.wIndex = 0;
    req.wLength = sizeof(usb_dev_desc_t);

    if (!ehci_exec_control_transfer(hc, dev_addr, speed, ep0_max_packet, hub_addr, hub_port, &req, &dev_desc, sizeof(dev_desc), true)) {
        klog_warn("EHCI: Failed to read full Device Descriptor for Addr %u", dev_addr);
        return;
    }

    /* 4. GET_DESCRIPTOR (Configuration) - 2-step read: Header (9 bytes) -> Full bytes */
    uint8_t cfg_hdr[9];
    memset(cfg_hdr, 0, sizeof(cfg_hdr));

    req.bmRequestType = 0x80;
    req.bRequest = USB_REQ_GET_DESCRIPTOR;
    req.wValue = (USB_DESC_CONFIGURATION << 8);
    req.wIndex = 0;
    req.wLength = sizeof(cfg_hdr);

    if (!ehci_exec_control_transfer(hc, dev_addr, speed, ep0_max_packet, hub_addr, hub_port, &req, cfg_hdr, sizeof(cfg_hdr), true)) {
        klog_warn("EHCI: Failed to read Configuration Header for Addr %u", dev_addr);
        return;
    }

    usb_cfg_desc_t *cfg_desc = (usb_cfg_desc_t *)cfg_hdr;
    size_t total_len = cfg_desc->wTotalLength;
    if (total_len < 9 || total_len > 512) {
        total_len = 256;
    }

    uint8_t cfg_buf[512];
    memset(cfg_buf, 0, sizeof(cfg_buf));

    req.bmRequestType = 0x80;
    req.bRequest = USB_REQ_GET_DESCRIPTOR;
    req.wValue = (USB_DESC_CONFIGURATION << 8);
    req.wIndex = 0;
    req.wLength = total_len;

    if (!ehci_exec_control_transfer(hc, dev_addr, speed, ep0_max_packet, hub_addr, hub_port, &req, cfg_buf, total_len, true)) {
        klog_warn("EHCI: Failed to read full Configuration Descriptors for Addr %u", dev_addr);
        return;
    }

    /* Check if this device is a USB Hub (Class 0x09) */
    if (dev_desc.bDeviceClass == USB_CLASS_HUB) {
        req.bmRequestType = 0x00;
        req.bRequest = USB_REQ_SET_CONFIGURATION;
        req.wValue = 1;
        req.wIndex = 0;
        req.wLength = 0;
        ehci_exec_control_transfer(hc, dev_addr, speed, ep0_max_packet, hub_addr, hub_port, &req, NULL, 0, false);

        ehci_enumerate_hub_ports(hc, dev_addr, speed, ep0_max_packet);
        return;
    }

    /* Parse Configuration Descriptors for HID Keyboard */
    bool is_keyboard = false;
    uint8_t ep_in_num = 0;
    uint16_t ep_in_max_packet = 8;
    uint8_t ep_in_interval = 10;

    size_t offset = 0;
    usb_cfg_desc_t *cfg = (usb_cfg_desc_t *)cfg_buf;
    size_t actual_len = cfg->wTotalLength < sizeof(cfg_buf) ? cfg->wTotalLength : sizeof(cfg_buf);
    offset += cfg->bLength;

    while (offset + 2 <= actual_len) {
        uint8_t len = cfg_buf[offset];
        uint8_t type = cfg_buf[offset + 1];
        if (len == 0 || offset + len > actual_len)
            break;

        if (type == USB_DESC_INTERFACE) {
            usb_if_desc_t *if_desc = (usb_if_desc_t *)&cfg_buf[offset];
            if (if_desc->bInterfaceClass == USB_CLASS_HID && if_desc->bInterfaceProtocol == 1) {
                is_keyboard = true;
            }
        } else if (type == USB_DESC_ENDPOINT) {
            usb_ep_desc_t *ep = (usb_ep_desc_t *)&cfg_buf[offset];
            if ((ep->bEndpointAddress & 0x80) && is_keyboard && ep_in_num == 0) {
                ep_in_num = ep->bEndpointAddress & 0x0F;
                ep_in_max_packet = ep->wMaxPacketSize & 0x7FF;
                ep_in_interval = ep->bInterval;
            }
        }
        offset += len;
    }

    /* 5. SET_CONFIGURATION */
    req.bmRequestType = 0x00;
    req.bRequest = USB_REQ_SET_CONFIGURATION;
    req.wValue = cfg->bConfigurationValue ? cfg->bConfigurationValue : 1;
    req.wIndex = 0;
    req.wLength = 0;
    ehci_exec_control_transfer(hc, dev_addr, speed, ep0_max_packet, hub_addr, hub_port, &req, NULL, 0, false);

    if (is_keyboard) {
        /* 6. HID Class: SET_IDLE (0, 0) */
        req.bmRequestType = 0x21;
        req.bRequest = USB_HID_REQ_SET_IDLE;
        req.wValue = 0;
        req.wIndex = 0;
        req.wLength = 0;
        ehci_exec_control_transfer(hc, dev_addr, speed, ep0_max_packet, hub_addr, hub_port, &req, NULL, 0, false);

        /* 7. HID Class: SET_PROTOCOL (0 = Boot Protocol) */
        req.bmRequestType = 0x21;
        req.bRequest = USB_HID_REQ_SET_PROTOCOL;
        req.wValue = 0;
        req.wIndex = 0;
        req.wLength = 0;
        ehci_exec_control_transfer(hc, dev_addr, speed, ep0_max_packet, hub_addr, hub_port, &req, NULL, 0, false);

        /* Allocate device slot */
        ehci_device_t *dev = &hc->devices[hc->device_count++];
        dev->active = true;
        dev->address = dev_addr;
        dev->port_num = port_num;
        dev->speed = speed;
        dev->vendor_id = dev_desc.idVendor;
        dev->product_id = dev_desc.idProduct;
        dev->device_class = dev_desc.bDeviceClass;
        dev->is_keyboard = true;
        dev->parent_hub_addr = hub_addr;
        dev->parent_hub_port = hub_port;
        dev->ep_in_num = ep_in_num ? ep_in_num : 1;
        dev->ep_in_max_packet = ep_in_max_packet ? ep_in_max_packet : 8;
        dev->ep_in_interval = ep_in_interval;
        dev->ep_in_toggle = 0;

        /* Allocate Interrupt DMA Buffer and Descriptors */
        uintptr_t intr_dma_phys = pmm_alloc_page();
        if (intr_dma_phys) {
            memset((void *)PHYS_TO_VIRT(intr_dma_phys), 0, PAGE_SIZE);
            dev->report_buf_phys = intr_dma_phys;
            dev->report_buf_virt = (uint8_t *)PHYS_TO_VIRT(intr_dma_phys);
            dev->report_len = 8;

            dev->intr_qh = (ehci_qh_t *)(dev->report_buf_virt + 64);
            dev->intr_qh_phys = (uint32_t)(intr_dma_phys + 64);

            dev->intr_qtd = (ehci_qtd_t *)(dev->report_buf_virt + 192);
            dev->intr_qtd_phys = (uint32_t)(intr_dma_phys + 192);

            ehci_init_qh(dev->intr_qh, dev->intr_qh_phys, dev_addr, dev->ep_in_num, speed, dev->ep_in_max_packet, hub_addr, hub_port);
            ehci_init_qtd(dev->intr_qtd, dev->intr_qtd_phys, EHCI_QTD_PID_IN, dev->report_buf_virt, dev->report_buf_phys, 8, false, true);

            dev->intr_qh->qtd_next = dev->intr_qtd_phys;
            dev->intr_qh->qh_curqtd = 0;
            dev->intr_qh->qh_link = EHCI_LINK_TERMINATE;

            /* Link Interrupt QH into the 1024-entry Periodic Frame List (1ms schedule) */
            if (hc->periodic_frame_list) {
                for (int i = 0; i < EHCI_FRAMELIST_COUNT; i++) {
                    hc->periodic_frame_list[i] = dev->intr_qh_phys | EHCI_LINK_TYPE_QH;
                }
            }

            klog_info("EHCI: USB Keyboard attached on Port %u, Addr %u (VID: %04x, PID: %04x)",
                      port_num, dev_addr, dev->vendor_id, dev->product_id);
        }
    } else {
        klog_info("EHCI: USB Device attached on Port %u, Addr %u (VID: %04x, PID: %04x, Class: %02x)",
                  port_num, dev_addr, dev_desc.idVendor, dev_desc.idProduct, dev_desc.bDeviceClass);
    }
}

/* ==============================================================================
 * Section 6: Root Hub Port Probe & Reset Engine
 * ============================================================================== */
static void ehci_probe_ports(ehci_controller_t *hc) {
    for (uint8_t p = 0; p < hc->max_ports && p < 16; p++) {
        uintptr_t port_reg = hc->op_base + EHCI_PORTSC_BASE + (p * 4);
        uint32_t portsc = ehci_read32(port_reg);

        /* 1. Turn on Port Power (PP = 1) */
        if (!(portsc & EHCI_PORT_PP)) {
            ehci_write32(port_reg, portsc | EHCI_PORT_PP);
            udelay(20000); /* 20ms power stabilization */
            portsc = ehci_read32(port_reg);
        }

        /* 2. Check for Device Connection */
        if (portsc & EHCI_PORT_CCS) {
            uint32_t line_status = (portsc >> 10) & 0x03;

            /* If Low-Speed device on Root Port without internal TT, release to companion */
            if (line_status == 1) {
                klog_info("EHCI: Low-speed device on Root Port %u (Line status 0x%x), releasing to companion", p + 1, line_status);
                ehci_write32(port_reg, portsc | EHCI_PORT_PO);
                continue;
            }

            /* 3. Reset Port (Port Reset = 1) */
            portsc = ehci_read32(port_reg);
            portsc &= ~EHCI_PORT_W1C_MASK;
            portsc |= EHCI_PORT_PR;
            ehci_write32(port_reg, portsc);

            udelay(50000); /* 50ms Port Reset */

            portsc = ehci_read32(port_reg);
            portsc &= ~EHCI_PORT_PR;
            ehci_write32(port_reg, portsc);

            udelay(20000); /* 20ms Reset Recovery */

            portsc = ehci_read32(port_reg);
            uint8_t speed = (portsc & EHCI_PORT_PE) ? USB_SPEED_HIGH : USB_SPEED_FULL;

            ehci_enumerate_device(hc, p + 1, speed, 0, 0);
        }
    }
}

/* ==============================================================================
 * Section 7: Controller Initialization
 * ============================================================================== */
static void ehci_init_controller(pci_device_t *pci_dev) {
    if (g_ehci_count >= MAX_EHCI_CONTROLLERS)
        return;

    ehci_controller_t *hc = &g_ehci_controllers[g_ehci_count];
    memset(hc, 0, sizeof(ehci_controller_t));
    hc->pci_dev = pci_dev;
    hc->next_address = 1;
    spinlock_init(&hc->lock);

    /* Enable PCI Bus Master and MMIO */
    pci_enable_bus_mastering(pci_dev);

    uintptr_t bar0 = pci_dev->bar[0] & ~0xFULL;
    if (!bar0) {
        klog_warn("EHCI: Invalid BAR0 for PCI %02x:%02x.%u", pci_dev->bus, pci_dev->slot, pci_dev->func);
        return;
    }

    /* Map MMIO Space (2 pages = 8KB) */
    uintptr_t bar0_virt = (uintptr_t)PHYS_TO_VIRT(bar0);
    vmm_map_page(&g_kernel_pagemap, bar0_virt, bar0,
                 VMM_FLAG_WRITABLE | VMM_FLAG_PRESENT | VMM_FLAG_CACHE_DISABLE);
    vmm_map_page(&g_kernel_pagemap, bar0_virt + PAGE_SIZE, bar0 + PAGE_SIZE,
                 VMM_FLAG_WRITABLE | VMM_FLAG_PRESENT | VMM_FLAG_CACHE_DISABLE);

    hc->cap_base = bar0_virt;
    hc->cap_length = *(volatile uint8_t *)(hc->cap_base + EHCI_CAPLENGTH);
    hc->version = *(volatile uint16_t *)(hc->cap_base + EHCI_HCIVERSION);
    hc->op_base = hc->cap_base + hc->cap_length;

    uint32_t hcsparams = ehci_read32(hc->cap_base + EHCI_HCSPARAMS);
    hc->max_ports = EHCI_HCS_N_PORTS(hcsparams);

    klog_info("EHCI: Initializing USB 2.0 Controller v%x.%02x (Ports: %u, CAPLEN: %u)",
              hc->version >> 8, hc->version & 0xFF, hc->max_ports, hc->cap_length);

    /* 1. Perform BIOS-to-OS Handover */
    ehci_bios_handover(hc);

    /* 2. Stop Controller (RS = 0) */
    uint32_t usbcmd = ehci_read32(hc->op_base + EHCI_USBCMD);
    usbcmd &= ~EHCI_CMD_RS;
    ehci_write32(hc->op_base + EHCI_USBCMD, usbcmd);

    int timeout = 1000;
    while (!(ehci_read32(hc->op_base + EHCI_USBSTS) & EHCI_STS_HCH) && --timeout > 0) {
        udelay(10);
    }

    /* 3. Reset Controller (HCRESET = 1) */
    ehci_write32(hc->op_base + EHCI_USBCMD, EHCI_CMD_HCRESET);
    timeout = 1000;
    while ((ehci_read32(hc->op_base + EHCI_USBCMD) & EHCI_CMD_HCRESET) && --timeout > 0) {
        udelay(10);
    }
    udelay(10000);

    /* 4. Set 32-bit DMA Segment */
    ehci_write32(hc->op_base + EHCI_CTRLDSSEGMENT, 0);

    /* 5. Clear Interrupts & Status */
    ehci_write32(hc->op_base + EHCI_USBSTS, 0x3F);
    ehci_write32(hc->op_base + EHCI_USBINTR, 0);

    /* 6. Allocate DMA Pool for Asynchronous Schedule & Control Transfers */
    uintptr_t pool_phys = pmm_alloc_page();
    if (!pool_phys) {
        klog_error("EHCI: Failed to allocate DMA pool for Async Schedule");
        return;
    }
    memset((void *)PHYS_TO_VIRT(pool_phys), 0, PAGE_SIZE);
    uint8_t *pool_virt = (uint8_t *)PHYS_TO_VIRT(pool_phys);

    /* Async Head QH */
    hc->async_head_qh = (ehci_qh_t *)pool_virt;
    hc->async_head_qh_phys = pool_phys;
    ehci_init_qh(hc->async_head_qh, (uint32_t)pool_phys, 0, 0, USB_SPEED_HIGH, 64, 0, 0);
    hc->async_head_qh->qh_endp |= EHCI_QH_HRECL; /* Head of Reclamation List */
    hc->async_head_qh->qh_link = (uint32_t)pool_phys | EHCI_LINK_TYPE_QH; /* Points to itself */

    /* Temporary Control QH (Slot 1) */
    hc->ctrl_qh = (ehci_qh_t *)(pool_virt + sizeof(ehci_qh_t));
    hc->ctrl_qh_phys = pool_phys + sizeof(ehci_qh_t);

    /* Control qTDs (Offset 512) */
    hc->ctrl_qtd_pool = (ehci_qtd_t *)(pool_virt + 512);
    hc->ctrl_qtd_pool_phys = pool_phys + 512;

    /* Control Data Buffer (Offset 1024) */
    hc->ctrl_data_buf = pool_virt + 1024;
    hc->ctrl_data_buf_phys = pool_phys + 1024;

    /* Set Async List Base Address */
    ehci_write32(hc->op_base + EHCI_ASYNCLISTADDR, (uint32_t)pool_phys);

    /* 7. Allocate & Configure Periodic Frame List (1024 entries = 4KB page) */
    uintptr_t pframe_phys = pmm_alloc_page();
    if (pframe_phys) {
        memset((void *)PHYS_TO_VIRT(pframe_phys), 0, PAGE_SIZE);
        hc->periodic_frame_list = (uint32_t *)PHYS_TO_VIRT(pframe_phys);
        hc->periodic_frame_list_phys = pframe_phys;

        for (int i = 0; i < EHCI_FRAMELIST_COUNT; i++) {
            hc->periodic_frame_list[i] = EHCI_LINK_TERMINATE;
        }

        ehci_write32(hc->op_base + EHCI_PERIODICLISTBASE, (uint32_t)pframe_phys);
    }

    /* 8. Route all ports to EHCI (CONFIGFLAG = 1) */
    ehci_write32(hc->op_base + EHCI_CONFIGFLAG, EHCI_CONFIGFLAG_ROUTE);
    udelay(10000);

    /* 9. Enable Async + Periodic Schedules and Start Controller (RS = 1 | ASE | PSE | ITC_1) */
    usbcmd = ehci_read32(hc->op_base + EHCI_USBCMD);
    usbcmd |= EHCI_CMD_RS | EHCI_CMD_ASE | EHCI_CMD_PSE | EHCI_CMD_ITC_1;
    ehci_write32(hc->op_base + EHCI_USBCMD, usbcmd);

    timeout = 1000;
    while ((ehci_read32(hc->op_base + EHCI_USBSTS) & EHCI_STS_HCH) && --timeout > 0) {
        udelay(10);
    }

    hc->initialized = true;
    g_ehci_count++;

    /* 10. Probe Root Hub Ports */
    ehci_probe_ports(hc);
}

/* ==============================================================================
 * Section 8: Dynamic Plug-and-Play Hotplug Engine
 * ============================================================================== */
static void ehci_check_hotplug(ehci_controller_t *hc) {
    if (!hc->has_rmh_hub || hc->rmh_hub_ports == 0)
        return;

    usb_setup_pkt_t req;
    req.bmRequestType = 0xA3;
    req.bRequest = USB_REQ_GET_STATUS;
    req.wValue = 0;
    req.wLength = 4;

    for (uint8_t p = 1; p <= hc->rmh_hub_ports; p++) {
        req.wIndex = p;
        uint32_t port_status = 0;
        if (ehci_exec_control_transfer(hc, hc->rmh_hub_addr, hc->rmh_hub_speed, 64, 0, 0, &req, &port_status, 4, true)) {
            bool connected = (port_status & 1) != 0;
            bool tracked = false;
            for (size_t d = 0; d < hc->device_count; d++) {
                if (hc->devices[d].port_num == p && hc->devices[d].active) {
                    tracked = true;
                    break;
                }
            }

            /* Case 1: Newly plugged in device */
            if (connected && !tracked) {
                klog_info("EHCI: Dynamic Hotplug: Device connected on Hub Port %u", p);

                /* Reset Hub Port */
                usb_setup_pkt_t r_req;
                r_req.bmRequestType = 0x23;
                r_req.bRequest = USB_REQ_SET_FEATURE;
                r_req.wValue = 4; /* PORT_RESET */
                r_req.wIndex = p;
                r_req.wLength = 0;
                ehci_exec_control_transfer(hc, hc->rmh_hub_addr, hc->rmh_hub_speed, 64, 0, 0, &r_req, NULL, 0, false);
                udelay(50000);

                /* Clear Reset Change */
                r_req.bRequest = USB_REQ_CLEAR_FEATURE;
                r_req.wValue = 20; /* C_PORT_RESET */
                ehci_exec_control_transfer(hc, hc->rmh_hub_addr, hc->rmh_hub_speed, 64, 0, 0, &r_req, NULL, 0, false);
                udelay(20000);

                /* Query Speed */
                req.wIndex = p;
                port_status = 0;
                ehci_exec_control_transfer(hc, hc->rmh_hub_addr, hc->rmh_hub_speed, 64, 0, 0, &req, &port_status, 4, true);

                uint8_t dev_speed = USB_SPEED_FULL;
                if (port_status & (1 << 9)) {
                    dev_speed = USB_SPEED_LOW;
                } else if (port_status & (1 << 10)) {
                    dev_speed = USB_SPEED_HIGH;
                }

                ehci_enumerate_device(hc, p, dev_speed, hc->rmh_hub_addr, p);
            }
            /* Case 2: Device unplugged */
            else if (!connected && tracked) {
                for (size_t d = 0; d < hc->device_count; d++) {
                    if (hc->devices[d].port_num == p && hc->devices[d].active) {
                        hc->devices[d].active = false;
                        klog_info("EHCI: Dynamic Hotplug: Device detached from Hub Port %u", p);
                        break;
                    }
                }
            }
        }
    }
}

/* ==============================================================================
 * Section 9: Public API & Periodic Polling Engine
 * ============================================================================== */
void ehci_poll(void) {
    for (size_t c = 0; c < g_ehci_count; c++) {
        ehci_controller_t *hc = &g_ehci_controllers[c];
        if (!hc->initialized)
            continue;

        /* Check dynamic PnP hotplug events periodically */
        if (++hc->pnp_poll_counter >= 100) {
            hc->pnp_poll_counter = 0;
            ehci_check_hotplug(hc);
        }

        for (size_t d = 0; d < hc->device_count; d++) {
            ehci_device_t *dev = &hc->devices[d];
            if (!dev->active || !dev->is_keyboard || !dev->intr_qtd)
                continue;

            uint32_t token = dev->intr_qtd->qtd_status;

            /* Check if transfer completed (Active bit 7 cleared) */
            if (!(token & EHCI_QTD_ACTIVE)) {
                if (!(token & (EHCI_QTD_HALTED | EHCI_QTD_BABBLE))) {
                    size_t rem = EHCI_QTD_GET_BYTES(token);
                    if (rem < dev->report_len) {
                        /* Process USB Keyboard 8-byte HID report */
                        hid_process_keyboard_report(dev->report_buf_virt);
                    }
                }

                /* Re-arm interrupt qTD for next report */
                dev->ep_in_toggle = !dev->ep_in_toggle;
                uint32_t next_token = EHCI_QTD_ACTIVE |
                                      EHCI_QTD_PID_IN |
                                      EHCI_QTD_CERR_MAX |
                                      EHCI_QTD_IOC |
                                      EHCI_QTD_BYTES(dev->report_len);
                if (dev->ep_in_toggle) {
                    next_token |= EHCI_QTD_TOGGLE;
                }
                dev->intr_qtd->qtd_status = next_token;

                dev->intr_qh->qh_curqtd = 0;
                dev->intr_qh->qtd_next = dev->intr_qtd_phys;
                dev->intr_qh->qtd_status = 0;
            }
        }
    }
}

bool ehci_is_active(void) {
    return g_ehci_count > 0;
}

size_t ehci_get_device_list_info(char *buf, size_t max_len) {
    if (!buf || max_len == 0)
        return 0;

    size_t pos = 0;
    for (size_t c = 0; c < g_ehci_count && pos < max_len; c++) {
        ehci_controller_t *hc = &g_ehci_controllers[c];
        if (!hc->initialized)
            continue;

        uint16_t hc_vid = hc->pci_dev ? hc->pci_dev->vendor_id : 0x8086;
        uint16_t hc_pid = hc->pci_dev ? hc->pci_dev->device_id : 0x3b34;
        pos += ksnprintf(buf + pos, max_len - pos,
                         "Bus %03zu Device 001: ID %04x:%04x EHCI USB 2.0 Host Controller (Ports: %u)\n",
                         c + 1, hc_vid, hc_pid, hc->max_ports);

        for (size_t d = 0; d < hc->device_count && pos < max_len; d++) {
            ehci_device_t *dev = &hc->devices[d];
            if (!dev->active)
                continue;
            const char *type_str = dev->is_keyboard ? "USB Keyboard (HID)" : "USB Device";
            pos += ksnprintf(buf + pos, max_len - pos,
                             "Bus %03zu Device %03u: ID %04x:%04x %s (Port %u, Speed High/Full)\n",
                             c + 1, (unsigned int)(d + 2), dev->vendor_id, dev->product_id,
                             type_str, dev->port_num);
        }
    }

    return pos;
}

void ehci_init(void) {
    for (pci_device_t *dev = pci_get_device_list(); dev != NULL; dev = dev->next) {
        if (dev->class_code == 0x0C && dev->subclass == 0x03 && dev->prog_if == 0x20) {
            ehci_init_controller(dev);
        }
    }

    if (g_ehci_count == 0) {
        klog_info("EHCI: No EHCI USB 2.0 controllers found on PCI bus");
    }
}
