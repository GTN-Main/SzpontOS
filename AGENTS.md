# SzpontOS — Developer & Agent Reference Manual

> **Branding & Copyright:**  
> `(C) Copyright by Szpont Industries. All rights reserved.`  
> **Target Architecture:** `x86_64` (Higher-Half Bare Metal & Ring 3 Userland)  
> **Boot Protocol:** Limine Boot Protocol v8.x (BIOS & UEFI support)

---

## 1. Project Overview

**SzpontOS** is an independent, 64-bit Unix-like operating system written from scratch in C17 and x86_64 Assembly. It features:
- **Higher-Half Monolithic Kernel:** Direct physical mapping via HHDM (`0xFFFF800000000000`) and higher-half execution base (`0xFFFFFFFF80000000`).
- **Symmetric Multiprocessing (SMP):** Native multi-core scheduling (up to 64 CPUs), LAPIC timers, ACPI MADT parsing, and Inter-Processor Interrupts (IPIs).
- **Fast Hardware Syscalls:** Hardware-assisted `syscall` / `sysretq` with per-CPU GS kernel stack switching and full System V AMD64 ABI compliance.
- **Virtual Memory Management:** 4-level x86_64 paging with userland Ring 3 isolation, copy-on-write `fork()`, dynamic heap expansion (`brk()`), and anonymous/file `mmap()`.
- **DRM/KMS Graphics Subsystem:** Native Direct Rendering Manager kernel driver (`/dev/dri/card0`, `/dev/dri/renderD128`), PRIME dma-buf sharing, Syncobj handles, dumb buffers, modesetting, and Virtio-VGA acceleration.
- **Complete X11 Graphical Desktop:** Official X.Org X11 server port with modesetting and XKB, `szpontdesktop` reparenting window manager (glassmorphic TopBar, traffic light controls, cyber titlebars), `szponterm` terminal emulator, and standard X11 libraries (`libX11`, `libxcb`, `libXext`, `libpixman-1`, `libdrm`, `libgbm`).
- **Freestanding C Standard Library & C++ Runtime:** LP64 POSIX C library (`libc.a`, `libc.so`, `libm.so`, `libdl.a`), full C++ runtime (`libstdc++.so`, `libstdc++.a`) with RTTI and exception handling.
- **In-Kernel Dynamic ELF Loader:** Automatic `DT_NEEDED` dependency resolution, shared object mapping at `0x0000700000000000`, and runtime ELF relocations.
- **Modular VFS:** Virtual File System with DevFS, ProcFS, TmpFS, Ext2 filesystem driver, Buffer Cache (`bcache`), and USTAR Initramfs.
- **Networking Stack & Daemons:** BSD sockets (TCP/IP, UDP, ICMP, ARP, DHCP, DNS), Intel E1000 and Realtek RTL8139 NIC drivers, native OpenSSH suite (`sshd`, `ssh`), and embedded HTTP server (`httpd`).
- **Hardware & Input Subsystem:** xHCI (USB 3.0) and EHCI (USB 2.0) controllers with USB HID keyboard/mouse driver, i8042 PS/2 controller, evdev subsystem (`/dev/input/event*`, `/dev/input/mice`), CMOS RTC, and TSC precision timing.

---

## 2. Directory Structure

