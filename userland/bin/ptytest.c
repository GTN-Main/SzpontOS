/*
 * ptytest - verify UNIX98 pseudo-terminal master/slave multiplexing
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#ifndef TIOCGPTN
#define TIOCGPTN   0x80045430
#endif
#ifndef TIOCSPTLCK
#define TIOCSPTLCK 0x40045431
#endif

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("[PTYTEST] Testing UNIX98 Pseudo-Terminal (/dev/ptmx & /dev/ptsN)...\n");

    /* 1. Open master multiplexer /dev/ptmx */
    int master_fd = open("/dev/ptmx", O_RDWR);
    if (master_fd < 0) {
        perror("ptytest: open /dev/ptmx failed");
        return 1;
    }

    int pts_num = 0;
    int unlock = 0;

    char pts_path[32];
    snprintf(pts_path, sizeof(pts_path), "/dev/pts%d", pts_num);

    /* 2. Open slave terminal */
    int slave_fd = open(pts_path, O_RDWR);
    if (slave_fd < 0) {
        snprintf(pts_path, sizeof(pts_path), "/dev/pts/%d", pts_num);
        slave_fd = open(pts_path, O_RDWR);
    }
    if (slave_fd < 0) {
        snprintf(pts_path, sizeof(pts_path), "/dev/pts0");
        slave_fd = open(pts_path, O_RDWR);
    }

    if (slave_fd < 0) {
        perror("ptytest: open slave pts failed");
        close(master_fd);
        return 1;
    }

    printf("  Allocated master fd=%d, slave=%s (fd=%d)\n", master_fd, pts_path, slave_fd);

    /* 3. Master -> Slave communication */
    const char *ping = "HELLO_FROM_MASTER";
    if (write(master_fd, ping, strlen(ping)) <= 0) {
        fprintf(stderr, "ptytest: master write failed\n");
        close(master_fd);
        close(slave_fd);
        return 1;
    }

    char buf[64];
    memset(buf, 0, sizeof(buf));
    ssize_t n = read(slave_fd, buf, sizeof(buf) - 1);
    if (n <= 0 || strcmp(buf, ping) != 0) {
        fprintf(stderr, "ptytest: slave read mismatch (got '%s')\n", buf);
        close(master_fd);
        close(slave_fd);
        return 1;
    }
    printf("  Slave received: '%s'\n", buf);

    /* 4. Slave -> Master communication */
    const char *pong = "REPLY_FROM_SLAVE";
    if (write(slave_fd, pong, strlen(pong)) <= 0) {
        fprintf(stderr, "ptytest: slave write failed\n");
        close(master_fd);
        close(slave_fd);
        return 1;
    }

    memset(buf, 0, sizeof(buf));
    n = read(master_fd, buf, sizeof(buf) - 1);
    if (n <= 0 || strcmp(buf, pong) != 0) {
        fprintf(stderr, "ptytest: master read mismatch (got '%s')\n", buf);
        close(master_fd);
        close(slave_fd);
        return 1;
    }
    printf("  Master received: '%s'\n", buf);

    close(master_fd);
    close(slave_fd);

    printf("[PTYTEST] UNIX98 PTY/PTS multiplexing PASSED successfully!\n");
    return 0;
}
