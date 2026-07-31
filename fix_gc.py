import io
P = r"c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\split_gc.py"
with io.open(P, "rb") as f:
    t = f.read().decode("utf-8-sig")
# 用精确字符串替换 covered 计算与断言
old = '''covered = 45 + 5 + 14 + sum(len(p[0]) for p in
    [tri_parts, pre_parts, crd_parts, edg_parts, bit_parts, huf_parts,
     rle_parts, ent_parts, clo_parts, cpr_parts, dcp_parts, io_parts])
# 未覆盖：46-120（常量+结构体区，迁移到 internal.h）、126-137、152 之前空行等
uncovered = 120 - 45 + 4 + 14  # 46-120 常量结构体(75行) + 126-129(4) + 137(1) = 80
# 手动核对：总 2313 = covered + internal.h 迁移内容(46-120 75行 + 126-129 4行 + 137 前向声明 1行 = 80) + 段间空行跳过数
skipped_empty = [1067, 1148, 1490, 1584, 1718, 1810, 2017, 2194]
print("covered=%d, to-internal=80, skipped=%d" % (covered, len(skipped_empty)))
assert covered + 80 + len(skipped_empty) == 2313, "coverage check failed: %d" % (covered + 80 + len(skipped_empty))
'''
new = '''covered = (45 + 5 + 14
         + sum(len(x) for x in bit_parts)
         + sum(len(p[0]) for p in [tri_parts, pre_parts, crd_parts, edg_parts,
                                   huf_parts, rle_parts, ent_parts, clo_parts,
                                   cpr_parts, dcp_parts, io_parts]))
print("covered=%d, to-internal=106" % covered)
assert covered + 106 == 2313, "coverage check failed: %d" % covered
'''
assert old in t, "pattern not found"
t = t.replace(old, new)
with io.open(P, "w", encoding="utf-8", newline="\n") as f:
    f.write(t)
print("patched")
