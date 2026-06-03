#!/usr/bin/env python3
"""
Lv-00 代码清理脚本

功能：
1. 扫描并标记冗余代码、无效废弃代码
2. 检测未使用的函数和变量
3. 识别重复代码块
4. 检查过时的注释和文档
5. 生成代码清理报告

使用方法：
    python tool/scripts/cleanup_code.py [选项]

选项：
    --src-dir DIR       源代码目录 (默认: core/src)
    --include-dir DIR   头文件目录 (默认: core/include)
    --output FILE       输出报告文件 (默认: log/cleanup_report.md)
    --json FILE         输出 JSON 格式报告
    --threshold N       相似度阈值，用于重复代码检测 (默认: 0.8)
    --verbose           显示详细输出
    --fix               尝试自动修复一些问题

作者: Lv-00 Project
版本: 1.0.0
"""

import argparse
import ast
import hashlib
import json
import os
import re
import sys
from collections import defaultdict
from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple


@dataclass
class CodeIssue:
    """代码问题基类"""
    file_path: str
    line_number: int
    issue_type: str
    severity: str  # error, warning, info
    message: str
    suggestion: str = ""


@dataclass
class UnusedSymbol:
    """未使用符号"""
    name: str
    symbol_type: str  # function, variable, macro, type
    file_path: str
    line_number: int
    is_static: bool = False


@dataclass
class DeadCode:
    """废弃代码"""
    file_path: str
    start_line: int
    end_line: int
    code_type: str  # function, block, macro
    reason: str


@dataclass
class DuplicateCode:
    """重复代码"""
    original_file: str
    original_start: int
    duplicate_file: str
    duplicate_start: int
    similarity: float
    code_snippet: str


@dataclass
class CleanupReport:
    """代码清理报告"""
    scan_date: str
    total_files: int = 0
    total_lines: int = 0
    issues: List[CodeIssue] = field(default_factory=list)
    unused_symbols: List[UnusedSymbol] = field(default_factory=list)
    dead_code: List[DeadCode] = field(default_factory=list)
    duplicate_code: List[DuplicateCode] = field(default_factory=list)
    outdated_comments: List[CodeIssue] = field(default_factory.list)


def get_file_list(directory: str, extensions: Tuple[str, ...]) -> List[str]:
    """获取指定目录下所有指定扩展名的文件"""
    files = []
    for root, _, filenames in os.walk(directory):
        for filename in filenames:
            if filename.endswith(extensions):
                files.append(os.path.join(root, filename))
    return files


def read_file(file_path: str) -> str:
    """读取文件内容"""
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            return f.read()
    except Exception as e:
        print(f"Warning: Could not read {file_path}: {e}")
        return ""


def extract_c_functions(content: str) -> List[Tuple[str, int, bool]]:
    """
    提取 C 文件中的函数定义
    返回: [(函数名, 行号, 是否静态), ...]
    """
    functions = []

    # 匹配函数定义: static? return_type function_name(args) {
    # 简化版本，处理常见情况
    pattern = r'^(static\s+)?(?:const\s+)?(?:struct\s+\w+\s+)?(?:\w+\s+)+(\w+)\s*\([^)]*\)\s*\{'

    lines = content.split('\n')
    in_multiline_comment = False

    for i, line in enumerate(lines, 1):
        stripped = line.strip()

        # 处理多行注释
        if '/*' in stripped:
            if '*/' not in stripped:
                in_multiline_comment = True
            continue

        if in_multiline_comment:
            if '*/' in stripped:
                in_multiline_comment = False
            continue

        # 跳过单行注释
        if stripped.startswith('//') or stripped.startswith('*'):
            continue

        # 尝试匹配函数定义
        match = re.match(pattern, stripped)
        if match:
            is_static = match.group(1) is not None
            func_name = match.group(2)
            # 排除常见的非函数模式
            if func_name not in ('if', 'while', 'for', 'switch', 'return', 'sizeof'):
                functions.append((func_name, i, is_static))

    return functions


def extract_c_macros(content: str) -> List[Tuple[str, int]]:
    """提取 C 文件中的宏定义"""
    macros = []
    pattern = r'^#define\s+(\w+)'

    for i, line in enumerate(content.split('\n'), 1):
        match = re.match(pattern, line.strip())
        if match:
            macros.append((match.group(1), i))

    return macros


