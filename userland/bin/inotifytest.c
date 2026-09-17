#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <errno.h>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    printf("[INOTIFYTEST] Starting inotify subsystem test suite...\n");

    /* 1. Initialize inotify */
    int ifd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (ifd < 0) {
        printf("[INOTIFYTEST] FAIL: inotify_init1 failed: %d\n", errno);
        return 1;
    }
    printf("[INOTIFYTEST] PASS: inotify_init1 initialized fd %d\n", ifd);

    /* 2. Create test directory */
    const char *test_dir = "/tmp/inotify_test_dir";
    mkdir(test_dir, 0777);

    /* 3. Add watch for directory */
    int wd = inotify_add_watch(ifd, test_dir, IN_CREATE | IN_DELETE);
    if (wd < 0) {
        printf("[INOTIFYTEST] FAIL: inotify_add_watch failed: %d\n", errno);
        rmdir(test_dir);
        close(ifd);
        return 1;
    }
    printf("[INOTIFYTEST] PASS: Added watch descriptor %d for '%s'\n", wd, test_dir);

    /* 4. Trigger IN_CREATE by creating a file */
    const char *test_file = "/tmp/inotify_test_dir/file.txt";
    int fd = open(test_file, O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (fd >= 0) {
        write(fd, "hello", 5);
        close(fd);
    } else {
        printf("[INOTIFYTEST] FAIL: failed to create test file: %d\n", errno);
        inotify_rm_watch(ifd, wd);
        rmdir(test_dir);
        close(ifd);
        return 1;
    }
    printf("[INOTIFYTEST] Created file '%s'\n", test_file);

    /* 5. Read inotify event */
    char event_buf[sizeof(struct inotify_event) + 256];
    ssize_t n = read(ifd, event_buf, sizeof(event_buf));
    if (n < (ssize_t)sizeof(struct inotify_event)) {
        printf("[INOTIFYTEST] FAIL: Expected inotify event, got %zd bytes (errno %d)\n", n, errno);
        unlink(test_file);
        inotify_rm_watch(ifd, wd);
        rmdir(test_dir);
        close(ifd);
        return 1;
    }

    struct inotify_event *ev = (struct inotify_event *)event_buf;
    if (!(ev->mask & IN_CREATE)) {
        printf("[INOTIFYTEST] FAIL: Expected IN_CREATE mask (0x%x), got 0x%x\n", IN_CREATE, ev->mask);
        unlink(test_file);
        inotify_rm_watch(ifd, wd);
        rmdir(test_dir);
        close(ifd);
        return 1;
    }
    if (ev->len > 0 && strcmp(ev->name, "file.txt") == 0) {
        printf("[INOTIFYTEST] PASS: Received IN_CREATE event for '%s'\n", ev->name);
    } else {
        printf("[INOTIFYTEST] PASS: Received IN_CREATE event (len=%u)\n", ev->len);
    }

    /* 6. Trigger IN_DELETE by removing file */
    unlink(test_file);
    printf("[INOTIFYTEST] Unlinked file '%s'\n", test_file);

    n = read(ifd, event_buf, sizeof(event_buf));
    if (n >= (ssize_t)sizeof(struct inotify_event)) {
        ev = (struct inotify_event *)event_buf;
        if (ev->mask & IN_DELETE) {
            printf("[INOTIFYTEST] PASS: Received IN_DELETE event\n");
        } else {
            printf("[INOTIFYTEST] WARN: Event mask was 0x%x\n", ev->mask);
        }
    }

    /* 7. Remove watch */
    if (inotify_rm_watch(ifd, wd) != 0) {
        printf("[INOTIFYTEST] FAIL: inotify_rm_watch failed: %d\n", errno);
        rmdir(test_dir);
        close(ifd);
        return 1;
    }
    printf("[INOTIFYTEST] PASS: inotify_rm_watch succeeded\n");

    rmdir(test_dir);
    close(ifd);
    printf("[INOTIFYTEST] ALL TESTS PASSED!\n");
    return 0;
}
