/*
 * SzpontOS — szponterm Native X11 Terminal Emulator
 * (C) Copyright by Szpont Industries. All rights reserved.
 *
 * Fast, lightweight, 256-color & TrueColor ANSI/VT100 terminal emulator
 * with dynamic interactive window resizing, mouse drag grip, and SIGWINCH support.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pty.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

#define MAX_COLS     256
#define MAX_ROWS     128
#define MIN_COLS     24
#define MIN_ROWS     6

#define DEFAULT_COLS 80
#define DEFAULT_ROWS 25
#define CHAR_W       8
#define CHAR_H       16
#define PAD_X        10
#define PAD_Y        8

/* Parser States */
enum {
    STATE_NORMAL = 0,
    STATE_ESC,
    STATE_CSI,
    STATE_OSC
};

/* Cell Attributes */
#define ATTR_BOLD      (1 << 0)
#define ATTR_UNDERLINE (1 << 1)
#define ATTR_REVERSE   (1 << 2)

/* Modern Cyber Slate Palette */
#define COLOR_BG          0xFF0F172A /* Dark Navy Slate */
#define COLOR_FG          0xFFF8FAFC /* Soft Crisp White */
#define COLOR_CURSOR      0xFF38BDF8 /* Neon Cyan Accent */
#define COLOR_CURSOR_TEXT 0xFF0F172A

static const uint32_t g_ansi_palette[16] = {
    0xFF181825, /* 0: Black */
    0xFFF87171, /* 1: Red */
    0xFF4ADE80, /* 2: Green */
    0xFFFBBF24, /* 3: Yellow */
    0xFF60A5FA, /* 4: Blue */
    0xFFF472B6, /* 5: Magenta */
    0xFF38BDF8, /* 6: Cyan */
    0xFFE2E8F0, /* 7: White */

    0xFF64748B, /* 8: Bright Black / Gray */
    0xFFEF4444, /* 9: Bright Red */
    0xFF22C55E, /* 10: Bright Green */
    0xFFF59E0B, /* 11: Bright Yellow */
    0xFF3B82F6, /* 12: Bright Blue */
    0xFFEC4899, /* 13: Bright Magenta */
    0xFF06B6D4, /* 14: Bright Cyan */
    0xFFFFFFFF  /* 15: Bright White */
};

typedef struct {
    char     ch;
    uint32_t fg;
    uint32_t bg;
    uint8_t  flags;
} cell_t;

typedef struct {
    cell_t   grid[MAX_ROWS][MAX_COLS];
    int      cols;
    int      rows;
    int      cursor_x;
    int      cursor_y;
    int      saved_x;
    int      saved_y;
    uint32_t cur_fg;
    uint32_t cur_bg;
    uint8_t  cur_flags;
    bool     cursor_visible;

    int      parser_state;
    int      csi_args[16];
    int      csi_argc;
    bool     csi_private;
} term_t;

static Display  *g_dpy = NULL;
static Window    g_win;
static GC        g_gc;
static Pixmap    g_backbuffer = 0;
static Atom      g_wm_delete;
static int       g_master_fd = -1;
static pid_t     g_child_pid = -1;
static term_t    g_term;
static bool      g_running = true;
static bool      g_needs_redraw = true;
static bool      g_has_focus = true;

/* Dynamic Dimensions & Resizing State */
static int       g_win_w = DEFAULT_COLS * CHAR_W + PAD_X * 2;
static int       g_win_h = DEFAULT_ROWS * CHAR_H + PAD_Y * 2;
static bool      g_is_resizing = false;
static int       g_resize_start_mx = 0;
static int       g_resize_start_my = 0;
static int       g_orig_w = 0;
static int       g_orig_h = 0;
static bool      g_is_maximized = false;
static int       g_saved_premax_w = 0;
static int       g_saved_premax_h = 0;

static void term_reset_attributes(void) {
    g_term.cur_fg = COLOR_FG;
    g_term.cur_bg = COLOR_BG;
    g_term.cur_flags = 0;
}

static void term_clear_line(int row, int start_col, int end_col) {
    if (row < 0 || row >= g_term.rows || row >= MAX_ROWS) return;
    if (start_col < 0) start_col = 0;
    if (end_col > g_term.cols) end_col = g_term.cols;
    if (end_col > MAX_COLS) end_col = MAX_COLS;

    for (int c = start_col; c < end_col; c++) {
        g_term.grid[row][c].ch = ' ';
        g_term.grid[row][c].fg = g_term.cur_fg;
        g_term.grid[row][c].bg = g_term.cur_bg;
        g_term.grid[row][c].flags = 0;
    }
}

static void term_clear_all(void) {
    for (int r = 0; r < g_term.rows; r++) {
        term_clear_line(r, 0, g_term.cols);
    }
}

static void term_scroll_up(void) {
    if (g_term.rows <= 1) return;
    memmove(&g_term.grid[0][0], &g_term.grid[1][0], sizeof(cell_t) * MAX_COLS * (g_term.rows - 1));
    term_clear_line(g_term.rows - 1, 0, g_term.cols);
}

static void term_init(void) {
    memset(&g_term, 0, sizeof(term_t));
    g_term.cols = DEFAULT_COLS;
    g_term.rows = DEFAULT_ROWS;
    g_term.cursor_x = 0;
    g_term.cursor_y = 0;
    g_term.cursor_visible = true;
    term_reset_attributes();
    term_clear_all();
}

