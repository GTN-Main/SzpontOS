#ifndef _UTIME_H
#define _UTIME_H

#include <sys/types.h>
#include <time.h>

struct utimbuf {
    time_t actime;  /* Access time */
    time_t modtime; /* Modification time */
};

int utime(const char *filename, const struct utimbuf *times);

#endif /* _UTIME_H */
