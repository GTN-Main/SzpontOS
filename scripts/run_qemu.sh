#!/usr/bin/env bash
set -e

ISO_PATH="build/szpontos.iso"
DISPLAY_OPT="cocoa"
EXTRA_FLAGS=()
VGA_FLAGS=("-vga" "std" "-global" "VGA.xres=1280" "-global" "VGA.yres=960")

# Detect Host OS
OS_TYPE="$(uname -s)"
if [ "$OS_TYPE" == "Darwin" ]; then
    DISPLAY_OPT="cocoa"
    CPU_TYPE="max"
else
    # Linux / BSD
    DISPLAY_OPT="default"
    # Check for KVM hardware acceleration on Linux
    if [ -w /dev/kvm ]; then
        echo "[*] Włączono sprzętową akcelerację KVM na Linuxie (-enable-kvm -cpu host)"
        EXTRA_FLAGS+=("-enable-kvm")
        CPU_TYPE="host"
    else
        CPU_TYPE="qemu64"
    fi
fi

# Parse arguments
for arg in "$@"; do
    case "$arg" in
        --debug)
            echo "[*] Tryb debugowania GDB włączony (QEMU czeka na porcie localhost:1234)..."
            EXTRA_FLAGS+=("-s" "-S")
            ;;
        --headless|--cli|-nographic)
            DISPLAY_OPT="none"
            ;;
        --virtio)
            VGA_FLAGS=("-vga" "virtio")
            ;;
        *.iso)
            ISO_PATH="$arg"
            ;;
    esac
done

if [ ! -f "$ISO_PATH" ]; then
    echo "[!] Nie znaleziono obrazu ISO: $ISO_PATH"
    echo "[*] Budowanie obrazu ISO..."
    make iso
fi

QEMU_CMD="qemu-system-x86_64"
if ! command -v $QEMU_CMD >/dev/null 2>&1; then
    if [ -x "/opt/homebrew/bin/qemu-system-x86_64" ]; then
        QEMU_CMD="/opt/homebrew/bin/qemu-system-x86_64"
    elif [ -x "/usr/bin/qemu-system-x86_64" ]; then
        QEMU_CMD="/usr/bin/qemu-system-x86_64"
    else
        echo "[!] Błąd: Nie znaleziono qemu-system-x86_64. Zainstaluj QEMU."
        exit 1
    fi
fi

DISK_PATH="build/disk.img"
if [ ! -f "$DISK_PATH" ]; then
    echo "[*] Generowanie obrazu dysku ext2: $DISK_PATH..."
    python3 scripts/make_ext2_disk.py "$DISK_PATH"
fi

if [ -f "$DISK_PATH" ]; then
    EXTRA_FLAGS+=("-drive" "file=$DISK_PATH,format=raw,if=ide,index=0,media=disk")
fi

# Intel E1000 Network Card with User-mode NAT and port forwarding (Host 8080 -> Guest 80)
EXTRA_FLAGS+=("-netdev" "user,id=net0,hostfwd=tcp::8080-:80" "-device" "e1000,netdev=net0")

exec $QEMU_CMD \
    -M pc \
    -cpu "$CPU_TYPE" \
    -m 512M \
    "${VGA_FLAGS[@]}" \
    -display "$DISPLAY_OPT" \
    -cdrom "$ISO_PATH" \
    -serial stdio \
    -no-shutdown \
    "${EXTRA_FLAGS[@]}"
