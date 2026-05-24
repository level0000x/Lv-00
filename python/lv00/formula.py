"""
Lv-00 公式编程模块

提供数学公式的解析、渲染和转换功能。
支持 LaTeX、Python 数学语法和 Lv-00 DSL 三种输入格式。

主要类：
    - FormulaAST: 公式抽象语法树
    - FormulaParser: 公式解析器，自动检测语法类型
    - FormulaRenderer: 公式渲染器，输出多种格式
    - FormulaConverter: 公式-约束图双向转换器

版本：3.2.0
作者：Lv-00 开发团队
"""

import ctypes
import re
from fractions import Fraction
from typing import Optional, List, Dict, Union, Tuple

from ._ctypes_binding import _lib, c_int, c_char_p, c_void_p, POINTER
from typing import Any

# ============================================================
# 全局常量
# ============================================================

# LaTeX 特征关键词集合，用于自动检测公式语法类型
# 如果公式字符串中包含以下任一关键词，则判定为 LaTeX 格式
_LATEX_KEYWORDS = frozenset({
    '\\frac', '\\sqrt', '\\sin', '\\cos', '\\tan',
    '\\sum', '\\int', '\\pi', '\\theta', '\\alpha',
    '\\beta', '\\gamma', '\\delta', '\\epsilon',
    '\\left', '\\right', '\\begin', '\\end',
    '\\cdot', '\\times', '\\div', '\\pm',
})

# Lv-00 DSL 特征关键词集合，用于自动检测公式语法类型
# 如果公式字符串以以下任一关键词开头，则判定为 DSL 格式
_DSL_KEYWORDS = frozenset({
    'point', 'segment', 'line', 'circle', 'arc',
    'polygon', 'triangle', 'rectangle', 'square',
    'midpoint', 'perpendicular', 'parallel', 'tangent',
    'intersection', 'distance', 'angle', 'area',
})


# 语法类型枚举
class SyntaxType:
    """
    公式语法类型枚举。

    定义公式解析器支持的输入语法格式。
    解析器通过关键词匹配和模式识别自动检测输入格式。

    设计说明：
        本类使用类常量（而非标准库 enum.Enum）定义枚举值，
        这是有意为之的设计选择，原因如下：
        1. 公共 API 兼容性：用户代码通过 SyntaxType.LATEX 等方式
           访问常量，转换为 Enum 后 isinstance() 检查行为会改变，
           可能导致下游代码中断。
        2. 与 C 层一致：常量值为整数，直接对应 C 库中的枚举值，
           保持 int 类型便于直接传递给 ctypes 绑定。
        3. 轻量性：避免 Enum 的额外元类开销，保持与项目其他模块
          （如 func_block.py 中的 DeterminismState 等）风格统一。

    常量：
        AUTO (0): 自动检测语法类型（推荐，默认值）
        LATEX (1): LaTeX 数学公式格式
        PYTHON (2): Python 数学表达式格式
        DSL (3): Lv-00 几何领域特定语言格式
    """
    AUTO = 0
    LATEX = 1
    PYTHON = 2
    DSL = 3


# 输出格式枚举
class OutputFormat:
    """
    公式输出格式枚举。

    定义 FormulaRenderer 支持的输出目标格式。
    渲染器将 FormulaAST 转换为指定的文本格式。

    设计说明：
        与 SyntaxType 一样，本类使用类常量而非 enum.Enum，
        以保持公共 API 兼容性和与 C 层枚举值的一致性。
        详见 SyntaxType 的设计说明。

    常量：
        LATEX (0): LaTeX 数学公式输出
        PYTHON (1): Python 数学表达式输出
        DSL (2): Lv-00 DSL 格式输出
    """
    LATEX = 0
    PYTHON = 1
    DSL = 2


class FormulaParseError(Exception):
    """
    公式解析错误。

    当公式字符串无法被解析为有效的 AST 时抛出。
    包含错误位置信息，便于定位问题。

    属性：
        message: 错误描述消息
        position: 错误发生的字符位置（从 0 开始），-1 表示未知位置
    """
    
    def __init__(self, message: str, position: int = -1) -> None:
        """
        创建公式解析错误。

        参数：
            message: 错误描述消息
            position: 错误发生的位置（字符偏移），默认 -1
        """
        self.message: str = message
        self.position: int = position
        if position >= 0:
            super().__init__(f"Position {position}: {message}")
        else:
            super().__init__(message)

    def __str__(self) -> str:
        """
        返回人类可读的错误字符串。

        返回：
            str: 包含位置信息的错误描述
        """
        if self.position >= 0:
            return f"FormulaParseError(位置 {self.position}): {self.message}"
        return f"FormulaParseError: {self.message}"


