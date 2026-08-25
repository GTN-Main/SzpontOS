/*
 * SzpontOS - POSIX sys/select.h
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef _SYS_SELECT_H
#define _SYS_SELECT_H

#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>

#define FD_SETSIZE 1024

typedef struct {
    unsigned long fds_bits[FD_SETSIZE / (8 * sizeof(unsigned long))];
} fd_set;

#define FD_ZERO(set)                                                                                                   \
    do {                                                                                                               \
        unsigned int __i;                                                                                              \
        for (__i = 0; __i < sizeof((set)->fds_bits) / sizeof((set)->fds_bits[0]); __i++)                               \
            (set)->fds_bits[__i] = 0;                                                                                  \
    } while (0)

#define FD_SET(d, set)                                                                                                 \
    ((set)->fds_bits[(d) / (8 * sizeof(unsigned long))] |= (1UL << ((d) % (8 * sizeof(unsigned long)))))

#define FD_CLR(d, set)                                                                                                 \
    ((set)->fds_bits[(d) / (8 * sizeof(unsigned long))] &= ~(1UL << ((d) % (8 * sizeof(unsigned long)))))

#define FD_ISSET(d, set)                                                                                               \
    (((set)->fds_bits[(d) / (8 * sizeof(unsigned long))] & (1UL << ((d) % (8 * sizeof(unsigned long))))) != 0)

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
int pselect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timespec *timeout,
            const sigset_t *sigmask);

#endif /* _SYS_SELECT_H */
