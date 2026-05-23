"""
Lv-00 Python 绑定包

Lv-00 几何元编程库的 Python 接口。
提供符号坐标、约束图、几何构造、公式编程等核心功能。

主要模块：
    - core: 核心类（SymbolicCoord、Point、LineSegment、Graph）
    - engine: Lv-00 主引擎
    - func_block: 函数块系统
    - preset_func_blocks: 预设函数块（常用几何构造）
        - 基础构造：中点、垂直平分线、距离
        - 三角形构造：重心、垂心、外心、内心、面积、等边三角形
        - 多边形构造：正方形
        - 变换构造：反射、平移、旋转
    - constraints: 几何约束类型
    - normalization: 图归一化结果处理
    - formula: 公式编程（解析、渲染、转换）
    - _ctypes_binding: 底层 C 库的 ctypes 绑定

版本：3.0.2
作者：Lv-00 开发团队

示例：
    >>> from lv00 import Graph, Point
    >>> g = Graph()
    >>> p1 = g.add_point(0, 0)
    >>> p2 = g.add_point(1, 1)
    >>> g.add_line_segment(p1, p2)
    >>> g.normalize()
    
    # 使用引擎
    >>> from lv00 import Engine
    >>> engine = Engine()
    >>> engine.load_module("my_module.lv00")
    >>> engine.solve()
    
    # 使用预设函数块
    >>> from lv00.preset_func_blocks import create_midpoint, create_centroid
    >>> from lv00.preset_func_blocks import create_reflection, create_square
    >>> a = g.add_point(0, 0)
    >>> b = g.add_point(2, 0)
    >>> m = create_midpoint(g, a, b)  # 中点
    >>> c, d = create_square(g, a, b)  # 正方形：A(0,0)、B(2,0)、C(2,2)、D(0,2)
"""

import logging

__version__ = "3.0.2"
__author__ = "Lv-00 开发团队"
__description__ = "Lv-00 几何元编程库 Python 接口"

# ============================================================
# 核心模块
# ============================================================

try:
    from .core import (
        # 核心类
        Graph,
        Point,
        LineSegment,
        SymbolicCoord,
        GeomNode,
        NormalizationResult,
        # 异常类
        Lv00Error,
        Lv00LibraryError,
        Lv00ArgumentError,
        Lv00ConstraintError,
        Lv00SolverError,
        # 工具函数
        init,
        cleanup,
        get_version,
        get_last_error,
        set_debug_mode,
        set_log_level,
        reset_counters,
        get_counter_report,
        # 日志级别常量
        LOG_LEVEL_DEBUG,
        LOG_LEVEL_INFO,
        LOG_LEVEL_WARN,
        LOG_LEVEL_ERROR,
    )
except ImportError as e:
    raise ImportError(
        f"无法导入 Lv-00 核心模块 (lv00.core)。\n"
        f"请确认以下各项已正确配置：\n"
        f"  1. Lv-00 动态链接库已编译并放置于正确路径\n"
        f"  2. LV00_LIBRARY_PATH 环境变量已设置（如需要）\n"
        f"  3. 所有依赖的运行时库均已安装\n"
        f"原始错误：{e}"
    ) from e

# ============================================================
# 引擎模块
# ============================================================

try:
    from .engine import (
        Engine,
        EngineError,
        EngineMemoryError,
        EngineStateError,
        EngineConflictError,
        EngineModuleError,
        # 结果码
        ENGINE_OK,
        ENGINE_OUT_OF_MEMORY,
        ENGINE_INVALID_STATE,
        ENGINE_CONSTRAINT_CONFLICT,
        ENGINE_MODULE_ERROR,
        ENGINE_SOLVE_OK,
        ENGINE_SOLVE_CONFLICT,
        ENGINE_SOLVE_TIMEOUT,
        ENGINE_SOLVE_ERROR,
        # 合一状态
        UNIFY_OK,
        UNIFY_FAILED,
        UNIFY_TYPE_MISMATCH,
    )
except ImportError as e:
    raise ImportError(
        f"无法导入 Lv-00 引擎模块 (lv00.engine)。\n"
        f"原始错误：{e}"
    ) from e

# ============================================================
# 函数块模块
# ============================================================

try:
    from .func_block import (
        FuncBlock,
        FuncBlockError,
        FuncBlockPackError,
        FuncBlockInstantiateError,
        FuncBlockDeterminismError,
        SolutionSelector,
        DeterminismState,
        SelectorType,
        PackResult,
        InstantiateResult,
        func_block_pack,
    )
except ImportError as e:
    raise ImportError(
        f"无法导入 Lv-00 函数块模块 (lv00.func_block)。\n"
        f"原始错误：{e}"
    ) from e

# ============================================================
# 约束模块
# ============================================================
# 约束模块是包的核心依赖，直接导入——失败时不允许降级，
# 因为约束类型是整个约束图系统的基础组件。

try:
    from .constraints import (
        Constraint,
        IncidenceConstraint,
        BetweennessConstraint,
        IntersectionConstraint,
        ContainmentConstraint,
        ConnectionConstraint,
        ConstraintType,
    )
except ImportError as e:
    raise ImportError(
        f"无法导入 Lv-00 约束模块 (lv00.constraints)。\n"
        f"原始错误：{e}"
    ) from e

# ============================================================
# 规范化模块（薄层重导出）
# ============================================================
# NormalizationResult 的真正定义位于 lv00.core 模块。
# lv00.normalization 模块仅作为薄层从 core.py 重新导出，
# 为保持向后兼容性而保留。新代码建议直接从 lv00.core 导入。
#
# 此处的 NormalizationResult 已通过上方的 "from .core import NormalizationResult" 导入，
# 因此无需额外操作。

