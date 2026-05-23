"""
Lv-00 DSL 模块 — PyEuclid 风格链式几何构造语言

汲取 PyEuclid 的设计哲学——"让用户用最简单语法表达复杂几何关系"——
本模块在 lv00 现有核心基础上实现了极致简洁的链式几何 DSL。

设计原则：
    1. 链式调用（Chainable）: 方法返回 self，支持流畅的链式调用
    2. 声明式（Declarative）: 代码读起来像几何命题
    3. 自动命名（Auto-naming）: 未命名点自动按 A, B, C, ... 命名
    4. 上下文管理器（Context Manager）: ``with G() as g:`` 模式管理构造作用域
    5. 运算符重载: ``A - B`` 表示连接线段 AB

用法示例：
    >>> from lv00.dsl import G
    >>>
    >>> with G() as g:
    ...     A = g.point(0, 0, "A")
    ...     B = g.point(4, 0, "B")
    ...     C = g.point(0, 3, "C")
    ...
    ...     AB = A - B          # 运算符重载创建线段
    ...     BC = B - C
    ...     CA = C - A
    ...
    ...     M = g.midpoint(A, B, "M_AB")
    ...     O = g.circumcenter(A, B, C)
    ...
    ...     g.on(M, AB)          # 关联约束：M 在 AB 上
    ...     d = g.distance(A, B)  # → 4.0
    ...
    ...     print(g.to_dsl())    # 导出为 Lv-00 DSL 文本

版本：3.1.0
作者：Lv-00 开发团队
"""

from __future__ import annotations

import math
from typing import Any, Dict, List, Optional, Tuple, Union

from .core import Graph, LineSegment, Point, SymbolicCoord, Lv00Error

# ============================================================
# 自定义异常
# ============================================================


class DSLError(Lv00Error):
    """DSL 构造异常。

    当几何构造违反数学约束（如三点共线无法构成三角形、
    平行线被要求相交等）时抛出。所有错误消息均为中文。
    """

    def __init__(self, message: str = "", error_code: int = -1) -> None:
        if not message:
            message = "几何 DSL 构造错误"
        super().__init__(message, error_code)


# ============================================================
# 自动命名辅助
# ============================================================


def _make_name_generator():
    """生成 A, B, C, ..., Z, AA, AB, ... 序列的命名器。
    
    模仿 Excel 列标命名规则：单个字母用完后递增为双字母组合。
    """

    def gen():
        n = 0
        while True:
            letters = []
            temp = n
            while True:
                letters.append(chr(ord('A') + temp % 26))
                temp = temp // 26 - 1
                if temp < 0:
                    break
            yield ''.join(reversed(letters))
            n += 1

    g = gen()
    return lambda: next(g)


# 模块级自动命名器，在同一个进程中共享
_auto_name = _make_name_generator()


# ============================================================
# 内部辅助
# ============================================================


def _to_coord(value: Union[SymbolicCoord, int, float]) -> SymbolicCoord:
    """将数值转为 SymbolicCoord。"""
    if isinstance(value, SymbolicCoord):
        return value
    return SymbolicCoord(value)


def _to_core_point(value, graph: Graph) -> Point:
    """将 PointWrapper / Point / tuple 转为 core.Point。

    参数：
        value: 点表示（PointWrapper、Point、或 (x, y) 元组）
        graph: 关联的约束图

    返回：
        Point: core.Point 对象

    异常：
        TypeError: 不支持的输入类型
    """
    # 延迟导入避免循环
    if type(value).__name__ == 'PointWrapper':
        return value._point
    if isinstance(value, Point):
        return value
    if isinstance(value, (tuple, list)):
        x, y = float(value[0]), float(value[1])
        return Point(_to_coord(x), _to_coord(y))
    raise TypeError(f"无法将 {type(value).__name__} 转换为 Point")


def _float(val: SymbolicCoord) -> float:
    """将 SymbolicCoord 安全转为 float。"""
    return val.to_double()


# ============================================================
# PointWrapper — 点包装器
# ============================================================


class PointWrapper:
    """点包装器。

    封装 core.Point，提供 DSL 便利方法和运算符重载。

    属性：
        x (SymbolicCoord): X 坐标
        y (SymbolicCoord): Y 坐标
        id (int): 在约束图中的节点 ID
        name (str): 点的名称

    运算符：
        - : A - B 返回连接 A 和 B 的线段

    示例：
        >>> A = g.point(0, 0, "A")
        >>> B = g.point(3, 4, "B")
        >>> d = A.distance_to(B)       # 距离
        >>> AB = A - B                  # 线段
        >>> M = A.midpoint(B)           # 中点
    """

    __slots__ = ('_point', '_ctx', '_name')

    def __init__(self, point: Point, context: 'G', name: Optional[str] = None):
        """创建点包装器。

        参数：
            point: 底层 core.Point 对象
            context: 所属的 G 上下文
            name: 点名称（可选）
        """
        self._point = point
        self._ctx = context
        self._name = name

    # ---- 属性 ----

    @property
    def x(self) -> SymbolicCoord:
        """X 坐标（符号坐标）。"""
        return self._point.x

    @property
    def y(self) -> SymbolicCoord:
        """Y 坐标（符号坐标）。"""
        return self._point.y

    @property
    def id(self) -> int:
        """在约束图中的节点 ID。"""
        return self._point._id

    @property
    def name(self) -> Optional[str]:
        """点的名称。"""
        return self._name

    @name.setter
    def name(self, value: str) -> None:
        self._name = value

    # ---- 运算符重载 ----

    def __sub__(self, other: PointWrapper) -> 'SegmentWrapper':
        """A - B：创建连接 A 和 B 的线段。

        参数：
            other: 另一个 PointWrapper

        返回：
            SegmentWrapper: 连接两点的线段
        """
        if not isinstance(other, PointWrapper):
            return NotImplemented
        return self._ctx.segment(self, other)

    def __repr__(self) -> str:
        """返回调试表示。"""
        name = self._name or '?'
        return f"PointWrapper({name}: ({_float(self.x):.4f}, {_float(self.y):.4f}))"

    def __eq__(self, other: Any) -> bool:
        """判断两点是否相等（基于 core.Point 的坐标相等性）。"""
        if isinstance(other, PointWrapper):
            return self._point == other._point
        return False

    def __hash__(self) -> int:
        """哈希值（基于对象 id）。"""
        return hash(id(self._point))

    # ---- 几何方法 ----

    def distance_to(self, other: 'PointWrapper') -> float:
        """计算到另一点的距离（浮点近似值）。

        参数：
            other: 另一个点

        返回：
            float: 欧几里得距离
        """
        return _float(self._point.distance_to(other._point))

    def midpoint(self, other: 'PointWrapper') -> 'PointWrapper':
        """计算到另一点的中点。

        参数：
            other: 另一个点

        返回：
            PointWrapper: 中点
        """
        mid = self._point.mid_point(other._point)
        return self._ctx._wrap_point(Point(mid.x, mid.y))

    def vector_to(self, other: 'PointWrapper') -> Tuple[SymbolicCoord, SymbolicCoord]:
        """返回到另一点的向量 (dx, dy)。

        参数：
            other: 目标点

        返回：
            Tuple[SymbolicCoord, SymbolicCoord]: (dx, dy)
        """
        return self._point.vector_to(other._point)

    def is_collinear_with(self, p2: 'PointWrapper', p3: 'PointWrapper') -> bool:
        """判断三点是否共线。

        参数：
            p2: 第二个点
            p3: 第三个点

        返回：
            bool: 三点共线返回 True
        """
        return self._point.is_collinear_with(p2._point, p3._point)

    # ---- 导出 ----

    def to_dsl(self) -> str:
        """导出为 Lv-00 DSL 表示。

        返回：
            str: 形如 point(x, y) 的字符串
        """
        return f"point({self._point.x}, {self._point.y})"


