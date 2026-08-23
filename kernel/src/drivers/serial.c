/*
 * SzpontOS - 16550A UART Serial Driver (COM1)
 * High-performance buffered I/O with 16-byte hardware FIFO burst transfers
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <drivers/serial.h>
#include <arch/x86_64/io.h>

#define UART_FIFO_SIZE 16
#define UART_TX_TIMEOUT 100000

static bool g_serial_initialized = false;

void serial_init(void) {
    outb(COM1_PORT + 1, 0x00);    /* Disable all interrupts */
    outb(COM1_PORT + 3, 0x80);    /* Enable DLAB (set baud rate divisor) */
    outb(COM1_PORT + 0, 0x01);    /* Set divisor to 1 (lo byte) 115200 baud */
    outb(COM1_PORT + 1, 0x00);    /*                  (hi byte) */
    outb(COM1_PORT + 3, 0x03);    /* 8 bits, no parity, one stop bit */
    outb(COM1_PORT + 2, 0xC7);    /* Enable FIFO, clear RX/TX FIFOs, 14-byte threshold */
    outb(COM1_PORT + 4, 0x0B);    /* RTS/DSR set, IRQ enabled */

    g_serial_initialized = true;
    serial_puts("\n[SERIAL] UART COM1 (115200 baud, 16-byte FIFO) initialized.\n");
}

static inline bool is_transmit_empty(void) {
    return (inb(COM1_PORT + 5) & 0x20) != 0;
}

static inline bool wait_tx_ready(void) {
    int timeout = UART_TX_TIMEOUT;
    while (!is_transmit_empty() && --timeout > 0) {
        __asm__ volatile ("pause");
    }
    return timeout > 0;
}

void serial_putc(char c) {
    if (!g_serial_initialized) return;

    if (!wait_tx_ready()) return;

    if (c == '\n') {
        outb(COM1_PORT, '\r');
        if (!wait_tx_ready()) return;
    }

    outb(COM1_PORT, (uint8_t)c);
}

void serial_puts(const char *str) {
    if (!str) return;
    while (*str) {
        serial_putc(*str++);
    }
}

void serial_write(const char *buf, size_t len) {
    if (!g_serial_initialized || !buf || len == 0) return;

    size_t i = 0;
    while (i < len) {
        if (!wait_tx_ready()) break;

        /* Write up to 16 bytes in a single burst into 16550 FIFO */
        size_t burst = 0;
        while (i < len && burst < UART_FIFO_SIZE) {
            char c = buf[i++];
            if (c == '\n') {
                outb(COM1_PORT, '\r');
                burst++;
                if (burst >= UART_FIFO_SIZE) {
                    if (!wait_tx_ready()) break;
                    burst = 0;
                }
            }
            outb(COM1_PORT, (uint8_t)c);
            burst++;
        }
    }
}

bool serial_received(void) {
    return (inb(COM1_PORT + 5) & 1) != 0;
}

char serial_getc(void) {
    while (!serial_received()) {
        __asm__ volatile ("pause");
    }
    return (char)inb(COM1_PORT);
}
