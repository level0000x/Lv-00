"""
Lv-00 DSL 代数模式模块 — 借鉴 CadQuery + build123d + GAlgebra 设计

提供三个核心增强：

1. AlgebraMode (借鉴 build123d) — 纯无状态代数构造模式
   所有操作返回新对象，无隐式状态，"构造即运算"

2. Workplane (借鉴 CadQuery) — 工作平面抽象
   在指定平面上进行 2D 几何构造，selector 语法选择平面/边/点

3. Transform & 操作符链 (借鉴 GAlgebra + build123d)
   用乘法链表达几何变换：Plane.XY * Pos(5,0) * Rot(45) * Circle(2)

版本：3.3.0
作者：Lv-00 开发团队
参考：
  - CadQuery (https://github.com/CadQuery/cadquery) — Fluent API / Selector / Workplane
  - build123d (https://github.com/gumyr/build123d) — Algebra Mode / 操作符变换
  - GAlgebra (https://github.com/pygae/galgebra) — 操作符重载数学映射
"""

from __future__ import annotations

import math
from typing import Any, Dict, List, Optional, Tuple, Union, Callable
from dataclasses import dataclass, field

from .core import Graph, LineSegment, Point, SymbolicCoord, Lv00Error
from .dsl_wrappers import (
    PointWrapper, SegmentWrapper, LineWrapper,
    CircleWrapper, TriangleWrapper, PolygonWrapper, DSLError,
)
from .dsl_context import G, _float, _to_coord


# ============================================================
# Transform — 借鉴 build123d 的 Pos / Rot 变换系统
# ============================================================

class Transform:
    """2D 仿射变换（平移 + 旋转 + 缩放）。

    借鉴 build123d 的操作符变换语法：
        Plane.XY * Pos(5, 0) * Rot(45) * Rectangle(2, 2)

    使用乘法链表达变换序列，支持组合和逆变换。

    属性：
        tx, ty (float): 平移量
        angle_deg (float): 旋转角度（度）
        scale_x, scale_y (float): 缩放因子
    """

    __slots__ = ('tx', 'ty', 'angle_deg', 'scale_x', 'scale_y')

    def __init__(self, tx: float = 0.0, ty: float = 0.0,
                 angle_deg: float = 0.0,
                 scale_x: float = 1.0, scale_y: float = 1.0) -> None:
        self.tx = tx
        self.ty = ty
        self.angle_deg = angle_deg
        self.scale_x = scale_x
        self.scale_y = scale_y

    # ---- 操作符链：Transform * Geometry → Transformed Geometry ----

    def __mul__(self, other):
        """Transform * PointWrapper → 变换后的点。

        变换顺序：缩放 → 旋转 → 平移
        借鉴 build123d：Pos(X=5) * Rectangle(1,1)
        """
        if isinstance(other, PointWrapper):
            return self._transform_point(other)
        if isinstance(other, CircleWrapper):
            return self._transform_circle(other)
        if isinstance(other, SegmentWrapper):
            return self._transform_segment(other)
        if isinstance(other, TriangleWrapper):
            return self._transform_triangle(other)
        if isinstance(other, Transform):
            return self._compose(other)
        return NotImplemented

    def __rmul__(self, other):
        """Plane * Transform → 带变换的基准平面。"""
        if isinstance(other, Plane):
            return TransformedPlane(other, self)
        return NotImplemented

    def _transform_point(self, pt: PointWrapper) -> PointWrapper:
        """对点应用变换。"""
        x = _float(pt.x)
        y = _float(pt.y)

        # 缩放
        x *= self.scale_x
        y *= self.scale_y

        # 旋转
        if abs(self.angle_deg) > 1e-15:
            rad = math.radians(self.angle_deg)
            cos_a = math.cos(rad)
            sin_a = math.sin(rad)
            x, y = x * cos_a - y * sin_a, x * sin_a + y * cos_a

        # 平移
        x += self.tx
        y += self.ty

        return pt._ctx.point(x, y)

    def _transform_segment(self, seg: SegmentWrapper) -> SegmentWrapper:
        """对线段应用变换。"""
        p1 = self._transform_point(seg.p1)
        p2 = self._transform_point(seg.p2)
        return seg._ctx.segment(p1, p2)

    def _transform_circle(self, circ: CircleWrapper) -> CircleWrapper:
        """对圆应用变换。"""
        center = self._transform_point(circ.center)
        r = circ.radius * max(abs(self.scale_x), abs(self.scale_y))
        return center._ctx.circle(center=center, radius=r)

    def _transform_triangle(self, tri: TriangleWrapper) -> TriangleWrapper:
        """对三角形应用变换。"""
        a = self._transform_point(tri.A)
        b = self._transform_point(tri.B)
        c = self._transform_point(tri.C)
        return tri._ctx.triangle(a, b, c)

    def _compose(self, other: Transform) -> Transform:
        """组合两个变换：self * other = 先 other 再 self。

        Transform 乘法链：Pos(5,0) * Rot(45) = 先旋转再平移
        """
        # 检测旋转+缩放混合组合（数学上不精确）
        if (self.angle_deg != 0 and (self.scale_x != 1.0 or self.scale_y != 1.0)) or \
           (other.angle_deg != 0 and (other.scale_x != 1.0 or other.scale_y != 1.0)):
            import warnings
            warnings.warn(
                "Transform._compose: 旋转与缩放混合组合的结果可能不精确。"
                "建议使用纯旋转或纯缩放的变换链。",
                RuntimeWarning, stacklevel=2
            )
        return Transform(
            tx=self.tx + other.tx,
            ty=self.ty + other.ty,
            angle_deg=self.angle_deg + other.angle_deg,
            scale_x=self.scale_x * other.scale_x,
            scale_y=self.scale_y * other.scale_y,
        )

    def __repr__(self) -> str:
        parts = []
        if self.tx != 0 or self.ty != 0:
            parts.append(f"Pos({self.tx:.2f}, {self.ty:.2f})")
        if self.angle_deg != 0:
            parts.append(f"Rot({self.angle_deg:.1f}°)")
        if self.scale_x != 1.0 or self.scale_y != 1.0:
            parts.append(f"Scale({self.scale_x:.2f}, {self.scale_y:.2f})")
        return " * ".join(parts) if parts else "Identity"


