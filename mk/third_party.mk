# SzpontOS Third-Party Packages & X11 Libraries Build Rules
# Included by root Makefile

# ==============================================================================
# Shared Libraries Required in Rootfs
# ==============================================================================
ALL_ROOTFS_SOS := \
	$(LIBC_SO) $(LIBM_SO) \
	$(LIBSTDCXX_SO) \
	$(ROOTFS_DIR)/lib/libz.so \
	$(ROOTFS_DIR)/lib/libdrm.so \
	$(ROOTFS_DIR)/lib/libgbm.so \
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
	$(ROOTFS_DIR)/lib/libXaw.so \
	$(ROOTFS_DIR)/lib/libcrypto.so \
	$(ROOTFS_DIR)/lib/libssl.so \
	$(ROOTFS_DIR)/lib/libcurl.so

# ==============================================================================
# GNU libstdc++-v3 (Out-of-tree build)
# ==============================================================================
$(LIBSTDCXX_SO): scripts/build_libstdcxx.py | $(SYSROOT_STAMP) $(ROOTFS_DIR) $(LIBC_SO) $(LIBM_SO)
	@echo "  [BUILD-LIBSTDCXX] Building GNU libstdc++-v3 runtime & shared library..."
	@python3 scripts/build_libstdcxx.py third_party/libstdc++ $(BUILD_DIR)/third_party/libstdc++ $(SYSROOT_DIR) $(ROOTFS_DIR)

$(LIBSTDCXX_A): $(LIBSTDCXX_SO)

# ==============================================================================
# GNU Ncurses (Cross-compiled via original Autotools)
# ==============================================================================
$(NCURSES_BUILD_DIR)/Makefile: third_party/ncurses/configure | $(SYSROOT_STAMP) $(NCURSES_BUILD_DIR)
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