```
SzpontOS/
├── kernel/                      # Higher-Half Operating System Kernel
│   ├── arch/x86_64/             # Architecture-specific CPU initialization
│   │   ├── gdt.c / idt.c        # GDT, TSS, IDT, 256 interrupt gates
│   │   ├── pic.c / pit.c        # 8259 PIC remapping, PIT 100 Hz timer
│   │   ├── mtrr.c               # MTRR Write-Combining setup for VRAM
│   │   ├── isr.asm              # Interrupt Service Routines & CPU exception handlers
│   │   ├── context.asm          # arch_switch_context & arch_enter_user_mode (iretq)
│   │   ├── syscall_arch.c       # Fast syscall MSR initialization (EFER.SCE, STAR, LSTAR)
│   │   └── syscall_entry.asm    # Syscall hardware entry, GS stack switch & sysret
│   ├── include/                 # Kernel internal headers
│   │   ├── arch/x86_64/         # GDT, IDT, PIC, PIT, IO port primitives, CPU registers
│   │   ├── drivers/             # DRM, FB console, UART, keyboard, RTC, ATA, AHCI, E1000, PCI, IOAPIC, ACPI, PTY, Evdev
│   │   ├── fs/                  # VFS, DevFS, ProcFS, Ext2, TmpFS, Buffer Cache, Initramfs, ELF-64 loader
│   │   ├── mm/                  # PMM (bitmap), VMM (4-level paging), Heap (slab/buddy), Usercopy
│   │   ├── mod/                 # Loadable Kernel Modules (.sko) subsystem
│   │   ├── net/                 # Ethernet, ARP, IPv4, ICMP, UDP, TCP, Netdev, Sockets
│   │   ├── sched/               # process_t, thread_t, futex, waitqueues, SMP scheduler
│   │   ├── syscall/             # POSIX syscall dispatcher and prototypes
│   │   ├── kernel/              # Types, kprint, panic, spinlocks, SMP (cpu_t), string utilities
│   │   └── limine.h             # Limine bootloader protocol specification
│   ├── src/                     # Kernel core implementation
│   │   ├── main.c               # Kernel entry point (_start) and subsystem initialization
│   │   ├── string.c / kprint.c  # Kernel string library, kprintf, ksnprintf, klog
│   │   ├── panic.c              # Kernel panic handler and register dump
│   │   ├── kernel/              # smp.c (multi-core bootstrap, IPI, LAPIC timer)
│   │   ├── drivers/             # drm.c, framebuffer.c, serial.c, keyboard.c, mouse.c, ps2_mouse.c,
│   │   │                        # rtc.c, ata.c, ahci.c, pci.c, e1000.c, rtl8139.c, ioapic.c, acpi.c,
│   │   │                        # pty.c, evdev.c, random.c, power.c, usb/ (ehci.c, xhci.c, hid.c)
│   │   ├── fs/                  # vfs.c, devfs.c, procfs.c, tmpfs.c, ext2.c, bcache.c, initramfs.c, elf.c
│   │   ├── mm/                  # pmm.c, vmm.c, heap.c, usercopy.c
│   │   ├── mod/                 # module.c (ELF module loader, symbol resolution, relocations)
│   │   ├── net/                 # netif.c, ethernet.c, arp.c, ipv4.c, icmp.c, udp.c, tcp.c, socket.c
│   │   ├── sched/               # process.c, sched.c, futex.c, waitqueue.c
│   │   └── syscall/             # syscall.c (100+ POSIX system call handlers)
│   └── linker.ld                # Higher-half linker script (0xFFFFFFFF80000000)
│
├── libc/                        # Freestanding C Standard Library (builds libc.a, libc.so, libm.so, libdl.a)
│   ├── include/                 # Standard POSIX headers (100% clean POSIX/C17/BSD)
│   │   ├── stdio.h, stdlib.h, string.h, unistd.h, fcntl.h, dirent.h, errno.h, time.h
│   │   ├── pthread.h, semaphore.h, dlfcn.h, math.h, termios.h, poll.h, signal.h
│   │   ├── stdint.h, stddef.h, stdbool.h, stdarg.h (LP64 self-contained headers)
│   │   └── sys/ (stat.h, types.h, socket.h, mman.h, poll.h, utsname.h, wait.h, time.h, ioctl.h, shm.h)
│   └── src/
│       ├── arch/x86_64/         # crt0.asm, syscall.asm, setjmp.asm
│       ├── stdio/               # printf.c, snprintf.c, puts, putchar, getchar, file ops
│       ├── stdlib/              # malloc.c (sbrk/mmap heap allocator), strtol, atoi, env
│       ├── string/              # Standard string and memory manipulation routines
│       ├── time/                # time.c (clock_gettime, gettimeofday, time, nanosleep)
│       ├── pthread/             # POSIX threads, mutexes, condvars, barriers, semaphores, TLS
│       ├── socket/              # Berkeley Sockets API (socket, connect, bind, listen, recv, send, SCM_RIGHTS)
│       ├── dlfcn/               # Dynamic linker routines (dlopen, dlsym, dlclose, dlerror)
│       ├── netdb/               # getaddrinfo, gethostbyname, DNS resolver
│       └── unistd/              # POSIX syscall wrappers (fork, execve, read, write, sleep, etc.)
│
├── libdrm/                      # Native Direct Rendering Manager Library (builds libdrm.so)
│   ├── include/                 # xf86drm.h, xf86drmMode.h, drm/*
│   └── src/                     # xf86drm.c, xf86drmMode.c, syncobj.c
│
├── libgbm/                      # Generic Buffer Management Library (builds libgbm.so)
│   ├── include/                 # gbm.h
│   └── src/                     # gbm.c
│
├── third_party/                 # Ported open-source packages cross-compiled against libc sysroot
│   ├── xorg/                    # Official X.Org X11 Server (Xorg binary with native DRM/KMS modesetting)
│   ├── libX11/, libxcb/, ...    # Core X11 client libraries (libX11, libXext, libXau, libXdmcp, libxkbfile, etc.)
│   ├── pixman/                  # Low-level pixel manipulation library (libpixman-1.so)
│   ├── libstdc++/               # GNU C++ Standard Library runtime (libstdc++.so, libstdc++.a)
│   ├── openssh/                 # OpenSSH client and daemon (sshd, ssh, ssh-keygen)
│   ├── curl/                    # Libcurl & curl command-line utility
│   ├── zlib/                    # Compression library (libz.so)
│   ├── ncurses/                 # GNU Ncurses (libncurses.a)
│   ├── nano/                    # GNU nano editor (/bin/nano)
│   ├── zsh/                     # Z shell (/bin/zsh)
│   ├── file/                    # Libmagic & GNU file (/bin/file)
│   └── fastfetch/               # System information tool (/bin/fastfetch)
│
├── userland/                    # User space programs and root filesystem
│   ├── init/main.c              # PID 1 init process (spawns /bin/sh or graphical session)
│   ├── sh/main.c                # Interactive Unix shell with built-ins & history
│   ├── bin/                     # Core utilities: cat, chmod, chown, clock, cpptest, curltest, date, df,
│   │                            # dltest, dmesg, donut, drmtest, find, free, gittest, grep, groupadd,
│   │                            # head, hello, host, hostname, httpd, httpget, id, ifconfig, insmod,
│   │                            # kill, killall, kqueuetest, ls, lsmod, lspci, lsusb, makaljer, mathtest,
│   │                            # mesadrmtest, mkdir, modinfo, mount, mousetest, nc, ping, poweroff, ps,
│   │                            # ptytest, randtest, reboot, rm, rmmod, shmtest, shutdown, sleep, startx,
│   │                            # su, sync, sysctl, szpontdesktop, szpontdetected, szponterm, tail,
│   │                            # threadtest, tmpfstest, top, touch, tuitest, uname, unixtest, uptime,
│   │                            # useradd, userdel, wc, whoami
│   └── skeleton/                # Static rootfs skeleton templates (/etc/passwd, /etc/magic, /etc/ssh, etc.)
│
├── mk/                          # Modular Build System Makefiles
│   ├── third_party.mk           # Third-party packages, shared libraries & Autotools cross-build rules
│   ├── iso.mk                   # USTAR initramfs packaging and bootable Limine ISO generation
│   └── qemu.mk                  # Emulator execution profiles (virtio, ps2, usb, stress, cli, debug)
│
├── scripts/                     # Toolchain & execution helper scripts
│   ├── run_qemu.sh              # Unified QEMU launcher with port forwarding and hardware toggles
│   ├── make_initramfs.py        # Python script packaging rootfs into USTAR initramfs.tar
│   ├── make_ext2_disk.py        # Disk image generator for secondary block storage
│   ├── build_libstdcxx.py       # Standalone C++ runtime builder
│   └── generate_ncurses_fallbacks.py
│
├── limine.conf                  # Limine bootloader boot configuration
├── config.mk                    # Toolchain detection, compiler flags, and sysroot paths
├── Makefile                     # Root orchestration Makefile with parallel DAG dependency rules
├── .clangd                      # Clangd language server bare-metal configuration
└── .vscode/c_cpp_properties.json# IDE include paths and IntelliSense configuration
```

