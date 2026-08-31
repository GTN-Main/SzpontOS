# SzpontOS Makefile
# Modern 64-bit Unix-like Operating System
# Multi-platform build system supporting macOS & Linux with GCC / Clang

OS_NAME := szpontos
ARCH    := x86_64

# ==============================================================================
# Parallel Build Configuration
# ==============================================================================
NPROC := $(shell nproc 2>/dev/null || sysctl -n hw.logicalcpu 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)
JOBS ?= $(NPROC)
MAKEFLAGS += -j$(JOBS)

# ==============================================================================
# Toolchain Auto-detection (Prefer GCC, fallback to Clang)
# ==============================================================================

# C Compiler detection
ifneq ($(shell which x86_64-elf-gcc 2>/dev/null),)
    CC := x86_64-elf-gcc
    TOOLCHAIN_TYPE := gcc
else ifneq ($(shell which x86_64-linux-gnu-gcc 2>/dev/null),)
    CC := x86_64-linux-gnu-gcc
    TOOLCHAIN_TYPE := gcc
else ifeq ($(shell uname -s),Linux)
    CC := gcc
    TOOLCHAIN_TYPE := gcc
else ifneq ($(shell which /opt/homebrew/opt/llvm/bin/clang 2>/dev/null),)
    CC := /opt/homebrew/opt/llvm/bin/clang
    TOOLCHAIN_TYPE := clang
else
    CC := clang
    TOOLCHAIN_TYPE := clang
endif

# Linker detection
ifneq ($(shell which x86_64-elf-ld 2>/dev/null),)
    LD := x86_64-elf-ld
else ifneq ($(shell which x86_64-linux-gnu-ld 2>/dev/null),)
    LD := x86_64-linux-gnu-ld
else ifneq ($(shell which ld.lld 2>/dev/null),)
    LD := ld.lld
else ifneq ($(shell which /opt/homebrew/opt/lld/bin/ld.lld 2>/dev/null),)
    LD := /opt/homebrew/opt/lld/bin/ld.lld
else
    LD := ld
endif

# Archiver detection
ifneq ($(shell which x86_64-elf-ar 2>/dev/null),)
    AR := x86_64-elf-ar
else ifneq ($(shell which x86_64-linux-gnu-ar 2>/dev/null),)
    AR := x86_64-linux-gnu-ar
else
    AR := ar
endif

# Ranlib detection
ifneq ($(shell which x86_64-elf-ranlib 2>/dev/null),)
    RANLIB := x86_64-elf-ranlib
else ifneq ($(shell which x86_64-linux-gnu-ranlib 2>/dev/null),)
    RANLIB := x86_64-linux-gnu-ranlib
else
    RANLIB := ranlib
endif

NASM    := nasm
XORRISO := xorriso
QEMU    := qemu-system-x86_64

# ==============================================================================
# Compilation Flags & Directories
# ==============================================================================

BUILD_DIR := build

# Kernel Flags
CFLAGS := \
    -ffreestanding \
    -fno-stack-protector \
    -fno-stack-check \
    -fno-lto \
    -fno-pie \
    -fno-pic \
    -mno-80387 \
    -mno-mmx \
    -mno-sse \
    -mno-sse2 \
    -mno-red-zone \
    -mcmodel=kernel \
    -Wall \
    -Wextra \
    -std=c17 \
    -O2 \
    -g \
    -I kernel/include \
    -I $(BUILD_DIR)/include \
    -I .

ifeq ($(TOOLCHAIN_TYPE),clang)
    CFLAGS += -target $(ARCH)-unknown-none-elf
endif

ASMFLAGS := -f elf64

LDFLAGS := \
    -nostdlib \
    -static \
    -m elf_x86_64 \
    -z max-page-size=0x1000 \
    -T kernel/linker.ld

# Userland Flags
USER_CFLAGS := \
    -ffreestanding \
    -fno-stack-protector \
    -fno-stack-check \
    -fno-lto \
    -fPIC \
    -mno-red-zone \
    -Wall \
    -Wextra \
    -std=c17 \
    -O2 \
    -g \
    -I libc/include

ifeq ($(TOOLCHAIN_TYPE),clang)
    USER_CFLAGS += -target $(ARCH)-unknown-none-elf
endif

USER_LDFLAGS := \
    -nostdlib \
    -m elf_x86_64 \
    -z max-page-size=0x1000

# Directories
BUILD_DIR := build
ISO_DIR   := $(BUILD_DIR)/iso_root
KERNEL_ELF := $(BUILD_DIR)/$(OS_NAME)-kernel
ISO_IMAGE  := $(BUILD_DIR)/$(OS_NAME).iso
DISK_IMAGE := $(BUILD_DIR)/disk.img
LIBC_A     := $(BUILD_DIR)/libc/libc.a
LIBC_SO    := $(BUILD_DIR)/rootfs/lib/libc.so
LIBCALC_SO := $(BUILD_DIR)/rootfs/lib/libcalc.so
CRT0_O     := $(BUILD_DIR)/libc/crt0.o

# Kernel Sources
KERNEL_C_SRCS := \
    kernel/src/main.c \
    kernel/src/string.c \
    kernel/src/kprint.c \
    kernel/src/panic.c \
    kernel/src/mm/pmm.c \
    kernel/src/mm/vmm.c \
    kernel/src/mm/heap.c \
    kernel/src/fs/vfs.c \
    kernel/src/fs/devfs.c \
    kernel/src/fs/bcache.c \
    kernel/src/fs/ext2.c \
    kernel/src/fs/initramfs.c \
    kernel/src/fs/procfs.c \
    kernel/src/fs/tmpfs.c \
    kernel/src/fs/elf.c \
    kernel/src/sched/process.c \
    kernel/src/sched/futex.c \
    kernel/src/sched/sched.c \
    kernel/src/kernel/module.c \
    kernel/src/kernel/ksymtab.c \
    kernel/src/kernel/sysctl.c \
    kernel/src/kernel/kqueue.c \
    kernel/src/syscall/syscall.c \
    kernel/src/drivers/serial.c \
    kernel/src/drivers/power.c \
    kernel/src/drivers/random.c \
    kernel/src/drivers/pty.c \
    kernel/src/drivers/font8x16.c \
    kernel/src/drivers/framebuffer.c \
    kernel/src/drivers/keyboard.c \
    kernel/src/drivers/mouse.c \
    kernel/src/drivers/ps2_mouse.c \
    kernel/src/drivers/ps2_test.c \
    kernel/src/drivers/speaker.c \
    kernel/src/drivers/tty.c \
    kernel/src/drivers/drm.c \
    kernel/src/drivers/evdev.c \
    kernel/src/drivers/acpi.c \
    kernel/src/drivers/ioapic.c \
    kernel/src/drivers/usb/usb.c \
    kernel/src/drivers/usb/xhci.c \
    kernel/src/drivers/usb/ehci.c \
    kernel/src/drivers/usb/hid.c \
    kernel/src/drivers/rtc.c \
    kernel/src/drivers/ata.c \
    kernel/src/drivers/ahci.c \
    kernel/src/drivers/pci.c \
    kernel/src/drivers/e1000.c \
    kernel/src/drivers/rtl8139.c \
    kernel/src/net/net.c \
    kernel/src/net/netif.c \
    kernel/src/net/loopback.c \
    kernel/src/net/arp.c \
    kernel/src/net/ipv4.c \
    kernel/src/net/icmp.c \
    kernel/src/net/udp.c \
    kernel/src/net/tcp.c \
    kernel/src/net/socket.c \
    kernel/arch/x86_64/gdt.c \
    kernel/arch/x86_64/idt.c \
    kernel/arch/x86_64/pic.c \
    kernel/arch/x86_64/pit.c \
    kernel/arch/x86_64/syscall_arch.c

KERNEL_ASM_SRCS := \
    kernel/arch/x86_64/isr.asm \
    kernel/arch/x86_64/context.asm \
    kernel/arch/x86_64/syscall_entry.asm

KERNEL_C_OBJS   := $(patsubst %.c, $(BUILD_DIR)/%.o, $(KERNEL_C_SRCS))
KERNEL_ASM_OBJS := $(patsubst %.asm, $(BUILD_DIR)/%.o, $(KERNEL_ASM_SRCS))
KERNEL_ALL_OBJS := $(KERNEL_C_OBJS) $(KERNEL_ASM_OBJS)

# Libc Sources
LIBC_C_SRCS := \
    libc/src/string/string.c \
    libc/src/stdlib/malloc.c \
    libc/src/stdlib/atexit.c \
    libc/src/stdlib/locale.c \
    libc/src/stdlib/system.c \
    libc/src/stdio/printf.c \
    libc/src/unistd/unistd.c \
    libc/src/unistd/sysconf.c \
    libc/src/unistd/sendfile.c \
    libc/src/unistd/uio.c \
    libc/src/dirent/dirent.c \
    libc/src/pwd/pwd.c \
    libc/src/grp/grp.c \
    libc/src/dlfcn/dlfcn.c \
    libc/src/ctype/ctype.c \
    libc/src/time/time.c \
    libc/src/getopt/getopt.c \
    libc/src/mman/mman.c \
    libc/src/regex/regex.c \
    libc/src/math/math.c \
    libc/src/crypto/sha1.c \
    libc/src/pthread/pthread.c \
    libc/src/pthread/mutex.c \
    libc/src/pthread/cond.c \
    libc/src/pthread/sem.c \
    libc/src/termios/termios.c \
    libc/src/wchar/wchar.c \
    libc/src/glob/glob.c \
    libc/src/mntent/mntent.c \
    libc/src/fnmatch/fnmatch.c \
    libc/src/spawn/spawn.c \
    libc/src/poll/poll.c \
    libc/src/uchar/uchar.c \
    libc/src/socket/socket.c \
    libc/src/netdb/netdb.c \
    libc/src/net/if.c \
    libc/src/utmp/utmp.c \
    libc/src/sched/sched.c \
    libc/src/drm/drm.c \
    libc/src/pty/pty.c \
    libc/src/arch/x86_64/builtins.c


LIBC_ASM_SRCS := \
    libc/src/arch/x86_64/syscall.asm \
    libc/src/arch/x86_64/setjmp.asm

LIBC_C_OBJS   := $(patsubst %.c, $(BUILD_DIR)/%.o, $(LIBC_C_SRCS))
LIBC_ASM_OBJS := $(patsubst %.asm, $(BUILD_DIR)/%.o, $(LIBC_ASM_SRCS))
LIBC_OBJS     := $(LIBC_C_OBJS) $(LIBC_ASM_OBJS)

# Sysroot Definition
SYSROOT_DIR   := $(BUILD_DIR)/sysroot
SYSROOT_STAMP := $(SYSROOT_DIR)/.sysroot_installed

# GNU Ncurses (Cross-compiled via original Autotools)
NCURSES_BUILD_DIR := $(BUILD_DIR)/third_party/ncurses
LIBNCURSES_A      := $(SYSROOT_DIR)/usr/lib/libncurses.a

# GNU nano (Cross-compiled via original Autotools)
NANO_BUILD_DIR    := $(BUILD_DIR)/third_party/nano

# Libmagic & GNU file (Cross-compiled via original Autotools)
FILE_BUILD_DIR := $(BUILD_DIR)/third_party/file
MAGIC_DB       := $(BUILD_DIR)/rootfs/etc/magic

# Zsh (Cross-compiled via original Autotools)
ZSH_BUILD_DIR := $(BUILD_DIR)/third_party/zsh

# zlib (Cross-compiled via original Makefile)
ZLIB_BUILD_DIR := $(BUILD_DIR)/third_party/zlib
LIBZ_A         := $(SYSROOT_DIR)/usr/lib/libz.a

# Git (Libre-WD-40, Cross-compiled via original Makefile & userland/config/git/config.mak)
GIT_BUILD_DIR  := $(BUILD_DIR)/third_party/git

# Linux Compatibility Layer
LINUX_COMPAT_DIR    := compat/linux
LINUX_COMPAT_CFLAGS := -isystem $(abspath $(LINUX_COMPAT_DIR)/include)

# Fastfetch (Cross-compiled via CMake)
FASTFETCH_BUILD_DIR := $(BUILD_DIR)/third_party/fastfetch

# Rootfs & Userland Programs
ROOTFS_DIR          := $(BUILD_DIR)/rootfs
ROOTFS_SKELETON_DIR := userland/skeleton

LIBM_A        := $(BUILD_DIR)/libc/libm.a
LIBM_SO       := $(ROOTFS_DIR)/lib/libm.so
LIBDL_A       := $(BUILD_DIR)/libc/libdl.a

