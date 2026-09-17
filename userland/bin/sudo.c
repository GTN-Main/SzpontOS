/*
 * SzpontOS — Superuser Do (sudo)
 * (C) Copyright by Szpont Industries. All rights reserved.
 *
 * Allows authorized users to execute commands as superuser (or another user)
 * with PAM/shadow authentication, credential caching, and sudoers authorization.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <shadow.h>
#include <crypt.h>
#include <termios.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <fcntl.h>
#include <stdbool.h>

#define SUDO_TIMEOUT_SECS 300 /* 5 minutes credential cache */
#define SUDO_RUN_DIR "/var/run"

static void print_usage(void) {
    fprintf(stderr, "usage: sudo -h | -V\n");
    fprintf(stderr, "usage: sudo [-v] [-k] [-n] [-u user] [-g group] [-s | -i] [command [arg ...]]\n");
    fprintf(stderr, "\nOptions:\n");
    fprintf(stderr, "  -h, --help       display this help message\n");
    fprintf(stderr, "  -V               display version information\n");
    fprintf(stderr, "  -v               update user's timestamp without running command\n");
    fprintf(stderr, "  -k               invalidate user's cached credentials\n");
    fprintf(stderr, "  -n               non-interactive mode (fail if password prompt is needed)\n");
    fprintf(stderr, "  -u user          run command as specified user (default: root)\n");
    fprintf(stderr, "  -g group         run command as specified group\n");
    fprintf(stderr, "  -s               run shell as superuser\n");
    fprintf(stderr, "  -i               run login shell as superuser\n");
}

static bool get_terminal_password(const char *prompt, char *buf, size_t max_len) {
    if (!buf || max_len == 0) return false;
    buf[0] = '\0';

    int tty_fd = open("/dev/tty", O_RDWR);
    if (tty_fd < 0) {
        tty_fd = STDIN_FILENO;
    }

    struct termios orig_term, raw_term;
    bool term_modified = false;

    if (tcgetattr(tty_fd, &orig_term) == 0) {
        raw_term = orig_term;
        raw_term.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
        if (tcsetattr(tty_fd, TCSANOW, &raw_term) == 0) {
            term_modified = true;
        }
    }

    /* Print prompt to stderr */
    fprintf(stderr, "%s", prompt);
    fflush(stderr);

    /* Read password */
    size_t idx = 0;
    char c = 0;
    while (idx + 1 < max_len) {
        ssize_t n = read(tty_fd, &c, 1);
        if (n <= 0) break;
        if (c == '\n' || c == '\r') break;
        buf[idx++] = c;
    }
    buf[idx] = '\0';

    /* Restore terminal settings */
    if (term_modified) {
        tcsetattr(tty_fd, TCSANOW, &orig_term);
    }
    if (tty_fd != STDIN_FILENO) {
        close(tty_fd);
    }

    /* Print newline after password */
    fprintf(stderr, "\n");
    fflush(stderr);

    return true;
}

static bool check_user_password(const char *username, const char *entered) {
    if (!username || !entered) return false;

    const char *stored = NULL;
    struct spwd *sp = getspnam(username);
    if (sp && sp->sp_pwdp) {
        stored = sp->sp_pwdp;
    } else {
        struct passwd *pw = getpwnam(username);
        if (pw && pw->pw_passwd && strcmp(pw->pw_passwd, "x") != 0) {
            stored = pw->pw_passwd;
        }
    }

    if (!stored) return false;

    /* Locked account */
    if (stored[0] == '*' || stored[0] == '!') return false;

    /* Empty password match */
    if (stored[0] == '\0') return (entered[0] == '\0');
    if (entered[0] == '\0') return false;

    char *hash = crypt(entered, stored);
    if (hash && strcmp(hash, stored) == 0) {
        return true;
    }

    /* Direct plaintext match fallback */
    if (strcmp(entered, stored) == 0) {
        return true;
    }

    return false;
}

