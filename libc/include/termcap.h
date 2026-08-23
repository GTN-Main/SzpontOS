/*
 * SzpontOS - POSIX termcap.h
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef _TERMCAP_H
#define _TERMCAP_H

#include <term.h>

extern char PC;
extern char *UP;
extern char *BC;
extern short ospeed;

int tgetent(char *bp, const char *name);
int tgetflag(const char *id);
int tgetnum(const char *id);
char *tgetstr(const char *id, char **area);
char *tgoto(const char *cap, int col, int row);

#endif /* _TERMCAP_H */