$(LIBNCURSES_A): $(NCURSES_BUILD_DIR)/Makefile | $(LIBC_A) $(CRT0_O)
	@echo "  [GEN-NCURSES-FALLBACKS] Generowanie wbudowanych terminali (xterm-256color, vt100)..."
	@python3 scripts/generate_ncurses_fallbacks.py $(NCURSES_BUILD_DIR)/ncurses/fallback.c
	@sed -i '' 's/mkdir $$@/mkdir -p $$@/g' $(NCURSES_BUILD_DIR)/ncurses/Makefile 2>/dev/null || sed -i 's/mkdir $$@/mkdir -p $$@/g' $(NCURSES_BUILD_DIR)/ncurses/Makefile 2>/dev/null || true
	@echo "  [MAKE-NCURSES] Kompilacja GNU Ncurses (-j$(JOBS))..."
	@$(MAKE) -j$(JOBS) -C $(NCURSES_BUILD_DIR)/include
	@$(MAKE) -j$(JOBS) -C $(NCURSES_BUILD_DIR)/ncurses
	@mkdir -p $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include $(ROOTFS_DIR)/lib
	@cp $(NCURSES_BUILD_DIR)/lib/libncurses.a $(SYSROOT_DIR)/usr/lib/
	@cp $(NCURSES_BUILD_DIR)/include/*.h $(SYSROOT_DIR)/usr/include/ 2>/dev/null || true
	@cp third_party/ncurses/include/curses.h $(SYSROOT_DIR)/usr/include/ 2>/dev/null || true
	@cp $(SYSROOT_DIR)/usr/include/curses.h $(SYSROOT_DIR)/usr/include/ncurses.h 2>/dev/null || true

# ==============================================================================
# GNU nano (Direct Parallel Compilation against libc & libncurses)
# ==============================================================================
$(NANO_BUILD_DIR)/revision.h: | $(NANO_BUILD_DIR)
	@mkdir -p $(NANO_BUILD_DIR)
	@echo '#define REVISION "GNU nano 9.2.4"' > $@

$(NANO_BUILD_DIR):
	@mkdir -p $@

NANO_SRCS := $(wildcard third_party/nano/src/*.c)
NANO_OBJS := $(patsubst third_party/nano/src/%.c,$(NANO_BUILD_DIR)/%.o,$(NANO_SRCS))

$(NANO_BUILD_DIR)/%.o: third_party/nano/src/%.c $(NANO_BUILD_DIR)/revision.h | $(SYSROOT_STAMP) $(NANO_BUILD_DIR) $(LIBNCURSES_A)
	@echo "  [CC-NANO] $<"
	@$(CC) $(USER_CFLAGS) -nostdlib -I$(NANO_BUILD_DIR) -Ithird_party/nano/src -isystem $(abspath $(SYSROOT_DIR))/usr/include \
	    -DPACKAGE=\"nano\" -DVERSION=\"7.2\" -DENABLE_UTF8=1 -DENABLE_COLOR=1 -DENABLE_NANORC=1 \
	    -DENABLE_MULTIBUFFER=1 -DHAVE_NCURSES_H=1 -DHAVE_CURSES_H=1 -DHAVE_LIMITS_H=1 -DHAVE_SYS_PARAM_H=1 \
	    -DHAVE_TERMIOS_H=1 -DHAVE_UNISTD_H=1 -DHAVE_FCNTL_H=1 -DHAVE_DIRENT_H=1 -DHAVE_PWD_H=1 -DHAVE_GRP_H=1 \
	    -DHAVE_GETOPT_H=1 -DHAVE_GETOPT_LONG=1 -DHAVE_SIGACTION=1 -DHAVE_SIGNAL_H=1 \
	    -DNANO_REG_EXTENDED=REG_EXTENDED -DSYSCONFDIR=\"/etc\" \
	    -c $< -o $@

$(ROOTFS_DIR)/bin/nano: $(NANO_OBJS) $(LIBNCURSES_A) | $(ROOTFS_DIR) $(LIBC_SO) $(LIBM_SO) $(CRT0_O)
	@mkdir -p $(ROOTFS_DIR)/bin
	@echo "  [LD-NANO] $@"
	@$(CC) $(USER_CFLAGS) -nostdlib $(CRT0_O) $(NANO_OBJS) -L$(ROOTFS_DIR)/lib -L$(abspath $(SYSROOT_DIR))/usr/lib -lncurses -lm -lc -o $@
	@chmod +x $@

# ==============================================================================
# GNU file & libmagic (Autotools cross-compile)
# ==============================================================================
third_party/file/configure:
	@echo "  [PRECONF-FILE] Generowanie configure dla GNU file..."
	@cd third_party/file && autoreconf -fi || true

$(FILE_BUILD_DIR)/Makefile: third_party/file/configure | $(SYSROOT_STAMP) $(FILE_BUILD_DIR)
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

$(ROOTFS_DIR)/bin/file: $(FILE_BUILD_DIR)/Makefile | $(LIBC_A) $(CRT0_O) $(LIBM_A) $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(ROOTFS_DIR)/lib
	@echo "  [MAKE-FILE] Kompilacja GNU file (-j$(JOBS))..."
	@$(MAKE) -j$(JOBS) -C $(FILE_BUILD_DIR)/src file_LDADD="$(abspath $(SYSROOT_DIR))/usr/lib/crt0.o libmagic.la -lm"
	@cp $(FILE_BUILD_DIR)/src/file $@

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
	@mkdir -p third_party/zsh/Doc && touch third_party/zsh/Doc/help.txt
	@cd third_party/zsh && (./Util/preconfig || (autoconf && autoheader && echo > stamp-h.in))

$(ZSH_BUILD_DIR)/Makefile: third_party/zsh/configure | $(LIBNCURSES_A) $(SYSROOT_STAMP) $(ZSH_BUILD_DIR)
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

$(ROOTFS_DIR)/bin/zsh: $(ZSH_BUILD_DIR)/Makefile | $(LIBNCURSES_A) $(LIBC_A) $(CRT0_O) $(LIBM_A) $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin
	@echo "  [MAKE-ZSH] Kompilacja powłoki Zsh (-j$(JOBS))..."
	@$(MAKE) -j$(JOBS) -C $(ZSH_BUILD_DIR)/Src zsh
	@cp $(ZSH_BUILD_DIR)/Src/zsh $@

# ==============================================================================
# Fastfetch (CMake cross-compile)
# ==============================================================================
$(FASTFETCH_BUILD_DIR)/Makefile: third_party/fastfetch/CMakeLists.txt | $(SYSROOT_STAMP) $(LIBC_A) $(CRT0_O) $(LIBM_A) $(LIBDL_A) $(FASTFETCH_BUILD_DIR)
	@echo "  [CONF-FASTFETCH] Konfiguracja Fastfetch (CMake cross-compile)..."
	@cd $(FASTFETCH_BUILD_DIR) && \
	cmake ../../../third_party/fastfetch \
	    -DCMAKE_SYSTEM_NAME=Linux \
	    -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
	    -DCMAKE_C_COMPILER="$(shell which -a $(CC) 2>/dev/null | grep -v '\.bear' | head -n 1 || which $(CC) 2>/dev/null || echo $(CC))" \
	    -DCMAKE_C_FLAGS="--sysroot=$(abspath $(SYSROOT_DIR)) -isystem $(abspath $(SYSROOT_DIR))/usr/include -D__linux__=1 -ffreestanding -fno-builtin -O2" \
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

$(ROOTFS_DIR)/bin/fastfetch: $(FASTFETCH_BUILD_DIR)/Makefile | $(SYSROOT_STAMP) $(LIBC_A) $(CRT0_O) $(LIBM_A) $(LIBDL_A) $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin
	@echo "  [MAKE-FASTFETCH] Kompilacja narzędzia Fastfetch (-j$(JOBS))..."
	@$(MAKE) -j$(JOBS) -C $(FASTFETCH_BUILD_DIR) fastfetch
	@cp $(FASTFETCH_BUILD_DIR)/fastfetch $@

# ==============================================================================
# zlib (Cross-compiled via original Makefile)
# ==============================================================================
ZLIB_SRCS := $(wildcard third_party/zlib/*.c)
ZLIB_OBJS := $(patsubst third_party/zlib/%.c,$(ZLIB_BUILD_DIR)/%.o,$(ZLIB_SRCS))

$(ZLIB_BUILD_DIR):
	@mkdir -p $@

$(ZLIB_BUILD_DIR)/%.o: third_party/zlib/%.c | $(SYSROOT_STAMP) $(ZLIB_BUILD_DIR)
	@echo "  [CC-ZLIB] $<"
	@$(CC) $(USER_CFLAGS) -DZ_HAVE_UNISTD_H=1 -DHAVE_UNISTD_H=1 -Ithird_party/zlib -c $< -o $@

$(SYSROOT_DIR)/usr/include/zlib.h: third_party/zlib/zlib.h third_party/zlib/zconf.h | $(SYSROOT_STAMP)
	@mkdir -p $(SYSROOT_DIR)/usr/include
	@cp -f third_party/zlib/zlib.h third_party/zlib/zconf.h $(SYSROOT_DIR)/usr/include/

$(LIBZ_A): $(ZLIB_OBJS) | $(SYSROOT_STAMP) $(ROOTFS_DIR)
	@mkdir -p $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include $(ROOTFS_DIR)/lib
	@echo "  [AR-ZLIB] $@"
	@$(AR) rcs $@ $(ZLIB_OBJS)
	@echo "  [LD-ZLIB] $(ROOTFS_DIR)/lib/libz.so"
	@$(LD) -shared -soname libz.so.1 -o $(ROOTFS_DIR)/lib/libz.so.1.3.1 $(ZLIB_OBJS) -L$(abspath $(SYSROOT_DIR))/usr/lib -L$(ROOTFS_DIR)/lib -lc
	@ln -sf libz.so.1.3.1 $(ROOTFS_DIR)/lib/libz.so.1
	@ln -sf libz.so.1.3.1 $(ROOTFS_DIR)/lib/libz.so
	@cp -a $(ROOTFS_DIR)/lib/libz.so* $(SYSROOT_DIR)/usr/lib/
	@cp -f third_party/zlib/zlib.h third_party/zlib/zconf.h $(SYSROOT_DIR)/usr/include/

$(ROOTFS_DIR)/lib/libz.so: $(LIBZ_A)

# ==============================================================================
# Git (Libre-WD-40 cross-compile with OpenSSL & cURL)
# ==============================================================================
$(ROOTFS_DIR)/bin/git: $(LIBZ_A) $(OPENSSL_STAMP) $(ROOTFS_DIR)/bin/curl | $(ROOTFS_DIR)/etc/ssl/cert.pem $(SYSROOT_STAMP) $(LIBC_A) $(CRT0_O) $(LIBM_A) $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin $(ROOTFS_DIR)/usr/libexec/git-core $(SYSROOT_DIR)/usr/libexec/git-core
	@echo "  [MAKE-GIT] Kompilacja narzędzia Git (-j$(JOBS)) z obsługą cURL i OpenSSL..."
	@$(MAKE) -j$(JOBS) -C third_party/git -I $(abspath userland/config/git) \
	    CC="$(CC)" \
	    AR="$(AR)" \
	    RANLIB="$(RANLIB)" \
	    CFLAGS="-O2 -ffreestanding -fno-builtin -isystem $(abspath $(SYSROOT_DIR))/usr/include -B$(abspath $(SYSROOT_DIR))/usr/lib" \
	    LDFLAGS="-nostdlib -L$(abspath $(SYSROOT_DIR))/usr/lib -B$(abspath $(SYSROOT_DIR))/usr/lib $(abspath $(SYSROOT_DIR))/usr/lib/crt0.o" \
	    uname_S=Linux uname_M=x86_64 \
	    git git-remote-http git-http-fetch
	@cp third_party/git/git $@
	@for bin in git-remote-http git-http-fetch; do \
	    if [ -f third_party/git/$$bin ]; then \
	        rm -f $(ROOTFS_DIR)/bin/$$bin $(ROOTFS_DIR)/usr/libexec/git-core/$$bin; \
	        cp -f third_party/git/$$bin $(ROOTFS_DIR)/bin/$$bin; \
	        cp -f third_party/git/$$bin $(ROOTFS_DIR)/usr/libexec/git-core/$$bin; \
	    fi; \
	done
	@rm -f $(ROOTFS_DIR)/bin/git-remote-https $(ROOTFS_DIR)/bin/git-remote-ftp $(ROOTFS_DIR)/bin/git-remote-ftps
	@ln -sf git-remote-http $(ROOTFS_DIR)/bin/git-remote-https
	@ln -sf git-remote-http $(ROOTFS_DIR)/bin/git-remote-ftp
	@ln -sf git-remote-http $(ROOTFS_DIR)/bin/git-remote-ftps
	@rm -f $(ROOTFS_DIR)/usr/libexec/git-core/git-remote-https $(ROOTFS_DIR)/usr/libexec/git-core/git-remote-ftp
	@ln -sf git-remote-http $(ROOTFS_DIR)/usr/libexec/git-core/git-remote-https
	@ln -sf git-remote-http $(ROOTFS_DIR)/usr/libexec/git-core/git-remote-ftp

# ==============================================================================
# X11 Headers and Protocol Specifications (xorgproto & xtrans)
# ==============================================================================
$(SYSROOT_DIR)/usr/include/X11/X.h: | $(SYSROOT_STAMP)
	@mkdir -p $(SYSROOT_DIR)/usr/include/X11 $(SYSROOT_DIR)/usr/include/X11/extensions
	@cp -rf third_party/xorgproto/include/X11/* $(SYSROOT_DIR)/usr/include/X11/ 2>/dev/null || true
	@cp -rf third_party/xtrans/Xtrans*.h $(SYSROOT_DIR)/usr/include/X11/ 2>/dev/null || true

# ==============================================================================
# libXau Target
# ==============================================================================
XAU_SRCS := $(filter-out %Autest.c, $(wildcard third_party/libXau/Au*.c))
XAU_OBJS := $(patsubst third_party/libXau/%.c, $(BUILD_DIR)/third_party/libXau/%.o, $(XAU_SRCS))

$(BUILD_DIR)/third_party/libXau:
	@mkdir -p $@

$(BUILD_DIR)/third_party/libXau/config.h: | $(BUILD_DIR)/third_party/libXau
	@touch $@

$(BUILD_DIR)/third_party/libXau/%.o: third_party/libXau/%.c | $(SYSROOT_STAMP) $(BUILD_DIR)/third_party/libXau $(BUILD_DIR)/third_party/libXau/config.h
	@echo "  [CC-LIBXAU] $<"
	@$(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -isystem $(abspath $(SYSROOT_DIR))/usr/include -Ithird_party/libXau/include -I$(BUILD_DIR)/third_party/libXau -c $< -o $@

$(ROOTFS_DIR)/lib/libXau.so: $(XAU_OBJS) | $(SYSROOT_STAMP) $(ROOTFS_DIR)
	@mkdir -p $(BUILD_DIR)/third_party/libXau $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/X11
	@echo "  [LD-LIBXAU] $@"
	@$(LD) -shared -soname libXau.so.6 -o $(SYSROOT_DIR)/usr/lib/libXau.so.6 $(XAU_OBJS) -L$(abspath $(SYSROOT_DIR))/usr/lib -lc
	@ln -sf libXau.so.6 $(SYSROOT_DIR)/usr/lib/libXau.so
	@cp -f $(SYSROOT_DIR)/usr/lib/libXau.so.6 $(ROOTFS_DIR)/lib/libXau.so.6
	@ln -sf libXau.so.6 $(ROOTFS_DIR)/lib/libXau.so
	@cp -r third_party/libXau/include/X11/* $(SYSROOT_DIR)/usr/include/X11/

# ==============================================================================
# libXdmcp Target
# ==============================================================================
XDMCP_SRCS := $(wildcard third_party/libXdmcp/*.c)
XDMCP_OBJS := $(patsubst third_party/libXdmcp/%.c, $(BUILD_DIR)/third_party/libXdmcp/%.o, $(XDMCP_SRCS))

$(BUILD_DIR)/third_party/libXdmcp:
	@mkdir -p $@

$(BUILD_DIR)/third_party/libXdmcp/config.h: | $(BUILD_DIR)/third_party/libXdmcp
	@touch $@

$(BUILD_DIR)/third_party/libXdmcp/%.o: third_party/libXdmcp/%.c | $(SYSROOT_STAMP) $(BUILD_DIR)/third_party/libXdmcp $(BUILD_DIR)/third_party/libXdmcp/config.h
	@echo "  [CC-LIBXDMCP] $<"
	@$(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DHASXDMAUTH=1 -isystem $(abspath $(SYSROOT_DIR))/usr/include -Ithird_party/libXdmcp/include -I$(BUILD_DIR)/third_party/libXdmcp -c $< -o $@

$(ROOTFS_DIR)/lib/libXdmcp.so: $(XDMCP_OBJS) | $(SYSROOT_STAMP) $(ROOTFS_DIR)
	@mkdir -p $(BUILD_DIR)/third_party/libXdmcp $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/X11
	@echo "  [LD-LIBXDMCP] $@"
	@$(LD) -shared -soname libXdmcp.so.6 -o $(SYSROOT_DIR)/usr/lib/libXdmcp.so.6 $(XDMCP_OBJS) -L$(abspath $(SYSROOT_DIR))/usr/lib -lc
	@ln -sf libXdmcp.so.6 $(SYSROOT_DIR)/usr/lib/libXdmcp.so
	@cp -f $(SYSROOT_DIR)/usr/lib/libXdmcp.so.6 $(ROOTFS_DIR)/lib/libXdmcp.so.6
	@ln -sf libXdmcp.so.6 $(ROOTFS_DIR)/lib/libXdmcp.so
	@cp -r third_party/libXdmcp/include/X11/* $(SYSROOT_DIR)/usr/include/X11/

# ==============================================================================
# libxcb Target
# ==============================================================================
third_party/libxcb/configure: third_party/libxcb/configure.ac
	@echo "  [PRECONF-LIBXCB] Generowanie configure dla libxcb..."
	@cd third_party/libxcb && autoreconf -fi -I ../util-macros -I /opt/homebrew/share/aclocal 2>/dev/null || true

$(BUILD_DIR)/third_party/libxcb/Makefile: third_party/libxcb/configure | $(ROOTFS_DIR)/lib/libXau.so $(ROOTFS_DIR)/lib/libXdmcp.so $(SYSROOT_STAMP)
	@mkdir -p $(BUILD_DIR)/third_party/libxcb
	@echo "  [CONF-LIBXCB] Konfiguracja libxcb..."
	@cd $(BUILD_DIR)/third_party/libxcb && \
	PKG_CONFIG_PATH="$(abspath $(SYSROOT_DIR))/usr/lib/pkgconfig" \
	$(abspath third_party/libxcb)/configure --host=x86_64-elf --prefix=/usr --enable-shared --disable-static --disable-devel-docs \
	    NEEDED_CFLAGS="-I$(abspath $(SYSROOT_DIR))/usr/include" \
	    NEEDED_LIBS="-L$(abspath $(SYSROOT_DIR))/usr/lib -lXau" \
	    CC="$(CC)" \
	    CFLAGS="-fPIC -O2 -ffreestanding -fno-builtin -isystem $(abspath $(SYSROOT_DIR))/usr/include -B$(abspath $(SYSROOT_DIR))/usr/lib" \
	    LDFLAGS="-nostdlib -L$(abspath $(SYSROOT_DIR))/usr/lib -B$(abspath $(SYSROOT_DIR))/usr/lib"

$(ROOTFS_DIR)/lib/libxcb.so: $(BUILD_DIR)/third_party/libxcb/Makefile
	@mkdir -p $(BUILD_DIR)/third_party/libxcb $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/xcb
	@echo "  [MAKE-LIBXCB] Kompilacja libxcb (-j$(JOBS))..."
	@cd $(BUILD_DIR)/third_party/libxcb && \
	export PYTHONPATH="$(abspath third_party/xcb-proto)" && \
	$(MAKE) -j$(JOBS) -C src XCBPROTO_XCBINCLUDEDIR="$(abspath $(SYSROOT_DIR))/usr/share/xcb" && \
	cp -f src/*.h $(abspath $(SYSROOT_DIR))/usr/include/xcb/ && \
	cd src && \
	$(LD) -shared -soname libxcb.so.1 -o $(abspath $(SYSROOT_DIR))/usr/lib/libxcb.so.1 *.o -L$(abspath $(SYSROOT_DIR))/usr/lib -lXau -lXdmcp -lc && \
	ln -sf libxcb.so.1 $(abspath $(SYSROOT_DIR))/usr/lib/libxcb.so && \
	cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libxcb.so.1 $(abspath $(ROOTFS_DIR))/lib/libxcb.so.1 && \
	ln -sf libxcb.so.1 $(abspath $(ROOTFS_DIR))/lib/libxcb.so && \
	cp -f $(abspath $(BUILD_DIR)/third_party/libxcb)/*.pc $(abspath $(SYSROOT_DIR))/usr/lib/pkgconfig/ 2>/dev/null || true && \
	cp -f $(abspath $(BUILD_DIR)/third_party/libxcb)/*.pc $(abspath $(SYSROOT_DIR))/usr/share/pkgconfig/ 2>/dev/null || true


# ==============================================================================
# libX11 Target
# ==============================================================================
third_party/libX11/configure: third_party/libX11/configure.ac
	@echo "  [PRECONF-LIBX11] Generowanie configure dla libX11..."
	@cd third_party/libX11 && autoreconf -fi -I ../util-macros -I ../xtrans -I /opt/homebrew/share/aclocal 2>/dev/null || true

$(BUILD_DIR)/third_party/libX11/Makefile: third_party/libX11/configure | $(ROOTFS_DIR)/lib/libxcb.so $(ROOTFS_DIR)/lib/libXau.so $(ROOTFS_DIR)/lib/libXdmcp.so $(SYSROOT_STAMP)
	@mkdir -p $(BUILD_DIR)/third_party/libX11 $(SYSROOT_DIR)/usr/include/X11 $(SYSROOT_DIR)/usr/include/xcb
	@cp -rf third_party/xorgproto/include/X11/* $(SYSROOT_DIR)/usr/include/X11/ 2>/dev/null || true
	@cp -r third_party/libX11/include/X11/* $(SYSROOT_DIR)/usr/include/X11/ 2>/dev/null || true
	@cp -f third_party/libxcb/src/*.h $(SYSROOT_DIR)/usr/include/xcb/ 2>/dev/null || true
	@cp -f $(BUILD_DIR)/third_party/libxcb/src/*.h $(SYSROOT_DIR)/usr/include/xcb/ 2>/dev/null || true
	@echo "  [CONF-LIBX11] Konfiguracja libX11..."
	@cd $(BUILD_DIR)/third_party/libX11 && \
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
	    LDFLAGS="-nostdlib -L$(abspath $(SYSROOT_DIR))/usr/lib -B$(abspath $(SYSROOT_DIR))/usr/lib"

$(ROOTFS_DIR)/lib/libX11.so: $(BUILD_DIR)/third_party/libX11/Makefile
	@mkdir -p $(BUILD_DIR)/third_party/libX11 $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib
	@echo "  [MAKE-LIBX11] Kompilacja libX11 (-j$(JOBS))..."
	@cd $(BUILD_DIR)/third_party/libX11 && \
	$(MAKE) -j$(JOBS) -C modules && \
	$(MAKE) -j$(JOBS) -C src && \
	$(LD) -shared -soname libX11.so.6 -o $(abspath $(SYSROOT_DIR))/usr/lib/libX11.so.6 --whole-archive src/.libs/libX11.a --no-whole-archive -L$(abspath $(SYSROOT_DIR))/usr/lib -lxcb -lXau -lXdmcp -lc && \
	ln -sf libX11.so.6 $(abspath $(SYSROOT_DIR))/usr/lib/libX11.so && \
	cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libX11.so.6 $(abspath $(ROOTFS_DIR))/lib/libX11.so.6 && \
	ln -sf libX11.so.6 $(abspath $(ROOTFS_DIR))/lib/libX11.so && \
	cp -r $(abspath third_party/libX11/include/X11)/* $(abspath $(SYSROOT_DIR))/usr/include/X11/ && \
	cp -f $(abspath $(BUILD_DIR)/third_party/libX11)/include/X11/XlibConf.h $(abspath $(SYSROOT_DIR))/usr/include/X11/ 2>/dev/null || true && \
	cp -f $(abspath $(BUILD_DIR)/third_party/libX11)/*.pc $(abspath $(SYSROOT_DIR))/usr/lib/pkgconfig/ 2>/dev/null || true


# ==============================================================================
# libxkbfile Target
# ==============================================================================
XKBFILE_SRCS := $(wildcard third_party/libxkbfile/src/*.c)
XKBFILE_OBJS := $(patsubst third_party/libxkbfile/src/%.c, $(BUILD_DIR)/third_party/libxkbfile/%.o, $(XKBFILE_SRCS))

$(BUILD_DIR)/third_party/libxkbfile:
	@mkdir -p $@

$(BUILD_DIR)/third_party/libxkbfile/config.h: | $(BUILD_DIR)/third_party/libxkbfile
	@touch $@

$(BUILD_DIR)/third_party/libxkbfile/%.o: third_party/libxkbfile/src/%.c | $(ROOTFS_DIR)/lib/libX11.so $(SYSROOT_STAMP) $(BUILD_DIR)/third_party/libxkbfile $(BUILD_DIR)/third_party/libxkbfile/config.h
	@echo "  [CC-LIBXKBFILE] $<"
	@$(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DHAVE_STRCASECMP=1 \
	    -isystem $(abspath $(SYSROOT_DIR))/usr/include \
	    -Ithird_party/libxkbfile/include \
	    -Ithird_party/libxkbfile/include/X11/extensions \
	    -Ithird_party/libxkbfile/src \
	    -I$(BUILD_DIR)/third_party/libxkbfile -c $< -o $@

$(ROOTFS_DIR)/lib/libxkbfile.so: $(XKBFILE_OBJS) | $(ROOTFS_DIR)/lib/libX11.so $(SYSROOT_STAMP) $(ROOTFS_DIR)
	@mkdir -p $(BUILD_DIR)/third_party/libxkbfile $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/X11/extensions
	@echo "  [LD-LIBXKBFILE] $@"
	@$(LD) -shared -soname libxkbfile.so.1 -o $(SYSROOT_DIR)/usr/lib/libxkbfile.so.1 $(XKBFILE_OBJS) -L$(abspath $(SYSROOT_DIR))/usr/lib -lX11 -lc
	@ln -sf libxkbfile.so.1 $(SYSROOT_DIR)/usr/lib/libxkbfile.so
	@cp -f $(SYSROOT_DIR)/usr/lib/libxkbfile.so.1 $(ROOTFS_DIR)/lib/libxkbfile.so.1
	@ln -sf libxkbfile.so.1 $(ROOTFS_DIR)/lib/libxkbfile.so
	@cp -f third_party/libxkbfile/include/X11/extensions/*.h $(SYSROOT_DIR)/usr/include/X11/extensions/ 2>/dev/null || true
	@printf "prefix=/usr\nexec_prefix=\$${prefix}\nlibdir=\$${exec_prefix}/lib\nincludedir=\$${prefix}/include\n\nName: xkbfile\nDescription: The xkbfile Library\nVersion: 1.1.0\nRequires: kbproto\nRequires.private: x11\nLibs: -L\$${libdir} -lxkbfile\nCflags: -I\$${includedir}\n" > $(SYSROOT_DIR)/usr/lib/pkgconfig/xkbfile.pc
	@cp -f $(SYSROOT_DIR)/usr/lib/pkgconfig/xkbfile.pc $(SYSROOT_DIR)/usr/share/pkgconfig/ 2>/dev/null || true

# ==============================================================================
# libfontenc Target
# ==============================================================================
FONTENC_SRCS := $(wildcard third_party/libfontenc/src/*.c)
FONTENC_OBJS := $(patsubst third_party/libfontenc/src/%.c, $(BUILD_DIR)/third_party/libfontenc/%.o, $(FONTENC_SRCS))

$(BUILD_DIR)/third_party/libfontenc:
	@mkdir -p $@

$(BUILD_DIR)/third_party/libfontenc/config.h: | $(BUILD_DIR)/third_party/libfontenc
	@touch $@

$(BUILD_DIR)/third_party/libfontenc/%.o: third_party/libfontenc/src/%.c | $(SYSROOT_STAMP) $(BUILD_DIR)/third_party/libfontenc $(BUILD_DIR)/third_party/libfontenc/config.h
	@echo "  [CC-LIBFONTENC] $<"
	@$(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DHAVE_REALLOCARRAY=1 \
	    -DFONT_ENCODINGS_DIRECTORY='"/usr/share/fonts/X11/encodings/encodings.dir"' \
	    -isystem $(abspath $(SYSROOT_DIR))/usr/include \
	    -Ithird_party/libfontenc/include \
	    -Ithird_party/libfontenc/src \
	    -I$(BUILD_DIR)/third_party/libfontenc -c $< -o $@

$(ROOTFS_DIR)/lib/libfontenc.so: $(FONTENC_OBJS) | $(LIBZ_A) $(SYSROOT_STAMP) $(ROOTFS_DIR)
	@mkdir -p $(BUILD_DIR)/third_party/libfontenc $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/X11/fonts
	@echo "  [LD-LIBFONTENC] $@"
	@$(LD) -shared -soname libfontenc.so.1 -o $(SYSROOT_DIR)/usr/lib/libfontenc.so.1 $(FONTENC_OBJS) -L$(abspath $(SYSROOT_DIR))/usr/lib -lz -lc
	@ln -sf libfontenc.so.1 $(SYSROOT_DIR)/usr/lib/libfontenc.so
	@cp -f $(SYSROOT_DIR)/usr/lib/libfontenc.so.1 $(ROOTFS_DIR)/lib/libfontenc.so.1
	@ln -sf libfontenc.so.1 $(ROOTFS_DIR)/lib/libfontenc.so
	@cp -f third_party/libfontenc/include/X11/fonts/*.h $(SYSROOT_DIR)/usr/include/X11/fonts/ 2>/dev/null || true
	@printf "prefix=/usr\nexec_prefix=\$${prefix}\nlibdir=\$${exec_prefix}/lib\nincludedir=\$${prefix}/include\n\nName: fontenc\nDescription: The fontenc Library\nVersion: 1.1.4\nLibs: -L\$${libdir} -lfontenc\nCflags: -I\$${includedir}\n" > $(SYSROOT_DIR)/usr/lib/pkgconfig/fontenc.pc
	@cp -f $(SYSROOT_DIR)/usr/lib/pkgconfig/fontenc.pc $(SYSROOT_DIR)/usr/share/pkgconfig/ 2>/dev/null || true

# ==============================================================================
# libXfont2 Target
# ==============================================================================
third_party/libXfont2/configure: third_party/libXfont2/configure.ac
	@echo "  [PRECONF-LIBXFONT2] Generowanie configure dla libXfont2..."
	@mkdir -p third_party/libXfont2/m4
	@cd third_party/libXfont2 && autoreconf -fi -I ../util-macros -I ../xtrans -I ../font-util -I /opt/homebrew/share/aclocal 2>/dev/null || true

$(BUILD_DIR)/third_party/libXfont2/Makefile: third_party/libXfont2/configure | $(ROOTFS_DIR)/lib/libfontenc.so $(LIBZ_A) $(SYSROOT_STAMP)
	@mkdir -p $(BUILD_DIR)/third_party/libXfont2
	@echo "  [CONF-LIBXFONT2] Konfiguracja libXfont2..."
	@cd $(BUILD_DIR)/third_party/libXfont2 && \
	PKG_CONFIG_LIBDIR="$(abspath $(SYSROOT_DIR))/usr/lib/pkgconfig:$(abspath $(SYSROOT_DIR))/usr/share/pkgconfig" \
	PKG_CONFIG_PATH="$(abspath $(SYSROOT_DIR))/usr/lib/pkgconfig:$(abspath $(SYSROOT_DIR))/usr/share/pkgconfig" \
	$(abspath third_party/libXfont2)/configure --host=x86_64-elf --prefix=/usr --enable-shared --disable-static \
	    --disable-freetype --disable-devel-docs \
	    CC="$(CC)" \
	    CFLAGS="-fPIC -O2 -ffreestanding -fno-builtin -isystem $(abspath $(SYSROOT_DIR))/usr/include -B$(abspath $(SYSROOT_DIR))/usr/lib" \
	    LDFLAGS="-nostdlib -L$(abspath $(SYSROOT_DIR))/usr/lib -B$(abspath $(SYSROOT_DIR))/usr/lib -lc" \
	    LIBS="-L$(abspath $(SYSROOT_DIR))/usr/lib -lfontenc -lz -lc"

$(ROOTFS_DIR)/lib/libXfont2.so: $(BUILD_DIR)/third_party/libXfont2/Makefile
	@mkdir -p $(BUILD_DIR)/third_party/libXfont2 $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib
	@echo "  [MAKE-LIBXFONT2] Kompilacja libXfont2 (-j$(JOBS))..."
	@cd $(BUILD_DIR)/third_party/libXfont2 && \
	$(MAKE) -j$(JOBS) && \
	$(MAKE) -j$(JOBS) install DESTDIR="$(abspath $(SYSROOT_DIR))" && \
	rm -f $(abspath $(SYSROOT_DIR))/usr/lib/*.la && \
	$(LD) -shared -soname libXfont2.so.2 -o $(abspath $(SYSROOT_DIR))/usr/lib/libXfont2.so.2 --whole-archive .libs/libXfont2.a --no-whole-archive -L$(abspath $(SYSROOT_DIR))/usr/lib -lfontenc -lz -lc && \
	ln -sf libXfont2.so.2 $(abspath $(SYSROOT_DIR))/usr/lib/libXfont2.so && \
	cp -f $(abspath $(SYSROOT_DIR))/usr/lib/libXfont2.so.2 $(abspath $(ROOTFS_DIR))/lib/libXfont2.so.2 && \
	ln -sf libXfont2.so.2 $(abspath $(ROOTFS_DIR))/lib/libXfont2.so


# ==============================================================================
# libxcvt Target
# ==============================================================================
$(ROOTFS_DIR)/lib/libxcvt.so: $(wildcard third_party/libxcvt/lib/*.c) | $(SYSROOT_STAMP) $(ROOTFS_DIR)
	@mkdir -p $(BUILD_DIR)/third_party/libxcvt $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/libxcvt
	@echo "  [MAKE-LIBXCVT] Kompilacja libxcvt..."
	@$(CC) -fPIC -O2 -ffreestanding -fno-builtin -isystem $(abspath $(SYSROOT_DIR))/usr/include -Ithird_party/libxcvt/include -Ithird_party/libxcvt/lib -c third_party/libxcvt/lib/libxcvt.c -o $(BUILD_DIR)/third_party/libxcvt/libxcvt.o
	@$(LD) -shared -soname libxcvt.so.0 -o $(SYSROOT_DIR)/usr/lib/libxcvt.so.0 $(BUILD_DIR)/third_party/libxcvt/libxcvt.o -L$(abspath $(SYSROOT_DIR))/usr/lib -lc
	@ln -sf libxcvt.so.0 $(SYSROOT_DIR)/usr/lib/libxcvt.so
	@cp -f $(SYSROOT_DIR)/usr/lib/libxcvt.so.0 $(ROOTFS_DIR)/lib/libxcvt.so.0
	@ln -sf libxcvt.so.0 $(ROOTFS_DIR)/lib/libxcvt.so
	@cp -f third_party/libxcvt/include/libxcvt/*.h $(SYSROOT_DIR)/usr/include/libxcvt/ 2>/dev/null || true

# ==============================================================================
# libpciaccess Target
# ==============================================================================
PCIACCESS_SRCS := $(addprefix third_party/libpciaccess/src/, common_bridge.c common_iterator.c common_init.c common_interface.c common_capability.c common_device_name.c common_map.c common_vgaarb.c common_io.c linux_sysfs.c linux_devmem.c)
PCIACCESS_OBJS := $(patsubst third_party/libpciaccess/src/%.c, $(BUILD_DIR)/third_party/libpciaccess/%.o, $(PCIACCESS_SRCS))

$(BUILD_DIR)/third_party/libpciaccess:
	@mkdir -p $@

$(BUILD_DIR)/third_party/libpciaccess/config.h: | $(BUILD_DIR)/third_party/libpciaccess
	@touch $@

$(BUILD_DIR)/third_party/libpciaccess/%.o: third_party/libpciaccess/src/%.c | $(BUILD_DIR)/third_party/libpciaccess/config.h $(SYSROOT_STAMP) $(BUILD_DIR)/third_party/libpciaccess
	@echo "  [CC-LIBPCIACCESS] $<"
	@$(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DHAVE_STDINT_H -DHAVE_INTTYPES_H \
	    -DPCIIDS_PATH=\"/usr/share/hwdata\" -D__linux__=1 \
	    -Ithird_party/libpciaccess/include -Ithird_party/libpciaccess/src \
	    -isystem $(abspath $(SYSROOT_DIR))/usr/include -I$(BUILD_DIR)/third_party/libpciaccess -c $< -o $@

$(ROOTFS_DIR)/lib/libpciaccess.so: $(PCIACCESS_OBJS) | $(SYSROOT_STAMP) $(ROOTFS_DIR)
	@mkdir -p $(BUILD_DIR)/third_party/libpciaccess $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include
	@echo "  [LD-LIBPCIACCESS] $@"
	@$(LD) -shared -soname libpciaccess.so.0 -o $(SYSROOT_DIR)/usr/lib/libpciaccess.so.0 $(PCIACCESS_OBJS) -L$(abspath $(SYSROOT_DIR))/usr/lib -lc
	@ln -sf libpciaccess.so.0 $(SYSROOT_DIR)/usr/lib/libpciaccess.so
	@cp -f $(SYSROOT_DIR)/usr/lib/libpciaccess.so.0 $(ROOTFS_DIR)/lib/libpciaccess.so.0
	@ln -sf libpciaccess.so.0 $(ROOTFS_DIR)/lib/libpciaccess.so
	@cp -f third_party/libpciaccess/include/pciaccess.h $(SYSROOT_DIR)/usr/include/

# ==============================================================================
# libpixman-1 Target
# ==============================================================================
PIXMAN_SRCS_NAMES := pixman.c pixman-access.c pixman-access-accessors.c pixman-arm.c pixman-bits-image.c \
                     pixman-combine32.c pixman-combine-float.c pixman-conical-gradient.c pixman-edge.c \
                     pixman-edge-accessors.c pixman-fast-path.c pixman-filter.c pixman-glyph.c \
                     pixman-general.c pixman-gradient-walker.c pixman-image.c pixman-implementation.c \
                     pixman-linear-gradient.c pixman-matrix.c pixman-mips.c pixman-noop.c pixman-ppc.c \
                     pixman-radial-gradient.c pixman-region16.c pixman-region32.c pixman-region64f.c \
                     pixman-riscv.c pixman-solid-fill.c pixman-timer.c pixman-trap.c pixman-utils.c pixman-x86.c
PIXMAN_SRCS := $(addprefix third_party/pixman/pixman/, $(PIXMAN_SRCS_NAMES))
PIXMAN_OBJS := $(patsubst third_party/pixman/pixman/%.c, $(BUILD_DIR)/third_party/pixman/%.o, $(PIXMAN_SRCS))

$(BUILD_DIR)/third_party/pixman:
	@mkdir -p $@

$(BUILD_DIR)/third_party/pixman/config.h: | $(BUILD_DIR)/third_party/pixman
	@printf '#ifndef CONFIG_H\n#define CONFIG_H\n#define PACKAGE "pixman"\n#define PACKAGE_VERSION "0.43.4"\n#define PIXMAN_NO_TLS 1\n#define HAVE_POSIX_MEMALIGN 1\n#define HAVE_SIGACTION 1\n#define HAVE_ALARM 1\n#define HAVE_MPROTECT 1\n#define HAVE_GETPAGESIZE 1\n#define HAVE_MMAP 1\n#define HAVE_GETTIMEOFDAY 1\n#define SIZEOF_LONG 8\n#endif\n' > $@
	@cp -f $@ $(BUILD_DIR)/third_party/pixman/pixman-config.h

$(BUILD_DIR)/third_party/pixman/pixman-version.h: | $(BUILD_DIR)/third_party/pixman $(SYSROOT_STAMP)
	@mkdir -p $(SYSROOT_DIR)/usr/include/pixman-1
	@printf '#ifndef PIXMAN_VERSION_H\n#define PIXMAN_VERSION_H\n#define PIXMAN_VERSION_MAJOR 0\n#define PIXMAN_VERSION_MINOR 43\n#define PIXMAN_VERSION_MICRO 4\n#define PIXMAN_VERSION_STRING "0.43.4"\n#define PIXMAN_VERSION (PIXMAN_VERSION_MAJOR*10000 + PIXMAN_VERSION_MINOR*100 + PIXMAN_VERSION_MICRO)\n#ifndef PIXMAN_API\n#define PIXMAN_API\n#endif\n#endif\n' > $@
	@cp -f $@ $(SYSROOT_DIR)/usr/include/pixman-1/
	@cp -f $@ $(SYSROOT_DIR)/usr/include/

$(BUILD_DIR)/third_party/pixman/%.o: third_party/pixman/pixman/%.c | $(BUILD_DIR)/third_party/pixman/config.h $(BUILD_DIR)/third_party/pixman/pixman-version.h $(SYSROOT_STAMP) $(BUILD_DIR)/third_party/pixman
	@echo "  [CC-PIXMAN] $<"
	@$(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DPIXMAN_NO_TLS=1 \
	    -isystem $(abspath $(SYSROOT_DIR))/usr/include \
	    -Ithird_party/pixman/pixman -I$(BUILD_DIR)/third_party/pixman -I. -c $< -o $@

$(ROOTFS_DIR)/lib/libpixman-1.so: $(PIXMAN_OBJS) | $(LIBM_SO) $(SYSROOT_STAMP) $(ROOTFS_DIR)
	@mkdir -p $(BUILD_DIR)/third_party/pixman $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/pixman-1
	@echo "  [LD-PIXMAN] $@"
	@$(LD) -shared -soname libpixman-1.so.0 -o $(SYSROOT_DIR)/usr/lib/libpixman-1.so.0 $(PIXMAN_OBJS) -L$(abspath $(SYSROOT_DIR))/usr/lib -lm -lc
	@ln -sf libpixman-1.so.0 $(SYSROOT_DIR)/usr/lib/libpixman-1.so
	@cp -f $(SYSROOT_DIR)/usr/lib/libpixman-1.so.0 $(ROOTFS_DIR)/lib/libpixman-1.so.0
	@ln -sf libpixman-1.so.0 $(ROOTFS_DIR)/lib/libpixman-1.so
	@cp -f third_party/pixman/pixman/*.h $(SYSROOT_DIR)/usr/include/pixman-1/ 2>/dev/null || true
	@cp -f third_party/pixman/pixman/*.h $(SYSROOT_DIR)/usr/include/ 2>/dev/null || true

# ==============================================================================
# libICE Target
# ==============================================================================
ICE_SRCS := $(wildcard third_party/libICE/src/*.c)
ICE_OBJS := $(patsubst third_party/libICE/src/%.c, $(BUILD_DIR)/third_party/libICE/%.o, $(ICE_SRCS))

$(BUILD_DIR)/third_party/libICE:
	@mkdir -p $@

$(BUILD_DIR)/third_party/libICE/config.h: | $(BUILD_DIR)/third_party/libICE
	@touch $@

$(BUILD_DIR)/third_party/libICE/%.o: third_party/libICE/src/%.c | $(BUILD_DIR)/third_party/libICE/config.h $(SYSROOT_STAMP) $(BUILD_DIR)/third_party/libICE
	@echo "  [CC-LIBICE] $<"
	@$(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DHAVE_ASPRINTF=1 -DICE_t -DTRANS_CLIENT -DTRANS_SERVER \
	    -isystem $(abspath $(SYSROOT_DIR))/usr/include \
	    -Ithird_party/libICE/include -Ithird_party/libICE/src -Ithird_party/xtrans -I$(BUILD_DIR)/third_party/libICE -c $< -o $@

$(ROOTFS_DIR)/lib/libICE.so: $(ICE_OBJS) | $(LIBC_SO) $(SYSROOT_STAMP) $(ROOTFS_DIR)
	@mkdir -p $(BUILD_DIR)/third_party/libICE $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/X11/ICE
	@echo "  [LD-LIBICE] $@"
	@$(LD) -shared -soname libICE.so.6 -o $(SYSROOT_DIR)/usr/lib/libICE.so.6 $(ICE_OBJS) -L$(abspath $(SYSROOT_DIR))/usr/lib -lc
	@ln -sf libICE.so.6 $(SYSROOT_DIR)/usr/lib/libICE.so
	@cp -f $(SYSROOT_DIR)/usr/lib/libICE.so.6 $(ROOTFS_DIR)/lib/libICE.so.6
	@ln -sf libICE.so.6 $(ROOTFS_DIR)/lib/libICE.so
	@mkdir -p $(SYSROOT_DIR)/usr/include/X11/ICE
	@cp -r third_party/libICE/include/X11/ICE/* $(SYSROOT_DIR)/usr/include/X11/ICE/

# ==============================================================================
# libSM Target
# ==============================================================================
SM_SRCS := $(wildcard third_party/libSM/src/*.c)
SM_OBJS := $(patsubst third_party/libSM/src/%.c, $(BUILD_DIR)/third_party/libSM/%.o, $(SM_SRCS))

$(BUILD_DIR)/third_party/libSM:
	@mkdir -p $@

$(BUILD_DIR)/third_party/libSM/config.h: | $(BUILD_DIR)/third_party/libSM
	@touch $@

$(BUILD_DIR)/third_party/libSM/%.o: third_party/libSM/src/%.c | $(BUILD_DIR)/third_party/libSM/config.h $(SYSROOT_STAMP) $(BUILD_DIR)/third_party/libSM
	@echo "  [CC-LIBSM] $<"
	@$(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DHAVE_ASPRINTF=1 \
	    -isystem $(abspath $(SYSROOT_DIR))/usr/include \
	    -Ithird_party/libSM/include -Ithird_party/libSM/src -Ithird_party/libICE/include -I$(BUILD_DIR)/third_party/libSM -c $< -o $@

$(ROOTFS_DIR)/lib/libSM.so: $(SM_OBJS) | $(ROOTFS_DIR)/lib/libICE.so $(SYSROOT_STAMP) $(ROOTFS_DIR)
	@mkdir -p $(BUILD_DIR)/third_party/libSM $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/X11/SM
	@echo "  [LD-LIBSM] $@"
	@$(LD) -shared -soname libSM.so.6 -o $(SYSROOT_DIR)/usr/lib/libSM.so.6 $(SM_OBJS) -L$(abspath $(SYSROOT_DIR))/usr/lib -lICE -lc
	@ln -sf libSM.so.6 $(SYSROOT_DIR)/usr/lib/libSM.so
	@cp -f $(SYSROOT_DIR)/usr/lib/libSM.so.6 $(ROOTFS_DIR)/lib/libSM.so.6
	@ln -sf libSM.so.6 $(ROOTFS_DIR)/lib/libSM.so
	@mkdir -p $(SYSROOT_DIR)/usr/include/X11/SM
	@cp -r third_party/libSM/include/X11/SM/* $(SYSROOT_DIR)/usr/include/X11/SM/

# ==============================================================================
# libXpm Target
# ==============================================================================
XPM_SRCS := $(wildcard third_party/libXpm/src/*.c)
XPM_OBJS := $(patsubst third_party/libXpm/src/%.c, $(BUILD_DIR)/third_party/libXpm/%.o, $(XPM_SRCS))

$(BUILD_DIR)/third_party/libXpm:
	@mkdir -p $@

$(BUILD_DIR)/third_party/libXpm/config.h: | $(BUILD_DIR)/third_party/libXpm
	@touch $@

$(BUILD_DIR)/third_party/libXpm/%.o: third_party/libXpm/src/%.c | $(ROOTFS_DIR)/lib/libX11.so $(BUILD_DIR)/third_party/libXpm/config.h $(SYSROOT_STAMP) $(BUILD_DIR)/third_party/libXpm
	@echo "  [CC-LIBXPM] $<"
	@$(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DNO_ZPIPE=1 -DHAVE_STRCASECMP=1 -DHAVE_ASPRINTF=1 -DHAS_GETCWD=1 \
	    -isystem $(abspath $(SYSROOT_DIR))/usr/include \
	    -Ithird_party/libXpm/include -Ithird_party/libXpm/include/X11 -Ithird_party/libXpm/src -I$(BUILD_DIR)/third_party/libXpm -c $< -o $@

$(ROOTFS_DIR)/lib/libXpm.so: $(XPM_OBJS) | $(ROOTFS_DIR)/lib/libX11.so $(SYSROOT_STAMP) $(ROOTFS_DIR)
	@mkdir -p $(BUILD_DIR)/third_party/libXpm $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/X11
	@echo "  [LD-LIBXPM] $@"
	@$(LD) -shared -soname libXpm.so.4 -o $(SYSROOT_DIR)/usr/lib/libXpm.so.4 $(XPM_OBJS) -L$(abspath $(SYSROOT_DIR))/usr/lib -lX11 -lc
	@ln -sf libXpm.so.4 $(SYSROOT_DIR)/usr/lib/libXpm.so
	@cp -f $(SYSROOT_DIR)/usr/lib/libXpm.so.4 $(ROOTFS_DIR)/lib/libXpm.so.4
	@ln -sf libXpm.so.4 $(ROOTFS_DIR)/lib/libXpm.so
	@mkdir -p $(SYSROOT_DIR)/usr/include/X11
	@cp -r third_party/libXpm/include/X11/* $(SYSROOT_DIR)/usr/include/X11/
	@printf "prefix=/usr\nexec_prefix=\$${prefix}\nlibdir=\$${exec_prefix}/lib\nincludedir=\$${prefix}/include\n\nName: Xpm\nDescription: X Pixmap Library\nVersion: 3.5.17\nLibs: -L\$${libdir} -lXpm\nCflags: -I\$${includedir}\n" > $(SYSROOT_DIR)/usr/lib/pkgconfig/xpm.pc
	@cp -f $(SYSROOT_DIR)/usr/lib/pkgconfig/xpm.pc $(SYSROOT_DIR)/usr/share/pkgconfig/ 2>/dev/null || true

# ==============================================================================
# libXext Target
# ==============================================================================
XEXT_SRCS := $(wildcard third_party/libXext/src/*.c)
XEXT_OBJS := $(patsubst third_party/libXext/src/%.c, $(BUILD_DIR)/third_party/libXext/%.o, $(XEXT_SRCS))

$(BUILD_DIR)/third_party/libXext:
	@mkdir -p $@

$(BUILD_DIR)/third_party/libXext/config.h: | $(BUILD_DIR)/third_party/libXext
	@touch $@

$(BUILD_DIR)/third_party/libXext/%.o: third_party/libXext/src/%.c | $(ROOTFS_DIR)/lib/libX11.so $(BUILD_DIR)/third_party/libXext/config.h $(SYSROOT_STAMP) $(BUILD_DIR)/third_party/libXext
	@echo "  [CC-LIBXEXT] $<"
	@$(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H \
	    -isystem $(abspath $(SYSROOT_DIR))/usr/include \
	    -Ithird_party/libXext/include -Ithird_party/libXext/include/X11/extensions -Ithird_party/libXext/src -I$(BUILD_DIR)/third_party/libXext -c $< -o $@

$(ROOTFS_DIR)/lib/libXext.so: $(XEXT_OBJS) | $(ROOTFS_DIR)/lib/libX11.so $(SYSROOT_STAMP) $(ROOTFS_DIR)
	@mkdir -p $(BUILD_DIR)/third_party/libXext $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/X11/extensions
	@echo "  [LD-LIBXEXT] $@"
	@$(LD) -shared -soname libXext.so.6 -o $(SYSROOT_DIR)/usr/lib/libXext.so.6 $(XEXT_OBJS) -L$(abspath $(SYSROOT_DIR))/usr/lib -lX11 -lc
	@ln -sf libXext.so.6 $(SYSROOT_DIR)/usr/lib/libXext.so
	@cp -f $(SYSROOT_DIR)/usr/lib/libXext.so.6 $(ROOTFS_DIR)/lib/libXext.so.6
	@ln -sf libXext.so.6 $(ROOTFS_DIR)/lib/libXext.so
	@mkdir -p $(SYSROOT_DIR)/usr/include/X11/extensions
	@cp -r third_party/libXext/include/X11/extensions/* $(SYSROOT_DIR)/usr/include/X11/extensions/
	@printf "prefix=/usr\nexec_prefix=\$${prefix}\nlibdir=\$${exec_prefix}/lib\nincludedir=\$${prefix}/include\n\nName: Xext\nDescription: Misc X Extension Library\nVersion: 1.3.6\nLibs: -L\$${libdir} -lXext\nCflags: -I\$${includedir}\n" > $(SYSROOT_DIR)/usr/lib/pkgconfig/xext.pc
	@cp -f $(SYSROOT_DIR)/usr/lib/pkgconfig/xext.pc $(SYSROOT_DIR)/usr/share/pkgconfig/ 2>/dev/null || true

# ==============================================================================
# libXt Target
# ==============================================================================
$(BUILD_DIR)/third_party/libXt:
	@mkdir -p $@

$(BUILD_DIR)/third_party/libXt/makestrs: third_party/libXt/util/makestrs.c | $(BUILD_DIR)/third_party/libXt
	@clang $< -o $@

$(BUILD_DIR)/third_party/libXt/StringDefs.c: $(BUILD_DIR)/third_party/libXt/makestrs third_party/libXt/util/string.list | $(BUILD_DIR)/third_party/libXt
	@mkdir -p $(BUILD_DIR)/third_party/libXt/include/X11 $(SYSROOT_DIR)/usr/include/X11
	@cd $(BUILD_DIR)/third_party/libXt && ./makestrs -i $(abspath third_party/libXt) < $(abspath third_party/libXt)/util/string.list > StringDefs.c
	@cp -f $(BUILD_DIR)/third_party/libXt/StringDefs.h $(BUILD_DIR)/third_party/libXt/Shell.h $(BUILD_DIR)/third_party/libXt/include/X11/
	@cp -f $(BUILD_DIR)/third_party/libXt/StringDefs.h $(BUILD_DIR)/third_party/libXt/Shell.h $(SYSROOT_DIR)/usr/include/X11/

$(BUILD_DIR)/third_party/libXt/config.h: | $(BUILD_DIR)/third_party/libXt
	@touch $@

XT_SRCS := $(filter-out %/StringDefs.c, $(sort $(wildcard third_party/libXt/src/*.c)))
XT_OBJS := $(patsubst third_party/libXt/src/%.c, $(BUILD_DIR)/third_party/libXt/%.o, $(XT_SRCS)) $(BUILD_DIR)/third_party/libXt/StringDefs.o

$(BUILD_DIR)/third_party/libXt/StringDefs.o: $(BUILD_DIR)/third_party/libXt/StringDefs.c | $(ROOTFS_DIR)/lib/libX11.so $(BUILD_DIR)/third_party/libXt/config.h $(SYSROOT_STAMP) $(BUILD_DIR)/third_party/libXt
	@echo "  [CC-LIBXT] $<"
	@$(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DHAVE_ASPRINTF=1 -DHAVE_REALLOCARRAY=1 -DHAS_GETCWD=1 \
	    -DXTHREADS -D_POSIX_THREAD_SAFE_FUNCTIONS -include sys/select.h \
	    -isystem $(abspath $(SYSROOT_DIR))/usr/include \
	    -Ithird_party/libXt/include -Ithird_party/libXt/include/X11 -I$(BUILD_DIR)/third_party/libXt/include -Ithird_party/libXt/src -I$(BUILD_DIR)/third_party/libXt -c $< -o $@

$(BUILD_DIR)/third_party/libXt/%.o: third_party/libXt/src/%.c | $(BUILD_DIR)/third_party/libXt/StringDefs.c $(ROOTFS_DIR)/lib/libX11.so $(BUILD_DIR)/third_party/libXt/config.h $(SYSROOT_STAMP) $(BUILD_DIR)/third_party/libXt
	@echo "  [CC-LIBXT] $<"
	@$(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DHAVE_ASPRINTF=1 -DHAVE_REALLOCARRAY=1 -DHAS_GETCWD=1 \
	    -DXTHREADS -D_POSIX_THREAD_SAFE_FUNCTIONS -include sys/select.h \
	    -isystem $(abspath $(SYSROOT_DIR))/usr/include \
	    -Ithird_party/libXt/include -Ithird_party/libXt/include/X11 -I$(BUILD_DIR)/third_party/libXt/include -Ithird_party/libXt/src -I$(BUILD_DIR)/third_party/libXt -c $< -o $@

$(ROOTFS_DIR)/lib/libXt.so: $(XT_OBJS) | $(ROOTFS_DIR)/lib/libX11.so $(ROOTFS_DIR)/lib/libSM.so $(ROOTFS_DIR)/lib/libICE.so $(SYSROOT_STAMP) $(ROOTFS_DIR)
	@mkdir -p $(BUILD_DIR)/third_party/libXt $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/X11
	@echo "  [LD-LIBXT] $@"
	@$(LD) -shared -soname libXt.so.6 -o $(SYSROOT_DIR)/usr/lib/libXt.so.6 $(XT_OBJS) -L$(abspath $(SYSROOT_DIR))/usr/lib -lX11 -lSM -lICE -lc
	@ln -sf libXt.so.6 $(SYSROOT_DIR)/usr/lib/libXt.so
	@cp -f $(SYSROOT_DIR)/usr/lib/libXt.so.6 $(ROOTFS_DIR)/lib/libXt.so.6
	@ln -sf libXt.so.6 $(ROOTFS_DIR)/lib/libXt.so
	@mkdir -p $(SYSROOT_DIR)/usr/include/X11
	@cp -r third_party/libXt/include/X11/* $(SYSROOT_DIR)/usr/include/X11/
	@cp -f $(BUILD_DIR)/third_party/libXt/StringDefs.h $(BUILD_DIR)/third_party/libXt/Shell.h $(SYSROOT_DIR)/usr/include/X11/

# ==============================================================================
# libXmu Target
# ==============================================================================
XMU_SRCS := $(wildcard third_party/libXmu/src/*.c)
XMU_OBJS := $(patsubst third_party/libXmu/src/%.c, $(BUILD_DIR)/third_party/libXmu/%.o, $(XMU_SRCS))

$(BUILD_DIR)/third_party/libXmu:
	@mkdir -p $@

$(BUILD_DIR)/third_party/libXmu/config.h: | $(BUILD_DIR)/third_party/libXmu
	@touch $@

$(BUILD_DIR)/third_party/libXmu/%.o: third_party/libXmu/src/%.c | $(ROOTFS_DIR)/lib/libXt.so $(BUILD_DIR)/third_party/libXmu/config.h $(SYSROOT_STAMP) $(BUILD_DIR)/third_party/libXmu
	@echo "  [CC-LIBXMU] $<"
	@$(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DHAVE_ASPRINTF=1 -DHAVE_REALLOCARRAY=1 -DHAS_GETCWD=1 \
	    -DXTHREADS -D_POSIX_THREAD_SAFE_FUNCTIONS \
	    -isystem $(abspath $(SYSROOT_DIR))/usr/include \
	    -Ithird_party/libXmu/include -Ithird_party/libXmu/include/X11/Xmu -Ithird_party/libXmu/src \
	    -Ithird_party/libXt/include -Ithird_party/libXt/include/X11 -I$(BUILD_DIR)/third_party/libXmu -c $< -o $@

$(ROOTFS_DIR)/lib/libXmu.so: $(XMU_OBJS) | $(ROOTFS_DIR)/lib/libXt.so $(ROOTFS_DIR)/lib/libXext.so $(SYSROOT_STAMP) $(ROOTFS_DIR)
	@mkdir -p $(BUILD_DIR)/third_party/libXmu $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/X11/Xmu
	@echo "  [LD-LIBXMU] $@"
	@$(LD) -shared -soname libXmu.so.6 -o $(SYSROOT_DIR)/usr/lib/libXmu.so.6 $(XMU_OBJS) -L$(abspath $(SYSROOT_DIR))/usr/lib -lXt -lXext -lX11 -lSM -lICE -lc
	@ln -sf libXmu.so.6 $(SYSROOT_DIR)/usr/lib/libXmu.so
	@cp -f $(SYSROOT_DIR)/usr/lib/libXmu.so.6 $(ROOTFS_DIR)/lib/libXmu.so.6
	@ln -sf libXmu.so.6 $(ROOTFS_DIR)/lib/libXmu.so
	@mkdir -p $(SYSROOT_DIR)/usr/include/X11/Xmu
	@cp -r third_party/libXmu/include/X11/Xmu/* $(SYSROOT_DIR)/usr/include/X11/Xmu/

# ==============================================================================
# libXaw Target
# ==============================================================================
XAW_SRCS := $(wildcard third_party/libXaw/src/*.c)
XAW_OBJS := $(patsubst third_party/libXaw/src/%.c, $(BUILD_DIR)/third_party/libXaw/%.o, $(XAW_SRCS))

$(BUILD_DIR)/third_party/libXaw:
	@mkdir -p $@

$(BUILD_DIR)/third_party/libXaw/config.h: | $(BUILD_DIR)/third_party/libXaw
	@touch $@

$(BUILD_DIR)/third_party/libXaw/%.o: third_party/libXaw/src/%.c | $(ROOTFS_DIR)/lib/libXmu.so $(ROOTFS_DIR)/lib/libXpm.so $(BUILD_DIR)/third_party/libXaw/config.h $(SYSROOT_STAMP) $(BUILD_DIR)/third_party/libXaw
	@echo "  [CC-LIBXAW] $<"
	@$(CC) -fPIC -O2 -ffreestanding -fno-builtin -DHAVE_CONFIG_H -DHAVE_ASPRINTF=1 -DHAVE_REALLOCARRAY=1 -DHAS_GETCWD=1 \
	    -DHAVE_WCHAR_H=1 -DHAVE_WCTYPE_H=1 -DHAVE_UNISTD_H=1 -DXTHREADS -D_POSIX_THREAD_SAFE_FUNCTIONS -include sys/select.h \
	    -isystem $(abspath $(SYSROOT_DIR))/usr/include \
	    -Ithird_party/libXaw/include -Ithird_party/libXaw/include/X11/Xaw -Ithird_party/libXaw/src \
	    -Ithird_party/libXpm/include -Ithird_party/libXmu/include -Ithird_party/libXt/include -Ithird_party/libXt/include/X11 \
	    -I$(BUILD_DIR)/third_party/libXaw -c $< -o $@

$(ROOTFS_DIR)/lib/libXaw.so: $(XAW_OBJS) | $(ROOTFS_DIR)/lib/libXmu.so $(ROOTFS_DIR)/lib/libXpm.so $(SYSROOT_STAMP) $(ROOTFS_DIR)
	@mkdir -p $(BUILD_DIR)/third_party/libXaw $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/X11/Xaw
	@echo "  [LD-LIBXAW] $@"
	@$(LD) -shared -soname libXaw7.so.7 -o $(SYSROOT_DIR)/usr/lib/libXaw7.so.7 $(XAW_OBJS) -L$(abspath $(SYSROOT_DIR))/usr/lib -lXmu -lXt -lXext -lX11 -lXpm -lc
	@ln -sf libXaw7.so.7 $(SYSROOT_DIR)/usr/lib/libXaw.so
	@ln -sf libXaw7.so.7 $(SYSROOT_DIR)/usr/lib/libXaw7.so
	@cp -f $(SYSROOT_DIR)/usr/lib/libXaw7.so.7 $(ROOTFS_DIR)/lib/libXaw7.so.7
	@ln -sf libXaw7.so.7 $(ROOTFS_DIR)/lib/libXaw7.so
	@ln -sf libXaw7.so.7 $(ROOTFS_DIR)/lib/libXaw.so
	@ln -sf libXaw7.so.7 $(ROOTFS_DIR)/lib/libXaw.so.7
	@mkdir -p $(SYSROOT_DIR)/usr/include/X11/Xaw
	@cp -r third_party/libXaw/include/X11/Xaw/* $(SYSROOT_DIR)/usr/include/X11/Xaw/

# ==============================================================================
# Official Upstream X11 Terminal Emulator (xterm)
# ==============================================================================
$(XTERM_BUILD_DIR):
	@mkdir -p $@

$(XTERM_BUILD_DIR)/Makefile: | $(XTERM_BUILD_DIR) $(ROOTFS_DIR)/lib/libXaw.so $(ROOTFS_DIR)/lib/libXmu.so $(ROOTFS_DIR)/lib/libXt.so $(ROOTFS_DIR)/lib/libXpm.so $(ROOTFS_DIR)/lib/libXext.so $(ROOTFS_DIR)/lib/libSM.so $(ROOTFS_DIR)/lib/libICE.so $(ROOTFS_DIR)/lib/libX11.so $(LIBNCURSES_A) $(ROOTFS_DIR)
	@echo "  [CONF-XTERM] Konfiguracja xterm (Autotools out-of-tree)..."
	@cd $(XTERM_BUILD_DIR) && \
	CC="$(CC) -nostdlib $(abspath $(SYSROOT_DIR))/usr/lib/crt0.o" \
	CPP="$(CC) -E -isystem $(abspath $(SYSROOT_DIR))/usr/include" \
	CFLAGS="-O2 -ffreestanding -isystem $(abspath $(SYSROOT_DIR))/usr/include -DUSE_SYSV_PGRP=1 -DUSE_POSIX_TERMIOS=1" \
	LDFLAGS="-L$(abspath $(ROOTFS_DIR))/lib -L$(abspath $(SYSROOT_DIR))/usr/lib -Wl,-rpath-link=$(abspath $(ROOTFS_DIR))/lib -lXaw7 -lXmu -lXt -lSM -lICE -lXpm -lXext -lX11 -lxcb -lXau -lXdmcp -lncurses -lm -lc" \
	$(abspath third_party/xterm)/configure --host=x86_64-elf --without-xinerama --disable-imake --disable-setuid --disable-setgid --disable-freetype --without-pcre --without-pcre2 --disable-luit

$(ROOTFS_DIR)/bin/xterm: $(XTERM_BUILD_DIR)/Makefile | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin
	@echo "  [MAKE-XTERM] Kompilacja oficjalnego upstream xterm (-j$(JOBS))..."
	@cd $(XTERM_BUILD_DIR) && \
	$(MAKE) -j$(JOBS) EXTRA_CFLAGS="-DUSE_SYSV_PGRP=1 -DHAVE_GRANTPT_PTY_ISATTY=1 -DUSE_POSIX_TERMIOS=1" && \
	cp -f xterm $(abspath $(ROOTFS_DIR))/bin/xterm && \
	cp -f resize $(abspath $(ROOTFS_DIR))/bin/resize 2>/dev/null || true
	@chmod +x $@


# ==============================================================================
# CA Certificates and SSL Setup
# ==============================================================================
$(ROOTFS_DIR)/etc/ssl/cert.pem: scripts/fetch_cacerts.py | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/etc/ssl/certs $(ROOTFS_DIR)/etc/ssl/private $(SYSROOT_DIR)/etc/ssl/certs
	@python3 scripts/fetch_cacerts.py $(ROOTFS_DIR)/etc/ssl/cert.pem $(ROOTFS_DIR)/etc/ssl/certs/ca-certificates.crt $(SYSROOT_DIR)/etc/ssl/cert.pem
	@if [ -f $(ROOTFS_SKELETON_DIR)/etc/ssl/openssl.cnf ]; then \
	    cp -f $(ROOTFS_SKELETON_DIR)/etc/ssl/openssl.cnf $(ROOTFS_DIR)/etc/ssl/openssl.cnf; \
	    cp -f $(ROOTFS_SKELETON_DIR)/etc/ssl/openssl.cnf $(SYSROOT_DIR)/etc/ssl/openssl.cnf 2>/dev/null || true; \
	fi

# ==============================================================================
# OpenSSL (libcrypto.so, libssl.so, /bin/openssl)
# ==============================================================================
$(OPENSSL_BUILD_DIR)/Makefile: $(abspath third_party/openssl)/Configure | $(SYSROOT_STAMP) $(OPENSSL_BUILD_DIR)
	@echo "  [CONF-OPENSSL] Konfiguracja OpenSSL (x86_64 cross-compile)..."
	@cd $(OPENSSL_BUILD_DIR) && \
	perl $(abspath third_party/openssl)/Configure \
	    linux-x86_64 \
	    shared \
	    no-threads \
	    no-async \
	    no-tests \
	    no-docs \
	    no-module \
	    no-engine \
	    no-legacy \
	    --prefix=/usr \
	    --openssldir=/etc/ssl \
	    CC="$(CC)" \
	    AR="$(AR)" \
	    RANLIB="$(RANLIB)" \
	    CFLAGS="-O2 -ffreestanding -fno-builtin -isystem $(abspath $(SYSROOT_DIR))/usr/include -B$(abspath $(SYSROOT_DIR))/usr/lib -fPIC" \
	    LDFLAGS="-nostdlib -Wl,-shared -L$(abspath $(SYSROOT_DIR))/usr/lib -B$(abspath $(SYSROOT_DIR))/usr/lib" \
	    LDLIBS="-lc -lm"

$(OPENSSL_BUILD_DIR):
	@mkdir -p $@

OPENSSL_APP_OBJS = \
	$(OPENSSL_BUILD_DIR)/apps/lib/openssl-bin-cmp_mock_srv.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-asn1parse.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-ca.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-ciphers.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-cmp.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-cms.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-configutl.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-crl.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-crl2pkcs7.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-dgst.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-dhparam.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-dsa.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-dsaparam.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-ec.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-ech.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-ecparam.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-enc.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-errstr.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-fipsinstall.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-gendsa.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-genpkey.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-genrsa.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-info.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-kdf.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-list.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-mac.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-nseq.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-ocsp.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-openssl.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-passwd.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-pkcs12.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-pkcs7.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-pkcs8.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-pkey.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-pkeyparam.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-pkeyutl.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-prime.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-progs.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-rand.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-rehash.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-req.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-rsa.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-rsautl.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-s_client.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-s_server.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-s_time.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-sess_id.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-skeyutl.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-smime.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-speed.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-spkac.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-srp.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-storeutl.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-ts.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-verify.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-version.o \
	$(OPENSSL_BUILD_DIR)/apps/openssl-bin-x509.o

OPENSSL_STAMP := $(OPENSSL_BUILD_DIR)/.built

$(OPENSSL_STAMP): $(OPENSSL_BUILD_DIR)/Makefile | $(LIBC_SO) $(LIBM_SO) $(CRT0_O) $(LIBC_A) $(ROOTFS_DIR)
	@mkdir -p $(OPENSSL_BUILD_DIR) $(OPENSSL_BUILD_DIR)/ssl $(OPENSSL_BUILD_DIR)/crypto $(OPENSSL_BUILD_DIR)/apps $(OPENSSL_BUILD_DIR)/providers $(ROOTFS_DIR)/lib $(ROOTFS_DIR)/bin $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/openssl $(SYSROOT_DIR)/usr/lib/pkgconfig $(SYSROOT_DIR)/usr/share/pkgconfig
	@echo "  [MAKE-OPENSSL] Kompilacja OpenSSL (libcrypto, libssl, CLI)..."
	@$(MAKE) -j$(JOBS) -C $(OPENSSL_BUILD_DIR) build_sw
	@$(LD) -shared -soname libcrypto.so.4 -o $(OPENSSL_BUILD_DIR)/libcrypto.so.4 --whole-archive $(OPENSSL_BUILD_DIR)/libcrypto.a --no-whole-archive -L$(abspath $(SYSROOT_DIR))/usr/lib -lc -lm
	@rm -f $(OPENSSL_BUILD_DIR)/libcrypto.so
	@ln -sf libcrypto.so.4 $(OPENSSL_BUILD_DIR)/libcrypto.so
	@rm -f $(SYSROOT_DIR)/usr/lib/libcrypto.so $(SYSROOT_DIR)/usr/lib/libcrypto.so.4 $(ROOTFS_DIR)/lib/libcrypto.so $(ROOTFS_DIR)/lib/libcrypto.so.4
	@cp -f $(OPENSSL_BUILD_DIR)/libcrypto.so.4 $(SYSROOT_DIR)/usr/lib/libcrypto.so.4
	@ln -sf libcrypto.so.4 $(SYSROOT_DIR)/usr/lib/libcrypto.so
	@cp -f $(OPENSSL_BUILD_DIR)/libcrypto.so.4 $(ROOTFS_DIR)/lib/libcrypto.so.4
	@ln -sf libcrypto.so.4 $(ROOTFS_DIR)/lib/libcrypto.so
	@cp -f $(OPENSSL_BUILD_DIR)/libcrypto.a $(SYSROOT_DIR)/usr/lib/ 2>/dev/null || true
	@cp -rf $(OPENSSL_BUILD_DIR)/include/openssl/* $(SYSROOT_DIR)/usr/include/openssl/ 2>/dev/null || true
	@cp -rf third_party/openssl/include/openssl/* $(SYSROOT_DIR)/usr/include/openssl/ 2>/dev/null || true
	@cp -f $(OPENSSL_BUILD_DIR)/*.pc $(SYSROOT_DIR)/usr/lib/pkgconfig/ 2>/dev/null || true
	@cp -f $(OPENSSL_BUILD_DIR)/*.pc $(SYSROOT_DIR)/usr/share/pkgconfig/ 2>/dev/null || true
	@$(LD) -shared -soname libssl.so.4 -o $(OPENSSL_BUILD_DIR)/libssl.so.4 --whole-archive $(OPENSSL_BUILD_DIR)/libssl.a --no-whole-archive -L$(OPENSSL_BUILD_DIR) -L$(abspath $(SYSROOT_DIR))/usr/lib -lcrypto -lc -lm
	@rm -f $(OPENSSL_BUILD_DIR)/libssl.so
	@ln -sf libssl.so.4 $(OPENSSL_BUILD_DIR)/libssl.so
	@rm -f $(SYSROOT_DIR)/usr/lib/libssl.so $(SYSROOT_DIR)/usr/lib/libssl.so.4 $(ROOTFS_DIR)/lib/libssl.so $(ROOTFS_DIR)/lib/libssl.so.4
	@cp -f $(OPENSSL_BUILD_DIR)/libssl.so.4 $(SYSROOT_DIR)/usr/lib/libssl.so.4
	@ln -sf libssl.so.4 $(SYSROOT_DIR)/usr/lib/libssl.so
	@cp -f $(OPENSSL_BUILD_DIR)/libssl.so.4 $(ROOTFS_DIR)/lib/libssl.so.4
	@ln -sf libssl.so.4 $(ROOTFS_DIR)/lib/libssl.so
	@cp -f $(OPENSSL_BUILD_DIR)/libssl.a $(SYSROOT_DIR)/usr/lib/ 2>/dev/null || true
	@$(CC) $(USER_CFLAGS) -nostdlib $(abspath $(SYSROOT_DIR))/usr/lib/crt0.o -o $(ROOTFS_DIR)/bin/openssl \
	    $(OPENSSL_APP_OBJS) \
	    $(OPENSSL_BUILD_DIR)/apps/libapps.a \
	    -L$(OPENSSL_BUILD_DIR) -L$(abspath $(SYSROOT_DIR))/usr/lib -lssl -lcrypto -ldl -lc -lm
	@touch $@

$(ROOTFS_DIR)/lib/libcrypto.so: $(OPENSSL_STAMP)
$(ROOTFS_DIR)/lib/libssl.so: $(OPENSSL_STAMP)
$(ROOTFS_DIR)/bin/openssl: $(OPENSSL_STAMP)

# ==============================================================================
# cURL (libcurl.so, /bin/curl with OpenSSL & Zlib support)
# ==============================================================================
third_party/curl/configure: third_party/curl/configure.ac
	@echo "  [PRECONF-CURL] Generowanie configure dla cURL..."
	@cd third_party/curl && autoreconf -fi 2>/dev/null || true

$(CURL_BUILD_DIR)/Makefile: third_party/curl/configure | $(ROOTFS_DIR)/lib/libssl.so $(ROOTFS_DIR)/lib/libcrypto.so $(LIBZ_A) $(SYSROOT_STAMP) $(CURL_BUILD_DIR)
	@echo "  [CONF-CURL] Konfiguracja cURL (Autotools cross-compile z OpenSSL)..."
	@cd $(CURL_BUILD_DIR) && \
	PKG_CONFIG_PATH="$(abspath $(SYSROOT_DIR))/usr/lib/pkgconfig:$(abspath $(SYSROOT_DIR))/usr/share/pkgconfig" \
	PKG_CONFIG_LIBDIR="$(abspath $(SYSROOT_DIR))/usr/lib/pkgconfig:$(abspath $(SYSROOT_DIR))/usr/share/pkgconfig" \
	$(abspath third_party/curl)/configure \
	    --host=x86_64-elf \
	    --prefix=/usr \
	    --with-openssl="$(abspath $(SYSROOT_DIR))/usr" \
	    --with-zlib="$(abspath $(SYSROOT_DIR))/usr" \
	    --without-libpsl \
	    --without-libidn2 \
	    --with-ca-bundle=/etc/ssl/cert.pem \
	    --with-ca-path=/etc/ssl/certs \
	    --enable-shared \
	    --disable-static \
	    --disable-symbol-hiding \
	    --disable-versioned-symbols \
	    --disable-manual \
	    --disable-ldap \
	    --disable-ldaps \
	    --disable-rtsp \
	    --disable-dict \
	    --disable-telnet \
	    --disable-tftp \
	    --disable-pop3 \
	    --disable-imap \
	    --disable-smtp \
	    --disable-gopher \
	    --disable-mqtt \
	    --disable-threaded-resolver \
	    --enable-http \
	    --enable-proxy \
	    CC="$(CC)" \
	    AR="$(AR)" \
	    RANLIB="$(RANLIB)" \
	    CFLAGS="-O2 -ffreestanding -fno-builtin -isystem $(abspath $(SYSROOT_DIR))/usr/include -B$(abspath $(SYSROOT_DIR))/usr/lib -fPIC" \
	    LDFLAGS="-nostdlib -L$(abspath $(SYSROOT_DIR))/usr/lib -B$(abspath $(SYSROOT_DIR))/usr/lib" \
	    LIBS="-lssl -lcrypto -lz -lm -lc"

$(CURL_BUILD_DIR):
	@mkdir -p $@

$(ROOTFS_DIR)/lib/libcurl.so: $(CURL_BUILD_DIR)/Makefile $(ROOTFS_DIR)/lib/libssl.so $(ROOTFS_DIR)/lib/libcrypto.so $(LIBZ_A)
	@mkdir -p $(CURL_BUILD_DIR) $(ROOTFS_DIR)/lib $(SYSROOT_DIR)/usr/lib $(SYSROOT_DIR)/usr/include/curl $(SYSROOT_DIR)/usr/lib/pkgconfig
	@echo "  [MAKE-LIBCURL] Kompilacja libcurl..."
	@$(MAKE) -j$(JOBS) -C $(CURL_BUILD_DIR)/lib
	@$(LD) -shared -soname libcurl.so.4 -o $(CURL_BUILD_DIR)/lib/libcurl.so.4 --whole-archive $(CURL_BUILD_DIR)/lib/.libs/libcurl.a --no-whole-archive -L$(abspath $(SYSROOT_DIR))/usr/lib -lssl -lcrypto -lz -lc -lm
	@rm -f $(CURL_BUILD_DIR)/lib/libcurl.so
	@ln -sf libcurl.so.4 $(CURL_BUILD_DIR)/lib/libcurl.so
	@rm -f $(SYSROOT_DIR)/usr/lib/libcurl.so $(SYSROOT_DIR)/usr/lib/libcurl.so.4 $(ROOTFS_DIR)/lib/libcurl.so $(ROOTFS_DIR)/lib/libcurl.so.4
	@cp -f $(CURL_BUILD_DIR)/lib/libcurl.so.4 $(SYSROOT_DIR)/usr/lib/libcurl.so.4
	@ln -sf libcurl.so.4 $(SYSROOT_DIR)/usr/lib/libcurl.so
	@cp -f $(CURL_BUILD_DIR)/lib/libcurl.so.4 $(ROOTFS_DIR)/lib/libcurl.so.4
	@ln -sf libcurl.so.4 $(ROOTFS_DIR)/lib/libcurl.so
	@cp -rf third_party/curl/include/curl/*.h $(SYSROOT_DIR)/usr/include/curl/ 2>/dev/null || true
	@cp -f $(CURL_BUILD_DIR)/libcurl.pc $(SYSROOT_DIR)/usr/lib/pkgconfig/ 2>/dev/null || true
	@cp -f $(CURL_BUILD_DIR)/libcurl.pc $(SYSROOT_DIR)/usr/share/pkgconfig/ 2>/dev/null || true

$(ROOTFS_DIR)/bin/curl: $(ROOTFS_DIR)/lib/libcurl.so $(ROOTFS_DIR)/lib/libssl.so $(ROOTFS_DIR)/lib/libcrypto.so $(LIBZ_A) $(CRT0_O) | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/bin
	@echo "  [MAKE-CURL] Kompilacja narzędzia CLI cURL..."
	@$(MAKE) -j$(JOBS) -C $(CURL_BUILD_DIR)/src curl-config2setopts.o curl-tool_main.o 2>/dev/null || true
	@$(MAKE) -j$(JOBS) -C $(CURL_BUILD_DIR)/src curl 2>/dev/null || true
	@$(CC) $(USER_CFLAGS) -nostdlib $(abspath $(SYSROOT_DIR))/usr/lib/crt0.o -o $@ \
	    $(CURL_BUILD_DIR)/src/curl-*.o \
	    $(CURL_BUILD_DIR)/src/toolx/curl-*.o \
	    $(CURL_BUILD_DIR)/lib/.libs/libcurlu.a \
	    -L$(CURL_BUILD_DIR)/lib -L$(abspath $(SYSROOT_DIR))/usr/lib -lcurl -lssl -lcrypto -lz -ldl -lc -lm

# ==============================================================================
# OpenSSH Portable (sshd, sshd-session, sshd-auth, ssh-keygen, ssh, sftp-server)
# ==============================================================================
third_party/openssh/configure: third_party/openssh/configure.ac
	@echo "  [PRECONF-OPENSSH] Generowanie configure dla OpenSSH..."
	@cd $(OPENSSH_SRC_DIR) && autoreconf -fi

$(OPENSSH_BUILD_DIR)/Makefile: third_party/openssh/configure | $(ROOTFS_DIR)/lib/libssl.so $(ROOTFS_DIR)/lib/libcrypto.so $(LIBZ_A) $(SYSROOT_STAMP) $(OPENSSH_BUILD_DIR)
	@echo "  [CONF-OPENSSH] Konfiguracja OpenSSH (Autotools cross-compile)..."
	@cd $(OPENSSH_BUILD_DIR) && \
	$(OPENSSH_SRC_DIR)/configure \
	    --host=x86_64-elf \
	    --prefix=/usr \
	    --sysconfdir=/etc/ssh \
	    --libexecdir=/usr/libexec \
	    --sbindir=/usr/sbin \
	    --bindir=/usr/bin \
	    --with-privsep-path=/var/empty \
	    --with-privsep-user=sshd \
	    --with-ssl-dir="$(abspath $(SYSROOT_DIR))/usr" \
	    --with-zlib="$(abspath $(SYSROOT_DIR))/usr" \
	    --with-sandbox=no \
	    --without-pam \
	    --without-selinux \
	    --disable-strip \
	    --disable-fd-passing \
	    CC="$(CC)" \
	    AR="$(AR)" \
	    RANLIB="$(RANLIB)" \
	    CFLAGS="-O2 -ffreestanding -fno-builtin -isystem $(abspath $(SYSROOT_DIR))/usr/include -B$(abspath $(SYSROOT_DIR))/usr/lib -fPIC" \
	    CPPFLAGS="-isystem $(abspath $(SYSROOT_DIR))/usr/include" \
	    LDFLAGS="-nostdlib $(abspath $(SYSROOT_DIR))/usr/lib/crt0.o -L$(abspath $(SYSROOT_DIR))/usr/lib -B$(abspath $(SYSROOT_DIR))/usr/lib" \
	    LIBS="-lssl -lcrypto -lz -lc -lm"
	@sed -i '' 's|/\* #undef HAVE_BZERO \*/|#define HAVE_BZERO 1|' $(OPENSSH_BUILD_DIR)/config.h 2>/dev/null || sed -i 's|/\* #undef HAVE_BZERO \*/|#define HAVE_BZERO 1|' $(OPENSSH_BUILD_DIR)/config.h
	@sed -i '' 's|/\* #undef HAVE_FSTATVFS \*/|#define HAVE_FSTATVFS 1|' $(OPENSSH_BUILD_DIR)/config.h 2>/dev/null || sed -i 's|/\* #undef HAVE_FSTATVFS \*/|#define HAVE_FSTATVFS 1|' $(OPENSSH_BUILD_DIR)/config.h
	@sed -i '' 's|/\* #undef HAVE_SYS_MOUNT_H \*/|#define HAVE_SYS_MOUNT_H 1|' $(OPENSSH_BUILD_DIR)/config.h 2>/dev/null || sed -i 's|/\* #undef HAVE_SYS_MOUNT_H \*/|#define HAVE_SYS_MOUNT_H 1|' $(OPENSSH_BUILD_DIR)/config.h
	@sed -i '' 's|/\* #undef HAVE_SETRESUID \*/|#define HAVE_SETRESUID 1|' $(OPENSSH_BUILD_DIR)/config.h 2>/dev/null || sed -i 's|/\* #undef HAVE_SETRESUID \*/|#define HAVE_SETRESUID 1|' $(OPENSSH_BUILD_DIR)/config.h
	@sed -i '' 's|/\* #undef HAVE_SETRESGID \*/|#define HAVE_SETRESGID 1|' $(OPENSSH_BUILD_DIR)/config.h 2>/dev/null || sed -i 's|/\* #undef HAVE_SETRESGID \*/|#define HAVE_SETRESGID 1|' $(OPENSSH_BUILD_DIR)/config.h
	@sed -i '' 's|/\* #undef NO_UID_RESTORATION_TEST \*/|#define NO_UID_RESTORATION_TEST 1|' $(OPENSSH_BUILD_DIR)/config.h 2>/dev/null || sed -i 's|/\* #undef NO_UID_RESTORATION_TEST \*/|#define NO_UID_RESTORATION_TEST 1|' $(OPENSSH_BUILD_DIR)/config.h
	@sed -i '' 's|/\* #undef DISABLE_FD_PASSING \*/|#define DISABLE_FD_PASSING 1|' $(OPENSSH_BUILD_DIR)/config.h 2>/dev/null || sed -i 's|/\* #undef DISABLE_FD_PASSING \*/|#define DISABLE_FD_PASSING 1|' $(OPENSSH_BUILD_DIR)/config.h

