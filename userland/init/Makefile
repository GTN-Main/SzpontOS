# SzpontOS Init Process Makefile (/bin/init)
ROOT_DIR ?= $(abspath ../..)
include $(ROOT_DIR)/config.mk

TARGET := $(ROOTFS_DIR)/bin/init
SRCS   := $(ROOT_DIR)/userland/init/main.c
OBJS   := $(BUILD_DIR)/userland/init/main.o

.PHONY: all init

all: init

init: $(TARGET)

$(TARGET): $(SRCS) $(CRT0_O) $(LIBC_A) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(BUILD_DIR)/userland/init
	@echo "  [CC]  $<"
	@$(CC) $(USER_CFLAGS) -c $< -o $(OBJS)
	@echo "  [LD]  $@"
	@$(LD) $(USER_LDFLAGS) $(CRT0_O) $(OBJS) $(LIBC_A) -o $@
