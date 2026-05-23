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

版本：3.0.2
作者：Lv-00 开发团队
"""

import ctypes
from fractions import Fraction
from typing import Any, List, Optional, Tuple, Union

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
# 异常类定义
# ============================================================

class Lv00Error(Exception):
    """
    Lv-00 异常基类。

    所有 Lv-00 相关异常的父类，包括库错误、参数错误、
    约束冲突等。

    属性：
        message: 异常消息字符串
        error_code: 可选的错误码（整数，默认为 -1）
    """

    def __init__(self, message: str = "", error_code: int = -1) -> None:
        """
        创建 Lv-00 异常。

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

        返回：
            str: 格式为 "Lv00Error(错误码): 消息" 的字符串
        """
        if self.error_code >= 0:
            return f"{self.__class__.__name__}({self.error_code}): {self.message}"
        return f"{self.__class__.__name__}: {self.message}" if self.message else self.__class__.__name__


class Lv00LibraryError(Lv00Error):
    """库加载或初始化失败异常。"""
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
    符号坐标类。
    
    支持四种坐标类型：
    - RATIONAL：有理数，精确表示
    - ALGEBRAIC：代数数，通过最小多项式定义
    - QUADRATIC：二次根式，a + b*sqrt(n) 形式
    - TRANSCENDENTAL：超越数（π、e 等）
    
    属性：
        type: 坐标类型（字符串）
        trust: 信任颜色（green/blue/yellow/orange/amber）
    
    示例：
        >>> coord = SymbolicCoord(1, 2)  # 创建 1/2
        >>> coord2 = SymbolicCoord(Fraction(3, 4))  # 创建 3/4
        >>> result = coord + coord2  # 加法
        >>> print(result)  # 输出: 7/4
    """
    
    def __init__(self, value: Union[Fraction, int, float, str, 'SymbolicCoord']):
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
            b = value.encode('utf-8')
            self._ptr = _lib.symbolic_coord_deserialize(b)
            if not self._ptr:
                raise Lv00Error(f"无法解析符号坐标: {value}")
        else:
            raise TypeError(f"不支持的 SymbolicCoord 类型: {type(value)}")
        
        if not self._ptr:
            raise Lv00Error("创建符号坐标失败")
    
    def __del__(self):
        """
        析构函数：释放 C 分配的内存。
        
        注意：在解释器关闭时 _lib 可能已不可用，
        所以需要捕获异常。
        """
        try:
            if hasattr(self, '_ptr') and self._ptr:
                _lib.symbolic_coord_destroy(self._ptr)
                self._ptr = None
        except Exception:
            pass  # 解释器关闭时 _lib 可能已不可用
    
    def __repr__(self) -> str:
        """
        返回坐标的调试表示。
        
        返回：
            str: 格式为 "SymbolicCoord(value)" 的字符串
        """
        s = _lib.symbolic_coord_serialize(self._ptr)
        if s:
            result = s.decode('utf-8')
            _lib.free(s)
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
            _lib.free(s)
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
        """减法反向运算：other - self"""
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
        """小于比较"""
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
        """大于比较"""
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
            _lib.free(s)
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
        """创建零坐标"""
        return cls.from_rational(0)
    
    @classmethod
    def one(cls) -> 'SymbolicCoord':
        """创建单位坐标（1）"""
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
    几何点类。
    
    表示二维平面上的一个点，由 x 和 y 坐标定义。
    
    属性：
        x: X 坐标（SymbolicCoord）
        y: Y 坐标（SymbolicCoord）
        _id: 在 Graph 中的节点 ID（内部使用）
    
    示例：
        >>> p = Point(SymbolicCoord(0), SymbolicCoord(0))  # 原点
        >>> p2 = Point(1, 2)  # 坐标 (1, 2)
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
        """返回点的调试表示"""
        return f"Point({self.x}, {self.y})"
    
    def __eq__(self, other: Any) -> bool:
        """检查两点是否相等"""
        if not isinstance(other, Point):
            return False
        return self.x == other.x and self.y == other.y

    # 修复：定义了 __eq__ 后必须显式设置 __hash__，否则 Python 3 会将其设为 None
    # Point 的相等性基于可变坐标值，不适合用作哈希键，因此显式标记为不可哈希
    __hash__ = None
    
    def distance_to(self, other: 'Point') -> SymbolicCoord:
        """
        计算到另一个点的距离。
        
        参数：
            other: 另一个点
        
        返回：
            SymbolicCoord: 欧几里得距离
        """
        dx = self.x - other.x
        dy = self.y - other.y
        return (dx * dx + dy * dy) ** SymbolicCoord.from_rational(1, 2)
    
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
    线段类。
    
    表示连接两个点之间的线段。
    
    属性：
        p1: 起点（Point）
        p2: 终点（Point）
        _id: 在 Graph 中的节点 ID（内部使用）
    
    示例：
        >>> seg = LineSegment(Point(0, 0), Point(1, 1))
    """
    
    def __init__(self, p1: Point, p2: Point):
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
    
    def __init__(self, ptr):
        """
        内部构造函数。
        
        参数：
            ptr: 底层 C 指针
        """
        self._ptr = ptr
    
    @property
    def id(self) -> int:
        """节点 ID"""
        return self._ptr.contents.id
    
    @property
    def type(self) -> int:
        """节点几何类型"""
        return self._ptr.contents.type
    
    @property
    def type_name(self) -> str:
        """节点几何类型名称"""
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
    规范化结果类。

    表示图规范化操作的结果，由 Graph.normalize() 方法返回。
    封装底层 C 结构的 _NormalizationResult，自动管理 C 内存生命周期。

    注意：
        此对象由 normalization.py 作为薄层重新导出，
        以保持向后兼容性。新代码应直接从 lv00.core 导入。

    属性：
        _ptr: 底层 C 指针（POINTER(_NormalizationResult)）
    """
    
    def __init__(self, ptr):
        """
        内部构造函数。
        
        参数：
            ptr: 底层 C 指针
        """
        self._ptr = ptr
    
    def __del__(self):
        """析构函数：释放 C 分配的内存"""
        try:
            if hasattr(self, '_ptr') and self._ptr:
                _lib.normalization_result_destroy(self._ptr)
                self._ptr = None
        except Exception:
            pass