static void term_resize(int new_w, int new_h) {
    if (new_w < MIN_COLS * CHAR_W + PAD_X * 2) new_w = MIN_COLS * CHAR_W + PAD_X * 2;
    if (new_h < MIN_ROWS * CHAR_H + PAD_Y * 2) new_h = MIN_ROWS * CHAR_H + PAD_Y * 2;

    int new_cols = (new_w - PAD_X * 2) / CHAR_W;
    int new_rows = (new_h - PAD_Y * 2) / CHAR_H;
    if (new_cols > MAX_COLS) new_cols = MAX_COLS;
    if (new_rows > MAX_ROWS) new_rows = MAX_ROWS;

    g_win_w = new_w;
    g_win_h = new_h;
    int old_rows = g_term.rows;
    int old_cols = g_term.cols;
    g_term.cols = new_cols;
    g_term.rows = new_rows;

    /* Clear any newly exposed grid area */
    if (new_rows > old_rows || new_cols > old_cols) {
        for (int r = 0; r < new_rows; r++) {
            for (int c = 0; c < new_cols; c++) {
                if (r >= old_rows || c >= old_cols) {
                    g_term.grid[r][c].ch = ' ';
                    g_term.grid[r][c].fg = COLOR_FG;
                    g_term.grid[r][c].bg = COLOR_BG;
                    g_term.grid[r][c].flags = 0;
                }
            }
        }
    }

    if (g_term.cursor_x >= g_term.cols) g_term.cursor_x = g_term.cols - 1;
    if (g_term.cursor_y >= g_term.rows) g_term.cursor_y = g_term.rows - 1;
    if (g_term.cursor_x < 0) g_term.cursor_x = 0;
    if (g_term.cursor_y < 0) g_term.cursor_y = 0;

    /* Reallocate Backbuffer */
    if (g_dpy && g_win) {
        if (g_backbuffer) {
            XFreePixmap(g_dpy, g_backbuffer);
            g_backbuffer = 0;
        }
        int screen = DefaultScreen(g_dpy);
        g_backbuffer = XCreatePixmap(g_dpy, g_win, (unsigned int)g_win_w, (unsigned int)g_win_h, DefaultDepth(g_dpy, screen));
    }

    /* Update PTY Window Size and notify Shell via SIGWINCH */
    if (g_master_fd >= 0) {
        struct winsize ws;
        ws.ws_col = (unsigned short)g_term.cols;
        ws.ws_row = (unsigned short)g_term.rows;
        ws.ws_xpixel = (unsigned short)g_win_w;
        ws.ws_ypixel = (unsigned short)g_win_h;
        ioctl(g_master_fd, TIOCSWINSZ, &ws);
    }
    if (g_child_pid > 0) {
        kill(g_child_pid, SIGWINCH);
    }

    g_needs_redraw = true;
}

static void term_handle_sgr(void) {
    if (g_term.csi_argc == 0) {
        term_reset_attributes();
        return;
    }

    for (int i = 0; i < g_term.csi_argc; i++) {
        int code = g_term.csi_args[i];
        if (code == 0) {
            term_reset_attributes();
        } else if (code == 1) {
            g_term.cur_flags |= ATTR_BOLD;
        } else if (code == 4) {
            g_term.cur_flags |= ATTR_UNDERLINE;
        } else if (code == 7) {
            g_term.cur_flags |= ATTR_REVERSE;
        } else if (code == 22) {
            g_term.cur_flags &= ~ATTR_BOLD;
        } else if (code == 24) {
            g_term.cur_flags &= ~ATTR_UNDERLINE;
        } else if (code == 27) {
            g_term.cur_flags &= ~ATTR_REVERSE;
        } else if (code >= 30 && code <= 37) {
            g_term.cur_fg = g_ansi_palette[code - 30];
        } else if (code == 39) {
            g_term.cur_fg = COLOR_FG;
        } else if (code >= 40 && code <= 47) {
            g_term.cur_bg = g_ansi_palette[code - 40];
        } else if (code == 49) {
            g_term.cur_bg = COLOR_BG;
        } else if (code >= 90 && code <= 97) {
            g_term.cur_fg = g_ansi_palette[code - 90 + 8];
        } else if (code >= 100 && code <= 107) {
            g_term.cur_bg = g_ansi_palette[code - 100 + 8];
        } else if (code == 38 && i + 2 < g_term.csi_argc && g_term.csi_args[i + 1] == 5) {
            /* 256-color FG: 38;5;idx */
            int idx = g_term.csi_args[i + 2];
            if (idx >= 0 && idx < 16) {
                g_term.cur_fg = g_ansi_palette[idx];
            } else {
                g_term.cur_fg = COLOR_FG;
            }
            i += 2;
        } else if (code == 48 && i + 2 < g_term.csi_argc && g_term.csi_args[i + 1] == 5) {
            /* 256-color BG: 48;5;idx */
            int idx = g_term.csi_args[i + 2];
            if (idx >= 0 && idx < 16) {
                g_term.cur_bg = g_ansi_palette[idx];
            } else {
                g_term.cur_bg = COLOR_BG;
            }
            i += 2;
        }
    }
}