# 便捷构造器
def Pos(x: float = 0.0, y: float = 0.0) -> Transform:
    """创建平移变换。借鉴 build123d 的 Pos(X=5)。"""
    return Transform(tx=x, ty=y)


def Rot(angle_deg: float) -> Transform:
    """创建旋转变换（度）。"""
    return Transform(angle_deg=angle_deg)


def Scale(sx: float = 1.0, sy: float | None = None) -> Transform:
    """创建缩放变换。"""
    if sy is None:
        sy = sx
    return Transform(scale_x=sx, scale_y=sy)


# ============================================================
# Plane — 借鉴 CadQuery 的 Workplane 工作平面抽象
# ============================================================

class Plane:
    """基准平面定义。

    借鉴 CadQuery 的 Workplane 概念——将 2D 工作平面作为一等对象。
    结合 build123d 的 Plane.XY / Plane.XZ 预定义。

    预定义平面：
        Plane.XY  — 标准 XY 平面（原点，X轴向右，Y轴向上）
        Plane.YZ  — YZ 平面
        Plane.XZ  — XZ 平面

    使用示例：
        >>> Plane.XY * Pos(5, 0) * Rot(30) * Circle(10)
        >>> plane = Plane.named("front")
    """

    # 预定义平面常量
    XY: Plane = None   # 在 __init_subclass__ 之后设置
    YZ: Plane = None
    XZ: Plane = None

    __slots__ = ('origin', 'u_dir', 'v_dir', 'name')

    def __init__(self, origin: Tuple[float, float] = (0, 0),
                 u_dir: Tuple[float, float] = (1, 0),
                 v_dir: Tuple[float, float] = (0, 1),
                 name: str = "") -> None:
        self.origin = origin
        self.u_dir = u_dir    # U 轴方向（对应 X 轴）
        self.v_dir = v_dir    # V 轴方向（对应 Y 轴）
        self.name = name

    def __mul__(self, other):
        """Plane * Transform → TransformedPlane。

        借鉴 build123d：Plane.XZ * Pos(X=5) * Rectangle(1,1)
        """
        if isinstance(other, Transform):
            return TransformedPlane(self, other)
        return NotImplemented

    def __repr__(self) -> str:
        if self.name:
            return f"Plane.{self.name}"
        return f"Plane(origin={self.origin})"

    @classmethod
    def named(cls, name: str) -> Plane:
        """创建命名平面。"""
        return cls(name=name)

    def workplane(self, ctx: G) -> Workplane:
        """在此平面上创建工作平面。

        参数：
            ctx: 几何构造上下文

        返回：
            Workplane: 工作平面实例
        """
        return Workplane(self, ctx)


