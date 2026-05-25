"""
PyEuclid 风格高层 API 模块

汲取 PyEuclid 的设计哲学——"高层 API 设计，看怎么让用户用最简单语法
表达复杂几何关系"——本模块在 lv00 现有核心基础上封装了极致简洁的
面向对象几何接口。

设计原则：
    1. 超简洁构造语法：用户仅需 G/P/L/C 四个入口
    2. 运算符重载：+ 平移、- 向量、@ 关联、* 交点
    3. 流畅链式调用：G.constrain(A).incident_to(AB).build()
    4. 自动类型转换：tuple/list/int/float 无缝转换为 Point/SymbolicCoord
    5. 中文错误信息：所有异常提供中文描述
    6. 与现有 Graph 深度集成：每个几何对象持有内部 Graph 引用

用法示例：
    from lv00.py_euclid_style import G, P, L, C

    A, B, C = P(0, 0), P(3, 4), P(6, 0)
    AB = L(A, B)
    A @ AB                      # A 在 AB 上
    mid = A.midpoint(B)         # 中点
    tri = G.triangle(A, B, C).with_circumcircle().with_centroid()
    tri.solve()

版本：3.3.0
作者：Lv-00 开发团队
"""

from __future__ import annotations

import math
from typing import (TYPE_CHECKING, Any, Dict, List, Optional,
                    Sequence, Tuple, Union)

from .core import (Graph, LineSegment, Point, SymbolicCoord,
                   Lv00ConstraintError, Lv00Error)

# ============================================================
# 自定义异常
# ============================================================

class GeometryError(Lv00Error):
    """几何约束冲突异常。

    当几何构造违反数学约束（如三点共线无法构成三角形、
    平行线被要求相交等）时抛出。

    所有错误消息均为中文，便于国内用户理解。
    """

    def __init__(self, message: str = "", error_code: int = -1) -> None:
        if not message:
            message = "几何约束冲突"
        super().__init__(message, error_code)


# ============================================================
# 类型辅助
# ============================================================

_CoordLike = Union[SymbolicCoord, int, float]
_PointLike = Union['P', Tuple[float, float], List[float], Point]
_PointSeq = Sequence[_PointLike]


def _to_coord(value: Union[SymbolicCoord, int, float]) -> SymbolicCoord:
    """将数值转为 SymbolicCoord。"""
    if isinstance(value, SymbolicCoord):
        return value
    return SymbolicCoord(value)


def _to_point(value: _PointLike, graph: Graph) -> Point:
    """将任意点表示转为 core.Point。

    支持的类型：
        - P（本模块点包装器）
        - core.Point
        - tuple (x, y)
        - list [x, y]
    """
    if isinstance(value, P):
        return value._core
    if isinstance(value, Point):
        return value
    if isinstance(value, (tuple, list)):
        x, y = float(value[0]), float(value[1])
        return Point(_to_coord(x), _to_coord(y))
    raise TypeError(f"无法将 {type(value)} 转换为 Point")


# ============================================================
# P — 点包装器
# ============================================================

