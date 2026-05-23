"""
Lv-00 预设函数块模块

提供理论数学研究中常用的几何函数块预设，包括：
    - 基础几何构造：中点、垂直平分线、角平分线等
    - 圆相关构造：圆心、切线、交点等
    - 三角形构造：重心、垂心、外心、内心等
    - 多边形构造：正多边形、内接多边形等
    - 高级构造：相似变换、仿射变换等

设计原则：
    1. 每个预设函数块都是确定性的（唯一解）
    2. 支持符号计算，保持精确性
    3. 提供完整的中文文档和数学描述
    4. 遵循局部最优解原则

版本：3.0.2
作者：Lv-00 开发团队
"""

from __future__ import annotations
from typing import TYPE_CHECKING, Any, Callable, Dict, List, Optional, Tuple, Union
from enum import Enum, auto
from dataclasses import dataclass, field
import math

# 类型检查时导入，避免运行时循环依赖
if TYPE_CHECKING:
    from .core import Graph, Point, LineSegment, SymbolicCoord
    from .func_block import FuncBlock, SolutionSelector


# ============================================================
# 枚举和常量定义
# ============================================================

class PresetFuncBlockCategory(Enum):
    """
    预设函数块分类枚举。
    
    按照几何构造的类型对预设函数块进行分类，
    便于用户查找和管理。
    
    枚举值：
        BASIC: 基础几何构造（点、线、距离等）
        CIRCLE: 圆相关构造
        TRIANGLE: 三角形构造
        POLYGON: 多边形构造
        TRANSFORMATION: 变换构造
        ADVANCED: 高级构造
    """
    BASIC = auto()           # 基础几何构造
    CIRCLE = auto()          # 圆相关构造
    TRIANGLE = auto()        # 三角形构造
    POLYGON = auto()         # 多边形构造
    TRANSFORMATION = auto()  # 变换构造
    ADVANCED = auto()        # 高级构造


class DeterminismLevel(Enum):
    """
    确定性级别枚举。
    
    描述函数块在不同输入条件下的解的唯一性保证。
    
    枚举值：
        ALWAYS_UNIQUE: 对于所有有效输入，解总是唯一的
        CONDITIONALLY_UNIQUE: 在满足特定条件时解唯一
        MULTIPLE_SOLUTIONS: 可能产生多个解，需要选择器
    """
    ALWAYS_UNIQUE = auto()          # 总是唯一解
    CONDITIONALLY_UNIQUE = auto()   # 条件唯一解
    MULTIPLE_SOLUTIONS = auto()     # 多解情况


@dataclass
class FuncBlockSpec:
    """
    函数块规格说明数据类。
    
    描述一个预设函数块的完整规格，包括其输入输出、
    数学描述、前置条件和使用示例。
    
    属性：
        name: 函数块名称（英文标识符）
        chinese_name: 中文名称
        category: 所属分类
        description: 详细描述
        input_count: 输入端口数量
        output_count: 输出端口数量
        determinism: 确定性级别
        preconditions: 前置条件列表
        mathematical_definition: 数学定义描述
        example_usage: 使用示例代码
        notes: 额外注意事项
    """
    name: str
    chinese_name: str
    category: PresetFuncBlockCategory
    description: str
    input_count: int
    output_count: int
    determinism: DeterminismLevel = DeterminismLevel.ALWAYS_UNIQUE
    preconditions: List[str] = field(default_factory=list)
    mathematical_definition: str = ""
    example_usage: str = ""
    notes: str = ""


# ============================================================
# 预设函数块规格注册表
# ============================================================

# 全局预设函数块规格注册表
_PRESET_FUNC_BLOCK_SPECS: Dict[str, FuncBlockSpec] = {}


