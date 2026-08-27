# SzpontOS Git Build Configuration (Libre-WD-40)
# (C) Copyright by Szpont Industries. All rights reserved.

uname_S = Linux
uname_M = x86_64

prefix = /usr
gitexecdir = /usr/libexec/git-core
mandir = /usr/share/man
htmldir = /usr/share/doc/git-doc

# Disable unsupported/unnecessary features and host headers
NO_OPENSSL = YesPlease
BLK_SHA1 = YesPlease
NO_CURL = YesPlease
NO_EXPAT = YesPlease
NO_TCLTK = YesPlease
NO_GETTEXT = YesPlease
NO_PERL = YesPlease
NO_PYTHON = YesPlease
NO_ICONV = YesPlease
NO_NSEC = YesPlease
NO_SYS_POLL_H = 1
NO_R_TO_ARM_BINUTILS = 1
FREAD_READS_DIRECTORIES = UnfortunatelyYes
SNPRINTF_RETURNS_BOGUS =
HAVE_DEV_TTY = YesPlease
HAVE_PATHS_H = YesPlease
HAVE_STRINGS_H = YesPlease
HAVE_ALLOCA_H =
HAVE_SYSINFO =
HAVE_PLATFORM_PROCINFO =
HAVE_SYNC_FILE_RANGE =
LIBC_CONTAINS_LIBINTL =
FSMONITOR_DAEMON_BACKEND =
FSMONITOR_OS_SETTINGS =
COMPAT_OBJS =
NEEDS_LIBGEN =
PTHREAD_LIBS =
USE_SYS_REGEX = 1
CSPRNG_METHOD = urandom
NO_STRLCPY =
DEFAULT_PAGER = cat
DEFAULT_EDITOR = nano

