/*
 * SzpontOS - POSIX strings.h
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef _STRINGS_H
#define _STRINGS_H

#include <string.h>

int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t n);
int ffs(int i);
int ffsl(long int i);

#define bcmp(s1, s2, n) memcmp((s1), (s2), (size_t)(n))
#define bcopy(src, dst, n) memcpy((dst), (src), (size_t)(n))
#define bzero(s, n) memset((s), 0, (size_t)(n))
#define index(s, c) strchr((s), (c))
#define rindex(s, c) strrchr((s), (c))

#endif /* _STRINGS_H */