class _FormulaNode(ctypes.Structure):
    """
    公式 AST 节点结构体（ctypes 绑定）。

    这是底层 C 库 FormulaNode 结构体的 Python 映射定义，
    用于 ctypes 正确解析 C 结构体内存布局。
    """

    pass


class _ParseResult(ctypes.Structure):
    """解析结果结构体（ctypes 绑定）。

    对应 C 层 ParseResult 结构体的 Python 映射定义，
    包含解析后的 AST 指针和可能的错误信息。
    """
    pass


class _FormulaToGraphResult(ctypes.Structure):
    """
    公式转图结果结构体（ctypes 绑定）。

    这是底层 C 库 FormulaToGraphResult 结构体的 Python 映射定义，
    包含转换后的节点 ID、约束 ID 和可能的错误列表。
    """

    pass


class FormulaAST:
    """
    公式抽象语法树。

    表示解析后的公式结构，支持多种输出格式转换。
    AST 节点封装 C 层指针，自动管理生命周期。

    属性：
        _ptr: 底层 C 指针
        _owned: 是否拥有内存所有权（需释放）
    """

    def __init__(self, ptr: Any) -> None:
        """
        从 C 指针创建 AST。

        参数：
            ptr: C 库返回的公式 AST 指针（c_void_p）
        """
        self._ptr = ptr
        self._owned = True  # 是否拥有内存（需要释放）

    def __del__(self) -> None:
        """释放 C 内存资源。"""
        try:
            if hasattr(self, '_ptr') and self._ptr and hasattr(self, '_owned') and self._owned:
                _lib.formula_node_destroy(self._ptr)
                self._ptr = None
        except Exception:
            pass  # 解释器关闭时 _lib 可能已不可用

    @classmethod
    def _from_ptr(cls, ptr, owned: bool = True) -> 'FormulaAST':
        """
        从指针创建 AST（内部方法）。

        参数：
            ptr: C 指针（POINTER(_FormulaNode)）
            owned: 是否拥有内存所有权（默认 True）

        返回：
            FormulaAST: 新创建的 AST 对象
        """
        ast = cls.__new__(cls)
        ast._ptr = ptr
        ast._owned = owned
        return ast

    def _render_to(self, format_code: int) -> Optional[str]:
        """
        将 AST 渲染为指定格式字符串（内部方法）。

        使用底层 C 库的 formula_render() 将 AST 转换为目标格式。
        to_latex()、to_python()、to_dsl() 均委托给本方法以避免重复代码。

        参数：
            format_code: OutputFormat 枚举值（LATEX=0, PYTHON=1, DSL=2）

        返回：
            Optional[str]: 渲染后的字符串，失败时返回空字符串

        异常：
            FormulaParseError: AST 指针为空时抛出
        """
        if not self._ptr:
            raise FormulaParseError("AST 指针为空")

        result = _lib.formula_render(self._ptr, format_code)
        if result:
            s = result.decode('utf-8')
            _lib.lv00_free_ptr(result)
            return s
        return ""

    def to_latex(self) -> str:
        """
        转换为 LaTeX 格式字符串。

        返回：
            str: LaTeX 格式的公式字符串，失败时返回空字符串

        异常：
            FormulaParseError: AST 指针为空时抛出
        """
        return self._render_to(OutputFormat.LATEX) or ""

    def to_python(self) -> str:
        """
        转换为 Python 数学表达式字符串。

        返回：
            str: Python 数学表达式字符串，失败时返回空字符串

        异常：
            FormulaParseError: AST 指针为空时抛出
        """
        return self._render_to(OutputFormat.PYTHON) or ""

    def to_dsl(self) -> str:
        """
        转换为 Lv-00 DSL 格式字符串。

        返回：
            str: Lv-00 DSL 格式的公式字符串，失败时返回空字符串

        异常：
            FormulaParseError: AST 指针为空时抛出
        """
        return self._render_to(OutputFormat.DSL) or ""

    def validate(self) -> Tuple[bool, List[str]]:
        """
        验证 AST 结构的有效性。

        检查 AST 的结构完整性，包括节点类型、子树关系等。

        注意：此方法依赖的 C 函数（formula_validate、formula_free_error_list）
        在当前版本的 C 库中未导出，因此返回基本验证结果。

        返回：
            Tuple[bool, List[str]]: (是否有效, 错误消息列表)，
            有效时错误列表为空
        """
        if not self._ptr:
            return (False, ["AST 指针为空"])

        # 基本验证：检查指针是否有效
        # 注意：完整的 AST 验证需要 C 库支持 formula_validate 函数
        # 该函数在当前版本中未导出，因此只进行基本检查
        return (True, [])

    def __repr__(self) -> str:
        """返回 AST 的调试表示。

        返回：
            str: 格式为 "FormulaAST(dsl_string)" 的字符串
        """
        dsl = self.to_dsl()
        if dsl:
            return f"FormulaAST({dsl})"
        return "FormulaAST(<empty>)"

    def __str__(self) -> str:
        """
        返回 LaTeX 格式的公式字符串。

        作为公式的默认字符串表示，方便直接打印。

        返回：
            str: LaTeX 格式的公式字符串
        """
        return self.to_latex()