# ============================================================
# 公式模块（大型可选模块，使用惰性导入）
# ============================================================
# 公式模块依赖较多，采用惰性导入模式以加快包加载速度。
# 首次访问 lv00.FormulaParser 等属性时才会触发实际导入。

_formula_exports = {}

def __getattr__(name: str):
    """
    惰性加载公式模块和预设函数块模块的属性和类。

    当首次访问公式相关导出或预设函数块时，才真正导入对应子模块，
    避免在包初始化时加载重量级的依赖。

    参数：
        name: 属性名称

    返回：
        Any: 对应的类或函数对象

    异常：
        AttributeError: 属性名不在模块的导出列表中
    """
    # 首先尝试预设函数块模块
    preset_result = _get_preset_attr(name)
    if preset_result is not None:
        return preset_result
    
    # 公式模块的导出名称列表
    _formula_names = {
        'FormulaParser', 'FormulaAST', 'FormulaRenderer', 'FormulaConverter',
        'FormulaParseError', 'SyntaxType', 'OutputFormat',
        'parse', 'render', 'to_graph', 'from_graph',
    }
    if name in _formula_names:
        if name not in _formula_exports:
            from . import formula as _formula_mod
            for attr_name in _formula_names:
                if hasattr(_formula_mod, attr_name):
                    _formula_exports[attr_name] = getattr(_formula_mod, attr_name)
        return _formula_exports[name]
    raise AttributeError(f"module 'lv00' has no attribute '{name}'")

# ============================================================
# 预设函数块模块（惰性导入）
# ============================================================
# 预设函数块模块提供常用几何构造的预设函数块。
# 采用惰性导入模式以加快包加载速度。

_preset_exports = {}

def _get_preset_attr(name: str):
    """
    惰性加载预设函数块模块的属性。

    参数：
        name: 属性名称

    返回：
        Any: 对应的类或函数对象

    异常：
        AttributeError: 属性名不在预设模块的导出列表中
    """
    _preset_names = {
        'create_midpoint', 'create_perpendicular_bisector',
        'create_centroid', 'create_circumcenter', 'create_incenter', 'create_orthocenter',
        'create_reflection', 'create_translation', 'create_rotation',
        'create_distance', 'create_area',
        'create_square', 'create_equilateral_triangle', 'create_triangle_centroid',
        'PresetFuncBlockCategory', 'DeterminismLevel', 'FuncBlockSpec',
        'get_preset_info', 'list_all_presets', 'validate_preset_inputs',
    }
    if name in _preset_names:
        if name not in _preset_exports:
            from . import preset_func_blocks as _preset_mod
            for attr_name in _preset_names:
                if hasattr(_preset_mod, attr_name):
                    _preset_exports[attr_name] = getattr(_preset_mod, attr_name)
        return _preset_exports[name]
    return None


# ============================================================
# 包级别导出
# ============================================================

__all__ = [
    # 版本信息
    "__version__",
    "__author__",
    "__description__",
    
    # 核心类
    "Graph",
    "Point",
    "LineSegment",
    "SymbolicCoord",
    "GeomNode",
    "NormalizationResult",
    
    # 引擎
    "Engine",
    
    # 函数块
    "FuncBlock",
    "SolutionSelector",
    "DeterminismState",
    "SelectorType",
    "PackResult",
    "InstantiateResult",
    "func_block_pack",
    
    # 异常类
    "Lv00Error",
    "Lv00LibraryError",
    "Lv00ArgumentError",
    "Lv00ConstraintError",
    "Lv00SolverError",
    "EngineError",
    "EngineMemoryError",
    "EngineStateError",
    "EngineConflictError",
    "EngineModuleError",
    "FuncBlockError",
    "FuncBlockPackError",
    "FuncBlockInstantiateError",
    "FuncBlockDeterminismError",
    
    # 工具函数
    "init",
    "cleanup",
    "get_version",
    "get_last_error",
    "set_debug_mode",
    "set_log_level",
    "reset_counters",
    "get_counter_report",
    
    # 日志级别
    "LOG_LEVEL_DEBUG",
    "LOG_LEVEL_INFO",
    "LOG_LEVEL_WARN",
    "LOG_LEVEL_ERROR",
    
    # 合一状态
    "UNIFY_OK",
    "UNIFY_FAILED",
    "UNIFY_TYPE_MISMATCH",
    
    # 预设函数块（通过惰性导入）
    "create_midpoint",
    "create_perpendicular_bisector",
    "create_centroid",
    "create_circumcenter",
    "create_incenter",
    "create_orthocenter",
    "create_reflection",
    "create_translation",
    "create_rotation",
    "create_distance",
    "create_area",
    "create_square",
    "create_equilateral_triangle",
    "create_triangle_centroid",
    "PresetFuncBlockCategory",
    "DeterminismLevel",
    "FuncBlockSpec",
    "get_preset_info",
    "list_all_presets",
    "validate_preset_inputs",
]


def _check_ctypes_binding() -> bool:
    """
    检查 ctypes 绑定是否正确加载。

    在模块导入时自动调用。

    返回：
        bool: True 表示 ctypes 绑定加载成功，False 表示失败
    """
    try:
        from . import _ctypes_binding
        if hasattr(_ctypes_binding, '_lib'):
            return True
    except Exception as exc:
        logging.warning(
            "ctypes 绑定加载失败，Lv-00 C 库功能不可用。"
            "错误详情: %s",
            exc,
        )
    return False


# 自动初始化检查
_ctypes_binding_ok = _check_ctypes_binding()

if not _ctypes_binding_ok:
    import warnings
    warnings.warn(
        "Lv-00 C 库未正确加载。\n"
        "请确保已编译 Lv-00 库并设置 LV00_LIBRARY_PATH 环境变量。\n"
        "基础功能可能不可用。",
        ImportWarning
    )