class P:
    """平面几何点。

    封装 core.Point，提供运算符重载和高层几何方法。

    属性：
        x (SymbolicCoord): X 坐标
        y (SymbolicCoord): Y 坐标
        _core (Point): 底层 core.Point 对象
        _graph (Graph): 关联的约束图

    运算符：
        + : 平移
        - : 向量差（两个 P 相减）或反向平移（P 减向量元组）
        @ : 关联（点到线的 incidence）

    示例：
        >>> A = P(0, 0)
        >>> B = P(3, 4)
        >>> M = A.midpoint(B)
        >>> d = A.distance_to(B)
    """

    __slots__ = ('_core', '_graph')

    def __init__(self, x: Union[_PointLike, _CoordLike],
                 y: Optional[Union[SymbolicCoord, int, float]] = None,
                 *,
                 graph: Optional[Graph] = None):
        """创建点。

        参数：
            x: X 坐标，或另一个点表示（tuple/list/P/Point），
               此时 y 忽略
            y: Y 坐标（当 x 为数值时必填）
            graph: 关联的约束图（可选，自动创建）
        """
        if graph is None:
            graph = Graph()
        self._graph = graph

        if isinstance(x, (P, Point, tuple, list)):
            pt = _to_point(x, graph)
            self._core = graph.add_point(pt.x, pt.y)
        elif y is not None:
            pt = Point(_to_coord(x), _to_coord(y))
            self._core = graph.add_point(pt.x, pt.y)
        else:
            raise GeometryError(
                f"创建 P 失败：参数 ({x!r}, {y!r}) 无效。"
                f"请使用 P(x, y)、P((x, y)) 或 P(another_point)。"
            )

    # ---- 属性 ----

    @property
    def x(self) -> SymbolicCoord:
        return self._core.x

    @property
    def y(self) -> SymbolicCoord:
        return self._core.y

    # ---- 运算符重载 ----

    def __add__(self, other: Union[Tuple[float, float], 'P', Point]) -> 'P':
        """平移：P + (dx, dy) 返回平移后的点；P + Q 返回点加向量。"""
        if isinstance(other, (P, Point)):
            dx = other.x - self.x
            dy = other.y - self.y
            return P(self.x + dx, self.y + dy, graph=self._graph)
        if isinstance(other, (tuple, list)):
            dx, dy = float(other[0]), float(other[1])
            return P(self.x + dx, self.y + dy, graph=self._graph)
        return NotImplemented

    def __sub__(self, other: Union['P', Point, Tuple[float, float]]) -> Union['P', Tuple[SymbolicCoord, SymbolicCoord]]:
        """P - Q 返回向量 (dx, dy)；P - (dx, dy) 返回平移后的点。"""
        if isinstance(other, (P, Point)):
            return (self.x - other.x, self.y - other.y)
        if isinstance(other, (tuple, list)):
            dx, dy = float(other[0]), float(other[1])
            return P(self.x - dx, self.y - dy, graph=self._graph)
        return NotImplemented

    def __matmul__(self, line: 'L') -> 'P':
        """关联约束：A @ AB 表示点 A 在线段 AB 上。"""
        if not isinstance(line, L):
            return NotImplemented
        try:
            self._graph.add_incidence(self._core._id, line._core._id)
        except Lv00ConstraintError as e:
            raise GeometryError(
                f"关联约束失败：点 ({self}) 无法放置在 "
                f"线段 ({line}) 上。{e}"
            ) from e
        return self

    def __neg__(self) -> 'P':
        """取反：-P 返回关于原点的对称点。"""
        return P(-self.x, -self.y, graph=self._graph)

    def __eq__(self, other: Any) -> bool:
        if isinstance(other, P):
            return self._core == other._core
        return False

    def __hash__(self) -> int:
        return hash((str(self.x), str(self.y)))

    def __repr__(self) -> str:
        return f"P({self.x}, {self.y})"

    # ---- 几何方法 ----

    def distance_to(self, other: _PointLike) -> SymbolicCoord:
        """计算到另一点的欧几里得距离。"""
        pt = _to_point(other, self._graph)
        return self._core.distance_to(pt)

    def midpoint(self, other: _PointLike) -> 'P':
        """计算到另一点的中点。"""
        pt = _to_point(other, self._graph)
        mid = self._core.mid_point(pt)
        return P(mid.x, mid.y, graph=self._graph)

    def between(self, p1: _PointLike, p2: _PointLike) -> 'P':
        """约束：当前点位于 p1 和 p2 之间（介子约束）。"""
        a = _to_point(p1, self._graph)
        b = _to_point(p2, self._graph)
        try:
            self._graph.add_betweenness(a._id, self._core._id, b._id)
        except Lv00ConstraintError as e:
            raise GeometryError(
                f"中间约束失败：({self}) 不在 ({a}) 和 ({b}) 之间。{e}"
            ) from e
        return self

    def project_onto(self, line: 'L') -> 'P':
        """投影到直线上，返回垂足点。"""
        if not isinstance(line, L):
            raise TypeError("project_onto 需要 L 类型的线段参数")
        seg = line._core
        vx, vy = seg.direction_vector()
        wx = self.x - seg.p1.x
        wy = self.y - seg.p1.y
        dot_vv = vx * vx + vy * vy
        if dot_vv.is_zero():
            raise GeometryError("投影失败：线段退化为一个点")
        dot_wv = wx * vx + wy * vy
        t = dot_wv / dot_vv
        proj_x = seg.p1.x + t * vx
        proj_y = seg.p1.y + t * vy
        return P(proj_x, proj_y, graph=self._graph)

    def translate(self, dx: _CoordLike, dy: _CoordLike) -> 'P':
        """平移点，返回沿 (dx, dy) 方向平移后的新点。"""
        return P(self.x + dx, self.y + dy, graph=self._graph)

    def is_collinear_with(self, p2: _PointLike, p3: _PointLike) -> bool:
        """判断三点是否共线。"""
        a = _to_point(p2, self._graph)
        b = _to_point(p3, self._graph)
        return self._core.is_collinear_with(a, b)

    def vector_to(self, other: _PointLike) -> Tuple[SymbolicCoord, SymbolicCoord]:
        """返回到另一点的向量 (dx, dy)。"""
        pt = _to_point(other, self._graph)
        return self._core.vector_to(pt)

    # ---- 导出方法 ----

    def to_dict(self) -> Dict[str, Any]:
        """导出为字典，包含类型和坐标信息。"""
        return {'type': 'Point', 'x': str(self.x), 'y': str(self.y)}

    def to_latex(self) -> str:
        """导出为 LaTeX 坐标表示。"""
        return f"({self.x}, {self.y})"

    def to_dsl(self) -> str:
        """导出为 Lv-00 DSL 表示。"""
        return f"point({self.x}, {self.y})"


