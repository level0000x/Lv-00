"""
Lv-00 预设函数块 - 几何构造模块
================================

模块功能概述:
    提供理论数学研究中常用的几何构造函数块，包括:
    - 基础几何构造（中点、垂直平分线、角平分线等）
    - 圆相关构造（圆心、切线、交点等）
    - 三角形构造（重心、垂心、外心、内心等）
    - 多边形构造（正方形、正多边形等）

数学严谨性:
    所有函数都基于严格的几何定义实现，支持符号计算保持精确性。

版本: 4.0.0
作者: Lv-00 开发团队
"""

from __future__ import annotations

import math
from typing import TYPE_CHECKING, List, Optional, Tuple, Union

if TYPE_CHECKING:
    from ..core import Graph, Point, LineSegment, SymbolicCoord
    from ..func_block import FuncBlock, SolutionSelector

from .base import (
    PresetCategory,
    DeterminismLevel,
    OutputFormat,
    ParamSpec,
    OutputSpec,
    FuncBlockSpec,
    register_preset,
)

# ============================================================
# 基础几何构造
# ============================================================

# ---- 中点构造 ----
register_preset(FuncBlockSpec(
    name="midpoint",
    chinese_name="中点",
    category=PresetCategory.BASIC,
    description="""
    计算两点的中点。
    
    给定平面上的两个点 P 和 Q，构造线段 PQ 的中点 M。
    中点 M 满足：|PM| = |MQ| = |PQ|/2。
    
    这是几何构造中最基本的操作之一，广泛应用于:
    - 三角形中位线构造
    - 平行四边形对角线交点
    - 圆心定位
    """,
    params=[
        ParamSpec("p1", "点P", "第一个点", "Point", required=True),
        ParamSpec("p2", "点Q", "第二个点", "Point", required=True),
    ],
    outputs=[OutputSpec("midpoint", "中点", "线段PQ的中点", OutputFormat.POINT)],
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=["两个输入点必须不同（P ≠ Q）"],
    mathematical_definition="""
    设 P = (x₁, y₁), Q = (x₂, y₂)
    则中点 M = ((x₁+x₂)/2, (y₁+y₂)/2)
    
    在符号坐标系统中:
    M.x = (P.x + Q.x) / 2
    M.y = (P.y + Q.y) / 2
    """,
    example_usage="""
    >>> from lv import Graph
    >>> from lv.preset_blocks import create_midpoint
    >>> g = Graph()
    >>> p1 = g.add_point(0, 0)
    >>> p2 = g.add_point(2, 0)
    >>> midpoint = create_midpoint(g, p1, p2)
    >>> print(midpoint)  # Point(1, 0)
    """,
    notes="中点构造是确定性的，对于任意两个不同的点，中点唯一确定。"
))


def create_midpoint(graph: 'Graph', p1: 'Point', p2: 'Point') -> 'Point':
    """
    创建中点构造。
    
    给定两个点，计算它们的中点。
    
    Args:
        graph: 约束图对象
        p1: 第一个点
        p2: 第二个点
    
    Returns:
        Point: 中点对象
    
    Raises:
        ValueError: 两点重合时抛出
    
    Example:
        >>> g = Graph()
        >>> a = g.add_point(0, 0)
        >>> b = g.add_point(2, 0)
        >>> m = create_midpoint(g, a, b)
        >>> print(m)  # Point(1, 0)
    """
    # 验证输入点不同
    if p1.x == p2.x and p1.y == p2.y:
        raise ValueError("两点重合，无法计算中点")
    
    from ..core import SymbolicCoord
    
    # 计算中点坐标
    mid_x = (p1.x + p2.x) / SymbolicCoord.from_rational(2)
    mid_y = (p1.y + p2.y) / SymbolicCoord.from_rational(2)
    
    return graph.add_point(mid_x, mid_y)


# ---- 垂直平分线构造 ----
register_preset(FuncBlockSpec(
    name="perpendicular_bisector",
    chinese_name="垂直平分线",
    category=PresetCategory.BASIC,
    description="""
    构造线段的垂直平分线。
    
    给定线段 PQ，构造过中点 M 且垂直于 PQ 的直线。
    垂直平分线上的任意点到 P 和 Q 的距离相等。
    
    数学性质:
    - 垂直平分线是到 P、Q 等距点的轨迹
    - 三角形三边的垂直平分线交于外心
    """,
    params=[
        ParamSpec("p1", "端点P", "线段的第一个端点", "Point", required=True),
        ParamSpec("p2", "端点Q", "线段的第二个端点", "Point", required=True),
    ],
    outputs=[OutputSpec("bisector", "垂直平分线", "线段PQ的垂直平分线", OutputFormat.LINE)],
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=["两个端点必须不同（P ≠ Q）"],
    mathematical_definition="""
    设 P = (x₁, y₁), Q = (x₂, y₂)
    中点 M = ((x₁+x₂)/2, (y₁+y₂)/2)
    
    垂直平分线方程:
    (x - M.x)(Q.x - P.x) + (y - M.y)(Q.y - P.y) = 0
    
    或用斜率表示:
    若 PQ 斜率为 k，则垂直平分线斜率为 -1/k
    """,
    example_usage="""
    >>> from lv import Graph
    >>> from lv.preset_blocks import create_perpendicular_bisector
    >>> g = Graph()
    >>> p1 = g.add_point(0, 0)
    >>> p2 = g.add_point(2, 0)
    >>> bisector = create_perpendicular_bisector(g, p1, p2)
    """,
    notes="垂直平分线用线段表示，需要指定长度或延伸到边界。"
))


def create_perpendicular_bisector(
    graph: 'Graph',
    p1: 'Point',
    p2: 'Point',
    length: Optional['SymbolicCoord'] = None
) -> 'LineSegment':
    """
    创建垂直平分线构造。
    
    给定两个点，构造过中点且垂直于连线的直线（用有限线段表示）。
    
    Args:
        graph: 约束图对象
        p1: 第一个端点
        p2: 第二个端点
        length: 线段长度（可选，默认为单位长度）
    
    Returns:
        LineSegment: 垂直平分线段
    
    Raises:
        ValueError: 两点重合时抛出
    
    Example:
        >>> g = Graph()
        >>> a = g.add_point(0, 0)
        >>> b = g.add_point(2, 0)
        >>> bisector = create_perpendicular_bisector(g, a, b)
    """
    # 验证输入点不同
    if p1.x == p2.x and p1.y == p2.y:
        raise ValueError("两点重合，无法构造垂直平分线")
    
    from ..core import SymbolicCoord
    
    # 计算中点
    mid_x = (p1.x + p2.x) / SymbolicCoord.from_rational(2)
    mid_y = (p1.y + p2.y) / SymbolicCoord.from_rational(2)
    mid = graph.add_point(mid_x, mid_y)
    
    # 计算垂直方向向量
    dx = p2.x - p1.x
    dy = p2.y - p1.y
    
    # 垂直方向（旋转90度）
    perp_x = -dy
    perp_y = dx
    
    # 归一化（如果需要指定长度）
    if length is not None:
        half_len = length / SymbolicCoord.from_rational(2)
    else:
        half_len = SymbolicCoord.from_rational(1)
    
    # 创建垂直平分线的两个端点
    end1 = graph.add_point(
        mid_x + perp_x * half_len,
        mid_y + perp_y * half_len
    )
    end2 = graph.add_point(
        mid_x - perp_x * half_len,
        mid_y - perp_y * half_len
    )
    
    return graph.add_line_segment(end1, end2)


# ---- 角平分线构造 ----
register_preset(FuncBlockSpec(
    name="angle_bisector",
    chinese_name="角平分线",
    category=PresetCategory.BASIC,
    description="""
    构造角的平分线。
    
    给定三个点 A、B、C（B 为角的顶点），构造 ∠ABC 的平分线。
    角平分线将角分成两个相等的角。
    
    数学性质:
    - 角平分线上的点到角两边的距离相等
    - 三角形内角平分线交于内心
    """,
    params=[
        ParamSpec("a", "点A", "角的一边端点", "Point", required=True),
        ParamSpec("b", "顶点B", "角的顶点", "Point", required=True),
        ParamSpec("c", "点C", "角的另一边端点", "Point", required=True),
    ],
    outputs=[OutputSpec("bisector", "角平分线", "∠ABC的平分线", OutputFormat.LINE)],
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=[
        "三个点不能共线",
        "顶点 B 不能与 A 或 C 重合"
    ],
    mathematical_definition="""
    设 A = (x₁, y₁), B = (x₂, y₂), C = (x₃, y₃)
    
    单位方向向量:
    u = (A - B) / |A - B|
    v = (C - B) / |C - B|
    
    角平分线方向向量:
    w = u + v （归一化）
    
    角平分线: B + t·w, t ∈ ℝ
    """,
    example_usage="""
    >>> from lv import Graph
    >>> from lv.preset_blocks import create_angle_bisector
    >>> g = Graph()
    >>> a = g.add_point(0, 1)
    >>> b = g.add_point(0, 0)  # 顶点
    >>> c = g.add_point(1, 0)
    >>> bisector = create_angle_bisector(g, a, b, c)
    """,
    notes="对于平角（180°），角平分线有两条，需要选择器。"
))