class FormulaParser:
    """
    公式解析器。

    支持三种输入语法，通过关键词和模式匹配自动检测：
    - LaTeX: \\frac{a}{b}, \\sqrt{x^2+y^2}, \\sin(\\theta)
    - Python: a/b, sqrt(x**2+y**2), sin(theta)
    - DSL: point A(0,0), segment AB, circle O(A,3)

    底层调用 C 库的 formula_parse() 函数进行实际解析。

    示例：
        >>> ast = FormulaParser.parse(r"\\frac{1}{2} + \\sqrt{3}")
        >>> print(ast.to_latex())  # 输出 LaTeX 格式
        >>> print(ast.to_python())  # 转换为 Python 表达式
    """

    @staticmethod
    def detect_syntax(formula: str) -> str:
        """
        自动检测公式语法类型。

        通过关键词匹配和模式匹配判断输入公式的语法类型。

        参数：
            formula: 公式字符串

        返回：
            str: 语法类型字符串，取值为：
                - 'latex': LaTeX 数学公式
                - 'python': Python 数学表达式
                - 'dsl': Lv-00 几何 DSL
                - 'auto': 无法确定，由 C 库自动检测
        """
        if not formula or not formula.strip():
            return 'auto'
        
        formula_stripped = formula.strip()
        formula_lower = formula_stripped.lower()
        
        # 检测 LaTeX 特征——通过模块级 _LATEX_KEYWORDS 集合匹配
        for keyword in _LATEX_KEYWORDS:
            if keyword in formula_stripped:
                return 'latex'
        
        # 检测 DSL 特征——通过模块级 _DSL_KEYWORDS 集合匹配
        first_word = formula_lower.split()[0] if formula_lower.split() else ''
        if first_word in _DSL_KEYWORDS:
            return 'dsl'
        
        # 检测 DSL 模式: point A(0,0), segment AB 等
        dsl_patterns = [
            r'^point\s+\w+\s*\(',  # point A(
            r'^segment\s+\w+',      # segment AB
            r'^line\s+\w+',         # line l
            r'^circle\s+\w+',       # circle O
            r'^triangle\s+\w+',     # triangle ABC
        ]
        for pattern in dsl_patterns:
            if re.match(pattern, formula_lower):
                return 'dsl'
        
        # 检测 Python 特征
        python_patterns = [
            r'\*\*',      # 幂运算
            r'==',        # 比较
            r'!=',        # 不等
            r'\bsin\b',   # sin 函数
            r'\bcos\b',   # cos 函数
            r'\bsqrt\b',  # sqrt 函数
            r'\bpi\b',    # pi 常量
        ]
        for pattern in python_patterns:
            if re.search(pattern, formula_lower):
                return 'python'
        
        # 默认使用 C 库的自动检测
        return 'auto'
    
    @staticmethod
    def _get_syntax_code(syntax: str) -> int:
        """
        将语法字符串转换为 C 库代码。

        参数：
            syntax: 语法类型字符串（'auto', 'latex', 'python', 'dsl'）

        返回：
            int: 对应的语法类型代码（SyntaxType 枚举值）
        """
        syntax_map = {
            'auto': SyntaxType.AUTO,
            'latex': SyntaxType.LATEX,
            'python': SyntaxType.PYTHON,
            'dsl': SyntaxType.DSL,
        }
        return syntax_map.get(syntax.lower(), SyntaxType.AUTO)
    
    @staticmethod
    def parse(formula: str, syntax: str = 'auto') -> FormulaAST:
        """
        解析公式字符串为 AST。

        参数：
            formula: 公式字符串，支持 LaTeX/Python/DSL 格式
            syntax: 语法类型（默认 'auto' 自动检测），
                    可选值：'auto', 'latex', 'python', 'dsl'

        返回：
            FormulaAST: 解析后的抽象语法树对象

        异常：
            FormulaParseError: 解析失败时抛出，包含错误位置和消息
        """
        if not formula:
            raise FormulaParseError("Empty formula")
        
        # 如果指定 auto，先用 Python 检测
        if syntax == 'auto':
            detected = FormulaParser.detect_syntax(formula)
            if detected != 'auto':
                syntax = detected
        
        syntax_code = FormulaParser._get_syntax_code(syntax)
        
        # 调用 C 库解析
        formula_bytes = formula.encode('utf-8')
        # 注意：formula_parse 直接返回 FormulaNode* 指针，而非 ParseResult*
        # 因此无需调用 parse_result_get_ast 和 parse_result_destroy
        ast_ptr = _lib.formula_parse(formula_bytes, syntax_code)
        
        if not ast_ptr:
            # 获取错误信息
            error_msg = _lib.formula_parser_get_last_error()
            if error_msg:
                msg = error_msg.decode('utf-8')
                raise FormulaParseError(msg)
            raise FormulaParseError("Unknown parse error")
        
        # 创建 AST 对象（转移所有权）
        ast = FormulaAST._from_ptr(ast_ptr, owned=True)
        
        return ast