static void term_handle_csi(char final_char) {
    int arg1 = (g_term.csi_argc > 0 && g_term.csi_args[0] > 0) ? g_term.csi_args[0] : 1;
    int arg2 = (g_term.csi_argc > 1 && g_term.csi_args[1] > 0) ? g_term.csi_args[1] : 1;

    switch (final_char) {
    case 'A': /* Cursor Up */
        g_term.cursor_y -= arg1;
        if (g_term.cursor_y < 0) g_term.cursor_y = 0;
        break;
    case 'B': /* Cursor Down */
        g_term.cursor_y += arg1;
        if (g_term.cursor_y >= g_term.rows) g_term.cursor_y = g_term.rows - 1;
        break;
    case 'C': /* Cursor Forward */
        g_term.cursor_x += arg1;
        if (g_term.cursor_x >= g_term.cols) g_term.cursor_x = g_term.cols - 1;
        break;
    case 'D': /* Cursor Backward */
        g_term.cursor_x -= arg1;
        if (g_term.cursor_x < 0) g_term.cursor_x = 0;
        break;
    case 'H': /* Cursor Position */
    case 'f':
        g_term.cursor_y = arg1 - 1;
        g_term.cursor_x = arg2 - 1;
        if (g_term.cursor_y < 0) g_term.cursor_y = 0;
        if (g_term.cursor_y >= g_term.rows) g_term.cursor_y = g_term.rows - 1;
        if (g_term.cursor_x < 0) g_term.cursor_x = 0;
        if (g_term.cursor_x >= g_term.cols) g_term.cursor_x = g_term.cols - 1;
        break;
    case 'J': /* Erase in Display */
        if (g_term.csi_argc == 0 || g_term.csi_args[0] == 0) {
            term_clear_line(g_term.cursor_y, g_term.cursor_x, g_term.cols);
            for (int r = g_term.cursor_y + 1; r < g_term.rows; r++) {
                term_clear_line(r, 0, g_term.cols);
            }
        } else if (g_term.csi_args[0] == 1) {
            for (int r = 0; r < g_term.cursor_y; r++) {
                term_clear_line(r, 0, g_term.cols);
            }
            term_clear_line(g_term.cursor_y, 0, g_term.cursor_x + 1);
        } else if (g_term.csi_args[0] == 2 || g_term.csi_args[0] == 3) {
            term_clear_all();
            g_term.cursor_x = 0;
            g_term.cursor_y = 0;
        }
        break;
    case 'K': /* Erase in Line */
        if (g_term.csi_argc == 0 || g_term.csi_args[0] == 0) {
            term_clear_line(g_term.cursor_y, g_term.cursor_x, g_term.cols);
        } else if (g_term.csi_args[0] == 1) {
            term_clear_line(g_term.cursor_y, 0, g_term.cursor_x + 1);
        } else if (g_term.csi_args[0] == 2) {
            term_clear_line(g_term.cursor_y, 0, g_term.cols);
        }
        break;
    case 'm': /* SGR Select Graphic Rendition */
        term_handle_sgr();
        break;
    case 's': /* Save Cursor */
        g_term.saved_x = g_term.cursor_x;
        g_term.saved_y = g_term.cursor_y;
        break;
    case 'u': /* Restore Cursor */
        g_term.cursor_x = g_term.saved_x;
        g_term.cursor_y = g_term.saved_y;
        break;
    case 'h':
        if (g_term.csi_private && arg1 == 25) {
            g_term.cursor_visible = true;
        }
        break;
    case 'l':
        if (g_term.csi_private && arg1 == 25) {
            g_term.cursor_visible = false;
        }
        break;
    default:
        break;
    }
}

static void term_put_char(char c) {
    if (g_term.parser_state == STATE_NORMAL) {
        if (c == '\033') {
            g_term.parser_state = STATE_ESC;
        } else if (c == '\r') {
            g_term.cursor_x = 0;
        } else if (c == '\n') {
            g_term.cursor_x = 0;
            g_term.cursor_y++;
            if (g_term.cursor_y >= g_term.rows) {
                g_term.cursor_y = g_term.rows - 1;
                term_scroll_up();
            }
        } else if (c == '\b') {
            if (g_term.cursor_x > 0) {
                g_term.cursor_x--;
            }
        } else if (c == '\t') {
            int next_tab = (g_term.cursor_x + 8) & ~7;
            if (next_tab >= g_term.cols) next_tab = g_term.cols - 1;
            while (g_term.cursor_x < next_tab) {
                term_put_char(' ');
            }
        } else if (c == '\a') {
            /* Bell */
        } else if ((uint8_t)c >= 32) {
            if (g_term.cursor_x >= g_term.cols) {
                g_term.cursor_x = 0;
                g_term.cursor_y++;
                if (g_term.cursor_y >= g_term.rows) {
                    g_term.cursor_y = g_term.rows - 1;
                    term_scroll_up();
                }
            }

            if (g_term.cursor_y >= 0 && g_term.cursor_y < g_term.rows &&
                g_term.cursor_x >= 0 && g_term.cursor_x < g_term.cols) {
                cell_t *cell = &g_term.grid[g_term.cursor_y][g_term.cursor_x];
                cell->ch = c;
                cell->fg = g_term.cur_fg;
                cell->bg = g_term.cur_bg;
                cell->flags = g_term.cur_flags;
                g_term.cursor_x++;
            }
        }
    } else if (g_term.parser_state == STATE_ESC) {
        if (c == '[') {
            g_term.parser_state = STATE_CSI;
            g_term.csi_argc = 0;
            memset(g_term.csi_args, 0, sizeof(g_term.csi_args));
            g_term.csi_private = false;
        } else if (c == ']') {
            g_term.parser_state = STATE_OSC;
        } else if (c == '7') {
            g_term.saved_x = g_term.cursor_x;
            g_term.saved_y = g_term.cursor_y;
            g_term.parser_state = STATE_NORMAL;
        } else if (c == '8') {
            g_term.cursor_x = g_term.saved_x;
            g_term.cursor_y = g_term.saved_y;
            g_term.parser_state = STATE_NORMAL;
        } else {
            g_term.parser_state = STATE_NORMAL;
        }
    } else if (g_term.parser_state == STATE_CSI) {
        if (c == '?') {
            g_term.csi_private = true;
        } else if (c >= '0' && c <= '9') {
            if (g_term.csi_argc == 0) {
                g_term.csi_argc = 1;
            }
            g_term.csi_args[g_term.csi_argc - 1] = g_term.csi_args[g_term.csi_argc - 1] * 10 + (c - '0');
        } else if (c == ';') {
            if (g_term.csi_argc < 16) {
                g_term.csi_argc++;
                g_term.csi_args[g_term.csi_argc - 1] = 0;
            }
        } else {
            term_handle_csi(c);
            g_term.parser_state = STATE_NORMAL;
        }
    } else if (g_term.parser_state == STATE_OSC) {
        if (c == '\a' || c == '\033') {
            g_term.parser_state = STATE_NORMAL;
        }
    }
}

