"""
Lv-00 底层 C 库 ctypes 绑定模块

负责加载 Lv-00 动态链接库并定义所有 C 函数的签名。
本模块是 Python 层与 C 层之间的桥梁，通常不应直接使用。

功能：
    - 自动搜索并加载平台对应的共享库（.dll/.so/.dylib）
    - 定义 C 结构体类型（SymbolicCoord、ConstraintGraph 等）
    - 注册所有 C 函数的参数类型和返回值类型
    - 定义常量（结果码、几何类型枚举）

环境变量：
    LV00_LIBRARY_PATH: 可手动指定库文件路径

作者：Lv-00 开发团队
版本：3.0.1
"""

import ctypes
import os
import sys
from ctypes import c_int, c_int64, c_uint64, c_double, c_char_p, c_void_p, c_bool, POINTER, CFUNCTYPE

# ============================================================
# 库文件搜索函数
# ============================================================

def _find_library():
    """
    搜索并定位 Lv-00 共享库文件。

    搜索策略（按优先级排序）：
    1. 环境变量 LV00_LIBRARY_PATH（最高优先级）
    2. 平台特定的标准库名称（win32: .dll, darwin: .dylib, linux: .so）
    3. 预定义的搜索路径列表（包目录、构建目录、系统库目录）
    4. Windows 上从 PATH 环境变量搜索

    返回：
        str: 库文件的完整路径

    异常：
        ImportError: 无法找到库文件时抛出，附带搜索路径信息
    """
    # 优先检查环境变量
    if 'LV00_LIBRARY_PATH' in os.environ:
        lib_path = os.environ['LV00_LIBRARY_PATH']
        if os.path.exists(lib_path):
            return lib_path
        raise ImportError(f"LV00_LIBRARY_PATH 指定的文件不存在: {lib_path}")
    
    # 平台特定的库名称
    # Windows 优先搜索 liblv00.dll（符合 GNU 命名惯例），其次为 lv00.dll（短名称）
    if sys.platform == 'win32':
        names = ['liblv00.dll', 'lv00.dll']
    elif sys.platform == 'darwin':
        names = ['liblv00.dylib', 'lv00.dylib']
    else:
        names = ['liblv00.so', 'lv00.so']

    # 搜索路径列表
    # 搜索优先级：包目录 > 父目录 > 构建输出目录 > 项目根目录
    # 仅包含当前平台有效的路径，避免无意义的文件系统调用
    package_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    search_paths = [
        package_dir,  # python/lv00/
        os.path.join(package_dir, '..'),  # python/
        os.path.join(package_dir, '..', '..', 'build'),  # project_root/build
        os.path.join(package_dir, '..', '..', 'build', 'Release'),  # Windows 构建目录
        os.path.join(package_dir, '..', '..'),  # project_root
    ]
    # 仅在非 Windows 平台添加 Unix 风格的库路径
    if sys.platform != 'win32':
        search_paths.extend(['/usr/local/lib', '/usr/lib'])

    # 在所有路径中搜索
    for path in search_paths:
        for name in names:
            full_path = os.path.join(path, name)
            if os.path.exists(full_path):
                return full_path

    # Windows 上尝试从 PATH 搜索
    if sys.platform == 'win32':
        for name in names:
            try:
                ctypes.CDLL(name)
                return name
            except OSError:
                continue

    # 构建详细错误信息，列出所有尝试过的搜索位置
    searched_locations = []
    for path in search_paths:
        for name in names:
            searched_locations.append(os.path.join(path, name))
    searched_locations_str = '\n    '.join(searched_locations)
    raise ImportError(
        f"无法找到 Lv-00 库文件。\n"
        f"已尝试以下位置：\n"
        f"    {searched_locations_str}\n"
        f"解决方法：\n"
        f"  1. 设置 LV00_LIBRARY_PATH 环境变量指向库文件\n"
        f"  2. 将库文件放入上述搜索路径之一\n"
        f"  3. 将库文件目录添加到系统 PATH 环境变量"
    )

# ============================================================
# 库加载
# ============================================================

try:
    _lib_path = _find_library()
    _lib = ctypes.CDLL(_lib_path)
except OSError as e:
    # 加载失败时提供更详细的帮助信息
    raise ImportError(
        f"加载 Lv-00 动态链接库失败。\n"
        f"库路径: {_find_library.__wrapped__ if hasattr(_find_library, '__wrapped__') else '未知'}\n"
        f"系统错误: {e}\n"
        f"可能原因：\n"
        f"  1. 库文件已损坏或与当前 Python 版本不兼容\n"
        f"  2. 缺少必要的运行时依赖（如 MSVC 运行时）\n"
        f"  3. 库文件位数与 Python 位数不匹配（32位/64位）\n"
        f"请检查 LV00_LIBRARY_PATH 环境变量设置，或重新编译 Lv-00。"
    ) from e
except ImportError:
    raise
except Exception as e:
    raise ImportError(f"加载 Lv-00 库时发生未预期的错误: {e}") from e

# ============================================================
# C 结构体类型定义
# ============================================================
# 以下结构体均为 C 层对应类型的 ctypes 占位符。
# 它们被声明为不透明类型（opaque type）——只定义类型名，
# 不暴露内部字段。所有字段访问通过 C 函数接口完成。
# 这样设计的好处：
#   1. 二进制兼容：C 结构体内部布局变化不影响 Python 代码
#   2. 内存安全：Python 无法直接读写敏感字段
#   3. 解耦版本：Python 绑定与 C 库版本可以独立迭代

class _SymbolicCoord(ctypes.Structure):
    """
    符号坐标的 C 结构体占位符。
    
    对应 C 层 SymbolicCoord 类型，用于表示精确的符号数值。
    支持有理数、代数数、二次根式和超越数四种表示形式。
    通过 _lib.symbolic_coord_* 系列函数进行操作。
    """
    pass

