import re

FILE = "C:/Users/xingg/Desktop/\u77e5\u8bc6\u4f53\u7cfb\u5316Wiki/Lv-00/core/src/layer1_parser/formula_renderer.c"

with open(FILE, "r", encoding="utf-8") as f:
    lines = f.read().split(chr(10))

print("Read " + str(len(lines)) + " lines")
