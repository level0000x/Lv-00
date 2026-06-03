"""
Lv-00 核心模块 (优化版 v3.5.1)
==========================

模块功能概述：
    提供 Lv-00 几何元编程系统的核心 Python 类，是整个 Python 绑定层的基础。
    本模块封装了底层 C 库的核心数据结构和操作，提供符号精确计算、几何对象
    管理和约束图构建等能力，是所有上层模块（engine、func_block、dsl_wrappers
    等）的依赖基础。

核心类列表：
    - SymbolicCoord: 符号坐标，支持有理数、代数数和超越数的精确表示与运算
    - Point: 几何点，由两个 SymbolicCoord 构成
    - LineSegment: 线段，由两个端点定义
    - Graph: 约束图，表示完整的几何构造，管理节点和约束关系
    - GeomNode: 几何节点包装类，统一表示点、线段、端口、函数块等几何实体
    - NormalizationResult: 规范化结果类，封装求解/规范化操作的输出
    - Lv00BaseError: 统一异常基类，提供 message 和 error_code 属性
    - Lv00Error: 核心模块异常基类，涵盖库错误、参数错误、约束冲突等
    - Lv00LibraryError: 库加载或初始化失败异常

使用示例：
    >>> from lv00.core import Graph, Point, SymbolicCoord
    >>>
    >>> # 创建约束图并添加几何对象
    >>> g = Graph()
    >>> A = g.add_point(SymbolicCoord(0), SymbolicCoord(0), "A")
    >>> B = g.add_point(SymbolicCoord(3), SymbolicCoord(4), "B")
    >>>
    >>> # 添加约束
    >>> g.add_constraint(A, B, "distance", SymbolicCoord(5))
    >>>
    >>> # 规范化求解
    >>> result = g.normalize()
    >>> print(result)

    >>> # 使用上下文管理器确保资源释放
    >>> with Graph() as g:
    ...     P = g.add_point(SymbolicCoord(1), SymbolicCoord(2), "P")
    ...     # 自动管理 C 对象生命周期

与 C 库的绑定关系：
    本模块通过 ctypes 与底层 C 共享库（lv00_core）交互，绑定关系如下：
    - _SymbolicCoord (ctypes 结构体) ↔ C 层 SymbolicCoord 结构体
    - _ConstraintGraph (ctypes 指针) ↔ C 层 ConstraintGraph* 不透明句柄
    - _NormalizationResult (ctypes 结构体) ↔ C 层 NormalizationResult 结构体
    - 所有几何常量（GEOM_POINT, GEOM_LINE_SEGMENT 等）直接映射 C 层枚举值
    - 所有约束常量（CONSTRAINT_INCIDENCE 等）直接映射 C 层枚举值
    - Python 对象持有 C 指针，通过 __del__ 和上下文管理器自动释放

设计原则：
    1. 符号计算优先：所有坐标运算保持精确性，避免浮点误差
    2. 内存安全：自动管理 C 对象的生命周期，防止内存泄漏
    3. 类型安全：完整的类型提示和参数验证
    4. 异常处理：清晰的错误消息和错误码，统一的异常层次结构

版本：3.5.1 (优化版)
作者：Lv-00 开发团队
"""

from __future__ import annotations

import ctypes
import logging
import sys
import warnings
from dataclasses import dataclass, field
from fractions import Fraction
from typing import (
    Any, ClassVar, Dict, Final, Iterator, List, Optional, Protocol, 
    Set, Tuple, TypeVar, Union, runtime_checkable
)

# 导入底层 C 库绑定
from ._ctypes_binding import (
    _lib, _SymbolicCoord, _ConstraintGraph, _NormalizationResult,
    ADD_NODE_OK, ADD_CONSTRAINT_OK, GEOM_POINT, GEOM_LINE_SEGMENT,
    GEOM_PORT, GEOM_FUNCTION_BLOCK, PORT_INPUT, PORT_OUTPUT,
    CONSTRAINT_INCIDENCE, CONSTRAINT_BETWEENNESS, CONSTRAINT_INTERSECTION,
    CONSTRAINT_CONTAINMENT, CONSTRAINT_CONNECTION,
    LOG_LEVEL_DEBUG, LOG_LEVEL_INFO, LOG_LEVEL_WARN, LOG_LEVEL_ERROR
)

# 模块级常量
__version__: Final[str] = "3.5.1"
__author__: Final[str] = "Lv-00 开发团队"

# 类型变量
T = TypeVar('T')

# 模块级日志记录器
logger = logging.getLogger(__name__)


# ============================================================
# 工具函数
# ============================================================

def _is_interpreter_shutting_down() -> bool:
    """
    检测 Python 解释器是否正在关闭。
    
    当模块对象被 None 替换时为关闭状态，此时不应执行清理操作。
    参考 PEP 442 (CPython 3.4+) 的终结机制。
    
    Returns:
        bool: 如果解释器正在关闭则返回 True
    """
    return sys.modules.get(__name__) is None


def _safe_ctypes_call(func: Callable[..., T], *args: Any, **kwargs: Any) -> T:
    """
    安全的 ctypes 函数调用包装器。
    
    在解释器关闭期间捕获所有异常，避免在 __del__ 中引发错误。
    
    Args:
        func: 要调用的 ctypes 函数
        *args: 位置参数
        **kwargs: 关键字参数
        
    Returns:
        函数返回值
        
    Raises:
        Exception: 非解释器关闭期间的异常
    """
    try:
        return func(*args, **kwargs)
    except Exception as e:
        if _is_interpreter_shutting_down():
            # 解释器关闭期间静默忽略
            return None  # type: ignore
        raise


