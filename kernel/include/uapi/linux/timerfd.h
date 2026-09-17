/*
 * Linux-compatible timerfd API definitions for SzpontOS
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef _UAPI_LINUX_TIMERFD_H
#define _UAPI_LINUX_TIMERFD_H

#include <stdint.h>

/* Flags for timerfd_settime */
#define TFD_TIMER_ABSTIME       (1 << 0)
#define TFD_TIMER_CANCEL_ON_SET (1 << 1)

/* Flags for timerfd_create */
#define TFD_CLOEXEC             0x80000
#define TFD_NONBLOCK            0x800

#endif /* _UAPI_LINUX_TIMERFD_H */
