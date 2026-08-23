/*
 * SzpontOS - rm (Remove files or directories)
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

static int g_recursive = 0;
static int g_force = 0;
static int g_verbose = 0;
static int g_dir = 0;

static int remove_path(const char *path);

static int remove_directory_contents(const char *dir_path) {
    DIR *d = opendir(dir_path);
    if (!d) {
        if (!g_force) {
            fprintf(stderr, "rm: cannot open directory '%s'\n", dir_path);
        }
        return -1;
    }

    struct dirent *dent;
    int err = 0;

    while ((dent = readdir(d)) != NULL) {
        if (strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0) {
            continue;
        }

        char sub_path[512];
        if (strcmp(dir_path, "/") == 0) {
            snprintf(sub_path, sizeof(sub_path), "/%s", dent->d_name);
        } else {
            snprintf(sub_path, sizeof(sub_path), "%s/%s", dir_path, dent->d_name);
        }

        if (remove_path(sub_path) != 0) {
            err = -1;
        }
    }

    closedir(d);
    return err;
}

static int remove_path(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        if (g_force) return 0;
        fprintf(stderr, "rm: cannot remove '%s': No such file or directory\n", path);
        return -1;
    }

    if (S_ISDIR(st.st_mode)) {
        if (!g_recursive && !g_dir) {
            fprintf(stderr, "rm: cannot remove '%s': Is a directory\n", path);
            return -1;
        }

        if (g_recursive) {
            if (remove_directory_contents(path) != 0 && !g_force) {
                return -1;
            }
        }

        if (unlink(path) != 0) {
            if (!g_force) {
                fprintf(stderr, "rm: cannot remove directory '%s'\n", path);
                return -1;
            }
        } else {
            if (g_verbose) {
                printf("removed directory '%s'\n", path);
            }
        }
        return 0;
    }

    /* Regular file / device / pipe */
    if (unlink(path) != 0) {
        if (!g_force) {
            fprintf(stderr, "rm: cannot remove '%s'\n", path);
            return -1;
        }
    } else {
        if (g_verbose) {
            printf("removed '%s'\n", path);
        }
    }

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s [-f] [-r|-R] [-v] [-d] <file...>\n", argv[0]);
        return 1;
    }

    int file_count = 0;
    int has_error = 0;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            const char *flag = argv[i] + 1;
            if (strcmp(flag, "-recursive") == 0) {
                g_recursive = 1;
                continue;
            }
            if (strcmp(flag, "-force") == 0) {
                g_force = 1;
                continue;
            }
            if (strcmp(flag, "-verbose") == 0) {
                g_verbose = 1;
                continue;
            }

            while (*flag) {
                if (*flag == 'r' || *flag == 'R') g_recursive = 1;
                else if (*flag == 'f') g_force = 1;
                else if (*flag == 'v') g_verbose = 1;
                else if (*flag == 'd') g_dir = 1;
                else {
                    fprintf(stderr, "rm: invalid option -- '%c'\n", *flag);
                    return 1;
                }
                flag++;
            }
        } else {
            file_count++;
            if (remove_path(argv[i]) != 0) {
                has_error = 1;
            }
        }
    }

    if (file_count == 0 && !g_force) {
        fprintf(stderr, "rm: missing operand\n");
        return 1;
    }

    return has_error ? 1 : 0;
}
