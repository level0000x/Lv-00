"""
Lv-00 预设函数块模块 - 基础构造

提供理论数学研究中常用的几何函数块基础预设，包括：
    - 枚举和常量定义
    - 注册表系统
    - 基础几何构造（中点、垂直平分线、距离等）
    - 辅助函数

设计原则：
    1. 每个预设函数块都是确定性的（唯一解）
    2. 支持符号计算，保持精确性
    3. 提供完整的中文文档和数学描述
    4. 遵循局部最优解原则

版本：3.3.0
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
# 预设函数块规格注册
# ============================================================



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
    >>> from lv import Graph
    >>> from lv.preset_func_blocks import create_midpoint
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
    >>> from lv import Graph
    >>> from lv.preset_func_blocks import create_perpendicular_bisector
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
    >>> from lv import Graph
    >>> from lv.preset_func_blocks import create_angle_bisector
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
    >>> from lv import Graph
    >>> from lv.preset_func_blocks import create_line_intersection
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
    >>> from lv import Graph
    >>> from lv.preset_func_blocks import create_perpendicular_line
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
    >>> from lv import Graph
    >>> from lv.preset_func_blocks import create_parallel_line
    >>> g = Graph()
    >>> a = g.add_point(0, 0)
    >>> b = g.add_point(1, 1)
    >>> p = g.add_point(0, 1)
    >>> parallel = create_parallel_line(g, a, b, p)
    """,
    notes="平行线是唯一确定的。"
))



# ============================================================
# 预设函数块规格注册
# ============================================================


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
    >>> from lv import Graph
    >>> from lv.preset_func_blocks import create_distance
    >>> g = Graph()
    >>> p = g.add_point(0, 0)
    >>> q = g.add_point(3, 4)
    >>> d = create_distance(g, p, q)
    >>> print(d)  # 5  (使用数值坐标时)
    """,
    notes="当两点重合时距离为零。返回 SymbolicCoord 类型。"
))



# ============================================================
# 预设函数块规格注册
# ============================================================


# ============================================================
# 预设函数块创建函数
# ============================================================

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

    # 辅助函数
    'get_preset_info',
    'validate_preset_inputs',
    # 注意：三角形构造（create_centroid, create_circumcenter, create_incenter,
    # create_orthocenter）、多边形构造（create_square）、变换构造（create_reflection,
    # create_translation, create_rotation）及其他（create_area, create_equilateral_triangle,
    # create_triangle_centroid）定义在 preset_analysis.py 中，通过 __init__.py 的
    # 惰性导入机制统一导出，不在此处重复声明。
]

