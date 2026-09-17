#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    printf("[EPOLLTEST] Starting epoll subsystem test suite...\n");

    /* 1. Create epoll instance */
    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0) {
        printf("[EPOLLTEST] FAIL: epoll_create1 failed: %d\n", errno);
        return 1;
    }
    printf("[EPOLLTEST] PASS: epoll_create1 returned fd %d\n", epfd);

    /* 2. Create non-blocking pipe */
    int pfd[2];
    if (pipe2(pfd, O_NONBLOCK | O_CLOEXEC) < 0) {
        printf("[EPOLLTEST] FAIL: pipe2 failed: %d\n", errno);
        close(epfd);
        return 1;
    }
    printf("[EPOLLTEST] PASS: pipe2 created fds [%d, %d]\n", pfd[0], pfd[1]);

    /* 3. Register pipe read-end with EPOLLET | EPOLLIN */
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = pfd[0];

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, pfd[0], &ev) < 0) {
        printf("[EPOLLTEST] FAIL: epoll_ctl ADD failed: %d\n", errno);
        close(pfd[0]);
        close(pfd[1]);
        close(epfd);
        return 1;
    }
    printf("[EPOLLTEST] PASS: epoll_ctl ADD registered pfd[0]\n");

    /* 4. Initially no data, epoll_wait should return 0 */
    struct epoll_event events[4];
    int nfds = epoll_wait(epfd, events, 4, 10);
    if (nfds != 0) {
        printf("[EPOLLTEST] FAIL: epoll_wait expected 0 events, got %d\n", nfds);
        return 1;
    }
    printf("[EPOLLTEST] PASS: epoll_wait on empty pipe returned 0\n");

    /* 5. Write to pipe and wait */
    const char msg[] = "SzpontOS-epoll-test";
    ssize_t written = write(pfd[1], msg, sizeof(msg));
    if (written != sizeof(msg)) {
        printf("[EPOLLTEST] FAIL: write to pipe failed\n");
        return 1;
    }

    nfds = epoll_wait(epfd, events, 4, 1000);
    if (nfds != 1 || events[0].data.fd != pfd[0] || !(events[0].events & EPOLLIN)) {
        printf("[EPOLLTEST] FAIL: epoll_wait expected 1 event on pfd[0], got nfds=%d, fd=%d, events=0x%x\n",
               nfds, events[0].data.fd, events[0].events);
        return 1;
    }
    printf("[EPOLLTEST] PASS: epoll_wait successfully received EPOLLIN on pfd[0]\n");

    /* 6. Verify Edge-Triggered behavior: without reading, subsequent epoll_wait returns 0 */
    nfds = epoll_wait(epfd, events, 4, 0);
    if (nfds != 0) {
        printf("[EPOLLTEST] FAIL: EPOLLET re-notified without state transition (got %d events)\n", nfds);
        return 1;
    }
    printf("[EPOLLTEST] PASS: EPOLLET edge-triggered semantics validated (no duplicate notification)\n");

    /* 7. Drain pipe */
    char buf[64];
    ssize_t nread = read(pfd[0], buf, sizeof(buf));
    if (nread != sizeof(msg) || memcmp(buf, msg, sizeof(msg)) != 0) {
        printf("[EPOLLTEST] FAIL: read from pipe corrupted or incomplete\n");
        return 1;
    }
    printf("[EPOLLTEST] PASS: Read %zd bytes from pipe: '%s'\n", nread, buf);

    /* 8. Modify subscription to add EPOLLOUT (though pipe read-end won't trigger EPOLLOUT) */
    ev.events = EPOLLIN;
    ev.data.fd = pfd[0];
    if (epoll_ctl(epfd, EPOLL_CTL_MOD, pfd[0], &ev) < 0) {
        printf("[EPOLLTEST] FAIL: epoll_ctl MOD failed\n");
        return 1;
    }
    printf("[EPOLLTEST] PASS: epoll_ctl MOD succeeded\n");

    /* 9. Delete subscription */
    if (epoll_ctl(epfd, EPOLL_CTL_DEL, pfd[0], NULL) < 0) {
        printf("[EPOLLTEST] FAIL: epoll_ctl DEL failed\n");
        return 1;
    }
    printf("[EPOLLTEST] PASS: epoll_ctl DEL succeeded\n");

    /* Clean up */
    close(pfd[0]);
    close(pfd[1]);
    close(epfd);

    printf("[EPOLLTEST] ALL TESTS PASSED!\n");
    return 0;
}
