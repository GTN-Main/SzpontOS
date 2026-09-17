/*
 * SzpontOS — szponterm Native X11 Terminal Emulator
 * (C) Copyright by Szpont Industries. All rights reserved.
 *
 * Fast, lightweight, 256-color & TrueColor ANSI/VT100/VT220 terminal emulator
 * inspired by xterm architecture. Features full DECSTBM scrolling regions,
 * Alternate Screen Buffer, CPR/DA status reporting, 2000-line scrollback buffer,
 * mouse wheel scrolling, dynamic resizing with SIGWINCH, and cyber slate aesthetic.
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
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pty.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

#define MAX_COLS 256
#define MAX_ROWS 128
#define MIN_COLS 24
#define MIN_ROWS 6

#define DEFAULT_COLS 80
#define DEFAULT_ROWS 25
#define PAD_X 10
#define PAD_Y 8

#define SCROLLBACK_MAX 2000

/* Parser States */
enum {
    STATE_NORMAL = 0,
    STATE_ESC,
    STATE_CSI,
    STATE_OSC,
    STATE_CHARSET
};

/* Cell Attributes */
#define ATTR_BOLD      (1 << 0)
#define ATTR_UNDERLINE (1 << 1)
#define ATTR_REVERSE   (1 << 2)
#define ATTR_DIM       (1 << 3)

/* Modern Cyber Slate Palette */
#define COLOR_BG          0xFF0F172A     /* Dark Navy Slate */
#define COLOR_FG          0xFFF8FAFC     /* Soft Crisp White */
#define COLOR_CURSOR      0xFF38BDF8     /* Neon Cyan Accent */
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

static uint32_t get_color_256(int idx) {
    if (idx < 0) return COLOR_FG;
    if (idx < 16) return g_ansi_palette[idx];
    if (idx < 232) {
        /* 6x6x6 RGB color cube */
        int code = idx - 16;
        int r = code / 36;
        int g = (code / 6) % 6;
        int b = code % 6;
        uint8_t cr = r ? (uint8_t)(r * 40 + 55) : 0;
        uint8_t cg = g ? (uint8_t)(g * 40 + 55) : 0;
        uint8_t cb = b ? (uint8_t)(b * 40 + 55) : 0;
        return 0xFF000000 | (cr << 16) | (cg << 8) | cb;
    }
    if (idx < 256) {
        /* 24-step grayscale ramp */
        uint8_t gray = (uint8_t)((idx - 232) * 10 + 8);
        return 0xFF000000 | (gray << 16) | (gray << 8) | gray;
    }
    return COLOR_FG;
}

typedef struct {
    char ch;
    uint32_t fg;
    uint32_t bg;
    uint8_t flags;
} cell_t;

typedef struct {
    cell_t cells[MAX_COLS];
    int cols;
} scrollback_line_t;

typedef struct {
    cell_t grid[MAX_ROWS][MAX_COLS];
    cell_t main_grid[MAX_ROWS][MAX_COLS];
    bool alt_screen;

    int cols;
    int rows;
    int cursor_x;
    int cursor_y;

    /* Scrolling region (DECSTBM) */
    int top_margin;
    int bottom_margin;

    /* Saved state (DECSC / SCOSC) */
    int saved_x;
    int saved_y;
    uint32_t saved_fg;
    uint32_t saved_bg;
    uint8_t saved_flags;

    uint32_t cur_fg;
    uint32_t cur_bg;
    uint8_t cur_flags;
    bool cursor_visible;
    bool app_cursor; /* DECCKM */
    bool auto_wrap;  /* DECAWM */

    int parser_state;
    int csi_args[16];
    int csi_argc;
    bool csi_private;

    char osc_buf[256];
    int osc_len;
} term_t;

static Display *g_dpy = NULL;
static Window g_win;
static GC g_gc;
static Pixmap g_backbuffer = 0;
static Atom g_wm_delete;
static int g_master_fd = -1;
static pid_t g_child_pid = -1;
static term_t g_term;
static bool g_running = true;
static bool g_needs_redraw = true;
static bool g_has_focus = true;

/* Scrollback Ring Buffer */
static scrollback_line_t g_scrollback[SCROLLBACK_MAX];
static int g_scrollback_count = 0;
static int g_scrollback_head = 0;
static int g_scroll_offset = 0;

/* Dynamic Font Geometry & Dimensions */
static int g_char_w = 6;
static int g_char_h = 13;
static int g_char_ascent = 11;
static XFontStruct *g_font_info = NULL;

/* Dynamic Dimensions & Resizing State */
static int g_win_w = DEFAULT_COLS * 6 + PAD_X * 2;
static int g_win_h = DEFAULT_ROWS * 13 + PAD_Y * 2;
static bool g_is_resizing = false;
static int g_resize_start_mx = 0;
static int g_resize_start_my = 0;
static int g_orig_w = 0;
static int g_orig_h = 0;
static bool g_is_maximized = false;
static int g_saved_premax_w = 0;
static int g_saved_premax_h = 0;

static void term_reset_attributes(void) {
    g_term.cur_fg = COLOR_FG;
    g_term.cur_bg = COLOR_BG;
    g_term.cur_flags = 0;
}

static void term_clear_line(int row, int start_col, int end_col) {
    if (row < 0 || row >= g_term.rows || row >= MAX_ROWS)
        return;
    if (start_col < 0)
        start_col = 0;
    if (end_col > g_term.cols)
        end_col = g_term.cols;
    if (end_col > MAX_COLS)
        end_col = MAX_COLS;

    for (int c = start_col; c < end_col; c++) {
        g_term.grid[row][c].ch = ' ';
        g_term.grid[row][c].fg = g_term.cur_fg;
        g_term.grid[row][c].bg = g_term.cur_bg;
        g_term.grid[row][c].flags = 0;
    }
}

static void term_clear_line_default(int row, int start_col, int end_col) {
    if (row < 0 || row >= g_term.rows || row >= MAX_ROWS)
        return;
    if (start_col < 0)
        start_col = 0;
    if (end_col > g_term.cols)
        end_col = g_term.cols;
    if (end_col > MAX_COLS)
        end_col = MAX_COLS;

    for (int c = start_col; c < end_col; c++) {
        g_term.grid[row][c].ch = ' ';
        g_term.grid[row][c].fg = COLOR_FG;
        g_term.grid[row][c].bg = COLOR_BG;
        g_term.grid[row][c].flags = 0;
    }
}

static void term_clear_all(void) {
    for (int r = 0; r < g_term.rows; r++) {
        term_clear_line_default(r, 0, g_term.cols);
    }
}

