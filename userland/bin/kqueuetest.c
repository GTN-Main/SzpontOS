/*
 * kqueuetest - verify BSD kqueue & kevent event notification
 * Inspired by FreeBSD kqueue(2) test suite
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/event.h>
#include <sys/time.h>

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("[KQUEUETEST] Testing BSD kqueue() & kevent()...\n");

    /* 1. Create kqueue descriptor */
    int kq = kqueue();
    if (kq < 0) {
        perror("kqueuetest: kqueue() failed");
        return 1;
    }
    printf("  Created kqueue descriptor fd=%d\n", kq);

    /* 2. Test EVFILT_TIMER */
    struct kevent change_event;
    struct kevent event;
    struct timespec timeout;

    /* Register 50ms periodic timer */
    EV_SET(&change_event, 1, EVFILT_TIMER, EV_ADD | EV_ENABLE, 0, 50, (void*)0x1234);

    timeout.tv_sec = 1;
    timeout.tv_nsec = 0;

    int nev = kevent(kq, &change_event, 1, &event, 1, &timeout);
    if (nev <= 0) {
        fprintf(stderr, "kqueuetest: kevent timer wait failed (returned %d)\n", nev);
        close(kq);
        return 1;
    }

    printf("  Triggered EVFILT_TIMER event: ident=%lu, filter=%d, udata=%p\n",
           (unsigned long)event.ident, (int)event.filter, event.udata);

    /* 3. Test EVFILT_READ on pipe */
    int pipefds[2];
    if (pipe(pipefds) != 0) {
        perror("kqueuetest: pipe() failed");
        close(kq);
        return 1;
    }

    EV_SET(&change_event, pipefds[0], EVFILT_READ, EV_ADD | EV_ENABLE | EV_ONESHOT, 0, 0, (void*)0x5678);
    kevent(kq, &change_event, 1, NULL, 0, NULL);

    /* Write data into pipe */
    const char *msg = "KQUEUE_NOTIFY";
    write(pipefds[1], msg, strlen(msg));

    timeout.tv_sec = 1;
    timeout.tv_nsec = 0;

    memset(&event, 0, sizeof(event));
    nev = kevent(kq, NULL, 0, &event, 1, &timeout);
    if (nev <= 0) {
        fprintf(stderr, "kqueuetest: kevent pipe read wait failed (returned %d)\n", nev);
        close(pipefds[0]);
        close(pipefds[1]);
        close(kq);
        return 1;
    }

    printf("  Triggered EVFILT_READ on pipe: ident=%lu (fd=%d), udata=%p, data=%ld bytes\n",
           (unsigned long)event.ident, pipefds[0], event.udata, (long)event.data);

    char read_buf[32];
    memset(read_buf, 0, sizeof(read_buf));
    read(pipefds[0], read_buf, sizeof(read_buf) - 1);
    printf("  Read from pipe: '%s'\n", read_buf);

    close(pipefds[0]);
    close(pipefds[1]);
    close(kq);

    printf("[KQUEUETEST] BSD kqueue & kevent PASSED successfully!\n");
    return 0;
}
