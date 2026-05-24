"""
Lv-00 预设函数块模块（兼容性重新导出层）

警告：此模块已拆分为四个领域文件：
    - preset_basic.py:   基础构造（中点、垂直平分线、距离等）+ 注册表系统
    - preset_analysis.py: 圆与三角形构造（外心、重心、垂心、内心等）
    - preset_algebra.py:  变换构造（反射、平移、旋转）
    - preset_topology.py: 多边形构造（正方形、正多边形）

所有导出名称保持不变，现有代码无需修改。

版本：3.2.0
作者：Lv-00 开发团队
"""

# 重新导出基础模块（包含枚举、注册表和基础构造）
from .preset_basic import (
    # 枚举和常量
    PresetFuncBlockCategory,
    DeterminismLevel,
    # 数据类
    FuncBlockSpec,
    # 注册表函数
    register_preset,
    get_preset_spec,
    list_all_presets,
    list_presets_by_category,
    # 基础几何构造
    create_midpoint,
    create_perpendicular_bisector,
    create_distance,
    # 辅助函数
    get_preset_info,
    validate_preset_inputs,
)

# 重新导出分析模块（圆与三角形）
from .preset_analysis import (
    create_centroid,
    create_circumcenter,
    create_incenter,
    create_orthocenter,
    create_area,
    create_equilateral_triangle,
    create_triangle_centroid,
)

# 重新导出代数模块（变换）
from .preset_algebra import (
    create_reflection,
    create_translation,
    create_rotation,
)

# 重新导出拓扑模块（多边形）
from .preset_topology import (
    create_square,
)


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
