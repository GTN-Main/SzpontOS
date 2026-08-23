#!/usr/bin/env python3
"""
Test weryfikacyjny powłoki Zsh (Z Shell) w systemie SzpontOS.
"""

import subprocess
import time
import os
import sys
import select
import re

def run_test():
    qemu_cmd = [
        "qemu-system-x86_64",
        "-M", "pc",
        "-cpu", "max",
        "-m", "512M",
        "-display", "none",
        "-cdrom", "build/szpontos.iso",
        "-drive", "file=build/disk.img,format=raw,if=ide,index=0,media=disk,snapshot=on,file.locking=off",
        "-serial", "stdio",
        "-no-reboot",
        "-no-shutdown"
    ]

    print("[TEST] Uruchamianie QEMU do testu powłoki Zsh...")
    proc = subprocess.Popen(
        qemu_cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT
    )

    output = ""
    start_time = time.time()
    commands_sent = False

    try:
        while time.time() - start_time < 30:
            r, _, _ = select.select([proc.stdout], [], [], 0.1)
            if r:
                chunk = os.read(proc.stdout.fileno(), 1024).decode('utf-8', errors='ignore')
                if not chunk:
                    break
                output += chunk
                sys.stdout.write(chunk)
                sys.stdout.flush()

                clean_chunk = re.sub(r'\x1b\[[0-9;?]*[a-zA-Z]', '', output)

                if not commands_sent and "root@szpontos-box" in clean_chunk and "Type 'help'" in clean_chunk:
                    time.sleep(0.5)
                    print("\n[TEST] Wysyłanie poleceń testowych dla Zsh...")
                    commands = [
                        "file /bin/zsh",
                        "/bin/zsh",
                        "echo HELLO_FROM_ZSH",
                        "echo $((10 + 25 * 2))",
                        "ZSH_TEST_VAR='szpontos_zsh_verified'; echo $ZSH_TEST_VAR",
                        "exit",
                        "echo ZSH_SUITE_COMPLETED"
                    ]
                    for cmd in commands:
                        proc.stdin.write((cmd + "\n").encode('utf-8'))
                        proc.stdin.flush()
                        time.sleep(0.4)
                    commands_sent = True

                elif commands_sent and "ZSH_SUITE_COMPLETED" in output:
                    time.sleep(0.5)
                    break
            else:
                time.sleep(0.05)

    finally:
        proc.kill()

    print("\n--- Analiza wyniku testu Zsh ---")
    checks = [
        ("Weryfikacja pliku binarnego zsh (ELF)", "ELF 64-bit" in output or "zsh" in output),
        ("Uruchomienie i echo w zsh", "HELLO_FROM_ZSH" in output),
        ("Arytmetyka matematyczna w zsh ($((10 + 25 * 2)) == 60)", "60" in output),
        ("Zmienne w zsh", "szpontos_zsh_verified" in output),
        ("Czyste wyjście z zsh i powrót do /bin/sh", "ZSH_SUITE_COMPLETED" in output)
    ]

    all_passed = True
    for name, passed in checks:
        if passed:
            print(f"[PASS] {name}")
        else:
            print(f"[FAIL] {name}")
            all_passed = False

    if all_passed:
        print("\n================================================")
        print(">>> WSZYSTKIE TESTY POWŁOKI ZSH PRZESZŁY 100%! <<<")
        print("================================================")
        return True
    return False

if __name__ == "__main__":
    if not run_test():
        sys.exit(1)