def create_angle_bisector(
    graph: 'Graph',
    a: 'Point',
    b: 'Point',
    c: 'Point'
) -> 'LineSegment':
    """
    创建角平分线构造。
    
    给定三个点 A、B、C（B 为顶点），构造 ∠ABC 的平分线。
    
    Args:
        graph: 约束图对象
        a: 角的一边端点
        b: 角的顶点
        c: 角的另一边端点
    
    Returns:
        LineSegment: 角平分线（用线段表示）
    
    Raises:
        ValueError: 三点共线或顶点重合时抛出
    """
    from ..core import SymbolicCoord
    
    # 检查顶点不与端点重合
    if (b.x == a.x and b.y == a.y) or (b.x == c.x and b.y == c.y):
        raise ValueError("顶点不能与端点重合")
    
    # 检查三点不共线
    det = (a.x - b.x) * (c.y - b.y) - (a.y - b.y) * (c.x - b.x)
    if det.is_zero():
        raise ValueError("三点共线，无法构造角平分线")
    
    # 计算单位方向向量
    # BA 方向
    ba_dx = a.x - b.x
    ba_dy = a.y - b.y
    ba_len = (ba_dx * ba_dx + ba_dy * ba_dy) ** SymbolicCoord.from_rational(1, 2)
    
    # BC 方向
    bc_dx = c.x - b.x
    bc_dy = c.y - b.y
    bc_len = (bc_dx * bc_dx + bc_dy * bc_dy) ** SymbolicCoord.from_rational(1, 2)
    
    # 单位向量之和（角平分线方向）
    bisect_dx = ba_dx / ba_len + bc_dx / bc_len
    bisect_dy = ba_dy / ba_len + bc_dy / bc_len
    
    # 创建角平分线上的点
    end_point = graph.add_point(
        b.x + bisect_dx,
        b.y + bisect_dy
    )
    
    return graph.add_line_segment(b, end_point)


# ---- 线段交点构造 ----
register_preset(FuncBlockSpec(
    name="line_intersection",
    chinese_name="线段交点",
    category=PresetCategory.BASIC,
    description="""
    计算两条线段的交点。
    
    给定两条线段 AB 和 CD，计算它们的交点（若存在）。
    
    情况分析:
    - 若线段相交，返回唯一交点
    - 若线段平行但不重合，无解
    - 若线段重合，可能有无限多解
    """,
    params=[
        ParamSpec("a", "点A", "第一条线段的端点", "Point", required=True),
        ParamSpec("b", "点B", "第一条线段的端点", "Point", required=True),
        ParamSpec("c", "点C", "第二条线段的端点", "Point", required=True),
        ParamSpec("d", "点D", "第二条线段的端点", "Point", required=True),
    ],
    outputs=[OutputSpec("intersection", "交点", "两条线段的交点", OutputFormat.POINT)],
    determinism=DeterminismLevel.CONDITIONALLY_UNIQUE,
    preconditions=[
        "两条线段不能退化（端点不重合）",
        "线段不平行或重合"
    ],
    mathematical_definition="""
    设 AB: A + t(B-A), t ∈ [0,1]
    设 CD: C + s(D-C), s ∈ [0,1]
    
    求解方程组:
    A + t(B-A) = C + s(D-C)
    
    若行列式 |B-A, D-C| ≠ 0，有唯一解:
    t = |C-A, D-C| / |B-A, D-C|
    s = |C-A, B-A| / |B-A, D-C|
    
    当 t, s ∈ [0,1] 时，线段相交
    """,
    example_usage="""
    >>> from lv import Graph
    >>> from lv.preset_blocks import create_line_intersection
    >>> g = Graph()
    >>> a = g.add_point(0, 0)
    >>> b = g.add_point(2, 2)
    >>> c = g.add_point(0, 2)
    >>> d = g.add_point(2, 0)
    >>> intersection = create_line_intersection(g, a, b, c, d)
    >>> print(intersection)  # Point(1, 1)
    """,
    notes="平行线无交点，重合线段需要特殊处理。"
))


def create_line_intersection(
    graph: 'Graph',
    a: 'Point',
    b: 'Point',
    c: 'Point',
    d: 'Point'
) -> 'Point':
    """
    创建线段交点构造。
    
    计算两条线段 AB 和 CD 的交点。
    
    Args:
        graph: 约束图对象
        a: 第一条线段的端点
        b: 第一条线段的端点
        c: 第二条线段的端点
        d: 第二条线段的端点
    
    Returns:
        Point: 交点
    
    Raises:
        ValueError: 线段平行或退化时抛出
    """
    from ..core import SymbolicCoord
    
    # 计算方向向量
    ab_dx = b.x - a.x
    ab_dy = b.y - a.y
    cd_dx = d.x - c.x
    cd_dy = d.y - c.y
    
    # 计算行列式
    det = ab_dx * cd_dy - ab_dy * cd_dx
    
    if det.is_zero():
        raise ValueError("线段平行或共线，无唯一交点")
    
    # 计算交点参数
    ac_dx = c.x - a.x
    ac_dy = c.y - a.y
    
    t = (ac_dx * cd_dy - ac_dy * cd_dx) / det
    
    # 计算交点坐标
    ix = a.x + t * ab_dx
    iy = a.y + t * ab_dy
    
    return graph.add_point(ix, iy)


# ---- 垂线构造 ----
register_preset(FuncBlockSpec(
    name="perpendicular_line",
    chinese_name="垂线",
    category=PresetCategory.BASIC,
    description="""
    过一点作直线的垂线。
    
    给定直线 l 和点 P（不在 l 上或在其上），构造过 P 且垂直于 l 的直线。
    
    数学性质:
    - 垂线与原直线的夹角为 90°
    - 点 P 到直线 l 的距离等于 P 到垂足的距离
    """,
    params=[
        ParamSpec("a", "点A", "直线上第一点", "Point", required=True),
        ParamSpec("b", "点B", "直线上第二点", "Point", required=True),
        ParamSpec("p", "点P", "垂线经过的点", "Point", required=True),
    ],
    outputs=[OutputSpec("perp", "垂线", "过P垂直于AB的直线", OutputFormat.LINE)],
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=["直线不能退化（两点不重合）"],
    mathematical_definition="""
    设直线 l 过点 A(x₁,y₁) 和 B(x₂,y₂)
    点 P = (x₀, y₀)
    
    直线 l 的方向向量: d = (x₂-x₁, y₂-y₁)
    垂线的方向向量: d⊥ = (y₁-y₂, x₂-x₁)
    
    垂线方程: P + t·d⊥
    """,
    example_usage="""
    >>> from lv import Graph
    >>> from lv.preset_blocks import create_perpendicular_line
    >>> g = Graph()
    >>> a = g.add_point(0, 0)
    >>> b = g.add_point(2, 0)
    >>> p = g.add_point(1, 1)
    >>> perp = create_perpendicular_line(g, a, b, p)
    """,
    notes="无论点 P 是否在直线上，垂线都是唯一确定的。"
))


def create_perpendicular_line(
    graph: 'Graph',
    a: 'Point',
    b: 'Point',
    p: 'Point'
) -> 'LineSegment':
    """
    创建垂线构造。
    
    过点 P 作直线 AB 的垂线。
    
    Args:
        graph: 约束图对象
        a: 直线上第一点
        b: 直线上第二点
        p: 垂线经过的点
    
    Returns:
        LineSegment: 垂线（用线段表示）
    
    Raises:
        ValueError: 直线退化时抛出
    """
    from ..core import SymbolicCoord
    
    # 验证直线不退化
    if a.x == b.x and a.y == b.y:
        raise ValueError("直线退化：A 和 B 重合")
    
    # 计算垂直方向向量
    dx = b.x - a.x
    dy = b.y - a.y
    perp_dx = -dy
    perp_dy = dx
    
    # 创建垂线上的点
    end_point = graph.add_point(
        p.x + perp_dx,
        p.y + perp_dy
    )
    
    return graph.add_line_segment(p, end_point)


# ---- 平行线构造 ----
register_preset(FuncBlockSpec(
    name="parallel_line",
    chinese_name="平行线",
    category=PresetCategory.BASIC,
    description="""
    过一点作直线的平行线。
    
    给定直线 l 和点 P，构造过 P 且平行于 l 的直线。
    
    数学性质:
    - 平行线永不相交
    - 平行线具有相同的斜率
    """,
    params=[
        ParamSpec("a", "点A", "直线上第一点", "Point", required=True),
        ParamSpec("b", "点B", "直线上第二点", "Point", required=True),
        ParamSpec("p", "点P", "平行线经过的点", "Point", required=True),
    ],
    outputs=[OutputSpec("parallel", "平行线", "过P平行于AB的直线", OutputFormat.LINE)],
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=["直线不能退化（两点不重合）"],
    mathematical_definition="""
    设直线 l 过点 A(x₁,y₁) 和 B(x₂,y₂)
    点 P = (x₀, y₀)
    
    平行线方向向量: d = (x₂-x₁, y₂-y₁)
    平行线方程: P + t·d
    """,
    example_usage="""
    >>> from lv import Graph
    >>> from lv.preset_blocks import create_parallel_line
    >>> g = Graph()
    >>> a = g.add_point(0, 0)
    >>> b = g.add_point(1, 1)
    >>> p = g.add_point(0, 1)
    >>> parallel = create_parallel_line(g, a, b, p)
    """,
    notes="平行线是唯一确定的。"
))


def create_parallel_line(
    graph: 'Graph',
    a: 'Point',
    b: 'Point',
    p: 'Point'
) -> 'LineSegment':
    """
    创建平行线构造。
    
    过点 P 作直线 AB 的平行线。
    
    Args:
        graph: 约束图对象
        a: 直线上第一点
        b: 直线上第二点
        p: 平行线经过的点
    
    Returns:
        LineSegment: 平行线（用线段表示）
    
    Raises:
        ValueError: 直线退化时抛出
    """
    # 验证直线不退化
    if a.x == b.x and a.y == b.y:
        raise ValueError("直线退化：A 和 B 重合")
    
    # 计算方向向量
    dx = b.x - a.x
    dy = b.y - a.y
    
    # 创建平行线上的点
    end_point = graph.add_point(
        p.x + dx,
        p.y + dy
    )
    
    return graph.add_line_segment(p, end_point)


