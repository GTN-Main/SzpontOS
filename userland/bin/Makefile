# SzpontOS Userland Core Utilities & Applications Makefile
ROOT_DIR ?= $(abspath ../..)
include $(ROOT_DIR)/config.mk

CORE_PROGS := \
    $(ROOTFS_DIR)/bin/hello \
    $(ROOTFS_DIR)/bin/cat \
    $(ROOTFS_DIR)/bin/env \
    $(ROOTFS_DIR)/bin/id \
    $(ROOTFS_DIR)/bin/whoami \
    $(ROOTFS_DIR)/bin/useradd \
    $(ROOTFS_DIR)/bin/userdel \
    $(ROOTFS_DIR)/bin/groupadd \
    $(ROOTFS_DIR)/bin/su \
    $(ROOTFS_DIR)/bin/sudo \
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
    $(ROOTFS_DIR)/bin/insmod \
    $(ROOTFS_DIR)/bin/rmmod \
    $(ROOTFS_DIR)/bin/lsmod \
    $(ROOTFS_DIR)/bin/modinfo \
    $(ROOTFS_DIR)/bin/rm \
    $(ROOTFS_DIR)/bin/hostname \
    $(ROOTFS_DIR)/bin/uname \
    $(ROOTFS_DIR)/bin/ifconfig \
    $(ROOTFS_DIR)/bin/ping \
    $(ROOTFS_DIR)/bin/sleep \
    $(ROOTFS_DIR)/bin/nc \
    $(ROOTFS_DIR)/bin/httpd \
    $(ROOTFS_DIR)/bin/sysctl \
    $(ROOTFS_DIR)/bin/kill \
    $(ROOTFS_DIR)/bin/killall \
    $(ROOTFS_DIR)/bin/dmesg \
    $(ROOTFS_DIR)/bin/grep \
    $(ROOTFS_DIR)/bin/find \
    $(ROOTFS_DIR)/bin/top \
    $(ROOTFS_DIR)/bin/reboot \
    $(ROOTFS_DIR)/bin/host \
    $(ROOTFS_DIR)/bin/httpget \
    $(ROOTFS_DIR)/bin/shutdown \
    $(ROOTFS_DIR)/bin/poweroff \
    $(ROOTFS_DIR)/bin/sync \
    $(ROOTFS_DIR)/bin/ls \
    $(ROOTFS_DIR)/bin/lspci \
    $(ROOTFS_DIR)/bin/lsusb \
    $(ROOTFS_DIR)/bin/donut \
    $(ROOTFS_DIR)/bin/startx \
    $(ROOTFS_DIR)/bin/szpontlogin \
    $(ROOTFS_DIR)/bin/szpontdesktop \
    $(ROOTFS_DIR)/bin/szponterm \
    $(ROOTFS_DIR)/bin/makaljer \
    $(ROOTFS_DIR)/bin/szpontdetected

TEST_PROGS := \
    $(ROOTFS_DIR)/usr/tbin/dltest \
    $(ROOTFS_DIR)/usr/tbin/mathtest \
    $(ROOTFS_DIR)/usr/tbin/threadtest \
    $(ROOTFS_DIR)/usr/tbin/tuitest \
    $(ROOTFS_DIR)/usr/tbin/drmtest \
    $(ROOTFS_DIR)/usr/tbin/randtest \
    $(ROOTFS_DIR)/usr/tbin/tmpfstest \
    $(ROOTFS_DIR)/usr/tbin/shmtest \
    $(ROOTFS_DIR)/usr/tbin/ptytest \
    $(ROOTFS_DIR)/usr/tbin/kqueuetest \
    $(ROOTFS_DIR)/usr/tbin/mousetest \
    $(ROOTFS_DIR)/usr/tbin/unixtest \
    $(ROOTFS_DIR)/usr/tbin/gittest \
    $(ROOTFS_DIR)/usr/tbin/curltest \
    $(ROOTFS_DIR)/usr/tbin/mesadrmtest \
    $(ROOTFS_DIR)/usr/tbin/cpptest \
    $(ROOTFS_DIR)/usr/tbin/gltriangle \
    $(ROOTFS_DIR)/usr/tbin/glxgears \
    $(ROOTFS_DIR)/usr/tbin/epolltest \
    $(ROOTFS_DIR)/usr/tbin/eventfdtest \
    $(ROOTFS_DIR)/usr/tbin/inotifytest \
    $(ROOTFS_DIR)/usr/tbin/timerfdtest \
    $(ROOTFS_DIR)/usr/tbin/signalfdtest \
    $(ROOTFS_DIR)/usr/tbin/sysfstest

