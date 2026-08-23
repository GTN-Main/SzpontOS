#include <drivers/keyboard.h>
#include <arch/x86_64/idt.h>
#include <arch/x86_64/io.h>
#include <arch/x86_64/pic.h>
#include <drivers/framebuffer.h>
#include <drivers/serial.h>
#include <sched/process.h>
#include <sched/sched.h>
#include <kernel/signal.h>
#include <kernel/kprint.h>
#include <kernel/spinlock.h>

#define KEYBOARD_DATA_PORT    0x60
#define KEYBOARD_STATUS_PORT  0x64
#define KEYBOARD_BUFFER_SIZE  512

static char g_kb_buffer[KEYBOARD_BUFFER_SIZE];
static size_t g_kb_read_ptr = 0;
static size_t g_kb_write_ptr = 0;
static spinlock_t g_kb_lock = SPINLOCK_INIT;

/* Modifiers and parser state */
static bool g_shift = false;
static bool g_caps_lock = false;
static bool g_ctrl = false;
static bool g_alt = false;
static bool g_extended = false;
static bool g_set2_release = false;

/* Scancode decoding mode: 0 = Auto-detect, 1 = Scancode Set 1, 2 = Scancode Set 2 */
static int g_scancode_mode = 0;

/* ==============================================================================
 * Scancode Set 1 (IBM PC/XT, translated PS/2, QEMU default)
 * ============================================================================== */
