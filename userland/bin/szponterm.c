/*
 * SzpontOS — szponterm Native X11 Terminal Emulator
 * (C) Copyright by Szpont Industries. All rights reserved.
 *
 * Fast, lightweight, 256-color & TrueColor ANSI/VT100 terminal emulator
 * written natively for SzpontOS and SzpontX11.
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

#define TERM_COLS 80
#define TERM_ROWS 25
#define CHAR_W    8
#define CHAR_H    16
#define PAD_X     10
#define PAD_Y     8

#define WIN_W     (TERM_COLS * CHAR_W + PAD_X * 2)
#define WIN_H     (TERM_ROWS * CHAR_H + PAD_Y * 2)

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
    cell_t   grid[TERM_ROWS][TERM_COLS];
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

static void term_reset_attributes(void) {
    g_term.cur_fg = COLOR_FG;
    g_term.cur_bg = COLOR_BG;
    g_term.cur_flags = 0;
}

static void term_clear_line(int row, int start_col, int end_col) {
    if (row < 0 || row >= TERM_ROWS) return;
    if (start_col < 0) start_col = 0;
    if (end_col > TERM_COLS) end_col = TERM_COLS;

    for (int c = start_col; c < end_col; c++) {
        g_term.grid[row][c].ch = ' ';
        g_term.grid[row][c].fg = g_term.cur_fg;
        g_term.grid[row][c].bg = g_term.cur_bg;
        g_term.grid[row][c].flags = 0;
    }
}

static void term_clear_all(void) {
    for (int r = 0; r < TERM_ROWS; r++) {
        term_clear_line(r, 0, TERM_COLS);
    }
}

static void term_scroll_up(void) {
    memmove(&g_term.grid[0], &g_term.grid[1], sizeof(cell_t) * TERM_COLS * (TERM_ROWS - 1));
    term_clear_line(TERM_ROWS - 1, 0, TERM_COLS);
}

static void term_init(void) {
    memset(&g_term, 0, sizeof(term_t));
    g_term.cursor_x = 0;
    g_term.cursor_y = 0;
    g_term.cursor_visible = true;
    term_reset_attributes();
    term_clear_all();
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
        if (g_term.cursor_y >= TERM_ROWS) g_term.cursor_y = TERM_ROWS - 1;
        break;
    case 'C': /* Cursor Forward */
        g_term.cursor_x += arg1;
        if (g_term.cursor_x >= TERM_COLS) g_term.cursor_x = TERM_COLS - 1;
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
        if (g_term.cursor_y >= TERM_ROWS) g_term.cursor_y = TERM_ROWS - 1;
        if (g_term.cursor_x < 0) g_term.cursor_x = 0;
        if (g_term.cursor_x >= TERM_COLS) g_term.cursor_x = TERM_COLS - 1;
        break;
    case 'J': /* Erase in Display */
        if (g_term.csi_argc == 0 || g_term.csi_args[0] == 0) {
            term_clear_line(g_term.cursor_y, g_term.cursor_x, TERM_COLS);
            for (int r = g_term.cursor_y + 1; r < TERM_ROWS; r++) {
                term_clear_line(r, 0, TERM_COLS);
            }
        } else if (g_term.csi_args[0] == 1) {
            for (int r = 0; r < g_term.cursor_y; r++) {
                term_clear_line(r, 0, TERM_COLS);
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
            term_clear_line(g_term.cursor_y, g_term.cursor_x, TERM_COLS);
        } else if (g_term.csi_args[0] == 1) {
            term_clear_line(g_term.cursor_y, 0, g_term.cursor_x + 1);
        } else if (g_term.csi_args[0] == 2) {
            term_clear_line(g_term.cursor_y, 0, TERM_COLS);
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
            if (g_term.cursor_y >= TERM_ROWS) {
                g_term.cursor_y = TERM_ROWS - 1;
                term_scroll_up();
            }
        } else if (c == '\b') {
            if (g_term.cursor_x > 0) {
                g_term.cursor_x--;
            }
        } else if (c == '\t') {
            int next_tab = (g_term.cursor_x + 8) & ~7;
            if (next_tab >= TERM_COLS) next_tab = TERM_COLS - 1;
            while (g_term.cursor_x < next_tab) {
                term_put_char(' ');
            }
        } else if (c == '\a') {
            /* Bell / Beep */
        } else if ((uint8_t)c >= 32) {
            if (g_term.cursor_x >= TERM_COLS) {
                g_term.cursor_x = 0;
                g_term.cursor_y++;
                if (g_term.cursor_y >= TERM_ROWS) {
                    g_term.cursor_y = TERM_ROWS - 1;
                    term_scroll_up();
                }
            }

            cell_t *cell = &g_term.grid[g_term.cursor_y][g_term.cursor_x];
            cell->ch = c;
            cell->fg = g_term.cur_fg;
            cell->bg = g_term.cur_bg;
            cell->flags = g_term.cur_flags;

            g_term.cursor_x++;
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
    if (!g_dpy || !g_win) return;

    /* Fill background */
    XSetForeground(g_dpy, g_gc, COLOR_BG);
    XFillRectangle(g_dpy, g_backbuffer, g_gc, 0, 0, WIN_W, WIN_H);

    /* Render text cells */
    for (int r = 0; r < TERM_ROWS; r++) {
        int py = PAD_Y + r * CHAR_H + 12; /* Baseline */
        int col = 0;

        while (col < TERM_COLS) {
            cell_t *first = &g_term.grid[r][col];
            uint32_t fg = first->fg;
            uint32_t bg = first->bg;
            if (first->flags & ATTR_REVERSE) {
                uint32_t tmp = fg; fg = bg; bg = tmp;
            }

            char str[TERM_COLS + 1];
            int len = 0;
            int start_col = col;

            while (col < TERM_COLS) {
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
    if (g_term.cursor_visible && g_term.cursor_x >= 0 && g_term.cursor_x < TERM_COLS &&
        g_term.cursor_y >= 0 && g_term.cursor_y < TERM_ROWS) {
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

    /* Flip double-buffered backbuffer to window */
    XCopyArea(g_dpy, g_backbuffer, g_win, g_gc, 0, 0, WIN_W, WIN_H, 0, 0);
    XFlush(g_dpy);
}

static void send_pty_input(const char *buf, size_t len) {
    if (g_master_fd >= 0 && buf && len > 0) {
        write(g_master_fd, buf, len);
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

        case FocusIn:
            g_has_focus = true;
            g_needs_redraw = true;
            break;

        case FocusOut:
            g_has_focus = false;
            g_needs_redraw = true;
            break;

        case KeyPress: {
            char buf[32];
            KeySym sym;
            int len = XLookupString(&ev.xkey, buf, sizeof(buf) - 1, &sym, NULL);

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
    (void)argc; (void)argv;

    term_init();

    /* Initialize pseudo-terminal pair */
    struct winsize ws;
    ws.ws_col = TERM_COLS;
    ws.ws_row = TERM_ROWS;
    ws.ws_xpixel = WIN_W;
    ws.ws_ypixel = WIN_H;

    int slave_fd = -1;
    if (openpty(&g_master_fd, &slave_fd, NULL, NULL, &ws) < 0) {
        perror("openpty");
        return 1;
    }

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

        char *envp[] = {
            "DISPLAY=:0",
            "TERM=xterm-256color",
            "COLORTERM=truecolor",
            "PATH=/bin:/usr/bin",
            "HOME=/root",
            "USER=root",
            "SHELL=/bin/sh",
            NULL
        };

        char *sh_args[] = {"/bin/sh", NULL};
        execve("/bin/sh", sh_args, envp);
        perror("execve /bin/sh");
        _exit(127);
    }

    close(slave_fd);
    fcntl(g_master_fd, F_SETFL, O_NONBLOCK);

    const char *disp_name = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-display") == 0 && i + 1 < argc) {
            disp_name = argv[i + 1];
            i++;
        }
    }
    if (!disp_name) {
        disp_name = getenv("DISPLAY");
    }
    if (!disp_name || !*disp_name) {
        disp_name = ":0";
    }

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

    g_win = XCreateSimpleWindow(g_dpy, root, 140, 90, WIN_W, WIN_H, 2, 0xFF3B82F6, COLOR_BG);
    XStoreName(g_dpy, g_win, "szponterm — SzpontOS Terminal (x86_64)");

    /* Set Window Manager Protocols */
    g_wm_delete = XInternAtom(g_dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(g_dpy, g_win, &g_wm_delete, 1);

    /* Allocate GC and Double-Buffered Backbuffer */
    g_gc = XCreateGC(g_dpy, g_win, 0, NULL);
    g_backbuffer = XCreatePixmap(g_dpy, g_win, WIN_W, WIN_H, DefaultDepth(g_dpy, screen));

    XSelectInput(g_dpy, g_win, ExposureMask | KeyPressMask | KeyReleaseMask | FocusChangeMask | StructureNotifyMask);
    XMapWindow(g_dpy, g_win);
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