.PHONY: all progs symlinks

all: progs symlinks

progs: $(CORE_PROGS) $(TEST_PROGS)

symlinks: $(CORE_PROGS)
	@mkdir -p $(ROOTFS_DIR)/usr/bin
	@ln -sf /bin/killall $(ROOTFS_DIR)/usr/bin/killall
	@ln -sf /bin/kill $(ROOTFS_DIR)/usr/bin/kill
	@ln -sf /bin/sudo $(ROOTFS_DIR)/usr/bin/sudo
	@ln -sf /bin/su $(ROOTFS_DIR)/usr/bin/su
	@chmod 4755 $(ROOTFS_DIR)/bin/sudo $(ROOTFS_DIR)/bin/su 2>/dev/null || true

# Compile standard utility objects
$(BUILD_DIR)/userland/bin/%.o: $(ROOT_DIR)/userland/bin/%.c | $(SYSROOT_STAMP) $(BUILD_DIR)/userland/bin
	@mkdir -p $(BUILD_DIR)/userland/bin
	@echo "  [CC]  $<"
	@$(CC) $(USER_CFLAGS) -MMD -MP -MF $(BUILD_DIR)/userland/bin/$*.d -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath $(SYSROOT_DIR))/usr/include -c $< -o $@

$(ROOTFS_DIR)/bin $(ROOTFS_DIR)/usr/tbin:
	@mkdir -p $@

# Link standard dynamic binaries
$(ROOTFS_DIR)/bin/%: $(BUILD_DIR)/userland/bin/%.o $(CRT0_O) | $(SYSROOT_STAMP) $(ROOTFS_DIR)/bin $(LIBC_SO) $(LIBM_SO)
	@mkdir -p $(ROOTFS_DIR)/bin
	@echo "  [LD]  $@"
	@$(CC) -O2 -ffreestanding -fno-builtin -nostdlib -B$(abspath $(SYSROOT_DIR))/usr/lib $(CRT0_O) $< -L$(ROOTFS_DIR)/lib -L$(abspath $(SYSROOT_DIR))/usr/lib -lm -lc -o $@
	@chmod +x $@

# Link standard test binaries
$(ROOTFS_DIR)/usr/tbin/%: $(BUILD_DIR)/userland/bin/%.o $(CRT0_O) | $(SYSROOT_STAMP) $(ROOTFS_DIR)/usr/tbin $(LIBC_SO) $(LIBM_SO)
	@mkdir -p $(ROOTFS_DIR)/usr/tbin
	@echo "  [LD]  $@"
	@$(CC) -O2 -ffreestanding -fno-builtin -nostdlib -B$(abspath $(SYSROOT_DIR))/usr/lib $(CRT0_O) $< -L$(ROOTFS_DIR)/lib -L$(abspath $(SYSROOT_DIR))/usr/lib -lm -lc -o $@
	@chmod +x $@

# Build donut
$(BUILD_DIR)/userland/bin/donut.o: $(ROOT_DIR)/userland/bin/donut.c | $(SYSROOT_STAMP) $(BUILD_DIR)/userland/bin
	@mkdir -p $(BUILD_DIR)/userland/bin
	@echo "  [CC-DYN] $<"
	@$(CC) $(USER_CFLAGS) -MMD -MP -MF $(BUILD_DIR)/userland/bin/donut.d -c $< -o $@

$(ROOTFS_DIR)/bin/donut: $(BUILD_DIR)/userland/bin/donut.o $(CRT0_O) | $(SYSROOT_STAMP) $(ROOTFS_DIR)/bin $(LIBC_SO) $(LIBM_SO)
	@mkdir -p $(ROOTFS_DIR)/bin
	@echo "  [LD-DYN] $@"
	@$(LD) $(USER_LDFLAGS) -pie $(CRT0_O) $< $(LIBM_SO) $(LIBC_SO) -o $@