static void term_write_data(const char *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        term_put_char(buf[i]);
    }
    g_needs_redraw = true;
}

static void render_screen(void) {
    if (!g_dpy || !g_win || !g_backbuffer) return;

    /* Fill background */
    XSetForeground(g_dpy, g_gc, COLOR_BG);
    XFillRectangle(g_dpy, g_backbuffer, g_gc, 0, 0, (unsigned int)g_win_w, (unsigned int)g_win_h);

    /* Render text cells */
    for (int r = 0; r < g_term.rows; r++) {
        int py = PAD_Y + r * CHAR_H + 12; /* Baseline */
        int col = 0;

        while (col < g_term.cols) {
            cell_t *first = &g_term.grid[r][col];
            uint32_t fg = first->fg;
            uint32_t bg = first->bg;
            if (first->flags & ATTR_REVERSE) {
                uint32_t tmp = fg; fg = bg; bg = tmp;
            }

            char str[MAX_COLS + 1];
            int len = 0;
            int start_col = col;

            while (col < g_term.cols) {
                cell_t *curr = &g_term.grid[r][col];
                uint32_t cfg = curr->fg;
                uint32_t cbg = curr->bg;
                if (curr->flags & ATTR_REVERSE) {
                    uint32_t tmp = cfg; cfg = cbg; cbg = tmp;
                }

                if (cfg != fg || cbg != bg) break;

                str[len++] = (curr->ch >= 32 && curr->ch <= 126) ? curr->ch : ' ';
                col++;
            }
            str[len] = '\0';

            int px = PAD_X + start_col * CHAR_W;

            /* Draw background block if not default background */
            if (bg != COLOR_BG) {
                XSetForeground(g_dpy, g_gc, bg);
                XFillRectangle(g_dpy, g_backbuffer, g_gc, px, PAD_Y + r * CHAR_H, len * CHAR_W, CHAR_H);
            }

            /* Draw string */
            XSetForeground(g_dpy, g_gc, fg);
            XDrawString(g_dpy, g_backbuffer, g_gc, px, py, str, len);
        }
    }

    /* Render Cursor */
    if (g_term.cursor_visible && g_term.cursor_x >= 0 && g_term.cursor_x < g_term.cols &&
        g_term.cursor_y >= 0 && g_term.cursor_y < g_term.rows) {
        int cx = PAD_X + g_term.cursor_x * CHAR_W;
        int cy = PAD_Y + g_term.cursor_y * CHAR_H;

        if (g_has_focus) {
            /* Solid bright cursor block */
            XSetForeground(g_dpy, g_gc, COLOR_CURSOR);
            XFillRectangle(g_dpy, g_backbuffer, g_gc, cx, cy, CHAR_W, CHAR_H);

            /* Inverted cursor character */
            char ch = g_term.grid[g_term.cursor_y][g_term.cursor_x].ch;
            if (ch >= 32 && ch <= 126) {
                char s[2] = {ch, '\0'};
                XSetForeground(g_dpy, g_gc, COLOR_CURSOR_TEXT);
                XDrawString(g_dpy, g_backbuffer, g_gc, cx, cy + 12, s, 1);
            }
        } else {
            /* Hollow unfocused cursor frame */
            XSetForeground(g_dpy, g_gc, COLOR_CURSOR);
            XDrawRectangle(g_dpy, g_backbuffer, g_gc, cx, cy, CHAR_W - 1, CHAR_H - 1);
        }
    }

    /* Render Modern Neon Bottom-Right Resize Grip Handle */
    int rx = g_win_w - 14;
    int ry = g_win_h - 14;
    XSetForeground(g_dpy, g_gc, COLOR_CURSOR);
    XDrawLine(g_dpy, g_backbuffer, g_gc, rx + 10, ry + 2, rx + 2, ry + 10);
    XDrawLine(g_dpy, g_backbuffer, g_gc, rx + 10, ry + 6, rx + 6, ry + 10);
    XDrawLine(g_dpy, g_backbuffer, g_gc, rx + 10, ry + 10, rx + 10, ry + 10);

    /* Flip double-buffered backbuffer to window */
    XCopyArea(g_dpy, g_backbuffer, g_win, g_gc, 0, 0, (unsigned int)g_win_w, (unsigned int)g_win_h, 0, 0);
    XFlush(g_dpy);
}

