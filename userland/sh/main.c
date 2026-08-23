#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>

#define MAX_LINE 256
#define MAX_ARGS 32
#define HISTORY_SIZE 32

/* ANSI Color definitions */
#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_RED     "\033[1;31m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_BLUE    "\033[1;34m"
#define COLOR_MAGENTA "\033[1;35m"
#define COLOR_CYAN    "\033[1;36m"
#define COLOR_WHITE   "\033[1;37m"
#define COLOR_GRAY    "\033[0;90m"

static char g_history[HISTORY_SIZE][MAX_LINE];
static int g_history_count = 0;
static int g_history_pos = 0;

#include <pwd.h>

static void history_add(const char *cmd) {
    if (!cmd || !*cmd) return;
    if (g_history_count > 0 && strcmp(g_history[(g_history_count - 1) % HISTORY_SIZE], cmd) == 0) {
        return; /* Do not duplicate consecutive commands */
    }
    strncpy(g_history[g_history_count % HISTORY_SIZE], cmd, MAX_LINE - 1);
    g_history_count++;
}

static void print_prompt(void) {
    char cwd[128];
    if (!getcwd(cwd, sizeof(cwd))) {
        strcpy(cwd, "/");
    }

    char hostname[64];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strcpy(hostname, "szpontos");
    }

    uid_t euid = geteuid();
    if (euid == 0) {
        printf(COLOR_RED "root" COLOR_RESET "@" COLOR_GREEN "%s" COLOR_RESET ":" COLOR_BLUE "%s" COLOR_RESET "# ", hostname, cwd);
    } else {
        struct passwd *pw = getpwuid(euid);
        const char *name = (pw && pw->pw_name) ? pw->pw_name : "user";
        printf(COLOR_CYAN "%s" COLOR_RESET "@" COLOR_GREEN "%s" COLOR_RESET ":" COLOR_BLUE "%s" COLOR_RESET "$ ", name, hostname, cwd);
    }
}

static void cmd_help(void) {
    printf(COLOR_CYAN "SzpontOS Shell (sh) - Built-in Commands:" COLOR_RESET "\n");
    printf("  " COLOR_GREEN "help" COLOR_RESET "            - Display this help message\n");
    printf("  " COLOR_GREEN "echo [args...]" COLOR_RESET "  - Print text to standard output\n");
    printf("  " COLOR_GREEN "pwd" COLOR_RESET "             - Print current working directory\n");
    printf("  " COLOR_GREEN "cd <path>" COLOR_RESET "       - Change current working directory\n");
    printf("  " COLOR_GREEN "exit" COLOR_RESET "            - Exit the shell\n");
    printf("\n" COLOR_YELLOW "External Commands (/bin/):" COLOR_RESET "\n");
    printf("  " COLOR_WHITE "ls, cat, head, tail, wc, grep, find" COLOR_RESET " - Files & text\n");
    printf("  " COLOR_WHITE "uname, hostname, uptime, ps, top" COLOR_RESET " - System info\n");
    printf("  " COLOR_WHITE "df, free, mount, dmesg, sysctl" COLOR_RESET "  - Diagnostics\n");
    printf("  " COLOR_WHITE "mkdir, touch, rm, chmod, chown" COLOR_RESET "  - File management\n");
    printf("  " COLOR_WHITE "id, whoami, su, useradd, userdel" COLOR_RESET " - Users & groups\n");
    printf("  " COLOR_WHITE "kill, killall, sleep, clear" COLOR_RESET "     - Process control\n");
    printf("  " COLOR_WHITE "ping, ifconfig, nc, httpd" COLOR_RESET "      - Networking\n");
    printf("  " COLOR_WHITE "nano, fastfetch, file, zsh, donut" COLOR_RESET " - Applications\n");
    printf("  " COLOR_WHITE "insmod, rmmod, lsmod, modinfo" COLOR_RESET "  - Kernel modules\n");
    printf("  " COLOR_WHITE "reboot, shutdown, poweroff" COLOR_RESET "     - Power management\n");
    printf("\n" COLOR_YELLOW "Keyboard Shortcuts & Line Editing:" COLOR_RESET "\n");
    printf("  " COLOR_WHITE "Up/Down Arrows" COLOR_RESET "  - Command history navigation\n");
    printf("  " COLOR_WHITE "Left/Right" COLOR_RESET "      - Cursor movement\n");
    printf("  " COLOR_WHITE "Home / End" COLOR_RESET "      - Jump to beginning / end of line\n");
    printf("  " COLOR_WHITE "Ctrl+C" COLOR_RESET "          - Send SIGINT (interrupt running command)\n");
    printf("  " COLOR_WHITE "Ctrl+D" COLOR_RESET "          - Exit shell on empty line / EOF\n");
    printf("  " COLOR_WHITE "Ctrl+L" COLOR_RESET "          - Clear screen and redraw prompt\n");
    printf("  " COLOR_WHITE "Ctrl+U" COLOR_RESET "          - Clear entire input line\n");
}







