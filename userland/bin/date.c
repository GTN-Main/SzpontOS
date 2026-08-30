/*
 * SzpontOS - /bin/date (Display Date & Time)
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    bool utc = false;
    bool rfc2822 = false;
    bool iso8601 = false;
    const char *format = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-u") == 0 || strcmp(argv[i], "--utc") == 0) {
            utc = true;
        } else if (strcmp(argv[i], "-R") == 0 || strcmp(argv[i], "--rfc-2822") == 0) {
            rfc2822 = true;
        } else if (strcmp(argv[i], "-I") == 0 || strcmp(argv[i], "--iso-8601") == 0) {
            iso8601 = true;
        } else if (argv[i][0] == '+') {
            format = argv[i] + 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: date [OPTION]... [+FORMAT]\n");
            printf("Display the current time in the given FORMAT.\n\n");
            printf("  -u, --utc       print Coordinated Universal Time (UTC)\n");
            printf("  -R, --rfc-2822  output date and time in RFC 2822 format\n");
            printf("  -I, --iso-8601  output date/time in ISO 8601 format\n");
            printf("  -h, --help      display this help and exit\n");
            return 0;
        }
    }

    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        ts.tv_sec = time(NULL);
        ts.tv_nsec = 0;
    }

    struct tm tm_buf;
    if (utc || 1) { /* Freestanding SzpontOS defaults to UTC */
        gmtime_r(&ts.tv_sec, &tm_buf);
    } else {
        localtime_r(&ts.tv_sec, &tm_buf);
    }

    if (format) {
        if (strcmp(format, "%s") == 0) {
            printf("%ld\n", (long)ts.tv_sec);
            return 0;
        }
        char out[256];
        strftime(out, sizeof(out), format, &tm_buf);
        printf("%s\n", out);
        return 0;
    }

    if (rfc2822) {
        static const char *WDAYS[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        static const char *MONTHS[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        printf("%s, %02d %s %04d %02d:%02d:%02d +0000\n",
               WDAYS[tm_buf.tm_wday % 7], tm_buf.tm_mday, MONTHS[tm_buf.tm_mon % 12],
               1900 + tm_buf.tm_year, tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);
        return 0;
    }

    if (iso8601) {
        printf("%04d-%02d-%02dT%02d:%02d:%02dZ\n",
               1900 + tm_buf.tm_year, tm_buf.tm_mon + 1, tm_buf.tm_mday,
               tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec);
        return 0;
    }

    char *str = asctime(&tm_buf);
    if (str) {
        /* asctime ends with '\n' */
        /* Insert UTC tag before the year */
        char buf[64];
        static const char *WDAYS[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        static const char *MONTHS[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        snprintf(buf, sizeof(buf), "%s %s %2d %02d:%02d:%02d UTC %04d\n",
                 WDAYS[tm_buf.tm_wday % 7], MONTHS[tm_buf.tm_mon % 12], tm_buf.tm_mday,
                 tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec, 1900 + tm_buf.tm_year);
        printf("%s", buf);
    }

    return 0;
}