# ---- 距离计算 ----
register_preset(FuncBlockSpec(
    name="distance",
    chinese_name="两点距离",
    category=PresetCategory.BASIC,
    description="""
    计算两点之间的欧几里得距离。
    
    给定平面上的两个点 P 和 Q，计算线段 PQ 的长度。
    
    数学性质:
    - 距离满足三角不等式
    - 距离非负，仅当 P=Q 时为零
    - 距离具有对称性: |PQ| = |QP|
    """,
    params=[
        ParamSpec("p1", "点P", "第一个点", "Point", required=True),
        ParamSpec("p2", "点Q", "第二个点", "Point", required=True),
    ],
    outputs=[OutputSpec("distance", "距离", "两点间的欧几里得距离", OutputFormat.SCALAR)],
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=[],
    mathematical_definition="""
    设 P = (x₁, y₁), Q = (x₂, y₂)
    
    距离 d = √((x₁ - x₂)² + (y₁ - y₂)²)
    
    使用符号计算时，结果以符号形式保留:
    d = sqrt((P.x - Q.x)² + (P.y - Q.y)²)
    """,
    example_usage="""
    >>> from lv import Graph
    >>> from lv.preset_blocks import create_distance
    >>> g = Graph()
    >>> p = g.add_point(0, 0)
    >>> q = g.add_point(3, 4)
    >>> d = create_distance(g, p, q)
    >>> print(d)  # 5
    """,
    notes="当两点重合时距离为零。返回 SymbolicCoord 类型。"
))


def create_distance(
    graph: 'Graph',
    p1: 'Point',
    p2: 'Point'
) -> 'SymbolicCoord':
    """
    计算两点之间的欧几里得距离。
    
    Args:
        graph: 约束图对象（保留参数，用于接口统一）
        p1: 第一个点
        p2: 第二个点
    
    Returns:
        SymbolicCoord: 两点之间的距离（符号值）
    
    Example:
        >>> g = Graph()
        >>> p = g.add_point(0, 0)
        >>> q = g.add_point(3, 4)
        >>> d = create_distance(g, p, q)
        >>> print(d)  # 5
    """
    return p1.distance_to(p2)


# ---- 垂足构造 ----
register_preset(FuncBlockSpec(
    name="foot_of_perpendicular",
    chinese_name="垂足",
    category=PresetCategory.BASIC,
    description="""
    计算点到直线的垂足。
    
    给定直线 AB 和点 P，计算 P 到直线 AB 的垂足（投影点）。
    
    数学性质:
    - 垂足是直线上距离 P 最近的点
    - 垂足与 P 的连线垂直于直线
    """,
    params=[
        ParamSpec("a", "点A", "直线上第一点", "Point", required=True),
        ParamSpec("b", "点B", "直线上第二点", "Point", required=True),
        ParamSpec("p", "点P", "待投影的点", "Point", required=True),
    ],
    outputs=[OutputSpec("foot", "垂足", "P到直线AB的垂足", OutputFormat.POINT)],
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=["直线不能退化（两点不重合）"],
    mathematical_definition="""
    设直线 AB，点 P
    
    直线的方向向量: v = B - A
    向量 w = P - A
    
    投影参数: t = (w·v) / (v·v)
    垂足: Q = A + t·v
    """,
    example_usage="""
    >>> from lv import Graph
    >>> from lv.preset_blocks import create_foot_of_perpendicular
    >>> g = Graph()
    >>> a = g.add_point(0, 0)
    >>> b = g.add_point(2, 0)
    >>> p = g.add_point(1, 2)
    >>> foot = create_foot_of_perpendicular(g, a, b, p)
    >>> print(foot)  # Point(1, 0)
    """,
    notes="垂足是直线上距离给定点最近的点。"
))


def create_foot_of_perpendicular(
    graph: 'Graph',
    a: 'Point',
    b: 'Point',
    p: 'Point'
) -> 'Point':
    """
    创建垂足构造。
    
    计算点 P 到直线 AB 的垂足。
    
    Args:
        graph: 约束图对象
        a: 直线上第一点
        b: 直线上第二点
        p: 待投影的点
    
    Returns:
        Point: 垂足
    
    Raises:
        ValueError: 直线退化时抛出
    """
    from ..core import SymbolicCoord
    
    # 验证直线不退化
    if a.x == b.x and a.y == b.y:
        raise ValueError("直线退化：A 和 B 重合")
    
    # 直线的方向向量
    v_dx = b.x - a.x
    v_dy = b.y - a.y
    
    # 向量 w = P - A
    w_dx = p.x - a.x
    w_dy = p.y - a.y
    
    # 计算投影参数 t = (w·v) / (v·v)
    dot_wv = w_dx * v_dx + w_dy * v_dy
    dot_vv = v_dx * v_dx + v_dy * v_dy
    
    if dot_vv.is_zero():
        raise ValueError("方向向量为零")
    
    t = dot_wv / dot_vv
    
    # 垂足坐标
    foot_x = a.x + t * v_dx
    foot_y = a.y + t * v_dy
    
    return graph.add_point(foot_x, foot_y)


# ============================================================
# 圆相关构造
# ============================================================

# ---- 圆心构造 ----
register_preset(FuncBlockSpec(
    name="circle_center",
    chinese_name="圆心",
    category=PresetCategory.CIRCLE,
    description="""
    由圆上三点确定圆心。
    
    给定圆上的三个不共线点 A、B、C，确定圆心 O。
    
    数学原理:
    - 圆心到三点的距离相等
    - 圆心是任意两边垂直平分线的交点
    """,
    params=[
        ParamSpec("a", "点A", "圆上第一点", "Point", required=True),
        ParamSpec("b", "点B", "圆上第二点", "Point", required=True),
        ParamSpec("c", "点C", "圆上第三点", "Point", required=True),
    ],
    outputs=[OutputSpec("center", "圆心", "过A、B、C三点的圆的圆心", OutputFormat.POINT)],
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=[
        "三点不能共线",
        "三点必须互不相同"
    ],
    mathematical_definition="""
    设 A、B、C 为圆上三点
    
    方法一：垂直平分线交点
    - AB 的垂直平分线 l₁
    - BC 的垂直平分线 l₂
    - O = l₁ ∩ l₂
    
    方法二：代数求解
    圆心 O = (x₀, y₀) 满足:
    |O - A|² = |O - B|² = |O - C|²
    """,
    example_usage="""
    >>> from lv import Graph
    >>> from lv.preset_blocks import create_circle_center
    >>> g = Graph()
    >>> a = g.add_point(1, 0)
    >>> b = g.add_point(0, 1)
    >>> c = g.add_point(-1, 0)
    >>> center = create_circle_center(g, a, b, c)
    >>> print(center)  # Point(0, 0)
    """,
    notes="三点共线时无解，因为不存在过共线三点的圆。"
))


def create_circle_center(
    graph: 'Graph',
    a: 'Point',
    b: 'Point',
    c: 'Point'
) -> 'Point':
    """
    创建圆心构造。
    
    由圆上三点确定圆心。
    
    Args:
        graph: 约束图对象
        a: 圆上第一点
        b: 圆上第二点
        c: 圆上第三点
    
    Returns:
        Point: 圆心
    
    Raises:
        ValueError: 三点共线时抛出
    """
    # 复用外心构造（三点确定圆心等价于三角形外心）
    return create_circumcenter(graph, a, b, c)


# ---- 圆心和半径构造圆 ----
register_preset(FuncBlockSpec(
    name="circle_by_center_radius",
    chinese_name="圆心和半径作圆",
    category=PresetCategory.CIRCLE,
    description="""
    由圆心和半径构造圆。
    
    给定圆心 O 和半径 r，构造圆。
    
    数学定义:
    - 圆是到定点（圆心）距离等于定长（半径）的点的轨迹
    """,
    params=[
        ParamSpec("center", "圆心", "圆的圆心", "Point", required=True),
        ParamSpec("radius", "半径", "圆的半径", "Scalar", required=True),
    ],
    outputs=[OutputSpec("circle", "圆", "以center为圆心、radius为半径的圆", OutputFormat.CIRCLE)],
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=["半径必须为正数"],
    mathematical_definition="""
    设圆心 O = (x₀, y₀)，半径 r
    
    圆方程: (x - x₀)² + (y - y₀)² = r²
    
    圆上任意点 P 满足: |P - O| = r
    """,
    example_usage="""
    >>> from lv import Graph
    >>> from lv.preset_blocks import create_circle_by_center_radius
    >>> from ..core import SymbolicCoord
    >>> g = Graph()
    >>> o = g.add_point(0, 0)
    >>> r = SymbolicCoord(5)
    >>> circle = create_circle_by_center_radius(g, o, r)
    """,
    notes="半径必须为正数。"
))


def create_circle_by_center_radius(
    graph: 'Graph',
    center: 'Point',
    radius: Union['SymbolicCoord', int, float]
) -> Tuple['Point', 'SymbolicCoord']:
    """
    由圆心和半径构造圆。
    
    Args:
        graph: 约束图对象
        center: 圆心
        radius: 半径
    
    Returns:
        Tuple[Point, SymbolicCoord]: (圆心, 半径)
    
    Raises:
        ValueError: 半径非正时抛出
    """
    from ..core import SymbolicCoord
    
    if not isinstance(radius, SymbolicCoord):
        radius = SymbolicCoord(radius)
    
    # 检查半径为正
    if not radius.is_positive():
        raise ValueError("半径必须为正数")
    
    return (center, radius)


