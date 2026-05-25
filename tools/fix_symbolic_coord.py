#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
修复 symbolic_coord.c 中的内存管理函数
将所有 malloc/calloc/realloc/free 替换为 lv00_ 版本

用法：
    python fix_symbolic_coord.py

此脚本应在关闭所有编辑器的情况下运行，
以避免文件锁定导致写入失败。
"""

import re
import os


def main() -> None:
    """主函数：读取源文件，替换内存管理函数，写回文件。"""
    SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'symbolic_coord.c')

    with open(SRC, 'r', encoding='utf-8') as f:
        content = f.read()

    changes = 0

    # 1. 替换 calloc -> lv00_calloc
    new_content = re.sub(r'(?<!lv00_)calloc\(', 'lv00_calloc(', content)
    if new_content != content:
        c = len(re.findall(r'(?<!lv00_)calloc\(', content))
        changes += c
        print(f"  calloc -> lv00_calloc: {c} 处")
        content = new_content

    # 2. 替换 realloc -> lv00_realloc
    new_content = re.sub(r'(?<!lv00_)realloc\(', 'lv00_realloc(', content)
    if new_content != content:
        c = len(re.findall(r'(?<!lv00_)realloc\(', content))
        changes += c
        print(f"  realloc -> lv00_realloc: {c} 处")
        content = new_content

    # 3. 替换 malloc -> lv00_malloc (必须在calloc/realloc之后)
    new_content = re.sub(r'(?<!lv00_)malloc\(', 'lv00_malloc(', content)
    if new_content != content:
        c = len(re.findall(r'(?<!lv00_)malloc\(', content))
        changes += c
        print(f"  malloc -> lv00_malloc: {c} 处")
        content = new_content

    # 4. 替换 free(ptr) -> lv00_free((void **)&ptr)
    old_content = content
    # 匹配 free(identifier) 模式
    content = re.sub(
        r'\bfree\s*\(\s*([a-zA-Z_][a-zA-Z0-9_>*-]*)\s*\)\s*;',
        r'lv00_free((void **)&\1);',
        content
    )
    if old_content != content:
        c = len(re.findall(r'\bfree\s*\(\s*([a-zA-Z_][a-zA-Z0-9_>*-]*)\s*\)\s*;', old_content))
        changes += c
        print(f"  free -> lv00_free: {c} 处")
    else:
        # 尝试更宽泛的模式
        content = re.sub(
            r'\bfree\s*\(\s*([^)]+)\s*\)',
            r'lv00_free((void **)&\1)',
            content
        )
        d = len(re.findall(r'\bfree\s*\(\s*([^)]+)\s*\)', old_content)) - \
            len(re.findall(r'\bfree\s*\(\s*([^)]+)\s*\)', content))
        if d > 0:
            changes += d
            print(f"  free -> lv00_free (宽泛模式): {d} 处")

    print(f"\n总计修改 {changes} 处")

    with open(SRC, 'w', encoding='utf-8') as f:
        f.write(content)

    print("文件已保存")


if __name__ == '__main__':
    main()
