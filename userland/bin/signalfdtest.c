/*
 * SzpontOS Userland - signalfd(2) Test Suite
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/epoll.h>

#define TEST_PASS(name) printf("  [\033[32mPASS\033[0m] %s\n", name)
#define TEST_FAIL(name, msg) do { printf("  [\033[31mFAIL\033[0m] %s: %s (errno=%d)\n", name, msg, errno); exit(1); } while (0)

int main(void) {
    printf("=== Starting SzpontOS signalfd(2) Test Suite ===\n");

    /* Step 1: Block SIGUSR1 and SIGUSR2 so signalfd can receive them */
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);

    if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) {
        TEST_FAIL("sigprocmask", "Failed to block signals");
    }
    TEST_PASS("sigprocmask blocked SIGUSR1 and SIGUSR2");

    /* Step 2: Create signalfd with SFD_NONBLOCK */
    int sfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (sfd < 0) {
        TEST_FAIL("signalfd create", "Failed to create signalfd");
    }
    TEST_PASS("signalfd created descriptor with SFD_NONBLOCK and SFD_CLOEXEC");

    /* Step 3: Non-blocking read when no signal is pending -> EAGAIN */
    struct signalfd_siginfo fdsi;
    ssize_t s = read(sfd, &fdsi, sizeof(struct signalfd_siginfo));
    if (s >= 0 || errno != EAGAIN) {
        TEST_FAIL("signalfd read empty", "Expected EAGAIN when no signal is pending");
    }
    TEST_PASS("signalfd returns EAGAIN on empty non-blocking read");

    /* Step 4: Epoll integration */
    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0) {
        TEST_FAIL("epoll_create1", "Failed to create epoll instance");
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = sfd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, sfd, &ev) != 0) {
        TEST_FAIL("epoll_ctl ADD signalfd", "Failed to add signalfd to epoll");
    }

    /* Step 5: Raise SIGUSR1 and observe epoll trigger */
    kill(getpid(), SIGUSR1);

    struct epoll_event events[4];
    int n = epoll_wait(epfd, events, 4, 500);
    if (n <= 0) {
        TEST_FAIL("epoll_wait signalfd", "signalfd did not trigger epoll on pending signal");
    }
    if (!(events[0].events & EPOLLIN)) {
        TEST_FAIL("epoll_wait event flags", "EPOLLIN not set on signalfd event");
    }
    TEST_PASS("epoll_wait woke up on pending SIGUSR1 via signalfd");

    /* Step 6: Read signal information from signalfd */
    s = read(sfd, &fdsi, sizeof(struct signalfd_siginfo));
    if (s != sizeof(struct signalfd_siginfo)) {
        TEST_FAIL("signalfd read info", "Failed to read struct signalfd_siginfo");
    }
    if (fdsi.ssi_signo != SIGUSR1) {
        TEST_FAIL("signalfd ssi_signo", "Expected ssi_signo == SIGUSR1");
    }
    printf("     -> Received signal %u from PID %u\n", fdsi.ssi_signo, fdsi.ssi_pid);
    TEST_PASS("signalfd read returned correct signalfd_siginfo for SIGUSR1");

    /* Step 7: Raise SIGUSR2 and read directly */
    kill(getpid(), SIGUSR2);
    s = read(sfd, &fdsi, sizeof(struct signalfd_siginfo));
    if (s != sizeof(struct signalfd_siginfo) || fdsi.ssi_signo != SIGUSR2) {
        TEST_FAIL("signalfd read SIGUSR2", "Failed to read SIGUSR2");
    }
    printf("     -> Received signal %u from PID %u\n", fdsi.ssi_signo, fdsi.ssi_pid);
    TEST_PASS("signalfd read returned correct signalfd_siginfo for SIGUSR2");

    close(epfd);
    close(sfd);

    /* Unblock signals */
    sigprocmask(SIG_UNBLOCK, &mask, NULL);

    printf("=== All signalfd(2) tests PASSED successfully! ===\n");
    return 0;
}