$(OPENSSH_BUILD_DIR):
	@mkdir -p $@

$(ROOTFS_DIR)/usr/sbin/sshd: $(OPENSSH_BUILD_DIR)/Makefile | $(ROOTFS_DIR)/lib/libssl.so $(ROOTFS_DIR)/lib/libcrypto.so $(LIBZ_A) $(SYSROOT_STAMP) $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/usr/sbin $(ROOTFS_DIR)/usr/bin $(ROOTFS_DIR)/usr/libexec $(ROOTFS_DIR)/bin $(ROOTFS_DIR)/etc/ssh $(ROOTFS_DIR)/var/empty
	@echo "  [MAKE-OPENSSH] Kompilacja OpenSSH (sshd, sshd-session, sshd-auth, ssh-keygen, ssh)..."
	@$(MAKE) -j$(JOBS) -C $(OPENSSH_BUILD_DIR) sshd sshd-session sshd-auth ssh-keygen ssh sftp-server
	@cp -f $(OPENSSH_BUILD_DIR)/sshd $(ROOTFS_DIR)/usr/sbin/sshd
	@cp -f $(OPENSSH_BUILD_DIR)/sshd-session $(ROOTFS_DIR)/usr/libexec/sshd-session
	@cp -f $(OPENSSH_BUILD_DIR)/sshd-auth $(ROOTFS_DIR)/usr/libexec/sshd-auth
	@cp -f $(OPENSSH_BUILD_DIR)/ssh-keygen $(ROOTFS_DIR)/usr/bin/ssh-keygen
	@cp -f $(OPENSSH_BUILD_DIR)/ssh $(ROOTFS_DIR)/usr/bin/ssh
	@cp -f $(OPENSSH_BUILD_DIR)/sftp-server $(ROOTFS_DIR)/usr/libexec/sftp-server
	@ln -sf /usr/sbin/sshd $(ROOTFS_DIR)/bin/sshd
	@ln -sf /usr/bin/ssh-keygen $(ROOTFS_DIR)/bin/ssh-keygen
	@ln -sf /usr/bin/ssh $(ROOTFS_DIR)/bin/ssh
	@chmod 755 $(ROOTFS_DIR)/var/empty

