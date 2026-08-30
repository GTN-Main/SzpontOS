/*
 * SzpontOS - PS/2 Test Driver (Raw Keypress Detection, NO IRQ)
 *
 * Test-only driver. Its sole purpose is to detect that PS/2 keys are
 * being pressed. It does NOT translate scancodes into characters, does NOT
 * feed the normal keyboard ring buffer, and most importantly it NEVER
 * enables the keyboard IRQ. All input is gathered by polling the i8042
 * output buffer (port 0x60) directly.
 *
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <drivers/ps2_test.h>
#include <drivers/keyboard.h>
#include <drivers/serial.h>
#include <arch/x86_64/io.h>
#include <arch/x86_64/pic.h>
#include <kernel/kprint.h>

/* i8042 ports */
#define PS2T_DATA_PORT 0x60
#define PS2T_STATUS_PORT 0x64
#define PS2T_COMMAND_PORT 0x64

/* Status register bits */
#define PS2T_OBF 0x01
#define PS2T_IBF 0x02
#define PS2T_AUX_OBF 0x20

/* Controller commands */
#define PS2T_READ_CTR 0x20
#define PS2T_WRITE_CTR 0x60
#define PS2T_DISABLE_KBD 0xAD
#define PS2T_ENABLE_KBD 0xAE
#define PS2T_DISABLE_AUX 0xA7

/* CCB bits */
#define PS2T_CTR_KBDINT 0x01
#define PS2T_CTR_AUXINT 0x02
#define PS2T_CTR_XLATE 0x40

/* Keyboard device commands */
#define PS2T_CMD_ENABLE_KBD 0xF4
#define PS2T_CMD_SET_DEFAULTS 0xF6

#define PS2T_BUFFER_SIZE 128

/*
 * The keyboard IRQ (IRQ1) is NEVER enabled in this test driver.
 * We unconditionally mask it in the legacy PIC so that even if some other
 * subsystem tried to set the CCB bit it would stay silent at the PIC level.
 */
#define PS2T_KEYBOARD_IRQ 1

/* Raw scancode ring (do not decode, just report presses) */
static uint8_t g_ps2t_buffer[PS2T_BUFFER_SIZE];
static volatile size_t g_ps2t_read_ptr = 0;
static volatile size_t g_ps2t_write_ptr = 0;

static bool g_ps2t_present = false;

bool ps2_test_active(void) {
    return g_ps2t_present;
}

static inline uint8_t ps2t_read_status(void) {
    return inb(PS2T_STATUS_PORT);
}

static inline uint8_t ps2t_read_data(void) {
    return inb(PS2T_DATA_PORT);
}

static inline void ps2t_write_cmd(uint8_t cmd) {
    outb(PS2T_COMMAND_PORT, cmd);
    io_wait();
}

static inline void ps2t_write_data(uint8_t data) {
    outb(PS2T_DATA_PORT, data);
    io_wait();
}

static bool ps2t_wait_write(uint32_t timeout_us) {
    uint32_t elapsed = 0;
    while (elapsed < timeout_us) {
        uint8_t status = ps2t_read_status();
        if (!(status & PS2T_IBF)) {
            return true;
        }
        /* Drain OBF to avoid controller deadlock */
        if (status != 0xFF && (status & PS2T_OBF)) {
            ps2t_read_data();
        }
        udelay(50);
        elapsed += 50;
    }
    return false;
}

static bool ps2t_wait_read(uint32_t timeout_us) {
    uint32_t elapsed = 0;
    while (elapsed < timeout_us) {
        uint8_t status = ps2t_read_status();
        if (status != 0xFF && (status & PS2T_OBF)) {
            return true;
        }
        udelay(50);
        elapsed += 50;
    }
    return false;
}

static int ps2t_read_controller_byte(void) {
    if (!ps2t_wait_write(100000)) {
        return -1;
    }
    ps2t_write_cmd(PS2T_READ_CTR);
    if (!ps2t_wait_read(100000)) {
        return -1;
    }
    return ps2t_read_data();
}

static bool ps2t_write_controller_byte(uint8_t ccb) {
    if (!ps2t_wait_write(100000)) {
        return false;
    }
    ps2t_write_cmd(PS2T_WRITE_CTR);
    if (!ps2t_wait_write(100000)) {
        return false;
    }
    ps2t_write_data(ccb);
    return true;
}

static void ps2t_drain(void) {
    for (int i = 0; i < 64; i++) {
        uint8_t status = ps2t_read_status();
        if (status == 0xFF || !(status & PS2T_OBF)) {
            break;
        }
        ps2t_read_data();
        io_wait();
    }
}

static void ps2t_push_scancode(uint8_t scancode) {
    size_t w = __atomic_load_n(&g_ps2t_write_ptr, __ATOMIC_RELAXED);
    size_t r = __atomic_load_n(&g_ps2t_read_ptr, __ATOMIC_ACQUIRE);
    size_t next = (w + 1) % PS2T_BUFFER_SIZE;
    if (next != r) {
        g_ps2t_buffer[w] = scancode;
        __atomic_store_n(&g_ps2t_write_ptr, next, __ATOMIC_RELEASE);
    }
}

