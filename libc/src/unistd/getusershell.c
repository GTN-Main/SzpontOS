/*
 * SzpontOS — Freestanding POSIX C Library
 * (C) Copyright by Szpont Industries. All rights reserved.
 *
 * getusershell(), setusershell(), endusershell() implementation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static FILE *g_shells_fp = NULL;
static char g_shell_line[256];
static int g_default_shell_idx = 0;
static const char *const g_default_shells[] = {
    "/bin/sh",
    "/bin/zsh",
    NULL
};

void setusershell(void) {
    if (g_shells_fp) {
        rewind(g_shells_fp);
    } else {
        g_shells_fp = fopen("/etc/shells", "r");
    }
    g_default_shell_idx = 0;
}

void endusershell(void) {
    if (g_shells_fp) {
        fclose(g_shells_fp);
        g_shells_fp = NULL;
    }
    g_default_shell_idx = 0;
}

char *getusershell(void) {
    if (!g_shells_fp) {
        g_shells_fp = fopen("/etc/shells", "r");
    }

    if (g_shells_fp) {
        while (fgets(g_shell_line, sizeof(g_shell_line), g_shells_fp)) {
            char *p = g_shell_line;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '#' || *p == '\n' || *p == '\0') {
                continue;
            }
            char *nl = strchr(p, '\n');
            if (nl) *nl = '\0';
            char *cr = strchr(p, '\r');
            if (cr) *cr = '\0';
            /* Strip trailing spaces */
            size_t len = strlen(p);
            while (len > 0 && (p[len - 1] == ' ' || p[len - 1] == '\t')) {
                p[--len] = '\0';
            }
            if (len > 0) {
                return p;
            }
        }
        /* Reached EOF in /etc/shells */
        return NULL;
    }

    /* Fallback if /etc/shells does not exist */
    if (g_default_shells[g_default_shell_idx]) {
        return (char *)g_default_shells[g_default_shell_idx++];
    }
    return NULL;
}