# ==============================================================================
# X.Org X11 Server (x11libre/xserver), Drivers & XKB Configuration
# ==============================================================================
XSERVER_BUILD_DIR := $(BUILD_DIR)/third_party/xserver
XKBCOMP_BUILD_DIR := $(BUILD_DIR)/third_party/xkbcomp
XKBCONFIG_BUILD_DIR := $(BUILD_DIR)/third_party/xkeyboard-config
MOUSE_BUILD_DIR := $(BUILD_DIR)/third_party/xf86-input-mouse
KBD_BUILD_DIR := $(BUILD_DIR)/third_party/xf86-input-keyboard

$(BUILD_DIR)/szpontos_cross.ini: scripts/szpontos_cross.ini | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)
	@sed -e 's|@ROOT_DIR@|$(abspath $(ROOT_DIR))|g' \
	     -e 's|@SYSROOT_DIR@|$(abspath $(SYSROOT_DIR))|g' $< > $@

third_party/xkbcomp/configure: third_party/xkbcomp/configure.ac
	@echo "  [PRECONF-XKBCOMP] Generowanie configure dla xkbcomp..."
	@cd third_party/xkbcomp && autoreconf -fi -I ../util-macros -I /opt/homebrew/share/aclocal 2>/dev/null || true

$(XKBCOMP_BUILD_DIR)/Makefile: third_party/xkbcomp/configure | $(ROOTFS_DIR)/lib/libxkbfile.so $(ROOTFS_DIR)/lib/libX11.so $(SYSROOT_STAMP) $(LIBC_SO) $(CRT0_O)
	@mkdir -p $(XKBCOMP_BUILD_DIR)
	@echo "  [CONF-XKBCOMP] Konfiguracja xkbcomp..."
	@cd $(XKBCOMP_BUILD_DIR) && \
	PKG_CONFIG_PATH="$(abspath $(SYSROOT_DIR))/usr/lib/pkgconfig:$(abspath $(SYSROOT_DIR))/usr/share/pkgconfig" \
	$(abspath third_party/xkbcomp)/configure \
	    --host=x86_64-elf \
	    --prefix=/usr \
	    --recheck 2>/dev/null || true
	@cd $(XKBCOMP_BUILD_DIR) && \
	PKG_CONFIG_PATH="$(abspath $(SYSROOT_DIR))/usr/lib/pkgconfig:$(abspath $(SYSROOT_DIR))/usr/share/pkgconfig" \
	$(abspath third_party/xkbcomp)/configure \
	    --host=x86_64-elf \
	    --prefix=/usr \
	    CC="$(CC)" \
	    CFLAGS="-O2 -ffreestanding -fno-builtin -isystem $(abspath $(SYSROOT_DIR))/usr/include -B$(abspath $(SYSROOT_DIR))/usr/lib" \
	    LDFLAGS="-nostdlib -L$(abspath $(SYSROOT_DIR))/usr/lib -B$(abspath $(SYSROOT_DIR))/usr/lib $(abspath $(SYSROOT_DIR))/usr/lib/crt0.o" \
	    LIBS="-lxkbfile -lX11 -lxcb -lXau -lXdmcp -lc -lm"

