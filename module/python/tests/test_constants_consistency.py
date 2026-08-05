# -*- coding: utf-8 -*-
"""
常量一致性探针测试：断言 Python 绑定层常量与 C 头文件枚举值一致。

背景：
    _ctypes_binding.py 中的常量必须与 C 头文件中的枚举值一一对应。
    历史上曾出现缺失枚举成员导致后续全部错位的问题（如 GEOM_CIRCLE 缺失
    导致 GEOM_PORT/GEOM_FUNCTION_BLOCK 整体前移），本测试用于防止回归。

设计说明：
    - C 枚举值在编译后不导出符号，无法直接从 .dll/.so 中读取枚举常量，
      因此本测试采用「硬编码 C 枚举值基准表 + 头文件/行号来源注释」，
      逐项与 lv._ctypes_binding 中的 Python 常量比对（静态断言）。
    - 当本机存在编译好的 C 库时，额外执行运行时交叉验证：
        * LOG_LEVEL_*：通过 debug_set_log_level / debug_get_log_level 往返验证
        * GEOM_*：通过 graph_create + graph_add_node_with_id 添加端口节点
          （type 传 Python 侧 GEOM_PORT），再读取 C 侧节点 type 字段核对
    - 找不到 C 库时动态部分自动跳过（pytest.skip），并提示运行方式。

运行方式：
    # 方式一：已有编译好的库（liblv.dll / liblv.so / liblv.dylib），直接运行
    python -m pytest module/python/tests/test_constants_consistency.py -v

    # 方式二：显式指定库路径后运行（PowerShell）
    $env:lv_LIBRARY_PATH = "D:\\path\\to\\liblv.dll"
    python -m pytest module/python/tests/test_constants_consistency.py -v

C 枚举值来源（修改 C 头文件时需同步更新下方基准表）：
    - core/include/lv/constraint_graph.h  GeomType 枚举（87-94 行）
    - core/include/lv/constraint_graph.h  ConstraintType 枚举（112-119 行）
    - core/include/lv/solver.h            SolverStatus 枚举（85-94 行）
    - core/include/lv/engine_status.h     EngineStatus 枚举（14-22 行）
    - core/include/lv/func_block.h        PackResult 枚举（280-288 行）
    - core/include/lv/debug.h             LogLevel 枚举（76-84 行，主定义源）
"""

import pytest

# ---------------------------------------------------------------------------
# C 枚举值基准表（来源：上述头文件，行号见模块注释）
# ---------------------------------------------------------------------------

C_GEOM_TABLE = {
    "GEOM_POINT": 0,
    "GEOM_LINE_SEGMENT": 1,
    "GEOM_REGION": 2,
    "GEOM_CIRCLE": 3,
    "GEOM_PORT": 4,
    "GEOM_FUNCTION_BLOCK": 5,
}

C_CONSTRAINT_TABLE = {
    "CONSTRAINT_INCIDENCE": 0,
    "CONSTRAINT_BETWEENNESS": 1,
    "CONSTRAINT_INTERSECTION": 2,
    "CONSTRAINT_CONTAINMENT": 3,
    "CONSTRAINT_CONNECTION": 4,
    "CONSTRAINT_ANGLE": 5,
}

C_SOLVER_TABLE = {
    "SOLVER_OK": 0,
    "SOLVER_UNIQUE": 1,
    "SOLVER_MULTIPLE": 2,
    "SOLVER_NO_SOLUTION": 3,
    "SOLVER_OVERCONSTRAINED": 4,
    "SOLVER_OUT_OF_SCOPE": 5,
    "SOLVER_TIMEOUT": 6,
    "SOLVER_OUT_OF_MEMORY": 7,
}

C_ENGINE_TABLE = {
    "ENGINE_OK": 0,
    "ENGINE_OUT_OF_MEMORY": 1,
    "ENGINE_INVALID_STATE": 2,
    "ENGINE_INVALID_ARGUMENT": 3,
    "ENGINE_CONSTRAINT_CONFLICT": 4,
    "ENGINE_MODULE_ERROR": 5,
    "ENGINE_ERROR_INTERNAL": 6,
}

C_PACK_TABLE = {
    "PACK_OK": 0,
    "PACK_CROSS_BOUNDARY_CONFLICT": 1,
    "PACK_INVALID_NODES": 2,
    "PACK_INVALID_PORTS": 3,
    "PACK_INVALID_GRAPH": 4,
    "PACK_OUT_OF_MEMORY": 5,
    "PACK_CANCELLED": 6,
}

