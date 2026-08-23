#!/usr/bin/env python3
"""
Test weryfikacyjny zgodności komendy uname ze standardem UNIX w SzpontOS.
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

    print("[TEST] Uruchamianie QEMU do testu komendy uname...")
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
                    print("\n[TEST] Wysyłanie poleceń testowych dla uname...")
                    commands = [
                        "uname",
                        "uname -s",
                        "uname -r",
                        "uname -n",
                        "uname -m",
                        "uname -a",
                        "uname -ar",
                        "uname -sr",
                        "uname -srm",
                        "/bin/uname",
                        "/bin/uname -s",
                        "/bin/uname -r",
                        "/bin/uname -n",
                        "/bin/uname -m",
                        "/bin/uname -a",
                        "/bin/uname -ar",
                        "/bin/uname -srm",
                        "echo UNAME_TEST_COMPLETED"
                    ]
                    for cmd in commands:
                        proc.stdin.write((cmd + "\n").encode('utf-8'))
                        proc.stdin.flush()
                        time.sleep(0.3)
                    commands_sent = True

                elif commands_sent and "UNAME_TEST_COMPLETED" in output:
                    time.sleep(0.5)
                    break
            else:
                time.sleep(0.05)

    finally:
        proc.kill()

    clean_output = re.sub(r'\x1b\[[0-9;?]*[a-zA-Z]', '', output)
    lines = clean_output.splitlines()

    print("\n--- Analiza wyników testu uname ---")
    checks = [
        ("Wbudowany: Domyślny uname zwraca 'SzpontOS'", any(line.strip() == "SzpontOS" for line in lines)),
        ("Wbudowany: uname -s zwraca 'SzpontOS'", any(line.strip() == "SzpontOS" for line in lines)),
        ("Wbudowany: uname -r zwraca '0.1.0'", any(line.strip() == "0.1.0" for line in lines)),
        ("Wbudowany: uname -n zwraca 'szpontos-box'", any(line.strip() == "szpontos-box" for line in lines)),
        ("Wbudowany: uname -m zwraca 'x86_64'", any(line.strip() == "x86_64" for line in lines)),
        ("Wbudowany: uname -a zwraca pełne info", any("SzpontOS" in line and "szpontos-box" in line and "0.1.0" in line and "x86_64" in line for line in lines)),
        ("Wbudowany: uname -ar zwraca pełne info", any("SzpontOS" in line and "szpontos-box" in line and "0.1.0" in line and "x86_64" in line for line in lines)),
        ("Wbudowany: uname -sr zwraca 'SzpontOS 0.1.0'", any(line.strip() == "SzpontOS 0.1.0" for line in lines)),
        ("Wbudowany: uname -srm zwraca 'SzpontOS 0.1.0 x86_64'", any(line.strip() == "SzpontOS 0.1.0 x86_64" for line in lines)),
        ("Binarny: /bin/uname zwraca 'SzpontOS'", any(line.strip() == "SzpontOS" for line in lines)),
        ("Binarny: /bin/uname -a zwraca pełne info", any("SzpontOS" in line and "szpontos-box" in line and "0.1.0" in line and "x86_64" in line for line in lines)),
        ("Binarny: /bin/uname -ar zwraca pełne info", any("SzpontOS" in line and "szpontos-box" in line and "0.1.0" in line and "x86_64" in line for line in lines)),
        ("Binarny: /bin/uname -srm zwraca 'SzpontOS 0.1.0 x86_64'", any(line.strip() == "SzpontOS 0.1.0 x86_64" for line in lines)),
        ("Pomyślne ukończenie całego pakietu testów", "UNAME_TEST_COMPLETED" in clean_output)
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
        print(">>> WSZYSTKIE TESTY KOMENDY UNAME PRZESZŁY 100%! <<<")
        print("================================================")
        return True
    return False

if __name__ == "__main__":
    if not run_test():
        sys.exit(1)