static bool verify_password(const char *caller_user, const char *entered) {
    /* 1. Try authenticating as calling user */
    if (check_user_password(caller_user, entered)) {
        return true;
    }

    /* 2. Try authenticating against root password */
    if (strcmp(caller_user, "root") != 0 && check_user_password("root", entered)) {
        return true;
    }

    return false;
}

static void get_timestamp_path(const char *username, char *path, size_t path_len) {
    snprintf(path, path_len, "%s/.sudo_%s", SUDO_RUN_DIR, username);
}

static bool check_timestamp(const char *username, uid_t uid) {
    char path[256];
    get_timestamp_path(username, path, sizeof(path));

    struct stat st;
    if (stat(path, &st) == 0) {
        if (st.st_uid == uid || st.st_uid == 0) {
            time_t now = time(NULL);
            if (now >= st.st_mtime && (now - st.st_mtime) < SUDO_TIMEOUT_SECS) {
                return true;
            }
        }
    }
    return false;
}

static void update_timestamp(const char *username, uid_t uid) {
    char path[256];
    get_timestamp_path(username, path, sizeof(path));

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd >= 0) {
        fchown(fd, uid, 0);
        close(fd);
    }
}

static void remove_timestamp(const char *username) {
    char path[256];
    get_timestamp_path(username, path, sizeof(path));
    unlink(path);
}

static bool is_user_authorized(const char *username, uid_t uid, gid_t gid) {
    /* Root is always authorized */
    if (uid == 0 || strcmp(username, "root") == 0) {
        return true;
    }

    /* Primary user "szpont" is authorized */
    if (strcmp(username, "szpont") == 0) {
        return true;
    }

    /* Check if in wheel group (GID 10 or group name "wheel") */
    if (gid == 10) {
        return true;
    }

    struct group *gr = getgrnam("wheel");
    if (gr) {
        if (gr->gr_gid == gid) return true;
        if (gr->gr_mem) {
            for (char **m = gr->gr_mem; *m != NULL; m++) {
                if (strcmp(*m, username) == 0) {
                    return true;
                }
            }
        }
    }

    /* Parse /etc/sudoers if it exists */
    FILE *fp = fopen("/etc/sudoers", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            char *p = line;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '#' || *p == '\0' || *p == '\n') continue;

            if (*p == '%') {
                /* Group directive, e.g. %wheel */
                p++;
                char grp_name[64];
                int n = 0;
                while (*p && *p != ' ' && *p != '\t' && n < 63) {
                    grp_name[n++] = *p++;
                }
                grp_name[n] = '\0';
                struct group *g = getgrnam(grp_name);
                if (g && g->gr_mem) {
                    for (char **m = g->gr_mem; *m != NULL; m++) {
                        if (strcmp(*m, username) == 0) {
                            fclose(fp);
                            return true;
                        }
                    }
                }
            } else {
                /* User directive, e.g. szpont ALL=(ALL:ALL) ALL */
                char usr_name[64];
                int n = 0;
                while (*p && *p != ' ' && *p != '\t' && n < 63) {
                    usr_name[n++] = *p++;
                }
                usr_name[n] = '\0';
                if (strcmp(usr_name, username) == 0 || strcmp(usr_name, "ALL") == 0) {
                    fclose(fp);
                    return true;
                }
            }
        }
        fclose(fp);
    }

    return false;
}