static void scrollback_push(const cell_t *line, int cols) {
    int idx;
    if (g_scrollback_count < SCROLLBACK_MAX) {
        idx = (g_scrollback_head + g_scrollback_count) % SCROLLBACK_MAX;
        g_scrollback_count++;
    } else {
        idx = g_scrollback_head;
        g_scrollback_head = (g_scrollback_head + 1) % SCROLLBACK_MAX;
    }
    int copy_cols = (cols < MAX_COLS) ? cols : MAX_COLS;
    memcpy(g_scrollback[idx].cells, line, sizeof(cell_t) * copy_cols);
    g_scrollback[idx].cols = copy_cols;
}

static void term_scroll_up(int count) {
    if (count <= 0) count = 1;
    int top = g_term.top_margin;
    int bot = g_term.bottom_margin;
    if (top < 0) top = 0;
    if (bot >= g_term.rows) bot = g_term.rows - 1;
    if (top >= bot) return;

    for (int i = 0; i < count; i++) {
        if (top == 0 && bot == g_term.rows - 1 && !g_term.alt_screen) {
            scrollback_push(g_term.grid[0], g_term.cols);
        }
        memmove(&g_term.grid[top][0], &g_term.grid[top + 1][0], sizeof(cell_t) * MAX_COLS * (bot - top));
        term_clear_line_default(bot, 0, g_term.cols);
    }
}

static void term_scroll_down(int count) {
    if (count <= 0) count = 1;
    int top = g_term.top_margin;
    int bot = g_term.bottom_margin;
    if (top < 0) top = 0;
    if (bot >= g_term.rows) bot = g_term.rows - 1;
    if (top >= bot) return;

    for (int i = 0; i < count; i++) {
        memmove(&g_term.grid[top + 1][0], &g_term.grid[top][0], sizeof(cell_t) * MAX_COLS * (bot - top));
        term_clear_line_default(top, 0, g_term.cols);
    }
}

static void term_insert_lines(int count) {
    if (count <= 0) count = 1;
    int top = g_term.cursor_y;
    int bot = g_term.bottom_margin;
    if (top < g_term.top_margin || top > bot) return;
    for (int i = 0; i < count && top <= bot; i++) {
        memmove(&g_term.grid[top + 1][0], &g_term.grid[top][0], sizeof(cell_t) * MAX_COLS * (bot - top));
        term_clear_line_default(top, 0, g_term.cols);
    }
}

static void term_delete_lines(int count) {
    if (count <= 0) count = 1;
    int top = g_term.cursor_y;
    int bot = g_term.bottom_margin;
    if (top < g_term.top_margin || top > bot) return;
    for (int i = 0; i < count && top <= bot; i++) {
        memmove(&g_term.grid[top][0], &g_term.grid[top + 1][0], sizeof(cell_t) * MAX_COLS * (bot - top));
        term_clear_line_default(bot, 0, g_term.cols);
    }
}

static void term_insert_chars(int count) {
    if (count <= 0) count = 1;
    int r = g_term.cursor_y;
    int c = g_term.cursor_x;
    if (r < 0 || r >= g_term.rows || c < 0 || c >= g_term.cols) return;
    if (count > g_term.cols - c) count = g_term.cols - c;
    memmove(&g_term.grid[r][c + count], &g_term.grid[r][c], sizeof(cell_t) * (g_term.cols - c - count));
    term_clear_line(r, c, c + count);
}

static void term_delete_chars(int count) {
    if (count <= 0) count = 1;
    int r = g_term.cursor_y;
    int c = g_term.cursor_x;
    if (r < 0 || r >= g_term.rows || c < 0 || c >= g_term.cols) return;
    if (count > g_term.cols - c) count = g_term.cols - c;
    memmove(&g_term.grid[r][c], &g_term.grid[r][c + count], sizeof(cell_t) * (g_term.cols - c - count));
    term_clear_line(r, g_term.cols - count, g_term.cols);
}

static void term_erase_chars(int count) {
    if (count <= 0) count = 1;
    int r = g_term.cursor_y;
    int c = g_term.cursor_x;
    if (r < 0 || r >= g_term.rows || c < 0 || c >= g_term.cols) return;
    int end = c + count;
    if (end > g_term.cols) end = g_term.cols;
    term_clear_line(r, c, end);
}

static void term_set_alt_screen(bool enable) {
    if (g_term.alt_screen == enable) return;
    g_term.alt_screen = enable;
    if (enable) {
        /* Save cursor position & attributes */
        g_term.saved_x = g_term.cursor_x;
        g_term.saved_y = g_term.cursor_y;
        g_term.saved_fg = g_term.cur_fg;
        g_term.saved_bg = g_term.cur_bg;
        g_term.saved_flags = g_term.cur_flags;
        /* Copy current screen to main grid and clear active */
        memcpy(g_term.main_grid, g_term.grid, sizeof(g_term.grid));
        term_clear_all();
        g_term.cursor_x = 0;
        g_term.cursor_y = 0;
    } else {
        /* Restore main grid and saved cursor */
        memcpy(g_term.grid, g_term.main_grid, sizeof(g_term.grid));
        g_term.cursor_x = g_term.saved_x;
        g_term.cursor_y = g_term.saved_y;
        g_term.cur_fg = g_term.saved_fg;
        g_term.cur_bg = g_term.saved_bg;
        g_term.cur_flags = g_term.saved_flags;
    }
    g_scroll_offset = 0;
    g_needs_redraw = true;
}

static void term_init(void) {
    memset(&g_term, 0, sizeof(term_t));
    g_term.cols = DEFAULT_COLS;
    g_term.rows = DEFAULT_ROWS;
    g_term.top_margin = 0;
    g_term.bottom_margin = DEFAULT_ROWS - 1;
    g_term.cursor_x = 0;
    g_term.cursor_y = 0;
    g_term.cursor_visible = true;
    g_term.auto_wrap = true;
    g_term.app_cursor = false;
    term_reset_attributes();
    term_clear_all();
}

