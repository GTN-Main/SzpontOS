/*
 * SzpontOS - shutdown (System shutdown & poweroff utility)
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/reboot.h>

static void usage(void) {
    fprintf(stderr, "Usage: shutdown [-h | -r | -P] [time]\n");
    fprintf(stderr, "  -h, -P  Power off / halt the machine (default)\n");
    fprintf(stderr, "  -r      Reboot the machine\n");
}

int main(int argc, char *argv[]) {
    bool do_reboot = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--reboot") == 0) {
            do_reboot = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-P") == 0 || strcmp(argv[i], "--poweroff") == 0) {
            do_reboot = false;
        } else if (strcmp(argv[i], "--help") == 0) {
            usage();
            return 0;
        } else if (strcmp(argv[i], "now") == 0) {
            /* Immediate shutdown */
        }
    }

    if (do_reboot) {
        printf("\033[1;33mThe system is going down for reboot NOW!\033[0m\n");
        sync();
        reboot(RB_AUTOBOOT);
    } else {
        printf("\033[1;31mThe system is going down for poweroff NOW!\033[0m\n");
        sync();
        reboot(RB_POWER_OFF);
    }

    return 0;
}
