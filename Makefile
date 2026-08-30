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
    libc/src/stdio/printf.c \
    libc/src/unistd/unistd.c \
    libc/src/unistd/sysconf.c \
    libc/src/unistd/sendfile.c \
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
$(SYSROOT_STAMP): $(LIBC_A) $(CRT0_O) $(LIBM_A) $(LIBDL_A)
	@mkdir -p $(SYSROOT_DIR)/usr/include $(SYSROOT_DIR)/usr/lib
	@cp -r libc/include/* $(SYSROOT_DIR)/usr/include/
	@cp $(LIBC_A) $(CRT0_O) $(LIBM_A) $(LIBDL_A) $(SYSROOT_DIR)/usr/lib/ 2>/dev/null || true
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

# Build initramfs archive
SKELETON_FILES := $(shell find $(ROOTFS_SKELETON_DIR) -type f 2>/dev/null)
$(BUILD_DIR)/initramfs.tar: $(USER_PROGS) $(MODULES) $(MAGIC_DB) $(LIBC_SO) $(LIBM_SO) $(LIBCALC_SO) $(LIBNCURSES_A) $(LIBZ_A) $(SKELETON_FILES) | $(ROOTFS_DIR)
	@mkdir -p $(BUILD_DIR)
	@if [ -d $(ROOTFS_SKELETON_DIR) ]; then cp -r $(ROOTFS_SKELETON_DIR)/. $(ROOTFS_DIR)/ 2>/dev/null || true; fi
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