static void term_resize(int new_w, int new_h) {
    if (new_w < MIN_COLS * g_char_w + PAD_X * 2)
        new_w = MIN_COLS * g_char_w + PAD_X * 2;
    if (new_h < MIN_ROWS * g_char_h + PAD_Y * 2)
        new_h = MIN_ROWS * g_char_h + PAD_Y * 2;

    int new_cols = (new_w - PAD_X * 2) / g_char_w;
    int new_rows = (new_h - PAD_Y * 2) / g_char_h;
    if (new_cols > MAX_COLS) new_cols = MAX_COLS;
    if (new_rows > MAX_ROWS) new_rows = MAX_ROWS;

    g_win_w = new_w;
    g_win_h = new_h;
    int old_rows = g_term.rows;
    int old_cols = g_term.cols;
    g_term.cols = new_cols;
    g_term.rows = new_rows;
    g_term.top_margin = 0;
    g_term.bottom_margin = new_rows - 1;

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
        g_backbuffer =
            XCreatePixmap(g_dpy, g_win, (unsigned int)g_win_w, (unsigned int)g_win_h, DefaultDepth(g_dpy, screen));
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
    if (g_child_pid > 1) {
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
        } else if (code == 2) {
            g_term.cur_flags |= ATTR_DIM;
        } else if (code == 4) {
            g_term.cur_flags |= ATTR_UNDERLINE;
        } else if (code == 7) {
            g_term.cur_flags |= ATTR_REVERSE;
        } else if (code == 22) {
            g_term.cur_flags &= ~(ATTR_BOLD | ATTR_DIM);
        } else if (code == 24) {
            g_term.cur_flags &= ~ATTR_UNDERLINE;
        } else if (code == 27) {
            g_term.cur_flags &= ~ATTR_REVERSE;
        } else if (code >= 30 && code <= 37) {
            g_term.cur_fg = g_ansi_palette[code - 30];
        } else if (code == 38) {
            if (i + 2 < g_term.csi_argc && g_term.csi_args[i + 1] == 5) {
                /* 256-color FG: 38;5;idx */
                g_term.cur_fg = get_color_256(g_term.csi_args[i + 2]);
                i += 2;
            } else if (i + 4 < g_term.csi_argc && g_term.csi_args[i + 1] == 2) {
                /* TrueColor 24-bit FG: 38;2;r;g;b */
                uint8_t r = (uint8_t)g_term.csi_args[i + 2];
                uint8_t g = (uint8_t)g_term.csi_args[i + 3];
                uint8_t b = (uint8_t)g_term.csi_args[i + 4];
                g_term.cur_fg = 0xFF000000 | (r << 16) | (g << 8) | b;
                i += 4;
            }
        } else if (code == 39) {
            g_term.cur_fg = COLOR_FG;
        } else if (code >= 40 && code <= 47) {
            g_term.cur_bg = g_ansi_palette[code - 40];
        } else if (code == 48) {
            if (i + 2 < g_term.csi_argc && g_term.csi_args[i + 1] == 5) {
                /* 256-color BG: 48;5;idx */
                g_term.cur_bg = get_color_256(g_term.csi_args[i + 2]);
                i += 2;
            } else if (i + 4 < g_term.csi_argc && g_term.csi_args[i + 1] == 2) {
                /* TrueColor 24-bit BG: 48;2;r;g;b */
                uint8_t r = (uint8_t)g_term.csi_args[i + 2];
                uint8_t g = (uint8_t)g_term.csi_args[i + 3];
                uint8_t b = (uint8_t)g_term.csi_args[i + 4];
                g_term.cur_bg = 0xFF000000 | (r << 16) | (g << 8) | b;
                i += 4;
            }
        } else if (code == 49) {
            g_term.cur_bg = COLOR_BG;
        } else if (code >= 90 && code <= 97) {
            g_term.cur_fg = g_ansi_palette[code - 90 + 8];
        } else if (code >= 100 && code <= 107) {
            g_term.cur_bg = g_ansi_palette[code - 100 + 8];
        }
    }
}