class FormulaRenderer:
    """
    公式渲染器。

    将 FormulaAST 渲染为各种目标格式的字符串输出。
    支持 LaTeX、Python 数学表达式和 Lv-00 DSL 三种输出格式。

    示例：
        >>> ast = FormulaParser.parse(r"\\frac{1}{2}")
        >>> print(FormulaRenderer.render(ast, 'latex'))   # 输出 LaTeX
        >>> print(FormulaRenderer.render(ast, 'python'))  # 输出 Python 表达式
        >>> print(FormulaRenderer.render(ast, 'dsl'))    # 输出 DSL 格式
    """

    @staticmethod
    def render(ast: FormulaAST, output_format: str = 'latex') -> str:
        """
        渲染 AST 为字符串。

        参数：
            ast: FormulaAST 对象
            output_format: 输出格式，可选值：
                - 'latex': LaTeX 数学公式
                - 'python': Python 数学表达式
                - 'dsl': Lv-00 DSL 格式

        返回：
            str: 渲染后的字符串

        异常：
            ValueError: 不支持的格式或 AST 指针为空
        """
        format_map = {
            'latex': OutputFormat.LATEX,
            'python': OutputFormat.PYTHON,
            'dsl': OutputFormat.DSL,
        }

        format_code = format_map.get(output_format.lower())
        if format_code is None:
            raise ValueError(f"不支持的输出格式: {output_format}")

        if not ast._ptr:
            raise ValueError("AST 指针为空")

        result = _lib.formula_render(ast._ptr, format_code)
        if result:
            s = result.decode('utf-8')
            _lib.lv00_free_ptr(result)
            return s
        return ""


