/*
 * SzpontOS - Rock-Solid PS/2 & AT Keyboard Driver
 * Faithfully engineered following standard i8042 hardware specification,
 * OSDev reference, Linux (drivers/input/serio/i8042.c), and FreeBSD (sys/dev/atkbdc).
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <drivers/keyboard.h>
#include <drivers/ps2_mouse.h>
#include <drivers/xhci.h>
#include <drivers/ehci.h>
#include <drivers/acpi.h>
#include <drivers/ioapic.h>
#include <arch/x86_64/idt.h>
#include <arch/x86_64/io.h>
#include <arch/x86_64/pic.h>
#include <drivers/framebuffer.h>
#include <drivers/serial.h>
#include <sched/process.h>
#include <sched/sched.h>
#include <kernel/signal.h>
#include <kernel/kprint.h>

/* ==============================================================================
 * 8042 Controller Ports & Status Flags
 * ============================================================================== */
#define KBD_DATA_PORT 0x60
#define KBD_STATUS_PORT 0x64
#define KBD_COMMAND_PORT 0x64

/* Status Register Bits (Port 0x64 Read) */
#define KBDS_OBF 0x01          /* Bit 0: Output Buffer Full (Data ready in 0x60) */
#define KBDS_IBF 0x02          /* Bit 1: Input Buffer Full (Controller busy) */
#define KBDS_SYSTEM_FLAG 0x04  /* Bit 2: System Flag (Passed POST) */
#define KBDS_CMD_DATA 0x08     /* Bit 3: 0 = Data for device, 1 = Command for controller */
#define KBDS_INHIBIT_FLAG 0x10 /* Bit 4: 1 = Keyboard not inhibited / Keylock open */
#define KBDS_AUX_OBF 0x20      /* Bit 5: 1 = Mouse/AUX data in 0x60 (on Dual PS/2) */
#define KBDS_TIMEOUT_ERR 0x40  /* Bit 6: Timeout Error */
#define KBDS_PARITY_ERR 0x80   /* Bit 7: Parity Error */

/* Controller Commands (Port 0x64 Write) */
#define KBDC_READ_CTR 0x20         /* Read Controller Command Byte (CCB) */
#define KBDC_WRITE_CTR 0x60        /* Write Controller Command Byte (CCB) */
#define KBDC_DISABLE_AUX_PORT 0xA7 /* Disable Mouse/AUX port clock */
#define KBDC_ENABLE_AUX_PORT 0xA8  /* Enable Mouse/AUX port clock */
#define KBDC_TEST_AUX_PORT 0xA9    /* Test Mouse/AUX port */
#define KBDC_SELF_TEST 0xAA        /* Test Controller (Returns 0x55 on success) */
#define KBDC_TEST_KBD_PORT 0xAB    /* Test Keyboard port (Returns 0x00 on success) */
#define KBDC_DISABLE_KBD_PORT 0xAD /* Disable Keyboard port clock */
#define KBDC_ENABLE_KBD_PORT 0xAE  /* Enable Keyboard port clock */
#define KBDC_WRITE_TO_AUX 0xD4     /* Send next byte to Mouse/AUX port */

/* Controller Command Byte (CCB) Bits */
#define KBD_CTR_KBDINT 0x01  /* Bit 0: Enable Keyboard IRQ 1 */
#define KBD_CTR_AUXINT 0x02  /* Bit 1: Enable Mouse IRQ 12 */
#define KBD_CTR_SYSFLAG 0x04 /* Bit 2: System Flag (POST passed) */
#define KBD_CTR_KBDDIS 0x10  /* Bit 4: 1 = Disable KBD clock, 0 = Enable clock */
#define KBD_CTR_AUXDIS 0x20  /* Bit 5: 1 = Disable AUX clock, 0 = Enable clock */
#define KBD_CTR_XLATE 0x40   /* Bit 6: 1 = Translate Set 2 to XT Set 1 */

/* Keyboard Device Commands (Port 0x60 Write) */
#define KBD_CMD_SET_LEDS 0xED
#define KBD_CMD_ECHO 0xEE
#define KBD_CMD_SET_SCANCODE_SET 0xF0
#define KBD_CMD_GET_ID 0xF2
#define KBD_CMD_SET_TYPEMATIC 0xF3
#define KBD_CMD_ENABLE_KBD 0xF4
#define KBD_CMD_DISABLE_KBD 0xF5
#define KBD_CMD_SET_DEFAULTS 0xF6
#define KBD_CMD_RESEND 0xFE
#define KBD_CMD_RESET_BAT 0xFF

