import re

with open('lvFormal/Theory/LvDSL.lean', 'r') as f:
    content = f.read()

# Print all sorry locations
lines = content.split('\n')
for i, line in enumerate(lines):
    if 'sorry' in line:
        start = max(0, i-2)
        end = min(len(lines), i+1)
        for j in range(start, end):
            marker = '>>>' if j == i else '   '
            print(f'{marker} {j+1}: {lines[j]}')
        print('---')