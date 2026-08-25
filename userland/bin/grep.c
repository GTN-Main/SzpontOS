/*
 * SzpontOS - POSIX grep (Pattern matching utility)
 * Inspired by FreeBSD usr.bin/grep/
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <regex.h>

static void usage(void) {
    fprintf(stderr, "Usage: grep [-ivnclE] pattern [file ...]\n");
    fprintf(stderr, "  -i  Ignore case distinctions\n");
    fprintf(stderr, "  -v  Invert match (select non-matching lines)\n");
    fprintf(stderr, "  -n  Print line number with output lines\n");
    fprintf(stderr, "  -c  Print only a count of selected lines\n");
    fprintf(stderr, "  -l  Print only names of files with matching lines\n");
    fprintf(stderr, "  -E  Interpret pattern as an extended regular expression\n");
}

static void grep_file(FILE *f, const char *filename, regex_t *preg, bool opt_v, bool opt_n, bool opt_c, bool opt_l,
                      bool multiple_files) {
    char line[4096];
    size_t line_num = 0;
    size_t match_count = 0;

    while (fgets(line, sizeof(line), f)) {
        line_num++;
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        int r = regexec(preg, line, 0, NULL, 0);
        bool match = (r == 0);
        if (opt_v)
            match = !match;

        if (match) {
            match_count++;
            if (opt_l) {
                printf("%s\n", filename);
                return;
            }
            if (!opt_c) {
                if (multiple_files) {
                    printf("\033[35m%s\033[0m:", filename);
                }
                if (opt_n) {
                    printf("\033[32m%zu\033[0m:", line_num);
                }
                printf("%s\n", line);
            }
        }
    }

    if (opt_c) {
        if (multiple_files) {
            printf("%s:%zu\n", filename, match_count);
        } else {
            printf("%zu\n", match_count);
        }
    }
}

int main(int argc, char *argv[]) {
    bool opt_i = false;
    bool opt_v = false;
    bool opt_n = false;
    bool opt_c = false;
    bool opt_l = false;
    bool opt_E = false;

    int opt_idx = 1;
    while (opt_idx < argc && argv[opt_idx][0] == '-' && argv[opt_idx][1] != '\0') {
        if (strcmp(argv[opt_idx], "--help") == 0) {
            usage();
            return 0;
        }
        for (int j = 1; argv[opt_idx][j]; j++) {
            switch (argv[opt_idx][j]) {
            case 'i':
                opt_i = true;
                break;
            case 'v':
                opt_v = true;
                break;
            case 'n':
                opt_n = true;
                break;
            case 'c':
                opt_c = true;
                break;
            case 'l':
                opt_l = true;
                break;
            case 'E':
                opt_E = true;
                break;
            default:
                fprintf(stderr, "grep: invalid option -- '%c'\n", argv[opt_idx][j]);
                usage();
                return 2;
            }
        }
        opt_idx++;
    }

    if (opt_idx >= argc) {
        usage();
        return 2;
    }

    const char *pattern = argv[opt_idx++];
    int cflags = 0;
    if (opt_i)
        cflags |= REG_ICASE;
    if (opt_E)
        cflags |= REG_EXTENDED;

    regex_t preg;
    int err = regcomp(&preg, pattern, cflags);
    if (err != 0) {
        char errbuf[128];
        regerror(err, &preg, errbuf, sizeof(errbuf));
        fprintf(stderr, "grep: invalid regex '%s': %s\n", pattern, errbuf);
        return 2;
    }

    int files_start = opt_idx;
    int file_count = argc - files_start;

    if (file_count == 0) {
        grep_file(stdin, "(standard input)", &preg, opt_v, opt_n, opt_c, opt_l, false);
    } else {
        bool multiple = (file_count > 1);
        for (int i = files_start; i < argc; i++) {
            FILE *f = fopen(argv[i], "r");
            if (!f) {
                perror(argv[i]);
                continue;
            }
            grep_file(f, argv[i], &preg, opt_v, opt_n, opt_c, opt_l, multiple);
            fclose(f);
        }
    }

    regfree(&preg);
    return 0;
}
