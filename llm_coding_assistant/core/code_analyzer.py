"""
LLM编程辅助系统 - 代码分析模块
提供代码分析、优化建议、错误诊断等功能
"""
import re
import ast
import json
from typing import Dict, Any, List, Optional, Tuple
from dataclasses import dataclass
from enum import Enum


class IssueSeverity(Enum):
    """问题严重程度"""
    INFO = "info"
    WARNING = "warning"
    ERROR = "error"
    CRITICAL = "critical"


class IssueCategory(Enum):
    """问题类别"""
    SYNTAX = "syntax"
    STYLE = "style"
    PERFORMANCE = "performance"
    SECURITY = "security"
    BEST_PRACTICE = "best_practice"
    COMPLEXITY = "complexity"


@dataclass
class CodeIssue:
    """代码问题"""
    line: int
    column: int
    severity: IssueSeverity
    category: IssueCategory
    message: str
    suggestion: str
    code_snippet: str = ""
    
    def to_dict(self) -> Dict[str, Any]:
        return {
            "line": self.line,
            "column": self.column,
            "severity": self.severity.value,
            "category": self.category.value,
            "message": self.message,
            "suggestion": self.suggestion,
            "code_snippet": self.code_snippet
        }


@dataclass
class AnalysisResult:
    """分析结果"""
    language: str
    total_lines: int
    code_lines: int
    comment_lines: int
    blank_lines: int
    issues: List[CodeIssue]
    metrics: Dict[str, Any]
    summary: str
    
    def to_dict(self) -> Dict[str, Any]:
        return {
            "language": self.language,
            "total_lines": self.total_lines,
            "code_lines": self.code_lines,
            "comment_lines": self.comment_lines,
            "blank_lines": self.blank_lines,
            "issues": [issue.to_dict() for issue in self.issues],
            "metrics": self.metrics,
            "summary": self.summary
        }


