/*
 * SzpontOS - POSIX find (File hierarchy search utility)
 * Inspired by FreeBSD usr.bin/find/
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fnmatch.h>

static void find_recurse(const char *path, const char *name_pattern, char type_filter) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return;
    }

    bool matches = true;

    /* Check type filter */
    if (type_filter == 'f' && !S_ISREG(st.st_mode)) matches = false;
    else if (type_filter == 'd' && !S_ISDIR(st.st_mode)) matches = false;

    /* Check name pattern */
    if (name_pattern && matches) {
        const char *basename = strrchr(path, '/');
        if (basename) basename++;
        else basename = path;

        if (fnmatch(name_pattern, basename, 0) != 0) {
            matches = false;
        }
    }

    if (matches) {
        printf("%s\n", path);
    }

    if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(path);
        if (!d) return;

        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
                continue;
            }

            char subpath[512];
            if (strcmp(path, "/") == 0) {
                snprintf(subpath, sizeof(subpath), "/%s", ent->d_name);
            } else {
                snprintf(subpath, sizeof(subpath), "%s/%s", path, ent->d_name);
            }

            find_recurse(subpath, name_pattern, type_filter);
        }
        closedir(d);
    }
}

int main(int argc, char *argv[]) {
    const char *start_path = ".";
    const char *name_pattern = NULL;
    char type_filter = 0;

    int idx = 1;
    if (idx < argc && argv[idx][0] != '-') {
        start_path = argv[idx++];
    }

    while (idx < argc) {
        if (strcmp(argv[idx], "-name") == 0 && idx + 1 < argc) {
            name_pattern = argv[++idx];
        } else if (strcmp(argv[idx], "-type") == 0 && idx + 1 < argc) {
            type_filter = argv[++idx][0];
        } else if (strcmp(argv[idx], "-print") == 0) {
            /* Default action */
        } else {
            fprintf(stderr, "find: unknown predicate '%s'\n", argv[idx]);
            return 1;
        }
        idx++;
    }

    find_recurse(start_path, name_pattern, type_filter);
    return 0;
}