static void cmd_pwd(void) {
    char buf[256];
    if (getcwd(buf, sizeof(buf))) {
        printf(COLOR_YELLOW "%s" COLOR_RESET "\n", buf);
    }
}

static void cmd_cd(const char *path) {
    if (!path || !*path) path = "/";
    if (chdir(path) != 0) {
        printf(COLOR_RED "cd: %s: No such directory" COLOR_RESET "\n", path);
    }
}



static void execute_command(int argc, char *argv[]) {
    if (argc == 0) return;

    if (strcmp(argv[0], "help") == 0) {
        cmd_help();
    } else if (strcmp(argv[0], "echo") == 0) {
        for (int i = 1; i < argc; i++) {
            printf("%s%s", argv[i], (i == argc - 1) ? "" : " ");
        }
        printf("\n");
    } else if (strcmp(argv[0], "pwd") == 0) {
        cmd_pwd();
    } else if (strcmp(argv[0], "cd") == 0) {
        cmd_cd(argc > 1 ? argv[1] : "/");
    } else if (strcmp(argv[0], "exit") == 0) {
        exit(0);
    } else {
        /* Execute external program from /bin or path */
        pid_t pid = fork();
        if (pid == 0) {
            char exec_path[128];
            if (argv[0][0] == '/' || (argv[0][0] == '.' && argv[0][1] == '/')) {
                strncpy(exec_path, argv[0], sizeof(exec_path) - 1);
            } else {
                snprintf(exec_path, sizeof(exec_path), "/bin/%s", argv[0]);
            }
            char *default_envp[] = {
                "PATH=/bin",
                "TERM=xterm-256color",
                "USER=root",
                "HOME=/root",
                "SHELL=/bin/sh",
                NULL
            };
            execve(exec_path, argv, default_envp);
            printf(COLOR_RED "sh: command not found: %s" COLOR_RESET "\n", argv[0]);
            exit(127);
        } else if (pid > 0) {
            int status = 0;
            waitpid(pid, &status, 0);
            if ((status & 0x7F) == 2 || status == 130) {
                printf(COLOR_RED "\n[Process interrupted by SIGINT]" COLOR_RESET "\n");
            }
        }
    }
}