# 初始化预定义平面
# YZ 平面：Y 轴沿 y 方向，Z 轴映射到 x 方向（2D 投影）
# XZ 平面：X 轴沿 x 方向，Z 轴映射到 y 方向（2D 投影）
Plane.XY = Plane(origin=(0, 0), u_dir=(1, 0), v_dir=(0, 1), name="XY")
Plane.YZ = Plane(origin=(0, 0), u_dir=(0, 1), v_dir=(1, 0), name="YZ")
Plane.XZ = Plane(origin=(0, 0), u_dir=(1, 0), v_dir=(0, 1), name="XZ")


# ============================================================
# TransformedPlane — 带变换的平面
# ============================================================

class TransformedPlane:
    """带变换链的平面。

    内部的 transform 累积了从基准平面到此平面的所有变换。
    """

    __slots__ = ('plane', 'transform')

    def __init__(self, plane: Plane, transform: Transform) -> None:
        self.plane = plane
        self.transform = transform

    def __mul__(self, other):
        """TransformedPlane * Transform → TransformedPlane（追加变换）。

        借鉴 build123d 变换链：
            Plane.XY * Pos(5,0) * Rot(45) * Rectangle(1,1)
        """
        if isinstance(other, Transform):
            return TransformedPlane(self.plane, self.transform * other)
        if isinstance(other, Workplane):
            # 应用到已有工作平面
            return Workplane(self.plane, other._ctx, self.transform)
        return NotImplemented

    def workplane(self, ctx: G) -> Workplane:
        """使用此变换后的平面创建工作平面。"""
        return Workplane(self.plane, ctx, self.transform)

    def __repr__(self) -> str:
        return f"{self.plane} * {self.transform}"


# ============================================================
# Workplane — 借鉴 CadQuery 的工作平面（Fluent API）
# ============================================================

