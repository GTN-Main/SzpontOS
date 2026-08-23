/*
 * SzpontOS - TUI & ANSI/VT100 Terminal Rendering Demonstration
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

/* ANSI Escape Code Helpers */
#define CLS          "\033[2J\033[H"
#define RESET        "\033[0m"
#define BOLD         "\033[1m"
#define UNDERLINE    "\033[4m"
#define REVERSE      "\033[7m"
#define HIDE_CURSOR  "\033[?25l"
#define SHOW_CURSOR  "\033[?25h"

#define FG_BLACK     "\033[30m"
#define FG_RED       "\033[31m"
#define FG_GREEN     "\033[32m"
#define FG_YELLOW    "\033[33m"
#define FG_BLUE      "\033[34m"
#define FG_MAGENTA   "\033[35m"
#define FG_CYAN      "\033[36m"
#define FG_WHITE     "\033[37m"

static void move_to(int row, int col) {
    printf("\033[%d;%dH", row, col);
}

static void draw_box(int top, int left, int width, int height, const char *title, const char *color) {
    char hline[256];
    int inner_w = width - 2;
    if (inner_w < 0) inner_w = 0;
    if (inner_w > 70) inner_w = 70;

    /* Top border */
    move_to(top, left);
    printf("%s┌", color);
    for (int i = 0; i < inner_w; i++) printf("─");
    printf("┐%s", RESET);

    if (title && strlen(title) > 0) {
        move_to(top, left + 2);
        printf("%s┤ %s%s%s ├%s", color, BOLD, title, color, RESET);
    }

    /* Side borders */
    for (int r = top + 1; r < top + height - 1; r++) {
        move_to(r, left);
        printf("%s│%s", color, RESET);
        move_to(r, left + width - 1);
        printf("%s│%s", color, RESET);
    }

    /* Bottom border */
    move_to(top + height - 1, left);
    printf("%s└", color);
    for (int i = 0; i < inner_w; i++) printf("─");
    printf("┘%s", RESET);
}

static void draw_double_box(int top, int left, int width, int height, const char *title, const char *color) {
    int inner_w = width - 2;
    if (inner_w < 0) inner_w = 0;
    if (inner_w > 70) inner_w = 70;

    /* Top border */
    move_to(top, left);
    printf("%s╔", color);
    for (int i = 0; i < inner_w; i++) printf("═");
    printf("╗%s", RESET);

    if (title && strlen(title) > 0) {
        move_to(top, left + 2);
        printf("%s╣ %s%s%s ╠%s", color, BOLD, title, color, RESET);
    }

    /* Side borders */
    for (int r = top + 1; r < top + height - 1; r++) {
        move_to(r, left);
        printf("%s║%s", color, RESET);
        move_to(r, left + width - 1);
        printf("%s║%s", color, RESET);
    }

    /* Bottom border */
    move_to(top + height - 1, left);
    printf("%s╚", color);
    for (int i = 0; i < inner_w; i++) printf("═");
    printf("╝%s", RESET);
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    struct winsize ws;
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) < 0) {
        ws.ws_col = 80;
        ws.ws_row = 24;
    }

    int cols = (ws.ws_col > 0) ? ws.ws_col : 80;
    int rows = (ws.ws_row > 0) ? ws.ws_row : 24;

    printf(HIDE_CURSOR);
    printf(CLS);

    /* 1. Header Bar */
    move_to(1, 1);
    printf(REVERSE FG_CYAN);
    for (int i = 0; i < cols; i++) putchar(' ');
    move_to(1, 2);
    printf("%s SzpontOS TUI System Dashboard - Framebuffer & Virtio Terminal Test%s", BOLD, RESET);

    /* 2. System Overview Box */
    draw_double_box(3, 3, 36, 9, "System Info", FG_GREEN);
    move_to(4, 5); printf("%sOS:%s       SzpontOS v0.1.0", BOLD, RESET);
    move_to(5, 5); printf("%sArch:%s     x86_64 Higher-Half", BOLD, RESET);
    move_to(6, 5); printf("%sTerminal:%s Framebuffer Console", BOLD, RESET);
    move_to(7, 5); printf("%sGeometry:%s %d cols x %d rows", BOLD, RESET, cols, rows);
    move_to(8, 5); printf("%sResolution:%s%dx%d px", BOLD, RESET, ws.ws_xpixel, ws.ws_ypixel);
    move_to(9, 5); printf("%sDriver:%s   Virtio-VGA / Limine FB", BOLD, RESET);
    move_to(10, 5); printf("%sStatus:%s   %s● OK (Active)%s", BOLD, RESET, FG_GREEN, RESET);

    /* 3. Color Palette Test Box */
    draw_box(3, 42, 35, 9, "ANSI 16 Colors", FG_YELLOW);
    move_to(5, 44);
    printf("Normal: %s██%s%s██%s%s██%s%s██%s%s██%s%s██%s%s██%s%s██%s",
           FG_BLACK, RESET, FG_RED, RESET, FG_GREEN, RESET, FG_YELLOW, RESET,
           FG_BLUE, RESET, FG_MAGENTA, RESET, FG_CYAN, RESET, FG_WHITE, RESET);
    move_to(7, 44);
    printf("Bright: \033[90m██\033[0m\033[91m██\033[0m\033[92m██\033[0m\033[93m██\033[0m\033[94m██\033[0m\033[95m██\033[0m\033[96m██\033[0m\033[97m██\033[0m");

    /* 4. Box Drawing & Line Styles Test */
    draw_box(13, 3, 36, 8, "Line & Border Styles", FG_MAGENTA);
    move_to(14, 5); printf("Single: ┌─┬─┐ │ ├─┼─┤ └─┴─┘");
    move_to(15, 5); printf("Double: ╔═╦═╗ ║ ╠═╬═╣ ╚═╩═╝");
    move_to(16, 5); printf("Blocks: █ ▀ ▄ ▌ ▐ ░ ▒ ▓ ■");
    move_to(17, 5); printf("Arrows: ↑ ↓ → ← • ° ± ≤ ≥");
    move_to(18, 5); printf("Styles: %sBold%s %sUnderline%s %sReverse%s", BOLD, RESET, UNDERLINE, RESET, REVERSE, RESET);

    /* 5. Interactive Dialog Simulation */
    draw_double_box(13, 42, 35, 8, "Dialog / Notification", FG_CYAN);
    move_to(15, 45); printf("%sTUI Rendering Verified!%s", BOLD, RESET);
    move_to(16, 45); printf("All ANSI & UTF-8 boxes OK.");
    move_to(18, 50); printf("%s[ OK / ENTER ]%s", REVERSE FG_WHITE, RESET);

    /* 6. Footer Status Line */
    move_to(rows > 23 ? 23 : rows, 1);
    printf(REVERSE FG_WHITE);
    for (int i = 0; i < cols; i++) putchar(' ');
    move_to(rows > 23 ? 23 : rows, 2);
    printf("%s^G Help   ^O Save   ^R Read   ^X Exit   |   Press any key to exit...%s\n", BOLD, RESET);

    move_to(rows > 24 ? 24 : rows, 1);
    printf(SHOW_CURSOR);
    printf("\n[SUCCESS] TUI Rendering test completed successfully!\n");
    fflush(stdout);
    return 0;
}