# ============================================================
# SegmentWrapper — 线段包装器
# ============================================================


class SegmentWrapper:
    """线段包装器。

    封装 core.LineSegment 或表示两个 PointWrapper 之间的线段。

    属性：
        p1 (PointWrapper): 起点
        p2 (PointWrapper): 终点
        id (int): 在约束图中的节点 ID（已注册到图时有效）

    示例：
        >>> AB = g.segment(A, B)
        >>> AB.length()           # 长度
        >>> M = AB.midpoint()     # 中点
        >>> L = AB.perpendicular_at(A)  # 过 A 的垂线
    """

    __slots__ = ('_p1', '_p2', '_seg', '_ctx', '_name')

    def __init__(self, p1: PointWrapper, p2: PointWrapper,
                 context: 'G', name: Optional[str] = None,
                 segment: Optional[LineSegment] = None):
        """创建线段包装器。

        参数：
            p1: 起点
            p2: 终点
            context: 所属的 G 上下文
            name: 线段名称（可选）
            segment: 底层 core.LineSegment（可选，延迟创建）
        """
        self._p1 = p1
        self._p2 = p2
        self._seg = segment
        self._ctx = context
        self._name = name

    @property
    def p1(self) -> PointWrapper:
        """起点。"""
        return self._p1

    @property
    def p2(self) -> PointWrapper:
        """终点。"""
        return self._p2

    @property
    def id(self) -> int:
        """在约束图中的节点 ID。"""
        if self._seg is not None:
            return self._seg._id
        return -1

    @property
    def core(self) -> LineSegment:
        """获取底层 core.LineSegment 对象（延迟创建）。

        返回：
            LineSegment: 底层线段对象
        """
        if self._seg is None:
            self._seg = LineSegment(self._p1._point, self._p2._point)
        return self._seg

    # ---- 几何方法 ----

    def length(self) -> float:
        """线段长度（浮点近似值）。

        返回：
            float: 欧几里得长度
        """
        return _float(self.core.length())

    def midpoint(self) -> PointWrapper:
        """线段中点。

        返回：
            PointWrapper: 中点
        """
        mid = self.core.midpoint()
        return self._ctx._wrap_point(Point(mid.x, mid.y))

    def direction(self) -> Tuple[SymbolicCoord, SymbolicCoord]:
        """线段方向向量 (dx, dy)。

        返回：
            Tuple[SymbolicCoord, SymbolicCoord]: 方向向量
        """
        return self.core.direction_vector()

    def perpendicular_at(self, point: PointWrapper) -> 'LineWrapper':
        """过指定点作垂线。

        参数：
            point: 垂线通过的点

        返回：
            LineWrapper: 垂直于当前线段的直线
        """
        dx, dy = self.direction()
        # 垂直方向：将方向向量逆时针旋转 90 度
        perp_dx = -dy
        perp_dy = dx
        p2_core = Point(
            point._point.x + perp_dx,
            point._point.y + perp_dy,
        )
        p2_w = self._ctx._wrap_point(p2_core)
        return self._ctx.line(point, p2_w)

    def parallel_at(self, point: PointWrapper) -> 'LineWrapper':
        """过指定点作平行线。

        参数：
            point: 平行线通过的点

        返回：
            LineWrapper: 平行于当前线段的直线
        """
        dx, dy = self.direction()
        p2_core = Point(
            point._point.x + dx,
            point._point.y + dy,
        )
        p2_w = self._ctx._wrap_point(p2_core)
        return self._ctx.line(point, p2_w)

    def is_parallel_to(self, other: 'SegmentWrapper') -> bool:
        """是否平行于另一线段。

        参数：
            other: 另一线段

        返回：
            bool: 平行返回 True
        """
        return self.core.is_parallel_to(other.core)

    def is_perpendicular_to(self, other: 'SegmentWrapper') -> bool:
        """是否垂直于另一线段。

        参数：
            other: 另一线段

        返回：
            bool: 垂直返回 True
        """
        return self.core.is_perpendicular_to(other.core)

    def __repr__(self) -> str:
        """返回调试表示。"""
        name = f"'{self._name}' " if self._name else ""
        return f"SegmentWrapper({name}{self._p1.name or '?'}-{self._p2.name or '?'})"

    # ---- 导出 ----

    def to_dsl(self) -> str:
        """导出为 Lv-00 DSL 表示。

        返回：
            str: 形如 segment(point(...), point(...)) 的字符串
        """
        return f"segment({self._p1.to_dsl()}, {self._p2.to_dsl()})"


