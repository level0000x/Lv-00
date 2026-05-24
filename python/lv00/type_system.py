"""
Lv-00 类型系统模块

提供类型系统的 Python 接口，支持宇宙层级、类型等价检查、类型推断。
借鉴 Agda/Idris/Dependent Type Theory 的类型系统设计。

功能：
    - 类型系统创建与配置（良基模式、累积性）
    - 类型区域创建（点/线段/区域/函数/乘积/和/变量/依赖/Bottom）
    - 宇宙层级检查
    - 类型等价检查与端口兼容性检查
    - 类型推断（节点/端口）
    - 类型变量实例化与替换
    - 非良基模式检测
    - 类型规范化
    - 节点-类型附加/查询/分离
    - 依赖类型检查
    - 规则表驱动的类型推断
    - 重写路径记录与回放
    - 交互式路径探索器

版本：3.3.0
作者：Lv-00 开发团队
"""

import ctypes
from typing import Any, List, Optional, Tuple

from ._ctypes_binding import _lib, _ConstraintGraph, c_int, c_char_p, c_void_p, c_bool, POINTER

__all__ = [
    "TypeKind", "TypeEquivResult", "TypeCheckResult",
    "TypeSystem", "TypeRegion", "TypeSystemError",
    "type_system_create", 
    "type_check_equivalence", "type_check_port_compatibility",
    "type_infer_node", "type_infer_port",
    "type_infer_by_rules",
    "type_system_register_inference_rule",
    "PathExplorer",
]


# ============================================================
# 枚举常量
# ============================================================

class TypeKind:
    """类型种类枚举。

    常量:
        POINT (0): 点类型
        LINE_SEGMENT (1): 线段类型
        REGION (2): 区域类型
        FUNCTION (3): 函数类型
        PRODUCT (4): 乘积类型
        SUM (5): 和类型
        VARIABLE (6): 类型变量（多态）
        DEPENDENT (7): 依赖类型
        BOTTOM (8): 底类型
    """
    POINT = 0
    LINE_SEGMENT = 1
    REGION = 2
    FUNCTION = 3
    PRODUCT = 4
    SUM = 5
    VARIABLE = 6
    DEPENDENT = 7
    BOTTOM = 8


class TypeEquivResult:
    """类型等价检查结果枚举。

    常量:
        OK (0): 类型等价
        NOT_EQUIV (1): 类型不等价
        UNKNOWN (2): 未能证明等价
        ERROR (3): 检查出错
        NEEDS_INTERACTION (4): 需要交互式证明
    """
    OK = 0
    NOT_EQUIV = 1
    UNKNOWN = 2
    ERROR = 3
    NEEDS_INTERACTION = 4


class TypeCheckResult:
    """类型检查结果枚举。

    常量:
        OK (0): 类型检查通过
        MISMATCH (1): 类型不匹配
        INCOMPATIBLE (2): 类型不兼容
        LEVEL_ERROR (3): 宇宙层级错误
        CYCLE (4): 类型循环
        INFERRED (5): 类型已推断
        ERROR (6): 检查出错
    """
    OK = 0
    MISMATCH = 1
    INCOMPATIBLE = 2
    LEVEL_ERROR = 3
    CYCLE = 4
    INFERRED = 5
    ERROR = 6


# ============================================================
# 异常类
# ============================================================

class TypeSystemError(Exception):
    """类型系统错误基类。

    所有类型系统相关异常的父类。

    属性:
        message: 异常消息字符串
        error_code: 可选的错误码（整数，默认为 -1）
    """

    def __init__(self, message: str = "", error_code: int = -1) -> None:
        super().__init__(message)
        self.message: str = message
        self.error_code: int = error_code

    def __str__(self) -> str:
        if self.error_code >= 0:
            return f"{self.__class__.__name__}({self.error_code}): {self.message}"
        return f"{self.__class__.__name__}: {self.message}" if self.message else self.__class__.__name__


# ============================================================
# TypeRegion 类
# ============================================================