static void term_handle_csi(char final_char) {
    int arg1 = (g_term.csi_argc > 0 && g_term.csi_args[0] > 0) ? g_term.csi_args[0] : 1;
    int arg2 = (g_term.csi_argc > 1 && g_term.csi_args[1] > 0) ? g_term.csi_args[1] : 1;

    if (g_term.csi_private) {
        switch (final_char) {
        case 'h': /* DECSET */
            for (int i = 0; i < (g_term.csi_argc ? g_term.csi_argc : 1); i++) {
                int mode = g_term.csi_args[i];
                if (mode == 1) {
                    g_term.app_cursor = true;
                } else if (mode == 7) {
                    g_term.auto_wrap = true;
                } else if (mode == 25) {
                    g_term.cursor_visible = true;
                } else if (mode == 47 || mode == 1047 || mode == 1049) {
                    term_set_alt_screen(true);
                }
            }
            break;
        case 'l': /* DECRST */
            for (int i = 0; i < (g_term.csi_argc ? g_term.csi_argc : 1); i++) {
                int mode = g_term.csi_args[i];
                if (mode == 1) {
                    g_term.app_cursor = false;
                } else if (mode == 7) {
                    g_term.auto_wrap = false;
                } else if (mode == 25) {
                    g_term.cursor_visible = false;
                } else if (mode == 47 || mode == 1047 || mode == 1049) {
                    term_set_alt_screen(false);
                }
            }
            break;
        }
        return;
    }

    switch (final_char) {
    case 'A': /* Cursor Up */
        g_term.cursor_y -= arg1;
        if (g_term.cursor_y < g_term.top_margin) g_term.cursor_y = g_term.top_margin;
        break;
    case 'B': /* Cursor Down */
        g_term.cursor_y += arg1;
        if (g_term.cursor_y > g_term.bottom_margin) g_term.cursor_y = g_term.bottom_margin;
        break;
    case 'C': /* Cursor Forward */
        g_term.cursor_x += arg1;
        if (g_term.cursor_x >= g_term.cols) g_term.cursor_x = g_term.cols - 1;
        break;
    case 'D': /* Cursor Backward */
        g_term.cursor_x -= arg1;
        if (g_term.cursor_x < 0) g_term.cursor_x = 0;
        break;
    case 'E': /* Cursor Next Line */
        g_term.cursor_x = 0;
        g_term.cursor_y += arg1;
        if (g_term.cursor_y > g_term.bottom_margin) g_term.cursor_y = g_term.bottom_margin;
        break;
    case 'F': /* Cursor Previous Line */
        g_term.cursor_x = 0;
        g_term.cursor_y -= arg1;
        if (g_term.cursor_y < g_term.top_margin) g_term.cursor_y = g_term.top_margin;
        break;
    case 'G': /* Cursor Character Absolute */
    case '`':
        g_term.cursor_x = arg1 - 1;
        if (g_term.cursor_x < 0) g_term.cursor_x = 0;
        if (g_term.cursor_x >= g_term.cols) g_term.cursor_x = g_term.cols - 1;
        break;
    case 'd': /* Vertical Line Position Absolute */
        g_term.cursor_y = arg1 - 1;
        if (g_term.cursor_y < 0) g_term.cursor_y = 0;
        if (g_term.cursor_y >= g_term.rows) g_term.cursor_y = g_term.rows - 1;
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
    case 'L': /* Insert Lines */
        term_insert_lines(arg1);
        break;
    case 'M': /* Delete Lines */
        term_delete_lines(arg1);
        break;
    case '@': /* Insert Characters */
        term_insert_chars(arg1);
        break;
    case 'P': /* Delete Characters */
        term_delete_chars(arg1);
        break;
    case 'X': /* Erase Characters */
        term_erase_chars(arg1);
        break;
    case 'S': /* Scroll Up */
        term_scroll_up(arg1);
        break;
    case 'T': /* Scroll Down */
        term_scroll_down(arg1);
        break;
    case 'r': /* Set Scrolling Region (DECSTBM) */
        g_term.top_margin = (arg1 > 0) ? (arg1 - 1) : 0;
        g_term.bottom_margin = (g_term.csi_argc > 1 && g_term.csi_args[1] > 0) ? (g_term.csi_args[1] - 1) : (g_term.rows - 1);
        if (g_term.top_margin < 0) g_term.top_margin = 0;
        if (g_term.bottom_margin >= g_term.rows) g_term.bottom_margin = g_term.rows - 1;
        if (g_term.top_margin >= g_term.bottom_margin) {
            g_term.top_margin = 0;
            g_term.bottom_margin = g_term.rows - 1;
        }
        g_term.cursor_x = 0;
        g_term.cursor_y = 0;
        break;
    case 'm': /* SGR Select Graphic Rendition */
        term_handle_sgr();
        break;
    case 's': /* Save Cursor */
        g_term.saved_x = g_term.cursor_x;
        g_term.saved_y = g_term.cursor_y;
        g_term.saved_fg = g_term.cur_fg;
        g_term.saved_bg = g_term.cur_bg;
        g_term.saved_flags = g_term.cur_flags;
        break;
    case 'u': /* Restore Cursor */
        g_term.cursor_x = g_term.saved_x;
        g_term.cursor_y = g_term.saved_y;
        g_term.cur_fg = g_term.saved_fg;
        g_term.cur_bg = g_term.saved_bg;
        g_term.cur_flags = g_term.saved_flags;
        break;
    case 'n': /* Device Status Report */
        if (arg1 == 6) {
            /* Cursor Position Report (CPR) */
            char cpr[32];
            int len = snprintf(cpr, sizeof(cpr), "\033[%d;%dR", g_term.cursor_y + 1, g_term.cursor_x + 1);
            if (g_master_fd >= 0) write(g_master_fd, cpr, (size_t)len);
        } else if (arg1 == 5) {
            /* Terminal status OK */
            if (g_master_fd >= 0) write(g_master_fd, "\033[0n", 4);
        }
        break;
    case 'c': /* Primary Device Attributes */
        if (g_master_fd >= 0) write(g_master_fd, "\033[?1;2c", 7);
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
        } else if (c == '\n' || c == '\013' || c == '\014') {
            /* Line feed */
            g_term.cursor_x = 0;
            g_term.cursor_y++;
            if (g_term.cursor_y > g_term.bottom_margin) {
                g_term.cursor_y = g_term.bottom_margin;
                term_scroll_up(1);
            }
        } else if (c == '\b') {
            if (g_term.cursor_x > 0) {
                g_term.cursor_x--;
            }
        } else if ((unsigned char)c == 127) {
            /* DEL character ignored */
        } else if (c == '\t') {
            int next_tab = (g_term.cursor_x + 8) & ~7;
            if (next_tab >= g_term.cols)
                next_tab = g_term.cols - 1;
            g_term.cursor_x = next_tab;
        } else if (c == '\a') {
            /* Bell */
        } else if ((uint8_t)c >= 32 && (uint8_t)c != 127) {
            if (g_term.cursor_x >= g_term.cols) {
                if (g_term.auto_wrap) {
                    g_term.cursor_x = 0;
                    g_term.cursor_y++;
                    if (g_term.cursor_y > g_term.bottom_margin) {
                        g_term.cursor_y = g_term.bottom_margin;
                        term_scroll_up(1);
                    }
                } else {
                    g_term.cursor_x = g_term.cols - 1;
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
            g_term.osc_len = 0;
            g_term.osc_buf[0] = '\0';
        } else if (c == '(' || c == ')') {
            g_term.parser_state = STATE_CHARSET;
        } else if (c == '7') {
            /* DECSC */
            g_term.saved_x = g_term.cursor_x;
            g_term.saved_y = g_term.cursor_y;
            g_term.saved_fg = g_term.cur_fg;
            g_term.saved_bg = g_term.cur_bg;
            g_term.saved_flags = g_term.cur_flags;
            g_term.parser_state = STATE_NORMAL;
        } else if (c == '8') {
            /* DECRC */
            g_term.cursor_x = g_term.saved_x;
            g_term.cursor_y = g_term.saved_y;
            g_term.cur_fg = g_term.saved_fg;
            g_term.cur_bg = g_term.saved_bg;
            g_term.cur_flags = g_term.saved_flags;
            g_term.parser_state = STATE_NORMAL;
        } else if (c == 'M') {
            /* RI - Reverse Index */
            if (g_term.cursor_y <= g_term.top_margin) {
                term_scroll_down(1);
            } else {
                g_term.cursor_y--;
            }
            g_term.parser_state = STATE_NORMAL;
        } else if (c == 'D') {
            /* IND - Index */
            if (g_term.cursor_y >= g_term.bottom_margin) {
                term_scroll_up(1);
            } else {
                g_term.cursor_y++;
            }
            g_term.parser_state = STATE_NORMAL;
        } else if (c == 'E') {
            /* NEL - Next Line */
            g_term.cursor_x = 0;
            if (g_term.cursor_y >= g_term.bottom_margin) {
                term_scroll_up(1);
            } else {
                g_term.cursor_y++;
            }
            g_term.parser_state = STATE_NORMAL;
        } else if (c == 'c') {
            /* RIS - Full Reset */
            term_init();
            g_term.parser_state = STATE_NORMAL;
        } else {
            g_term.parser_state = STATE_NORMAL;
        }
    } else if (g_term.parser_state == STATE_CHARSET) {
        g_term.parser_state = STATE_NORMAL;
    } else if (g_term.parser_state == STATE_CSI) {
        if (c == '?') {
            g_term.csi_private = true;
        } else if (c >= '0' && c <= '9') {
            if (g_term.csi_argc == 0) {
                g_term.csi_argc = 1;
                g_term.csi_args[0] = 0;
            }
            g_term.csi_args[g_term.csi_argc - 1] = g_term.csi_args[g_term.csi_argc - 1] * 10 + (c - '0');
        } else if (c == ';') {
            if (g_term.csi_argc == 0) {
                g_term.csi_argc = 1;
                g_term.csi_args[0] = 0;
            }
            if (g_term.csi_argc < 16) {
                g_term.csi_args[g_term.csi_argc] = 0;
                g_term.csi_argc++;
            }
        } else {
            term_handle_csi(c);
            g_term.parser_state = STATE_NORMAL;
        }
    } else if (g_term.parser_state == STATE_OSC) {
        if (c == '\a' || c == '\033') {
            g_term.osc_buf[g_term.osc_len] = '\0';
            if ((g_term.osc_buf[0] == '0' || g_term.osc_buf[0] == '2') && g_term.osc_buf[1] == ';') {
                const char *new_title = &g_term.osc_buf[2];
                if (*new_title && g_dpy && g_win) {
                    XStoreName(g_dpy, g_win, new_title);
                }
            }
            g_term.parser_state = STATE_NORMAL;
        } else if (g_term.osc_len < (int)sizeof(g_term.osc_buf) - 1) {
            g_term.osc_buf[g_term.osc_len++] = c;
        }
    }
}

static void term_write_data(const char *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        term_put_char(buf[i]);
    }
    g_needs_redraw = true;
}