/* Keyboard / Controller Responses */
#define KBD_RESP_ACK 0xFA
#define KBD_RESP_RESEND 0xFE
#define KBD_RESP_BAT_OK 0xAA
#define KBD_RESP_BAT_FAIL 0xFC
#define KBD_RESP_SELF_TEST_OK 0x55

#define KEYBOARD_BUFFER_SIZE 1024

/* ==============================================================================
 * Global Lockless Keyboard Ring Buffer State
 * ============================================================================== */
static char g_kb_buffer[KEYBOARD_BUFFER_SIZE];
static volatile size_t g_kb_read_ptr = 0;
static volatile size_t g_kb_write_ptr = 0;

/* Modifiers State */
static bool g_lshift = false;
static bool g_rshift = false;
static bool g_lctrl = false;
static bool g_rctrl = false;
static bool g_lalt = false;
static bool g_ralt = false;
static bool g_caps_lock = false;
static bool g_num_lock = true;
static bool g_scroll_lock = false;

/* Multi-byte Protocol Decoder State */
static bool g_extended = false;      /* 0xE0 prefix received */
static int g_e1_pause_state = 0;     /* 0xE1 pause key sequence counter */
static bool g_set2_release = false;  /* 0xF0 prefix received in Set 2 */
static bool g_i8042_present = false;
static spinlock_t g_i8042_lock = SPINLOCK_INIT;

static inline uint64_t kbd_irqsave(void) {
    uint64_t rflags;
    __asm__ volatile("pushfq; popq %0; cli" : "=r"(rflags) :: "memory");
    return rflags;
}

static inline void kbd_irqrestore(uint64_t rflags) {
    if (rflags & (1 << 9)) {
        __asm__ volatile("sti" ::: "memory");
    }
}

/* ==============================================================================
 * XT Scancode Set 1 Tables (Translated Mode)
 * ============================================================================== */
static const char g_scancode_set1_low[128] = {
    /* 0x00 - 0x0F */
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    /* 0x10 - 0x1F */
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\r', 0, 'a', 's',
    /* 0x20 - 0x2F */
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    /* 0x30 - 0x3F */
    'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
    /* 0x40 - 0x4F (F6-F10, Keypad) */
    0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
    /* 0x50 - 0x5F (Keypad 2, 3, 0, ., F11, F12) */
    '2', '3', '0', '.', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 0x60 - 0x7F */
    0};

static const char g_scancode_set1_shift[128] = {
    /* 0x00 - 0x0F */
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    /* 0x10 - 0x1F */
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\r', 0, 'A', 'S',
    /* 0x20 - 0x2F */
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
    /* 0x30 - 0x3F */
    'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
    /* 0x40 - 0x4F (F6-F10, Keypad) */
    0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
    /* 0x50 - 0x5F (Keypad 2, 3, 0, ., F11, F12) */
    '2', '3', '0', '.', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 0x60 - 0x7F */
    0};

/* Forward declarations */
static void process_scancode(uint8_t scancode);

/* ==============================================================================
 * Low-Level I/O Routines with Microsecond-Precision Delays & OBF-Draining
 * ============================================================================== */
static inline uint8_t kbd_read_status(void) {
    return inb(KBD_STATUS_PORT);
}

static inline uint8_t kbd_read_data(void) {
    return inb(KBD_DATA_PORT);
}

static inline void kbd_write_cmd(uint8_t cmd) {
    outb(KBD_COMMAND_PORT, cmd);
    io_wait();
}

static inline void kbd_write_data(uint8_t data) {
    outb(KBD_DATA_PORT, data);
    io_wait();
}

/*
 * Wait for Input Buffer (IBF) to become clear before writing.
 * Follows FreeBSD atkbdc model: If Output Buffer (OBF) is full while waiting for IBF,
 * drain OBF immediately to prevent hardware controller deadlock on Laptop ECs.
 */