---

## 3. Core Technical Principles & Architecture

### 3.1 Memory Layout (x86_64 4-Level Paging)
- **Higher-Half Kernel Base:** `0xFFFFFFFF80000000` (mapped via linker script and Limine).
- **HHDM (Higher-Half Direct Map Base):** `0xFFFF800000000000` (physical memory offset `g_hhdm_base`).
- **Userland Virtual Address Space:** `0x0000000000400000` – `0x00007FFFFFFFFFFF` (Ring 3, DPL=3).
- **Shared Object (.so) Load Base:** Starts at `0x0000700000000000` (dynamically assigned per library).
- **User Stack Base:** `0x00007FFFF0000000` (grows down, default size 4 MiB).
- **User Heap (`brk`):** Starts at `0x0000000000800000` and dynamically expands via `sys_brk`.

### 3.2 Physical Address Masking Rule
> [!IMPORTANT]
> When traversing or modifying 64-bit page table entries (PML4, PDPT, PD, PT), **always** mask using `PHYS_ADDR_MASK` (`0x000FFFFFFFFFF000ULL`).
> Never use `~0xFFFULL`, as bit 63 (`NX` / No Execute) or other high architectural bits will corrupt the physical pointer converted via `PHYS_TO_VIRT()`.

### 3.3 Hardware Interrupts, PIC EOI & Idle Loop
1. **Interrupt Handler then EOI Ordering:** In [kernel/arch/x86_64/idt.c](kernel/arch/x86_64/idt.c), registered interrupt handlers are executed **before** sending EOI (PIC `outb(0x20, 0x20)` or LAPIC EOI). This ensures hardware device handlers (e.g. keyboard port `0x60`) can read data before the next interrupt is allowed. On bare metal hardware, sending EOI before the handler causes scancode loss due to real IO-APIC/LAPIC timing.
2. **Idle Thread CPU Halt:** In [kernel/src/sched/sched.c](kernel/src/sched/sched.c), `g_idle_thread` executes `__asm__ volatile ("sti; hlt; cli");`. This ensures the CPU halts in low-power state with interrupts enabled so timer ticks can wake it.
3. **Blocking vs Spinning:** Never spin in a tight `sched_yield()` loop in syscalls. For blocking operations (e.g. `waitpid`), use `thread_sleep(10)` or waitqueues so the scheduler can yield to `idle_thread` and allow timer ticks to advance.

### 3.4 Hardware RTC & CPU TSC Precision Timing
- **CMOS RTC:** [kernel/src/drivers/rtc.c](kernel/src/drivers/rtc.c) reads ports `0x70`/`0x71` with Update-In-Progress (`UIP`) polling and BCD conversion to Unix epoch.
- **TSC Calibration:** At boot, TSC frequency is calibrated against PIT hardware counter latch (port `0x40`) without requiring interrupts (`g_tsc_freq_hz`).
- **Monotonic High-Precision Time:** `rtc_get_monotonic()` / `sys_clock_gettime(CLOCK_MONOTONIC)` computes time directly from `rdtsc()`, providing sub-microsecond precision (used by `ping`, `sleep`, profiling).

### 3.5 Networking Stack & NIC Driver (Intel 8254x / E1000)
- The network stack supports Ethernet, ARP, IPv4, ICMP, UDP, and TCP (state machine with 3-way handshake).
- **Packet Polling Rule:** During socket reads (`recvfrom`, `read`) and `poll`, `e1000_poll()` is called immediately to process incoming frames from SLIRP without waiting for the PIT/LAPIC timer tick.

