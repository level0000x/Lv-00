"""
Lv-00 预设函数块模块 - 变换构造

提供理论数学研究中常用的几何函数块预设，包括：
    - 反射变换
    - 平移变换
    - 旋转变换

设计原则：
    1. 每个预设函数块都是确定性的（唯一解）
    2. 支持符号计算，保持精确性
    3. 提供完整的中文文档和数学描述

版本：3.3.0
作者：Lv-00 开发团队
"""

from __future__ import annotations
from typing import TYPE_CHECKING, List, Optional, Tuple, Union

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


# ---- 变换构造 ----


# ---- 反射变换 ----
register_preset(FuncBlockSpec(
    name="reflection",
    chinese_name="反射变换",
    category=PresetFuncBlockCategory.TRANSFORMATION,
    description="""
    点关于直线的反射（镜像）变换。

    给定一个点 P 和一条由 A、B 确定的直线，计算 P 关于该直线的对称点 P'。
    直线 AB 是线段 PP' 的垂直平分线。

    数学性质：
    - 反射是等距变换（保长变换）
    - 反射保持角度大小但反转方向
    - 两次反射等价于恒等变换
    """,
    input_count=3,
    output_count=1,
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=[
        "直线不能退化（A ≠ B）"
    ],
    mathematical_definition="""
    设点 P，直线 l 过 A、B

    1. 计算 P 到直线 l 的投影 Q
    2. 对称点 P' = 2Q - P

    投影 Q 的计算：
    v = B - A  （直线方向向量）
    w = P - A
    t = (w·v) / (v·v)
    Q = A + t·v

    则 P' = 2Q - P
    """,
    example_usage="""
    >>> from lv00 import Graph
    >>> from lv00.preset_func_blocks import create_reflection
    >>> g = Graph()
    >>> p = g.add_point(1, 2)
    >>> a = g.add_point(0, 0)
    >>> b = g.add_point(1, 0)   # x 轴
    >>> p_mirror = create_reflection(g, p, a, b)  # 关于 x 轴对称
    >>> print(p_mirror)  # Point(1, -2)
    """,
    notes="若点 P 在直线上，则反射后仍在原位（P' = P）。"
))


# ---- 平移变换 ----
register_preset(FuncBlockSpec(
    name="translation",
    chinese_name="平移变换",
    category=PresetFuncBlockCategory.TRANSFORMATION,
    description="""
    对点施加平移变换。

    给定一个点 P 和平移向量 (dx, dy)，计算平移后的点 P'。

    数学性质：
    - 平移是等距变换
    - 平移保持图形的形状和大小
    - 所有点沿相同方向移动相同距离
    """,
    input_count=3,
    output_count=1,
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=[
        "dx, dy 为有效坐标值"
    ],
    mathematical_definition="""
    设 P = (x, y)，平移向量 (dx, dy)

    平移后 P' = (x + dx, y + dy)

    平移是向量的加法运算：
    P' = P + (dx, dy)
    """,
    example_usage="""
    >>> from lv00 import Graph
    >>> from lv00.preset_func_blocks import create_translation
    >>> g = Graph()
    >>> p = g.add_point(1, 2)
    >>> p_translated = create_translation(g, p, 3, -1)
    >>> print(p_translated)  # Point(4, 1)
    """,
    notes="平移变换是刚体变换的基础之一。"
))


# ---- 旋转变换 ----
register_preset(FuncBlockSpec(
    name="rotation",
    chinese_name="旋转变换",
    category=PresetFuncBlockCategory.TRANSFORMATION,
    description="""
    点绕中心点的旋转变换。

    给定一个点 P、旋转中心 C 和旋转角度（弧度），计算旋转后的点 P'。
    逆时针方向为正角度。

    数学性质：
    - 旋转是等距变换
    - 旋转保持图形的形状和大小
    - 旋转保持点到中心的距离不变
    """,
    input_count=3,
    output_count=1,
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=[
        "角度为有效数值"
    ],
    mathematical_definition="""
    设 P = (x, y)，中心 C = (cx, cy)，角度 θ

    旋转后：
    x' = cx + (x - cx)·cosθ - (y - cy)·sinθ
    y' = cy + (x - cx)·sinθ + (y - cy)·cosθ

    旋转矩阵：
    | cosθ  -sinθ |  | x - cx |
    | sinθ   cosθ |  | y - cy |
    """,
    example_usage="""
    >>> from lv00 import Graph
    >>> from lv00.preset_func_blocks import create_rotation
    >>> g = Graph()
    >>> center = g.add_point(0, 0)
    >>> p = g.add_point(1, 0)
    >>> import math
    >>> p_rot = create_rotation(g, p, center, math.pi / 2)  # 旋转 90°
    >>> print(p_rot)  # Point(0, 1)（近似）
    """,
    notes="角度使用弧度制。若不指定中心，可将原点作为旋转中心。"
))


