#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/statvfs.h>

static void format_size(uint64_t bytes, char *buf, size_t buf_size) {
    if (bytes >= 1024 * 1024 * 1024) {
        snprintf(buf, buf_size, "%luG", bytes / (1024 * 1024 * 1024));
    } else if (bytes >= 1024 * 1024) {
        snprintf(buf, buf_size, "%luM", bytes / (1024 * 1024));
    } else if (bytes >= 1024) {
        snprintf(buf, buf_size, "%luK", bytes / 1024);
    } else {
        snprintf(buf, buf_size, "%luB", bytes);
    }
}

static void print_df_entry(const char *fs_name, const char *mount_point, int human) {
    struct statfs s;
    if (statfs(mount_point, &s) != 0)
        return;

    uint64_t total_bytes = s.f_blocks * s.f_bsize;
    uint64_t free_bytes = s.f_bfree * s.f_bsize;
    uint64_t used_bytes = (total_bytes >= free_bytes) ? (total_bytes - free_bytes) : 0;
    int use_pct = (total_bytes > 0) ? (int)((used_bytes * 100) / total_bytes) : 0;

    if (human) {
        char s_total[16], s_used[16], s_avail[16];
        format_size(total_bytes, s_total, sizeof(s_total));
        format_size(used_bytes, s_used, sizeof(s_used));
        format_size(free_bytes, s_avail, sizeof(s_avail));
        printf("%-15s %8s %8s %8s %4d%%  %s\n", fs_name, s_total, s_used, s_avail, use_pct, mount_point);
    } else {
        uint64_t total_kb = total_bytes / 1024;
        uint64_t used_kb = used_bytes / 1024;
        uint64_t avail_kb = free_bytes / 1024;
        printf("%-15s %10lu %10lu %10lu %4d%%  %s\n", fs_name, total_kb, used_kb, avail_kb, use_pct, mount_point);
    }
}

int main(int argc, char *argv[]) {
    int human = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--human-readable") == 0) {
            human = 1;
        }
    }

    if (human) {
        printf("%-15s %8s %8s %8s %5s  %s\n", "Filesystem", "Size", "Used", "Avail", "Use%", "Mounted on");
    } else {
        printf("%-15s %10s %10s %10s %5s  %s\n", "Filesystem", "1K-blocks", "Used", "Available", "Use%", "Mounted on");
    }

    print_df_entry("rootfs", "/", human);
    print_df_entry("devfs", "/dev", human);
    print_df_entry("/dev/hda", "/mnt", human);

    return 0;
}
