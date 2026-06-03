#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
修复 symbolic_coord.c 中的内存管理函数
将所有 malloc/calloc/realloc/free 替换为 lv00_ 版本

用法：
    python fix_symbolic_coord.py              # 直接修改文件
    python fix_symbolic_coord.py --dry-run    # 仅预览修改，不写入文件

此脚本应在关闭所有编辑器的情况下运行，
以避免文件锁定导致写入失败。
"""

import re
import os
import sys
import argparse
import shutil


def main() -> None:
    """主函数：读取源文件，替换内存管理函数，写回文件。"""
    parser = argparse.ArgumentParser(
        description="修复 symbolic_coord.c 中的内存管理函数，替换为 lv00_ 版本"
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="仅预览修改内容，不实际写入文件",
    )
    args = parser.parse_args()

    SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'symbolic_coord.c')

    with open(SRC, 'r', encoding='utf-8') as f:
        content = f.read()

    changes = 0

    # 1. 替换 calloc -> lv00_calloc
    # 正则说明：(?<!lv00_) 是负向后行断言，确保匹配的 calloc 前面没有 lv00_ 前缀，
    # 避免将已经替换过的 lv00_calloc 再次匹配。
    new_content = re.sub(r'(?<!lv00_)calloc\(', 'lv00_calloc(', content)
    if new_content != content:
        c = len(re.findall(r'(?<!lv00_)calloc\(', content))
        changes += c
        print(f"  calloc -> lv00_calloc: {c} 处")
        content = new_content

    # 2. 替换 realloc -> lv00_realloc
    # 正则说明：同上，使用负向后行断言排除已有的 lv00_realloc。
    new_content = re.sub(r'(?<!lv00_)realloc\(', 'lv00_realloc(', content)
    if new_content != content:
        c = len(re.findall(r'(?<!lv00_)realloc\(', content))
        changes += c
        print(f"  realloc -> lv00_realloc: {c} 处")
        content = new_content

    # 3. 替换 malloc -> lv00_malloc (必须在 calloc/realloc 之后)
    # 正则说明：必须在 calloc 和 realloc 替换之后执行，因为 "malloc" 是 "calloc" 的子串，
    # 先替换 calloc 可以避免部分匹配问题。负向后行断言排除 lv00_malloc。
    new_content = re.sub(r'(?<!lv00_)malloc\(', 'lv00_malloc(', content)
    if new_content != content:
        c = len(re.findall(r'(?<!lv00_)malloc\(', content))
        changes += c
        print(f"  malloc -> lv00_malloc: {c} 处")
        content = new_content

    # 4. 替换 free(ptr) -> lv00_free((void **)&ptr)
    old_content = content
    # 正则说明：\b 确保匹配完整单词 "free"；\s* 允许自由空格；
    # [a-zA-Z_][a-zA-Z0-9_>*-]* 匹配 C 标识符（含指针符号 *>）
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
        # 正则说明：当精确模式无法匹配时（如参数为复杂表达式），使用 [^)]+ 匹配括号内任意内容
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

    if args.dry_run:
        print("\n[DRY RUN] 未实际修改文件。如需写入，请去掉 --dry-run 参数。")
        return

    if changes == 0:
        print("无需修改。")
        return

    # 创建 .bak 备份文件
    backup_path = SRC + '.bak'
    shutil.copy2(SRC, backup_path)
    print(f"已创建备份文件: {backup_path}")

    with open(SRC, 'w', encoding='utf-8') as f:
        f.write(content)

    print("文件已保存")


if __name__ == '__main__':
    main()