# ============================================================
# 异常类定义
# ============================================================

class Lv00BaseError(Exception):
    """
    Lv-00 统一异常基类。

    所有 Lv-00 相关异常的最顶层父类，提供统一的 message 和 error_code 属性
    以及标准的 __str__ 格式化逻辑。其他模块的异常类（如 EngineError、
    FuncBlockError）也应继承此基类，以保持异常体系的一致性。

    Attributes:
        message: 异常消息字符串
        error_code: 可选的错误码（整数，默认为 -1）
    """

    def __init__(self, message: str = "", error_code: int = -1) -> None:
        """
        创建 Lv-00 基础异常。

        Args:
            message: 异常描述消息
            error_code: 可选的错误码，用于区分具体的错误类型
        """
        super().__init__(message)
        self.message: str = message
        self.error_code: int = error_code

    def __str__(self) -> str:
        """
        返回人类可读的异常字符串。

        格式规则：
            - 有错误码（>=0）时: "ClassName(错误码): 消息"
            - 有消息无错误码时: "ClassName: 消息"
            - 无消息无错误码时: "ClassName"

        Returns:
            str: 格式化的异常字符串
        """
        if self.error_code >= 0:
            return f"{self.__class__.__name__}({self.error_code}): {self.message}"
        return f"{self.__class__.__name__}: {self.message}" if self.message else self.__class__.__name__


class Lv00Error(Lv00BaseError):
    """
    Lv-00 异常基类，所有核心模块异常的通用父类。

    所有 Lv-00 相关异常的父类，涵盖库错误、参数错误、
    约束冲突、求解失败等场景。

    Attributes:
        message (str): 异常描述消息，供上层捕获和展示
        error_code (int): 可选的错误码（默认为 -1），大于等于 0 时会被纳入
                          __str__ 输出，便于日志和自动化处理

    Examples:
        >>> raise Lv00Error("创建符号坐标失败", error_code=1001)
        Lv00Error(1001): 创建符号坐标失败
    """
    pass


class Lv00LibraryError(Lv00Error):
    """
    库加载或初始化失败异常。

    当 Lv-00 底层 C 动态库（DLL/SO）无法加载、符号解析失败、
    或初始化例程返回错误时抛出。此类异常通常意味着运行环境不兼容
    或缺少必要的系统依赖。

    常见触发场景：
        - C 动态库文件缺失或路径不正确
        - 缺少所需的运行时（如 MSVC 运行时、glibc 版本不匹配）
        - 平台架构不匹配（如 32 位/64 位混淆）
        - lv00_init() 调用失败

    Examples:
        >>> try:
        ...     g = Graph()
        ... except Lv00LibraryError as e:
        ...     print(f"库加载失败：{e}")
    """
    pass


class Lv00ArgumentError(Lv00Error):
    """参数无效异常。"""
    pass


class Lv00ConstraintError(Lv00Error):
    """几何约束冲突异常。"""
    pass


class Lv00SolverError(Lv00Error):
    """方程求解失败异常。"""
    pass


# ============================================================
# 符号坐标类
# ============================================================