USER_PROGS := \
    $(ROOTFS_DIR)/bin/init \
    $(ROOTFS_DIR)/bin/sh \
    $(ROOTFS_DIR)/bin/hello \
    $(ROOTFS_DIR)/bin/cat \
    $(ROOTFS_DIR)/bin/id \
    $(ROOTFS_DIR)/bin/whoami \
    $(ROOTFS_DIR)/bin/useradd \
    $(ROOTFS_DIR)/bin/userdel \
    $(ROOTFS_DIR)/bin/groupadd \
    $(ROOTFS_DIR)/bin/su \
    $(ROOTFS_DIR)/bin/chmod \
    $(ROOTFS_DIR)/bin/chown \
    $(ROOTFS_DIR)/bin/df \
    $(ROOTFS_DIR)/bin/ps \
    $(ROOTFS_DIR)/bin/free \
    $(ROOTFS_DIR)/bin/uptime \
    $(ROOTFS_DIR)/bin/date \
    $(ROOTFS_DIR)/bin/clock \
    $(ROOTFS_DIR)/bin/mkdir \
    $(ROOTFS_DIR)/bin/touch \
    $(ROOTFS_DIR)/bin/head \
    $(ROOTFS_DIR)/bin/tail \
    $(ROOTFS_DIR)/bin/wc \
    $(ROOTFS_DIR)/bin/clear \
    $(ROOTFS_DIR)/bin/mount \
    $(ROOTFS_DIR)/bin/dltest \
    $(ROOTFS_DIR)/bin/file \
    $(ROOTFS_DIR)/bin/mathtest \
    $(ROOTFS_DIR)/bin/threadtest \
    $(ROOTFS_DIR)/bin/insmod \
    $(ROOTFS_DIR)/bin/rmmod \
    $(ROOTFS_DIR)/bin/lsmod \
    $(ROOTFS_DIR)/bin/modinfo \
    $(ROOTFS_DIR)/bin/rm \
    $(ROOTFS_DIR)/bin/hostname \
    $(ROOTFS_DIR)/bin/uname \
    $(ROOTFS_DIR)/bin/tuitest \
    $(ROOTFS_DIR)/bin/drmtest \
    $(ROOTFS_DIR)/bin/SzpontX11 \
    $(ROOTFS_DIR)/bin/Xorg \
    $(ROOTFS_DIR)/bin/startx \
    $(ROOTFS_DIR)/bin/szpontdesktop \
    $(ROOTFS_DIR)/bin/szponterm \
    $(ROOTFS_DIR)/bin/xterm \
    $(ROOTFS_DIR)/bin/nano \
    $(ROOTFS_DIR)/bin/zsh \
    $(ROOTFS_DIR)/bin/fastfetch \
    $(ROOTFS_DIR)/bin/git \
    $(ROOTFS_DIR)/bin/ifconfig \
    $(ROOTFS_DIR)/bin/ping \
    $(ROOTFS_DIR)/bin/sleep \
    $(ROOTFS_DIR)/bin/nc \
    $(ROOTFS_DIR)/bin/httpd \
    $(ROOTFS_DIR)/bin/sysctl \
    $(ROOTFS_DIR)/bin/kill \
    $(ROOTFS_DIR)/bin/killall \
    $(ROOTFS_DIR)/bin/dmesg \
    $(ROOTFS_DIR)/bin/randtest \
    $(ROOTFS_DIR)/bin/tmpfstest \
    $(ROOTFS_DIR)/bin/ptytest \
    $(ROOTFS_DIR)/bin/kqueuetest \
    $(ROOTFS_DIR)/bin/grep \
    $(ROOTFS_DIR)/bin/find \
    $(ROOTFS_DIR)/bin/top \
    $(ROOTFS_DIR)/bin/reboot \
    $(ROOTFS_DIR)/bin/shutdown \
    $(ROOTFS_DIR)/bin/poweroff \
    $(ROOTFS_DIR)/bin/ls \
    $(ROOTFS_DIR)/bin/lspci \
    $(ROOTFS_DIR)/bin/lsusb \
    $(ROOTFS_DIR)/bin/donut \
    $(ROOTFS_DIR)/bin/mousetest \
    $(ROOTFS_DIR)/bin/unixtest \
    $(ROOTFS_DIR)/bin/gittest


MODULE_DIR    := $(ROOTFS_DIR)/lib/modules
MODULE_CFLAGS := $(CFLAGS) -mcmodel=kernel -fno-pic -fno-pie
MODULES       := $(MODULE_DIR)/hello.sko $(MODULE_DIR)/dummy_dev.sko

.PHONY: all build iso initramfs run run-virtio run-ps2 run-usb run-stress run-cli debug clean distclean limine libc userland compile_commands.json compile-commands bear toolchain-info disk sysroot

all: $(ISO_IMAGE)

toolchain-info:
	@echo "  [TOOLCHAIN] CC:    $(CC) ($(TOOLCHAIN_TYPE))"
	@echo "  [TOOLCHAIN] LD:    $(LD)"
	@echo "  [TOOLCHAIN] AR:    $(AR)"
	@echo "  [TOOLCHAIN] ASM:   $(NASM)"
	@echo "  [TOOLCHAIN] ISO:   $(XORRISO)"
	@echo "  [TOOLCHAIN] CORES: $(NPROC) (Parallel Jobs: $(JOBS))"

build: toolchain-info libc userland $(KERNEL_ELF)

GEN_PANIC_IMAGE := $(BUILD_DIR)/include/drivers/panic_image.h

$(GEN_PANIC_IMAGE): artwork/szpont-detected.jpg scripts/generate_panic_image.py
	@mkdir -p $(dir $@)
	@python3 scripts/generate_panic_image.py $< $@

$(BUILD_DIR)/kernel/src/drivers/framebuffer.o: $(GEN_PANIC_IMAGE)
$(BUILD_DIR)/kernel/src/panic.o: $(GEN_PANIC_IMAGE)

# Compile Kernel C sources
$(BUILD_DIR)/kernel/%.o: kernel/%.c
	@mkdir -p $(dir $@)
	@echo "  [CC]  $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Assemble Kernel ASM sources
$(BUILD_DIR)/kernel/%.o: kernel/%.asm
	@mkdir -p $(dir $@)
	@echo "  [ASM] $<"
	@$(NASM) $(ASMFLAGS) $< -o $@

# Compile Libc C sources
$(BUILD_DIR)/libc/%.o: libc/%.c
	@mkdir -p $(dir $@)
	@echo "  [CC-LIBC] $<"
	@$(CC) $(USER_CFLAGS) -c $< -o $@

# Assemble Libc ASM sources
$(BUILD_DIR)/libc/%.o: libc/%.asm
	@mkdir -p $(dir $@)
	@echo "  [ASM-LIBC] $<"
	@$(NASM) $(ASMFLAGS) $< -o $@

# Assemble crt0.o
$(CRT0_O): libc/src/arch/x86_64/crt0.asm
	@mkdir -p $(dir $@)
	@echo "  [ASM] $<"
	@$(NASM) $(ASMFLAGS) $< -o $@

# Build Libc static library
$(LIBC_A): $(LIBC_OBJS)
	@mkdir -p $(dir $@)
	@echo "  [AR]  $@"
	@$(AR) rcs $@ $(LIBC_OBJS)

# Build Libc shared library (.so)
$(LIBC_SO): $(LIBC_OBJS) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/lib $(BUILD_DIR)/libc
	@echo "  [LD-SO] $@"
	@$(LD) -shared -soname libc.so $(LIBC_OBJS) -o $@

# Build Libcalc shared library (.so)
$(LIBCALC_SO): userland/lib/libcalc.c | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/lib $(BUILD_DIR)/userland/lib
	@echo "  [CC-SO] $<"
	@$(CC) $(USER_CFLAGS) -c $< -o $(BUILD_DIR)/userland/lib/libcalc.o
	@echo "  [LD-SO] $@"
	@$(LD) -shared -soname libcalc.so $(BUILD_DIR)/userland/lib/libcalc.o -o $@

# Build Libm static library (.a)
$(LIBM_A): $(BUILD_DIR)/libc/src/math/math.o
	@mkdir -p $(dir $@)
	@echo "  [AR]  $@"
	@$(AR) rcs $@ $<

# Build Libm shared library (.so)
$(LIBM_SO): $(BUILD_DIR)/libc/src/math/math.o | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/lib
	@echo "  [LD-SO] $@"
	@$(LD) -shared -soname libm.so $< -o $@

# Build Libdl static library (.a)
$(LIBDL_A):
	@mkdir -p $(dir $@)
	@echo "  [AR]  $@"
	@$(AR) rcs $@

libc: $(CRT0_O) $(LIBC_A) $(LIBC_SO) $(LIBM_A) $(LIBM_SO) $(LIBDL_A) $(LIBCALC_SO)