# ============================================================
# L — 线段/直线包装器
# ============================================================

class L:
    """线段（直线）包装器。

    封装 core.LineSegment，提供高层几何查询和构造方法。

    属性：
        p1 (P): 起点
        p2 (P): 终点
        _core (LineSegment): 底层 core.LineSegment 对象
        _graph (Graph): 关联的约束图

    示例：
        >>> AB = L(A, B)
        >>> AB.length
        >>> AB.angle_with(BC)
        >>> AB.is_parallel_to(CD)
        >>> X = AB * CD  # 两条线段的交点
    """

    __slots__ = ('_core', '_graph', '_p1', '_p2')

    def __init__(self, p1: _PointLike, p2: _PointLike, *,
                 graph: Optional[Graph] = None):
        """创建线段。

        参数：
            p1: 起点（P/Point/tuple/list）
            p2: 终点（P/Point/tuple/list）
            graph: 约束图（自动共享 P 的图或新建）
        """
        if isinstance(p1, P):
            graph = graph or p1._graph
        elif isinstance(p2, P):
            graph = graph or p2._graph
        if graph is None:
            graph = Graph()
        self._graph = graph

        core_p1 = _to_point(p1, graph)
        core_p2 = _to_point(p2, graph)
        self._p1 = P(core_p1.x, core_p1.y, graph=graph)
        self._p2 = P(core_p2.x, core_p2.y, graph=graph)
        self._core = graph.add_line_segment(core_p1, core_p2)

    @property
    def p1(self) -> P:
        return self._p1

    @property
    def p2(self) -> P:
        return self._p2

    @property
    def length(self) -> SymbolicCoord:
        """线段长度。"""
        return self._core.length()

    def midpoint(self) -> P:
        """线段中点。"""
        m = self._core.midpoint()
        return P(m.x, m.y, graph=self._graph)

    def angle_with(self, other: 'L') -> float:
        """计算与另一线段的夹角（弧度，范围 [0, pi]）。"""
        v1 = self._core.direction_vector()
        v2 = other._core.direction_vector()
        dot = v1[0] * v2[0] + v1[1] * v2[1]
        mag1 = (v1[0] * v1[0] + v1[1] * v1[1]) ** SymbolicCoord.from_rational(1, 2)
        mag2 = (v2[0] * v2[0] + v2[1] * v2[1]) ** SymbolicCoord.from_rational(1, 2)
        if mag1.is_zero() or mag2.is_zero():
            raise GeometryError("计算角度失败：线段退化为一个点")
        cos_val = dot / (mag1 * mag2)
        cos_f = cos_val.to_double()
        cos_f = max(-1.0, min(1.0, cos_f))
        return math.acos(cos_f)

    def is_parallel_to(self, other: 'L') -> bool:
        """判断两条线段是否平行。"""
        return self._core.is_parallel_to(other._core)

    def is_perpendicular_to(self, other: 'L') -> bool:
        """判断两条线段是否垂直。"""
        return self._core.is_perpendicular_to(other._core)

    def contains_point(self, point: _PointLike) -> bool:
        """判断点是否在线段上。"""
        pt = _to_point(point, self._graph)
        return self._core.contains_point(pt)

    def perpendicular_through(self, point: _PointLike) -> 'L':
        """过指定点作当前线段的垂线。"""
        pt = _to_point(point, self._graph)
        seg = self._core
        dx, dy = seg.direction_vector()
        perp_x = -dy
        perp_y = dx
        p2_core = Point(pt.x + perp_x, pt.y + perp_y)
        return L(pt, p2_core, graph=self._graph)

    def parallel_through(self, point: _PointLike) -> 'L':
        """过指定点作平行线。"""
        pt = _to_point(point, self._graph)
        seg = self._core
        dx, dy = seg.direction_vector()
        p2_core = Point(pt.x + dx, pt.y + dy)
        return L(pt, p2_core, graph=self._graph)

    def __mul__(self, other: 'L') -> 'P':
        """交点：self * other 返回两条线段的交点。

        计算当前线段与另一条线段的交点，结果作为新的 P 对象返回。
        交点通过约束图求解器计算，支持符号化坐标。

        参数：
            other: 另一条线段（L 实例）

        返回：
            P: 两条线段的交点

        异常：
            GeometryError: 两条线段平行（无交点）时抛出

        示例：
            >>> X = AB * CD  # AB 和 CD 的交点
        """
        if not isinstance(other, L):
            return NotImplemented
        result_point = self._graph.add_point(
            SymbolicCoord.zero(), SymbolicCoord.zero()
        )
        try:
            self._graph.add_intersection(
                self._core._id, other._core._id, result_point._id
            )
        except Lv00ConstraintError as e:
            raise GeometryError(
                f"无法计算交点：两条线段可能平行。{e}"
            ) from e
        p = P.__new__(P)
        p._core = result_point
        p._graph = self._graph
        return p

    def __repr__(self) -> str:
        return f"L({self._p1}, {self._p2})"

    def __eq__(self, other: Any) -> bool:
        if isinstance(other, L):
            return (self._p1 == other._p1 and self._p2 == other._p2) or \
                   (self._p1 == other._p2 and self._p2 == other._p1)
        return False

    def __hash__(self) -> int:
        s = sorted([hash(self._p1), hash(self._p2)])
        return hash(tuple(s))

    # ---- 导出 ----

    def to_dict(self) -> Dict[str, Any]:
        return {
            'type': 'LineSegment',
            'p1': self._p1.to_dict(),
            'p2': self._p2.to_dict(),
            'length': str(self.length),
        }

    def to_latex(self) -> str:
        return (f"\\overline{{{self._p1.to_latex()}"
                f"\\,{self._p2.to_latex()}}}")

    def to_dsl(self) -> str:
        return f"segment({self._p1.to_dsl()}, {self._p2.to_dsl()})"

    def to_svg(self, indent: str = "") -> str:
        """生成 SVG <line> 元素。"""
        return (f'{indent}<line x1="{self._p1.x.to_double()}" '
                f'y1="{self._p1.y.to_double()}" '
                f'x2="{self._p2.x.to_double()}" '
                f'y2="{self._p2.y.to_double()}" '
                f'stroke="black" stroke-width="1"/>')