static bool kbd_wait_write(uint32_t timeout_us) {
    uint32_t elapsed = 0;
    while (elapsed < timeout_us) {
        uint8_t status = kbd_read_status();
        if (!(status & KBDS_IBF)) {
            return true;
        }

        /* If OBF is set while waiting for IBF, drain it to unblock controller */
        if (status != 0xFF && (status & KBDS_OBF)) {
            uint8_t data = kbd_read_data();
            if ((status & KBDS_AUX_OBF) && ps2_mouse_is_enabled()) {
                ps2_mouse_handle_byte(data);
            } else {
                process_scancode(data);
            }
        }

        udelay(50);
        io_wait();
        elapsed += 50;
    }
    return false;
}

/*
 * Wait for Output Buffer (OBF) to have data available for reading.
 */
static bool kbd_wait_read(uint32_t timeout_us) {
    uint32_t elapsed = 0;
    while (elapsed < timeout_us) {
        uint8_t status = kbd_read_status();
        if (status != 0xFF && (status & KBDS_OBF)) {
            return true;
        }
        udelay(50);
        io_wait();
        elapsed += 50;
    }
    return false;
}

void keyboard_drain_buffers(void) {
    for (int i = 0; i < 128; i++) {
        uint8_t status = kbd_read_status();
        if (status == 0xFF || !(status & KBDS_OBF)) {
            break;
        }
        udelay(50);
        uint8_t byte = kbd_read_data();
        if ((status & KBDS_AUX_OBF) && ps2_mouse_is_enabled()) {
            ps2_mouse_handle_byte(byte);
        } else if (!(status & KBDS_AUX_OBF)) {
            process_scancode(byte);
        }
    }
}

static int kbd_read_controller_byte(void) {
    if (!kbd_wait_write(100000))
        return -1;
    kbd_write_cmd(KBDC_READ_CTR);
    udelay(50);
    if (!kbd_wait_read(100000))
        return -1;
    return kbd_read_data();
}

static bool kbd_write_controller_byte(uint8_t ccb) {
    if (!kbd_wait_write(100000))
        return false;
    kbd_write_cmd(KBDC_WRITE_CTR);
    udelay(50);
    if (!kbd_wait_write(100000))
        return false;
    kbd_write_data(ccb);
    udelay(50);
    return true;
}

static int kbd_send_device_command(uint8_t cmd) {
    for (int retry = 0; retry < 3; retry++) {
        if (!kbd_wait_write(100000))
            continue;
        kbd_write_data(cmd);
        udelay(50);

        uint32_t elapsed = 0;
        while (elapsed < 200000) {
            if (kbd_wait_read(10000)) {
                uint8_t resp = kbd_read_data();
                if (resp == KBD_RESP_ACK) {
                    return 0; /* Success */
                }
                if (resp == KBD_RESP_RESEND) {
                    break;
                }
            }
            elapsed += 10000;
        }
    }
    return -1;
}

/* ==============================================================================
 * Lockless Atomic Ring Buffer Operations
 * ============================================================================== */
static void push_char_lockless(char ch) {
    size_t w = __atomic_load_n(&g_kb_write_ptr, __ATOMIC_RELAXED);
    size_t r = __atomic_load_n(&g_kb_read_ptr, __ATOMIC_ACQUIRE);
    size_t next_write = (w + 1) % KEYBOARD_BUFFER_SIZE;
    if (next_write != r) {
        g_kb_buffer[w] = ch;
        __atomic_store_n(&g_kb_write_ptr, next_write, __ATOMIC_RELEASE);
    }
}

static void push_str_lockless(const char *str) {
    if (!str)
        return;
    while (*str) {
        push_char_lockless(*str++);
    }
}

void keyboard_push_char(char ch) {
    push_char_lockless(ch);
}

void keyboard_push_str(const char *str) {
    push_str_lockless(str);
}

uint8_t keyboard_get_modifiers(void) {
    uint8_t mods = 0;
    if (g_lshift || g_rshift)
        mods |= KBD_MOD_LSHIFT;
    if (g_lctrl || g_rctrl)
        mods |= KBD_MOD_LCTRL;
    if (g_lalt || g_ralt)
        mods |= KBD_MOD_LALT;
    if (g_caps_lock)
        mods |= KBD_MOD_CAPSLOCK;
    if (g_num_lock)
        mods |= KBD_MOD_NUMLOCK;
    return mods;
}

bool keyboard_is_caps_lock(void) {
    return g_caps_lock;
}

bool keyboard_is_num_lock(void) {
    return g_num_lock;
}

