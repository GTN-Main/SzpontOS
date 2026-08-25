/*
 * SzpontOS - USB HID (Human Interface Device) Keyboard Driver
 * Complete HID Usage Page 0x07 (Keyboard/Keypad) Implementation with Typematic Repeat.
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <drivers/hid.h>
#include <drivers/keyboard.h>
#include <sched/process.h>
#include <kernel/signal.h>
#include <kernel/kprint.h>
#include <kernel/string.h>

/* State tracking */
static uint8_t g_prev_keys[6] = {0};
static bool g_hid_caps_lock = false;
static bool g_hid_num_lock = true;

/* Typematic repeat tracking */
static uint8_t g_repeat_key = 0;
static uint8_t g_repeat_modifiers = 0;
static uint32_t g_repeat_counter = 0;
#define HID_REPEAT_DELAY 25   /* ~250ms initial delay (at 10ms report rate) */
#define HID_REPEAT_RATE 4     /* ~40ms repeat interval */

/* HID Usage Page 0x07 (Keyboard/Keypad) to ASCII mapping (unshifted) */
static const char g_hid_to_ascii_low[256] = {
    /* 0x00 - 0x03: Reserved */
    0, 0, 0, 0,
    /* 0x04 - 0x1D: 'a' - 'z' */
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
    'x', 'y', 'z',
    /* 0x1E - 0x27: '1' - '0' */
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
    /* 0x28 - 0x38: Control & Punctuation */
    '\r', 27, '\b', '\t', ' ', '-', '=', '[', ']', '\\', 0, ';', '\'', '`', ',', '.', '/',
    /* 0x39: Caps Lock */
    0,
    /* 0x3A - 0x45: F1 - F12 (Handled via ANSI escape sequences) */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 0x46 - 0x4E: PrintScreen, ScrollLock, Pause, Insert, Home, PgUp, Delete, End, PgDn */
    0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 0x4F - 0x52: Right, Left, Down, Up (Handled via ANSI escape sequences) */
    0, 0, 0, 0,
    /* 0x53: Keypad Num Lock */
    0,
    /* 0x54 - 0x63: Keypad '/', '*', '-', '+', Enter, 1-9, 0, '.' */
    '/', '*', '-', '+', '\r', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '.'};

/* HID Usage Page 0x07 (Keyboard/Keypad) to ASCII mapping (shifted) */
static const char g_hid_to_ascii_shift[256] = {
    /* 0x00 - 0x03: Reserved */
    0, 0, 0, 0,
    /* 0x04 - 0x1D: 'A' - 'Z' */
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
    'X', 'Y', 'Z',
    /* 0x1E - 0x27: '!' - ')' */
    '!', '@', '#', '$', '%', '^', '&', '*', '(', ')',
    /* 0x28 - 0x38: Control & Punctuation */
    '\r', 27, '\b', '\t', ' ', '_', '+', '{', '}', '|', 0, ':', '"', '~', '<', '>', '?',
    /* 0x39: Caps Lock */
    0,
    /* 0x3A - 0x45: F1 - F12 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 0x46 - 0x4E: Navigation / Editing */
    0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 0x4F - 0x52: Arrow keys */
    0, 0, 0, 0,
    /* 0x53: Keypad Num Lock */
    0,
    /* 0x54 - 0x63: Keypad */
    '/', '*', '-', '+', '\r', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '.'};

static bool key_was_pressed(uint8_t key) {
    for (int i = 0; i < 6; i++) {
        if (g_prev_keys[i] == key)
            return true;
    }
    return false;
}