# ============================================================
# C — 圆包装器
# ============================================================

class C:
    """圆。

    表示以 center 为圆心、radius 为半径的圆。

    属性：
        center (P): 圆心
        radius (SymbolicCoord): 半径
        _graph (Graph): 关联的约束图

    示例：
        >>> O = P(0, 0)
        >>> c = C(O, 5)
        >>> c.area
        >>> c.circumference
        >>> c.contains_point(P(3, 4))
    """

    __slots__ = ('_center', '_radius', '_graph')

    def __init__(self, center: _PointLike,
                 radius: Union[SymbolicCoord, int, float],
                 *,
                 graph: Optional[Graph] = None):
        """创建圆。

        参数：
            center: 圆心
            radius: 半径（正数）
            graph: 约束图
        """
        r = _to_coord(radius)
        if r.is_zero() or r.is_negative():
            raise GeometryError(f"创建圆失败：半径必须为正数，当前值为 {r}")
        self._radius = r

        if isinstance(center, P):
            graph = graph or center._graph
        if graph is None:
            graph = Graph()
        self._graph = graph

        core_c = _to_point(center, graph)
        self._center = P(core_c.x, core_c.y, graph=graph)

    @property
    def center(self) -> P:
        return self._center

    @property
    def radius(self) -> SymbolicCoord:
        return self._radius

    @property
    def area(self) -> SymbolicCoord:
        """面积 = pi * r^2。"""
        return SymbolicCoord(math.pi) * self._radius * self._radius

    @property
    def circumference(self) -> SymbolicCoord:
        """周长 = 2 * pi * r。"""
        return SymbolicCoord(2 * math.pi) * self._radius

    def contains_point(self, point: _PointLike) -> bool:
        """点是否在圆内（含边界）。"""
        pt = _to_point(point, self._graph)
        d = self._center.distance_to(pt)
        return d <= self._radius

    def intersects_line(self, line: L) -> bool:
        """圆与直线是否相交。"""
        n1 = P(line._p1.x, line._p1.y, graph=self._graph)
        n2 = P(line._p2.x, line._p2.y, graph=self._graph)
        seg = LineSegment(n1._core, n2._core)
        vx, vy = seg.direction_vector()
        wx = self._center.x - seg.p1.x
        wy = self._center.y - seg.p1.y
        dot_vv = vx * vx + vy * vy
        if dot_vv.is_zero():
            return False
        dot_wv = wx * vx + wy * vy
        t = dot_wv / dot_vv
        t_f = max(0.0, min(1.0, t.to_double()))
        px = seg.p1.x.to_double() + t_f * vx.to_double()
        py = seg.p1.y.to_double() + t_f * vy.to_double()
        dx = px - self._center.x.to_double()
        dy = py - self._center.y.to_double()
        dist_sq = dx * dx + dy * dy
        return dist_sq <= self._radius.to_double() ** 2

    def tangent_at(self, point: _PointLike) -> L:
        """圆上一点的切线。

        参数：
            point: 圆上的点

        返回：
            L: 该点处的切线段
        """
        pt = _to_point(point, self._graph)
        dx = pt.x - self._center.x
        dy = pt.y - self._center.y
        tan_dx = -dy
        tan_dy = dx
        t2 = Point(pt.x + tan_dx, pt.y + tan_dy)
        return L(pt, t2, graph=self._graph)

    def __repr__(self) -> str:
        return f"C(center={self._center}, radius={self._radius})"

    # ---- 导出 ----

    def to_dict(self) -> Dict[str, Any]:
        return {
            'type': 'Circle',
            'center': self._center.to_dict(),
            'radius': str(self._radius),
            'area': str(self.area),
            'circumference': str(self.circumference),
        }

    def to_latex(self) -> str:
        return (f"\\odot\\!\\left({self._center.to_latex()},"
                f"{self._radius}\\right)")

    def to_dsl(self) -> str:
        return f"circle({self._center.to_dsl()}, {self._radius})"

    def to_svg(self, indent: str = "") -> str:
        """生成 SVG <circle> 元素。"""
        return (f'{indent}<circle cx="{self._center.x.to_double()}" '
                f'cy="{self._center.y.to_double()}" '
                f'r="{self._radius.to_double()}" '
                f'fill="none" stroke="black" stroke-width="1"/>')


