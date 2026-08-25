/*
 * SzpontOS - ls (List directory contents)
 * Inspired by FreeBSD /bin/ls
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

/* ANSI Colors */
#define C_RESET "\033[0m"
#define C_BOLD "\033[1m"
#define C_BLUE "\033[1;34m"
#define C_GREEN "\033[1;32m"
#define C_CYAN "\033[1;36m"
#define C_RED "\033[1;31m"
#define C_YELLOW "\033[1;33m"
#define C_MAGENTA "\033[1;35m"
#define C_WHITE "\033[1;37m"
#define C_GRAY "\033[0;90m"

static void format_mode(mode_t mode, char *buf) {
    buf[0] = S_ISDIR(mode) ? 'd' : (S_ISLNK(mode) ? 'l' : '-');
    buf[1] = (mode & S_IRUSR) ? 'r' : '-';
    buf[2] = (mode & S_IWUSR) ? 'w' : '-';
    buf[3] = (mode & S_IXUSR) ? 'x' : '-';
    buf[4] = (mode & S_IRGRP) ? 'r' : '-';
    buf[5] = (mode & S_IWGRP) ? 'w' : '-';
    buf[6] = (mode & S_IXGRP) ? 'x' : '-';
    buf[7] = (mode & S_IROTH) ? 'r' : '-';
    buf[8] = (mode & S_IWOTH) ? 'w' : '-';
    buf[9] = (mode & S_IXOTH) ? 'x' : '-';
    buf[10] = '\0';
}

static const char *color_for_entry(struct dirent *d, const char *fullpath) {
    if (d->d_type == DT_DIR)
        return C_BLUE;

    struct stat st;
    if (stat(fullpath, &st) == 0) {
        if (S_ISLNK(st.st_mode))
            return C_CYAN;
        if (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
            return C_GREEN;
    }

    /* Color by extension */
    const char *dot = strrchr(d->d_name, '.');
    if (dot) {
        if (strcmp(dot, ".c") == 0 || strcmp(dot, ".h") == 0)
            return C_YELLOW;
        if (strcmp(dot, ".so") == 0 || strcmp(dot, ".a") == 0)
            return C_MAGENTA;
        if (strcmp(dot, ".sko") == 0)
            return C_RED;
        if (strcmp(dot, ".txt") == 0 || strcmp(dot, ".md") == 0)
            return C_WHITE;
        if (strcmp(dot, ".conf") == 0 || strcmp(dot, ".jsonc") == 0)
            return C_GRAY;
    }
    return C_RESET;
}

static void format_size(off_t size, char *buf, size_t buflen, bool human) {
    if (!human) {
        snprintf(buf, buflen, "%7ld", (long)size);
        return;
    }
    if (size < 1024)
        snprintf(buf, buflen, "%4ldB", (long)size);
    else if (size < 1024 * 1024)
        snprintf(buf, buflen, "%4ldK", (long)(size / 1024));
    else
        snprintf(buf, buflen, "%4ldM", (long)(size / (1024 * 1024)));
}

static void ls_long(const char *path, bool show_all, bool human) {
    DIR *dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "ls: cannot access '%s': No such file or directory\n", path);
        return;
    }

    printf(C_BOLD "total" C_RESET "\n");

    struct dirent *d;
    while ((d = readdir(dir)) != NULL) {
        if (!show_all && d->d_name[0] == '.')
            continue;

        char fullpath[512];
        if (strcmp(path, "/") == 0)
            snprintf(fullpath, sizeof(fullpath), "/%s", d->d_name);
        else
            snprintf(fullpath, sizeof(fullpath), "%s/%s", path, d->d_name);

        struct stat st;
        if (stat(fullpath, &st) != 0) {
            memset(&st, 0, sizeof(st));
        }

        char modebuf[12];
        format_mode(st.st_mode, modebuf);

        char sizebuf[16];
        format_size(st.st_size, sizebuf, sizeof(sizebuf), human);

        const char *color = color_for_entry(d, fullpath);
        const char *suffix = (d->d_type == DT_DIR) ? "/" : "";

        printf("%s %s %s%s%s%s\n", modebuf, sizebuf, color, d->d_name, suffix, C_RESET);
    }
    closedir(dir);
}

static void ls_short(const char *path, bool show_all) {
    DIR *dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "ls: cannot access '%s': No such file or directory\n", path);
        return;
    }

    struct dirent *d;
    while ((d = readdir(dir)) != NULL) {
        if (!show_all && d->d_name[0] == '.')
            continue;

        char fullpath[512];
        if (strcmp(path, "/") == 0)
            snprintf(fullpath, sizeof(fullpath), "/%s", d->d_name);
        else
            snprintf(fullpath, sizeof(fullpath), "%s/%s", path, d->d_name);

        const char *color = color_for_entry(d, fullpath);
        const char *suffix = (d->d_type == DT_DIR) ? "/" : "";

        printf("%s%s%s%s  ", color, d->d_name, suffix, C_RESET);
    }
    printf("\n");
    closedir(dir);
}

static void usage(void) {
    fprintf(stderr, "Usage: ls [-lahR] [directory...]\n");
}

int main(int argc, char *argv[]) {
    bool opt_long = false;
    bool opt_all = false;
    bool opt_human = false;
    const char *paths[32];
    int path_count = 0;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            for (int j = 1; argv[i][j]; j++) {
                switch (argv[i][j]) {
                case 'l':
                    opt_long = true;
                    break;
                case 'a':
                    opt_all = true;
                    break;
                case 'h':
                    opt_human = true;
                    break;
                case '-':
                    if (strcmp(argv[i], "--help") == 0) {
                        usage();
                        return 0;
                    }
                    break;
                default:
                    fprintf(stderr, "ls: invalid option -- '%c'\n", argv[i][j]);
                    usage();
                    return 1;
                }
            }
        } else {
            if (path_count < 32)
                paths[path_count++] = argv[i];
        }
    }

    if (path_count == 0) {
        paths[0] = ".";
        path_count = 1;
    }

    for (int i = 0; i < path_count; i++) {
        if (path_count > 1)
            printf("%s%s:%s\n", C_BOLD, paths[i], C_RESET);

        if (opt_long)
            ls_long(paths[i], opt_all, opt_human);
        else
            ls_short(paths[i], opt_all);

        if (i < path_count - 1)
            printf("\n");
    }

    return 0;
}