$(XKBCOMP_BUILD_DIR)/xkbcomp: $(XKBCOMP_BUILD_DIR)/Makefile
	@echo "  [MAKE-XKBCOMP] Kompilacja xkbcomp (-j$(JOBS))..."
	@$(MAKE) -j$(JOBS) -C $(XKBCOMP_BUILD_DIR)

$(ROOTFS_DIR)/usr/bin/xkbcomp: $(XKBCOMP_BUILD_DIR)/xkbcomp | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/usr/bin $(ROOTFS_DIR)/bin
	@cp -f $(XKBCOMP_BUILD_DIR)/xkbcomp $(ROOTFS_DIR)/usr/bin/xkbcomp
	@ln -sf /usr/bin/xkbcomp $(ROOTFS_DIR)/bin/xkbcomp

$(XKBCONFIG_BUILD_DIR)/build.ninja: | $(SYSROOT_STAMP)
	@mkdir -p $(XKBCONFIG_BUILD_DIR)
	@echo "  [CONF-XKBCONFIG] Konfiguracja xkeyboard-config (meson)..."
	@meson setup $(XKBCONFIG_BUILD_DIR) third_party/xkeyboard-config --prefix=/usr

$(ROOTFS_DIR)/usr/share/X11/xkb: $(XKBCONFIG_BUILD_DIR)/build.ninja | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/usr/share $(ROOTFS_DIR)/usr/share/X11
	@DESTDIR=$(SYSROOT_DIR) ninja -j$(JOBS) -C $(XKBCONFIG_BUILD_DIR) install >/dev/null 2>&1 || true
	@cp -rf $(SYSROOT_DIR)/usr/share/xkeyboard-config-2 $(ROOTFS_DIR)/usr/share/
	@cd $(ROOTFS_DIR)/usr/share/X11 && ln -sfn ../xkeyboard-config-2 xkb