# ============================================================
# Triangle — 三角形
# ============================================================

class Triangle:
    """三角形。

    由三个顶点定义的三角形，提供丰富的几何查询和构造方法。

    属性：
        A, B, C (P): 三个顶点
        _graph (Graph): 约束图

    示例：
        >>> t = Triangle(A, B, C)
        >>> t.area
        >>> G = t.centroid()
        >>> t.is_right()
    """

    __slots__ = ('_A', '_B', '_C', '_graph')

    def __init__(self, a: _PointLike, b: _PointLike, c: _PointLike, *,
                 graph: Optional[Graph] = None):
        """创建三角形。

        参数：
            a, b, c: 三个顶点
            graph: 约束图

        异常：
            GeometryError: 三点共线
        """
        for pt in (a, b, c):
            if isinstance(pt, P):
                graph = graph or pt._graph
                break
        if graph is None:
            graph = Graph()
        self._graph = graph

        self._A = P(_to_point(a, graph).x, _to_point(a, graph).y, graph=graph)
        self._B = P(_to_point(b, graph).x, _to_point(b, graph).y, graph=graph)
        self._C = P(_to_point(c, graph).x, _to_point(c, graph).y, graph=graph)

        if self._A._core.is_collinear_with(self._B._core, self._C._core):
            raise GeometryError(
                f"无法构造三角形：三点 ({self._A}, {self._B}, {self._C}) 共线"
            )

    @property
    def A(self) -> P:
        return self._A

    @property
    def B(self) -> P:
        return self._B

    @property
    def C(self) -> P:
        return self._C

    @property
    def area(self) -> SymbolicCoord:
        """三角形面积。"""
        det = ((self._B.x - self._A.x) * (self._C.y - self._A.y) -
               (self._B.y - self._A.y) * (self._C.x - self._A.x))
        return det.__abs__() / SymbolicCoord.from_rational(2)

    @property
    def perimeter(self) -> SymbolicCoord:
        """三角形周长。"""
        from .preset_func_blocks import create_distance
        a_len = create_distance(self._graph, self._B._core, self._C._core)
        b_len = create_distance(self._graph, self._C._core, self._A._core)
        c_len = create_distance(self._graph, self._A._core, self._B._core)
        return a_len + b_len + c_len

    def centroid(self) -> P:
        """重心。"""
        from .preset_func_blocks import create_centroid
        c = create_centroid(self._graph, self._A._core,
                            self._B._core, self._C._core)
        return P(c.x, c.y, graph=self._graph)

    def circumcenter(self) -> P:
        """外心。"""
        from .preset_func_blocks import create_circumcenter
        c = create_circumcenter(self._graph, self._A._core,
                                self._B._core, self._C._core)
        return P(c.x, c.y, graph=self._graph)

    def orthocenter(self) -> P:
        """垂心。"""
        from .preset_func_blocks import create_orthocenter
        c = create_orthocenter(self._graph, self._A._core,
                               self._B._core, self._C._core)
        return P(c.x, c.y, graph=self._graph)

    def incenter(self) -> P:
        """内心。"""
        from .preset_func_blocks import create_incenter
        c = create_incenter(self._graph, self._A._core,
                            self._B._core, self._C._core)
        return P(c.x, c.y, graph=self._graph)

    def is_equilateral(self) -> bool:
        """是否为等边三角形。"""
        ab = self._A.distance_to(self._B)
        bc = self._B.distance_to(self._C)
        ca = self._C.distance_to(self._A)
        return ab == bc == ca

    def is_right(self) -> bool:
        """是否为直角三角形（使用勾股定理，容差 1e-9）。"""
        sides = [
            self._A.distance_to(self._B),
            self._B.distance_to(self._C),
            self._C.distance_to(self._A),
        ]
        sides.sort(key=lambda s: s.to_double())
        a2 = sides[0].to_double() ** 2
        b2 = sides[1].to_double() ** 2
        c2 = sides[2].to_double() ** 2
        return abs(a2 + b2 - c2) < 1e-9

    def __repr__(self) -> str:
        return f"Triangle({self._A}, {self._B}, {self._C})"

    # ---- 导出 ----

    def to_dict(self) -> Dict[str, Any]:
        return {
            'type': 'Triangle',
            'vertices': [self._A.to_dict(), self._B.to_dict(),
                         self._C.to_dict()],
            'area': str(self.area),
            'perimeter': str(self.perimeter),
        }

    def to_latex(self) -> str:
        return (f"\\triangle {self._A.to_latex()}"
                f"{self._B.to_latex()}{self._C.to_latex()}")

    def to_dsl(self) -> str:
        return (f"triangle({self._A.to_dsl()}, "
                f"{self._B.to_dsl()}, {self._C.to_dsl()})")

    def to_svg(self, indent: str = "") -> str:
        """生成三角形 SVG 多边形。"""
        pts = [self._A, self._B, self._C]
        coords = " ".join(f"{p.x.to_double()},{p.y.to_double()}" for p in pts)
        return (f'{indent}<polygon points="{coords}" '
                f'fill="none" stroke="black" stroke-width="1"/>')


