/*
 * SzpontOS - POSIX alloca.h
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef _ALLOCA_H
#define _ALLOCA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#ifndef alloca
#define alloca(size) __builtin_alloca(size)
#endif

void *alloca(size_t size);

#ifdef __cplusplus
}
#endif

#endif /* _ALLOCA_H */
