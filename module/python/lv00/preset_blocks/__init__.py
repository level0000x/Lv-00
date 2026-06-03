"""
Lv-00 预设函数块增强模块 v4.0.0
================================

模块功能概述:
    提供理论数学研究中常用的几何函数块预设集合。
    本模块是 Lv-00 函数块系统的扩展，支持几何构造、代数运算、
    拓扑分析等多个数学领域的预设函数块。

架构设计:
    采用分层模块化设计，按数学领域划分为多个子模块:
    - base: 基础架构和共享组件
    - geometry: 几何构造预设
    - algebra: 代数运算预设
    - topology: 拓扑分析预设
    - analysis: 数学分析预设
    - logic: 逻辑推导预设

核心特性:
    1. 完整的类型注解支持
    2. 统一的中文文档规范
    3. 数学定义与实现严格对应
    4. 支持符号计算和数值计算双模式
    5. 完善的错误处理和前置条件检查

使用示例:
    >>> from lv00.preset_blocks import create_midpoint, create_centroid
    >>> from lv00 import Graph
    >>> 
    >>> g = Graph()
    >>> a = g.add_point(0, 0)
    >>> b = g.add_point(2, 0)
    >>> midpoint = create_midpoint(g, a, b)

版本: 4.0.0
作者: Lv-00 开发团队
许可证: MIT
"""

from __future__ import annotations

# 版本信息
__version__: str = "4.0.0"
__author__: str = "Lv-00 开发团队"

# ============================================================
# 从基础模块重新导出核心类型和函数
# ============================================================

from .base import (
    # 枚举类型
    PresetCategory,
    DeterminismLevel,
    OutputFormat,
    
    # 数据类
    FuncBlockSpec,
    ParamSpec,
    OutputSpec,
    ValidationResult,
    
    # 注册表
    PresetRegistry,
    get_registry,
    register_preset,
    get_preset_spec,
    list_all_presets,
    list_presets_by_category,
    
    # 工具函数
    validate_inputs,
    format_math_definition,
    get_preset_info,
)

# ============================================================
# 从几何模块导出
# ============================================================

from .geometry import (
    # 基础构造
    create_midpoint,
    create_perpendicular_bisector,
    create_angle_bisector,
    create_line_intersection,
    create_perpendicular_line,
    create_parallel_line,
    create_distance,
    create_foot_of_perpendicular,
    
    # 圆相关
    create_circle_center,
    create_circle_by_center_radius,
    create_circle_line_intersection,
    create_tangent_line,
    
    # 三角形相关
    create_centroid,
    create_circumcenter,
    create_incenter,
    create_orthocenter,
    create_excenter,
    create_triangle_area,
    create_equilateral_triangle,
    create_nine_point_circle,
    
    # 多边形
    create_square,
    create_regular_polygon,
)

# ============================================================
# 从代数模块导出
# ============================================================

from .algebra import (
    create_reflection,
    create_translation,
    create_rotation,
    create_homothety,
    create_circle_inversion,
    create_affine_transform,
)

# ============================================================
# 导出列表
# ============================================================

__all__ = [
    # 元信息
    "__version__",
    "__author__",
    
    # 枚举类型
    "PresetCategory",
    "DeterminismLevel",
    "OutputFormat",
    
    # 数据类
    "FuncBlockSpec",
    "ParamSpec",
    "OutputSpec",
    "ValidationResult",
    
    # 注册表
    "PresetRegistry",
    "get_registry",
    "register_preset",
    "get_preset_spec",
    "list_all_presets",
    "list_presets_by_category",
    
    # 工具函数
    "validate_inputs",
    "format_math_definition",
    "get_preset_info",
    
    # 几何构造 - 基础
    "create_midpoint",
    "create_perpendicular_bisector",
    "create_angle_bisector",
    "create_line_intersection",
    "create_perpendicular_line",
    "create_parallel_line",
    "create_distance",
    "create_foot_of_perpendicular",
    
    # 几何构造 - 圆
    "create_circle_center",
    "create_circle_by_center_radius",
    "create_circle_line_intersection",
    "create_tangent_line",
    
    # 几何构造 - 三角形
    "create_centroid",
    "create_circumcenter",
    "create_incenter",
    "create_orthocenter",
    "create_excenter",
    "create_triangle_area",
    "create_equilateral_triangle",
    "create_nine_point_circle",
    
    # 几何构造 - 多边形
    "create_square",
    "create_regular_polygon",
    
    # 代数变换
    "create_reflection",
    "create_translation",
    "create_rotation",
    "create_homothety",
    "create_circle_inversion",
    "create_affine_transform",
]