# ============================================================
# Polygon — 多边形
# ============================================================

class Polygon:
    """多边形。

    由顶点序列定义的多边形，提供几何查询方法。

    属性：
        vertices (List[P]): 顶点列表
        _graph (Graph): 约束图

    示例：
        >>> poly = Polygon(A, B, C, D)
        >>> poly.area
        >>> poly.is_convex()
    """

    __slots__ = ('_vertices', '_graph')

    def __init__(self, *vertices: _PointLike,
                 graph: Optional[Graph] = None):
        """创建多边形。

        参数：
            *vertices: 顶点序列（至少 3 个）
            graph: 约束图

        异常：
            GeometryError: 顶点数不足
        """
        if len(vertices) < 3:
            raise GeometryError(
                f"多边形至少需要 3 个顶点，当前仅 {len(vertices)} 个"
            )

        for pt in vertices:
            if isinstance(pt, P):
                graph = graph or pt._graph
                break
        if graph is None:
            graph = Graph()
        self._graph = graph

        self._vertices = [
            P(_to_point(v, graph).x, _to_point(v, graph).y, graph=graph)
            for v in vertices
        ]

    @property
    def vertices(self) -> List[P]:
        return list(self._vertices)

    @property
    def area(self) -> SymbolicCoord:
        """多边形面积（鞋带公式）。"""
        n = len(self._vertices)
        total = SymbolicCoord.zero()
        for i in range(n):
            xi = self._vertices[i].x
            yi = self._vertices[i].y
            xj = self._vertices[(i + 1) % n].x
            yj = self._vertices[(i + 1) % n].y
            total = total + xi * yj - xj * yi
        return total.__abs__() / SymbolicCoord.from_rational(2)

    @property
    def perimeter(self) -> SymbolicCoord:
        """多边形周长。"""
        total = SymbolicCoord.zero()
        n = len(self._vertices)
        for i in range(n):
            total = total + self._vertices[i].distance_to(
                self._vertices[(i + 1) % n]
            )
        return total

    def is_regular(self) -> bool:
        """是否为正多边形（所有边长相等且所有内角相等）。"""
        n = len(self._vertices)
        if n < 3:
            return False
        side = self._vertices[0].distance_to(self._vertices[1])
        for i in range(1, n):
            s = self._vertices[i].distance_to(
                self._vertices[(i + 1) % n]
            )
            if s != side:
                return False
        return True

    def is_convex(self) -> bool:
        """是否为凸多边形（使用叉积符号检查）。"""
        n = len(self._vertices)
        if n < 3:
            return False

        def _cross(o: P, a: P, b: P) -> float:
            return ((a.x - o.x) * (b.y - o.y) -
                    (b.x - o.x) * (a.y - o.y)).to_double()

        signs = []
        for i in range(n):
            c = _cross(
                self._vertices[i],
                self._vertices[(i + 1) % n],
                self._vertices[(i + 2) % n],
            )
            if abs(c) > 1e-12:
                signs.append(c > 0)

        if not signs:
            return True
        return all(s == signs[0] for s in signs)

    def triangulate(self) -> List[Triangle]:
        """耳切法三角剖分。返回三角形列表。

        警告：当前实现仅适用于凸多边形！
        本方法使用简单的"每次取前三个顶点"策略进行三角剖分，
        该策略仅对凸多边形能产生正确的三角剖分结果。

        对于非凸（凹）多边形，此方法会产生错误的三角形（可能跨越
        多边形边界），导致几何计算结果不正确。如需处理非凸多边形，
        应使用完整的耳切法（Ear Clipping）或 Delaunay 三角剖分算法。

        Returns:
            由 Triangle 对象组成的列表。
        """
        verts = list(self._vertices)
        triangles = []
        while len(verts) >= 3:
            tri = Triangle(verts[0], verts[1], verts[2],
                           graph=self._graph)
            triangles.append(tri)
            verts.pop(1)
        return triangles

    def __repr__(self) -> str:
        v_str = ", ".join(str(v) for v in self._vertices)
        return f"Polygon({v_str})"

    # ---- 导出 ----

    def to_dict(self) -> Dict[str, Any]:
        return {
            'type': 'Polygon',
            'vertex_count': len(self._vertices),
            'vertices': [v.to_dict() for v in self._vertices],
            'area': str(self.area),
            'perimeter': str(self.perimeter),
        }

    def to_latex(self) -> str:
        coords = ",\\,".join(v.to_latex() for v in self._vertices)
        return f"\\text{{Polygon}}({coords})"

    def to_dsl(self) -> str:
        coords = ", ".join(v.to_dsl() for v in self._vertices)
        return f"polygon({coords})"

    def to_svg(self, indent: str = "") -> str:
        """生成多边形 SVG <polygon> 元素。"""
        coords = " ".join(
            f"{v.x.to_double()},{v.y.to_double()}"
            for v in self._vertices
        )
        return (f'{indent}<polygon points="{coords}" '
                f'fill="none" stroke="black" stroke-width="1"/>')