static void emit_hid_key(uint8_t key, uint8_t modifiers) {
    bool shift = (modifiers & (HID_MOD_LSHIFT | HID_MOD_RSHIFT)) != 0;
    bool ctrl = (modifiers & (HID_MOD_LCTRL | HID_MOD_RCTRL)) != 0;

    /* 1. Caps Lock Toggle (0x39) */
    if (key == 0x39) {
        g_hid_caps_lock = !g_hid_caps_lock;
        return;
    }

    /* 2. Num Lock Toggle (0x53) */
    if (key == 0x53) {
        g_hid_num_lock = !g_hid_num_lock;
        return;
    }

    /* 3. Function Keys F1 - F12 (0x3A - 0x45) */
    switch (key) {
    case 0x3A: keyboard_push_str("\033OP"); return; /* F1 */
    case 0x3B: keyboard_push_str("\033OQ"); return; /* F2 */
    case 0x3C: keyboard_push_str("\033OR"); return; /* F3 */
    case 0x3D: keyboard_push_str("\033OS"); return; /* F4 */
    case 0x3E: keyboard_push_str("\033[15~"); return; /* F5 */
    case 0x3F: keyboard_push_str("\033[17~"); return; /* F6 */
    case 0x40: keyboard_push_str("\033[18~"); return; /* F7 */
    case 0x41: keyboard_push_str("\033[19~"); return; /* F8 */
    case 0x42: keyboard_push_str("\033[20~"); return; /* F9 */
    case 0x43: keyboard_push_str("\033[21~"); return; /* F10 */
    case 0x44: keyboard_push_str("\033[23~"); return; /* F11 */
    case 0x45: keyboard_push_str("\033[24~"); return; /* F12 */
    default: break;
    }

    /* 4. Navigation and Editing Keys */
    switch (key) {
    case 0x49: keyboard_push_str("\033[2~"); return; /* Insert */
    case 0x4A: keyboard_push_str("\033[H"); return;  /* Home */
    case 0x4B: keyboard_push_str("\033[5~"); return; /* Page Up */
    case 0x4C: keyboard_push_str("\033[3~"); return; /* Delete */
    case 0x4D: keyboard_push_str("\033[F"); return;  /* End */
    case 0x4E: keyboard_push_str("\033[6~"); return; /* Page Down */
    case 0x4F: keyboard_push_str("\033[C"); return;  /* Right Arrow */
    case 0x50: keyboard_push_str("\033[D"); return;  /* Left Arrow */
    case 0x51: keyboard_push_str("\033[B"); return;  /* Down Arrow */
    case 0x52: keyboard_push_str("\033[A"); return;  /* Up Arrow */
    default: break;
    }

    /* 5. Keypad Navigation when NumLock is OFF */
    if (!g_hid_num_lock) {
        switch (key) {
        case 0x59: keyboard_push_str("\033[F"); return;  /* Keypad 1 -> End */
        case 0x5A: keyboard_push_str("\033[B"); return;  /* Keypad 2 -> Down */
        case 0x5B: keyboard_push_str("\033[6~"); return; /* Keypad 3 -> PgDn */
        case 0x5C: keyboard_push_str("\033[D"); return;  /* Keypad 4 -> Left */
        case 0x5E: keyboard_push_str("\033[C"); return;  /* Keypad 6 -> Right */
        case 0x5F: keyboard_push_str("\033[H"); return;  /* Keypad 7 -> Home */
        case 0x60: keyboard_push_str("\033[A"); return;  /* Keypad 8 -> Up */
        case 0x61: keyboard_push_str("\033[5~"); return; /* Keypad 9 -> PgUp */
        case 0x62: keyboard_push_str("\033[2~"); return; /* Keypad 0 -> Insert */
        case 0x63: keyboard_push_char(127); return;      /* Keypad . -> Delete */
        default: break;
        }
    }

    /* 6. Standard Character Translation */
    char ch = shift ? g_hid_to_ascii_shift[key] : g_hid_to_ascii_low[key];

    if (g_hid_caps_lock && ch >= 'a' && ch <= 'z') {
        ch -= 32;
    } else if (g_hid_caps_lock && ch >= 'A' && ch <= 'Z' && shift) {
        ch += 32;
    }

    if (ch == 0)
        return;

    /* 7. Unix Control Keys & TTY Signals */
    if (ctrl) {
        char lower = (ch >= 'A' && ch <= 'Z') ? (char)(ch + 32) : ch;
        if (lower >= 'a' && lower <= 'z') {
            char ctrl_char = (char)(lower - 'a' + 1);

            if (ctrl_char == 0x03) { /* Ctrl+C -> SIGINT */
                process_t *fg = process_get_foreground();
                if (fg)
                    process_send_signal(fg, SIGINT);
                keyboard_push_char(0x03);
                return;
            } else if (ctrl_char == 0x1A) { /* Ctrl+Z -> SIGTSTP */
                process_t *fg = process_get_foreground();
                if (fg)
                    process_send_signal(fg, SIGTSTP);
                keyboard_push_char(0x1A);
                return;
            } else if (ctrl_char == 0x1C) { /* Ctrl+\ -> SIGQUIT */
                process_t *fg = process_get_foreground();
                if (fg)
                    process_send_signal(fg, SIGQUIT);
                keyboard_push_char(0x1C);
                return;
            } else if (ctrl_char == 0x04) { /* Ctrl+D -> EOF */
                keyboard_push_char(0x04);
                return;
            }

            keyboard_push_char(ctrl_char);
            return;
        }

        if (ch == '[') {
            keyboard_push_char(0x1B);
            return;
        }
        if (ch == ']') {
            keyboard_push_char(0x1D);
            return;
        }
        if (ch == '\\') {
            process_t *fg = process_get_foreground();
            if (fg)
                process_send_signal(fg, SIGQUIT);
            keyboard_push_char(0x1C);
            return;
        }
        if (ch == ' ') {
            keyboard_push_char(0x00);
            return;
        }
        if (ch == '\r' || ch == '\n') {
            keyboard_push_char('\r');
            return;
        }
    }

    keyboard_push_char(ch);
}

void hid_process_keyboard_report(const uint8_t report[8]) {
    uint8_t modifiers = report[0];
    uint8_t active_key = 0;

    /* Process newly pressed keys */
    for (int i = 2; i < 8; i++) {
        uint8_t key = report[i];
        if (key == 0 || key == 1)
            continue; /* No key or ErrorRollOver */

        active_key = key;

        /* Only process on leading edge (new press) */
        if (!key_was_pressed(key)) {
            emit_hid_key(key, modifiers);
            g_repeat_key = key;
            g_repeat_modifiers = modifiers;
            g_repeat_counter = 0;
        }
    }

    /* Typematic auto-repeat handling for held key */
    if (active_key != 0 && active_key == g_repeat_key) {
        g_repeat_counter++;
        if (g_repeat_counter >= HID_REPEAT_DELAY) {
            if ((g_repeat_counter - HID_REPEAT_DELAY) % HID_REPEAT_RATE == 0) {
                emit_hid_key(g_repeat_key, g_repeat_modifiers);
            }
        }
    } else if (active_key == 0) {
        g_repeat_key = 0;
        g_repeat_counter = 0;
    }

    /* Save current active keys for rollover diff */
    for (int i = 0; i < 6; i++) {
        g_prev_keys[i] = report[i + 2];
    }
}
