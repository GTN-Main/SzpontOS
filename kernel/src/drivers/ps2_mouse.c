/*
 * SzpontOS - PS/2 Mouse Driver (i8042 AUX Port / IRQ 12)
 * Inspired by FreeBSD sys/dev/atkbdc/psm.c
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <drivers/ps2_mouse.h>
#include <drivers/keyboard.h>
#include <drivers/ioapic.h>
#include <drivers/acpi.h>
#include <arch/x86_64/idt.h>
#include <arch/x86_64/io.h>
#include <arch/x86_64/pic.h>
#include <kernel/kprint.h>
#include <kernel/spinlock.h>
#include <kernel/string.h>

#define I8042_DATA_PORT 0x60
#define I8042_STATUS_PORT 0x64
#define I8042_COMMAND_PORT 0x64

#define I8042_STATUS_OBF (1 << 0)
#define I8042_STATUS_IBF (1 << 1)
#define I8042_STATUS_AUX_OBF (1 << 5)

#define I8042_CMD_READ_CONFIG 0x20
#define I8042_CMD_WRITE_CONFIG 0x60
#define I8042_CMD_DISABLE_AUX 0xA7
#define I8042_CMD_ENABLE_AUX 0xA8
#define I8042_CMD_TEST_AUX 0xA9
#define I8042_CMD_WRITE_AUX 0xD4

#define MOUSE_CMD_SET_SCALE11 0xE6
#define MOUSE_CMD_SET_SCALE21 0xE7
#define MOUSE_CMD_SET_RES 0xE8
#define MOUSE_CMD_STATUS_REQ 0xE9
#define MOUSE_CMD_SET_STREAM 0xEA
#define MOUSE_CMD_GET_ID 0xF2
#define MOUSE_CMD_SET_SAMPLE 0xF3
#define MOUSE_CMD_ENABLE_DATA 0xF4
#define MOUSE_CMD_DISABLE_DATA 0xF5
#define MOUSE_CMD_SET_DEFAULT 0xF6
#define MOUSE_CMD_RESET 0xFF

#define MOUSE_RESP_ACK 0xFA
#define MOUSE_RESP_BAT_OK 0xAA

#define MOUSE_QUEUE_SIZE 256

static mouse_packet_t g_mouse_queue[MOUSE_QUEUE_SIZE];
static size_t g_mouse_read_ptr = 0;
static size_t g_mouse_write_ptr = 0;
static spinlock_t g_mouse_lock = SPINLOCK_INIT;

static uint8_t g_packet_bytes[4];
static uint8_t g_packet_idx = 0;
static bool g_has_wheel = false;
static bool g_mouse_initialized = false;

static bool ps2_mouse_wait_write(void) {
    int timeout = 2000;
    while (--timeout) {
        uint8_t status = inb(I8042_STATUS_PORT);
        if (!(status & I8042_STATUS_IBF))
            return true;
        udelay(50);
        io_wait();
    }
    return false;
}

static bool ps2_mouse_wait_read(void) {
    int timeout = 2000;
    while (--timeout) {
        uint8_t status = inb(I8042_STATUS_PORT);
        if (status != 0xFF && (status & I8042_STATUS_OBF))
            return true;
        udelay(50);
        io_wait();
    }
    return false;
}

static void ps2_mouse_write(uint8_t data) {
    if (!ps2_mouse_wait_write())
        return;
    outb(I8042_COMMAND_PORT, I8042_CMD_WRITE_AUX);
    io_wait();
    udelay(50);
    if (!ps2_mouse_wait_write())
        return;
    outb(I8042_DATA_PORT, data);
    io_wait();
    udelay(50);
}

static uint8_t ps2_mouse_read(void) {
    if (!ps2_mouse_wait_read())
        return 0xFF;
    return inb(I8042_DATA_PORT);
}

static bool ps2_mouse_send_cmd(uint8_t cmd) {
    for (int retry = 0; retry < 2; retry++) {
        ps2_mouse_write(cmd);
        uint8_t resp = ps2_mouse_read();
        if (resp == MOUSE_RESP_ACK)
            return true;
    }
    return false;
}

static void ps2_mouse_set_sample_rate(uint8_t rate) {
    ps2_mouse_send_cmd(MOUSE_CMD_SET_SAMPLE);
    ps2_mouse_send_cmd(rate);
}

static void mouse_enqueue_packet(const mouse_packet_t *pkt) {
    spinlock_acquire(&g_mouse_lock);
    size_t next = (g_mouse_write_ptr + 1) % MOUSE_QUEUE_SIZE;
    if (next != g_mouse_read_ptr) {
        g_mouse_queue[g_mouse_write_ptr] = *pkt;
        g_mouse_write_ptr = next;
    }
    spinlock_release(&g_mouse_lock);
}

#include <drivers/mouse.h>

void ps2_mouse_handle_byte(uint8_t byte) {
    /* Byte 0 must have bit 3 always set (Sync bit) */
    if (g_packet_idx == 0 && !(byte & 0x08)) {
        /* Discard out-of-sync byte — NEVER forward to keyboard handler */
        return;
    }

    g_packet_bytes[g_packet_idx++] = byte;
    uint8_t max_bytes = g_has_wheel ? 4 : 3;

    if (g_packet_idx >= max_bytes) {
        g_packet_idx = 0;

        uint8_t flags = g_packet_bytes[0];
        int32_t dx = g_packet_bytes[1];
        int32_t dy = g_packet_bytes[2];
        int8_t dz = 0;

        /* Sign extension */
        if (flags & 0x10)
            dx |= 0xFFFFFF00; /* X sign bit */
        if (flags & 0x20)
            dy |= 0xFFFFFF00; /* Y sign bit */

        if (g_has_wheel) {
            dz = (int8_t)g_packet_bytes[3];
            if (dz & 0x80)
                dz |= (int8_t)0xF0;
            dz &= 0x0F;
            if (dz & 0x08)
                dz -= 16;
        }

        mouse_packet_t pkt;
        pkt.buttons = flags & 0x07;
        pkt.dx = dx;
        pkt.dy = dy;
        pkt.dz = dz;

        mouse_enqueue_packet(&pkt);

        /* Push to generic mouse subsystem */
        mouse_event_t ev;
        ev.buttons = flags & 0x07;
        ev.dx = dx;
        ev.dy = dy;
        ev.dz = dz;
        ev.abs_x = 0;
        ev.abs_y = 0;
        ev.is_absolute = false;
        mouse_push_event(&ev);
    }
}