@dataclass(frozen=True, slots=True)
class SymbolicCoord:
    """
    符号坐标类，Lv-00 精确几何计算的基础数值类型（不可变）。

    提供任意精度的坐标表示，支持有理数、代数数和超越数之间的
    精确算术运算。所有运算保持符号精度，避免浮点舍入误差，
    确保几何计算的可证明正确性。

    支持的坐标类型：
        - RATIONAL：有理数（如 3/4），精确分数表示，无限精度运算
        - ALGEBRAIC：代数数（如 sqrt(2)），通过最小多项式定义
        - QUADRATIC：二次根式，a + b*sqrt(n) 形式的特化代数数
        - TRANSCENDENTAL：超越数（π、e 等），符号形式保留

    Attributes:
        _ptr: 底层 C 指针，指向 C 语言的 SymbolicCoord 结构体
        type (str): 坐标类型字符串，由 C 库在序列化时返回
        trust (str): 信任颜色（green/blue/yellow/orange/amber）

    Supported Operations:
        - 加法/减法：+, -, __radd__, __rsub__
        - 乘法/除法：*, /, __rmul__
        - 取负/绝对值：-, abs()
        - 幂运算：**（仅整数指数）
        - 比较运算：==, !=, <, <=, >, >=

    Factory Methods:
        - SymbolicCoord.from_rational(num, den)：从分子分母创建有理数
        - SymbolicCoord.zero()：零值坐标
        - SymbolicCoord.one()：单位坐标
        - SymbolicCoord.parse(s)：从字符串解析

    Memory Management:
        通过 __del__ 析构函数自动释放 C 层分配的内存。
        在 Python 解释器关闭期间会安全跳过清理，避免崩溃。

    Examples:
        >>> from lv00.core import SymbolicCoord
        >>> from fractions import Fraction
        >>>
        >>> a = SymbolicCoord(1, 2)          # 创建 1/2
        >>> b = SymbolicCoord(Fraction(3, 4)) # 创建 3/4
        >>> c = a + b                         # 精确加法 → 5/4
        >>> d = SymbolicCoord.parse("3/4")    # 字符串解析
        >>> print(a < b)                      # 比较 → True
        >>> print(a.to_double())              # 近似值 → 0.5
    """
    
    _value: Union[Fraction, int, float, str, 'SymbolicCoord']
    _ptr: Optional[Any] = field(default=None, init=False, repr=False)
    
    # 类级缓存，用于常用常量
    _ZERO: ClassVar[Optional['SymbolicCoord']] = None
    _ONE: ClassVar[Optional['SymbolicCoord']] = None
    
    def __post_init__(self) -> None:
        """初始化后处理：创建 C 层对象。"""
        object.__setattr__(self, '_ptr', self._create_ptr(self._value))
    
    def _create_ptr(self, value: Union[Fraction, int, float, str, 'SymbolicCoord']) -> Any:
        """根据值类型创建 C 层指针。"""
        if isinstance(value, SymbolicCoord):
            ptr = _lib.symbolic_coord_copy(value._ptr)
        elif isinstance(value, Fraction):
            ptr = _lib.symbolic_coord_create_rational(value.numerator, value.denominator)
        elif isinstance(value, int):
            ptr = _lib.symbolic_coord_create_rational(value, 1)
        elif isinstance(value, float):
            frac = Fraction(value).limit_denominator(1000000)
            ptr = _lib.symbolic_coord_create_rational(frac.numerator, frac.denominator)
        elif isinstance(value, str):
            ptr = self._parse_string(value)
        else:
            raise TypeError(f"不支持的 SymbolicCoord 类型: {type(value)}")
        
        if not ptr:
            raise Lv00Error("创建符号坐标失败")
        return ptr
    
    def _parse_string(self, s: str) -> Any:
        """解析字符串为符号坐标。"""
        b = s.encode('utf-8')
        ptr = _lib.rational_parse(b)
        if ptr:
            return ptr
        # 尝试整数回退
        try:
            int_val = int(s)
            return _lib.symbolic_coord_create_rational(int_val, 1)
        except ValueError:
            pass
        # 尝试浮点数回退
        try:
            frac = Fraction(float(s)).limit_denominator(1000000)
            return _lib.symbolic_coord_create_rational(frac.numerator, frac.denominator)
        except (ValueError, OverflowError):
            raise Lv00Error(f"无法解析符号坐标表达式: {s}")
    
    def __del__(self) -> None:
        """析构函数：释放 C 分配的内存。"""
        try:
            if hasattr(self, '_ptr') and self._ptr is not None:
                _lib.symbolic_coord_destroy(self._ptr)
        except Exception:
            # 解释器关闭时静默忽略
            pass
    
    def __repr__(self) -> str:
        """返回坐标的调试表示。"""
        s = _lib.symbolic_coord_serialize(self._ptr)
        if s:
            result = s.decode('utf-8')
            _lib.lv00_free_ptr(s)
            return f"SymbolicCoord({result!r})"
        return "SymbolicCoord(<unknown>)"
    
    def __str__(self) -> str:
        """返回坐标的人类可读字符串表示。"""
        s = _lib.symbolic_coord_serialize(self._ptr)
        if s:
            result = s.decode('utf-8')
            _lib.lv00_free_ptr(s)
            return result
        return "<unknown>"
    
    # ========== 算术运算 ==========
    
    def _to_coord(self, other: Union['SymbolicCoord', int, float, Fraction]) -> 'SymbolicCoord':
        """确保另一个参数是 SymbolicCoord 类型。"""
        if isinstance(other, SymbolicCoord):
            return other
        return SymbolicCoord(other)
    
    def __add__(self, other: Union['SymbolicCoord', int, float, Fraction]) -> 'SymbolicCoord':
        """加法运算。"""
        other = self._to_coord(other)
        ptr = _lib.symbolic_coord_add(self._ptr, other._ptr)
        if not ptr:
            raise Lv00Error("加法运算失败")
        # 创建新对象而不经过 __post_init__
        result = object.__new__(SymbolicCoord)
        object.__setattr__(result, '_value', Fraction(0))  # 占位值
        object.__setattr__(result, '_ptr', ptr)
        return result
    
    def __radd__(self, other: Union[int, float, Fraction]) -> 'SymbolicCoord':
        """加法反向运算（other + self）。"""
        return self.__add__(other)
    
    def __sub__(self, other: Union['SymbolicCoord', int, float, Fraction]) -> 'SymbolicCoord':
        """减法运算。"""
        other = self._to_coord(other)
        ptr = _lib.symbolic_coord_subtract(self._ptr, other._ptr)
        if not ptr:
            raise Lv00Error("减法运算失败")
        result = object.__new__(SymbolicCoord)
        object.__setattr__(result, '_value', Fraction(0))
        object.__setattr__(result, '_ptr', ptr)
        return result
    
    def __rsub__(self, other: Union[int, float, Fraction]) -> 'SymbolicCoord':
        """减法反向运算：other - self。"""
        other = self._to_coord(other)
        ptr = _lib.symbolic_coord_subtract(other._ptr, self._ptr)
        if not ptr:
            raise Lv00Error("减法运算失败")
        result = object.__new__(SymbolicCoord)
        object.__setattr__(result, '_value', Fraction(0))
        object.__setattr__(result, '_ptr', ptr)
        return result
    
    def __mul__(self, other: Union['SymbolicCoord', int, float, Fraction]) -> 'SymbolicCoord':
        """乘法运算。"""
        other = self._to_coord(other)
        ptr = _lib.symbolic_coord_multiply(self._ptr, other._ptr)
        if not ptr:
            raise Lv00Error("乘法运算失败")
        result = object.__new__(SymbolicCoord)
        object.__setattr__(result, '_value', Fraction(0))
        object.__setattr__(result, '_ptr', ptr)
        return result
    
    def __rmul__(self, other: Union[int, float, Fraction]) -> 'SymbolicCoord':
        """乘法反向运算（other * self）。"""
        return self.__mul__(other)
    
    def __truediv__(self, other: Union['SymbolicCoord', int, float, Fraction]) -> 'SymbolicCoord':
        """除法运算。"""
        other = self._to_coord(other)
        ptr = _lib.symbolic_coord_divide(self._ptr, other._ptr)
        if not ptr:
            raise Lv00Error("除法运算失败（可能除数为零）")
        result = object.__new__(SymbolicCoord)
        object.__setattr__(result, '_value', Fraction(0))
        object.__setattr__(result, '_ptr', ptr)
        return result
    
    def __neg__(self) -> 'SymbolicCoord':
        """取负运算。"""
        ptr = _lib.symbolic_coord_negate(self._ptr)
        if not ptr:
            raise Lv00Error("取负运算失败")
        result = object.__new__(SymbolicCoord)
        object.__setattr__(result, '_value', Fraction(0))
        object.__setattr__(result, '_ptr', ptr)
        return result
    
    def __abs__(self) -> 'SymbolicCoord':
        """绝对值运算。"""
        if self.is_negative():
            return self.__neg__()
        # 创建副本
        ptr = _lib.symbolic_coord_copy(self._ptr)
        result = object.__new__(SymbolicCoord)
        object.__setattr__(result, '_value', Fraction(0))
        object.__setattr__(result, '_ptr', ptr)
        return result
    
    def __pow__(self, other: Union['SymbolicCoord', int, float, Fraction]) -> 'SymbolicCoord':
        """幂运算（仅支持整数指数）。"""
        # 转换指数为整数
        if isinstance(other, SymbolicCoord):
            try:
                exp = int(other.to_fraction())
            except (ValueError, Lv00Error):
                raise Lv00Error("幂运算仅支持整数指数")
        else:
            exp = int(other)
        
        # 处理特殊情况
        if exp == 0:
            return SymbolicCoord.from_rational(1)
        if exp == 1:
            ptr = _lib.symbolic_coord_copy(self._ptr)
            result = object.__new__(SymbolicCoord)
            object.__setattr__(result, '_value', Fraction(0))
            object.__setattr__(result, '_ptr', ptr)
            return result
        
        # 正整数幂：使用快速幂算法
        if exp > 0:
            result = SymbolicCoord.from_rational(1)
            base = self
            while exp > 0:
                if exp % 2 == 1:
                    result = result * base
                base = base * base
                exp //= 2
            return result
        
        # 负整数幂：计算倒数
        positive_result = self.__pow__(-exp)
        return SymbolicCoord.from_rational(1) / positive_result
    
    # ========== 比较运算 ==========
    
    def __eq__(self, other: Any) -> bool:
        """相等比较。"""
        if not isinstance(other, SymbolicCoord):
            return False
        return _lib.symbolic_coord_compare(self._ptr, other._ptr) == 0
    
    def __lt__(self, other: Union['SymbolicCoord', int, float, Fraction]) -> bool:
        """小于比较。"""
        other = self._to_coord(other)
        return _lib.symbolic_coord_compare(self._ptr, other._ptr) < 0
    
    def __le__(self, other: Union['SymbolicCoord', int, float, Fraction]) -> bool:
        """小于等于比较。"""
        other = self._to_coord(other)
        return _lib.symbolic_coord_compare(self._ptr, other._ptr) <= 0
    
    def __gt__(self, other: Union['SymbolicCoord', int, float, Fraction]) -> bool:
        """大于比较。"""
        other = self._to_coord(other)
        return _lib.symbolic_coord_compare(self._ptr, other._ptr) > 0
    
    def __ge__(self, other: Union['SymbolicCoord', int, float, Fraction]) -> bool:
        """大于等于比较。"""
        other = self._to_coord(other)
        return _lib.symbolic_coord_compare(self._ptr, other._ptr) >= 0
    
    def __hash__(self) -> int:
        """哈希值。"""
        return _lib.symbolic_coord_hash(self._ptr)
    
    # ========== 类型检查 ==========
    
    def is_zero(self) -> bool:
        """检查是否为零。"""
        return _lib.symbolic_coord_is_zero(self._ptr)
    
    def is_positive(self) -> bool:
        """检查是否为正数。"""
        return _lib.symbolic_coord_is_positive(self._ptr)
    
    def is_negative(self) -> bool:
        """检查是否为负数。"""
        return _lib.symbolic_coord_is_negative(self._ptr)
    
    # ========== 类型转换 ==========
    
    def to_double(self) -> float:
        """转换为双精度浮点数（可能丢失精度）。"""
        return _lib.symbolic_coord_to_double(self._ptr)
    
    def to_fraction(self) -> Fraction:
        """转换为分数（仅适用于有理数类型）。"""
        s = _lib.symbolic_coord_serialize(self._ptr)
        if not s:
            raise Lv00Error("序列化失败")
        try:
            result = Fraction(s.decode('utf-8'))
        finally:
            _lib.lv00_free_ptr(s)
        return result
    
    # ========== 工厂方法 ==========
    
    @classmethod
    def from_rational(cls, numerator: int, denominator: int = 1) -> 'SymbolicCoord':
        """
        从分子和分母创建有理数坐标。

        Args:
            numerator: 分子（整数）
            denominator: 分母（默认1，必须非零）

        Returns:
            SymbolicCoord: 创建的坐标

        Raises:
            ValueError: 分母为零
            Lv00Error: C 库创建失败
        """
        if denominator == 0:
            raise ValueError("分母不能为零")
        return cls(Fraction(numerator, denominator))
    
    @classmethod
    def zero(cls) -> 'SymbolicCoord':
        """创建零坐标。"""
        if cls._ZERO is None:
            cls._ZERO = cls.from_rational(0)
        return cls._ZERO
    
    @classmethod
    def one(cls) -> 'SymbolicCoord':
        """创建单位坐标（1）。"""
        if cls._ONE is None:
            cls._ONE = cls.from_rational(1)
        return cls._ONE
    
    @classmethod
    def parse(cls, s: str) -> 'SymbolicCoord':
        """解析字符串为符号坐标。"""
        return cls(s)


