# SzpontOS Userland Dynamic Libraries Makefile
ROOT_DIR ?= $(abspath ../..)
include $(ROOT_DIR)/config.mk

.PHONY: all libs

all: libs

libs: $(LIBCALC_SO)

$(LIBCALC_SO): $(ROOT_DIR)/userland/lib/libcalc.c | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/lib $(BUILD_DIR)/userland/lib
	@echo "  [CC-SO] $<"
	@$(CC) $(USER_CFLAGS) -c $< -o $(BUILD_DIR)/userland/lib/libcalc.o
	@echo "  [LD-SO] $@"
	@$(LD) -shared -soname libcalc.so $(BUILD_DIR)/userland/lib/libcalc.o -o $@
