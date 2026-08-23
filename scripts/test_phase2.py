#!/usr/bin/env python3
"""
Test weryfikacyjny Phase 2: CSPRNG /dev/urandom, TmpFS, UNIX98 PTY/PTS w SzpontOS.
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

    print("[TEST] Uruchamianie QEMU do testu Fazy 2 (CSPRNG, TmpFS, PTY/PTS)...")
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
                    print("\n[TEST] Wysyłanie poleceń testowych dla Phase 2...")
                    commands = [
                        "randtest",
                        "tmpfstest",
                        "ptytest",
                        "ls -la /tmp",
                        "ls -la /dev",
                        "echo PHASE2_TEST_COMPLETED"
                    ]
                    for cmd in commands:
                        proc.stdin.write((cmd + "\n").encode('utf-8'))
                        proc.stdin.flush()
                        time.sleep(0.5)
                    commands_sent = True

                if "PHASE2_TEST_COMPLETED" in clean_chunk:
                    time.sleep(1.0)
                    break

    finally:
        proc.kill()
        proc.wait()

    print("\n\n" + "="*60)
    print("ANALIZA WYNIKÓW TESTU PHASE 2:")
    print("="*60)

    clean_output = re.sub(r'\x1b\[[0-9;?]*[a-zA-Z]', '', output)

    checks = [
        ("CSPRNG: /dev/urandom & getrandom()", "[RANDTEST] CSPRNG test PASSED" in clean_output),
        ("TmpFS: /tmp in-memory filesystem", "[TMPFSTEST] TmpFS In-Memory filesystem PASSED" in clean_output),
        ("UNIX98 PTY/PTS: /dev/ptmx master-slave communication", "[PTYTEST] UNIX98 PTY/PTS multiplexing PASSED" in clean_output),
        ("Weryfikacja urządzeń w /dev (urandom, random, ptmx)", "urandom" in clean_output and "ptmx" in clean_output)
    ]

    all_passed = True
    for desc, passed in checks:
        status = "[PASSED]" if passed else "[FAILED]"
        print(f"  {status} {desc}")
        if not passed:
            all_passed = False

    if all_passed:
        print("\n[OK] Wszystkie testy Phase 2 zakończone sukcesem!")
        sys.exit(0)
    else:
        print("\n[ERROR] Część testów Phase 2 nie powiodła się.")
        sys.exit(1)

if __name__ == "__main__":
    run_test()
