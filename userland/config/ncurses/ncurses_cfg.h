/*
 * SzpontOS - GNU Ncurses Configuration Header (ncurses_cfg.h)
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#ifndef _NCURSES_CFG_H
#define _NCURSES_CFG_H

#define NCURSES_VERSION "6.4"
#define NCURSES_VERSION_MAJOR 6
#define NCURSES_VERSION_MINOR 4
#define NCURSES_VERSION_PATCH 20230603

#define PACKAGE "ncurses"
#define SYSTEM_NAME "szpontos"

#define HAVE_UNISTD_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_IOCTL_H 1
#define HAVE_SYS_PARAM_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_TERMIOS_H 1
#define HAVE_TERMIO_H 0
#define HAVE_SGTTY_H 0
#define HAVE_DIRENT_H 1
#define HAVE_FCNTL_H 1
#define HAVE_GETOPT_H 1
#define HAVE_LIMITS_H 1
#define HAVE_LOCALE_H 1
#define HAVE_MATH_H 1
#define HAVE_POLL_H 0
#define HAVE_REGEX_H 1
#define HAVE_SELECT 0
#define HAVE_SETBUF 1
#define HAVE_SETBUFFER 0
#define HAVE_SETVBUF 1
#define HAVE_SIGACTION 1
#define HAVE_SIGNAL_H 1
#define HAVE_STDARG_H 1
#define HAVE_STDBOOL_H 1
#define HAVE_STDDEF_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDIO_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_STRINGS_H 1
#define HAVE_TIME_H 1
#define HAVE_UNISTD_H 1
#define HAVE_WCHAR_H 1
#define HAVE_WCTYPE_H 1

#define HAVE_GETTIMEOFDAY 1
#define HAVE_ISATTY 1
#define HAVE_NANOSLEEP 0
#define HAVE_PAUSE 0
#define HAVE_POLL 0
#define HAVE_REMOVE 1
#define HAVE_SLEEP 1
#define HAVE_SNPRINTF 1
#define HAVE_STRDUP 1
#define HAVE_STRSTR 1
#define HAVE_TCGETATTR 1
#define HAVE_TCSETATTR 1
#define HAVE_USLEEP 1
#define HAVE_VSNPRINTF 1
#define HAVE_VSSCANF 1
#define HAVE_WCWIDTH 1

#define USE_TERMIOS 1
#define USE_SYSV_UTMP 0
#define USE_GETCAP 0
#define USE_HASHMAP 1
#define USE_SCROLL_HINTS 1
#define USE_SIGWINCH 1
#define USE_WIDEC_SUPPORT 0

#define NCURSES_EXT_FUNCS 1
#define NCURSES_EXT_COLORS 1
#define NCURSES_SP_FUNCS 1
#define NCURSES_OPAQUE 0
#define NCURSES_WRAPPED_VAR 0

#define STDC_HEADERS 1
#define RETSIGTYPE void

#endif /* _NCURSES_CFG_H */
