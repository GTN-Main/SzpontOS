/*
 * SzpontOS - POSIX sys/times.h
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef _SYS_TIMES_H
#define _SYS_TIMES_H

#include <sys/types.h>

struct tms {
    clock_t tms_utime;  /* User CPU time */
    clock_t tms_stime;  /* System CPU time */
    clock_t tms_cutime; /* User CPU time of terminated children */
    clock_t tms_cstime; /* System CPU time of terminated children */
};

clock_t times(struct tms *buf);

#endif /* _SYS_TIMES_H */
