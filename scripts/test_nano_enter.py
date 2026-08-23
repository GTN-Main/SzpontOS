#!/usr/bin/env python3
"""
Test interaktywnego wprowadzania nowej linii i tekstu w GNU nano.
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

    print("[TEST] Uruchamianie QEMU do testu nowej linii w nano...")
    proc = subprocess.Popen(
        qemu_cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT
    )

    output = ""
    start_time = time.time()
    nano_launched = False
    text_typed = False
    nano_saved = False

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

                if not nano_launched and "root@szpontos-box" in clean_chunk and "Type 'help'" in clean_chunk:
                    time.sleep(0.5)
                    print("\n[TEST] Uruchamianie nano /test_doc.txt...")
                    proc.stdin.write(b"nano /test_doc.txt\n")
                    proc.stdin.flush()
                    nano_launched = True

                elif nano_launched and not text_typed and ("GNU nano" in clean_chunk or "test_doc.txt" in clean_chunk or "^X" in clean_chunk):
                    time.sleep(0.5)
                    print("\n[TEST] Wpisywanie pierwszej linii, znaku Enter (\\r) i drugiej linii...")
                    proc.stdin.write(b"Pierwsza Linia\r")
                    proc.stdin.flush()
                    time.sleep(0.3)
                    proc.stdin.write(b"Druga Linia\r")
                    proc.stdin.flush()
                    time.sleep(0.3)
                    text_typed = True

                    print("\n[TEST] Zapisywanie pliku (Ctrl+O) i wyjście (Ctrl+X)...")
                    proc.stdin.write(b"\x0f") # Ctrl+O
                    proc.stdin.flush()
                    time.sleep(0.3)
                    proc.stdin.write(b"\r")   # Enter to confirm filename
                    proc.stdin.flush()
                    time.sleep(0.3)
                    proc.stdin.write(b"\x18") # Ctrl+X
                    proc.stdin.flush()
                    nano_saved = True
                    time.sleep(0.5)
                    proc.stdin.write(b"cat /test_doc.txt\n")
                    proc.stdin.flush()
                    time.sleep(0.5)
                    proc.stdin.write(b"echo NANO_ENTER_TEST_COMPLETED\n")
                    proc.stdin.flush()

                elif nano_saved and "NANO_ENTER_TEST_COMPLETED" in output:
                    time.sleep(0.5)
                    break
            else:
                time.sleep(0.05)

    finally:
        proc.kill()

    print("\n--- Analiza wyniku ---")
    if "Pierwsza Linia" in output and "Druga Linia" in output and "NANO_ENTER_TEST_COMPLETED" in output:
        print("[PASS] Nowa linia w nano dziala poprawnie!")
        return True
    else:
        print("[FAIL] Brak oczekiwanych linii tekstu w pliku!")
        return False

if __name__ == "__main__":
    if not run_test():
        sys.exit(1)
