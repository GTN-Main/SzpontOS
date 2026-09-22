# SzpontOS Userland Master Makefile
ROOT_DIR ?= $(abspath ..)
include $(ROOT_DIR)/config.mk

.PHONY: all userland skeleton init sh bin xserver

all: userland

userland: skeleton init sh bin xserver
	@mkdir -p $(dir $(USERLAND_STAMP))
	@touch $(USERLAND_STAMP)

skeleton:
	@mkdir -p $(ROOTFS_DIR)/bin $(ROOTFS_DIR)/sbin $(ROOTFS_DIR)/lib \
		$(ROOTFS_DIR)/usr/bin $(ROOTFS_DIR)/usr/sbin $(ROOTFS_DIR)/usr/lib $(ROOTFS_DIR)/usr/tbin \
		$(ROOTFS_DIR)/dev $(ROOTFS_DIR)/etc $(ROOTFS_DIR)/etc/X11/app-defaults \
		$(ROOTFS_DIR)/proc $(ROOTFS_DIR)/sys $(ROOTFS_DIR)/mnt \
		$(ROOTFS_DIR)/tmp $(ROOTFS_DIR)/tmp/.X11-unix \
		$(ROOTFS_DIR)/var $(ROOTFS_DIR)/var/log $(ROOTFS_DIR)/var/run $(ROOTFS_DIR)/var/empty $(ROOTFS_DIR)/var/lib/xkb \
		$(ROOTFS_DIR)/root $(ROOTFS_DIR)/home $(ROOTFS_DIR)/home/szpont \
		$(ROOTFS_DIR)/usr/share $(ROOTFS_DIR)/usr/share/artwork $(ROOTFS_DIR)/usr/share/X11/app-defaults $(ROOTFS_DIR)/usr/lib/X11/app-defaults
	@chmod 1777 $(ROOTFS_DIR)/tmp $(ROOTFS_DIR)/tmp/.X11-unix 2>/dev/null || true
	@chmod 755 $(ROOTFS_DIR)/var/empty 2>/dev/null || true
	@if [ -d $(ROOTFS_SKELETON_DIR) ]; then cp -a $(ROOTFS_SKELETON_DIR)/. $(ROOTFS_DIR)/ 2>/dev/null || true; fi
	@if [ -f $(ROOTFS_SKELETON_DIR)/etc/X11/app-defaults/XTerm ]; then \
		cp -f $(ROOTFS_SKELETON_DIR)/etc/X11/app-defaults/XTerm $(ROOTFS_DIR)/usr/share/X11/app-defaults/XTerm; \
		cp -f $(ROOTFS_SKELETON_DIR)/etc/X11/app-defaults/XTerm $(ROOTFS_DIR)/usr/lib/X11/app-defaults/XTerm; \
	fi

init: | skeleton
	@$(MAKE) -j$(JOBS) -C $(ROOT_DIR)/userland/init

sh: | skeleton
	@$(MAKE) -j$(JOBS) -C $(ROOT_DIR)/userland/sh

bin: | skeleton
	@$(MAKE) -j$(JOBS) -C $(ROOT_DIR)/userland/bin

xserver: | skeleton
	@$(MAKE) -j$(JOBS) -C $(ROOT_DIR)/userland/xserver
