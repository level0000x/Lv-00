import subprocess
exe = r"C:\Users\xingg\AppData\Local\Temp\lv_verify\_verify_circle.exe"
cmds = ["gdb", "-q", "-batch", "-ex", "run", "-ex", "bt 25", exe]
r = subprocess.run(cmds, capture_output=True, text=True, encoding="utf-8", errors="replace")
print(r.stdout)
print(r.stderr)