# SzpontOS Modern C++ Desktop Environment & Window Manager (szpontdesktop)
ROOT_DIR ?= $(abspath ../..)
include $(ROOT_DIR)/config.mk

TARGET_BIN := $(ROOTFS_DIR)/usr/bin/szpontdesktop
SYMLINK_BIN := $(ROOTFS_DIR)/bin/szpontdesktop
BUILD_OBJ_DIR := $(BUILD_DIR)/userland/szpontdesktop

SRCS := $(wildcard src/*.cpp)
OBJS := $(patsubst src/%.cpp, $(BUILD_OBJ_DIR)/%.o, $(SRCS))

CXXFLAGS_DESKTOP := $(USER_CXXFLAGS) \
	-Iinclude \
	-I$(ROOT_DIR)/userland/lib/libszpontui/include \
	-I$(ROOT_DIR)/third_party/stb \
	-I$(SYSROOT_DIR)/usr/include \
	-isystem $(SYSROOT_DIR)/usr/include/freetype2 \
	-isystem $(SYSROOT_DIR)/usr/include/harfbuzz \
	-isystem $(SYSROOT_DIR)/usr/include/pixman-1

.PHONY: all clean

all: $(TARGET_BIN) $(SYMLINK_BIN)

$(BUILD_OBJ_DIR):
	@mkdir -p $@

$(BUILD_OBJ_DIR)/%.o: src/%.cpp | $(SYSROOT_STAMP) $(BUILD_OBJ_DIR)
	@mkdir -p $(BUILD_OBJ_DIR)
	@echo "  [CXX-SZPONTDESKTOP] $<"
	@$(CXX) $(CXXFLAGS_DESKTOP) -MMD -MP -MF $(BUILD_OBJ_DIR)/$*.d -c $< -o $@

$(TARGET_BIN): $(OBJS) $(CRT0_O) $(CRTI_O) $(CRTN_O) | $(SYSROOT_STAMP) $(ROOTFS_DIR)/usr/bin $(LIBC_SO) $(LIBM_SO) $(LIBSTDCXX_SO) $(ROOTFS_DIR)/usr/lib/libszpontui.so
	@mkdir -p $(ROOTFS_DIR)/usr/bin
	@rm -f $@
	@echo "  [LD-SZPONTDESKTOP] $@"
	@$(CC) -O2 -ffreestanding -fno-builtin -nostdlib -B$(abspath $(SYSROOT_DIR))/usr/lib \
		$(CRT0_O) $(CRTI_O) $$($(CC) -print-file-name=crtbegin.o) $(OBJS) $$($(CC) -print-file-name=crtend.o) $(CRTN_O) \
		-L$(ROOTFS_DIR)/lib -L$(ROOTFS_DIR)/usr/lib -L$(abspath $(SYSROOT_DIR))/usr/lib \
		-Wl,-rpath-link=$(ROOTFS_DIR)/usr/lib \
		-lszpontui -lharfbuzz -lfreetype -lz -lpixman-1 -lX11 -lXext -lxcb -lXau -lXdmcp -lstdc++ -lm -lc -o $@
	@chmod +x $@

$(SYMLINK_BIN): $(TARGET_BIN)
	@mkdir -p $(ROOTFS_DIR)/bin
	@ln -sf /usr/bin/szpontdesktop $(SYMLINK_BIN)

clean:
	@rm -rf $(BUILD_OBJ_DIR) $(TARGET_BIN) $(SYMLINK_BIN)

-include $(wildcard $(BUILD_OBJ_DIR)/*.d)
