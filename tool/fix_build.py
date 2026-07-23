#!/usr/bin/env python3
"""Fix Lv-00 CMake build: create missing .h/.c stubs + clean CMakeLists.txt."""
import os, re

BASE = r"C:\Users\xingg\Desktop\知识体系化Wiki\Lv-00"
CMAKE = os.path.join(BASE, "CMakeLists.txt")
INCLUDE = os.path.join(BASE, "core", "include", "lv")

# ── 1) Scan CMakeLists.txt for all referenced files and create missing stubs ──
with open(CMAKE, "r", encoding="utf-8") as f:
    cmake_text = f.read()

# Find all core/include/lv/*.h and core/src/**/*.c references
refs = set(re.findall(r'(core/(?:include/lv|src/[\w_/]+)/[\w_]+\.(?:h|c))', cmake_text))

created = 0
for ref in sorted(refs):
    path = os.path.join(BASE, ref)
    if os.path.exists(path):
        continue
    d = os.path.dirname(path)
    os.makedirs(d, exist_ok=True)
    name = os.path.splitext(os.path.basename(ref))[0]
    if ref.endswith('.h'):
        guard = "lv_" + name.upper() + "_H"
        content = f"#ifndef {guard}\n#define {guard}\n/* TODO: {name} stub */\n#ifdef __cplusplus\nextern \"C\" {{\n#endif\n#include <stdbool.h>\n#ifdef __cplusplus\n}}\n#endif\n#endif\n"
    else:
        content = f"/* Stub for {name} — TODO: implement */\n#include \"lv/lv.h\"\n"
    with open(path, "w", encoding="utf-8") as f:
        f.write(content)
    created += 1

print(f"Created {created} missing files")

# ── 2) Fix CMakeLists.txt: uncomment [QA] + # core/src lines ──
# [QA] file missing: core/src/XXX -> core/src/XXX  =>  core/src/XXX
cmake_text = re.sub(r'# \[QA\] file missing: (\S+) -> \S+', r'    \1', cmake_text)
# Uncomment # core/src/xxx.c (single # followed by 4 spaces + core/src)
cmake_text = re.sub(r'(?m)^    # (core/src/\S+\.c)', r'    \1', cmake_text)

with open(CMAKE, "w", encoding="utf-8") as f:
    f.write(cmake_text)

print("CMakeLists.txt fixed (QA comments uncommented)")
print("Done. Now run: cmake .. -G \"MinGW Makefiles\" -DBUILD_TESTS=ON && mingw32-make -j4")
