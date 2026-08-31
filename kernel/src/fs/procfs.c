/*
 * SzpontOS - ProcFS (Virtual Process & System Information Filesystem)
 * (C) Copyright by Szpont Industries. All rights reserved.
 */

#include <fs/procfs.h>
#include <fs/vfs.h>
#include <sched/process.h>
#include <sched/sched.h>
#include <mm/pmm.h>
#include <mm/heap.h>
#include <kernel/string.h>
#include <kernel/kprint.h>
#include <kernel/module.h>
#include <kernel/sysctl.h>
#include <drivers/pci.h>
#include <drivers/xhci.h>
#include <drivers/ehci.h>
#include <arch/x86_64/pit.h>

#define PROCFS_TYPE_ROOT 0
#define PROCFS_TYPE_PID_DIR 1
#define PROCFS_TYPE_MEMINFO 2
#define PROCFS_TYPE_CPUINFO 3
#define PROCFS_TYPE_VERSION 4
#define PROCFS_TYPE_UPTIME 5
#define PROCFS_TYPE_STAT 6
#define PROCFS_TYPE_LOADAVG 7
#define PROCFS_TYPE_MOUNTS 8
#define PROCFS_TYPE_FILESYSTEMS 9
#define PROCFS_TYPE_CMDLINE 10
#define PROCFS_TYPE_DEVICES 11
#define PROCFS_TYPE_SELF 12
#define PROCFS_TYPE_MODULES 13
#define PROCFS_TYPE_DMESG 14
#define PROCFS_TYPE_SYSCTL 15
#define PROCFS_TYPE_PCI 16
#define PROCFS_TYPE_USB 17

#define PROCFS_PID_STATUS 100
#define PROCFS_PID_CMDLINE 101
#define PROCFS_PID_COMM 102
#define PROCFS_PID_CWD 103
#define PROCFS_PID_EXE 104
#define PROCFS_PID_STAT 105
#define PROCFS_PID_STATM 106
#define PROCFS_PID_MAPS 107
#define PROCFS_PID_ENVIRON 108

typedef struct procfs_file_data {
    uint32_t type;
    pid_t pid;
} procfs_file_data_t;

static vfs_ops_t g_procfs_root_ops;
static vfs_ops_t g_procfs_pid_dir_ops;
static vfs_ops_t g_procfs_file_ops;
static vfs_node_t *g_procfs_root = NULL;

static inline void cpuid(uint32_t leaf, uint32_t subleaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
    __asm__ volatile("cpuid" : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx) : "a"(leaf), "c"(subleaf));
}

/* =========================================================================
 * Content Generators for System-Wide Files
 * ========================================================================= */

static size_t procfs_gen_meminfo(char *buf, size_t max_len) {
    uint64_t total = pmm_get_total_memory() / 1024;
    uint64_t free_k = pmm_get_free_memory() / 1024;
    uint64_t avail = free_k;
    uint64_t buffers = 256;
    uint64_t cached = 0;

    return ksnprintf(buf, max_len,
                     "MemTotal:       %8lu kB\n"
                     "MemFree:        %8lu kB\n"
                     "MemAvailable:   %8lu kB\n"
                     "Buffers:        %8lu kB\n"
                     "Cached:         %8lu kB\n"
                     "SwapCached:            0 kB\n"
                     "Active:         %8lu kB\n"
                     "Inactive:              0 kB\n"
                     "SwapTotal:             0 kB\n"
                     "SwapFree:              0 kB\n"
                     "Dirty:                 0 kB\n"
                     "Writeback:             0 kB\n"
                     "AnonPages:      %8lu kB\n"
                     "Mapped:         %8lu kB\n"
                     "Shmem:                 0 kB\n"
                     "Slab:               8192 kB\n",
                     total, free_k, avail, buffers, cached, (total > free_k ? total - free_k : 0),
                     (total > free_k ? total - free_k : 0), (total > free_k ? total - free_k : 0));
}

