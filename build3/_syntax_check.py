# -*- coding: utf-8 -*-
import json, os, shlex, subprocess, sys

BASE = r"C:\Users\xingg\Desktop\知识体系化Wiki\Lv-00"
CCDB = os.path.join(BASE, "build3", "compile_commands.json")

targets = [
    "core/src/layer1_parser/dsl_compiler_load.c",
    "core/src/layer6_visual/runtime/block_scheduler.c",
    "core/src/core/lv_convenience.c",
    "core/src/layer4_reasoning/rewrite/rewrite_vf2.c",
    "core/src/layer4_reasoning/rewrite/beta_reduce.c",
    "core/src/layer4_reasoning/proof/proof_dependency.c",
    "core/src/layer4_reasoning/solver/solver_symbolic.c",
    "core/src/layer4_reasoning/model/relation_model.c",
]

with open(CCDB, "r", encoding="utf-8") as f:
    entries = json.load(f)

def norm(p):
    return os.path.normpath(p).replace("\\", "/")

fail = 0
for t in targets:
    abs_t = norm(os.path.join(BASE, t))
    hit = None
    for e in entries:
        f = norm(e.get("file", ""))
        if f == abs_t or f.endswith("/" + t):
            hit = e
            break
    if hit is None:
        print("NO-ENTRY", t)
        fail += 1
        continue
    alist = hit.get("arguments")
    if isinstance(alist, list) and alist:
        args = list(alist)
    else:
        args = shlex.split(hit["command"], posix=False)
    out = []
    skip_next = False
    for a in args:
        if skip_next:
            skip_next = False
            continue
        if a == "-o" or a == "-c":
            skip_next = (a == "-o")
            continue
        if a.startswith("-o") and len(a) > 2:
            continue
        if a in ("-MD", "-MMD", "-MP", "-Winvalid-pch"):
            continue
        if a.startswith("-MF") or a.startswith("-MT") or a.startswith("-Wl,"):
            continue
        out.append(a)
    cmd = out + ["-fsyntax-only"]
    r = subprocess.run(cmd, cwd=hit.get("directory", BASE), capture_output=True, text=True)
    err = (r.stderr or "").strip()
    if r.returncode == 0:
        print("PASS", t)
    else:
        print("FAIL", t, "rc=", r.returncode)
        print(err[-4000:])
        fail += 1
print("TOTAL FAILURES:", fail)
sys.exit(1 if fail else 0)