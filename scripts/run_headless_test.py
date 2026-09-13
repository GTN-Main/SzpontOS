#!/usr/bin/env python3
import subprocess
import time
import os
import sys
import select
import re

def main():
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

    print("[TEST] Running QEMU to test Xorg/xkbcomp...")
    proc = subprocess.Popen(
        qemu_cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT
    )

    output = ""
    start_time = time.time()

    try:
        while time.time() - start_time < 60:
            r, _, _ = select.select([proc.stdout], [], [], 0.2)
            if r:
                chunk = os.read(proc.stdout.fileno(), 1024).decode('utf-8', errors='ignore')
                if not chunk:
                    break
                output += chunk
                sys.stdout.write(chunk)
                sys.stdout.flush()

                if "Failed to activate virtual core keyboard" in output or "Szpont Experience on ':0'" in output:
                    time.sleep(2)
                    break
    finally:
        ret = proc.poll()
        print(f"\n[TEST] Process poll result: {ret}")
        proc.kill()
        proc.wait()

if __name__ == "__main__":
    main()
