#ifndef _STDDEF_H
#define _STDDEF_H

typedef unsigned long size_t;
typedef long ptrdiff_t;

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef _WCHAR_T_DECLARED
#define _WCHAR_T_DECLARED
#ifndef __cplusplus
typedef unsigned int wchar_t;
#endif
#endif

#ifndef offsetof
#define offsetof(type, member) ((size_t)&(((type *)0)->member))
#endif

#endif /* _STDDEF_H */
