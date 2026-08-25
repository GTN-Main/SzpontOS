#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/utsname.h>

static void print_help(void) {
    printf("Usage: uname [OPTION]...\n");
    printf("Print certain system information.  With no OPTION, same as -s.\n\n");
    printf("  -a, --all                print all information, in the following order:\n");
    printf("  -s, --kernel-name        print the kernel name\n");
    printf("  -n, --nodename           print the network node hostname\n");
    printf("  -r, --kernel-release     print the kernel release\n");
    printf("  -v, --kernel-version     print the kernel version\n");
    printf("  -m, --machine            print the machine hardware name\n");
    printf("  -p, --processor          print the processor type\n");
    printf("  -i, --hardware-platform  print the hardware platform\n");
    printf("  -o, --operating-system   print the operating system\n");
    printf("      --help               display this help and exit\n");
    printf("      --version            output version information and exit\n");
}

int main(int argc, char *argv[]) {
    struct utsname u;
    if (uname(&u) != 0) {
        perror("uname");
        return 1;
    }

    bool opt_s = false;
    bool opt_n = false;
    bool opt_r = false;
    bool opt_v = false;
    bool opt_m = false;
    bool opt_o = false;

    if (argc == 1) {
        opt_s = true;
    } else {
        for (int i = 1; i < argc; i++) {
            const char *arg = argv[i];
            if (strcmp(arg, "--help") == 0) {
                print_help();
                return 0;
            } else if (strcmp(arg, "--version") == 0) {
                printf("uname (SzpontOS coreutils) 0.1.0\n");
                return 0;
            } else if (strcmp(arg, "--all") == 0) {
                opt_s = opt_n = opt_r = opt_v = opt_m = true;
            } else if (strcmp(arg, "--kernel-name") == 0) {
                opt_s = true;
            } else if (strcmp(arg, "--nodename") == 0) {
                opt_n = true;
            } else if (strcmp(arg, "--kernel-release") == 0) {
                opt_r = true;
            } else if (strcmp(arg, "--kernel-version") == 0) {
                opt_v = true;
            } else if (strcmp(arg, "--machine") == 0 || strcmp(arg, "--processor") == 0 ||
                       strcmp(arg, "--hardware-platform") == 0) {
                opt_m = true;
            } else if (strcmp(arg, "--operating-system") == 0) {
                opt_o = true;
            } else if (arg[0] == '-' && arg[1] != '\0') {
                for (int j = 1; arg[j]; j++) {
                    switch (arg[j]) {
                    case 'a':
                        opt_s = opt_n = opt_r = opt_v = opt_m = true;
                        break;
                    case 's':
                        opt_s = true;
                        break;
                    case 'n':
                        opt_n = true;
                        break;
                    case 'r':
                        opt_r = true;
                        break;
                    case 'v':
                        opt_v = true;
                        break;
                    case 'm':
                    case 'p':
                    case 'i':
                        opt_m = true;
                        break;
                    case 'o':
                        opt_o = true;
                        break;
                    case 'h':
                        print_help();
                        return 0;
                    default:
                        fprintf(stderr, "uname: invalid option -- '%c'\n", arg[j]);
                        fprintf(stderr, "Try 'uname --help' for more information.\n");
                        return 1;
                    }
                }
            } else {
                fprintf(stderr, "uname: extra operand '%s'\n", arg);
                fprintf(stderr, "Try 'uname --help' for more information.\n");
                return 1;
            }
        }
    }

    if (!opt_s && !opt_n && !opt_r && !opt_v && !opt_m && !opt_o) {
        opt_s = true;
    }

    bool first = true;
    if (opt_s) {
        printf("%s%s", first ? "" : " ", u.sysname);
        first = false;
    }
    if (opt_n) {
        printf("%s%s", first ? "" : " ", u.nodename);
        first = false;
    }
    if (opt_r) {
        printf("%s%s", first ? "" : " ", u.release);
        first = false;
    }
    if (opt_v) {
        printf("%s%s", first ? "" : " ", u.version);
        first = false;
    }
    if (opt_m) {
        printf("%s%s", first ? "" : " ", u.machine);
        first = false;
    }
    if (opt_o) {
        printf("%s%s", first ? "" : " ", u.sysname);
        first = false;
    }
    printf("\n");
    return 0;
}