class _ConstraintGraph(ctypes.Structure):
    """
    约束图的 C 结构体占位符。
    
    对应 C 层 ConstraintGraph 类型，包含几何节点和约束关系的完整有向图。
    通过 _lib.graph_* 系列函数进行节点增删、约束管理等操作。
    """
    pass

class _NormalizationResult(ctypes.Structure):
    """
    规范化结果的 C 结构体占位符。
    
    对应 C 层 NormalizationResult 类型，包含图规范化操作的统计信息，
    如合并的等价节点数量、化简的约束数量等。
    通过 _lib.normalization_result_destroy() 释放。
    """
    pass

class _GeomNode(ctypes.Structure):
    """
    几何节点的 C 结构体占位符。
    
    对应 C 层 GeomNode 类型，表示约束图中的一个几何元素。
    包含节点 ID（id 字段）和几何类型（type 字段，如 GEOM_POINT）。
    通过 _lib.graph_get_node() 获取。
    """
    pass

class _Constraint(ctypes.Structure):
    """
    约束的 C 结构体占位符。
    
    对应 C 层 Constraint 类型，表示两个或多个几何节点之间的约束关系。
    包含约束类型（如关联、介子、交点等）和参与节点的 ID。
    """
    pass

class _FuncBlock(ctypes.Structure):
    """
    函数块的 C 结构体占位符。
    
    对应 C 层 FuncBlock 类型，封装了可重用的几何构造模板。
    包含内部节点、输入/输出端口、确定性状态等信息。
    通过 _lib.func_block_* 系列函数进行打包、实例化和确定性检查。
    """
    pass

class _ProofNavigator(ctypes.Structure):
    """
    证明导航器的 C 结构体占位符。
    
    对应 C 层 ProofNavigator 类型，用于遍历和操作证明树结构。
    支持前进（next）、后退（prev）、跳转（goto）等导航操作，
    并可以导出为 HTML/LaTeX 格式的证明文档。
    """
    pass

class _Proposition(ctypes.Structure):
    """
    命题的 C 结构体占位符。
    
    对应 C 层 Proposition 类型，表示一个逻辑命题。
    支持原子命题、合取、析取、蕴含、否定等多种命题形式。
    通过 _lib.proposition_create() 创建，_lib.proposition_destroy() 释放。
    """
    pass

class _LV00Engine(ctypes.Structure):
    """
    引擎的 C 结构体占位符。
    
    对应 C 层 LV00Engine 类型，是 Lv-00 系统的主引擎。
    协调约束图管理、模块加载、函数打包/实例化、求解和重写等核心功能。
    通过 _lib.engine_create() 创建，_lib.engine_destroy() 释放。
    """
    pass

class _MeasureSystem(ctypes.Structure):
    """
    测度系统的 C 结构体占位符。
    
    对应 C 层 MeasureSystem 类型，用于管理递归终止测度。
    包含测度集合和默认测度，支持测度的创建、添加和比较操作。
    通过 _lib.measure_system_create() 创建，_lib.measure_system_destroy() 释放。
    """
    pass

class _RecursionContext(ctypes.Structure):
    """
    递归上下文的 C 结构体占位符。
    
    对应 C 层 RecursionContext 类型，用于管理递归调用的上下文状态。
    跟踪递归深度、检查终止测度、检测循环依赖。
    通过 _lib.recursion_context_create() 创建，_lib.recursion_context_destroy() 释放。
    """
    pass

class _Port(ctypes.Structure):
    """
    端口的 C 结构体占位符。
    
    对应 C 层 Port 类型，表示函数块的输入或输出端口。
    端口是函数块系统的核心概念，用于定义函数块的参数接口。
    通过 _lib.graph_add_port() 添加到约束图中。
    """
    pass

# ============================================================
# SymbolicCoord 函数签名
# ============================================================
# 以下函数用于创建、销毁、序列化和操作符号坐标。
# 符号坐标是 Lv-00 精确计算的基础——所有几何坐标
# 都以符号形式存储，避免浮点数精度损失。

# 创建有理数坐标：numerator/denominator
_lib.symbolic_coord_create_rational.argtypes = [c_int64, c_uint64]
_lib.symbolic_coord_create_rational.restype = POINTER(_SymbolicCoord)

# 从 GMP 大整数（mpz）创建坐标，用于处理超过 64 位的精确整数
_lib.symbolic_coord_create_from_mpz.argtypes = [c_void_p, c_void_p]
_lib.symbolic_coord_create_from_mpz.restype = POINTER(_SymbolicCoord)

# 销毁符号坐标对象，释放所有关联的内存资源
_lib.symbolic_coord_destroy.argtypes = [POINTER(_SymbolicCoord)]
_lib.symbolic_coord_destroy.restype = None

# 序列化为字符串（如 "3/4"），返回的字符串需要调用 free() 释放
_lib.symbolic_coord_serialize.argtypes = [POINTER(_SymbolicCoord)]
_lib.symbolic_coord_serialize.restype = c_char_p

# 从字符串反序列化创建坐标，支持 "3/4"、"1.5" 等格式
_lib.symbolic_coord_deserialize.argtypes = [c_char_p]
_lib.symbolic_coord_deserialize.restype = POINTER(_SymbolicCoord)

# 深拷贝符号坐标对象
_lib.symbolic_coord_copy.argtypes = [POINTER(_SymbolicCoord)]
_lib.symbolic_coord_copy.restype = POINTER(_SymbolicCoord)

# 加法：a + b，返回新的坐标对象（调用者负责释放）
_lib.symbolic_coord_add.argtypes = [POINTER(_SymbolicCoord), POINTER(_SymbolicCoord)]
_lib.symbolic_coord_add.restype = POINTER(_SymbolicCoord)

# 减法：a - b
_lib.symbolic_coord_subtract.argtypes = [POINTER(_SymbolicCoord), POINTER(_SymbolicCoord)]
_lib.symbolic_coord_subtract.restype = POINTER(_SymbolicCoord)