def extract_c_typedefs(content: str) -> List[Tuple[str, int]]:
    """提取 C 文件中的类型定义"""
    typedefs = []

    # 匹配 typedef ... name;
    patterns = [
        r'typedef\s+struct\s+\w+\s*\{[^}]*\}\s*(\w+)\s*;',
        r'typedef\s+enum\s*\{[^}]*\}\s*(\w+)\s*;',
        r'typedef\s+\w+\s+(\w+)\s*;',
        r'typedef\s+struct\s+(\w+)\s*;',
    ]

    for i, line in enumerate(content.split('\n'), 1):
        for pattern in patterns:
            match = re.search(pattern, line)
            if match:
                typedefs.append((match.group(1), i))
                break

    return typedefs


def check_symbol_usage(content: str, symbol: str) -> int:
    """检查符号在代码中的使用次数"""
    # 简单的词法匹配，不包括定义处
    pattern = r'\b' + re.escape(symbol) + r'\b'
    return len(re.findall(pattern, content))


def find_unused_symbols(file_path: str, content: str) -> List[UnusedSymbol]:
    """查找未使用的符号"""
    unused = []

    # 提取所有函数
    functions = extract_c_functions(content)
    for func_name, line_num, is_static in functions:
        if is_static:  # 只检查静态函数
            usage_count = check_symbol_usage(content, func_name)
            if usage_count <= 1:  # 只有定义，没有使用
                unused.append(UnusedSymbol(
                    name=func_name,
                    symbol_type='function',
                    file_path=file_path,
                    line_number=line_num,
                    is_static=True
                ))

    # 提取宏定义
    macros = extract_c_macros(content)
    for macro_name, line_num in macros:
        # 排除常见的保护宏
        if not macro_name.endswith('_H') and not macro_name.startswith('_'):
            usage_count = check_symbol_usage(content, macro_name)
            if usage_count <= 1:
                unused.append(UnusedSymbol(
                    name=macro_name,
                    symbol_type='macro',
                    file_path=file_path,
                    line_number=line_num
                ))

    return unused


def find_dead_code(file_path: str, content: str) -> List[DeadCode]:
    """查找废弃代码"""
    dead_code = []
    lines = content.split('\n')

    # 检查条件永远为假的代码块
    false_patterns = [
        (r'if\s*\(\s*0\s*\)', "Condition always false"),
        (r'if\s*\(\s*false\s*\)', "Condition always false"),
        (r'#if\s+0', "Preprocessor condition always false"),
    ]

    for i, line in enumerate(lines, 1):
        for pattern, reason in false_patterns:
            if re.search(pattern, line, re.IGNORECASE):
                # 找到代码块结束
                end_line = i
                brace_count = 0
                for j in range(i, len(lines)):
                    end_line = j
                    if '{' in lines[j]:
                        brace_count += lines[j].count('{')
                    if '}' in lines[j]:
                        brace_count -= lines[j].count('}')
                    if brace_count <= 0 and j > i:
                        break

                dead_code.append(DeadCode(
                    file_path=file_path,
                    start_line=i,
                    end_line=end_line,
                    code_type='block',
                    reason=reason
                ))

    # 检查标记为废弃的代码
    deprecated_pattern = r'(?:/\*\s*DEPRECATED|\/\/\s*DEPRECATED|#pragma\s+deprecated)'
    for i, line in enumerate(lines, 1):
        if re.search(deprecated_pattern, line, re.IGNORECASE):
            dead_code.append(DeadCode(
                file_path=file_path,
                start_line=i,
                end_line=i,
                code_type='deprecated',
                reason="Explicitly marked as deprecated"
            ))

    return dead_code