# ============================================================
# Point 类
# ============================================================

@dataclass
class Point:
    """
    二维几何点类，表示平面上的一个精确坐标位置。

    点由 x 和 y 两个 SymbolicCoord 坐标组成，支持精确坐标运算。
    可被解构（x, y = point）、索引（point[0]）、迭代。

    Attributes:
        x (SymbolicCoord): X 坐标，精确符号值
        y (SymbolicCoord): Y 坐标，精确符号值
        _id (Optional[int]): 在关联 Graph 中的节点 ID

    Examples:
        >>> from lv00.core import Point, SymbolicCoord
        >>>
        >>> p = Point(0, 0)                         # 原点
        >>> p2 = Point(SymbolicCoord(1), SymbolicCoord(2))  # 精确坐标
        >>> x, y = p2                               # 解构
        >>> dist = p.distance_to(p2)                # 距离平方
        >>> mid = p.mid_point(p2)                   # 中点
        >>> print(p.is_collinear_with(p2, Point(2, 4)))  # 共线 → True
    """
    
    x: SymbolicCoord
    y: SymbolicCoord
    _id: Optional[int] = field(default=None, repr=False)
    
    def __post_init__(self) -> None:
        """确保坐标是 SymbolicCoord 类型。"""
        if not isinstance(self.x, SymbolicCoord):
            object.__setattr__(self, 'x', SymbolicCoord(self.x))
        if not isinstance(self.y, SymbolicCoord):
            object.__setattr__(self, 'y', SymbolicCoord(self.y))
    
    def __repr__(self) -> str:
        """返回点的调试表示。"""
        return f"Point({self.x}, {self.y})"
    
    def __eq__(self, other: Any) -> bool:
        """判断两点是否相等。"""
        if not isinstance(other, Point):
            return False
        return self.x == other.x and self.y == other.y
    
    # Point 是可变的，不可哈希
    __hash__ = None  # type: ignore
    
    def __iter__(self) -> Iterator[SymbolicCoord]:
        """迭代点的坐标，支持解构：x, y = point。"""
        yield self.x
        yield self.y
    
    def __getitem__(self, index: int) -> SymbolicCoord:
        """通过索引访问坐标：point[0] 返回 x，point[1] 返回 y。"""
        if index == 0:
            return self.x
        elif index == 1:
            return self.y
        raise IndexError(f"Point 索引超出范围: {index}（有效范围: 0-1）")
    
    def distance_to(self, other: 'Point') -> SymbolicCoord:
        """
        计算到另一个点的距离平方。
        
        Note:
            返回的是距离的平方而非距离本身，因为符号坐标系统
            对平方根（代数数）的支持有限。
        """
        dx = self.x - other.x
        dy = self.y - other.y
        return dx * dx + dy * dy
    
    def mid_point(self, other: 'Point') -> 'Point':
        """计算到另一个点的中点。"""
        return Point(
            (self.x + other.x) / SymbolicCoord.from_rational(2),
            (self.y + other.y) / SymbolicCoord.from_rational(2)
        )
    
    def translate(self, dx: Union[SymbolicCoord, int, float], 
                  dy: Union[SymbolicCoord, int, float]) -> 'Point':
        """平移点。"""
        return Point(self.x + dx, self.y + dy)
    
    def reflect_over_point(self, center: 'Point') -> 'Point':
        """关于某点对称。"""
        new_x = center.x * SymbolicCoord.from_rational(2) - self.x
        new_y = center.y * SymbolicCoord.from_rational(2) - self.y
        return Point(new_x, new_y)
    
    def is_collinear_with(self, p2: 'Point', p3: 'Point') -> bool:
        """判断三点是否共线。"""
        det = (p2.x - self.x) * (p3.y - self.y) - (p2.y - self.y) * (p3.x - self.x)
        return det.is_zero()
    
    def vector_to(self, other: 'Point') -> Tuple[SymbolicCoord, SymbolicCoord]:
        """计算到另一个点的向量。"""
        return (other.x - self.x, other.y - self.y)


