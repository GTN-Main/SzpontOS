# SzpontOS Native libgbm Makefile
# (C) Copyright by Szpont Industries. All rights reserved.

ROOT_DIR ?= $(abspath ..)
include $(ROOT_DIR)/config.mk

LIBGBM_SRCS := $(ROOT_DIR)/libgbm/src/gbm.c
LIBGBM_OBJS := $(patsubst $(ROOT_DIR)/libgbm/%.c, $(BUILD_DIR)/libgbm/%.o, $(LIBGBM_SRCS))

LIBGBM_SO := $(BUILD_DIR)/libgbm/libgbm.so
LIBGBM_PC := $(SYSROOT_DIR)/usr/lib/pkgconfig/gbm.pc

LIBGBM_CFLAGS := $(USER_CFLAGS) -fPIC \
    -isystem $(abspath $(SYSROOT_DIR))/usr/include \
    -isystem $(abspath $(SYSROOT_DIR))/usr/include/libdrm \
    -I$(ROOT_DIR)/libgbm/include

.PHONY: all libgbm install clean

all: libgbm install

libgbm: $(LIBGBM_SO)

$(BUILD_DIR)/libgbm/%.o: $(ROOT_DIR)/libgbm/%.c | $(SYSROOT_STAMP)
	@mkdir -p $(dir $@)
	@echo "  [CC-LIBGBM] $<"
	@$(CC) $(LIBGBM_CFLAGS) -c $< -o $@

$(LIBGBM_SO): $(LIBGBM_OBJS) | $(ROOTFS_DIR) $(SYSROOT_STAMP)
	@mkdir -p $(BUILD_DIR)/libgbm $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib
	@echo "  [LD-LIBGBM] $@"
	@$(LD) -shared -soname libgbm.so.1 $(LIBGBM_OBJS) -L$(abspath $(SYSROOT_DIR))/usr/lib -ldrm -lc -o $@

install: $(LIBGBM_SO)
	@mkdir -p $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/lib/pkgconfig $(SYSROOT_DIR)/usr/include
	@rm -f $(ROOTFS_DIR)/lib/libgbm* $(SYSROOT_DIR)/usr/lib/libgbm*
	@cp -f $(LIBGBM_SO) $(ROOTFS_DIR)/lib/libgbm.so
	@ln -sf libgbm.so $(ROOTFS_DIR)/lib/libgbm.so.1
	@cp -f $(LIBGBM_SO) $(SYSROOT_DIR)/usr/lib/libgbm.so
	@ln -sf libgbm.so $(SYSROOT_DIR)/usr/lib/libgbm.so.1
	@cp -f $(ROOT_DIR)/libgbm/include/gbm.h $(SYSROOT_DIR)/usr/include/
	@sed -e 's|@prefix@|/usr|g' \
	     -e 's|@exec_prefix@|/usr|g' \
	     -e 's|@libdir@|/usr/lib|g' \
	     -e 's|@includedir@|/usr/include|g' \
	     $(ROOT_DIR)/libgbm/gbm.pc.in > $(LIBGBM_PC)
	@cp -f $(LIBGBM_PC) $(SYSROOT_DIR)/usr/share/pkgconfig/gbm.pc 2>/dev/null || true
	@echo "  [OK]  libgbm installed to sysroot and rootfs"

clean:
	@rm -rf $(BUILD_DIR)/libgbm
