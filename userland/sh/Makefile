# SzpontOS Interactive Unix Shell Makefile (/bin/sh)
ROOT_DIR ?= $(abspath ../..)
include $(ROOT_DIR)/config.mk

TARGET := $(ROOTFS_DIR)/bin/sh
SRCS   := $(ROOT_DIR)/userland/sh/main.c
OBJS   := $(BUILD_DIR)/userland/sh/main.o

.PHONY: all sh

all: sh

sh: $(TARGET)

$(TARGET): $(SRCS) $(CRT0_O) $(LIBC_A) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(BUILD_DIR)/userland/sh
	@echo "  [CC]  $<"
	@$(CC) $(USER_CFLAGS) -c $< -o $(OBJS)
	@echo "  [LD]  $@"
	@$(LD) $(USER_LDFLAGS) $(CRT0_O) $(OBJS) $(LIBC_A) -o $@
