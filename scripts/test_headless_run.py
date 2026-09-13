#!/usr/bin/env python3
import subprocess
import time
import os
import sys
import select

def main():
    cmd = ["./scripts/run_qemu.sh", "--headless"]
    proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

    start = time.time()
    desktop_time = None
    sent = False
    all_output = ""
    try:
        while time.time() - start < 45:
            r, _, _ = select.select([proc.stdout], [], [], 0.1)
            if r:
                data = os.read(proc.stdout.fileno(), 4096).decode('utf-8', errors='ignore')
                if not data:
                    break
                all_output += data
                sys.stdout.write(data)
                sys.stdout.flush()

                if desktop_time is None and "Desktop environment" in all_output:
                    desktop_time = time.time()

            if desktop_time and not sent and (time.time() - desktop_time > 2.0):
                print("\n[TEST] Desktop running! Sending commands to serial /bin/sh...\n", flush=True)
                proc.stdin.write(b"ls -la /var/lib/xkb\n")
                proc.stdin.flush()
                proc.stdin.write(b"head -n 60 /var/log/Xorg.0.log\n")
                proc.stdin.flush()
                sent = True
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=2)
        except Exception:
            proc.kill()

if __name__ == "__main__":
    main()