static size_t procfs_gen_cpuinfo(char *buf, size_t max_len) {
    uint32_t eax, ebx, ecx, edx;
    char vendor[13] = {0};
    cpuid(0, 0, &eax, (uint32_t *)&vendor[0], (uint32_t *)&vendor[8], (uint32_t *)&vendor[4]);

    char brand[49] = {0};
    uint32_t max_ext = 0;
    cpuid(0x80000000, 0, &max_ext, &ebx, &ecx, &edx);
    if (max_ext >= 0x80000004) {
        cpuid(0x80000002, 0, (uint32_t *)&brand[0], (uint32_t *)&brand[4], (uint32_t *)&brand[8],
              (uint32_t *)&brand[12]);
        cpuid(0x80000003, 0, (uint32_t *)&brand[16], (uint32_t *)&brand[20], (uint32_t *)&brand[24],
              (uint32_t *)&brand[28]);
        cpuid(0x80000004, 0, (uint32_t *)&brand[32], (uint32_t *)&brand[36], (uint32_t *)&brand[40],
              (uint32_t *)&brand[44]);
    } else {
        strcpy(brand, "x86_64 Virtual / Compatible Processor");
    }

    /* CPU Family / Model */
    uint32_t family = 6, model = 1, stepping = 0;
    uint32_t f_edx = 0, f_ecx = 0;
    if (eax >= 1) {
        cpuid(1, 0, &eax, &ebx, &f_ecx, &f_edx);
        stepping = eax & 0xF;
        model = (eax >> 4) & 0xF;
        family = (eax >> 8) & 0xF;
    }

    return ksnprintf(buf, max_len,
                     "processor\t: 0\n"
                     "vendor_id\t: %s\n"
                     "cpu family\t: %u\n"
                     "model\t\t: %u\n"
                     "model name\t: %s\n"
                     "stepping\t: %u\n"
                     "cpu MHz\t\t: 2400.000\n"
                     "cache size\t: 4096 KB\n"
                     "fpu\t\t: yes\n"
                     "flags\t\t: fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx "
                     "fxsr sse sse2 sse3 ssse3 sse4_1 sse4_2 avx\n"
                     "bogomips\t: 4800.00\n"
                     "clflush size\t: 64\n"
                     "address sizes\t: 48 bits physical, 48 bits virtual\n\n",
                     vendor, family, model, brand, stepping);
}

static size_t procfs_gen_version(char *buf, size_t max_len) {
    return ksnprintf(buf, max_len, "SzpontOS version 0.1.0 (x86_64 Higher-Half) (gcc 13.2.0, Limine 8.x) #1 PREEMPT\n");
}

static size_t procfs_gen_uptime(char *buf, size_t max_len) {
    uint64_t ticks = pit_get_ticks();
    uint64_t freq = pit_get_frequency();
    uint64_t sec = ticks / freq;
    uint64_t csec = (ticks % freq) * 100 / freq;
    return ksnprintf(buf, max_len, "%lu.%02lu %lu.%02lu\n", sec, csec, sec, csec);
}

static size_t procfs_gen_stat(char *buf, size_t max_len) {
    uint64_t ticks = pit_get_ticks();
    uint64_t user = ticks / 2;
    uint64_t sys = ticks / 4;
    uint64_t idle = ticks - user - sys;

    proc_info_t procs[64];
    size_t count = process_get_list(procs, 64);

    return ksnprintf(buf, max_len,
                     "cpu  %lu 0 %lu %lu 0 0 0 0 0 0\n"
                     "cpu0 %lu 0 %lu %lu 0 0 0 0 0 0\n"
                     "intr %lu\n"
                     "ctxt %lu\n"
                     "btime 1700000000\n"
                     "processes %lu\n"
                     "procs_running 1\n"
                     "procs_blocked 0\n",
                     user, sys, idle, user, sys, idle, ticks * 2, ticks, count);
}

static size_t procfs_gen_loadavg(char *buf, size_t max_len) {
    proc_info_t procs[64];
    size_t count = process_get_list(procs, 64);
    return ksnprintf(buf, max_len, "0.01 0.01 0.00 1/%lu 42\n", count);
}

static size_t procfs_gen_mounts(char *buf, size_t max_len) {
    vfs_mount_info_t mounts[32];
    size_t count = vfs_get_mount_list(mounts, 32);
    size_t pos = 0;

    for (size_t i = 0; i < count && pos < max_len; i++) {
        const char *fs_type = "rootfs";
        const char *dev_name = mounts[i].name;

        if (strcmp(mounts[i].path, "/dev") == 0)
            fs_type = "devfs";
        else if (strcmp(mounts[i].path, "/proc") == 0)
            fs_type = "procfs";
        else if (strcmp(mounts[i].path, "/mnt") == 0)
            fs_type = "ext2";

        pos += ksnprintf(buf + pos, max_len - pos, "%s %s %s rw,relatime 0 0\n", dev_name, mounts[i].path, fs_type);
    }
    return pos;
}

