import subprocess
import time
import os
import sys
import select
import pty
import re

def run_test():
    master, slave = pty.openpty()

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
        stdin=slave,
        stdout=slave,
        stderr=slave,
        close_fds=True
    )
    os.close(slave)

    output = ""
    start_time = time.time()
    sent_command = False
    sent_time = 0

    try:
        while True:
            if sent_command and time.time() - sent_time > 8:
                break
            if not sent_command and time.time() - start_time > 25:
                break

            r, _, _ = select.select([master], [], [], 0.1)
            if r:
                try:
                    chunk = os.read(master, 1024).decode('utf-8', errors='ignore')
                    if not chunk:
                        break
                    output += chunk
                    sys.stdout.write(chunk)
                    sys.stdout.flush()

                    if not sent_command and "Type 'help'" in output and "# " in output:
                        print("\n[TEST] Sending '/bin/threadtest'...", flush=True)
                        os.write(master, b"/bin/threadtest\n")
                        sent_command = True
                        sent_time = time.time()

                    if "All POSIX Threads & Synchronization Tests PASSED!" in output:
                        time.sleep(0.5)
                        break
                except OSError:
                    break
            else:
                time.sleep(0.05)
    finally:
        proc.kill()
        os.close(master)

    return output

if __name__ == "__main__":
    out = run_test()
    if "All POSIX Threads & Synchronization Tests PASSED!" in out:
        print("\n[SUCCESS] Thread test verified successfully!")
        sys.exit(0)
    else:
        print("\n[FAILURE] Test did not produce expected success output.")
        sys.exit(1)