# ---- 圆与直线交点 ----
register_preset(FuncBlockSpec(
    name="circle_line_intersection",
    chinese_name="圆与直线交点",
    category=PresetCategory.CIRCLE,
    description="""
    计算圆与直线的交点。
    
    给定圆心 O、半径 r 和直线 AB，计算它们的交点。
    
    情况分析:
    - 相交：两个交点
    - 相切：一个交点
    - 相离：无交点
    """,
    params=[
        ParamSpec("center", "圆心", "圆的圆心", "Point", required=True),
        ParamSpec("radius", "半径", "圆的半径", "Scalar", required=True),
        ParamSpec("a", "点A", "直线上第一点", "Point", required=True),
        ParamSpec("b", "点B", "直线上第二点", "Point", required=True),
    ],
    outputs=[OutputSpec("intersections", "交点", "圆与直线的交点列表", OutputFormat.COLLECTION)],
    determinism=DeterminismLevel.MULTIPLE_SOLUTIONS,
    preconditions=[
        "半径必须为正",
        "直线不能退化"
    ],
    mathematical_definition="""
    设圆: (x - x₀)² + (y - y₀)² = r²   设直线: ax + by + c = 0
    
    联立方程求解:
    圆心到直线的距离 d = |ax₀ + by₀ + c| / √(a² + b²)
    
    - 若 d > r：无交点
    - 若 d = r：一个交点（相切）
    - 若 d < r：两个交点
    """,
    example_usage="""
    >>> from lv import Graph
    >>> from lv.preset_blocks import create_circle_line_intersection
    >>> g = Graph()
    >>> o = g.add_point(0, 0)
    >>> r = 1  # 半径
    >>> a = g.add_point(-1, 0)
    >>> b = g.add_point(1, 0)
    >>> intersections = create_circle_line_intersection(g, o, r, a, b)
    """,
    notes="当有两个交点时，需要选择器确定返回哪个。"
))


def create_circle_line_intersection(
    graph: 'Graph',
    center: 'Point',
    radius: Union['SymbolicCoord', int, float],
    a: 'Point',
    b: 'Point'
) -> List['Point']:
    """
    计算圆与直线的交点。
    
    Args:
        graph: 约束图对象
        center: 圆心
        radius: 半径
        a: 直线上第一点
        b: 直线上第二点
    
    Returns:
        List[Point]: 交点列表（0、1 或 2 个点）
    
    Raises:
        ValueError: 直线退化时抛出
    """
    from ..core import SymbolicCoord
    
    if not isinstance(radius, SymbolicCoord):
        radius = SymbolicCoord(radius)
    
    # 验证直线不退化
    if a.x == b.x and a.y == b.y:
        raise ValueError("直线退化：A 和 B 重合")
    
    # 计算圆心到直线的距离
    # 直线方程: (b.y - a.y)(x - a.x) - (b.x - a.x)(y - a.y) = 0
    line_a = b.y - a.y
    line_b = a.x - b.x
    line_c = -(line_a * a.x + line_b * a.y)
    
    # 距离 d = |Ax₀ + By₀ + C| / √(A² + B²)
    dist_num = (line_a * center.x + line_b * center.y + line_c).__abs__()
    dist_den = (line_a * line_a + line_b * line_b) ** SymbolicCoord.from_rational(1, 2)
    distance = dist_num / dist_den
    
    # 比较距离和半径
    # distance² = dist_num² / dist_den²
    # 比较 distance 与 radius，等价于比较 distance² 与 radius²
    dist_sq = dist_num * dist_num
    radius_sq = radius * radius * dist_den * dist_den
    
    # 计算差值: dist_sq - radius_sq
    diff = dist_sq - radius_sq
    
    if diff.is_positive():
        # distance > radius，无交点
        return []
    
    # 计算圆心在直线上的投影点
    # t = - (A*x0 + B*y0 + C) / (A² + B²)
    # 投影点 = (x0 + A*t, y0 + B*t)
    t_num = -(line_a * center.x + line_b * center.y + line_c)
    t_den = line_a * line_a + line_b * line_b
    
    proj_x = center.x + line_a * t_num / t_den
    proj_y = center.y + line_b * t_num / t_den
    
    if diff.is_zero():
        # distance == radius，相切，一个交点
        return [graph.add_point(proj_x, proj_y)]
    
    # distance < radius，两个交点
    # 半弦长 h = sqrt(r² - d²)
    # 沿直线方向单位向量 (B, -A) / sqrt(A²+B²) 或 (-B, A) / sqrt(A²+B²)
    # 交点 = 投影点 ± h * 单位方向向量
    
    # h² = r² - d² = (radius² * dist_den² - dist_num²) / dist_den²
    h_sq_num = radius_sq - dist_sq
    h_sq_den = dist_den * dist_den
    
    # 半弦长因子: h = sqrt(h_sq_num / h_sq_den) = sqrt(h_sq_num) / dist_den
    # 方向向量 (line_b, -line_a) 的模也是 dist_den
    # 所以偏移量 = h * (line_b, -line_a) / dist_den
    #            = sqrt(h_sq_num) / dist_den * (line_b, -line_a) / dist_den
    #            = sqrt(h_sq_num) * (line_b, -line_a) / (dist_den * dist_den)
    
    # 使用符号平方根
    h_sqrt = h_sq_num ** SymbolicCoord.from_rational(1, 2)
    
    # 方向向量 (line_b, -line_a)
    # 归一化因子: dist_den * dist_den (因为 h 已经除以 dist_den 一次)
    denom = t_den * dist_den
    
    offset_x = h_sqrt * line_b / denom
    offset_y = h_sqrt * (-line_a) / denom
    
    p1_x = proj_x + offset_x
    p1_y = proj_y + offset_y
    p2_x = proj_x - offset_x
    p2_y = proj_y - offset_y
    
    return [graph.add_point(p1_x, p1_y), graph.add_point(p2_x, p2_y)]


# ---- 切线构造 ----
register_preset(FuncBlockSpec(
    name="tangent_line",
    chinese_name="切线",
    category=PresetCategory.CIRCLE,
    description="""
    过圆外一点作圆的切线。
    
    给定圆心 O、半径 r 和圆外一点 P，构造过 P 的切线。
    
    数学性质:
    - 切线与半径垂直
    - 从圆外一点可作两条切线
    """,
    params=[
        ParamSpec("center", "圆心", "圆的圆心", "Point", required=True),
        ParamSpec("radius", "半径", "圆的半径", "Scalar", required=True),
        ParamSpec("p", "点P", "圆外一点", "Point", required=True),
    ],
    outputs=[OutputSpec("tangents", "切线", "从P到圆的切线列表", OutputFormat.COLLECTION)],
    determinism=DeterminismLevel.MULTIPLE_SOLUTIONS,
    preconditions=[
        "点 P 必须在圆外（|OP| > r）",
        "半径必须为正"
    ],
    mathematical_definition="""
    设圆心 O，半径 r，圆外点 P
    
    切点 T 满足:
    1. |OT| = r
    2. OT ⊥ PT
    3. T 在以 OP 为直径的圆上
    
    切点方程:
    以 OP 为直径的圆与原圆的交点即为切点
    """,
    example_usage="""
    >>> from lv import Graph
    >>> from lv.preset_blocks import create_tangent_line
    >>> g = Graph()
    >>> o = g.add_point(0, 0)
    >>> p = g.add_point(2, 0)
    >>> r = 1
    >>> tangents = create_tangent_line(g, o, r, p)
    """,
    notes="圆上的点只能作一条切线，圆内的点不能作切线。"
))