static const cell_t *term_get_render_row(int screen_row) {
    if (g_scroll_offset == 0 || g_term.alt_screen) {
        return g_term.grid[screen_row];
    }
    int total_lines = g_scrollback_count + g_term.rows;
    int target_idx = (total_lines - 1) - g_scroll_offset - (g_term.rows - 1 - screen_row);
    if (target_idx < 0) {
        return NULL;
    }
    if (target_idx < g_scrollback_count) {
        int sb_idx = (g_scrollback_head + target_idx) % SCROLLBACK_MAX;
        return g_scrollback[sb_idx].cells;
    }
    int grid_row = target_idx - g_scrollback_count;
    if (grid_row >= 0 && grid_row < g_term.rows) {
        return g_term.grid[grid_row];
    }
    return g_term.grid[screen_row];
}

static void render_screen(void) {
    if (!g_dpy || !g_win || !g_backbuffer)
        return;

    /* Fill background */
    XSetForeground(g_dpy, g_gc, COLOR_BG);
    XFillRectangle(g_dpy, g_backbuffer, g_gc, 0, 0, (unsigned int)g_win_w, (unsigned int)g_win_h);

    /* Render text cells */
    for (int r = 0; r < g_term.rows; r++) {
        const cell_t *row_cells = term_get_render_row(r);
        if (!row_cells) continue;

        int py = PAD_Y + r * g_char_h + g_char_ascent; /* Baseline */
        int col = 0;

        while (col < g_term.cols) {
            const cell_t *first = &row_cells[col];
            uint32_t fg = first->fg;
            uint32_t bg = first->bg;
            if (first->flags & ATTR_REVERSE) {
                uint32_t tmp = fg;
                fg = bg;
                bg = tmp;
            }

            char str[MAX_COLS + 1];
            int len = 0;
            int start_col = col;

            while (col < g_term.cols) {
                const cell_t *curr = &row_cells[col];
                uint32_t cfg = curr->fg;
                uint32_t cbg = curr->bg;
                if (curr->flags & ATTR_REVERSE) {
                    uint32_t tmp = cfg;
                    cfg = cbg;
                    cbg = tmp;
                }

                if (cfg != fg || cbg != bg)
                    break;

                uint8_t uch = (uint8_t)curr->ch;
                str[len++] = (uch >= 32 && uch != 127) ? (char)uch : ' ';
                col++;
            }
            str[len] = '\0';

            int px = PAD_X + start_col * g_char_w;

            /* Check if this run contains any non-space character */
            bool has_non_space = false;
            for (int k = 0; k < len; k++) {
                if (str[k] != ' ') {
                    has_non_space = true;
                    break;
                }
            }

            /* Draw background block if not default background */
            if (bg != COLOR_BG) {
                XSetForeground(g_dpy, g_gc, bg);
                XFillRectangle(g_dpy, g_backbuffer, g_gc, px, PAD_Y + r * g_char_h, (unsigned int)(len * g_char_w), (unsigned int)g_char_h);
            }

            /* Draw string */
            if (has_non_space) {
                XSetForeground(g_dpy, g_gc, fg);
                XDrawString(g_dpy, g_backbuffer, g_gc, px, py, str, len);
                if (first->flags & ATTR_UNDERLINE) {
                    XDrawLine(g_dpy, g_backbuffer, g_gc, px, py + 1, px + len * g_char_w - 1, py + 1);
                }
            }
        }
    }

    /* Render Cursor if viewing live screen (scroll_offset == 0) */
    if (g_scroll_offset == 0 && g_term.cursor_visible &&
        g_term.cursor_x >= 0 && g_term.cursor_x < g_term.cols &&
        g_term.cursor_y >= 0 && g_term.cursor_y < g_term.rows) {
        int cx = PAD_X + g_term.cursor_x * g_char_w;
        int cy = PAD_Y + g_term.cursor_y * g_char_h;

        if (g_has_focus) {
            /* Solid bright cursor block */
            XSetForeground(g_dpy, g_gc, COLOR_CURSOR);
            XFillRectangle(g_dpy, g_backbuffer, g_gc, cx, cy, (unsigned int)g_char_w, (unsigned int)g_char_h);

            /* Inverted cursor character */
            uint8_t ch = (uint8_t)g_term.grid[g_term.cursor_y][g_term.cursor_x].ch;
            if (ch >= 32 && ch != 127) {
                char s[2] = {(char)ch, '\0'};
                XSetForeground(g_dpy, g_gc, COLOR_CURSOR_TEXT);
                XDrawString(g_dpy, g_backbuffer, g_gc, cx, cy + g_char_ascent, s, 1);
            }
        } else {
            /* Hollow unfocused cursor frame */
            XSetForeground(g_dpy, g_gc, COLOR_CURSOR);
            XDrawRectangle(g_dpy, g_backbuffer, g_gc, cx, cy, (unsigned int)(g_char_w - 1), (unsigned int)(g_char_h - 1));
        }
    }

    /* If scrolled back in history, render subtle scroll indicator bar on right edge */
    if (g_scroll_offset > 0 && g_scrollback_count > 0) {
        int bar_h = (g_win_h - PAD_Y * 2) * g_term.rows / (g_scrollback_count + g_term.rows);
        if (bar_h < 12) bar_h = 12;
        int bar_y = PAD_Y + ((g_win_h - PAD_Y * 2 - bar_h) * (g_scrollback_count - g_scroll_offset)) / g_scrollback_count;
        XSetForeground(g_dpy, g_gc, COLOR_CURSOR);
        XFillRectangle(g_dpy, g_backbuffer, g_gc, g_win_w - 5, bar_y, 3, (unsigned int)bar_h);
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
        /* If user was viewing scrollback, jump back to current live screen on input */
        if (g_scroll_offset != 0) {
            g_scroll_offset = 0;
            g_needs_redraw = true;
        }
    }
}

