#!/usr/bin/env python3
import subprocess
import time
import os
import sys
import select
import re

def test_ping():
    qemu_cmd = [
        "qemu-system-x86_64",
        "-M", "pc",
        "-cpu", "max",
        "-m", "512M",
        "-display", "none",
        "-cdrom", "build/szpontos.iso",
        "-drive", "file=build/disk.img,format=raw,if=ide,index=0,media=disk,snapshot=on,file.locking=off",
        "-netdev", "user,id=net0,hostfwd=tcp::8080-:80",
        "-device", "e1000,netdev=net0",
        "-serial", "stdio",
        "-no-reboot",
        "-no-shutdown"
    ]

    print("[TEST] Launching QEMU to test ping and RTC time...")
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
                    print("\n[TEST] Sending 'ping 127.0.0.1 -c 3'...")
                    proc.stdin.write(b"ping 127.0.0.1 -c 3\n")
                    proc.stdin.flush()
                    commands_sent = True

                elif commands_sent and "rtt min/avg/max" in output:
                    time.sleep(1.0)
                    break
            else:
                time.sleep(0.05)

    finally:
        proc.kill()

    clean_output = re.sub(r'\x1b\[[0-9;?]*[a-zA-Z]', '', output)
    if "rtt min/avg/max" in clean_output and "0% packet loss" in clean_output:
        print("\n[SUCCESS] RTC, gettimeofday/clock_gettime and ping RTT measurement verified!")
        return 0
    else:
        print("\n[FAILURE] Did not find expected RTT statistics.")
        return 1

if __name__ == "__main__":
    sys.exit(test_ping())