def register_preset(spec: FuncBlockSpec) -> None:
    """
    注册预设函数块规格到全局注册表。
    
    将给定的函数块规格添加到全局注册表中，以便后续查询和使用。
    如果同名规格已存在，将覆盖旧规格。
    
    参数：
        spec: 函数块规格说明对象
        
    示例：
        >>> spec = FuncBlockSpec(
        ...     name="midpoint",
        ...     chinese_name="中点",
        ...     category=PresetFuncBlockCategory.BASIC,
        ...     description="计算两点的中点",
        ...     input_count=2,
        ...     output_count=1
        ... )
        >>> register_preset(spec)
    """
    _PRESET_FUNC_BLOCK_SPECS[spec.name] = spec


def get_preset_spec(name: str) -> Optional[FuncBlockSpec]:
    """
    根据名称获取预设函数块规格。
    
    从全局注册表中查找指定名称的函数块规格。
    
    参数：
        name: 函数块名称（英文标识符）
        
    返回：
        FuncBlockSpec 或 None: 找到的规格对象，若不存在则返回 None
        
    示例：
        >>> spec = get_preset_spec("midpoint")
        >>> if spec:
        ...     print(spec.chinese_name)  # 输出: 中点
    """
    return _PRESET_FUNC_BLOCK_SPECS.get(name)


def list_all_presets() -> List[str]:
    """
    列出所有已注册的预设函数块名称。
    
    返回全局注册表中所有预设函数块的名称列表。
    
    返回：
        List[str]: 预设函数块名称列表，按注册顺序排列
        
    示例：
        >>> names = list_all_presets()
        >>> print(names)  # ['midpoint', 'perpendicular_bisector', ...]
    """
    return list(_PRESET_FUNC_BLOCK_SPECS.keys())


def list_presets_by_category(category: PresetFuncBlockCategory) -> List[FuncBlockSpec]:
    """
    按分类列出预设函数块规格。
    
    返回指定分类下的所有预设函数块规格列表。
    
    参数：
        category: 预设函数块分类枚举值
        
    返回：
        List[FuncBlockSpec]: 该分类下的所有规格列表
        
    示例：
        >>> specs = list_presets_by_category(PresetFuncBlockCategory.BASIC)
        >>> for spec in specs:
        ...     print(spec.chinese_name)
    """
    return [spec for spec in _PRESET_FUNC_BLOCK_SPECS.values() 
            if spec.category == category]


# ============================================================
# 基础几何构造预设
# ============================================================

# ---- 中点构造 ----
register_preset(FuncBlockSpec(
    name="midpoint",
    chinese_name="中点",
    category=PresetFuncBlockCategory.BASIC,
    description="""
    计算两点的中点。
    
    给定平面上的两个点 P 和 Q，构造线段 PQ 的中点 M。
    中点 M 满足：|PM| = |MQ| = |PQ|/2。
    
    这是几何构造中最基本的操作之一，广泛应用于：
    - 三角形中位线构造
    - 平行四边形对角线交点
    - 圆心定位
    """,
    input_count=2,
    output_count=1,
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=[
        "两个输入点必须不同（P ≠ Q）"
    ],
    mathematical_definition="""
    设 P = (x₁, y₁), Q = (x₂, y₂)
    则中点 M = ((x₁+x₂)/2, (y₁+y₂)/2)
    
    在符号坐标系统中：
    M.x = (P.x + Q.x) / 2
    M.y = (P.y + Q.y) / 2
    """,
    example_usage="""
    >>> from lv00 import Graph
    >>> from lv00.preset_func_blocks import create_midpoint
    >>> g = Graph()
    >>> p1 = g.add_point(0, 0)
    >>> p2 = g.add_point(2, 0)
    >>> midpoint = create_midpoint(g, p1, p2)
    >>> print(midpoint)  # Point(1, 0)
    """,
    notes="中点构造是确定性的，对于任意两个不同的点，中点唯一确定。"
))