void keyboard_set_leds(bool numlock, bool capslock, bool scrolllock) {
    g_num_lock = numlock;
    g_caps_lock = capslock;
    g_scroll_lock = scrolllock;

    if (!g_i8042_present)
        return;

    uint8_t leds = 0;
    if (scrolllock)
        leds |= 0x01;
    if (numlock)
        leds |= 0x02;
    if (capslock)
        leds |= 0x04;

    kbd_send_device_command(KBD_CMD_SET_LEDS);
    kbd_wait_write(50000);
    kbd_write_data(leds);
    kbd_wait_read(50000);
    kbd_read_data();
}

/* ==============================================================================
 * Native Set 2 to Set 1 Conversion Table (Fallback for non-translating controllers)
 * ============================================================================== */
static bool g_is_set2_mode = false;

static const uint8_t g_set2_to_set1_table[256] = {
    [0x01] = 0x43, /* F9 */
    [0x03] = 0x3F, /* F5 */
    [0x04] = 0x3D, /* F3 */
    [0x05] = 0x3B, /* F1 */
    [0x06] = 0x3C, /* F2 */
    [0x07] = 0x58, /* F12 */
    [0x09] = 0x44, /* F10 */
    [0x0A] = 0x42, /* F8 */
    [0x0B] = 0x40, /* F6 */
    [0x0C] = 0x3E, /* F4 */
    [0x0D] = 0x0F, /* Tab */
    [0x0E] = 0x29, /* ` */
    [0x11] = 0x38, /* LAlt */
    [0x12] = 0x2A, /* LShift */
    [0x14] = 0x1D, /* LCtrl */
    [0x15] = 0x10, /* Q */
    [0x16] = 0x02, /* 1 */
    [0x1A] = 0x2C, /* Z */
    [0x1B] = 0x1F, /* S */
    [0x1C] = 0x1E, /* A */
    [0x1D] = 0x11, /* W */
    [0x1E] = 0x03, /* 2 */
    [0x21] = 0x2E, /* C */
    [0x22] = 0x2D, /* X */
    [0x23] = 0x20, /* D */
    [0x24] = 0x12, /* E */
    [0x25] = 0x05, /* 4 */
    [0x26] = 0x04, /* 3 */
    [0x29] = 0x39, /* Space */
    [0x2A] = 0x2F, /* V */
    [0x2B] = 0x21, /* F */
    [0x2C] = 0x14, /* T */
    [0x2D] = 0x13, /* R */
    [0x2E] = 0x06, /* 5 */
    [0x31] = 0x31, /* N */
    [0x32] = 0x30, /* B */
    [0x33] = 0x23, /* H */
    [0x34] = 0x22, /* G */
    [0x35] = 0x15, /* Y */
    [0x36] = 0x07, /* 6 */
    [0x3A] = 0x32, /* M */
    [0x3B] = 0x24, /* J */
    [0x3C] = 0x16, /* U */
    [0x3D] = 0x08, /* 7 */
    [0x3E] = 0x09, /* 8 */
    [0x41] = 0x33, /* , */
    [0x42] = 0x25, /* K */
    [0x43] = 0x17, /* I */
    [0x44] = 0x18, /* O */
    [0x45] = 0x0B, /* 0 */
    [0x46] = 0x0A, /* 9 */
    [0x49] = 0x34, /* . */
    [0x4A] = 0x35, /* / */
    [0x4B] = 0x26, /* L */
    [0x4C] = 0x27, /* ; */
    [0x4D] = 0x19, /* P */
    [0x4E] = 0x0C, /* - */
    [0x52] = 0x28, /* ' */
    [0x54] = 0x1A, /* [ */
    [0x55] = 0x0D, /* = */
    [0x58] = 0x3A, /* Caps Lock */
    [0x59] = 0x36, /* RShift */
    [0x5A] = 0x1C, /* Enter */
    [0x5B] = 0x1B, /* ] */
    [0x5D] = 0x2B, /* \ */
    [0x66] = 0x0E, /* Backspace */
    [0x69] = 0x4F, /* End */
    [0x6B] = 0x4B, /* Left */
    [0x6C] = 0x47, /* Home */
    [0x70] = 0x52, /* Insert */
    [0x71] = 0x53, /* Delete */
    [0x72] = 0x50, /* Down */
    [0x73] = 0x4C, /* Keypad 5 */
    [0x74] = 0x4D, /* Right */
    [0x75] = 0x48, /* Up */
    [0x76] = 0x01, /* Escape */
    [0x77] = 0x45, /* Num Lock */
    [0x78] = 0x57, /* F11 */
    [0x79] = 0x4E, /* Keypad + */
    [0x7A] = 0x51, /* Page Down */
    [0x7B] = 0x4A, /* Keypad - */
    [0x7C] = 0x37, /* Keypad * */
    [0x7D] = 0x49, /* Page Up */
    [0x7E] = 0x46, /* Scroll Lock */
    [0x83] = 0x41, /* F7 */
};

