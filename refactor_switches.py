#!/usr/bin/env python3
"""
Refactor 4 switch statements in formula_converter.c to use function pointer tables.
"""

import re

FILEPATH = r"c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\core\src\layer1_parser\formula_converter.c"

with open(FILEPATH, 'r', encoding='utf-8') as f:
    content = f.read()

lines = content.split('\n')
print(f"File has {len(lines)} lines")

# =====================================================
# Switch 1: graph_to_formula - node iteration switch
# Lines 1399-1603 (0-indexed: 1398-1602)
# =====================================================
# The switch text starts at line 1399 (index 1398):
# "        switch (node->type) {"
# and ends at line 1603 (index 1602):
# "        }"

# We need to:
# 1. Find the switch statement and extract all case bodies
# 2. Create 6 helper functions
# 3. Create the function table
# 4. Replace the switch with table lookup

# Let's find the exact switch text
switch1_start = None
switch1_end = None
for i, line in enumerate(lines):
    if 'switch (node->type) {' in line and i >= 1395 and i <= 1405:
        switch1_start = i
        break

if switch1_start is not None:
    # Find the matching closing brace
    depth = 0
    for i in range(switch1_start, len(lines)):
        line = lines[i]
        depth += line.count('{') - line.count('}')
        if depth <= 0 and '{' in lines[switch1_start]:
            # The opening { is on the switch line or after
            pass
        if depth == 0 and i > switch1_start:
            # Check if this line closes the switch
            if '}' in line and i > switch1_start + 1:
                switch1_end = i
                break

    print(f"Switch 1: lines {switch1_start+1}-{switch1_end+1}")
    print(f"  first line: {lines[switch1_start]}")
    print(f"  last line: {lines[switch1_end]}")
else:
    print("Switch 1 NOT FOUND!")
    # Try broader search
    for i, line in enumerate(lines):
        if 'switch (node->type)' in line:
            switch1_start = i
            break

print("\nLooking at surrounding context...")
for i in range(switch1_start - 3, switch1_start + 3):
    if 0 <= i < len(lines):
        print(f"  {i+1}: {lines[i]}")
