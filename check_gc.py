import io
SRC = r"c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\core\src\layer3_geometry\geometry_compress.c"
with io.open(SRC, "rb") as f:
    raw = f.read()
lines = raw.decode("utf-8-sig").splitlines(keepends=True)
print("total:", len(lines))
marks = [138,152,305,770,797,1069,1126,1149,1190,1341,1491,1585,1719,1811,2018,2195]
for m in marks:
    print("PRE@%d:" % m)
    for j in range(m-3, m):
        print("   %d %r" % (j, lines[j-1].strip()[:55]))
print("TAIL:", len(lines), repr(lines[-1].strip()[:55]))