# ============================================================
# LineWrapper — 无限直线包装器
# ============================================================


class LineWrapper:
    """无限直线包装器。

    表示通过 p1 和 p2 的无限直线。

    属性：
        p1 (PointWrapper): 直线上一点
        p2 (PointWrapper): 直线上另一点

    示例：
        >>> l = g.line(A, B)
        >>> l2 = g.perpendicular(C, AB)
    """

    __slots__ = ('_p1', '_p2', '_ctx')

    def __init__(self, p1: PointWrapper, p2: PointWrapper, context: 'G'):
        """创建直线包装器。

        参数：
            p1: 直线上一点
            p2: 直线上另一点
            context: 所属的 G 上下文
        """
        self._p1 = p1
        self._p2 = p2
        self._ctx = context

    @property
    def p1(self) -> PointWrapper:
        """直线上第一个点。"""
        return self._p1

    @property
    def p2(self) -> PointWrapper:
        """直线上第二个点。"""
        return self._p2

    def direction(self) -> Tuple[SymbolicCoord, SymbolicCoord]:
        """方向向量 (dx, dy)。

        返回：
            Tuple[SymbolicCoord, SymbolicCoord]: 方向向量
        """
        return (
            self._p2._point.x - self._p1._point.x,
            self._p2._point.y - self._p1._point.y,
        )

    def __repr__(self) -> str:
        """返回调试表示。"""
        return f"LineWrapper({self._p1.name or '?'}, {self._p2.name or '?'})"

    def to_dsl(self) -> str:
        """导出为 Lv-00 DSL 表示。"""
        return f"line({self._p1.to_dsl()}, {self._p2.to_dsl()})"


# ============================================================
# CircleWrapper — 圆包装器
# ============================================================


class CircleWrapper:
    """圆包装器。

    表示以 center 为圆心、radius 为半径的圆。

    属性：
        center (PointWrapper): 圆心
        radius (float): 半径（浮点近似值）
        radius_exact (SymbolicCoord): 精确半径

    示例：
        >>> c = g.circle(center=O, radius=5)
        >>> c2 = g.circle(center=O, through=A)  # 以 OA 为半径
        >>> c.area()
        >>> c.intersect_line(l)  # 圆与直线的交点
    """

    __slots__ = ('_center', '_radius_exact', '_ctx')

    def __init__(self, center: PointWrapper,
                 radius: Union[SymbolicCoord, int, float],
                 context: 'G'):
        """创建圆包装器。

        参数：
            center: 圆心
            radius: 半径（正数）
            context: 所属的 G 上下文

        异常：
            DSLError: 半径为零或负数
        """
        if not isinstance(radius, SymbolicCoord):
            radius = SymbolicCoord(radius)
        if radius.is_zero() or radius.is_negative():
            raise DSLError(f"圆的半径必须为正数，当前值为 {radius}")
        self._center = center
        self._radius_exact = radius
        self._ctx = context

    @property
    def center(self) -> PointWrapper:
        """圆心。"""
        return self._center

    @property
    def radius(self) -> float:
        """半径（浮点近似值）。"""
        return _float(self._radius_exact)

    @property
    def radius_exact(self) -> SymbolicCoord:
        """精确半径（符号坐标）。"""
        return self._radius_exact

    def area(self) -> float:
        """圆的面积（浮点近似值）。

        返回：
            float: pi * r^2
        """
        return math.pi * self.radius ** 2

    def circumference(self) -> float:
        """圆的周长（浮点近似值）。

        返回：
            float: 2 * pi * r
        """
        return 2 * math.pi * self.radius

    def contains_point(self, point: PointWrapper) -> bool:
        """判断点是否在圆内（含边界）。

        参数：
            point: 待检测的点

        返回：
            bool: 在圆内返回 True
        """
        d = self._center.distance_to(point)
        return d <= self.radius

    def tangent_at(self, point: PointWrapper) -> 'LineWrapper':
        """圆上一点的切线。

        参数：
            point: 圆上的点

        返回：
            LineWrapper: 过该点的切线
        """
        dx = point._point.x - self._center._point.x
        dy = point._point.y - self._center._point.y
        # 切线方向为 (-dy, dx)，即半径向量逆时针转 90 度
        tan_dx = -dy
        tan_dy = dx
        p2_core = Point(
            point._point.x + tan_dx,
            point._point.y + tan_dy,
        )
        p2_w = self._ctx._wrap_point(p2_core)
        return LineWrapper(point, p2_w, self._ctx)

    def intersect_line(self, line: 'LineWrapper') -> List[PointWrapper]:
        """计算圆与直线的交点。

        使用解析几何方法求解二次方程。

        参数：
            line: 直线

        返回：
            List[PointWrapper]: 交点列表（0、1 或 2 个）
        """
        cx = self._center._point.x
        cy = self._center._point.y
        r = self._radius_exact

        dx = line._p2._point.x - line._p1._point.x
        dy = line._p2._point.y - line._p1._point.y
        fx = line._p1._point.x - cx
        fy = line._p1._point.y - cy

        a = dx * dx + dy * dy
        b = SymbolicCoord(2) * (fx * dx + fy * dy)
        c = fx * fx + fy * fy - r * r

        discriminant = b * b - SymbolicCoord(4) * a * c

        if discriminant.is_negative():
            return []

        if discriminant.is_zero():
            t = (-b) / (SymbolicCoord(2) * a)
            ix = line._p1._point.x + t * dx
            iy = line._p1._point.y + t * dy
            pw = self._ctx._wrap_point(Point(ix, iy))
            return [pw]

        # 两个交点
        sqrt_disc = discriminant ** SymbolicCoord.from_rational(1, 2)
        t1 = (-b - sqrt_disc) / (SymbolicCoord(2) * a)
        t2 = (-b + sqrt_disc) / (SymbolicCoord(2) * a)

        i1 = Point(
            line._p1._point.x + t1 * dx,
            line._p1._point.y + t1 * dy,
        )
        i2 = Point(
            line._p1._point.x + t2 * dx,
            line._p1._point.y + t2 * dy,
        )
        return [self._ctx._wrap_point(i1), self._ctx._wrap_point(i2)]

    def __repr__(self) -> str:
        """返回调试表示。"""
        return (f"CircleWrapper(center={self._center.name or '?'}, "
                f"r={self.radius:.4f})")

    def to_dsl(self) -> str:
        """导出为 Lv-00 DSL 表示。"""
        return f"circle({self._center.to_dsl()}, {self._radius_exact})"


