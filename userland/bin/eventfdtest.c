#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <poll.h>
#include <errno.h>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    printf("[EVENTFDTEST] Starting eventfd subsystem test suite...\n");

    /* 1. Test standard non-blocking eventfd */
    int efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (efd < 0) {
        printf("[EVENTFDTEST] FAIL: eventfd creation failed: %d\n", errno);
        return 1;
    }
    printf("[EVENTFDTEST] PASS: eventfd created (fd %d)\n", efd);

    eventfd_t val = 0;
    int ret = eventfd_read(efd, &val);
    if (ret != -1 || errno != EAGAIN) {
        printf("[EVENTFDTEST] FAIL: eventfd_read on empty nonblock fd expected EAGAIN, got ret=%d, errno=%d\n", ret, errno);
        close(efd);
        return 1;
    }
    printf("[EVENTFDTEST] PASS: Empty eventfd returned EAGAIN on non-blocking read\n");

    /* 2. Write 42 to eventfd */
    if (eventfd_write(efd, 42) != 0) {
        printf("[EVENTFDTEST] FAIL: eventfd_write failed: %d\n", errno);
        close(efd);
        return 1;
    }
    printf("[EVENTFDTEST] PASS: Wrote 42 to eventfd\n");

    /* 3. Read value back */
    val = 0;
    if (eventfd_read(efd, &val) != 0 || val != 42) {
        printf("[EVENTFDTEST] FAIL: eventfd_read expected 42, got %llu (errno %d)\n", (unsigned long long)val, errno);
        close(efd);
        return 1;
    }
    printf("[EVENTFDTEST] PASS: Read 42 from eventfd, counter reset to 0\n");

    close(efd);

    /* 4. Test semaphore mode */
    int sem_efd = eventfd(3, EFD_NONBLOCK | EFD_SEMAPHORE | EFD_CLOEXEC);
    if (sem_efd < 0) {
        printf("[EVENTFDTEST] FAIL: eventfd semaphore creation failed: %d\n", errno);
        return 1;
    }

    val = 0;
    if (eventfd_read(sem_efd, &val) != 0 || val != 1) {
        printf("[EVENTFDTEST] FAIL: Semaphore mode read expected 1, got %llu\n", (unsigned long long)val);
        close(sem_efd);
        return 1;
    }
    printf("[EVENTFDTEST] PASS: Semaphore mode read 1 (counter 3 -> 2)\n");

    val = 0;
    if (eventfd_read(sem_efd, &val) != 0 || val != 1) {
        printf("[EVENTFDTEST] FAIL: Semaphore mode second read expected 1, got %llu\n", (unsigned long long)val);
        close(sem_efd);
        return 1;
    }
    printf("[EVENTFDTEST] PASS: Semaphore mode read 1 (counter 2 -> 1)\n");

    /* 5. Test poll integration */
    struct pollfd pfd;
    pfd.fd = sem_efd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    int pcount = poll(&pfd, 1, 100);
    if (pcount != 1 || !(pfd.revents & POLLIN)) {
        printf("[EVENTFDTEST] FAIL: poll on eventfd with counter=1 expected POLLIN, got pcount=%d, revents=0x%x\n",
               pcount, pfd.revents);
        close(sem_efd);
        return 1;
    }
    printf("[EVENTFDTEST] PASS: poll() returned POLLIN on non-zero eventfd\n");

    /* Read the last count */
    eventfd_read(sem_efd, &val);

    /* Now counter is 0, poll should return 0 */
    pfd.revents = 0;
    pcount = poll(&pfd, 1, 10);
    if (pcount != 0) {
        printf("[EVENTFDTEST] FAIL: poll on drained eventfd expected 0, got %d\n", pcount);
        close(sem_efd);
        return 1;
    }
    printf("[EVENTFDTEST] PASS: poll() returned 0 on empty eventfd\n");

    close(sem_efd);
    printf("[EVENTFDTEST] ALL TESTS PASSED!\n");
    return 0;
}