# ============================================================
# LineSegment 类
# ============================================================

@dataclass
class LineSegment:
    """
    线段类，表示连接两个点之间的有向几何线段。

    Attributes:
        p1 (Point): 线段起点
        p2 (Point): 线段终点
        _id (Optional[int]): 在关联 Graph 中的节点 ID

    Examples:
        >>> from lv00.core import Point, LineSegment
        >>>
        >>> seg = LineSegment(Point(0, 0), Point(3, 4))
        >>> print(seg.midpoint())              # Point(3/2, 2)
        >>> print(seg.slope())                 # 4/3
    """
    
    p1: Point
    p2: Point
    _id: Optional[int] = field(default=None, repr=False)
    
    def __post_init__(self) -> None:
        """验证端点类型。"""
        if not isinstance(self.p1, Point) or not isinstance(self.p2, Point):
            raise TypeError("LineSegment 端点必须是 Point 类型")
    
    def __repr__(self) -> str:
        """返回线段的调试表示。"""
        return f"LineSegment({self.p1}, {self.p2})"
    
    def length(self) -> SymbolicCoord:
        """计算线段长度（距离平方）。"""
        return self.p1.distance_to(self.p2)
    
    def midpoint(self) -> Point:
        """计算线段中点。"""
        return self.p1.mid_point(self.p2)
    
    def direction_vector(self) -> Tuple[SymbolicCoord, SymbolicCoord]:
        """获取线段的方向向量。"""
        return self.p1.vector_to(self.p2)
    
    def perpendicular_direction(self) -> Tuple[SymbolicCoord, SymbolicCoord]:
        """获取线段的垂直方向向量（旋转90度）。"""
        dx, dy = self.direction_vector()
        return (-dy, dx)
    
    def slope(self) -> Optional[SymbolicCoord]:
        """计算线段的斜率，垂直线返回 None。"""
        dx, dy = self.direction_vector()
        if dx.is_zero():
            return None
        return dy / dx
    
    def contains_point(self, point: Point) -> bool:
        """判断点是否在线段上。"""
        # 检查共线性
        if not self.p1.is_collinear_with(self.p2, point):
            return False
        
        # 检查点是否在端点之间
        min_x = min(self.p1.x, self.p2.x)
        max_x = max(self.p1.x, self.p2.x)
        min_y = min(self.p1.y, self.p2.y)
        max_y = max(self.p1.y, self.p2.y)
        
        return (min_x <= point.x <= max_x) and (min_y <= point.y <= max_y)
    
    def is_parallel_to(self, other: 'LineSegment') -> bool:
        """判断两条线段是否平行。"""
        dx1, dy1 = self.direction_vector()
        dx2, dy2 = other.direction_vector()
        cross = dx1 * dy2 - dy1 * dx2
        return cross.is_zero()
    
    def is_perpendicular_to(self, other: 'LineSegment') -> bool:
        """判断两条线段是否垂直。"""
        dx1, dy1 = self.direction_vector()
        dx2, dy2 = other.direction_vector()
        dot = dx1 * dx2 + dy1 * dy2
        return dot.is_zero()


