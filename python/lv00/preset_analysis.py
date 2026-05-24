"""
Lv-00 预设函数块模块 - 分析与三角形构造

提供理论数学研究中常用的几何函数块预设，包括：
    - 圆相关构造（圆心、切线、交点等）
    - 三角形构造（重心、垂心、外心、内心、面积、等边三角形）

设计原则：
    1. 每个预设函数块都是确定性的（唯一解）
    2. 支持符号计算，保持精确性
    3. 提供完整的中文文档和数学描述

版本：3.2.0
作者：Lv-00 开发团队
"""

from __future__ import annotations
from typing import TYPE_CHECKING, Any, List, Optional, Tuple, Union

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
    get_preset_spec,
)


# ============================================================
# 预设函数块规格注册
# ============================================================


# ---- 圆相关构造 ----

# ============================================================
# 圆相关构造预设
# ============================================================

# ---- 圆心构造 ----
register_preset(FuncBlockSpec(
    name="circle_center",
    chinese_name="圆心",
    category=PresetFuncBlockCategory.CIRCLE,
    description="""
    由圆上三点确定圆心。
    
    给定圆上的三个不共线点 A、B、C，确定圆心 O。
    
    数学原理：
    - 圆心到三点的距离相等
    - 圆心是任意两边垂直平分线的交点
    """,
    input_count=3,
    output_count=1,
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
    圆心 O = (x₀, y₀) 满足：
    |O - A|² = |O - B|² = |O - C|²
    """,
    example_usage="""
    >>> from lv00 import Graph
    >>> from lv00.preset_func_blocks import create_circle_center
    >>> g = Graph()
    >>> a = g.add_point(1, 0)
    >>> b = g.add_point(0, 1)
    >>> c = g.add_point(-1, 0)
    >>> center = create_circle_center(g, a, b, c)
    >>> print(center)  # Point(0, 0)
    """,
    notes="三点共线时无解，因为不存在过共线三点的圆。"
))


# ---- 圆与直线交点 ----
register_preset(FuncBlockSpec(
    name="circle_line_intersection",
    chinese_name="圆与直线交点",
    category=PresetFuncBlockCategory.CIRCLE,
    description="""
    计算圆与直线的交点。
    
    给定圆心 O、半径 r 和直线 l，计算它们的交点。
    
    情况分析：
    - 相交：两个交点
    - 相切：一个交点
    - 相离：无交点
    """,
    input_count=4,
    output_count=2,
    determinism=DeterminismLevel.MULTIPLE_SOLUTIONS,
    preconditions=[
        "半径必须为正",
        "直线不能退化"
    ],
    mathematical_definition="""
    设圆：(x - x₀)² + (y - y₀)² = r²
    设直线：ax + by + c = 0
    
    联立方程求解：
    圆心到直线的距离 d = |ax₀ + by₀ + c| / √(a² + b²)
    
    - 若 d > r：无交点
    - 若 d = r：一个交点（相切）
    - 若 d < r：两个交点
    """,
    example_usage="""
    >>> from lv00 import Graph
    >>> from lv00.preset_func_blocks import create_circle_line_intersection
    >>> g = Graph()
    >>> o = g.add_point(0, 0)
    >>> r = 1  # 半径
    >>> a = g.add_point(-1, 0)
    >>> b = g.add_point(1, 0)
    >>> intersections = create_circle_line_intersection(g, o, r, a, b)
    """,
    notes="当有两个交点时，需要选择器确定返回哪个。"
))


# ---- 切线构造 ----
register_preset(FuncBlockSpec(
    name="tangent_line",
    chinese_name="切线",
    category=PresetFuncBlockCategory.CIRCLE,
    description="""
    过圆外一点作圆的切线。
    
    给定圆心 O、半径 r 和圆外一点 P，构造过 P 的切线。
    
    数学性质：
    - 切线与半径垂直
    - 从圆外一点可作两条切线
    """,
    input_count=3,
    output_count=2,
    determinism=DeterminismLevel.MULTIPLE_SOLUTIONS,
    preconditions=[
        "点 P 必须在圆外（|OP| > r）",
        "半径必须为正"
    ],
    mathematical_definition="""
    设圆心 O，半径 r，圆外点 P
    
    切点 T 满足：
    1. |OT| = r
    2. OT ⊥ PT
    3. T 在以 OP 为直径的圆上
    
    切点方程：
    以 OP 为直径的圆与原圆的交点即为切点
    """,
    example_usage="""
    >>> from lv00 import Graph
    >>> from lv00.preset_func_blocks import create_tangent_line
    >>> g = Graph()
    >>> o = g.add_point(0, 0)
    >>> p = g.add_point(2, 0)
    >>> r = 1
    >>> tangents = create_tangent_line(g, o, r, p)
    """,
    notes="圆上的点只能作一条切线，圆内的点不能作切线。"
))




# ============================================================
# 预设函数块规格注册
# ============================================================


# ---- 三角形构造 ----
# ============================================================
# 三角形构造预设
# ============================================================

# ---- 重心构造 ----
register_preset(FuncBlockSpec(
    name="centroid",
    chinese_name="重心",
    category=PresetFuncBlockCategory.TRIANGLE,
    description="""
    计算三角形的重心（质心）。
    
    给定三角形的三个顶点 A、B、C，计算重心 G。
    重心是三条中线的交点。
    
    数学性质：
    - 重心将每条中线分为 2:1 的比例
    - 重心坐标等于三顶点坐标的平均值
    - 重心是三角形的几何中心
    """,
    input_count=3,
    output_count=1,
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=[
        "三点不能共线",
        "三点必须互不相同"
    ],
    mathematical_definition="""
    设 A = (x₁, y₁), B = (x₂, y₂), C = (x₃, y₃)
    
    重心 G = ((x₁+x₂+x₃)/3, (y₁+y₂+y₃)/3)
    
    或用向量表示：
    G = (A + B + C) / 3
    """,
    example_usage="""
    >>> from lv00 import Graph
    >>> from lv00.preset_func_blocks import create_centroid
    >>> g = Graph()
    >>> a = g.add_point(0, 0)
    >>> b = g.add_point(2, 0)
    >>> c = g.add_point(1, 2)
    >>> centroid = create_centroid(g, a, b, c)
    >>> print(centroid)  # Point(1, 2/3)
    """,
    notes="重心总是唯一确定的，且一定在三角形内部。"
))


# ---- 垂心构造 ----
register_preset(FuncBlockSpec(
    name="orthocenter",
    chinese_name="垂心",
    category=PresetFuncBlockCategory.TRIANGLE,
    description="""
    计算三角形的垂心。
    
    给定三角形的三个顶点 A、B、C，计算垂心 H。
    垂心是三条高的交点。
    
    数学性质：
    - 锐角三角形的垂心在三角形内部
    - 直角三角形的垂心在直角顶点
    - 钝角三角形的垂心在三角形外部
    """,
    input_count=3,
    output_count=1,
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
    >>> from lv00 import Graph
    >>> from lv00.preset_func_blocks import create_orthocenter
    >>> g = Graph()
    >>> a = g.add_point(0, 0)
    >>> b = g.add_point(2, 0)
    >>> c = g.add_point(1, 2)
    >>> orthocenter = create_orthocenter(g, a, b, c)
    """,
    notes="直角三角形的垂心就是直角顶点。"
))