static void mouse_irq_handler(interrupt_frame_t *frame) {
    UNUSED(frame);
    keyboard_poll_hardware();
}

void ps2_mouse_init(void) {
    spinlock_init(&g_mouse_lock);

    uint8_t status = inb(I8042_STATUS_PORT);
    if (status == 0xFF) {
        g_mouse_initialized = false;
        return;
    }

    /* 1. Enable AUX Port Clock (Command 0xA8) */
    if (ps2_mouse_wait_write()) {
        outb(I8042_COMMAND_PORT, I8042_CMD_ENABLE_AUX);
        io_wait();
    }
    udelay(10000);

    /* 2. Read and update controller configuration byte (Enable IRQ 12 & IRQ 1) */
    uint8_t config = 0x47;
    if (ps2_mouse_wait_write()) {
        outb(I8042_COMMAND_PORT, I8042_CMD_READ_CONFIG);
        io_wait();
        if (ps2_mouse_wait_read()) {
            config = inb(I8042_DATA_PORT);
        }
    }

    /* Bit 0: Enable KB IRQ 1, Bit 1: Enable AUX IRQ 12, Bit 4: Enable KB Clock, Bit 5: Enable AUX Clock, Bit 6: KB Translation */
    config |= (1 << 0) | (1 << 1) | (1 << 6);
    config &= ~((1 << 4) | (1 << 5));

    if (ps2_mouse_wait_write()) {
        outb(I8042_COMMAND_PORT, I8042_CMD_WRITE_CONFIG);
        io_wait();
        udelay(50);
        if (ps2_mouse_wait_write()) {
            outb(I8042_DATA_PORT, config);
            io_wait();
        }
    }

    /* 3. Send Reset to Pointing Device (Command 0xFF via 0xD4) */
    int reset_res = ps2_mouse_send_cmd(MOUSE_CMD_RESET);
    if (reset_res < 0) {
        /* No mouse responded to reset */
        goto no_mouse;
    }
    udelay(50000); /* 50ms BAT delay */
    keyboard_drain_buffers();

    uint8_t bat = ps2_mouse_read();
    uint8_t dev_id = ps2_mouse_read();
    if (bat != 0xAA) {
        goto no_mouse;
    }

    /* 5. Probe for IntelliMouse 3D Scroll Wheel (Sample rates: 200 -> 100 -> 80) */
    ps2_mouse_set_sample_rate(200);
    ps2_mouse_set_sample_rate(100);
    ps2_mouse_set_sample_rate(80);

    ps2_mouse_send_cmd(MOUSE_CMD_GET_ID);
    uint8_t mouse_id = ps2_mouse_read();
    if (mouse_id == 3 || mouse_id == 4) {
        g_has_wheel = true;
        klog_info("PS/2 Mouse: IntelliMouse with scroll wheel detected (ID %d)", mouse_id);
    } else {
        g_has_wheel = false;
        klog_info("PS/2 Mouse: Standard PS/2 mouse detected (ID %d)", dev_id != 0xFF ? dev_id : 0);
    }

    /* 6. Set default sample rate & enable streaming */
    ps2_mouse_set_sample_rate(100);
    ps2_mouse_send_cmd(MOUSE_CMD_SET_RES);
    ps2_mouse_send_cmd(3); /* 8 counts/mm */
    ps2_mouse_send_cmd(MOUSE_CMD_ENABLE_DATA);

    /* 7. Register Interrupt Handler & Route IRQ 12 */
    isr_register_handler(IRQ12, mouse_irq_handler);
    if (!ioapic_is_active()) {
        pic_clear_mask(12);
    }
    ioapic_map_irq(12, 0x2C, 0, false, false);

    g_mouse_initialized = true;
    klog_info("PS/2 Mouse: Driver initialized successfully (IRQ 12 active)");
    return;

no_mouse:
    /*
     * Dell Latitude / Laptop EC safety rule:
     * Never send 0xA7 (Disable AUX Port) or set Bit 5 (AUX Clock Disable) in CCB!
     * On Dell Latitudes and laptops with multiplexed ECs, disabling AUX clock
     * shuts down the shared scan matrix clock for the keyboard.
     */
    config = 0x47;
    if (ps2_mouse_wait_write()) {
        outb(I8042_COMMAND_PORT, I8042_CMD_READ_CONFIG);
        io_wait();
        if (ps2_mouse_wait_read()) {
            config = inb(I8042_DATA_PORT);
        }
    }

    /* Enable KBD IRQ 1, Keep both KBD and AUX clocks active, Enable Translation */
    config |= (1 << 0) | (1 << 6);
    config &= ~((1 << 4) | (1 << 5)); /* Ensure both clocks remain ENABLED */

    if (ps2_mouse_wait_write()) {
        outb(I8042_COMMAND_PORT, I8042_CMD_WRITE_CONFIG);
        io_wait();
        udelay(50);
        if (ps2_mouse_wait_write()) {
            outb(I8042_DATA_PORT, config);
            io_wait();
        }
    }

    /* Ensure Keyboard Port (0xAE) remains enabled */
    if (ps2_mouse_wait_write()) {
        outb(I8042_COMMAND_PORT, 0xAE);
        io_wait();
    }

    pic_clear_mask(1);
    g_mouse_initialized = false;
    keyboard_drain_buffers();

    klog_info("PS/2 Mouse: No PS/2 mouse detected on AUX port (Controller left in safe dual-clock mode)");
}

