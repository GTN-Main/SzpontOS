/*
 * SzpontOS - USB (Universal Serial Bus) Core Subsystem
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <drivers/usb.h>
#include <drivers/xhci.h>
#include <drivers/ehci.h>
#include <kernel/kprint.h>

void usb_init(void) {
    klog_info("USB: Initializing USB Host Controller Subsystem...");
    xhci_init();
    ehci_init();
    klog_info("USB: USB Core Subsystem initialized successfully!");
}