# 乘法：a * b
_lib.symbolic_coord_multiply.argtypes = [POINTER(_SymbolicCoord), POINTER(_SymbolicCoord)]
_lib.symbolic_coord_multiply.restype = POINTER(_SymbolicCoord)

# 除法：a / b，除数为零时返回 NULL
_lib.symbolic_coord_divide.argtypes = [POINTER(_SymbolicCoord), POINTER(_SymbolicCoord)]
_lib.symbolic_coord_divide.restype = POINTER(_SymbolicCoord)

# 比较两个坐标：返回 0（相等）、负数（a < b）或正数（a > b）
_lib.symbolic_coord_compare.argtypes = [POINTER(_SymbolicCoord), POINTER(_SymbolicCoord)]
_lib.symbolic_coord_compare.restype = c_int

# 判断是否为零
_lib.symbolic_coord_is_zero.argtypes = [POINTER(_SymbolicCoord)]
_lib.symbolic_coord_is_zero.restype = c_bool

# 判断是否为正数（> 0）
_lib.symbolic_coord_is_positive.argtypes = [POINTER(_SymbolicCoord)]
_lib.symbolic_coord_is_positive.restype = c_bool

# 判断是否为负数（< 0）
_lib.symbolic_coord_is_negative.argtypes = [POINTER(_SymbolicCoord)]
_lib.symbolic_coord_is_negative.restype = c_bool

# 取负：-a
_lib.symbolic_coord_negate.argtypes = [POINTER(_SymbolicCoord)]
_lib.symbolic_coord_negate.restype = POINTER(_SymbolicCoord)

# 转换为双精度浮点数（可能丢失精度）
_lib.symbolic_coord_to_double.argtypes = [POINTER(_SymbolicCoord)]
_lib.symbolic_coord_to_double.restype = c_double

# 计算哈希值，用于 dict/set 等哈希容器
_lib.symbolic_coord_hash.argtypes = [POINTER(_SymbolicCoord)]
_lib.symbolic_coord_hash.restype = c_uint64

# ============================================================
# ConstraintGraph 函数签名
# ============================================================
# 以下函数用于创建、管理和查询约束图。
# 约束图是 Lv-00 的核心数据结构——所有几何构造都以图的形式存储，
# 节点表示几何元素（点、线段、区域等），边表示约束关系。

# 创建空约束图
_lib.graph_create.argtypes = []
_lib.graph_create.restype = POINTER(_ConstraintGraph)

# 销毁约束图，释放所有节点和约束的内存
_lib.graph_destroy.argtypes = [POINTER(_ConstraintGraph)]
_lib.graph_destroy.restype = None

# 获取图中的节点总数
_lib.graph_get_node_count.argtypes = [POINTER(_ConstraintGraph)]
_lib.graph_get_node_count.restype = c_int

# 获取图中的约束总数
_lib.graph_get_constraint_count.argtypes = [POINTER(_ConstraintGraph)]
_lib.graph_get_constraint_count.restype = c_int

# 根据节点 ID 获取节点对象（只读），返回 NULL 表示未找到
_lib.graph_get_node.argtypes = [POINTER(_ConstraintGraph), c_int]
_lib.graph_get_node.restype = POINTER(_GeomNode)

# 向图中添加一个点节点，coords 为 pair 坐标数组（x, y）
_lib.graph_add_point.argtypes = [POINTER(_ConstraintGraph), POINTER(POINTER(_SymbolicCoord)), c_int]
_lib.graph_add_point.restype = c_int

# 添加线段节点，由两个已有点 ID 定义
_lib.graph_add_line_segment.argtypes = [POINTER(_ConstraintGraph), c_int, c_int]
_lib.graph_add_line_segment.restype = c_int

# 添加区域节点，由边界线段 ID 列表定义
_lib.graph_add_region.argtypes = [POINTER(_ConstraintGraph), POINTER(c_int), c_int]
_lib.graph_add_region.restype = c_int

# 添加端口节点，用于函数块系统的输入/输出接口
_lib.graph_add_port.argtypes = [POINTER(_ConstraintGraph), c_int, c_int, c_int]
_lib.graph_add_port.restype = c_int

# 添加函数块节点，定义可重用的几何构造模板
_lib.graph_add_function_block.argtypes = [POINTER(_ConstraintGraph), POINTER(c_int), c_int, POINTER(c_int), c_int, POINTER(c_int), c_int]
_lib.graph_add_function_block.restype = c_int

# 添加关联约束：点位于线段或区域上
_lib.graph_add_incidence.argtypes = [POINTER(_ConstraintGraph), c_int, c_int]
_lib.graph_add_incidence.restype = c_int

# 添加介子约束：三点共线且中间点在两端点之间
_lib.graph_add_betweenness.argtypes = [POINTER(_ConstraintGraph), c_int, c_int, c_int]
_lib.graph_add_betweenness.restype = c_int

# 添加交点约束：两条线段交于一点
_lib.graph_add_intersection.argtypes = [POINTER(_ConstraintGraph), c_int, c_int, c_int]
_lib.graph_add_intersection.restype = c_int

# 添加包含约束：一个区域包含在另一个区域内
_lib.graph_add_containment.argtypes = [POINTER(_ConstraintGraph), c_int, c_int]
_lib.graph_add_containment.restype = c_int

# 添加连接约束：端口之间的数据流连接
_lib.graph_add_connection.argtypes = [POINTER(_ConstraintGraph), c_int, c_int]
_lib.graph_add_connection.restype = c_int

# 从图中移除指定 ID 的节点及其关联约束
_lib.graph_remove_node.argtypes = [POINTER(_ConstraintGraph), c_int]
_lib.graph_remove_node.restype = c_int

# 从图中移除指定 ID 的约束
_lib.graph_remove_constraint.argtypes = [POINTER(_ConstraintGraph), c_int]
_lib.graph_remove_constraint.restype = c_int