class CodeAnalyzer:
    """代码分析器"""
    
    def __init__(self):
        self.language_patterns = {
            "python": r"\.py$",
            "javascript": r"\.(js|jsx)$",
            "typescript": r"\.(ts|tsx)$",
            "java": r"\.java$",
            "cpp": r"\.(cpp|c\+\+|cc)$",
            "c": r"\.(c|h)$",
            "go": r"\.go$",
            "rust": r"\.rs$",
            "html": r"\.html?$",
            "css": r"\.css$",
            "json": r"\.json$",
            "yaml": r"\.(yaml|yml)$",
            "markdown": r"\.(md|markdown)$"
        }
    
    def detect_language(self, code: str, filename: str = "") -> str:
        """检测编程语言"""
        if filename:
            for lang, pattern in self.language_patterns.items():
                if re.search(pattern, filename, re.IGNORECASE):
                    return lang
        
        # 基于内容检测
        if "def " in code or "import " in code or "class " in code:
            if "{" not in code[:500]:
                return "python"
        
        if "function" in code or "const " in code or "let " in code:
            return "javascript"
        
        if "public class" in code or "private class" in code:
            return "java"
        
        return "unknown"
    
    def analyze(self, code: str, filename: str = "") -> AnalysisResult:
        """
        分析代码
        
        Args:
            code: 代码内容
            filename: 文件名（用于语言检测）
        
        Returns:
            AnalysisResult: 分析结果
        """
        language = self.detect_language(code, filename)
        
        # 基础统计
        lines = code.split('\n')
        total_lines = len(lines)
        blank_lines = sum(1 for line in lines if not line.strip())
        
        # 计算注释行数
        comment_lines = self._count_comment_lines(code, language)
        code_lines = total_lines - blank_lines - comment_lines
        
        # 分析代码问题
        issues = []
        
        if language == "python":
            issues.extend(self._analyze_python_syntax(code))
            issues.extend(self._analyze_python_style(code))
            issues.extend(self._analyze_python_complexity(code))
        
        # 通用分析
        issues.extend(self._analyze_common_issues(code, language))
        
        # 计算指标
        metrics = self._calculate_metrics(code, language, issues)
        
        # 生成摘要
        summary = self._generate_summary(language, total_lines, issues, metrics)
        
        return AnalysisResult(
            language=language,
            total_lines=total_lines,
            code_lines=code_lines,
            comment_lines=comment_lines,
            blank_lines=blank_lines,
            issues=issues,
            metrics=metrics,
            summary=summary
        )
    
    def _count_comment_lines(self, code: str, language: str) -> int:
        """计算注释行数"""
        lines = code.split('\n')
        comment_count = 0
        in_multiline_comment = False
        
        if language == "python":
            for line in lines:
                stripped = line.strip()
                if stripped.startswith('"""') or stripped.startswith("'''"):
                    if stripped.count('"""') < 2 and stripped.count("'''") < 2:
                        in_multiline_comment = not in_multiline_comment
                    comment_count += 1
                elif in_multiline_comment:
                    comment_count += 1
                elif self._is_python_comment(stripped):
                    comment_count += 1
        
        elif language in ["javascript", "typescript", "java", "cpp", "c", "go"]:
            for line in lines:
                stripped = line.strip()
                if '/*' in stripped:
                    in_multiline_comment = True
                if in_multiline_comment or stripped.startswith('//') or stripped.startswith('*'):
                    comment_count += 1
                if '*/' in stripped:
                    in_multiline_comment = False
        
        return comment_count

    @staticmethod
    def _is_python_comment(stripped_line: str) -> bool:
        """
        判断去除首尾空白的 Python 行是否为注释行

        排除字符串中包含 '#' 但并非注释的情况，
        例如: url = "https://example.com#anchor" 或 color = '#fff'
        """
        if not stripped_line.startswith('#'):
            return False
        # 如果行以 # 开头，但前面有奇数个引号，说明 # 在字符串内
        # 简单启发式：统计行中三引号和普通引号的数量来判断
        # 更可靠的方式：检查 # 之前是否有未闭合的字符串
        in_string = False
        quote_char = None
        i = 0
        while i < len(stripped_line):
            ch = stripped_line[i]
            if in_string:
                if ch == '\\':
                    i += 2  # 跳过转义字符
                    continue
                if ch == quote_char:
                    # 检查是否是三引号
                    if i + 2 < len(stripped_line) and stripped_line[i:i+3] == quote_char * 3:
                        in_string = False
                        i += 3
                        continue
                    in_string = False
            else:
                if ch in ('"', "'"):
                    # 检查是否是三引号
                    if i + 2 < len(stripped_line) and stripped_line[i:i+3] == ch * 3:
                        in_string = True
                        quote_char = ch * 3
                        i += 3
                        continue
                    in_string = True
                    quote_char = ch
            i += 1
        # 如果 # 不在字符串内，则是注释
        # 由于我们已经知道行以 # 开头，如果在字符串外就是注释
        return not in_string
    
    def _analyze_python_syntax(self, code: str) -> List[CodeIssue]:
        """分析Python语法问题"""
        issues = []
        lines = code.split('\n')

        try:
            ast.parse(code)
        except SyntaxError as e:
            issues.append(CodeIssue(
                line=e.lineno or 1,
                column=e.offset or 0,
                severity=IssueSeverity.ERROR,
                category=IssueCategory.SYNTAX,
                message=f"语法错误: {e.msg}",
                suggestion="请检查代码语法，确保括号、缩进等正确",
                code_snippet=lines[e.lineno - 1] if e.lineno and e.lineno <= len(lines) else ""
            ))

        # 检查未使用的导入
        try:
            tree = ast.parse(code)
            imports = []
            used_names = set()

            for node in ast.walk(tree):
                if isinstance(node, ast.Import):
                    for alias in node.names:
                        imports.append((alias.name, alias.asname or alias.name, node.lineno))
                elif isinstance(node, ast.ImportFrom):
                    for alias in node.names:
                        name = alias.asname or alias.name
                        imports.append((f"{node.module}.{alias.name}", name, node.lineno))
                elif isinstance(node, ast.Name):
                    used_names.add(node.id)

            for module, name, line in imports:
                if name not in used_names and name != '*':
                    issues.append(CodeIssue(
                        line=line,
                        column=0,
                        severity=IssueSeverity.WARNING,
                        category=IssueCategory.BEST_PRACTICE,
                        message=f"未使用的导入: {name}",
                        suggestion=f"删除未使用的导入: {module}",
                        code_snippet=lines[line - 1] if line <= len(lines) else ""
                    ))

        except SyntaxError:
            pass

        return issues
    
    def _analyze_python_style(self, code: str) -> List[CodeIssue]:
        """分析Python代码风格"""
        issues = []
        lines = code.split('\n')
        
        for i, line in enumerate(lines, 1):
            # 检查行长度
            if len(line) > 120:
                issues.append(CodeIssue(
                    line=i,
                    column=120,
                    severity=IssueSeverity.WARNING,
                    category=IssueCategory.STYLE,
                    message=f"行长度超过120字符 ({len(line)}字符)",
                    suggestion="将长行拆分为多行",
                    code_snippet=line[:100] + "..."
                ))
            
            # 检查尾随空格
            if line.rstrip() != line:
                issues.append(CodeIssue(
                    line=i,
                    column=len(line.rstrip()),
                    severity=IssueSeverity.INFO,
                    category=IssueCategory.STYLE,
                    message="行尾有尾随空格",
                    suggestion="删除行尾空格",
                    code_snippet=line
                ))
            
            # 检查使用 print（生产代码建议用日志）
            if re.search(r'\bprint\s*\(', line) and not line.strip().startswith('#'):
                issues.append(CodeIssue(
                    line=i,
                    column=line.find('print'),
                    severity=IssueSeverity.INFO,
                    category=IssueCategory.BEST_PRACTICE,
                    message="使用了 print 语句",
                    suggestion="建议使用 logging 模块替代 print",
                    code_snippet=line.strip()
                ))
            
            # 检查可变默认参数
            if re.search(r'def\s+\w+\s*\([^)]*=\s*(\[|\{)', line):
                issues.append(CodeIssue(
                    line=i,
                    column=0,
                    severity=IssueSeverity.WARNING,
                    category=IssueCategory.BEST_PRACTICE,
                    message="函数使用了可变默认参数（列表或字典）",
                    suggestion="使用 None 作为默认值，在函数内部初始化",
                    code_snippet=line.strip()
                ))
        
        return issues
    
    def _analyze_python_complexity(self, code: str) -> List[CodeIssue]:
        """分析Python代码复杂度"""
        issues = []
        
        try:
            tree = ast.parse(code)
            
            for node in ast.walk(tree):
                if isinstance(node, ast.FunctionDef):
                    # 计算圈复杂度（简化版）
                    complexity = 1
                    for child in ast.walk(node):
                        if isinstance(child, (ast.If, ast.While, ast.For, ast.ExceptHandler)):
                            complexity += 1
                        elif isinstance(child, ast.BoolOp):
                            complexity += len(child.values) - 1
                    
                    if complexity > 10:
                        issues.append(CodeIssue(
                            line=node.lineno,
                            column=0,
                            severity=IssueSeverity.WARNING,
                            category=IssueCategory.COMPLEXITY,
                            message=f"函数 '{node.name}' 圈复杂度过高 ({complexity})",
                            suggestion="考虑将函数拆分为多个小函数",
                            code_snippet=f"def {node.name}(...)"
                        ))
                    
                    # 检查函数长度
                    func_lines = node.end_lineno - node.lineno if node.end_lineno else 0
                    if func_lines > 50:
                        issues.append(CodeIssue(
                            line=node.lineno,
                            column=0,
                            severity=IssueSeverity.INFO,
                            category=IssueCategory.COMPLEXITY,
                            message=f"函数 '{node.name}' 过长 ({func_lines} 行)",
                            suggestion="考虑将函数拆分为多个小函数",
                            code_snippet=f"def {node.name}(...)"
                        ))
        
        except Exception:
            # AST 解析失败时跳过复杂度分析（如语法错误已在 _analyze_python_syntax 中报告）
            pass
        
        return issues
    
    def _analyze_common_issues(self, code: str, language: str) -> List[CodeIssue]:
        """分析通用问题"""
        issues = []
        lines = code.split('\n')
        
        # 检查硬编码的敏感信息
        sensitive_patterns = [
            (r'password\s*=\s*["\'][^"\']+["\']', "硬编码密码"),
            (r'api_key\s*=\s*["\'][^"\']+["\']', "硬编码API密钥"),
            (r'secret\s*=\s*["\'][^"\']+["\']', "硬编码密钥"),
            (r'token\s*=\s*["\'][^"\']+["\']', "硬编码Token"),
        ]
        
        for i, line in enumerate(lines, 1):
            for pattern, desc in sensitive_patterns:
                if re.search(pattern, line, re.IGNORECASE):
                    issues.append(CodeIssue(
                        line=i,
                        column=0,
                        severity=IssueSeverity.CRITICAL,
                        category=IssueCategory.SECURITY,
                        message=f"发现{desc}",
                        suggestion="使用环境变量或配置文件存储敏感信息",
                        code_snippet=line.strip()[:50] + "..."
                    ))
        
        # 检查TODO/FIXME/HACK/XXX注释
        for i, line in enumerate(lines, 1):
            todo_match = re.search(r'#\s*(TODO|FIXME|HACK|XXX)', line, re.IGNORECASE)
            if todo_match:
                tag = todo_match.group(1).upper()
                tag_messages = {
                    "TODO": "发现TODO注释",
                    "FIXME": "发现FIXME注释（需要修复的问题）",
                    "HACK": "发现HACK注释（临时解决方案）",
                    "XXX": "发现XXX注释（需要关注的问题）",
                }
                tag_suggestions = {
                    "TODO": "确保在发布前完成TODO事项",
                    "FIXME": "尽快修复此问题",
                    "HACK": "考虑用更规范的方案替代临时实现",
                    "XXX": "审查并处理此标记的问题",
                }
                issues.append(CodeIssue(
                    line=i,
                    column=todo_match.start(),
                    severity=IssueSeverity.INFO,
                    category=IssueCategory.BEST_PRACTICE,
                    message=tag_messages.get(tag, f"发现{tag}注释"),
                    suggestion=tag_suggestions.get(tag, "确保在发布前处理"),
                    code_snippet=line.strip()
                ))
        
        return issues
    
    def _calculate_metrics(self, code: str, language: str, issues: List[CodeIssue]) -> Dict[str, Any]:
        """计算代码指标"""
        lines = code.split('\n')
        
        # 统计各严重程度问题数量
        severity_counts = {s.value: 0 for s in IssueSeverity}
        category_counts = {c.value: 0 for c in IssueCategory}
        
        for issue in issues:
            severity_counts[issue.severity.value] += 1
            category_counts[issue.category.value] += 1
        
        # 计算代码质量分数（简化版）
        score = 100
        score -= severity_counts["critical"] * 20
        score -= severity_counts["error"] * 10
        score -= severity_counts["warning"] * 5
        score -= severity_counts["info"] * 1
        score = max(0, score)
        
        return {
            "quality_score": score,
            "issue_counts": severity_counts,
            "category_counts": category_counts,
            "total_issues": len(issues),
            "average_line_length": sum(len(line) for line in lines) / len(lines) if lines else 0
        }
    
    def _generate_summary(self, language: str, total_lines: int, 
                         issues: List[CodeIssue], metrics: Dict[str, Any]) -> str:
        """生成分析摘要"""
        parts = []
        
        parts.append(f"检测到 {language.upper()} 代码，共 {total_lines} 行")
        
        if metrics["total_issues"] == 0:
            parts.append("未发现明显问题，代码质量良好")
        else:
            parts.append(f"发现 {metrics['total_issues']} 个问题")
            
            critical = metrics["issue_counts"].get("critical", 0)
            errors = metrics["issue_counts"].get("error", 0)
            warnings = metrics["issue_counts"].get("warning", 0)
            
            if critical > 0:
                parts.append(f"⚠️ {critical} 个严重问题需要立即处理")
            if errors > 0:
                parts.append(f"❌ {errors} 个错误需要修复")
            if warnings > 0:
                parts.append(f"⚡ {warnings} 个警告建议优化")
        
        parts.append(f"代码质量评分: {metrics['quality_score']}/100")
        
        return "\n".join(parts)
    
    def generate_report(self, result: AnalysisResult, report_format: str = "markdown") -> str:
        """生成分析报告"""
        if report_format == "markdown":
            return self._generate_markdown_report(result)
        elif report_format == "json":
            return json.dumps(result.to_dict(), indent=2, ensure_ascii=False)
        else:
            return self._generate_text_report(result)
    
    def _generate_markdown_report(self, result: AnalysisResult) -> str:
        """生成Markdown格式报告"""
        lines = [
            "# 代码分析报告",
            "",
            f"## 概览",
            f"- **语言**: {result.language}",
            f"- **总行数**: {result.total_lines}",
            f"- **代码行数**: {result.code_lines}",
            f"- **注释行数**: {result.comment_lines}",
            f"- **空行**: {result.blank_lines}",
            f"- **质量评分**: {result.metrics['quality_score']}/100",
            "",
            "## 问题统计",
        ]
        
        for severity, count in result.metrics["issue_counts"].items():
            if count > 0:
                lines.append(f"- **{severity.upper()}**: {count}")
        
        if result.issues:
            lines.extend(["", "## 详细问题"])
            for issue in result.issues:
                lines.extend([
                    "",
                    f"### {issue.severity.value.upper()}: {issue.message}",
                    f"- **位置**: 第 {issue.line} 行, 第 {issue.column} 列",
                    f"- **类别**: {issue.category.value}",
                    f"- **建议**: {issue.suggestion}",
                ])
                if issue.code_snippet:
                    lines.extend([
                        "- **代码片段**:",
                        "```",
                        issue.code_snippet,
                        "```"
                    ])
        
        lines.extend(["", "## 摘要", result.summary])
        
        return "\n".join(lines)
    
    def _generate_text_report(self, result: AnalysisResult) -> str:
        """生成纯文本格式报告"""
        lines = [
            "=" * 50,
            "代码分析报告",
            "=" * 50,
            f"语言: {result.language}",
            f"总行数: {result.total_lines}",
            f"质量评分: {result.metrics['quality_score']}/100",
            f"问题总数: {result.metrics['total_issues']}",
            "-" * 50,
        ]
        
        if result.issues:
            lines.append("\n发现问题:")
            for issue in result.issues:
                lines.append(f"\n[{issue.severity.value.upper()}] 第{issue.line}行: {issue.message}")
                lines.append(f"  建议: {issue.suggestion}")
        
        lines.extend(["", result.summary, "=" * 50])
        
        return "\n".join(lines)
