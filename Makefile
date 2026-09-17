# SzpontOS Top-Level Orchestration Makefile
# Multi-platform build system supporting macOS & Linux with GCC / Clang

ROOT_DIR := $(abspath .)
include $(ROOT_DIR)/config.mk
include $(ROOT_DIR)/mk/third_party.mk
include $(ROOT_DIR)/mk/iso.mk
include $(ROOT_DIR)/mk/qemu.mk

# ==============================================================================
# Phony Targets
# ==============================================================================
.PHONY: all build toolchain-info kernel libc libdrm userland modules sysroot \
        third-party clean distclean compile_commands.json compile-commands bear

# Default target: build bootable ISO image
all: build $(ISO_IMAGE)

# Display detected toolchain information
toolchain-info:
	@echo "  [TOOLCHAIN] CC:    $(CC) ($(TOOLCHAIN_TYPE))"
	@echo "  [TOOLCHAIN] LD:    $(LD)"
	@echo "  [TOOLCHAIN] AR:    $(AR)"
	@echo "  [TOOLCHAIN] ASM:   $(NASM)"
	@echo "  [TOOLCHAIN] ISO:   $(XORRISO)"
	@echo "  [TOOLCHAIN] CORES: $(NPROC) (Parallel Jobs: $(JOBS))"

# Build all core components
build: toolchain-info $(SYSROOT_STAMP) $(ROOTFS_DIR)/lib/libdrm.so $(ROOTFS_DIR)/lib/libgbm.so $(USERLAND_STAMP) $(MODULES_STAMP) $(THIRDPARTY_STAMP) $(KERNEL_ELF)

KERNEL_SRCS := $(shell find $(ROOT_DIR)/kernel/src $(ROOT_DIR)/kernel/include $(ROOT_DIR)/kernel/arch -type f 2>/dev/null)
LIBC_SRCS   := $(shell find $(ROOT_DIR)/libc/src $(ROOT_DIR)/libc/include -type f 2>/dev/null)

# Subsystem delegates
$(KERNEL_ELF): $(KERNEL_SRCS) $(ROOT_DIR)/kernel/linker.ld $(ROOT_DIR)/kernel/Makefile
	@$(MAKE) -j$(JOBS) -C $(ROOT_DIR)/kernel

kernel: $(KERNEL_ELF)

$(LIBC_SO): $(LIBC_SRCS)
	@$(MAKE) -j$(JOBS) -C $(ROOT_DIR)/libc libc

libc: $(LIBC_SO)

$(SYSROOT_STAMP): $(LIBC_SRCS)
	@$(MAKE) -j$(JOBS) -C $(ROOT_DIR)/libc sysroot

sysroot: $(SYSROOT_STAMP)

LIBDRM_SRCS := $(shell find $(ROOT_DIR)/libdrm -type f 2>/dev/null)
LIBGBM_SRCS := $(shell find $(ROOT_DIR)/libgbm -type f 2>/dev/null)

$(ROOTFS_DIR)/lib/libdrm.so: $(LIBDRM_SRCS) | $(SYSROOT_STAMP) $(LIBC_SO)
	@$(MAKE) -j$(JOBS) -C $(ROOT_DIR)/libdrm

libdrm: $(ROOTFS_DIR)/lib/libdrm.so

$(ROOTFS_DIR)/lib/libgbm.so: $(LIBGBM_SRCS) | $(ROOTFS_DIR)/lib/libdrm.so $(SYSROOT_STAMP)
	@$(MAKE) -j$(JOBS) -C $(ROOT_DIR)/libgbm

libgbm: $(ROOTFS_DIR)/lib/libgbm.so

MODULES_SRCS := $(shell find $(ROOT_DIR)/modules -type f 2>/dev/null)
$(MODULES_STAMP): $(MODULES_SRCS) | $(SYSROOT_STAMP)
	@$(MAKE) -j$(JOBS) -C $(ROOT_DIR)/modules
	@mkdir -p $(dir $@) && touch $@

modules: $(MODULES_STAMP)

USERLAND_SRCS := $(shell find $(ROOT_DIR)/userland -type f 2>/dev/null)
$(USERLAND_STAMP): $(USERLAND_SRCS) $(SYSROOT_STAMP) $(ALL_ROOTFS_SOS)
	@$(MAKE) -j$(JOBS) -C $(ROOT_DIR)/userland
	@mkdir -p $(dir $@) && touch $@

userland: $(USERLAND_STAMP)

third-party: $(THIRDPARTY_STAMP)

# ==============================================================================
# Development Tooling & Compilation Database (clangd / bear)
# ==============================================================================
compile_commands.json compile-commands bear:
	@echo "  [BEAR] Generowanie compile_commands.json za pomocą bear..."
	@if command -v bear >/dev/null 2>&1; then \
		rm -f compile_commands.json; \
		bear -- $(MAKE) clean all >/dev/null 2>&1 || true; \
		if [ -f compile_commands.json ]; then \
			python3 -c "import json; db = json.load(open('compile_commands.json')); json.dump([e for e in db if not e.get('file','').endswith('.asm') and (not e.get('arguments') or e['arguments'][0] != 'nasm')], open('compile_commands.json', 'w'), indent=2)" 2>/dev/null || true; \
		fi; \
		echo "  [OK]   Wygenerowano compile_commands.json dla clangd / IDE."; \
	else \
		echo "  [!]    Błąd: Brak narzędzia bear. Zainstaluj 'brew install bear'."; \
		exit 1; \
	fi

# ==============================================================================
# Cleanup Rules
# ==============================================================================
clean:
	@echo "  [CLEAN] Czyszczenie katalogu build..."
	@rm -rf $(BUILD_DIR)

distclean: clean
	@echo "  [CLEAN] Usuwanie pobranych binariów Limine i bazy kompilacji..."
	@rm -rf limine-bin compile_commands.json