static void send_pty_input(const char *buf, size_t len) {
    if (g_master_fd >= 0 && buf && len > 0) {
        write(g_master_fd, buf, len);
        g_needs_redraw = true;
        char tmp[512];
        ssize_t n = read(g_master_fd, tmp, sizeof(tmp));
        if (n > 0) {
            term_write_data(tmp, (size_t)n);
        }
    }
}

static KeySym keycode_to_fallback_sym(unsigned int keycode, unsigned int state) {
    bool shift = (state & ShiftMask) != 0;
    bool caps = (state & LockMask) != 0;
    bool upper = shift ^ caps;
    unsigned int evcode = (keycode >= 8) ? (keycode - 8) : 0;
    switch (evcode) {
    case 1: /* KEY_ESC */ return XK_Escape;
    case 2: /* KEY_1 */ return shift ? '!' : '1';
    case 3: /* KEY_2 */ return shift ? '@' : '2';
    case 4: /* KEY_3 */ return shift ? '#' : '3';
    case 5: /* KEY_4 */ return shift ? '$' : '4';
    case 6: /* KEY_5 */ return shift ? '%' : '5';
    case 7: /* KEY_6 */ return shift ? '^' : '6';
    case 8: /* KEY_7 */ return shift ? '&' : '7';
    case 9: /* KEY_8 */ return shift ? '*' : '8';
    case 10: /* KEY_9 */ return shift ? '(' : '9';
    case 11: /* KEY_0 */ return shift ? ')' : '0';
    case 12: /* KEY_MINUS */ return shift ? '_' : '-';
    case 13: /* KEY_EQUAL */ return shift ? '+' : '=';
    case 14: /* KEY_BACKSPACE */ return XK_BackSpace;
    case 15: /* KEY_TAB */ return XK_Tab;
    case 16: /* KEY_Q */ return upper ? 'Q' : 'q';
    case 17: /* KEY_W */ return upper ? 'W' : 'w';
    case 18: /* KEY_E */ return upper ? 'E' : 'e';
    case 19: /* KEY_R */ return upper ? 'R' : 'r';
    case 20: /* KEY_T */ return upper ? 'T' : 't';
    case 21: /* KEY_Y */ return upper ? 'Y' : 'y';
    case 22: /* KEY_U */ return upper ? 'U' : 'u';
    case 23: /* KEY_I */ return upper ? 'I' : 'i';
    case 24: /* KEY_O */ return upper ? 'O' : 'o';
    case 25: /* KEY_P */ return upper ? 'P' : 'p';
    case 26: /* KEY_LEFTBRACE */ return shift ? '{' : '[';
    case 27: /* KEY_RIGHTBRACE */ return shift ? '}' : ']';
    case 28: /* KEY_ENTER */ return XK_Return;
    case 30: /* KEY_A */ return upper ? 'A' : 'a';
    case 31: /* KEY_S */ return upper ? 'S' : 's';
    case 32: /* KEY_D */ return upper ? 'D' : 'd';
    case 33: /* KEY_F */ return upper ? 'F' : 'f';
    case 34: /* KEY_G */ return upper ? 'G' : 'g';
    case 35: /* KEY_H */ return upper ? 'H' : 'h';
    case 36: /* KEY_J */ return upper ? 'J' : 'j';
    case 37: /* KEY_K */ return upper ? 'K' : 'k';
    case 38: /* KEY_L */ return upper ? 'L' : 'l';
    case 39: /* KEY_SEMICOLON */ return shift ? ':' : ';';
    case 40: /* KEY_APOSTROPHE */ return shift ? '"' : '\'';
    case 41: /* KEY_GRAVE */ return shift ? '~' : '`';
    case 43: /* KEY_BACKSLASH */ return shift ? '|' : '\\';
    case 44: /* KEY_Z */ return upper ? 'Z' : 'z';
    case 45: /* KEY_X */ return upper ? 'X' : 'x';
    case 46: /* KEY_C */ return upper ? 'C' : 'c';
    case 47: /* KEY_V */ return upper ? 'V' : 'v';
    case 48: /* KEY_B */ return upper ? 'B' : 'b';
    case 49: /* KEY_N */ return upper ? 'N' : 'n';
    case 50: /* KEY_M */ return upper ? 'M' : 'm';
    case 51: /* KEY_COMMA */ return shift ? '<' : ',';
    case 52: /* KEY_DOT */ return shift ? '>' : '.';
    case 53: /* KEY_SLASH */ return shift ? '?' : '/';
    case 57: /* KEY_SPACE */ return ' ';
    case 103: /* KEY_UP */ return XK_Up;
    case 108: /* KEY_DOWN */ return XK_Down;
    case 105: /* KEY_LEFT */ return XK_Left;
    case 106: /* KEY_RIGHT */ return XK_Right;
    case 102: /* KEY_HOME */ return XK_Home;
    case 107: /* KEY_END */ return XK_End;
    case 104: /* KEY_PAGEUP */ return XK_Page_Up;
    case 109: /* KEY_PAGEDOWN */ return XK_Page_Down;
    case 111: /* KEY_DELETE */ return XK_Delete;
    default: return NoSymbol;
    }
}

