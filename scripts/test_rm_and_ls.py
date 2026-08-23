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
        ("ls /\n", "List root directory"),
        ("ls /proc\n", "List /proc directory"),
        ("touch /test_file.txt\n", "Create test file"),
        ("ls /\n", "List root with test file"),
        ("rm -v /test_file.txt\n", "Remove test file"),
        ("ls /\n", "List root after removal"),
        ("mkdir /mydir\n", "Create test dir"),
        ("touch /mydir/subfile.txt\n", "Create subfile"),
        ("rm -rv /mydir\n", "Recursive remove dir"),
        ("rm -f /nonexistent_file_xyz\n", "Force remove nonexistent"),
        ("ls /\n", "Final list root")
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

                if ("root@szpontos" in output or "#" in output) and "Type 'help'" in output:
                    if step < len(commands):
                        cmd, desc = commands[step]
                        time.sleep(0.35)
                        proc.stdin.write(cmd.encode('utf-8'))
                        proc.stdin.flush()
                        step += 1
                    elif step == len(commands):
                        time.sleep(1.0)
                        break
            else:
                time.sleep(0.05)
    finally:
        proc.kill()

    return output

if __name__ == "__main__":
    out = run_test()
    success_markers = [
        "proc/",
        "dev/",
        "cpuinfo",
        "meminfo",
        "modules",
        "removed '/test_file.txt'",
        "removed directory '/mydir'"
    ]
    all_passed = True
    for marker in success_markers:
        if marker not in out:
            print(f"[ERROR] Missing success marker: '{marker}'")
            all_passed = False

    if all_passed:
        print("\n[SUCCESS] All 'ls' /proc and 'rm' tests PASSED successfully!")
        sys.exit(0)
    else:
        print("\n[FAILURE] Some tests failed.")
        sys.exit(1)