# Prepare Rootfs staging directory from skeleton
$(ROOTFS_DIR):
	@mkdir -p $(ROOTFS_DIR)/bin $(ROOTFS_DIR)/lib $(ROOTFS_DIR)/dev $(ROOTFS_DIR)/etc $(ROOTFS_DIR)/proc $(ROOTFS_DIR)/mnt
	@if [ -d $(ROOTFS_SKELETON_DIR) ]; then cp -r $(ROOTFS_SKELETON_DIR)/* $(ROOTFS_DIR)/ 2>/dev/null || true; fi

# Userland Binaries
$(ROOTFS_DIR)/bin/init: userland/init/main.c $(CRT0_O) $(LIBC_A) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(BUILD_DIR)/userland/init
	@echo "  [CC]  $<"
	@$(CC) $(USER_CFLAGS) -c $< -o $(BUILD_DIR)/userland/init/main.o
	@echo "  [LD]  $@"
	@$(LD) $(USER_LDFLAGS) $(CRT0_O) $(BUILD_DIR)/userland/init/main.o $(LIBC_A) -o $@

$(ROOTFS_DIR)/bin/sh: userland/sh/main.c $(CRT0_O) $(LIBC_A) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(BUILD_DIR)/userland/sh
	@echo "  [CC]  $<"
	@$(CC) $(USER_CFLAGS) -c $< -o $(BUILD_DIR)/userland/sh/main.o
	@echo "  [LD]  $@"
	@$(LD) $(USER_LDFLAGS) $(CRT0_O) $(BUILD_DIR)/userland/sh/main.o $(LIBC_A) -o $@

$(ROOTFS_DIR)/bin/hello: userland/bin/hello.c $(CRT0_O) $(LIBC_A) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(BUILD_DIR)/userland/bin
	@echo "  [CC]  $<"
	@$(CC) $(USER_CFLAGS) -c $< -o $(BUILD_DIR)/userland/bin/hello.o
	@echo "  [LD]  $@"
	@$(LD) $(USER_LDFLAGS) $(CRT0_O) $(BUILD_DIR)/userland/bin/hello.o $(LIBC_A) -o $@

$(ROOTFS_DIR)/bin/cat: userland/bin/cat.c $(CRT0_O) $(LIBC_A) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(BUILD_DIR)/userland/bin
	@echo "  [CC]  $<"
	@$(CC) $(USER_CFLAGS) -c $< -o $(BUILD_DIR)/userland/bin/cat.o
	@echo "  [LD]  $@"
	@$(LD) $(USER_LDFLAGS) $(CRT0_O) $(BUILD_DIR)/userland/bin/cat.o $(LIBC_A) -o $@

$(ROOTFS_DIR)/bin/id: userland/bin/id.c $(CRT0_O) $(LIBC_A) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(BUILD_DIR)/userland/bin
	@echo "  [CC]  $<"
	@$(CC) $(USER_CFLAGS) -c $< -o $(BUILD_DIR)/userland/bin/id.o
	@echo "  [LD]  $@"
	@$(LD) $(USER_LDFLAGS) $(CRT0_O) $(BUILD_DIR)/userland/bin/id.o $(LIBC_A) -o $@

$(ROOTFS_DIR)/bin/whoami: userland/bin/whoami.c $(CRT0_O) $(LIBC_A) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(BUILD_DIR)/userland/bin
	@echo "  [CC]  $<"
	@$(CC) $(USER_CFLAGS) -c $< -o $(BUILD_DIR)/userland/bin/whoami.o
	@echo "  [LD]  $@"
	@$(LD) $(USER_LDFLAGS) $(CRT0_O) $(BUILD_DIR)/userland/bin/whoami.o $(LIBC_A) -o $@

$(ROOTFS_DIR)/bin/useradd: userland/bin/useradd.c $(CRT0_O) $(LIBC_A) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(BUILD_DIR)/userland/bin
	@echo "  [CC]  $<"
	@$(CC) $(USER_CFLAGS) -c $< -o $(BUILD_DIR)/userland/bin/useradd.o
	@echo "  [LD]  $@"
	@$(LD) $(USER_LDFLAGS) $(CRT0_O) $(BUILD_DIR)/userland/bin/useradd.o $(LIBC_A) -o $@

$(ROOTFS_DIR)/bin/userdel: userland/bin/userdel.c $(CRT0_O) $(LIBC_A) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(BUILD_DIR)/userland/bin
	@echo "  [CC]  $<"
	@$(CC) $(USER_CFLAGS) -c $< -o $(BUILD_DIR)/userland/bin/userdel.o
	@echo "  [LD]  $@"
	@$(LD) $(USER_LDFLAGS) $(CRT0_O) $(BUILD_DIR)/userland/bin/userdel.o $(LIBC_A) -o $@

$(ROOTFS_DIR)/bin/groupadd: userland/bin/groupadd.c $(CRT0_O) $(LIBC_A) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(BUILD_DIR)/userland/bin
	@echo "  [CC]  $<"
	@$(CC) $(USER_CFLAGS) -c $< -o $(BUILD_DIR)/userland/bin/groupadd.o
	@echo "  [LD]  $@"
	@$(LD) $(USER_LDFLAGS) $(CRT0_O) $(BUILD_DIR)/userland/bin/groupadd.o $(LIBC_A) -o $@

$(ROOTFS_DIR)/bin/su: userland/bin/su.c $(CRT0_O) $(LIBC_A) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(BUILD_DIR)/userland/bin
	@echo "  [CC]  $<"
	@$(CC) $(USER_CFLAGS) -c $< -o $(BUILD_DIR)/userland/bin/su.o
	@echo "  [LD]  $@"
	@$(LD) $(USER_LDFLAGS) $(CRT0_O) $(BUILD_DIR)/userland/bin/su.o $(LIBC_A) -o $@

$(ROOTFS_DIR)/bin/chmod: userland/bin/chmod.c $(CRT0_O) $(LIBC_A) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(BUILD_DIR)/userland/bin
	@echo "  [CC]  $<"
	@$(CC) $(USER_CFLAGS) -c $< -o $(BUILD_DIR)/userland/bin/chmod.o
	@echo "  [LD]  $@"
	@$(LD) $(USER_LDFLAGS) $(CRT0_O) $(BUILD_DIR)/userland/bin/chmod.o $(LIBC_A) -o $@

$(ROOTFS_DIR)/bin/%: userland/bin/%.c $(CRT0_O) $(LIBC_A) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(BUILD_DIR)/userland/bin
	@echo "  [CC]  $<"
	@$(CC) $(USER_CFLAGS) -c $< -o $(BUILD_DIR)/userland/bin/$*.o
	@echo "  [LD]  $@"
	@$(LD) $(USER_LDFLAGS) $(CRT0_O) $(BUILD_DIR)/userland/bin/$*.o $(LIBC_A) -o $@



# Build mathtest
$(ROOTFS_DIR)/bin/mathtest: userland/bin/mathtest.c $(CRT0_O) $(LIBC_A) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(BUILD_DIR)/userland/bin
	@echo "  [CC]  $<"
	@$(CC) $(USER_CFLAGS) -c $< -o $(BUILD_DIR)/userland/bin/mathtest.o
	@echo "  [LD]  $@"
	@$(LD) $(USER_LDFLAGS) $(CRT0_O) $(BUILD_DIR)/userland/bin/mathtest.o $(LIBC_A) -o $@

# Build threadtest
$(ROOTFS_DIR)/bin/threadtest: userland/bin/threadtest.c $(CRT0_O) $(LIBC_A) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(BUILD_DIR)/userland/bin
	@echo "  [CC]  $<"
	@$(CC) $(USER_CFLAGS) -c $< -o $(BUILD_DIR)/userland/bin/threadtest.o
	@echo "  [LD]  $@"
	@$(LD) $(USER_LDFLAGS) $(CRT0_O) $(BUILD_DIR)/userland/bin/threadtest.o $(LIBC_A) -o $@

# Build insmod
$(ROOTFS_DIR)/bin/insmod: userland/bin/insmod.c $(CRT0_O) $(LIBC_A) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(BUILD_DIR)/userland/bin
	@echo "  [CC]  $<"
	@$(CC) $(USER_CFLAGS) -c $< -o $(BUILD_DIR)/userland/bin/insmod.o
	@echo "  [LD]  $@"
	@$(LD) $(USER_LDFLAGS) $(CRT0_O) $(BUILD_DIR)/userland/bin/insmod.o $(LIBC_A) -o $@

# Build rmmod
$(ROOTFS_DIR)/bin/rmmod: userland/bin/rmmod.c $(CRT0_O) $(LIBC_A) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(BUILD_DIR)/userland/bin
	@echo "  [CC]  $<"
	@$(CC) $(USER_CFLAGS) -c $< -o $(BUILD_DIR)/userland/bin/rmmod.o
	@echo "  [LD]  $@"
	@$(LD) $(USER_LDFLAGS) $(CRT0_O) $(BUILD_DIR)/userland/bin/rmmod.o $(LIBC_A) -o $@

# Build lsmod
$(ROOTFS_DIR)/bin/lsmod: userland/bin/lsmod.c $(CRT0_O) $(LIBC_A) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(BUILD_DIR)/userland/bin
	@echo "  [CC]  $<"
	@$(CC) $(USER_CFLAGS) -c $< -o $(BUILD_DIR)/userland/bin/lsmod.o
	@echo "  [LD]  $@"
	@$(LD) $(USER_LDFLAGS) $(CRT0_O) $(BUILD_DIR)/userland/bin/lsmod.o $(LIBC_A) -o $@

# Build modinfo
$(ROOTFS_DIR)/bin/modinfo: userland/bin/modinfo.c $(CRT0_O) $(LIBC_A) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(BUILD_DIR)/userland/bin
	@echo "  [CC]  $<"
	@$(CC) $(USER_CFLAGS) -c $< -o $(BUILD_DIR)/userland/bin/modinfo.o
	@echo "  [LD]  $@"
	@$(LD) $(USER_LDFLAGS) $(CRT0_O) $(BUILD_DIR)/userland/bin/modinfo.o $(LIBC_A) -o $@

# Build rm
$(ROOTFS_DIR)/bin/rm: userland/bin/rm.c $(CRT0_O) $(LIBC_A) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(BUILD_DIR)/userland/bin
	@echo "  [CC]  $<"
	@$(CC) $(USER_CFLAGS) -c $< -o $(BUILD_DIR)/userland/bin/rm.o
	@echo "  [LD]  $@"
	@$(LD) $(USER_LDFLAGS) $(CRT0_O) $(BUILD_DIR)/userland/bin/rm.o $(LIBC_A) -o $@

# Build donut (dynamically linked against libc.so and libm.so)
$(ROOTFS_DIR)/bin/donut: userland/bin/donut.c $(CRT0_O) $(LIBC_SO) $(LIBM_SO) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(BUILD_DIR)/userland/bin
	@echo "  [CC-DYN] $<"
	@$(CC) $(USER_CFLAGS) -c $< -o $(BUILD_DIR)/userland/bin/donut.o
	@echo "  [LD-DYN] $@"
	@$(LD) $(USER_LDFLAGS) -pie $(CRT0_O) $(BUILD_DIR)/userland/bin/donut.o $(LIBM_SO) $(LIBC_SO) -o $@

# GNU Ncurses Targets (Autotools cross-compile)
$(NCURSES_BUILD_DIR)/Makefile: $(SYSROOT_STAMP) | $(NCURSES_BUILD_DIR)
	@echo "  [CONF-NCURSES] Konfiguracja GNU Ncurses (Autotools cross-compile)..."
	@cd $(NCURSES_BUILD_DIR) && \
	../../../third_party/ncurses/configure \
	    --host=x86_64-elf \
	    --prefix=/usr \
	    --with-build-cc=gcc \
	    --without-ada \
	    --without-cxx \
	    --without-tests \
	    --without-progs \
	    --without-manpages \
	    --without-debug \
	    --without-gpm \
	    --without-sysmouse \
	    --enable-overwrite \
	    --enable-termcap \
	    --without-fallbacks \
	    --disable-database \
	    --disable-home-terminfo \
	    --enable-static \
	    --without-shared \
	    CC="$(CC)" \
	    CPP="$(CC) -E" \
	    AR="$(AR)" \
	    RANLIB="$(RANLIB)" \
	    CFLAGS="-O2 -ffreestanding -fno-builtin -isystem $(abspath $(SYSROOT_DIR))/usr/include -B$(abspath $(SYSROOT_DIR))/usr/lib" \
	    CPPFLAGS="-isystem $(abspath $(SYSROOT_DIR))/usr/include" \
	    LDFLAGS="-nostdlib -L$(abspath $(SYSROOT_DIR))/usr/lib -B$(abspath $(SYSROOT_DIR))/usr/lib" \
	    LIBS="-lc"
	@echo "#define SIG_ATOMIC_T int" >> $(NCURSES_BUILD_DIR)/include/ncurses_cfg.h
	@echo "#define TYPE_SIG_ATOMIC_T int" >> $(NCURSES_BUILD_DIR)/include/ncurses_cfg.h
	@echo "#define HAVE_SETENV 1" >> $(NCURSES_BUILD_DIR)/include/ncurses_cfg.h
	@echo "#define HAVE_PUTENV 1" >> $(NCURSES_BUILD_DIR)/include/ncurses_cfg.h
	@echo "#define HAVE_GETCWD 1" >> $(NCURSES_BUILD_DIR)/include/ncurses_cfg.h
	@echo "#define HAVE_FCNTL_H 1" >> $(NCURSES_BUILD_DIR)/include/ncurses_cfg.h
	@echo "#define HAVE_UNISTD_H 1" >> $(NCURSES_BUILD_DIR)/include/ncurses_cfg.h
	@echo "#define HAVE_SYS_IOCTL_H 1" >> $(NCURSES_BUILD_DIR)/include/ncurses_cfg.h
	@echo "#include <fcntl.h>" >> $(NCURSES_BUILD_DIR)/include/ncurses_cfg.h
	@echo "#include <unistd.h>" >> $(NCURSES_BUILD_DIR)/include/ncurses_cfg.h
	@echo "#include <signal.h>" >> $(NCURSES_BUILD_DIR)/include/ncurses_cfg.h
	@echo "#include <stdlib.h>" >> $(NCURSES_BUILD_DIR)/include/ncurses_cfg.h
	@sed -i '' 's/mkdir $$@/mkdir -p $$@/g' $(NCURSES_BUILD_DIR)/ncurses/Makefile 2>/dev/null || sed -i 's/mkdir $$@/mkdir -p $$@/g' $(NCURSES_BUILD_DIR)/ncurses/Makefile 2>/dev/null || true

$(NCURSES_BUILD_DIR):
	@mkdir -p $@

$(LIBNCURSES_A): $(NCURSES_BUILD_DIR)/Makefile $(LIBC_A) $(CRT0_O)
	@echo "  [GEN-NCURSES-FALLBACKS] Generowanie wbudowanych terminali (xterm-256color, vt100)..."
	@python3 scripts/generate_ncurses_fallbacks.py $(NCURSES_BUILD_DIR)/ncurses/fallback.c
	@sed -i '' 's/mkdir $$@/mkdir -p $$@/g' $(NCURSES_BUILD_DIR)/ncurses/Makefile 2>/dev/null || sed -i 's/mkdir $$@/mkdir -p $$@/g' $(NCURSES_BUILD_DIR)/ncurses/Makefile 2>/dev/null || true
	@echo "  [MAKE-NCURSES] Kompilacja GNU Ncurses..."
	@$(MAKE) -C $(NCURSES_BUILD_DIR)/include
	@$(MAKE) -C $(NCURSES_BUILD_DIR)/ncurses
	@mkdir -p $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include $(ROOTFS_DIR)/lib
	@cp $(NCURSES_BUILD_DIR)/lib/libncurses.a $(SYSROOT_DIR)/usr/lib/
	@cp $(NCURSES_BUILD_DIR)/lib/libncurses.a $(ROOTFS_DIR)/lib/
	@cp $(NCURSES_BUILD_DIR)/include/*.h $(SYSROOT_DIR)/usr/include/ 2>/dev/null || true
	@cp third_party/ncurses/include/curses.h $(SYSROOT_DIR)/usr/include/ 2>/dev/null || true
	@cp $(SYSROOT_DIR)/usr/include/curses.h $(SYSROOT_DIR)/usr/include/ncurses.h 2>/dev/null || true

# GNU nano Targets (Direct Compilation against libc & libncurses)
$(NANO_BUILD_DIR)/revision.h: | $(NANO_BUILD_DIR)
	@mkdir -p $(NANO_BUILD_DIR)
	@echo '#define REVISION "GNU nano 9.2.4"' > $@

$(NANO_BUILD_DIR):
	@mkdir -p $@

NANO_SRCS := $(wildcard third_party/nano/src/*.c)
$(ROOTFS_DIR)/bin/nano: $(NANO_SRCS) $(NANO_BUILD_DIR)/revision.h $(LIBNCURSES_A) $(LIBC_A) $(CRT0_O) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(NANO_BUILD_DIR)
	@echo "  [MAKE-NANO] Kompilacja GNU nano..."
	@$(CC) $(USER_CFLAGS) -nostdlib -I$(NANO_BUILD_DIR) -Ithird_party/nano/src -isystem $(abspath $(SYSROOT_DIR))/usr/include \
	    -DPACKAGE=\"nano\" -DVERSION=\"7.2\" -DENABLE_UTF8=1 -DENABLE_COLOR=1 -DENABLE_NANORC=1 \
	    -DENABLE_MULTIBUFFER=1 -DHAVE_NCURSES_H=1 -DHAVE_CURSES_H=1 -DHAVE_LIMITS_H=1 -DHAVE_SYS_PARAM_H=1 \
	    -DHAVE_TERMIOS_H=1 -DHAVE_UNISTD_H=1 -DHAVE_FCNTL_H=1 -DHAVE_DIRENT_H=1 -DHAVE_PWD_H=1 -DHAVE_GRP_H=1 \
	    -DHAVE_GETOPT_H=1 -DHAVE_GETOPT_LONG=1 -DHAVE_SIGACTION=1 -DHAVE_SIGNAL_H=1 \
	    -DNANO_REG_EXTENDED=REG_EXTENDED -DSYSCONFDIR=\"/etc\" \
	    $(NANO_SRCS) $(CRT0_O) -L$(abspath $(SYSROOT_DIR))/usr/lib -lncurses $(LIBC_A) $(LIBM_A) -o $@

# Build Kernel Modules (.sko)
$(MODULE_DIR)/hello.sko: modules/hello/hello.c | $(ROOTFS_DIR)
	@mkdir -p $(MODULE_DIR)
	@echo "  [CC-SKO] $< -> $@"
	@$(CC) $(MODULE_CFLAGS) -c $< -o $@

$(MODULE_DIR)/dummy_dev.sko: modules/dummy_dev/dummy_dev.c | $(ROOTFS_DIR)
	@mkdir -p $(MODULE_DIR)
	@echo "  [CC-SKO] $< -> $@"
	@$(CC) $(MODULE_CFLAGS) -c $< -o $@

# Sysroot Target (cross-compilation root for third-party libraries)
$(SYSROOT_STAMP): $(LIBC_A) $(CRT0_O) $(LIBM_A) $(LIBDL_A) $(LIBC_SO) $(LIBM_SO)
	@mkdir -p $(SYSROOT_DIR)/usr/include $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/lib/pkgconfig $(SYSROOT_DIR)/usr/share/pkgconfig $(SYSROOT_DIR)/usr/share/xcb $(SYSROOT_DIR)/usr/include/libdrm $(SYSROOT_DIR)/usr/include/drm $(SYSROOT_DIR)/usr/include/libxcvt $(SYSROOT_DIR)/usr/include/pixman-1 $(SYSROOT_DIR)/usr/include/X11/Xtrans
	@cp -r libc/include/* $(SYSROOT_DIR)/usr/include/
	@cp $(LIBC_A) $(CRT0_O) $(LIBM_A) $(LIBDL_A) $(SYSROOT_DIR)/usr/lib/ 2>/dev/null || true
	@cp $(LIBC_SO) $(LIBM_SO) $(SYSROOT_DIR)/usr/lib/ 2>/dev/null || true
	@cp -f compat/linux/include/drm/*.h $(SYSROOT_DIR)/usr/include/ 2>/dev/null || true
	@cp -f compat/linux/include/drm/*.h $(SYSROOT_DIR)/usr/include/libdrm/ 2>/dev/null || true
	@cp -f compat/linux/include/drm/*.h $(SYSROOT_DIR)/usr/include/drm/ 2>/dev/null || true
	@cp -r third_party/xorgproto/include/* $(SYSROOT_DIR)/usr/include/ 2>/dev/null || true
	@cp third_party/xtrans/*.h third_party/xtrans/*.c $(SYSROOT_DIR)/usr/include/X11/Xtrans/ 2>/dev/null || true
	@cp third_party/xtrans/*.h third_party/xtrans/*.c $(SYSROOT_DIR)/usr/include/X11/ 2>/dev/null || true
	@if [ -f third_party/xcb-proto/xcb-proto.pc ]; then \
	    cp third_party/xcb-proto/xcb-proto.pc $(SYSROOT_DIR)/usr/lib/pkgconfig/ 2>/dev/null || true; \
	elif [ -f third_party/xcb-proto/xcb-proto.pc.in ]; then \
	    sed -e 's|@prefix@|/usr|g' \
	        -e 's|@exec_prefix@|$${prefix}|g' \
	        -e 's|@datarootdir@|$${prefix}/share|g' \
	        -e 's|@datadir@|$${datarootdir}|g' \
	        -e 's|@xcbincludedir@|$${datadir}/xcb|g' \
	        -e 's|@PACKAGE_VERSION@|1.17.0|g' \
	        third_party/xcb-proto/xcb-proto.pc.in > $(SYSROOT_DIR)/usr/lib/pkgconfig/xcb-proto.pc 2>/dev/null || true; \
	fi
	@cp -r third_party/xcb-proto/src/* $(SYSROOT_DIR)/usr/share/xcb/ 2>/dev/null || true
	@cp -f third_party/libxcvt/include/libxcvt/*.h $(SYSROOT_DIR)/usr/include/libxcvt/ 2>/dev/null || true
	@cp -f third_party/pixman/pixman/*.h $(SYSROOT_DIR)/usr/include/pixman-1/ 2>/dev/null || true
	@cp -f third_party/pixman/pixman/*.h $(SYSROOT_DIR)/usr/include/ 2>/dev/null || true
	@for pcin in third_party/xorgproto/*.pc.in; do \
	    pcname=$$(basename $$pcin .in); \
	    sed -e 's|@prefix@|/usr|g' \
	        -e 's|@exec_prefix@|/usr|g' \
	        -e 's|@libdir@|/usr/lib|g' \
	        -e 's|@includedir@|/usr/include|g' \
	        -e 's|@datarootdir@|/usr/share|g' \
	        -e 's|@datadir@|/usr/share|g' \
	        -e 's|@PACKAGE_VERSION@|2024.1|g' \
	        -e 's|@VERSION@|2024.1|g' \
	        $$pcin > $(SYSROOT_DIR)/usr/lib/pkgconfig/$$pcname; \
	done
	@printf "prefix=/usr\nexec_prefix=\$${prefix}\nlibdir=\$${exec_prefix}/lib\nincludedir=\$${prefix}/include\n\nName: pthread-stubs\nDescription: Stubs for pthread functions\nVersion: 0.4\nLibs:\nCflags:\n" > $(SYSROOT_DIR)/usr/lib/pkgconfig/pthread-stubs.pc
	@printf "prefix=/usr\nexec_prefix=\$${prefix}\nlibdir=\$${exec_prefix}/lib\nincludedir=\$${prefix}/include\n\nName: Xau\nDescription: X authorization file management library\nVersion: 1.0.11\nLibs: -L\$${libdir} -lXau\nCflags: -I\$${includedir}\n" > $(SYSROOT_DIR)/usr/lib/pkgconfig/xau.pc
	@printf "prefix=/usr\nexec_prefix=\$${prefix}\nlibdir=\$${exec_prefix}/lib\nincludedir=\$${prefix}/include\n\nName: Xdmcp\nDescription: X Display Manager Control Protocol library\nVersion: 1.1.5\nLibs: -L\$${libdir} -lXdmcp\nCflags: -I\$${includedir}\n" > $(SYSROOT_DIR)/usr/lib/pkgconfig/xdmcp.pc
	@printf "prefix=/usr\nexec_prefix=\$${prefix}\nlibdir=\$${exec_prefix}/lib\nincludedir=\$${prefix}/include\n\nName: zlib\nDescription: zlib compression library\nVersion: 1.3.1\nLibs: -L\$${libdir} -lz\nCflags: -I\$${includedir}\n" > $(SYSROOT_DIR)/usr/lib/pkgconfig/zlib.pc
	@printf "prefix=/usr\nexec_prefix=\$${prefix}\nlibdir=\$${exec_prefix}/lib\nincludedir=\$${prefix}/include\n\nName: libxcvt\nDescription: VESA CVT standard timing modelines generator\nVersion: 0.1.2\nLibs: -L\$${libdir} -lxcvt\nCflags: -I\$${includedir}\n" > $(SYSROOT_DIR)/usr/lib/pkgconfig/libxcvt.pc
	@printf "prefix=/usr\nexec_prefix=\$${prefix}\nlibdir=\$${exec_prefix}/lib\nincludedir=\$${prefix}/include\n\nName: pciaccess\nDescription: Generic PCI access library\nVersion: 0.17\nLibs: -L\$${libdir} -lpciaccess\nCflags: -I\$${includedir}\n" > $(SYSROOT_DIR)/usr/lib/pkgconfig/pciaccess.pc
	@printf "prefix=/usr\nexec_prefix=\$${prefix}\nlibdir=\$${exec_prefix}/lib\nincludedir=\$${prefix}/include\n\nName: pixman-1\nDescription: The pixman library (version 1)\nVersion: 0.43.4\nLibs: -L\$${libdir} -lpixman-1\nCflags: -I\$${includedir}/pixman-1 -I\$${includedir}\n" > $(SYSROOT_DIR)/usr/lib/pkgconfig/pixman-1.pc
	@rm -f $(SYSROOT_DIR)/usr/lib/*.la
	@touch $@

sysroot: $(SYSROOT_STAMP)


# Generate configure for GNU file if missing
third_party/file/configure:
	@echo "  [PRECONF-FILE] Generowanie configure dla GNU file..."
	@cd third_party/file && autoreconf -fi || true

# Build GNU file using its original Autotools configure & Makefile
$(FILE_BUILD_DIR)/Makefile: third_party/file/configure $(SYSROOT_STAMP) | $(FILE_BUILD_DIR)
	@echo "  [CONF-FILE] Konfiguracja GNU file (Autotools cross-compile)..."
	@cd $(FILE_BUILD_DIR) && \
	../../../third_party/file/configure \
	    --host=x86_64-elf \
	    --prefix=/usr \
	    --sysconfdir=/etc \
	    --datadir=/usr/share \
	    --disable-shared \
	    --enable-static \
	    --disable-zlib \
	    --disable-bzlib \
	    --disable-xzlib \
	    --disable-zstdlib \
	    --disable-lzlib \
	    --disable-lrziplib \
	    --disable-lz4lib \
	    --disable-libseccomp \
	    --disable-landlock \
	    --disable-warnings \
	    CC="$(CC)" \
	    AR="$(AR)" \
	    RANLIB="$(RANLIB)" \
	    CFLAGS="-O2 -ffreestanding -fno-builtin -isystem $(abspath $(SYSROOT_DIR))/usr/include -B$(abspath $(SYSROOT_DIR))/usr/lib" \
	    LDFLAGS="-nostdlib -L$(abspath $(SYSROOT_DIR))/usr/lib -B$(abspath $(SYSROOT_DIR))/usr/lib" \
	    LIBS="-lc"

$(FILE_BUILD_DIR):
	@mkdir -p $@

$(ROOTFS_DIR)/bin/file: $(FILE_BUILD_DIR)/Makefile $(LIBC_A) $(CRT0_O) $(LIBM_A) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(ROOTFS_DIR)/lib
	@echo "  [MAKE-FILE] Kompilacja GNU file za pomocą oryginalnego Makefile..."
	@$(MAKE) -C $(FILE_BUILD_DIR)/src file_LDADD="$(abspath $(SYSROOT_DIR))/usr/lib/crt0.o libmagic.la -lm"
	@cp $(FILE_BUILD_DIR)/src/file $@
	@cp $(FILE_BUILD_DIR)/src/.libs/libmagic.a $(ROOTFS_DIR)/lib/ 2>/dev/null || true

# Build /etc/magic database
$(MAGIC_DB): scripts/build_magic_db.py | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/etc $(ROOTFS_DIR)/usr/share/misc
	@echo "  [MAGIC-DB] Generowanie bazy /etc/magic..."
	@python3 scripts/build_magic_db.py third_party/file/magic/Magdir $@
	@cp $@ $(ROOTFS_DIR)/usr/share/misc/magic 2>/dev/null || true

# Zsh Targets (Autotools cross-compile)
third_party/zsh/configure:
	@echo "  [PRECONF-ZSH] Generowanie configure dla Zsh..."
	@cd third_party/zsh && (./Util/preconfig || (autoconf && autoheader && echo > stamp-h.in))

$(ZSH_BUILD_DIR)/Makefile: third_party/zsh/configure $(LIBNCURSES_A) $(SYSROOT_STAMP) | $(ZSH_BUILD_DIR)
	@echo "  [CONF-ZSH] Konfiguracja Zsh (Autotools cross-compile)..."
	@cd $(ZSH_BUILD_DIR) && \
	../../../third_party/zsh/configure \
	    --host=x86_64-elf \
	    --prefix=/usr \
	    --sysconfdir=/etc \
	    --disable-dynamic \
	    --disable-gdbm \
	    --disable-pcre \
	    --disable-cap \
	    --with-term-lib="ncurses" \
	    CC="$(CC)" \
	    CPP="$(CC) -E -isystem $(abspath $(SYSROOT_DIR))/usr/include" \
	    AR="$(AR)" \
	    RANLIB="$(RANLIB)" \
	    CFLAGS="-O2 -ffreestanding -fno-builtin -isystem $(abspath $(SYSROOT_DIR))/usr/include -B$(abspath $(SYSROOT_DIR))/usr/lib" \
	    CPPFLAGS="-isystem $(abspath $(SYSROOT_DIR))/usr/include" \
	    LDFLAGS="-nostdlib -L$(abspath $(SYSROOT_DIR))/usr/lib -B$(abspath $(SYSROOT_DIR))/usr/lib" \
	    LIBS="$(abspath $(SYSROOT_DIR))/usr/lib/crt0.o -lncurses -lc -lm"

$(ZSH_BUILD_DIR):
	@mkdir -p $@

$(ROOTFS_DIR)/bin/zsh: $(ZSH_BUILD_DIR)/Makefile $(LIBNCURSES_A) $(LIBC_A) $(CRT0_O) $(LIBM_A) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin
	@echo "  [MAKE-ZSH] Kompilacja powłoki Zsh..."
	@$(MAKE) -C $(ZSH_BUILD_DIR)/Src zsh
	@cp $(ZSH_BUILD_DIR)/Src/zsh $@

# Fastfetch Targets (CMake cross-compile)
$(FASTFETCH_BUILD_DIR)/Makefile: $(SYSROOT_STAMP) $(LIBC_A) $(CRT0_O) $(LIBM_A) $(LIBDL_A) | $(FASTFETCH_BUILD_DIR)
	@echo "  [CONF-FASTFETCH] Konfiguracja Fastfetch (CMake cross-compile)..."
	@cd $(FASTFETCH_BUILD_DIR) && \
	cmake ../../../third_party/fastfetch \
	    -DCMAKE_SYSTEM_NAME=Linux \
	    -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
	    -DCMAKE_C_COMPILER="$(shell which -a $(CC) 2>/dev/null | grep -v '\.bear' | head -n 1 || which $(CC) 2>/dev/null || echo $(CC))" \
	    -DCMAKE_C_FLAGS="-isystem $(abspath $(SYSROOT_DIR))/usr/include $(LINUX_COMPAT_CFLAGS) -D__linux__=1 -ffreestanding -fno-builtin -O2" \
	    -DCMAKE_EXE_LINKER_FLAGS="-nostdlib -L$(abspath $(SYSROOT_DIR))/usr/lib -B$(abspath $(SYSROOT_DIR))/usr/lib $(abspath $(SYSROOT_DIR))/usr/lib/crt0.o" \
	    -DCMAKE_C_STANDARD_LIBRARIES="-Wl,--start-group $(abspath $(SYSROOT_DIR))/usr/lib/libc.a $(abspath $(SYSROOT_DIR))/usr/lib/libm.a $(abspath $(SYSROOT_DIR))/usr/lib/libdl.a -Wl,--end-group" \
	    -DBINARY_LINK_TYPE=static \
	    -DENABLE_VULKAN=OFF \
	    -DENABLE_WAYLAND=OFF \
	    -DENABLE_XCB_RANDR=OFF \
	    -DENABLE_XRANDR=OFF \
	    -DENABLE_DRM=OFF \
	    -DENABLE_GIO=OFF \
	    -DENABLE_DCONF=OFF \
	    -DENABLE_DBUS=OFF \
	    -DENABLE_SQLITE3=OFF \
	    -DENABLE_PULSE=OFF \
	    -DENABLE_ELF=OFF \
	    -DENABLE_ZLIB=OFF \
	    -DENABLE_LUA=OFF \
	    -DENABLE_QUICKJS=OFF \
	    -DENABLE_OPENCL=OFF \
	    -DENABLE_GLX=OFF \
	    -DENABLE_EGL=OFF \
	    -DENABLE_IMAGEMAGICK7=OFF \
	    -DENABLE_IMAGEMAGICK6=OFF \
	    -DENABLE_CHAFA=OFF \
	    -DENABLE_LIBZFS=OFF \
	    -DENABLE_DDCUTIL=OFF \
	    -DENABLE_THREADS=OFF \
	    -DBUILD_TESTS=OFF \
	    -DBUILD_FLASHFETCH=OFF

$(FASTFETCH_BUILD_DIR):
	@mkdir -p $@

$(ROOTFS_DIR)/bin/fastfetch: $(FASTFETCH_BUILD_DIR)/Makefile $(SYSROOT_STAMP) $(LIBC_A) $(CRT0_O) $(LIBM_A) $(LIBDL_A) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin
	@echo "  [MAKE-FASTFETCH] Kompilacja narzędzia Fastfetch..."
	@$(MAKE) -C $(FASTFETCH_BUILD_DIR) fastfetch
	@cp $(FASTFETCH_BUILD_DIR)/fastfetch $@

# zlib Targets (Cross-compiled via original Makefile)
ZLIB_SRCS := $(wildcard third_party/zlib/*.c)
ZLIB_OBJS := $(patsubst third_party/zlib/%.c,$(ZLIB_BUILD_DIR)/%.o,$(ZLIB_SRCS))

$(ZLIB_BUILD_DIR):
	@mkdir -p $@

$(ZLIB_BUILD_DIR)/%.o: third_party/zlib/%.c $(SYSROOT_STAMP) | $(ZLIB_BUILD_DIR)
	@echo "  [CC-ZLIB] $<"
	@$(CC) $(USER_CFLAGS) -DZ_HAVE_UNISTD_H=1 -DHAVE_UNISTD_H=1 -Ithird_party/zlib -c $< -o $@

$(LIBZ_A): $(ZLIB_OBJS)
	@mkdir -p $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include $(ROOTFS_DIR)/lib
	@echo "  [AR-ZLIB] $@"
	@$(AR) rcs $@ $(ZLIB_OBJS)
	@cp third_party/zlib/zlib.h third_party/zlib/zconf.h $(SYSROOT_DIR)/usr/include/
	@cp $@ $(ROOTFS_DIR)/lib/

# Git Targets (Libre-WD-40 cross-compile using original Makefile and userland/config/git/config.mak)
$(ROOTFS_DIR)/bin/git: $(LIBZ_A) $(SYSROOT_STAMP) $(LIBC_A) $(CRT0_O) $(LIBM_A) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin
	@echo "  [MAKE-GIT] Kompilacja narzędzia Git (Libre-WD-40)..."
	@cp userland/config/git/config.mak third_party/git/config.mak
	@$(MAKE) -C third_party/git \
	    CC="$(CC)" \
	    AR="$(AR)" \
	    RANLIB="$(RANLIB)" \
	    CFLAGS="-O2 -ffreestanding -fno-builtin -isystem $(abspath $(SYSROOT_DIR))/usr/include -B$(abspath $(SYSROOT_DIR))/usr/lib" \
	    LDFLAGS="-nostdlib -L$(abspath $(SYSROOT_DIR))/usr/lib -B$(abspath $(SYSROOT_DIR))/usr/lib $(abspath $(SYSROOT_DIR))/usr/lib/crt0.o" \
	    EXTLIBS="$(abspath $(SYSROOT_DIR))/usr/lib/libz.a $(abspath $(SYSROOT_DIR))/usr/lib/libc.a $(abspath $(SYSROOT_DIR))/usr/lib/libm.a" \
	    uname_S=Linux uname_M=x86_64 \
	    git
	@cp third_party/git/git $@
	@rm -f third_party/git/config.mak

# X11 Headers and Protocol Specifications (xorgproto & xtrans)
$(SYSROOT_DIR)/usr/include/X11/X.h: $(SYSROOT_STAMP)
	@mkdir -p $(SYSROOT_DIR)/usr/include/X11/Xtrans $(SYSROOT_DIR)/usr/lib/pkgconfig $(SYSROOT_DIR)/usr/share/xcb
	@cp -r third_party/xorgproto/include/* $(SYSROOT_DIR)/usr/include/
	@cp third_party/xtrans/*.h third_party/xtrans/*.c $(SYSROOT_DIR)/usr/include/X11/Xtrans/ 2>/dev/null || true
	@cp third_party/xtrans/*.h third_party/xtrans/*.c $(SYSROOT_DIR)/usr/include/X11/ 2>/dev/null || true
	@cp third_party/xcb-proto/xcb-proto.pc $(SYSROOT_DIR)/usr/lib/pkgconfig/ 2>/dev/null || true
	@cp -r third_party/xcb-proto/src/* $(SYSROOT_DIR)/usr/share/xcb/ 2>/dev/null || true

# libXau Target
$(ROOTFS_DIR)/lib/libXau.so: $(SYSROOT_DIR)/usr/include/X11/X.h $(LIBC_SO)
	@mkdir -p $(BUILD_DIR)/third_party/libXau $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib
	@echo "  [MAKE-LIBXAU] Kompilacja libXau..."
	@cd $(BUILD_DIR)/third_party/libXau && \
	    rm -f *.o && \
	    for src in $(abspath third_party/libXau)/Au*.c; do \
	        if [ "$$(basename $$src)" != "Autest.c" ]; then \
	            $(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath third_party/libXau/include) -c "$$src" -o "$$(basename $$src .c).o"; \
	        fi; \
	    done && \
	    $(LD) -shared -soname libXau.so.6 -o $(abspath $(SYSROOT_DIR))/usr/lib/libXau.so *.o && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXau.so $(abspath $(ROOTFS_DIR))/lib/libXau.so && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXau.so $(abspath $(ROOTFS_DIR))/lib/libXau.so.6 && \
	    cp -r $(abspath third_party/libXau/include/X11)/* $(abspath $(SYSROOT_DIR))/usr/include/X11/

# libXdmcp Target
$(ROOTFS_DIR)/lib/libXdmcp.so: $(SYSROOT_DIR)/usr/include/X11/X.h $(LIBC_SO)
	@mkdir -p $(BUILD_DIR)/third_party/libXdmcp $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib
	@echo "  [MAKE-LIBXDMCP] Kompilacja libXdmcp..."
	@cd $(BUILD_DIR)/third_party/libXdmcp && \
	    rm -f *.o && \
	    for src in $(abspath third_party/libXdmcp)/*.c; do \
	        $(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DHASXDMAUTH=1 -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath third_party/libXdmcp)/include -c "$$src" -o "$$(basename $$src .c).o"; \
	    done && \
	    $(LD) -shared -soname libXdmcp.so.6 -o $(abspath $(SYSROOT_DIR))/usr/lib/libXdmcp.so *.o && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXdmcp.so $(abspath $(ROOTFS_DIR))/lib/libXdmcp.so && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXdmcp.so $(abspath $(ROOTFS_DIR))/lib/libXdmcp.so.6 && \
	    cp -r $(abspath third_party/libXdmcp/include/X11)/* $(abspath $(SYSROOT_DIR))/usr/include/X11/

# libxcb Target
$(ROOTFS_DIR)/lib/libxcb.so: $(ROOTFS_DIR)/lib/libXau.so $(ROOTFS_DIR)/lib/libXdmcp.so
	@mkdir -p $(BUILD_DIR)/third_party/libxcb $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/xcb
	@echo "  [CONF-LIBXCB] Konfiguracja i kompilacja libxcb..."
	@cd $(BUILD_DIR)/third_party/libxcb && \
	    if [ ! -f Makefile ]; then \
	        PKG_CONFIG_PATH="$(abspath $(SYSROOT_DIR))/usr/lib/pkgconfig" \
	        $(abspath third_party/libxcb)/configure --host=x86_64-elf --prefix=/usr --enable-shared --disable-static --disable-devel-docs \
	            NEEDED_CFLAGS="-I$(abspath $(SYSROOT_DIR))/usr/include" \
	            NEEDED_LIBS="-L$(abspath $(SYSROOT_DIR))/usr/lib -lXau" \
	            CC="$(CC)" \
	            CFLAGS="-fPIC -O2 -ffreestanding -fno-builtin -isystem $(abspath $(SYSROOT_DIR))/usr/include -B$(abspath $(SYSROOT_DIR))/usr/lib" \
	            LDFLAGS="-nostdlib -L$(abspath $(SYSROOT_DIR))/usr/lib -B$(abspath $(SYSROOT_DIR))/usr/lib"; \
	    fi && \
	    export PYTHONPATH="$(abspath third_party/xcb-proto)" && \
	    $(MAKE) -C src && \
	    cp -f src/*.h $(abspath $(SYSROOT_DIR))/usr/include/xcb/ && \
	    cd src && \
	    $(LD) -shared -soname libxcb.so.1 -o $(abspath $(SYSROOT_DIR))/usr/lib/libxcb.so.1 *.o && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libxcb.so.1 $(abspath $(SYSROOT_DIR))/usr/lib/libxcb.so && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libxcb.so.1 $(abspath $(ROOTFS_DIR))/lib/libxcb.so.1 && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libxcb.so.1 $(abspath $(ROOTFS_DIR))/lib/libxcb.so

# libX11 Target
$(ROOTFS_DIR)/lib/libX11.so: $(ROOTFS_DIR)/lib/libxcb.so $(ROOTFS_DIR)/lib/libXau.so $(ROOTFS_DIR)/lib/libXdmcp.so
	@mkdir -p $(BUILD_DIR)/third_party/libX11 $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/X11 $(SYSROOT_DIR)/usr/include/xcb
	@echo "  [MAKE-LIBX11] Kompilacja libX11..."
	@cp -rf third_party/xorgproto/include/X11/* $(SYSROOT_DIR)/usr/include/X11/ 2>/dev/null || true
	@cp -f third_party/libxcb/src/*.h $(SYSROOT_DIR)/usr/include/xcb/ 2>/dev/null || true
	@cp -f $(BUILD_DIR)/third_party/libxcb/src/*.h $(SYSROOT_DIR)/usr/include/xcb/ 2>/dev/null || true
	@cd $(BUILD_DIR)/third_party/libX11 && \
	    if [ ! -f Makefile ]; then \
	        PKG_CONFIG_PATH="$(abspath $(SYSROOT_DIR))/usr/lib/pkgconfig" \
	        xorg_cv_malloc0_returns_null=no \
	        $(abspath third_party/libX11)/configure \
	            --host=x86_64-elf \
	            --prefix=/usr \
	            --with-keysymdefdir="$(abspath $(SYSROOT_DIR))/usr/include/X11" \
	            --enable-shared \
	            --disable-static \
	            --disable-specs \
	            --disable-unit-tests \
	            CC="$(CC)" \
	            CFLAGS="-fPIC -O2 -ffreestanding -fno-builtin -D_POSIX_THREAD_SAFE_FUNCTIONS=1 -isystem $(abspath $(SYSROOT_DIR))/usr/include -B$(abspath $(SYSROOT_DIR))/usr/lib" \
	            LDFLAGS="-nostdlib -L$(abspath $(SYSROOT_DIR))/usr/lib -B$(abspath $(SYSROOT_DIR))/usr/lib"; \
	    fi && \
	    $(MAKE) -C modules && \
	    $(MAKE) -C src && \
	    $(LD) -shared -soname libX11.so.6 -o $(abspath $(SYSROOT_DIR))/usr/lib/libX11.so.6 --whole-archive src/.libs/libX11.a --no-whole-archive && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libX11.so.6 $(abspath $(SYSROOT_DIR))/usr/lib/libX11.so && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libX11.so.6 $(abspath $(ROOTFS_DIR))/lib/libX11.so.6 && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libX11.so.6 $(abspath $(ROOTFS_DIR))/lib/libX11.so && \
	    cp -r $(abspath third_party/libX11/include/X11)/* $(abspath $(SYSROOT_DIR))/usr/include/X11/ && \
	    cp -f $(abspath $(BUILD_DIR)/third_party/libX11)/*.pc $(abspath $(SYSROOT_DIR))/usr/lib/pkgconfig/ 2>/dev/null || true

# libxkbfile Target
$(ROOTFS_DIR)/lib/libxkbfile.so: $(SYSROOT_STAMP) $(ROOTFS_DIR)/lib/libX11.so
	@mkdir -p $(BUILD_DIR)/third_party/libxkbfile $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/X11/extensions
	@echo "  [MAKE-LIBXKBFILE] Kompilacja libxkbfile..."
	@cd $(BUILD_DIR)/third_party/libxkbfile && \
	    rm -f *.o && \
	    for src in $(abspath third_party/libxkbfile)/src/*.c; do \
	        $(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DHAVE_STRCASECMP=1 -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath third_party/libxkbfile)/include -I$(abspath third_party/libxkbfile)/include/X11/extensions -I$(abspath third_party/libxkbfile)/src -c "$$src" -o "$$(basename $$src .c).o" || exit 1; \
	    done && \
	    $(LD) -shared -soname libxkbfile.so.1 -o $(abspath $(SYSROOT_DIR))/usr/lib/libxkbfile.so.1 *.o && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libxkbfile.so.1 $(abspath $(SYSROOT_DIR))/usr/lib/libxkbfile.so && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libxkbfile.so.1 $(abspath $(ROOTFS_DIR))/lib/libxkbfile.so.1 && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libxkbfile.so.1 $(abspath $(ROOTFS_DIR))/lib/libxkbfile.so && \
	    cp -f $(abspath third_party/libxkbfile)/include/X11/extensions/*.h $(abspath $(SYSROOT_DIR))/usr/include/X11/extensions/ 2>/dev/null || true

# libfontenc Target
$(ROOTFS_DIR)/lib/libfontenc.so: $(SYSROOT_STAMP) $(LIBZ_A)
	@mkdir -p $(BUILD_DIR)/third_party/libfontenc $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/X11/fonts
	@echo "  [MAKE-LIBFONTENC] Kompilacja libfontenc..."
	@cd $(BUILD_DIR)/third_party/libfontenc && \
	    rm -f *.o && \
	    for src in $(abspath third_party/libfontenc)/src/*.c; do \
	        $(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DHAVE_REALLOCARRAY=1 -DFONT_ENCODINGS_DIRECTORY='"/usr/share/fonts/X11/encodings/encodings.dir"' -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath third_party/libfontenc)/include -I$(abspath third_party/libfontenc)/src -c "$$src" -o "$$(basename $$src .c).o" || exit 1; \
	    done && \
	    $(LD) -shared -soname libfontenc.so.1 -o $(abspath $(SYSROOT_DIR))/usr/lib/libfontenc.so.1 *.o && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libfontenc.so.1 $(abspath $(SYSROOT_DIR))/usr/lib/libfontenc.so && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libfontenc.so.1 $(abspath $(ROOTFS_DIR))/lib/libfontenc.so.1 && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libfontenc.so.1 $(abspath $(ROOTFS_DIR))/lib/libfontenc.so && \
	    cp -f $(abspath third_party/libfontenc)/include/X11/fonts/*.h $(abspath $(SYSROOT_DIR))/usr/include/X11/fonts/ 2>/dev/null || true

# libXfont2 Target
$(ROOTFS_DIR)/lib/libXfont2.so: $(SYSROOT_STAMP) $(ROOTFS_DIR)/lib/libfontenc.so $(LIBZ_A)
	@mkdir -p $(BUILD_DIR)/third_party/libXfont2 $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib
	@echo "  [MAKE-LIBXFONT2] Kompilacja libXfont2..."
	@cd $(BUILD_DIR)/third_party/libXfont2 && \
	    if [ ! -f Makefile ]; then \
	        PKG_CONFIG_LIBDIR="$(abspath $(SYSROOT_DIR))/usr/lib/pkgconfig" \
	        PKG_CONFIG_PATH="$(abspath $(SYSROOT_DIR))/usr/lib/pkgconfig" \
	        $(abspath third_party/libXfont2)/configure --host=x86_64-elf --prefix=/usr --enable-shared --disable-static \
	            --disable-freetype --disable-devel-docs \
	            CC="$(CC)" \
	            CFLAGS="-fPIC -O2 -ffreestanding -fno-builtin -isystem $(abspath $(SYSROOT_DIR))/usr/include -B$(abspath $(SYSROOT_DIR))/usr/lib" \
	            LDFLAGS="-nostdlib -L$(abspath $(SYSROOT_DIR))/usr/lib -B$(abspath $(SYSROOT_DIR))/usr/lib -lc" \
	            LIBS="-L$(abspath $(SYSROOT_DIR))/usr/lib -lfontenc -lz -lc"; \
	    fi && \
	    $(MAKE) -j4 && \
	    $(MAKE) install DESTDIR="$(abspath $(SYSROOT_DIR))" && \
	    rm -f $(abspath $(SYSROOT_DIR))/usr/lib/*.la && \
	    $(LD) -shared -soname libXfont2.so.2 -o $(abspath $(SYSROOT_DIR))/usr/lib/libXfont2.so.2 --whole-archive .libs/libXfont2.a --no-whole-archive && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXfont2.so.2 $(abspath $(SYSROOT_DIR))/usr/lib/libXfont2.so && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXfont2.so.2 $(abspath $(ROOTFS_DIR))/lib/libXfont2.so.2 && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXfont2.so.2 $(abspath $(ROOTFS_DIR))/lib/libXfont2.so

# libxcvt Target
$(ROOTFS_DIR)/lib/libxcvt.so: $(SYSROOT_STAMP)
	@mkdir -p $(BUILD_DIR)/third_party/libxcvt $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/libxcvt
	@echo "  [MAKE-LIBXCVT] Kompilacja libxcvt..."
	@$(CC) -fPIC -O2 -ffreestanding -fno-builtin -isystem $(abspath $(SYSROOT_DIR))/usr/include -Ithird_party/libxcvt/include -Ithird_party/libxcvt/lib -c third_party/libxcvt/lib/libxcvt.c -o $(BUILD_DIR)/third_party/libxcvt/libxcvt.o
	@$(LD) -shared -soname libxcvt.so.0 -o $(SYSROOT_DIR)/usr/lib/libxcvt.so $(BUILD_DIR)/third_party/libxcvt/libxcvt.o
	@cp -f $(SYSROOT_DIR)/usr/lib/libxcvt.so $@
	@cp -f $(SYSROOT_DIR)/usr/lib/libxcvt.so $(ROOTFS_DIR)/lib/libxcvt.so.0
	@cp -f third_party/libxcvt/include/libxcvt/*.h $(SYSROOT_DIR)/usr/include/libxcvt/ 2>/dev/null || true

# libpciaccess Target
$(ROOTFS_DIR)/lib/libpciaccess.so: $(SYSROOT_STAMP)
	@mkdir -p $(BUILD_DIR)/third_party/libpciaccess $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include
	@echo "  [MAKE-LIBPCIACCESS] Kompilacja libpciaccess..."
	@cd $(BUILD_DIR)/third_party/libpciaccess && \
	    rm -f *.o && \
	    touch config.h && \
	    for src in common_bridge.c common_iterator.c common_init.c common_interface.c common_capability.c common_device_name.c common_map.c common_vgaarb.c common_io.c linux_sysfs.c linux_devmem.c; do \
	        $(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DHAVE_STDINT_H -DHAVE_INTTYPES_H -DPCIIDS_PATH=\"/usr/share/hwdata\" -D__linux__=1 -I$(abspath third_party/libpciaccess/include) -I$(abspath third_party/libpciaccess/src) -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath $(BUILD_DIR)/third_party/libpciaccess) -c "$(abspath third_party/libpciaccess/src)/$$src" -o "$$(basename $$src .c).o" || true; \
	    done && \
	    $(LD) -shared -soname libpciaccess.so.0 -o $(abspath $(SYSROOT_DIR))/usr/lib/libpciaccess.so *.o && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libpciaccess.so $(abspath $(ROOTFS_DIR))/lib/libpciaccess.so && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libpciaccess.so $(abspath $(ROOTFS_DIR))/lib/libpciaccess.so.0 && \
	    cp -f $(abspath third_party/libpciaccess/include/pciaccess.h) $(abspath $(SYSROOT_DIR))/usr/include/

# libpixman-1 Target
$(ROOTFS_DIR)/lib/libpixman-1.so: $(SYSROOT_STAMP) $(LIBM_SO)
	@mkdir -p $(BUILD_DIR)/third_party/pixman $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/pixman-1
	@echo "  [MAKE-PIXMAN] Kompilacja pixman..."
	@cd $(BUILD_DIR)/third_party/pixman && \
	    rm -f *.o && \
	    printf '#ifndef CONFIG_H\n#define CONFIG_H\n#define PACKAGE "pixman"\n#define PACKAGE_VERSION "0.43.4"\n#define PIXMAN_NO_TLS 1\n#define HAVE_POSIX_MEMALIGN 1\n#define HAVE_SIGACTION 1\n#define HAVE_ALARM 1\n#define HAVE_MPROTECT 1\n#define HAVE_GETPAGESIZE 1\n#define HAVE_MMAP 1\n#define HAVE_GETTIMEOFDAY 1\n#define SIZEOF_LONG 8\n#endif\n' > config.h && \
	    for src in pixman.c pixman-access.c pixman-access-accessors.c pixman-bits-image.c \
	               pixman-combine32.c pixman-combine-float.c pixman-conical-gradient.c \
	               pixman-edge.c pixman-edge-accessors.c pixman-fast-path.c pixman-filter.c \
	               pixman-general.c pixman-glyph.c pixman-gradient-walker.c pixman-image.c \
	               pixman-implementation.c pixman-linear-gradient.c pixman-matrix.c \
	               pixman-noop.c pixman-radial-gradient.c pixman-region16.c pixman-region32.c \
	               pixman-solid-fill.c pixman-trap.c pixman-utils.c pixman-x86.c pixman-arm.c pixman-ppc.c pixman-mips.c; do \
	        $(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DPIXMAN_NO_TLS=1 -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath third_party/pixman/pixman) -I. -c "$(abspath third_party/pixman/pixman)/$$src" -o "$$(basename $$src .c).o" || true; \
	    done && \
	    $(LD) -shared -soname libpixman-1.so.0 -o $(abspath $(SYSROOT_DIR))/usr/lib/libpixman-1.so *.o && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libpixman-1.so $(abspath $(ROOTFS_DIR))/lib/libpixman-1.so && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libpixman-1.so $(abspath $(ROOTFS_DIR))/lib/libpixman-1.so.0 && \
	    cp -f $(abspath third_party/pixman/pixman)/*.h $(abspath $(SYSROOT_DIR))/usr/include/pixman-1/ 2>/dev/null || true && \
	    cp -f $(abspath third_party/pixman/pixman)/*.h $(abspath $(SYSROOT_DIR))/usr/include/ 2>/dev/null || true

# libdrm Target
$(ROOTFS_DIR)/lib/libdrm.so: $(LIBC_A) $(SYSROOT_STAMP) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/lib/pkgconfig $(SYSROOT_DIR)/usr/include/libdrm
	@echo "  [LD-LIBDRM] $@"
	@$(LD) -shared -soname libdrm.so.2 -o $(SYSROOT_DIR)/usr/lib/libdrm.so build/libc/src/drm/drm.o
	@cp -f $(SYSROOT_DIR)/usr/lib/libdrm.so $@
	@cp -f $(SYSROOT_DIR)/usr/lib/libdrm.so $(ROOTFS_DIR)/lib/libdrm.so.2
	@printf "prefix=/usr\nexec_prefix=\$${prefix}\nlibdir=\$${exec_prefix}/lib\nincludedir=\$${prefix}/include\n\nName: libdrm\nDescription: Userspace interface to kernel DRM services\nVersion: 2.4.110\nLibs: -L\$${libdir} -ldrm\nCflags: -I\$${includedir} -I\$${includedir}/libdrm\n" > $(SYSROOT_DIR)/usr/lib/pkgconfig/libdrm.pc
	@cp -f compat/linux/include/drm/*.h $(SYSROOT_DIR)/usr/include/ 2>/dev/null || true
	@cp -f compat/linux/include/drm/*.h $(SYSROOT_DIR)/usr/include/libdrm/ 2>/dev/null || true

# SzpontX11 Native X11 Graphical Display Server
SZPONT_X11_SRCS := $(wildcard userland/xserver/src/*.c)
SZPONT_X11_OBJS := $(patsubst userland/xserver/src/%.c,$(BUILD_DIR)/userland/xserver/%.o,$(SZPONT_X11_SRCS))

$(BUILD_DIR)/userland/xserver/%.o: userland/xserver/src/%.c | $(BUILD_DIR)/userland/xserver
	@mkdir -p $(BUILD_DIR)/userland/xserver
	@echo "  [CC-SZPONTX11] $<"
	@$(CC) $(USER_CFLAGS) -Iuserland/xserver/include -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath $(SYSROOT_DIR))/usr/include -I$(abspath $(SYSROOT_DIR))/usr/include/libdrm -Icompat/linux/include -c $< -o $@

$(BUILD_DIR)/userland/xserver:
	@mkdir -p $@

$(ROOTFS_DIR)/bin/SzpontX11: $(SZPONT_X11_OBJS) $(CRT0_O) $(LIBC_SO) $(LIBM_SO) $(ROOTFS_DIR)/lib/libdrm.so $(ROOTFS_DIR)/lib/libpixman-1.so | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin
	@echo "  [LD-SZPONTX11] $@"
	@$(CC) -O2 -ffreestanding -fno-builtin -nostdlib -B$(abspath $(SYSROOT_DIR))/usr/lib $(CRT0_O) $(SZPONT_X11_OBJS) \
	    -L$(ROOTFS_DIR)/lib -L$(abspath $(SYSROOT_DIR))/usr/lib -ldrm -lpixman-1 -lm -lc -o $@
	@chmod +x $@
	@ln -sf SzpontX11 $(ROOTFS_DIR)/bin/Xorg
	@ln -sf SzpontX11 $(ROOTFS_DIR)/bin/X

$(ROOTFS_DIR)/bin/Xorg: $(ROOTFS_DIR)/bin/SzpontX11


# X11 Desktop Environment (szpontdesktop)
$(ROOTFS_DIR)/bin/szpontdesktop: userland/bin/szpontdesktop.c $(CRT0_O) $(LIBC_SO) $(LIBM_SO) $(ROOTFS_DIR)/lib/libX11.so | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(BUILD_DIR)/userland/bin
	@echo "  [CC-SZPONTDESKTOP] $<"
	@$(CC) $(USER_CFLAGS) -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath $(SYSROOT_DIR))/usr/include -Ithird_party/stb -c $< -o $(BUILD_DIR)/userland/bin/szpontdesktop.o
	@echo "  [LD-SZPONTDESKTOP] $@"
	@$(CC) -O2 -ffreestanding -fno-builtin -nostdlib -B$(abspath $(SYSROOT_DIR))/usr/lib $(CRT0_O) $(BUILD_DIR)/userland/bin/szpontdesktop.o -L$(ROOTFS_DIR)/lib -L$(abspath $(SYSROOT_DIR))/usr/lib -lX11 -lxcb -lXau -lXdmcp -lm -lc -o $@
	@chmod +x $@

# Native X11 Terminal Emulator (szponterm)
$(ROOTFS_DIR)/bin/szponterm: userland/bin/szponterm.c $(CRT0_O) $(LIBC_SO) $(LIBM_SO) $(ROOTFS_DIR)/lib/libX11.so | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(BUILD_DIR)/userland/bin
	@echo "  [CC-SZPONTERM] $<"
	@$(CC) $(USER_CFLAGS) -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath $(SYSROOT_DIR))/usr/include -c $< -o $(BUILD_DIR)/userland/bin/szponterm.o
	@echo "  [LD-SZPONTERM] $@"
	@$(CC) -O2 -ffreestanding -fno-builtin -nostdlib -B$(abspath $(SYSROOT_DIR))/usr/lib $(CRT0_O) $(BUILD_DIR)/userland/bin/szponterm.o -L$(ROOTFS_DIR)/lib -L$(abspath $(SYSROOT_DIR))/usr/lib -lX11 -lxcb -lXau -lXdmcp -lm -lc -o $@
	@chmod +x $@

# libICE Target
$(ROOTFS_DIR)/lib/libICE.so: $(SYSROOT_STAMP) $(LIBC_SO)
	@mkdir -p $(BUILD_DIR)/third_party/libICE $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/X11/ICE
	@echo "  [MAKE-LIBICE] Kompilacja libICE..."
	@cd $(BUILD_DIR)/third_party/libICE && \
	    rm -f *.o && \
	    for f in $(abspath third_party/libICE)/src/*.c; do \
	        $(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DHAVE_ASPRINTF=1 -DICE_t -DTRANS_CLIENT -DTRANS_SERVER -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath third_party/libICE)/include -I$(abspath third_party/libICE)/src -I$(abspath third_party/xtrans) -c "$$f" -o "$$(basename $$f .c).o" || exit 1; \
	    done && \
	    $(LD) -shared -soname libICE.so.6 -o $(abspath $(SYSROOT_DIR))/usr/lib/libICE.so *.o && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libICE.so $(abspath $(ROOTFS_DIR))/lib/libICE.so && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libICE.so $(abspath $(ROOTFS_DIR))/lib/libICE.so.6 && \
	    cp -r $(abspath third_party/libICE)/include/X11/ICE/* $(abspath $(SYSROOT_DIR))/usr/include/X11/ICE/

# libSM Target
$(ROOTFS_DIR)/lib/libSM.so: $(SYSROOT_STAMP) $(ROOTFS_DIR)/lib/libICE.so
	@mkdir -p $(BUILD_DIR)/third_party/libSM $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/X11/SM
	@echo "  [MAKE-LIBSM] Kompilacja libSM..."
	@cd $(BUILD_DIR)/third_party/libSM && \
	    rm -f *.o && \
	    for f in $(abspath third_party/libSM)/src/*.c; do \
	        $(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DHAVE_ASPRINTF=1 -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath third_party/libSM)/include -I$(abspath third_party/libSM)/src -I$(abspath third_party/libICE)/include -c "$$f" -o "$$(basename $$f .c).o" || exit 1; \
	    done && \
	    $(LD) -shared -soname libSM.so.6 -o $(abspath $(SYSROOT_DIR))/usr/lib/libSM.so *.o && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libSM.so $(abspath $(ROOTFS_DIR))/lib/libSM.so && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libSM.so $(abspath $(ROOTFS_DIR))/lib/libSM.so.6 && \
	    cp -r $(abspath third_party/libSM)/include/X11/SM/* $(abspath $(SYSROOT_DIR))/usr/include/X11/SM/

# libXpm Target
$(ROOTFS_DIR)/lib/libXpm.so: $(SYSROOT_STAMP) $(ROOTFS_DIR)/lib/libX11.so
	@mkdir -p $(BUILD_DIR)/third_party/libXpm $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/X11
	@echo "  [MAKE-LIBXPM] Kompilacja libXpm..."
	@cd $(BUILD_DIR)/third_party/libXpm && \
	    rm -f *.o && \
	    for f in $(abspath third_party/libXpm)/src/*.c; do \
	        $(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DHAVE_STRCASECMP=1 -DHAS_STRCASECMP=1 -DNO_ZPIPE -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath third_party/libXpm)/include -I$(abspath third_party/libXpm)/include/X11 -I$(abspath third_party/libXpm)/src -I$(abspath third_party/libX11)/include -c "$$f" -o "$$(basename $$f .c).o" || exit 1; \
	    done && \
	    $(LD) -shared -soname libXpm.so.4 -o $(abspath $(SYSROOT_DIR))/usr/lib/libXpm.so *.o && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXpm.so $(abspath $(ROOTFS_DIR))/lib/libXpm.so && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXpm.so $(abspath $(ROOTFS_DIR))/lib/libXpm.so.4 && \
	    cp -r $(abspath third_party/libXpm)/include/X11/* $(abspath $(SYSROOT_DIR))/usr/include/X11/

# libXext Target
$(ROOTFS_DIR)/lib/libXext.so: $(SYSROOT_STAMP) $(ROOTFS_DIR)/lib/libX11.so
	@mkdir -p $(BUILD_DIR)/third_party/libXext $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/X11/extensions
	@echo "  [MAKE-LIBXEXT] Kompilacja libXext..."
	@cd $(BUILD_DIR)/third_party/libXext && \
	    rm -f *.o && \
	    for f in $(abspath third_party/libXext)/src/*.c; do \
	        [ "$$(basename $$f)" = "reallocarray.c" ] && continue; \
	        $(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DHAVE_REALLOCARRAY=1 -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath third_party/libXext)/include -I$(abspath third_party/libXext)/src -I$(abspath third_party/libX11)/include -c "$$f" -o "$$(basename $$f .c).o" || exit 1; \
	    done && \
	    $(LD) -shared -soname libXext.so.6 -o $(abspath $(SYSROOT_DIR))/usr/lib/libXext.so *.o && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXext.so $(abspath $(ROOTFS_DIR))/lib/libXext.so && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXext.so $(abspath $(ROOTFS_DIR))/lib/libXext.so.6 && \
	    cp -r $(abspath third_party/libXext)/include/X11/* $(abspath $(SYSROOT_DIR))/usr/include/X11/

# libXt Target
$(ROOTFS_DIR)/lib/libXt.so: $(SYSROOT_STAMP) $(ROOTFS_DIR)/lib/libICE.so $(ROOTFS_DIR)/lib/libSM.so $(ROOTFS_DIR)/lib/libX11.so
	@mkdir -p $(BUILD_DIR)/third_party/libXt $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/X11
	@echo "  [MAKE-LIBXT] Kompilacja libXt..."
	@clang third_party/libXt/util/makestrs.c -o $(BUILD_DIR)/third_party/libXt/makestrs
	@$(BUILD_DIR)/third_party/libXt/makestrs -i $(abspath third_party/libXt) < third_party/libXt/util/string.list > third_party/libXt/src/StringDefs.c
	@cp -f StringDefs.h Shell.h third_party/libXt/include/X11/ && cp -f StringDefs.h Shell.h $(SYSROOT_DIR)/usr/include/X11/ && rm -f StringDefs.h Shell.h
	@cd $(BUILD_DIR)/third_party/libXt && \
	    rm -f *.o && \
	    for f in $(abspath third_party/libXt)/src/*.c; do \
	        $(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DHAVE_ASPRINTF=1 -DHAS_GETCWD=1 -DXTHREADS -D_POSIX_THREAD_SAFE_FUNCTIONS -DLIBXT_COMPILATION -DXFILESEARCHPATHDEFAULT='"/usr/lib/X11/%L/%T/%N%S:/usr/lib/X11/%l/%T/%N%S:/usr/lib/X11/%T/%N%S"' -DERRORDB='"/usr/lib/X11/XtErrorDB"' -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath third_party/libXt)/include -I$(abspath third_party/libXt)/include/X11 -I$(abspath third_party/libXt)/src -I$(abspath third_party/libICE)/include -I$(abspath third_party/libSM)/include -c "$$f" -o "$$(basename $$f .c).o" || exit 1; \
	    done && \
	    $(LD) -shared -soname libXt.so.6 -o $(abspath $(SYSROOT_DIR))/usr/lib/libXt.so *.o && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXt.so $(abspath $(ROOTFS_DIR))/lib/libXt.so && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXt.so $(abspath $(ROOTFS_DIR))/lib/libXt.so.6 && \
	    cp -r $(abspath third_party/libXt)/include/X11/* $(abspath $(SYSROOT_DIR))/usr/include/X11/

# libXmu Target
$(ROOTFS_DIR)/lib/libXmu.so: $(SYSROOT_STAMP) $(ROOTFS_DIR)/lib/libXt.so $(ROOTFS_DIR)/lib/libXext.so
	@mkdir -p $(BUILD_DIR)/third_party/libXmu $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/X11/Xmu
	@echo "  [MAKE-LIBXMU] Kompilacja libXmu..."
	@cd $(BUILD_DIR)/third_party/libXmu && \
	    rm -f *.o && \
	    for f in $(abspath third_party/libXmu)/src/*.c; do \
	        $(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DHAVE_ASPRINTF=1 -DHAVE_REALLOCARRAY=1 -DHAS_GETCWD=1 -DXTHREADS -D_POSIX_THREAD_SAFE_FUNCTIONS -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath third_party/libXmu)/include -I$(abspath third_party/libXmu)/include/X11/Xmu -I$(abspath third_party/libXmu)/src -I$(abspath third_party/libXt)/include -I$(abspath third_party/libXt)/include/X11 -c "$$f" -o "$$(basename $$f .c).o" || exit 1; \
	    done && \
	    $(LD) -shared -soname libXmu.so.6 -o $(abspath $(SYSROOT_DIR))/usr/lib/libXmu.so *.o && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXmu.so $(abspath $(ROOTFS_DIR))/lib/libXmu.so && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXmu.so $(abspath $(ROOTFS_DIR))/lib/libXmu.so.6 && \
	    cp -r $(abspath third_party/libXmu)/include/X11/Xmu/* $(abspath $(SYSROOT_DIR))/usr/include/X11/Xmu/

# libXaw Target
$(ROOTFS_DIR)/lib/libXaw.so: $(SYSROOT_STAMP) $(ROOTFS_DIR)/lib/libXmu.so $(ROOTFS_DIR)/lib/libXpm.so
	@mkdir -p $(BUILD_DIR)/third_party/libXaw $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/X11/Xaw
	@echo "  [MAKE-LIBXAW] Kompilacja libXaw..."
	@cd $(BUILD_DIR)/third_party/libXaw && \
	    rm -f *.o && \
	    for f in $(abspath third_party/libXaw)/src/*.c; do \
	        $(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DHAVE_ASPRINTF=1 -DHAVE_REALLOCARRAY=1 -DHAS_GETCWD=1 -DHAVE_WCHAR_H=1 -DHAVE_WCTYPE_H=1 -DXTHREADS -D_POSIX_THREAD_SAFE_FUNCTIONS -include sys/select.h -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath third_party/libXaw)/include -I$(abspath third_party/libXaw)/include/X11/Xaw -I$(abspath third_party/libXaw)/src -I$(abspath third_party/libXpm)/include -I$(abspath third_party/libXmu)/include -I$(abspath third_party/libXt)/include -I$(abspath third_party/libXt)/include/X11 -c "$$f" -o "$$(basename $$f .c).o" || exit 1; \
	    done && \
	    $(LD) -shared -soname libXaw7.so.7 -o $(abspath $(SYSROOT_DIR))/usr/lib/libXaw7.so *.o && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXaw7.so $(abspath $(SYSROOT_DIR))/usr/lib/libXaw.so && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXaw7.so $(abspath $(ROOTFS_DIR))/lib/libXaw.so && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXaw7.so $(abspath $(ROOTFS_DIR))/lib/libXaw7.so.7 && \
	    cp -r $(abspath third_party/libXaw)/include/X11/Xaw/* $(abspath $(SYSROOT_DIR))/usr/include/X11/Xaw/

# Official Upstream X11 Terminal Emulator (xterm)
$(ROOTFS_DIR)/bin/xterm: $(ROOTFS_DIR)/lib/libXaw.so $(ROOTFS_DIR)/lib/libXmu.so $(ROOTFS_DIR)/lib/libXt.so $(ROOTFS_DIR)/lib/libXpm.so $(ROOTFS_DIR)/lib/libXext.so $(ROOTFS_DIR)/lib/libSM.so $(ROOTFS_DIR)/lib/libICE.so $(ROOTFS_DIR)/lib/libX11.so $(LIBNCURSES_A) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin
	@echo "  [MAKE-XTERM] Kompilacja oficjalnego upstream xterm..."
	@cd third_party/xterm && \
	    if [ ! -f Makefile ]; then \
	        CC="$(CC) -nostdlib $(abspath $(SYSROOT_DIR))/usr/lib/crt0.o" \
	        CPP="x86_64-elf-cpp -isystem $(abspath $(SYSROOT_DIR))/usr/include" \
	        CFLAGS="-O2 -ffreestanding -isystem $(abspath $(SYSROOT_DIR))/usr/include -DUSE_SYSV_PGRP=1" \
	        LDFLAGS="-L$(abspath $(ROOTFS_DIR))/lib -L$(abspath $(SYSROOT_DIR))/usr/lib -lXaw7 -lXmu -lXt -lSM -lICE -lXpm -lXext -lX11 -lxcb -lXau -lXdmcp -lncurses -lm -lc" \
	        ./configure --host=x86_64-elf --without-xinerama --disable-imake --disable-setuid --disable-setgid --disable-freetype --without-pcre --without-pcre2 --disable-luit; \
	    fi && \
	    $(MAKE) EXTRA_CFLAGS="-DUSE_SYSV_PGRP=1 -DHAVE_GRANTPT_PTY_ISATTY=1" && \
	    cp -f xterm $(abspath $(ROOTFS_DIR))/bin/xterm && \
	    cp -f resize $(abspath $(ROOTFS_DIR))/bin/resize 2>/dev/null || true
	@chmod +x $@

# X11 Session Launcher (startx)
$(ROOTFS_DIR)/bin/startx: userland/bin/startx.c $(CRT0_O) $(LIBC_SO) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(BUILD_DIR)/userland/bin
	@echo "  [CC-STARTX] $<"
	@$(CC) $(USER_CFLAGS) -c $< -o $(BUILD_DIR)/userland/bin/startx.o
	@echo "  [LD-STARTX] $@"
	@$(CC) -O2 -ffreestanding -fno-builtin -nostdlib -B$(abspath $(SYSROOT_DIR))/usr/lib $(CRT0_O) $(BUILD_DIR)/userland/bin/startx.o -L$(ROOTFS_DIR)/lib -L$(abspath $(SYSROOT_DIR))/usr/lib -lc -o $@
	@chmod +x $@

userland: sysroot $(ROOTFS_DIR) $(LIBNCURSES_A) $(LIBZ_A) $(USER_PROGS) $(MAGIC_DB) $(MODULES)

# Link kernel ELF binary
$(KERNEL_ELF): $(KERNEL_ALL_OBJS) kernel/linker.ld
	@mkdir -p $(BUILD_DIR)
	@echo "  [LD]  $@"
	@$(LD) $(LDFLAGS) $(KERNEL_ALL_OBJS) -o $@

# Download and prepare Limine bootloader
limine-bin/limine-bios.sys:
	@if [ ! -d limine-bin ]; then \
		echo "  [GIT] Pobieranie Limine bootloader v8.x-binary..."; \
		git clone https://github.com/limine-bootloader/limine.git --branch=v8.x-binary --depth=1 limine-bin; \
	fi
	@if [ -d limine-bin ] && [ ! -f limine-bin/limine-bios.sys ]; then \
		$(MAKE) -C limine-bin; \
	fi

limine: limine-bin/limine-bios.sys

# Shared libraries required in rootfs
ALL_ROOTFS_SOS := \
	$(LIBC_SO) $(LIBM_SO) $(LIBCALC_SO) \
	$(ROOTFS_DIR)/lib/libdrm.so \
	$(ROOTFS_DIR)/lib/libpixman-1.so \
	$(ROOTFS_DIR)/lib/libX11.so \
	$(ROOTFS_DIR)/lib/libxcb.so \
	$(ROOTFS_DIR)/lib/libXau.so \
	$(ROOTFS_DIR)/lib/libXdmcp.so \
	$(ROOTFS_DIR)/lib/libxkbfile.so \
	$(ROOTFS_DIR)/lib/libfontenc.so \
	$(ROOTFS_DIR)/lib/libXfont2.so \
	$(ROOTFS_DIR)/lib/libxcvt.so \
	$(ROOTFS_DIR)/lib/libpciaccess.so \
	$(ROOTFS_DIR)/lib/libICE.so \
	$(ROOTFS_DIR)/lib/libSM.so \
	$(ROOTFS_DIR)/lib/libXpm.so \
	$(ROOTFS_DIR)/lib/libXext.so \
	$(ROOTFS_DIR)/lib/libXt.so \
	$(ROOTFS_DIR)/lib/libXmu.so \
	$(ROOTFS_DIR)/lib/libXaw.so

# Build initramfs archive
SKELETON_FILES := $(shell find $(ROOTFS_SKELETON_DIR) -type f 2>/dev/null)
$(BUILD_DIR)/initramfs.tar: $(USER_PROGS) $(MODULES) $(MAGIC_DB) $(ALL_ROOTFS_SOS) $(LIBNCURSES_A) $(LIBZ_A) $(SKELETON_FILES) | $(ROOTFS_DIR)
	@mkdir -p $(BUILD_DIR)
	@if [ -d $(ROOTFS_SKELETON_DIR) ]; then cp -r $(ROOTFS_SKELETON_DIR)/. $(ROOTFS_DIR)/ 2>/dev/null || true; fi
	@mkdir -p $(ROOTFS_DIR)/usr/share/artwork $(ROOTFS_DIR)/usr/share
	@if [ -d artwork ]; then cp -r artwork/* $(ROOTFS_DIR)/usr/share/artwork/ 2>/dev/null || true; fi
	@if [ -d artwork ]; then cp -r artwork/* $(ROOTFS_DIR)/usr/share/ 2>/dev/null || true; fi
	@if [ -f artwork/szpont-detected.jpg ]; then cp artwork/szpont-detected.jpg $(ROOTFS_DIR)/usr/share/artwork/szpont-detected.png 2>/dev/null || true; fi
	@if [ -f artwork/szpont-detected.jpg ]; then cp artwork/szpont-detected.jpg $(ROOTFS_DIR)/usr/share/szpont-detected.png 2>/dev/null || true; fi
	@./scripts/make_initramfs.py $(ROOTFS_DIR) $(BUILD_DIR)/initramfs.tar

initramfs: $(BUILD_DIR)/initramfs.tar

# Generate ext2 Disk Image
$(DISK_IMAGE): scripts/make_ext2_disk.py
	@mkdir -p $(BUILD_DIR)
	@echo "  [EXT2] Generowanie obrazu dysku ext2 $(DISK_IMAGE)..."
	@python3 scripts/make_ext2_disk.py $(DISK_IMAGE)

disk: $(DISK_IMAGE)

# Build bootable ISO
$(ISO_IMAGE): $(KERNEL_ELF) $(BUILD_DIR)/initramfs.tar limine-bin/limine-bios.sys $(DISK_IMAGE)
	@echo "  [ISO] Tworzenie obrazu rozruchowego $(ISO_IMAGE)..."
	@rm -rf $(ISO_DIR)
	@mkdir -p $(ISO_DIR)/boot $(ISO_DIR)/boot/limine $(ISO_DIR)/EFI/BOOT
	@cp $(KERNEL_ELF) $(ISO_DIR)/boot/$(OS_NAME)-kernel
	@cp $(BUILD_DIR)/initramfs.tar $(ISO_DIR)/boot/initramfs.tar
	@cp limine.conf $(ISO_DIR)/boot/limine/limine.conf
	@cp limine-bin/limine-bios.sys $(ISO_DIR)/boot/limine/
	@cp limine-bin/limine-bios-cd.bin $(ISO_DIR)/boot/limine/
	@cp limine-bin/limine-uefi-cd.bin $(ISO_DIR)/boot/limine/
	@cp limine-bin/BOOTX64.EFI $(ISO_DIR)/EFI/BOOT/
	@cp limine-bin/BOOTIA32.EFI $(ISO_DIR)/EFI/BOOT/ 2>/dev/null || true
	@$(XORRISO) -as mkisofs -b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		$(ISO_DIR) -o $(ISO_IMAGE) >/dev/null 2>&1
	@./limine-bin/limine bios-install $(ISO_IMAGE) >/dev/null 2>&1 || true
	@echo "  [OK]  Obraz ISO gotowy: $(ISO_IMAGE)"

iso: $(ISO_IMAGE)

# Run in graphical QEMU
run: $(ISO_IMAGE)
	@./scripts/run_qemu.sh $(ISO_IMAGE)

# Run in graphical QEMU with Virtio-VGA
run-virtio: $(ISO_IMAGE)
	@./scripts/run_qemu.sh $(ISO_IMAGE) --virtio

# Run in graphical QEMU with Bare Metal PS/2 simulation
run-ps2: $(ISO_IMAGE)
	@./scripts/run_qemu.sh $(ISO_IMAGE) --baremetal-ps2

# Run in graphical QEMU with Pure USB xHCI (UEFI Modern Bare Metal)
run-usb: $(ISO_IMAGE)
	@./scripts/run_qemu.sh $(ISO_IMAGE) --baremetal-usb

# Run with realistic timing and instruction cycle stress test
run-stress: $(ISO_IMAGE)
	@./scripts/run_qemu.sh $(ISO_IMAGE) --timing-stress

# Run in headless QEMU (terminal only)
run-cli: $(ISO_IMAGE)
	@./scripts/run_qemu.sh $(ISO_IMAGE) --headless

# Run in graphical QEMU with GDB debug stub
debug: $(ISO_IMAGE)
	@./scripts/run_qemu.sh $(ISO_IMAGE) --debug

compile_commands.json compile-commands bear:
	@echo "  [BEAR] Generowanie compile_commands.json za pomocą bear..."
	@if command -v bear >/dev/null 2>&1; then \
		rm -f compile_commands.json; \
		bear -- $(MAKE) clean all >/dev/null 2>&1 || true; \
		if [ -f compile_commands.json ]; then \
			python3 -c "import json; db = json.load(open('compile_commands.json')); json.dump([e for e in db if not e.get('file','').endswith('.asm') and (not e.get('arguments') or e['arguments'][0] != 'nasm')], open('compile_commands.json', 'w'), indent=2)" 2>/dev/null || true; \
		fi; \
		echo "  [OK]   Wygenerowano compile_commands.json dla clangd / IDE."; \
	else \
		echo "  [!]    Błąd: Brak narzędzia bear. Zainstaluj 'brew install bear'."; \
		exit 1; \
	fi

clean:
	@echo "  [CLEAN] Czyszczenie katalogu build..."
	@rm -rf $(BUILD_DIR)

distclean: clean
	@echo "  [CLEAN] Usuwanie pobranych binariów Limine i bazy kompilacji..."
	@rm -rf limine-bin compile_commands.json