class TypeRegion:
    """类型区域包装类。

    表示类型系统中的一个类型区域节点，封装底层 C 指针。
    通过 `_ptr` 属性访问底层 C 指针，内部通过 type_region_destroy 管理生命周期。

    属性:
        _ptr: 底层 C TypeRegion 指针（不透明）
        _owns_ptr: 是否拥有指针所有权（析构时决定是否释放）
    """

    def __init__(self, ptr, owns: bool = True) -> None:
        """创建 TypeRegion 包装。

        参数:
            ptr: 底层 C TypeRegion 指针
            owns: 是否拥有所有权（默认 True）
        """
        self._ptr = ptr
        self._owns_ptr: bool = owns

    def __del__(self) -> None:
        """析构：释放底层 C 类型区域资源。"""
        if hasattr(self, '_ptr') and self._ptr and getattr(self, '_owns_ptr', False):
            try:
                _lib.type_region_destroy(self._ptr)
            except Exception:
                pass
            self._ptr = None

    def add_alias(self, alias: str) -> bool:
        """添加类型别名。

        参数:
            alias: 别名字符串

        返回:
            bool: 成功返回 True
        """
        return _lib.type_add_alias(self._ptr, alias.encode('utf-8'))

    def get_level(self) -> int:
        """获取类型的宇宙层级。

        返回:
            int: 宇宙层级编号
        """
        return _lib.type_get_level(self._ptr)


# ============================================================
# TypeSystem 类
# ============================================================