# ---- 两点距离 ----


# ============================================================
# 预设函数块规格注册
# ============================================================


# ============================================================
# 预设函数块创建函数
# ============================================================


def create_reflection(
    graph: 'Graph',
    p: 'Point',
    a: 'Point',
    b: 'Point'
) -> 'Point':
    """
    创建反射变换构造 —— 点关于直线的镜像对称。

    给定一个点 P 和由 A、B 确定的直线，计算 P 关于该直线的反射点 P'。
    直线 AB 是线段 PP' 的垂直平分线。

    参数：
        graph: 约束图对象
        p: 待反射的点
        a: 直线上第一个点
        b: 直线上第二个点

    返回：
        Point: 反射后的对称点 P'

    异常：
        ValueError: 当 A 和 B 重合时抛出（直线退化）

    示例：
        >>> g = Graph()
        >>> p = g.add_point(1, 2)
        >>> a = g.add_point(0, 0)
        >>> b = g.add_point(1, 0)  # x 轴
        >>> p_mirror = create_reflection(g, p, a, b)
        >>> print(p_mirror)  # Point(1, -2)
    """
    from .core import SymbolicCoord

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



def create_translation(
    graph: 'Graph',
    point: 'Point',
    dx: Union['SymbolicCoord', int, float],
    dy: Union['SymbolicCoord', int, float]
) -> 'Point':
    """
    创建平移变换构造。

    将给定点沿 x、y 方向平移指定距离，返回平移后的新点。

    参数：
        graph: 约束图对象
        point: 原始点
        dx: x 方向平移距离（SymbolicCoord、int 或 float）
        dy: y 方向平移距离（SymbolicCoord、int 或 float）

    返回：
        Point: 平移后的点

    示例：
        >>> g = Graph()
        >>> p = g.add_point(1, 2)
        >>> p_trans = create_translation(g, p, 3, -1)
        >>> print(p_trans)  # Point(4, 1)
    """
    from .core import SymbolicCoord

    if not isinstance(dx, SymbolicCoord):
        dx = SymbolicCoord(dx)
    if not isinstance(dy, SymbolicCoord):
        dy = SymbolicCoord(dy)

    return graph.add_point(point.x + dx, point.y + dy)



def create_rotation(
    graph: 'Graph',
    point: 'Point',
    center: 'Point',
    angle: float
) -> 'Point':
    """
    创建旋转变换构造。

    将给定点绕中心点逆时针旋转指定角度（弧度制），
    返回旋转后的新点。

    参数：
        graph: 约束图对象
        point: 原始点
        center: 旋转中心点
        angle: 旋转角度（弧度，逆时针为正）

    返回：
        Point: 旋转后的点

    示例：
        >>> import math
        >>> g = Graph()
        >>> center = g.add_point(0, 0)
        >>> p = g.add_point(1, 0)
        >>> p_rot = create_rotation(g, p, center, math.pi / 2)  # 旋转 90°
        >>> print(p_rot)  # Point(0, 1)（近似值）
    """
    from .core import SymbolicCoord

    cos_a = SymbolicCoord(math.cos(angle))
    sin_a = SymbolicCoord(math.sin(angle))

    # 相对于旋转中心的坐标
    rel_x = point.x - center.x
    rel_y = point.y - center.y

    # 旋转公式
    new_x = center.x + rel_x * cos_a - rel_y * sin_a
    new_y = center.y + rel_x * sin_a + rel_y * cos_a

    return graph.add_point(new_x, new_y)



# ============================================================
# 模块导出
# ============================================================

__all__ = [
    'create_reflection',
    'create_translation',
    'create_rotation',
]