def find_duplicate_code(files: List[str], threshold: float = 0.8) -> List[DuplicateCode]:
    """查找重复代码"""
    duplicates = []
    code_blocks = []

    # 提取所有代码块（简化版本：按函数提取）
    for file_path in files:
        content = read_file(file_path)
        if not content:
            continue

        # 提取函数体
        func_pattern = r'((?:static\s+)?\w+\s+\w+\s*\([^)]*\)\s*\{[^}]*\})'
        for match in re.finditer(func_pattern, content, re.DOTALL):
            func_body = match.group(1)
            # 计算代码块的哈希
            normalized = ' '.join(func_body.split())  # 规范化空白
            code_hash = hashlib.md5(normalized.encode()).hexdigest()

            # 找到行号
            start_pos = match.start()
            line_num = content[:start_pos].count('\n') + 1

            code_blocks.append({
                'file': file_path,
                'line': line_num,
                'hash': code_hash,
                'content': func_body[:200]  # 只保留前200字符
            })

    # 查找重复
    seen = {}
    for block in code_blocks:
        if block['hash'] in seen:
            original = seen[block['hash']]
            duplicates.append(DuplicateCode(
                original_file=original['file'],
                original_start=original['line'],
                duplicate_file=block['file'],
                duplicate_start=block['line'],
                similarity=1.0,  # 完全相同的代码
                code_snippet=block['content'][:100]
            ))
        else:
            seen[block['hash']] = block

    return duplicates


def find_outdated_comments(file_path: str, content: str) -> List[CodeIssue]:
    """查找过时的注释"""
    issues = []
    lines = content.split('\n')

    # 检查 TODO/FIXME 注释
    todo_pattern = r'(?:TODO|FIXME|XXX|HACK)\s*[:\-]?\s*(.+)'

    for i, line in enumerate(lines, 1):
        match = re.search(todo_pattern, line, re.IGNORECASE)
        if match:
            issues.append(CodeIssue(
                file_path=file_path,
                line_number=i,
                issue_type='todo_comment',
                severity='info',
                message=f"TODO/FIXME found: {match.group(1).strip()}",
                suggestion="Review and address or remove outdated TODO items"
            ))

    # 检查空注释
    empty_comment_pattern = r'/\*\s*\*/|//\s*$'
    for i, line in enumerate(lines, 1):
        if re.search(empty_comment_pattern, line):
            issues.append(CodeIssue(
                file_path=file_path,
                line_number=i,
                issue_type='empty_comment',
                severity='warning',
                message="Empty comment found",
                suggestion="Remove empty comments or add meaningful content"
            ))

    # 检查版本过时的注释
    version_pattern = r'(?:version|v)\s*(\d+\.\d+)'
    for i, line in enumerate(lines, 1):
        match = re.search(version_pattern, line, re.IGNORECASE)
        if match:
            version = match.group(1)
            # 如果注释中的版本与当前项目版本差异很大，可能是过时的
            if version.startswith('2.') or version.startswith('1.'):
                issues.append(CodeIssue(
                    file_path=file_path,
                    line_number=i,
                    issue_type='outdated_version_comment',
                    severity='warning',
                    message=f"Comment references old version {version}",
                    suggestion="Update or remove outdated version reference"
                ))

    return issues