### 3.6 Ring 3 Transition & Fast Syscalls
- **Switching to User Mode:** Handled in [kernel/arch/x86_64/context.asm](kernel/arch/x86_64/context.asm) (`arch_enter_user_mode`) by setting selectors `CS=0x23`, `DS/ES/FS/GS=0x1B`, `SS=0x1B`, `RFLAGS=0x202`, and executing `iretq`.
- **Fast Syscalls:** Handled via `syscall` / `sysretq`. Hardware jumps to `syscall_entry` in [kernel/arch/x86_64/syscall_entry.asm](kernel/arch/x86_64/syscall_entry.asm), saves user RSP, switches to `g_current_kernel_stack` (via `%gs:16`), and maps arguments according to System V AMD64 ABI:
  - `RAX` (sys_no) $\rightarrow$ `RDI`
  - `RDI` (arg1) $\rightarrow$ `RSI`
  - `RSI` (arg2) $\rightarrow$ `RDX`
  - `RDX` (arg3) $\rightarrow$ `RCX`
  - `R10` (arg4) $\rightarrow$ `R8`
  - `R8`  (arg5) $\rightarrow$ `R9`

### 3.7 Symmetric Multiprocessing (SMP) & Per-CPU GS Layout
> [!CAUTION]
> **CRITICAL ASSEMBLY STRUCT OFFSETS:**  
> The per-CPU structure `cpu_t` defined in [kernel/include/kernel/smp.h](kernel/include/kernel/smp.h) is accessed directly by assembly routines in [syscall_entry.asm](kernel/arch/x86_64/syscall_entry.asm) via the `%gs` segment base (`%gs:0`).  
> **Never insert, reorder, or resize fields** in `cpu_t` without synchronizing the exact byte offsets in `syscall_entry.asm` and `context.asm`:
> - `Offset 0x00 (0)`: `self` (pointer to this `cpu_t`)
> - `Offset 0x08 (8)`: `cpu_id` (`uint32_t`)
> - `Offset 0x0C (12)`: `lapic_id` (`uint32_t`)
> - `Offset 0x10 (16)`: `kernel_stack` (`uintptr_t`, loaded into `%rsp` during syscall)
> - `Offset 0x18 (24)`: `user_rsp` (`uintptr_t`, saved user `%rsp` during syscall)
> - `Offset 0x20 (32)`: `current_thread` (`struct thread *`)
> - `Offset 0x28 (40)`: `idle_thread` (`struct thread *`)
> - `Offset 0x30 (48)`: `boot_rsp` (`uintptr_t`)
> - `Offset 0x38 (56)`: `online` (`volatile uint8_t`)
> - `Offset 0x39 (57)`: `is_bsp` (`uint8_t`)

- Multi-core initialization is handled via Limine's SMP boot protocol in [kernel/src/kernel/smp.c](kernel/src/kernel/smp.c).
- Each CPU core configures its own TSS, GDT, LAPIC timer (1000 Hz), and `%gs` base via `wrmsr(MSR_GS_BASE)`.
- Inter-processor coordination uses IPIs (`smp_send_ipi`) and spinlocks.

### 3.8 Safe Ring 3 Memory Access (`usercopy`)
> [!IMPORTANT]
> **NEVER DIRECTLY DEREFERENCE USERSPACE POINTERS IN RING 0!**  
> Direct dereferencing of userland pointers in kernel mode causes unhandled page faults or critical security vulnerabilities.
> Always use the checked copy functions from [kernel/include/mm/usercopy.h](kernel/include/mm/usercopy.h):
> - `bool copy_from_user(void *dst, uintptr_t user_src, size_t len)`
> - `bool copy_to_user(uintptr_t user_dst, const void *src, size_t len)`
> - `char *copy_string_from_user(uintptr_t user_src, size_t max_len)`
>
> These functions validate canonical userland boundaries (`USER_ADDR_MAX = 0x00007FFFFFFFFFFFULL`), walk the process page table, and copy bytes through the kernel's HHDM direct physical mapping. Unmapped or non-writable pages return `false` / `-EFAULT` cleanly.

### 3.9 In-Kernel Dynamic ELF Loader & Shared Library Resolution
- Unlike traditional Linux where `ld-linux.so` runs in userland, SzpontOS incorporates an in-kernel dynamic ELF loader in [kernel/src/fs/elf.c](kernel/src/fs/elf.c).
- When executing a dynamically linked binary:
  1. Segments (`PT_LOAD`) are mapped into user memory with appropriate permissions (`PF_R`, `PF_W`, `PF_X`).
  2. The dynamic section (`PT_DYNAMIC`) is inspected for `DT_NEEDED` dependencies.
  3. Shared objects (`.so`) are automatically located in `/lib` and `/usr/lib`, mapped into virtual memory starting from `SO_BASE_START = 0x0000700000000000ULL`.
  4. Dynamic symbol tables (`DT_SYMTAB`, `DT_STRTAB`, `DT_HASH`) are resolved and relocations applied (`R_X86_64_RELATIVE`, `R_X86_64_GLOB_DAT`, `R_X86_64_JUMP_SLOT`, `R_X86_64_64`).
- Userland binaries also have access to `dlopen()`, `dlsym()`, `dlclose()`, and `dlerror()` through `libdl.a` / `libc/src/dlfcn/dlfcn.c`.

