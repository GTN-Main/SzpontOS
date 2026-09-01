/*
 * SzpontOS Libc - sys/random.h (getrandom, getentropy)
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef _SYS_RANDOM_H
#define _SYS_RANDOM_H

#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Flags for getrandom(2) */
#define GRND_NONBLOCK 0x0001
#define GRND_RANDOM   0x0002

ssize_t getrandom(void *buf, size_t buflen, unsigned int flags);
int getentropy(void *buffer, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_RANDOM_H */