class TypeSystem:
    """类型系统上下文类。

    封装 Lv-00 类型系统的完整功能，包括类型区域管理、
    等价检查、推断、规范化等。所有类型操作都在此上下文中进行。

    属性:
        _ptr: 底层 C TypeSystem 指针
    """

    def __init__(self) -> None:
        """创建类型系统。

        异常:
            TypeSystemError: 创建失败
        """
        self._ptr = _lib.type_system_create()
        if not self._ptr:
            raise TypeSystemError("创建类型系统失败")

    def __del__(self) -> None:
        """析构：释放类型系统资源。"""
        if hasattr(self, '_ptr') and self._ptr:
            try:
                _lib.type_system_destroy(self._ptr)
            except Exception:
                pass
            self._ptr = None

    def set_well_founded(self, well_founded: bool) -> None:
        """设置良基模式。

        参数:
            well_founded: True 启用良基模式
        """
        _lib.type_system_set_well_founded(self._ptr, well_founded)

    def set_cumulative(self, cumulative: bool) -> None:
        """设置累积性。

        参数:
            cumulative: True 启用累积性
        """
        _lib.type_system_set_cumulative(self._ptr, cumulative)

    # ---- 类型区域创建 ----

    def create_point(self) -> TypeRegion:
        """创建点类型。

        返回:
            TypeRegion: 点类型区域
        """
        ptr = _lib.type_create_point(self._ptr)
        if not ptr:
            raise TypeSystemError("创建点类型失败")
        return TypeRegion(ptr)

    def create_line_segment(self) -> TypeRegion:
        """创建线段类型。

        返回:
            TypeRegion: 线段类型区域
        """
        ptr = _lib.type_create_line_segment(self._ptr)
        if not ptr:
            raise TypeSystemError("创建线段类型失败")
        return TypeRegion(ptr)

    def create_region(self, contained_ids: List[int]) -> TypeRegion:
        """创建区域类型。

        参数:
            contained_ids: 包含的节点 ID 列表

        返回:
            TypeRegion: 区域类型区域
        """
        arr = (c_int * len(contained_ids))(*contained_ids)
        ptr = _lib.type_create_region(self._ptr, arr, len(contained_ids))
        if not ptr:
            raise TypeSystemError("创建区域类型失败")
        return TypeRegion(ptr)

    def create_function(self, input_type: TypeRegion, output_type: TypeRegion) -> TypeRegion:
        """创建函数类型。

        参数:
            input_type: 输入类型
            output_type: 输出类型

        返回:
            TypeRegion: 函数类型区域
        """
        ptr = _lib.type_create_function(self._ptr, input_type._ptr, output_type._ptr)
        if not ptr:
            raise TypeSystemError("创建函数类型失败")
        return TypeRegion(ptr)

    def create_product(self, left: TypeRegion, right: TypeRegion) -> TypeRegion:
        """创建乘积类型。

        参数:
            left: 左类型
            right: 右类型

        返回:
            TypeRegion: 乘积类型区域
        """
        ptr = _lib.type_create_product(self._ptr, left._ptr, right._ptr)
        if not ptr:
            raise TypeSystemError("创建乘积类型失败")
        return TypeRegion(ptr)

    def create_sum(self, first: TypeRegion, second: TypeRegion) -> TypeRegion:
        """创建和类型。

        参数:
            first: 第一类型
            second: 第二类型

        返回:
            TypeRegion: 和类型区域
        """
        ptr = _lib.type_create_sum(self._ptr, first._ptr, second._ptr)
        if not ptr:
            raise TypeSystemError("创建和类型失败")
        return TypeRegion(ptr)

    def create_variable(self, name: str) -> TypeRegion:
        """创建类型变量。

        参数:
            name: 变量名称

        返回:
            TypeRegion: 类型变量区域
        """
        ptr = _lib.type_create_variable(self._ptr, name.encode('utf-8'))
        if not ptr:
            raise TypeSystemError("创建类型变量失败")
        return TypeRegion(ptr)

    def create_dependent(self, param_id: int, body: TypeRegion) -> TypeRegion:
        """创建依赖类型。

        参数:
            param_id: 参数节点 ID
            body: 体类型

        返回:
            TypeRegion: 依赖类型区域
        """
        ptr = _lib.type_create_dependent(self._ptr, param_id, body._ptr)
        if not ptr:
            raise TypeSystemError("创建依赖类型失败")
        return TypeRegion(ptr)

    def create_bottom(self) -> TypeRegion:
        """创建底部类型（矛盾类型）。

        返回:
            TypeRegion: 底部类型区域
        """
        ptr = _lib.type_create_bottom(self._ptr)
        if not ptr:
            raise TypeSystemError("创建底部类型失败")
        return TypeRegion(ptr)

    # ---- 宇宙层级 ----

    def check_level_validity(self, container: TypeRegion, contained: TypeRegion) -> bool:
        """检查层级有效性。

        参数:
            container: 包含者类型
            contained: 被包含者类型

        返回:
            bool: 层级有效返回 True
        """
        return _lib.type_check_level_validity(self._ptr, container._ptr, contained._ptr)

    def check_cumulative(self, lower: TypeRegion, higher: TypeRegion) -> bool:
        """检查累积性。

        参数:
            lower: 较低层级类型
            higher: 较高层级类型

        返回:
            bool: lower 自动属于 higher 返回 True
        """
        return _lib.type_check_cumulative(self._ptr, lower._ptr, higher._ptr)

    # ---- 类型等价检查 ----

    def check_equivalence(self, type1: TypeRegion, type2: TypeRegion,
                          use_rewrite: bool = True) -> int:
        """检查两个类型是否等价。

        参数:
            type1: 第一个类型
            type2: 第二个类型
            use_rewrite: 是否使用重写引擎

        返回:
            int: TypeEquivResult 枚举值
        """
        return _lib.type_check_equivalence(self._ptr, type1._ptr, type2._ptr, use_rewrite)

    def check_port_compatibility(self, source: TypeRegion, target: TypeRegion) -> int:
        """检查端口类型兼容性。

        参数:
            source: 源端口类型
            target: 目标端口类型

        返回:
            int: TypeCheckResult 枚举值
        """
        return _lib.type_check_port_compatibility(self._ptr, source._ptr, target._ptr)

    # ---- 类型推断 ----

    def infer_node(self, graph, node_id: int) -> Optional[TypeRegion]:
        """推断节点类型。

        参数:
            graph: 约束图对象
            node_id: 节点 ID

        返回:
            Optional[TypeRegion]: 推断出的类型，失败返回 None

        异常:
            TypeError: graph 没有有效的 _ptr 属性
        """
        if not hasattr(graph, '_ptr') or not graph._ptr:
            raise TypeError("graph 必须具有有效的 _ptr 属性")
        out_ptr = c_void_p()
        success = _lib.type_infer_node(self._ptr, graph._ptr, node_id, ctypes.byref(out_ptr))
        if success and out_ptr:
            return TypeRegion(out_ptr)
        return None

    def infer_port(self, graph, port_id: int) -> Optional[TypeRegion]:
        """推断端口类型。

        参数:
            graph: 约束图对象
            port_id: 端口 ID

        返回:
            Optional[TypeRegion]: 推断出的类型，失败返回 None
        """
        if not hasattr(graph, '_ptr') or not graph._ptr:
            raise TypeError("graph 必须具有有效的 _ptr 属性")
        out_ptr = c_void_p()
        success = _lib.type_infer_port(self._ptr, graph._ptr, port_id, ctypes.byref(out_ptr))
        if success and out_ptr:
            return TypeRegion(out_ptr)
        return None

    # ---- 类型变量实例化 ----

    def instantiate_variable(self, var_id: int, concrete_type: TypeRegion) -> bool:
        """实例化类型变量。

        参数:
            var_id: 变量 ID
            concrete_type: 具体类型

        返回:
            bool: 成功返回 True
        """
        return _lib.type_instantiate_variable(self._ptr, var_id, concrete_type._ptr)

    def substitute_variable(self, t: TypeRegion, var_id: int,
                            replacement: TypeRegion) -> Optional[TypeRegion]:
        """替换类型中的变量。

        参数:
            t: 类型
            var_id: 变量 ID
            replacement: 替换类型

        返回:
            Optional[TypeRegion]: 结果类型，失败返回 None
        """
        out_ptr = c_void_p()
        success = _lib.type_substitute_variable(self._ptr, t._ptr, var_id, replacement._ptr,
                                                ctypes.byref(out_ptr))
        if success and out_ptr:
            return TypeRegion(out_ptr)
        return None

    # ---- 非良基模式 ----

    def detect_cycle(self, t: TypeRegion) -> bool:
        """检测类型循环。

        参数:
            t: 类型

        返回:
            bool: 存在循环返回 True
        """
        return _lib.type_detect_cycle(self._ptr, t._ptr)

    def check_non_well_founded_compatibility(self, t: TypeRegion) -> bool:
        """检查非良基相容性。

        参数:
            t: 类型

        返回:
            bool: 相容返回 True
        """
        return _lib.type_check_non_well_founded_compatibility(self._ptr, t._ptr)

    # ---- 类型规范化 ----

    def normalize(self, t: TypeRegion) -> Optional[TypeRegion]:
        """规范化类型。

        参数:
            t: 类型

        返回:
            Optional[TypeRegion]: 规范化后的类型，失败返回 None
        """
        out_ptr = c_void_p()
        success = _lib.type_normalize(self._ptr, t._ptr, ctypes.byref(out_ptr))
        if success and out_ptr:
            return TypeRegion(out_ptr)
        return None

    # ---- 节点-类型附加 ----

    def attach_to_node(self, node_id: int, t: TypeRegion) -> bool:
        """将类型区域附加到节点。

        参数:
            node_id: 节点 ID
            t: 要附加的类型区域

        返回:
            bool: 成功返回 True
        """
        return _lib.type_attach_to_node(self._ptr, node_id, t._ptr)

    def get_node_type(self, node_id: int) -> Optional[TypeRegion]:
        """获取节点附加的类型区域。

        参数:
            node_id: 节点 ID

        返回:
            Optional[TypeRegion]: 类型区域，未找到返回 None
        """
        ptr = _lib.type_get_node_type(self._ptr, node_id)
        if ptr:
            return TypeRegion(ptr, owns=False)
        return None

    def detach_node_type(self, node_id: int) -> bool:
        """从节点分离类型区域。

        参数:
            node_id: 节点 ID

        返回:
            bool: 成功返回 True
        """
        return _lib.type_detach_node_type(self._ptr, node_id)

    # ---- 依赖类型检查 ----

    def check_dependent(self, output_type: TypeRegion, input_type: TypeRegion,
                        input_values: Any = None) -> bool:
        """依赖类型检查。

        参数:
            output_type: 输出类型
            input_type: 输入类型
            input_values: 输入值（可为 None）

        返回:
            bool: 兼容返回 True
        """
        return _lib.type_check_dependent(self._ptr, output_type._ptr, input_type._ptr,
                                          None if input_values is None else input_values)

    # ---- 规则表驱动的类型推断 ----

    def register_inference_rule(self, source_node_type: int, target_type_kind: int,
                                 priority: int, description: str) -> int:
        """注册一条类型推断规则。

        参数:
            source_node_type: 源节点几何类型
            target_type_kind: 目标类型种类
            priority: 规则优先级（数值越小优先级越高）
            description: 人类可读的规则描述

        返回:
            int: 0 成功，-1 失败
        """
        return _lib.type_system_register_inference_rule(
            self._ptr, source_node_type, target_type_kind, priority,
            description.encode('utf-8'))

    def clear_inference_rules(self) -> None:
        """清除所有自定义推断规则（恢复为默认规则集）。"""
        _lib.type_system_clear_inference_rules(self._ptr)

    def infer_by_rules(self, graph, node_id: int) -> int:
        """使用已注册的规则链执行类型推断。

        参数:
            graph: 约束图对象
            node_id: 节点 ID

        返回:
            int: TypeEquivResult 枚举值

        异常:
            TypeError: graph 没有有效的 _ptr 属性
        """
        if not hasattr(graph, '_ptr') or not graph._ptr:
            raise TypeError("graph 必须具有有效的 _ptr 属性")
        return _lib.type_infer_by_rules(self._ptr, graph._ptr, node_id)

    # ---- 重写路径 ----

    def get_rewrite_path(self) -> Any:
        """获取类型系统的重写路径。

        返回:
            Any: 重写路径指针（只读），未设置返回 None
        """
        return _lib.type_system_get_rewrite_path(self._ptr)

    # ---- 路径探索器 ----

    def create_path_explorer(self, current: TypeRegion, target: TypeRegion) -> 'PathExplorer':
        """创建路径探索器。

        参数:
            current: 当前类型区域（探索起点）
            target: 目标类型区域（探索终点）

        返回:
            PathExplorer: 路径探索器

        异常:
            TypeSystemError: 创建失败
        """
        return PathExplorer(self, current, target)