static inline uint8_t set2_to_set1(uint8_t set2_code) {
    uint8_t mapped = g_set2_to_set1_table[set2_code];
    return mapped ? mapped : set2_code;
}

/* ==============================================================================
 * Deterministic Scancode Decoder (Set 1 XT & Set 2 Native)
 * ============================================================================== */
static void process_scancode(uint8_t scancode) {
    /* 1. Extended Prefix 0xE0 */
    if (scancode == 0xE0) {
        g_extended = true;
        return;
    }

    /* 3. Pause Key Multi-Byte Prefix 0xE1 */
    if (scancode == 0xE1) {
        g_e1_pause_state = 2;
        return;
    }
    if (g_e1_pause_state > 0) {
        g_e1_pause_state--;
        return;
    }

    /* 4. Set 2 Break Code Prefix 0xF0 (only in native Set 2 mode) */
    if (g_is_set2_mode && scancode == 0xF0) {
        g_set2_release = true;
        return;
    }

    /* 5. Handle Set 2 Translation if in Set 2 mode */
    if (g_is_set2_mode) {
        if (g_set2_release) {
            g_set2_release = false;
            uint8_t set1 = set2_to_set1(scancode);
            scancode = set1 | 0x80; /* Mark as released */
        } else {
            scancode = set2_to_set1(scancode);
        }
    }

    /* 6. Extended Keys (0xE0 Prefix) */
    if (g_extended) {
        g_extended = false;

        /* Convert Set 2 extended key codes if applicable */
        if (scancode == 0x75) scancode = 0x48; /* Up */
        else if (scancode == 0x72) scancode = 0x50; /* Down */
        else if (scancode == 0x6B) scancode = 0x4B; /* Left */
        else if (scancode == 0x74) scancode = 0x4D; /* Right */
        else if (scancode == 0x6C) scancode = 0x47; /* Home */
        else if (scancode == 0x69) scancode = 0x4F; /* End */
        else if (scancode == 0x7D) scancode = 0x49; /* Page Up */
        else if (scancode == 0x7A) scancode = 0x51; /* Page Down */
        else if (scancode == 0x70) scancode = 0x52; /* Insert */
        else if (scancode == 0x71) scancode = 0x53; /* Delete */
        else if (scancode == 0x5A) scancode = 0x1C; /* Keypad Enter */

        /* Extended Key Release (Bit 7 Set) */
        if (scancode & 0x80) {
            uint8_t rel = scancode & 0x7F;
            if (rel == 0x1D)
                g_rctrl = false;
            else if (rel == 0x38)
                g_ralt = false;
            return;
        }

        switch (scancode) {
        case 0x48: push_str_lockless("\033[A"); return; /* Up */
        case 0x50: push_str_lockless("\033[B"); return; /* Down */
        case 0x4D: push_str_lockless("\033[C"); return; /* Right */
        case 0x4B: push_str_lockless("\033[D"); return; /* Left */
        case 0x47: push_str_lockless("\033[H"); return; /* Home */
        case 0x4F: push_str_lockless("\033[F"); return; /* End */
        case 0x49: push_str_lockless("\033[5~"); return; /* Page Up */
        case 0x51: push_str_lockless("\033[6~"); return; /* Page Down */
        case 0x52: push_str_lockless("\033[2~"); return; /* Insert */
        case 0x53: push_str_lockless("\033[3~"); return; /* Delete */
        case 0x1C: push_char_lockless('\r'); return;    /* Keypad Enter */
        case 0x35: push_char_lockless('/'); return;     /* Keypad Slash */
        case 0x1D: g_rctrl = true; return;              /* Right Ctrl */
        case 0x38: g_ralt = true; return;               /* Right Alt (AltGr) */
        default: return;
        }
    }


    /* 7. Standard Set 1 Key Releases (Bit 7 Set) */
    if (scancode & 0x80) {
        uint8_t released = scancode & 0x7F;
        if (released == 0x2A)
            g_lshift = false;
        else if (released == 0x36)
            g_rshift = false;
        else if (released == 0x1D)
            g_lctrl = false;
        else if (released == 0x38)
            g_lalt = false;
        return;
    }

    /* 8. Standard Set 1 Modifiers & Special Keys */
    if (scancode == 0x2A) {
        g_lshift = true;
        return;
    }
    if (scancode == 0x36) {
        g_rshift = true;
        return;
    }
    if (scancode == 0x1D) {
        g_lctrl = true;
        return;
    }
    if (scancode == 0x38) {
        g_lalt = true;
        return;
    }
    if (scancode == 0x3A) {
        g_caps_lock = !g_caps_lock;
        return;
    }
    if (scancode == 0x45) {
        g_num_lock = !g_num_lock;
        return;
    }
    if (scancode == 0x46) {
        g_scroll_lock = !g_scroll_lock;
        return;
    }
    if (scancode == 0x01) {
        push_char_lockless(0x1B);
        return; /* ESC */
    }

    /* 9. Function Keys F1-F12 */
    switch (scancode) {
    case 0x3B: push_str_lockless("\033OP"); return; /* F1 */
    case 0x3C: push_str_lockless("\033OQ"); return; /* F2 */
    case 0x3D: push_str_lockless("\033OR"); return; /* F3 */
    case 0x3E: push_str_lockless("\033OS"); return; /* F4 */
    case 0x3F: push_str_lockless("\033[15~"); return; /* F5 */
    case 0x40: push_str_lockless("\033[17~"); return; /* F6 */
    case 0x41: push_str_lockless("\033[18~"); return; /* F7 */
    case 0x42: push_str_lockless("\033[19~"); return; /* F8 */
    case 0x43: push_str_lockless("\033[20~"); return; /* F9 */
    case 0x44: push_str_lockless("\033[21~"); return; /* F10 */
    case 0x57: push_str_lockless("\033[23~"); return; /* F11 */
    case 0x58: push_str_lockless("\033[24~"); return; /* F12 */
    default: break;
    }

    /* 10. Keypad Translation */
    if (!g_num_lock) {
        switch (scancode) {
        case 0x47: push_str_lockless("\033[H"); return;
        case 0x48: push_str_lockless("\033[A"); return;
        case 0x49: push_str_lockless("\033[5~"); return;
        case 0x4B: push_str_lockless("\033[D"); return;
        case 0x4D: push_str_lockless("\033[C"); return;
        case 0x4F: push_str_lockless("\033[F"); return;
        case 0x50: push_str_lockless("\033[B"); return;
        case 0x51: push_str_lockless("\033[6~"); return;
        case 0x52: push_str_lockless("\033[2~"); return;
        case 0x53: push_char_lockless(127); return;
        default: break;
        }
    }

    /* 11. Standard Character Translation */
    bool shift = (g_lshift || g_rshift);
    bool ctrl = (g_lctrl || g_rctrl);

    char ch = 0;
    if (scancode < 128) {
        char base = g_scancode_set1_low[scancode];
        bool use_shift = shift;
        if (g_caps_lock && base >= 'a' && base <= 'z')
            use_shift = !use_shift;
        ch = use_shift ? g_scancode_set1_shift[scancode] : base;
    }

    if (ch == 0)
        return;

    /* 12. Unix Control Characters & Signals */
    if (ctrl) {
        char lower = (ch >= 'A' && ch <= 'Z') ? (char)(ch + 32) : ch;
        if (lower >= 'a' && lower <= 'z') {
            char ctrl_char = (char)(lower - 'a' + 1);
            if (ctrl_char == 0x03) { /* Ctrl+C -> SIGINT */
                process_t *fg = process_get_foreground();
                if (fg)
                    process_send_signal(fg, SIGINT);
                push_char_lockless(0x03);
                return;
            } else if (ctrl_char == 0x1A) { /* Ctrl+Z -> SIGTSTP */
                process_t *fg = process_get_foreground();
                if (fg)
                    process_send_signal(fg, SIGTSTP);
                push_char_lockless(0x1A);
                return;
            } else if (ctrl_char == 0x1C) { /* Ctrl+\ -> SIGQUIT */
                process_t *fg = process_get_foreground();
                if (fg)
                    process_send_signal(fg, SIGQUIT);
                push_char_lockless(0x1C);
                return;
            } else if (ctrl_char == 0x04) { /* Ctrl+D -> EOF */
                push_char_lockless(0x04);
                return;
            }
            push_char_lockless(ctrl_char);
            return;
        }
        if (ch == '[') { push_char_lockless(0x1B); return; }
        if (ch == ']') { push_char_lockless(0x1D); return; }
        if (ch == '\\') {
            process_t *fg = process_get_foreground();
            if (fg)
                process_send_signal(fg, SIGQUIT);
            push_char_lockless(0x1C);
            return;
        }
        if (ch == ' ') { push_char_lockless(0x00); return; }
        if (ch == '\r' || ch == '\n') { push_char_lockless('\r'); return; }
    }

    push_char_lockless(ch);
}