int main(int argc, char *argv[]) {
    uid_t real_uid = getuid();
    gid_t real_gid = getgid();
    uid_t eff_uid = geteuid();

    /* SUID sanity check: must be able to act as root */
    if (eff_uid != 0 && real_uid != 0) {
        fprintf(stderr, "sudo: effective UID is not 0. sudo must be installed setuid root (chmod 4755).\n");
        return 1;
    }

    struct passwd *caller_pw = getpwuid(real_uid);
    const char *caller_name = caller_pw ? caller_pw->pw_name : "unknown";

    /* Authorization check */
    if (!is_user_authorized(caller_name, real_uid, real_gid)) {
        fprintf(stderr, "sudo: %s is not in the sudoers file. This incident will be reported.\n", caller_name);
        return 1;
    }

    const char *target_user = "root";
    const char *target_group = NULL;
    bool shell_flag = false;
    bool login_shell_flag = false;
    bool validate_only = false;
    bool non_interactive = false;

    int arg_idx = 1;
    while (arg_idx < argc && argv[arg_idx][0] == '-' && argv[arg_idx][1] != '\0') {
        if (strcmp(argv[arg_idx], "--") == 0) {
            arg_idx++;
            break;
        }
        if (strcmp(argv[arg_idx], "-h") == 0 || strcmp(argv[arg_idx], "--help") == 0) {
            print_usage();
            return 0;
        }
        if (strcmp(argv[arg_idx], "-V") == 0 || strcmp(argv[arg_idx], "--version") == 0) {
            printf("SzpontOS sudo version 1.0\n(C) Copyright by Szpont Industries. All rights reserved.\n");
            return 0;
        }
        if (strcmp(argv[arg_idx], "-v") == 0) {
            validate_only = true;
            arg_idx++;
            continue;
        }
        if (strcmp(argv[arg_idx], "-k") == 0) {
            remove_timestamp(caller_name);
            return 0;
        }
        if (strcmp(argv[arg_idx], "-n") == 0) {
            non_interactive = true;
            arg_idx++;
            continue;
        }
        if (strcmp(argv[arg_idx], "-s") == 0) {
            shell_flag = true;
            arg_idx++;
            continue;
        }
        if (strcmp(argv[arg_idx], "-i") == 0) {
            login_shell_flag = true;
            arg_idx++;
            continue;
        }
        if (strcmp(argv[arg_idx], "-u") == 0) {
            arg_idx++;
            if (arg_idx >= argc) {
                fprintf(stderr, "sudo: option requires an argument -- u\n");
                return 1;
            }
            target_user = argv[arg_idx++];
            continue;
        }
        if (strcmp(argv[arg_idx], "-g") == 0) {
            arg_idx++;
            if (arg_idx >= argc) {
                fprintf(stderr, "sudo: option requires an argument -- g\n");
                return 1;
            }
            target_group = argv[arg_idx++];
            continue;
        }

        /* Unknown option */
        fprintf(stderr, "sudo: unrecognized option '%s'\n", argv[arg_idx]);
        print_usage();
        return 1;
    }

    /* Authentication required if not root and no active timestamp cache */
    if (real_uid != 0 && !check_timestamp(caller_name, real_uid)) {
        if (non_interactive) {
            fprintf(stderr, "sudo: a password is required\n");
            return 1;
        }

        char prompt[128];
        snprintf(prompt, sizeof(prompt), "[sudo] password for %s: ", caller_name);

        int attempts = 0;
        bool authenticated = false;

        while (attempts < 3) {
            char entered_password[128];
            if (!get_terminal_password(prompt, entered_password, sizeof(entered_password))) {
                fprintf(stderr, "sudo: error reading password\n");
                return 1;
            }

            if (verify_password(caller_name, entered_password)) {
                authenticated = true;
                break;
            }

            attempts++;
            if (attempts < 3) {
                fprintf(stderr, "Sorry, try again.\n");
            }
        }

        if (!authenticated) {
            fprintf(stderr, "sudo: 3 incorrect password attempts\n");
            return 1;
        }

        update_timestamp(caller_name, real_uid);
    } else if (real_uid != 0) {
        /* Refresh active timestamp */
        update_timestamp(caller_name, real_uid);
    }

    if (validate_only) {
        return 0;
    }

    /* Resolve target user */
    struct passwd *target_pw = getpwnam(target_user);
    if (!target_pw) {
        char *endptr;
        uid_t target_uid_val = (uid_t)strtol(target_user, &endptr, 10);
        if (*endptr == '\0') {
            target_pw = getpwuid(target_uid_val);
        }
    }
    if (!target_pw) {
        fprintf(stderr, "sudo: unknown user: %s\n", target_user);
        return 1;
    }

    /* Resolve target group */
    gid_t target_gid_val = target_pw->pw_gid;
    if (target_group) {
        struct group *grp = getgrnam(target_group);
        if (!grp) {
            char *endptr;
            gid_t gid_val = (gid_t)strtol(target_group, &endptr, 10);
            if (*endptr == '\0') {
                grp = getgrgid(gid_val);
            }
        }
        if (!grp) {
            fprintf(stderr, "sudo: unknown group: %s\n", target_group);
            return 1;
        }
        target_gid_val = grp->gr_gid;
    }

    /* Setup target command */
    char *cmd_path = NULL;
    char **cmd_argv = NULL;

    const char *target_shell = (target_pw->pw_shell && *target_pw->pw_shell) ? target_pw->pw_shell : "/bin/sh";

    if (login_shell_flag) {
        cmd_path = (char *)target_shell;
        cmd_argv = (char **)malloc(3 * sizeof(char *));
        cmd_argv[0] = (char *)target_shell;
        cmd_argv[1] = (char *)"-l";
        cmd_argv[2] = NULL;
    } else if (shell_flag || arg_idx >= argc) {
        cmd_path = (char *)target_shell;
        if (arg_idx < argc) {
            /* Execute args under shell */
            cmd_argv = (char **)malloc((argc - arg_idx + 3) * sizeof(char *));
            cmd_argv[0] = (char *)target_shell;
            cmd_argv[1] = (char *)"-c";
            cmd_argv[2] = argv[arg_idx];
            cmd_argv[3] = NULL;
        } else {
            /* Interactive shell */
            cmd_argv = (char **)malloc(2 * sizeof(char *));
            cmd_argv[0] = (char *)target_shell;
            cmd_argv[1] = NULL;
        }
    } else {
        cmd_path = argv[arg_idx];
        cmd_argv = &argv[arg_idx];
    }

    /* Transition credentials */
    if (initgroups(target_pw->pw_name, target_gid_val) != 0) {
        /* Non-fatal on failure to set supplementary groups */
    }
    if (setgid(target_gid_val) != 0) {
        fprintf(stderr, "sudo: unable to set gid to %d\n", target_gid_val);
        return 1;
    }
    if (setuid(target_pw->pw_uid) != 0) {
        fprintf(stderr, "sudo: unable to set uid to %d\n", target_pw->pw_uid);
        return 1;
    }

    /* Configure environment */
    setenv("USER", target_pw->pw_name, 1);
    setenv("LOGNAME", target_pw->pw_name, 1);
    setenv("HOME", target_pw->pw_dir ? target_pw->pw_dir : "/", 1);
    setenv("SHELL", target_shell, 1);

    char uid_str[32], gid_str[32];
    snprintf(uid_str, sizeof(uid_str), "%d", real_uid);
    snprintf(gid_str, sizeof(gid_str), "%d", real_gid);
    setenv("SUDO_USER", caller_name, 1);
    setenv("SUDO_UID", uid_str, 1);
    setenv("SUDO_GID", gid_str, 1);

    /* Secure default PATH */
    const char *cur_path = getenv("PATH");
    if (!cur_path || !*cur_path || strstr(cur_path, "/usr/tbin") == NULL) {
        setenv("PATH", "/bin:/usr/bin:/usr/tbin:/sbin:/usr/sbin", 1);
    }

    if (login_shell_flag) {
        if (target_pw->pw_dir && *target_pw->pw_dir) {
            chdir(target_pw->pw_dir);
        }
    }

    /* Execute target command */
    execvp(cmd_path, cmd_argv);

    /* If execvp returned, execution failed */
    fprintf(stderr, "sudo: %s: command not found\n", cmd_path);
    return 127;
}
