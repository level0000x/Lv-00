"""
Lv-00 预设函数块模块 - 多边形构造

提供理论数学研究中常用的几何函数块预设，包括：
    - 正多边形构造
    - 正方形构造

设计原则：
    1. 每个预设函数块都是确定性的（唯一解）
    2. 支持符号计算，保持精确性
    3. 提供完整的中文文档和数学描述

版本：3.3.0
作者：Lv-00 开发团队
"""

from __future__ import annotations
from typing import TYPE_CHECKING, List, Optional, Tuple

# 类型检查时导入，避免运行时循环依赖
if TYPE_CHECKING:
    from .core import Graph, Point, LineSegment, SymbolicCoord
    from .func_block import FuncBlock, SolutionSelector

# 从基础模块导入枚举、注册表和辅助函数
from .preset_basic import (
    PresetFuncBlockCategory,
    DeterminismLevel,
    FuncBlockSpec,
    register_preset,
)


# ============================================================
# 预设函数块规格注册
# ============================================================


# ---- 多边形构造 ----
# ============================================================
# 多边形构造预设
# ============================================================

# ---- 正多边形构造 ----
register_preset(FuncBlockSpec(
    name="regular_polygon",
    chinese_name="正多边形",
    category=PresetFuncBlockCategory.POLYGON,
    description="""
    构造正多边形。
    
    给定中心点 O、一个顶点 P 和边数 n，构造正 n 边形。
    
    数学性质：
    - 所有边等长
    - 所有内角相等
    - 内角 = (n-2) × 180° / n
    """,
    input_count=3,
    output_count=1,  # 返回顶点列表
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
    
    第 k 个顶点坐标：
    x_k = x₀ + r·cos(θ₀ + 2πk/n)
    y_k = y₀ + r·sin(θ₀ + 2πk/n)
    
    k = 0, 1, 2, ..., n-1
    """,
    example_usage="""
    >>> from lv00 import Graph
    >>> from lv00.preset_func_blocks import create_regular_polygon
    >>> g = Graph()
    >>> o = g.add_point(0, 0)
    >>> p = g.add_point(1, 0)
    >>> vertices = create_regular_polygon(g, o, p, 6)  # 正六边形
    """,
    notes="正多边形总是唯一确定的。"
))


# ============================================================
# 预设函数块规格注册
# ============================================================


# ---- 正方形构造 ----
register_preset(FuncBlockSpec(
    name="square",
    chinese_name="正方形",
    category=PresetFuncBlockCategory.POLYGON,
    description="""
    由相邻两顶点构造正方形。

    给定正方形的两个相邻顶点 A 和 B，计算其余两个顶点 C 和 D。
    正方形 ABCD 按逆时针方向排列。

    数学性质：
    - 四条边等长
    - 四个内角均为 90°
    - 对角线等长且互相垂直平分
    """,
    input_count=2,
    output_count=2,
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=[
        "A 和 B 必须不同（A ≠ B）"
    ],
    mathematical_definition="""
    设 A = (x₁, y₁), B = (x₂, y₂)
    v = B - A = (dx, dy)

    其余顶点（逆时针方向）：
    C = B + (-dy, dx)
    D = A + (-dy, dx)

    即 AB 绕 A 逆时针旋转 90° 得到 AD，
    B 做相同平移得到 C。
    """,
    example_usage="""
    >>> from lv00 import Graph
    >>> from lv00.preset_func_blocks import create_square
    >>> g = Graph()
    >>> a = g.add_point(0, 0)
    >>> b = g.add_point(2, 0)
    >>> c, d = create_square(g, a, b)
    >>> print(c)  # Point(2, 2)
    >>> print(d)  # Point(0, 2)
    """,
    notes="返回 (C, D) 两个点。对称的正方形可通过交换 A、B 顺序获得。"
))




# ============================================================
# 预设函数块规格注册
# ============================================================


# ============================================================
# 预设函数块创建函数
# ============================================================


def create_square(
    graph: 'Graph',
    a: 'Point',
    b: 'Point'
) -> Tuple['Point', 'Point']:
    """
    由相邻两顶点构造正方形（逆时针方向）。

    给定正方形的两个相邻顶点 A 和 B，计算其余两个顶点 C 和 D。
    顶点按 A -> B -> C -> D 逆时针排列。

    参数：
        graph: 约束图对象
        a: 正方形第一个顶点 A
        b: 正方形第二个顶点 B（与 A 相邻）

    返回：
        Tuple[Point, Point]: (C, D) 两个顶点，其中 C 与 B 相邻，
                             D 与 A 相邻，按逆时针方向排列

    异常：
        ValueError: 当 A 和 B 重合时抛出

    示例：
        >>> g = Graph()
        >>> a = g.add_point(0, 0)
        >>> b = g.add_point(2, 0)
        >>> c, d = create_square(g, a, b)
        >>> print(c)  # Point(2, 2)
        >>> print(d)  # Point(0, 2)
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




# ============================================================
# 模块导出
# ============================================================

__all__ = [
    'create_square',
]