void keyboard_handle_incoming_byte(uint8_t scancode) {
    process_scancode(scancode);
}

/* ==============================================================================
 * Hardware Polling Engine with Re-entrancy Protection
 * ============================================================================== */
void keyboard_poll_hardware(void) {
    uint8_t quick_status = kbd_read_status();
    if (quick_status == 0xFF || !(quick_status & KBDS_OBF)) {
        return;
    }

    uint64_t flags = kbd_irqsave();
    spinlock_acquire(&g_i8042_lock);

    for (int retry = 0; retry < 64; retry++) {
        for (volatile int d = 0; d < 10; d++) {
            __asm__ volatile("pause");
        }

        uint8_t status = kbd_read_status();
        if (status == 0xFF || !(status & KBDS_OBF)) {
            break;
        }

        uint8_t data = kbd_read_data();

        if (status & KBDS_AUX_OBF) {
            /* AUX/Mouse data — route to mouse driver or fallback to keyboard */
            if (ps2_mouse_is_enabled()) {
                ps2_mouse_handle_byte(data);
            } else {
                keyboard_handle_incoming_byte(data);
            }
        } else {
            keyboard_handle_incoming_byte(data);
        }
    }

    spinlock_release(&g_i8042_lock);
    kbd_irqrestore(flags);
}