$(XSERVER_BUILD_DIR)/build.ninja: $(BUILD_DIR)/szpontos_cross.ini | $(ROOTFS_DIR)/lib/libdrm.so $(ROOTFS_DIR)/lib/libgbm.so $(ROOTFS_DIR)/lib/libpixman-1.so $(ROOTFS_DIR)/lib/libxkbfile.so $(ROOTFS_DIR)/lib/libXfont2.so $(ROOTFS_DIR)/lib/libfontenc.so $(ROOTFS_DIR)/lib/libpciaccess.so $(SYSROOT_STAMP)
	@mkdir -p $(XSERVER_BUILD_DIR)
	@echo "  [CONF-XORG] Konfiguracja X.Org Server (meson cross-compile)..."
	@PKG_CONFIG_PATH="$(abspath $(SYSROOT_DIR))/usr/lib/pkgconfig:$(abspath $(SYSROOT_DIR))/usr/share/pkgconfig" \
	meson setup $(XSERVER_BUILD_DIR) third_party/xserver \
	    --cross-file $(BUILD_DIR)/szpontos_cross.ini \
	    -Dprefix=/usr \
	    -Dxorg=true \
	    -Dxephyr=false \
	    -Dxfbdev=false \
	    -Dxnest=false \
	    -Dxvfb=false \
	    -Dxwin=false \
	    -Dxquartz=false \
	    -Dglamor=false \
	    -Dglx=false \
	    -Ddri1=false \
	    -Ddri2=false \
	    -Ddri3=false \
	    -Dgbm=true \
	    -Dudev=false \
	    -Dudev_kms=false \
	    -Dhal=false \
	    -Dsystemd_logind=false \
	    -Dseatd_libseat=false \
	    -Dpciaccess=false \
	    -Dint10=false \
	    -Dvgahw=false \
	    -Dsuid_wrapper=false \
	    -Dlibunwind=false \
	    -Dtests=false \
	    -Ddocs=false \
	    -Ddevel-docs=false \
	    -Ddrm=true \
	    -Dmitshm=true \
	    -Dsha1=libc \
	    -Dlinux_acpi=false \
	    -Dlinux_apm=false \
	    -Db_lundef=false \
	    -Dxkb_dir=/usr/share/X11/xkb \
	    -Dxkb_bin_dir=/usr/bin \
	    -Dxkb_output_dir=/var/lib/xkb --reconfigure 2>/dev/null || \
	PKG_CONFIG_PATH="$(abspath $(SYSROOT_DIR))/usr/lib/pkgconfig:$(abspath $(SYSROOT_DIR))/usr/share/pkgconfig" \
	meson setup $(XSERVER_BUILD_DIR) third_party/xserver \
	    --cross-file $(BUILD_DIR)/szpontos_cross.ini \
	    -Dprefix=/usr \
	    -Dxorg=true \
	    -Dxephyr=false \
	    -Dxfbdev=false \
	    -Dxnest=false \
	    -Dxvfb=false \
	    -Dxwin=false \
	    -Dxquartz=false \
	    -Dglamor=false \
	    -Dglx=false \
	    -Ddri1=false \
	    -Ddri2=false \
	    -Ddri3=false \
	    -Dgbm=true \
	    -Dudev=false \
	    -Dudev_kms=false \
	    -Dhal=false \
	    -Dsystemd_logind=false \
	    -Dseatd_libseat=false \
	    -Dpciaccess=false \
	    -Dint10=false \
	    -Dvgahw=false \
	    -Dsuid_wrapper=false \
	    -Dlibunwind=false \
	    -Dtests=false \
	    -Ddocs=false \
	    -Ddevel-docs=false \
	    -Ddrm=true \
	    -Dmitshm=true \
	    -Dsha1=libc \
	    -Dlinux_acpi=false \
	    -Dlinux_apm=false \
	    -Db_lundef=false \
	    -Dxkb_dir=/usr/share/X11/xkb \
	    -Dxkb_bin_dir=/usr/bin \
	    -Dxkb_output_dir=/var/lib/xkb

