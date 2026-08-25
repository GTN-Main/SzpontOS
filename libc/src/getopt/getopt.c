#include <getopt.h>
#include <string.h>
#include <stdio.h>

char *optarg = NULL;
int optind = 1;
int opterr = 1;
int optopt = '?';

static char *next_char = NULL;

int getopt(int argc, char *const argv[], const char *optstring) {
    return getopt_long(argc, argv, optstring, NULL, NULL);
}

int getopt_long(int argc, char *const argv[], const char *optstring, const struct option *longopts, int *longindex) {
    if (optind >= argc || !argv[optind]) {
        return -1;
    }

    if (!next_char || !*next_char) {
        char *arg = argv[optind];
        if (!arg || arg[0] != '-' || arg[1] == '\0') {
            return -1;
        }

        if (arg[0] == '-' && arg[1] == '-' && arg[2] == '\0') {
            optind++;
            return -1;
        }

        /* Check for long options */
        if (arg[0] == '-' && arg[1] == '-' && longopts) {
            const char *opt_name = arg + 2;
            char *eq = strchr(opt_name, '=');
            size_t name_len = eq ? (size_t)(eq - opt_name) : strlen(opt_name);

            for (int i = 0; longopts[i].name != NULL; i++) {
                if (strncmp(longopts[i].name, opt_name, name_len) == 0 && strlen(longopts[i].name) == name_len) {
                    optind++;
                    if (longindex)
                        *longindex = i;

                    if (longopts[i].has_arg == required_argument) {
                        if (eq) {
                            optarg = eq + 1;
                        } else if (optind < argc) {
                            optarg = argv[optind++];
                        } else {
                            if (opterr)
                                printf("%s: option '--%s' requires an argument\n", argv[0], longopts[i].name);
                            return ':';
                        }
                    } else if (longopts[i].has_arg == optional_argument) {
                        optarg = eq ? (eq + 1) : NULL;
                    } else {
                        optarg = NULL;
                    }

                    if (longopts[i].flag) {
                        *longopts[i].flag = longopts[i].val;
                        return 0;
                    }
                    return longopts[i].val;
                }
            }

            if (opterr)
                printf("%s: unrecognized option '--%s'\n", argv[0], opt_name);
            optind++;
            return '?';
        }

        next_char = arg + 1;
    }

    char c = *next_char++;
    const char *match = strchr(optstring, c);

    if (!match || c == ':') {
        optopt = c;
        if (opterr)
            printf("%s: invalid option -- '%c'\n", argv[0], c);
        if (!*next_char)
            optind++;
        return '?';
    }

    if (match[1] == ':') {
        if (*next_char) {
            optarg = next_char;
            next_char = NULL;
            optind++;
        } else if (optind + 1 < argc) {
            optind++;
            optarg = argv[optind++];
        } else {
            optopt = c;
            if (opterr)
                printf("%s: option requires an argument -- '%c'\n", argv[0], c);
            optind++;
            return (optstring[0] == ':') ? ':' : '?';
        }
    } else {
        optarg = NULL;
        if (!*next_char)
            optind++;
    }

    return c;
}

int getopt_long_only(int argc, char *const argv[], const char *optstring, const struct option *longopts,
                     int *longindex) {
    return getopt_long(argc, argv, optstring, longopts, longindex);
}