# ============================================================
# GeomNode 类
# ============================================================

class GeomNode:
    """
    几何节点包装类。

    封装底层 C 结构的 GeomNode，提供 Python 友好的接口。
    此对象由约束图内部管理，不应直接创建。
    """
    
    # 类型名称映射
    _TYPE_NAMES: ClassVar[Dict[int, str]] = {
        GEOM_POINT: "点",
        GEOM_LINE_SEGMENT: "线段",
        GEOM_PORT: "端口",
        GEOM_FUNCTION_BLOCK: "函数块"
    }
    
    def __init__(self, ptr: Any) -> None:
        """内部构造函数。"""
        self._ptr = ptr
    
    @property
    def id(self) -> int:
        """节点 ID（在图中的唯一标识符）。"""
        return self._ptr.contents.id
    
    @property
    def type(self) -> int:
        """节点几何类型编码。"""
        return self._ptr.contents.type
    
    @property
    def type_name(self) -> str:
        """节点几何类型的可读中文名称。"""
        return self._TYPE_NAMES.get(self.type, "未知")


# ============================================================
# NormalizationResult 类
# ============================================================

class NormalizationResult:
    """
    图规范化操作的结果封装类。

    表示对约束图执行 normalize() 操作后产生的结果。
    此对象由 Graph.normalize() 创建并返回，不应直接实例化。
    """
    
    def __init__(self, ptr: Any) -> None:
        """内部构造函数。"""
        self._ptr = ptr
    
    def __del__(self) -> None:
        """析构函数：释放 C 分配的内存。"""
        try:
            if hasattr(self, '_ptr') and self._ptr is not None:
                _lib.normalization_result_destroy(self._ptr)
        except Exception:
            pass


# ============================================================
# Graph 类
# ============================================================