# X11 Display & Login Manager (szpontlogin)
$(BUILD_DIR)/userland/bin/szpontlogin.o: $(ROOT_DIR)/userland/bin/szpontlogin.c | $(SYSROOT_STAMP) $(BUILD_DIR)/userland/bin
	@mkdir -p $(BUILD_DIR)/userland/bin
	@echo "  [CC-SZPONTLOGIN] $<"
	@$(CC) $(USER_CFLAGS) -MMD -MP -MF $(BUILD_DIR)/userland/bin/szpontlogin.d -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath $(SYSROOT_DIR))/usr/include -c $< -o $@

$(ROOTFS_DIR)/bin/szpontlogin: $(BUILD_DIR)/userland/bin/szpontlogin.o $(CRT0_O) | $(SYSROOT_STAMP) $(ROOTFS_DIR)/bin $(LIBC_SO) $(LIBM_SO) $(ROOTFS_DIR)/lib/libX11.so $(ROOTFS_DIR)/lib/libXext.so
	@mkdir -p $(ROOTFS_DIR)/bin
	@echo "  [LD-SZPONTLOGIN] $@"
	@$(CC) -O2 -ffreestanding -fno-builtin -nostdlib -B$(abspath $(SYSROOT_DIR))/usr/lib $(CRT0_O) $< -L$(ROOTFS_DIR)/lib -L$(abspath $(SYSROOT_DIR))/usr/lib -lXext -lX11 -lxcb -lXau -lXdmcp -lm -lc -o $@
	@chmod +x $@

# X11 Desktop Environment (szpontdesktop)
$(BUILD_DIR)/userland/bin/szpontdesktop.o: $(ROOT_DIR)/userland/bin/szpontdesktop.c | $(SYSROOT_STAMP) $(BUILD_DIR)/userland/bin
	@mkdir -p $(BUILD_DIR)/userland/bin
	@echo "  [CC-SZPONTDESKTOP] $<"
	@$(CC) $(USER_CFLAGS) -MMD -MP -MF $(BUILD_DIR)/userland/bin/szpontdesktop.d -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath $(SYSROOT_DIR))/usr/include -I$(ROOT_DIR)/third_party/stb -c $< -o $@

$(ROOTFS_DIR)/bin/szpontdesktop: $(BUILD_DIR)/userland/bin/szpontdesktop.o $(CRT0_O) | $(SYSROOT_STAMP) $(ROOTFS_DIR)/bin $(LIBC_SO) $(LIBM_SO) $(ROOTFS_DIR)/lib/libX11.so $(ROOTFS_DIR)/lib/libXext.so
	@mkdir -p $(ROOTFS_DIR)/bin
	@echo "  [LD-SZPONTDESKTOP] $@"
	@$(CC) -O2 -ffreestanding -fno-builtin -nostdlib -B$(abspath $(SYSROOT_DIR))/usr/lib $(CRT0_O) $< -L$(ROOTFS_DIR)/lib -L$(abspath $(SYSROOT_DIR))/usr/lib -lXext -lX11 -lxcb -lXau -lXdmcp -lm -lc -o $@
	@chmod +x $@

# Native X11 Terminal Emulator (szponterm)
$(BUILD_DIR)/userland/bin/szponterm.o: $(ROOT_DIR)/userland/bin/szponterm.c | $(SYSROOT_STAMP) $(BUILD_DIR)/userland/bin
	@mkdir -p $(BUILD_DIR)/userland/bin
	@echo "  [CC-SZPONTERM] $<"
	@$(CC) $(USER_CFLAGS) -MMD -MP -MF $(BUILD_DIR)/userland/bin/szponterm.d -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath $(SYSROOT_DIR))/usr/include -c $< -o $@

$(ROOTFS_DIR)/bin/szponterm: $(BUILD_DIR)/userland/bin/szponterm.o $(CRT0_O) | $(SYSROOT_STAMP) $(ROOTFS_DIR)/bin $(LIBC_SO) $(LIBM_SO) $(ROOTFS_DIR)/lib/libX11.so
	@mkdir -p $(ROOTFS_DIR)/bin
	@echo "  [LD-SZPONTERM] $@"
	@$(CC) -O2 -ffreestanding -fno-builtin -nostdlib -B$(abspath $(SYSROOT_DIR))/usr/lib $(CRT0_O) $< -L$(ROOTFS_DIR)/lib -L$(abspath $(SYSROOT_DIR))/usr/lib -lX11 -lxcb -lXau -lXdmcp -lm -lc -o $@
	@chmod +x $@