/*
 * Poll the i8042 output buffer without any IRQ dependency.
 * Every byte removed from port 0x60 is reported verbatim as a raw
 * scancode (press + release, including 0xE0/0xE1 prefixes on Set 2).
 * No character translation is performed.
 */
void ps2_test_poll(void) {
    if (!g_ps2t_present) {
        return;
    }

    uint8_t status = ps2t_read_status();
    if (status == 0xFF || !(status & PS2T_OBF)) {
        return;
    }

    for (int retry = 0; retry < 64; retry++) {
        uint8_t s = ps2t_read_status();
        if (s == 0xFF || !(s & PS2T_OBF)) {
            break;
        }

        uint8_t data = ps2t_read_data();

        /* Mouse bytes (AUX OBF set) are silently dropped. */
        if (s & PS2T_AUX_OBF) {
            continue;
        }

        /* [kbd-trace] B1: log EVERY byte including prefixes (0xE0/0xE1/0xF0).
         * TEMPORARY instrumentation. */
        klog_info("[kbd-trace] B1 ps2 raw=0x%02x", data);

        ps2t_push_scancode(data);

        /* Forward the FULL raw stream (prefixes included) to the keyboard
         * layer state machine. It owns press/release, E0/E1/F0 and keymap. */
        keyboard_handle_incoming_byte(data);
    }
}

bool ps2_test_has_key(void) {
    ps2_test_poll();
    return __atomic_load_n(&g_ps2t_read_ptr, __ATOMIC_ACQUIRE) !=
           __atomic_load_n(&g_ps2t_write_ptr, __ATOMIC_ACQUIRE);
}

bool ps2_test_key_was_pressed(uint8_t *scancode) {
    if (!ps2_test_has_key()) {
        return false;
    }
    size_t r = __atomic_load_n(&g_ps2t_read_ptr, __ATOMIC_RELAXED);
    if (scancode) {
        *scancode = g_ps2t_buffer[r];
    }
    size_t next = (r + 1) % PS2T_BUFFER_SIZE;
    __atomic_store_n(&g_ps2t_read_ptr, next, __ATOMIC_RELEASE);
    return true;
}

/*
 * Minimal i8042 bring-up. Keyboard IRQ is left completely disabled:
 *  - CCB keyboard IRQ bit is cleared.
 *  - The keyboard IRQ is masked in the legacy PIC.
 * No IDT handler is registered, so IRQ1 is never serviced.
 */
void ps2_test_init(void) {
    /* 1. Mask the keyboard IRQ in the PIC so it can never fire. */
    pic_set_mask(PS2T_KEYBOARD_IRQ);
    pic_set_mask(2); /* Cascade */
    klog_info("ps2-test: Keyboard IRQ1 masked (test driver, IRQ disabled)");

    /* 2. Flush any stale data. */
    ps2t_drain();

    /* 3. Read CCB, force-disable keyboard IRQ, enable interface. */
    int ccb = ps2t_read_controller_byte();
    if (ccb < 0) {
        klog_warn("ps2-test: Could not read CCB, controller may be absent");
        return;
    }

    uint8_t ctbb = (uint8_t)ccb;
    ctbb &= ~(PS2T_CTR_KBDINT | PS2T_CTR_AUXINT); /* No keyboard/mouse IRQs */
    ctbb |= PS2T_CTR_XLATE;
    ps2t_write_controller_byte(ctbb);

    /* [kbd-trace] Verify the controller actually ACCEPTED the XLATE bit.
     * On some bare-metal firmware/controllers the write is ignored and the
     * device emits raw Set 2 — switch the decoder accordingly. */
    int ccb_readback = ps2t_read_controller_byte();
    if (ccb_readback >= 0) {
        if (((uint8_t)ccb_readback) & PS2T_CTR_XLATE) {
            klog_info("ps2-test: CCB readback 0x%02x (XLATE accepted, Set 1 mode)",
                      (uint8_t)ccb_readback);
        } else {
            klog_warn("ps2-test: CCB readback 0x%02x (XLATE REJECTED, switching decoder to native Set 2)",
                      (uint8_t)ccb_readback);
            keyboard_force_set2_mode();
        }
    }

    /* 4. Disable keyboard port during setup, then re-enable scanning. */
    if (ps2t_wait_write(100000)) {
        ps2t_write_cmd(PS2T_DISABLE_KBD);
    }
    ps2t_drain();

    /* 5. Put keyboard into defaults + scanning (commands, not IRQs). */
    if (ps2t_wait_write(100000)) {
        ps2t_write_data(PS2T_CMD_SET_DEFAULTS);
    }
    if (ps2t_wait_read(200000)) {
        ps2t_read_data(); /* ACK */
    }
    ps2t_drain();

    if (ps2t_wait_write(100000)) {
        ps2t_write_data(PS2T_CMD_ENABLE_KBD);
    }
    if (ps2t_wait_read(200000)) {
        ps2t_read_data(); /* ACK */
    }

    /* 6. Enable keyboard clock line. */
    if (ps2t_wait_write(100000)) {
        ps2t_write_cmd(PS2T_ENABLE_KBD);
    }

    ps2t_drain();

    g_ps2t_present = true;
    klog_info("ps2-test: i8042 present (polling mode, IRQ disabled)");
}
