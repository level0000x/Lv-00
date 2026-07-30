import re
filepath = r'c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\core\src\layer1_parser\formula_renderer.c'
with open(filepath, 'r', encoding='utf-8') as f:
    content = f.read()

# Fix remaining } break; patterns
# Pattern: '    } break;\n    \n    /* XXX */\n    return written;\n}'
content = re.sub(
    r'    \} break;\s*\n\s*\n\s*/\*.*?\*/\s*\n    return written;\n\}',
    '    return written;\n}',
    content
)

with open(filepath, 'w', encoding='utf-8') as f:
    f.write(content)

remaining = content.count('} break;')
print(f"Fixed. Remaining '} break;' count: {remaining}")
