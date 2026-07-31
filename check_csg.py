import io
SRC = r"c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\core\src\layer3_geometry\geometry_csg.c"
with io.open(SRC, "rb") as f:
    raw = f.read()
lines = raw.decode("utf-8-sig").splitlines(keepends=True)
print("total:", len(lines))
# 函数起始行附近检查段注释
for n in [248, 249, 360, 361, 453, 602, 815, 1361, 1557, 2021, 2174, 2222]:
    for j in range(n-2, n+1):
        print("   %d %r" % (j, lines[j-1].strip()[:70]))
    print()
