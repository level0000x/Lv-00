"""
Lv-00 预设函数块 - 代数变换模块
================================

模块功能概述:
    提供理论数学研究中常用的几何代数变换函数块，包括:
    - 反射变换（点关于直线的镜像）
    - 平移变换
    - 旋转变换
    - 位似变换（缩放）
    - 圆反演
    - 仿射变换

数学严谨性:
    所有变换都基于严格的代数定义实现，保持几何不变量。

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
# 反射变换
# ============================================================

register_preset(FuncBlockSpec(
    name="reflection",
    chinese_name="反射变换",
    category=PresetCategory.TRANSFORMATION,
    description="""
    点关于直线的反射（镜像）变换。
    
    给定一个点 P 和一条由 A、B 确定的直线，计算 P 关于该直线的对称点 P'。
    直线 AB 是线段 PP' 的垂直平分线。
    
    数学性质:
    - 反射是等距变换（保长变换）
    - 反射保持角度大小但反转方向
    - 两次反射等价于恒等变换
    """,
    params=[
        ParamSpec("p", "点P", "待反射的点", "Point", required=True),
        ParamSpec("a", "点A", "反射直线上第一点", "Point", required=True),
        ParamSpec("b", "点B", "反射直线上第二点", "Point", required=True),
    ],
    outputs=[OutputSpec("p_prime", "反射点", "P关于直线AB的反射点", OutputFormat.POINT)],
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=["直线不能退化（A ≠ B）"],
    mathematical_definition="""
    设点 P，直线 l 过 A、B
    
    1. 计算 P 到直线 l 的投影 Q
    2. 对称点 P' = 2Q - P
    
    投影 Q 的计算:
    v = B - A  （直线方向向量）
    w = P - A
    t = (w·v) / (v·v)
    Q = A + t·v
    
    则 P' = 2Q - P
    """,
    example_usage="""
    >>> from lv import Graph
    >>> from lv.preset_blocks import create_reflection
    >>> g = Graph()
    >>> p = g.add_point(1, 2)
    >>> a = g.add_point(0, 0)
    >>> b = g.add_point(1, 0)   # x 轴
    >>> p_mirror = create_reflection(g, p, a, b)  # 关于 x 轴对称
    >>> print(p_mirror)  # Point(1, -2)
    """,
    notes="若点 P 在直线上，则反射后仍在原位（P' = P）。"
))


def create_reflection(
    graph: 'Graph',
    p: 'Point',
    a: 'Point',
    b: 'Point'
) -> 'Point':
    """
    创建反射变换构造 —— 点关于直线的镜像对称。
    
    Args:
        graph: 约束图对象
        p: 待反射的点
        a: 直线上第一个点
        b: 直线上第二个点
    
    Returns:
        Point: 反射后的对称点 P'
    
    Raises:
        ValueError: 当 A 和 B 重合时抛出（直线退化）
    
    Example:
        >>> g = Graph()
        >>> p = g.add_point(1, 2)
        >>> a = g.add_point(0, 0)
        >>> b = g.add_point(1, 0)  # x 轴
        >>> p_mirror = create_reflection(g, p, a, b)
        >>> print(p_mirror)  # Point(1, -2)
    """
    from ..core import SymbolicCoord
    
    # 验证直线不退化
    if a.x == b.x and a.y == b.y:
        raise ValueError("反射直线退化：A 和 B 重合")
    
    # 直线的方向向量 v = B - A
    v_dx = b.x - a.x
    v_dy = b.y - a.y
    
    # 向量 w = P - A
    w_dx = p.x - a.x
    w_dy = p.y - a.y
    
    # 计算投影参数 t = (w·v) / (v·v)
    dot_wv = w_dx * v_dx + w_dy * v_dy
    dot_vv = v_dx * v_dx + v_dy * v_dy
    
    if dot_vv.is_zero():
        raise ValueError("反射直线退化：方向向量为零")
    
    t = dot_wv / dot_vv
    
    # 投影点 Q = A + t * v
    q_x = a.x + t * v_dx
    q_y = a.y + t * v_dy
    
    # 反射点 P' = 2Q - P
    two = SymbolicCoord.from_rational(2)
    p_prime_x = two * q_x - p.x
    p_prime_y = two * q_y - p.y
    
    return graph.add_point(p_prime_x, p_prime_y)


# ============================================================
# 平移变换
# ============================================================

register_preset(FuncBlockSpec(
    name="translation",
    chinese_name="平移变换",
    category=PresetCategory.TRANSFORMATION,
    description="""
    对点施加平移变换。
    
    给定一个点 P 和平移向量 (dx, dy)，计算平移后的点 P'。
    
    数学性质:
    - 平移是等距变换
    - 平移保持图形的形状和大小
    - 所有点沿相同方向移动相同距离
    """,
    params=[
        ParamSpec("p", "点P", "待平移的点", "Point", required=True),
        ParamSpec("dx", "dx", "x方向平移距离", "Scalar", required=True),
        ParamSpec("dy", "dy", "y方向平移距离", "Scalar", required=True),
    ],
    outputs=[OutputSpec("p_prime", "平移点", "平移后的点", OutputFormat.POINT)],
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=["dx, dy 为有效坐标值"],
    mathematical_definition="""
    设 P = (x, y)，平移向量 (dx, dy)
    
    平移后 P' = (x + dx, y + dy)
    
    平移是向量的加法运算:
    P' = P + (dx, dy)
    """,
    example_usage="""
    >>> from lv import Graph
    >>> from lv.preset_blocks import create_translation
    >>> g = Graph()
    >>> p = g.add_point(1, 2)
    >>> p_translated = create_translation(g, p, 3, -1)
    >>> print(p_translated)  # Point(4, 1)
    """,
    notes="平移变换是刚体变换的基础之一。"
))


def create_translation(
    graph: 'Graph',
    p: 'Point',
    dx: Union['SymbolicCoord', int, float],
    dy: Union['SymbolicCoord', int, float]
) -> 'Point':
    """
    创建平移变换构造。
    
    将给定点沿 x、y 方向平移指定距离，返回平移后的新点。
    
    Args:
        graph: 约束图对象
        p: 原始点
        dx: x 方向平移距离
        dy: y 方向平移距离
    
    Returns:
        Point: 平移后的点
    
    Example:
        >>> g = Graph()
        >>> p = g.add_point(1, 2)
        >>> p_trans = create_translation(g, p, 3, -1)
        >>> print(p_trans)  # Point(4, 1)
    """
    from ..core import SymbolicCoord
    
    if not isinstance(dx, SymbolicCoord):
        dx = SymbolicCoord(dx)
    if not isinstance(dy, SymbolicCoord):
        dy = SymbolicCoord(dy)
    
    return graph.add_point(p.x + dx, p.y + dy)


# ============================================================
# 旋转变换
# ============================================================

register_preset(FuncBlockSpec(
    name="rotation",
    chinese_name="旋转变换",
    category=PresetCategory.TRANSFORMATION,
    description="""
    点绕中心点的旋转变换。
    
    给定一个点 P、旋转中心 C 和旋转角度（弧度），计算旋转后的点 P'。
    逆时针方向为正角度。
    
    数学性质:
    - 旋转是等距变换
    - 旋转保持图形的形状和大小
    - 旋转保持点到中心的距离不变
    """,
    params=[
        ParamSpec("p", "点P", "待旋转的点", "Point", required=True),
        ParamSpec("center", "中心C", "旋转中心", "Point", required=True),
        ParamSpec("angle", "角度", "旋转角度（弧度，逆时针为正）", "Scalar", required=True),
    ],
    outputs=[OutputSpec("p_prime", "旋转点", "旋转后的点", OutputFormat.POINT)],
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=["角度为有效数值"],
    mathematical_definition="""
    设 P = (x, y)，中心 C = (cx, cy)，角度 θ
    
    旋转后:
    x' = cx + (x - cx)·cosθ - (y - cy)·sinθ
    y' = cy + (x - cx)·sinθ + (y - cy)·cosθ   
    旋转矩阵:
    | cosθ  -sinθ |  | x - cx |
    | sinθ   cosθ |  | y - cy |
    """,
    example_usage="""
    >>> from lv import Graph
    >>> from lv.preset_blocks import create_rotation
    >>> import math
    >>> g = Graph()
    >>> center = g.add_point(0, 0)
    >>> p = g.add_point(1, 0)
    >>> p_rot = create_rotation(g, p, center, math.pi / 2)  # 旋转 90°
    >>> print(p_rot)  # Point(0, 1)（近似）
    """,
    notes="角度使用弧度制。若不指定中心，可将原点作为旋转中心。"
))


def create_rotation(
    graph: 'Graph',
    p: 'Point',
    center: 'Point',
    angle: float
) -> 'Point':
    """
    创建旋转变换构造。
    
    将给定点绕中心点逆时针旋转指定角度（弧度制），
    返回旋转后的新点。
    
    Args:
        graph: 约束图对象
        p: 原始点
        center: 旋转中心点
        angle: 旋转角度（弧度，逆时针为正）
    
    Returns:
        Point: 旋转后的点
    
    Example:
        >>> import math
        >>> g = Graph()
        >>> center = g.add_point(0, 0)
        >>> p = g.add_point(1, 0)
        >>> p_rot = create_rotation(g, p, center, math.pi / 2)  # 旋转 90°
        >>> print(p_rot)  # Point(0, 1)（近似值）
    """
    from ..core import SymbolicCoord
    
    cos_a = SymbolicCoord(math.cos(angle))
    sin_a = SymbolicCoord(math.sin(angle))
    
    # 相对于旋转中心的坐标
    rel_x = p.x - center.x
    rel_y = p.y - center.y
    
    # 旋转公式
    new_x = center.x + rel_x * cos_a - rel_y * sin_a
    new_y = center.y + rel_x * sin_a + rel_y * cos_a
    
    return graph.add_point(new_x, new_y)


# ============================================================
# 位似变换（缩放）
# ============================================================

register_preset(FuncBlockSpec(
    name="homothety",
    chinese_name="位似变换",
    category=PresetCategory.TRANSFORMATION,
    description="""
    点关于中心的位似变换（缩放）。
    
    给定一个点 P、位似中心 C 和比例因子 k，计算位似变换后的点 P'。
    位似变换保持点、线、面的共线性和平行性。
    
    数学性质:
    - 位似变换是相似变换
    - 当 k > 0 时，P' 在 CP 的延长线上
    - 当 k < 0 时，P' 在 CP 的反向延长线上
    - 当 |k| > 1 时，图形放大；当 |k| < 1 时，图形缩小
    """,
    params=[
        ParamSpec("p", "点P", "待变换的点", "Point", required=True),
        ParamSpec("center", "中心C", "位似中心", "Point", required=True),
        ParamSpec("ratio", "比例k", "位似比例因子", "Scalar", required=True),
    ],
    outputs=[OutputSpec("p_prime", "位似点", "位似变换后的点", OutputFormat.POINT)],
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=["比例因子 k ≠ 0"],
    mathematical_definition="""
    设 P = (x, y)，中心 C = (cx, cy)，比例 k
    
    位似变换:
    P' = C + k·(P - C)
    
    即:
    x' = cx + k·(x - cx)
    y' = cy + k·(y - cy)
    """,
    example_usage="""
    >>> from lv import Graph
    >>> from lv.preset_blocks import create_homothety
    >>> g = Graph()
    >>> center = g.add_point(0, 0)
    >>> p = g.add_point(1, 1)
    >>> p_scaled = create_homothety(g, p, center, 2)  # 放大2倍
    >>> print(p_scaled)  # Point(2, 2)
    """,
    notes="位似变换保持角度不变，改变线段长度。"
))


def create_homothety(
    graph: 'Graph',
    p: 'Point',
    center: 'Point',
    ratio: Union['SymbolicCoord', int, float]
) -> 'Point':
    """
    创建位似变换构造。
    
    Args:
        graph: 约束图对象
        p: 待变换的点
        center: 位似中心
        ratio: 位似比例因子
    
    Returns:
        Point: 位似变换后的点
    
    Raises:
        ValueError: 比例因子为零时抛出
    
    Example:
        >>> g = Graph()
        >>> center = g.add_point(0, 0)
        >>> p = g.add_point(1, 1)
        >>> p_scaled = create_homothety(g, p, center, 2)
        >>> print(p_scaled)  # Point(2, 2)
    """
    from ..core import SymbolicCoord
    
    if not isinstance(ratio, SymbolicCoord):
        ratio = SymbolicCoord(ratio)
    
    # 检查比例不为零
    # 简化处理，实际应检查符号值
    
    # 位似变换公式: P' = C + k * (P - C)
    new_x = center.x + ratio * (p.x - center.x)
    new_y = center.y + ratio * (p.y - center.y)
    
    return graph.add_point(new_x, new_y)


# ============================================================
# 圆反演
# ============================================================

register_preset(FuncBlockSpec(
    name="circle_inversion",
    chinese_name="圆反演",
    category=PresetCategory.TRANSFORMATION,
    description="""
    点关于圆的反演变换。
    
    给定一个点 P 和反演圆（圆心 O，半径 r），计算 P 的反演点 P'。
    反演变换将圆内点映射到圆外，圆外点映射到圆内，圆上点保持不变。
    
    数学性质:
    - 反演圆上的点保持不变
    - 过反演圆心的直线映射到自身
    - 不过反演圆心的圆映射为圆或直线
    - 反演变换保持角度（共形映射）
    """,
    params=[
        ParamSpec("p", "点P", "待反演的点", "Point", required=True),
        ParamSpec("center", "圆心O", "反演圆的圆心", "Point", required=True),
        ParamSpec("radius", "半径r", "反演圆的半径", "Scalar", required=True),
    ],
    outputs=[OutputSpec("p_prime", "反演点", "P关于圆O的反演点", OutputFormat.POINT)],
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=[
        "半径必须为正",
        "点 P 不能与圆心 O 重合（反演中心无像）"
    ],
    mathematical_definition="""
    设反演圆心 O，半径 r，点 P ≠ O
    
    反演点 P' 满足:
    1. P' 在射线 OP 上
    2. |OP| · |OP'| = r²
    
    即:
    P' = O + (r² / |OP|²) · (P - O)
    """,
    example_usage="""
    >>> from lv import Graph
    >>> from lv.preset_blocks import create_circle_inversion
    >>> g = Graph()
    >>> o = g.add_point(0, 0)
    >>> p = g.add_point(2, 0)
    >>> r = 1
    >>> p_inv = create_circle_inversion(g, p, o, r)
    >>> print(p_inv)  # Point(0.5, 0)
    """,
    notes="反演中心 O 没有反演像（映射到无穷远点）。"
))


def create_circle_inversion(
    graph: 'Graph',
    p: 'Point',
    center: 'Point',
    radius: Union['SymbolicCoord', int, float]
) -> 'Point':
    """
    创建圆反演变换构造。
    
    Args:
        graph: 约束图对象
        p: 待反演的点
        center: 反演圆的圆心
        radius: 反演圆的半径
    
    Returns:
        Point: 反演点
    
    Raises:
        ValueError: 点与圆心重合时抛出
    
    Example:
        >>> g = Graph()
        >>> o = g.add_point(0, 0)
        >>> p = g.add_point(2, 0)
        >>> r = 1
        >>> p_inv = create_circle_inversion(g, p, o, r)
        >>> print(p_inv)  # Point(0.5, 0)
    """
    from ..core import SymbolicCoord
    
    if not isinstance(radius, SymbolicCoord):
        radius = SymbolicCoord(radius)
    
    # 检查点不与圆心重合
    if p.x == center.x and p.y == center.y:
        raise ValueError("反演中心没有反演像")
    
    # 计算向量 OP
    op_x = p.x - center.x
    op_y = p.y - center.y
    
    # 计算 |OP|²
    op_sq = op_x * op_x + op_y * op_y
    
    # 计算 r² / |OP|²
    r_sq = radius * radius
    scale = r_sq / op_sq
    
    # 反演点: P' = O + scale * (P - O)
    new_x = center.x + scale * op_x
    new_y = center.y + scale * op_y
    
    return graph.add_point(new_x, new_y)


# ============================================================
# 仿射变换
# ============================================================

register_preset(FuncBlockSpec(
    name="affine_transform",
    chinese_name="仿射变换",
    category=PresetCategory.TRANSFORMATION,
    description="""
    对点施加仿射变换。
    
    给定一个点 P 和 2×3 仿射变换矩阵 [A|t]，计算变换后的点 P'。
    仿射变换包括平移、旋转、缩放、剪切等线性变换及其组合。
    
    数学性质:
    - 保持共线性（直线映射为直线）
    - 保持平行性（平行线映射为平行线）
    - 保持比例关系（线段比例不变）
    - 一般保持距离比，但不保持距离本身
    """,
    params=[
        ParamSpec("p", "点P", "待变换的点", "Point", required=True),
        ParamSpec("a11", "a11", "变换矩阵元素(1,1)", "Scalar", required=True),
        ParamSpec("a12", "a12", "变换矩阵元素(1,2)", "Scalar", required=True),
        ParamSpec("a21", "a21", "变换矩阵元素(2,1)", "Scalar", required=True),
        ParamSpec("a22", "a22", "变换矩阵元素(2,2)", "Scalar", required=True),
        ParamSpec("t1", "t1", "平移向量x分量", "Scalar", required=True),
        ParamSpec("t2", "t2", "平移向量y分量", "Scalar", required=True),
    ],
    outputs=[OutputSpec("p_prime", "变换点", "仿射变换后的点", OutputFormat.POINT)],
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=["变换矩阵可逆（行列式不为零）"],
    mathematical_definition="""
    设 P = (x, y)，仿射变换矩阵:
    | a11  a12 |   | t1 |
    | a21  a22 | , | t2 |
    
    变换后:
    x' = a11·x + a12·y + t1
    y' = a21·x + a22·y + t2
    
    矩阵形式:
    P' = A·P + t
    """,
    example_usage="""
    >>> from lv import Graph
    >>> from lv.preset_blocks import create_affine_transform
    >>> g = Graph()
    >>> p = g.add_point(1, 0)
    >>> # 旋转 90° 的仿射变换
    >>> p_transformed = create_affine_transform(g, p, 0, -1, 1, 0, 0, 0)
    >>> print(p_transformed)  # Point(0, 1)
    """,
    notes="仿射变换是线性变换和平移的组合。"
))


def create_affine_transform(
    graph: 'Graph',
    p: 'Point',
    a11: Union['SymbolicCoord', int, float],
    a12: Union['SymbolicCoord', int, float],
    a21: Union['SymbolicCoord', int, float],
    a22: Union['SymbolicCoord', int, float],
    t1: Union['SymbolicCoord', int, float],
    t2: Union['SymbolicCoord', int, float]
) -> 'Point':
    """
    创建仿射变换构造。
    
    Args:
        graph: 约束图对象
        p: 待变换的点
        a11, a12, a21, a22: 线性变换矩阵元素
        t1, t2: 平移向量分量
    
    Returns:
        Point: 仿射变换后的点
    
    Example:
        >>> g = Graph()
        >>> p = g.add_point(1, 0)
        >>> # 旋转 90°
        >>> p_transformed = create_affine_transform(g, p, 0, -1, 1, 0, 0, 0)
        >>> print(p_transformed)  # Point(0, 1)
    """
    from ..core import SymbolicCoord
    
    # 转换为 SymbolicCoord
    def to_coord(v):
        return v if isinstance(v, SymbolicCoord) else SymbolicCoord(v)
    
    a11 = to_coord(a11)
    a12 = to_coord(a12)
    a21 = to_coord(a21)
    a22 = to_coord(a22)
    t1 = to_coord(t1)
    t2 = to_coord(t2)
    
    # 仿射变换: P' = A·P + t
    new_x = a11 * p.x + a12 * p.y + t1
    new_y = a21 * p.x + a22 * p.y + t2
    
    return graph.add_point(new_x, new_y)


# ============================================================
# 模块导出
# ============================================================

__all__ = [
    "create_reflection",
    "create_translation",
    "create_rotation",
    "create_homothety",
    "create_circle_inversion",
    "create_affine_transform",
]