# ---- 垂直平分线构造 ----
register_preset(FuncBlockSpec(
    name="perpendicular_bisector",
    chinese_name="垂直平分线",
    category=PresetFuncBlockCategory.BASIC,
    description="""
    构造线段的垂直平分线。
    
    给定线段 PQ，构造过中点 M 且垂直于 PQ 的直线。
    垂直平分线上的任意点到 P 和 Q 的距离相等。
    
    数学性质：
    - 垂直平分线是到 P、Q 等距点的轨迹
    - 三角形三边的垂直平分线交于外心
    """,
    input_count=2,
    output_count=1,
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=[
        "两个端点必须不同（P ≠ Q）"
    ],
    mathematical_definition="""
    设 P = (x₁, y₁), Q = (x₂, y₂)
    中点 M = ((x₁+x₂)/2, (y₁+y₂)/2)
    
    垂直平分线方程：
    (x - M.x)(Q.x - P.x) + (y - M.y)(Q.y - P.y) = 0
    
    或用斜率表示：
    若 PQ 斜率为 k，则垂直平分线斜率为 -1/k
    """,
    example_usage="""
    >>> from lv00 import Graph
    >>> from lv00.preset_func_blocks import create_perpendicular_bisector
    >>> g = Graph()
    >>> p1 = g.add_point(0, 0)
    >>> p2 = g.add_point(2, 0)
    >>> bisector = create_perpendicular_bisector(g, p1, p2)
    """,
    notes="垂直平分线用线段表示，需要指定长度或延伸到边界。"
))


# ---- 角平分线构造 ----
register_preset(FuncBlockSpec(
    name="angle_bisector",
    chinese_name="角平分线",
    category=PresetFuncBlockCategory.BASIC,
    description="""
    构造角的平分线。
    
    给定三个点 A、B、C（B 为角的顶点），构造 ∠ABC 的平分线。
    角平分线将角分成两个相等的角。
    
    数学性质：
    - 角平分线上的点到角两边的距离相等
    - 三角形内角平分线交于内心
    """,
    input_count=3,
    output_count=1,
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=[
        "三个点不能共线",
        "顶点 B 不能与 A 或 C 重合"
    ],
    mathematical_definition="""
    设 A = (x₁, y₁), B = (x₂, y₂), C = (x₃, y₃)
    
    单位方向向量：
    u = (A - B) / |A - B|
    v = (C - B) / |C - B|
    
    角平分线方向向量：
    w = u + v （归一化）
    
    角平分线：B + t·w, t ∈ ℝ
    """,
    example_usage="""
    >>> from lv00 import Graph
    >>> from lv00.preset_func_blocks import create_angle_bisector
    >>> g = Graph()
    >>> a = g.add_point(0, 1)
    >>> b = g.add_point(0, 0)  # 顶点
    >>> c = g.add_point(1, 0)
    >>> bisector = create_angle_bisector(g, a, b, c)
    """,
    notes="对于平角（180°），角平分线有两条，需要选择器。"
))


