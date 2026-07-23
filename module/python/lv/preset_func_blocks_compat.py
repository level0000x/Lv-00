"""
Lv-00 预设函数块模块（向后兼容层）
====================================

模块功能概述:
    此模块为旧版 preset_func_blocks 提供向后兼容性支持。
    所有功能已迁移到 preset_blocks 包中，本模块仅做重新导出。

迁移说明:
    - 旧导入: from lv.preset_func_blocks import create_midpoint
    - 新导入: from lv.preset_blocks import create_midpoint

版本: 4.0.0
作者: Lv-00 开发团队
警告: 此模块将在 v5.0.0 中移除，请尽快迁移到新模块。
"""

import warnings

# 发出弃用警告
warnings.warn(
    "preset_func_blocks 模块已弃用，将在 v5.0.0 中移除。"
    "请使用 preset_blocks 模块替代。"
    "旧导入: from lv.preset_func_blocks import create_midpoint"
    "新导入: from lv.preset_blocks import create_midpoint",
    DeprecationWarning,
    stacklevel=2
)

# ============================================================
# 从新的 preset_blocks 包重新导出所有内容
# ============================================================

from .preset_blocks import (
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
    
    # 几何构造 - 基础
    create_midpoint,
    create_perpendicular_bisector,
    create_angle_bisector,
    create_line_intersection,
    create_perpendicular_line,
    create_parallel_line,
    create_distance,
    create_foot_of_perpendicular,
    
    # 几何构造 - 圆
    create_circle_center,
    create_circle_by_center_radius,
    create_circle_line_intersection,
    create_tangent_line,
    
    # 几何构造 - 三角形
    create_centroid,
    create_circumcenter,
    create_incenter,
    create_orthocenter,
    create_excenter,
    create_triangle_area,
    create_equilateral_triangle,
    create_nine_point_circle,
    
    # 几何构造 - 多边形
    create_square,
    create_regular_polygon,
    
    # 代数变换
    create_reflection,
    create_translation,
    create_rotation,
    create_homothety,
    create_circle_inversion,
    create_affine_transform,
)

# ============================================================
# 保持旧版名称的别名（向后兼容）
# ============================================================

# 旧版枚举名称别名
PresetFuncBlockCategory = PresetCategory

# 旧版函数别名
create_area = create_triangle_area
create_triangle_centroid = create_centroid

# ============================================================
# 导出列表（与旧版保持一致）
# ============================================================

__all__ = [
    # 枚举和常量
    'PresetFuncBlockCategory',
    'PresetCategory',
    'DeterminismLevel',
    'OutputFormat',
    
    # 数据类
    'FuncBlockSpec',
    'ParamSpec',
    'OutputSpec',
    'ValidationResult',
    
    # 注册表函数
    'PresetRegistry',
    'get_registry',
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
    'create_triangle_area',
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
    'validate_inputs',
    'format_math_definition',
]
