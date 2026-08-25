#include <kernel/kprint.h>
#include <kernel/string.h>
#include <kernel/spinlock.h>
#include <drivers/serial.h>
#include <drivers/framebuffer.h>

static spinlock_t g_kprint_lock = SPINLOCK_INIT;

static const char *hex_digits = "0123456789abcdef";
static const char *hex_digits_upper = "0123456789ABCDEF";

static int format_uint(char *buf, size_t size, uint64_t val, int base, bool uppercase, int width, char pad) {
    char tmp[65];
    int pos = 0;
    const char *digits = uppercase ? hex_digits_upper : hex_digits;

    if (val == 0) {
        tmp[pos++] = '0';
    } else {
        while (val > 0) {
            tmp[pos++] = digits[val % base];
            val /= base;
        }
    }

    int written = 0;
    int pad_len = width - pos;
    if (pad_len < 0)
        pad_len = 0;

    for (int i = 0; i < pad_len && written + 1 < (int)size; i++) {
        buf[written++] = pad;
    }

    for (int i = pos - 1; i >= 0 && written + 1 < (int)size; i--) {
        buf[written++] = tmp[i];
    }

    buf[written] = '\0';
    return written;
}

static int format_int(char *buf, size_t size, int64_t val, int width, char pad) {
    int written = 0;
    if (val < 0) {
        if (written + 1 < (int)size)
            buf[written++] = '-';
        val = -val;
        if (width > 0)
            width--;
    }
    written += format_uint(buf + written, size - written, (uint64_t)val, 10, false, width, pad);
    return written;
}

int kvsnprintf(char *buf, size_t size, const char *fmt, va_list args) {
    if (!buf || size == 0)
        return 0;

    size_t out = 0;
    const char *p = fmt;

    while (*p && out + 1 < size) {
        if (*p != '%') {
            buf[out++] = *p++;
            continue;
        }

        p++; /* Skip '%' */

        /* Flags and width */
        char pad = ' ';
        int width = 0;
        bool is_long = false;
        bool is_long_long = false;

        if (*p == '0') {
            pad = '0';
            p++;
        }

        while (*p >= '0' && *p <= '9') {
            width = width * 10 + (*p - '0');
            p++;
        }

        if (*p == 'l') {
            is_long = true;
            p++;
            if (*p == 'l') {
                is_long_long = true;
                p++;
            }
        } else if (*p == 'z') {
            is_long = true;
            p++;
        }

        switch (*p) {
        case 'c': {
            char c = (char)va_arg(args, int);
            if (out + 1 < size)
                buf[out++] = c;
            break;
        }
        case 's': {
            const char *s = va_arg(args, const char *);
            if (!s)
                s = "(null)";
            while (*s && out + 1 < size) {
                buf[out++] = *s++;
            }
            break;
        }
        case 'd':
        case 'i': {
            int64_t val = (is_long || is_long_long) ? va_arg(args, int64_t) : (int64_t)va_arg(args, int);
            out += format_int(buf + out, size - out, val, width, pad);
            break;
        }
        case 'u': {
            uint64_t val = (is_long || is_long_long) ? va_arg(args, uint64_t) : (uint64_t)va_arg(args, unsigned int);
            out += format_uint(buf + out, size - out, val, 10, false, width, pad);
            break;
        }
        case 'x': {
            uint64_t val = (is_long || is_long_long) ? va_arg(args, uint64_t) : (uint64_t)va_arg(args, unsigned int);
            out += format_uint(buf + out, size - out, val, 16, false, width, pad);
            break;
        }
        case 'X': {
            uint64_t val = (is_long || is_long_long) ? va_arg(args, uint64_t) : (uint64_t)va_arg(args, unsigned int);
            out += format_uint(buf + out, size - out, val, 16, true, width, pad);
            break;
        }
        case 'p': {
            uintptr_t ptr = (uintptr_t)va_arg(args, void *);
            if (out + 2 < size) {
                buf[out++] = '0';
                buf[out++] = 'x';
            }
            out += format_uint(buf + out, size - out, ptr, 16, false, sizeof(uintptr_t) * 2, '0');
            break;
        }
        case '%': {
            if (out + 1 < size)
                buf[out++] = '%';
            break;
        }
        default: {
            if (out + 1 < size)
                buf[out++] = *p;
            break;
        }
        }
        p++;
    }

    buf[out] = '\0';
    return (int)out;
}

int ksnprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int len = kvsnprintf(buf, size, fmt, args);
    va_end(args);
    return len;
}

int kvprintf(const char *fmt, va_list args) {
    char buf[1024];
    int len = kvsnprintf(buf, sizeof(buf), fmt, args);

    spinlock_acquire(&g_kprint_lock);
    serial_write(buf, len);
    fb_console_write(buf, len);
    spinlock_release(&g_kprint_lock);

    return len;
}

int kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int len = kvprintf(fmt, args);
    va_end(args);
    return len;
}

static char g_klog_ring[65536];
static size_t g_klog_len = 0;

void klog(log_level_t level, const char *fmt, ...) {
    const char *tag = "";
    uint32_t fg_color = FB_COLOR_WHITE;

    switch (level) {
    case LOG_LEVEL_DEBUG:
        tag = "[DEBUG] ";
        fg_color = FB_COLOR_GRAY;
        break;
    case LOG_LEVEL_INFO:
        tag = "[INFO]  ";
        fg_color = FB_COLOR_CYAN;
        break;
    case LOG_LEVEL_WARN:
        tag = "[WARN]  ";
        fg_color = FB_COLOR_YELLOW;
        break;
    case LOG_LEVEL_ERROR:
        tag = "[ERROR] ";
        fg_color = FB_COLOR_RED;
        break;
    case LOG_LEVEL_PANIC:
        tag = "[PANIC] ";
        fg_color = FB_COLOR_MAGENTA;
        break;
    }

    char msg_buf[1024];
    va_list args;
    va_start(args, fmt);
    kvsnprintf(msg_buf, sizeof(msg_buf), fmt, args);
    va_end(args);

    spinlock_acquire(&g_kprint_lock);

    /* Store in kernel log ring buffer */
    const char *parts[] = {tag, msg_buf, "\n"};
    for (int p = 0; p < 3; p++) {
        const char *s = parts[p];
        while (*s) {
            if (g_klog_len < sizeof(g_klog_ring)) {
                g_klog_ring[g_klog_len++] = *s;
            } else {
                /* Shift left by 1024 bytes to make room */
                memmove(g_klog_ring, g_klog_ring + 1024, sizeof(g_klog_ring) - 1024);
                g_klog_len = sizeof(g_klog_ring) - 1024;
                g_klog_ring[g_klog_len++] = *s;
            }
            s++;
        }
    }

    /* Serial output */
    serial_puts(tag);
    serial_puts(msg_buf);
    serial_putc('\n');

    /* Framebuffer output with colors */
    uint32_t orig_fg = FB_COLOR_WHITE;
    fb_console_set_color(fg_color, FB_COLOR_BG);
    fb_console_puts(tag);
    fb_console_set_color(orig_fg, FB_COLOR_BG);
    fb_console_puts(msg_buf);
    fb_console_putc('\n');

    spinlock_release(&g_kprint_lock);
}

size_t klog_read_ring(char *dst, size_t max_len, size_t offset) {
    if (!dst || max_len == 0)
        return 0;
    spinlock_acquire(&g_kprint_lock);
    if (offset >= g_klog_len) {
        spinlock_release(&g_kprint_lock);
        return 0;
    }
    size_t avail = g_klog_len - offset;
    size_t to_copy = (avail < max_len) ? avail : max_len;
    memcpy(dst, g_klog_ring + offset, to_copy);
    spinlock_release(&g_kprint_lock);
    return to_copy;
}

size_t klog_get_ring_size(void) {
    spinlock_acquire(&g_kprint_lock);
    size_t sz = g_klog_len;
    spinlock_release(&g_kprint_lock);
    return sz;
}

void kprint_init(void) {
    spinlock_init(&g_kprint_lock);
    g_klog_len = 0;
}