def generate_markdown_report(report: CleanupReport) -> str:
    """生成 Markdown 格式的报告"""
    lines = [
        "# Lv-00 代码清理报告",
        "",
        f"**生成时间**: {report.scan_date}",
        f"**扫描文件数**: {report.total_files}",
        f"**总代码行数**: {report.total_lines}",
        "",
        "## 摘要",
        "",
        f"- 发现问题: {len(report.issues)} 个",
        f"- 未使用符号: {len(report.unused_symbols)} 个",
        f"- 废弃代码块: {len(report.dead_code)} 个",
        f"- 重复代码: {len(report.duplicate_code)} 处",
        f"- 过时注释: {len(report.outdated_comments)} 个",
        "",
    ]

    # 未使用符号
    if report.unused_symbols:
        lines.extend([
            "## 未使用符号",
            "",
            "| 符号名 | 类型 | 文件 | 行号 | 静态 |",
            "|--------|------|------|------|------|",
        ])
        for sym in report.unused_symbols:
            static_mark = "是" if sym.is_static else "否"
            lines.append(f"| {sym.name} | {sym.symbol_type} | {sym.file_path} | {sym.line_number} | {static_mark} |")
        lines.append("")

    # 废弃代码
    if report.dead_code:
        lines.extend([
            "## 废弃代码",
            "",
            "| 文件 | 起始行 | 结束行 | 类型 | 原因 |",
            "|------|--------|--------|------|------|",
        ])
        for dc in report.dead_code:
            lines.append(f"| {dc.file_path} | {dc.start_line} | {dc.end_line} | {dc.code_type} | {dc.reason} |")
        lines.append("")

    # 重复代码
    if report.duplicate_code:
        lines.extend([
            "## 重复代码",
            "",
            "| 原文件 | 原位置 | 重复文件 | 重复位置 | 相似度 |",
            "|--------|--------|----------|----------|--------|",
        ])
        for dup in report.duplicate_code:
            similarity_pct = f"{dup.similarity * 100:.1f}%"
            lines.append(f"| {dup.original_file}:{dup.original_start} | | {dup.duplicate_file}:{dup.duplicate_start} | | {similarity_pct} |")
            lines.append(f"| | | | | 代码片段: `{dup.code_snippet[:50]}...` |")
        lines.append("")

    # 问题列表
    if report.issues:
        lines.extend([
            "## 代码问题",
            "",
            "| 文件 | 行号 | 类型 | 严重度 | 问题描述 | 建议 |",
            "|------|------|------|--------|----------|------|",
        ])
        for issue in report.issues:
            lines.append(f"| {issue.file_path} | {issue.line_number} | {issue.issue_type} | {issue.severity} | {issue.message} | {issue.suggestion} |")
        lines.append("")

    # 过时注释
    if report.outdated_comments:
        lines.extend([
            "## 过时/待处理注释",
            "",
            "| 文件 | 行号 | 类型 | 描述 |",
            "|------|------|------|------|",
        ])
        for comment in report.outdated_comments:
            lines.append(f"| {comment.file_path} | {comment.line_number} | {comment.issue_type} | {comment.message} |")
        lines.append("")

    # 建议
    lines.extend([
        "## 清理建议",
        "",
        "### 高优先级",
        "1. 删除确认未使用的静态函数",
        "2. 移除条件永远为假的代码块",
        "3. 合并或重构重复代码",
        "",
        "### 中优先级",
        "1. 更新过时的版本注释",
        "2. 处理 TODO/FIXME 注释",
        "3. 删除空注释",
        "",
        "### 低优先级",
        "1. 审查标记为废弃的代码",
        "2. 优化代码结构",
        "",
        "---",
        "*报告由 Lv-00 代码清理工具生成*",
    ])

    return '\n'.join(lines)


def generate_json_report(report: CleanupReport) -> str:
    """生成 JSON 格式的报告"""
    data = {
        'scan_date': report.scan_date,
        'total_files': report.total_files,
        'total_lines': report.total_lines,
        'summary': {
            'total_issues': len(report.issues),
            'unused_symbols': len(report.unused_symbols),
            'dead_code': len(report.dead_code),
            'duplicate_code': len(report.duplicate_code),
            'outdated_comments': len(report.outdated_comments),
        },
        'details': {
            'issues': [
                {
                    'file': i.file_path,
                    'line': i.line_number,
                    'type': i.issue_type,
                    'severity': i.severity,
                    'message': i.message,
                    'suggestion': i.suggestion,
                }
                for i in report.issues
            ],
            'unused_symbols': [
                {
                    'name': s.name,
                    'type': s.symbol_type,
                    'file': s.file_path,
                    'line': s.line_number,
                    'is_static': s.is_static,
                }
                for s in report.unused_symbols
            ],
            'dead_code': [
                {
                    'file': d.file_path,
                    'start_line': d.start_line,
                    'end_line': d.end_line,
                    'code_type': d.code_type,
                    'reason': d.reason,
                }
                for d in report.dead_code
            ],
            'duplicate_code': [
                {
                    'original_file': d.original_file,
                    'original_line': d.original_start,
                    'duplicate_file': d.duplicate_file,
                    'duplicate_line': d.duplicate_start,
                    'similarity': d.similarity,
                }
                for d in report.duplicate_code
            ],
            'outdated_comments': [
                {
                    'file': c.file_path,
                    'line': c.line_number,
                    'type': c.issue_type,
                    'message': c.message,
                }
                for c in report.outdated_comments
            ],
        }
    }
    return json.dumps(data, indent=2, ensure_ascii=False)


