#!/usr/bin/env python3
with open(r"c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\core\src\layer1_parser\formula_converter.c", 'r', encoding='utf-8') as f:
    lines = f.readlines()

# Find switch statements
for i, line in enumerate(lines):
    stripped = line.strip()
    if stripped.startswith('switch ') and i > 100:
        print(f"Switch at line {i+1}: {stripped[:80]}")
