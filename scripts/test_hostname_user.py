#!/usr/bin/env python3
import subprocess
import time
import os
import sys
import select

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

    proc = subprocess.Popen(
        qemu_cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT
    )

    output = ""
    start_time = time.time()
    step = 0
    commands = [
        ("hostname\n", "Read hostname from /etc/hostname"),
        ("cat /etc/hostname\n", "Verify /etc/hostname file content"),
        ("uname -a\n", "Verify nodename in utsname"),
        ("whoami\n", "Check logged in user name"),
        ("hostname custom-host\n", "Set new hostname"),
        ("hostname\n", "Verify updated hostname"),
        ("cat /etc/hostname\n", "Verify updated /etc/hostname file"),
        ("echo HOSTNAME_USER_TEST_COMPLETED\n", "Test completion marker")
    ]

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

                if ("root@szpontos-box" in output or "#" in output) and "Type 'help'" in output:
                    if step < len(commands):
                        cmd, desc = commands[step]
                        time.sleep(0.4)
                        proc.stdin.write(cmd.encode('utf-8'))
                        proc.stdin.flush()
                        step += 1
                    elif step == len(commands) and "HOSTNAME_USER_TEST_COMPLETED" in output:
                        time.sleep(0.5)
                        break
            else:
                time.sleep(0.05)
    finally:
        proc.kill()

    return output

import re

if __name__ == "__main__":
    out = run_test()
    clean_out = re.sub(r'\x1b\[[0-9;]*[mK]', '', out)
    success_markers = [
        "root@szpontos-box",
        "szpontos-box",
        "root@custom-host",
        "custom-host",
        "HOSTNAME_USER_TEST_COMPLETED"
    ]
    all_passed = True
    for marker in success_markers:
        if marker in clean_out:
            print(f"[PASS] Found marker: '{marker}'")
        else:
            print(f"[FAIL] Missing marker: '{marker}'")
            all_passed = False

    if all_passed:
        print("\n==============================================")
        print(">>> ALL HOSTNAME & USER TESTS PASSED 100%! <<<")
        print("==============================================")
        sys.exit(0)
    else:
        print("\n[FAIL] Some tests failed.")
        sys.exit(1)
