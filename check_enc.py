import io
SRC = r"c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\core\src\layer4_reasoning\proof_system\prop_verifier.c"
with io.open(SRC, "rb") as f:
    raw = f.read(2000)
print("BOM:", raw[:4])
# 尝试 UTF-8 解码
try:
    raw.decode("utf-8")
    print("UTF-8 decode: OK")
except UnicodeDecodeError as e:
    print("UTF-8 decode FAIL:", e)
# 尝试 GBK 解码
try:
    raw.decode("gbk")
    print("GBK decode: OK")
except UnicodeDecodeError as e:
    print("GBK decode FAIL:", e)