static size_t procfs_gen_filesystems(char *buf, size_t max_len) {
    return ksnprintf(buf, max_len,
                     "nodev\trootfs\n"
                     "nodev\tdevfs\n"
                     "nodev\tprocfs\n"
                     "\text2\n");
}

static size_t procfs_gen_cmdline(char *buf, size_t max_len) {
    return ksnprintf(buf, max_len, "BOOT_IMAGE=/boot/szpontos-kernel console=tty0 root=/dev/ram0\n");
}

static size_t procfs_gen_devices(char *buf, size_t max_len) {
    return ksnprintf(buf, max_len,
                     "Character devices:\n"
                     "  1 mem\n"
                     "  4 tty\n"
                     "  5 /dev/tty\n"
                     "  5 /dev/serial\n"
                     "\n"
                     "Block devices:\n"
                     "  8 hda\n");
}

static size_t procfs_gen_self(char *buf, size_t max_len) {
    process_t *proc = sched_get_current_process();
    pid_t pid = proc ? proc->pid : 1;
    return ksnprintf(buf, max_len, "/proc/%d\n", pid);
}

struct procfs_module_ctx {
    char *buf;
    size_t max_len;
    size_t offset;
};

static void procfs_module_formatter(module_t *mod, void *arg) {
    struct procfs_module_ctx *ctx = (struct procfs_module_ctx *)arg;
    if (ctx->offset >= ctx->max_len)
        return;

    const char *state_str = "Live";
    if (mod->state == MODULE_STATE_COMING)
        state_str = "Loading";
    else if (mod->state == MODULE_STATE_GOING)
        state_str = "Unloading";

    ctx->offset += ksnprintf(ctx->buf + ctx->offset, ctx->max_len - ctx->offset, "%s %lu %d - %s 0x%lx\n", mod->name,
                             mod->size, mod->refcnt, state_str, mod->base_addr);
}

static size_t procfs_gen_modules(char *buf, size_t max_len) {
    struct procfs_module_ctx ctx = {buf, max_len, 0};
    module_list_for_each(procfs_module_formatter, &ctx);
    return ctx.offset;
}

static size_t procfs_gen_sysctl(char *buf, size_t max_len) {
    return sysctl_get_all(buf, max_len);
}

static size_t procfs_gen_pci(char *buf, size_t max_len) {
    size_t pos = 0;
    for (pci_device_t *dev = pci_get_device_list(); dev != NULL && pos < max_len; dev = dev->next) {
        pos += ksnprintf(buf + pos, max_len - pos,
                         "%02x:%02x.%x %02x%02x: %04x:%04x (rev 00, prog-if %02x) IRQ %u",
                         dev->bus, dev->slot, dev->func, dev->class_code, dev->subclass,
                         dev->vendor_id, dev->device_id, dev->prog_if, dev->irq);
        for (int i = 0; i < 6 && pos < max_len; i++) {
            if (dev->bar[i] != 0) {
                pos += ksnprintf(buf + pos, max_len - pos, " [BAR%d: 0x%lx (%s)]",
                                 i, (unsigned long)dev->bar[i], dev->bar_is_io[i] ? "I/O" : "MMIO");
            }
        }
        pos += ksnprintf(buf + pos, max_len - pos, "\n");
    }
    return pos;
}

static size_t procfs_gen_usb(char *buf, size_t max_len) {
    size_t pos = xhci_get_device_list_info(buf, max_len);
    if (pos < max_len) {
        pos += ehci_get_device_list_info(buf + pos, max_len - pos);
    }
    if (pos == 0) {
        pos += ksnprintf(buf, max_len, "No active USB host controllers or devices found.\n");
    }
    return pos;
}

/* =========================================================================
 * Content Generators for Per-Process Files (/proc/<pid>/)
 * ========================================================================= */

