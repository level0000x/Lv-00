"""
Lv-00 预设函数块模块（兼容性重新导出层）v4.0.0
=================================================

模块功能概述:
    此模块为旧版 API 提供向后兼容性支持。
    所有功能已重构并迁移到 preset_blocks 包中。

架构变更:
    原架构（v3.x）:
        - preset_basic.py: 基础构造 + 注册表
        - preset_analysis.py: 三角形构造
        - preset_algebra.py: 变换构造
        - preset_topology.py: 多边形构造
    
    新架构（v4.x）:
        - preset_blocks/
            - base.py: 基础架构和注册表
            - geometry.py: 几何构造
            - algebra.py: 代数变换
            - advanced.py: 高级构造

迁移指南:
    旧导入（仍兼容）:
        from lv00.preset_func_blocks import create_midpoint
    
    新导入（推荐）:
        from lv00.preset_blocks import create_midpoint

版本: 4.0.0
作者: Lv-00 开发团队
警告: 此模块将在 v5.0.0 中移除，请尽快迁移到新模块。
"""

import warnings

# 发出弃用警告
warnings.warn(
    "preset_func_blocks 模块已弃用，将在 v5.0.0 中移除。"
    "请使用 preset_blocks 模块替代。"
    "迁移: from lv00.preset_func_blocks import X → from lv00.preset_blocks import X",
    DeprecationWarning,
    stacklevel=2
)

# ============================================================
# 从新的 preset_blocks 包重新导出所有内容
# ============================================================

from .preset_blocks import (
    # 版本信息
    __version__,
    __author__,
    
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

# 旧版函数别名（保持兼容性）
create_area = create_triangle_area
create_triangle_centroid = create_centroid

# 旧版辅助函数别名
validate_preset_inputs = validate_inputs

# ============================================================
# 导出列表（与旧版保持一致，确保完全兼容）
# ============================================================

__all__ = [
    # 元信息
    '__version__',
    '__author__',
    
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
    
    # 注册表
    'PresetRegistry',
    'get_registry',
    'register_preset',
    'get_preset_spec',
    'list_all_presets',
    'list_presets_by_category',
    
    # 工具函数
    'validate_inputs',
    'validate_preset_inputs',
    'format_math_definition',
    'get_preset_info',
    
    # 几何构造 - 基础
    'create_midpoint',
    'create_perpendicular_bisector',
    'create_angle_bisector',
    'create_line_intersection',
    'create_perpendicular_line',
    'create_parallel_line',
    'create_distance',
    'create_foot_of_perpendicular',
    
    # 几何构造 - 圆
    'create_circle_center',
    'create_circle_by_center_radius',
    'create_circle_line_intersection',
    'create_tangent_line',
    
    # 几何构造 - 三角形
    'create_centroid',
    'create_circumcenter',
    'create_incenter',
    'create_orthocenter',
    'create_excenter',
    'create_area',
    'create_triangle_area',
    'create_equilateral_triangle',
    'create_triangle_centroid',
    'create_nine_point_circle',
    
    # 几何构造 - 多边形
    'create_square',
    'create_regular_polygon',
    
    # 代数变换
    'create_reflection',
    'create_translation',
    'create_rotation',
    'create_homothety',
    'create_circle_inversion',
    'create_affine_transform',
]
