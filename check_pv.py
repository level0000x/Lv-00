import io, re
SRC = r"c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\core\src\layer4_reasoning\proof_system\prop_verifier.c"
with io.open(SRC, "rb") as f:
    raw = f.read()
text = raw.decode("utf-8-sig")
lines = text.splitlines(keepends=True)
print("total:", len(lines))
for i, ln in enumerate(lines, 1):
    s = ln.strip()
    if s.startswith("/* ===") or s.startswith("=== "):
        print(i, s[:70])
