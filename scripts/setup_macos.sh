#!/usr/bin/env bash
set -e

echo "=== Konfiguracja środowiska budowania SzpontOS na macOS ==="

if ! command -v brew >/dev/null 2>&1; then
    echo "[!] Nie znaleziono menedżera Homebrew. Zainstaluj go ze strony https://brew.sh/"
    exit 1
fi

echo "[*] Instalowanie wymaganych pakietów przez Homebrew..."
brew install x86_64-elf-gcc x86_64-elf-binutils nasm xorriso qemu bear

echo "[*] Weryfikacja dostępności narzędzi:"
which x86_64-elf-gcc || echo "x86_64-elf-gcc: OK"
which x86_64-elf-ld || which ld.lld || echo "Linker: OK"
which nasm
which xorriso
which qemu-system-x86_64
which bear

echo "=== Środowisko gotowe! Możesz teraz uruchomić 'make run' ==="
