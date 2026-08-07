# -*- coding: utf-8 -*-
import io
s = "/* 线性 +1 扩容（ctx 无容量字段，改动最小：保持原样，不迁移到 lv_ensure_capacity） */"
with io.open(r"C:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\build3\_enc_probe.txt", "w", encoding="utf-8") as f:
    f.write(s)
with io.open(r"C:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\build3\_enc_probe.txt", "r", encoding="utf-8") as f:
    t = f.read()
print("MATCH" if t == s else "MISMATCH", len(s), t.encode("utf-8").hex()[:60])