def create_tangent_line(
    graph: 'Graph',
    center: 'Point',
    radius: Union['SymbolicCoord', int, float],
    p: 'Point'
) -> List['LineSegment']:
    """
    过圆外一点作圆的切线。
    
    Args:
        graph: 约束图对象
        center: 圆心
        radius: 半径
        p: 圆外一点
    
    Returns:
        List[LineSegment]: 切线列表
    
    Raises:
        ValueError: 点在圆内时抛出
    """
    from ..core import SymbolicCoord
    
    if not isinstance(radius, SymbolicCoord):
        radius = SymbolicCoord(radius)
    
    # 计算点到圆心的距离平方
    dist_sq = center.distance_to(p)
    radius_sq = radius * radius
    
    # 比较距离与半径
    diff = dist_sq - radius_sq
    
    if diff.is_zero():
        # 点在圆上，一条切线：垂直于半径
        # 切线方向向量 = (-(p.y - center.y), p.x - center.x)
        dir_x = center.y - p.y
        dir_y = p.x - center.x
        # 切线上另一点
        q_x = p.x + dir_x
        q_y = p.y + dir_y
        q = graph.add_point(q_x, q_y)
        return [graph.add_line_segment(p, q)]
    
    if not diff.is_positive():
        # 点在圆内，无切线
        raise ValueError("点 P 在圆内，无法作切线")
    
    # 点在圆外，两条切线
    # 使用幂点法（power of a point）
    # 设 d = |OP|, r = radius
    # 切点 T 满足：
    #   |OT| = r
    #   OT ⟂ PT
    # 以 OP 为直径的圆与原圆的交点即为切点
    
    # 以 OP 为直径的圆：圆心为 OP 中点，半径为 d/2
    # 原圆：圆心 center，半径 r
    # 两圆交点即为切点
    
    # 代数方法：
    # 设 O = center, P = p
    # 向量 OP = (p.x - center.x, p.y - center.y)
    # 距离平方 d² = dist_sq
    # 
    # 切点 T 在圆上：|OT|² = r²
    # 且 OT ⟂ PT：(T-O)·(T-P) = 0
    # 展开：T·T - T·O - T·P + O·P = 0
    # 结合 |T-O|² = r²：T·T - 2T·O + O·O = r²
    # 
    # 由 OT ⟂ PT：(T-O)·(T-P) = 0
    # |T|² - T·P - T·O + O·P = 0
    # (r² + 2T·O - O·O) - T·P - T·O + O·P = 0
    # r² + T·O - O·O - T·P + O·P = 0
    # T·(O - P) = O·O - O·P - r²
    # 
    # 设 T = O + u * v1 + v * v2，其中 v1 = (P-O)/|P-O|，v2 ⟂ v1
    # 由于对称性，T 在 OP 所在直线的垂线上有对称的两个解
    
    # 更简洁的方法：
    # 设 d = sqrt(dist_sq)，单位向量 u = (P-O)/d
    # 则切点 T = O + r²/d² * (P-O) ± r/d * sqrt(1 - r²/d²) * (P-O)⟂
    #          = O + r²/d² * OP ± r/d² * sqrt(d² - r²) * OP⟂
    
    dx = p.x - center.x
    dy = p.y - center.y
    
    # 使用符号计算避免 float
    # r² / d²
    r2_over_d2 = radius_sq / dist_sq
    
    # 基础点：O + (r²/d²) * OP
    base_x = center.x + r2_over_d2 * dx
    base_y = center.y + r2_over_d2 * dy
    
    # 垂直方向：(-dy, dx)
    # 系数：r/d² * sqrt(d² - r²) = r * sqrt(d² - r²) / d²
    # d² = dist_sq, d² - r² = diff
    # 所以系数 = r * sqrt(diff) / dist_sq
    perp_coeff = radius * (diff ** SymbolicCoord.from_rational(1, 2)) / dist_sq
    
    # 两个切点
    t1_x = base_x + perp_coeff * (-dy)
    t1_y = base_y + perp_coeff * dx
    t2_x = base_x - perp_coeff * (-dy)
    t2_y = base_y - perp_coeff * dx
    
    t1 = graph.add_point(t1_x, t1_y)
    t2 = graph.add_point(t2_x, t2_y)
    
    return [graph.add_line_segment(p, t1), graph.add_line_segment(p, t2)]


# ============================================================
# 三角形构造
# ============================================================

# ---- 重心构造 ----
register_preset(FuncBlockSpec(
    name="centroid",
    chinese_name="重心",
    category=PresetCategory.TRIANGLE,
    description="""
    计算三角形的重心（质心）。
    
    给定三角形的三个顶点 A、B、C，计算重心 G。
    重心是三条中线的交点。
    
    数学性质:
    - 重心将每条中线分为 2:1 的比例
    - 重心坐标等于三顶点坐标的平均值
    - 重心是三角形的几何中心
    """,
    params=[
        ParamSpec("a", "顶点A", "三角形第一个顶点", "Point", required=True),
        ParamSpec("b", "顶点B", "三角形第二个顶点", "Point", required=True),
        ParamSpec("c", "顶点C", "三角形第三个顶点", "Point", required=True),
    ],
    outputs=[OutputSpec("centroid", "重心", "三角形ABC的重心", OutputFormat.POINT)],
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=[
        "三点不能共线",
        "三点必须互不相同"
    ],
    mathematical_definition="""
    设 A = (x₁, y₁), B = (x₂, y₂), C = (x₃, y₃)
    
    重心 G = ((x₁+x₂+x₃)/3, (y₁+y₂+y₃)/3)
    
    或用向量表示:
    G = (A + B + C) / 3
    """,
    example_usage="""
    >>> from lv import Graph
    >>> from lv.preset_blocks import create_centroid
    >>> g = Graph()
    >>> a = g.add_point(0, 0)
    >>> b = g.add_point(2, 0)
    >>> c = g.add_point(1, 2)
    >>> centroid = create_centroid(g, a, b, c)
    >>> print(centroid)  # Point(1, 2/3)
    """,
    notes="重心总是唯一确定的，且一定在三角形内部。"
))


def create_centroid(
    graph: 'Graph',
    a: 'Point',
    b: 'Point',
    c: 'Point'
) -> 'Point':
    """
    创建三角形重心构造。
    
    Args:
        graph: 约束图对象
        a: 第一个顶点
        b: 第二个顶点
        c: 第三个顶点
    
    Returns:
        Point: 重心
    
    Raises:
        ValueError: 三点共线时抛出
    """
    from ..core import SymbolicCoord
    
    # 检查三点是否共线
    det = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)
    if det.is_zero():
        raise ValueError("三点共线，无法构成三角形")
    
    # 计算重心坐标
    centroid_x = (a.x + b.x + c.x) / SymbolicCoord.from_rational(3)
    centroid_y = (a.y + b.y + c.y) / SymbolicCoord.from_rational(3)
    
    return graph.add_point(centroid_x, centroid_y)


# ---- 外心构造 ----
register_preset(FuncBlockSpec(
    name="circumcenter",
    chinese_name="外心",
    category=PresetCategory.TRIANGLE,
    description="""
    计算三角形的外心（外接圆圆心）。
    
    给定三角形的三个顶点 A、B、C，计算外心 O。
    外心是三边垂直平分线的交点。
    
    数学性质:
    - 外心到三个顶点的距离相等
    - 外心是三角形外接圆的圆心
    - 直角三角形的外心在斜边中点
    """,
    params=[
        ParamSpec("a", "顶点A", "三角形第一个顶点", "Point", required=True),
        ParamSpec("b", "顶点B", "三角形第二个顶点", "Point", required=True),
        ParamSpec("c", "顶点C", "三角形第三个顶点", "Point", required=True),
    ],
    outputs=[OutputSpec("circumcenter", "外心", "三角形ABC的外心", OutputFormat.POINT)],
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=[
        "三点不能共线",
        "三点必须互不相同"
    ],
    mathematical_definition="""
    设 A、B、C 为三角形顶点
    
    方法：求两边垂直平分线的交点
    - AB 的垂直平分线 l_ab
    - BC 的垂直平分线 l_bc
    - O = l_ab ∩ l_bc
    
    外接圆半径 R = |OA| = |OB| = |OC|
    """,
    example_usage="""
    >>> from lv import Graph
    >>> from lv.preset_blocks import create_circumcenter
    >>> g = Graph()
    >>> a = g.add_point(0, 0)
    >>> b = g.add_point(2, 0)
    >>> c = g.add_point(1, 2)
    >>> circumcenter = create_circumcenter(g, a, b, c)
    """,
    notes="钝角三角形的外心在三角形外部。"
))


def create_circumcenter(
    graph: 'Graph',
    a: 'Point',
    b: 'Point',
    c: 'Point'
) -> 'Point':
    """
    创建三角形外心构造。
    
    Args:
        graph: 约束图对象
        a: 第一个顶点
        b: 第二个顶点
        c: 第三个顶点
    
    Returns:
        Point: 外心
    
    Raises:
        ValueError: 三点共线时抛出
    """
    from ..core import SymbolicCoord
    
    # 检查三点是否共线
    det = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)
    if det.is_zero():
        raise ValueError("三点共线，无法构成三角形")
    
    # 使用垂直平分线交点法
    # AB 的中点
    mid_ab_x = (a.x + b.x) / SymbolicCoord.from_rational(2)
    mid_ab_y = (a.y + b.y) / SymbolicCoord.from_rational(2)
    
    # BC 的中点
    mid_bc_x = (b.x + c.x) / SymbolicCoord.from_rational(2)
    mid_bc_y = (b.y + c.y) / SymbolicCoord.from_rational(2)
    
    # AB 的垂直方向
    ab_perp_x = a.y - b.y
    ab_perp_y = b.x - a.x
    
    # BC 的垂直方向
    bc_perp_x = b.y - c.y
    bc_perp_y = c.x - b.x
    
    # 求解两条垂直平分线的交点
    D = ab_perp_y * bc_perp_x - ab_perp_x * bc_perp_y
    
    if D.is_zero():
        raise ValueError("计算外心时出现数值错误")
    
    dx = mid_bc_x - mid_ab_x
    dy = mid_bc_y - mid_ab_y
    
    t = (dy * bc_perp_x - dx * bc_perp_y) / D
    
    # 外心坐标
    o_x = mid_ab_x + t * ab_perp_x
    o_y = mid_ab_y + t * ab_perp_y
    
    return graph.add_point(o_x, o_y)