static size_t procfs_gen_proc_status(process_t *proc, char *buf, size_t max_len) {
    const char *state_str = "R (running)";
    if (proc->status == PROCESS_ZOMBIE)
        state_str = "Z (zombie)";
    else if (proc->status == PROCESS_DEAD)
        state_str = "X (dead)";

    return ksnprintf(buf, max_len,
                     "Name:\t%s\n"
                     "Umask:\t0022\n"
                     "State:\t%s\n"
                     "Tgid:\t%d\n"
                     "Pid:\t%d\n"
                     "PPid:\t%d\n"
                     "Uid:\t%d\t%d\t%d\t%d\n"
                     "Gid:\t%d\t%d\t%d\t%d\n"
                     "FDSize:\t%d\n"
                     "Groups:\t%d\n"
                     "Threads:\t1\n"
                     "VmPeak:\t    8192 kB\n"
                     "VmSize:\t    8192 kB\n"
                     "VmRSS:\t      64 kB\n",
                     proc->name, state_str, proc->pid, proc->pid, proc->ppid, proc->uid, proc->euid, proc->uid,
                     proc->euid, proc->gid, proc->egid, proc->gid, proc->egid, MAX_FD, proc->gid);
}

static size_t procfs_gen_proc_cmdline(process_t *proc, char *buf, size_t max_len) {
    return ksnprintf(buf, max_len, "%s\n", proc->name);
}

static size_t procfs_gen_proc_comm(process_t *proc, char *buf, size_t max_len) {
    const char *comm = proc->name;
    const char *slash = strrchr(proc->name, '/');
    if (slash)
        comm = slash + 1;
    return ksnprintf(buf, max_len, "%s\n", comm);
}

static size_t procfs_gen_proc_cwd(process_t *proc, char *buf, size_t max_len) {
    return ksnprintf(buf, max_len, "%s\n", proc->cwd[0] ? proc->cwd : "/");
}

static size_t procfs_gen_proc_exe(process_t *proc, char *buf, size_t max_len) {
    if (!proc || !buf || max_len == 0)
        return 0;
    if (proc->name[0] == '/') {
        return ksnprintf(buf, max_len, "%s\n", proc->name);
    } else {
        return ksnprintf(buf, max_len, "/bin/%s\n", proc->name);
    }
}

static size_t procfs_gen_proc_stat(process_t *proc, char *buf, size_t max_len) {
    char state_char = 'R';
    if (proc->status == PROCESS_ZOMBIE)
        state_char = 'Z';
    else if (proc->status == PROCESS_DEAD)
        state_char = 'X';

    const char *comm = proc->name;
    const char *slash = strrchr(proc->name, '/');
    if (slash)
        comm = slash + 1;

    return ksnprintf(buf, max_len,
                     "%d (%s) %c %d %d %d 0 -1 4194304 0 0 0 0 10 5 0 0 20 0 1 0 100 8388608 16 18446744073709551615 "
                     "4194304 4362240 140737218686976 0 0 0 0 0 0 0 0 0 17 0 0 0 0 0 0\n",
                     proc->pid, comm, state_char, proc->ppid, proc->pid, proc->pid);
}

static size_t procfs_gen_proc_statm(process_t *proc, char *buf, size_t max_len) {
    (void)proc;
    return ksnprintf(buf, max_len, "2048 16 12 8 0 4 0\n");
}

static size_t procfs_gen_proc_maps(process_t *proc, char *buf, size_t max_len) {
    uintptr_t brk_c = proc->brk_current ? proc->brk_current : proc->brk_start + 4096;
    return ksnprintf(buf, max_len,
                     "00400000-00428000 r-xp 00000000 00:00 0 %s\n"
                     "00428000-0042a000 rw-p 00028000 00:00 0 %s\n"
                     "%08lx-%08lx rwxp 00000000 00:00 0 [heap]\n"
                     "7ffff0000000-7ffff0010000 rw-p 00000000 00:00 0 [stack]\n",
                     proc->name, proc->name, proc->brk_start, brk_c);
}

static size_t procfs_gen_proc_environ(process_t *proc, char *buf, size_t max_len) {
    (void)proc;
    return ksnprintf(buf, max_len, "PATH=/bin:/usr/bin\nUSER=root\nHOME=/root\nTERM=xterm-256color\n");
}

/* =========================================================================
 * VFS Operations: File Read
 * ========================================================================= */

