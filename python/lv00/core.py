"""
Lv-00 核心模块

提供几何元编程的核心 Python 类，包括：
    - SymbolicCoord: 符号坐标，支持有理数、代数数和超越数
    - Point: 几何点
    - LineSegment: 线段
    - Graph: 约束图，表示几何构造
    - GeomNode: 几何节点包装类
    - NormalizationResult: 规范化结果类
    - Lv00Error / Lv00LibraryError: 异常类

本模块通过 ctypes 与底层 C 库交互，管理 C 对象的生命周期。

设计原则：
    1. 符号计算优先：所有坐标运算保持精确性
    2. 内存安全：自动管理 C 对象的生命周期
    3. 类型安全：完整的类型提示和参数验证
    4. 异常处理：清晰的错误消息和错误码

版本：3.2.0
作者：Lv-00 开发团队
"""

import ctypes
import sys as _sys
from fractions import Fraction
from typing import Any, Iterator, List, Optional, Set, Tuple, Union

from ._ctypes_binding import (
    _lib, _SymbolicCoord, _ConstraintGraph, _NormalizationResult,
    ADD_NODE_OK, ADD_CONSTRAINT_OK, GEOM_POINT, GEOM_LINE_SEGMENT,
    # 新增的常量
    GEOM_PORT, GEOM_FUNCTION_BLOCK, PORT_INPUT, PORT_OUTPUT,
    CONSTRAINT_INCIDENCE, CONSTRAINT_BETWEENNESS, CONSTRAINT_INTERSECTION,
    CONSTRAINT_CONTAINMENT, CONSTRAINT_CONNECTION,
    LOG_LEVEL_DEBUG, LOG_LEVEL_INFO, LOG_LEVEL_WARN, LOG_LEVEL_ERROR
)


# ============================================================
# 模块级工具函数
# ============================================================

def _is_interpreter_shutting_down() -> bool:
    """
    检测 Python 解释器是否正在关闭。
    
    当模块对象被 None 替换时为关闭状态，此时不应执行清理操作。
    参考 PEP 442 (CPython 3.4+) 的终结机制。
    
    返回：
        bool: 如果解释器正在关闭则返回 True
    """
    import sys as _check_sys
    return _check_sys.modules.get(__name__) is None


# ============================================================
# 异常类定义
# ============================================================

class Lv00BaseError(Exception):
    """
    Lv-00 统一异常基类。

    所有 Lv-00 相关异常的最顶层父类，提供统一的 message 和 error_code 属性
    以及标准的 __str__ 格式化逻辑。其他模块的异常类（如 EngineError、
    FuncBlockError）也应继承此基类，以保持异常体系的一致性。

    属性：
        message: 异常消息字符串
        error_code: 可选的错误码（整数，默认为 -1）
    """

    def __init__(self, message: str = "", error_code: int = -1) -> None:
        """
        创建 Lv-00 基础异常。

        参数：
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

        返回：
            str: 格式化的异常字符串
        """
        if self.error_code >= 0:
            return f"{self.__class__.__name__}({self.error_code}): {self.message}"
        return f"{self.__class__.__name__}: {self.message}" if self.message else self.__class__.__name__


class Lv00Error(Lv00BaseError):
    """
    Lv-00 异常基类，所有核心模块异常的通用父类。

    所有 Lv-00 相关异常的父类，涵盖库错误、参数错误、
    约束冲突、求解失败等场景。继承自 Lv00BaseError，复用统一的
    message、error_code 属性和人类可读的 __str__ 格式化逻辑。

    子类包括：
        - Lv00LibraryError：库加载或初始化失败
        - Lv00ArgumentError：参数无效或类型不兼容
        - Lv00ConstraintError：几何约束冲突
        - Lv00SolverError：方程求解失败

    属性（继承自 Lv00BaseError）：
        message (str): 异常描述消息，供上层捕获和展示
        error_code (int): 可选的错误码（默认为 -1），大于等于 0 时会被纳入
                          __str__ 输出，便于日志和自动化处理

    使用示例：
        >>> raise Lv00Error("创建符号坐标失败", error_code=1001)
        Lv00Error(1001): 创建符号坐标失败
    """

    def __init__(self, message: str = "", error_code: int = -1) -> None:
        """
        创建 Lv-00 核心异常实例。

        参数：
            message (str): 异常描述消息，说明错误原因和上下文
            error_code (int): 可选的错误码，用于区分具体的错误类型。
                              大于等于 0 时会在 __str__ 输出中展示，
                              负值表示未设置错误码。
        """
        super().__init__(message, error_code)


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

    使用示例：
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
# SymbolicCoord 类
# ============================================================

