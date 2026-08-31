# SzpontOS Third-Party Packages & X11 Libraries Build Rules
# Included by root Makefile

# ==============================================================================
# Shared Libraries Required in Rootfs
# ==============================================================================
ALL_ROOTFS_SOS := \
	$(LIBC_SO) $(LIBM_SO) \
	$(ROOTFS_DIR)/lib/libdrm.so \
	$(ROOTFS_DIR)/lib/libpixman-1.so \
	$(ROOTFS_DIR)/lib/libX11.so \
	$(ROOTFS_DIR)/lib/libxcb.so \
	$(ROOTFS_DIR)/lib/libXau.so \
	$(ROOTFS_DIR)/lib/libXdmcp.so \
	$(ROOTFS_DIR)/lib/libxkbfile.so \
	$(ROOTFS_DIR)/lib/libfontenc.so \
	$(ROOTFS_DIR)/lib/libXfont2.so \
	$(ROOTFS_DIR)/lib/libxcvt.so \
	$(ROOTFS_DIR)/lib/libpciaccess.so \
	$(ROOTFS_DIR)/lib/libICE.so \
	$(ROOTFS_DIR)/lib/libSM.so \
	$(ROOTFS_DIR)/lib/libXpm.so \
	$(ROOTFS_DIR)/lib/libXext.so \
	$(ROOTFS_DIR)/lib/libXt.so \
	$(ROOTFS_DIR)/lib/libXmu.so \
	$(ROOTFS_DIR)/lib/libXaw.so

# ==============================================================================
# GNU Ncurses (Cross-compiled via original Autotools)
# ==============================================================================
$(NCURSES_BUILD_DIR)/Makefile: $(SYSROOT_STAMP) | $(NCURSES_BUILD_DIR)
	@echo "  [CONF-NCURSES] Konfiguracja GNU Ncurses (Autotools cross-compile)..."
	@cd $(NCURSES_BUILD_DIR) && \
	../../../third_party/ncurses/configure \
	    --host=x86_64-elf \
	    --prefix=/usr \
	    --with-build-cc=gcc \
	    --without-ada \
	    --without-cxx \
	    --without-tests \
	    --without-progs \
	    --without-manpages \
	    --without-debug \
	    --without-gpm \
	    --without-sysmouse \
	    --enable-overwrite \
	    --enable-termcap \
	    --without-fallbacks \
	    --disable-database \
	    --disable-home-terminfo \
	    --enable-static \
	    --without-shared \
	    CC="$(CC)" \
	    CPP="$(CC) -E" \
	    AR="$(AR)" \
	    RANLIB="$(RANLIB)" \
	    CFLAGS="-O2 -ffreestanding -fno-builtin -isystem $(abspath $(SYSROOT_DIR))/usr/include -B$(abspath $(SYSROOT_DIR))/usr/lib" \
	    CPPFLAGS="-isystem $(abspath $(SYSROOT_DIR))/usr/include" \
	    LDFLAGS="-nostdlib -L$(abspath $(SYSROOT_DIR))/usr/lib -B$(abspath $(SYSROOT_DIR))/usr/lib" \
	    LIBS="-lc"
	@echo "#define SIG_ATOMIC_T int" >> $(NCURSES_BUILD_DIR)/include/ncurses_cfg.h
	@echo "#define TYPE_SIG_ATOMIC_T int" >> $(NCURSES_BUILD_DIR)/include/ncurses_cfg.h
	@echo "#define HAVE_SETENV 1" >> $(NCURSES_BUILD_DIR)/include/ncurses_cfg.h
	@echo "#define HAVE_PUTENV 1" >> $(NCURSES_BUILD_DIR)/include/ncurses_cfg.h
	@echo "#define HAVE_GETCWD 1" >> $(NCURSES_BUILD_DIR)/include/ncurses_cfg.h
	@echo "#define HAVE_FCNTL_H 1" >> $(NCURSES_BUILD_DIR)/include/ncurses_cfg.h
	@echo "#define HAVE_UNISTD_H 1" >> $(NCURSES_BUILD_DIR)/include/ncurses_cfg.h
	@echo "#define HAVE_SYS_IOCTL_H 1" >> $(NCURSES_BUILD_DIR)/include/ncurses_cfg.h
	@echo "#include <fcntl.h>" >> $(NCURSES_BUILD_DIR)/include/ncurses_cfg.h
	@echo "#include <unistd.h>" >> $(NCURSES_BUILD_DIR)/include/ncurses_cfg.h
	@echo "#include <signal.h>" >> $(NCURSES_BUILD_DIR)/include/ncurses_cfg.h
	@echo "#include <stdlib.h>" >> $(NCURSES_BUILD_DIR)/include/ncurses_cfg.h
	@sed -i '' 's/mkdir $$@/mkdir -p $$@/g' $(NCURSES_BUILD_DIR)/ncurses/Makefile 2>/dev/null || sed -i 's/mkdir $$@/mkdir -p $$@/g' $(NCURSES_BUILD_DIR)/ncurses/Makefile 2>/dev/null || true

$(NCURSES_BUILD_DIR):
	@mkdir -p $@

