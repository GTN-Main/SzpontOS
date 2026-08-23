#ifndef _FILE_CONFIG_H
#define _FILE_CONFIG_H

#define PACKAGE "file"
#define PACKAGE_BUGREPORT "christos@astron.com"
#define PACKAGE_NAME "file"
#define PACKAGE_STRING "file 5.45"
#define PACKAGE_TARNAME "file"
#define PACKAGE_URL ""
#define PACKAGE_VERSION "5.45"
#define VERSION "5.45"

#define MAGIC "/etc/magic"

#define HAVE_CONFIG_H 1
#define HAVE_STDINT_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_UNISTD_H 1
#define HAVE_FCNTL_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_PARAM_H 1
#define HAVE_SYS_MMAN_H 1
#define HAVE_SYS_WAIT_H 1
#define HAVE_STRING_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STDARG_H 1
#define HAVE_CTYPE_H 1
#define HAVE_LIMITS_H 1
#define HAVE_ERRNO_H 1
#define HAVE_UTIME_H 1
#define HAVE_LIBGEN_H 1
#define HAVE_GETOPT_H 1
#define HAVE_GETOPT_LONG 1
#define HAVE_STRUCT_OPTION 1
#define HAVE_STRSIGNAL 1
#define HAVE_STRCASESTR 1
#define HAVE_STRNDUP 1
#define HAVE_STRLCPY 1
#define HAVE_STRLCAT 1
#define HAVE_MEMCHR 1
#define HAVE_VASPRINTF 1
#define HAVE_ASPRINTF 1
#define HAVE_DPRINTF 1
#define HAVE_GETLINE 1
#define HAVE_MMAP 1
#define HAVE_FORK 1
#define HAVE_UTIMES 1

#define SIZEOF_UINT8_T 1
#define SIZEOF_UINT16_T 2
#define SIZEOF_UINT32_T 4
#define SIZEOF_UINT64_T 8
#define SIZEOF_INT8_T 1
#define SIZEOF_INT16_T 2
#define SIZEOF_INT32_T 4
#define SIZEOF_INT64_T 8
#define SIZEOF_OFF_T 8
#define SIZEOF_SIZE_T 8

#define STDC_HEADERS 1

#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include <getopt.h>
#include <sys/wait.h>

#endif /* _FILE_CONFIG_H */
