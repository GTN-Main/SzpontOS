/*
 * SzpontOS C Standard Library - POSIX/Linux timerfd definitions
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef _SYS_TIMERFD_H
#define _SYS_TIMERFD_H

#include <time.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TFD_TIMER_ABSTIME       (1 << 0)
#define TFD_TIMER_CANCEL_ON_SET (1 << 1)

#define TFD_CLOEXEC             0x80000
#define TFD_NONBLOCK            0x800

int timerfd_create(int clockid, int flags);
int timerfd_settime(int fd, int flags, const struct itimerspec *new_value, struct itimerspec *old_value);
int timerfd_gettime(int fd, struct itimerspec *curr_value);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_TIMERFD_H */