# 规范化约束图：合并等价节点、消除冗余约束
_lib.graph_normalize.argtypes = [POINTER(_ConstraintGraph), c_bool]
_lib.graph_normalize.restype = POINTER(_NormalizationResult)

# 检测冗余约束：返回可以安全移除的约束 ID 列表
_lib.graph_detect_redundant_constraints.argtypes = [POINTER(_ConstraintGraph), POINTER(c_int)]
_lib.graph_detect_redundant_constraints.restype = POINTER(c_int)

# 检测冲突约束组：返回互相冲突的约束集合
_lib.graph_detect_conflicts.argtypes = [POINTER(_ConstraintGraph), POINTER(c_int), POINTER(POINTER(c_int))]
_lib.graph_detect_conflicts.restype = POINTER(POINTER(c_int))

# 验证区域的闭合性：检查边界线段是否构成闭合环
_lib.graph_validate_region_closure.argtypes = [POINTER(_ConstraintGraph), c_int]
_lib.graph_validate_region_closure.restype = c_bool

# ============================================================
# NormalizationResult 函数签名
# ============================================================

_lib.normalization_result_destroy.argtypes = [POINTER(_NormalizationResult)]
_lib.normalization_result_destroy.restype = None

# ============================================================
# FuncBlock 函数签名
# ============================================================

_lib.func_block_create.argtypes = [c_int]
_lib.func_block_create.restype = POINTER(_FuncBlock)

_lib.func_block_destroy.argtypes = [POINTER(_FuncBlock)]
_lib.func_block_destroy.restype = None

_lib.func_block_pack.argtypes = [
    POINTER(_ConstraintGraph),
    POINTER(c_int), c_int,
    POINTER(c_int), c_int,
    POINTER(c_int), c_int,
    POINTER(c_int), c_int,
    POINTER(POINTER(_FuncBlock))
]
_lib.func_block_pack.restype = c_int

_lib.func_block_instantiate.argtypes = [
    POINTER(_FuncBlock),
    POINTER(_ConstraintGraph),
    POINTER(c_int), c_int,
    POINTER(POINTER(c_int)), POINTER(c_int)
]
_lib.func_block_instantiate.restype = c_int

_lib.func_block_determinism_check_static.argtypes = [POINTER(_FuncBlock), POINTER(_ConstraintGraph)]
_lib.func_block_determinism_check_static.restype = c_int

_lib.func_block_determinism_check_dynamic.argtypes = [POINTER(_FuncBlock), POINTER(_ConstraintGraph), POINTER(POINTER(_SymbolicCoord)), c_int]
_lib.func_block_determinism_check_dynamic.restype = c_int

_lib.func_block_verify_determinism.argtypes = [POINTER(_FuncBlock), POINTER(_ConstraintGraph), c_int]
_lib.func_block_verify_determinism.restype = c_int

# 选择器相关
_lib.selector_create.argtypes = [c_int]
_lib.selector_create.restype = c_void_p

_lib.selector_create_with_reference.argtypes = [c_int, c_int]
_lib.selector_create_with_reference.restype = c_void_p

_lib.selector_destroy.argtypes = [c_void_p]
_lib.selector_destroy.restype = None

_lib.selector_apply.argtypes = [c_void_p, POINTER(POINTER(_GeomNode)), c_int, POINTER(c_int)]
_lib.selector_apply.restype = c_bool

# ============================================================
# Proof 函数签名
# ============================================================

_lib.proposition_create.argtypes = [c_int, c_int]
_lib.proposition_create.restype = POINTER(_Proposition)

_lib.proposition_destroy.argtypes = [POINTER(_Proposition)]
_lib.proposition_destroy.restype = None

_lib.proof_navigator_create.argtypes = [POINTER(_Proposition)]
_lib.proof_navigator_create.restype = POINTER(_ProofNavigator)

_lib.proof_navigator_destroy.argtypes = [POINTER(_ProofNavigator)]
_lib.proof_navigator_destroy.restype = None

_lib.proof_unify.argtypes = [POINTER(_ConstraintGraph), POINTER(_Proposition), c_bool]
_lib.proof_unify.restype = c_int

_lib.proof_create_ex_falso_block.argtypes = [POINTER(_ConstraintGraph), POINTER(c_int)]
_lib.proof_create_ex_falso_block.restype = c_bool

_lib.proof_apply_ex_falso.argtypes = [POINTER(_ProofNavigator), POINTER(_ConstraintGraph), POINTER(_Proposition)]
_lib.proof_apply_ex_falso.restype = c_bool

_lib.proof_navigator_next.argtypes = [POINTER(_ProofNavigator)]
_lib.proof_navigator_next.restype = c_bool

_lib.proof_navigator_prev.argtypes = [POINTER(_ProofNavigator)]
_lib.proof_navigator_prev.restype = c_bool

_lib.proof_navigator_goto.argtypes = [POINTER(_ProofNavigator), c_int]
_lib.proof_navigator_goto.restype = c_bool

_lib.proof_navigator_compute_final_color.argtypes = [POINTER(_ProofNavigator)]
_lib.proof_navigator_compute_final_color.restype = c_int

_lib.proof_export_html.argtypes = [POINTER(_ProofNavigator), c_char_p]
_lib.proof_export_html.restype = c_bool

_lib.proof_export_latex.argtypes = [POINTER(_ProofNavigator), c_char_p]
_lib.proof_export_latex.restype = c_bool

# ============================================================
# Recursion 函数签名
# ============================================================

_lib.measure_system_create.argtypes = []
_lib.measure_system_create.restype = POINTER(_MeasureSystem)

_lib.measure_system_destroy.argtypes = [POINTER(_MeasureSystem)]
_lib.measure_system_destroy.restype = None

_lib.measure_create_symbolic.argtypes = [c_char_p, c_int, c_int]
_lib.measure_create_symbolic.restype = c_void_p

_lib.measure_destroy.argtypes = [c_void_p]
_lib.measure_destroy.restype = None

