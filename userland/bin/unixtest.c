/*
 * SzpontOS - Complete POSIX/Unix Compliance & Regression Test Suite
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/times.h>
#include <errno.h>

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(expr, msg) \
    do { \
        if (expr) { \
            printf("  [PASS] %s\n", msg); \
            g_tests_passed++; \
        } else { \
            printf("  [FAIL] %s (errno=%d: %s)\n", msg, errno, strerror(errno)); \
            g_tests_failed++; \
        } \
    } while (0)

static void test_relative_path_and_home(void) {
    printf("[*] Testing Relative Paths and File Creation...\n");

    /* Create file with relative path */
    int fd = open("test_rel.txt", O_CREAT | O_RDWR | O_TRUNC, 0644);
    TEST_ASSERT(fd >= 0, "open relative path ('test_rel.txt') with O_CREAT");
    if (fd >= 0) {
        const char *msg = "Hello Unix World!";
        ssize_t w = write(fd, msg, strlen(msg));
        TEST_ASSERT(w == (ssize_t)strlen(msg), "write to relative file");
        close(fd);
    }

    /* Verify with stat on relative and absolute path */
    struct stat st;
    int s = stat("test_rel.txt", &st);
    TEST_ASSERT(s == 0 && st.st_size == 17, "stat relative file 'test_rel.txt'");

    /* Access test */
    TEST_ASSERT(access("test_rel.txt", F_OK) == 0, "access(F_OK) on relative file");
    TEST_ASSERT(access("test_rel.txt", R_OK | W_OK) == 0, "access(R_OK | W_OK) on relative file");
}

static void test_rename_and_remove(void) {
    printf("[*] Testing Rename and Remove Operations...\n");

    int r = rename("test_rel.txt", "test_renamed.txt");
    TEST_ASSERT(r == 0, "rename('test_rel.txt', 'test_renamed.txt')");

    TEST_ASSERT(access("test_rel.txt", F_OK) < 0, "old file should no longer exist");
    TEST_ASSERT(access("test_renamed.txt", F_OK) == 0, "new renamed file exists");

    /* Read from renamed file */
    int fd = open("test_renamed.txt", O_RDONLY);
    TEST_ASSERT(fd >= 0, "open renamed file");
    if (fd >= 0) {
        char buf[32] = {0};
        ssize_t bytes = read(fd, buf, sizeof(buf) - 1);
        TEST_ASSERT(bytes > 0 && strcmp(buf, "Hello Unix World!") == 0, "content matches after rename");
        close(fd);
    }

    /* Clean up */
    TEST_ASSERT(unlink("test_renamed.txt") == 0, "unlink renamed file");
}

static void test_mkdir_and_rmdir(void) {
    printf("[*] Testing Directory Management (mkdir, rmdir)...\n");

    int m = mkdir("test_dir", 0755);
    TEST_ASSERT(m == 0, "mkdir('test_dir', 0755)");

    /* Create file inside new dir */
    int fd = open("test_dir/file.txt", O_CREAT | O_WRONLY, 0644);
    TEST_ASSERT(fd >= 0, "create file inside new directory");
    if (fd >= 0) {
        write(fd, "test", 4);
        close(fd);
    }

    /* rmdir on non-empty directory should fail */
    int r_fail = rmdir("test_dir");
    TEST_ASSERT(r_fail < 0, "rmdir on non-empty directory fails (ENOTEMPTY)");

    /* Unlink file and rmdir should succeed */
    unlink("test_dir/file.txt");
    int r_ok = rmdir("test_dir");
    TEST_ASSERT(r_ok == 0, "rmdir on empty directory succeeds");
}