# ---- 线段交点构造 ----
register_preset(FuncBlockSpec(
    name="line_intersection",
    chinese_name="线段交点",
    category=PresetFuncBlockCategory.BASIC,
    description="""
    计算两条线段的交点。
    
    给定两条线段 AB 和 CD，计算它们的交点（若存在）。
    
    情况分析：
    - 若线段相交，返回唯一交点
    - 若线段平行但不重合，无解
    - 若线段重合，可能有无限多解
    """,
    input_count=4,
    output_count=1,
    determinism=DeterminismLevel.CONDITIONALLY_UNIQUE,
    preconditions=[
        "两条线段不能退化（端点不重合）",
        "线段不平行或重合"
    ],
    mathematical_definition="""
    设 AB: A + t(B-A), t ∈ [0,1]
    设 CD: C + s(D-C), s ∈ [0,1]
    
    求解方程组：
    A + t(B-A) = C + s(D-C)
    
    若行列式 |B-A, D-C| ≠ 0，有唯一解：
    t = |C-A, D-C| / |B-A, D-C|
    s = |C-A, B-A| / |B-A, D-C|
    
    当 t, s ∈ [0,1] 时，线段相交
    """,
    example_usage="""
    >>> from lv00 import Graph
    >>> from lv00.preset_func_blocks import create_line_intersection
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


# ---- 垂线构造 ----
register_preset(FuncBlockSpec(
    name="perpendicular_line",
    chinese_name="垂线",
    category=PresetFuncBlockCategory.BASIC,
    description="""
    过一点作直线的垂线。
    
    给定直线 l 和点 P（不在 l 上或在其上），构造过 P 且垂直于 l 的直线。
    
    数学性质：
    - 垂线与原直线的夹角为 90°
    - 点 P 到直线 l 的距离等于 P 到垂足的距离
    """,
    input_count=3,
    output_count=1,
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=[
        "直线不能退化（两点不重合）"
    ],
    mathematical_definition="""
    设直线 l 过点 A(x₁,y₁) 和 B(x₂,y₂)
    点 P = (x₀, y₀)
    
    直线 l 的方向向量：d = (x₂-x₁, y₂-y₁)
    垂线的方向向量：d⊥ = (y₁-y₂, x₂-x₁)
    
    垂线方程：P + t·d⊥
    """,
    example_usage="""
    >>> from lv00 import Graph
    >>> from lv00.preset_func_blocks import create_perpendicular_line
    >>> g = Graph()
    >>> a = g.add_point(0, 0)
    >>> b = g.add_point(2, 0)
    >>> p = g.add_point(1, 1)
    >>> perp = create_perpendicular_line(g, a, b, p)
    """,
    notes="无论点 P 是否在直线上，垂线都是唯一确定的。"
))


# ---- 平行线构造 ----
register_preset(FuncBlockSpec(
    name="parallel_line",
    chinese_name="平行线",
    category=PresetFuncBlockCategory.BASIC,
    description="""
    过一点作直线的平行线。
    
    给定直线 l 和点 P，构造过 P 且平行于 l 的直线。
    
    数学性质：
    - 平行线永不相交
    - 平行线具有相同的斜率
    """,
    input_count=3,
    output_count=1,
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=[
        "直线不能退化（两点不重合）"
    ],
    mathematical_definition="""
    设直线 l 过点 A(x₁,y₁) 和 B(x₂,y₂)
    点 P = (x₀, y₀)
    
    平行线方向向量：d = (x₂-x₁, y₂-y₁)
    平行线方程：P + t·d
    """,
    example_usage="""
    >>> from lv00 import Graph
    >>> from lv00.preset_func_blocks import create_parallel_line
    >>> g = Graph()
    >>> a = g.add_point(0, 0)
    >>> b = g.add_point(1, 1)
    >>> p = g.add_point(0, 1)
    >>> parallel = create_parallel_line(g, a, b, p)
    """,
    notes="平行线是唯一确定的。"
))


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
register_preset(FuncBlockSpec(
    name="distance",
    chinese_name="两点距离",
    category=PresetFuncBlockCategory.BASIC,
    description="""
    计算两点之间的欧几里得距离。

    给定平面上的两个点 P 和 Q，计算线段 PQ 的长度。

    数学性质：
    - 距离满足三角不等式
    - 距离非负，仅当 P=Q 时为零
    - 距离具有对称性：|PQ| = |QP|
    """,
    input_count=2,
    output_count=1,
    determinism=DeterminismLevel.ALWAYS_UNIQUE,
    preconditions=[],
    mathematical_definition="""
    设 P = (x₁, y₁), Q = (x₂, y₂)

    距离 d = √((x₁ - x₂)² + (y₁ - y₂)²)

    使用符号计算时，结果以符号形式保留：
    d = sqrt((P.x - Q.x)² + (P.y - Q.y)²)
    """,
    example_usage="""
    >>> from lv00 import Graph
    >>> from lv00.preset_func_blocks import create_distance
    >>> g = Graph()
    >>> p = g.add_point(0, 0)
    >>> q = g.add_point(3, 4)
    >>> d = create_distance(g, p, q)
    >>> print(d)  # 5  (使用数值坐标时)
    """,
    notes="当两点重合时距离为零。返回 SymbolicCoord 类型。"
))


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
# 预设函数块创建函数
# ============================================================

def create_midpoint(graph: 'Graph', p1: 'Point', p2: 'Point') -> 'Point':
    """
    创建中点构造。
    
    给定两个点，计算它们的中点。
    
    参数：
        graph: 约束图对象
        p1: 第一个点
        p2: 第二个点
        
    返回：
        Point: 中点对象
        
    异常：
        ValueError: 两点重合时抛出
        
    示例：
        >>> g = Graph()
        >>> a = g.add_point(0, 0)
        >>> b = g.add_point(2, 0)
        >>> m = create_midpoint(g, a, b)
        >>> print(m)  # Point(1, 0)
    """
    # 验证输入点不同
    if p1.x == p2.x and p1.y == p2.y:
        raise ValueError("两点重合，无法计算中点")
    
    # 计算中点坐标
    from .core import SymbolicCoord
    
    mid_x = (p1.x + p2.x) / SymbolicCoord.from_rational(2)
    mid_y = (p1.y + p2.y) / SymbolicCoord.from_rational(2)
    
    return graph.add_point(mid_x, mid_y)


def create_perpendicular_bisector(
    graph: 'Graph', 
    p1: 'Point', 
    p2: 'Point',
    length: Optional['SymbolicCoord'] = None
) -> 'LineSegment':
    """
    创建垂直平分线构造。
    
    给定两个点，构造过中点且垂直于连线的直线（用有限线段表示）。
    
    参数：
        graph: 约束图对象
        p1: 第一个端点
        p2: 第二个端点
        length: 线段长度（可选，默认为单位长度）
        
    返回：
        LineSegment: 垂直平分线段
        
    异常：
        ValueError: 两点重合时抛出
        
    示例：
        >>> g = Graph()
        >>> a = g.add_point(0, 0)
        >>> b = g.add_point(2, 0)
        >>> bisector = create_perpendicular_bisector(g, a, b)
    """
    # 验证输入点不同
    if p1.x == p2.x and p1.y == p2.y:
        raise ValueError("两点重合，无法构造垂直平分线")
    
    from .core import SymbolicCoord
    
    # 计算中点
    mid_x = (p1.x + p2.x) / SymbolicCoord.from_rational(2)
    mid_y = (p1.y + p2.y) / SymbolicCoord.from_rational(2)
    mid = graph.add_point(mid_x, mid_y)
    
    # 计算垂直方向向量
    # 原方向向量：(p2.x - p1.x, p2.y - p1.y)
    # 垂直方向向量：(p1.y - p2.y, p2.x - p1.x)
    
    dx = p2.x - p1.x
    dy = p2.y - p1.y
    
    # 垂直方向（旋转90度）
    perp_x = -dy
    perp_y = dx
    
    # 归一化（如果需要指定长度）
    if length is not None:
        from .core import SymbolicCoord
        # 计算原向量长度
        # 这里简化处理，使用单位向量乘以长度
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
    # a = |BC|, b = |CA|, c = |AB|
    a = b.distance_to(c)
    b = c.distance_to(a)
    c = a.distance_to(b)
    
    # 内心公式：I = (a*A + b*B + c*C) / (a + b + c)
    # 内心是三条角平分线的交点，到三边距离相等
    perimeter = a + b + c
    
    # 计算内心坐标
    incenter_x = (a * a.x + b * b.x + c * c.x) / perimeter
    incenter_y = (a * a.y + b * b.y + c * c.y) / perimeter
    
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


def create_distance(
    graph: 'Graph',
    p1: 'Point',
    p2: 'Point'
) -> 'SymbolicCoord':
    """
    计算两点之间的欧几里得距离。

    给定平面上的两个点，计算其欧几里得距离（直线距离）。
    使用符号坐标进行精确计算，返回 SymbolicCoord 类型的结果。

    参数：
        graph: 约束图对象（保留参数，用于接口统一）
        p1: 第一个点
        p2: 第二个点

    返回：
        SymbolicCoord: 两点之间的距离（符号值）

    示例：
        >>> g = Graph()
        >>> p = g.add_point(0, 0)
        >>> q = g.add_point(3, 4)
        >>> d = create_distance(g, p, q)
        >>> print(d)  # 5
    """
    return p1.distance_to(p2)


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
# 辅助函数
# ============================================================

def get_preset_info(name: str) -> str:
    """
    获取预设函数块的详细信息字符串。
    
    格式化输出预设函数块的完整规格说明，
    包括名称、分类、描述、数学定义等。
    
    参数：
        name: 预设函数块名称
        
    返回：
        str: 格式化的信息字符串
        
    示例：
        >>> info = get_preset_info("midpoint")
        >>> print(info)
    """
    spec = get_preset_spec(name)
    if spec is None:
        return f"未找到预设函数块: {name}"
    
    lines = [
        f"=== {spec.chinese_name} ({spec.name}) ===",
        f"分类: {spec.category.name}",
        f"输入端口: {spec.input_count}",
        f"输出端口: {spec.output_count}",
        f"确定性: {spec.determinism.name}",
        "",
        "描述:",
        spec.description,
    ]
    
    if spec.preconditions:
        lines.append("")
        lines.append("前置条件:")
        for cond in spec.preconditions:
            lines.append(f"  - {cond}")
    
    if spec.mathematical_definition:
        lines.append("")
        lines.append("数学定义:")
        lines.append(spec.mathematical_definition)
    
    if spec.notes:
        lines.append("")
        lines.append("注意事项:")
        lines.append(spec.notes)
    
    return "\n".join(lines)


def validate_preset_inputs(
    name: str, 
    inputs: List[Any]
) -> Tuple[bool, str]:
    """
    验证预设函数块的输入参数。
    
    检查输入参数的数量和类型是否符合预设函数块的规格要求。
    
    参数：
        name: 预设函数块名称
        inputs: 输入参数列表
        
    返回：
        Tuple[bool, str]: (是否有效, 错误消息)
        
    示例：
        >>> valid, msg = validate_preset_inputs("midpoint", [p1, p2])
        >>> if not valid:
        ...     print(msg)
    """
    spec = get_preset_spec(name)
    if spec is None:
        return False, f"未找到预设函数块: {name}"
    
    if len(inputs) != spec.input_count:
        return False, (
            f"输入参数数量不匹配: 期望 {spec.input_count} 个, "
            f"实际 {len(inputs)} 个"
        )
    
    # 基本类型检查
    from .core import Point, LineSegment, SymbolicCoord
    
    for i, inp in enumerate(inputs):
        if inp is None:
            return False, f"第 {i+1} 个输入参数为 None"
    
    return True, ""


# ============================================================
# 模块导出
# ============================================================

__all__ = [
    # 枚举和常量
    'PresetFuncBlockCategory',
    'DeterminismLevel',

    # 数据类
    'FuncBlockSpec',

    # 注册表函数
    'register_preset',
    'get_preset_spec',
    'list_all_presets',
    'list_presets_by_category',

    # 基础几何构造
    'create_midpoint',
    'create_perpendicular_bisector',
    'create_distance',

    # 三角形构造
    'create_centroid',
    'create_circumcenter',
    'create_incenter',
    'create_orthocenter',
    'create_area',
    'create_equilateral_triangle',
    'create_triangle_centroid',

    # 多边形构造
    'create_square',

    # 变换构造
    'create_reflection',
    'create_translation',
    'create_rotation',

    # 辅助函数
    'get_preset_info',
    'validate_preset_inputs',
]