def main():
    parser = argparse.ArgumentParser(
        description='Lv-00 代码清理工具 - 扫描冗余代码、无效废弃代码并生成报告',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
示例:
    python tool/scripts/cleanup_code.py
    python tool/scripts/cleanup_code.py --src-dir core/src --output report.md
    python tool/scripts/cleanup_code.py --json report.json --verbose
        '''
    )

    parser.add_argument('--src-dir', default='core/src',
                        help='源代码目录 (默认: core/src)')
    parser.add_argument('--include-dir', default='core/include',
                        help='头文件目录 (默认: core/include)')
    parser.add_argument('--output', default='log/cleanup_report.md',
                        help='输出报告文件 (默认: log/cleanup_report.md)')
    parser.add_argument('--json', dest='json_output',
                        help='输出 JSON 格式报告')
    parser.add_argument('--threshold', type=float, default=0.8,
                        help='相似度阈值，用于重复代码检测 (默认: 0.8)')
    parser.add_argument('--verbose', action='store_true',
                        help='显示详细输出')
    parser.add_argument('--fix', action='store_true',
                        help='尝试自动修复一些问题 (谨慎使用)')

    args = parser.parse_args()

    # 确保输出目录存在
    output_dir = os.path.dirname(args.output)
    if output_dir and not os.path.exists(output_dir):
        os.makedirs(output_dir)

    if args.json_output:
        json_dir = os.path.dirname(args.json_output)
        if json_dir and not os.path.exists(json_dir):
            os.makedirs(json_dir)

    print("=" * 60)
    print("Lv-00 代码清理工具")
    print("=" * 60)
    print()

    # 创建报告
    report = CleanupReport(
        scan_date=datetime.now().isoformat()
    )

    # 获取源文件列表
    src_files = get_file_list(args.src_dir, ('.c', '.h'))
    include_files = get_file_list(args.include_dir, ('.h',))
    all_files = src_files + include_files

    report.total_files = len(all_files)

    if args.verbose:
        print(f"扫描目录: {args.src_dir}, {args.include_dir}")
        print(f"发现文件: {len(all_files)} 个")
        print()

    # 扫描每个文件
    for i, file_path in enumerate(all_files, 1):
        if args.verbose:
            print(f"[{i}/{len(all_files)}] 扫描: {file_path}")

        content = read_file(file_path)
        if not content:
            continue

        report.total_lines += len(content.split('\n'))

        # 查找未使用符号
        unused = find_unused_symbols(file_path, content)
        report.unused_symbols.extend(unused)

        # 查找废弃代码
        dead = find_dead_code(file_path, content)
        report.dead_code.extend(dead)

        # 查找过时注释
        outdated = find_outdated_comments(file_path, content)
        report.outdated_comments.extend(outdated)

    # 查找重复代码
    if args.verbose:
        print()
        print("分析代码重复...")

    duplicates = find_duplicate_code(all_files, args.threshold)
    report.duplicate_code.extend(duplicates)

    # 生成报告
    if args.verbose:
        print()
        print("生成报告...")

    markdown_report = generate_markdown_report(report)

    with open(args.output, 'w', encoding='utf-8') as f:
        f.write(markdown_report)

    print(f"Markdown 报告已保存: {args.output}")

    if args.json_output:
        json_report = generate_json_report(report)
        with open(args.json_output, 'w', encoding='utf-8') as f:
            f.write(json_report)
        print(f"JSON 报告已保存: {args.json_output}")

    # 打印摘要
    print()
    print("=" * 60)
    print("扫描摘要")
    print("=" * 60)
    print(f"扫描文件数: {report.total_files}")
    print(f"总代码行数: {report.total_lines}")
    print(f"未使用符号: {len(report.unused_symbols)} 个")
    print(f"废弃代码块: {len(report.dead_code)} 个")
    print(f"重复代码:   {len(report.duplicate_code)} 处")
    print(f"过时注释:   {len(report.outdated_comments)} 个")
    print()

    if report.unused_symbols or report.dead_code or report.duplicate_code:
        print("建议运行以下命令查看详细报告:")
        print(f"  cat {args.output}")
        return 1
    else:
        print("未发现明显问题，代码状态良好！")
        return 0


if __name__ == '__main__':
    sys.exit(main())