static KeySym keycode_to_fallback_sym(unsigned int keycode, unsigned int state) {
    bool shift = (state & ShiftMask) != 0;
    bool caps = (state & LockMask) != 0;
    bool upper = shift ^ caps;
    unsigned int evcode = (keycode >= 8) ? (keycode - 8) : 0;
    switch (evcode) {
    case 1: return XK_Escape;
    case 2: return shift ? '!' : '1';
    case 3: return shift ? '@' : '2';
    case 4: return shift ? '#' : '3';
    case 5: return shift ? '$' : '4';
    case 6: return shift ? '%' : '5';
    case 7: return shift ? '^' : '6';
    case 8: return shift ? '&' : '7';
    case 9: return shift ? '*' : '8';
    case 10: return shift ? '(' : '9';
    case 11: return shift ? ')' : '0';
    case 12: return shift ? '_' : '-';
    case 13: return shift ? '+' : '=';
    case 14: return XK_BackSpace;
    case 15: return XK_Tab;
    case 16: return upper ? 'Q' : 'q';
    case 17: return upper ? 'W' : 'w';
    case 18: return upper ? 'E' : 'e';
    case 19: return upper ? 'R' : 'r';
    case 20: return upper ? 'T' : 't';
    case 21: return upper ? 'Y' : 'y';
    case 22: return upper ? 'U' : 'u';
    case 23: return upper ? 'I' : 'i';
    case 24: return upper ? 'O' : 'o';
    case 25: return upper ? 'P' : 'p';
    case 26: return shift ? '{' : '[';
    case 27: return shift ? '}' : ']';
    case 28: return XK_Return;
    case 30: return upper ? 'A' : 'a';
    case 31: return upper ? 'S' : 's';
    case 32: return upper ? 'D' : 'd';
    case 33: return upper ? 'F' : 'f';
    case 34: return upper ? 'G' : 'g';
    case 35: return upper ? 'H' : 'h';
    case 36: return upper ? 'J' : 'j';
    case 37: return upper ? 'K' : 'k';
    case 38: return upper ? 'L' : 'l';
    case 39: return shift ? ':' : ';';
    case 40: return shift ? '"' : '\'';
    case 41: return shift ? '~' : '`';
    case 43: return shift ? '|' : '\\';
    case 44: return upper ? 'Z' : 'z';
    case 45: return upper ? 'X' : 'x';
    case 46: return upper ? 'C' : 'c';
    case 47: return upper ? 'V' : 'v';
    case 48: return upper ? 'B' : 'b';
    case 49: return upper ? 'N' : 'n';
    case 50: return upper ? 'M' : 'm';
    case 51: return shift ? '<' : ',';
    case 52: return shift ? '>' : '.';
    case 53: return shift ? '?' : '/';
    case 57: return ' ';
    case 103: return XK_Up;
    case 108: return XK_Down;
    case 105: return XK_Left;
    case 106: return XK_Right;
    case 102: return XK_Home;
    case 107: return XK_End;
    case 104: return XK_Page_Up;
    case 109: return XK_Page_Down;
    case 110: return XK_Insert;
    case 111: return XK_Delete;
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
            XSetInputFocus(g_dpy, g_win, RevertToParent, CurrentTime);

            if (ev.xbutton.button == 4) {
                /* Mouse Wheel Up: scroll into history */
                if (g_scroll_offset < g_scrollback_count) {
                    g_scroll_offset += 3;
                    if (g_scroll_offset > g_scrollback_count) g_scroll_offset = g_scrollback_count;
                    g_needs_redraw = true;
                }
                break;
            } else if (ev.xbutton.button == 5) {
                /* Mouse Wheel Down: scroll toward bottom */
                if (g_scroll_offset > 0) {
                    g_scroll_offset -= 3;
                    if (g_scroll_offset < 0) g_scroll_offset = 0;
                    g_needs_redraw = true;
                }
                break;
            }

            /* Check if clicked in bottom-right resize grip or near right/bottom edges */
            if ((ev.xbutton.x >= g_win_w - 24 && ev.xbutton.y >= g_win_h - 24) ||
                (ev.xbutton.x >= g_win_w - 8) || (ev.xbutton.y >= g_win_h - 8)) {
                g_is_resizing = true;
                g_resize_start_mx = ev.xbutton.x_root;
                g_resize_start_my = ev.xbutton.y_root;
                g_orig_w = g_win_w;
                g_orig_h = g_win_h;
                XGrabPointer(g_dpy, g_win, False, ButtonReleaseMask | PointerMotionMask,
                             GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
            }
            g_needs_redraw = true;
            break;

        case ButtonRelease:
            if (g_is_resizing) {
                g_is_resizing = false;
                XUngrabPointer(g_dpy, CurrentTime);
                g_needs_redraw = true;
            }
            break;

        case MotionNotify:
            if (g_is_resizing) {
                int dx = ev.xmotion.x_root - g_resize_start_mx;
                int dy = ev.xmotion.y_root - g_resize_start_my;
                int nw = g_orig_w + dx;
                int nh = g_orig_h + dy;
                if (nw < MIN_COLS * g_char_w + PAD_X * 2)
                    nw = MIN_COLS * g_char_w + PAD_X * 2;
                if (nh < MIN_ROWS * g_char_h + PAD_Y * 2)
                    nh = MIN_ROWS * g_char_h + PAD_Y * 2;
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
                int nw = g_win_w + 80;
                int nh = g_win_h + 48;
                XResizeWindow(g_dpy, g_win, (unsigned int)nw, (unsigned int)nh);
                term_resize(nw, nh);
                break;
            } else if ((ev.xkey.state & ControlMask) &&
                       (sym == XK_minus || sym == XK_underscore || sym == XK_KP_Subtract)) {
                int nw = g_win_w - 80;
                int nh = g_win_h - 48;
                XResizeWindow(g_dpy, g_win, (unsigned int)nw, (unsigned int)nh);
                term_resize(nw, nh);
                break;
            } else if (sym == XK_F11) {
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
                    int rw = (g_saved_premax_w > 0) ? g_saved_premax_w : (DEFAULT_COLS * g_char_w + PAD_X * 2);
                    int rh = (g_saved_premax_h > 0) ? g_saved_premax_h : (DEFAULT_ROWS * g_char_h + PAD_Y * 2);
                    XResizeWindow(g_dpy, g_win, (unsigned int)rw, (unsigned int)rh);
                    term_resize(rw, rh);
                    g_is_maximized = false;
                }
                break;
            }

            if (sym == XK_Return || sym == XK_KP_Enter) {
                send_pty_input("\r", 1);
            } else if (sym == XK_BackSpace) {
                /* Standard Unix DEL for backspace in xterm / vt100 */
                send_pty_input("\177", 1);
            } else if (sym == XK_Tab) {
                send_pty_input("\t", 1);
            } else if (sym == XK_Escape) {
                send_pty_input("\033", 1);
            } else if (sym == XK_Up) {
                if (g_term.app_cursor) send_pty_input("\033OA", 3);
                else send_pty_input("\033[A", 3);
            } else if (sym == XK_Down) {
                if (g_term.app_cursor) send_pty_input("\033OB", 3);
                else send_pty_input("\033[B", 3);
            } else if (sym == XK_Right) {
                if (g_term.app_cursor) send_pty_input("\033OC", 3);
                else send_pty_input("\033[C", 3);
            } else if (sym == XK_Left) {
                if (g_term.app_cursor) send_pty_input("\033OD", 3);
                else send_pty_input("\033[D", 3);
            } else if (sym == XK_Home) {
                if (g_term.app_cursor) send_pty_input("\033OH", 3);
                else send_pty_input("\033[H", 3);
            } else if (sym == XK_End) {
                if (g_term.app_cursor) send_pty_input("\033OF", 3);
                else send_pty_input("\033[F", 3);
            } else if (sym == XK_Insert) {
                send_pty_input("\033[2~", 4);
            } else if (sym == XK_Delete) {
                send_pty_input("\033[3~", 4);
            } else if (sym == XK_Page_Up) {
                if (ev.xkey.state & ShiftMask) {
                    g_scroll_offset += g_term.rows / 2;
                    if (g_scroll_offset > g_scrollback_count) g_scroll_offset = g_scrollback_count;
                    g_needs_redraw = true;
                } else {
                    send_pty_input("\033[5~", 4);
                }
            } else if (sym == XK_Page_Down) {
                if (ev.xkey.state & ShiftMask) {
                    g_scroll_offset -= g_term.rows / 2;
                    if (g_scroll_offset < 0) g_scroll_offset = 0;
                    g_needs_redraw = true;
                } else {
                    send_pty_input("\033[6~", 4);
                }
            } else if (sym >= XK_F1 && sym <= XK_F12) {
                static const char *fkeys[] = {
                    "\033OP", "\033OQ", "\033OR", "\033OS",
                    "\033[15~", "\033[17~", "\033[18~", "\033[19~",
                    "\033[20~", "\033[21~", "\033[23~", "\033[24~"
                };
                int fidx = sym - XK_F1;
                send_pty_input(fkeys[fidx], strlen(fkeys[fidx]));
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

static int xerror_handler(Display *d, XErrorEvent *e) {
    (void)d;
    (void)e;
    return 0;
}

int main(int argc, char *argv[]) {
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

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

    /* 1. Connect to SzpontX11 Server FIRST before fork */
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
        fprintf(stderr, "szponterm: Cannot open display %s\n", disp_name);
        return 1;
    }

    /* Install safe X error handler */
    XSetErrorHandler(xerror_handler);

    /* 2. Query and configure optimal monospace font from X server */
    const char *font_names[] = {
        "fixed",
        "6x13",
        "-misc-fixed-medium-r-semicondensed--13-120-75-75-c-60-iso8859-1",
        "9x15",
        "8x16",
        NULL
    };
    for (int i = 0; font_names[i]; i++) {
        g_font_info = XLoadQueryFont(g_dpy, font_names[i]);
        if (g_font_info) break;
    }
    if (g_font_info) {
        if (g_font_info->max_bounds.width > 0)
            g_char_w = g_font_info->max_bounds.width;
        if (g_font_info->ascent + g_font_info->descent > 0) {
            g_char_h = g_font_info->ascent + g_font_info->descent;
            g_char_ascent = g_font_info->ascent;
        }
    }

    g_win_w = DEFAULT_COLS * g_char_w + PAD_X * 2;
    g_win_h = DEFAULT_ROWS * g_char_h + PAD_Y * 2;

    int screen = DefaultScreen(g_dpy);
    Window root = RootWindow(g_dpy, screen);

    term_init();

    int screen_w = DisplayWidth(g_dpy, screen);
    int screen_h = DisplayHeight(g_dpy, screen);
    pid_t my_pid = getpid();
    int win_x = 80 + ((int)(my_pid * 37) % 300);
    int win_y = 80 + ((int)(my_pid * 23) % 200);
    if (win_x + g_win_w > screen_w) win_x = 40;
    if (win_y + g_win_h > screen_h) win_y = 60;

    g_win = XCreateSimpleWindow(g_dpy, root, win_x, win_y,
                                (unsigned int)g_win_w, (unsigned int)g_win_h,
                                0, 0xFF38BDF8, COLOR_BG);

    /* Set Window Manager Size Hints for precise cell resizing */
    XSizeHints *hints = XAllocSizeHints();
    if (hints) {
        hints->flags = PSize | PMinSize | PResizeInc | PBaseSize;
        hints->width_inc = g_char_w;
        hints->height_inc = g_char_h;
        hints->base_width = PAD_X * 2;
        hints->base_height = PAD_Y * 2;
        hints->min_width = MIN_COLS * g_char_w + PAD_X * 2;
        hints->min_height = MIN_ROWS * g_char_h + PAD_Y * 2;
        hints->width = g_win_w;
        hints->height = g_win_h;
        XSetWMNormalHints(g_dpy, g_win, hints);
        XFree(hints);
    }

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
    if (g_font_info) {
        XSetFont(g_dpy, g_gc, g_font_info->fid);
    } else {
        XFontStruct *fs = XQueryFont(g_dpy, XGContextFromGC(g_gc));
        if (fs) {
            if (fs->max_bounds.width > 0)
                g_char_w = fs->max_bounds.width;
            if (fs->ascent + fs->descent > 0) {
                g_char_h = fs->ascent + fs->descent;
                g_char_ascent = fs->ascent;
            }
        }
    }
    g_backbuffer = XCreatePixmap(g_dpy, g_win, (unsigned int)g_win_w, (unsigned int)g_win_h, DefaultDepth(g_dpy, screen));

    XSelectInput(g_dpy, g_win,
                 ExposureMask | KeyPressMask | KeyReleaseMask | FocusChangeMask |
                 StructureNotifyMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask);
    XMapWindow(g_dpy, g_win);
    XFlush(g_dpy);

    /* 3. Initialize pseudo-terminal pair with POSIX compliant termios */
    struct termios tio;
    memset(&tio, 0, sizeof(tio));
    tio.c_iflag = ICRNL | IXON;
    tio.c_oflag = OPOST | ONLCR;
    tio.c_cflag = CS8 | CREAD;
    tio.c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK | IEXTEN;
    tio.c_cc[VINTR] = 0x03;
    tio.c_cc[VQUIT] = 0x1C;
    tio.c_cc[VERASE] = 0x7F;
    tio.c_cc[VKILL] = 0x15;
    tio.c_cc[VEOF] = 0x04;
    tio.c_cc[VSTART] = 0x11;
    tio.c_cc[VSTOP] = 0x13;
    tio.c_cc[VSUSP] = 0x1A;
    tio.c_cc[VTIME] = 0;
    tio.c_cc[VMIN] = 1;

    struct winsize ws;
    ws.ws_col = (unsigned short)g_term.cols;
    ws.ws_row = (unsigned short)g_term.rows;
    ws.ws_xpixel = (unsigned short)g_win_w;
    ws.ws_ypixel = (unsigned short)g_win_h;

    int slave_fd = -1;
    if (openpty(&g_master_fd, &slave_fd, NULL, &tio, &ws) < 0) {
        perror("openpty");
        return 1;
    }

    /* Prepare environment for child shell */
    char env_display[128];
    char env_term[64] = "TERM=xterm-256color";
    char env_colorterm[64] = "COLORTERM=truecolor";
    char env_path[256];
    char env_home[256];
    char env_user[128];
    char env_logname[128];
    char env_shell[256];
    char env_pwd[512];
    char env_lang[64] = "LANG=C";
    char env_lcall[64] = "LC_ALL=C";
    char env_lines[64];
    char env_columns[64];

    snprintf(env_display, sizeof(env_display), "DISPLAY=%s", disp_name);
    snprintf(env_path, sizeof(env_path), "PATH=%s", getenv("PATH") ? getenv("PATH") : "/bin:/usr/bin:/usr/tbin:/usr/local/bin");
    snprintf(env_home, sizeof(env_home), "HOME=%s", getenv("HOME") ? getenv("HOME") : "/root");
    snprintf(env_user, sizeof(env_user), "USER=%s", getenv("USER") ? getenv("USER") : "root");
    snprintf(env_logname, sizeof(env_logname), "LOGNAME=%s",
             getenv("LOGNAME") ? getenv("LOGNAME") : (getenv("USER") ? getenv("USER") : "root"));
    snprintf(env_shell, sizeof(env_shell), "SHELL=%s", getenv("SHELL") ? getenv("SHELL") : "/bin/sh");

    char cwd_buf[256];
    if (getcwd(cwd_buf, sizeof(cwd_buf))) {
        snprintf(env_pwd, sizeof(env_pwd), "PWD=%s", cwd_buf);
    } else {
        snprintf(env_pwd, sizeof(env_pwd), "PWD=%s", getenv("PWD") ? getenv("PWD") : "/root");
    }

    snprintf(env_lines, sizeof(env_lines), "LINES=%d", g_term.rows);
    snprintf(env_columns, sizeof(env_columns), "COLUMNS=%d", g_term.cols);

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
            if (strncmp(environ[i], "DISPLAY=", 8) != 0 && strncmp(environ[i], "TERM=", 5) != 0 &&
                strncmp(environ[i], "COLORTERM=", 10) != 0 && strncmp(environ[i], "PATH=", 5) != 0 &&
                strncmp(environ[i], "HOME=", 5) != 0 && strncmp(environ[i], "USER=", 5) != 0 &&
                strncmp(environ[i], "LOGNAME=", 8) != 0 && strncmp(environ[i], "SHELL=", 6) != 0 &&
                strncmp(environ[i], "PWD=", 4) != 0 && strncmp(environ[i], "LANG=", 5) != 0 &&
                strncmp(environ[i], "LC_ALL=", 7) != 0 && strncmp(environ[i], "LINES=", 6) != 0 &&
                strncmp(environ[i], "COLUMNS=", 8) != 0) {
                child_envp[env_idx++] = environ[i];
            }
        }
    }
    child_envp[env_idx] = NULL;

    /* 3. Fork child process */
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
        tcsetpgrp(slave_fd, getpid());

        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);

        dup2(slave_fd, 0);
        dup2(slave_fd, 1);
        dup2(slave_fd, 2);
        if (slave_fd > 2)
            close(slave_fd);

        if (exec_cmd && *exec_cmd) {
            execve(exec_cmd[0], exec_cmd, child_envp);
            perror("execve custom command");
        } else {
            const char *shell_path = getenv("SHELL");
            if (!shell_path || !*shell_path)
                shell_path = "/bin/sh";
            char *sh_args[] = {(char *)shell_path, NULL};
            execve(shell_path, sh_args, child_envp);
            perror("execve shell");
        }
        _exit(127);
    }

    close(slave_fd);
    fcntl(g_master_fd, F_SETFL, O_NONBLOCK);

    struct pollfd pfds[2];
    pfds[0].fd = g_master_fd;
    pfds[0].events = POLLIN | POLLHUP | POLLERR;
    pfds[1].fd = ConnectionNumber(g_dpy);
    pfds[1].events = POLLIN;

    char read_buf[4096];

    while (g_running) {
        int ret = poll(pfds, 2, 16); /* ~60 FPS responsive updates */

        if (ret > 0) {
            /* 1. Process PTY shell output */
            if (pfds[0].revents & (POLLIN | POLLHUP | POLLERR)) {
                ssize_t n;
                while ((n = read(g_master_fd, read_buf, sizeof(read_buf))) > 0) {
                    term_write_data(read_buf, (size_t)n);
                }
                if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                    if (pfds[0].revents & (POLLHUP | POLLERR)) {
                        g_running = false;
                    }
                }
            }

            /* 2. Process X11 events */
            if (pfds[1].revents & POLLIN) {
                handle_x_events();
            }
        }

        /* Check for any pending X11 queue events */
        if (XPending(g_dpy)) {
            handle_x_events();
        }

        /* Check if child process died */
        int status = 0;
        pid_t wp = waitpid(g_child_pid, &status, WNOHANG);
        if (wp == g_child_pid) {
            g_running = false;
        }

        /* Redraw frame if dirty */
        if (g_needs_redraw) {
            render_screen();
            g_needs_redraw = false;
        }
    }

    /* Close master PTY first: triggers kernel hangup and sends SIGHUP to slave ctty */
    if (g_master_fd >= 0) {
        close(g_master_fd);
        g_master_fd = -1;
    }

    if (g_child_pid > 1) {
        /* Clean process group termination */
        kill(-g_child_pid, SIGHUP);
        kill(-g_child_pid, SIGTERM);
        kill(g_child_pid, SIGHUP);
        kill(g_child_pid, SIGTERM);

        for (int i = 0; i < 5; i++) {
            int status = 0;
            if (waitpid(g_child_pid, &status, WNOHANG) == g_child_pid) {
                g_child_pid = -1;
                break;
            }
            usleep(10000);
        }

        if (g_child_pid > 1) {
            kill(-g_child_pid, SIGKILL);
            kill(g_child_pid, SIGKILL);
            waitpid(g_child_pid, NULL, WNOHANG);
        }
    }

    if (g_backbuffer)
        XFreePixmap(g_dpy, g_backbuffer);
    if (g_gc)
        XFreeGC(g_dpy, g_gc);
    if (g_font_info)
        XFreeFont(g_dpy, g_font_info);
    if (g_win)
        XDestroyWindow(g_dpy, g_win);
    if (g_dpy)
        XCloseDisplay(g_dpy);

    return 0;
}
