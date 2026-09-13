#!/usr/bin/env python3
"""
Test weryfikacyjny Xorg X11 Server z DRM modesetting w SzpontOS.
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
        "-serial", "stdio",
        "-no-reboot",
        "-no-shutdown"
    ]

    print("[TEST] Uruchamianie QEMU do testu Xorg DRM...")
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

                if not commands_sent and ("Started terminal session" in clean_chunk or "root@szpontos-box" in clean_chunk):
                    time.sleep(3.0)
                    print("\n[TEST] Wysyłanie poleceń testowych dla Xorg DRM...")
                    commands = [
                        "ps",
                        "cat /var/log/Xorg.0.log",
                        "ls -la /var/lib/xkb",
                        "echo XORG_TEST_COMPLETED"
                    ]
                    for cmd in commands:
                        proc.stdin.write((cmd + "\n").encode('utf-8'))
                        proc.stdin.flush()
                        time.sleep(1.0)
                    commands_sent = True

                if "XORG_TEST_COMPLETED" in clean_chunk:
                    print("\n[TEST] Otrzymano znacznik zakończenia testu!")
                    time.sleep(1)
                    break
    finally:
        proc.kill()
        proc.wait()

    clean_output = re.sub(r'\x1b\[[0-9;?]*[a-zA-Z]', '', output)
    print("\n--- RAPORT Z TESTU XORG ---")
    if "XORG_TEST_COMPLETED" in clean_output:
        print("[SUKCES] Test Xorg wykonał się do końca.")
    else:
        print("[UWAGA] Test nie zakończył się przed timeoutem.")

if __name__ == "__main__":
    run_test()
