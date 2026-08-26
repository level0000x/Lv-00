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
    lv_LIBRARY_PATH: 可手动指定库文件路径

作者：Lv-00 开发团队
版本：3.3.0
"""

import ctypes
import os
import sys
from ctypes import c_int, c_int64, c_uint64, c_double, c_char_p, c_char, c_void_p, c_bool, POINTER, CFUNCTYPE

# ============================================================
# 库文件搜索函数
# ============================================================

def _find_library():
    """
    搜索并定位 Lv-00 共享库文件。

    搜索策略（按优先级排序）：
    1. 环境变量 lv_LIBRARY_PATH（最高优先级）
    2. 平台特定的标准库名称（win32: .dll, darwin: .dylib, linux: .so）
    3. 预定义的搜索路径列表（包目录、构建目录、系统库目录）
    4. Windows 上从 PATH 环境变量搜索

    返回：
        str: 库文件的完整路径

    异常：
        ImportError: 无法找到库文件时抛出，附带搜索路径信息
    """
    # 优先检查环境变量
    if 'lv_LIBRARY_PATH' in os.environ:
        lib_path = os.environ['lv_LIBRARY_PATH']
        if os.path.exists(lib_path):
            if os.path.isdir(lib_path):
                # 容忍目录：自动追加平台共享库名（Windows: liblv.dll/lv.dll，
                # macOS: liblv.dylib，Linux: liblv.so），避免用户误设目录导致
                # ctypes.CDLL(目录) 加载失败（修复：此前目录路径 exists 即返回，
                # CDLL 报 "Could not find module" 被误判为 CRT 不匹配）
                for name in (['liblv.dll', 'lv.dll'] if sys.platform == 'win32'
                             else ['liblv.dylib', 'lv.dylib'] if sys.platform == 'darwin'
                             else ['liblv.so', 'lv.so']):
                    full = os.path.join(lib_path, name)
                    if os.path.exists(full):
                        return full
                raise ImportError(
                    f"lv_LIBRARY_PATH 指向目录但不含共享库（{lib_path}）；"
                    f"目录内未找到平台库名（Windows: liblv.dll，Linux: liblv.so，"
                    f"macOS: liblv.dylib）。"
                )
            if lib_path.lower().endswith(('.a', '.lib')):
                raise ImportError(
                    f"lv_LIBRARY_PATH 指向的是静态库，ctypes 无法加载静态库（.a/.lib）: {lib_path}\n"
                    f"需要共享库（以 BUILD_SHARED_LIBS=ON 构建，"
                    f"Windows: liblv.dll，Linux: liblv.so，macOS: liblv.dylib）。"
                )
            return lib_path
        raise ImportError(f"lv_LIBRARY_PATH 指定的文件不存在: {lib_path}")
    
    # 平台特定的库名称
    # Windows 优先搜索 liblv.dll（符合 GNU 命名惯例），其次为 lv.dll（短名称）
    if sys.platform == 'win32':
        names = ['liblv.dll', 'lv.dll']
    elif sys.platform == 'darwin':
        names = ['liblv.dylib', 'lv.dylib']
    else:
        names = ['liblv.so', 'lv.so']

    # 搜索路径列表
    # 搜索优先级：包目录 > 父目录 > 构建输出目录 > 项目根目录
    # 仅包含当前平台有效的路径，避免无意义的文件系统调用
    package_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    project_root = os.path.join(package_dir, '..', '..')
    search_paths = [
        package_dir,  # module/python/
        os.path.join(package_dir, '..'),  # module/
        os.path.join(project_root, 'build3'),  # 实际静态库构建目录（liblv.a）
        os.path.join(project_root, 'build4'),  # 实际构建目录（lv.pc）
        os.path.join(project_root, 'build'),  # project_root/build
        os.path.join(project_root, 'build', 'Release'),  # Windows 构建目录
        os.path.join(project_root, 'bin'),  # project_root/bin
        os.path.join(project_root, 'lib'),  # project_root/lib
        project_root,  # project_root
    ]
    # 根据平台添加对应的库路径
    if sys.platform == 'darwin':
        search_paths.extend(['/usr/local/lib', '/opt/homebrew/lib'])
    elif sys.platform == 'linux':
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

    # 检测是否存在静态库（.a/.lib）——ctypes 无法加载静态库，
    # 需以 BUILD_SHARED_LIBS=ON 重新构建获得共享库
    static_names = ['liblv.a', 'lv.a', 'liblv.lib', 'lv.lib']
    static_hits = []
    for path in search_paths:
        for sname in static_names:
            full_path = os.path.join(path, sname)
            if os.path.exists(full_path):
                static_hits.append(full_path)

    # 构建详细错误信息，列出所有尝试过的搜索位置
    searched_locations = []
    for path in search_paths:
        for name in names:
            searched_locations.append(os.path.join(path, name))
    searched_locations_str = '\n    '.join(searched_locations)

    if static_hits:
        static_hits_str = '\n    '.join(static_hits)
        raise ImportError(
            f"仅找到 Lv-00 静态库，ctypes 无法加载静态库（.a/.lib）。\n"
            f"已找到以下静态库：\n"
            f"    {static_hits_str}\n"
            f"需要共享库（以 BUILD_SHARED_LIBS=ON 构建，"
            f"Windows: liblv.dll，Linux: liblv.so，macOS: liblv.dylib）。\n"
            f"解决方法：\n"
            f"  1. 以 BUILD_SHARED_LIBS=ON 重新配置并构建 Lv-00（CMake）\n"
            f"  2. 设置 lv_LIBRARY_PATH 环境变量指向共享库文件"
        )

    raise ImportError(
        f"无法找到 Lv-00 库文件。\n"
        f"已尝试以下位置：\n"
        f"    {searched_locations_str}\n"
        f"解决方法：\n"
        f"  1. 设置 lv_LIBRARY_PATH 环境变量指向库文件\n"
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
    _safe_lib_path = _lib_path if '_lib_path' in dir() else '<未知>'
    raise ImportError(
        f"加载 Lv-00 动态链接库失败。\n"
        f"库路径: {_safe_lib_path}\n"
        f"系统错误: {e}\n"
        f"可能原因：\n"
        f"  1. 库文件已损坏或与当前 Python 版本不兼容\n"
        f"  2. 缺少必要的运行时依赖（如 MSVC 运行时）\n"
        f"  3. 库文件位数与 Python 位数不匹配（32位/64位）\n"
        f"请检查 lv_LIBRARY_PATH 环境变量设置，或重新编译 Lv-00。"
    ) from e
except ImportError:
    raise
except Exception as e:
    raise ImportError(f"加载 Lv-00 库时发生未预期的错误: {e}") from e

# ============================================================
# C 结构体类型定义
# ============================================================
# 以下结构体对应 C 层的类型，定义了 ctypes 的 _fields_ 布局。
# 字段类型和顺序基于 C 层结构体的预期布局。
# 所有字段访问也可通过 C 函数接口完成，以保持二进制兼容性。

class _SymbolicCoord(ctypes.Structure):
    """
    符号坐标的不透明句柄。

    对应 C 层 SymbolicCoord 类型。Python 侧不直接解引用内部字段，
    统一通过 _lib.symbolic_coord_* 系列 C 函数读写。
    """
    _fields_ = [
        ("_opaque", c_void_p),
    ]

class _ConstraintGraph(ctypes.Structure):
    """
    约束图的不透明句柄。

    对应 C 层 ConstraintGraph 类型。Python 侧不直接解引用内部字段，
    统一通过 _lib.graph_* 系列 C 函数访问。
    """
    _fields_ = [
        ("_opaque", c_void_p),
    ]

class _NormalizationResult(ctypes.Structure):
    """
    规范化结果的 C 结构体。

    对应 C 层 NormalizationResult 类型，包含图规范化操作的统计信息，
    如合并的等价节点数量、化简的约束数量等。
    通过 _lib.normalization_result_destroy() 释放。

    C 层预期布局（normalization.h）：
        int *merged_node_ids;         // 合并的节点 ID 数组
        int merged_count;             // 合并的节点数量
        int merged_capacity;          // 预分配的合并记录数组容量
        int *original_ids;            // 原始 ID 数组
        int *representative_ids;      // 代表 ID 数组
        bool user_confirmed;          // 用户是否确认
        NormalizationLog *log;        // 详细合并日志（结果拥有所有权）
    """
    _fields_ = [
        ("merged_node_ids", c_void_p),
        ("merged_count", c_int),
        ("merged_capacity", c_int),
        ("original_ids", c_void_p),
        ("representative_ids", c_void_p),
        ("user_confirmed", c_bool),
        ("log", c_void_p),
    ]

class _GeomNode(ctypes.Structure):
    """
    几何节点的 C 结构体（仅保留被 Python 侧解引用的前两字段）。

    对应 C 层 GeomNode 类型（constraint_graph.h）。Python 侧仅解引用
    id/type 两个字段，恰好对应 C 布局的前两字段（int id / GeomType type），
    其余内部字段统一通过 C 函数访问。
    """
    _fields_ = [
        ("id", c_int),
        ("type", c_int),
    ]

class _Constraint(ctypes.Structure):
    """
    约束的不透明句柄。

    对应 C 层 Constraint 类型。Python 侧不直接解引用内部字段，
    统一通过 _lib.graph_* 系列 C 函数访问。
    """
    _fields_ = [
        ("_opaque", c_void_p),
    ]

class _FuncBlock(ctypes.Structure):
    """
    函数块的不透明句柄。

    对应 C 层 FuncBlock 类型。Python 侧不直接解引用内部字段，
    统一通过 _lib.func_block_* 系列 C 函数访问。
    """
    _fields_ = [
        ("_opaque", c_void_p),
    ]

class _ProofNavigator(ctypes.Structure):
    """
    证明导航器的不透明句柄。

    对应 C 层 ProofNavigator 类型。Python 侧不直接解引用内部字段，
    统一通过 _lib.proof_* 系列 C 函数访问。
    """
    _fields_ = [
        ("_opaque", c_void_p),
    ]

class _Proposition(ctypes.Structure):
    """
    命题的不透明句柄。

    对应 C 层 Proposition 类型。Python 侧不直接解引用内部字段，
    统一通过 _lib.proposition_* / _lib.proof_* 系列 C 函数访问。
    """
    _fields_ = [
        ("_opaque", c_void_p),
    ]

class _lvEngine(ctypes.Structure):
    """
    引擎的不透明句柄。

    对应 C 层 lvEngine 类型。Python 侧不直接解引用内部字段，
    统一通过 _lib.engine_* 系列 C 函数访问。
    """
    _fields_ = [
        ("_opaque", c_void_p),
    ]

class _MeasureSystem(ctypes.Structure):
    """
    测度系统的不透明句柄。

    对应 C 层 MeasureSystem 类型。Python 侧不直接解引用内部字段，
    统一通过 _lib.measure_* 系列 C 函数访问。
    """
    _fields_ = [
        ("_opaque", c_void_p),
    ]

class _RecursionContext(ctypes.Structure):
    """
    递归上下文的不透明句柄。

    对应 C 层 RecursionContext 类型。Python 侧不直接解引用内部字段，
    统一通过 _lib.recursion_* 系列 C 函数访问。
    """
    _fields_ = [
        ("_opaque", c_void_p),
    ]

class _Port(ctypes.Structure):
    """
    端口的不透明句柄。

    对应 C 层 Port 类型。Python 侧不直接解引用内部字段，
    统一通过 _lib.graph_* 系列 C 函数访问。
    """
    _fields_ = [
        ("_opaque", c_void_p),
    ]

class _RecursionTestResult(ctypes.Structure):
    """
    递归内置测试结果结构体（recursion.h:649-653）。

    C 层预期布局：
        char name[64];       // 测试名称
        bool passed;         // 是否通过
        char error_msg[128]; // 错误信息（passed 为 false 时有效）
    """
    _fields_ = [
        ("name", c_char * 64),
        ("passed", c_bool),
        ("error_msg", c_char * 128),
    ]

# ============================================================
# SymbolicCoord 函数签名
# ============================================================
# 以下函数用于创建、销毁、序列化和操作符号坐标。
# 符号坐标是 Lv-00 精确计算的基础——所有几何坐标
# 都以符号形式存储，避免浮点数精度损失。

# 创建有理数坐标：numerator/denominator
_lib.symbolic_coord_create_rational.argtypes = [c_int64, c_uint64]
_lib.symbolic_coord_create_rational.restype = POINTER(_SymbolicCoord)

_lib.symbolic_coord_from_string.argtypes = [c_char_p]
_lib.symbolic_coord_from_string.restype = POINTER(_SymbolicCoord)

# [已移除] symbolic_coord_create_from_mpz, symbolic_coord_deserialize: C 库中不存在这些导出函数

# 销毁符号坐标对象，释放所有关联的内存资源
_lib.symbolic_coord_destroy.argtypes = [POINTER(_SymbolicCoord)]
_lib.symbolic_coord_destroy.restype = None

# 序列化为字符串（如 "3/4"），返回的字符串需要调用 free() 释放
_lib.symbolic_coord_serialize.argtypes = [POINTER(_SymbolicCoord)]
# 返回堆分配字符串（C 侧 lv_malloc），ctypes 不能自动管理：
# 若 restype=c_char_p，ctypes 会拷贝为 bytes 且 Python 端无法正确释放
# 原指针（此前对 bytes 调 lv_free_ptr 触发未定义行为/崩溃）。
# 改为 c_void_p，由 Python 端 string_at 读取 + lv_free_ptr 释放。
_lib.symbolic_coord_serialize.restype = c_void_p

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

# 带指定ID添加节点（用于反序列化）
_lib.graph_add_node_with_id.argtypes = [POINTER(_ConstraintGraph), c_int, c_int, POINTER(POINTER(_SymbolicCoord)), c_int]
_lib.graph_add_node_with_id.restype = POINTER(_GeomNode)

# 带指定ID添加约束（用于反序列化）
_lib.graph_add_constraint_with_id.argtypes = [POINTER(_ConstraintGraph), c_int, c_int, POINTER(c_int), c_int]
_lib.graph_add_constraint_with_id.restype = POINTER(_Constraint)

# 设置全局流式上下文
_lib.graph_set_stream_context.argtypes = [c_void_p]
_lib.graph_set_stream_context.restype = None

# 查找涉及指定节点的所有约束
_lib.graph_find_constraints_involving.argtypes = [POINTER(_ConstraintGraph), c_int, POINTER(c_int), c_int]
_lib.graph_find_constraints_involving.restype = c_int

# 序列化图为 JSON 字符串
_lib.graph_serialize_to_json.argtypes = [POINTER(_ConstraintGraph)]
_lib.graph_serialize_to_json.restype = c_void_p

# 从 JSON 字符串反序列化图
_lib.graph_deserialize_from_json.argtypes = [c_char_p]
_lib.graph_deserialize_from_json.restype = POINTER(_ConstraintGraph)

# 检测冗余（按类型和参与者）
_lib.graph_detect_redundancy.argtypes = [POINTER(_ConstraintGraph), c_int, POINTER(c_int), c_int]
_lib.graph_detect_redundancy.restype = c_int

# ============================================================
# NormalizationResult 函数签名
# ============================================================
# 规范化结果的生命周期管理函数。
# NormalizationResult 由 graph_normalize() 创建，
# 使用完毕后必须调用 normalization_result_destroy() 释放。

_lib.normalization_result_destroy.argtypes = [POINTER(_NormalizationResult)]
_lib.normalization_result_destroy.restype = None

# ============================================================
# FuncBlock 函数签名
# ============================================================
# 函数块（FuncBlock）是 Lv-00 的可重用几何构造模板系统。
# 支持打包（pack）、实例化（instantiate）和确定性检查（determinism check）。
# 选择器（Selector）用于在多解情况下选择唯一解。

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

# FuncBlock Getter 函数
_lib.func_block_get_input_count.argtypes = [POINTER(_FuncBlock)]
_lib.func_block_get_input_count.restype = c_int

_lib.func_block_get_output_count.argtypes = [POINTER(_FuncBlock)]
_lib.func_block_get_output_count.restype = c_int

_lib.func_block_get_internal_count.argtypes = [POINTER(_FuncBlock)]
_lib.func_block_get_internal_count.restype = c_int

_lib.func_block_get_id.argtypes = [POINTER(_FuncBlock)]
_lib.func_block_get_id.restype = c_int

_lib.func_block_get_determinism.argtypes = [POINTER(_FuncBlock)]
_lib.func_block_get_determinism.restype = c_int

_lib.func_block_get_name.argtypes = [POINTER(_FuncBlock)]
_lib.func_block_get_name.restype = c_char_p

_lib.func_block_get_description.argtypes = [POINTER(_FuncBlock)]
_lib.func_block_get_description.restype = c_char_p

# FuncBlock Setter 函数
_lib.func_block_set_internal_nodes.argtypes = [POINTER(_FuncBlock), POINTER(c_int), c_int]
_lib.func_block_set_internal_nodes.restype = c_bool

_lib.func_block_set_input_ports.argtypes = [POINTER(_FuncBlock), POINTER(c_int), c_int]
_lib.func_block_set_input_ports.restype = c_bool

_lib.func_block_set_output_ports.argtypes = [POINTER(_FuncBlock), POINTER(c_int), c_int]
_lib.func_block_set_output_ports.restype = c_bool

_lib.func_block_set_selector.argtypes = [POINTER(_FuncBlock), c_void_p]
_lib.func_block_set_selector.restype = c_bool

_lib.func_block_add_port_dependency.argtypes = [POINTER(_FuncBlock), c_void_p]
_lib.func_block_add_port_dependency.restype = c_bool

_lib.func_block_set_preconditions.argtypes = [POINTER(_FuncBlock), POINTER(c_int), c_int]
_lib.func_block_set_preconditions.restype = c_bool

_lib.func_block_set_name.argtypes = [POINTER(_FuncBlock), c_char_p]
_lib.func_block_set_name.restype = c_bool

_lib.func_block_set_description.argtypes = [POINTER(_FuncBlock), c_char_p]
_lib.func_block_set_description.restype = c_bool

# 深拷贝函数块
_lib.func_block_copy.argtypes = [POINTER(_FuncBlock)]
_lib.func_block_copy.restype = POINTER(_FuncBlock)

# 检测跨边界约束
_lib.func_block_detect_cross_boundary.argtypes = [POINTER(_ConstraintGraph), POINTER(c_int), c_int, POINTER(c_void_p), POINTER(c_int)]
_lib.func_block_detect_cross_boundary.restype = c_bool

# 打包操作（扩展版）
_lib.func_block_pack_ex.argtypes = [POINTER(_ConstraintGraph), c_void_p, POINTER(POINTER(_FuncBlock))]
_lib.func_block_pack_ex.restype = c_int

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
# 证明系统相关函数，包括命题创建、合一检查、证明导航和导出。
# 证明导航器（ProofNavigator）支持遍历证明树并导出为 HTML/LaTeX 格式。

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

# proof_unify_detailed: 带详细失败原因报告的合一检查
# C 函数签名: proof_unify_detailed(ConstraintGraph*, Proposition*, bool, char**) -> int
# 第四个参数为输出参数，接收 C 引擎分配的诊断字符串指针
_lib.proof_unify_detailed.argtypes = [POINTER(_ConstraintGraph), POINTER(_Proposition), c_bool, POINTER(c_char_p)]
_lib.proof_unify_detailed.restype = c_int

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
# 递归终止检查相关函数。
# 测度系统（MeasureSystem）管理递归终止条件，
# 递归上下文（RecursionContext）跟踪递归调用状态并检测循环依赖。

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

# 运行递归模块内置测试套件（recursion.h:715）
_lib.recursion_run_builtin_tests.argtypes = [POINTER(_MeasureSystem), POINTER(POINTER(_RecursionTestResult)), POINTER(c_int)]
_lib.recursion_run_builtin_tests.restype = c_int

# ============================================================
# Engine 函数签名
# ============================================================
# 以下函数用于创建和操作 Lv-00 主引擎。
# 引擎是系统的顶层协调器，管理约束图、模块加载、
# 函数打包/实例化、求解和重写等核心功能。

# 创建新的引擎实例
_lib.engine_create.argtypes = []
_lib.engine_create.restype = POINTER(_lvEngine)

# 销毁引擎实例，释放所有关联资源
_lib.engine_destroy.argtypes = [POINTER(_lvEngine)]
_lib.engine_destroy.restype = None

# 执行求解：重写 -> 求解 -> 冲突检查
_lib.engine_solve.argtypes = [POINTER(_lvEngine)]
_lib.engine_solve.restype = c_int

# 执行重写-求解协作流程，max_rewrite_steps 和 max_solve_steps 控制步数上限
_lib.engine_rewrite_and_solve.argtypes = [POINTER(_lvEngine), c_int, c_int]
_lib.engine_rewrite_and_solve.restype = c_int

# 设置重写步数上限（单次重写的最大步数）
_lib.engine_set_rewrite_step_limit.argtypes = [POINTER(_lvEngine), c_int]
_lib.engine_set_rewrite_step_limit.restype = None

# 获取当前重写步数上限
_lib.engine_get_rewrite_step_limit.argtypes = [POINTER(_lvEngine)]
_lib.engine_get_rewrite_step_limit.restype = c_int

# 加载模块文件（.lv 格式），将模块内容加载到引擎的约束图中
_lib.engine_load_module.argtypes = [POINTER(_lvEngine), c_char_p]
_lib.engine_load_module.restype = c_int

# 加载公理包文件，添加公理到引擎的规则库中
_lib.engine_load_axiom_package.argtypes = [POINTER(_lvEngine), c_char_p]
_lib.engine_load_axiom_package.restype = c_int

# 打包函数块：将内部节点和端口封装为可重用的函数模板
_lib.engine_pack_function.argtypes = [
    POINTER(_lvEngine),
    POINTER(c_int), c_int,
    POINTER(c_int), c_int,
    POINTER(c_int), c_int,
    POINTER(c_int)
]
_lib.engine_pack_function.restype = c_bool

# 实例化函数块：使用实参替换形式参数创建函数实例
_lib.engine_instantiate_function.argtypes = [
    POINTER(_lvEngine), c_int,
    POINTER(c_int), c_int,
    POINTER(c_int)
]
_lib.engine_instantiate_function.restype = POINTER(c_int)

# 执行合一检查：检查构造图是否满足命题模式
_lib.engine_unify.argtypes = [POINTER(_lvEngine), POINTER(_ConstraintGraph), POINTER(_ConstraintGraph)]
_lib.engine_unify.restype = c_int

# 处理位电路跳闸事件（默认恢复策略）
_lib.engine_handle_circuit_trip.argtypes = [POINTER(_lvEngine)]
_lib.engine_handle_circuit_trip.restype = c_int

# 使用指定动作处理位电路跳闸
_lib.engine_handle_circuit_trip_with_action.argtypes = [POINTER(_lvEngine), c_int]
_lib.engine_handle_circuit_trip_with_action.restype = c_int

# 创建冻结点快照，保存当前引擎状态用于后续回滚
_lib.engine_create_frozen_point.argtypes = [POINTER(_lvEngine)]
_lib.engine_create_frozen_point.restype = c_void_p

# 恢复到冻结点状态，丢弃快照之后的所有修改
_lib.engine_restore_frozen_point.argtypes = [POINTER(_lvEngine), c_void_p]
_lib.engine_restore_frozen_point.restype = c_bool

# 销毁冻结点快照，释放占用的内存
_lib.engine_destroy_frozen_point.argtypes = [c_void_p]
_lib.engine_destroy_frozen_point.restype = None

# 获取引擎最后一次操作的状态码
_lib.engine_get_last_status.argtypes = [POINTER(_lvEngine)]
_lib.engine_get_last_status.restype = c_int

# 获取引擎最后一次操作的错误消息
_lib.engine_get_last_error.argtypes = [POINTER(_lvEngine)]
_lib.engine_get_last_error.restype = c_char_p

# 添加重写规则到引擎
_lib.engine_add_rewrite_rule.argtypes = [POINTER(_lvEngine), c_void_p]
_lib.engine_add_rewrite_rule.restype = c_bool

# 获取引擎的流式上下文
_lib.engine_get_stream_context.argtypes = [POINTER(_lvEngine)]
_lib.engine_get_stream_context.restype = c_void_p

# 设置流式输出开关
_lib.engine_set_streaming_enabled.argtypes = [POINTER(_lvEngine), c_bool]
_lib.engine_set_streaming_enabled.restype = None

# 查询流式输出是否启用
_lib.engine_is_streaming_enabled.argtypes = [POINTER(_lvEngine)]
_lib.engine_is_streaming_enabled.restype = c_bool

# 发射引擎流式事件
_lib.engine_emit_stream_event.argtypes = [POINTER(_lvEngine), c_int, c_char_p, c_int, c_int, c_int]
_lib.engine_emit_stream_event.restype = None

# ============================================================
# Debug 函数签名
# ============================================================
# 调试和诊断相关函数，包括日志控制、性能计数器和内存池管理。
# 用于开发调试和性能分析。

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
_lib.debug_counters_report.restype = c_void_p

_lib.mem_pool_create.argtypes = [ctypes.c_size_t, c_int]
_lib.mem_pool_create.restype = c_void_p

_lib.mem_pool_destroy.argtypes = [c_void_p]
_lib.mem_pool_destroy.restype = None

# ============================================================
# Unify 函数签名
# ============================================================
# 合一（Unification）相关函数，用于检查约束图是否满足命题模式。
# 注意：部分旧接口已移除，请使用 proof_unify_detailed 等新接口。

# [已移除] unify_check, unify_detailed: C 库中不存在这些导出函数，请使用 unify_construction_with_proposition 系列

# ============================================================
# Rewrite 函数签名
# ============================================================
# 重写（Rewrite）规则相关函数，用于几何图的等价变换。
# 注意：大部分旧接口已移除，重写功能现通过引擎接口提供。

# [已移除] rewrite_create_rule, rewrite_add_rule, rewrite_rewrite, rewrite_rewrite_until: C 库中不存在这些导出函数

# [重命名] rewrite_destroy_rule -> rewrite_rule_destroy
_lib.rewrite_rule_destroy.argtypes = [c_void_p]
_lib.rewrite_rule_destroy.restype = None

# [已移除] rewrite_add_rule: C 库中不存在此导出函数，请使用 engine_add_rewrite_rule 代替

# [已移除] rewrite_rewrite: C 库中不存在此导出函数，实际导出为 rewrite_with_rules

# [已移除] rewrite_rewrite_until: C 库中不存在此导出函数

# ============================================================
# Solver 函数签名
# ============================================================
# 代数求解器相关函数，包括方程组求解、自由度计算和冲突检测。

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

# lv_free_ptr: FFI 兼容释放函数，接受 void*（与 lv_free(void**) 不同）
# lv_free 接受 void**（双重指针），ctypes 无法方便传递双重指针
# 因此 C 层新增了 lv_free_ptr(void*) 专供 FFI 使用
_lib.lv_free_ptr = _lib.lv_free_ptr if hasattr(_lib, 'lv_free_ptr') else _lib.free
_lib.lv_free_ptr.argtypes = [c_void_p]
_lib.lv_free_ptr.restype = None

_lib.lv_init.argtypes = []
_lib.lv_init.restype = c_bool

_lib.lv_cleanup.argtypes = []
_lib.lv_cleanup.restype = None

# [已移除] lv_get_version: 该函数在 C 头文件中为 static inline，不在 DLL 导出中

_lib.lv_get_last_error_code.argtypes = []
_lib.lv_get_last_error_code.restype = c_int

_lib.lv_get_last_error_message.argtypes = []
_lib.lv_get_last_error_message.restype = c_char_p

_lib.lv_set_error.argtypes = [c_int, c_char_p]
_lib.lv_set_error.restype = None

_lib.lv_clear_error.argtypes = []
_lib.lv_clear_error.restype = None

# ============================================================
# Formula 函数签名
# ============================================================
# 公式解析和渲染相关函数，支持将几何公式字符串解析为 AST，
# 并将 AST 渲染为可视化输出或转换为约束图。

_lib.formula_parse.argtypes = [c_char_p, c_char_p]
_lib.formula_parse.restype = c_void_p

_lib.formula_render.argtypes = [c_void_p, c_int]
_lib.formula_render.restype = c_void_p

# [已移除] formula_validate, formula_free_error_list: C 库中不存在这些导出函数

# [重命名] formula_get_last_error -> formula_parser_get_last_error
_lib.formula_parser_get_last_error.argtypes = []
_lib.formula_parser_get_last_error.restype = c_char_p

# [已移除] parse_result_get_ast, parse_result_destroy: C 库中不存在这些导出函数

_lib.formula_to_graph.argtypes = [c_void_p, c_void_p]
_lib.formula_to_graph.restype = c_void_p

# [已移除] formula_to_graph_result_*: C 库中不存在这些导出函数，FormulaToGraphResult 结构体通过 formula_to_graph 返回指针后直接访问字段

_lib.formula_to_graph_result_destroy.argtypes = [c_void_p]
_lib.formula_to_graph_result_destroy.restype = None

_lib.graph_to_formula.argtypes = [c_void_p]
_lib.graph_to_formula.restype = c_void_p

_lib.graph_to_formula_result_destroy.argtypes = [c_void_p]
_lib.graph_to_formula_result_destroy.restype = None

# 注册 formula_node_destroy，供 formula.py 的 __del__ 调用
_lib.formula_node_destroy.argtypes = [c_void_p]
_lib.formula_node_destroy.restype = None

# ============================================================
# 常量定义
# ============================================================
# 以下常量映射 Lv-00 C 库中定义的结果码、枚举值和状态标志。
# 所有常量值与 C 头文件中的定义保持一致。

# ===== 节点添加结果码 =====
# graph_add_* 函数的返回值，表示添加操作的结果
ADD_NODE_OK = 0              # 添加成功
ADD_NODE_CONFLICT = 1        # 添加导致约束冲突
ADD_NODE_INVALID_REGION = 2  # 添加的区域定义无效（如非闭合边界）

# ===== 约束添加结果码 =====
# graph_add_*_constraint 函数的返回值
ADD_CONSTRAINT_OK = 0        # 约束添加成功
ADD_CONSTRAINT_DUPLICATE = 1 # 约束已存在（重复添加）
ADD_CONSTRAINT_CONFLICT = 2  # 约束与现有约束冲突

# ===== 节点移除结果码 =====
REMOVE_NODE_OK = 0           # 节点移除成功
REMOVE_NODE_NOT_FOUND = 1    # 未找到指定 ID 的节点
REMOVE_NODE_ERROR = 2        # 移除过程发生错误

# ===== 约束移除结果码 =====
REMOVE_CONSTRAINT_OK = 0           # 约束移除成功
REMOVE_CONSTRAINT_NOT_FOUND = 1    # 未找到指定 ID 的约束
REMOVE_CONSTRAINT_ERROR = 2        # 移除过程发生错误

# ===== 几何节点类型常量 =====
# 约束图中节点的几何类型标识
# 与 C 头文件 core/include/lv/constraint_graph.h 中 GeomType 枚举（87-94 行）保持一致
GEOM_POINT = 0              # 点：由 (x, y) 坐标定义
GEOM_LINE_SEGMENT = 1       # 线段：由两个端点定义
GEOM_REGION = 2             # 区域：由闭合边界线段定义
GEOM_CIRCLE = 3             # 圆：由圆心和半径定义的二维几何对象
GEOM_PORT = 4               # 端口：函数块系统的输入/输出接口
GEOM_FUNCTION_BLOCK = 5     # 函数块：可重用的几何构造模板

# ===== 端口类型常量 =====
PORT_INPUT = 0              # 输入端口：接收外部数据/参数
PORT_OUTPUT = 1             # 输出端口：产生结果/返回值

# ===== 约束类型常量 =====
# 约束图中节点之间关系的类型枚举
# 与 C 头文件 core/include/lv/constraint_graph.h 中 ConstraintType 枚举（112-119 行）保持一致
CONSTRAINT_INCIDENCE = 0    # 关联约束：点位于线段或区域上
CONSTRAINT_BETWEENNESS = 1  # 介子约束：三点共线，一点在另两点之间
CONSTRAINT_INTERSECTION = 2 # 交点约束：两条线交于一点
CONSTRAINT_CONTAINMENT = 3  # 包含约束：一个区域包含另一个区域
CONSTRAINT_CONNECTION = 4   # 连接约束：端口间的数据流连接
CONSTRAINT_ANGLE = 5        # 角度约束：两条线段之间的夹角

# ===== 合一状态常量 =====
# 证明系统中合一检查的结果码
UNIFY_OK = 0             # 合一成功
UNIFY_FAILED = 1         # 合一失败
UNIFY_TYPE_MISMATCH = 2  # 类型不匹配，无法合一

# ===== 求解器状态常量 =====
# 代数求解器的返回状态
SOLVER_OK = 0              # 求解成功
SOLVER_UNIQUE = 1          # 存在唯一解
SOLVER_MULTIPLE = 2        # 存在多个解
SOLVER_NO_SOLUTION = 3     # 无解
SOLVER_OVERCONSTRAINED = 4 # 过约束（约束过多）
SOLVER_OUT_OF_SCOPE = 5    # 超出求解范围
SOLVER_TIMEOUT = 6         # 求解超时
SOLVER_OUT_OF_MEMORY = 7   # 内存不足（对应 solver.h 93 行 SOLVER_STATUS_OUT_OF_MEMORY）

# ===== 引擎状态常量 =====
# 引擎操作的返回状态码
# 与 C 头文件 core/include/lv/engine_status.h 中 EngineStatus 枚举（14-22 行）保持一致
ENGINE_OK = 0                 # 操作成功
ENGINE_OUT_OF_MEMORY = 1      # 内存不足
ENGINE_INVALID_STATE = 2      # 引擎状态无效
ENGINE_INVALID_ARGUMENT = 3   # 传入参数无效（空指针、越界等）
ENGINE_CONSTRAINT_CONFLICT = 4 # 约束冲突
ENGINE_MODULE_ERROR = 5       # 模块加载/解析错误
ENGINE_ERROR_INTERNAL = 6     # 内部错误

# ===== 引擎求解结果常量 =====
ENGINE_SOLVE_OK = 0       # 求解成功
ENGINE_SOLVE_CONFLICT = 1 # 求解过程中发现约束冲突
ENGINE_SOLVE_TIMEOUT = 2  # 求解超时
ENGINE_SOLVE_ERROR = 3    # 求解发生错误

# ===== 函数块打包结果常量 =====
# 函数块打包操作的返回码
PACK_OK = 0                     # 打包成功
PACK_CROSS_BOUNDARY_CONFLICT = 1 # 跨边界约束冲突（跨作用域）
PACK_INVALID_NODES = 2          # 无效节点（节点不存在或类型错误）
PACK_INVALID_PORTS = 3          # 无效端口（端口定义不正确）
PACK_INVALID_GRAPH = 4          # 无效图（打包目标不是有效子图）
PACK_OUT_OF_MEMORY = 5          # 内存不足
PACK_CANCELLED = 6              # 打包被取消

# ===== 函数块实例化结果常量 =====
# 函数块实例化操作的返回码
INSTANTIATE_OK = 0                 # 实例化成功
INSTANTIATE_NO_SOLUTION = 1        # 无解（给定实参不满足函数块约束）
INSTANTIATE_MULTIPLE_SOLUTIONS = 2 # 存在多个解（需要选择器）
INSTANTIATE_SELECTOR_NEEDED = 3    # 需要选择器确定唯一解
INSTANTIATE_PRECONDITION_FAILED = 4 # 前置条件不满足
INSTANTIATE_OUT_OF_MEMORY = 5      # 内存不足

# ===== 确定性状态常量 =====
# 函数块确定性检查的结果
DETERMINISM_UNVERIFIED = 0          # 未验证（尚未进行确定性检查）
DETERMINISM_VERIFIED = 1            # 已验证（确认解唯一）
DETERMINISM_NON_DETERMINISTIC = 2   # 非确定性（存在多个可能的解）
DETERMINISM_PARTIALLY_VERIFIED = 3  # 部分验证（某些路径已验证）

# ===== 证明颜色常量 =====
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

# ===== 命题类型常量 =====
# 逻辑命题的类型枚举
PROPOSITION_ATOMIC = 0       # 原子命题（不可再分的基本命题）
PROPOSITION_CONJUNCTION = 1  # 合取（AND）
PROPOSITION_DISJUNCTION = 2  # 析取（OR）
PROPOSITION_IMPLICATION = 3  # 蕴含（IF-THEN）
PROPOSITION_NEGATION = 4     # 否定（NOT）
PROPOSITION_UNIVERSAL = 5    # 全称量化（FOR ALL）
PROPOSITION_EXISTENTIAL = 6  # 存在量化（EXISTS）
PROPOSITION_BOTTOM = 7       # 矛盾命题（FALSE/矛盾）

# ===== 证明步骤类型常量 =====
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

# ===== 递归检查结果常量 =====
# 递归终止检查的返回状态
RECURSION_CHECK_RESULT_OK = 0               # 递归检查通过
RECURSION_CHECK_RESULT_NOT_DECREASING = 1   # 测度未递减（可能不终止）
RECURSION_CHECK_RESULT_DEPTH_EXCEEDED = 2   # 递归深度超限
RECURSION_CHECK_RESULT_CYCLE_DETECTED = 3   # 检测到循环依赖
RECURSION_CHECK_RESULT_MEASURE_UNKNOWN = 4  # 测度未知
RECURSION_CHECK_RESULT_ERROR = 5            # 检查过程发生错误

# ===== 日志级别常量 =====
# 调试日志输出级别，从最详细到最简洁
# 与 C 头文件 core/include/lv/debug.h 中 LogLevel 枚举（76-84 行，主定义源）保持一致
LOG_LEVEL_TRACE = -1  # 追踪级别：最详细的逐步骤日志（函数进入/退出、参数转储）
LOG_LEVEL_DEBUG = 0   # 调试级别：输出所有调试信息
LOG_LEVEL_INFO = 1    # 信息级别：输出一般运行信息
LOG_LEVEL_WARN = 2    # 警告级别：输出警告和重要信息
LOG_LEVEL_ERROR = 3   # 错误级别：仅输出错误信息
LOG_LEVEL_FATAL = 4   # 致命级别：不可恢复错误，记录后触发保护性动作
LOG_LEVEL_NONE = 5    # 关闭日志：不输出任何日志

# ===== 信任颜色常量 =====
# 几何元素的信任级别，颜色表示可信任程度
TRUST_GREEN = 0        # 绿色：完全验证，最高信任度
TRUST_BLUE = 1         # 蓝色：系统性原因，需要额外验证
TRUST_YELLOW = 2       # 黄色：需要人工审查
TRUST_ORANGE = 3       # 橙色：神谕/外部依赖
TRUST_LIGHT_ORANGE = 4  # 浅橙色：轻度可疑
TRUST_AMBER = 5        # 琥珀色：最低信任度，需进一步证明

# ===== 选择器类型常量 =====
# 多解情况下选择唯一解的策略类型
SELECTOR_POSITIVE_ROOT = 0     # 取正根：选择正的平方根/根
SELECTOR_NEGATIVE_ROOT = 1     # 取负根：选择负的平方根/根
SELECTOR_IN_REGION = 2         # 区域内选解：选择位于指定区域内的解
SELECTOR_NEAREST_TO_POINT = 3  # 最近优先：选择距离参考点最近的解
SELECTOR_CUSTOM = 4            # 自定义：使用用户定义的选择逻辑

# ===== 坐标类型常量 =====
# 符号坐标的内部表示类型
COORD_RATIONAL = 0       # 有理数：精确分数表示
COORD_ALGEBRAIC = 1      # 代数数：通过最小多项式定义
COORD_QUADRATIC = 2      # 二次根式：a + b*sqrt(n) 形式
COORD_TRANSCENDENTAL = 3 # 超越数：π、e 等非代数数

# ============================================================
# Groebner 引擎函数签名
# ============================================================
# Groebner基计算引擎，借鉴Singular/Macaulay2的多项式理想与Gröbner基计算

# 环管理
_lib.ring_registry_create.argtypes = [c_int]
_lib.ring_registry_create.restype = c_void_p

_lib.ring_registry_destroy.argtypes = [c_void_p]
_lib.ring_registry_destroy.restype = None

_lib.ring_create.argtypes = [c_void_p, POINTER(c_char_p), c_int, c_int, c_int, c_char_p]
_lib.ring_create.restype = c_int

_lib.ring_destroy.argtypes = [c_void_p, c_int]
_lib.ring_destroy.restype = None

_lib.ring_register.argtypes = [c_void_p, c_void_p]
_lib.ring_register.restype = c_int

_lib.ring_find.argtypes = [c_void_p, c_int]
_lib.ring_find.restype = c_void_p

# 多项式操作
_lib.poly_create.argtypes = [c_void_p, c_int, c_int, c_char_p]
_lib.poly_create.restype = c_int

_lib.poly_destroy.argtypes = [c_void_p, c_int]
_lib.poly_destroy.restype = None

_lib.poly_add.argtypes = [c_void_p, c_int, c_int, c_char_p]
_lib.poly_add.restype = c_int

_lib.poly_multiply.argtypes = [c_void_p, c_int, c_int, c_char_p]
_lib.poly_multiply.restype = c_int

_lib.poly_substitute.argtypes = [c_void_p, c_int, c_int, c_int, c_char_p]
_lib.poly_substitute.restype = c_int

_lib.poly_get.argtypes = [c_void_p, c_int]
_lib.poly_get.restype = c_void_p

# 理想与Groebner基
_lib.ideal_create.argtypes = [c_void_p, c_int, c_char_p]
_lib.ideal_create.restype = c_int

_lib.ideal_destroy.argtypes = [c_void_p, c_int]
_lib.ideal_destroy.restype = None

_lib.ideal_add_generator.argtypes = [c_void_p, c_int, c_int]
_lib.ideal_add_generator.restype = c_int

_lib.groebner_compute.argtypes = [c_void_p, c_int, c_int]
_lib.groebner_compute.restype = c_int

_lib.groebner_compute_incremental.argtypes = [c_void_p, c_int, c_int]
_lib.groebner_compute_incremental.restype = c_int

_lib.ideal_membership.argtypes = [c_void_p, c_int, c_int]
_lib.ideal_membership.restype = c_bool

_lib.ideal_intersection.argtypes = [c_void_p, c_int, c_int, c_char_p]
_lib.ideal_intersection.restype = c_int

_lib.ideal_quotient.argtypes = [c_void_p, c_int, c_int, c_char_p]
_lib.ideal_quotient.restype = c_int

# 代数簇
_lib.variety_compute.argtypes = [c_void_p, c_int, c_char_p]
_lib.variety_compute.restype = c_int

_lib.variety_dimension.argtypes = [c_void_p, c_int]
_lib.variety_dimension.restype = c_int

_lib.variety_is_zero_dimensional.argtypes = [c_void_p, c_int]
_lib.variety_is_zero_dimensional.restype = c_bool

# 约束图->多项式理想转换
_lib.constraint_graph_to_ideal.argtypes = [c_void_p, POINTER(_ConstraintGraph), c_int, c_char_p]
_lib.constraint_graph_to_ideal.restype = c_int

# ============================================================
# 类型系统函数签名
# ============================================================
# 类型系统：宇宙层级、类型等价检查、类型推断

# 流式上下文
_lib.type_system_set_stream_context.argtypes = [c_void_p]
_lib.type_system_set_stream_context.restype = None

# 类型系统管理
_lib.type_system_create.argtypes = []
_lib.type_system_create.restype = c_void_p

_lib.type_system_destroy.argtypes = [c_void_p]
_lib.type_system_destroy.restype = None

_lib.type_system_set_well_founded.argtypes = [c_void_p, c_bool]
_lib.type_system_set_well_founded.restype = None

_lib.type_system_set_cumulative.argtypes = [c_void_p, c_bool]
_lib.type_system_set_cumulative.restype = None

# 类型区域创建
_lib.type_create_point.argtypes = [c_void_p]
_lib.type_create_point.restype = c_void_p

_lib.type_create_line_segment.argtypes = [c_void_p]
_lib.type_create_line_segment.restype = c_void_p

_lib.type_create_region.argtypes = [c_void_p, POINTER(c_int), c_int]
_lib.type_create_region.restype = c_void_p

_lib.type_create_function.argtypes = [c_void_p, c_void_p, c_void_p]
_lib.type_create_function.restype = c_void_p

_lib.type_create_product.argtypes = [c_void_p, c_void_p, c_void_p]
_lib.type_create_product.restype = c_void_p

_lib.type_create_sum.argtypes = [c_void_p, c_void_p, c_void_p]
_lib.type_create_sum.restype = c_void_p

_lib.type_create_variable.argtypes = [c_void_p, c_char_p]
_lib.type_create_variable.restype = c_void_p

_lib.type_create_dependent.argtypes = [c_void_p, c_int, c_void_p]
_lib.type_create_dependent.restype = c_void_p

_lib.type_create_bottom.argtypes = [c_void_p]
_lib.type_create_bottom.restype = c_void_p

_lib.type_region_destroy.argtypes = [c_void_p]
_lib.type_region_destroy.restype = None

_lib.type_add_alias.argtypes = [c_void_p, c_char_p]
_lib.type_add_alias.restype = c_bool

# 宇宙层级
_lib.type_get_level.argtypes = [c_void_p]
_lib.type_get_level.restype = c_int

_lib.type_check_level_validity.argtypes = [c_void_p, c_void_p, c_void_p]
_lib.type_check_level_validity.restype = c_bool

_lib.type_check_cumulative.argtypes = [c_void_p, c_void_p, c_void_p]
_lib.type_check_cumulative.restype = c_bool

# 类型等价检查
_lib.type_check_equivalence.argtypes = [c_void_p, c_void_p, c_void_p, c_bool]
_lib.type_check_equivalence.restype = c_int

_lib.type_check_port_compatibility.argtypes = [c_void_p, c_void_p, c_void_p]
_lib.type_check_port_compatibility.restype = c_int

# 类型推断
_lib.type_infer_node.argtypes = [c_void_p, POINTER(_ConstraintGraph), c_int, POINTER(c_void_p)]
_lib.type_infer_node.restype = c_bool

_lib.type_infer_port.argtypes = [c_void_p, POINTER(_ConstraintGraph), c_int, POINTER(c_void_p)]
_lib.type_infer_port.restype = c_bool

# 类型变量实例化
_lib.type_instantiate_variable.argtypes = [c_void_p, c_int, c_void_p]
_lib.type_instantiate_variable.restype = c_bool

_lib.type_substitute_variable.argtypes = [c_void_p, c_void_p, c_int, c_void_p, POINTER(c_void_p)]
_lib.type_substitute_variable.restype = c_bool

# 非良基模式
_lib.type_detect_cycle.argtypes = [c_void_p, c_void_p]
_lib.type_detect_cycle.restype = c_bool

_lib.type_check_non_well_founded_compatibility.argtypes = [c_void_p, c_void_p]
_lib.type_check_non_well_founded_compatibility.restype = c_bool

# 类型规范化
_lib.type_normalize.argtypes = [c_void_p, c_void_p, POINTER(c_void_p)]
_lib.type_normalize.restype = c_bool

# 类型附加到节点
_lib.type_attach_to_node.argtypes = [c_void_p, c_int, c_void_p]
_lib.type_attach_to_node.restype = c_bool

_lib.type_get_node_type.argtypes = [c_void_p, c_int]
_lib.type_get_node_type.restype = c_void_p

_lib.type_detach_node_type.argtypes = [c_void_p, c_int]
_lib.type_detach_node_type.restype = c_bool

# 依赖类型检查
_lib.type_check_dependent.argtypes = [c_void_p, c_void_p, c_void_p, c_void_p]
_lib.type_check_dependent.restype = c_bool

# 辅助函数
_lib.type_kind_to_string.argtypes = [c_int]
_lib.type_kind_to_string.restype = c_char_p

_lib.universe_level_to_string.argtypes = [c_int]
_lib.universe_level_to_string.restype = c_char_p

_lib.type_equiv_result_to_string.argtypes = [c_int]
_lib.type_equiv_result_to_string.restype = c_char_p

_lib.type_check_result_to_string.argtypes = [c_int]
_lib.type_check_result_to_string.restype = c_char_p

_lib.type_print.argtypes = [c_void_p, c_int]
_lib.type_print.restype = None

# 规则表驱动的类型推断
_lib.type_system_register_inference_rule.argtypes = [c_void_p, c_int, c_int, c_int, c_char_p]
_lib.type_system_register_inference_rule.restype = c_int

_lib.type_system_get_inference_rules.argtypes = [c_void_p, POINTER(c_int)]
_lib.type_system_get_inference_rules.restype = c_void_p

_lib.type_system_clear_inference_rules.argtypes = [c_void_p]
_lib.type_system_clear_inference_rules.restype = None

_lib.type_infer_by_rules.argtypes = [c_void_p, POINTER(_ConstraintGraph), c_int]
_lib.type_infer_by_rules.restype = c_int

# 重写路径
_lib.type_rewrite_path_create.argtypes = []
_lib.type_rewrite_path_create.restype = c_void_p

_lib.type_rewrite_path_destroy.argtypes = [c_void_p]
_lib.type_rewrite_path_destroy.restype = None

_lib.type_rewrite_path_record.argtypes = [c_void_p, c_char_p, c_void_p, c_void_p]
_lib.type_rewrite_path_record.restype = None

_lib.type_rewrite_path_replay.argtypes = [c_void_p, c_int]
_lib.type_rewrite_path_replay.restype = c_bool

_lib.type_system_get_rewrite_path.argtypes = [c_void_p]
_lib.type_system_get_rewrite_path.restype = c_void_p

# 路径探索器
_lib.path_explorer_create.argtypes = [c_void_p, c_void_p, c_void_p]
_lib.path_explorer_create.restype = c_void_p

_lib.path_explorer_destroy.argtypes = [c_void_p]
_lib.path_explorer_destroy.restype = None

_lib.path_explorer_get_applicable_rules.argtypes = [c_void_p, POINTER(POINTER(c_int)), POINTER(c_int)]
_lib.path_explorer_get_applicable_rules.restype = c_int

_lib.path_explorer_preview_rule.argtypes = [c_void_p, c_int, POINTER(c_void_p)]
_lib.path_explorer_preview_rule.restype = c_int

_lib.path_explorer_apply_rule.argtypes = [c_void_p, c_int]
_lib.path_explorer_apply_rule.restype = c_int

_lib.path_explorer_undo.argtypes = [c_void_p]
_lib.path_explorer_undo.restype = c_int

_lib.path_explorer_check_goal.argtypes = [c_void_p, POINTER(c_bool)]
_lib.path_explorer_check_goal.restype = c_int

_lib.path_explorer_save_path.argtypes = [c_void_p, POINTER(c_void_p)]
_lib.path_explorer_save_path.restype = c_int

_lib.path_explorer_get_step_count.argtypes = [c_void_p]
_lib.path_explorer_get_step_count.restype = c_int

_lib.path_explorer_get_steps.argtypes = [c_void_p]
_lib.path_explorer_get_steps.restype = c_void_p

_lib.path_explorer_get_current.argtypes = [c_void_p]
_lib.path_explorer_get_current.restype = c_void_p

# ============================================================
# 证明搜索树与多策略引擎函数签名
# ============================================================
# 证明系统的回溯搜索树可视化与多证明方法并存引擎

# 证明搜索树
_lib.proof_search_tree_create.argtypes = []
_lib.proof_search_tree_create.restype = c_void_p

_lib.proof_search_tree_destroy.argtypes = [c_void_p]
_lib.proof_search_tree_destroy.restype = None

_lib.backtrack_node_create.argtypes = [c_int, c_char_p]
_lib.backtrack_node_create.restype = c_void_p

_lib.proof_search_tree_add_child.argtypes = [c_void_p, c_void_p, c_void_p]
_lib.proof_search_tree_add_child.restype = c_bool

_lib.backtrack_node_mark_backtrack.argtypes = [c_void_p, c_char_p]
_lib.backtrack_node_mark_backtrack.restype = None

_lib.proof_search_tree_register_strategy.argtypes = [c_void_p, c_char_p]
_lib.proof_search_tree_register_strategy.restype = None

_lib.proof_search_tree_set_strategy.argtypes = [c_void_p, c_char_p]
_lib.proof_search_tree_set_strategy.restype = None

_lib.proof_search_tree_export_json.argtypes = [c_void_p, c_char_p]
_lib.proof_search_tree_export_json.restype = c_bool

_lib.proof_search_tree_export_dot.argtypes = [c_void_p, c_char_p]
_lib.proof_search_tree_export_dot.restype = c_bool

# 多策略引擎
_lib.proof_multi_strategy_create.argtypes = [POINTER(_ProofNavigator)]
_lib.proof_multi_strategy_create.restype = c_void_p

_lib.proof_multi_strategy_destroy.argtypes = [c_void_p]
_lib.proof_multi_strategy_destroy.restype = None

_lib.proof_multi_strategy_register.argtypes = [c_void_p, c_void_p]
_lib.proof_multi_strategy_register.restype = c_bool

_lib.proof_multi_strategy_activate.argtypes = [c_void_p, c_int]
_lib.proof_multi_strategy_activate.restype = c_bool

_lib.proof_multi_strategy_get_active.argtypes = [c_void_p]
_lib.proof_multi_strategy_get_active.restype = c_void_p

_lib.proof_multi_strategy_evaluate_applicability.argtypes = [c_void_p, POINTER(_ConstraintGraph), POINTER(_Proposition), POINTER(c_int), c_int]
_lib.proof_multi_strategy_evaluate_applicability.restype = c_int

_lib.proof_multi_strategy_execute.argtypes = [c_void_p]
_lib.proof_multi_strategy_execute.restype = c_bool

_lib.proof_multi_strategy_try_all.argtypes = [c_void_p]
_lib.proof_multi_strategy_try_all.restype = c_int

_lib.proof_multi_strategy_pipeline.argtypes = [c_void_p, POINTER(c_int), c_int]
_lib.proof_multi_strategy_pipeline.restype = c_bool

_lib.proof_multi_strategy_set_fallback_order.argtypes = [c_void_p, POINTER(c_int), c_int]
_lib.proof_multi_strategy_set_fallback_order.restype = None

_lib.proof_multi_strategy_switch.argtypes = [c_void_p, c_int]
_lib.proof_multi_strategy_switch.restype = c_bool

_lib.proof_multi_strategy_get_stats.argtypes = [c_void_p, POINTER(c_int), POINTER(c_int)]
_lib.proof_multi_strategy_get_stats.restype = None

_lib.proof_strategy_type_to_string.argtypes = [c_int]
_lib.proof_strategy_type_to_string.restype = c_char_p

_lib.proof_strategy_status_to_string.argtypes = [c_int]
_lib.proof_strategy_status_to_string.restype = c_char_p

# 证明步骤管理（额外API）
_lib.proof_navigator_add_step.argtypes = [POINTER(_ProofNavigator), c_void_p]
_lib.proof_navigator_add_step.restype = c_bool

_lib.proof_navigator_current_step.argtypes = [POINTER(_ProofNavigator)]
_lib.proof_navigator_current_step.restype = c_void_p

_lib.proof_navigator_next_breakpoint.argtypes = [POINTER(_ProofNavigator)]
_lib.proof_navigator_next_breakpoint.restype = c_bool

_lib.proof_navigator_set_strategy_note.argtypes = [POINTER(_ProofNavigator), c_char_p]
_lib.proof_navigator_set_strategy_note.restype = c_bool

_lib.proof_navigator_get_strategy_note.argtypes = [POINTER(_ProofNavigator)]
_lib.proof_navigator_get_strategy_note.restype = c_char_p

_lib.proof_step_create.argtypes = [c_int]
_lib.proof_step_create.restype = c_void_p

_lib.proof_step_destroy.argtypes = [c_void_p]
_lib.proof_step_destroy.restype = None

_lib.proof_step_add_dependency.argtypes = [c_void_p, c_int]
_lib.proof_step_add_dependency.restype = c_bool

_lib.proof_step_set_breakpoint.argtypes = [c_void_p, c_bool]
_lib.proof_step_set_breakpoint.restype = None

_lib.proof_step_set_note.argtypes = [c_void_p, c_char_p]
_lib.proof_step_set_note.restype = c_bool

_lib.proof_step_get_natural_language.argtypes = [c_void_p, c_int]
_lib.proof_step_get_natural_language.restype = c_char_p

# 证明依赖
_lib.proof_dependency_create.argtypes = [c_int]
_lib.proof_dependency_create.restype = c_void_p

_lib.proof_dependency_destroy.argtypes = [c_void_p]
_lib.proof_dependency_destroy.restype = None

_lib.proof_dependency_add_sub.argtypes = [c_void_p, c_void_p]
_lib.proof_dependency_add_sub.restype = c_bool

_lib.proof_dependency_compute_color.argtypes = [c_void_p]
_lib.proof_dependency_compute_color.restype = c_int

# 证明导出
_lib.proof_export_coq.argtypes = [POINTER(_ProofNavigator), c_char_p]
_lib.proof_export_coq.restype = c_bool

_lib.proof_export_natural_language.argtypes = [POINTER(_ProofNavigator), c_char_p, c_int]
_lib.proof_export_natural_language.restype = c_bool

# 交互式证明
_lib.proof_interactive_step.argtypes = [POINTER(_ProofNavigator), c_int, c_void_p]
_lib.proof_interactive_step.restype = c_bool

_lib.proof_save_breakpoint.argtypes = [POINTER(_ProofNavigator), c_int]
_lib.proof_save_breakpoint.restype = c_bool

_lib.proof_restore_breakpoint.argtypes = [POINTER(_ProofNavigator), c_int]
_lib.proof_restore_breakpoint.restype = c_bool

# 命题等价
_lib.proof_declare_proposition_equivalence.argtypes = [POINTER(_ProofNavigator), c_int, c_int]
_lib.proof_declare_proposition_equivalence.restype = None

_lib.proof_find_equivalent_proposition.argtypes = [POINTER(_ProofNavigator), c_int, POINTER(c_int), c_int]
_lib.proof_find_equivalent_proposition.restype = c_int

# 依赖验证
_lib.proof_validate_dependencies.argtypes = [POINTER(_ProofNavigator), c_void_p, c_int]
_lib.proof_validate_dependencies.restype = c_int

# 矛盾定义
_lib.proof_set_bottom_definition.argtypes = [POINTER(_ProofNavigator), c_void_p]
_lib.proof_set_bottom_definition.restype = None

_lib.proof_get_bottom_definition.argtypes = [POINTER(_ProofNavigator)]
_lib.proof_get_bottom_definition.restype = c_void_p

# 引理折叠
_lib.proof_set_lemma_view_state.argtypes = [POINTER(_ProofNavigator), c_int, c_int]
_lib.proof_set_lemma_view_state.restype = None

_lib.proof_get_lemma_view_state.argtypes = [POINTER(_ProofNavigator), c_int]
_lib.proof_get_lemma_view_state.restype = c_int

# 命题实例化
_lib.proof_has_type_variables.argtypes = [POINTER(_Proposition)]
_lib.proof_has_type_variables.restype = c_bool

_lib.proof_instantiate_proposition.argtypes = [POINTER(_Proposition), POINTER(c_int), c_int]
_lib.proof_instantiate_proposition.restype = POINTER(_Proposition)

# 不可构造性证明
_lib.proof_check_unconstructibility.argtypes = [POINTER(_ProofNavigator), POINTER(_ConstraintGraph), POINTER(_Proposition), c_void_p]
_lib.proof_check_unconstructibility.restype = c_int

_lib.proof_attempt_unconstructibility.argtypes = [POINTER(_ProofNavigator), POINTER(_ConstraintGraph), POINTER(_Proposition), c_void_p]
_lib.proof_attempt_unconstructibility.restype = c_int

_lib.unconstruct_info_destroy.argtypes = [c_void_p]
_lib.unconstruct_info_destroy.restype = None

# 命题管理（额外API）
_lib.proposition_set_input_ports.argtypes = [POINTER(_Proposition), POINTER(c_int), c_int]
_lib.proposition_set_input_ports.restype = c_bool

_lib.proposition_set_output_ports.argtypes = [POINTER(_Proposition), POINTER(c_int), c_int]
_lib.proposition_set_output_ports.restype = c_bool

_lib.proposition_set_pattern.argtypes = [POINTER(_Proposition), POINTER(_ConstraintGraph)]
_lib.proposition_set_pattern.restype = c_bool

_lib.proposition_set_preconditions.argtypes = [POINTER(_Proposition), POINTER(c_int), c_int]
_lib.proposition_set_preconditions.restype = c_bool

_lib.proposition_set_postconditions.argtypes = [POINTER(_Proposition), POINTER(c_int), c_int]
_lib.proposition_set_postconditions.restype = c_bool

_lib.proposition_add_sub_proposition.argtypes = [POINTER(_Proposition), POINTER(_Proposition)]
_lib.proposition_add_sub_proposition.restype = c_bool

# 辅助函数
_lib.proof_color_to_string.argtypes = [c_int]
_lib.proof_color_to_string.restype = c_char_p

_lib.proposition_type_to_string.argtypes = [c_int]
_lib.proposition_type_to_string.restype = c_char_p

_lib.proof_step_type_to_string.argtypes = [c_int]
_lib.proof_step_type_to_string.restype = c_char_p

_lib.unify_result_to_string.argtypes = [c_int]
_lib.unify_result_to_string.restype = c_char_p

# 证明流式上下文
_lib.proof_set_stream_context.argtypes = [c_void_p]
_lib.proof_set_stream_context.restype = None

# Agda/Idris2/Isabelle/F* 借鉴API
_lib.proof_guided_fill.argtypes = [c_void_p, c_char_p, c_int]
_lib.proof_guided_fill.restype = c_void_p

_lib.fill_suggestions_destroy.argtypes = [c_void_p]
_lib.fill_suggestions_destroy.restype = None

_lib.proof_mark_ghost.argtypes = [c_int, c_int]
_lib.proof_mark_ghost.restype = c_bool

_lib.proof_check_ghost_conflicts.argtypes = []
_lib.proof_check_ghost_conflicts.restype = c_int

_lib.proof_sledgehammer_dispatch.argtypes = [c_void_p, c_int, c_int]
_lib.proof_sledgehammer_dispatch.restype = c_void_p

_lib.sledgehammer_report_destroy.argtypes = [c_void_p]
_lib.sledgehammer_report_destroy.restype = None

_lib.proof_export_isar.argtypes = [POINTER(POINTER(_Proposition)), c_int]
_lib.proof_export_isar.restype = c_char_p

_lib.proof_minimal_verify.argtypes = [c_int, POINTER(c_char_p), c_char_p, POINTER(c_char_p)]
_lib.proof_minimal_verify.restype = c_int

_lib.proof_refinement_check.argtypes = [c_void_p, c_void_p, c_int]
_lib.proof_refinement_check.restype = c_void_p

_lib.refinement_check_report_destroy.argtypes = [c_void_p]
_lib.refinement_check_report_destroy.restype = None

# ============================================================
# 交互几何函数签名
# ============================================================
# 借鉴Cinderella与Dr. Geo的交互几何UX设计

_lib.interactive_geo_init.argtypes = [c_void_p]
_lib.interactive_geo_init.restype = c_void_p

_lib.interactive_geo_destroy.argtypes = [c_void_p]
_lib.interactive_geo_destroy.restype = None

_lib.interactive_geo_set_mode.argtypes = [c_void_p, c_int]
_lib.interactive_geo_set_mode.restype = None

_lib.interactive_geo_get_mode.argtypes = [c_void_p]
_lib.interactive_geo_get_mode.restype = c_int

_lib.interactive_geo_select.argtypes = [c_void_p, c_int]
_lib.interactive_geo_select.restype = c_int

_lib.interactive_geo_deselect.argtypes = [c_void_p, c_int]
_lib.interactive_geo_deselect.restype = None

_lib.interactive_geo_drag_start.argtypes = [c_void_p, c_int, c_double, c_double]
_lib.interactive_geo_drag_start.restype = c_int

_lib.interactive_geo_drag_move.argtypes = [c_void_p, c_double, c_double]
_lib.interactive_geo_drag_move.restype = c_int

_lib.interactive_geo_drag_end.argtypes = [c_void_p, c_double, c_double]
_lib.interactive_geo_drag_end.restype = c_int

_lib.interactive_geo_randomized_check.argtypes = [c_void_p, c_int, c_double, c_char_p, c_void_p]
_lib.interactive_geo_randomized_check.restype = c_int

_lib.interactive_geo_generate_script.argtypes = [c_void_p, c_int, POINTER(c_char_p)]
_lib.interactive_geo_generate_script.restype = c_int

_lib.interactive_geo_detect_singularity.argtypes = [c_void_p, POINTER(c_int)]
_lib.interactive_geo_detect_singularity.restype = c_bool

_lib.interactive_geo_maintain_constraints.argtypes = [c_void_p, c_int, c_double, c_double]
_lib.interactive_geo_maintain_constraints.restype = c_int

_lib.interactive_geo_export_state.argtypes = [c_void_p]
# 返回堆分配字符串（需 lv_free_ptr 释放），同 symbolic_coord_serialize 处理
_lib.interactive_geo_export_state.restype = c_void_p

_lib.interactive_geo_import_state.argtypes = [c_void_p, c_char_p]
_lib.interactive_geo_import_state.restype = c_int

_lib.interactive_geo_get_all_objects.argtypes = [c_void_p, POINTER(c_int)]
_lib.interactive_geo_get_all_objects.restype = POINTER(c_int)

_lib.interactive_geo_snapshot.argtypes = [c_void_p]
_lib.interactive_geo_snapshot.restype = c_int

_lib.interactive_geo_restore.argtypes = [c_void_p, c_int]
_lib.interactive_geo_restore.restype = c_int

# ============================================================
# 稀疏线性代数函数签名
# ============================================================
# SuiteSparse/GraphBLAS风格的半环矩阵运算与约束传播
#
# 注意：以下 sparse_* 系列 API 在 C 库中为"可选扩展"（从未实现/未导出），
# 通过 _bind_if_present 存在性检查绑定：符号存在才设置签名，缺失时跳过
# （调用方 sparse_la.py 已有 try/except AttributeError 回退，如
# graph_degree_analysis 的纯 Python 回退）。若在此无条件访问缺失符号，
# import lv 包即崩溃，导致所有 Python 测试收集失败。

def _bind_if_present(name, argtypes, restype):
    """仅在 C 库导出该符号时绑定其 ctypes 签名；缺失返回 None。"""
    if hasattr(_lib, name):
        fn = getattr(_lib, name)
        fn.argtypes = argtypes
        fn.restype = restype
        return fn
    return None

_bind_if_present('sparse_matrix_create', [c_int, c_int, c_int], c_void_p)
_bind_if_present('sparse_matrix_destroy', [c_void_p], None)
_bind_if_present('sparse_matrix_clone', [c_void_p], c_void_p)
_bind_if_present('sparse_matrix_print', [c_void_p, c_char_p], None)
_bind_if_present('sparse_matrix_get_dims', [c_void_p, POINTER(c_int), POINTER(c_int)], c_int)

# 半环
# semiring_create 按值返回结构体，ctypes 无法直接处理，需要通过包装函数
# 此处声明为返回 void*（需 C 侧提供包装）

_bind_if_present('semiring_propagate_constraints', [POINTER(_ConstraintGraph), c_int, POINTER(c_double), c_int], c_int)
_bind_if_present('sparse_cholesky_solve', [c_void_p, POINTER(c_double), POINTER(c_double)], c_bool)
_bind_if_present('sparse_lu_solve', [c_void_p, POINTER(c_double), POINTER(c_double)], c_bool)
_bind_if_present('sparse_qr_solve', [c_void_p, POINTER(c_double), POINTER(c_double)], c_bool)
_bind_if_present('graph_to_constraint_matrix', [POINTER(_ConstraintGraph), POINTER(c_void_p)], c_bool)
_bind_if_present('sparse_matrix_multiply', [c_void_p, c_void_p, POINTER(c_void_p)], c_bool)
_bind_if_present('sparse_matrix_transpose', [c_void_p, POINTER(c_void_p)], c_bool)
_bind_if_present('graph_degree_analysis', [POINTER(_ConstraintGraph), POINTER(c_void_p)], c_bool)
_bind_if_present('degree_analysis_free', [c_void_p], None)
