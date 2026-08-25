/*
 * SzpontOS Libc - POSIX glob implementation with directory scanning & fnmatch
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <glob.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <fnmatch.h>
#include <sys/stat.h>

static void glob_add_path(glob_t *pglob, const char *path) {
    char **new_pathv = (char **)realloc(pglob->gl_pathv, sizeof(char *) * (pglob->gl_pathc + 2));
    if (!new_pathv)
        return;

    pglob->gl_pathv = new_pathv;
    pglob->gl_pathv[pglob->gl_pathc] = strdup(path);
    pglob->gl_pathc++;
    pglob->gl_pathv[pglob->gl_pathc] = NULL;
}

int glob(const char *pattern, int flags, int (*errfunc)(const char *epath, int eerrno), glob_t *pglob) {
    (void)errfunc;
    if (!pattern || !pglob)
        return GLOB_ABORTED;

    pglob->gl_pathc = 0;
    pglob->gl_pathv = NULL;

    /* Check if pattern contains wildcards */
    if (!strchr(pattern, '*') && !strchr(pattern, '?') && !strchr(pattern, '[')) {
        struct stat st;
        if (stat(pattern, &st) == 0) {
            glob_add_path(pglob, pattern);
            return 0;
        }
        if (flags & GLOB_NOCHECK) {
            glob_add_path(pglob, pattern);
            return 0;
        }
        return GLOB_NOMATCH;
    }

    /* Extract directory part and filename pattern */
    char dir_path[256];
    const char *file_pattern = strrchr(pattern, '/');

    if (file_pattern) {
        size_t dirlen = (size_t)(file_pattern - pattern);
        if (dirlen == 0) {
            strcpy(dir_path, "/");
        } else {
            strncpy(dir_path, pattern, dirlen);
            dir_path[dirlen] = '\0';
        }
        file_pattern++; /* skip '/' */
    } else {
        strcpy(dir_path, ".");
        file_pattern = pattern;
    }

    DIR *d = opendir(dir_path);
    if (!d) {
        if (flags & GLOB_NOCHECK) {
            glob_add_path(pglob, pattern);
            return 0;
        }
        return GLOB_NOMATCH;
    }

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.' && file_pattern[0] != '.') {
            continue; /* Skip hidden files unless pattern starts with '.' */
        }
        if (fnmatch(file_pattern, de->d_name, 0) == 0) {
            char full_path[512];
            if (strcmp(dir_path, ".") == 0) {
                snprintf(full_path, sizeof(full_path), "%s", de->d_name);
            } else if (strcmp(dir_path, "/") == 0) {
                snprintf(full_path, sizeof(full_path), "/%s", de->d_name);
            } else {
                snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, de->d_name);
            }
            glob_add_path(pglob, full_path);
        }
    }
    closedir(d);

    if (pglob->gl_pathc == 0) {
        if (flags & GLOB_NOCHECK) {
            glob_add_path(pglob, pattern);
            return 0;
        }
        return GLOB_NOMATCH;
    }

    return 0;
}

void globfree(glob_t *pglob) {
    if (!pglob || !pglob->gl_pathv)
        return;
    for (size_t i = 0; i < pglob->gl_pathc; i++) {
        if (pglob->gl_pathv[i]) {
            free(pglob->gl_pathv[i]);
        }
    }
    free(pglob->gl_pathv);
    pglob->gl_pathv = NULL;
    pglob->gl_pathc = 0;
}