### 3.10 DRM/KMS Graphics Subsystem & Buffer Management
- **Device Nodes:**
  - `/dev/dri/card0`: Primary DRM node with master PID authentication, mode enumeration, CRTC/connector configuration, and dumb buffer allocation.
  - `/dev/dri/renderD128`: Render node allowing unprivileged 3D/compute access and buffer sharing without modesetting privileges.
- **Dumb Buffers & Framebuffers:** Allocated via `DRM_IOCTL_MODE_CREATE_DUMB` and wrapped in FB IDs via `DRM_IOCTL_MODE_ADDFB`.
- **PRIME dma-buf Sharing:** Kernel supports exporting buffer objects to file descriptors (`DRM_IOCTL_PRIME_HANDLE_TO_FD`) and importing them (`DRM_IOCTL_PRIME_FD_TO_HANDLE`), enabling zero-copy buffer sharing across processes.
- **Sync Objects:** Explicit GPU synchronization primitives (`drm_syncobj_t`) supporting create, destroy, signal, and wait ioctls.
- **X.Org & Desktop Integration:** The official X11 server connects to `/dev/dri/card0` using standard modesetting and `libdrm.so`.

### 3.11 IPC, Sockets & File Descriptor Passing (`SCM_RIGHTS`)
- UNIX domain sockets (`AF_UNIX`) fully support ancillary control messages (`sendmsg` / `recvmsg` with `SCM_RIGHTS`).
- This allows transferring open file descriptors (including PRIME dma-buf graphics buffers) between the X11 server and client applications (e.g. DRI3 protocol).
- SysV Shared Memory (`SYS_shmget`, `SYS_shmat`, `SYS_shmctl`, `SYS_shmdt`) is implemented for MIT-SHM high-speed graphics blitting.
- PTY subsystem (`/dev/ptmx` and `/dev/pts/N`) provides full pseudoterminal master/slave multiplexing for terminal emulators like `szponterm`.

### 3.12 Input Pipeline: Evdev, PS/2 & USB HID
- Drivers for i8042 PS/2 controller (`ps2_mouse.c`, `keyboard.c`) and USB HID (`usb/hid.c` via `xhci.c` and `ehci.c`) feed directly into the unified evdev subsystem in [kernel/src/drivers/evdev.c](kernel/src/drivers/evdev.c).
- Exposes:
  - `/dev/input/mice`: Emulates standard 3-byte / 4-byte Explorer PS/2 packets for legacy X11 mouse drivers.
  - `/dev/input/event0` .. `/dev/input/eventN`: Linux-compatible `struct input_event` streams for modern event handling.

---

## 4. Syscall Reference Table

The kernel implements over 100 POSIX system calls in [kernel/src/syscall/syscall.c](kernel/src/syscall/syscall.c):