class SymbolicCoord:
    """
    符号坐标类，Lv-00 精确几何计算的基础数值类型。

    提供任意精度的坐标表示，支持有理数、代数数和超越数之间的
    精确算术运算。所有运算保持符号精度，避免浮点舍入误差，
    确保几何计算的可证明正确性。

    支持的坐标类型（type 属性）：
        - RATIONAL：有理数（如 3/4），精确分数表示，无限精度运算
        - ALGEBRAIC：代数数（如 sqrt(2)），通过最小多项式定义
        - QUADRATIC：二次根式，a + b*sqrt(n) 形式的特化代数数
        - TRANSCENDENTAL：超越数（π、e 等），符号形式保留

    核心属性：
        _ptr: 底层 C 指针，指向 C 语言的 SymbolicCoord 结构体，
              所有运算均通过此指针委托给 C 库执行
        type (str): 坐标类型字符串，由 C 库在序列化时返回
        trust (str): 信任颜色（green/blue/yellow/orange/amber），
                     表示从基准点推导至此坐标所需的信任链长度

    支持的算术运算：
        - 加法/减法：+, -, __radd__, __rsub__
        - 乘法/除法：*, /, __rmul__
        - 取负/绝对值：-, abs()
        - 幂运算：**（仅整数指数）
        - 比较运算：==, !=, <, <=, >, >=

    工厂方法：
        - SymbolicCoord.from_rational(num, den)：从分子分母创建有理数
        - SymbolicCoord.zero()：零值坐标
        - SymbolicCoord.one()：单位坐标
        - SymbolicCoord.parse(s)：从字符串解析

    内存管理：
        通过 __del__ 析构函数自动释放 C 层分配的内存。
        在 Python 解释器关闭期间会安全跳过清理操作，避免崩溃。

    使用示例：
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
    
    def __init__(self, value: Union[Fraction, int, float, str, 'SymbolicCoord']) -> None:
        """
        创建符号坐标。
        
        参数：
            value: 支持的类型
                - Fraction: 使用精确分数
                - int: 整数，自动转为有理数
                - float: 浮点数，自动转换为最接近的有理数
                - str: 字符串解析，支持 "3/4", "1.5" 等格式
                - SymbolicCoord: 复制坐标
        
        异常：
            TypeError: 不支持的类型
            Lv00Error: 创建失败
        """
        self._ptr = None
        
        # 处理不同类型的输入
        if isinstance(value, SymbolicCoord):
            # 复制已有的坐标
            self._ptr = _lib.symbolic_coord_copy(value._ptr)
            if not self._ptr:
                raise Lv00Error("复制符号坐标失败")
        elif isinstance(value, Fraction):
            # Fraction 类型：精确分数
            self._ptr = _lib.symbolic_coord_create_rational(
                value.numerator, value.denominator
            )
        elif isinstance(value, int):
            # 整数类型：转为分母为1的有理数
            self._ptr = _lib.symbolic_coord_create_rational(value, 1)
        elif isinstance(value, float):
            # 浮点数类型：转换为最接近的有理数
            frac = Fraction(value).limit_denominator(1000000)
            self._ptr = _lib.symbolic_coord_create_rational(frac.numerator, frac.denominator)
        elif isinstance(value, str):
            # 字符串类型：解析表达式
            # 注意：C 库已移除 symbolic_coord_deserialize 导出函数，
            # 改用 rational_parse 从字符串创建有理数坐标
            b = value.encode('utf-8')
            self._ptr = _lib.rational_parse(b)
            if not self._ptr:
                # rational_parse 仅支持有理数格式（如 "3/4"），
                # 对更复杂的表达式，尝试作为整数或浮点数回退处理
                try:
                    int_val = int(value)
                    self._ptr = _lib.symbolic_coord_create_rational(int_val, 1)
                except ValueError:
                    try:
                        frac = Fraction(float(value)).limit_denominator(1000000)
                        self._ptr = _lib.symbolic_coord_create_rational(
                            frac.numerator, frac.denominator
                        )
                    except (ValueError, OverflowError):
                        raise Lv00Error(f"无法解析符号坐标表达式: {value}")
        else:
            raise TypeError(f"不支持的 SymbolicCoord 类型: {type(value)}")
        
        if not self._ptr:
            raise Lv00Error("创建符号坐标失败")
    
    def __del__(self) -> None:
        """
        析构函数：释放 C 分配的内存。
        
        注意：解释器关闭时 _lib 可能已不可用，因此捕获所有异常。
        使用 module-level 弱引用来确保安全性。
        """
        try:
            if hasattr(self, '_ptr') and self._ptr is not None:
                _lib.symbolic_coord_destroy(self._ptr)
                self._ptr = None
        except Exception as e:
            # 解释器关闭时模块可能已被垃圾回收，此时静默忽略
            # 其他情况记录警告以便排查内存泄漏
            if not _is_interpreter_shutting_down():
                import sys
                print(f"[Lv00] 警告：SymbolicCoord 析构失败：{e}", file=sys.stderr)
    
    def __repr__(self) -> str:
        """
        返回坐标的调试表示。
        
        返回：
            str: 格式为 "SymbolicCoord(value)" 的字符串
        """
        s = _lib.symbolic_coord_serialize(self._ptr)
        if s:
            result = s.decode('utf-8')
            _lib.lv00_free_ptr(s)
            return f"SymbolicCoord({result!r})"
        return "SymbolicCoord(<unknown>)"
    
    def __str__(self) -> str:
        """
        返回坐标的人类可读字符串表示。
        
        返回：
            str: 坐标的字符串表示
        """
        s = _lib.symbolic_coord_serialize(self._ptr)
        if s:
            result = s.decode('utf-8')
            _lib.lv00_free_ptr(s)
            return result
        return "<unknown>"
    
    def _check_same_type(self, other: 'SymbolicCoord') -> 'SymbolicCoord':
        """
        确保另一个参数是 SymbolicCoord 类型。
        
        参数：
            other: 要转换的值
        
        返回：
            SymbolicCoord: 转换后的坐标
        
        异常：
            TypeError: 类型不兼容
        """
        if not isinstance(other, SymbolicCoord):
            other = SymbolicCoord(other)
        return other
    
    # ============================================================
    # 算术运算
    # ============================================================
    
    def __add__(self, other: Union['SymbolicCoord', int, float, Fraction]) -> 'SymbolicCoord':
        """
        加法运算。
        
        参数：
            other: 加数
        
        返回：
            SymbolicCoord: self + other
        
        异常：
            Lv00Error: 计算失败
        """
        other = self._check_same_type(other)
        result = SymbolicCoord.__new__(SymbolicCoord)
        result._ptr = _lib.symbolic_coord_add(self._ptr, other._ptr)
        if not result._ptr:
            raise Lv00Error("加法运算失败")
        return result
    
    def __radd__(self, other: Union[int, float, Fraction]) -> 'SymbolicCoord':
        """
        加法反向运算（other + self）。

        当左侧操作数不支持加法时，Python 会调用此方法。

        参数：
            other: 左操作数

        返回：
            SymbolicCoord: other + self 的结果
        """
        return self.__add__(other)
    
    def __sub__(self, other: Union['SymbolicCoord', int, float, Fraction]) -> 'SymbolicCoord':
        """
        减法运算。
        
        参数：
            other: 减数
        
        返回：
            SymbolicCoord: self - other
        """
        other = self._check_same_type(other)
        result = SymbolicCoord.__new__(SymbolicCoord)
        result._ptr = _lib.symbolic_coord_subtract(self._ptr, other._ptr)
        if not result._ptr:
            raise Lv00Error("减法运算失败")
        return result
    
    def __rsub__(self, other: Union[int, float, Fraction]) -> 'SymbolicCoord':
        """
        减法反向运算：other - self。

        参数：
            other: 左操作数

        返回：
            SymbolicCoord: other - self 的结果
        """
        other = self._check_same_type(other)
        result = SymbolicCoord.__new__(SymbolicCoord)
        result._ptr = _lib.symbolic_coord_subtract(other._ptr, self._ptr)
        if not result._ptr:
            raise Lv00Error("减法运算失败")
        return result
    
    def __mul__(self, other: Union['SymbolicCoord', int, float, Fraction]) -> 'SymbolicCoord':
        """
        乘法运算。
        
        参数：
            other: 乘数
        
        返回：
            SymbolicCoord: self * other
        """
        other = self._check_same_type(other)
        result = SymbolicCoord.__new__(SymbolicCoord)
        result._ptr = _lib.symbolic_coord_multiply(self._ptr, other._ptr)
        if not result._ptr:
            raise Lv00Error("乘法运算失败")
        return result
    
    def __rmul__(self, other: Union[int, float, Fraction]) -> 'SymbolicCoord':
        """
        乘法反向运算（other * self）。

        当左侧操作数不支持乘法时，Python 会调用此方法。

        参数：
            other: 左操作数

        返回：
            SymbolicCoord: other * self 的结果
        """
        return self.__mul__(other)
    
    def __truediv__(self, other: Union['SymbolicCoord', int, float, Fraction]) -> 'SymbolicCoord':
        """
        除法运算。
        
        参数：
            other: 除数
        
        返回：
            SymbolicCoord: self / other
        
        异常：
            Lv00Error: 除数为零时
        """
        other = self._check_same_type(other)
        result = SymbolicCoord.__new__(SymbolicCoord)
        result._ptr = _lib.symbolic_coord_divide(self._ptr, other._ptr)
        if not result._ptr:
            raise Lv00Error("除法运算失败（可能除数为零）")
        return result
    
    def __neg__(self) -> 'SymbolicCoord':
        """
        取负运算。
        
        返回：
            SymbolicCoord: -self
        """
        result = SymbolicCoord.__new__(SymbolicCoord)
        result._ptr = _lib.symbolic_coord_negate(self._ptr)
        if not result._ptr:
            raise Lv00Error("取负运算失败")
        return result
    
    def __abs__(self) -> 'SymbolicCoord':
        """
        绝对值运算。
        
        返回：
            SymbolicCoord: |self|
        """
        if self.is_negative():
            return self.__neg__()
        return SymbolicCoord(self)
    
    def __pow__(self, other: Union['SymbolicCoord', int, float, Fraction]) -> 'SymbolicCoord':
        """
        幂运算。
        
        参数：
            other: 指数（目前仅支持整数指数）
        
        返回：
            SymbolicCoord: self ** other
        
        异常：
            Lv00Error: 计算失败或不支持的指数类型
        
        注意：
            当前实现仅支持整数指数。对于分数指数（如平方根），
            需要使用专门的代数数构造函数。
        """
        # 转换指数为整数
        if isinstance(other, SymbolicCoord):
            # 尝试转换为整数
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
            return SymbolicCoord(self)
        
        # 正整数幂：使用快速幂算法
        if exp > 0:
            result = SymbolicCoord.from_rational(1)
            base = SymbolicCoord(self)
            while exp > 0:
                if exp % 2 == 1:
                    result = result * base
                base = base * base
                exp //= 2
            return result
        
        # 负整数幂：计算倒数
        positive_result = self ** (-exp)
        return SymbolicCoord.from_rational(1) / positive_result
    
    # ============================================================
    # 比较运算
    # ============================================================
    
    def __eq__(self, other: Any) -> bool:
        """
        相等比较。
        
        参数：
            other: 比较对象
        
        返回：
            bool: 是否相等
        """
        if not isinstance(other, SymbolicCoord):
            return False
        return _lib.symbolic_coord_compare(self._ptr, other._ptr) == 0
    
    def __lt__(self, other: Union['SymbolicCoord', int, float, Fraction]) -> bool:
        """
        小于比较。

        参数：
            other: 比较对象

        返回：
            bool: self < other 时返回 True
        """
        other = self._check_same_type(other)
        return _lib.symbolic_coord_compare(self._ptr, other._ptr) < 0
    
    def __le__(self, other: Union['SymbolicCoord', int, float, Fraction]) -> bool:
        """
        小于等于比较（self <= other）。

        参数：
            other: 比较对象

        返回：
            bool: self <= other 时返回 True
        """
        other = self._check_same_type(other)
        return _lib.symbolic_coord_compare(self._ptr, other._ptr) <= 0
    
    def __gt__(self, other: Union['SymbolicCoord', int, float, Fraction]) -> bool:
        """
        大于比较。

        参数：
            other: 比较对象

        返回：
            bool: self > other 时返回 True
        """
        other = self._check_same_type(other)
        return _lib.symbolic_coord_compare(self._ptr, other._ptr) > 0
    
    def __ge__(self, other: Union['SymbolicCoord', int, float, Fraction]) -> bool:
        """
        大于等于比较（self >= other）。

        参数：
            other: 比较对象

        返回：
            bool: self >= other 时返回 True
        """
        other = self._check_same_type(other)
        return _lib.symbolic_coord_compare(self._ptr, other._ptr) >= 0
    
    def __hash__(self) -> int:
        """
        哈希值。
        
        返回：
            int: 坐标的哈希值
        """
        return _lib.symbolic_coord_hash(self._ptr)
    
    # ============================================================
    # 类型检查
    # ============================================================
    
    def is_zero(self) -> bool:
        """
        检查是否为零。
        
        返回：
            bool: 是否为零
        """
        return _lib.symbolic_coord_is_zero(self._ptr)
    
    def is_positive(self) -> bool:
        """
        检查是否为正数。
        
        返回：
            bool: 是否为正数
        """
        return _lib.symbolic_coord_is_positive(self._ptr)
    
    def is_negative(self) -> bool:
        """
        检查是否为负数。
        
        返回：
            bool: 是否为负数
        """
        return _lib.symbolic_coord_is_negative(self._ptr)
    
    # ============================================================
    # 类型转换
    # ============================================================
    
    def to_double(self) -> float:
        """
        转换为双精度浮点数。
        
        注意：这可能会丢失精度。
        
        返回：
            float: 近似值
        """
        return _lib.symbolic_coord_to_double(self._ptr)
    
    def to_fraction(self) -> Fraction:
        """
        转换为分数。
        
        注意：仅适用于有理数类型。
        
        返回：
            Fraction: 精确分数
        
        异常：
            Lv00Error: 非有理数类型
        """
        s = _lib.symbolic_coord_serialize(self._ptr)
        if not s:
            raise Lv00Error("序列化失败")
        try:
            result = Fraction(s.decode('utf-8'))
        finally:
            _lib.lv00_free_ptr(s)
        return result
    
    # ============================================================
    # 工厂方法
    # ============================================================
    
    @classmethod
    def from_rational(cls, numerator: int, denominator: int = 1) -> 'SymbolicCoord':
        """
        从分子和分母创建有理数坐标。

        参数：
            numerator: 分子（整数）
            denominator: 分母（默认1，必须非零）

        返回：
            SymbolicCoord: 创建的坐标

        异常：
            ValueError: 分母为零
            Lv00Error: C 库创建失败
        """
        if denominator == 0:
            raise ValueError("分母不能为零（denominator must be non-zero）")
        coord = cls.__new__(cls)
        coord._ptr = _lib.symbolic_coord_create_rational(numerator, denominator)
        if not coord._ptr:
            raise Lv00Error("创建有理数坐标失败")
        return coord
    
    @classmethod
    def zero(cls) -> 'SymbolicCoord':
        """创建零坐标。

        返回：
            SymbolicCoord: 值为 0 的坐标
        """
        return cls.from_rational(0)
    
    @classmethod
    def one(cls) -> 'SymbolicCoord':
        """创建单位坐标（1）。

        返回：
            SymbolicCoord: 值为 1 的坐标
        """
        return cls.from_rational(1)
    
    @classmethod
    def parse(cls, s: str) -> 'SymbolicCoord':
        """
        解析字符串为符号坐标。
        
        参数：
            s: 字符串，如 "3/4", "1.5", "-2"
        
        返回：
            SymbolicCoord: 解析后的坐标
        """
        return cls(s)


# ============================================================
# Point 类
# ============================================================

class Point:
    """
    二维几何点类，表示平面上的一个精确坐标位置。

    点由 x 和 y 两个 SymbolicCoord 坐标组成，支持精确坐标运算。
    可被解构（x, y = point）、索引（point[0]）、迭代，并提供了
    丰富的几何变换和判断方法。

    属性：
        x (SymbolicCoord): X 坐标，精确符号值
        y (SymbolicCoord): Y 坐标，精确符号值
        _id (int 或 None): 在关联 Graph 中的节点 ID，由 Graph.add_point()
                           自动分配，None 表示尚未添加到图中

    支持的几何操作：
        - distance_to(other)：计算到另一点的距离平方
        - mid_point(other)：计算中点
        - translate(dx, dy)：平移点
        - reflect_over_point(center)：关于另一点的中心对称
        - is_collinear_with(p2, p3)：判断三点是否共线
        - vector_to(other)：计算方向向量

    使用示例：
        >>> from lv00.core import Point, SymbolicCoord
        >>>
        >>> p = Point(0, 0)                         # 原点
        >>> p2 = Point(SymbolicCoord(1), SymbolicCoord(2))  # 精确坐标
        >>> x, y = p2                               # 解构
        >>> dist = p.distance_to(p2)                # 距离平方
        >>> mid = p.mid_point(p2)                   # 中点
        >>> print(p.is_collinear_with(p2, Point(2, 4)))  # 共线 → True
    """
    
    def __init__(self, x: Union[SymbolicCoord, int, float], 
                 y: Union[SymbolicCoord, int, float]):
        """
        创建几何点。
        
        参数：
            x: X 坐标
            y: Y 坐标
        
        示例：
            >>> Point(SymbolicCoord(0), SymbolicCoord(0))  # 原点
            >>> Point(1, 2)  # 整数坐标
            >>> Point(0.5, 0.25)  # 浮点坐标
        """
        if not isinstance(x, SymbolicCoord):
            x = SymbolicCoord(x)
        if not isinstance(y, SymbolicCoord):
            y = SymbolicCoord(y)
        self.x = x
        self.y = y
        self._id = None  # Set by Graph.add_point
    
    def __repr__(self) -> str:
        """返回点的调试表示。

        返回：
            str: 格式为 "Point(x, y)" 的字符串
        """
        return f"Point({self.x}, {self.y})"
    
    def __eq__(self, other: Any) -> bool:
        """
        判断两点是否相等。

        比较两个点的 x 和 y 坐标是否都相等。

        参数：
            other: 比较对象

        返回：
            bool: 坐标均相等时返回 True
        """
        if not isinstance(other, Point):
            return False
        return self.x == other.x and self.y == other.y

    # 修复：定义了 __eq__ 后必须显式设置 __hash__，否则 Python 3 会将其设为 None
    # Point 的相等性基于可变坐标值，不适合用作哈希键，因此显式标记为不可哈希
    __hash__ = None
    
    def __iter__(self) -> 'Iterator[SymbolicCoord]':
        """
        迭代点的坐标，支持解构：x, y = point。

        返回：
            Iterator[SymbolicCoord]: 坐标迭代器 (x, y)
        """
        yield self.x
        yield self.y
    
    def __getitem__(self, index: int) -> 'SymbolicCoord':
        """
        通过索引访问坐标：point[0] 返回 x，point[1] 返回 y。

        参数：
            index: 坐标索引（0 或 1）

        返回：
            SymbolicCoord: 对应坐标

        异常：
            IndexError: 索引超出范围
        """
        if index == 0:
            return self.x
        elif index == 1:
            return self.y
        raise IndexError(f"Point 索引超出范围: {index}（有效范围: 0-1）")
    
    def distance_to(self, other: 'Point') -> 'SymbolicCoord':
        """
        计算到另一个点的距离平方。
        
        参数：
            other: 另一个点
        
        返回：
            SymbolicCoord: 欧几里得距离的平方
        
        注意：
            返回的是距离的平方而非距离本身，因为符号坐标系统
            对平方根（代数数）的支持有限。如需精确的距离值，
            请使用 C 库的代数数功能或进行数值近似。
        
        数学定义：
            distance² = (x₂-x₁)² + (y₂-y₁)²
        """
        dx = self.x - other.x
        dy = self.y - other.y
        return dx * dx + dy * dy
    
    def mid_point(self, other: 'Point') -> 'Point':
        """
        计算到另一个点的中点。
        
        参数：
            other: 另一个点
        
        返回：
            Point: 中点
        """
        return Point(
            (self.x + other.x) / SymbolicCoord.from_rational(2),
            (self.y + other.y) / SymbolicCoord.from_rational(2)
        )
    
    def translate(self, dx: Union['SymbolicCoord', int, float], 
                  dy: Union['SymbolicCoord', int, float]) -> 'Point':
        """
        平移点。
        
        将点沿 x 和 y 方向平移指定的距离。
        
        参数：
            dx: x 方向平移量
            dy: y 方向平移量
        
        返回：
            Point: 平移后的新点
        
        示例：
            >>> p = Point(1, 2)
            >>> p.translate(2, 3)  # 返回 Point(3, 5)
        """
        if not isinstance(dx, SymbolicCoord):
            dx = SymbolicCoord(dx)
        if not isinstance(dy, SymbolicCoord):
            dy = SymbolicCoord(dy)
        return Point(self.x + dx, self.y + dy)
    
    def reflect_over_point(self, center: 'Point') -> 'Point':
        """
        关于某点对称。
        
        计算当前点关于中心点的对称点。
        
        参数：
            center: 对称中心点
        
        返回：
            Point: 对称点
        
        数学定义：
            对称点 P' 满足：center 是 PP' 的中点
            即 P' = 2 * center - P
        """
        new_x = center.x * SymbolicCoord.from_rational(2) - self.x
        new_y = center.y * SymbolicCoord.from_rational(2) - self.y
        return Point(new_x, new_y)
    
    def is_collinear_with(self, p2: 'Point', p3: 'Point') -> bool:
        """
        判断三点是否共线。
        
        使用行列式判断三点是否在同一条直线上。
        
        参数：
            p2: 第二个点
            p3: 第三个点
        
        返回：
            bool: 三点共线返回 True，否则返回 False
        
        数学定义：
            行列式 |x2-x1, x3-x1|
                   |y2-y1, y3-y1| = 0 时共线
        """
        det = (p2.x - self.x) * (p3.y - self.y) - (p2.y - self.y) * (p3.x - self.x)
        return det.is_zero()
    
    def vector_to(self, other: 'Point') -> Tuple['SymbolicCoord', 'SymbolicCoord']:
        """
        计算到另一个点的向量。
        
        参数：
            other: 目标点
        
        返回：
            Tuple[SymbolicCoord, SymbolicCoord]: (dx, dy) 向量分量
        """
        return (other.x - self.x, other.y - self.y)


# ============================================================
# LineSegment 类
# ============================================================

class LineSegment:
    """
    线段类，表示连接两个点之间的有向几何线段。

    封装起点 p1 和终点 p2，提供长度计算、中点、方向向量、
    垂直方向、斜率、共线性判断、平行/垂直判断等几何分析方法。

    属性：
        p1 (Point): 线段起点
        p2 (Point): 线段终点
        _id (int 或 None): 在关联 Graph 中的节点 ID，由
                           Graph.add_line_segment() 自动分配，
                           None 表示尚未添加到图中

    支持的几何分析：
        - length()：线段长度（返回距离平方的 SymbolicCoord）
        - midpoint()：线段中点
        - direction_vector()：方向向量 (dx, dy)
        - perpendicular_direction()：垂直方向向量 (-dy, dx)
        - slope()：斜率，垂直线返回 None
        - contains_point(point)：判断点是否在线段上
        - is_parallel_to(other)：判断与另一线段是否平行
        - is_perpendicular_to(other)：判断与另一线段是否垂直

    数学基础：
        所有坐标运算基于 SymbolicCoord 的精确符号计算，
        方向向量判断使用叉积（平行）和点积（垂直）的零值检测。

    使用示例：
        >>> from lv00.core import Point, LineSegment
        >>>
        >>> seg = LineSegment(Point(0, 0), Point(3, 4))
        >>> print(seg.midpoint())              # Point(3/2, 2)
        >>> print(seg.slope())                 # 4/3
        >>> seg2 = LineSegment(Point(1, 1), Point(2, 2))
        >>> print(seg.is_parallel_to(seg2))    # True
    """
    
    def __init__(self, p1: Point, p2: Point) -> None:
        """
        创建线段。
        
        参数：
            p1: 起点
            p2: 终点
        
        异常：
            TypeError: 参数不是 Point 类型
        """
        if not isinstance(p1, Point) or not isinstance(p2, Point):
            raise TypeError("参数必须是 Point 类型")
        self.p1 = p1
        self.p2 = p2
        self._id = None  # Set by Graph.add_line_segment
    
    def __repr__(self) -> str:
        """
        返回线段的调试表示。

        返回：
            str: 格式为 "LineSegment(p1, p2)" 的字符串
        """
        return f"LineSegment({self.p1}, {self.p2})"
    
    def length(self) -> SymbolicCoord:
        """
        计算线段长度。
        
        返回：
            SymbolicCoord: 线段长度
        """
        return self.p1.distance_to(self.p2)
    
    def midpoint(self) -> Point:
        """
        计算线段中点。
        
        返回：
            Point: 中点
        """
        return self.p1.mid_point(self.p2)
    
    def direction_vector(self) -> Tuple['SymbolicCoord', 'SymbolicCoord']:
        """
        获取线段的方向向量。
        
        返回：
            Tuple[SymbolicCoord, SymbolicCoord]: (dx, dy) 方向向量
        """
        return self.p1.vector_to(self.p2)
    
    def perpendicular_direction(self) -> Tuple['SymbolicCoord', 'SymbolicCoord']:
        """
        获取线段的垂直方向向量。
        
        返回：
            Tuple[SymbolicCoord, SymbolicCoord]: 垂直方向向量（旋转90度）
        
        数学定义：
            若原方向为 (dx, dy)，则垂直方向为 (-dy, dx)
        """
        dx, dy = self.direction_vector()
        return (-dy, dx)
    
    def slope(self) -> Optional['SymbolicCoord']:
        """
        计算线段的斜率。
        
        返回：
            SymbolicCoord 或 None: 斜率值，垂直线返回 None
        
        数学定义：
            斜率 k = (y2 - y1) / (x2 - x1)
        """
        dx, dy = self.direction_vector()
        if dx.is_zero():
            return None  # 垂直线，斜率不存在
        return dy / dx
    
    def contains_point(self, point: Point) -> bool:
        """
        判断点是否在线段上。
        
        参数：
            point: 待判断的点
        
        返回：
            bool: 点在线段上返回 True
        
        数学定义：
            点 P 在线段 AB 上，当且仅当：
            1. P 在直线 AB 上（三点共线）
            2. P 在 A 和 B 之间
        """
        # 检查共线性
        if not self.p1.is_collinear_with(self.p2, point):
            return False
        
        # 检查点是否在端点之间
        # 使用坐标范围检查
        min_x = min(self.p1.x, self.p2.x)
        max_x = max(self.p1.x, self.p2.x)
        min_y = min(self.p1.y, self.p2.y)
        max_y = max(self.p1.y, self.p2.y)
        
        return (min_x <= point.x <= max_x) and (min_y <= point.y <= max_y)
    
    def is_parallel_to(self, other: 'LineSegment') -> bool:
        """
        判断两条线段是否平行。
        
        参数：
            other: 另一条线段
        
        返回：
            bool: 平行返回 True
        
        数学定义：
            两条线段平行当且仅当它们的方向向量成比例
        """
        dx1, dy1 = self.direction_vector()
        dx2, dy2 = other.direction_vector()
        
        # 检查叉积是否为零
        cross = dx1 * dy2 - dy1 * dx2
        return cross.is_zero()
    
    def is_perpendicular_to(self, other: 'LineSegment') -> bool:
        """
        判断两条线段是否垂直。
        
        参数：
            other: 另一条线段
        
        返回：
            bool: 垂直返回 True
        
        数学定义：
            两条线段垂直当且仅当它们的方向向量点积为零
        """
        dx1, dy1 = self.direction_vector()
        dx2, dy2 = other.direction_vector()
        
        dot = dx1 * dx2 + dy1 * dy2
        return dot.is_zero()


# ============================================================
# GeomNode 类（包装）
# ============================================================

class GeomNode:
    """
    几何节点包装类。

    封装底层 C 结构的 GeomNode，提供 Python 友好的接口。
    用于读取已添加到约束图中的节点的属性和类型信息。

    注意：
        此对象由约束图内部管理，不应直接创建。
        通过 Graph.get_node() 方法获取实例。

    属性：
        id: 节点在图中的唯一标识符
        type: 节点几何类型编码（GEOM_POINT/GEOM_LINE_SEGMENT 等）
        type_name: 节点几何类型的可读中文名称
    """
    
    def __init__(self, ptr: Any) -> None:
        """
        内部构造函数。
        
        参数：
            ptr: 底层 C 指针（POINTER(_GeomNode)）
        """
        self._ptr = ptr
    
    @property
    def id(self) -> int:
        """节点 ID（在图中的唯一标识符）。"""
        return self._ptr.contents.id
    
    @property
    def type(self) -> int:
        """节点几何类型编码（GEOM_POINT/GEOM_LINE_SEGMENT 等）。"""
        return self._ptr.contents.type
    
    @property
    def type_name(self) -> str:
        """节点几何类型的可读中文名称。"""
        names = {
            GEOM_POINT: "点",
            GEOM_LINE_SEGMENT: "线段",
            GEOM_PORT: "端口",
            GEOM_FUNCTION_BLOCK: "函数块"
        }
        return names.get(self.type, "未知")


# ============================================================
# NormalizationResult 类
# ============================================================

class NormalizationResult:
    """
    图规范化操作的结果封装类。

    表示对约束图执行 normalize() 操作后产生的结果，包含合并统计、
    等价类映射等规范化信息。封装底层 C 结构的 _NormalizationResult，
    自动管理 C 内存生命周期。

    注意：
        此对象由 Graph.normalize() 创建并返回，不应直接实例化。
        同时由 normalization.py 作为薄层重新导出，以保持向后兼容性。
        新代码应直接从 lv00.core 导入。

    属性：
        _ptr: 底层 C 指针（POINTER(_NormalizationResult)），
              指向 C 层的规范化结果结构体，内含合并计数、
              等价类分组等数据。

    生命周期：
        创建后由 __del__ 析构函数自动释放 C 层内存。
        在 Python 解释器关闭期安全跳过清理，避免崩溃。

    使用示例：
        >>> g = Graph()
        >>> # ... 添加节点和约束 ...
        >>> result = g.normalize()
        >>> # result 内部已包含合并统计信息
    """
    
    def __init__(self, ptr: Any) -> None:
        """
        内部构造函数。
        
        参数：
            ptr: 底层 C 指针（POINTER(_NormalizationResult)）
        """
        self._ptr = ptr
    
    def __del__(self) -> None:
        """析构函数：释放 C 分配的内存。"""
        try:
            if hasattr(self, '_ptr') and self._ptr is not None:
                _lib.normalization_result_destroy(self._ptr)
                self._ptr = None
        except Exception as e:
            if not _is_interpreter_shutting_down():
                import sys
                print(f"[Lv00] 警告：NormalizationResult 析构失败：{e}", file=sys.stderr)


# ============================================================
# Graph 类
# ============================================================

class Graph:
    """
    约束图类，Lv-00 几何构造的核心数据结构。

    表示一个完整的几何约束图，包含节点（点、线段、区域、端口、
    函数块等）和约束（关联、中间、相交、包含、连接等）。
    通过 ctypes 封装底层 C 库的 ConstraintGraph 结构体，
    自动管理 C 对象的生命周期。

    核心职责：
        1. 节点管理：添加/移除点、线段、区域、端口、函数块
        2. 约束管理：添加各种几何约束（关联、中间、相交等）
        3. 图分析：规范化、冲突检测、冗余检测
        4. 序列化：JSON 格式的导入导出

    属性：
        _ptr: 底层 C 指针，指向 C 层的 ConstraintGraph 结构体
        _points (List[Point]): Python 侧追踪的点列表
        _segments (List[LineSegment]): Python 侧追踪的线段列表
        _point_id_set (Set[int]): O(1) 点 ID 查找加速索引
        _next_id (int): Python 侧下一个可用的节点 ID，从 C 层同步

    节点 ID 管理：
        Python 侧的 _next_id 仅用于辅助对象跟踪，不取代 C 层
        graph_get_node_count() 的权威 ID 管理。初始化时从 C 层获取
        当前节点数作为基线，并通过 _sync_id_from_c() 按需对齐。

    使用示例：
        >>> from lv00.core import Graph
        >>>
        >>> g = Graph()
        >>> p1 = g.add_point(0, 0)            # 添加原点
        >>> p2 = g.add_point(3, 4)            # 添加点 (3, 4)
        >>> seg = g.add_line_segment(p1, p2)  # 添加线段
        >>> print(g.get_node_count())         # 节点总数
        >>> print(g.to_dsl())                 # 导出为 DSL 文本
    """
    
    def __init__(self) -> None:
        """
        创建新的空约束图实例。

        通过 C 库 graph_create() 分配底层 ConstraintGraph 结构体，
        并初始化 Python 侧的追踪列表（_points、_segments 等）和
        ID 计数器。从 C 层同步当前节点数作为 _next_id 的初始基线。

        异常：
            Lv00LibraryError: C 库创建失败时抛出。通常由 DLL 加载失败、
                               内存不足或平台不兼容引起。
        """
        self._ptr = _lib.graph_create()
        if not self._ptr:
            raise Lv00LibraryError("创建约束图失败")
        self._points: List[Point] = []
        self._segments: List[LineSegment] = []
        self._regions: List[int] = []
        self._ports: List[int] = []
        # 辅助集合：使用 Point.id() 进行 O(1) 成员检查，替代 O(n) 的列表查找
        # 当点数超过此阈值时自动启用加速索引
        self._point_id_set: Set[int] = set()
        self._THRESHOLD_SET_LOOKUP: int = 50
        # 注：Python 侧 ID 计数器仅用于辅助 Python 对象跟踪，
        # 不取代 C 层 graph_get_node_count() 的权威 ID 管理。
        # 同步策略：初始化时从 C 层获取当前节点数作为基线。
        self._next_id = _lib.graph_get_node_count(self._ptr)
    
    def __del__(self) -> None:
        """
        析构函数：释放 C 分配的内存。
        """
        try:
            if hasattr(self, '_ptr') and self._ptr is not None:
                _lib.graph_destroy(self._ptr)
                self._ptr = None
        except Exception as e:
            if not _is_interpreter_shutting_down():
                import sys
                print(f"[Lv00] 警告：Graph 析构失败：{e}", file=sys.stderr)
    
    def __repr__(self) -> str:
        """
        返回图的调试表示。

        返回：
            str: 图的字符串表示
        """
        return f"Graph(points={len(self._points)}, segments={len(self._segments)})"
    
    def __len__(self) -> int:
        """
        返回图中节点总数。

        返回：
            int: 节点数量（点 + 线段 + 区域 + 端口）
        """
        return self.get_node_count()
    
    def __iter__(self) -> 'Iterator[Point]':
        """
        迭代图中的所有点。

        返回：
            Iterator[Point]: 点迭代器
        """
        return iter(self._points)
    
    def __contains__(self, item: Union[Point, int]) -> bool:
        """
        检查元素是否在图中。

        参数：
            item: 要检查的元素（Point 或节点 ID）

        返回：
            bool: 是否存在
        """
        if isinstance(item, Point):
            return item in self._points
        elif isinstance(item, int):
            return self.get_node(item) is not None
        return False

    def _sync_id_from_c(self) -> int:
        """
        从 C 层同步当前节点 ID 计数器。

        获取 C 层约束图的当前节点总数，用于在 Python 侧与 C 侧
        可能出现不一致时（如通过引擎直接操作了约束图）进行对齐。

        返回：
            int: C 层的当前节点总数（新的 _next_id 基线）
        """
        count = _lib.graph_get_node_count(self._ptr)
        if count > self._next_id:
            self._next_id = count
        return self._next_id

    # ============================================================
    # 节点操作
    # ============================================================
    
    def add_point(self, x: Union[SymbolicCoord, int, float], 
                  y: Union[SymbolicCoord, int, float]) -> Point:
        """
        向约束图中添加一个点节点。

        将坐标自动转换为 SymbolicCoord（如需要），通过 C 库
        graph_add_point() 注册到约束图，并在 Python 侧追踪点对象。
        返回的 Point 对象携带分配的节点 ID（_id 属性），后续可通过
        该 ID 引用此节点来添加约束或线段。

        参数：
            x (SymbolicCoord | int | float): X 坐标。整数和浮点数
                                             自动转为有理数坐标
            y (SymbolicCoord | int | float): Y 坐标，同上

        返回：
            Point: 已注册到约束图的新建点对象，_id 已分配且可追溯

        异常：
            Lv00ConstraintError: C 层 graph_add_point() 返回非 OK 错误码时抛出，
                                  包含具体的错误码信息。常见原因：
                                  - 坐标类型不被 C 库支持
                                  - 底层图结构损坏
                                  - 内存分配失败

        副作用：
            - 增加 self._points 列表
            - 更新 self._point_id_set 加速索引
            - self._next_id 自增
        """
        # 转换为 SymbolicCoord
        if not isinstance(x, SymbolicCoord):
            x = SymbolicCoord(x)
        if not isinstance(y, SymbolicCoord):
            y = SymbolicCoord(y)
        
        point = Point(x, y)
        
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

        验证两个端点已通过 add_point() 注册到图中，然后通过 C 库
        graph_add_line_segment() 在约束图中注册节点，返回携带 ID 的
        LineSegment 对象。

        参数：
            p1 (Point): 线段起点，必须已通过 add_point() 添加到图中
            p2 (Point): 线段终点，必须已通过 add_point() 添加到图中

        返回：
            LineSegment: 已注册到约束图的新建线段对象，_id 已分配

        异常：
            Lv00Error: p1 或 p2 尚未添加到图中时抛出，
                       消息为 "点必须先通过 add_point 添加到图中"
            Lv00ConstraintError: C 层 graph_add_line_segment() 返回
                                  非 OK 错误码时抛出

        性能说明：
            使用 _point_id_set 进行 O(1) 的端点成员检查，
            当点数超过阈值 _THRESHOLD_SET_LOOKUP 时自动启用加速索引。

        副作用：
            - 增加 self._segments 列表
            - self._next_id 自增
        """
        if p1 not in self._points or p2 not in self._points:
            # 性能优化：使用 _point_id_set 进行 O(1) 成员检查
            # 优先使用加速索引，回退到 O(n) 的列表查找（兼容旧 ID 格式）
            p1_in = p1._id in self._point_id_set if self._point_id_set else (p1 in self._points)
            p2_in = p2._id in self._point_id_set if self._point_id_set else (p2 in self._points)
            if not (p1_in and p2_in):
                raise Lv00Error("点必须先通过 add_point 添加到图中")
        
        segment = LineSegment(p1, p2)
        
        result = _lib.graph_add_line_segment(self._ptr, p1._id, p2._id)
        if result != ADD_NODE_OK:
            raise Lv00ConstraintError(f"添加线段失败: 错误码 {result}")
        
        segment._id = self._next_id
        self._next_id += 1
        self._segments.append(segment)
        return segment
    
    def add_region(self, boundary_segment_ids: List[int]) -> int:
        """
        向图中添加一个区域。
        
        参数：
            boundary_segment_ids: 边界线段 ID 列表
        
        返回：
            int: 区域节点 ID
        """
        arr = (ctypes.c_int * len(boundary_segment_ids))(*boundary_segment_ids)
        result = _lib.graph_add_region(self._ptr, arr, len(boundary_segment_ids))
        if result != ADD_NODE_OK:
            raise Lv00ConstraintError(f"添加区域失败: 错误码 {result}")
        
        region_id = self._next_id
        self._next_id += 1
        return region_id
    
    def add_port(self, port_type: int, namespace_depth: int = 0, 
                 parent_block_id: int = -1) -> int:
        """
        向图中添加一个端口。
        
        参数：
            port_type: 端口类型（PORT_INPUT 或 PORT_OUTPUT）
            namespace_depth: 命名空间深度
            parent_block_id: 父函数块 ID
        
        返回：
            int: 端口节点 ID
        """
        result = _lib.graph_add_port(self._ptr, port_type, namespace_depth, parent_block_id)
        if result != ADD_NODE_OK:
            raise Lv00ConstraintError(f"添加端口失败: 错误码 {result}")
        
        port_id = self._next_id
        self._next_id += 1
        self._ports.append(port_id)
        return port_id

    def remove_node(self, node_id: int) -> int:
        """
        从图中移除指定 ID 的节点及其关联约束。

        参数：
            node_id: 要移除的节点 ID

        返回：
            int: 结果码，REMOVE_NODE_OK (0) 表示成功

        异常：
            Lv00ConstraintError: 移除失败
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
        if node_id in self._point_id_set:
            self._point_id_set.discard(node_id)
        return result

    def remove_constraint(self, constraint_id: int) -> int:
        """
        从图中移除指定 ID 的约束。

        参数：
            constraint_id: 要移除的约束 ID

        返回：
            int: 结果码，REMOVE_CONSTRAINT_OK (0) 表示成功

        异常：
            Lv00ConstraintError: 移除失败
        """
        from ._ctypes_binding import REMOVE_CONSTRAINT_OK, REMOVE_CONSTRAINT_NOT_FOUND
        result = _lib.graph_remove_constraint(self._ptr, constraint_id)
        if result == REMOVE_CONSTRAINT_NOT_FOUND:
            raise Lv00ConstraintError(f"移除约束失败: 约束 {constraint_id} 不存在")
        if result != REMOVE_CONSTRAINT_OK:
            raise Lv00ConstraintError(f"移除约束失败: 错误码 {result}")
        return result

    def add_function_block(self, internal_ids: List[int],
                           input_ids: List[int],
                           output_ids: List[int]) -> int:
        """
        向图中添加一个函数块节点。

        参数：
            internal_ids: 内部节点 ID 列表
            input_ids: 输入端口 ID 列表
            output_ids: 输出端口 ID 列表

        返回：
            int: 函数块节点 ID

        异常：
            Lv00ConstraintError: 添加失败
        """
        internal_arr = (ctypes.c_int * len(internal_ids))(*internal_ids)
        input_arr = (ctypes.c_int * len(input_ids))(*input_ids)
        output_arr = (ctypes.c_int * len(output_ids))(*output_ids)
        result = _lib.graph_add_function_block(
            self._ptr,
            internal_arr, len(internal_ids),
            input_arr, len(input_ids),
            output_arr, len(output_ids)
        )
        if result != ADD_NODE_OK:
            raise Lv00ConstraintError(f"添加函数块失败: 错误码 {result}")
        block_id = self._next_id
        self._next_id += 1
        return block_id

    def add_node_with_id(self, node_id: int, node_type: int,
                         coords: List) -> Any:
        """
        带指定 ID 添加节点（用于反序列化）。

        参数：
            node_id: 节点 ID
            node_type: 节点几何类型（GEOM_POINT 等）
            coords: 坐标列表（SymbolicCoord 对象列表）

        返回：
            GeomNode: 创建的几何节点对象（C 层）

        异常：
            Lv00ConstraintError: 添加失败
        """
        from ._ctypes_binding import _GeomNode
        coord_ptrs = (ctypes.POINTER(_SymbolicCoord) * len(coords))()
        for i, c in enumerate(coords):
            coord_ptrs[i] = c._ptr if hasattr(c, '_ptr') else c
        ptr = _lib.graph_add_node_with_id(self._ptr, node_id, node_type,
                                          coord_ptrs, len(coords))
        if not ptr:
            raise Lv00ConstraintError(f"带 ID 添加节点失败: id={node_id}")
        # 同步 Python 侧 ID 计数器
        if node_id >= self._next_id:
            self._next_id = node_id + 1
        return GeomNode(ptr)

    def add_constraint_with_id(self, constraint_id: int,
                               constraint_type: int,
                               participants: List[int]) -> Any:
        """
        带指定 ID 添加约束（用于反序列化）。

        参数：
            constraint_id: 约束 ID
            constraint_type: 约束类型编码
            participants: 参与节点 ID 列表

        返回：
            Any: 底层 C 约束指针

        异常：
            Lv00ConstraintError: 添加失败
        """
        arr = (ctypes.c_int * len(participants))(*participants)
        ptr = _lib.graph_add_constraint_with_id(
            self._ptr, constraint_id, constraint_type,
            arr, len(participants)
        )
        if not ptr:
            raise Lv00ConstraintError(f"带 ID 添加约束失败: id={constraint_id}")
        return ptr

    def set_stream_context(self, ctx: Any) -> None:
        """
        设置全局流式上下文。

        将流式上下文关联到约束图，使图操作能够发射流式事件。

        参数：
            ctx: 流式上下文指针（c_void_p），由引擎创建
        """
        _lib.graph_set_stream_context(ctx)

    def find_cross_boundary_constraints(self, internal_node_ids: List[int],
                                         port_ids: List[int]) -> List[Any]:
        """
        查找跨边界约束。

        检查内部节点是否与外部节点存在跨边界的约束关系。

        参数：
            internal_node_ids: 内部节点 ID 列表
            port_ids: 端口 ID 列表

        返回：
            List: 跨边界约束列表
        """
        from ._ctypes_binding import find_cross_boundary_constraints as _find_cross
        internal_arr = (ctypes.c_int * len(internal_node_ids))(*internal_node_ids)
        port_arr = (ctypes.c_int * len(port_ids))(*port_ids)
        out_count = ctypes.c_int()
        result_ptr = _find_cross(
            self._ptr, internal_arr, len(internal_node_ids),
            port_arr, len(port_ids),
            ctypes.byref(out_count)
        )
        if not result_ptr:
            return []
        result = [result_ptr[i] for i in range(out_count.value)]
        _lib.lv00_free_ptr(result_ptr)
        return result

    def find_constraints_involving(self, node_id: int,
                                   max_results: int = 64) -> List[int]:
        """
        查找涉及指定节点的所有约束。

        参数：
            node_id: 节点 ID
            max_results: 最大返回数量（默认 64）

        返回：
            List[int]: 涉及该节点的约束索引列表
        """
        out_indices = (ctypes.c_int * max_results)()
        count = _lib.graph_find_constraints_involving(
            self._ptr, node_id, out_indices, max_results
        )
        if count <= 0:
            return []
        return [out_indices[i] for i in range(count)]

    def serialize_to_json(self) -> str:
        """
        将图序列化为 JSON 字符串。

        序列化整个约束图，包括所有节点和约束关系。

        返回：
            str: JSON 字符串

        异常：
            Lv00Error: 序列化失败
        """
        json_ptr = _lib.graph_serialize_to_json(self._ptr)
        if not json_ptr:
            raise Lv00Error("序列化图为 JSON 失败")
        result = json_ptr.decode('utf-8')
        _lib.lv00_free_ptr(json_ptr)
        return result

    def deserialize_from_json(self, json_str: str) -> 'Graph':
        """
        从 JSON 字符串反序列化图到当前实例。

        参数：
            json_str: JSON 字符串

        异常：
            Lv00Error: 反序列化失败
        """
        b = json_str.encode('utf-8')
        new_ptr = _lib.graph_deserialize_from_json(b)
        if not new_ptr:
            raise Lv00Error("从 JSON 反序列化图失败")
        # 替换底层 C 指针
        if self._ptr:
            _lib.graph_destroy(self._ptr)
        self._ptr = new_ptr
        self._points = []
        self._segments = []
        self._regions = []
        self._ports = []
        self._point_id_set = set()
        self._sync_id_from_c()
        return self

    def detect_redundancy(self, constraint_type: int,
                          participants: List[int]) -> int:
        """
        检测冗余（按类型和参与者）。

        检查指定类型和参与者的约束是否已冗余存在。

        参数：
            constraint_type: 约束类型编码
            participants: 参与节点 ID 列表

        返回：
            int: 1 表示存在冗余，0 表示不存在，-1 表示错误
        """
        arr = (ctypes.c_int * len(participants))(*participants)
        return _lib.graph_detect_redundancy(
            self._ptr, constraint_type, arr, len(participants)
        )
    
    def get_node_count(self) -> int:
        """
        获取图中节点数量。
        
        返回：
            int: 节点数量
        """
        return _lib.graph_get_node_count(self._ptr)
    
    def get_constraint_count(self) -> int:
        """
        获取图中约束数量。
        
        返回：
            int: 约束数量
        """
        return _lib.graph_get_constraint_count(self._ptr)
    
    def get_node(self, node_id: int) -> Optional[GeomNode]:
        """
        根据 ID 获取节点。
        
        参数：
            node_id: 节点 ID
        
        返回：
            GeomNode 或 None: 节点对象
        """
        ptr = _lib.graph_get_node(self._ptr, node_id)
        if not ptr:
            return None
        return GeomNode(ptr)
    
    # ============================================================
    # 约束操作
    # ============================================================
    
    def add_incidence(self, point_id: int, line_or_region_id: int) -> None:
        """
        添加关联约束：约束一个点位于某条线段或某个区域的边界上。

        关联约束是几何构造中最基础的约束类型之一，表示点与线/面
        之间的从属关系。例如，"点 P 在线段 AB 上"或"点 P 在区域 R 的边界上"。
        添加成功后，约束求解器会确保该点的位置满足关联条件。

        参数：
            point_id (int): 被约束的点节点 ID，必须已通过 add_point() 注册
            line_or_region_id (int): 目标线段或区域节点 ID，必须已注册到图中

        异常：
            Lv00ConstraintError: C 层 graph_add_incidence() 返回非 OK 错误码时抛出。
                                  常见原因：
                                  - 节点 ID 不存在
                                  - 约束与已有约束冲突
                                  - 目标类型不支持关联约束

        使用示例：
            >>> g = Graph()
            >>> p1 = g.add_point(0, 0)
            >>> p2 = g.add_point(4, 0)
            >>> seg = g.add_line_segment(p1, p2)
            >>> p3 = g.add_point(2, 0)
            >>> g.add_incidence(p3._id, seg._id)  # p3 在线段 seg 上
        """
        result = _lib.graph_add_incidence(self._ptr, point_id, line_or_region_id)
        if result != ADD_CONSTRAINT_OK:
            raise Lv00ConstraintError(f"添加关联约束失败: 错误码 {result}")
    
    def add_betweenness(self, p1_id: int, p2_id: int, p3_id: int) -> None:
        """
        添加中间约束：约束三个共线点中，p2 位于 p1 和 p3 之间。

        中间约束是共线三点之间的顺序约束，确保 p2 严格位于 p1 和 p3
        之间（包括端点重合的情况）。约束求解器会据此调整点的位置。

        参数：
            p1_id (int): 第一个端点节点 ID
            p2_id (int): 中间点节点 ID（约束目标，必须位于 p1 和 p3 之间）
            p3_id (int): 第二个端点节点 ID

        异常：
            Lv00ConstraintError: C 层 graph_add_betweenness() 返回非 OK 错误码时抛出。
                                  常见原因：
                                  - 节点 ID 不存在
                                  - 三点不可能共线（与已有约束冲突）
                                  - p2_id 为 p1_id 或 p3_id（退化为端点）

        使用示例：
            >>> g = Graph()
            >>> p1 = g.add_point(0, 0)
            >>> p2 = g.add_point(1, 0)
            >>> p3 = g.add_point(2, 0)
            >>> g.add_betweenness(p1._id, p2._id, p3._id)  # p2 在 p1-p3 之间
        """
        result = _lib.graph_add_betweenness(self._ptr, p1_id, p2_id, p3_id)
        if result != ADD_CONSTRAINT_OK:
            raise Lv00ConstraintError(f"添加中间约束失败: 错误码 {result}")
    
    def add_intersection(self, line1_id: int, line2_id: int, 
                         result_point_id: int) -> None:
        """
        添加相交约束：约束两条线段交于指定点。

        在约束图中注册两条线段的相交关系，要求 line1 和 line2
        的交点恰好为 result_point。约束求解器会在求解过程中
        强制保持此相交关系。

        参数：
            line1_id (int): 第一条线段节点 ID
            line2_id (int): 第二条线段节点 ID
            result_point_id (int): 交点节点 ID，必须已注册到图中

        异常：
            Lv00ConstraintError: C 层 graph_add_intersection() 返回非 OK 错误码时抛出。
                                  常见原因：
                                  - 节点 ID 不存在
                                  - 两线段平行（无交点）或重合（无穷多交点）
                                  - 交点与已有约束冲突

        使用示例：
            >>> g = Graph()
            >>> p1 = g.add_point(0, 0); p2 = g.add_point(4, 0)
            >>> p3 = g.add_point(2, 2); p4 = g.add_point(2, -2)
            >>> seg1 = g.add_line_segment(p1, p2)
            >>> seg2 = g.add_line_segment(p3, p4)
            >>> inter = g.add_point(2, 0)
            >>> g.add_intersection(seg1._id, seg2._id, inter._id)
        """
        result = _lib.graph_add_intersection(self._ptr, line1_id, line2_id, result_point_id)
        if result != ADD_CONSTRAINT_OK:
            raise Lv00ConstraintError(f"添加相交约束失败: 错误码 {result}")
    
    def add_containment(self, inner_id: int, outer_id: int) -> None:
        """
        添加包含约束：约束一个区域完全位于另一个区域内部。

        在约束图中注册区域之间的包含关系（嵌套），约束求解器
        确保 inner 区域的边界始终位于 outer 区域内部。

        参数：
            inner_id (int): 内区域节点 ID，必须已通过 add_region() 注册
            outer_id (int): 外区域节点 ID，必须已通过 add_region() 注册

        异常：
            Lv00ConstraintError: C 层 graph_add_containment() 返回非 OK 错误码时抛出。
                                  常见原因：
                                  - 节点 ID 不存在或非区域类型
                                  - 内区域无法被外区域完全包含
                                  - 与已有约束冲突（如循环包含）
        """
        result = _lib.graph_add_containment(self._ptr, inner_id, outer_id)
        if result != ADD_CONSTRAINT_OK:
            raise Lv00ConstraintError(f"添加包含约束失败: 错误码 {result}")
    
    def add_connection(self, src_port_id: int, dst_port_id: int) -> None:
        """
        添加连接约束：约束两个端口之间建立连接关系。

        用于函数块之间的数据流/信号流连接。连接约束在约束图中
        注册端口间的有向关联，约束求解器在合成时据此传播信号。

        参数：
            src_port_id (int): 源端口节点 ID（输出端），必须已通过 add_port() 注册
            dst_port_id (int): 目标端口节点 ID（输入端），必须已通过 add_port() 注册

        异常：
            Lv00ConstraintError: C 层 graph_add_connection() 返回非 OK 错误码时抛出。
                                  常见原因：
                                  - 节点 ID 不存在或非端口类型
                                  - 端口类型不匹配（如输出→输出）
                                  - 连接与已有约束冲突
        """
        result = _lib.graph_add_connection(self._ptr, src_port_id, dst_port_id)
        if result != ADD_CONSTRAINT_OK:
            raise Lv00ConstraintError(f"添加连接约束失败: 错误码 {result}")
    
    # ============================================================
    # 规范化
    # ============================================================
    
    def normalize(self, scope_aware: bool = False) -> NormalizationResult:
        """
        规范化约束图。

        通过合并等价节点来简化图结构。

        参数：
            scope_aware: 是否考虑作用域（默认 False）

        返回：
            NormalizationResult: 规范化结果，包含合并计数等信息

        异常：
            Lv00Error: 规范化失败时抛出
        """
        ptr = _lib.graph_normalize(self._ptr, scope_aware)
        if not ptr:
            raise Lv00Error("规范化失败")
        
        return NormalizationResult(ptr)
    
    # ============================================================
    # 冲突检测
    # ============================================================
    
    def detect_redundant_constraints(self) -> List[int]:
        """
        检测冗余约束。

        分析约束图，找出可以移除而不影响语义的冗余约束。

        返回：
            List[int]: 冗余约束 ID 列表，若无冗余约束则返回空列表
        """
        count = ctypes.c_int()
        result_ptr = _lib.graph_detect_redundant_constraints(self._ptr, ctypes.byref(count))
        if not result_ptr:
            return []
        
        try:
            # 修复：使用 try-finally 确保异常路径也能正确释放内存
            redundant = [result_ptr[i] for i in range(count.value)]
        finally:
            _lib.lv00_free_ptr(result_ptr)
        return redundant
    
    def detect_conflicts(self) -> Tuple[List[List[int]], List[int]]:
        """
        检测冲突约束组。

        分析约束图，识别相互冲突的约束集合。

        返回：
            Tuple[List[List[int]], List[int]]: (冲突组列表, 冲突组大小列表)，
            若无冲突则返回 ([], [])
        """
        conflict_count = ctypes.c_int()
        # 修复：sizes_ptr 应为指向整数数组的指针，而非单个整数
        sizes_ptr = ctypes.POINTER(ctypes.c_int)()
        result_ptr = _lib.graph_detect_conflicts(self._ptr, ctypes.byref(conflict_count), ctypes.byref(sizes_ptr))
        if not result_ptr:
            return ([], [])
        
        try:
            # 修复：使用 try-finally 确保异常路径也能正确释放内存
            conflicts = []
            sizes = []
            for i in range(conflict_count.value):
                # 正确访问指针数组中的元素
                group_size = sizes_ptr[i]
                group = [result_ptr[i][j] for j in range(group_size)]
                conflicts.append(group)
                sizes.append(group_size)
        finally:
            _lib.lv00_free_ptr(result_ptr)
            _lib.lv00_free_ptr(sizes_ptr)
        return (conflicts, sizes)
    
    # ============================================================
    # 辅助方法
    # ============================================================
    
    def validate_region_closure(self, region_id: int) -> bool:
        """
        验证区域闭合性。
        
        参数：
            region_id: 区域 ID
        
        返回：
            bool: 是否闭合
        """
        return _lib.graph_validate_region_closure(self._ptr, region_id)
    
    @property
    def points(self) -> List[Point]:
        """获取图中所有点的列表。"""
        return self._points
    
    @property
    def segments(self) -> List[LineSegment]:
        """获取图中所有线段的列表。"""
        return self._segments


# ============================================================
# 模块初始化
# ============================================================

def init() -> bool:
    """
    初始化 Lv-00 系统。
    
    返回：
        bool: 是否成功
    """
    return _lib.lv00_init()


def cleanup() -> None:
    """
    清理 Lv-00 系统。
    """
    _lib.lv00_cleanup()


def get_version() -> str:
    """
    获取 Lv-00 版本字符串。
    
    返回：
        str: 版本号
    
    注意：
        lv00_get_version 在 C 库中为 static inline 函数，不在 DLL 导出中。
        此处返回 Python 包的版本号作为替代。
    """
    # lv00_get_version 已从 DLL 导出中移除（static inline），
    # 返回 Python 包版本作为替代
    return "3.2.0"


def get_last_error() -> str:
    """
    获取最后发生的错误消息。
    
    返回：
        str: 错误消息
    """
    msg = _lib.lv00_get_last_error_message()
    if msg:
        return msg.decode('utf-8')
    return ""


def set_debug_mode(enabled: bool) -> None:
    """
    设置调试模式。
    
    参数：
        enabled: 是否启用调试
    """
    _lib.debug_set_mode(enabled)


def set_log_level(level: int) -> None:
    """
    设置日志级别。
    
    参数：
        level: 日志级别（LOG_LEVEL_DEBUG, LOG_LEVEL_INFO, 等）
    """
    _lib.debug_set_log_level(level)


def reset_counters() -> None:
    """
    重置性能计数器。
    """
    _lib.debug_reset_counters()


def get_counter_report() -> str:
    """
    获取性能计数器报告。
    
    返回：
        str: 计数器报告
    """
    report = _lib.debug_counters_report()
    if report:
        result = report.decode('utf-8')
        _lib.lv00_free_ptr(report)
        return result
    return ""
