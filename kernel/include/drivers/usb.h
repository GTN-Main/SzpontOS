/*
 * SzpontOS - USB (Universal Serial Bus) Core Definitions
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef SZPONTOS_DRIVERS_USB_H
#define SZPONTOS_DRIVERS_USB_H

#include <kernel/types.h>
#include <stdbool.h>

/* USB Speed constants */
#define USB_SPEED_FULL 1
#define USB_SPEED_LOW 2
#define USB_SPEED_HIGH 3
#define USB_SPEED_SUPER 4
#define USB_SPEED_SUPER_PLUS 5

/* USB Standard Request Types */
#define USB_REQ_TYPE_STANDARD (0 << 5)
#define USB_REQ_TYPE_CLASS (1 << 5)
#define USB_REQ_TYPE_VENDOR (2 << 5)

/* USB Request Recipients */
#define USB_RECIP_DEVICE 0
#define USB_RECIP_INTERFACE 1
#define USB_RECIP_ENDPOINT 2
#define USB_RECIP_OTHER 3

/* USB Standard Request Codes */
#define USB_REQ_GET_STATUS 0x00
#define USB_REQ_CLEAR_FEATURE 0x01
#define USB_REQ_SET_FEATURE 0x03
#define USB_REQ_SET_ADDRESS 0x05
#define USB_REQ_GET_DESCRIPTOR 0x06
#define USB_REQ_SET_DESCRIPTOR 0x07
#define USB_REQ_GET_CONFIGURATION 0x08
#define USB_REQ_SET_CONFIGURATION 0x09
#define USB_REQ_GET_INTERFACE 0x0A
#define USB_REQ_SET_INTERFACE 0x0B
#define USB_REQ_SYNCH_FRAME 0x0C

/* USB HID Class Request Codes */
#define USB_HID_REQ_GET_REPORT 0x01
#define USB_HID_REQ_GET_IDLE 0x02
#define USB_HID_REQ_GET_PROTOCOL 0x03
#define USB_HID_REQ_SET_REPORT 0x09
#define USB_HID_REQ_SET_IDLE 0x0A
#define USB_HID_REQ_SET_PROTOCOL 0x0B

/* USB Descriptor Types */
#define USB_DESC_DEVICE 1
#define USB_DESC_CONFIGURATION 2
#define USB_DESC_STRING 3
#define USB_DESC_INTERFACE 4
#define USB_DESC_ENDPOINT 5
#define USB_DESC_HID 0x21
#define USB_DESC_REPORT 0x22

/* USB Device Classes */
#define USB_CLASS_PER_INTERFACE 0x00
#define USB_CLASS_AUDIO 0x01
#define USB_CLASS_COMM 0x02
#define USB_CLASS_HID 0x03
#define USB_CLASS_PHYSICAL 0x05
#define USB_CLASS_IMAGE 0x06
#define USB_CLASS_PRINTER 0x07
#define USB_CLASS_MASS_STORAGE 0x08
#define USB_CLASS_HUB 0x09
#define USB_CLASS_DATA 0x0A
#define USB_CLASS_SMART_CARD 0x0B
#define USB_CLASS_VIDEO 0x0E
#define USB_CLASS_WIRELESS 0xE0
#define USB_CLASS_MISC 0xEF

/* USB HID Subclasses & Protocols */
#define USB_HID_SUBCLASS_BOOT 0x01
#define USB_HID_PROTO_KEYBOARD 0x01
#define USB_HID_PROTO_MOUSE 0x02

/* USB Setup Packet Structure (8 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} usb_setup_pkt_t;

/* USB Device Descriptor (18 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
} usb_dev_desc_t;

/* USB Configuration Descriptor (9 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wTotalLength;
    uint8_t bNumInterfaces;
    uint8_t bConfigurationValue;
    uint8_t iConfiguration;
    uint8_t bmAttributes;
    uint8_t bMaxPower;
} usb_cfg_desc_t;

/* USB Interface Descriptor (9 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} usb_if_desc_t;

/* USB Endpoint Descriptor (7 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bEndpointAddress;
    uint8_t bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t bInterval;
} usb_ep_desc_t;

/* USB HID Descriptor (9 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdHID;
    uint8_t bCountryCode;
    uint8_t bNumDescriptors;
    uint8_t bReportDescriptorType;
    uint16_t wDescriptorLength;
} usb_hid_desc_t;

void usb_init(void);

#endif /* SZPONTOS_DRIVERS_USB_H */