# ============================================================
# ConstraintBuilder — 流畅约束构建器
# ============================================================

class _ConstraintBuilder:
    """流畅约束构建器（内部类）。

    通过链式调用逐步添加几何约束，最后调用 .build() 执行。

    用法：
        G.constrain(A).incident_to(AB).and_constrain(B).between(A, C).build()
    """

    def __init__(self, graph: Graph):
        self._graph = graph
        self._pending: List[Tuple[str, Tuple[Any, ...]]] = []
        self._current_target: Optional[P] = None

    def incident_to(self, line: L) -> '_ConstraintBuilder':
        """约束当前对象关联到线段。"""
        if self._current_target is None:
            raise GeometryError("没有指定约束目标点")
        self._pending.append(('incidence',
                             (self._current_target, line)))
        return self

    def between(self, p1: _PointLike, p2: _PointLike) -> '_ConstraintBuilder':
        """约束当前对象在 p1 和 p2 之间。"""
        if self._current_target is None:
            raise GeometryError("没有指定约束目标点")
        self._pending.append(('betweenness',
                             (self._current_target, p1, p2)))
        return self

    def and_constrain(self, target: P) -> '_ConstraintBuilder':
        """切换到下一个约束目标。"""
        self._current_target = target
        return self

    def build(self) -> Graph:
        """执行所有待处理的约束。

        返回：
            Graph: 添加了约束后的图
        """
        for kind, args in self._pending:
            if kind == 'incidence':
                target, line = args
                target @ line
            elif kind == 'betweenness':
                target, p1, p2 = args
                target.between(p1, p2)
        self._pending.clear()
        return self._graph


# ============================================================
# G — 全局几何命名空间
# ============================================================