static bool read_input_line(char *line, size_t max_len) {
    int len = 0;
    int cursor = 0;
    line[0] = '\0';
    g_history_pos = g_history_count;

    while (1) {
        int c = getchar();

        /* EOF (Ctrl+D on empty line) */
        if (c == EOF || c == 0x04) {
            if (len == 0) {
                printf("exit\n");
                return false;
            }
            continue;
        }

        /* Ctrl+C (0x03) */
        if (c == 0x03) {
            printf("^C\n");
            line[0] = '\0';
            return true;
        }

        /* Enter / Newline */
        if (c == '\n' || c == '\r') {
            printf("\n");
            line[len] = '\0';
            if (len > 0) {
                history_add(line);
            }
            return true;
        }

        /* Ctrl+L (Clear Screen) */
        if (c == 0x0C) {
            printf("\033[2J\033[H");
            print_prompt();
            printf("%s", line);
            continue;
        }

        /* Ctrl+U (Clear Line) */
        if (c == 0x15) {
            while (cursor > 0) {
                printf("\b \b");
                cursor--;
            }
            len = 0;
            line[0] = '\0';
            continue;
        }

        /* Backspace (0x08 / 127) */
        if (c == '\b' || c == 127) {
            if (cursor > 0) {
                for (int i = cursor - 1; i < len - 1; i++) {
                    line[i] = line[i + 1];
                }
                len--;
                cursor--;
                line[len] = '\0';

                /* Redraw tail */
                printf("\b");
                for (int i = cursor; i < len; i++) {
                    putchar(line[i]);
                }
                putchar(' ');
                for (int i = cursor; i <= len; i++) {
                    putchar('\b');
                }
            }
            continue;
        }

        /* ANSI Escape Sequence (Arrows, Home, End, Delete) */
        if (c == '\033') {
            int c1 = getchar();
            if (c1 == '[') {
                int c2 = getchar();

                /* Up Arrow (History Previous) */
                if (c2 == 'A') {
                    if (g_history_count > 0 && g_history_pos > 0) {
                        g_history_pos--;
                        const char *hist = g_history[g_history_pos % HISTORY_SIZE];

                        /* Erase current line */
                        while (cursor > 0) { printf("\b \b"); cursor--; }
                        while (len > 0) { printf(" \b"); len--; }

                        strcpy(line, hist);
                        len = strlen(line);
                        cursor = len;
                        printf("%s", line);
                    }
                    continue;
                }

                /* Down Arrow (History Next) */
                if (c2 == 'B') {
                    if (g_history_pos < g_history_count) {
                        g_history_pos++;
                        const char *hist = (g_history_pos < g_history_count) ?
                                           g_history[g_history_pos % HISTORY_SIZE] : "";

                        /* Erase current line */
                        while (cursor > 0) { printf("\b \b"); cursor--; }
                        while (len > 0) { printf(" \b"); len--; }

                        strcpy(line, hist);
                        len = strlen(line);
                        cursor = len;
                        printf("%s", line);
                    }
                    continue;
                }

                /* Right Arrow */
                if (c2 == 'C') {
                    if (cursor < len) {
                        putchar(line[cursor]);
                        cursor++;
                    }
                    continue;
                }

                /* Left Arrow */
                if (c2 == 'D') {
                    if (cursor > 0) {
                        cursor--;
                        putchar('\b');
                    }
                    continue;
                }

                /* Home */
                if (c2 == 'H') {
                    while (cursor > 0) {
                        putchar('\b');
                        cursor--;
                    }
                    continue;
                }

                /* End */
                if (c2 == 'F') {
                    while (cursor < len) {
                        putchar(line[cursor]);
                        cursor++;
                    }
                    continue;
                }

                /* Delete key: \033[3~ */
                if (c2 == '3') {
                    getchar(); /* Consume '~' */
                    if (cursor < len) {
                        for (int i = cursor; i < len - 1; i++) {
                            line[i] = line[i + 1];
                        }
                        len--;
                        line[len] = '\0';

                        for (int i = cursor; i < len; i++) {
                            putchar(line[i]);
                        }
                        putchar(' ');
                        for (int i = cursor; i <= len; i++) {
                            putchar('\b');
                        }
                    }
                    continue;
                }
            }
            continue;
        }

        /* Printable Character Insertion */
        if (c >= 32 && len < (int)max_len - 1) {
            for (int i = len; i > cursor; i--) {
                line[i] = line[i - 1];
            }
            line[cursor] = (char)c;
            len++;
            line[len] = '\0';

            /* Draw inserted character and tail */
            for (int i = cursor; i < len; i++) {
                putchar(line[i]);
            }
            cursor++;
            for (int i = cursor; i < len; i++) {
                putchar('\b');
            }
        }
    }
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    /* Ignore SIGINT in the shell process itself so Ctrl+C cancels the prompt line without terminating sh */
    signal(SIGINT, SIG_IGN);

    printf(COLOR_CYAN "SzpontOS Unix Shell (sh) v0.1.0" COLOR_RESET "\n");
    printf(COLOR_GRAY "Type 'help' for built-in commands & keyboard shortcuts." COLOR_RESET "\n\n");

    char line[MAX_LINE];
    char *args[MAX_ARGS];

    while (1) {
        print_prompt();

        if (!read_input_line(line, sizeof(line))) {
            break;
        }

        /* Parse tokens with quotes support */
        int arg_count = 0;
        char *p = line;
        while (*p) {
            while (*p == ' ' || *p == '\t') p++;
            if (!*p) break;

            char *token_start;
            if (*p == '\'' || *p == '"') {
                char quote = *p++;
                token_start = p;
                while (*p && *p != quote) p++;
                if (*p == quote) {
                    *p++ = '\0';
                }
            } else {
                token_start = p;
                while (*p && *p != ' ' && *p != '\t') p++;
                if (*p) {
                    *p++ = '\0';
                }
            }

            args[arg_count++] = token_start;
            if (arg_count >= MAX_ARGS - 1) break;
        }
        args[arg_count] = NULL;

        if (arg_count > 0) {
            execute_command(arg_count, args);
        }
    }

    return 0;
}