# ============================================================
# PathExplorer 类
# ============================================================

class PathExplorer:
    """路径探索器——交互式类型重写路径搜索。

    提供在 TypeSystem 中从当前类型区域探索到目标类型区域的
    交互式路径搜索功能。

    属性:
        _ptr: 底层 C PathExplorer 指针
    """

    def __init__(self, ts: TypeSystem, current: TypeRegion, target: TypeRegion) -> None:
        """创建路径探索器。

        参数:
            ts: 类型系统
            current: 当前类型区域（探索起点）
            target: 目标类型区域（探索终点）

        异常:
            TypeSystemError: 创建失败
        """
        self._ptr = _lib.path_explorer_create(ts._ptr, current._ptr, target._ptr)
        if not self._ptr:
            raise TypeSystemError("创建路径探索器失败")

    def __del__(self) -> None:
        """析构：释放路径探索器资源。"""
        if hasattr(self, '_ptr') and self._ptr:
            try:
                _lib.path_explorer_destroy(self._ptr)
            except Exception:
                pass
            self._ptr = None

    def get_applicable_rules(self) -> List[int]:
        """获取当前状态下可应用的规则列表。

        返回:
            List[int]: 可应用规则索引列表
        """
        indices_ptr = POINTER(c_int)()
        count = c_int()
        _lib.path_explorer_get_applicable_rules(self._ptr, ctypes.byref(indices_ptr), ctypes.byref(count))
        if not indices_ptr or count.value == 0:
            return []
        result = [indices_ptr[i] for i in range(count.value)]
        _lib.lv00_free_ptr(indices_ptr)
        return result

    def preview_rule(self, rule_index: int) -> Optional[TypeRegion]:
        """预览规则应用效果（不修改状态）。

        参数:
            rule_index: 规则索引

        返回:
            Optional[TypeRegion]: 预览结果类型区域，失败返回 None
        """
        out_ptr = c_void_p()
        result = _lib.path_explorer_preview_rule(self._ptr, rule_index, ctypes.byref(out_ptr))
        if result == 0 and out_ptr:
            return TypeRegion(out_ptr)
        return None

    def apply_rule(self, rule_index: int) -> int:
        """应用规则（修改当前状态）。

        参数:
            rule_index: 规则索引

        返回:
            int: ExplorerResult 枚举值（0=成功）
        """
        return _lib.path_explorer_apply_rule(self._ptr, rule_index)

    def undo(self) -> int:
        """撤销上一步操作。

        返回:
            int: ExplorerResult 枚举值（0=成功）
        """
        return _lib.path_explorer_undo(self._ptr)

    def check_goal(self) -> Tuple[bool, int]:
        """检查当前类型是否已达到目标。

        返回:
            Tuple[bool, int]: (是否达到目标, ExplorerResult 枚举值)
        """
        reached = c_bool()
        result = _lib.path_explorer_check_goal(self._ptr, ctypes.byref(reached))
        return (reached.value, result)

    def save_path(self) -> Any:
        """将探索路径导出为 TypeRewritePath。

        返回:
            Any: 重写路径指针，失败返回 None
        """
        out_ptr = c_void_p()
        result = _lib.path_explorer_save_path(self._ptr, ctypes.byref(out_ptr))
        if result == 0 and out_ptr:
            return out_ptr
        return None

    def get_step_count(self) -> int:
        """获取已执行步骤数。

        返回:
            int: 步骤数量
        """
        return _lib.path_explorer_get_step_count(self._ptr)

    def get_current(self) -> Any:
        """获取当前类型区域。

        返回:
            Any: 当前类型区域指针（只读），失败返回 None
        """
        return _lib.path_explorer_get_current(self._ptr)