class Graph:
    """
    约束图类，Lv-00 几何构造的核心数据结构。

    表示一个完整的几何约束图，包含节点和约束关系。
    通过 ctypes 封装底层 C 库的 ConstraintGraph 结构体。

    Attributes:
        _ptr: 底层 C 指针
        _points: Python 侧追踪的点列表
        _segments: Python 侧追踪的线段列表
        _point_id_set: O(1) 点 ID 查找加速索引
    """
    
    # 使用集合查找的阈值
    _SET_LOOKUP_THRESHOLD: ClassVar[int] = 50
    
    def __init__(self) -> None:
        """
        创建新的空约束图实例。

        Raises:
            Lv00LibraryError: C 库创建失败时抛出
        """
        self._ptr = _lib.graph_create()
        if not self._ptr:
            raise Lv00LibraryError("创建约束图失败")
        
        self._points: List[Point] = []
        self._segments: List[LineSegment] = []
        self._regions: List[int] = []
        self._ports: List[int] = []
        self._point_id_set: Set[int] = set()
        self._next_id: int = _lib.graph_get_node_count(self._ptr)
    
    def __del__(self) -> None:
        """析构函数：释放 C 分配的内存。"""
        try:
            if hasattr(self, '_ptr') and self._ptr is not None:
                _lib.graph_destroy(self._ptr)
        except Exception:
            pass
    
    def __repr__(self) -> str:
        """返回图的调试表示。"""
        return f"Graph(points={len(self._points)}, segments={len(self._segments)})"
    
    def __len__(self) -> int:
        """返回图中节点总数。"""
        return self.get_node_count()
    
    def __iter__(self) -> Iterator[Point]:
        """迭代图中的所有点。"""
        return iter(self._points)
    
    def __contains__(self, item: Union[Point, int]) -> bool:
        """检查元素是否在图中。"""
        if isinstance(item, Point):
            return item in self._points
        elif isinstance(item, int):
            return self.get_node(item) is not None
        return False
    
    def __enter__(self) -> 'Graph':
        """上下文管理器入口。"""
        return self
    
    def __exit__(self, exc_type: Any, exc_val: Any, exc_tb: Any) -> None:
        """上下文管理器出口：自动释放资源。"""
        self.__del__()
    
    def _sync_id_from_c(self) -> int:
        """从 C 层同步当前节点 ID 计数器。"""
        count = _lib.graph_get_node_count(self._ptr)
        if count > self._next_id:
            self._next_id = count
        return self._next_id
    
    def _is_point_in_graph(self, point: Point) -> bool:
        """检查点是否已在图中（使用 O(1) 集合查找）。"""
        if self._point_id_set and point._id is not None:
            return point._id in self._point_id_set
        return point in self._points
    
    # ========== 节点操作 ==========
    
    def add_point(self, x: Union[SymbolicCoord, int, float], 
                  y: Union[SymbolicCoord, int, float]) -> Point:
        """
        向约束图中添加一个点节点。

        Args:
            x: X 坐标
            y: Y 坐标

        Returns:
            Point: 已注册到约束图的新建点对象

        Raises:
            Lv00ConstraintError: C 层添加失败时抛出
        """
        # 转换为 SymbolicCoord
        x_coord = x if isinstance(x, SymbolicCoord) else SymbolicCoord(x)
        y_coord = y if isinstance(y, SymbolicCoord) else SymbolicCoord(y)
        
        point = Point(x_coord, y_coord)
        
        # 创建坐标指针数组
        coords = (ctypes.POINTER(_SymbolicCoord) * 2)()
        coords[0] = point.x._ptr
        coords[1] = point.y._ptr
        
        result = _lib.graph_add_point(self._ptr, coords, 2)
        if result != ADD_NODE_OK:
            raise Lv00ConstraintError(f"添加点失败: 错误码 {result}")
        
        point._id = self._next_id
        self._next_id += 1
        self._points.append(point)
        
        # 维护 O(1) 查找加速索引
        self._point_id_set.add(point._id)
        
        return point
    
    def add_line_segment(self, p1: Point, p2: Point) -> LineSegment:
        """
        向约束图中添加一条线段节点。

        Args:
            p1: 线段起点，必须已添加到图中
            p2: 线段终点，必须已添加到图中

        Returns:
            LineSegment: 已注册到约束图的新建线段对象

        Raises:
            Lv00Error: 端点未添加到图中时抛出
            Lv00ConstraintError: C 层添加失败时抛出
        """
        # 验证端点已在图中
        if not self._is_point_in_graph(p1) or not self._is_point_in_graph(p2):
            raise Lv00Error("点必须先通过 add_point 添加到图中")
        
        segment = LineSegment(p1, p2)
        
        result = _lib.graph_add_line_segment(self._ptr, p1._id, p2._id)
        if result != ADD_NODE_OK:
            raise Lv00ConstraintError(f"添加线段失败: 错误码 {result}")
        
        segment._id = self._next_id
        self._next_id += 1
        self._segments.append(segment)
        
        return segment
    
    def remove_node(self, node_id: int) -> int:
        """
        从图中移除指定 ID 的节点及其关联约束。

        Args:
            node_id: 要移除的节点 ID

        Returns:
            int: 结果码，0 表示成功

        Raises:
            Lv00ConstraintError: 移除失败时抛出
        """
        from ._ctypes_binding import REMOVE_NODE_OK, REMOVE_NODE_NOT_FOUND
        
        result = _lib.graph_remove_node(self._ptr, node_id)
        if result == REMOVE_NODE_NOT_FOUND:
            raise Lv00ConstraintError(f"移除节点失败: 节点 {node_id} 不存在")
        if result != REMOVE_NODE_OK:
            raise Lv00ConstraintError(f"移除节点失败: 错误码 {result}")
        
        # 从 Python 追踪列表中移除
        self._points = [p for p in self._points if p._id != node_id]
        self._segments = [s for s in self._segments if s._id != node_id]
        self._point_id_set.discard(node_id)
        
        return result
    
    # ========== 约束操作 ==========
    
    def add_incidence(self, point_id: int, line_or_region_id: int) -> None:
        """添加关联约束：点位于线段或区域边界上。"""
        result = _lib.graph_add_incidence(self._ptr, point_id, line_or_region_id)
        if result != ADD_CONSTRAINT_OK:
            raise Lv00ConstraintError(f"添加关联约束失败: 错误码 {result}")
    
    def add_betweenness(self, p1_id: int, p2_id: int, p3_id: int) -> None:
        """添加中间约束：p2 位于 p1 和 p3 之间。"""
        result = _lib.graph_add_betweenness(self._ptr, p1_id, p2_id, p3_id)
        if result != ADD_CONSTRAINT_OK:
            raise Lv00ConstraintError(f"添加中间约束失败: 错误码 {result}")
    
    def add_intersection(self, line1_id: int, line2_id: int, 
                         result_point_id: int) -> None:
        """添加相交约束：两条线段交于指定点。"""
        result = _lib.graph_add_intersection(self._ptr, line1_id, line2_id, result_point_id)
        if result != ADD_CONSTRAINT_OK:
            raise Lv00ConstraintError(f"添加相交约束失败: 错误码 {result}")
    
    def add_containment(self, inner_id: int, outer_id: int) -> None:
        """添加包含约束：一个区域完全位于另一个区域内部。"""
        result = _lib.graph_add_containment(self._ptr, inner_id, outer_id)
        if result != ADD_CONSTRAINT_OK:
            raise Lv00ConstraintError(f"添加包含约束失败: 错误码 {result}")
    
    def add_connection(self, src_port_id: int, dst_port_id: int) -> None:
        """添加连接约束：两个端口之间建立连接关系。"""
        result = _lib.graph_add_connection(self._ptr, src_port_id, dst_port_id)
        if result != ADD_CONSTRAINT_OK:
            raise Lv00ConstraintError(f"添加连接约束失败: 错误码 {result}")
    
    # ========== 规范化与查询 ==========
    
    def normalize(self, scope_aware: bool = False) -> NormalizationResult:
        """
        规范化约束图。

        Args:
            scope_aware: 是否考虑作用域（默认 False）

        Returns:
            NormalizationResult: 规范化结果

        Raises:
            Lv00Error: 规范化失败时抛出
        """
        ptr = _lib.graph_normalize(self._ptr, scope_aware)
        if not ptr:
            raise Lv00Error("规范化失败")
        return NormalizationResult(ptr)
    
    def get_node_count(self) -> int:
        """获取图中节点数量。"""
        return _lib.graph_get_node_count(self._ptr)
    
    def get_constraint_count(self) -> int:
        """获取图中约束数量。"""
        return _lib.graph_get_constraint_count(self._ptr)
    
    def get_node(self, node_id: int) -> Optional[GeomNode]:
        """根据 ID 获取节点。"""
        ptr = _lib.graph_get_node(self._ptr, node_id)
        if not ptr:
            return None
        return GeomNode(ptr)
    
    @property
    def points(self) -> List[Point]:
        """获取图中所有点的列表。"""
        return self._points.copy()
    
    @property
    def segments(self) -> List[LineSegment]:
        """获取图中所有线段的列表。"""
        return self._segments.copy()


