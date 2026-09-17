/*
 * SzpontOS Userland - timerfd(2) Test Suite
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>

#define TEST_PASS(name) printf("  [\033[32mPASS\033[0m] %s\n", name)
#define TEST_FAIL(name, msg) do { printf("  [\033[31mFAIL\033[0m] %s: %s (errno=%d)\n", name, msg, errno); exit(1); } while (0)

int main(void) {
    printf("=== Starting SzpontOS timerfd(2) Test Suite ===\n");

    /* Test 1: timerfd_create */
    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (tfd < 0) {
        TEST_FAIL("timerfd_create", "Failed to create timerfd");
    }
    TEST_PASS("timerfd_create with CLOCK_MONOTONIC and TFD_NONBLOCK");

    /* Test 2: Read unexpired timerfd should return EAGAIN */
    uint64_t exp = 0;
    ssize_t rd = read(tfd, &exp, sizeof(exp));
    if (rd >= 0 || errno != EAGAIN) {
        TEST_FAIL("timerfd read unexpired", "Expected EAGAIN on non-blocking unexpired timerfd");
    }
    TEST_PASS("timerfd non-blocking read returns EAGAIN when disarmed");

    /* Test 3: Arm timer: 50ms initial expiration, 50ms interval */
    struct itimerspec new_val;
    new_val.it_value.tv_sec = 0;
    new_val.it_value.tv_nsec = 50 * 1000 * 1000; /* 50 ms */
    new_val.it_interval.tv_sec = 0;
    new_val.it_interval.tv_nsec = 50 * 1000 * 1000; /* 50 ms */

    struct itimerspec old_val;
    if (timerfd_settime(tfd, 0, &new_val, &old_val) != 0) {
        TEST_FAIL("timerfd_settime", "Failed to set timer");
    }
    TEST_PASS("timerfd_settime armed periodic timer (50ms interval)");

    /* Test 4: timerfd_gettime */
    struct itimerspec curr_val;
    if (timerfd_gettime(tfd, &curr_val) != 0) {
        TEST_FAIL("timerfd_gettime", "Failed to get current timer status");
    }
    if (curr_val.it_interval.tv_nsec != 50 * 1000 * 1000) {
        TEST_FAIL("timerfd_gettime interval", "Incorrect interval value returned");
    }
    TEST_PASS("timerfd_gettime successfully inspected armed timer");

    /* Test 5: Integration with epoll_wait */
    int epfd = epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0) {
        TEST_FAIL("epoll_create1", "Failed to create epoll instance");
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = tfd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, tfd, &ev) != 0) {
        TEST_FAIL("epoll_ctl ADD timerfd", "Failed to add timerfd to epoll");
    }

    struct epoll_event events[4];
    int n = epoll_wait(epfd, events, 4, 300); /* wait up to 300ms */
    if (n <= 0) {
        TEST_FAIL("epoll_wait timerfd", "Timerfd did not trigger epoll within timeout");
    }
    if (!(events[0].events & EPOLLIN)) {
        TEST_FAIL("epoll_wait events", "EPOLLIN not set on timerfd");
    }
    TEST_PASS("epoll_wait successfully woke up on timerfd expiration");

    /* Test 6: Read expirations */
    rd = read(tfd, &exp, sizeof(exp));
    if (rd != sizeof(exp) || exp < 1) {
        TEST_FAIL("timerfd read expirations", "Failed to read expirations count");
    }
    printf("     -> Read %lu expiration ticks\n", exp);
    TEST_PASS("timerfd read returned positive tick count");

    /* Test 7: Disarm timer (set value to 0) */
    new_val.it_value.tv_sec = 0;
    new_val.it_value.tv_nsec = 0;
    new_val.it_interval.tv_sec = 0;
    new_val.it_interval.tv_nsec = 0;
    if (timerfd_settime(tfd, 0, &new_val, NULL) != 0) {
        TEST_FAIL("timerfd_settime disarm", "Failed to disarm timerfd");
    }
    TEST_PASS("timerfd_settime successfully disarmed timer");

    close(epfd);
    close(tfd);
    printf("=== All timerfd(2) tests PASSED successfully! ===\n");
    return 0;
}
