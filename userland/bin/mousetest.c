/*
 * SzpontOS - Interactive Mouse Hardware Diagnostic & Test Utility
 * Visual ANSI TUI displaying live coordinates, deltas, buttons, and canvas.
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <termios.h>

#define CLS "\033[2J\033[H"
#define RESET "\033[0m"
#define BOLD "\033[1m"
#define HIDE_CURSOR "\033[?25l"
#define SHOW_CURSOR "\033[?25h"

#define FG_BLACK "\033[30m"
#define FG_RED "\033[31m"
#define FG_GREEN "\033[32m"
#define FG_YELLOW "\033[33m"
#define FG_BLUE "\033[34m"
#define FG_MAGENTA "\033[35m"
#define FG_CYAN "\033[36m"
#define FG_WHITE "\033[37m"

#define BG_BLUE "\033[44m"
#define BG_GREEN "\033[42m"
#define BG_RED "\033[41m"
#define BG_YELLOW "\033[43m"
#define BG_WHITE "\033[47m"

#define MOUSE_BTN_LEFT   (1 << 0)
#define MOUSE_BTN_RIGHT  (1 << 1)
#define MOUSE_BTN_MIDDLE (1 << 2)

typedef struct {
    uint8_t buttons;
    int32_t dx;
    int32_t dy;
    int8_t dz;
    int32_t abs_x;
    int32_t abs_y;
    bool is_absolute;
} mouse_event_t;

typedef struct {
    uint8_t buttons;
    int32_t dx;
    int32_t dy;
    int8_t dz;
} ps2_mouse_packet_t;

static volatile bool g_running = true;
static struct termios g_orig_termios;
static bool g_termios_saved = false;

static void handle_sigint(int sig) {
    (void)sig;
    g_running = false;
}

static void restore_terminal(void) {
    printf("%s%s", SHOW_CURSOR, RESET);
    fflush(stdout);
    if (g_termios_saved) {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_termios);
    }
}

static void set_raw_mode(void) {
    if (tcgetattr(STDIN_FILENO, &g_orig_termios) == 0) {
        g_termios_saved = true;
        struct termios raw = g_orig_termios;
        raw.c_lflag &= ~(ECHO | ICANON);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    }
}

static void move_to(int row, int col) {
    printf("\033[%d;%dH", row, col);
}

static void draw_box(int top, int left, int width, int height, const char *title, const char *color) {
    move_to(top, left);
    printf("%s┌", color);
    for (int i = 0; i < width - 2; i++)
        printf("─");
    printf("┐%s", RESET);

    if (title && strlen(title) > 0) {
        move_to(top, left + 2);
        printf("%s┤ %s%s%s ├%s", color, BOLD, title, color, RESET);
    }

    for (int r = top + 1; r < top + height - 1; r++) {
        move_to(r, left);
        printf("%s│%s", color, RESET);
        move_to(r, left + width - 1);
        printf("%s│%s", color, RESET);
    }

    move_to(top + height - 1, left);
    printf("%s└", color);
    for (int i = 0; i < width - 2; i++)
        printf("─");
    printf("┘%s", RESET);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    const char *dev_path = "/dev/mouse";
    bool is_universal = true;

    int fd = open(dev_path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        dev_path = "/dev/psaux";
        is_universal = false;
        fd = open(dev_path, O_RDONLY | O_NONBLOCK);
    }

    if (fd < 0) {
        fprintf(stderr, "mousetest: Failed to open /dev/mouse or /dev/psaux: %s\n", strerror(errno));
        return 1;
    }

    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    atexit(restore_terminal);
    set_raw_mode();

    printf("%s%s", CLS, HIDE_CURSOR);

    int canvas_top = 3;
    int canvas_left = 3;
    int canvas_w = 40;
    int canvas_h = 16;

    int cur_x = canvas_w / 2;
    int cur_y = canvas_h / 2;
    int total_packets = 0;
    int wheel_pos = 0;

    draw_box(canvas_top, canvas_left, canvas_w, canvas_h, "Canvas (Movement Area)", FG_CYAN);
    draw_box(3, 46, 32, 16, "Mouse Status & Telemetry", FG_YELLOW);

    move_to(1, 3);
    printf("%s%s=== SzpontOS Mouse Diagnostics ===%s  (Device: %s%s%s, Press Ctrl+C to exit)",
           BOLD, FG_MAGENTA, RESET, FG_GREEN, dev_path, RESET);

    while (g_running) {
        uint8_t btn = 0;
        int32_t dx = 0;
        int32_t dy = 0;
        int8_t dz = 0;
        bool got_data = false;

        if (is_universal) {
            mouse_event_t ev;
            ssize_t n = read(fd, &ev, sizeof(ev));
            if (n == (ssize_t)sizeof(ev)) {
                btn = ev.buttons;
                dx = ev.dx;
                dy = ev.dy;
                dz = ev.dz;
                got_data = true;
            }
        } else {
            ps2_mouse_packet_t pkt;
            ssize_t n = read(fd, &pkt, sizeof(pkt));
            if (n == (ssize_t)sizeof(pkt)) {
                btn = pkt.buttons;
                dx = pkt.dx;
                dy = pkt.dy;
                dz = pkt.dz;
                got_data = true;
            }
        }

        if (got_data) {
            total_packets++;
            wheel_pos += dz;

            /* Clear previous cursor on canvas */
            move_to(canvas_top + cur_y, canvas_left + cur_x);
            printf("·");

            /* Update virtual position (inverted Y for screen coords) */
            cur_x += dx / 2;
            cur_y -= dy / 2;

            if (cur_x < 1) cur_x = 1;
            if (cur_x > canvas_w - 2) cur_x = canvas_w - 2;
            if (cur_y < 1) cur_y = 1;
            if (cur_y > canvas_h - 2) cur_y = canvas_h - 2;

            /* Draw new cursor */
            move_to(canvas_top + cur_y, canvas_left + cur_x);
            printf("%s%s█%s", BOLD, FG_GREEN, RESET);

            /* Telemetry Panel */
            move_to(5, 48);
            printf("Packets Read : %s%8d%s", FG_WHITE, total_packets, RESET);

            move_to(6, 48);
            printf("Cursor X, Y  : %s%4d, %4d%s", FG_CYAN, cur_x, cur_y, RESET);

            move_to(7, 48);
            printf("Delta dX, dY : %s%+4d, %+4d%s", FG_YELLOW, dx, dy, RESET);

            move_to(8, 48);
            printf("Scroll Wheel : %s%+4d (Pos: %d)%s   ", FG_MAGENTA, dz, wheel_pos, RESET);

            /* Buttons Indicators */
            move_to(11, 48);
            printf("Buttons State:");

            move_to(13, 48);
            if (btn & MOUSE_BTN_LEFT) {
                printf("%s%s[ LEFT ]%s ", BOLD, BG_GREEN, RESET);
            } else {
                printf("%s[ LEFT ]%s ", FG_BLACK, RESET);
            }

            if (btn & MOUSE_BTN_MIDDLE) {
                printf("%s%s[ MID ]%s ", BOLD, BG_YELLOW, RESET);
            } else {
                printf("%s[ MID ]%s ", FG_BLACK, RESET);
            }

            if (btn & MOUSE_BTN_RIGHT) {
                printf("%s%s[ RIGHT ]%s", BOLD, BG_RED, RESET);
            } else {
                printf("%s[ RIGHT ]%s", FG_BLACK, RESET);
            }

            fflush(stdout);
        }

        usleep(10000); /* 10ms refresh */
    }

    close(fd);
    printf("%s%s", CLS, RESET);
    printf("mousetest: Diagnostic finished. Total packets processed: %d\n", total_packets);
    return 0;
}