static const char g_scancode_set1_low[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\r',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

static const char g_scancode_set1_shift[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\r',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

/* ==============================================================================
 * Scancode Set 2 (Native AT/PS/2, Modern UEFI USB Legacy Emulation)
 * ============================================================================== */
static const char g_scancode_set2_low[128] = {
    /* 0x00 - 0x0F */
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,   '\t', '`',  0,
    /* 0x10 - 0x1F */
    0,    0,    0,    0,    0,   'q',  '1',  0,
    0,    0,   'z',  's',  'a',  'w',  '2',  0,
    /* 0x20 - 0x2F */
    0,   'c',  'x',  'd',  'e',  '4',  '3',  0,
    0,   ' ',  'v',  'f',  't',  'r',  '5',  0,
    /* 0x30 - 0x3F */
    0,   'n',  'b',  'h',  'g',  'y',  '6',  0,
    0,    0,   'm',  'j',  'u',  '7',  '8',  0,
    /* 0x40 - 0x4F */
    0,   ',',  'k',  'i',  'o',  '0',  '9',  0,
    0,   '.',  '/',  'l',  ';',  'p',  '-',  0,
    /* 0x50 - 0x5F */
    0,    0,  '\'',  0,   '[',  '=',  0,    0,
    0,    0,   '\r', ']',   0,   '\\', 0,    0,
    /* 0x60 - 0x6F */
    0,    0,    0,    0,    0,    0,   '\b', 0,
    0,   '1',   0,   '4',  '7',   0,    0,    0,
    /* 0x70 - 0x7F */
    '0', '.',  '2',  '5',  '6',  '8',   27,   0,
    0,   '+',  '3',  '-',  '*',  '9',   0,    0
};

static const char g_scancode_set2_shift[128] = {
    /* 0x00 - 0x0F */
    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,   '\t', '~',  0,
    /* 0x10 - 0x1F */
    0,    0,    0,    0,    0,   'Q',  '!',  0,
    0,    0,   'Z',  'S',  'A',  'W',  '@',  0,
    /* 0x20 - 0x2F */
    0,   'C',  'X',  'D',  'E',  '$',  '#',  0,
    0,   ' ',  'V',  'F',  'T',  'R',  '%',  0,
    /* 0x30 - 0x3F */
    0,   'N',  'B',  'H',  'G',  'Y',  '^',  0,
    0,    0,   'M',  'J',  'U',  '&',  '*',  0,
    /* 0x40 - 0x4F */
    0,   '<',  'K',  'I',  'O',  ')',  '(',  0,
    0,   '>',  '?',  'L',  ':',  'P',  '_',  0,
    /* 0x50 - 0x5F */
    0,    0,   '"',  0,   '{',  '+',  0,    0,
    0,    0,   '\r', '}',   0,   '|',  0,    0,
    /* 0x60 - 0x6F */
    0,    0,    0,    0,    0,    0,   '\b', 0,
    0,   '1',   0,   '4',  '7',   0,    0,    0,
    /* 0x70 - 0x7F */
    '0', '.',  '2',  '5',  '6',  '8',   27,   0,
    0,   '+',  '3',  '-',  '*',  '9',   0,    0
};

static void push_char_unlocked(char ch) {
    size_t next_write = (g_kb_write_ptr + 1) % KEYBOARD_BUFFER_SIZE;
    if (next_write != g_kb_read_ptr) {
        g_kb_buffer[g_kb_write_ptr] = ch;
        g_kb_write_ptr = next_write;
    }
}

static void push_char(char ch) {
    spinlock_acquire(&g_kb_lock);
    push_char_unlocked(ch);
    spinlock_release(&g_kb_lock);
}

static void push_str_unlocked(const char *str) {
    if (!str) return;
    while (*str) {
        push_char_unlocked(*str++);
    }
}

static void process_scancode_unlocked(uint8_t scancode) {
    /* Extended scancode prefix (0xE0) */
    if (scancode == 0xE0) {
        g_extended = true;
        return;
    }

    /* Scancode Set 2 Break Prefix (0xF0) */
    if (scancode == 0xF0) {
        g_set2_release = true;
        g_scancode_mode = 2; /* Confirmed Scancode Set 2 */
        return;
    }

    /* Ignore controller responses (ACK, Resend, Self-test OK) */
    if (scancode == 0xFA || scancode == 0xFE || scancode == 0xAA) {
        return;
    }

    /* --------------------------------------------------------------------------
     * Key Release Handling
     * -------------------------------------------------------------------------- */
    if (g_set2_release) {
        g_set2_release = false;
        /* Set 2 Modifiers Release */
        if (scancode == 0x12 || scancode == 0x59) {
            g_shift = false;
        } else if (scancode == 0x14) {
            g_ctrl = false;
        } else if (scancode == 0x11) {
            g_alt = false;
        }
        g_extended = false;
        return;
    }

    /* Set 1 Key Release (Bit 7 set without 0xF0 prefix) */
    if (!g_extended && (scancode & 0x80) && g_scancode_mode != 2) {
        uint8_t released = scancode & 0x7F;
        if (released == 0x2A || released == 0x36) {
            g_shift = false;
        } else if (released == 0x1D) {
            g_ctrl = false;
        } else if (released == 0x38) {
            g_alt = false;
        }
        g_scancode_mode = 1; /* Confirmed Scancode Set 1 */
        g_extended = false;
        return;
    }

    /* --------------------------------------------------------------------------
     * Extended Keys Handling (Arrows, Home, End, Delete, Keypad)
     * -------------------------------------------------------------------------- */
    if (g_extended) {
        g_extended = false;

        /* Set 1 Extended (0x48/50/4D/4B etc.) & Set 2 Extended (0x75/72/74/6B etc.) */
        if (scancode == 0x48 || scancode == 0x75) { push_str_unlocked("\033[A"); return; } /* Up Arrow */
        if (scancode == 0x50 || scancode == 0x72) { push_str_unlocked("\033[B"); return; } /* Down Arrow */
        if (scancode == 0x4D || scancode == 0x74) { push_str_unlocked("\033[C"); return; } /* Right Arrow */
        if (scancode == 0x4B || scancode == 0x6B) { push_str_unlocked("\033[D"); return; } /* Left Arrow */
        if (scancode == 0x47 || scancode == 0x6C) { push_str_unlocked("\033[H"); return; } /* Home */
        if (scancode == 0x4F || scancode == 0x69) { push_str_unlocked("\033[F"); return; } /* End */
        if (scancode == 0x53 || scancode == 0x71) { push_str_unlocked("\033[3~"); return; } /* Delete */
        if (scancode == 0x49 || scancode == 0x7D) { push_str_unlocked("\033[5~"); return; } /* Page Up */
        if (scancode == 0x51 || scancode == 0x7A) { push_str_unlocked("\033[6~"); return; } /* Page Down */
        if (scancode == 0x1C || scancode == 0x5A) { push_char_unlocked('\r'); return; }    /* Keypad Enter */
        if (scancode == 0x1D || scancode == 0x14) { g_ctrl = true; return; }              /* Right Ctrl */
        if (scancode == 0x38 || scancode == 0x11) { g_alt = true; return; }               /* Right Alt */
        if (scancode == 0x35 || scancode == 0x4A) { push_char_unlocked('/'); return; }     /* Keypad Slash */
        return;
    }

    /* --------------------------------------------------------------------------
     * Modifiers Press Detection (Set 1 & Set 2)
     * -------------------------------------------------------------------------- */
    if (scancode == 0x2A || scancode == 0x36) {
        g_shift = true;
        g_scancode_mode = 1;
        return;
    }
    if (scancode == 0x12 || scancode == 0x59) {
        g_shift = true;
        g_scancode_mode = 2;
        return;
    }
    if (scancode == 0x1D) {
        g_ctrl = true;
        if (g_scancode_mode == 0) g_scancode_mode = 1;
        return;
    }
    if (scancode == 0x14) {
        g_ctrl = true;
        g_scancode_mode = 2;
        return;
    }
    if (scancode == 0x38) {
        g_alt = true;
        if (g_scancode_mode == 0) g_scancode_mode = 1;
        return;
    }
    if (scancode == 0x11) {
        g_alt = true;
        g_scancode_mode = 2;
        return;
    }
    if (scancode == 0x3A) {
        g_caps_lock = !g_caps_lock;
        g_scancode_mode = 1;
        return;
    }
    if (scancode == 0x58) {
        g_caps_lock = !g_caps_lock;
        g_scancode_mode = 2;
        return;
    }

    /* --------------------------------------------------------------------------
     * Character Translation (Adaptive Set 1 / Set 2)
     * -------------------------------------------------------------------------- */
    char ch = 0;

    if (g_scancode_mode == 2) {
        /* Set 2 decoding */
        if (scancode < 128) {
            ch = g_shift ? g_scancode_set2_shift[scancode] : g_scancode_set2_low[scancode];
        }
    } else if (g_scancode_mode == 1) {
        /* Set 1 decoding */
        if (scancode < 128) {
            ch = g_shift ? g_scancode_set1_shift[scancode] : g_scancode_set1_low[scancode];
        }
    } else {
        /* Mode 0 (Auto): Check unambiguous indicators */
        if (scancode == 0x5A) {
            /* Set 2 Enter */
            ch = '\r';
            g_scancode_mode = 2;
        } else if (scancode == 0x66) {
            /* Set 2 Backspace */
            ch = '\b';
            g_scancode_mode = 2;
        } else if (scancode == 0x76) {
            /* Set 2 Escape */
            ch = 27;
            g_scancode_mode = 2;
        } else if (scancode < 128) {
            ch = g_shift ? g_scancode_set1_shift[scancode] : g_scancode_set1_low[scancode];
        }
    }

    /* Caps Lock adjustment for letters */
    if (g_caps_lock && ch >= 'a' && ch <= 'z') {
        ch -= 32;
    } else if (g_caps_lock && ch >= 'A' && ch <= 'Z' && g_shift) {
        ch += 32;
    }

    if (ch == 0) return;

    /* --------------------------------------------------------------------------
     * Control Character Handling & Unix Signals (SIGINT, SIGTSTP, SIGQUIT)
     * -------------------------------------------------------------------------- */
    if (g_ctrl) {
        char lower = (ch >= 'A' && ch <= 'Z') ? (char)(ch + 32) : ch;
        if (lower >= 'a' && lower <= 'z') {
            char ctrl_char = (char)(lower - 'a' + 1);
            if (ctrl_char == 0x03) { /* Ctrl+C -> SIGINT */
                process_t *fg = process_get_foreground();
                if (fg) {
                    process_send_signal(fg, SIGINT);
                }
                push_char_unlocked(0x03);
                return;
            } else if (ctrl_char == 0x1A) { /* Ctrl+Z -> SIGTSTP */
                process_t *fg = process_get_foreground();
                if (fg) {
                    process_send_signal(fg, SIGTSTP);
                }
                push_char_unlocked(0x1A);
                return;
            } else if (ctrl_char == 0x1C) { /* Ctrl+\ -> SIGQUIT */
                process_t *fg = process_get_foreground();
                if (fg) {
                    process_send_signal(fg, SIGQUIT);
                }
                push_char_unlocked(0x1C);
                return;
            }
            push_char_unlocked(ctrl_char);
            return;
        }
        if (ch == '[') { push_char_unlocked(0x1B); return; /* ESC */ }
        if (ch == ']') { push_char_unlocked(0x1D); return; }
        if (ch == '\\') {
            process_t *fg = process_get_foreground();
            if (fg) process_send_signal(fg, SIGQUIT);
            push_char_unlocked(0x1C);
            return;
        }
        if (ch == ' ') { push_char_unlocked(0x00); return; }
        if (ch == '\r' || ch == '\n') { push_char_unlocked('\n'); return; }
    }

    push_char_unlocked(ch);
}

void keyboard_handle_incoming_byte(uint8_t scancode) {
    spinlock_acquire(&g_kb_lock);
    process_scancode_unlocked(scancode);
    spinlock_release(&g_kb_lock);
}

static void keyboard_irq_handler(interrupt_frame_t *frame) {
    UNUSED(frame);
    while (inb(KEYBOARD_STATUS_PORT) & 0x01) {
        uint8_t scancode = inb(KEYBOARD_DATA_PORT);
        keyboard_handle_incoming_byte(scancode);
    }
}

static void ps2_drain_buffer(void) {
    int timeout = 1000;
    while ((inb(KEYBOARD_STATUS_PORT) & 0x01) && --timeout) {
        inb(KEYBOARD_DATA_PORT);
        io_wait();
    }
}

static bool ps2_wait_write_timeout(void) {
    int timeout = 100000;
    while ((inb(KEYBOARD_STATUS_PORT) & 0x02) && --timeout) {
        io_wait();
    }
    return timeout > 0;
}

static bool ps2_wait_read_timeout(void) {
    int timeout = 100000;
    while (!(inb(KEYBOARD_STATUS_PORT) & 0x01) && --timeout) {
        io_wait();
    }
    return timeout > 0;
}

void keyboard_init(void) {
    spinlock_init(&g_kb_lock);

    /* Flush output buffer */
    ps2_drain_buffer();

    /* Disable first & second PS/2 ports during initialization */
    if (ps2_wait_write_timeout()) {
        outb(KEYBOARD_STATUS_PORT, 0xAD); /* Disable first port */
    }
    if (ps2_wait_write_timeout()) {
        outb(KEYBOARD_STATUS_PORT, 0xA7); /* Disable second port */
    }

    ps2_drain_buffer();

    /* Read controller configuration byte (Command 0x20) */
    uint8_t config = 0x47; /* Fallback safe config: IRQ1 + IRQ12 + Translation */
    if (ps2_wait_write_timeout()) {
        outb(KEYBOARD_STATUS_PORT, 0x20);
        if (ps2_wait_read_timeout()) {
            config = inb(KEYBOARD_DATA_PORT);
        }
    }

    /* Enable IRQ 1 (bit 0), enable port 1 clock (bit 4 = 0), enable translation (bit 6 = 1) */
    config |= (1 << 0);
    config &= ~(1 << 4);
    config |= (1 << 6);

    /* Write back controller configuration byte (Command 0x60) */
    if (ps2_wait_write_timeout()) {
        outb(KEYBOARD_STATUS_PORT, 0x60);
        if (ps2_wait_write_timeout()) {
            outb(KEYBOARD_DATA_PORT, config);
        }
    }

    /* Enable first PS/2 port (Command 0xAE) */
    if (ps2_wait_write_timeout()) {
        outb(KEYBOARD_STATUS_PORT, 0xAE);
    }

    /* Enable scanning on keyboard (Command 0xF4) */
    if (ps2_wait_write_timeout()) {
        outb(KEYBOARD_DATA_PORT, 0xF4);
    }

    /* Drain ACK / Self-test responses */
    ps2_drain_buffer();

    /* Register IRQ1 handler & unmask PIC interrupts */
    isr_register_handler(IRQ1, keyboard_irq_handler);
    pic_clear_mask(1); /* Unmask IRQ1 on Master PIC */
    pic_clear_mask(2); /* Unmask cascade IRQ2 */

    klog_info("PS/2 Keyboard driver initialized (Dual Scancode Set 1/Set 2 auto-decoding active)");
}

bool keyboard_has_char(void) {
    /* Check serial UART input */
    while (serial_received()) {
        char c = serial_getc();
        push_char(c);
    }

    /* Poll status port if hardware buffer has data */
    while (inb(KEYBOARD_STATUS_PORT) & 0x01) {
        uint8_t scancode = inb(KEYBOARD_DATA_PORT);
        keyboard_handle_incoming_byte(scancode);
    }

    spinlock_acquire(&g_kb_lock);
    bool has_char = (g_kb_read_ptr != g_kb_write_ptr);
    spinlock_release(&g_kb_lock);
    return has_char;
}

char keyboard_getc(void) {
    while (!keyboard_has_char()) {
        __asm__ volatile ("pause");
        io_wait();
        sched_yield();
    }

    spinlock_acquire(&g_kb_lock);
    char ch = g_kb_buffer[g_kb_read_ptr];
    g_kb_read_ptr = (g_kb_read_ptr + 1) % KEYBOARD_BUFFER_SIZE;
    spinlock_release(&g_kb_lock);

    return ch;
}