# Native X11 Artwork Viewer (makaljer)
$(BUILD_DIR)/userland/bin/makaljer.o: $(ROOT_DIR)/userland/bin/makaljer.c | $(SYSROOT_STAMP) $(BUILD_DIR)/userland/bin
	@mkdir -p $(BUILD_DIR)/userland/bin
	@echo "  [CC-MAKALJER] $<"
	@$(CC) $(USER_CFLAGS) -MMD -MP -MF $(BUILD_DIR)/userland/bin/makaljer.d -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath $(SYSROOT_DIR))/usr/include -I$(ROOT_DIR)/third_party/stb -c $< -o $@

$(ROOTFS_DIR)/bin/makaljer: $(BUILD_DIR)/userland/bin/makaljer.o $(CRT0_O) | $(SYSROOT_STAMP) $(ROOTFS_DIR)/bin $(LIBC_SO) $(LIBM_SO) $(ROOTFS_DIR)/lib/libX11.so $(ROOTFS_DIR)/lib/libXext.so
	@mkdir -p $(ROOTFS_DIR)/bin
	@echo "  [LD-MAKALJER] $@"
	@$(CC) -O2 -ffreestanding -fno-builtin -nostdlib -B$(abspath $(SYSROOT_DIR))/usr/lib $(CRT0_O) $< -L$(ROOTFS_DIR)/lib -L$(abspath $(SYSROOT_DIR))/usr/lib -lXext -lX11 -lxcb -lXau -lXdmcp -lm -lc -o $@
	@chmod +x $@

# Native X11 Artwork Viewer (szpontdetected)
$(BUILD_DIR)/userland/bin/szpontdetected.o: $(ROOT_DIR)/userland/bin/szpontdetected.c | $(SYSROOT_STAMP) $(BUILD_DIR)/userland/bin
	@mkdir -p $(BUILD_DIR)/userland/bin
	@echo "  [CC-SZPONTDETECTED] $<"
	@$(CC) $(USER_CFLAGS) -MMD -MP -MF $(BUILD_DIR)/userland/bin/szpontdetected.d -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath $(SYSROOT_DIR))/usr/include -I$(ROOT_DIR)/third_party/stb -c $< -o $@

$(ROOTFS_DIR)/bin/szpontdetected: $(BUILD_DIR)/userland/bin/szpontdetected.o $(CRT0_O) | $(SYSROOT_STAMP) $(ROOTFS_DIR)/bin $(LIBC_SO) $(LIBM_SO) $(ROOTFS_DIR)/lib/libX11.so $(ROOTFS_DIR)/lib/libXext.so
	@mkdir -p $(ROOTFS_DIR)/bin
	@echo "  [LD-SZPONTDETECTED] $@"
	@$(CC) -O2 -ffreestanding -fno-builtin -nostdlib -B$(abspath $(SYSROOT_DIR))/usr/lib $(CRT0_O) $< -L$(ROOTFS_DIR)/lib -L$(abspath $(SYSROOT_DIR))/usr/lib -lXext -lX11 -lxcb -lXau -lXdmcp -lm -lc -o $@
	@chmod +x $@

# X11 Session Launcher (startx)
$(BUILD_DIR)/userland/bin/startx.o: $(ROOT_DIR)/userland/bin/startx.c | $(SYSROOT_STAMP) $(BUILD_DIR)/userland/bin
	@mkdir -p $(BUILD_DIR)/userland/bin
	@echo "  [CC-STARTX] $<"
	@$(CC) $(USER_CFLAGS) -MMD -MP -MF $(BUILD_DIR)/userland/bin/startx.d -c $< -o $@

$(ROOTFS_DIR)/bin/startx: $(BUILD_DIR)/userland/bin/startx.o $(CRT0_O) | $(SYSROOT_STAMP) $(ROOTFS_DIR)/bin $(LIBC_SO)
	@mkdir -p $(ROOTFS_DIR)/bin
	@echo "  [LD-STARTX] $@"
	@$(CC) -O2 -ffreestanding -fno-builtin -nostdlib -B$(abspath $(SYSROOT_DIR))/usr/lib $(CRT0_O) $< -L$(ROOTFS_DIR)/lib -L$(abspath $(SYSROOT_DIR))/usr/lib -lc -o $@
	@chmod +x $@