static void handle_x_events(void) {
    while (XPending(g_dpy)) {
        XEvent ev;
        XNextEvent(g_dpy, &ev);

        switch (ev.type) {
        case Expose:
            g_needs_redraw = true;
            break;

        case ConfigureNotify:
            if (ev.xconfigure.width != g_win_w || ev.xconfigure.height != g_win_h) {
                term_resize(ev.xconfigure.width, ev.xconfigure.height);
            }
            break;

        case FocusIn:
            g_has_focus = true;
            g_needs_redraw = true;
            break;

        case FocusOut:
            g_has_focus = false;
            g_needs_redraw = true;
            break;

        case ButtonPress:
            g_has_focus = true;
            XSetInputFocus(g_dpy, g_win, RevertToPointerRoot, CurrentTime);
            /* Check if clicked in bottom-right resize grip or near right/bottom edges */
            if ((ev.xbutton.x >= g_win_w - 24 && ev.xbutton.y >= g_win_h - 24) ||
                (ev.xbutton.x >= g_win_w - 8) || (ev.xbutton.y >= g_win_h - 8)) {
                g_is_resizing = true;
                g_resize_start_mx = ev.xbutton.x_root;
                g_resize_start_my = ev.xbutton.y_root;
                g_orig_w = g_win_w;
                g_orig_h = g_win_h;
            }
            g_needs_redraw = true;
            break;

        case ButtonRelease:
            if (g_is_resizing) {
                g_is_resizing = false;
                g_needs_redraw = true;
            }
            break;

        case MotionNotify:
            if (g_is_resizing) {
                int dx = ev.xmotion.x_root - g_resize_start_mx;
                int dy = ev.xmotion.y_root - g_resize_start_my;
                int nw = g_orig_w + dx;
                int nh = g_orig_h + dy;
                if (nw < MIN_COLS * CHAR_W + PAD_X * 2) nw = MIN_COLS * CHAR_W + PAD_X * 2;
                if (nh < MIN_ROWS * CHAR_H + PAD_Y * 2) nh = MIN_ROWS * CHAR_H + PAD_Y * 2;
                if (nw != g_win_w || nh != g_win_h) {
                    XResizeWindow(g_dpy, g_win, (unsigned int)nw, (unsigned int)nh);
                    term_resize(nw, nh);
                }
            }
            break;

        case KeyPress: {
            char buf[32];
            KeySym sym = NoSymbol;
            int len = XLookupString(&ev.xkey, buf, sizeof(buf) - 1, &sym, NULL);
            if (sym == NoSymbol) {
                sym = keycode_to_fallback_sym(ev.xkey.keycode, ev.xkey.state);
            }

            /* Resizing Shortcuts */
            if ((ev.xkey.state & ControlMask) && (sym == XK_plus || sym == XK_equal || sym == XK_KP_Add)) {
                /* Increase size */
                int nw = g_win_w + 80;
                int nh = g_win_h + 48;
                XResizeWindow(g_dpy, g_win, (unsigned int)nw, (unsigned int)nh);
                term_resize(nw, nh);
                break;
            } else if ((ev.xkey.state & ControlMask) && (sym == XK_minus || sym == XK_underscore || sym == XK_KP_Subtract)) {
                /* Decrease size */
                int nw = g_win_w - 80;
                int nh = g_win_h - 48;
                XResizeWindow(g_dpy, g_win, (unsigned int)nw, (unsigned int)nh);
                term_resize(nw, nh);
                break;
            } else if (sym == XK_F11) {
                /* Toggle Fullscreen / Maximized Terminal */
                int screen = DefaultScreen(g_dpy);
                int sw = DisplayWidth(g_dpy, screen);
                int sh = DisplayHeight(g_dpy, screen);
                if (!g_is_maximized) {
                    g_saved_premax_w = g_win_w;
                    g_saved_premax_h = g_win_h;
                    int max_w = sw - 80;
                    int max_h = sh - 120;
                    XMoveResizeWindow(g_dpy, g_win, 40, 60, (unsigned int)max_w, (unsigned int)max_h);
                    term_resize(max_w, max_h);
                    g_is_maximized = true;
                } else {
                    int rw = (g_saved_premax_w > 0) ? g_saved_premax_w : (DEFAULT_COLS * CHAR_W + PAD_X * 2);
                    int rh = (g_saved_premax_h > 0) ? g_saved_premax_h : (DEFAULT_ROWS * CHAR_H + PAD_Y * 2);
                    XResizeWindow(g_dpy, g_win, (unsigned int)rw, (unsigned int)rh);
                    term_resize(rw, rh);
                    g_is_maximized = false;
                }
                break;
            }

            if (sym == XK_Return || sym == XK_KP_Enter) {
                send_pty_input("\r", 1);
            } else if (sym == XK_BackSpace) {
                send_pty_input("\b", 1);
            } else if (sym == XK_Tab) {
                send_pty_input("\t", 1);
            } else if (sym == XK_Escape) {
                send_pty_input("\033", 1);
            } else if (sym == XK_Up) {
                send_pty_input("\033[A", 3);
            } else if (sym == XK_Down) {
                send_pty_input("\033[B", 3);
            } else if (sym == XK_Right) {
                send_pty_input("\033[C", 3);
            } else if (sym == XK_Left) {
                send_pty_input("\033[D", 3);
            } else if (sym == XK_Home) {
                send_pty_input("\033[H", 3);
            } else if (sym == XK_End) {
                send_pty_input("\033[F", 3);
            } else if (sym == XK_Page_Up) {
                send_pty_input("\033[5~", 4);
            } else if (sym == XK_Page_Down) {
                send_pty_input("\033[6~", 4);
            } else if (ev.xkey.state & ControlMask) {
                if (sym >= 'a' && sym <= 'z') {
                    char c = (char)(sym - 'a' + 1);
                    send_pty_input(&c, 1);
                } else if (sym >= 'A' && sym <= 'Z') {
                    char c = (char)(sym - 'A' + 1);
                    send_pty_input(&c, 1);
                }
            } else if (len > 0) {
                send_pty_input(buf, len);
            } else if (sym >= 0x20 && sym <= 0x7E) {
                char c = (char)sym;
                send_pty_input(&c, 1);
            }
            break;
        }

        case ClientMessage:
            if ((Atom)ev.xclient.data.l[0] == g_wm_delete) {
                g_running = false;
            }
            break;
        }
    }
}