# ============================================================
# Graph 类
# ============================================================

class Graph:
    """
    约束图类。
    
    表示几何约束图，包含节点（点、线段、区域等）
    和约束（关联、中间、等距等）。
    
    属性：
        _ptr: 底层 C 指针
        _points: 添加的点列表
        _segments: 添加的线段列表
    
    示例：
        >>> g = Graph()
        >>> p1 = g.add_point(0, 0)
        >>> p2 = g.add_point(1, 0)
        >>> seg = g.add_line_segment(p1, p2)
    """
    
    def __init__(self):
        """
        创建新的约束图。

        异常：
            Lv00LibraryError: 创建失败
        """
        self._ptr = _lib.graph_create()
        if not self._ptr:
            raise Lv00LibraryError("创建约束图失败")
        self._points: List[Point] = []
        self._segments: List[LineSegment] = []
        self._regions: List[int] = []
        self._ports: List[int] = []
        # 注：Python 侧 ID 计数器仅用于辅助 Python 对象跟踪，
        # 不取代 C 层 graph_get_node_count() 的权威 ID 管理。
        # 同步策略：初始化时从 C 层获取当前节点数作为基线。
        self._next_id = _lib.graph_get_node_count(self._ptr)
    
    def __del__(self):
        """
        析构函数：释放 C 分配的内存。
        """
        try:
            if hasattr(self, '_ptr') and self._ptr:
                _lib.graph_destroy(self._ptr)
                self._ptr = None
        except Exception:
            pass
    
    def __repr__(self) -> str:
        """
        返回图的调试表示。

        返回：
            str: 图的字符串表示
        """
        return f"Graph(points={len(self._points)}, segments={len(self._segments)})"

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
        向图中添加一个点。
        
        参数：
            x: X 坐标
            y: Y 坐标
        
        返回：
            Point: 创建的点对象
        
        异常：
            Lv00Error: 添加失败
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
        return point
    
    def add_line_segment(self, p1: Point, p2: Point) -> LineSegment:
        """
        向图中添加一条线段。
        
        参数：
            p1: 起点
            p2: 终点
        
        返回：
            LineSegment: 创建的线段对象
        
        异常：
            Lv00Error: 点未添加到图中或添加失败
        """
        if p1 not in self._points or p2 not in self._points:
            # 性能说明：self._points 为 list，此处的 in 操作为 O(n)。
            # 由于 Point 定义了 __hash__ = None（不可哈希），无法直接使用 set 加速。
            # 当前实现适用于中小规模图（点数 < 1000）；若需支持大规模图，
            # 可考虑引入 _point_id_set: set[int] 辅助集合，以 _id 字段进行 O(1) 查找。
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
        添加关联约束（点在线/区域上）。

        参数：
            point_id: 点 ID
            line_or_region_id: 线段或区域 ID

        异常：
            Lv00ConstraintError: 添加关联约束失败时抛出
        """
        result = _lib.graph_add_incidence(self._ptr, point_id, line_or_region_id)
        if result != ADD_CONSTRAINT_OK:
            raise Lv00ConstraintError(f"添加关联约束失败: 错误码 {result}")
    
    def add_betweenness(self, p1_id: int, p2_id: int, p3_id: int) -> None:
        """
        添加中间约束（p1-p2-p3 共线且 p2 在中间）。

        参数：
            p1_id: 第一个点 ID
            p2_id: 中间点 ID
            p3_id: 第三个点 ID

        异常：
            Lv00ConstraintError: 添加中间约束失败时抛出
        """
        result = _lib.graph_add_betweenness(self._ptr, p1_id, p2_id, p3_id)
        if result != ADD_CONSTRAINT_OK:
            raise Lv00ConstraintError(f"添加中间约束失败: 错误码 {result}")
    
    def add_intersection(self, line1_id: int, line2_id: int, 
                         result_point_id: int) -> None:
        """
        添加相交约束（两线交于一点）。

        参数：
            line1_id: 第一条线段 ID
            line2_id: 第二条线段 ID
            result_point_id: 交点 ID

        异常：
            Lv00ConstraintError: 添加相交约束失败时抛出
        """
        result = _lib.graph_add_intersection(self._ptr, line1_id, line2_id, result_point_id)
        if result != ADD_CONSTRAINT_OK:
            raise Lv00ConstraintError(f"添加相交约束失败: 错误码 {result}")
    
    def add_containment(self, inner_id: int, outer_id: int) -> None:
        """
        添加包含约束（内区域在外区域内）。

        参数：
            inner_id: 内区域 ID
            outer_id: 外区域 ID

        异常：
            Lv00ConstraintError: 添加包含约束失败时抛出
        """
        result = _lib.graph_add_containment(self._ptr, inner_id, outer_id)
        if result != ADD_CONSTRAINT_OK:
            raise Lv00ConstraintError(f"添加包含约束失败: 错误码 {result}")
    
    def add_connection(self, src_port_id: int, dst_port_id: int) -> None:
        """
        添加连接约束（端口连接）。

        参数：
            src_port_id: 源端口 ID
            dst_port_id: 目标端口 ID

        异常：
            Lv00ConstraintError: 添加连接约束失败时抛出
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
        
        redundant = [result_ptr[i] for i in range(count.value)]
        _lib.free(result_ptr)
        return redundant
    
    def detect_conflicts(self) -> 'Tuple[List[List[int]], List[int]]':
        """
        检测冲突约束组。

        分析约束图，识别相互冲突的约束集合。

        返回：
            Tuple[List[List[int]], List[int]]: (冲突组列表, 冲突组大小列表)，
            若无冲突则返回 ([], [])
        """
        conflict_count = ctypes.c_int()
        sizes_ptr = ctypes.c_int()
        result_ptr = _lib.graph_detect_conflicts(self._ptr, ctypes.byref(conflict_count), ctypes.byref(sizes_ptr))
        if not result_ptr:
            return ([], [])
        
        conflicts = []
        sizes = []
        for i in range(conflict_count.value):
            group = [result_ptr[i][j] for j in range(sizes_ptr[i])]
            conflicts.append(group)
            sizes.append(sizes_ptr[i])
        
        _lib.free(result_ptr)
        _lib.free(sizes_ptr)
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
        """获取图中所有点"""
        return self._points
    
    @property
    def segments(self) -> List[LineSegment]:
        """获取图中所有线段"""
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
    """
    return _lib.lv00_get_version().decode('utf-8')


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
        _lib.free(report)
        return result
    return ""