# ---- 内心构造 ----
register_preset(FuncBlockSpec(
    name="incenter",
    chinese_name="内心",
    category=PresetCategory.TRIANGLE,
    description="""
    计算三角形的内心（内切圆圆心）。
    
    给定三角形的三个顶点 A、B、C，计算内心 I。
    内心是三条角平分线的交点。
    
    数学性质:
    - 内心到三边的距离相等
    - 内心是三角形内切圆的圆心
    - 内心一定在三角形内部
    """,
    params=[
        ParamSpec("a", "顶点A", "三角形第一个顶点", "Point", required=True),
        ParamSpec("b", "顶点B", "三角形第二个顶点", "Point", required=True),
        ParamSpec("c", "顶点C", "三角形第三个顶点", "Point", required=True),
    ],
    outputs=[OutputSpec("incenter", "内心", "三角形ABC的内心", OutputFormat.POINT)],
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=[
        "三点不能共线",
        "三点必须互不相同"
    ],
    mathematical_definition="""
    设 A、B、C 为三角形顶点
    边长 a = |BC|, b = |CA|, c = |AB|
    
    内心 I 的坐标:
    I = (a·A + b·B + c·C) / (a + b + c)
    
    或用角平分线求交:
    - ∠A 的平分线 l_a
    - ∠B 的平分线 l_b
    - I = l_a ∩ l_b
    """,
    example_usage="""
    >>> from lv import Graph
    >>> from lv.preset_blocks import create_incenter
    >>> g = Graph()
    >>> a = g.add_point(0, 0)
    >>> b = g.add_point(2, 0)
    >>> c = g.add_point(1, 2)
    >>> incenter = create_incenter(g, a, b, c)
    """,
    notes="内心总是唯一确定的，且一定在三角形内部。"
))


def create_incenter(
    graph: 'Graph',
    a: 'Point',
    b: 'Point',
    c: 'Point'
) -> 'Point':
    """
    创建三角形内心构造。
    
    Args:
        graph: 约束图对象
        a: 第一个顶点
        b: 第二个顶点
        c: 第三个顶点
    
    Returns:
        Point: 内心
    
    Raises:
        ValueError: 三点共线时抛出
    """
    # 检查三点是否共线
    det = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)
    if det.is_zero():
        raise ValueError("三点共线，无法构成三角形")
    
    # 计算边长
    side_a = b.distance_to(c)
    side_b = c.distance_to(a)
    side_c = a.distance_to(b)
    
    # 内心公式：I = (a*A + b*B + c*C) / (a + b + c)
    perimeter = side_a + side_b + side_c
    
    # 计算内心坐标
    incenter_x = (side_a * a.x + side_b * b.x + side_c * c.x) / perimeter
    incenter_y = (side_a * a.y + side_b * b.y + side_c * c.y) / perimeter
    
    return graph.add_point(incenter_x, incenter_y)


# ---- 垂心构造 ----
register_preset(FuncBlockSpec(
    name="orthocenter",
    chinese_name="垂心",
    category=PresetCategory.TRIANGLE,
    description="""
    计算三角形的垂心。
    
    给定三角形的三个顶点 A、B、C，计算垂心 H。
    垂心是三条高的交点。
    
    数学性质:
    - 锐角三角形的垂心在三角形内部
    - 直角三角形的垂心在直角顶点
    - 钝角三角形的垂心在三角形外部
    """,
    params=[
        ParamSpec("a", "顶点A", "三角形第一个顶点", "Point", required=True),
        ParamSpec("b", "顶点B", "三角形第二个顶点", "Point", required=True),
        ParamSpec("c", "顶点C", "三角形第三个顶点", "Point", required=True),
    ],
    outputs=[OutputSpec("orthocenter", "垂心", "三角形ABC的垂心", OutputFormat.POINT)],
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=[
        "三点不能共线",
        "三点必须互不相同"
    ],
    mathematical_definition="""
    设 A、B、C 为三角形顶点
    
    方法：求两条高的交点
    - 高 h_a：过 A 且垂直于 BC
    - 高 h_b：过 B 且垂直于 AC
    - H = h_a ∩ h_b
    
    欧拉线：O（外心）、G（重心）、H（垂心）共线
    且 OG : GH = 1 : 2
    """,
    example_usage="""
    >>> from lv import Graph
    >>> from lv.preset_blocks import create_orthocenter
    >>> g = Graph()
    >>> a = g.add_point(0, 0)
    >>> b = g.add_point(2, 0)
    >>> c = g.add_point(1, 2)
    >>> orthocenter = create_orthocenter(g, a, b, c)
    """,
    notes="直角三角形的垂心就是直角顶点。"
))


def create_orthocenter(
    graph: 'Graph',
    a: 'Point',
    b: 'Point',
    c: 'Point'
) -> 'Point':
    """
    创建三角形垂心构造。
    
    Args:
        graph: 约束图对象
        a: 第一个顶点
        b: 第二个顶点
        c: 第三个顶点
    
    Returns:
        Point: 垂心
    
    Raises:
        ValueError: 三点共线时抛出
    """
    from ..core import SymbolicCoord
    
    # 检查三点是否共线
    det = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)
    if det.is_zero():
        raise ValueError("三点共线，无法构成三角形")
    
    # 高的计算：过 A 垂直于 BC，过 B 垂直于 AC
    # BC 的方向向量
    bc_dx = c.x - b.x
    bc_dy = c.y - b.y
    
    # AC 的方向向量
    ac_dx = c.x - a.x
    ac_dy = c.y - a.y
    
    # 高 h_a：过 A，方向垂直于 BC
    ha_dir_x = -bc_dy
    ha_dir_y = bc_dx
    
    # 高 h_b：过 B，方向垂直于 AC
    hb_dir_x = -ac_dy
    hb_dir_y = ac_dx
    
    # 求解两条高的交点
    D = ha_dir_y * hb_dir_x - ha_dir_x * hb_dir_y
    
    if D.is_zero():
        raise ValueError("计算垂心时出现数值错误")
    
    dx = b.x - a.x
    dy = b.y - a.y
    
    t = (dy * hb_dir_x - dx * hb_dir_y) / D
    
    # 垂心坐标
    h_x = a.x + t * ha_dir_x
    h_y = a.y + t * ha_dir_y
    
    return graph.add_point(h_x, h_y)


# ---- 旁心构造 ----
register_preset(FuncBlockSpec(
    name="excenter",
    chinese_name="旁心",
    category=PresetCategory.TRIANGLE,
    description="""
    计算三角形的旁心。
    
    给定三角形的三个顶点 A、B、C 和一个顶点索引，
    计算对应顶点的旁心（该顶点内角平分线与另外两角外角平分线的交点）。
    
    数学性质:
    - 三角形有三个旁心
    - 旁心到一边和另外两边的延长线距离相等
    - 旁心是旁切圆的圆心
    """,
    params=[
        ParamSpec("a", "顶点A", "三角形第一个顶点", "Point", required=True),
        ParamSpec("b", "顶点B", "三角形第二个顶点", "Point", required=True),
        ParamSpec("c", "顶点C", "三角形第三个顶点", "Point", required=True),
        ParamSpec("vertex_idx", "顶点索引", "0=A, 1=B, 2=C", "int", required=True),
    ],
    outputs=[OutputSpec("excenter", "旁心", "指定顶点对应的旁心", OutputFormat.POINT)],
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=[
        "三点不能共线",
        "顶点索引必须是 0、1 或 2"
    ],
    mathematical_definition="""
    设 A、B、C 为三角形顶点
    边长 a = |BC|, b = |CA|, c = |AB|
    
    A 对应的旁心:
    I_a = (-a·A + b·B + c·C) / (-a + b + c)
    
    B 对应的旁心:
    I_b = (a·A - b·B + c·C) / (a - b + c)
    
    C 对应的旁心:
    I_c = (a·A + b·B - c·C) / (a + b - c)
    """,
    example_usage="""
    >>> from lv import Graph
    >>> from lv.preset_blocks import create_excenter
    >>> g = Graph()
    >>> a = g.add_point(0, 0)
    >>> b = g.add_point(2, 0)
    >>> c = g.add_point(1, 2)
    >>> excenter_a = create_excenter(g, a, b, c, 0)  # A 对应的旁心
    """,
    notes="三角形有三个旁心，分别对应三个顶点。"
))


def create_excenter(
    graph: 'Graph',
    a: 'Point',
    b: 'Point',
    c: 'Point',
    vertex_idx: int
) -> 'Point':
    """
    创建三角形旁心构造。
    
    Args:
        graph: 约束图对象
        a: 第一个顶点
        b: 第二个顶点
        c: 第三个顶点
        vertex_idx: 顶点索引（0=A, 1=B, 2=C）
    
    Returns:
        Point: 旁心
    
    Raises:
        ValueError: 三点共线或索引无效时抛出
    """
    if vertex_idx not in [0, 1, 2]:
        raise ValueError("顶点索引必须是 0、1 或 2")
    
    # 检查三点是否共线
    det = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)
    if det.is_zero():
        raise ValueError("三点共线，无法构成三角形")
    
    # 计算边长
    side_a = b.distance_to(c)
    side_b = c.distance_to(a)
    side_c = a.distance_to(b)
    
    # 根据顶点索引计算旁心
    if vertex_idx == 0:  # A 对应的旁心
        # I_a = (-a*A + b*B + c*C) / (-a + b + c)
        denom = -side_a + side_b + side_c
        excenter_x = (-side_a * a.x + side_b * b.x + side_c * c.x) / denom
        excenter_y = (-side_a * a.y + side_b * b.y + side_c * c.y) / denom
    elif vertex_idx == 1:  # B 对应的旁心
        # I_b = (a*A - b*B + c*C) / (a - b + c)
        denom = side_a - side_b + side_c
        excenter_x = (side_a * a.x - side_b * b.x + side_c * c.x) / denom
        excenter_y = (side_a * a.y - side_b * b.y + side_c * c.y) / denom
    else:  # C 对应的旁心
        # I_c = (a*A + b*B - c*C) / (a + b - c)
        denom = side_a + side_b - side_c
        excenter_x = (side_a * a.x + side_b * b.x - side_c * c.x) / denom
        excenter_y = (side_a * a.y + side_b * b.y - side_c * c.y) / denom
    
    return graph.add_point(excenter_x, excenter_y)