| Syscall # | Name | Description |
|---|---|---|
| 0 | `SYS_read` | Read data from file descriptor |
| 1 | `SYS_write` | Write data to file descriptor |
| 2 | `SYS_open` | Open file or device |
| 3 | `SYS_close` | Close file descriptor |
| 4 | `SYS_stat` | Retrieve file status by path |
| 5 | `SYS_fstat` | Retrieve file status by descriptor |
| 6 | `SYS_lstat` | Retrieve symbolic link status |
| 7 | `SYS_poll` | Wait for I/O events on file descriptors |
| 8 | `SYS_lseek` | Reposition read/write file offset |
| 9 | `SYS_mmap` | Map pages into process address space |
| 10 | `SYS_mprotect` | Set protection on memory region |
| 11 | `SYS_munmap` | Unmap pages from process address space |
| 12 | `SYS_brk` | Expand or contract process heap |
| 13 | `SYS_rt_sigaction` | Examine and change signal action |
| 14 | `SYS_rt_sigprocmask` | Change list of blocked signals |
| 16 | `SYS_ioctl` | Device control operations (DRM, TTY, FB, sockets) |
| 17 | `SYS_pread64` | Read from file offset without changing file pointer |
| 18 | `SYS_pwrite64` | Write to file offset without changing file pointer |
| 19 | `SYS_readv` | Read data into multiple buffers (scatter) |
| 20 | `SYS_writev` | Write data from multiple buffers (gather) |
| 21 | `SYS_access` | Check user permissions for file |
| 22 | `SYS_pipe` | Create unidirectional IPC data channel |
| 23 | `SYS_select` | Synchronous I/O multiplexing |
| 24 | `SYS_yield` | Yield remaining CPU timeslice |
| 29 | `SYS_shmget` | Allocate SysV shared memory segment |
| 30 | `SYS_shmat` | Attach SysV shared memory segment |
| 31 | `SYS_shmctl` | Control SysV shared memory segment |
| 32 | `SYS_dup` | Duplicate open file descriptor |
| 33 | `SYS_dup2` | Duplicate file descriptor to target index |
| 34 | `SYS_pause` | Wait for signal |
| 35 | `SYS_nanosleep` | High-precision thread sleep |
| 37 | `SYS_alarm` | Set alarm clock for delivery of signal |
| 39 | `SYS_getpid` | Get process ID of calling process |
| 41 | `SYS_socket` | Create communication endpoint |
| 42 | `SYS_connect` | Initiate connection on socket |
| 43 | `SYS_accept` | Accept connection on socket |
| 44 | `SYS_sendto` | Send message on socket |
| 45 | `SYS_recvfrom` | Receive message from socket |
| 46 | `SYS_sendmsg` | Send message with ancillary data (`SCM_RIGHTS`) |
| 47 | `SYS_recvmsg` | Receive message with ancillary data (`SCM_RIGHTS`) |
| 48 | `SYS_shutdown` | Shut down part of a full-duplex socket connection |
| 49 | `SYS_bind` | Bind socket to local address |
| 50 | `SYS_listen` | Listen for incoming socket connections |
| 51 | `SYS_getsockname` | Retrieve current address of socket |
| 52 | `SYS_getpeername` | Retrieve peer address of connected socket |
| 53 | `SYS_socketpair` | Create pair of connected UNIX sockets |
| 54 | `SYS_setsockopt` | Set options on socket |
| 55 | `SYS_getsockopt` | Retrieve options from socket |
| 56 | `SYS_clone` | Create thread/process (`CLONE_VM`, `CLONE_THREAD`, `CLONE_FS`) |
| 57 | `SYS_fork` | Fork child process with COW address space clone |
| 59 | `SYS_execve` | Execute ELF binary with arguments and environment |
| 60 | `SYS_exit` | Terminate calling thread / process |
| 61 | `SYS_wait4` | Wait for state changes in child processes |
| 62 | `SYS_kill` | Send signal to process |
| 63 | `SYS_uname` | Retrieve system name and OS identification |
| 67 | `SYS_shmdt` | Detach SysV shared memory segment |
| 72 | `SYS_fcntl` | Manipulate file descriptor properties |
| 76 | `SYS_truncate` | Truncate file to specified length by path |
| 77 | `SYS_ftruncate` | Truncate file to specified length by descriptor |
| 78 | `SYS_getdents` | Read directory entries into buffer |
| 79 | `SYS_getcwd` | Get current working directory pathname |
| 80 | `SYS_chdir` | Change current working directory |
| 82 | `SYS_rename` | Change name or location of file |
| 83 | `SYS_mkdir` | Create directory |
| 84 | `SYS_rmdir` | Remove directory |
| 85 | `SYS_creat` | Create and open new file |
| 86 | `SYS_link` | Create hard link to file |
| 87 | `SYS_unlink` | Remove directory entry / delete file |
| 88 | `SYS_symlink` | Create symbolic link |
| 89 | `SYS_readlink` | Read target value of symbolic link |
| 90 | `SYS_chmod` | Change file permissions by path |
| 91 | `SYS_fchmod` | Change file permissions by descriptor |
| 92 | `SYS_chown` | Change file ownership by path |
| 93 | `SYS_fchown` | Change file ownership by descriptor |
| 95 | `SYS_umask` | Set file mode creation mask |
| 96 | `SYS_gettimeofday` | Get time with microsecond resolution |
| 97 | `SYS_getrlimit` | Get process resource limits |
| 98 | `SYS_getrusage` | Get process resource utilization |
| 99 | `SYS_sysinfo` | Retrieve system statistics (memory, uptime, load) |
| 100 | `SYS_times` | Get process execution times |
| 101 | `SYS_sleep` | Sleep for specified seconds |
| 102 | `SYS_getuid` | Get real user ID |
| 103 | `SYS_syslog` | Read or control kernel message ring buffer |
| 104 | `SYS_getgid` | Get real group ID |
| 105 | `SYS_setuid` | Set real/effective user ID |
| 106 | `SYS_setgid` | Set real/effective group ID |
| 107 | `SYS_geteuid` | Get effective user ID |
| 108 | `SYS_getegid` | Get effective group ID |
| 109 | `SYS_setpgid` | Set process group ID |
| 110 | `SYS_getppid` | Get parent process ID |
| 111 | `SYS_getpgrp` | Get process group ID |
| 112 | `SYS_setsid` | Create session and set process group ID |
| 113 | `SYS_setreuid` | Set real and effective user IDs |
| 114 | `SYS_setregid` | Set real and effective group IDs |
| 115 | `SYS_getgroups` | Get list of supplementary group IDs |
| 116 | `SYS_setgroups` | Set list of supplementary group IDs |
| 117 | `SYS_setresuid` | Set real, effective, and saved user IDs |
| 118 | `SYS_getresuid` | Get real, effective, and saved user IDs |
| 119 | `SYS_setresgid` | Set real, effective, and saved group IDs |
| 120 | `SYS_getresgid` | Get real, effective, and saved group IDs |
| 121 | `SYS_getpgid` | Get process group ID of process |
| 124 | `SYS_getsid` | Get session ID of process |
| 125 | `SYS_seteuid` | Set effective user ID |
| 126 | `SYS_setegid` | Set effective group ID |
| 127 | `SYS_rt_sigpending` | Examine pending signals |
| 137 | `SYS_statfs` | Get filesystem statistics |
| 138 | `SYS_fstatfs` | Get filesystem statistics by descriptor |
| 156 | `SYS_sysctl` | Read or write system control parameters |
| 158 | `SYS_arch_prctl` | Set architecture-specific thread state (`FS_BASE` / `GS_BASE`) |
| 160 | `SYS_setrlimit` | Set process resource limits |
| 162 | `SYS_sync` | Synchronize cached filesystem buffers to disk |
| 169 | `SYS_reboot` | Reboot or power off system |
| 172 | `SYS_iopl` | Change I/O privilege level |
| 175 | `SYS_init_module` | Load kernel module (`.sko`) |
| 176 | `SYS_delete_module`| Unload kernel module |
| 178 | `SYS_getprocs` | Retrieve process table snapshot |
| 186 | `SYS_gettid` | Get thread ID |
| 200 | `SYS_tkill` | Send signal to specific thread |
| 201 | `SYS_time` | Get current Unix epoch timestamp (seconds) |
| 202 | `SYS_futex` | Fast user-space locking (`FUTEX_WAIT`, `FUTEX_WAKE`) |
| 218 | `SYS_set_tid_address`| Set pointer to thread ID for clear_child_tid |
| 227 | `SYS_clock_settime`| Set clock time |
| 228 | `SYS_clock_gettime`| Retrieve clock time (`CLOCK_REALTIME`, `CLOCK_MONOTONIC`) |
| 229 | `SYS_clock_getres` | Retrieve clock resolution |
| 231 | `SYS_exit_group` | Exit all threads in process |
| 235 | `SYS_utimes` | Change file timestamps |
| 257 | `SYS_openat` | Open file relative to directory descriptor |
| 258 | `SYS_mkdirat` | Create directory relative to directory descriptor |
| 260 | `SYS_fchownat` | Change ownership relative to directory descriptor |
| 261 | `SYS_futimesat` | Change file timestamps relative to directory descriptor |
| 262 | `SYS_newfstatat` | Retrieve file status relative to directory descriptor |
| 263 | `SYS_unlinkat` | Remove directory entry relative to directory descriptor |
| 265 | `SYS_linkat` | Create hard link relative to directory descriptor |
| 267 | `SYS_readlinkat` | Read symbolic link relative to directory descriptor |
| 268 | `SYS_fchmodat` | Change permissions relative to directory descriptor |
| 269 | `SYS_faccessat` | Check access relative to directory descriptor |
| 280 | `SYS_utimensat` | Update timestamps with nanosecond precision |
| 318 | `SYS_getrandom` | Obtain random bytes from kernel CSPRNG |
| 319 | `SYS_memfd_create` | Create anonymous in-memory file descriptor |
| 362 | `SYS_kqueue` | Allocate kernel event notification queue |
| 363 | `SYS_kevent` | Register events and receive pending notifications |