# cURL Diagnostic Test Tool
$(BUILD_DIR)/userland/bin/curltest.o: $(ROOT_DIR)/userland/bin/curltest.c | $(SYSROOT_STAMP) $(BUILD_DIR)/userland/bin
	@mkdir -p $(BUILD_DIR)/userland/bin
	@echo "  [CC-CURLTEST] $<"
	@$(CC) $(USER_CFLAGS) -MMD -MP -MF $(BUILD_DIR)/userland/bin/curltest.d -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath $(SYSROOT_DIR))/usr/include -c $< -o $@

$(ROOTFS_DIR)/usr/tbin/curltest: $(BUILD_DIR)/userland/bin/curltest.o $(CRT0_O) | $(SYSROOT_STAMP) $(ROOTFS_DIR)/usr/tbin $(LIBC_SO) $(ROOTFS_DIR)/lib/libcurl.so $(ROOTFS_DIR)/lib/libz.so
	@mkdir -p $(ROOTFS_DIR)/usr/tbin
	@echo "  [LD-CURLTEST] $@"
	@$(CC) -O2 -ffreestanding -fno-builtin -nostdlib -B$(abspath $(SYSROOT_DIR))/usr/lib $(CRT0_O) $< -L$(ROOTFS_DIR)/lib -Wl,-rpath-link=$(ROOTFS_DIR)/lib -lcurl -lssl -lcrypto -lz -lm -lc -o $@
	@chmod +x $@

# Native DRM test utilities linked against libdrm.so
$(BUILD_DIR)/userland/bin/drmtest.o: $(ROOT_DIR)/userland/bin/drmtest.c | $(SYSROOT_STAMP) $(BUILD_DIR)/userland/bin
	@mkdir -p $(BUILD_DIR)/userland/bin
	@echo "  [CC]  $<"
	@$(CC) $(USER_CFLAGS) -MMD -MP -MF $(BUILD_DIR)/userland/bin/drmtest.d -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath $(SYSROOT_DIR))/usr/include -I$(abspath $(SYSROOT_DIR))/usr/include/libdrm -c $< -o $@

$(ROOTFS_DIR)/usr/tbin/drmtest: $(BUILD_DIR)/userland/bin/drmtest.o $(CRT0_O) | $(SYSROOT_STAMP) $(ROOTFS_DIR)/usr/tbin $(LIBC_SO) $(LIBM_SO) $(ROOTFS_DIR)/lib/libdrm.so
	@mkdir -p $(ROOTFS_DIR)/usr/tbin
	@echo "  [LD]  $@"
	@$(CC) -O2 -ffreestanding -fno-builtin -nostdlib -B$(abspath $(SYSROOT_DIR))/usr/lib $(CRT0_O) $< -L$(ROOTFS_DIR)/lib -L$(abspath $(SYSROOT_DIR))/usr/lib -ldrm -lm -lc -o $@
	@chmod +x $@

$(BUILD_DIR)/userland/bin/mesadrmtest.o: $(ROOT_DIR)/userland/bin/mesadrmtest.c | $(SYSROOT_STAMP) $(BUILD_DIR)/userland/bin
	@mkdir -p $(BUILD_DIR)/userland/bin
	@echo "  [CC]  $<"
	@$(CC) $(USER_CFLAGS) -MMD -MP -MF $(BUILD_DIR)/userland/bin/mesadrmtest.d -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath $(SYSROOT_DIR))/usr/include -I$(abspath $(SYSROOT_DIR))/usr/include/libdrm -c $< -o $@

$(ROOTFS_DIR)/usr/tbin/mesadrmtest: $(BUILD_DIR)/userland/bin/mesadrmtest.o $(CRT0_O) | $(SYSROOT_STAMP) $(ROOTFS_DIR)/usr/tbin $(LIBC_SO) $(LIBM_SO) $(ROOTFS_DIR)/lib/libdrm.so
	@mkdir -p $(ROOTFS_DIR)/usr/tbin
	@echo "  [LD]  $@"
	@$(CC) -O2 -ffreestanding -fno-builtin -nostdlib -B$(abspath $(SYSROOT_DIR))/usr/lib $(CRT0_O) $< -L$(ROOTFS_DIR)/lib -L$(abspath $(SYSROOT_DIR))/usr/lib -ldrm -lm -lc -o $@
	@chmod +x $@

