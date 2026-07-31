import io, os

root = r"core\src"
files = []
for dirpath, _, fns in os.walk(root):
    for fn in fns:
        if fn.endswith(".c"):
            p = os.path.join(dirpath, fn)
            try:
                with io.open(p, "r", encoding="utf-8", errors="ignore") as f:
                    n = sum(1 for _ in f)
            except Exception:
                continue
            files.append((n, p))
files.sort(reverse=True)
for n, p in files[:25]:
    print("%5d  %s" % (n, p))
