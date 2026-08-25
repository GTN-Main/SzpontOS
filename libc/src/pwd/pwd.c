#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static struct passwd g_passwd_entry;
static char g_passwd_buf[512];
static int g_passwd_fd = -1;

void setpwent(void) {
    if (g_passwd_fd >= 0) {
        close(g_passwd_fd);
    }
    g_passwd_fd = open("/etc/passwd", O_RDONLY);
}

void endpwent(void) {
    if (g_passwd_fd >= 0) {
        close(g_passwd_fd);
        g_passwd_fd = -1;
    }
}

struct passwd *getpwent(void) {
    if (g_passwd_fd < 0) {
        setpwent();
        if (g_passwd_fd < 0)
            return NULL;
    }

    size_t pos = 0;
    while (pos < sizeof(g_passwd_buf) - 1) {
        char c;
        ssize_t bytes = read(g_passwd_fd, &c, 1);
        if (bytes <= 0) {
            if (pos == 0)
                return NULL;
            break;
        }
        if (c == '\n')
            break;
        g_passwd_buf[pos++] = c;
    }
    g_passwd_buf[pos] = '\0';
    if (pos == 0)
        return NULL;

    /* Parse: name:password:uid:gid:gecos:homedir:shell */
    char *fields[7];
    char *p = g_passwd_buf;
    for (int i = 0; i < 7; i++) {
        fields[i] = p;
        char *colon = strchr(p, ':');
        if (colon) {
            *colon = '\0';
            p = colon + 1;
        } else {
            if (i < 6)
                return NULL; /* Malformed */
            break;
        }
    }

    g_passwd_entry.pw_name = fields[0];
    g_passwd_entry.pw_passwd = fields[1];
    g_passwd_entry.pw_uid = (uid_t)atoi(fields[2]);
    g_passwd_entry.pw_gid = (gid_t)atoi(fields[3]);
    g_passwd_entry.pw_gecos = fields[4];
    g_passwd_entry.pw_dir = fields[5];
    g_passwd_entry.pw_shell = fields[6];

    return &g_passwd_entry;
}

struct passwd *getpwnam(const char *name) {
    if (!name)
        return NULL;
    setpwent();
    struct passwd *pw;
    while ((pw = getpwent()) != NULL) {
        if (strcmp(pw->pw_name, name) == 0) {
            endpwent();
            return pw;
        }
    }
    endpwent();
    return NULL;
}

struct passwd *getpwuid(uid_t uid) {
    setpwent();
    struct passwd *pw;
    while ((pw = getpwent()) != NULL) {
        if (pw->pw_uid == uid) {
            endpwent();
            return pw;
        }
    }
    endpwent();
    return NULL;
}