static ssize_t procfs_file_read(vfs_node_t *node, off_t offset, size_t size, void *buffer) {
    if (!node || !buffer || size == 0)
        return 0;

    procfs_file_data_t *data = (procfs_file_data_t *)node->device_data;
    if (!data)
        return 0;

    if (data->type == PROCFS_TYPE_DMESG) {
        return (ssize_t)klog_read_ring((char *)buffer, size, (size_t)offset);
    }

    char tmp_buf[4096];
    memset(tmp_buf, 0, sizeof(tmp_buf));
    size_t content_len = 0;

    switch (data->type) {
    case PROCFS_TYPE_MEMINFO:
        content_len = procfs_gen_meminfo(tmp_buf, sizeof(tmp_buf));
        break;
    case PROCFS_TYPE_CPUINFO:
        content_len = procfs_gen_cpuinfo(tmp_buf, sizeof(tmp_buf));
        break;
    case PROCFS_TYPE_VERSION:
        content_len = procfs_gen_version(tmp_buf, sizeof(tmp_buf));
        break;
    case PROCFS_TYPE_UPTIME:
        content_len = procfs_gen_uptime(tmp_buf, sizeof(tmp_buf));
        break;
    case PROCFS_TYPE_STAT:
        content_len = procfs_gen_stat(tmp_buf, sizeof(tmp_buf));
        break;
    case PROCFS_TYPE_LOADAVG:
        content_len = procfs_gen_loadavg(tmp_buf, sizeof(tmp_buf));
        break;
    case PROCFS_TYPE_MOUNTS:
        content_len = procfs_gen_mounts(tmp_buf, sizeof(tmp_buf));
        break;
    case PROCFS_TYPE_FILESYSTEMS:
        content_len = procfs_gen_filesystems(tmp_buf, sizeof(tmp_buf));
        break;
    case PROCFS_TYPE_CMDLINE:
        content_len = procfs_gen_cmdline(tmp_buf, sizeof(tmp_buf));
        break;
    case PROCFS_TYPE_DEVICES:
        content_len = procfs_gen_devices(tmp_buf, sizeof(tmp_buf));
        break;
    case PROCFS_TYPE_SELF:
        content_len = procfs_gen_self(tmp_buf, sizeof(tmp_buf));
        break;
    case PROCFS_TYPE_MODULES:
        content_len = procfs_gen_modules(tmp_buf, sizeof(tmp_buf));
        break;
    case PROCFS_TYPE_SYSCTL:
        content_len = procfs_gen_sysctl(tmp_buf, sizeof(tmp_buf));
        break;
    case PROCFS_TYPE_PCI:
        content_len = procfs_gen_pci(tmp_buf, sizeof(tmp_buf));
        break;
    case PROCFS_TYPE_USB:
        content_len = procfs_gen_usb(tmp_buf, sizeof(tmp_buf));
        break;
    default: {
        process_t *proc = process_get_by_pid(data->pid);
        if (!proc) {
            if (data->pid == 0)
                proc = sched_get_current_process();
        }
        if (!proc)
            return 0;

        switch (data->type) {
        case PROCFS_PID_STATUS:
            content_len = procfs_gen_proc_status(proc, tmp_buf, sizeof(tmp_buf));
            break;
        case PROCFS_PID_CMDLINE:
            content_len = procfs_gen_proc_cmdline(proc, tmp_buf, sizeof(tmp_buf));
            break;
        case PROCFS_PID_COMM:
            content_len = procfs_gen_proc_comm(proc, tmp_buf, sizeof(tmp_buf));
            break;
        case PROCFS_PID_CWD:
            content_len = procfs_gen_proc_cwd(proc, tmp_buf, sizeof(tmp_buf));
            break;
        case PROCFS_PID_EXE:
            content_len = procfs_gen_proc_exe(proc, tmp_buf, sizeof(tmp_buf));
            break;
        case PROCFS_PID_STAT:
            content_len = procfs_gen_proc_stat(proc, tmp_buf, sizeof(tmp_buf));
            break;
        case PROCFS_PID_STATM:
            content_len = procfs_gen_proc_statm(proc, tmp_buf, sizeof(tmp_buf));
            break;
        case PROCFS_PID_MAPS:
            content_len = procfs_gen_proc_maps(proc, tmp_buf, sizeof(tmp_buf));
            break;
        case PROCFS_PID_ENVIRON:
            content_len = procfs_gen_proc_environ(proc, tmp_buf, sizeof(tmp_buf));
            break;
        default:
            break;
        }
        break;
    }
    }

    if (offset >= (off_t)content_len) {
        return 0; /* EOF */
    }

    size_t available = content_len - offset;
    size_t to_copy = (size < available) ? size : available;
    memcpy(buffer, tmp_buf + offset, to_copy);
    return (ssize_t)to_copy;
}

