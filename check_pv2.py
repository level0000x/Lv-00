import io
SRC = r"c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\core\src\layer4_reasoning\proof_system\prop_verifier.c"
with io.open(SRC, "rb") as f:
    raw = f.read()
lines = raw.decode("utf-8-sig").splitlines(keepends=True)
marks = [34,85,381,422,596,640,710,735,748,794,1174,1240,1492,1735,1949,2111]
for m in marks:
    print("SEG@%d: %r" % (m, lines[m].strip()[:60]))
    print("   prev2: %r | prev1: %r" % (lines[m-3].strip()[:40], lines[m-2].strip()[:40]))
print("tail:", len(lines), repr(lines[-1].strip()[:50]))
for n in range(len(lines)-3, len(lines)+1):
    print("   %d %r" % (n, lines[n-1].strip()[:50]))