bool ps2_mouse_is_enabled(void) {
    return g_mouse_initialized;
}

bool ps2_mouse_has_packet(void) {
    spinlock_acquire(&g_mouse_lock);
    bool has_pkt = (g_mouse_read_ptr != g_mouse_write_ptr);
    spinlock_release(&g_mouse_lock);
    return has_pkt;
}

bool ps2_mouse_get_packet(mouse_packet_t *pkt) {
    if (!pkt)
        return false;
    spinlock_acquire(&g_mouse_lock);
    if (g_mouse_read_ptr == g_mouse_write_ptr) {
        spinlock_release(&g_mouse_lock);
        return false;
    }
    *pkt = g_mouse_queue[g_mouse_read_ptr];
    g_mouse_read_ptr = (g_mouse_read_ptr + 1) % MOUSE_QUEUE_SIZE;
    spinlock_release(&g_mouse_lock);
    return true;
}

ssize_t ps2_mouse_devfs_read(void *buf, size_t count) {
    if (!buf || count < sizeof(mouse_packet_t))
        return 0;
    mouse_packet_t pkt;
    if (ps2_mouse_get_packet(&pkt)) {
        memcpy(buf, &pkt, sizeof(mouse_packet_t));
        return sizeof(mouse_packet_t);
    }
    return 0;
}

