# SzpontUI C++ Modern UI Framework Makefile
ROOT_DIR ?= $(abspath ../../..)
include $(ROOT_DIR)/config.mk

LIB_NAME := libszpontui
SO_TARGET := $(ROOTFS_DIR)/usr/lib/$(LIB_NAME).so
A_TARGET  := $(ROOTFS_DIR)/usr/lib/$(LIB_NAME).a
SYSROOT_SO := $(SYSROOT_DIR)/usr/lib/$(LIB_NAME).so
SYSROOT_A  := $(SYSROOT_DIR)/usr/lib/$(LIB_NAME).a

SRC_DIRS := src/Core src/Gfx src/Font src/Platform src/Layout src/Theme src/Widgets src/App
SRCS := $(wildcard $(SRC_DIRS:%=%/*.cpp))
OBJS := $(patsubst src/%.cpp, $(BUILD_DIR)/userland/libszpontui/%.o, $(SRCS))

CXXFLAGS_SZPONTUI := $(USER_CXXFLAGS) \
	-I$(ROOT_DIR)/userland/lib/libszpontui/include \
	-isystem $(SYSROOT_DIR)/usr/include/freetype2 \
	-isystem $(SYSROOT_DIR)/usr/include/harfbuzz \
	-isystem $(SYSROOT_DIR)/usr/include/pixman-1 \
	-I$(SYSROOT_DIR)/usr/include/freetype2 \
	-I$(SYSROOT_DIR)/usr/include/harfbuzz \
	-I$(SYSROOT_DIR)/usr/include/pixman-1

.PHONY: all clean headers

all: $(SO_TARGET) $(A_TARGET) headers

headers:
	@mkdir -p $(SYSROOT_DIR)/usr/include/SzpontUI
	@cp -rf include/SzpontUI/* $(SYSROOT_DIR)/usr/include/SzpontUI/

$(BUILD_DIR)/userland/libszpontui/%.o: src/%.cpp | $(SYSROOT_STAMP)
	@mkdir -p $(dir $@)
	@echo "  [CXX-SZPONTUI] $<"
	@$(CXX) $(CXXFLAGS_SZPONTUI) -MMD -MP -MF $(BUILD_DIR)/userland/libszpontui/$*.d -c $< -o $@

$(SO_TARGET): $(OBJS) | $(ROOTFS_DIR)/usr/lib $(SYSROOT_DIR)/usr/lib
	@mkdir -p $(ROOTFS_DIR)/usr/lib $(SYSROOT_DIR)/usr/lib
	@echo "  [LD-SZPONTUI] $@"
	@$(LD) -shared -soname $(LIB_NAME).so.1 -o $(SYSROOT_DIR)/usr/lib/$(LIB_NAME).so.1.0.0 $(OBJS) \
		-L$(abspath $(SYSROOT_DIR))/usr/lib -lharfbuzz -lfreetype -lpixman-1 -lX11 -lXext -lstdc++ -lm -lc
	@ln -sf $(LIB_NAME).so.1.0.0 $(SYSROOT_DIR)/usr/lib/$(LIB_NAME).so.1
	@ln -sf $(LIB_NAME).so.1.0.0 $(SYSROOT_DIR)/usr/lib/$(LIB_NAME).so
	@cp -f $(SYSROOT_DIR)/usr/lib/$(LIB_NAME).so.1.0.0 $(ROOTFS_DIR)/usr/lib/$(LIB_NAME).so.1.0.0
	@ln -sf $(LIB_NAME).so.1.0.0 $(ROOTFS_DIR)/usr/lib/$(LIB_NAME).so.1
	@ln -sf $(LIB_NAME).so.1.0.0 $(ROOTFS_DIR)/usr/lib/$(LIB_NAME).so

$(A_TARGET): $(OBJS) | $(ROOTFS_DIR)/usr/lib $(SYSROOT_DIR)/usr/lib
	@mkdir -p $(ROOTFS_DIR)/usr/lib $(SYSROOT_DIR)/usr/lib
	@echo "  [AR-SZPONTUI] $@"
	@$(AR) rcs $(SYSROOT_A) $(OBJS)
	@$(RANLIB) $(SYSROOT_A)
	@cp -f $(SYSROOT_A) $(A_TARGET)

clean:
	@rm -rf $(BUILD_DIR)/userland/libszpontui

-include $(wildcard $(BUILD_DIR)/userland/libszpontui/**/*.d)