# ============================================================
# TriangleWrapper — 三角形包装器
# ============================================================


class TriangleWrapper:
    """三角形包装器。

    由三个顶点定义的三角形，提供丰富的几何查询和构造方法。

    属性：
        A, B, C (PointWrapper): 三个顶点

    示例：
        >>> t = g.triangle(A, B, C)
        >>> t.area()
        >>> G = t.centroid()        # 重心
        >>> O = t.circumcenter()    # 外心
        >>> H = t.orthocenter()     # 垂心
        >>> I = t.incenter()        # 内心
        >>> t.is_right()            # 是否直角三角形
    """

    __slots__ = ('_A', '_B', '_C', '_ctx')

    def __init__(self, a: PointWrapper, b: PointWrapper, c: PointWrapper,
                 context: 'G'):
        """创建三角形。

        参数：
            a, b, c: 三个顶点
            context: 所属的 G 上下文

        异常：
            DSLError: 三点共线无法构成三角形
        """
        if a._point.is_collinear_with(b._point, c._point):
            raise DSLError(
                f"无法构造三角形：三点 ({a.name}, {b.name}, {c.name}) 共线"
            )
        self._A = a
        self._B = b
        self._C = c
        self._ctx = context

    @property
    def A(self) -> PointWrapper:
        """顶点 A。"""
        return self._A

    @property
    def B(self) -> PointWrapper:
        """顶点 B。"""
        return self._B

    @property
    def C(self) -> PointWrapper:
        """顶点 C。"""
        return self._C

    def area(self) -> float:
        """三角形面积（浮点近似值）。

        使用行列式公式：|det(AB, AC)| / 2

        返回：
            float: 三角形面积
        """
        det = (
            (self._B._point.x - self._A._point.x) *
            (self._C._point.y - self._A._point.y) -
            (self._B._point.y - self._A._point.y) *
            (self._C._point.x - self._A._point.x)
        )
        return abs(_float(det)) / 2.0

    def perimeter(self) -> float:
        """三角形周长（浮点近似值）。

        返回：
            float: AB + BC + CA
        """
        return (
            self._A.distance_to(self._B) +
            self._B.distance_to(self._C) +
            self._C.distance_to(self._A)
        )

    def centroid(self) -> PointWrapper:
        """重心：三边中线的交点。

        重心坐标 = (A + B + C) / 3

        返回：
            PointWrapper: 重心点
        """
        cx = (_float(self._A.x) + _float(self._B.x) + _float(self._C.x)) / 3.0
        cy = (_float(self._A.y) + _float(self._B.y) + _float(self._C.y)) / 3.0
        return self._ctx.point(cx, cy)

    def circumcenter(self) -> PointWrapper:
        """外心：三边垂直平分线的交点。

        使用行列式方法计算外接圆圆心坐标。

        返回：
            PointWrapper: 外心点

        异常：
            DSLError: 三点共线或近共线时无法计算
        """
        ax, ay = _float(self._A.x), _float(self._A.y)
        bx, by = _float(self._B.x), _float(self._B.y)
        cx, cy = _float(self._C.x), _float(self._C.y)

        d = 2.0 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by))
        if abs(d) < 1e-15:
            raise DSLError("无法计算外心：三点共线或近共线")

        ux = ((ax*ax + ay*ay) * (by - cy) +
              (bx*bx + by*by) * (cy - ay) +
              (cx*cx + cy*cy) * (ay - by)) / d
        uy = ((ax*ax + ay*ay) * (cx - bx) +
              (bx*bx + by*by) * (ax - cx) +
              (cx*cx + cy*cy) * (bx - ax)) / d

        return self._ctx.point(ux, uy)

    def orthocenter(self) -> PointWrapper:
        """垂心：三边高线的交点。

        垂心 H 满足：H = A + B + C - 2*O （其中 O 为外心）

        返回：
            PointWrapper: 垂心点
        """
        O = self.circumcenter()
        hx = (_float(self._A.x) + _float(self._B.x) + _float(self._C.x) -
              2.0 * _float(O.x))
        hy = (_float(self._A.y) + _float(self._B.y) + _float(self._C.y) -
              2.0 * _float(O.y))
        return self._ctx.point(hx, hy)

    def incenter(self) -> PointWrapper:
        """内心：三边角平分线的交点。

        内心坐标 = (a*A + b*B + c*C) / (a+b+c)，
        其中 a, b, c 分别为边 BC, CA, AB 的长度。

        返回：
            PointWrapper: 内心点

        异常：
            DSLError: 三角形退化时无法计算
        """
        a = self._B.distance_to(self._C)
        b_len = self._C.distance_to(self._A)
        c = self._A.distance_to(self._B)
        p = a + b_len + c
        if abs(p) < 1e-15:
            raise DSLError("无法计算内心：三角形退化")
        ix = (a * _float(self._A.x) +
              b_len * _float(self._B.x) +
              c * _float(self._C.x)) / p
        iy = (a * _float(self._A.y) +
              b_len * _float(self._B.y) +
              c * _float(self._C.y)) / p
        return self._ctx.point(ix, iy)

    def is_right(self, tolerance: float = 1e-9) -> bool:
        """是否为直角三角形（勾股定理检测）。

        参数：
            tolerance: 容差

        返回：
            bool: 直角三角形返回 True
        """
        sides = [
            self._A.distance_to(self._B),
            self._B.distance_to(self._C),
            self._C.distance_to(self._A),
        ]
        sides.sort()
        return abs(sides[0]**2 + sides[1]**2 - sides[2]**2) < tolerance

    def is_equilateral(self, tolerance: float = 1e-9) -> bool:
        """是否为等边三角形。

        参数：
            tolerance: 容差

        返回：
            bool: 等边三角形返回 True
        """
        ab = self._A.distance_to(self._B)
        bc = self._B.distance_to(self._C)
        ca = self._C.distance_to(self._A)
        return abs(ab - bc) < tolerance and abs(bc - ca) < tolerance

    def __repr__(self) -> str:
        """返回调试表示。"""
        return (f"TriangleWrapper({self._A.name or '?'}, "
                f"{self._B.name or '?'}, {self._C.name or '?'})")

    def to_dsl(self) -> str:
        """导出为 Lv-00 DSL 表示。"""
        return (f"triangle({self._A.to_dsl()}, {self._B.to_dsl()}, "
                f"{self._C.to_dsl()})")