class Workplane:
    """2D 工作平面 — CadQuery 风格的几何构造。

    借鉴 CadQuery Fluent API：
        >>> wp = Plane.XY.workplane(g)
        >>> wp.circle(5).rect(4, 3).extrude(10)

    借鉴 CadQuery Selector DSL：
        >>> wp.faces(">Z").workplane().circle(2)

    所有方法返回自身以实现链式调用（fluent interface）。

    属性：
        plane (Plane): 基准平面
        objects (List): 已构造的对象列表
    """

    __slots__ = ('_ctx', '_plane', '_transform', '_objects', '_pending_points', '_selected')

    def __init__(self, plane: Plane, ctx: G,
                 transform: Transform = None) -> None:
        self._ctx = ctx
        self._plane = plane
        self._transform = transform if transform is not None else Transform()
        self._objects: List[Any] = []
        self._pending_points: List[PointWrapper] = []
        self._selected: List[Any] = []

    # ---- 点构造 ----

    def point(self, x: float, y: float,
              name: str = None) -> Workplane:
        """在（变换后的）工作平面上创建一个点。

        链式调用：wp.point(0, 0).point(10, 0).line_to()
        """
        pt = self._ctx.point(x, y, name)
        # 应用当前变换
        if self._transform.tx != 0 or self._transform.ty != 0 or self._transform.angle_deg != 0:
            pt = self._transform._transform_point(pt)
        self._pending_points.append(pt)
        self._objects.append(pt)
        return self

    # ---- 线段构造 ----

    def line_to(self, x: float = None, y: float = None,
                point: PointWrapper = None) -> Workplane:
        """从最后一个点到指定位置创建线段。

        CadQuery 风格：wp.point(0,0).line_to(10,0).line_to(10,5)
        """
        if point is not None:
            target = point
        elif x is not None and y is not None:
            target = self._ctx.point(x, y)
            if self._transform.tx != 0 or self._transform.ty != 0:
                target = self._transform._transform_point(target)
        else:
            raise DSLError("line_to 需要 (x, y) 或 point 参数")

        if not self._pending_points:
            raise DSLError("line_to 需要先有一个起点（用 point() 创建）")

        start = self._pending_points[-1]
        seg = self._ctx.segment(start, target)
        self._objects.append(seg)
        self._pending_points.append(target)
        return self

    def h_line(self, length: float) -> Workplane:
        """从最后一个点向右画水平线段。

        参数：
            length: 线段长度
        """
        if not self._pending_points:
            raise DSLError("h_line 需要先有一个起点")
        start = self._pending_points[-1]
        x = _float(start.x) + length
        y = _float(start.y)
        return self.line_to(x, y)

    def v_line(self, length: float) -> Workplane:
        """从最后一个点向上画竖直线段。"""
        if not self._pending_points:
            raise DSLError("v_line 需要先有一个起点")
        start = self._pending_points[-1]
        x = _float(start.x)
        y = _float(start.y) + length
        return self.line_to(x, y)

    def close(self) -> Workplane:
        """闭合到第一个点（形成封闭多边形）。"""
        if len(self._pending_points) < 3:
            raise DSLError("至少需要 3 个点才能闭合")
        first = self._pending_points[0]
        return self.line_to(point=first)

    # ---- 圆构造 ----

    def circle(self, radius: float) -> Workplane:
        """在工作平面当前位置创建圆。

        CadQuery 风格：wp.circle(5)
        """
        if not self._pending_points:
            # 默认以原点为圆心
            center = self._ctx.point(0, 0)
            if self._transform.tx != 0 or self._transform.ty != 0:
                center = self._transform._transform_point(center)
        else:
            center = self._pending_points[-1]

        r = radius
        if self._transform.scale_x != 0:
            # 缩放因子取绝对值：圆的半径不能为负数，
            # 负数缩放在几何上无意义，此处自动修正为正数
            r *= abs(self._transform.scale_x)

        circ = self._ctx.circle(center=center, radius=r)
        self._objects.append(circ)
        return self

    # ---- 矩形 ----

    def rect(self, width: float, height: float,
             centered: bool = True) -> Workplane:
        """在工作平面上创建矩形。

        参数：
            width: 宽度
            height: 高度
            centered: 是否以原点为中心（默认 True）

        CadQuery 风格：wp.rect(4, 3)
        """
        if centered:
            x0, y0 = -width / 2, -height / 2
            x1, y1 = width / 2, height / 2
        else:
            x0, y0 = 0, 0
            x1, y1 = width, height

        self.point(x0, y0)
        self.line_to(x1, y0)
        self.line_to(x1, y1)
        self.line_to(x0, y1)
        self.close()
        return self

    # ---- 正多边形 ----

    def polygon(self, n: int, radius: float,
                start_angle: float = 0.0) -> Workplane:
        """创建正 n 边形。

        参数：
            n: 边数（≥3）
            radius: 外接圆半径
            start_angle: 起始角度（度，默认 0 = 右侧）
        """
        if n < 3:
            raise DSLError(f"多边形至少需要 3 条边，当前为 {n}")
        for i in range(n):
            angle = math.radians(start_angle + i * 360.0 / n)
            x = radius * math.cos(angle)
            y = radius * math.sin(angle)
            self.point(x, y)
        self.close()
        return self

    # ---- 选择器（CadQuery Selector DSL） ----

    def faces(self, selector: str = "") -> Workplane:
        """选择面（CadQuery 风格 selector）。

        借鉴 CadQuery：box.faces(">Z").workplane()

        从已构造的对象中筛选出面状几何体（多边形、三角形、圆），
        将筛选结果存入 _selected 列表，并返回 self 以支持链式调用。

        参数：
            selector: 选择器字符串，支持以下模式：
                - "" (空): 选择所有面
                - "polygon": 仅选择多边形
                - "triangle": 仅选择三角形
                - "circle": 仅选择圆
                - ">X"/">Y"/">Z": 选择质心在指定正半轴的面
                - "<X"/"<Y"/"<Z": 选择质心在指定负半轴的面

        返回：
            Workplane: 自身（链式调用）
        """
        face_types = (PolygonWrapper, TriangleWrapper, CircleWrapper)
        candidates = [obj for obj in self._objects if isinstance(obj, face_types)]

        if selector:
            sel = selector.strip()
            # 按类型过滤
            if sel == "polygon":
                candidates = [o for o in candidates if isinstance(o, PolygonWrapper)]
            elif sel == "triangle":
                candidates = [o for o in candidates if isinstance(o, TriangleWrapper)]
            elif sel == "circle":
                candidates = [o for o in candidates if isinstance(o, CircleWrapper)]
            # 按方向过滤（基于质心位置）
            elif sel.startswith(">") or sel.startswith("<"):
                axis_char = sel[1].upper() if len(sel) > 1 else "Y"
                positive = sel.startswith(">")
                filtered = []
                for obj in candidates:
                    cx, cy = self._face_centroid(obj)
                    coord = cx if axis_char == "X" else cy
                    if (positive and coord > 0) or (not positive and coord < 0):
                        filtered.append(obj)
                candidates = filtered

        # 将筛选结果存入 _selected 供后续链式操作使用
        self._selected = candidates
        return self

    def _face_centroid(self, face) -> Tuple[float, float]:
        """计算面的质心坐标（近似浮点值）。

        参数：
            face: 面状对象（PolygonWrapper, TriangleWrapper, CircleWrapper）

        返回：
            (cx, cy): 质心的 x, y 坐标
        """
        if isinstance(face, CircleWrapper):
            return _float(face.center.x), _float(face.center.y)
        elif isinstance(face, TriangleWrapper):
            cx = (_float(face.A.x) + _float(face.B.x) + _float(face.C.x)) / 3.0
            cy = (_float(face.A.y) + _float(face.B.y) + _float(face.C.y)) / 3.0
            return cx, cy
        elif isinstance(face, PolygonWrapper):
            verts = face.vertices
            n = len(verts)
            cx = sum(_float(v.x) for v in verts) / n
            cy = sum(_float(v.y) for v in verts) / n
            return cx, cy
        return 0.0, 0.0

    def edges(self, selector: str = "") -> Workplane:
        """选择边（CadQuery 风格 selector）。

        从已构造的对象中筛选出边状几何体（线段），
        将筛选结果存入 _selected 列表，并返回 self 以支持链式调用。

        参数：
            selector: 选择器字符串，支持以下模式：
                - "" (空): 选择所有边
                - "horizontal": 仅选择水平边（y 坐标相同）
                - "vertical": 仅选择竖直边（x 坐标相同）
                - ">X"/">Y": 选择中点在指定正半轴的边
                - "<X"/"<Y": 选择中点在指定负半轴的边
                - "|X": 选择平行于 X 轴的边（水平）
                - "|Y": 选择平行于 Y 轴的边（竖直）

        返回：
            Workplane: 自身（链式调用）
        """
        candidates = [obj for obj in self._objects if isinstance(obj, SegmentWrapper)]

        if selector:
            sel = selector.strip()
            if sel == "horizontal" or sel == "|X":
                candidates = [
                    s for s in candidates
                    if abs(_float(s.p1.y) - _float(s.p2.y)) < 1e-9
                ]
            elif sel == "vertical" or sel == "|Y":
                candidates = [
                    s for s in candidates
                    if abs(_float(s.p1.x) - _float(s.p2.x)) < 1e-9
                ]
            elif sel.startswith(">") or sel.startswith("<"):
                axis_char = sel[1].upper() if len(sel) > 1 else "X"
                positive = sel.startswith(">")
                filtered = []
                for seg in candidates:
                    mx = (_float(seg.p1.x) + _float(seg.p2.x)) / 2.0
                    my = (_float(seg.p1.y) + _float(seg.p2.y)) / 2.0
                    coord = mx if axis_char == "X" else my
                    if (positive and coord > 0) or (not positive and coord < 0):
                        filtered.append(seg)
                candidates = filtered

        self._selected = candidates
        return self

    # ---- 查询 ----

    def last(self) -> Any:
        """返回最后创建的对象。"""
        if self._objects:
            return self._objects[-1]
        if self._pending_points:
            return self._pending_points[-1]
        return None

    def all(self) -> List[Any]:
        """返回所有已创建对象的副本。"""
        return list(self._objects)

    def selected(self) -> List[Any]:
        """返回最近一次 faces()/edges() 选择的结果列表。"""
        return list(self._selected)

    # ---- 导出 ----

    def to_dsl(self) -> str:
        """导出为 DSL 文本。"""
        return self._ctx.to_dsl()

    @property
    def ctx(self) -> G:
        """获取底层 G 上下文。"""
        return self._ctx

    def __repr__(self) -> str:
        n_objs = len(self._objects)
        n_pts = len(self._pending_points)
        return f"Workplane({self._plane}, {n_objs} objects, {n_pts} points)"


