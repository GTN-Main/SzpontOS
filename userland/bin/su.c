/*
 * SzpontOS — Switch User (su)
 * (C) Copyright by Szpont Industries. All rights reserved.
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
#include <fcntl.h>
#include <stdbool.h>

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

    fprintf(stderr, "%s", prompt);
    fflush(stderr);

    size_t idx = 0;
    char c = 0;
    while (idx + 1 < max_len) {
        ssize_t n = read(tty_fd, &c, 1);
        if (n <= 0) break;
        if (c == '\n' || c == '\r') break;
        buf[idx++] = c;
    }
    buf[idx] = '\0';

    if (term_modified) {
        tcsetattr(tty_fd, TCSANOW, &orig_term);
    }
    if (tty_fd != STDIN_FILENO) {
        close(tty_fd);
    }

    fprintf(stderr, "\n");
    fflush(stderr);

    return true;
}

static bool check_password(const char *target_user, const char *entered) {
    if (!target_user || !entered) return false;

    const char *stored = NULL;
    struct spwd *sp = getspnam(target_user);
    if (sp && sp->sp_pwdp) {
        stored = sp->sp_pwdp;
    } else {
        struct passwd *pw = getpwnam(target_user);
        if (pw && pw->pw_passwd && strcmp(pw->pw_passwd, "x") != 0) {
            stored = pw->pw_passwd;
        }
    }

    if (!stored) return false;
    if (stored[0] == '*' || stored[0] == '!') return false;
    if (stored[0] == '\0') return (entered[0] == '\0');
    if (entered[0] == '\0') return false;

    char *hash = crypt(entered, stored);
    if (hash && strcmp(hash, stored) == 0) {
        return true;
    }
    if (strcmp(entered, stored) == 0) {
        return true;
    }

    return false;
}

int main(int argc, char *argv[]) {
    uid_t real_uid = getuid();
    uid_t eff_uid = geteuid();

    const char *target_user = "root";
    bool login_shell = false;

    int idx = 1;
    while (idx < argc) {
        if (strcmp(argv[idx], "-") == 0 || strcmp(argv[idx], "-l") == 0 || strcmp(argv[idx], "--login") == 0) {
            login_shell = true;
            idx++;
        } else if (argv[idx][0] != '-') {
            target_user = argv[idx++];
            break;
        } else {
            idx++;
        }
    }

    struct passwd *pw = getpwnam(target_user);
    if (!pw) {
        fprintf(stderr, "su: user '%s' does not exist\n", target_user);
        return 1;
    }

    /* If not root, check SUID and authenticate */
    if (real_uid != 0) {
        if (eff_uid != 0) {
            fprintf(stderr, "su: must be installed setuid root (chmod 4755)\n");
            return 1;
        }

        char entered_pw[128];
        if (!get_terminal_password("Password: ", entered_pw, sizeof(entered_pw))) {
            fprintf(stderr, "su: error reading password\n");
            return 1;
        }

        /* Check against target user's password OR root password */
        if (!check_password(target_user, entered_pw) && !check_password("root", entered_pw)) {
            fprintf(stderr, "su: Sorry\n");
            return 1;
        }
    }

    /* Change supplementary groups */
    initgroups(pw->pw_name, pw->pw_gid);

    /* Change credentials */
    if (setgid(pw->pw_gid) != 0) {
        fprintf(stderr, "su: setgid failed\n");
        return 1;
    }

    if (setuid(pw->pw_uid) != 0) {
        fprintf(stderr, "su: setuid failed\n");
        return 1;
    }

    /* Switch working directory to user home directory */
    if (login_shell && pw->pw_dir && *pw->pw_dir) {
        chdir(pw->pw_dir);
    }

    const char *shell = (pw->pw_shell && *pw->pw_shell) ? pw->pw_shell : "/bin/sh";
    char *sh_argv[3];
    if (login_shell) {
        sh_argv[0] = (char *)shell;
        sh_argv[1] = (char *)"-l";
        sh_argv[2] = NULL;
    } else {
        sh_argv[0] = (char *)shell;
        sh_argv[1] = NULL;
    }

    setenv("HOME", pw->pw_dir ? pw->pw_dir : "/", 1);
    setenv("USER", pw->pw_name, 1);
    setenv("LOGNAME", pw->pw_name, 1);
    setenv("SHELL", shell, 1);
    setenv("PATH", "/bin:/usr/bin:/usr/tbin:/sbin:/usr/sbin", 1);

    execve(shell, sh_argv, environ);

    fprintf(stderr, "su: failed to execute shell '%s'\n", shell);
    return 1;
}