# ============================================================
# 便捷函数
# ============================================================

def type_system_create() -> TypeSystem:
    """创建类型系统（便捷函数）。

    返回:
        TypeSystem: 新创建的类型系统
    """
    return TypeSystem()


def type_check_equivalence(ts: TypeSystem, type1: TypeRegion, type2: TypeRegion,
                           use_rewrite: bool = True) -> int:
    """检查类型等价（便捷函数）。

    参数:
        ts: 类型系统
        type1: 第一个类型
        type2: 第二个类型
        use_rewrite: 是否使用重写引擎

    返回:
        int: TypeEquivResult 枚举值
    """
    return ts.check_equivalence(type1, type2, use_rewrite)


def type_check_port_compatibility(ts: TypeSystem, source: TypeRegion,
                                   target: TypeRegion) -> int:
    """检查端口类型兼容性（便捷函数）。

    参数:
        ts: 类型系统
        source: 源端口类型
        target: 目标端口类型

    返回:
        int: TypeCheckResult 枚举值
    """
    return ts.check_port_compatibility(source, target)


def type_infer_node(ts: TypeSystem, graph, node_id: int) -> Optional[TypeRegion]:
    """推断节点类型（便捷函数）。

    参数:
        ts: 类型系统
        graph: 约束图对象
        node_id: 节点 ID

    返回:
        Optional[TypeRegion]: 推断出的类型
    """
    return ts.infer_node(graph, node_id)


def type_infer_port(ts: TypeSystem, graph, port_id: int) -> Optional[TypeRegion]:
    """推断端口类型（便捷函数）。

    参数:
        ts: 类型系统
        graph: 约束图对象
        port_id: 端口 ID

    返回:
        Optional[TypeRegion]: 推断出的类型
    """
    return ts.infer_port(graph, port_id)


def type_infer_by_rules(ts: TypeSystem, graph, node_id: int) -> int:
    """使用规则链推断类型（便捷函数）。

    参数:
        ts: 类型系统
        graph: 约束图对象
        node_id: 节点 ID

    返回:
        int: TypeEquivResult 枚举值
    """
    return ts.infer_by_rules(graph, node_id)


def type_system_register_inference_rule(ts: TypeSystem, source_node_type: int,
                                         target_type_kind: int, priority: int,
                                         description: str) -> int:
    """注册类型推断规则（便捷函数）。

    参数:
        ts: 类型系统
        source_node_type: 源节点几何类型
        target_type_kind: 目标类型种类
        priority: 规则优先级
        description: 规则描述

    返回:
        int: 0 成功，-1 失败
    """
    return ts.register_inference_rule(source_node_type, target_type_kind, priority, description)