# ---- 三角形面积 ----
register_preset(FuncBlockSpec(
    name="triangle_area",
    chinese_name="三角形面积",
    category=PresetCategory.TRIANGLE,
    description="""
    计算三角形面积（使用行列式方法）。
    
    给定三角形三个顶点 A、B、C，计算其面积。
    使用行列式公式 |det(AB, AC)| / 2，比 Heron 公式在符号计算中
    更简洁且避免嵌套平方根。
    
    数学性质:
    - 面积为非负数
    - 三点共线时面积为零
    - 面积具有坐标交换的不变性
    """,
    params=[
        ParamSpec("a", "顶点A", "三角形第一个顶点", "Point", required=True),
        ParamSpec("b", "顶点B", "三角形第二个顶点", "Point", required=True),
        ParamSpec("c", "顶点C", "三角形第三个顶点", "Point", required=True),
    ],
    outputs=[OutputSpec("area", "面积", "三角形ABC的面积", OutputFormat.SCALAR)],
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=["三点不能共线（共线时面积为零）"],
    mathematical_definition="""
    行列式 |B-A, C-A| = (B.x-A.x)*(C.y-A.y) - (B.y-A.y)*(C.x-A.x)
    
    面积 S = |det| / 2
    
    等价于 Heron 公式，但更适合符号计算。
    """,
    example_usage="""
    >>> from lv import Graph
    >>> from lv.preset_blocks import create_triangle_area
    >>> g = Graph()
    >>> a = g.add_point(0, 0)
    >>> b = g.add_point(4, 0)
    >>> c = g.add_point(0, 3)
    >>> area = create_triangle_area(g, a, b, c)
    >>> print(area)  # 6
    """,
    notes="返回 SymbolicCoord 类型。若三点共线，面积为零。"
))


def create_triangle_area(
    graph: 'Graph',
    a: 'Point',
    b: 'Point',
    c: 'Point'
) -> 'SymbolicCoord':
    """
    创建三角形面积计算。
    
    Args:
        graph: 约束图对象
        a: 第一个顶点
        b: 第二个顶点
        c: 第三个顶点
    
    Returns:
        SymbolicCoord: 三角形面积
    """
    from ..core import SymbolicCoord
    
    # 行列式 |B-A, C-A|
    ab_x = b.x - a.x
    ab_y = b.y - a.y
    ac_x = c.x - a.x
    ac_y = c.y - a.y
    
    det = ab_x * ac_y - ab_y * ac_x
    
    # 面积 = |det| / 2
    area = det.__abs__() / SymbolicCoord.from_rational(2)
    
    return area


# ---- 等边三角形构造 ----
register_preset(FuncBlockSpec(
    name="equilateral_triangle",
    chinese_name="等边三角形",
    category=PresetCategory.TRIANGLE,
    description="""
    由一条边构造等边三角形。
    
    给定等边三角形的一条边 AB，构造第三个顶点 C，
    使得三角形 ABC 为等边三角形。第三个顶点位于
    AB 的逆时针方向。
    
    数学性质:
    - 三边等长
    - 三个内角均为 60°
    - 外心、内心、重心、垂心四心合一
    """,
    params=[
        ParamSpec("a", "顶点A", "等边三角形的一个顶点", "Point", required=True),
        ParamSpec("b", "顶点B", "等边三角形的相邻顶点", "Point", required=True),
    ],
    outputs=[OutputSpec("c", "顶点C", "等边三角形的第三个顶点", OutputFormat.POINT)],
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=["A 和 B 必须不同（A ≠ B）"],
    mathematical_definition="""
    设 A = (x₁, y₁), B = (x₂, y₂)
    v = B - A = (dx, dy)
    
    将 v 逆时针旋转 60° 得到 AC 方向:
    cos60° = 1/2, sin60° = √3/2
    
    C.x = A.x + dx·cos60° - dy·sin60°
    C.y = A.y + dx·sin60° + dy·cos60°
    """,
    example_usage="""
    >>> from lv import Graph
    >>> from lv.preset_blocks import create_equilateral_triangle
    >>> g = Graph()
    >>> a = g.add_point(0, 0)
    >>> b = g.add_point(2, 0)
    >>> c = create_equilateral_triangle(g, a, b)
    >>> print(c)  # Point(1, √3) — 近似 (1, 1.732)
    """,
    notes="AB 线段另一侧还有一个等边三角形，可通过交换 A 和 B 获得。"
))


def create_equilateral_triangle(
    graph: 'Graph',
    a: 'Point',
    b: 'Point'
) -> 'Point':
    """
    创建等边三角形构造。
    
    Args:
        graph: 约束图对象
        a: 第一个顶点
        b: 第二个顶点
    
    Returns:
        Point: 第三个顶点
    
    Raises:
        ValueError: 两点重合时抛出
    """
    from ..core import SymbolicCoord
    
    # 验证两个顶点不同
    if a.x == b.x and a.y == b.y:
        raise ValueError("A 和 B 重合，无法构造等边三角形")
    
    # 边向量 v = B - A
    v_dx = b.x - a.x
    v_dy = b.y - a.y
    
    # cos(60°) = 1/2, sin(60°) = sqrt(3)/2
    one = SymbolicCoord.from_rational(1)
    two = SymbolicCoord.from_rational(2)
    three = SymbolicCoord.from_rational(3)
    half = SymbolicCoord.from_rational(1, 2)
    
    # sin60 = sqrt(3) / 2
    sqrt3 = three ** half
    sin60 = sqrt3 / two
    
    # 将 v 逆时针旋转 60°
    c_x = a.x + v_dx * half - v_dy * sin60
    c_y = a.y + v_dx * sin60 + v_dy * half
    
    return graph.add_point(c_x, c_y)


# ---- 九点圆构造 ----
register_preset(FuncBlockSpec(
    name="nine_point_circle",
    chinese_name="九点圆",
    category=PresetCategory.TRIANGLE,
    description="""
    构造三角形的九点圆。
    
    给定三角形的三个顶点，构造其九点圆。
    九点圆通过三角形的：
    - 三边中点
    - 三个垂足
    - 三个顶点到垂心连线的中点
    
    数学性质:
    - 九点圆圆心在外心和垂心连线的中点
    - 九点圆半径是外接圆半径的一半
    """,
    params=[
        ParamSpec("a", "顶点A", "三角形第一个顶点", "Point", required=True),
        ParamSpec("b", "顶点B", "三角形第二个顶点", "Point", required=True),
        ParamSpec("c", "顶点C", "三角形第三个顶点", "Point", required=True),
    ],
    outputs=[
        OutputSpec("center", "圆心", "九点圆的圆心", OutputFormat.POINT),
        OutputSpec("radius", "半径", "九点圆的半径", OutputFormat.SCALAR),
    ],
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=[
        "三点不能共线",
        "三点必须互不相同"
    ],
    mathematical_definition="""
    设 O 为外心，H 为垂心
    
    九点圆圆心 N = (O + H) / 2
    九点圆半径 r_9 = R / 2
    
    其中 R 是外接圆半径
    """,
    example_usage="""
    >>> from lv import Graph
    >>> from lv.preset_blocks import create_nine_point_circle
    >>> g = Graph()
    >>> a = g.add_point(0, 0)
    >>> b = g.add_point(2, 0)
    >>> c = g.add_point(1, 2)
    >>> center, radius = create_nine_point_circle(g, a, b, c)
    """,
    notes="九点圆是三角形的重要伴随圆之一。"
))


def create_nine_point_circle(
    graph: 'Graph',
    a: 'Point',
    b: 'Point',
    c: 'Point'
) -> Tuple['Point', 'SymbolicCoord']:
    """
    创建九点圆构造。
    
    Args:
        graph: 约束图对象
        a: 第一个顶点
        b: 第二个顶点
        c: 第三个顶点
    
    Returns:
        Tuple[Point, SymbolicCoord]: (九点圆圆心, 半径)
    
    Raises:
        ValueError: 三点共线时抛出
    """
    from ..core import SymbolicCoord
    
    # 获取外心和垂心
    circumcenter = create_circumcenter(graph, a, b, c)
    orthocenter = create_orthocenter(graph, a, b, c)
    
    # 九点圆圆心 = (外心 + 垂心) / 2
    nine_center_x = (circumcenter.x + orthocenter.x) / SymbolicCoord.from_rational(2)
    nine_center_y = (circumcenter.y + orthocenter.y) / SymbolicCoord.from_rational(2)
    nine_center = graph.add_point(nine_center_x, nine_center_y)
    
    # 九点圆半径 = 外接圆半径 / 2
    # 外接圆半径 = 外心到任一顶点的距离
    circumradius = circumcenter.distance_to(a)
    nine_radius = circumradius / SymbolicCoord.from_rational(2)
    
    return (nine_center, nine_radius)


# ============================================================
# 多边形构造
# ============================================================

