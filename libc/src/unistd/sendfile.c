#include <sys/sendfile.h>
#include <unistd.h>
#include <errno.h>

ssize_t sendfile(int out_fd, int in_fd, off_t *offset, size_t count) {
    char buf[4096];
    size_t total = 0;
    if (offset) {
        lseek(in_fd, *offset, SEEK_SET);
    }
    while (total < count) {
        size_t to_read = count - total;
        if (to_read > sizeof(buf)) to_read = sizeof(buf);
        ssize_t nread = read(in_fd, buf, to_read);
        if (nread <= 0) break;
        ssize_t nwritten = write(out_fd, buf, (size_t)nread);
        if (nwritten <= 0) break;
        total += (size_t)nwritten;
    }
    if (offset) {
        *offset += (off_t)total;
    }
    return (ssize_t)total;
}
