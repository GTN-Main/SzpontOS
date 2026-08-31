# SzpontOS Userland Core Utilities & Applications Makefile
ROOT_DIR ?= $(abspath ../..)
include $(ROOT_DIR)/config.mk

CORE_PROGS := \
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
    $(ROOTFS_DIR)/bin/gittest \
    $(ROOTFS_DIR)/bin/startx \
    $(ROOTFS_DIR)/bin/szpontdesktop \
    $(ROOTFS_DIR)/bin/szponterm

.PHONY: all progs

all: progs

progs: $(CORE_PROGS)

# Standard static binaries linked against libc.a
$(ROOTFS_DIR)/bin/%: $(ROOT_DIR)/userland/bin/%.c $(CRT0_O) $(LIBC_A) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(BUILD_DIR)/userland/bin
	@echo "  [CC]  $<"
	@$(CC) $(USER_CFLAGS) -c $< -o $(BUILD_DIR)/userland/bin/$*.o
	@echo "  [LD]  $@"
	@$(LD) $(USER_LDFLAGS) $(CRT0_O) $(BUILD_DIR)/userland/bin/$*.o $(LIBC_A) -o $@

# Build donut (dynamically linked against libc.so and libm.so)
$(ROOTFS_DIR)/bin/donut: $(ROOT_DIR)/userland/bin/donut.c $(CRT0_O) $(LIBC_SO) $(LIBM_SO) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(BUILD_DIR)/userland/bin
	@echo "  [CC-DYN] $<"
	@$(CC) $(USER_CFLAGS) -c $< -o $(BUILD_DIR)/userland/bin/donut.o
	@echo "  [LD-DYN] $@"
	@$(LD) $(USER_LDFLAGS) -pie $(CRT0_O) $(BUILD_DIR)/userland/bin/donut.o $(LIBM_SO) $(LIBC_SO) -o $@

# X11 Desktop Environment (szpontdesktop)
$(ROOTFS_DIR)/bin/szpontdesktop: $(ROOT_DIR)/userland/bin/szpontdesktop.c $(CRT0_O) $(LIBC_SO) $(LIBM_SO) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(BUILD_DIR)/userland/bin
	@echo "  [CC-SZPONTDESKTOP] $<"
	@$(CC) $(USER_CFLAGS) -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath $(SYSROOT_DIR))/usr/include -I$(ROOT_DIR)/third_party/stb -c $< -o $(BUILD_DIR)/userland/bin/szpontdesktop.o
	@echo "  [LD-SZPONTDESKTOP] $@"
	@$(CC) -O2 -ffreestanding -fno-builtin -nostdlib -B$(abspath $(SYSROOT_DIR))/usr/lib $(CRT0_O) $(BUILD_DIR)/userland/bin/szpontdesktop.o -L$(ROOTFS_DIR)/lib -L$(abspath $(SYSROOT_DIR))/usr/lib -lX11 -lxcb -lXau -lXdmcp -lm -lc -o $@
	@chmod +x $@

# Native X11 Terminal Emulator (szponterm)
$(ROOTFS_DIR)/bin/szponterm: $(ROOT_DIR)/userland/bin/szponterm.c $(CRT0_O) $(LIBC_SO) $(LIBM_SO) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(BUILD_DIR)/userland/bin
	@echo "  [CC-SZPONTERM] $<"
	@$(CC) $(USER_CFLAGS) -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath $(SYSROOT_DIR))/usr/include -c $< -o $(BUILD_DIR)/userland/bin/szponterm.o
	@echo "  [LD-SZPONTERM] $@"
	@$(CC) -O2 -ffreestanding -fno-builtin -nostdlib -B$(abspath $(SYSROOT_DIR))/usr/lib $(CRT0_O) $(BUILD_DIR)/userland/bin/szponterm.o -L$(ROOTFS_DIR)/lib -L$(abspath $(SYSROOT_DIR))/usr/lib -lX11 -lxcb -lXau -lXdmcp -lm -lc -o $@
	@chmod +x $@

# X11 Session Launcher (startx)
$(ROOTFS_DIR)/bin/startx: $(ROOT_DIR)/userland/bin/startx.c $(CRT0_O) $(LIBC_SO) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(BUILD_DIR)/userland/bin
	@echo "  [CC-STARTX] $<"
	@$(CC) $(USER_CFLAGS) -c $< -o $(BUILD_DIR)/userland/bin/startx.o
	@echo "  [LD-STARTX] $@"
	@$(CC) -O2 -ffreestanding -fno-builtin -nostdlib -B$(abspath $(SYSROOT_DIR))/usr/lib $(CRT0_O) $(BUILD_DIR)/userland/bin/startx.o -L$(ROOTFS_DIR)/lib -L$(abspath $(SYSROOT_DIR))/usr/lib -lc -o $@
	@chmod +x $@