# Native C++ Validation Suite linked against libstdc++.so, libm.so, libc.so
$(BUILD_DIR)/userland/bin/cpptest.o: $(ROOT_DIR)/userland/bin/cpptest.cpp | $(SYSROOT_STAMP) $(BUILD_DIR)/userland/bin
	@mkdir -p $(BUILD_DIR)/userland/bin
	@echo "  [CXX] $<"
	@$(CXX) $(USER_CXXFLAGS) -MMD -MP -MF $(BUILD_DIR)/userland/bin/cpptest.d -c $< -o $@

$(ROOTFS_DIR)/usr/tbin/cpptest: $(BUILD_DIR)/userland/bin/cpptest.o $(CRT0_O) $(CRTI_O) $(CRTN_O) | $(SYSROOT_STAMP) $(ROOTFS_DIR)/usr/tbin $(LIBC_SO) $(LIBM_SO) $(LIBSTDCXX_SO)
	@mkdir -p $(ROOTFS_DIR)/usr/tbin
	@echo "  [LD]  $@"
	@$(CC) -O2 -ffreestanding -fno-builtin -nostdlib -B$(abspath $(SYSROOT_DIR))/usr/lib $(CRT0_O) $(CRTI_O) $$($(CC) -print-file-name=crtbegin.o) $< $$($(CC) -print-file-name=crtend.o) $(CRTN_O) -L$(ROOTFS_DIR)/lib -L$(abspath $(SYSROOT_DIR))/usr/lib -lstdc++ -lm -lc -o $@
	@chmod +x $@

# Native OpenGL / EGL smoke test linked against libEGL.so, libGLESv2.so, libm.so, libc.so
$(BUILD_DIR)/userland/bin/gltriangle.o: $(ROOT_DIR)/userland/bin/gltriangle.c | $(SYSROOT_STAMP) $(BUILD_DIR)/userland/bin
	@mkdir -p $(BUILD_DIR)/userland/bin
	@echo "  [CC]  $<"
	@$(CC) $(USER_CFLAGS) -MMD -MP -MF $(BUILD_DIR)/userland/bin/gltriangle.d -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath $(SYSROOT_DIR))/usr/include -c $< -o $@

$(ROOTFS_DIR)/usr/tbin/gltriangle: $(BUILD_DIR)/userland/bin/gltriangle.o $(CRT0_O) | $(SYSROOT_STAMP) $(ROOTFS_DIR)/usr/tbin $(LIBC_SO) $(LIBM_SO)
	@mkdir -p $(ROOTFS_DIR)/usr/tbin
	@echo "  [LD]  $@"
	@$(CC) -O2 -ffreestanding -fno-builtin -nostdlib -B$(abspath $(SYSROOT_DIR))/usr/lib $(CRT0_O) $< -L$(ROOTFS_DIR)/lib -L$(abspath $(SYSROOT_DIR))/usr/lib -lEGL -lGLESv2 -lm -lc -Wl,--allow-shlib-undefined -o $@
	@chmod +x $@

# Native GLX Gears demo linked against libGL.so, libX11.so, libm.so, libc.so
$(BUILD_DIR)/userland/bin/glxgears.o: $(ROOT_DIR)/userland/bin/glxgears.c | $(SYSROOT_STAMP) $(BUILD_DIR)/userland/bin
	@mkdir -p $(BUILD_DIR)/userland/bin
	@echo "  [CC]  $<"
	@$(CC) $(USER_CFLAGS) -MMD -MP -MF $(BUILD_DIR)/userland/bin/glxgears.d -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath $(SYSROOT_DIR))/usr/include -c $< -o $@

$(ROOTFS_DIR)/usr/tbin/glxgears: $(BUILD_DIR)/userland/bin/glxgears.o $(CRT0_O) | $(SYSROOT_STAMP) $(ROOTFS_DIR)/usr/tbin $(LIBC_SO) $(LIBM_SO) $(ROOTFS_DIR)/lib/libGL.so $(ROOTFS_DIR)/lib/libX11.so
	@mkdir -p $(ROOTFS_DIR)/usr/tbin
	@echo "  [LD]  $@"
	@$(CC) -O2 -ffreestanding -fno-builtin -nostdlib -B$(abspath $(SYSROOT_DIR))/usr/lib $(CRT0_O) $< -L$(ROOTFS_DIR)/lib -L$(abspath $(SYSROOT_DIR))/usr/lib -lGL -lX11 -lxcb -lm -lc -Wl,--allow-shlib-undefined -o $@
	@chmod +x $@

-include $(wildcard $(BUILD_DIR)/userland/bin/*.d)