$(LIBNCURSES_A): $(NCURSES_BUILD_DIR)/Makefile $(LIBC_A) $(CRT0_O)
	@echo "  [GEN-NCURSES-FALLBACKS] Generowanie wbudowanych terminali (xterm-256color, vt100)..."
	@python3 scripts/generate_ncurses_fallbacks.py $(NCURSES_BUILD_DIR)/ncurses/fallback.c
	@sed -i '' 's/mkdir $$@/mkdir -p $$@/g' $(NCURSES_BUILD_DIR)/ncurses/Makefile 2>/dev/null || sed -i 's/mkdir $$@/mkdir -p $$@/g' $(NCURSES_BUILD_DIR)/ncurses/Makefile 2>/dev/null || true
	@echo "  [MAKE-NCURSES] Kompilacja GNU Ncurses..."
	@$(MAKE) -C $(NCURSES_BUILD_DIR)/include
	@$(MAKE) -C $(NCURSES_BUILD_DIR)/ncurses
	@mkdir -p $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include $(ROOTFS_DIR)/lib
	@cp $(NCURSES_BUILD_DIR)/lib/libncurses.a $(SYSROOT_DIR)/usr/lib/
	@cp $(NCURSES_BUILD_DIR)/lib/libncurses.a $(ROOTFS_DIR)/lib/
	@cp $(NCURSES_BUILD_DIR)/include/*.h $(SYSROOT_DIR)/usr/include/ 2>/dev/null || true
	@cp third_party/ncurses/include/curses.h $(SYSROOT_DIR)/usr/include/ 2>/dev/null || true
	@cp $(SYSROOT_DIR)/usr/include/curses.h $(SYSROOT_DIR)/usr/include/ncurses.h 2>/dev/null || true

# ==============================================================================
# GNU nano (Direct Compilation against libc & libncurses)
# ==============================================================================
$(NANO_BUILD_DIR)/revision.h: | $(NANO_BUILD_DIR)
	@mkdir -p $(NANO_BUILD_DIR)
	@echo '#define REVISION "GNU nano 9.2.4"' > $@

$(NANO_BUILD_DIR):
	@mkdir -p $@

NANO_SRCS := $(wildcard third_party/nano/src/*.c)
$(ROOTFS_DIR)/bin/nano: $(NANO_SRCS) $(NANO_BUILD_DIR)/revision.h $(LIBNCURSES_A) $(LIBC_A) $(CRT0_O) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(NANO_BUILD_DIR)
	@echo "  [MAKE-NANO] Kompilacja GNU nano..."
	@$(CC) $(USER_CFLAGS) -nostdlib -I$(NANO_BUILD_DIR) -Ithird_party/nano/src -isystem $(abspath $(SYSROOT_DIR))/usr/include \
	    -DPACKAGE=\"nano\" -DVERSION=\"7.2\" -DENABLE_UTF8=1 -DENABLE_COLOR=1 -DENABLE_NANORC=1 \
	    -DENABLE_MULTIBUFFER=1 -DHAVE_NCURSES_H=1 -DHAVE_CURSES_H=1 -DHAVE_LIMITS_H=1 -DHAVE_SYS_PARAM_H=1 \
	    -DHAVE_TERMIOS_H=1 -DHAVE_UNISTD_H=1 -DHAVE_FCNTL_H=1 -DHAVE_DIRENT_H=1 -DHAVE_PWD_H=1 -DHAVE_GRP_H=1 \
	    -DHAVE_GETOPT_H=1 -DHAVE_GETOPT_LONG=1 -DHAVE_SIGACTION=1 -DHAVE_SIGNAL_H=1 \
	    -DNANO_REG_EXTENDED=REG_EXTENDED -DSYSCONFDIR=\"/etc\" \
	    $(NANO_SRCS) $(CRT0_O) -L$(abspath $(SYSROOT_DIR))/usr/lib -lncurses $(LIBC_A) $(LIBM_A) -o $@

# ==============================================================================
# GNU file & libmagic (Autotools cross-compile)
# ==============================================================================
third_party/file/configure:
	@echo "  [PRECONF-FILE] Generowanie configure dla GNU file..."
	@cd third_party/file && autoreconf -fi || true

$(FILE_BUILD_DIR)/Makefile: third_party/file/configure $(SYSROOT_STAMP) | $(FILE_BUILD_DIR)
	@echo "  [CONF-FILE] Konfiguracja GNU file (Autotools cross-compile)..."
	@cd $(FILE_BUILD_DIR) && \
	../../../third_party/file/configure \
	    --host=x86_64-elf \
	    --prefix=/usr \
	    --sysconfdir=/etc \
	    --datadir=/usr/share \
	    --disable-shared \
	    --enable-static \
	    --disable-zlib \
	    --disable-bzlib \
	    --disable-xzlib \
	    --disable-zstdlib \
	    --disable-lzlib \
	    --disable-lrziplib \
	    --disable-lz4lib \
	    --disable-libseccomp \
	    --disable-landlock \
	    --disable-warnings \
	    CC="$(CC)" \
	    AR="$(AR)" \
	    RANLIB="$(RANLIB)" \
	    CFLAGS="-O2 -ffreestanding -fno-builtin -isystem $(abspath $(SYSROOT_DIR))/usr/include -B$(abspath $(SYSROOT_DIR))/usr/lib" \
	    LDFLAGS="-nostdlib -L$(abspath $(SYSROOT_DIR))/usr/lib -B$(abspath $(SYSROOT_DIR))/usr/lib" \
	    LIBS="-lc"

$(FILE_BUILD_DIR):
	@mkdir -p $@

$(ROOTFS_DIR)/bin/file: $(FILE_BUILD_DIR)/Makefile $(LIBC_A) $(CRT0_O) $(LIBM_A) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(ROOTFS_DIR)/lib
	@echo "  [MAKE-FILE] Kompilacja GNU file za pomocą oryginalnego Makefile..."
	@$(MAKE) -C $(FILE_BUILD_DIR)/src file_LDADD="$(abspath $(SYSROOT_DIR))/usr/lib/crt0.o libmagic.la -lm"
	@cp $(FILE_BUILD_DIR)/src/file $@
	@cp $(FILE_BUILD_DIR)/src/.libs/libmagic.a $(ROOTFS_DIR)/lib/ 2>/dev/null || true

# Build /etc/magic database
$(MAGIC_DB): scripts/build_magic_db.py | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/etc $(ROOTFS_DIR)/usr/share/misc
	@echo "  [MAGIC-DB] Generowanie bazy /etc/magic..."
	@python3 scripts/build_magic_db.py third_party/file/magic/Magdir $@
	@cp $@ $(ROOTFS_DIR)/usr/share/misc/magic 2>/dev/null || true

# ==============================================================================
# Zsh (Autotools cross-compile)
# ==============================================================================
third_party/zsh/configure:
	@echo "  [PRECONF-ZSH] Generowanie configure dla Zsh..."
	@cd third_party/zsh && (./Util/preconfig || (autoconf && autoheader && echo > stamp-h.in))

$(ZSH_BUILD_DIR)/Makefile: third_party/zsh/configure $(LIBNCURSES_A) $(SYSROOT_STAMP) | $(ZSH_BUILD_DIR)
	@echo "  [CONF-ZSH] Konfiguracja Zsh (Autotools cross-compile)..."
	@cd $(ZSH_BUILD_DIR) && \
	../../../third_party/zsh/configure \
	    --host=x86_64-elf \
	    --prefix=/usr \
	    --sysconfdir=/etc \
	    --disable-dynamic \
	    --disable-gdbm \
	    --disable-pcre \
	    --disable-cap \
	    --with-term-lib="ncurses" \
	    CC="$(CC)" \
	    CPP="$(CC) -E -isystem $(abspath $(SYSROOT_DIR))/usr/include" \
	    AR="$(AR)" \
	    RANLIB="$(RANLIB)" \
	    CFLAGS="-O2 -ffreestanding -fno-builtin -isystem $(abspath $(SYSROOT_DIR))/usr/include -B$(abspath $(SYSROOT_DIR))/usr/lib" \
	    CPPFLAGS="-isystem $(abspath $(SYSROOT_DIR))/usr/include" \
	    LDFLAGS="-nostdlib -L$(abspath $(SYSROOT_DIR))/usr/lib -B$(abspath $(SYSROOT_DIR))/usr/lib" \
	    LIBS="$(abspath $(SYSROOT_DIR))/usr/lib/crt0.o -lncurses -lc -lm"

$(ZSH_BUILD_DIR):
	@mkdir -p $@

$(ROOTFS_DIR)/bin/zsh: $(ZSH_BUILD_DIR)/Makefile $(LIBNCURSES_A) $(LIBC_A) $(CRT0_O) $(LIBM_A) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin
	@echo "  [MAKE-ZSH] Kompilacja powłoki Zsh..."
	@$(MAKE) -C $(ZSH_BUILD_DIR)/Src zsh
	@cp $(ZSH_BUILD_DIR)/Src/zsh $@

# ==============================================================================
# Fastfetch (CMake cross-compile)
# ==============================================================================
$(FASTFETCH_BUILD_DIR)/Makefile: $(SYSROOT_STAMP) $(LIBC_A) $(CRT0_O) $(LIBM_A) $(LIBDL_A) | $(FASTFETCH_BUILD_DIR)
	@echo "  [CONF-FASTFETCH] Konfiguracja Fastfetch (CMake cross-compile)..."
	@cd $(FASTFETCH_BUILD_DIR) && \
	cmake ../../../third_party/fastfetch \
	    -DCMAKE_SYSTEM_NAME=Linux \
	    -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
	    -DCMAKE_C_COMPILER="$(shell which -a $(CC) 2>/dev/null | grep -v '\.bear' | head -n 1 || which $(CC) 2>/dev/null || echo $(CC))" \
	    -DCMAKE_C_FLAGS="-isystem $(abspath $(SYSROOT_DIR))/usr/include $(LINUX_COMPAT_CFLAGS) -D__linux__=1 -ffreestanding -fno-builtin -O2" \
	    -DCMAKE_EXE_LINKER_FLAGS="-nostdlib -L$(abspath $(SYSROOT_DIR))/usr/lib -B$(abspath $(SYSROOT_DIR))/usr/lib $(abspath $(SYSROOT_DIR))/usr/lib/crt0.o" \
	    -DCMAKE_C_STANDARD_LIBRARIES="-Wl,--start-group $(abspath $(SYSROOT_DIR))/usr/lib/libc.a $(abspath $(SYSROOT_DIR))/usr/lib/libm.a $(abspath $(SYSROOT_DIR))/usr/lib/libdl.a -Wl,--end-group" \
	    -DBINARY_LINK_TYPE=static \
	    -DENABLE_VULKAN=OFF \
	    -DENABLE_WAYLAND=OFF \
	    -DENABLE_XCB_RANDR=OFF \
	    -DENABLE_XRANDR=OFF \
	    -DENABLE_DRM=OFF \
	    -DENABLE_GIO=OFF \
	    -DENABLE_DCONF=OFF \
	    -DENABLE_DBUS=OFF \
	    -DENABLE_SQLITE3=OFF \
	    -DENABLE_PULSE=OFF \
	    -DENABLE_ELF=OFF \
	    -DENABLE_ZLIB=OFF \
	    -DENABLE_LUA=OFF \
	    -DENABLE_QUICKJS=OFF \
	    -DENABLE_OPENCL=OFF \
	    -DENABLE_GLX=OFF \
	    -DENABLE_EGL=OFF \
	    -DENABLE_IMAGEMAGICK7=OFF \
	    -DENABLE_IMAGEMAGICK6=OFF \
	    -DENABLE_CHAFA=OFF \
	    -DENABLE_LIBZFS=OFF \
	    -DENABLE_DDCUTIL=OFF \
	    -DENABLE_THREADS=OFF \
	    -DBUILD_TESTS=OFF \
	    -DBUILD_FLASHFETCH=OFF

$(FASTFETCH_BUILD_DIR):
	@mkdir -p $@

$(ROOTFS_DIR)/bin/fastfetch: $(FASTFETCH_BUILD_DIR)/Makefile $(SYSROOT_STAMP) $(LIBC_A) $(CRT0_O) $(LIBM_A) $(LIBDL_A) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin
	@echo "  [MAKE-FASTFETCH] Kompilacja narzędzia Fastfetch..."
	@$(MAKE) -C $(FASTFETCH_BUILD_DIR) fastfetch
	@cp $(FASTFETCH_BUILD_DIR)/fastfetch $@

# ==============================================================================
# zlib (Cross-compiled via original Makefile)
# ==============================================================================
ZLIB_SRCS := $(wildcard third_party/zlib/*.c)
ZLIB_OBJS := $(patsubst third_party/zlib/%.c,$(ZLIB_BUILD_DIR)/%.o,$(ZLIB_SRCS))

$(ZLIB_BUILD_DIR):
	@mkdir -p $@

$(ZLIB_BUILD_DIR)/%.o: third_party/zlib/%.c $(SYSROOT_STAMP) | $(ZLIB_BUILD_DIR)
	@echo "  [CC-ZLIB] $<"
	@$(CC) $(USER_CFLAGS) -DZ_HAVE_UNISTD_H=1 -DHAVE_UNISTD_H=1 -Ithird_party/zlib -c $< -o $@

$(LIBZ_A): $(ZLIB_OBJS)
	@mkdir -p $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include $(ROOTFS_DIR)/lib
	@echo "  [AR-ZLIB] $@"
	@$(AR) rcs $@ $(ZLIB_OBJS)
	@cp third_party/zlib/zlib.h third_party/zlib/zconf.h $(SYSROOT_DIR)/usr/include/
	@cp $@ $(ROOTFS_DIR)/lib/

# ==============================================================================
# Git (Libre-WD-40 cross-compile using original Makefile)
# ==============================================================================
$(ROOTFS_DIR)/bin/git: $(LIBZ_A) $(SYSROOT_STAMP) $(LIBC_A) $(CRT0_O) $(LIBM_A) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin
	@echo "  [MAKE-GIT] Kompilacja narzędzia Git (Libre-WD-40)..."
	@cp userland/config/git/config.mak third_party/git/config.mak
	@$(MAKE) -C third_party/git \
	    CC="$(CC)" \
	    AR="$(AR)" \
	    RANLIB="$(RANLIB)" \
	    CFLAGS="-O2 -ffreestanding -fno-builtin -isystem $(abspath $(SYSROOT_DIR))/usr/include -B$(abspath $(SYSROOT_DIR))/usr/lib" \
	    LDFLAGS="-nostdlib -L$(abspath $(SYSROOT_DIR))/usr/lib -B$(abspath $(SYSROOT_DIR))/usr/lib $(abspath $(SYSROOT_DIR))/usr/lib/crt0.o" \
	    EXTLIBS="$(abspath $(SYSROOT_DIR))/usr/lib/libz.a $(abspath $(SYSROOT_DIR))/usr/lib/libc.a $(abspath $(SYSROOT_DIR))/usr/lib/libm.a" \
	    uname_S=Linux uname_M=x86_64 \
	    git
	@cp third_party/git/git $@
	@rm -f third_party/git/config.mak

# ==============================================================================
# X11 Headers and Protocol Specifications (xorgproto & xtrans)
# ==============================================================================
$(SYSROOT_DIR)/usr/include/X11/X.h: $(SYSROOT_STAMP)

# ==============================================================================
# libXau Target
# ==============================================================================
$(ROOTFS_DIR)/lib/libXau.so: $(SYSROOT_STAMP) $(wildcard third_party/libXau/*.c) $(wildcard third_party/libXau/include/*.h)
	@mkdir -p $(BUILD_DIR)/third_party/libXau $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib
	@echo "  [MAKE-LIBXAU] Kompilacja libXau..."
	@cd $(BUILD_DIR)/third_party/libXau && \
	    touch config.h && \
	    rm -f *.o && \
	    for src in $(abspath third_party/libXau)/Au*.c; do \
	        if [ "$$(basename $$src)" != "Autest.c" ]; then \
	            $(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath third_party/libXau/include) -I$(abspath $(BUILD_DIR)/third_party/libXau) -c "$$src" -o "$$(basename $$src .c).o"; \
	        fi; \
	    done && \
	    $(LD) -shared -soname libXau.so.6 -o $(abspath $(SYSROOT_DIR))/usr/lib/libXau.so *.o && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXau.so $(abspath $(ROOTFS_DIR))/lib/libXau.so && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXau.so $(abspath $(ROOTFS_DIR))/lib/libXau.so.6 && \
	    cp -r $(abspath third_party/libXau/include/X11)/* $(abspath $(SYSROOT_DIR))/usr/include/X11/

# ==============================================================================
# libXdmcp Target
# ==============================================================================
$(ROOTFS_DIR)/lib/libXdmcp.so: $(SYSROOT_STAMP) $(wildcard third_party/libXdmcp/*.c) $(wildcard third_party/libXdmcp/include/X11/*.h)
	@mkdir -p $(BUILD_DIR)/third_party/libXdmcp $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib
	@echo "  [MAKE-LIBXDMCP] Kompilacja libXdmcp..."
	@cd $(BUILD_DIR)/third_party/libXdmcp && \
	    touch config.h && \
	    rm -f *.o && \
	    for src in $(abspath third_party/libXdmcp)/*.c; do \
	        $(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DHASXDMAUTH=1 -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath third_party/libXdmcp)/include -I$(abspath $(BUILD_DIR)/third_party/libXdmcp) -c "$$src" -o "$$(basename $$src .c).o"; \
	    done && \
	    $(LD) -shared -soname libXdmcp.so.6 -o $(abspath $(SYSROOT_DIR))/usr/lib/libXdmcp.so *.o && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXdmcp.so $(abspath $(ROOTFS_DIR))/lib/libXdmcp.so && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXdmcp.so $(abspath $(ROOTFS_DIR))/lib/libXdmcp.so.6 && \
	    cp -r $(abspath third_party/libXdmcp/include/X11)/* $(abspath $(SYSROOT_DIR))/usr/include/X11/

# ==============================================================================
# libxcb Target
# ==============================================================================
third_party/libxcb/configure: third_party/libxcb/configure.ac
	@echo "  [PRECONF-LIBXCB] Generowanie configure dla libxcb..."
	@cd third_party/libxcb && autoreconf -fi -I ../util-macros -I /opt/homebrew/share/aclocal 2>/dev/null || true

$(ROOTFS_DIR)/lib/libxcb.so: third_party/libxcb/configure $(ROOTFS_DIR)/lib/libXau.so $(ROOTFS_DIR)/lib/libXdmcp.so
	@mkdir -p $(BUILD_DIR)/third_party/libxcb $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/xcb
	@echo "  [CONF-LIBXCB] Konfiguracja i kompilacja libxcb..."
	@cd $(BUILD_DIR)/third_party/libxcb && \
	    if [ ! -f Makefile ]; then \
	        PKG_CONFIG_PATH="$(abspath $(SYSROOT_DIR))/usr/lib/pkgconfig" \
	        $(abspath third_party/libxcb)/configure --host=x86_64-elf --prefix=/usr --enable-shared --disable-static --disable-devel-docs \
	            NEEDED_CFLAGS="-I$(abspath $(SYSROOT_DIR))/usr/include" \
	            NEEDED_LIBS="-L$(abspath $(SYSROOT_DIR))/usr/lib -lXau" \
	            CC="$(CC)" \
	            CFLAGS="-fPIC -O2 -ffreestanding -fno-builtin -isystem $(abspath $(SYSROOT_DIR))/usr/include -B$(abspath $(SYSROOT_DIR))/usr/lib" \
	            LDFLAGS="-nostdlib -L$(abspath $(SYSROOT_DIR))/usr/lib -B$(abspath $(SYSROOT_DIR))/usr/lib"; \
	    fi && \
	    export PYTHONPATH="$(abspath third_party/xcb-proto)" && \
	    $(MAKE) -C src XCBPROTO_XCBINCLUDEDIR="$(abspath $(SYSROOT_DIR))/usr/share/xcb" && \
	    cp -f src/*.h $(abspath $(SYSROOT_DIR))/usr/include/xcb/ && \
	    cd src && \
	    $(LD) -shared -soname libxcb.so.1 -o $(abspath $(SYSROOT_DIR))/usr/lib/libxcb.so.1 *.o && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libxcb.so.1 $(abspath $(SYSROOT_DIR))/usr/lib/libxcb.so && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libxcb.so.1 $(abspath $(ROOTFS_DIR))/lib/libxcb.so.1 && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libxcb.so.1 $(abspath $(ROOTFS_DIR))/lib/libxcb.so && \
	    cp -f $(abspath $(BUILD_DIR)/third_party/libxcb)/*.pc $(abspath $(SYSROOT_DIR))/usr/lib/pkgconfig/ 2>/dev/null || true && \
	    cp -f $(abspath $(BUILD_DIR)/third_party/libxcb)/*.pc $(abspath $(SYSROOT_DIR))/usr/share/pkgconfig/ 2>/dev/null || true

# ==============================================================================
# libX11 Target
# ==============================================================================
third_party/libX11/configure: third_party/libX11/configure.ac
	@echo "  [PRECONF-LIBX11] Generowanie configure dla libX11..."
	@cd third_party/libX11 && autoreconf -fi -I ../util-macros -I ../xtrans -I /opt/homebrew/share/aclocal 2>/dev/null || true

$(ROOTFS_DIR)/lib/libX11.so: third_party/libX11/configure $(ROOTFS_DIR)/lib/libxcb.so $(ROOTFS_DIR)/lib/libXau.so $(ROOTFS_DIR)/lib/libXdmcp.so
	@mkdir -p $(BUILD_DIR)/third_party/libX11 $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/X11 $(SYSROOT_DIR)/usr/include/xcb
	@echo "  [MAKE-LIBX11] Kompilacja libX11..."
	@cp -rf third_party/xorgproto/include/X11/* $(SYSROOT_DIR)/usr/include/X11/ 2>/dev/null || true
	@cp -r third_party/libX11/include/X11/* $(SYSROOT_DIR)/usr/include/X11/ 2>/dev/null || true
	@cp -f third_party/libxcb/src/*.h $(SYSROOT_DIR)/usr/include/xcb/ 2>/dev/null || true
	@cp -f $(BUILD_DIR)/third_party/libxcb/src/*.h $(SYSROOT_DIR)/usr/include/xcb/ 2>/dev/null || true
	@cd $(BUILD_DIR)/third_party/libX11 && \
	    if [ ! -f Makefile ]; then \
	        PKG_CONFIG_PATH="$(abspath $(SYSROOT_DIR))/usr/lib/pkgconfig:$(abspath $(SYSROOT_DIR))/usr/share/pkgconfig" \
	        PKG_CONFIG_LIBDIR="$(abspath $(SYSROOT_DIR))/usr/lib/pkgconfig:$(abspath $(SYSROOT_DIR))/usr/share/pkgconfig" \
	        xorg_cv_malloc0_returns_null=no \
	        $(abspath third_party/libX11)/configure \
	            --host=x86_64-elf \
	            --prefix=/usr \
	            --with-keysymdefdir="$(abspath $(SYSROOT_DIR))/usr/include/X11" \
	            --enable-shared \
	            --disable-static \
	            --disable-specs \
	            --disable-unit-tests \
	            CC="$(CC)" \
	            CFLAGS="-fPIC -O2 -ffreestanding -fno-builtin -D_POSIX_THREAD_SAFE_FUNCTIONS=1 -isystem $(abspath $(SYSROOT_DIR))/usr/include -B$(abspath $(SYSROOT_DIR))/usr/lib" \
	            LDFLAGS="-nostdlib -L$(abspath $(SYSROOT_DIR))/usr/lib -B$(abspath $(SYSROOT_DIR))/usr/lib"; \
	    fi && \
	    $(MAKE) -C modules && \
	    $(MAKE) -C src && \
	    $(LD) -shared -soname libX11.so.6 -o $(abspath $(SYSROOT_DIR))/usr/lib/libX11.so.6 --whole-archive src/.libs/libX11.a --no-whole-archive && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libX11.so.6 $(abspath $(SYSROOT_DIR))/usr/lib/libX11.so && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libX11.so.6 $(abspath $(ROOTFS_DIR))/lib/libX11.so.6 && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libX11.so.6 $(abspath $(ROOTFS_DIR))/lib/libX11.so && \
	    cp -r $(abspath third_party/libX11/include/X11)/* $(abspath $(SYSROOT_DIR))/usr/include/X11/ && \
	    cp -f $(abspath $(BUILD_DIR)/third_party/libX11)/include/X11/XlibConf.h $(abspath $(SYSROOT_DIR))/usr/include/X11/ 2>/dev/null || true && \
	    cp -f $(abspath $(BUILD_DIR)/third_party/libX11)/*.pc $(abspath $(SYSROOT_DIR))/usr/lib/pkgconfig/ 2>/dev/null || true

# ==============================================================================
# libxkbfile Target
# ==============================================================================
$(ROOTFS_DIR)/lib/libxkbfile.so: $(SYSROOT_STAMP) $(ROOTFS_DIR)/lib/libX11.so $(wildcard third_party/libxkbfile/src/*.c)
	@mkdir -p $(BUILD_DIR)/third_party/libxkbfile $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/X11/extensions
	@echo "  [MAKE-LIBXKBFILE] Kompilacja libxkbfile..."
	@cd $(BUILD_DIR)/third_party/libxkbfile && \
	    touch config.h && \
	    rm -f *.o && \
	    for src in $(abspath third_party/libxkbfile)/src/*.c; do \
	        $(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DHAVE_STRCASECMP=1 -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath third_party/libxkbfile)/include -I$(abspath third_party/libxkbfile)/include/X11/extensions -I$(abspath third_party/libxkbfile)/src -I$(abspath $(BUILD_DIR)/third_party/libxkbfile) -c "$$src" -o "$$(basename $$src .c).o" || exit 1; \
	    done && \
	    $(LD) -shared -soname libxkbfile.so.1 -o $(abspath $(SYSROOT_DIR))/usr/lib/libxkbfile.so.1 *.o && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libxkbfile.so.1 $(abspath $(SYSROOT_DIR))/usr/lib/libxkbfile.so && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libxkbfile.so.1 $(abspath $(ROOTFS_DIR))/lib/libxkbfile.so.1 && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libxkbfile.so.1 $(abspath $(ROOTFS_DIR))/lib/libxkbfile.so && \
	    cp -f $(abspath third_party/libxkbfile)/include/X11/extensions/*.h $(abspath $(SYSROOT_DIR))/usr/include/X11/extensions/ 2>/dev/null || true

# ==============================================================================
# libfontenc Target
# ==============================================================================
$(ROOTFS_DIR)/lib/libfontenc.so: $(SYSROOT_STAMP) $(LIBZ_A) $(wildcard third_party/libfontenc/src/*.c)
	@mkdir -p $(BUILD_DIR)/third_party/libfontenc $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/X11/fonts
	@echo "  [MAKE-LIBFONTENC] Kompilacja libfontenc..."
	@cd $(BUILD_DIR)/third_party/libfontenc && \
	    touch config.h && \
	    rm -f *.o && \
	    for src in $(abspath third_party/libfontenc)/src/*.c; do \
	        $(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DHAVE_REALLOCARRAY=1 -DFONT_ENCODINGS_DIRECTORY='"/usr/share/fonts/X11/encodings/encodings.dir"' -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath third_party/libfontenc)/include -I$(abspath third_party/libfontenc)/src -I$(abspath $(BUILD_DIR)/third_party/libfontenc) -c "$$src" -o "$$(basename $$src .c).o" || exit 1; \
	    done && \
	    $(LD) -shared -soname libfontenc.so.1 -o $(abspath $(SYSROOT_DIR))/usr/lib/libfontenc.so.1 *.o && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libfontenc.so.1 $(abspath $(SYSROOT_DIR))/usr/lib/libfontenc.so && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libfontenc.so.1 $(abspath $(ROOTFS_DIR))/lib/libfontenc.so.1 && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libfontenc.so.1 $(abspath $(ROOTFS_DIR))/lib/libfontenc.so && \
	    cp -f $(abspath third_party/libfontenc)/include/X11/fonts/*.h $(abspath $(SYSROOT_DIR))/usr/include/X11/fonts/ 2>/dev/null || true && \
	    printf "prefix=/usr\nexec_prefix=\$${prefix}\nlibdir=\$${exec_prefix}/lib\nincludedir=\$${prefix}/include\n\nName: fontenc\nDescription: The fontenc Library\nVersion: 1.1.4\nLibs: -L\$${libdir} -lfontenc\nCflags: -I\$${includedir}\n" > $(abspath $(SYSROOT_DIR))/usr/lib/pkgconfig/fontenc.pc && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/pkgconfig/fontenc.pc $(abspath $(SYSROOT_DIR))/usr/share/pkgconfig/ 2>/dev/null || true

# ==============================================================================
# libXfont2 Target
# ==============================================================================
third_party/libXfont2/configure: third_party/libXfont2/configure.ac
	@echo "  [PRECONF-LIBXFONT2] Generowanie configure dla libXfont2..."
	@mkdir -p third_party/libXfont2/m4
	@cd third_party/libXfont2 && autoreconf -fi -I ../util-macros -I ../xtrans -I ../font-util -I /opt/homebrew/share/aclocal 2>/dev/null || true

$(ROOTFS_DIR)/lib/libXfont2.so: third_party/libXfont2/configure $(SYSROOT_STAMP) $(ROOTFS_DIR)/lib/libfontenc.so $(LIBZ_A)
	@mkdir -p $(BUILD_DIR)/third_party/libXfont2 $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib
	@echo "  [MAKE-LIBXFONT2] Kompilacja libXfont2..."
	@cd $(BUILD_DIR)/third_party/libXfont2 && \
	    if [ ! -f Makefile ]; then \
	        PKG_CONFIG_LIBDIR="$(abspath $(SYSROOT_DIR))/usr/lib/pkgconfig:$(abspath $(SYSROOT_DIR))/usr/share/pkgconfig" \
	        PKG_CONFIG_PATH="$(abspath $(SYSROOT_DIR))/usr/lib/pkgconfig:$(abspath $(SYSROOT_DIR))/usr/share/pkgconfig" \
	        $(abspath third_party/libXfont2)/configure --host=x86_64-elf --prefix=/usr --enable-shared --disable-static \
	            --disable-freetype --disable-devel-docs \
	            CC="$(CC)" \
	            CFLAGS="-fPIC -O2 -ffreestanding -fno-builtin -isystem $(abspath $(SYSROOT_DIR))/usr/include -B$(abspath $(SYSROOT_DIR))/usr/lib" \
	            LDFLAGS="-nostdlib -L$(abspath $(SYSROOT_DIR))/usr/lib -B$(abspath $(SYSROOT_DIR))/usr/lib -lc" \
	            LIBS="-L$(abspath $(SYSROOT_DIR))/usr/lib -lfontenc -lz -lc"; \
	    fi && \
	    $(MAKE) && \
	    $(MAKE) install DESTDIR="$(abspath $(SYSROOT_DIR))" && \
	    rm -f $(abspath $(SYSROOT_DIR))/usr/lib/*.la && \
	    $(LD) -shared -soname libXfont2.so.2 -o $(abspath $(SYSROOT_DIR))/usr/lib/libXfont2.so.2 --whole-archive .libs/libXfont2.a --no-whole-archive && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXfont2.so.2 $(abspath $(SYSROOT_DIR))/usr/lib/libXfont2.so && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXfont2.so.2 $(abspath $(ROOTFS_DIR))/lib/libXfont2.so.2 && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXfont2.so.2 $(abspath $(ROOTFS_DIR))/lib/libXfont2.so

# ==============================================================================
# libxcvt Target
# ==============================================================================
$(ROOTFS_DIR)/lib/libxcvt.so: $(SYSROOT_STAMP) $(wildcard third_party/libxcvt/lib/*.c)
	@mkdir -p $(BUILD_DIR)/third_party/libxcvt $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/libxcvt
	@echo "  [MAKE-LIBXCVT] Kompilacja libxcvt..."
	@$(CC) -fPIC -O2 -ffreestanding -fno-builtin -isystem $(abspath $(SYSROOT_DIR))/usr/include -Ithird_party/libxcvt/include -Ithird_party/libxcvt/lib -c third_party/libxcvt/lib/libxcvt.c -o $(BUILD_DIR)/third_party/libxcvt/libxcvt.o
	@$(LD) -shared -soname libxcvt.so.0 -o $(SYSROOT_DIR)/usr/lib/libxcvt.so $(BUILD_DIR)/third_party/libxcvt/libxcvt.o
	@cp -f $(SYSROOT_DIR)/usr/lib/libxcvt.so $@
	@cp -f $(SYSROOT_DIR)/usr/lib/libxcvt.so $(ROOTFS_DIR)/lib/libxcvt.so.0
	@cp -f third_party/libxcvt/include/libxcvt/*.h $(SYSROOT_DIR)/usr/include/libxcvt/ 2>/dev/null || true

# ==============================================================================
# libpciaccess Target
# ==============================================================================
$(ROOTFS_DIR)/lib/libpciaccess.so: $(SYSROOT_STAMP) $(wildcard third_party/libpciaccess/src/*.c)
	@mkdir -p $(BUILD_DIR)/third_party/libpciaccess $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include
	@echo "  [MAKE-LIBPCIACCESS] Kompilacja libpciaccess..."
	@cd $(BUILD_DIR)/third_party/libpciaccess && \
	    rm -f *.o && \
	    touch config.h && \
	    for src in common_bridge.c common_iterator.c common_init.c common_interface.c common_capability.c common_device_name.c common_map.c common_vgaarb.c common_io.c linux_sysfs.c linux_devmem.c; do \
	        $(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DHAVE_STDINT_H -DHAVE_INTTYPES_H -DPCIIDS_PATH=\"/usr/share/hwdata\" -D__linux__=1 -I$(abspath third_party/libpciaccess/include) -I$(abspath third_party/libpciaccess/src) -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath $(BUILD_DIR)/third_party/libpciaccess) -c "$(abspath third_party/libpciaccess/src)/$$src" -o "$$(basename $$src .c).o" || true; \
	    done && \
	    $(LD) -shared -soname libpciaccess.so.0 -o $(abspath $(SYSROOT_DIR))/usr/lib/libpciaccess.so *.o && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libpciaccess.so $(abspath $(ROOTFS_DIR))/lib/libpciaccess.so && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libpciaccess.so $(abspath $(ROOTFS_DIR))/lib/libpciaccess.so.0 && \
	    cp -f $(abspath third_party/libpciaccess/include/pciaccess.h) $(abspath $(SYSROOT_DIR))/usr/include/

# ==============================================================================
# libpixman-1 Target
# ==============================================================================
$(ROOTFS_DIR)/lib/libpixman-1.so: $(SYSROOT_STAMP) $(LIBM_SO) $(wildcard third_party/pixman/pixman/*.c)
	@mkdir -p $(BUILD_DIR)/third_party/pixman $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/pixman-1
	@echo "  [MAKE-PIXMAN] Kompilacja pixman..."
	@cd $(BUILD_DIR)/third_party/pixman && \
	    rm -f *.o && \
	    printf '#ifndef CONFIG_H\n#define CONFIG_H\n#define PACKAGE "pixman"\n#define PACKAGE_VERSION "0.43.4"\n#define PIXMAN_NO_TLS 1\n#define HAVE_POSIX_MEMALIGN 1\n#define HAVE_SIGACTION 1\n#define HAVE_ALARM 1\n#define HAVE_MPROTECT 1\n#define HAVE_GETPAGESIZE 1\n#define HAVE_MMAP 1\n#define HAVE_GETTIMEOFDAY 1\n#define SIZEOF_LONG 8\n#endif\n' > config.h && \
	    cp config.h pixman-config.h && \
	    printf '#ifndef PIXMAN_VERSION_H\n#define PIXMAN_VERSION_H\n#define PIXMAN_VERSION_MAJOR 0\n#define PIXMAN_VERSION_MINOR 43\n#define PIXMAN_VERSION_MICRO 4\n#define PIXMAN_VERSION_STRING "0.43.4"\n#define PIXMAN_VERSION (PIXMAN_VERSION_MAJOR*10000 + PIXMAN_VERSION_MINOR*100 + PIXMAN_VERSION_MICRO)\n#ifndef PIXMAN_API\n#define PIXMAN_API\n#endif\n#endif\n' > pixman-version.h && \
	    cp pixman-version.h $(SYSROOT_DIR)/usr/include/pixman-1/ && \
	    cp pixman-version.h $(SYSROOT_DIR)/usr/include/ && \
	    for src in pixman.c pixman-access.c pixman-access-accessors.c pixman-arm.c pixman-bits-image.c \
	               pixman-combine32.c pixman-combine-float.c pixman-conical-gradient.c pixman-edge.c \
	               pixman-edge-accessors.c pixman-fast-path.c pixman-filter.c pixman-glyph.c \
	               pixman-general.c pixman-gradient-walker.c pixman-image.c pixman-implementation.c \
	               pixman-linear-gradient.c pixman-matrix.c pixman-mips.c pixman-noop.c pixman-ppc.c \
	               pixman-radial-gradient.c pixman-region16.c pixman-region32.c pixman-region64f.c \
	               pixman-riscv.c pixman-solid-fill.c pixman-timer.c pixman-trap.c pixman-utils.c pixman-x86.c; do \
	        $(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DPIXMAN_NO_TLS=1 -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath third_party/pixman/pixman) -I$(abspath $(BUILD_DIR)/third_party/pixman) -I. -c "$(abspath third_party/pixman/pixman)/$$src" -o "$$(basename $$src .c).o" || exit 1; \
	    done && \
	    $(LD) -shared -soname libpixman-1.so.0 -o $(abspath $(SYSROOT_DIR))/usr/lib/libpixman-1.so *.o && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libpixman-1.so $(abspath $(ROOTFS_DIR))/lib/libpixman-1.so && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libpixman-1.so $(abspath $(ROOTFS_DIR))/lib/libpixman-1.so.0 && \
	    cp -f $(abspath third_party/pixman/pixman)/*.h $(abspath $(SYSROOT_DIR))/usr/include/pixman-1/ 2>/dev/null || true && \
	    cp -f $(abspath third_party/pixman/pixman)/*.h $(abspath $(SYSROOT_DIR))/usr/include/ 2>/dev/null || true

# ==============================================================================
# libdrm Target
# ==============================================================================
$(ROOTFS_DIR)/lib/libdrm.so: $(LIBC_A) $(SYSROOT_STAMP) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/lib/pkgconfig $(SYSROOT_DIR)/usr/include/libdrm
	@echo "  [LD-LIBDRM] $@"
	@$(LD) -shared -soname libdrm.so.2 -o $(SYSROOT_DIR)/usr/lib/libdrm.so $(BUILD_DIR)/libc/src/drm/drm.o
	@cp -f $(SYSROOT_DIR)/usr/lib/libdrm.so $@
	@cp -f $(SYSROOT_DIR)/usr/lib/libdrm.so $(ROOTFS_DIR)/lib/libdrm.so.2
	@printf "prefix=/usr\nexec_prefix=\$${prefix}\nlibdir=\$${exec_prefix}/lib\nincludedir=\$${prefix}/include\n\nName: libdrm\nDescription: Userspace interface to kernel DRM services\nVersion: 2.4.110\nLibs: -L\$${libdir} -ldrm\nCflags: -I\$${includedir} -I\$${includedir}/libdrm\n" > $(SYSROOT_DIR)/usr/lib/pkgconfig/libdrm.pc
	@cp -f compat/linux/include/drm/*.h $(SYSROOT_DIR)/usr/include/ 2>/dev/null || true
	@cp -f compat/linux/include/drm/*.h $(SYSROOT_DIR)/usr/include/libdrm/ 2>/dev/null || true

# ==============================================================================
# libICE Target
# ==============================================================================
$(ROOTFS_DIR)/lib/libICE.so: $(SYSROOT_STAMP) $(LIBC_SO) $(wildcard third_party/libICE/src/*.c)
	@mkdir -p $(BUILD_DIR)/third_party/libICE $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/X11/ICE
	@echo "  [MAKE-LIBICE] Kompilacja libICE..."
	@cd $(BUILD_DIR)/third_party/libICE && \
	    touch config.h && \
	    rm -f *.o && \
	    for f in $(abspath third_party/libICE)/src/*.c; do \
	        $(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DHAVE_ASPRINTF=1 -DICE_t -DTRANS_CLIENT -DTRANS_SERVER -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath third_party/libICE)/include -I$(abspath third_party/libICE)/src -I$(abspath third_party/xtrans) -I$(abspath $(BUILD_DIR)/third_party/libICE) -c "$$f" -o "$$(basename $$f .c).o" || exit 1; \
	    done && \
	    $(LD) -shared -soname libICE.so.6 -o $(abspath $(SYSROOT_DIR))/usr/lib/libICE.so *.o && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libICE.so $(abspath $(ROOTFS_DIR))/lib/libICE.so && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libICE.so $(abspath $(ROOTFS_DIR))/lib/libICE.so.6 && \
	    cp -r $(abspath third_party/libICE)/include/X11/ICE/* $(abspath $(SYSROOT_DIR))/usr/include/X11/ICE/

# ==============================================================================
# libSM Target
# ==============================================================================
$(ROOTFS_DIR)/lib/libSM.so: $(SYSROOT_STAMP) $(ROOTFS_DIR)/lib/libICE.so $(wildcard third_party/libSM/src/*.c)
	@mkdir -p $(BUILD_DIR)/third_party/libSM $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/X11/SM
	@echo "  [MAKE-LIBSM] Kompilacja libSM..."
	@cd $(BUILD_DIR)/third_party/libSM && \
	    touch config.h && \
	    rm -f *.o && \
	    for f in $(abspath third_party/libSM)/src/*.c; do \
	        $(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DHAVE_ASPRINTF=1 -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath third_party/libSM)/include -I$(abspath third_party/libSM)/src -I$(abspath third_party/libICE)/include -I$(abspath $(BUILD_DIR)/third_party/libSM) -c "$$f" -o "$$(basename $$f .c).o" || exit 1; \
	    done && \
	    $(LD) -shared -soname libSM.so.6 -o $(abspath $(SYSROOT_DIR))/usr/lib/libSM.so *.o && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libSM.so $(abspath $(ROOTFS_DIR))/lib/libSM.so && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libSM.so $(abspath $(ROOTFS_DIR))/lib/libSM.so.6 && \
	    cp -r $(abspath third_party/libSM)/include/X11/SM/* $(abspath $(SYSROOT_DIR))/usr/include/X11/SM/

# ==============================================================================
# libXpm Target
# ==============================================================================
$(ROOTFS_DIR)/lib/libXpm.so: $(SYSROOT_STAMP) $(ROOTFS_DIR)/lib/libX11.so $(wildcard third_party/libXpm/src/*.c)
	@mkdir -p $(BUILD_DIR)/third_party/libXpm $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/X11
	@echo "  [MAKE-LIBXPM] Kompilacja libXpm..."
	@cd $(BUILD_DIR)/third_party/libXpm && \
	    touch config.h && \
	    rm -f *.o && \
	    for f in $(abspath third_party/libXpm)/src/*.c; do \
	        $(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DHAVE_STRCASECMP=1 -DHAVE_ASPRINTF=1 -DHAS_GETCWD=1 -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath third_party/libXpm)/include -I$(abspath third_party/libXpm)/include/X11 -I$(abspath third_party/libXpm)/src -I$(abspath $(BUILD_DIR)/third_party/libXpm) -c "$$f" -o "$$(basename $$f .c).o" || exit 1; \
	    done && \
	    $(LD) -shared -soname libXpm.so.4 -o $(abspath $(SYSROOT_DIR))/usr/lib/libXpm.so *.o && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXpm.so $(abspath $(ROOTFS_DIR))/lib/libXpm.so && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXpm.so $(abspath $(ROOTFS_DIR))/lib/libXpm.so.4 && \
	    cp -r $(abspath third_party/libXpm)/include/X11/* $(abspath $(SYSROOT_DIR))/usr/include/X11/ && \
	    printf "prefix=/usr\nexec_prefix=\$${prefix}\nlibdir=\$${exec_prefix}/lib\nincludedir=\$${prefix}/include\n\nName: Xpm\nDescription: X Pixmap Library\nVersion: 3.5.17\nLibs: -L\$${libdir} -lXpm\nCflags: -I\$${includedir}\n" > $(abspath $(SYSROOT_DIR))/usr/lib/pkgconfig/xpm.pc && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/pkgconfig/xpm.pc $(abspath $(SYSROOT_DIR))/usr/share/pkgconfig/ 2>/dev/null || true

# ==============================================================================
# libXext Target
# ==============================================================================
$(ROOTFS_DIR)/lib/libXext.so: $(SYSROOT_STAMP) $(ROOTFS_DIR)/lib/libX11.so $(wildcard third_party/libXext/src/*.c)
	@mkdir -p $(BUILD_DIR)/third_party/libXext $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/X11/extensions
	@echo "  [MAKE-LIBXEXT] Kompilacja libXext..."
	@cd $(BUILD_DIR)/third_party/libXext && \
	    touch config.h && \
	    rm -f *.o && \
	    for f in $(abspath third_party/libXext)/src/*.c; do \
	        $(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath third_party/libXext)/include -I$(abspath third_party/libXext)/include/X11/extensions -I$(abspath third_party/libXext)/src -I$(abspath $(BUILD_DIR)/third_party/libXext) -c "$$f" -o "$$(basename $$f .c).o" || exit 1; \
	    done && \
	    $(LD) -shared -soname libXext.so.6 -o $(abspath $(SYSROOT_DIR))/usr/lib/libXext.so *.o && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXext.so $(abspath $(ROOTFS_DIR))/lib/libXext.so && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXext.so $(abspath $(ROOTFS_DIR))/lib/libXext.so.6 && \
	    cp -r $(abspath third_party/libXext)/include/X11/extensions/* $(abspath $(SYSROOT_DIR))/usr/include/X11/extensions/ && \
	    printf "prefix=/usr\nexec_prefix=\$${prefix}\nlibdir=\$${exec_prefix}/lib\nincludedir=\$${prefix}/include\n\nName: Xext\nDescription: Misc X Extension Library\nVersion: 1.3.6\nLibs: -L\$${libdir} -lXext\nCflags: -I\$${includedir}\n" > $(abspath $(SYSROOT_DIR))/usr/lib/pkgconfig/xext.pc && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/pkgconfig/xext.pc $(abspath $(SYSROOT_DIR))/usr/share/pkgconfig/ 2>/dev/null || true

# ==============================================================================
# libXt Target
# ==============================================================================
$(ROOTFS_DIR)/lib/libXt.so: $(SYSROOT_STAMP) $(ROOTFS_DIR)/lib/libX11.so $(ROOTFS_DIR)/lib/libSM.so $(ROOTFS_DIR)/lib/libICE.so $(wildcard third_party/libXt/src/*.c)
	@mkdir -p $(BUILD_DIR)/third_party/libXt $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/X11
	@echo "  [MAKE-LIBXT] Kompilacja libXt..."
	@clang third_party/libXt/util/makestrs.c -o $(BUILD_DIR)/third_party/libXt/makestrs
	@$(BUILD_DIR)/third_party/libXt/makestrs -i $(abspath third_party/libXt) < third_party/libXt/util/string.list > third_party/libXt/src/StringDefs.c
	@cd $(BUILD_DIR)/third_party/libXt && \
	    touch config.h && \
	    rm -f *.o && \
	    for f in $(abspath third_party/libXt)/src/*.c; do \
	        $(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DHAVE_ASPRINTF=1 -DHAVE_REALLOCARRAY=1 -DHAS_GETCWD=1 -DXTHREADS -D_POSIX_THREAD_SAFE_FUNCTIONS -include sys/select.h -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath third_party/libXt)/include -I$(abspath third_party/libXt)/include/X11 -I$(abspath third_party/libXt)/src -I$(abspath $(BUILD_DIR)/third_party/libXt) -c "$$f" -o "$$(basename $$f .c).o" || exit 1; \
	    done && \
	    $(LD) -shared -soname libXt.so.6 -o $(abspath $(SYSROOT_DIR))/usr/lib/libXt.so *.o && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXt.so $(abspath $(ROOTFS_DIR))/lib/libXt.so && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXt.so $(abspath $(ROOTFS_DIR))/lib/libXt.so.6 && \
	    cp -r $(abspath third_party/libXt)/include/X11/* $(abspath $(SYSROOT_DIR))/usr/include/X11/

# ==============================================================================
# libXmu Target
# ==============================================================================
$(ROOTFS_DIR)/lib/libXmu.so: $(SYSROOT_STAMP) $(ROOTFS_DIR)/lib/libXt.so $(ROOTFS_DIR)/lib/libXext.so $(wildcard third_party/libXmu/src/*.c)
	@mkdir -p $(BUILD_DIR)/third_party/libXmu $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/X11/Xmu
	@echo "  [MAKE-LIBXMU] Kompilacja libXmu..."
	@cd $(BUILD_DIR)/third_party/libXmu && \
	    touch config.h && \
	    rm -f *.o && \
	    for f in $(abspath third_party/libXmu)/src/*.c; do \
	        $(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DHAVE_ASPRINTF=1 -DHAVE_REALLOCARRAY=1 -DHAS_GETCWD=1 -DXTHREADS -D_POSIX_THREAD_SAFE_FUNCTIONS -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath third_party/libXmu)/include -I$(abspath third_party/libXmu)/include/X11/Xmu -I$(abspath third_party/libXmu)/src -I$(abspath third_party/libXt)/include -I$(abspath third_party/libXt)/include/X11 -I$(abspath $(BUILD_DIR)/third_party/libXmu) -c "$$f" -o "$$(basename $$f .c).o" || exit 1; \
	    done && \
	    $(LD) -shared -soname libXmu.so.6 -o $(abspath $(SYSROOT_DIR))/usr/lib/libXmu.so *.o && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXmu.so $(abspath $(ROOTFS_DIR))/lib/libXmu.so && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXmu.so $(abspath $(ROOTFS_DIR))/lib/libXmu.so.6 && \
	    cp -r $(abspath third_party/libXmu)/include/X11/Xmu/* $(abspath $(SYSROOT_DIR))/usr/include/X11/Xmu/

# ==============================================================================
# libXaw Target
# ==============================================================================
$(ROOTFS_DIR)/lib/libXaw.so: $(SYSROOT_STAMP) $(ROOTFS_DIR)/lib/libXmu.so $(ROOTFS_DIR)/lib/libXpm.so $(wildcard third_party/libXaw/src/*.c)
	@mkdir -p $(BUILD_DIR)/third_party/libXaw $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/X11/Xaw
	@echo "  [MAKE-LIBXAW] Kompilacja libXaw..."
	@cd $(BUILD_DIR)/third_party/libXaw && \
	    touch config.h && \
	    rm -f *.o && \
	    for f in $(abspath third_party/libXaw)/src/*.c; do \
	        $(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DHAVE_ASPRINTF=1 -DHAVE_REALLOCARRAY=1 -DHAS_GETCWD=1 -DHAVE_WCHAR_H=1 -DHAVE_WCTYPE_H=1 -DXTHREADS -D_POSIX_THREAD_SAFE_FUNCTIONS -include sys/select.h -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(abspath third_party/libXaw)/include -I$(abspath third_party/libXaw)/include/X11/Xaw -I$(abspath third_party/libXaw)/src -I$(abspath third_party/libXpm)/include -I$(abspath third_party/libXmu)/include -I$(abspath third_party/libXt)/include -I$(abspath third_party/libXt)/include/X11 -I$(abspath $(BUILD_DIR)/third_party/libXaw) -c "$$f" -o "$$(basename $$f .c).o" || exit 1; \
	    done && \
	    $(LD) -shared -soname libXaw7.so.7 -o $(abspath $(SYSROOT_DIR))/usr/lib/libXaw7.so *.o && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXaw7.so $(abspath $(SYSROOT_DIR))/usr/lib/libXaw.so && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXaw7.so $(abspath $(ROOTFS_DIR))/lib/libXaw.so && \
	    cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXaw7.so $(abspath $(ROOTFS_DIR))/lib/libXaw.so.7 && \
	    cp -r $(abspath third_party/libXaw)/include/X11/Xaw/* $(abspath $(SYSROOT_DIR))/usr/include/X11/Xaw/

# ==============================================================================
# Official Upstream X11 Terminal Emulator (xterm)
# ==============================================================================
$(ROOTFS_DIR)/bin/xterm: $(ROOTFS_DIR)/lib/libXaw.so $(ROOTFS_DIR)/lib/libXmu.so $(ROOTFS_DIR)/lib/libXt.so $(ROOTFS_DIR)/lib/libXpm.so $(ROOTFS_DIR)/lib/libXext.so $(ROOTFS_DIR)/lib/libSM.so $(ROOTFS_DIR)/lib/libICE.so $(ROOTFS_DIR)/lib/libX11.so $(LIBNCURSES_A) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin
	@echo "  [MAKE-XTERM] Kompilacja oficjalnego upstream xterm..."
	@cd third_party/xterm && \
	    if [ ! -f Makefile ]; then \
	        CC="$(CC) -nostdlib $(abspath $(SYSROOT_DIR))/usr/lib/crt0.o" \
	        CPP="x86_64-elf-cpp -isystem $(abspath $(SYSROOT_DIR))/usr/include" \
	        CFLAGS="-O2 -ffreestanding -isystem $(abspath $(SYSROOT_DIR))/usr/include -DUSE_SYSV_PGRP=1" \
	        LDFLAGS="-L$(abspath $(ROOTFS_DIR))/lib -L$(abspath $(SYSROOT_DIR))/usr/lib -lXaw7 -lXmu -lXt -lSM -lICE -lXpm -lXext -lX11 -lxcb -lXau -lXdmcp -lncurses -lm -lc" \
	        ./configure --host=x86_64-elf --without-xinerama --disable-imake --disable-setuid --disable-setgid --disable-freetype --without-pcre --without-pcre2 --disable-luit; \
	    fi && \
	    $(MAKE) EXTRA_CFLAGS="-DUSE_SYSV_PGRP=1 -DHAVE_GRANTPT_PTY_ISATTY=1" && \
	    cp -f xterm $(abspath $(ROOTFS_DIR))/bin/xterm && \
	    cp -f resize $(abspath $(ROOTFS_DIR))/bin/resize 2>/dev/null || true
	@chmod +x $@

.PHONY: third-party
third-party: $(LIBNCURSES_A) $(LIBZ_A) $(ROOTFS_DIR)/bin/nano $(ROOTFS_DIR)/bin/file $(MAGIC_DB) $(ROOTFS_DIR)/bin/zsh $(ROOTFS_DIR)/bin/fastfetch $(ROOTFS_DIR)/bin/git $(ALL_ROOTFS_SOS) $(ROOTFS_DIR)/bin/xterm
