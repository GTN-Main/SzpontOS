#include <utmp.h>
#include <utmpx.h>
#include <string.h>

static struct utmp g_dummy_utmp = {.ut_type = USER_PROCESS,
                                   .ut_pid = 1,
                                   .ut_line = "tty1",
                                   .ut_id = "1",
                                   .ut_user = "root",
                                   .ut_host = "szpontos",
                                   .ut_tv = {1771718400, 0}};

static int g_utmp_idx = 0;

void setutent(void) {
    g_utmp_idx = 0;
}

struct utmp *getutent(void) {
    if (g_utmp_idx == 0) {
        g_utmp_idx = 1;
        return &g_dummy_utmp;
    }
    return NULL;
}

void endutent(void) {
    g_utmp_idx = 0;
}

struct utmp *getutid(const struct utmp *ut) {
    (void)ut;
    return &g_dummy_utmp;
}

struct utmp *getutline(const struct utmp *ut) {
    (void)ut;
    return &g_dummy_utmp;
}

struct utmp *pututline(const struct utmp *ut) {
    (void)ut;
    return &g_dummy_utmp;
}

int utmpname(const char *file) {
    (void)file;
    return 0;
}

void setutxent(void) {
    setutent();
}

struct utmpx *getutxent(void) {
    return getutent();
}

void endutxent(void) {
    endutent();
}

struct utmpx *getutxid(const struct utmpx *ut) {
    return getutid(ut);
}

struct utmpx *getutxline(const struct utmpx *ut) {
    return getutline(ut);
}

struct utmpx *pututxline(const struct utmpx *ut) {
    return pututline(ut);
}
