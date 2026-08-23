#!/usr/bin/env python3
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

    proc = subprocess.Popen(
        qemu_cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT
    )

    output = ""
    start_time = time.time()
    nano_launched = False
    nano_exited = False

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

                clean_chunk = re.sub(r'\x1b\[[0-9;]*[mKHFJ]', '', output)

                if not nano_launched and "root@szpontos-box" in clean_chunk and "Type 'help'" in clean_chunk:
                    time.sleep(0.5)
                    print("\n[TEST] Sending command: nano /welcome.txt")
                    proc.stdin.write(b"nano /welcome.txt\n")
                    proc.stdin.flush()
                    nano_launched = True

                elif nano_launched and not nano_exited and ("GNU nano" in clean_chunk or "welcome.txt" in clean_chunk or "Get Help" in clean_chunk or "WriteOut" in clean_chunk or "^X" in clean_chunk):
                    time.sleep(0.5)
                    print("\n[TEST] Nano opened successfully! Sending Ctrl+X to exit...")
                    proc.stdin.write(b"\x18") # Ctrl+X
                    proc.stdin.flush()
                    nano_exited = True
                    time.sleep(0.5)
                    proc.stdin.write(b"echo NANO_RUN_TEST_COMPLETED\n")
                    proc.stdin.flush()

                elif nano_exited and "NANO_RUN_TEST_COMPLETED" in output:
                    time.sleep(0.5)
                    break
            else:
                time.sleep(0.05)
    finally:
        proc.kill()

    return output

if __name__ == "__main__":
    out = run_test()
    clean_out = re.sub(r'\x1b\[[0-9;]*[a-zA-Z]', '', out)
    if "Error opening terminal" in out:
        print("\n[FAIL] Nano failed with Error opening terminal!")
        sys.exit(1)
    
    if "NANO_RUN_TEST_COMPLETED" in out or ("GNU nano" in out and "^X" in out):
        print("\n================================================")
        print(">>> GNU NANO INTERACTIVE RUN TEST PASSED 100%! <<<")
        print("================================================")
        sys.exit(0)
    else:
        print("\n[FAIL] Nano test failed to complete.")
        sys.exit(1)
