# SzpontOS Loadable Kernel Modules (.sko) Makefile
ROOT_DIR ?= $(abspath ..)
include $(ROOT_DIR)/config.mk

MODULES := \
    $(MODULE_DIR)/hello.sko \
    $(MODULE_DIR)/dummy_dev.sko

.PHONY: all modules

all: modules

modules: $(MODULES)

$(MODULE_DIR)/hello.sko: $(ROOT_DIR)/modules/hello/hello.c | $(ROOTFS_DIR)
	@mkdir -p $(MODULE_DIR)
	@echo "  [CC-SKO] $< -> $@"
	@$(CC) $(MODULE_CFLAGS) -c $< -o $@

$(MODULE_DIR)/dummy_dev.sko: $(ROOT_DIR)/modules/dummy_dev/dummy_dev.c | $(ROOTFS_DIR)
	@mkdir -p $(MODULE_DIR)
	@echo "  [CC-SKO] $< -> $@"
	@$(CC) $(MODULE_CFLAGS) -c $< -o $@
