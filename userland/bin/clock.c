/*
 * SzpontOS - /bin/clock (Real-Time Precision Digital Clock & Uptime Monitor)
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/sysinfo.h>

static volatile bool g_running = true;

static void handle_sigint(int sig) {
    (void)sig;
    g_running = false;
}

/* 3-row ASCII art digits for big digital clock */
static const char *DIGITS_ROW1[10] = {
    " _ ", "   ", " _ ", " _ ", "   ", " _ ", " _ ", " _ ", " _ ", " _ "
};
static const char *DIGITS_ROW2[10] = {
    "| |", "  |", " _|", " _|", "|_|", "|_ ", "|_ ", "  |", "|_|", "|_|"
};
static const char *DIGITS_ROW3[10] = {
    "|_|", "  |", "|_ ", " _|", "  |", " _|", "|_|", "  |", "|_|", " _|"
};

static void print_big_clock(int hours, int mins, int secs, int msec) {
    int digits[6] = {
        hours / 10, hours % 10,
        mins / 10, mins % 10,
        secs / 10, secs % 10
    };

    /* Row 1 */
    printf("    \033[1;36m");
    for (int i = 0; i < 6; i++) {
        printf("%s ", DIGITS_ROW1[digits[i]]);
        if (i == 1 || i == 3) {
            printf("   ");
        }
    }
    printf("\033[0m\n");

    /* Row 2 */
    printf("    \033[1;36m");
    for (int i = 0; i < 6; i++) {
        printf("%s ", DIGITS_ROW2[digits[i]]);
        if (i == 1 || i == 3) {
            printf(" . ");
        }
    }
    printf("\033[0m\n");

    /* Row 3 */
    printf("    \033[1;36m");
    for (int i = 0; i < 6; i++) {
        printf("%s ", DIGITS_ROW3[digits[i]]);
        if (i == 1 || i == 3) {
            printf(" . ");
        }
    }
    printf(" \033[1;33m.%03d\033[0m\n", msec);
}

int main(int argc, char *argv[]) {
    bool line_mode = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--line") == 0) {
            line_mode = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: clock [options]\n");
            printf("Real-time live digital clock and precision uptime monitor.\n\n");
            printf("Options:\n");
            printf("  -l, --line    Display single-line compact continuous output\n");
            printf("  -h, --help    Show this help message\n");
            return 0;
        }
    }

    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    /* Hide cursor and clear screen if full-screen mode */
    if (!line_mode) {
        printf("\033[?25l\033[H\033[2J");
        fflush(stdout);
    }

    static const char *DAY_NAMES[7] = {
        "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
    };
    static const char *MONTH_NAMES[12] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };

    while (g_running) {
        struct timespec ts_real, ts_mono;
        if (clock_gettime(CLOCK_REALTIME, &ts_real) != 0) {
            ts_real.tv_sec = time(NULL);
            ts_real.tv_nsec = 0;
        }
        if (clock_gettime(CLOCK_MONOTONIC, &ts_mono) != 0) {
            ts_mono.tv_sec = 0;
            ts_mono.tv_nsec = 0;
        }

        struct tm tm_buf;
        gmtime_r(&ts_real.tv_sec, &tm_buf);

        int hours = tm_buf.tm_hour;
        int mins = tm_buf.tm_min;
        int secs = tm_buf.tm_sec;
        int msec = (int)(ts_real.tv_nsec / 1000000);
        int usec = (int)((ts_real.tv_nsec % 1000000) / 1000);

        long up_sec = ts_mono.tv_sec;
        int up_days = (int)(up_sec / 86400);
        int up_hours = (int)((up_sec % 86400) / 3600);
        int up_mins = (int)((up_sec % 3600) / 60);
        int up_secs = (int)(up_sec % 60);
        int up_msec = (int)(ts_mono.tv_nsec / 1000000);

        const char *wday_name = (tm_buf.tm_wday >= 0 && tm_buf.tm_wday < 7) ? DAY_NAMES[tm_buf.tm_wday] : "";
        const char *mon_name = (tm_buf.tm_mon >= 0 && tm_buf.tm_mon < 12) ? MONTH_NAMES[tm_buf.tm_mon] : "";

        if (line_mode) {
            printf("\r\033[K\033[1;32m%04d-%02d-%02d\033[0m \033[1;36m%02d:%02d:%02d\033[1;33m.%03d\033[0m UTC | Uptime: \033[1;35m%02d:%02d:%02d.%03d\033[0m",
                   1900 + tm_buf.tm_year, tm_buf.tm_mon + 1, tm_buf.tm_mday,
                   hours, mins, secs, msec,
                   up_hours, up_mins, up_secs, up_msec);
            fflush(stdout);
        } else {
            printf("\033[H");
            printf("\033[1;37;44m =============== SzpontOS Precision Real-Time Clock =============== \033[0m\n\n");

            print_big_clock(hours, mins, secs, msec);

            printf("\n");
            printf("  \033[1;33mDate:\033[0m      \033[1;37m%s, %02d %s %04d\033[0m\n",
                   wday_name, tm_buf.tm_mday, mon_name, 1900 + tm_buf.tm_year);
            printf("  \033[1;33mTime (UTC):\033[0m \033[1;32m%02d:%02d:%02d.%03d.%03d\033[0m\n",
                   hours, mins, secs, msec, usec);
            printf("  \033[1;33mUnix Epoch:\033[0m \033[1;36m%ld.%03d s\033[0m\n",
                   (long)ts_real.tv_sec, msec);
            printf("  \033[1;33mSystem Up:\033[0m  \033[1;35m%d days, %02d:%02d:%02d.%03d\033[0m\n\n",
                   up_days, up_hours, up_mins, up_secs, up_msec);

            printf("\033[2m  Press Ctrl+C to exit.\033[0m\n");
            fflush(stdout);
        }

        /* Refresh at ~20 Hz (50ms interval) for smooth millisecond updates */
        usleep(50000);
    }

    if (!line_mode) {
        /* Show cursor again and move to new line */
        printf("\033[?25h\n");
        fflush(stdout);
    } else {
        printf("\n");
    }

    return 0;
}