int main(int argc, char **argv) {
    const char *disp_name = NULL;
    const char *custom_title = NULL;
    char **exec_cmd = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-display") == 0 && i + 1 < argc) {
            disp_name = argv[++i];
        } else if (strcmp(argv[i], "-title") == 0 && i + 1 < argc) {
            custom_title = argv[++i];
        } else if (strcmp(argv[i], "-e") == 0 && i + 1 < argc) {
            exec_cmd = &argv[i + 1];
            break;
        }
    }

    if (!disp_name) {
        disp_name = getenv("DISPLAY");
    }
    if (!disp_name || !*disp_name) {
        disp_name = ":0";
    }

    term_init();

    /* Initialize pseudo-terminal pair */
    struct winsize ws;
    ws.ws_col = DEFAULT_COLS;
    ws.ws_row = DEFAULT_ROWS;
    ws.ws_xpixel = (unsigned short)g_win_w;
    ws.ws_ypixel = (unsigned short)g_win_h;

    int slave_fd = -1;
    if (openpty(&g_master_fd, &slave_fd, NULL, NULL, &ws) < 0) {
        perror("openpty");
        return 1;
    }

    /* Prepare comprehensive environment for child shell/process */
    char env_display[128];
    char env_term[64] = "TERM=xterm-256color";
    char env_colorterm[64] = "COLORTERM=truecolor";
    char env_path[256];
    char env_home[256];
    char env_user[128];
    char env_logname[128];
    char env_shell[256];
    char env_pwd[512];
    char env_lang[64] = "LANG=C.UTF-8";
    char env_lcall[64] = "LC_ALL=C.UTF-8";
    char env_lines[64];
    char env_columns[64];

    snprintf(env_display, sizeof(env_display), "DISPLAY=%s", disp_name);
    snprintf(env_path, sizeof(env_path), "PATH=%s", getenv("PATH") ? getenv("PATH") : "/bin:/usr/bin:/usr/local/bin");
    snprintf(env_home, sizeof(env_home), "HOME=%s", getenv("HOME") ? getenv("HOME") : "/root");
    snprintf(env_user, sizeof(env_user), "USER=%s", getenv("USER") ? getenv("USER") : "root");
    snprintf(env_logname, sizeof(env_logname), "LOGNAME=%s", getenv("LOGNAME") ? getenv("LOGNAME") : (getenv("USER") ? getenv("USER") : "root"));
    snprintf(env_shell, sizeof(env_shell), "SHELL=%s", getenv("SHELL") ? getenv("SHELL") : "/bin/sh");

    char cwd_buf[256];
    if (getcwd(cwd_buf, sizeof(cwd_buf))) {
        snprintf(env_pwd, sizeof(env_pwd), "PWD=%s", cwd_buf);
    } else {
        snprintf(env_pwd, sizeof(env_pwd), "PWD=%s", getenv("PWD") ? getenv("PWD") : "/root");
    }

    snprintf(env_lines, sizeof(env_lines), "LINES=%d", DEFAULT_ROWS);
    snprintf(env_columns, sizeof(env_columns), "COLUMNS=%d", DEFAULT_COLS);

    char *child_envp[128];
    int env_idx = 0;
    child_envp[env_idx++] = env_display;
    child_envp[env_idx++] = env_term;
    child_envp[env_idx++] = env_colorterm;
    child_envp[env_idx++] = env_path;
    child_envp[env_idx++] = env_home;
    child_envp[env_idx++] = env_user;
    child_envp[env_idx++] = env_logname;
    child_envp[env_idx++] = env_shell;
    child_envp[env_idx++] = env_pwd;
    child_envp[env_idx++] = env_lang;
    child_envp[env_idx++] = env_lcall;
    child_envp[env_idx++] = env_lines;
    child_envp[env_idx++] = env_columns;

    extern char **environ;
    if (environ) {
        for (int i = 0; environ[i] && env_idx < 120; i++) {
            if (strncmp(environ[i], "DISPLAY=", 8) != 0 &&
                strncmp(environ[i], "TERM=", 5) != 0 &&
                strncmp(environ[i], "COLORTERM=", 10) != 0 &&
                strncmp(environ[i], "PATH=", 5) != 0 &&
                strncmp(environ[i], "HOME=", 5) != 0 &&
                strncmp(environ[i], "USER=", 5) != 0 &&
                strncmp(environ[i], "LOGNAME=", 8) != 0 &&
                strncmp(environ[i], "SHELL=", 6) != 0 &&
                strncmp(environ[i], "PWD=", 4) != 0 &&
                strncmp(environ[i], "LANG=", 5) != 0 &&
                strncmp(environ[i], "LC_ALL=", 7) != 0 &&
                strncmp(environ[i], "LINES=", 6) != 0 &&
                strncmp(environ[i], "COLUMNS=", 8) != 0) {
                child_envp[env_idx++] = environ[i];
            }
        }
    }
    child_envp[env_idx] = NULL;

    g_child_pid = fork();
    if (g_child_pid < 0) {
        perror("fork");
        return 1;
    }

    if (g_child_pid == 0) {
        /* Child process: Shell */
        close(g_master_fd);
        setsid();
        ioctl(slave_fd, TIOCSCTTY, 0);

        dup2(slave_fd, 0);
        dup2(slave_fd, 1);
        dup2(slave_fd, 2);
        if (slave_fd > 2) close(slave_fd);

        if (exec_cmd && *exec_cmd) {
            execve(exec_cmd[0], exec_cmd, child_envp);
            perror("execve custom command");
        } else {
            const char *shell_path = getenv("SHELL");
            if (!shell_path || !*shell_path) shell_path = "/bin/sh";
            char *sh_args[] = {(char *)shell_path, NULL};
            execve(shell_path, sh_args, child_envp);
            perror("execve shell");
        }
        _exit(127);
    }

    close(slave_fd);
    fcntl(g_master_fd, F_SETFL, O_NONBLOCK);

    /* Connect to SzpontX11 Server */
    g_dpy = XOpenDisplay(disp_name);
    if (!g_dpy && strcmp(disp_name, ":0") != 0) {
        g_dpy = XOpenDisplay(":0");
    }
    if (!g_dpy) {
        for (int retries = 0; retries < 5 && !g_dpy; retries++) {
            usleep(50000);
            g_dpy = XOpenDisplay(":0");
        }
    }
    if (!g_dpy) {
        fprintf(stderr, "szponterm: Cannot open display :0\n");
        return 1;
    }

    int screen = DefaultScreen(g_dpy);
    Window root = RootWindow(g_dpy, screen);

    int screen_w = DisplayWidth(g_dpy, screen);
    int screen_h = DisplayHeight(g_dpy, screen);
    pid_t my_pid = getpid();
    int win_x = 860 + ((int)(my_pid * 47) % 450);
    int win_y = 660 + ((int)(my_pid * 31) % 300);
    if (win_x + g_win_w > screen_w) {
        win_x = 40 + ((int)(my_pid * 23) % 300);
    }
    if (win_y + g_win_h > screen_h) {
        win_y = 80 + ((int)(my_pid * 17) % 250);
    }

    g_win = XCreateSimpleWindow(g_dpy, root, win_x, win_y, (unsigned int)g_win_w, (unsigned int)g_win_h, 2, 0xFF38BDF8, COLOR_BG);

    char title_str[64];
    if (custom_title && *custom_title) {
        snprintf(title_str, sizeof(title_str), "%s", custom_title);
    } else {
        snprintf(title_str, sizeof(title_str), "SzponTerm (PID %d)", my_pid);
    }
    XStoreName(g_dpy, g_win, title_str);

    /* Set Window Manager Protocols */
    g_wm_delete = XInternAtom(g_dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_dpy, g_win, &g_wm_delete, 1);

    /* Allocate GC and Double-Buffered Backbuffer */
    g_gc = XCreateGC(g_dpy, g_win, 0, NULL);
    g_backbuffer = XCreatePixmap(g_dpy, g_win, (unsigned int)g_win_w, (unsigned int)g_win_h, DefaultDepth(g_dpy, screen));

    XSelectInput(g_dpy, g_win, ExposureMask | KeyPressMask | KeyReleaseMask | FocusChangeMask | StructureNotifyMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
    XMapWindow(g_dpy, g_win);
    XSetInputFocus(g_dpy, g_win, RevertToPointerRoot, CurrentTime);
    XFlush(g_dpy);

    struct pollfd pfds[2];
    pfds[0].fd = g_master_fd;
    pfds[0].events = POLLIN;
    pfds[1].fd = ConnectionNumber(g_dpy);
    pfds[1].events = POLLIN;

    char read_buf[4096];

    while (g_running) {
        int ret = poll(pfds, 2, 20); /* 20ms = ~50 FPS responsive updates */

        if (ret > 0) {
            /* 1. Process PTY shell output */
            if (pfds[0].revents & POLLIN) {
                ssize_t n = read(g_master_fd, read_buf, sizeof(read_buf));
                if (n > 0) {
                    term_write_data(read_buf, (size_t)n);
                } else if (n == 0) {
                    /* Shell terminated */
                    g_running = false;
                }
            }

            /* 2. Process X11 events */
            if (pfds[1].revents & POLLIN) {
                handle_x_events();
            }
        }

        /* Drain any pending PTY output */
        ssize_t pn = read(g_master_fd, read_buf, sizeof(read_buf));
        if (pn > 0) {
            term_write_data(read_buf, (size_t)pn);
        }

        /* Check for any pending X11 queue events */
        if (XPending(g_dpy)) {
            handle_x_events();
        }

        /* Check if child process died */
        int status;
        if (waitpid(g_child_pid, &status, WNOHANG) == g_child_pid) {
            g_running = false;
        }

        /* Redraw frame if dirty */
        if (g_needs_redraw) {
            render_screen();
            g_needs_redraw = false;
        }
    }

    if (g_child_pid > 0) {
        kill(g_child_pid, SIGTERM);
        waitpid(g_child_pid, NULL, WNOHANG);
    }

    if (g_backbuffer) XFreePixmap(g_dpy, g_backbuffer);
    if (g_gc) XFreeGC(g_dpy, g_gc);
    if (g_win) XDestroyWindow(g_dpy, g_win);
    if (g_dpy) XCloseDisplay(g_dpy);
    if (g_master_fd >= 0) close(g_master_fd);

    return 0;
}