_lib.measure_system_add.argtypes = [POINTER(_MeasureSystem), c_void_p]
_lib.measure_system_add.restype = c_bool

_lib.measure_system_set_default.argtypes = [POINTER(_MeasureSystem), c_void_p]
_lib.measure_system_set_default.restype = None

_lib.measure_compute_value.argtypes = [c_void_p, POINTER(_GeomNode), POINTER(_ConstraintGraph)]
_lib.measure_compute_value.restype = POINTER(_SymbolicCoord)

_lib.measure_compute_value_symbolic.argtypes = [c_void_p, POINTER(_GeomNode), POINTER(_ConstraintGraph)]
_lib.measure_compute_value_symbolic.restype = POINTER(_SymbolicCoord)

_lib.measure_compare.argtypes = [c_void_p, POINTER(_SymbolicCoord), POINTER(_SymbolicCoord)]
_lib.measure_compare.restype = c_int

_lib.recursion_context_create.argtypes = [c_int]
_lib.recursion_context_create.restype = POINTER(_RecursionContext)

_lib.recursion_context_destroy.argtypes = [POINTER(_RecursionContext)]
_lib.recursion_context_destroy.restype = None

_lib.recursion_context_enter.argtypes = [POINTER(_RecursionContext), c_int, POINTER(_GeomNode), POINTER(_ConstraintGraph)]
_lib.recursion_context_enter.restype = c_int

_lib.recursion_context_exit.argtypes = [POINTER(_RecursionContext)]
_lib.recursion_context_exit.restype = None

_lib.recursion_context_get_depth.argtypes = [POINTER(_RecursionContext)]
_lib.recursion_context_get_depth.restype = c_int

_lib.recursion_context_reset.argtypes = [POINTER(_RecursionContext)]
_lib.recursion_context_reset.restype = None

_lib.recursion_check_mutual.argtypes = [POINTER(c_int), c_int, POINTER(_MeasureSystem)]
_lib.recursion_check_mutual.restype = c_bool

_lib.recursion_run_builtin_tests.argtypes = [POINTER(_MeasureSystem), POINTER(POINTER(c_void_p)), POINTER(c_int)]
_lib.recursion_run_builtin_tests.restype = c_int

# ============================================================
# Engine 函数签名
# ============================================================
# 以下函数用于创建和操作 Lv-00 主引擎。
# 引擎是系统的顶层协调器，管理约束图、模块加载、
# 函数打包/实例化、求解和重写等核心功能。

# 创建新的引擎实例
_lib.engine_create.argtypes = []
_lib.engine_create.restype = POINTER(_LV00Engine)

# 销毁引擎实例，释放所有关联资源
_lib.engine_destroy.argtypes = [POINTER(_LV00Engine)]
_lib.engine_destroy.restype = None

# 执行求解：重写 -> 求解 -> 冲突检查
_lib.engine_solve.argtypes = [POINTER(_LV00Engine)]
_lib.engine_solve.restype = c_int

# 执行重写-求解协作流程，max_rewrite_steps 和 max_solve_steps 控制步数上限
_lib.engine_rewrite_and_solve.argtypes = [POINTER(_LV00Engine), c_int, c_int]
_lib.engine_rewrite_and_solve.restype = c_int

# 设置重写步数上限（单次重写的最大步数）
_lib.engine_set_rewrite_step_limit.argtypes = [POINTER(_LV00Engine), c_int]
_lib.engine_set_rewrite_step_limit.restype = None

# 获取当前重写步数上限
_lib.engine_get_rewrite_step_limit.argtypes = [POINTER(_LV00Engine)]
_lib.engine_get_rewrite_step_limit.restype = c_int

# 加载模块文件（.lv00 格式），将模块内容加载到引擎的约束图中
_lib.engine_load_module.argtypes = [POINTER(_LV00Engine), c_char_p]
_lib.engine_load_module.restype = c_int

# 加载公理包文件，添加公理到引擎的规则库中
_lib.engine_load_axiom_package.argtypes = [POINTER(_LV00Engine), c_char_p]
_lib.engine_load_axiom_package.restype = c_int

# 打包函数块：将内部节点和端口封装为可重用的函数模板
_lib.engine_pack_function.argtypes = [
    POINTER(_LV00Engine),
    POINTER(c_int), c_int,
    POINTER(c_int), c_int,
    POINTER(c_int), c_int,
    POINTER(c_int)
]
_lib.engine_pack_function.restype = c_bool

# 实例化函数块：使用实参替换形式参数创建函数实例
_lib.engine_instantiate_function.argtypes = [
    POINTER(_LV00Engine), c_int,
    POINTER(c_int), c_int,
    POINTER(c_int)
]
_lib.engine_instantiate_function.restype = POINTER(c_int)

# 执行合一检查：检查构造图是否满足命题模式
_lib.engine_unify.argtypes = [POINTER(_LV00Engine), POINTER(_ConstraintGraph), POINTER(_ConstraintGraph)]
_lib.engine_unify.restype = c_int

# 处理位电路跳闸事件（默认恢复策略）
_lib.engine_handle_circuit_trip.argtypes = [POINTER(_LV00Engine)]
_lib.engine_handle_circuit_trip.restype = c_int

# 使用指定动作处理位电路跳闸
_lib.engine_handle_circuit_trip_with_action.argtypes = [POINTER(_LV00Engine), c_int]
_lib.engine_handle_circuit_trip_with_action.restype = c_int

# 创建冻结点快照，保存当前引擎状态用于后续回滚
_lib.engine_create_frozen_point.argtypes = [POINTER(_LV00Engine)]
_lib.engine_create_frozen_point.restype = c_void_p

# 恢复到冻结点状态，丢弃快照之后的所有修改
_lib.engine_restore_frozen_point.argtypes = [POINTER(_LV00Engine), c_void_p]
_lib.engine_restore_frozen_point.restype = c_bool