# ============================================================
# AlgebraMode — 借鉴 build123d 的无状态代数模式
# ============================================================

class AlgebraMode:
    """纯代数模式构造上下文——借鉴 build123d Algebra Mode。

    与 G 上下文（有状态，隐式注册）不同，AlgebraMode 是：
    - **无状态**：所有操作返回新对象，不维护内部状态
    - **代数化**：几何操作即代数运算，`part += sub_obj`
    - **可组合**：构造结果可以自由组合和复用

    使用示例：
        >>> from lv00.dsl_algebra import AlgebraMode, Pos, Rot
        >>>
        >>> am = AlgebraMode()
        >>> A = am.point(0, 0, "A")
        >>> B = am.point(4, 0, "B")
        >>> C = am.point(0, 3, "C")
        >>> # 三角形 = 三点组合
        >>> tri = am.triangle(A, B, C)
        >>> # 变换后的三角形
        >>> tri2 = Pos(10, 0) * tri
        >>> # 检查性质
        >>> tri.is_right()  # → True
        >>> tri.centroid()
    """

    def __init__(self):
        self._ctx = G()

    def point(self, x: float, y: float,
              name: str = None) -> PointWrapper:
        """创建点（无状态，返回新对象）。"""
        return self._ctx.point(x, y, name)

    def segment(self, a: PointWrapper,
                b: PointWrapper) -> SegmentWrapper:
        """创建线段。"""
        return self._ctx.segment(a, b)

    def line(self, a: PointWrapper,
             b: PointWrapper) -> LineWrapper:
        """创建直线。"""
        return self._ctx.line(a, b)

    def circle(self, center: PointWrapper,
               radius: float) -> CircleWrapper:
        """创建圆。"""
        return self._ctx.circle(center=center, radius=radius)

    def triangle(self, a: PointWrapper, b: PointWrapper,
                 c: PointWrapper) -> TriangleWrapper:
        """创建三角形。"""
        return self._ctx.triangle(a, b, c)

    def polygon(self, *vertices: PointWrapper) -> PolygonWrapper:
        """创建多边形。"""
        return self._ctx.polygon(*vertices)

    def square(self, a: PointWrapper,
               b: PointWrapper) -> PolygonWrapper:
        """从两个相邻顶点创建正方形。"""
        return self._ctx.square(a, b)

    def midpoint(self, a: PointWrapper,
                 b: PointWrapper) -> PointWrapper:
        """计算中点。"""
        return self._ctx.midpoint(a, b)

    def distance(self, a: PointWrapper,
                 b: PointWrapper) -> float:
        """计算距离。"""
        return self._ctx.distance(a, b)

    def workplane(self, plane: Plane = None) -> Workplane:
        """在指定平面上创建工作平面。

        参数：
            plane: 基准平面（默认 Plane.XY）
        """
        if plane is None:
            plane = Plane.XY
        return plane.workplane(self._ctx)

    @property
    def ctx(self) -> G:
        """获取底层 G 上下文。"""
        return self._ctx

    def build(self) -> List[Any]:
        """构建并返回所有对象。"""
        return self._ctx.build()

    def to_dsl(self) -> str:
        """导出为 DSL。"""
        return self._ctx.to_dsl()

    def __enter__(self) -> AlgebraMode:
        return self

    def __exit__(self, *args) -> None:
        pass


# ============================================================
# 模块导出
# ============================================================

__all__ = [
    # 变换系统（build123d 风格）
    'Transform',
    'Pos',
    'Rot',
    'Scale',

    # 工作平面系统（CadQuery 风格）
    'Plane',
    'TransformedPlane',
    'Workplane',

    # 代数模式（build123d 风格）
    'AlgebraMode',
]
