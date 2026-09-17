/*
 * ptytest - verify UNIX98 pseudo-terminal master/slave multiplexing
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/ioctl.h>

#ifndef TIOCGPTN
#define TIOCGPTN 0x80045430
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

    /* 5. Process Group & Signal (VINTR 0x03) Isolation Test */
    printf("  Testing PTY foreground process group and Ctrl+C (0x03) signal isolation...\n");
    pid_t init_pgrp = -1;
    if (ioctl(slave_fd, 0x540F /* TIOCGPGRP */, &init_pgrp) < 0) {
        perror("ptytest: TIOCGPGRP on slave failed");
        close(master_fd);
        close(slave_fd);
        return 1;
    }
    printf("  Initial PTY foreground pgrp: %d\n", (int)init_pgrp);

    int pipefd[2];
    if (pipe(pipefd) < 0) {
        perror("ptytest: pipe failed");
        close(master_fd);
        close(slave_fd);
        return 1;
    }

    pid_t child = fork();
    if (child < 0) {
        perror("ptytest: fork failed");
        close(master_fd);
        close(slave_fd);
        return 1;
    }

    if (child == 0) {
        close(pipefd[0]);
        setpgid(0, 0);
        pid_t my_pgid = getpid();
        if (ioctl(slave_fd, 0x5410 /* TIOCSPGRP */, &my_pgid) < 0) {
            perror("ptytest child: TIOCSPGRP failed");
            _exit(1);
        }

        /* Signal parent that child is ready as foreground process group */
        write(pipefd[1], "OK", 2);
        close(pipefd[1]);

        /* Sleep waiting for SIGINT */
        for (int i = 0; i < 50; i++) {
            usleep(50000);
        }
        _exit(0); /* If SIGINT not received, exit 0 (failure) */
    }

    close(pipefd[1]);
    char sync_buf[4] = {0};
    read(pipefd[0], sync_buf, 2);
    close(pipefd[0]);

    /* Send 0x03 (Ctrl+C) to PTY master */
    char ctrl_c = 0x03;
    if (write(master_fd, &ctrl_c, 1) != 1) {
        perror("ptytest: write 0x03 failed");
        close(master_fd);
        close(slave_fd);
        return 1;
    }

    int status = 0;
    waitpid(child, &status, 0);

    /* Child must have been terminated by SIGINT (signal 2) */
    if (WIFSIGNALED(status) && WTERMSIG(status) == 2) {
        printf("  [PASS] Child was terminated by SIGINT from PTY master 0x03!\n");
    } else {
        printf("  [FAIL] Child status: %d (expected WTERMSIG=2)\n", status);
        close(master_fd);
        close(slave_fd);
        return 1;
    }

    close(master_fd);
    close(slave_fd);

    printf("[PTYTEST] UNIX98 PTY/PTS multiplexing & signal isolation PASSED successfully!\n");
    return 0;
}