# 销毁冻结点快照，释放占用的内存
_lib.engine_destroy_frozen_point.argtypes = [c_void_p]
_lib.engine_destroy_frozen_point.restype = None

# 获取引擎最后一次操作的状态码
_lib.engine_get_last_status.argtypes = [POINTER(_LV00Engine)]
_lib.engine_get_last_status.restype = c_int

# 获取引擎最后一次操作的错误消息
_lib.engine_get_last_error.argtypes = [POINTER(_LV00Engine)]
_lib.engine_get_last_error.restype = c_char_p

# ============================================================
# Debug 函数签名
# ============================================================

_lib.debug_log_init.argtypes = []
_lib.debug_log_init.restype = c_int

_lib.debug_log_shutdown.argtypes = []
_lib.debug_log_shutdown.restype = None

_lib.debug_set_log_level.argtypes = [c_int]
_lib.debug_set_log_level.restype = None

_lib.debug_get_log_level.argtypes = []
_lib.debug_get_log_level.restype = c_int

_lib.debug_set_mode.argtypes = [c_bool]
_lib.debug_set_mode.restype = None

_lib.debug_is_debug_mode.argtypes = []
_lib.debug_is_debug_mode.restype = c_bool

_lib.debug_reset_counters.argtypes = []
_lib.debug_reset_counters.restype = None

_lib.debug_get_counters.argtypes = [c_void_p]
_lib.debug_get_counters.restype = None

_lib.debug_counters_report.argtypes = []
_lib.debug_counters_report.restype = c_char_p

_lib.mem_pool_create.argtypes = [ctypes.c_size_t, c_int]
_lib.mem_pool_create.restype = c_void_p

_lib.mem_pool_destroy.argtypes = [c_void_p]
_lib.mem_pool_destroy.restype = None

# ============================================================
# Unify 函数签名
# ============================================================

_lib.unify_check.argtypes = [POINTER(_ConstraintGraph), POINTER(_ConstraintGraph)]
_lib.unify_check.restype = c_int

_lib.unify_detailed.argtypes = [POINTER(_ConstraintGraph), POINTER(_ConstraintGraph), POINTER(c_char_p)]
_lib.unify_detailed.restype = c_int

# ============================================================
# Rewrite 函数签名
# ============================================================

_lib.rewrite_create_rule.argtypes = [c_char_p, c_char_p, c_int]
_lib.rewrite_create_rule.restype = c_void_p

_lib.rewrite_destroy_rule.argtypes = [c_void_p]
_lib.rewrite_destroy_rule.restype = None

_lib.rewrite_add_rule.argtypes = [POINTER(_ConstraintGraph), c_void_p]
_lib.rewrite_add_rule.restype = c_bool

_lib.rewrite_rewrite.argtypes = [POINTER(_ConstraintGraph), c_int]
_lib.rewrite_rewrite.restype = c_int

_lib.rewrite_rewrite_until.argtypes = [POINTER(_ConstraintGraph), c_int, c_int]
_lib.rewrite_rewrite_until.restype = c_int

# ============================================================
# Solver 函数签名
# ============================================================

_lib.solve_algebraic_system.argtypes = [POINTER(_ConstraintGraph), POINTER(c_int), c_int, POINTER(POINTER(_SymbolicCoord))]
_lib.solve_algebraic_system.restype = c_int

_lib.count_degrees_of_freedom.argtypes = [POINTER(_ConstraintGraph), POINTER(POINTER(c_int))]
_lib.count_degrees_of_freedom.restype = c_int

_lib.check_conflict_equations.argtypes = [POINTER(_ConstraintGraph)]
_lib.check_conflict_equations.restype = c_bool

_lib.eliminate_geometry.argtypes = [POINTER(_ConstraintGraph), c_int, POINTER(c_int), c_int]
_lib.eliminate_geometry.restype = c_int

# ============================================================
# 辅助函数
# ============================================================

_lib.free.argtypes = [c_void_p]
_lib.free.restype = None

_lib.lv00_init.argtypes = []
_lib.lv00_init.restype = c_bool

_lib.lv00_cleanup.argtypes = []
_lib.lv00_cleanup.restype = None

_lib.lv00_get_version.argtypes = []
_lib.lv00_get_version.restype = c_char_p

_lib.lv00_get_last_error_code.argtypes = []
_lib.lv00_get_last_error_code.restype = c_int

_lib.lv00_get_last_error_message.argtypes = []
_lib.lv00_get_last_error_message.restype = c_char_p

_lib.lv00_set_error.argtypes = [c_int, c_char_p]
_lib.lv00_set_error.restype = None

_lib.lv00_clear_error.argtypes = []
_lib.lv00_clear_error.restype = None

# ============================================================
# Formula 函数签名
# 修复：添加缺失的公式解析/渲染/验证相关函数签名注册
# ============================================================

_lib.formula_parse.argtypes = [c_char_p, c_char_p]
_lib.formula_parse.restype = c_void_p

_lib.formula_render.argtypes = [c_void_p, c_int]
_lib.formula_render.restype = c_char_p

_lib.formula_validate.argtypes = [c_void_p]
_lib.formula_validate.restype = c_void_p

_lib.formula_free_error_list.argtypes = [c_void_p]
_lib.formula_free_error_list.restype = None

_lib.formula_get_last_error.argtypes = []
_lib.formula_get_last_error.restype = c_char_p

_lib.parse_result_get_ast.argtypes = [c_void_p]
_lib.parse_result_get_ast.restype = c_void_p

_lib.parse_result_destroy.argtypes = [c_void_p]
_lib.parse_result_destroy.restype = None

_lib.formula_to_graph.argtypes = [c_void_p, c_void_p]
_lib.formula_to_graph.restype = c_void_p

_lib.formula_to_graph_result_success.argtypes = [c_void_p]
_lib.formula_to_graph_result_success.restype = c_int

_lib.formula_to_graph_result_nodes.argtypes = [c_void_p]
_lib.formula_to_graph_result_nodes.restype = POINTER(c_int)

