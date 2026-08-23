#include <mntent.h>
#include <stdlib.h>
#include <string.h>

static struct mntent g_mnt;
static char g_mnt_line[512];

FILE *setmntent(const char *filename, const char *type) {
    return fopen(filename, type);
}

struct mntent *getmntent(FILE *stream) {
    if (!stream) return NULL;
    while (fgets(g_mnt_line, sizeof(g_mnt_line), stream)) {
        if (g_mnt_line[0] == '#' || g_mnt_line[0] == '\n') continue;
        char *p = g_mnt_line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') continue;

        g_mnt.mnt_fsname = strsep(&p, " \t\n");
        while (p && (*p == ' ' || *p == '\t')) p++;
        g_mnt.mnt_dir = strsep(&p, " \t\n");
        while (p && (*p == ' ' || *p == '\t')) p++;
        g_mnt.mnt_type = strsep(&p, " \t\n");
        while (p && (*p == ' ' || *p == '\t')) p++;
        g_mnt.mnt_opts = strsep(&p, " \t\n");
        while (p && (*p == ' ' || *p == '\t')) p++;
        char *freq = strsep(&p, " \t\n");
        while (p && (*p == ' ' || *p == '\t')) p++;
        char *passno = strsep(&p, " \t\n");

        g_mnt.mnt_freq = freq ? atoi(freq) : 0;
        g_mnt.mnt_passno = passno ? atoi(passno) : 0;

        if (g_mnt.mnt_fsname && g_mnt.mnt_dir && g_mnt.mnt_type) {
            if (!g_mnt.mnt_opts) g_mnt.mnt_opts = "rw";
            return &g_mnt;
        }
    }
    return NULL;
}

int endmntent(FILE *stream) {
    if (stream) {
        fclose(stream);
    }
    return 1;
}

char *hasmntopt(const struct mntent *mnt, const char *opt) {
    if (!mnt || !mnt->mnt_opts || !opt) return NULL;
    char *opts = mnt->mnt_opts;
    size_t optlen = strlen(opt);
    while (*opts) {
        char *comma = strchr(opts, ',');
        size_t len = comma ? (size_t)(comma - opts) : strlen(opts);
        if (len == optlen && strncmp(opts, opt, optlen) == 0) {
            return opts;
        }
        if (!comma) break;
        opts = comma + 1;
    }
    return NULL;
}
