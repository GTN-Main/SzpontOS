/*
 * SzpontOS - /bin/sleep utility
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: sleep <seconds>\n");
        return 1;
    }

    int seconds = atoi(argv[1]);
    if (seconds <= 0) {
        return 0;
    }

    sleep((unsigned int)seconds);
    return 0;
}
