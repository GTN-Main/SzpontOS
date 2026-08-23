#ifndef SZPONTOS_DRIVERS_E1000_H
#define SZPONTOS_DRIVERS_E1000_H

#include <kernel/types.h>
#include <drivers/pci.h>
#include <net/net.h>

#define E1000_VENDOR_INTEL 0x8086
#define E1000_DEV_82540EM  0x100E
#define E1000_DEV_82545EM  0x100F
#define E1000_DEV_82574L   0x10D3

/* E1000 MMIO Register Offsets */
#define REG_CTRL      0x0000
#define REG_STATUS    0x0008
#define REG_EEPROM    0x0014
#define REG_CTRL_EXT  0x0018
#define REG_ICR       0x00C0
#define REG_IMS       0x00D0
#define REG_RCTL      0x0100
#define REG_TCTL      0x0400
#define REG_RDBAL     0x2800
#define REG_RDBAH     0x2804
#define REG_RDLEN     0x2808
#define REG_RDH       0x2810
#define REG_RDT       0x2818
#define REG_TDBAL     0x3800
#define REG_TDBAH     0x3804
#define REG_TDLEN     0x3808
#define REG_TDH       0x3810
#define REG_TDT       0x3818
#define REG_MTA       0x5200
#define REG_RAL       0x5400
#define REG_RAH       0x5404

#define E1000_NUM_RX_DESC 32
#define E1000_NUM_TX_DESC 32

/* RX Descriptor */
typedef struct __attribute__((packed)) {
    uint64_t address;
    uint16_t length;
    uint16_t checksum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
} e1000_rx_desc_t;

/* TX Descriptor */
typedef struct __attribute__((packed)) {
    uint64_t address;
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
} e1000_tx_desc_t;

bool e1000_init(pci_device_t *pci_dev);
int  e1000_send(netif_t *netif, net_buf_t *buf);
void e1000_poll(void);

#endif /* SZPONTOS_DRIVERS_E1000_H */
