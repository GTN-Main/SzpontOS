# SzpontOS Modern X11 Display & Login Manager (szpontlogin)
ROOT_DIR ?= $(abspath ../..)
include $(ROOT_DIR)/config.mk

TARGET_BIN := $(ROOTFS_DIR)/usr/bin/szpontlogin
SYMLINK_BIN := $(ROOTFS_DIR)/bin/szpontlogin
BUILD_OBJ := $(BUILD_DIR)/userland/szpontlogin/main.o

CXXFLAGS_LOGIN := $(USER_CXXFLAGS) \
	-I$(ROOT_DIR)/userland/lib/libszpontui/include \
	-I$(SYSROOT_DIR)/usr/include \
	-isystem $(SYSROOT_DIR)/usr/include/freetype2 \
	-isystem $(SYSROOT_DIR)/usr/include/harfbuzz \
	-isystem $(SYSROOT_DIR)/usr/include/pixman-1

.PHONY: all clean

all: $(TARGET_BIN) $(SYMLINK_BIN)

$(BUILD_DIR)/userland/szpontlogin:
	@mkdir -p $@

$(BUILD_OBJ): main.cpp | $(SYSROOT_STAMP) $(BUILD_DIR)/userland/szpontlogin
	@mkdir -p $(BUILD_DIR)/userland/szpontlogin
	@echo "  [CXX-SZPONTLOGIN] $<"
	@$(CXX) $(CXXFLAGS_LOGIN) -MMD -MP -MF $(BUILD_DIR)/userland/szpontlogin/main.d -c $< -o $@

$(TARGET_BIN): $(BUILD_OBJ) $(CRT0_O) $(CRTI_O) $(CRTN_O) | $(SYSROOT_STAMP) $(ROOTFS_DIR)/usr/bin $(LIBC_SO) $(LIBM_SO) $(LIBSTDCXX_SO) $(ROOTFS_DIR)/usr/lib/libszpontui.so
	@mkdir -p $(ROOTFS_DIR)/usr/bin
	@rm -f $@
	@echo "  [LD-SZPONTLOGIN] $@"
	@$(CC) -O2 -ffreestanding -fno-builtin -nostdlib -B$(abspath $(SYSROOT_DIR))/usr/lib \
		$(CRT0_O) $(CRTI_O) $$($(CC) -print-file-name=crtbegin.o) $< $$($(CC) -print-file-name=crtend.o) $(CRTN_O) \
		-L$(ROOTFS_DIR)/lib -L$(ROOTFS_DIR)/usr/lib -L$(abspath $(SYSROOT_DIR))/usr/lib \
		-Wl,-rpath-link=$(ROOTFS_DIR)/usr/lib \
		-lszpontui -lharfbuzz -lfreetype -lz -lpixman-1 -lX11 -lXext -lxcb -lXau -lXdmcp -lstdc++ -lm -lc -o $@
	@chmod +x $@

$(SYMLINK_BIN): $(TARGET_BIN)
	@mkdir -p $(ROOTFS_DIR)/bin
	@ln -sf /usr/bin/szpontlogin $(SYMLINK_BIN)

clean:
	@rm -rf $(BUILD_DIR)/userland/szpontlogin $(TARGET_BIN) $(SYMLINK_BIN)

-include $(BUILD_DIR)/userland/szpontlogin/main.d
