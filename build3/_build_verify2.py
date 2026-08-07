# -*- coding: utf-8 -*-
import json, re, subprocess, sys, os, tempfile
root = r"c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00"
outdir = os.path.join(tempfile.gettempdir(), "lv_verify")
os.makedirs(outdir, exist_ok=True)
d = json.load(open(root + r"\build3\compile_commands.json", encoding="utf-8"))
gcc = r"C:\msys64\mingw64\bin\gcc.exe"
link_libs = "liblv.a C:/msys64/mingw64/lib/libgmp.a -lws2_32 -lkernel32 -luser32 -lgdi32 -lwinspool -lshell32 -lole32 -loleaut32 -luuid -lcomdlg32 -ladvapi32"
for src_name, base in [("circle_intersection.c", "_verify_circle"), ("streaming_demo.c", "_verify_streaming")]:
    entry = None
    for e in d:
        if e["file"].replace("\\", "/").endswith(src_name):
            entry = e
            break
    cmd = entry["command"]
    obj = os.path.join(outdir, base + ".obj")
    cmd = re.sub(r"-o\s+\S+", lambda m: "-o " + obj, cmd)
    r = subprocess.run(cmd, shell=True, cwd=root + r"\build3", capture_output=True, text=True, encoding="utf-8", errors="replace")
    print("=== compile %s: exit=%d ===" % (src_name, r.returncode))
    if (r.stdout + r.stderr).strip():
        print((r.stdout + r.stderr).strip()[-1500:])
    if r.returncode != 0:
        sys.exit(1)
    exe = os.path.join(outdir, base + ".exe")
    link_cmd = '%s -g "%s" -o "%s" %s' % (gcc, obj, exe, link_libs)
    r = subprocess.run(link_cmd, shell=True, cwd=root + r"\build3", capture_output=True, text=True, encoding="utf-8", errors="replace")
    print("=== link %s: exit=%d ===" % (base, r.returncode))
    if (r.stdout + r.stderr).strip():
        print((r.stdout + r.stderr).strip()[-1500:])
    if r.returncode != 0:
        sys.exit(1)
    print("built: %s" % exe)