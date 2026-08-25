#include <unistd.h>
#include <errno.h>
#include <sys/sysinfo.h>

long sysconf(int name) {
    switch (name) {
    case _SC_ARG_MAX:
        return 4096;
    case _SC_CHILD_MAX:
        return 256;
    case _SC_CLK_TCK:
        return 100;
    case _SC_NGROUPS_MAX:
        return 32;
    case _SC_OPEN_MAX:
        return 1024;
    case _SC_STREAM_MAX:
        return 64;
    case _SC_PAGESIZE:
        return 4096;
    case _SC_NPROCESSORS_CONF:
    case _SC_NPROCESSORS_ONLN:
        return get_nprocs();
    case _SC_PHYS_PAGES:
        return get_phys_pages();
    case _SC_AVPHYS_PAGES:
        return get_avphys_pages();
    case _SC_GETPW_R_SIZE_MAX:
    case _SC_GETGR_R_SIZE_MAX:
        return 1024;
    default:
        errno = EINVAL;
        return -1;
    }
}
