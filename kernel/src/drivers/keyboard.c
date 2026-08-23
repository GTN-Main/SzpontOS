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
#define KEYBOARD_BUFFER_SIZE  256

static char g_kb_buffer[KEYBOARD_BUFFER_SIZE];
static size_t g_kb_read_ptr = 0;
static size_t g_kb_write_ptr = 0;
static spinlock_t g_kb_lock = SPINLOCK_INIT;

static bool g_shift = false;
static bool g_caps_lock = false;
static bool g_ctrl = false;
static bool g_alt = false;
static bool g_extended = false;

static const char g_scancode_ascii_low[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\r',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

static const char g_scancode_ascii_shift[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\r',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

static void push_char(char ch) {
    spinlock_acquire(&g_kb_lock);
    size_t next_write = (g_kb_write_ptr + 1) % KEYBOARD_BUFFER_SIZE;
    if (next_write != g_kb_read_ptr) {
        g_kb_buffer[g_kb_write_ptr] = ch;
        g_kb_write_ptr = next_write;
    }
    spinlock_release(&g_kb_lock);
}

static void push_str(const char *str) {
    if (!str) return;
    while (*str) {
        push_char(*str++);
    }
}

static void process_scancode(uint8_t scancode) {
    /* Extended scancode prefix */
    if (scancode == 0xE0) {
        g_extended = true;
        return;
    }

    /* Key release */
    if (scancode & 0x80) {
        uint8_t released = scancode & 0x7F;
        if (released == 0x2A || released == 0x36) {
            g_shift = false;
        } else if (released == 0x1D) {
            g_ctrl = false;
        } else if (released == 0x38) {
            g_alt = false;
        }
        g_extended = false;
        return;
    }

    /* Handle Extended Keys (Arrow keys, Home, End, Delete, Keypad Enter) */
    if (g_extended) {
        g_extended = false;
        switch (scancode) {
            case 0x48: push_str("\033[A"); return; /* Up Arrow */
            case 0x50: push_str("\033[B"); return; /* Down Arrow */
            case 0x4D: push_str("\033[C"); return; /* Right Arrow */
            case 0x4B: push_str("\033[D"); return; /* Left Arrow */
            case 0x47: push_str("\033[H"); return; /* Home */
            case 0x4F: push_str("\033[F"); return; /* End */
            case 0x53: push_str("\033[3~"); return; /* Delete */
            case 0x49: push_str("\033[5~"); return; /* Page Up */
            case 0x51: push_str("\033[6~"); return; /* Page Down */
            case 0x1C: push_char('\r'); return;    /* Keypad Enter */
            case 0x1D: g_ctrl = true; return;      /* Right Ctrl */
            case 0x38: g_alt = true; return;       /* Right Alt */
            default: break;
        }
    }

    /* Modifiers */
    if (scancode == 0x2A || scancode == 0x36) {
        g_shift = true;
        return;
    } else if (scancode == 0x1D) {
        g_ctrl = true;
        return;
    } else if (scancode == 0x38) {
        g_alt = true;
        return;
    } else if (scancode == 0x3A) {
        g_caps_lock = !g_caps_lock;
        return;
    }

    /* Handle Control Key Combinations (Universal A-Z mapping & Signals) */
    if (g_ctrl) {
        char low = 0;
        if (scancode < 128) {
            low = g_scancode_ascii_low[scancode];
        }
        if (low >= 'a' && low <= 'z') {
            char ctrl_char = (char)(low - 'a' + 1);
            if (ctrl_char == 0x03) { /* Ctrl+C -> SIGINT */
                process_t *fg = process_get_foreground();
                if (fg) {
                    process_send_signal(fg, SIGINT);
                }
                push_char(0x03);
                return;
            } else if (ctrl_char == 0x1A) { /* Ctrl+Z -> SIGTSTP */
                process_t *fg = process_get_foreground();
                if (fg) {
                    process_send_signal(fg, SIGTSTP);
                }
                push_char(0x1A);
                return;
            } else if (ctrl_char == 0x1C) { /* Ctrl+\ -> SIGQUIT */
                process_t *fg = process_get_foreground();
                if (fg) {
                    process_send_signal(fg, SIGQUIT);
                }
                push_char(0x1C);
                return;
            }
            push_char(ctrl_char);
            return;
        }
        if (low == '[') { push_char(0x1B); return; /* ESC */ }
        if (low == ']') { push_char(0x1D); return; }
        if (low == '\\') {
            process_t *fg = process_get_foreground();
            if (fg) process_send_signal(fg, SIGQUIT);
            push_char(0x1C);
            return;
        }
        if (low == ' ') { push_char(0x00); return; /* NUL */ }
        if (low == '\r' || low == '\n') { push_char('\n'); return; }
    }

    /* Standard character mapping */
    char ch = 0;
    if (scancode < 128) {
        if (g_shift) {
            ch = g_scancode_ascii_shift[scancode];
        } else {
            ch = g_scancode_ascii_low[scancode];
            if (g_caps_lock && ch >= 'a' && ch <= 'z') {
                ch -= 32;
            }
        }
    }

    if (ch != 0) {
        push_char(ch);
    }
}

static void keyboard_irq_handler(interrupt_frame_t *frame) {
    UNUSED(frame);
    while (inb(KEYBOARD_STATUS_PORT) & 0x01) {
        uint8_t scancode = inb(KEYBOARD_DATA_PORT);
        process_scancode(scancode);
    }
}

static void ps2_wait_write(void) {
    int timeout = 100000;
    while ((inb(KEYBOARD_STATUS_PORT) & 0x02) && --timeout) {
        io_wait();
    }
}

static void ps2_wait_read(void) {
    int timeout = 100000;
    while (!(inb(KEYBOARD_STATUS_PORT) & 0x01) && --timeout) {
        io_wait();
    }
}

void keyboard_init(void) {
    spinlock_init(&g_kb_lock);

    /* Flush output buffer */
    while (inb(KEYBOARD_STATUS_PORT) & 0x01) {
        inb(KEYBOARD_DATA_PORT);
    }

    /* Enable first PS/2 port */
    ps2_wait_write();
    outb(KEYBOARD_STATUS_PORT, 0xAE);

    /* Read controller configuration byte (Command 0x20) */
    ps2_wait_write();
    outb(KEYBOARD_STATUS_PORT, 0x20);
    ps2_wait_read();
    uint8_t config = inb(KEYBOARD_DATA_PORT);

    /* Enable IRQ 1 (bit 0), enable port 1 clock (bit 4 = 0), enable translation (bit 6 = 1) */
    config |= (1 << 0);
    config &= ~(1 << 4);
    config |= (1 << 6);

    /* Write back controller configuration byte (Command 0x60) */
    ps2_wait_write();
    outb(KEYBOARD_STATUS_PORT, 0x60);
    ps2_wait_write();
    outb(KEYBOARD_DATA_PORT, config);

    /* Enable scanning on keyboard (Command 0xF4 to data port) */
    ps2_wait_write();
    outb(KEYBOARD_DATA_PORT, 0xF4);

    /* Read ACK (0xFA) if available */
    int timeout = 10000;
    while (timeout--) {
        if (inb(KEYBOARD_STATUS_PORT) & 0x01) {
            inb(KEYBOARD_DATA_PORT);
            break;
        }
        io_wait();
    }

    isr_register_handler(IRQ1, keyboard_irq_handler);
    pic_clear_mask(1); /* Unmask IRQ1 on PIC */
    pic_clear_mask(2); /* Unmask cascade IRQ2 */
    klog_info("PS/2 Keyboard driver initialized (Ctrl/Signals/Arrows enabled)");
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
        process_scancode(scancode);
    }
    return g_kb_read_ptr != g_kb_write_ptr;
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
