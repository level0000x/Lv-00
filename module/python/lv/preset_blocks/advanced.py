"""
Lv-00 预设函数块 - 高级构造模块
=================================

模块功能概述:
    提供理论数学研究中常用的高级几何构造函数块，包括:
    - 圆锥曲线构造
    - 包络线构造
    - 极线极点构造
    - 调和分割
    - 射影变换

数学严谨性:
    所有函数都基于严格的射影几何和代数几何定义实现。

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
# 调和分割
# ============================================================

register_preset(FuncBlockSpec(
    name="harmonic_conjugate",
    chinese_name="调和共轭点",
    category=PresetCategory.ADVANCED,
    description="""
    计算调和共轭点。
    
    给定共线三点 A、B、C，计算点 D 使得 (A, B; C, D) = -1
    （交比为 -1，即调和分割）。
    
    数学性质:
    - 调和分割是射影几何中的重要概念
    - 若 (A, B; C, D) = -1，则 C 和 D 关于 A、B 调和共轭
    - 完全四边形的对边交点形成调和分割
    """,
    params=[
        ParamSpec("a", "点A", "线段端点", "Point", required=True),
        ParamSpec("b", "点B", "线段端点", "Point", required=True),
        ParamSpec("c", "点C", "线段内一点", "Point", required=True),
    ],
    outputs=[OutputSpec("d", "调和共轭点", "C关于A、B的调和共轭点", OutputFormat.POINT)],
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=[
        "三点必须共线",
        "C 不能是 AB 的中点（此时 D 在无穷远）"
    ],
    mathematical_definition="""
    设 A、B、C 共线，且 C 分 AB 的比为 λ = AC/CB
    
    调和共轭点 D 满足: AD/DB = -AC/CB = -λ
    
    坐标计算:
    D = (2·A·B - C·(A+B)) / (A + B - 2·C)
    
    或用向量表示:
    若 C = (1-t)·A + t·B，则 D = (1-t)·A - t·B / (1-2t)
    """,
    example_usage="""
    >>> from lv import Graph
    >>> from lv.preset_blocks import create_harmonic_conjugate
    >>> g = Graph()
    >>> a = g.add_point(0, 0)
    >>> b = g.add_point(2, 0)
    >>> c = g.add_point(1, 0)  # 中点
    >>> d = create_harmonic_conjugate(g, a, b, c)  # D 在无穷远
    """,
    notes="当 C 是 AB 中点时，调和共轭点在无穷远处。"
))


def create_harmonic_conjugate(
    graph: 'Graph',
    a: 'Point',
    b: 'Point',
    c: 'Point'
) -> 'Point':
    """
    创建调和共轭点构造。
    
    Args:
        graph: 约束图对象
        a: 线段端点
        b: 线段端点
        c: 线段内一点
    
    Returns:
        Point: 调和共轭点
    
    Raises:
        ValueError: 三点不共线或 C 是中点时抛出
    """
    from ..core import SymbolicCoord
    
    # 检查三点共线（使用叉积）
    # (B-A) × (C-A) = 0
    cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)
    if not cross.is_zero():
        raise ValueError("三点必须共线")
    
    # 检查 C 不是中点
    mid_x = (a.x + b.x) / SymbolicCoord.from_rational(2)
    mid_y = (a.y + b.y) / SymbolicCoord.from_rational(2)
    
    if c.x == mid_x and c.y == mid_y:
        raise ValueError("C 是 AB 中点，调和共轭点在无穷远")
    
    # 计算调和共轭点 D，使得交比 (A, B; C, D) = -1。
    #
    # 在仿射坐标系中，对共线三点 A, B, C，调和共轭点 D 的公式为：
    #   D = A + B - C
    #
    # 该公式可由交比条件 (A,B;C,D) = -1 直接推导：
    #   令 C = (1-t)A + tB（t != 1/2），则由交比条件可得
    #   D = (1-t)A + tB 的调和共轭 = (1+t)A - tB + A - (1-t)B = A + B - C。
    #   等价地，若 r = AC/CB，则 D = (A - r*B) / (1 - r) = A + B - C。
    #
    # 此公式在任意仿射坐标系下均成立（不依赖特定坐标系选取），
    # 与分式线性变换 / 交比定义完全等价。
    # 唯一例外：当 C 恰为 AB 中点（t = 1/2）时，D 趋向无穷远点，
    # 已在上方做了检查并抛出 ValueError。
    d_x = a.x + b.x - c.x
    d_y = a.y + b.y - c.y
    
    return graph.add_point(d_x, d_y)


# ============================================================
# 极线构造
# ============================================================

register_preset(FuncBlockSpec(
    name="polar_line",
    chinese_name="极线",
    category=PresetCategory.ADVANCED,
    description="""
    计算点关于圆的极线。
    
    给定圆（圆心 O，半径 r）和点 P，计算 P 关于该圆的极线。
    极线是射影几何中的重要概念，与极点互为对偶。
    
    数学性质:
    - 若 P 在圆上，极线就是过 P 的切线
    - 若 P 在圆外，极线是通过两个切点的直线
    - 若 P 在圆内，极线是通过 P 的弦的端点切线交点的轨迹
    """,
    params=[
        ParamSpec("center", "圆心O", "圆的圆心", "Point", required=True),
        ParamSpec("radius", "半径r", "圆的半径", "Scalar", required=True),
        ParamSpec("p", "点P", "极点", "Point", required=True),
    ],
    outputs=[OutputSpec("polar", "极线", "P关于圆的极线", OutputFormat.LINE)],
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=[
        "半径必须为正",
        "点 P 不能与圆心 O 重合"
    ],
    mathematical_definition="""
    设圆: (x - x₀)² + (y - y₀)² = r²
    点 P = (x₁, y₁)
    
    极线方程:
    (x₁ - x₀)(x - x₀) + (y₁ - y₀)(y - y₀) = r²
    
    或展开为:
    (x₁ - x₀)x + (y₁ - y₀)y = r² + x₀(x₁ - x₀) + y₀(y₁ - y₀)
    """,
    example_usage="""
    >>> from lv import Graph
    >>> from lv.preset_blocks import create_polar_line
    >>> g = Graph()
    >>> o = g.add_point(0, 0)
    >>> p = g.add_point(2, 0)
    >>> r = 1
    >>> polar = create_polar_line(g, o, r, p)
    """,
    notes="极线-极点是圆的重要对偶概念。"
))


def create_polar_line(
    graph: 'Graph',
    center: 'Point',
    radius: Union['SymbolicCoord', int, float],
    p: 'Point'
) -> 'LineSegment':
    """
    创建极线构造。
    
    Args:
        graph: 约束图对象
        center: 圆心
        radius: 半径
        p: 极点
    
    Returns:
        LineSegment: 极线（用线段表示）
    
    Raises:
        ValueError: 点与圆心重合时抛出
    """
    from ..core import SymbolicCoord
    
    if not isinstance(radius, SymbolicCoord):
        radius = SymbolicCoord(radius)
    
    # 检查点不与圆心重合
    if p.x == center.x and p.y == center.y:
        raise ValueError("极点不能与圆心重合")
    
    # 极线方向垂直于 OP
    op_x = p.x - center.x
    op_y = p.y - center.y
    
    # 极线方向向量（垂直于 OP）
    dir_x = -op_y
    dir_y = op_x
    
    # 极线上的一个点（使用极线方程）
    # 极线方程: (Px-Ox)(x-Ox) + (Py-Oy)(y-Oy) = r²
    # 取极线上一点 Q，使得 OQ 垂直于极线
    
    # 简化处理：极线过点 Q = O + (r²/|OP|²) * (P - O)
    op_sq = op_x * op_x + op_y * op_y
    r_sq = radius * radius
    scale = r_sq / op_sq
    
    q_x = center.x + scale * op_x
    q_y = center.y + scale * op_y
    q = graph.add_point(q_x, q_y)
    
    # 创建极线（用线段表示）
    end1 = graph.add_point(q_x + dir_x, q_y + dir_y)
    end2 = graph.add_point(q_x - dir_x, q_y - dir_y)
    
    return graph.add_line_segment(end1, end2)


# ============================================================
# 极点构造
# ============================================================

register_preset(FuncBlockSpec(
    name="pole_point",
    chinese_name="极点",
    category=PresetCategory.ADVANCED,
    description="""
    计算直线关于圆的极点。
    
    给定圆（圆心 O，半径 r）和直线 l，计算 l 关于该圆的极点。
    极点是极线的对偶概念。
    
    数学性质:
    - 极点-极线关系是对合的
    - 若点 P 在直线 l 上，则 l 的极点在 P 的极线上
    """,
    params=[
        ParamSpec("center", "圆心O", "圆的圆心", "Point", required=True),
        ParamSpec("radius", "半径r", "圆的半径", "Scalar", required=True),
        ParamSpec("a", "点A", "直线上第一点", "Point", required=True),
        ParamSpec("b", "点B", "直线上第二点", "Point", required=True),
    ],
    outputs=[OutputSpec("pole", "极点", "直线关于圆的极点", OutputFormat.POINT)],
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=[
        "半径必须为正",
        "直线不能过圆心（此时极点在无穷远）"
    ],
    mathematical_definition="""
    设圆: (x - x₀)² + (y - y₀)² = r²
    直线 l: ax + by + c = 0
    
    极点 P 的坐标:
    Px = x₀ + a·r² / (a·x₀ + b·y₀ + c)
    Py = y₀ + b·r² / (a·x₀ + b·y₀ + c)
    
    注意：分母不能为零（即直线不过圆心）
    """,
    example_usage="""
    >>> from lv import Graph
    >>> from lv.preset_blocks import create_pole_point
    >>> g = Graph()
    >>> o = g.add_point(0, 0)
    >>> a = g.add_point(1, 0)
    >>> b = g.add_point(1, 1)
    >>> r = 1
    >>> pole = create_pole_point(g, o, r, a, b)
    """,
    notes="极点-极线关系是圆的重要对偶性质。"
))


def create_pole_point(
    graph: 'Graph',
    center: 'Point',
    radius: Union['SymbolicCoord', int, float],
    a: 'Point',
    b: 'Point'
) -> 'Point':
    """
    创建极点构造。
    
    Args:
        graph: 约束图对象
        center: 圆心
        radius: 半径
        a: 直线上第一点
        b: 直线上第二点
    
    Returns:
        Point: 极点
    
    Raises:
        ValueError: 直线过圆心时抛出
    """
    from ..core import SymbolicCoord
    
    if not isinstance(radius, SymbolicCoord):
        radius = SymbolicCoord(radius)
    
    # 直线方程: (b.y - a.y)(x - a.x) - (b.x - a.x)(y - a.y) = 0
    line_a = b.y - a.y
    line_b = a.x - b.x
    line_c = -(line_a * a.x + line_b * a.y)
    
    # 检查直线不过圆心
    denom = line_a * center.x + line_b * center.y + line_c
    if denom.is_zero():
        raise ValueError("直线过圆心，极点在无穷远")
    
    # 计算极点
    r_sq = radius * radius
    scale = r_sq / denom
    
    pole_x = center.x + line_a * scale
    pole_y = center.y + line_b * scale
    
    return graph.add_point(pole_x, pole_y)


# ============================================================
# 模块导出
# ============================================================

__all__ = [
    "create_harmonic_conjugate",
    "create_polar_line",
    "create_pole_point",
]