static void test_symlinks(void) {
    printf("[*] Testing Symlinks (symlink, readlink)...\n");

    /* Create target file */
    int fd = open("target.txt", O_CREAT | O_WRONLY, 0644);
    if (fd >= 0) {
        write(fd, "symlink_target_data", 19);
        close(fd);
    }

    int sym = symlink("target.txt", "link.txt");
    TEST_ASSERT(sym == 0, "symlink('target.txt', 'link.txt')");

    char target_buf[64] = {0};
    ssize_t rl = readlink("link.txt", target_buf, sizeof(target_buf) - 1);
    TEST_ASSERT(rl > 0 && strcmp(target_buf, "target.txt") == 0, "readlink retrieves exact target path");

    /* Access through symlink */
    int fd_link = open("link.txt", O_RDONLY);
    TEST_ASSERT(fd_link >= 0, "open file via symlink");
    if (fd_link >= 0) {
        char buf[32] = {0};
        read(fd_link, buf, sizeof(buf) - 1);
        TEST_ASSERT(strcmp(buf, "symlink_target_data") == 0, "read content through symlink");
        close(fd_link);
    }

    unlink("link.txt");
    unlink("target.txt");
}

static void test_truncate_and_pread(void) {
    printf("[*] Testing Truncate and Positional I/O (pread, pwrite)...\n");

    int fd = open("trunc_test.txt", O_CREAT | O_RDWR | O_TRUNC, 0644);
    TEST_ASSERT(fd >= 0, "open trunc_test.txt");
    if (fd >= 0) {
        pwrite(fd, "0123456789ABCDEF", 16, 0);

        char pbuf[5] = {0};
        ssize_t pr = pread(fd, pbuf, 4, 10);
        TEST_ASSERT(pr == 4 && strcmp(pbuf, "ABCD") == 0, "pread 4 bytes from offset 10");

        /* ftruncate to 8 bytes */
        ftruncate(fd, 8);
        struct stat st;
        fstat(fd, &st);
        TEST_ASSERT(st.st_size == 8, "ftruncate shrank size to 8 bytes");

        close(fd);
    }

    /* truncate by path to 4 bytes */
    truncate("trunc_test.txt", 4);
    struct stat st;
    stat("trunc_test.txt", &st);
    TEST_ASSERT(st.st_size == 4, "truncate shrank size to 4 bytes");

    unlink("trunc_test.txt");
}

static void test_umask(void) {
    printf("[*] Testing Process Umask...\n");

    mode_t old_mask = umask(0077);
    TEST_ASSERT(old_mask != (mode_t)-1, "umask(0077) returns previous mask");

    int fd = open("umask_test.txt", O_CREAT | O_WRONLY, 0777);
    if (fd >= 0) {
        close(fd);
    }

    struct stat st;
    stat("umask_test.txt", &st);
    /* Permissions should be 0777 & ~0077 = 0700 */
    mode_t masked_perm = st.st_mode & 0777;
    TEST_ASSERT(masked_perm == 0700, "file created with umask 0077 has 0700 permissions");

    unlink("umask_test.txt");
    umask(old_mask);
}

static void test_system_and_limits(void) {
    printf("[*] Testing Limits, Times, and Process IDs...\n");

    struct rlimit rl;
    int r = getrlimit(RLIMIT_NOFILE, &rl);
    TEST_ASSERT(r == 0 && rl.rlim_cur > 0, "getrlimit(RLIMIT_NOFILE)");

    struct tms tm;
    clock_t clk = times(&tm);
    TEST_ASSERT(clk >= 0, "times() retrieves tick count");

    pid_t pgrp = getpgrp();
    TEST_ASSERT(pgrp > 0, "getpgrp() returns valid group ID");

    unsigned int prev_alarm = alarm(10);
    alarm(0); /* Cancel */
    TEST_ASSERT(prev_alarm == 0, "alarm() call succeeds");
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("====================================================\n");
    printf(" SzpontOS Unix Compatibility & Regression Test Suite\n");
    printf(" (C) Copyright by Szpont Industries. All rights reserved.\n");
    printf("====================================================\n\n");

    test_relative_path_and_home();
    test_rename_and_remove();
    test_mkdir_and_rmdir();
    test_symlinks();
    test_truncate_and_pread();
    test_umask();
    test_system_and_limits();

    printf("\n====================================================\n");
    printf(" Results: %d Passed, %d Failed\n", g_tests_passed, g_tests_failed);
    printf("====================================================\n");

    return (g_tests_failed == 0) ? 0 : 1;
}