# ============================================================
# 模块级函数
# ============================================================

def init() -> bool:
    """初始化 Lv-00 系统。"""
    return _lib.lv00_init()


def cleanup() -> None:
    """清理 Lv-00 系统。"""
    _lib.lv00_cleanup()


def get_version() -> str:
    """获取 Lv-00 版本字符串。"""
    return __version__


def get_last_error() -> str:
    """获取最后发生的错误消息。"""
    msg = _lib.lv00_get_last_error_message()
    if msg:
        return msg.decode('utf-8')
    return ""


def set_debug_mode(enabled: bool) -> None:
    """设置调试模式。"""
    _lib.debug_set_mode(enabled)


def set_log_level(level: int) -> None:
    """设置日志级别。"""
    _lib.debug_set_log_level(level)


def reset_counters() -> None:
    """重置性能计数器。"""
    _lib.debug_reset_counters()


def get_counter_report() -> str:
    """获取性能计数器报告。"""
    report = _lib.debug_counters_report()
    if report:
        result = report.decode('utf-8')
        _lib.lv00_free_ptr(report)
        return result
    return ""


# ============================================================
# 导出列表
# ============================================================

__all__ = [
    # 版本信息
    '__version__',
    
    # 核心类
    'SymbolicCoord',
    'Point',
    'LineSegment',
    'Graph',
    'GeomNode',
    'NormalizationResult',
    
    # 异常类
    'Lv00BaseError',
    'Lv00Error',
    'Lv00LibraryError',
    'Lv00ArgumentError',
    'Lv00ConstraintError',
    'Lv00SolverError',
    
    # 工具函数
    'init',
    'cleanup',
    'get_version',
    'get_last_error',
    'set_debug_mode',
    'set_log_level',
    'reset_counters',
    'get_counter_report',
    
    # 常量
    'LOG_LEVEL_DEBUG',
    'LOG_LEVEL_INFO',
    'LOG_LEVEL_WARN',
    'LOG_LEVEL_ERROR',
    'GEOM_POINT',
    'GEOM_LINE_SEGMENT',
    'GEOM_PORT',
    'GEOM_FUNCTION_BLOCK',
    'CONSTRAINT_INCIDENCE',
    'CONSTRAINT_BETWEENNESS',
    'CONSTRAINT_INTERSECTION',
    'CONSTRAINT_CONTAINMENT',
    'CONSTRAINT_CONNECTION',
]