# ---- 外心构造 ----
register_preset(FuncBlockSpec(
    name="circumcenter",
    chinese_name="外心",
    category=PresetFuncBlockCategory.TRIANGLE,
    description="""
    计算三角形的外心（外接圆圆心）。
    
    给定三角形的三个顶点 A、B、C，计算外心 O。
    外心是三边垂直平分线的交点。
    
    数学性质：
    - 外心到三个顶点的距离相等
    - 外心是三角形外接圆的圆心
    - 直角三角形的外心在斜边中点
    """,
    input_count=3,
    output_count=1,
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
    >>> from lv00 import Graph
    >>> from lv00.preset_func_blocks import create_circumcenter
    >>> g = Graph()
    >>> a = g.add_point(0, 0)
    >>> b = g.add_point(2, 0)
    >>> c = g.add_point(1, 2)
    >>> circumcenter = create_circumcenter(g, a, b, c)
    """,
    notes="钝角三角形的外心在三角形外部。"
))


# ---- 内心构造 ----
register_preset(FuncBlockSpec(
    name="incenter",
    chinese_name="内心",
    category=PresetFuncBlockCategory.TRIANGLE,
    description="""
    计算三角形的内心（内切圆圆心）。
    
    给定三角形的三个顶点 A、B、C，计算内心 I。
    内心是三条角平分线的交点。
    
    数学性质：
    - 内心到三边的距离相等
    - 内心是三角形内切圆的圆心
    - 内心一定在三角形内部
    """,
    input_count=3,
    output_count=1,
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=[
        "三点不能共线",
        "三点必须互不相同"
    ],
    mathematical_definition="""
    设 A、B、C 为三角形顶点
    边长 a = |BC|, b = |CA|, c = |AB|
    
    内心 I 的坐标：
    I = (a·A + b·B + c·C) / (a + b + c)
    
    或用角平分线求交：
    - ∠A 的平分线 l_a
    - ∠B 的平分线 l_b
    - I = l_a ∩ l_b
    """,
    example_usage="""
    >>> from lv00 import Graph
    >>> from lv00.preset_func_blocks import create_incenter
    >>> g = Graph()
    >>> a = g.add_point(0, 0)
    >>> b = g.add_point(2, 0)
    >>> c = g.add_point(1, 2)
    >>> incenter = create_incenter(g, a, b, c)
    """,
    notes="内心总是唯一确定的，且一定在三角形内部。"
))




# ============================================================
# 预设函数块规格注册
# ============================================================



# ---- 三角形面积 ----
register_preset(FuncBlockSpec(
    name="area",
    chinese_name="三角形面积",
    category=PresetFuncBlockCategory.TRIANGLE,
    description="""
    计算三角形面积（使用 Heron 公式）。

    给定三角形三个顶点 A、B、C，计算三角形的面积。
    使用 Heron 公式通过三边长度精确计算，支持符号计算。

    数学性质：
    - 面积为非负数
    - 三点共线时面积为零
    - 面积具有坐标交换的不变性
    """,
    input_count=3,
    output_count=1,
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=[
        "三点不能共线（共线时面积为零）"
    ],
    mathematical_definition="""
    设三边：
    a = |BC|, b = |CA|, c = |AB|
    半周长 s = (a + b + c) / 2

    Heron 公式：
    面积 S = √(s(s-a)(s-b)(s-c))

    行列式形式（避免 Heron 的数值稳定性问题）：
    S = |det(AB, AC)| / 2
    """,
    example_usage="""
    >>> from lv00 import Graph
    >>> from lv00.preset_func_blocks import create_area
    >>> g = Graph()
    >>> a = g.add_point(0, 0)
    >>> b = g.add_point(4, 0)
    >>> c = g.add_point(0, 3)
    >>> area = create_area(g, a, b, c)
    >>> print(area)  # 6
    """,
    notes="返回 SymbolicCoord 类型。若三点共线，面积为零。"
))




# ============================================================
# 预设函数块规格注册
# ============================================================


# ---- 等边三角形构造 ----
register_preset(FuncBlockSpec(
    name="equilateral_triangle",
    chinese_name="等边三角形",
    category=PresetFuncBlockCategory.TRIANGLE,
    description="""
    由一条边构造等边三角形。

    给定等边三角形的一条边 AB，构造第三个顶点 C，
    使得三角形 ABC 为等边三角形。第三个顶点位于
    AB 的逆时针方向。

    数学性质：
    - 三边等长
    - 三个内角均为 60°
    - 外心、内心、重心、垂心四心合一
    """,
    input_count=2,
    output_count=1,
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=[
        "A 和 B 必须不同（A ≠ B）"
    ],
    mathematical_definition="""
    设 A = (x₁, y₁), B = (x₂, y₂)
    v = B - A = (dx, dy)

    将 v 逆时针旋转 60° 得到 AC 方向：
    cos60° = 1/2, sin60° = √3/2

    C.x = A.x + dx·cos60° - dy·sin60°
    C.y = A.y + dx·sin60° + dy·cos60°
    """,
    example_usage="""
    >>> from lv00 import Graph
    >>> from lv00.preset_func_blocks import create_equilateral_triangle
    >>> g = Graph()
    >>> a = g.add_point(0, 0)
    >>> b = g.add_point(2, 0)
    >>> c = create_equilateral_triangle(g, a, b)
    >>> print(c)  # Point(1, √3) — 近似 (1, 1.732)
    """,
    notes="AB 线段另一侧还有一个等边三角形，可通过交换 A 和 B 获得。"
))



# ============================================================
# 预设函数块规格注册
# ============================================================



# ---- 三角形重心别名 ----
register_preset(FuncBlockSpec(
    name="triangle_centroid",
    chinese_name="三角形重心",
    category=PresetFuncBlockCategory.TRIANGLE,
    description="""
    计算三角形的重心（质心）—— create_centroid 的别名。

    给定三角形的三个顶点 A、B、C，计算重心 G。
    重心是三条中线的交点。

    数学性质：
    - 重心将每条中线分为 2:1 的比例
    - 重心坐标等于三顶点坐标的平均值
    - 重心是三角形的几何中心

    此函数是 create_centroid 的语义化别名，三者行为完全相同。
    """,
    input_count=3,
    output_count=1,
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=[
        "三点不能共线",
        "三点必须互不相同"
    ],
    mathematical_definition="""
    设 A = (x₁, y₁), B = (x₂, y₂), C = (x₃, y₃)

    重心 G = ((x₁+x₂+x₃)/3, (y₁+y₂+y₃)/3)

    或用向量表示：
    G = (A + B + C) / 3
    """,
    example_usage="""
    >>> from lv00 import Graph
    >>> from lv00.preset_func_blocks import create_triangle_centroid
    >>> g = Graph()
    >>> a = g.add_point(0, 0)
    >>> b = g.add_point(2, 0)
    >>> c = g.add_point(1, 2)
    >>> centroid = create_triangle_centroid(g, a, b, c)
    >>> print(centroid)  # Point(1, 2/3)
    """,
    notes="与 create_centroid 完全等价，为增加可读性而提供的别名。"
))




# ============================================================
# 预设函数块规格注册
# ============================================================


# ============================================================
# 预设函数块创建函数
# ============================================================

def create_centroid(graph: 'Graph', a: 'Point', b: 'Point', c: 'Point') -> 'Point':
    """
    创建三角形重心构造。
    
    给定三角形的三个顶点，计算重心。
    
    参数：
        graph: 约束图对象
        a: 第一个顶点
        b: 第二个顶点
        c: 第三个顶点
        
    返回：
        Point: 重心对象
        
    异常：
        ValueError: 三点共线时抛出
        
    示例：
        >>> g = Graph()
        >>> a = g.add_point(0, 0)
        >>> b = g.add_point(2, 0)
        >>> c = g.add_point(1, 2)
        >>> centroid = create_centroid(g, a, b, c)
    """
    from .core import SymbolicCoord
    
    # 检查三点是否共线（使用行列式）
    # 行列式 = (b.x-a.x)*(c.y-a.y) - (b.y-a.y)*(c.x-a.x)
    det = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)
    
    if det.is_zero():
        raise ValueError("三点共线，无法构成三角形")
    
    # 计算重心坐标
    centroid_x = (a.x + b.x + c.x) / SymbolicCoord.from_rational(3)
    centroid_y = (a.y + b.y + c.y) / SymbolicCoord.from_rational(3)
    
    return graph.add_point(centroid_x, centroid_y)



def create_circumcenter(graph: 'Graph', a: 'Point', b: 'Point', c: 'Point') -> 'Point':
    """
    创建三角形外心构造。
    
    给定三角形的三个顶点，计算外心（外接圆圆心）。
    
    参数：
        graph: 约束图对象
        a: 第一个顶点
        b: 第二个顶点
        c: 第三个顶点
        
    返回：
        Point: 外心对象
        
    异常：
        ValueError: 三点共线时抛出
        
    示例：
        >>> g = Graph()
        >>> a = g.add_point(0, 0)
        >>> b = g.add_point(2, 0)
        >>> c = g.add_point(1, 2)
        >>> circumcenter = create_circumcenter(g, a, b, c)
    """
    from .core import SymbolicCoord
    
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
    # mid_ab + t * ab_perp = mid_bc + s * bc_perp
    # 这是一个线性方程组
    
    # 简化计算：使用行列式法
    # D = ab_perp_x * (-bc_perp_y) - ab_perp_y * (-bc_perp_x)
    # D = -ab_perp_x * bc_perp_y + ab_perp_y * bc_perp_x
    
    D = ab_perp_y * bc_perp_x - ab_perp_x * bc_perp_y
    
    if D.is_zero():
        raise ValueError("计算外心时出现数值错误")
    
    # dx = mid_bc_x - mid_ab_x
    # dy = mid_bc_y - mid_ab_y
    dx = mid_bc_x - mid_ab_x
    dy = mid_bc_y - mid_ab_y
    
    # t = (dx * (-bc_perp_y) - dy * (-bc_perp_x)) / D
    t = (dy * bc_perp_x - dx * bc_perp_y) / D
    
    # 外心坐标
    o_x = mid_ab_x + t * ab_perp_x
    o_y = mid_ab_y + t * ab_perp_y
    
    return graph.add_point(o_x, o_y)



def create_incenter(graph: 'Graph', a: 'Point', b: 'Point', c: 'Point') -> 'Point':
    """
    创建三角形内心构造。
    
    给定三角形的三个顶点，计算内心（内切圆圆心）。
    
    参数：
        graph: 约束图对象
        a: 第一个顶点
        b: 第二个顶点
        c: 第三个顶点
        
    返回：
        Point: 内心对象
        
    异常：
        ValueError: 三点共线时抛出
        
    示例：
        >>> g = Graph()
        >>> a = g.add_point(0, 0)
        >>> b = g.add_point(2, 0)
        >>> c = g.add_point(1, 2)
        >>> incenter = create_incenter(g, a, b, c)
    """
    from .core import SymbolicCoord
    
    # 检查三点是否共线
    det = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)
    
    if det.is_zero():
        raise ValueError("三点共线，无法构成三角形")
    
    # 计算边长
    # 修复：使用 side_a/side_b/side_c 避免覆盖函数参数 a/b/c
    # side_a = |BC|, side_b = |CA|, side_c = |AB|
    side_a = b.distance_to(c)
    side_b = c.distance_to(a)
    side_c = a.distance_to(b)
    
    # 内心公式：I = (a*A + b*B + c*C) / (a + b + c)
    # 内心是三条角平分线的交点，到三边距离相等
    perimeter = side_a + side_b + side_c
    
    # 计算内心坐标
    incenter_x = (side_a * a.x + side_b * b.x + side_c * c.x) / perimeter
    incenter_y = (side_a * a.y + side_b * b.y + side_c * c.y) / perimeter
    
    return graph.add_point(incenter_x, incenter_y)



def create_orthocenter(graph: 'Graph', a: 'Point', b: 'Point', c: 'Point') -> 'Point':
    """
    创建三角形垂心构造。
    
    给定三角形的三个顶点，计算垂心（三条高的交点）。
    
    参数：
        graph: 约束图对象
        a: 第一个顶点
        b: 第二个顶点
        c: 第三个顶点
        
    返回：
        Point: 垂心对象
        
    异常：
        ValueError: 三点共线时抛出
        
    示例：
        >>> g = Graph()
        >>> a = g.add_point(0, 0)
        >>> b = g.add_point(2, 0)
        >>> c = g.add_point(1, 2)
        >>> orthocenter = create_orthocenter(g, a, b, c)
    """
    from .core import SymbolicCoord
    
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
    # 垂直方向：(bc_dy, -bc_dx) 或 (-bc_dy, bc_dx)
    ha_dir_x = -bc_dy
    ha_dir_y = bc_dx
    
    # 高 h_b：过 B，方向垂直于 AC
    hb_dir_x = -ac_dy
    hb_dir_y = ac_dx
    
    # 求解两条高的交点
    # A + t * ha_dir = B + s * hb_dir
    
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



def create_area(
    graph: 'Graph',
    a: 'Point',
    b: 'Point',
    c: 'Point'
) -> 'SymbolicCoord':
    """
    计算三角形面积（使用行列式方法）。

    给定三角形的三个顶点，计算其面积。
    使用行列式公式 |det(AB, AC)| / 2，比 Heron 公式在符号计算中
    更简洁且避免嵌套平方根。

    参数：
        graph: 约束图对象（保留参数，用于接口统一）
        a: 第一个顶点
        b: 第二个顶点
        c: 第三个顶点

    返回：
        SymbolicCoord: 三角形面积（非负值；共线时为零）

    异常：
        ValueError: 当计算过程中出现问题时抛出

    示例：
        >>> g = Graph()
        >>> a = g.add_point(0, 0)
        >>> b = g.add_point(4, 0)
        >>> c = g.add_point(0, 3)
        >>> area = create_area(g, a, b, c)
        >>> print(area)  # 6
    """
    from .core import SymbolicCoord

    # 行列式 |B-A, C-A| = (B.x-A.x)*(C.y-A.y) - (B.y-A.y)*(C.x-A.x)
    ab_x = b.x - a.x
    ab_y = b.y - a.y
    ac_x = c.x - a.x
    ac_y = c.y - a.y

    det = ab_x * ac_y - ab_y * ac_x

    # 面积 = |det| / 2
    area = det.__abs__() / SymbolicCoord.from_rational(2)

    return area


def create_equilateral_triangle(
    graph: 'Graph',
    a: 'Point',
    b: 'Point'
) -> 'Point':
    """
    由一条边构造等边三角形。

    给定等边三角形的一条边 AB，计算第三个顶点 C，
    使得三角形 ABC 为等边三角形。C 位于 AB 的逆时针方向（上方）。

    参数：
        graph: 约束图对象
        a: 等边三角形的第一个顶点 A
        b: 等边三角形的第二个顶点 B

    返回：
        Point: 第三个顶点 C，使得 AB = BC = CA

    异常：
        ValueError: 当 A 和 B 重合时抛出

    示例：
        >>> g = Graph()
        >>> a = g.add_point(0, 0)
        >>> b = g.add_point(2, 0)
        >>> c = create_equilateral_triangle(g, a, b)
        >>> print(c)  # Point(1, sqrt(3)) — 近似 (1, 1.732)
    """
    from .core import SymbolicCoord

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

    # sin60 = sqrt(3) / 2 = (3^(1/2)) / 2
    sqrt3 = three ** half
    sin60 = sqrt3 / two

    # 将 v 逆时针旋转 60°
    # C = A + (v_dx * cos60 - v_dy * sin60, v_dx * sin60 + v_dy * cos60)
    c_x = a.x + v_dx * half - v_dy * sin60
    c_y = a.y + v_dx * sin60 + v_dy * half

    return graph.add_point(c_x, c_y)



def create_triangle_centroid(
    graph: 'Graph',
    a: 'Point',
    b: 'Point',
    c: 'Point'
) -> 'Point':
    """
    创建三角形重心构造 —— create_centroid 的别名。

    此函数是 create_centroid 的语义化别名，提供完全相同的功能。
    计算给定三角形三个顶点的重心（质心）。

    参数：
        graph: 约束图对象
        a: 第一个顶点
        b: 第二个顶点
        c: 第三个顶点

    返回：
        Point: 重心对象

    异常：
        ValueError: 三点共线时抛出

    示例：
        >>> g = Graph()
        >>> a = g.add_point(0, 0)
        >>> b = g.add_point(2, 0)
        >>> c = g.add_point(1, 2)
        >>> centroid = create_triangle_centroid(g, a, b, c)
        >>> print(centroid)  # Point(1, 2/3)
    """
    return create_centroid(graph, a, b, c)



# ============================================================
# 模块导出
# ============================================================

__all__ = [
    # 圆相关构造
    'create_circumcenter',
    # 三角形构造
    'create_centroid',
    'create_orthocenter',
    'create_incenter',
    'create_area',
    'create_equilateral_triangle',
    'create_triangle_centroid',
]