# ---- 正方形构造 ----
register_preset(FuncBlockSpec(
    name="square",
    chinese_name="正方形",
    category=PresetCategory.POLYGON,
    description="""
    由相邻两顶点构造正方形。
    
    给定正方形的两个相邻顶点 A 和 B，计算其余两个顶点 C 和 D。
    正方形 ABCD 按逆时针方向排列。
    
    数学性质:
    - 四条边等长
    - 四个内角均为 90°
    - 对角线等长且互相垂直平分
    """,
    params=[
        ParamSpec("a", "顶点A", "正方形第一个顶点", "Point", required=True),
        ParamSpec("b", "顶点B", "正方形第二个顶点（与A相邻）", "Point", required=True),
    ],
    outputs=[
        OutputSpec("c", "顶点C", "正方形第三个顶点", OutputFormat.POINT),
        OutputSpec("d", "顶点D", "正方形第四个顶点", OutputFormat.POINT),
    ],
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=["A 和 B 必须不同（A ≠ B）"],
    mathematical_definition="""
    设 A = (x₁, y₁), B = (x₂, y₂)
    v = B - A = (dx, dy)
    
    其余顶点（逆时针方向）:
    C = B + (-dy, dx)
    D = A + (-dy, dx)
    
    即 AB 绕 A 逆时针旋转 90° 得到 AD，
    B 做相同平移得到 C。
    """,
    example_usage="""
    >>> from lv import Graph
    >>> from lv.preset_blocks import create_square
    >>> g = Graph()
    >>> a = g.add_point(0, 0)
    >>> b = g.add_point(2, 0)
    >>> c, d = create_square(g, a, b)
    >>> print(c)  # Point(2, 2)
    >>> print(d)  # Point(0, 2)
    """,
    notes="返回 (C, D) 两个点。对称的正方形可通过交换 A、B 顺序获得。"
))


def create_square(
    graph: 'Graph',
    a: 'Point',
    b: 'Point'
) -> Tuple['Point', 'Point']:
    """
    创建正方形构造。
    
    Args:
        graph: 约束图对象
        a: 第一个顶点
        b: 第二个顶点
    
    Returns:
        Tuple[Point, Point]: (C, D) 两个顶点
    
    Raises:
        ValueError: 两点重合时抛出
    """
    # 验证两个顶点不同
    if a.x == b.x and a.y == b.y:
        raise ValueError("A 和 B 重合，无法构造正方形")
    
    # 边向量 v = B - A
    v_dx = b.x - a.x
    v_dy = b.y - a.y
    
    # 垂直向量（逆时针旋转 90°）：(-v_dy, v_dx)
    perp_dx = -v_dy
    perp_dy = v_dx
    
    # C = B + perp
    c_x = b.x + perp_dx
    c_y = b.y + perp_dy
    c = graph.add_point(c_x, c_y)
    
    # D = A + perp
    d_x = a.x + perp_dx
    d_y = a.y + perp_dy
    d = graph.add_point(d_x, d_y)
    
    return (c, d)


# ---- 正多边形构造 ----
register_preset(FuncBlockSpec(
    name="regular_polygon",
    chinese_name="正多边形",
    category=PresetCategory.POLYGON,
    description="""
    构造正多边形。
    
    给定中心点 O、一个顶点 P 和边数 n，构造正 n 边形。
    
    数学性质:
    - 所有边等长
    - 所有内角相等
    - 内角 = (n-2) × 180° / n
    """,
    params=[
        ParamSpec("center", "中心", "正多边形的中心", "Point", required=True),
        ParamSpec("p", "顶点P", "正多边形的一个顶点", "Point", required=True),
        ParamSpec("n", "边数", "正多边形的边数", "int", required=True),
    ],
    outputs=[OutputSpec("vertices", "顶点", "正多边形的顶点列表", OutputFormat.COLLECTION)],
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=[
        "边数 n ≥ 3",
        "中心点 O 和顶点 P 不能重合"
    ],
    mathematical_definition="""
    设中心 O = (x₀, y₀)
    第一个顶点 P = (x₁, y₁)
    边数 n
    
    半径 r = |OP|
    起始角 θ₀ = atan2(y₁-y₀, x₁-x₀)
    
    第 k 个顶点坐标:
    x_k = x₀ + r·cos(θ₀ + 2πk/n)
    y_k = y₀ + r·sin(θ₀ + 2πk/n)
    
    k = 0, 1, 2, ..., n-1
    """,
    example_usage="""
    >>> from lv import Graph
    >>> from lv.preset_blocks import create_regular_polygon
    >>> g = Graph()
    >>> o = g.add_point(0, 0)
    >>> p = g.add_point(1, 0)
    >>> vertices = create_regular_polygon(g, o, p, 6)  # 正六边形
    """,
    notes="正多边形总是唯一确定的。"
))


def create_regular_polygon(
    graph: 'Graph',
    center: 'Point',
    p: 'Point',
    n: int
) -> List['Point']:
    """
    创建正多边形构造。
    
    Args:
        graph: 约束图对象
        center: 中心点
        p: 一个顶点
        n: 边数
    
    Returns:
        List[Point]: 正多边形的顶点列表
    
    Raises:
        ValueError: 边数小于3或中心与顶点重合时抛出
    """
    if n < 3:
        raise ValueError("正多边形的边数必须至少为 3")
    
    if center.x == p.x and center.y == p.y:
        raise ValueError("中心点和顶点不能重合")
    
    from ..core import SymbolicCoord
    
    # 计算半径平方
    radius_sq = center.distance_to(p)
    
    # 计算起始向量 (dx, dy)
    dx = p.x - center.x
    dy = p.y - center.y
    
    vertices = []
    
    # 对 n=3,4,6 使用精确的符号旋转
    if n == 4:
        # 正方形：旋转 90°，cos=0, sin=±1
        # 顶点 k: 旋转 k * 90°
        # k=0: (dx, dy)
        # k=1: (-dy, dx)
        # k=2: (-dx, -dy)
        # k=3: (dy, -dx)
        rotations = [
            (dx, dy),
            (-dy, dx),
            (-dx, -dy),
            (dy, -dx),
        ]
        for rx, ry in rotations:
            vx = center.x + rx
            vy = center.y + ry
            vertices.append(graph.add_point(vx, vy))
        return vertices
    
    if n == 3:
        # 正三角形：旋转 120° 和 240°
        # cos(120°) = -1/2, sin(120°) = sqrt(3)/2
        # 使用符号计算
        half = SymbolicCoord.from_rational(1, 2)
        sqrt3 = SymbolicCoord.from_rational(3) ** half
        
        # 旋转矩阵:
        # R(120°) = [[-1/2, -sqrt(3)/2], [sqrt(3)/2, -1/2]]
        # R(240°) = [[-1/2, sqrt(3)/2], [-sqrt(3)/2, -1/2]]
        
        # k=0: (dx, dy)
        vertices.append(graph.add_point(center.x + dx, center.y + dy))
        
        # k=1: R(120°) * (dx, dy)
        r1_dx = -half * dx - sqrt3 * half * dy
        r1_dy = sqrt3 * half * dx - half * dy
        vertices.append(graph.add_point(center.x + r1_dx, center.y + r1_dy))
        
        # k=2: R(240°) * (dx, dy)
        r2_dx = -half * dx + sqrt3 * half * dy
        r2_dy = -sqrt3 * half * dx - half * dy
        vertices.append(graph.add_point(center.x + r2_dx, center.y + r2_dy))
        
        return vertices
    
    if n == 6:
        # 正六边形：旋转 60° 的倍数
        # cos(60°) = 1/2, sin(60°) = sqrt(3)/2
        half = SymbolicCoord.from_rational(1, 2)
        sqrt3 = SymbolicCoord.from_rational(3) ** half
        
        # 预计算所有旋转后的向量
        # k=0: (dx, dy)
        # k=1: (1/2*dx - sqrt(3)/2*dy, sqrt(3)/2*dx + 1/2*dy)
        # k=2: (-1/2*dx - sqrt(3)/2*dy, sqrt(3)/2*dx - 1/2*dy)
        # k=3: (-dx, -dy)
        # k=4: (-1/2*dx + sqrt(3)/2*dy, -sqrt(3)/2*dx - 1/2*dy)
        # k=5: (1/2*dx + sqrt(3)/2*dy, -sqrt(3)/2*dx + 1/2*dy)
        rotations = [
            (dx, dy),
            (half * dx - sqrt3 * half * dy, sqrt3 * half * dx + half * dy),
            (-half * dx - sqrt3 * half * dy, sqrt3 * half * dx - half * dy),
            (-dx, -dy),
            (-half * dx + sqrt3 * half * dy, -sqrt3 * half * dx - half * dy),
            (half * dx + sqrt3 * half * dy, -sqrt3 * half * dx + half * dy),
        ]
        for rx, ry in rotations:
            vertices.append(graph.add_point(center.x + rx, center.y + ry))
        return vertices
    
    # 其他 n：回退到数值计算（保留原有行为）
    import math
    radius = float(radius_sq) ** 0.5
    start_angle = math.atan2(float(dy), float(dx))
    
    for k in range(n):
        angle = start_angle + 2 * math.pi * k / n
        x = float(center.x) + radius * math.cos(angle)
        y = float(center.y) + radius * math.sin(angle)
        vertices.append(graph.add_point(SymbolicCoord(x), SymbolicCoord(y)))
    
    return vertices


# ============================================================
# 模块导出
# ============================================================

__all__ = [
    # 基础构造
    "create_midpoint",
    "create_perpendicular_bisector",
    "create_angle_bisector",
    "create_line_intersection",
    "create_perpendicular_line",
    "create_parallel_line",
    "create_distance",
    "create_foot_of_perpendicular",
    
    # 圆相关
    "create_circle_center",
    "create_circle_by_center_radius",
    "create_circle_line_intersection",
    "create_tangent_line",
    
    # 三角形相关
    "create_centroid",
    "create_circumcenter",
    "create_incenter",
    "create_orthocenter",
    "create_excenter",
    "create_triangle_area",
    "create_equilateral_triangle",
    "create_nine_point_circle",
    
    # 多边形
    "create_square",
    "create_regular_polygon",
]