---

## 5. Build System & Common Commands

All build workflows are managed through the central [Makefile](Makefile) and modular makefiles in `mk/`.

### Architecture of the Build DAG
```
[libc sources] ────> [libc.a / libc.so] ────> [SYSROOT (/usr/include & /usr/lib)]
                                                     │
               ┌─────────────────────────────────────┼─────────────────────────────┐
               ▼                                     ▼                             ▼
         [libdrm.so]                            [libgbm.so]                [libstdc++.so]
               │                                     │                             │
               └──────────────────┬──────────────────┘                             │
                                  ▼                                                │
                 [Third-Party Ecosystem & X11] <───────────────────────────────────┘
               (Xorg, libX11, libxcb, OpenSSH, curl, zlib, ncurses, etc.)
                                  │
                                  ▼
                           [Userland Binaries]
               (init, sh, szpontdesktop, szponterm, startx, coreutils)
                                  │
                                  ▼
                        [USTAR Initramfs Rootfs]
                                  │
      [Kernel ELF] ───────────────┼────────────────> [Bootable ISO Image]
                                                      (via xorriso & Limine)
```

### Toolchain Dependencies
- **Compiler:** `x86_64-elf-gcc` (Freestanding cross-compiler)
- **Assembler:** `nasm`
- **Linker:** `x86_64-elf-ld`
- **Archiver:** `x86_64-elf-ar`
- **ISO Generator:** `xorriso`
- **Emulator:** `qemu-system-x86_64`
- **Compilation DB Tool:** `bear`

### Standard Build & Run Commands
```bash
# Build complete bootable ISO image
make iso

# Run SzpontOS with Virtio-VGA acceleration (Recommended for desktop / X11)
make run-virtio

# Run SzpontOS in standard VGA graphical window
make run

# Run SzpontOS in headless CLI mode (serial output piped to terminal)
make run-cli

# Run SzpontOS in bare-metal PS/2 keyboard/mouse mode (i8042 enabled)
make run-ps2

# Run SzpontOS in pure modern UEFI USB mode (i8042 disabled, pure xHCI HID)
make run-usb

# Run with realistic hardware timing and virtual instruction counters (-icount)
make run-stress

# Run with GDB debugging stub enabled (listening on localhost:1234)
make debug

# Regenerate compile_commands.json for clangd and IDE IntelliSense
make compile-commands

# Clean all build artifacts, objects, and rootfs
make clean
```

### Guest Network & Forwarded Ports
When launched with QEMU, the user-mode SLIRP network forwarders are active:
| Guest Service | Guest Port | Host Forwarded Port | Connection Command |
|---|---|---|---|
| HTTP Web Server | 80 | `8080` (or next free) | `curl http://localhost:8080/` |
| OpenSSH Server | 22 | `2222` (or next free) | `ssh -p 2222 root@localhost` |

---

## 6. Coding & Development Guidelines for AI Agents

1. **Freestanding Environment:**
   - Kernel and Libc code must remain 100% freestanding (`-ffreestanding -fno-builtin -nostdlib`).
   - Do not include host standard library headers. Use `<stdint.h>`, `<stddef.h>`, `<stdbool.h>`, `<stdarg.h>` from `kernel/include/` or `libc/include/`.
   - Types must follow the standard 64-bit **LP64** model (`long` and `unsigned long` are 64 bits; `size_t` and `uintptr_t` are `unsigned long`).

