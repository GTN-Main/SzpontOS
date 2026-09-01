# SzpontOS Interactive Unix Shell Makefile (/bin/sh)
ROOT_DIR ?= $(abspath ../..)
include $(ROOT_DIR)/config.mk

TARGET := $(ROOTFS_DIR)/bin/sh
SRCS   := $(ROOT_DIR)/userland/sh/main.c
OBJS   := $(BUILD_DIR)/userland/sh/main.o

.PHONY: all sh

all: sh

sh: $(TARGET)

$(TARGET): $(SRCS) $(CRT0_O) $(LIBC_SO) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(BUILD_DIR)/userland/sh
	@echo "  [CC]  $<"
	@$(CC) $(USER_CFLAGS) -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath $(SYSROOT_DIR))/usr/include -c $< -o $(OBJS)
	@echo "  [LD]  $@"
	@$(CC) -O2 -ffreestanding -fno-builtin -nostdlib -B$(abspath $(SYSROOT_DIR))/usr/lib $(CRT0_O) $(OBJS) -L$(ROOTFS_DIR)/lib -L$(abspath $(SYSROOT_DIR))/usr/lib -lc -o $@
	@chmod +x $@