# ============================================================
# PolygonWrapper — 多边形包装器
# ============================================================


class PolygonWrapper:
    """多边形包装器。

    由顶点序列（按顺序）定义的多边形。

    属性：
        vertices (List[PointWrapper]): 顶点列表

    示例：
        >>> sq = g.square(A, B)
        >>> sq.area()
        >>> sq.perimeter()
        >>> sq.is_regular()
    """

    __slots__ = ('_vertices', '_ctx')

    def __init__(self, vertices: List[PointWrapper], context: 'G'):
        """创建多边形。

        参数：
            vertices: 顶点列表（至少 3 个）
            context: 所属的 G 上下文

        异常：
            DSLError: 顶点数不足
        """
        if len(vertices) < 3:
            raise DSLError(
                f"多边形至少需要 3 个顶点，当前仅 {len(vertices)} 个"
            )
        self._vertices = list(vertices)
        self._ctx = context

    @property
    def vertices(self) -> List[PointWrapper]:
        """顶点的副本列表。"""
        return list(self._vertices)

    def area(self) -> float:
        """多边形面积（鞋带公式 / 高斯面积公式）。

        返回：
            float: 多边形面积（绝对值）
        """
        n = len(self._vertices)
        total = 0.0
        for i in range(n):
            xi = _float(self._vertices[i].x)
            yi = _float(self._vertices[i].y)
            xj = _float(self._vertices[(i + 1) % n].x)
            yj = _float(self._vertices[(i + 1) % n].y)
            total += xi * yj - xj * yi
        return abs(total) / 2.0

    def perimeter(self) -> float:
        """多边形周长。

        返回：
            float: 所有边长的和
        """
        total = 0.0
        n = len(self._vertices)
        for i in range(n):
            total += self._vertices[i].distance_to(
                self._vertices[(i + 1) % n]
            )
        return total

    def is_regular(self, tolerance: float = 1e-9) -> bool:
        """是否为正多边形（所有边长相等）。

        参数：
            tolerance: 边长比较容差

        返回：
            bool: 正多边形返回 True
        """
        n = len(self._vertices)
        if n < 3:
            return False
        side = self._vertices[0].distance_to(self._vertices[1])
        for i in range(1, n):
            s = self._vertices[i].distance_to(
                self._vertices[(i + 1) % n]
            )
            if abs(s - side) > tolerance:
                return False
        return True

    def __repr__(self) -> str:
        """返回调试表示。"""
        v_str = ", ".join(v.name or '?' for v in self._vertices)
        return f"PolygonWrapper({v_str})"

    def to_dsl(self) -> str:
        """导出为 Lv-00 DSL 表示。"""
        coords = ", ".join(v.to_dsl() for v in self._vertices)
        return f"polygon({coords})"


# ============================================================
# G — 几何构造上下文
# ============================================================