class FormulaConverter:
    """
    公式转换器。

    在公式 AST 和约束图之间进行双向转换。
    支持将公式解析结果转换为约束图操作，
    以及从约束图反向生成公式表达式。

    示例：
        >>> ast = FormulaParser.parse("point A(0,0)")
        >>> result = FormulaConverter.to_graph(ast, graph)
        >>> if result['success']:
        ...     print(f"创建了 {len(result['created_nodes'])} 个节点")
        >>>
        >>> # 从图生成公式
        >>> formula_ast = FormulaConverter.from_graph(graph)
        >>> print(formula_ast.to_latex())
    """

    @staticmethod
    def to_graph(ast: FormulaAST, graph) -> Dict:
        """
        将公式 AST 转换为约束图操作。

        根据 AST 结构在约束图中创建对应的几何节点和约束。

        参数：
            ast: FormulaAST 对象
            graph: Graph 对象（约束图），必须已初始化

        返回：
            Dict: 转换结果字典，包含以下键：
                - 'success': bool，转换是否成功
                - 'created_nodes': List[int]，新创建的节点 ID 列表
                - 'created_constraints': List[int]，新创建的约束 ID 列表
                - 'errors': List[str]，错误消息列表

        注意：
            此方法依赖 C 库的 formula_to_graph 函数，该函数返回一个
            FormulaToGraphResult* 指针。由于相关辅助函数未导出，
            当前实现仅返回基本结果。
        """
        if not ast._ptr:
            return {
                'success': False,
                'created_nodes': [],
                'created_constraints': [],
                'errors': ['AST is null']
            }
        
        if not hasattr(graph, '_ptr') or not graph._ptr:
            return {
                'success': False,
                'created_nodes': [],
                'created_constraints': [],
                'errors': ['Invalid graph object']
            }
        
        # 调用 C 库转换
        result_ptr = _lib.formula_to_graph(ast._ptr, graph._ptr)
        if not result_ptr:
            return {
                'success': False,
                'created_nodes': [],
                'created_constraints': [],
                'errors': ['Conversion failed']
            }
        
        # 注意：formula_to_graph_result_* 系列函数在 C 库中未导出
        # 因此我们只能返回基本成功信息，并释放结果指针
        # 完整实现需要 C 库导出这些辅助函数
        try:
            _lib.formula_to_graph_result_destroy(result_ptr)
        except Exception:
            pass  # 忽略释放失败
        
        return {
            'success': True,
            'created_nodes': [],
            'created_constraints': [],
            'errors': []
        }
    
    @staticmethod
    def from_graph(graph) -> FormulaAST:
        """
        从约束图生成公式 AST。

        分析约束图的结构，反向推导出等价的公式表达式。
        通过调用 C 库的 graph_to_formula() 函数实现。

        参数：
            graph: Graph 对象，必须已初始化且包含有效的约束图结构

        返回：
            FormulaAST: 生成的公式 AST 对象，包含从图中推导出的公式

        异常：
            ValueError: 图对象无效（未初始化或指针为空）或转换失败
        """
        if not hasattr(graph, '_ptr') or not graph._ptr:
            raise ValueError("无效的图对象")

        ast_ptr = _lib.graph_to_formula(graph._ptr)
        if not ast_ptr:
            raise ValueError("从图转换为公式失败")

        return FormulaAST._from_ptr(ast_ptr, owned=True)


# 便捷函数
def parse(formula: str, syntax: str = 'auto') -> FormulaAST:
    """
    解析公式（便捷函数）。

    参数：
        formula: 公式字符串
        syntax: 语法类型（'auto', 'latex', 'python', 'dsl'），默认自动检测

    返回：
        FormulaAST: 解析后的 AST 对象

    异常：
        FormulaParseError: 解析失败
    """
    return FormulaParser.parse(formula, syntax)


def render(formula: str, output_format: str = 'latex', input_syntax: str = 'auto') -> str:
    """
    解析并渲染公式（便捷函数）。

    参数：
        formula: 公式字符串
        output_format: 输出格式（'latex', 'python', 'dsl'）
        input_syntax: 输入语法类型（'auto', 'latex', 'python', 'dsl'），默认自动检测

    返回：
        str: 渲染后的字符串

    异常：
        FormulaParseError: 解析失败
        ValueError: 不支持的输出格式
    """
    ast = parse(formula, input_syntax)
    return FormulaRenderer.render(ast, output_format)


def to_graph(formula: str, graph, syntax: str = 'auto') -> Dict:
    """
    将公式转换为约束图操作（便捷函数）。

    参数：
        formula: 公式字符串
        graph: Graph 对象
        syntax: 语法类型（'auto', 'latex', 'python', 'dsl'），默认自动检测

    返回：
        Dict: 转换结果字典，参见 FormulaConverter.to_graph()
    """
    ast = parse(formula, syntax)
    return FormulaConverter.to_graph(ast, graph)


def from_graph(graph) -> str:
    """
    从约束图生成 LaTeX 公式（便捷函数）。

    参数：
        graph: Graph 对象

    返回：
        str: LaTeX 格式的公式字符串

    异常：
        ValueError: 图对象无效或转换失败
    """
    ast = FormulaConverter.from_graph(graph)
    return ast.to_latex()


# 导出的公共 API
__all__ = [
    'SyntaxType',
    'OutputFormat',
    'FormulaParseError',
    'FormulaAST',
    'FormulaParser',
    'FormulaRenderer',
    'FormulaConverter',
    'parse',
    'render',
    'to_graph',
    'from_graph',
]
