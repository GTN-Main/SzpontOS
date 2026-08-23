#ifndef _UTMPX_H
#define _UTMPX_H

#include <utmp.h>

#define utmpx utmp

void setutxent(void);
struct utmpx *getutxent(void);
void endutxent(void);
struct utmpx *getutxid(const struct utmpx *ut);
struct utmpx *getutxline(const struct utmpx *ut);
struct utmpx *pututxline(const struct utmpx *ut);

#endif /* _UTMPX_H */