class G:
    """几何构造上下文（类似 PyEuclid 的 G）。

    使用 ``with G() as g:`` 模式创建局部几何构造作用域。
    所有在作用域内创建的点、线段等均注册到此上下文中。

    支持的操作：
        - 点构造：point() / P()
        - 线段构造：segment()
        - 直线构造：line()
        - 圆构造：circle()
        - 三角形构造：triangle()
        - 多边形构造：polygon() / square()
        - 中点：midpoint()
        - 垂线/平行线：perpendicular() / parallel()
        - 交点：intersect()
        - 特殊点：circumcenter() / centroid() / orthocenter() / incenter()
        - 测量：distance() / angle()
        - 约束：on() / between()
        - 导出：build() / to_dsl()

    示例：
        >>> from lv00.dsl import G
        >>>
        >>> with G() as g:
        ...     A = g.point(0, 0, "A")
        ...     B = g.point(4, 0, "B")
        ...     C = g.point(0, 3, "C")
        ...
        ...     AB = A - B  # 运算符重载
        ...     M = g.midpoint(A, B, "M_AB")
        ...     O = g.circumcenter(A, B, C)
        ...
        ...     print(g.to_dsl())
    """

    def __init__(self):
        """创建新的几何构造上下文。"""
        self._graph: Graph = Graph()
        self._point_counter: int = 0
        self._point_names: Dict[str, PointWrapper] = {}
        self._points: List[PointWrapper] = []
        self._segments: List[SegmentWrapper] = []
        self._wrappers: Dict[int, PointWrapper] = {}

    # ============================================================
    # 上下文管理器
    # ============================================================

    def __enter__(self) -> 'G':
        """进入上下文（with G() as g:）。"""
        return self

    def __exit__(self, *args) -> None:
        """退出上下文。"""
        pass

    # ============================================================
    # 内部辅助
    # ============================================================

    def _wrap_point(self, point: Point,
                    name: Optional[str] = None) -> PointWrapper:
        """将 core.Point 包装为 PointWrapper。

        如果该点的 core id 已被包装过，则复用已有的 PointWrapper；
        如果该点尚未注册到约束图，则先注册再包装。

        参数：
            point: 底层 core.Point 对象
            name: 点名称（可选，为 None 则自动命名）

        返回：
            PointWrapper: 包装后的点
        """
        # 复用已有包装器
        if point._id is not None and point._id in self._wrappers:
            pw = self._wrappers[point._id]
            if name is not None:
                pw.name = name
            return pw
        # 如果尚未加入图，先加入
        if point._id is None:
            point = self._graph.add_point(point.x, point.y)
        if name is None:
            name = _auto_name()
        pw = PointWrapper(point, self, name)
        self._wrappers[point._id] = pw
        self._points.append(pw)
        self._point_names[name] = pw
        self._point_counter += 1
        return pw

    # ============================================================
    # 点构造
    # ============================================================

    def point(self, x: Union[int, float, SymbolicCoord],
              y: Union[int, float, SymbolicCoord],
              name: Optional[str] = None) -> PointWrapper:
        """在图中创建一个点。

        参数：
            x: X 坐标
            y: Y 坐标
            name: 点名称（可选，默认自动命名 A, B, C, ...）

        返回：
            PointWrapper: 包装后的点对象
        """
        core_point = self._graph.add_point(_to_coord(x), _to_coord(y))
        if name is None:
            name = _auto_name()
        pw = PointWrapper(core_point, self, name)
        self._wrappers[core_point._id] = pw
        self._points.append(pw)
        self._point_names[name] = pw
        self._point_counter += 1
        return pw

    def P(self, x: Union[int, float, SymbolicCoord],
          y: Union[int, float, SymbolicCoord],
          name: Optional[str] = None) -> PointWrapper:
        """point() 的简写形式。

        参数：
            x: X 坐标
            y: Y 坐标
            name: 点名称（可选）

        返回：
            PointWrapper
        """
        return self.point(x, y, name)

    # ============================================================
    # 线段 / 直线构造
    # ============================================================

    def segment(self, a: PointWrapper, b: PointWrapper,
                name: Optional[str] = None) -> SegmentWrapper:
        """创建连接 a 和 b 的线段。

        参数：
            a: 起点
            b: 终点
            name: 线段名称（可选）

        返回：
            SegmentWrapper: 包装后的线段对象
        """
        seg = LineSegment(a._point, b._point)
        sw = SegmentWrapper(a, b, self, name, seg)
        self._segments.append(sw)
        return sw

    def line(self, a: PointWrapper, b: PointWrapper) -> LineWrapper:
        """创建通过 a 和 b 的无限直线。

        参数：
            a: 直线上一点
            b: 直线上另一点

        返回：
            LineWrapper
        """
        return LineWrapper(a, b, self)

    # ============================================================
    # 圆构造
    # ============================================================

    def circle(self, *,
               center: PointWrapper = None,
               radius: Union[SymbolicCoord, int, float] = None,
               through: PointWrapper = None) -> CircleWrapper:
        """创建圆。

        可通过以下两种方式指定：
        1. ``center`` + ``radius``：指定圆心和半径
        2. ``center`` + ``through``：指定圆心和圆周上一点（自动计算半径）

        参数：
            center: 圆心
            radius: 半径（与 through 互斥）
            through: 圆上一点（与 radius 互斥）

        返回：
            CircleWrapper

        异常：
            DSLError: 参数不完整或无效
        """
        if center is None:
            raise DSLError("创建圆需要指定圆心（center）")

        if through is not None:
            r = center.distance_to(through)
            if abs(r) < 1e-15:
                raise DSLError(f"圆心与 through 点重合，无法确定半径")
            return CircleWrapper(center, SymbolicCoord(r), self)

        if radius is None:
            raise DSLError("创建圆需要指定半径（radius 或 through）")

        return CircleWrapper(center, radius, self)

    # ============================================================
    # 三角形构造
    # ============================================================

    def triangle(self, a: PointWrapper, b: PointWrapper,
                 c: PointWrapper) -> TriangleWrapper:
        """创建三角形。

        参数：
            a, b, c: 三个顶点

        返回：
            TriangleWrapper

        异常：
            DSLError: 三点共线无法构成三角形
        """
        return TriangleWrapper(a, b, c, self)

    # ============================================================
    # 多边形构造
    # ============================================================

    def polygon(self, *vertices: PointWrapper) -> PolygonWrapper:
        """创建多边形。

        参数：
            *vertices: 顶点序列（至少 3 个）

        返回：
            PolygonWrapper
        """
        return PolygonWrapper(list(vertices), self)

    def square(self, a: PointWrapper,
               b: PointWrapper) -> PolygonWrapper:
        """从两个相邻顶点创建正方形。

        参数：
            a: 第一个顶点
            b: 第二个相邻顶点（与 a 相邻）

        返回：
            PolygonWrapper: 包含四个顶点的正方形

        说明：
            假定 a→b 为正方形的一条边，按逆时针方向生成另外两个顶点。
            若 A=(0,0), B=(2,0)，则生成正方形 A(0,0)-B(2,0)-C(2,2)-D(0,2)。
        """
        ax = _float(a.x)
        ay = _float(a.y)
        bx = _float(b.x)
        by = _float(b.y)

        # 边向量 AB
        abx = bx - ax
        aby = by - ay

        # 垂直向量（逆时针旋转 90 度）
        px = -aby
        py = abx

        # C = B + 垂直向量, D = A + 垂直向量
        cx = bx + px
        cy = by + py
        dx = ax + px
        dy = ay + py

        C = self.point(cx, cy)
        D = self.point(dx, dy)

        return PolygonWrapper([a, b, C, D], self)

    # ============================================================
    # 中点
    # ============================================================

    def midpoint(self, a: PointWrapper, b: PointWrapper,
                 name: Optional[str] = None) -> PointWrapper:
        """计算 a 和 b 的中点，并将其注册到图中。

        参数：
            a, b: 两个端点
            name: 中点名称（可选）

        返回：
            PointWrapper: 中点
        """
        mid = a._point.mid_point(b._point)
        core_pt = self._graph.add_point(mid.x, mid.y)
        return self._wrap_point(core_pt, name)

    # ============================================================
    # 三角形的特殊点（便捷方法）
    # ============================================================

    def circumcenter(self, a: PointWrapper, b: PointWrapper,
                     c: PointWrapper) -> PointWrapper:
        """计算三角形 ABC 的外心。

        参数：
            a, b, c: 三角形三顶点

        返回：
            PointWrapper: 外心
        """
        t = TriangleWrapper(a, b, c, self)
        return t.circumcenter()

    def centroid(self, a: PointWrapper, b: PointWrapper,
                 c: PointWrapper) -> PointWrapper:
        """计算三角形 ABC 的重心（三边中线的交点）。

        重心 = (A + B + C) / 3

        参数：
            a, b, c: 三角形三顶点

        返回：
            PointWrapper: 重心
        """
        cx = (_float(a.x) + _float(b.x) + _float(c.x)) / 3.0
        cy = (_float(a.y) + _float(b.y) + _float(c.y)) / 3.0
        return self.point(cx, cy)

    def orthocenter(self, a: PointWrapper, b: PointWrapper,
                    c: PointWrapper) -> PointWrapper:
        """计算三角形 ABC 的垂心。

        参数：
            a, b, c: 三角形三顶点

        返回：
            PointWrapper: 垂心
        """
        t = TriangleWrapper(a, b, c, self)
        return t.orthocenter()

    def incenter(self, a: PointWrapper, b: PointWrapper,
                 c: PointWrapper) -> PointWrapper:
        """计算三角形 ABC 的内心。

        参数：
            a, b, c: 三角形三顶点

        返回：
            PointWrapper: 内心
        """
        t = TriangleWrapper(a, b, c, self)
        return t.incenter()

    # ============================================================
    # 垂线 / 平行线
    # ============================================================

    def perpendicular(self, point: PointWrapper,
                      to_segment: SegmentWrapper) -> LineWrapper:
        """过指定点作已知线段的垂线。

        参数：
            point: 垂线通过的点
            to_segment: 垂线所垂直的线段

        返回：
            LineWrapper: 垂线
        """
        return to_segment.perpendicular_at(point)

    def parallel(self, point: PointWrapper,
                 to_segment: SegmentWrapper) -> LineWrapper:
        """过指定点作已知线段的平行线。

        参数：
            point: 平行线通过的点
            to_segment: 平行线所平行的线段

        返回：
            LineWrapper: 平行线
        """
        return to_segment.parallel_at(point)

    # ============================================================
    # 交点
    # ============================================================

    def intersect(self, obj1, obj2) -> PointWrapper:
        """计算两个几何对象的交点。

        支持以下组合：
            - LineWrapper x LineWrapper：两条直线的交点
            - LineWrapper x CircleWrapper：直线与圆的交点（返回第一个）
            - CircleWrapper x CircleWrapper：两圆的交点（返回第一个）

        参数：
            obj1, obj2: 两个几何对象

        返回：
            PointWrapper: 交点

        异常：
            DSLError: 无交点或类型组合不支持
        """
        # Line x Line
        if isinstance(obj1, LineWrapper) and isinstance(obj2, LineWrapper):
            return self._intersect_lines(obj1, obj2)

        # Line x Circle
        if isinstance(obj1, LineWrapper) and isinstance(obj2, CircleWrapper):
            pts = obj2.intersect_line(obj1)
            if not pts:
                raise DSLError("直线与圆无交点")
            return pts[0]

        if isinstance(obj1, CircleWrapper) and isinstance(obj2, LineWrapper):
            pts = obj1.intersect_line(obj2)
            if not pts:
                raise DSLError("直线与圆无交点")
            return pts[0]

        # Circle x Circle
        if isinstance(obj1, CircleWrapper) and isinstance(obj2, CircleWrapper):
            return self._intersect_circles(obj1, obj2)

        raise DSLError(
            f"不支持的求交类型："
            f"{type(obj1).__name__} x {type(obj2).__name__}"
        )

    def _intersect_lines(self, l1: LineWrapper,
                         l2: LineWrapper) -> PointWrapper:
        """计算两条直线的交点。

        使用行列式方法求解两条直线的参数方程。

        参数：
            l1, l2: 两条直线

        返回：
            PointWrapper: 唯一交点

        异常：
            DSLError: 两直线平行或重合
        """
        x1, y1 = _float(l1._p1.x), _float(l1._p1.y)
        x2, y2 = _float(l1._p2.x), _float(l1._p2.y)
        x3, y3 = _float(l2._p1.x), _float(l2._p1.y)
        x4, y4 = _float(l2._p2.x), _float(l2._p2.y)

        denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4)
        if abs(denom) < 1e-15:
            raise DSLError("两条直线平行或重合，无唯一交点")

        px = ((x1*y2 - y1*x2) * (x3 - x4) - (x1 - x2) * (x3*y4 - y3*x4)) / denom
        py = ((x1*y2 - y1*x2) * (y3 - y4) - (y1 - y2) * (x3*y4 - y3*x4)) / denom

        return self.point(px, py)

    def _intersect_circles(self, c1: CircleWrapper,
                           c2: CircleWrapper) -> PointWrapper:
        """计算两个圆的交点（返回第一个交点）。

        参数：
            c1, c2: 两个圆

        返回：
            PointWrapper: 第一个交点

        异常：
            DSLError: 两圆无交点或重合
        """
        x1, y1 = _float(c1._center.x), _float(c1._center.y)
        r1 = c1.radius
        x2, y2 = _float(c2._center.x), _float(c2._center.y)
        r2 = c2.radius

        dx = x2 - x1
        dy = y2 - y1
        d = math.sqrt(dx * dx + dy * dy)

        if d < 1e-15 and abs(r1 - r2) < 1e-15:
            raise DSLError("两圆重合，有无穷多个交点")
        if d > r1 + r2 + 1e-9:
            raise DSLError("两圆相离，无交点")
        if d < abs(r1 - r2) - 1e-9:
            raise DSLError("一个圆包含另一个圆，无交点")

        a = (r1*r1 - r2*r2 + d*d) / (2.0 * d)
        h_sq = r1*r1 - a*a
        h = math.sqrt(max(0.0, h_sq))

        px = x1 + a * dx / d
        py = y1 + a * dy / d

        # 第一个交点
        ix = px + h * (-dy) / d
        iy = py + h * dx / d

        return self.point(ix, iy)

    # ============================================================
    # 测量
    # ============================================================

    def distance(self, a: PointWrapper, b: PointWrapper) -> float:
        """计算两点之间的距离（浮点近似值）。

        参数：
            a, b: 两个点

        返回：
            float: 欧几里得距离
        """
        return a.distance_to(b)

    def angle(self, a: PointWrapper, b: PointWrapper,
              c: PointWrapper) -> float:
        """计算角 ABC 的度数。

        参数：
            a: 角的一边上的一点
            b: 角的顶点
            c: 角的另一边上的一点

        返回：
            float: 角度值（度，范围 [0, 180]）

        异常：
            DSLError: 边退化为点
        """
        bax = _float(a.x) - _float(b.x)
        bay = _float(a.y) - _float(b.y)
        bcx = _float(c.x) - _float(b.x)
        bcy = _float(c.y) - _float(b.y)

        dot = bax * bcx + bay * bcy
        mag_ba = math.sqrt(bax*bax + bay*bay)
        mag_bc = math.sqrt(bcx*bcx + bcy*bcy)

        if mag_ba < 1e-15 or mag_bc < 1e-15:
            raise DSLError("角度计算失败：边退化为点")

        cos_val = max(-1.0, min(1.0, dot / (mag_ba * mag_bc)))
        return math.degrees(math.acos(cos_val))

    # ============================================================
    # 约束
    # ============================================================

    def on(self, point: PointWrapper, obj) -> None:
        """添加关联约束：点位于几何对象上。

        参数：
            point: 被约束的点
            obj: 几何对象（目前支持 SegmentWrapper）

        异常：
            DSLError: 不支持的约束目标类型或约束失败
        """
        if isinstance(obj, SegmentWrapper):
            # 确保线段已注册到约束图
            if obj._seg is None or obj._seg._id is None:
                obj._seg = self._graph.add_line_segment(
                    obj._p1._point, obj._p2._point
                )
            try:
                self._graph.add_incidence(point.id, obj.id)
            except Lv00Error as e:
                raise DSLError(f"关联约束失败：{e}") from e
        else:
            raise DSLError(
                f"不支持的关联约束目标类型：{type(obj).__name__}"
            )

    def between(self, a: PointWrapper, b: PointWrapper,
                c: PointWrapper) -> None:
        """添加中间约束：b 位于 a 和 c 之间（三点共线且 b 在中间）。

        参数：
            a: 第一个端点
            b: 中间点（约束目标）
            c: 第二个端点

        异常：
            DSLError: 约束添加失败
        """
        try:
            self._graph.add_betweenness(a.id, b.id, c.id)
        except Lv00Error as e:
            raise DSLError(f"中间约束失败：{e}") from e

    # ============================================================
    # 构建 / 导出
    # ============================================================

    def build(self) -> List[Any]:
        """构建并返回所有创建的对象列表。

        返回：
            list: 包含所有 PointWrapper 和 SegmentWrapper 的列表
        """
        result: List[Any] = []
        result.extend(self._points)
        result.extend(self._segments)
        return result

    def to_dsl(self) -> str:
        """将所有构造导出为 Lv-00 DSL 文本格式。

        每行格式为 ``名称 = 构造表达式``。

        返回：
            str: 多行 DSL 文本
        """
        lines = []
        for pw in self._points:
            name = pw._name or '?'
            lines.append(f"{name} = {pw.to_dsl()}")
        for sw in self._segments:
            if sw._name:
                lines.append(f"{sw._name} = {sw.to_dsl()}")
            else:
                lines.append(sw.to_dsl())
        return '\n'.join(lines)

    # ============================================================
    # 属性
    # ============================================================

    @property
    def graph(self) -> Graph:
        """获取底层约束图。"""
        return self._graph

    @property
    def points(self) -> List[PointWrapper]:
        """获取所有已创建的点（副本）。"""
        return list(self._points)

    @property
    def segments(self) -> List[SegmentWrapper]:
        """获取所有已创建的线段（副本）。"""
        return list(self._segments)