static void keyboard_irq_handler(interrupt_frame_t *frame) {
    UNUSED(frame);
    keyboard_poll_hardware();
}

/* ==============================================================================
 * Fault-Tolerant, Bare-Metal & Laptop EC Proof i8042 Initialization Sequence
 * ============================================================================== */
void keyboard_init(void) {
    /* 1. Flush any stale data first */
    for (int i = 0; i < 100; i++) {
        if (inb(KBD_STATUS_PORT) & KBDS_OBF) {
            inb(KBD_DATA_PORT);
            io_wait();
        }
    }

    /* 2. Disable both keyboard and mouse ports during setup */
    if (kbd_wait_write(100000)) {
        kbd_write_cmd(KBDC_DISABLE_KBD_PORT);
    }
    io_wait();
    if (kbd_wait_write(100000)) {
        kbd_write_cmd(KBDC_DISABLE_AUX_PORT);
    }
    io_wait();

    /* 3. Flush any pending data */
    keyboard_drain_buffers();

    /* 4. Read Controller Command Byte (CCB) */
    int ccb = kbd_read_controller_byte();
    if (ccb < 0) {
        ccb = KBD_CTR_XLATE | KBD_CTR_SYSFLAG;
    } else {
        /* Disable IRQs (bits 0,1), enable clocks (bits 4,5 = 0), enable translation (bit 6), system flag (bit 2) */
        ccb &= ~(KBD_CTR_KBDINT | KBD_CTR_AUXINT | KBD_CTR_KBDDIS | KBD_CTR_AUXDIS);
        ccb |= (KBD_CTR_XLATE | KBD_CTR_SYSFLAG);
    }
    kbd_write_controller_byte((uint8_t)ccb);
    io_wait();

    /* 5. Controller Self-Test (Command 0xAA) with non-fatal timeout */
    if (kbd_wait_write(100000)) {
        kbd_write_cmd(KBDC_SELF_TEST);
        io_wait();
        int timeout = 10000;
        while (!(inb(KBD_STATUS_PORT) & KBDS_OBF) && timeout > 0) {
            io_wait();
            timeout--;
        }
        if (timeout > 0) {
            uint8_t self_test = inb(KBD_DATA_PORT);
            if (self_test == KBD_RESP_SELF_TEST_OK) {
                klog_info("atkbdc: Controller self-test PASSED (0x55 OK)");
            } else {
                klog_warn("atkbdc: Controller self-test returned 0x%02x (continuing)", self_test);
            }
        }
    }

    /* Re-write CCB (some controllers reset CCB after self-test) */
    kbd_write_controller_byte((uint8_t)ccb);
    io_wait();

    /* 6. Enable both keyboard and mouse ports */
    if (kbd_wait_write(100000)) {
        kbd_write_cmd(KBDC_ENABLE_KBD_PORT);
    }
    io_wait();
    if (kbd_wait_write(100000)) {
        kbd_write_cmd(KBDC_ENABLE_AUX_PORT);
    }
    io_wait();

    /* Flush output buffer */
    keyboard_drain_buffers();

    /* 7. Reset keyboard to defaults (0xF6) */
    kbd_send_device_command(KBD_CMD_SET_DEFAULTS);
    for (int i = 0; i < 5000; i++) io_wait();
    keyboard_drain_buffers();

    /* 8. Enable keyboard scanning (0xF4) */
    kbd_send_device_command(KBD_CMD_ENABLE_KBD);
    for (int i = 0; i < 5000; i++) io_wait();
    keyboard_drain_buffers();

    /* 9. Register Interrupt Handler in IDT */
    isr_register_handler(IRQ1, keyboard_irq_handler);

    /* 10. Enable Keyboard IRQ in CCB */
    ccb = kbd_read_controller_byte();
    if (ccb < 0) {
        ccb = KBD_CTR_XLATE | KBD_CTR_SYSFLAG | KBD_CTR_KBDINT;
    } else {
        ccb |= (KBD_CTR_KBDINT | KBD_CTR_XLATE | KBD_CTR_SYSFLAG);
        ccb &= ~(KBD_CTR_KBDDIS | KBD_CTR_AUXDIS);
    }
    kbd_write_controller_byte((uint8_t)ccb);

    /* Check if XLATE bit was retained */
    int final_ccb = kbd_read_controller_byte();
    if (final_ccb >= 0 && !(final_ccb & KBD_CTR_XLATE)) {
        g_is_set2_mode = true;
        klog_info("atkbdc: Translation XLATE is disabled by controller (using native Set 2 mode)");
    } else {
        g_is_set2_mode = false;
    }

    /* 11. Unmask IRQ 1 in legacy PIC and route in IO-APIC */
    pic_clear_mask(1);
    pic_clear_mask(2); /* Cascade IRQ 2 */

    ioapic_map_irq(1, 0x21, 0, false, false);

    /* Final drain */
    keyboard_drain_buffers();

    g_i8042_present = true;
    klog_info("atkbd: Driver attached successfully (IRQ 1 active, mode: %s)",
              g_is_set2_mode ? "Set 2 (Native)" : "Set 1 (Translated)");
}


/* ==============================================================================
 * Universal Character Interface
 * ============================================================================== */
bool keyboard_has_char(void) {
    /* 1. Poll USB Keyboards (xHCI 3.0 & EHCI 2.0) */
    xhci_poll();
    ehci_poll();

    /* 2. Poll Serial UART Input */
    while (serial_received()) {
        char c = serial_getc();
        keyboard_push_char(c);
    }

    /* 3. Poll Hardware i8042 Ports */
    keyboard_poll_hardware();

    return (__atomic_load_n(&g_kb_read_ptr, __ATOMIC_ACQUIRE) !=
            __atomic_load_n(&g_kb_write_ptr, __ATOMIC_ACQUIRE));
}

char keyboard_getc(void) {
    while (!keyboard_has_char()) {
        __asm__ volatile("pause");
        sched_yield();
    }

    size_t r = __atomic_load_n(&g_kb_read_ptr, __ATOMIC_RELAXED);
    char ch = g_kb_buffer[r];
    size_t next_read = (r + 1) % KEYBOARD_BUFFER_SIZE;
    __atomic_store_n(&g_kb_read_ptr, next_read, __ATOMIC_RELEASE);

    return ch;
}