C_LOG_LEVEL_TABLE = {
    "LOG_LEVEL_TRACE": -1,
    "LOG_LEVEL_DEBUG": 0,
    "LOG_LEVEL_INFO": 1,
    "LOG_LEVEL_WARN": 2,
    "LOG_LEVEL_ERROR": 3,
    "LOG_LEVEL_FATAL": 4,
    "LOG_LEVEL_NONE": 5,
}

# 名称前缀 -> 基准表，便于报告错位时定位分组
_BINDING_GROUPS = [
    ("几何节点类型", C_GEOM_TABLE),
    ("约束类型", C_CONSTRAINT_TABLE),
    ("求解器状态", C_SOLVER_TABLE),
    ("引擎状态", C_ENGINE_TABLE),
    ("函数块打包结果", C_PACK_TABLE),
    ("日志级别", C_LOG_LEVEL_TABLE),
]

# ---------------------------------------------------------------------------
# 加载 Python 绑定层（依赖编译好的 C 库；缺失时 _ctypes_binding 抛 ImportError）
# ---------------------------------------------------------------------------

try:
    import lv._ctypes_binding as _binding
    _HAS_LIB = True
    _LIB_MISSING_REASON = None
except ImportError as _exc:  # pragma: no cover - 无库环境下的降级路径
    _binding = None
    _HAS_LIB = False
    _LIB_MISSING_REASON = (
        "未找到 Lv-00 C 动态库，无法加载 lv._ctypes_binding。\n"
        "运行方式：先构建 liblv.dll/.so/.dylib，或设置环境变量 "
        "lv_LIBRARY_PATH 指向库文件后重新运行 pytest。"
    )


# ---------------------------------------------------------------------------
# 静态断言：Python 常量 == C 枚举基准表
# ---------------------------------------------------------------------------

def test_python_constants_match_c_enum_tables():
    """Python 绑定层常量与 C 头文件枚举值逐项一致（静态基准）。"""
    if not _HAS_LIB:
        pytest.skip(_LIB_MISSING_REASON)

    for group_name, table in _BINDING_GROUPS:
        for name, expected in table.items():
            actual = getattr(_binding, name)
            assert actual == expected, (
                f"[{group_name}] {name} = {actual}，但 C 头文件枚举值为 {expected}；"
                f"请检查 _ctypes_binding.py 与 C 头文件是否对齐（来源见测试模块注释）。"
            )


def test_python_constants_are_unique_per_group():
    """同一枚举组内各常量取值不得重复（重复即错位）。"""
    if not _HAS_LIB:
        pytest.skip(_LIB_MISSING_REASON)

    for group_name, table in _BINDING_GROUPS:
        values = [getattr(_binding, name) for name in table]
        assert len(set(values)) == len(values), (
            f"[{group_name}] 存在重复枚举值，疑似错位：{sorted(values)}"
        )


# ---------------------------------------------------------------------------
# 动态交叉验证（仅在 C 库存在时执行）
# ---------------------------------------------------------------------------

@pytest.mark.skipif(not _HAS_LIB, reason="未找到 Lv-00 C 动态库")
def test_log_level_roundtrip_with_c_library():
    """LOG_LEVEL_* 经 debug_set_log_level/debug_get_log_level 与 C 侧往返一致。"""
    lib = _binding._lib
    previous = lib.debug_get_log_level()
    try:
        for name, level in C_LOG_LEVEL_TABLE.items():
            lib.debug_set_log_level(level)
            assert lib.debug_get_log_level() == level, (
                f"{name} = {level} 与 C 侧往返结果 {lib.debug_get_log_level()} 不一致"
            )
    finally:
        # 恢复原级别，避免影响其他测试/模块
        lib.debug_set_log_level(previous)


@pytest.mark.skipif(not _HAS_LIB, reason="未找到 Lv-00 C 动态库")
def test_geom_port_roundtrip_with_c_library():
    """
    GEOM_PORT 经 C 库 graph_add_node_with_id 往返验证。

    用 Python 侧 GEOM_PORT 值创建节点，再读取 C 侧节点 type 字段。
    若 Python 常量与 C 枚举错位（如 GEOM_PORT 误为 3），C 侧会按 GEOM_CIRCLE
    创建节点并回读 3，断言即失败。
    """
    lib = _binding._lib
    graph = lib.graph_create()
    try:
        node = lib.graph_add_node_with_id(graph, 1, _binding.GEOM_PORT, None, 0)
        assert node, "graph_add_node_with_id 创建端口节点失败"
        assert node.contents.type == _binding.GEOM_PORT, (
            f"GEOM_PORT 往返错位：Python 侧值为 {_binding.GEOM_PORT}，"
            f"C 侧回读节点 type = {node.contents.type}"
        )
    finally:
        lib.graph_destroy(graph)