# ============================================================
# 模块级便捷函数
# ============================================================


def P(x: Union[int, float], y: Union[int, float]) -> Point:
    """创建一个游离点（不在任何约束图中）。

    用于预先创建坐标点，稍后在 G 上下文中使用。

    参数：
        x: X 坐标
        y: Y 坐标

    返回：
        Point: 游离的 core.Point 对象
    """
    return Point(_to_coord(x), _to_coord(y))


def line(a: PointWrapper, b: PointWrapper) -> LineWrapper:
    """创建一个游离直线（借用 PointWrapper 所属的 G 上下文）。

    参数：
        a: 直线上一点
        b: 直线上另一点

    返回：
        LineWrapper
    """
    return a._ctx.line(a, b)


def circle(center: PointWrapper,
           radius: Union[SymbolicCoord, int, float]) -> CircleWrapper:
    """创建一个游离圆（借用 center 所属的 G 上下文）。

    参数：
        center: 圆心
        radius: 半径

    返回：
        CircleWrapper
    """
    return center._ctx.circle(center=center, radius=radius)


# ============================================================
# 模块导出
# ============================================================

__all__ = [
    'G',
    'PointWrapper',
    'SegmentWrapper',
    'LineWrapper',
    'CircleWrapper',
    'TriangleWrapper',
    'PolygonWrapper',
    'P',
    'line',
    'circle',
    'DSLError',
]