static vfs_node_t *procfs_create_file_node(const char *name, uint32_t type, pid_t pid) {
    vfs_node_t *node = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
    if (!node)
        return NULL;

    strncpy(node->name, name, sizeof(node->name) - 1);
    node->flags = VFS_TYPE_FILE;
    node->permissions = 0444; /* r--r--r-- */
    node->uid = 0;
    node->gid = 0;
    node->ops = &g_procfs_file_ops;

    procfs_file_data_t *data = (procfs_file_data_t *)kzalloc(sizeof(procfs_file_data_t));
    data->type = type;
    data->pid = pid;
    node->device_data = data;

    return node;
}

/* =========================================================================
 * VFS Operations: Directory Readdir & Finddir for /proc/<pid>/
 * ========================================================================= */

static const char *g_proc_pid_files[] = {"status", "cmdline", "comm", "cwd", "exe", "stat", "statm", "maps", "environ"};
#define PROC_PID_FILE_COUNT (sizeof(g_proc_pid_files) / sizeof(g_proc_pid_files[0]))

static struct vfs_dirent *procfs_pid_dir_readdir(vfs_node_t *node, uint32_t index) {
    (void)node;
    static vfs_dirent_t dirent;
    if (index >= PROC_PID_FILE_COUNT)
        return NULL;

    strncpy(dirent.name, g_proc_pid_files[index], sizeof(dirent.name) - 1);
    dirent.inode = index + 100;
    dirent.type = VFS_TYPE_FILE;
    return &dirent;
}

static struct vfs_node *procfs_pid_dir_finddir(vfs_node_t *node, const char *name) {
    if (!node || !name)
        return NULL;
    procfs_file_data_t *dir_data = (procfs_file_data_t *)node->device_data;
    pid_t pid = dir_data ? dir_data->pid : 0;

    if (strcmp(name, "status") == 0)
        return procfs_create_file_node("status", PROCFS_PID_STATUS, pid);
    if (strcmp(name, "cmdline") == 0)
        return procfs_create_file_node("cmdline", PROCFS_PID_CMDLINE, pid);
    if (strcmp(name, "comm") == 0)
        return procfs_create_file_node("comm", PROCFS_PID_COMM, pid);
    if (strcmp(name, "cwd") == 0)
        return procfs_create_file_node("cwd", PROCFS_PID_CWD, pid);
    if (strcmp(name, "exe") == 0)
        return procfs_create_file_node("exe", PROCFS_PID_EXE, pid);
    if (strcmp(name, "stat") == 0)
        return procfs_create_file_node("stat", PROCFS_PID_STAT, pid);
    if (strcmp(name, "statm") == 0)
        return procfs_create_file_node("statm", PROCFS_PID_STATM, pid);
    if (strcmp(name, "maps") == 0)
        return procfs_create_file_node("maps", PROCFS_PID_MAPS, pid);
    if (strcmp(name, "environ") == 0)
        return procfs_create_file_node("environ", PROCFS_PID_ENVIRON, pid);

    return NULL;
}

static vfs_node_t *procfs_create_pid_dir(pid_t pid) {
    vfs_node_t *dir = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
    if (!dir)
        return NULL;

    ksnprintf(dir->name, sizeof(dir->name), "%d", pid);
    dir->flags = VFS_TYPE_DIRECTORY;
    dir->permissions = 0555;
    dir->uid = 0;
    dir->gid = 0;
    dir->ops = &g_procfs_pid_dir_ops;

    procfs_file_data_t *data = (procfs_file_data_t *)kzalloc(sizeof(procfs_file_data_t));
    data->type = PROCFS_TYPE_PID_DIR;
    data->pid = pid;
    dir->device_data = data;

    return dir;
}

/* =========================================================================
 * VFS Operations: Directory Readdir & Finddir for /proc/
 * ========================================================================= */

