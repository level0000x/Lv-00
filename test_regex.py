import re
import os

# Test with special_functions content
filepath = r'c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\core\src\layer4_reasoning\preset\preset_special_functions.c'
with open(filepath, 'r', encoding='utf-8') as f:
    content = f.read()

# Test the regex
pattern = r'preset_blocks_register_by_category\s*\(([^)]+)\)'
matches = list(re.finditer(pattern, content))
print(f"Found {len(matches)} matches with basic regex")

# Show first few
for m in matches[:3]:
    print(f"  Match: {m.group(0)[:100]}...")

# Also test with DOTALL
pattern2 = r'preset_blocks_register_by_category\s*\(([^)]+)\)'
matches2 = list(re.finditer(pattern2, content, re.DOTALL))
print(f"\nFound {len(matches2)} matches with DOTALL")

# Check if the issue is the BOM
print(f"\nFirst 10 bytes: {repr(content[:20])}")
print(f"File size: {len(content)}")