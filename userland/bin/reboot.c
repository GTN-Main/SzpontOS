/*
 * SzpontOS - reboot (Hardware system reboot utility)
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/reboot.h>

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("\033[1;33mRebooting SzpontOS system...\033[0m\n");
    sync();

    if (reboot(RB_AUTOBOOT) != 0) {
        perror("reboot");
        return 1;
    }

    return 0;
}