static const struct {
    const char *name;
    uint32_t type;
} g_proc_static_entries[] = {{"meminfo", PROCFS_TYPE_MEMINFO}, {"cpuinfo", PROCFS_TYPE_CPUINFO},
                             {"version", PROCFS_TYPE_VERSION}, {"uptime", PROCFS_TYPE_UPTIME},
                             {"stat", PROCFS_TYPE_STAT},       {"loadavg", PROCFS_TYPE_LOADAVG},
                             {"mounts", PROCFS_TYPE_MOUNTS},   {"filesystems", PROCFS_TYPE_FILESYSTEMS},
                             {"cmdline", PROCFS_TYPE_CMDLINE}, {"devices", PROCFS_TYPE_DEVICES},
                             {"self", PROCFS_TYPE_SELF},       {"modules", PROCFS_TYPE_MODULES},
                             {"dmesg", PROCFS_TYPE_DMESG},     {"kmsg", PROCFS_TYPE_DMESG},
                             {"sysctl", PROCFS_TYPE_SYSCTL},   {"pci", PROCFS_TYPE_PCI},
                             {"usb", PROCFS_TYPE_USB}};
#define PROC_STATIC_COUNT (sizeof(g_proc_static_entries) / sizeof(g_proc_static_entries[0]))

static struct vfs_dirent *procfs_root_readdir(vfs_node_t *node, uint32_t index) {
    (void)node;
    static vfs_dirent_t dirent;

    if (index < PROC_STATIC_COUNT) {
        strncpy(dirent.name, g_proc_static_entries[index].name, sizeof(dirent.name) - 1);
        dirent.inode = index + 1;
        dirent.type = (g_proc_static_entries[index].type == PROCFS_TYPE_SELF) ? VFS_TYPE_SYMLINK : VFS_TYPE_FILE;
        return &dirent;
    }

    uint32_t pid_idx = index - PROC_STATIC_COUNT;
    proc_info_t procs[64];
    size_t count = process_get_list(procs, 64);

    if (pid_idx < count) {
        ksnprintf(dirent.name, sizeof(dirent.name), "%d", procs[pid_idx].pid);
        dirent.inode = 1000 + procs[pid_idx].pid;
        dirent.type = VFS_TYPE_DIRECTORY;
        return &dirent;
    }

    return NULL;
}

static struct vfs_node *procfs_root_finddir(vfs_node_t *node, const char *name) {
    (void)node;
    if (!name)
        return NULL;

    if (strcmp(name, "self") == 0) {
        process_t *curr = sched_get_current_process();
        pid_t pid = curr ? curr->pid : 1;
        return procfs_create_pid_dir(pid);
    }

    for (size_t i = 0; i < PROC_STATIC_COUNT; i++) {
        if (strcmp(name, "self") != 0 && strcmp(name, g_proc_static_entries[i].name) == 0) {
            return procfs_create_file_node(name, g_proc_static_entries[i].type, 0);
        }
    }

    /* Check if name is numeric (PID directory) */
    bool is_num = true;
    for (size_t i = 0; name[i]; i++) {
        if (name[i] < '0' || name[i] > '9') {
            is_num = false;
            break;
        }
    }

    if (is_num && name[0] != '\0') {
        int pid = 0;
        for (size_t i = 0; name[i]; i++) {
            pid = pid * 10 + (name[i] - '0');
        }

        process_t *proc = process_get_by_pid(pid);
        if (proc) {
            return procfs_create_pid_dir(pid);
        }
    }

    return NULL;
}

/* =========================================================================
 * ProcFS Initialization
 * ========================================================================= */

vfs_node_t *procfs_init(void) {
    memset(&g_procfs_root_ops, 0, sizeof(vfs_ops_t));
    g_procfs_root_ops.readdir = procfs_root_readdir;
    g_procfs_root_ops.finddir = procfs_root_finddir;

    memset(&g_procfs_pid_dir_ops, 0, sizeof(vfs_ops_t));
    g_procfs_pid_dir_ops.readdir = procfs_pid_dir_readdir;
    g_procfs_pid_dir_ops.finddir = procfs_pid_dir_finddir;

    memset(&g_procfs_file_ops, 0, sizeof(vfs_ops_t));
    g_procfs_file_ops.read = procfs_file_read;

    g_procfs_root = (vfs_node_t *)kzalloc(sizeof(vfs_node_t));
    strcpy(g_procfs_root->name, "proc");
    g_procfs_root->flags = VFS_TYPE_DIRECTORY;
    g_procfs_root->permissions = 0555; /* r-xr-xr-x */
    g_procfs_root->uid = 0;
    g_procfs_root->gid = 0;
    g_procfs_root->ops = &g_procfs_root_ops;

    klog_info("ProcFS initialized with root node '/proc'");
    return g_procfs_root;
}