_lib.formula_to_graph_result_nodes_count.argtypes = [c_void_p]
_lib.formula_to_graph_result_nodes_count.restype = c_int

_lib.formula_to_graph_result_constraints.argtypes = [c_void_p]
_lib.formula_to_graph_result_constraints.restype = POINTER(c_int)

_lib.formula_to_graph_result_constraints_count.argtypes = [c_void_p]
_lib.formula_to_graph_result_constraints_count.restype = c_int

_lib.formula_to_graph_result_destroy.argtypes = [c_void_p]
_lib.formula_to_graph_result_destroy.restype = None

_lib.graph_to_formula.argtypes = [c_void_p]
_lib.graph_to_formula.restype = c_void_p

_lib.graph_to_formula_result_destroy.argtypes = [c_void_p]
_lib.graph_to_formula_result_destroy.restype = None

# ============================================================
# 常量定义
# ============================================================
# 以下常量映射 Lv-00 C 库中定义的结果码、枚举值和状态标志。
# 所有常量值与 C 头文件中的定义保持一致。

# ---- 节点添加结果码 ----
# graph_add_* 函数的返回值，表示添加操作的结果
ADD_NODE_OK = 0              # 添加成功
ADD_NODE_CONFLICT = 1        # 添加导致约束冲突
ADD_NODE_INVALID_REGION = 2  # 添加的区域定义无效（如非闭合边界）

# ---- 约束添加结果码 ----
# graph_add_*_constraint 函数的返回值
ADD_CONSTRAINT_OK = 0        # 约束添加成功
ADD_CONSTRAINT_DUPLICATE = 1 # 约束已存在（重复添加）
ADD_CONSTRAINT_CONFLICT = 2  # 约束与现有约束冲突

# ---- 节点移除结果码 ----
REMOVE_NODE_OK = 0           # 节点移除成功
REMOVE_NODE_NOT_FOUND = 1    # 未找到指定 ID 的节点
REMOVE_NODE_ERROR = 2        # 移除过程发生错误

# ---- 约束移除结果码 ----
REMOVE_CONSTRAINT_OK = 0           # 约束移除成功
REMOVE_CONSTRAINT_NOT_FOUND = 1    # 未找到指定 ID 的约束
REMOVE_CONSTRAINT_ERROR = 2        # 移除过程发生错误

# ---- 几何类型枚举 ----
# 约束图中节点的几何类型标识
GEOM_POINT = 0              # 点：由 (x, y) 坐标定义
GEOM_LINE_SEGMENT = 1       # 线段：由两个端点定义
GEOM_REGION = 2             # 区域：由闭合边界线段定义
GEOM_PORT = 3               # 端口：函数块系统的输入/输出接口
GEOM_FUNCTION_BLOCK = 4     # 函数块：可重用的几何构造模板

# ---- 端口类型 ----
PORT_INPUT = 0              # 输入端口：接收外部数据/参数
PORT_OUTPUT = 1             # 输出端口：产生结果/返回值

# ---- 约束类型 ----
# 约束图中节点之间关系的类型枚举
CONSTRAINT_INCIDENCE = 0    # 关联约束：点位于线段或区域上
CONSTRAINT_BETWEENNESS = 1  # 介子约束：三点共线，一点在另两点之间
CONSTRAINT_INTERSECTION = 2 # 交点约束：两条线交于一点
CONSTRAINT_CONTAINMENT = 3  # 包含约束：一个区域包含另一个区域
CONSTRAINT_CONNECTION = 4   # 连接约束：端口间的数据流连接

# ---- 合一状态 ----
# 证明系统中合一检查的结果码
UNIFY_OK = 0             # 合一成功
UNIFY_FAILED = 1         # 合一失败
UNIFY_TYPE_MISMATCH = 2  # 类型不匹配，无法合一

# ---- 求解器状态 ----
# 代数求解器的返回状态
SOLVER_OK = 0              # 求解成功
SOLVER_UNIQUE = 1          # 存在唯一解
SOLVER_MULTIPLE = 2        # 存在多个解
SOLVER_NO_SOLUTION = 3     # 无解
SOLVER_OVERCONSTRAINED = 4 # 过约束（约束过多）
SOLVER_OUT_OF_SCOPE = 5    # 超出求解范围
SOLVER_TIMEOUT = 6         # 求解超时

# ---- 引擎状态 ----
# 引擎操作的返回状态码
ENGINE_OK = 0                 # 操作成功
ENGINE_OUT_OF_MEMORY = 1      # 内存不足
ENGINE_INVALID_STATE = 2      # 引擎状态无效
ENGINE_CONSTRAINT_CONFLICT = 3 # 约束冲突
ENGINE_MODULE_ERROR = 4       # 模块加载/解析错误

# ---- 引擎求解结果 ----
ENGINE_SOLVE_OK = 0       # 求解成功
ENGINE_SOLVE_CONFLICT = 1 # 求解过程中发现约束冲突
ENGINE_SOLVE_TIMEOUT = 2  # 求解超时
ENGINE_SOLVE_ERROR = 3    # 求解发生错误

# ---- 打包结果 ----
# 函数块打包操作的返回码
PACK_OK = 0                     # 打包成功
PACK_CROSS_BOUNDARY_CONFLICT = 1 # 跨边界约束冲突（跨作用域）
PACK_INVALID_NODES = 2          # 无效节点（节点不存在或类型错误）
PACK_INVALID_PORTS = 3          # 无效端口（端口定义不正确）
PACK_OUT_OF_MEMORY = 4          # 内存不足
PACK_CANCELLED = 5              # 打包被取消

# ---- 例化结果 ----
# 函数块实例化操作的返回码
INSTANTIATE_OK = 0                 # 实例化成功
INSTANTIATE_NO_SOLUTION = 1        # 无解（给定实参不满足函数块约束）
INSTANTIATE_MULTIPLE_SOLUTIONS = 2 # 存在多个解（需要选择器）
INSTANTIATE_SELECTOR_NEEDED = 3    # 需要选择器确定唯一解
INSTANTIATE_PRECONDITION_FAILED = 4 # 前置条件不满足
INSTANTIATE_OUT_OF_MEMORY = 5      # 内存不足