$(XSERVER_BUILD_DIR)/hw/xfree86/Xorg: $(XSERVER_BUILD_DIR)/build.ninja
	@echo "  [NINJA-XORG] Kompilacja X.Org Server (ninja -j$(JOBS))..."
	@ninja -j$(JOBS) -C $(XSERVER_BUILD_DIR)

$(ROOTFS_DIR)/usr/bin/Xorg: $(XSERVER_BUILD_DIR)/hw/xfree86/Xorg | $(ROOTFS_DIR)
	@mkdir -p $(ROOTFS_DIR)/usr/bin $(ROOTFS_DIR)/bin $(ROOTFS_DIR)/usr/lib/xorg/modules/drivers $(ROOTFS_DIR)/usr/lib/xorg/modules/xlibre-25/drivers $(ROOTFS_DIR)/etc/X11 $(ROOTFS_DIR)/var/log $(ROOTFS_DIR)/var/lib/xkb
	@DESTDIR=$(SYSROOT_DIR) ninja -j$(JOBS) -C $(XSERVER_BUILD_DIR) install >/dev/null 2>&1 || true
	@cp -f $(XSERVER_BUILD_DIR)/hw/xfree86/Xorg $(ROOTFS_DIR)/usr/bin/Xorg
	@ln -sf /usr/bin/Xorg $(ROOTFS_DIR)/bin/Xorg
	@ln -sf /usr/bin/Xorg $(ROOTFS_DIR)/usr/bin/X
	@if [ -f $(XSERVER_BUILD_DIR)/hw/xfree86/drivers/video/modesetting/modesetting_drv.so ]; then \
		cp -f $(XSERVER_BUILD_DIR)/hw/xfree86/drivers/video/modesetting/modesetting_drv.so $(ROOTFS_DIR)/usr/lib/xorg/modules/drivers/ && \
		cp -f $(XSERVER_BUILD_DIR)/hw/xfree86/drivers/video/modesetting/modesetting_drv.so $(ROOTFS_DIR)/usr/lib/xorg/modules/xlibre-25/drivers/ && \
		cp -f $(XSERVER_BUILD_DIR)/hw/xfree86/drivers/video/modesetting/modesetting_drv.so $(ROOTFS_DIR)/usr/lib/xorg/modules/; \
	fi
	@cp -f $(XSERVER_BUILD_DIR)/hw/xfree86/dixmods/*.so $(ROOTFS_DIR)/usr/lib/xorg/modules/ 2>/dev/null || true
	@cp -f $(XSERVER_BUILD_DIR)/hw/xfree86/dixmods/*.so $(ROOTFS_DIR)/usr/lib/xorg/modules/xlibre-25/ 2>/dev/null || true
	@cp -f userland/skeleton/etc/X11/xorg.conf $(ROOTFS_DIR)/etc/X11/xorg.conf 2>/dev/null || true

MOUSE_SRCS := third_party/xf86-input-mouse/src/mouse.c third_party/xf86-input-mouse/src/pnp.c third_party/xf86-input-mouse/src/lnx_mouse.c
MOUSE_OBJS := $(patsubst third_party/xf86-input-mouse/src/%.c, $(MOUSE_BUILD_DIR)/%.o, $(MOUSE_SRCS))

$(MOUSE_BUILD_DIR):
	@mkdir -p $@

$(MOUSE_BUILD_DIR)/%.o: third_party/xf86-input-mouse/src/%.c | $(ROOTFS_DIR)/usr/bin/Xorg $(SYSROOT_STAMP) $(MOUSE_BUILD_DIR)
	@echo "  [CC-XORG-MOUSE] $<"
	@scripts/szpontos-gcc -fPIC -D__linux__=1 \
	    -Ithird_party/xf86-input-mouse/include \
	    -I$(SYSROOT_DIR)/usr/include/xorg \
	    -I$(SYSROOT_DIR)/usr/include/pixman-1 \
	    -I$(SYSROOT_DIR)/usr/include \
	    -DPACKAGE_VERSION_MAJOR=1 -DPACKAGE_VERSION_MINOR=9 -DPACKAGE_VERSION_PATCHLEVEL=5 -DPACKAGE_VERSION=\"1.9.5\" \
	    -c $< -o $@

$(ROOTFS_DIR)/usr/lib/xorg/modules/input/mouse_drv.so: $(MOUSE_OBJS) | $(ROOTFS_DIR) $(ROOTFS_DIR)/usr/bin/Xorg
	@mkdir -p $(ROOTFS_DIR)/usr/lib/xorg/modules/input $(ROOTFS_DIR)/usr/lib/xorg/modules/xlibre-25/input
	@echo "  [LD-XORG-MOUSE] $@"
	@scripts/szpontos-gcc -shared -fPIC -B$(SYSROOT_DIR)/usr/lib -L$(SYSROOT_DIR)/usr/lib -o $(MOUSE_BUILD_DIR)/mouse_drv.so $(MOUSE_OBJS)
	@cp -f $(MOUSE_BUILD_DIR)/mouse_drv.so $(ROOTFS_DIR)/usr/lib/xorg/modules/input/
	@cp -f $(MOUSE_BUILD_DIR)/mouse_drv.so $(ROOTFS_DIR)/usr/lib/xorg/modules/xlibre-25/input/
	@cp -f $(MOUSE_BUILD_DIR)/mouse_drv.so $(ROOTFS_DIR)/usr/lib/xorg/modules/

KBD_SRCS := third_party/xf86-input-keyboard/src/kbd.c third_party/xf86-input-keyboard/src/at_scancode.c third_party/xf86-input-keyboard/src/lnx_kbd.c third_party/xf86-input-keyboard/src/lnx_KbdMap.c
KBD_OBJS := $(patsubst third_party/xf86-input-keyboard/src/%.c, $(KBD_BUILD_DIR)/%.o, $(KBD_SRCS))

$(KBD_BUILD_DIR):
	@mkdir -p $@

$(KBD_BUILD_DIR)/%.o: third_party/xf86-input-keyboard/src/%.c | $(ROOTFS_DIR)/usr/bin/Xorg $(SYSROOT_STAMP) $(KBD_BUILD_DIR)
	@echo "  [CC-XORG-KBD] $<"
	@scripts/szpontos-gcc -fPIC -D__linux__=1 \
	    -I$(SYSROOT_DIR)/usr/include/xorg \
	    -I$(SYSROOT_DIR)/usr/include/pixman-1 \
	    -I$(SYSROOT_DIR)/usr/include \
	    -DPACKAGE_VERSION_MAJOR=1 -DPACKAGE_VERSION_MINOR=9 -DPACKAGE_VERSION_PATCHLEVEL=0 -DPACKAGE_VERSION=\"1.9.0\" \
	    -c $< -o $@

$(ROOTFS_DIR)/usr/lib/xorg/modules/input/kbd_drv.so: $(KBD_OBJS) | $(ROOTFS_DIR) $(ROOTFS_DIR)/usr/bin/Xorg
	@mkdir -p $(KBD_BUILD_DIR) $(ROOTFS_DIR)/usr/lib/xorg/modules/input $(ROOTFS_DIR)/usr/lib/xorg/modules/xlibre-25/input
	@echo "  [LD-XORG-KBD] $@"
	@scripts/szpontos-gcc -shared -fPIC -B$(SYSROOT_DIR)/usr/lib -L$(SYSROOT_DIR)/usr/lib -o $(KBD_BUILD_DIR)/kbd_drv.so $(KBD_OBJS)
	@cp -f $(KBD_BUILD_DIR)/kbd_drv.so $(ROOTFS_DIR)/usr/lib/xorg/modules/input/
	@cp -f $(KBD_BUILD_DIR)/kbd_drv.so $(ROOTFS_DIR)/usr/lib/xorg/modules/xlibre-25/input/
	@cp -f $(KBD_BUILD_DIR)/kbd_drv.so $(ROOTFS_DIR)/usr/lib/xorg/modules/

ALL_THIRDPARTY_OUTPUTS := \
	$(LIBNCURSES_A) $(LIBZ_A) $(ROOTFS_DIR)/bin/nano $(ROOTFS_DIR)/bin/file $(MAGIC_DB) \
	$(ROOTFS_DIR)/bin/zsh $(ROOTFS_DIR)/bin/fastfetch $(ROOTFS_DIR)/bin/git $(ALL_ROOTFS_SOS) \
	$(ROOTFS_DIR)/bin/xterm $(ROOTFS_DIR)/bin/openssl $(ROOTFS_DIR)/bin/curl $(ROOTFS_DIR)/etc/ssl/cert.pem \
	$(ROOTFS_DIR)/usr/bin/xkbcomp $(ROOTFS_DIR)/usr/share/X11/xkb $(ROOTFS_DIR)/usr/bin/Xorg \
	$(ROOTFS_DIR)/usr/lib/xorg/modules/input/mouse_drv.so $(ROOTFS_DIR)/usr/lib/xorg/modules/input/kbd_drv.so

$(THIRDPARTY_STAMP): $(ALL_THIRDPARTY_OUTPUTS)
	@mkdir -p $(dir $@)
	@touch $@

.PHONY: third-party
third-party: $(THIRDPARTY_STAMP)

# ==============================================================================
# Phony Target Aliases for Individual Components
# ==============================================================================
.PHONY: ncurses nano file zsh fastfetch zlib git \
        libXau libXdmcp libxcb libX11 libxkbfile libfontenc libXfont2 libxcvt \
        libpciaccess pixman libICE libSM libXpm libXext libXt libXmu libXaw \
        xterm openssl curl openssh xkbcomp xkeyboard-config xserver mouse-drv kbd-drv

ncurses: $(LIBNCURSES_A)
nano: $(ROOTFS_DIR)/bin/nano
file: $(ROOTFS_DIR)/bin/file $(MAGIC_DB)
zsh: $(ROOTFS_DIR)/bin/zsh
fastfetch: $(ROOTFS_DIR)/bin/fastfetch
zlib: $(ROOTFS_DIR)/lib/libz.so
git: $(ROOTFS_DIR)/bin/git
libXau: $(ROOTFS_DIR)/lib/libXau.so
libXdmcp: $(ROOTFS_DIR)/lib/libXdmcp.so
libxcb: $(ROOTFS_DIR)/lib/libxcb.so
libX11: $(ROOTFS_DIR)/lib/libX11.so
libxkbfile: $(ROOTFS_DIR)/lib/libxkbfile.so
libfontenc: $(ROOTFS_DIR)/lib/libfontenc.so
libXfont2: $(ROOTFS_DIR)/lib/libXfont2.so
libxcvt: $(ROOTFS_DIR)/lib/libxcvt.so
libpciaccess: $(ROOTFS_DIR)/lib/libpciaccess.so
pixman: $(ROOTFS_DIR)/lib/libpixman-1.so
libICE: $(ROOTFS_DIR)/lib/libICE.so
libSM: $(ROOTFS_DIR)/lib/libSM.so
libXpm: $(ROOTFS_DIR)/lib/libXpm.so
libXext: $(ROOTFS_DIR)/lib/libXext.so
libXt: $(ROOTFS_DIR)/lib/libXt.so
libXmu: $(ROOTFS_DIR)/lib/libXmu.so
libXaw: $(ROOTFS_DIR)/lib/libXaw.so
xterm: $(ROOTFS_DIR)/bin/xterm
openssl: $(ROOTFS_DIR)/bin/openssl $(ROOTFS_DIR)/lib/libcrypto.so $(ROOTFS_DIR)/lib/libssl.so
curl: $(ROOTFS_DIR)/bin/curl $(ROOTFS_DIR)/lib/libcurl.so
openssh: $(ROOTFS_DIR)/usr/sbin/sshd
xkbcomp: $(ROOTFS_DIR)/usr/bin/xkbcomp
xkeyboard-config: $(ROOTFS_DIR)/usr/share/X11/xkb
xserver: $(ROOTFS_DIR)/usr/bin/Xorg
mouse-drv: $(ROOTFS_DIR)/usr/lib/xorg/modules/input/mouse_drv.so
kbd-drv: $(ROOTFS_DIR)/usr/lib/xorg/modules/input/kbd_drv.so