class G:
    """全局几何构造命名空间。

    提供类方法用于创建各种几何对象和构造。

    所有方法均为类方法，无需实例化即可调用。

    构造方法：
        G.point(x, y)        -> P
        G.segment(p1, p2)    -> L
        G.circle(c, r)       -> C
        G.triangle(a, b, c)  -> Triangle
        G.polygon(*vs)       -> Polygon
        G.regular_polygon(center, vertex, n) -> Polygon

    约束构建：
        G.constrain(target)  -> _ConstraintBuilder

    求解：
        G.solve(graph)       -> None

    示例：
        >>> A = G.point(0, 0)
        >>> B = G.point(3, 4)
        >>> AB = G.segment(A, B)
        >>> tri = G.triangle(A, B, G.point(6, 0))
    """

    @staticmethod
    def point(x: _CoordLike, y: _CoordLike) -> P:
        """创建点。"""
        return P(x, y)

    @staticmethod
    def segment(p1: _PointLike, p2: _PointLike) -> L:
        """创建线段。"""
        return L(p1, p2)

    @staticmethod
    def circle(center: _PointLike,
               radius: Union[SymbolicCoord, int, float]) -> C:
        """创建圆。"""
        return C(center, radius)

    @staticmethod
    def triangle(a: _PointLike, b: _PointLike, c: _PointLike) -> Triangle:
        """创建三角形。"""
        return Triangle(a, b, c)

    @staticmethod
    def polygon(*vertices: _PointLike) -> Polygon:
        """创建多边形。"""
        return Polygon(*vertices)

    @staticmethod
    def regular_polygon(center: _PointLike,
                        vertex: _PointLike,
                        n: int) -> Polygon:
        """创建正 n 边形。

        参数：
            center: 中心点
            vertex: 一个顶点（决定半径和旋转角）
            n: 边数（>= 3）

        返回：
            Polygon: 正多边形
        """
        if n < 3:
            raise GeometryError(f"正多边形至少需要 3 条边，当前 n={n}")
        graph = None
        if isinstance(center, P):
            graph = center._graph
        if graph is None:
            graph = Graph()
        c = _to_point(center, graph)
        v = _to_point(vertex, graph)
        radius = c.distance_to(v)
        angle_start = math.atan2(
            float(v.y.to_double()) - float(c.y.to_double()),
            float(v.x.to_double()) - float(c.x.to_double()),
        )
        verts = []
        for k in range(n):
            angle = angle_start + 2 * math.pi * k / n
            px = c.x.to_double() + radius.to_double() * math.cos(angle)
            py = c.y.to_double() + radius.to_double() * math.sin(angle)
            verts.append(P(px, py, graph=graph))
        return Polygon(*verts, graph=graph)

    @staticmethod
    def constrain(target: P) -> _ConstraintBuilder:
        """开始流畅约束构建。

        参数：
            target: 要约束的目标点

        返回：
            _ConstraintBuilder: 约束构建器

        用法：
            G.constrain(A).incident_to(AB).build()
        """
        builder = _ConstraintBuilder(target._graph)
        builder._current_target = target
        return builder

    @staticmethod
    def solve(graph: Graph) -> None:
        """求解约束图。

        对图进行规范化处理。

        参数：
            graph: 约束图
        """
        graph.normalize()

    @staticmethod
    def svg(*objects: Any,
            width: int = 800, height: int = 600,
            viewbox: Optional[Tuple[float, float, float, float]] = None
            ) -> str:
        """将所有几何对象导出为完整 SVG 文档。

        参数：
            *objects: P/L/C/Triangle/Polygon 对象
            width, height: SVG 尺寸
            viewbox: 视口 (x, y, w, h)，自动计算

        返回：
            str: 完整 SVG XML 字符串
        """
        svg_elems = []
        for obj in objects:
            if hasattr(obj, 'to_svg'):
                svg_elems.append(obj.to_svg(indent="  "))

        if viewbox is None:
            all_pts: List[P] = []
            for obj in objects:
                if isinstance(obj, P):
                    all_pts.append(obj)
                elif isinstance(obj, L):
                    all_pts.extend([obj.p1, obj.p2])
                elif isinstance(obj, C):
                    all_pts.append(obj.center)
                elif isinstance(obj, Triangle):
                    all_pts.extend([obj.A, obj.B, obj.C])
                elif isinstance(obj, Polygon):
                    all_pts.extend(obj.vertices)
            if all_pts:
                xs = [p.x.to_double() for p in all_pts]
                ys = [p.y.to_double() for p in all_pts]
                margin = 20.0
                vx = min(xs) - margin
                vy = min(ys) - margin
                vw = max(xs) - min(xs) + 2 * margin
                vh = max(ys) - min(ys) + 2 * margin
                viewbox = (vx, vy, vw, vh)
            else:
                viewbox = (0, 0, float(width), float(height))

        lines = [
            f'<svg xmlns="http://www.w3.org/2000/svg" '
            f'width="{width}" height="{height}" '
            f'viewBox="{viewbox[0]} {viewbox[1]} {viewbox[2]} {viewbox[3]}">',
        ]
        lines.extend(svg_elems)
        lines.append('</svg>')
        return '\n'.join(lines)


# ============================================================
# 模块便捷函数
# ============================================================

def solve(*objects: Union[P, L, C, Triangle, Polygon]) -> Graph:
    """求解一组几何对象构成的约束图。

    参数：
        *objects: 几何对象

    返回：
        Graph: 规范化后的约束图

    示例：
        >>> A = P(0, 0); B = P(3, 4)
        >>> AB = L(A, B)
        >>> graph = solve(A, B, AB)
    """
    graph = None
    for obj in objects:
        if hasattr(obj, '_graph'):
            graph = obj._graph
            break
    if graph is None:
        graph = Graph()
    graph.normalize()
    return graph


def svg(*objects: Any, **kwargs: Any) -> str:
    """便捷 SVG 导出函数。等同于 G.svg(...)。"""
    return G.svg(*objects, **kwargs)


# ============================================================
# 模块导出
# ============================================================

__all__ = [
    # 主入口
    'G', 'P', 'L', 'C',
    # 组合类型
    'Triangle', 'Polygon',
    # 异常
    'GeometryError',
    # 便捷函数
    'solve', 'svg',
]