# ---- 确定性状态 ----
# 函数块确定性检查的结果
DETERMINISM_UNVERIFIED = 0          # 未验证（尚未进行确定性检查）
DETERMINISM_VERIFIED = 1            # 已验证（确认解唯一）
DETERMINISM_NON_DETERMINISTIC = 2   # 非确定性（存在多个可能的解）
DETERMINISM_PARTIALLY_VERIFIED = 3  # 部分验证（某些路径已验证）

# ---- 证明颜色 ----
# 证明树节点的信任颜色，从绿到琥珀表示信任度递减
PROOF_COLOR_GREEN = 0              # 绿色：完全验证，最高信任
PROOF_COLOR_BLUE_UNEXPLORED = 1    # 蓝色-未探索：尚未展开的节点
PROOF_COLOR_BLUE_RESOURCE = 2      # 蓝色-资源：外部资源引用的节点
PROOF_COLOR_BLUE_OUT_OF_RANGE = 3  # 蓝色-超范围：超出当前关注范围
PROOF_COLOR_GREEN_VERIFIED = 4     # 绿色-已验证：通过验证的绿色节点
PROOF_COLOR_YELLOW = 5             # 黄色：需要人工审查
PROOF_COLOR_ORANGE_ORACLE = 6      # 橙色-神谕：依赖神谕/外部求解器
PROOF_COLOR_ORANGE_EX_FALSO = 7    # 橙色-爆炸原理：从矛盾推导
PROOF_COLOR_AMBER = 8              # 琥珀色：最低信任，需进一步验证
PROOF_COLOR_DARK_ORANGE = 9        # 深橙色：高度可疑

# ---- 命题类型 ----
# 逻辑命题的类型枚举
PROPOSITION_ATOMIC = 0       # 原子命题（不可再分的基本命题）
PROPOSITION_CONJUNCTION = 1  # 合取（AND）
PROPOSITION_DISJUNCTION = 2  # 析取（OR）
PROPOSITION_IMPLICATION = 3  # 蕴含（IF-THEN）
PROPOSITION_NEGATION = 4     # 否定（NOT）
PROPOSITION_UNIVERSAL = 5    # 全称量化（FOR ALL）
PROPOSITION_EXISTENTIAL = 6  # 存在量化（EXISTS）
PROPOSITION_BOTTOM = 7       # 矛盾命题（FALSE/矛盾）

# ---- 证明步骤类型 ----
# 证明树中每个步骤的操作类型
PROOF_STEP_ADD_NODE = 0       # 添加节点
PROOF_STEP_ADD_CONSTRAINT = 1 # 添加约束
PROOF_STEP_REWRITE = 2        # 重写步骤
PROOF_STEP_FUNCTION_APP = 3   # 函数应用
PROOF_STEP_PACK_FUNCTION = 4  # 打包函数
PROOF_STEP_NORMALIZATION = 5  # 规范化步骤
PROOF_STEP_UNIFY = 6          # 合一检查
PROOF_STEP_EX_FALSO = 7       # 爆炸原理应用
PROOF_STEP_ORACLE = 8         # 神谕/外部求解

# ---- 递归检查结果 ----
# 递归终止检查的返回状态
RECURSION_OK = 0               # 递归检查通过
RECURSION_NOT_DECREASING = 1   # 测度未递减（可能不终止）
RECURSION_DEPTH_EXCEEDED = 2   # 递归深度超限
RECURSION_CYCLE_DETECTED = 3   # 检测到循环依赖
RECURSION_MEASURE_UNKNOWN = 4  # 测度未知
RECURSION_ERROR = 5            # 检查过程发生错误

# ---- 日志级别 ----
# 调试日志输出级别，从最详细到最简洁
LOG_LEVEL_DEBUG = 0  # 调试级别：输出所有调试信息
LOG_LEVEL_INFO = 1   # 信息级别：输出一般运行信息
LOG_LEVEL_WARN = 2   # 警告级别：输出警告和重要信息
LOG_LEVEL_ERROR = 3  # 错误级别：仅输出错误信息
LOG_LEVEL_NONE = 4   # 关闭日志：不输出任何日志

# ---- 信任颜色 ----
# 几何元素的信任级别，颜色表示可信任程度
TRUST_GREEN = 0        # 绿色：完全验证，最高信任度
TRUST_BLUE = 1         # 蓝色：系统性原因，需要额外验证
TRUST_YELLOW = 2       # 黄色：需要人工审查
TRUST_ORANGE = 3       # 橙色：神谕/外部依赖
TRUST_LIGHT_ORANGE = 4  # 浅橙色：轻度可疑
TRUST_AMBER = 5        # 琥珀色：最低信任度，需进一步证明

# ---- 选择器类型 ----
# 多解情况下选择唯一解的策略类型
SELECTOR_POSITIVE_ROOT = 0     # 取正根：选择正的平方根/根
SELECTOR_NEGATIVE_ROOT = 1     # 取负根：选择负的平方根/根
SELECTOR_IN_REGION = 2         # 区域内选解：选择位于指定区域内的解
SELECTOR_NEAREST_TO_POINT = 3  # 最近优先：选择距离参考点最近的解
SELECTOR_CUSTOM = 4            # 自定义：使用用户定义的选择逻辑

# ---- 坐标类型 ----
# 符号坐标的内部表示类型
COORD_RATIONAL = 0       # 有理数：精确分数表示
COORD_ALGEBRAIC = 1      # 代数数：通过最小多项式定义
COORD_QUADRATIC = 2      # 二次根式：a + b*sqrt(n) 形式
COORD_TRANSCENDENTAL = 3 # 超越数：π、e 等非代数数