2. **Concurrency & Thread Safety:**
   - Protect global kernel structures (process lists, runqueues, memory maps, VFS tables, DRM buffers) with spinlocks (`spinlock_t`, `spinlock_acquire`, `spinlock_release`).
   - Keep critical sections as short as possible. Never block or sleep while holding a spinlock.

3. **Memory Safety & Higher-Half Access:**
   - Always convert physical frame pointers to higher-half virtual addresses using `PHYS_TO_VIRT(phys)` before dereferencing in kernel code.
   - When modifying page tables for processes, invalidate TLB entries where appropriate (`invlpg`).
   - Always mask page table entries with `PHYS_ADDR_MASK` (`0x000FFFFFFFFFF000ULL`).

4. **Safe Userland Memory Copy (`usercopy`):**
   - **Never direct-dereference user pointers in Ring 0.**
   - Always use `copy_from_user` and `copy_to_user` (`<mm/usercopy.h>`).
   - Validate user-supplied buffers, strings, and sizes before copying. Return `-EFAULT` immediately on invalid addresses.

5. **Assembly Offsets & Hardware ABI Invariants:**
   - Invariants in `cpu_t` ([kernel/include/kernel/smp.h](kernel/include/kernel/smp.h)) are referenced by hard-coded numeric offsets in [kernel/arch/x86_64/syscall_entry.asm](kernel/arch/x86_64/syscall_entry.asm) and [context.asm](kernel/arch/x86_64/context.asm).
   - If any member is added or modified in `cpu_t`, you **must** update the corresponding numeric offsets in the assembly files.

6. **IDE, Clangd & Sysroot Synchronization:**
   - Whenever new C source files are added to `kernel/`, `libc/`, or `userland/`, update the corresponding file list in `Makefile` and run `make compile-commands`.
   - Any new library headers must be installed into `$(SYSROOT_DIR)/usr/include` so downstream userland and third-party packages can compile against them.

7. **Dynamic Linking & Shared Libraries (No Static Linking):**
   - Userland binaries and ported packages must be dynamically linked against shared libraries (`.so`).
   - Avoid static linking for userland programs whenever possible.
   - All shared libraries must reside in `/lib` (in `build/rootfs/lib/`) with valid ELF `DT_SONAME` tags (e.g. `libc.so`, `libm.so`, `libz.so`, `libX11.so`, `libpixman-1.so`, `libdrm.so`, `libgbm.so`, `libstdc++.so`).
   - When introducing a new shared library, add it to `ALL_ROOTFS_SOS` in [mk/third_party.mk](mk/third_party.mk).

8. **C++ Runtime & Modern Language Support:**
   - C++ applications and tests must link against `libstdc++.so` or `libstdc++.a`.
   - The runtime supports global constructors/destructors, exceptions, and RTTI. Ensure new C++ binaries call proper runtime entrypoints.

9. **No Stubs / Complete Implementation Rule:**
   - **Never create stubs or mock functions.** Every function must have a complete, robust, and working implementation.
   - Do not leave empty function bodies, placeholder `TODO` comments, dummy return values (e.g. returning dummy success/failure or `-ENOSYS` merely to pass compilation), or partial implementations.
   - All functions, drivers, system calls, and library routines introduced or edited must be fully implemented and functional.

10. **Third-Party Submodules & Patching Policy:**
    - **Never edit code directly in `third_party/` if it is a git submodule.** Submodule working trees must remain clean and track upstream commits without untracked changes, local edits, or dirty state.
    - **Always solve issues at the OS level first:** If a ported third-party package fails to compile, link, or run due to missing POSIX headers, system calls, ioctls, types, constants, socket options, or C library functions, **always implement the missing functionality directly in SzpontOS (`kernel/` or `libc/`)**. The operating system must evolve to adapt to standard software, not the other way around.
    - **Use patches strictly as a last resort:** Only if an issue fundamentally cannot be resolved at the OS level (e.g., hardcoded host tool paths in package build systems, unsupported compiler flags, or non-standard upstream assumptions), create cleanly isolated patch files applied non-destructively during the build process, rather than modifying files directly inside the submodule directory.

11. **No Absolute Host Paths (Portability & Hermetic Builds):**
    - **Never hardcode absolute host filesystem paths** (such as `/Users/...`, `/home/...`, `/opt/...`, or machine-specific developer directories) anywhere in source code, headers, Makefiles, helper scripts, configuration templates, cross-compilation definition files, or test suites.
    - All paths in the codebase and build pipeline must be relative to the repository root or derived dynamically at runtime:
      - In Makefiles: use variables derived from the root, e.g. `$(ROOT_DIR)`, `$(abspath $(ROOT_DIR))`, `$(BUILD_DIR)`, `$(SYSROOT_DIR)`.
      - In shell/python scripts: compute locations dynamically relative to the script file (e.g. `$(cd "$(dirname "$0")/.." && pwd)` or `Path(__file__).resolve().parent`).
      - For host tools and compilers: locate them via dynamic `PATH` lookups (`command -v <tool>`, `which <tool>`, or `pkg-config`) instead of hardcoding absolute binary paths.
    - The repository and build pipeline must remain completely portable, relocatable, and buildable across different developer machines, operating systems, and CI/CD environments.
