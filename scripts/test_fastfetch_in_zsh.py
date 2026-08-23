#!/usr/bin/env python3
"""
Test weryfikacyjny wywołania Fastfetch z wnętrza powłoki Zsh w SzpontOS pod QEMU.
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

    print("[TEST] Uruchamianie QEMU do testu Fastfetch w Zsh...")
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
        while time.time() - start_time < 35:
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
                    print("\n[TEST] Uruchamianie Zsh i wywołanie Fastfetch...")
                    commands = [
                        "zsh",
                        "fastfetch --version",
                        "exit",
                        "echo ZSH_FASTFETCH_COMPLETED"
                    ]
                    for cmd in commands:
                        proc.stdin.write((cmd + "\n").encode('utf-8'))
                        proc.stdin.flush()
                        time.sleep(1.0)
                    commands_sent = True

                elif commands_sent and "ZSH_FASTFETCH_COMPLETED" in output:
                    time.sleep(1.0)
                    break
            else:
                time.sleep(0.05)

    finally:
        proc.kill()

    clean_output = re.sub(r'\x1b\[[0-9;?]*[a-zA-Z]', '', output)
    lines = clean_output.splitlines()

    version_found = any("fastfetch" in line.lower() for line in lines if "version" not in line.lower())
    completed = "ZSH_FASTFETCH_COMPLETED" in clean_output

    if completed and version_found:
        print("\n[SUCCESS] Fastfetch uruchomiony pomyślnie z poziomu Zsh!")
        return 0
    else:
        return 1

if __name__ == "__main__":
    sys.exit(run_test())
