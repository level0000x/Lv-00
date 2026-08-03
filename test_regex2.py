import re, os

filepath = r'c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\core\src\layer4_reasoning\preset\preset_special_functions.c'
with open(filepath, 'r', encoding='utf-8') as f:
    content = f.read()

joined = content.replace('\\\n', ' ')

# Check if joined == content
print(f"joined == content: {joined == content}")
print(f"len joined: {len(joined)}, len content: {len(content)}")

# Test regex on joined
pattern = r'preset_blocks_register_by_category\s*\(([^)]+)\)'
matches = list(re.finditer(pattern, joined))
print(f"Found {len(matches)} matches in joined")

# Also try with re.MULTILINE
matches2 = list(re.finditer(pattern, joined, re.MULTILINE))
print(f"Found {len(matches2)} matches in joined with MULTILINE")

# Check if the issue is that the function call arguments span multiple lines
# Let's manually check the first call
idx = joined.find('preset_blocks_register_by_category')
if idx >= 0:
    # Show 300 chars from the match
    print(f"\nFirst 'preset_blocks_register_by_category' at position {idx}")
    print(f"Context: {repr(joined[idx:idx+200])}")