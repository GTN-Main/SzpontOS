/*
 * SzpontOS - rmmod (Remove Module from Kernel)
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/syscall.h>

extern int64_t __syscall2(int64_t num, int64_t a1, int64_t a2);

static int delete_module(const char *name, unsigned int flags) {
    return (int)__syscall2(SYS_delete_module, (int64_t)name, (int64_t)flags);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <module_name>\n", argv[0]);
        return 1;
    }

    const char *name = argv[1];
    int ret = delete_module(name, 0);

    if (ret != 0) {
        fprintf(stderr, "rmmod: ERROR: could not remove module '%s': error %d\n", name, ret);
        return 1;
    }

    printf("rmmod: Module '%s' removed successfully.\n", name);
    return 0;
}
