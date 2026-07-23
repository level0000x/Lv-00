"""
Lv-00 DSL 上下文模块 - 几何构造上下文

提供 G 上下文类，支持 with G() as g: 模式管理局部几何构造作用域。

版本：3.3.0
作者：Lv-00 开发团队
"""

from __future__ import annotations

import math
from typing import Any, Dict, List, Optional, Tuple, Union

from .core import Graph, LineSegment, Point, SymbolicCoord, lvError
from .dsl_wrappers import (
    PointWrapper, SegmentWrapper, LineWrapper,
    CircleWrapper, TriangleWrapper, PolygonWrapper,
)

# 浮点数相等性比较的容差值。
_EPSILON = 1e-15

# ============================================================
# 内部辅助
# ============================================================

def _make_name_generator():
    """生成 A, B, C, ..., Z, AA, AB, ... 序列的命名器。"""
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

_auto_name = _make_name_generator()

def _to_coord(value: Union[SymbolicCoord, int, float]) -> SymbolicCoord:
    """将数值转为 SymbolicCoord。"""
    if isinstance(value, SymbolicCoord):
        return value
    return SymbolicCoord(value)

def _to_core_point(
    value: Union['PointWrapper', Point, Tuple[float, float], List[float]],
    graph: Graph
) -> Point:
    """将 PointWrapper / Point / tuple 转为 core.Point。"""
    # 使用 isinstance() 进行类型检查，替代基于类型名称的字符串比较。
    if isinstance(value, PointWrapper):
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
        >>> from lv.dsl import G
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
        """退出上下文。

        当前上下文管理器不持有需要显式释放的资源（如图形句柄等），
        因此退出时无需执行额外清理操作。保留此方法以满足上下文管理器协议。
        """
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
            if abs(r) < _EPSILON:
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
        if abs(denom) < _EPSILON:
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

        if d < _EPSILON and abs(r1 - r2) < _EPSILON:
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

        if mag_ba < _EPSILON or mag_bc < _EPSILON:
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
            except lvError as e:
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
        except lvError as e:
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
