"""
Lv-00 DSL 包装器模块 - 几何对象包装类

提供几何对象的高级包装接口，支持链式调用、运算符重载和声明式几何构造。

包装类：
    - PointWrapper: 点包装器
    - SegmentWrapper: 线段包装器
    - LineWrapper: 无限直线包装器
    - CircleWrapper: 圆包装器
    - TriangleWrapper: 三角形包装器
    - PolygonWrapper: 多边形包装器

版本：3.2.0
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
    """DSL 构造异常。"""

    def __init__(self, message: str = "", error_code: int = -1) -> None:
        if not message:
            message = "几何 DSL 构造错误"
        super().__init__(message, error_code)

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

    def __init__(self, point: Point, context: 'G', name: Optional[str] = None) -> None:
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

    # ---- 运算符重载（借鉴 GAlgebra 的数学→代码操作符映射） ----

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

    def __add__(self, other: PointWrapper) -> 'PointWrapper':
        """A + B：两点的中点（借鉴 GAlgebra 向量加法语义）。

        参数：
            other: 另一个 PointWrapper

        返回：
            PointWrapper: 中点
        """
        if not isinstance(other, PointWrapper):
            return NotImplemented
        return self.midpoint(other)

    def __and__(self, other: PointWrapper) -> 'LineWrapper':
        """A & B：通过 A 和 B 的无限直线（借鉴位运算映射）。

        参数：
            other: 另一个 PointWrapper

        返回：
            LineWrapper: 通过两点的直线
        """
        if not isinstance(other, PointWrapper):
            return NotImplemented
        return self._ctx.line(self, other)

    def __or__(self, other) -> 'CircleWrapper':
        """A | B：以 A 为圆心、过 B 的圆；或 A | r（数值）为半径的圆。

        参数：
            other: PointWrapper 或 int/float

        返回：
            CircleWrapper: 以 A 为圆心的圆
        """
        if isinstance(other, PointWrapper):
            return self._ctx.circle(center=self, through=other)
        if isinstance(other, (int, float)):
            return self._ctx.circle(center=self, radius=other)
        return NotImplemented

    def __xor__(self, other: PointWrapper) -> 'PointWrapper':
        """A ^ B：A 关于 B 的对称点（反射，借鉴外积几何语义）。

        参数：
            other: 对称中心

        返回：
            PointWrapper: A 关于 B 的反射点
        """
        if not isinstance(other, PointWrapper):
            return NotImplemented
        mid = self.midpoint(other)
        dx = _float(self.x) - _float(other.x)
        dy = _float(self.y) - _float(other.y)
        return self._ctx.point(
            _float(other.x) - dx,
            _float(other.y) - dy,
        )

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
                 segment: Optional[LineSegment] = None) -> None:
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

    # ---- 运算符重载（借鉴 GAlgebra） ----

    def __invert__(self) -> 'LineWrapper':
        """~seg：线段的垂直平分线（借鉴 GAlgebra 反向操作符）。

        返回：
            LineWrapper: 通过线段中点并垂直于线段的直线
        """
        mid = self.midpoint()
        return self.perpendicular_at(mid)

    def __neg__(self) -> 'SegmentWrapper':
        """-seg：反向线段（交换端点）。

        返回：
            SegmentWrapper: 方向反转的线段
        """
        return self._ctx.segment(self._p2, self._p1)

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

    def __init__(self, p1: PointWrapper, p2: PointWrapper, context: 'G') -> None:
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
                 context: 'G') -> None:
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
                 context: 'G') -> None:
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

    def __init__(self, vertices: List[PointWrapper], context: 'G') -> None:
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



