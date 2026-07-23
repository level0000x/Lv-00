"""
Lv-00 函数块模块 (优化版 v3.5.1)
==========================

模块功能概述：
    提供 Lv-00 函数块系统的 Python 接口。函数块是 Lv-00 系统中实现几何构造
    抽象和复用的核心机制，允许将一组约束子图封装为可参数化的黑盒单元。
    本模块支持函数块的打包、实例化、确定性检查、多解选择、端口依赖管理
    以及跨边界约束处理等功能。

核心类列表：
    - FuncBlock: 函数块主类，封装约束子图和输入/输出端口
    - SolutionSelector: 多解选择器类，用于在函数块产生多个解时选择解
    - DeterminismState: 确定性状态常量类（UNVERIFIED/VERIFIED/NON_DETERMINISTIC/PARTIALLY_VERIFIED）
    - SelectorType: 选择器类型常量类
    - PackResult: 打包结果常量类
    - InstantiateResult: 实例化结果常量类

使用示例：
    >>> from lv.func_block import FuncBlock, DeterminismState, SolutionSelector, SelectorType
    >>> from lv.core import Graph, SymbolicCoord
    >>>
    >>> # 创建函数块定义
    >>> fb = FuncBlock(graph, name="midpoint_func")
    >>> fb.add_input_port("A")
    >>> fb.add_input_port("B")
    >>> fb.add_output_port("M")
    >>>
    >>> # 打包函数块
    >>> fb.pack()
    >>>
    >>> # 检查确定性
    >>> state = fb.check_determinism()
    >>> if state == DeterminismState.VERIFIED:
    ...     print("函数块具有唯一解")
    >>>
    >>> # 实例化函数块
    >>> result = fb.instantiate({"A": point_a, "B": point_b})
    >>> midpoint = result["M"]

与 C 库的绑定关系：
    - FuncBlock 类持有 _FuncBlock (ctypes 指针) ↔ C 层 FuncBlock* 不透明句柄
    - 打包结果常量（PACK_OK, PACK_CROSS_BOUNDARY_CONFLICT 等）映射 C 层枚举值
    - 实例化结果常量（INSTANTIATE_OK, INSTANTIATE_NO_SOLUTION 等）映射 C 层枚举值
    - 确定性常量（DETERMINISM_VERIFIED 等）映射 C 层枚举值
    - 选择器常量（SELECTOR_POSITIVE_ROOT 等）映射 C 层枚举值

设计原则：
    1. 封装性：函数块封装内部约束子图，对外仅暴露输入/输出端口
    2. 确定性追踪：函数块维护确定性状态机，确保使用安全
    3. beta-归约：实例化时执行变量捕获消解，实现正确的参数传递
    4. 组合子支持：预置 Compose、Product 等几何化组合子

版本：3.5.1 (优化版)
作者：Lv-00 开发团队
"""

from __future__ import annotations

import ctypes
import logging
from dataclasses import dataclass
from enum import IntEnum
from typing import Any, Callable, Dict, Final, List, Optional, Protocol, Tuple, TypeVar, Union

from .core import lvBaseError

from ._ctypes_binding import (
    _lib, _FuncBlock, _SymbolicCoord,
    PACK_OK, PACK_CROSS_BOUNDARY_CONFLICT, PACK_INVALID_NODES, 
    PACK_INVALID_PORTS, PACK_OUT_OF_MEMORY, PACK_CANCELLED,
    INSTANTIATE_OK, INSTANTIATE_NO_SOLUTION, INSTANTIATE_MULTIPLE_SOLUTIONS,
    INSTANTIATE_SELECTOR_NEEDED, INSTANTIATE_PRECONDITION_FAILED, INSTANTIATE_OUT_OF_MEMORY,
    DETERMINISM_UNVERIFIED, DETERMINISM_VERIFIED, 
    DETERMINISM_NON_DETERMINISTIC, DETERMINISM_PARTIALLY_VERIFIED,
    SELECTOR_POSITIVE_ROOT, SELECTOR_NEGATIVE_ROOT, SELECTOR_IN_REGION,
    SELECTOR_NEAREST_TO_POINT, SELECTOR_CUSTOM
)

# 模块级常量
__version__: Final[str] = "3.5.1"
__author__: Final[str] = "Lv-00 开发团队"

# 类型变量
T = TypeVar('T')

# 模块级日志记录器
logger = logging.getLogger(__name__)


# ============================================================
# 枚举定义
# ============================================================

class DeterminismState(IntEnum):
    """
    确定性状态枚举。
    
    定义函数块确定性验证的可能状态值，用于标识函数块
    在静态或动态分析下是否具有唯一解。
    """
    UNVERIFIED = DETERMINISM_UNVERIFIED          # 未验证
    VERIFIED = DETERMINISM_VERIFIED              # 已验证（唯一解）
    NON_DETERMINISTIC = DETERMINISM_NON_DETERMINISTIC  # 非确定性（多解）
    PARTIALLY_VERIFIED = DETERMINISM_PARTIALLY_VERIFIED  # 部分验证
    
    def __str__(self) -> str:
        """返回状态的中文描述。"""
        return {
            self.UNVERIFIED: "未验证",
            self.VERIFIED: "已验证（唯一解）",
            self.NON_DETERMINISTIC: "非确定性（多解）",
            self.PARTIALLY_VERIFIED: "部分验证"
        }.get(self, "未知状态")


class SelectorType(IntEnum):
    """
    选择器类型枚举。
    
    定义多解选择器的类型，用于在函数块产生多个解时决定返回哪个解。
    """
    POSITIVE_ROOT = SELECTOR_POSITIVE_ROOT       # 取正根
    NEGATIVE_ROOT = SELECTOR_NEGATIVE_ROOT       # 取负根
    IN_REGION = SELECTOR_IN_REGION               # 取区域内的解
    NEAREST_TO_POINT = SELECTOR_NEAREST_TO_POINT  # 取最近的解
    CUSTOM = SELECTOR_CUSTOM                     # 自定义选择器
    
    def __str__(self) -> str:
        """返回选择器类型的中文描述。"""
        return {
            self.POSITIVE_ROOT: "取正根",
            self.NEGATIVE_ROOT: "取负根",
            self.IN_REGION: "取区域内的解",
            self.NEAREST_TO_POINT: "取最近的解",
            self.CUSTOM: "自定义"
        }.get(self, "未知类型")


class PackResult(IntEnum):
    """
    打包结果枚举。
    
    定义函数块打包操作的返回结果状态码。
    """
    OK = PACK_OK                                 # 打包成功
    CROSS_BOUNDARY_CONFLICT = PACK_CROSS_BOUNDARY_CONFLICT  # 跨边界约束冲突
    INVALID_NODES = PACK_INVALID_NODES           # 无效节点
    INVALID_PORTS = PACK_INVALID_PORTS           # 无效端口
    OUT_OF_MEMORY = PACK_OUT_OF_MEMORY           # 内存不足
    CANCELLED = PACK_CANCELLED                   # 已取消
    
    def __str__(self) -> str:
        """返回结果的中文描述。"""
        return {
            self.OK: "成功",
            self.CROSS_BOUNDARY_CONFLICT: "跨边界约束冲突",
            self.INVALID_NODES: "无效节点",
            self.INVALID_PORTS: "无效端口",
            self.OUT_OF_MEMORY: "内存不足",
            self.CANCELLED: "已取消"
        }.get(self, "未知结果")
    
    @property
    def is_success(self) -> bool:
        """判断打包操作是否成功。"""
        return self == self.OK


class InstantiateResult(IntEnum):
    """
    实例化结果枚举。
    
    定义函数块实例化操作的返回结果状态码。
    """
    OK = INSTANTIATE_OK                          # 实例化成功
    NO_SOLUTION = INSTANTIATE_NO_SOLUTION        # 无解
    MULTIPLE_SOLUTIONS = INSTANTIATE_MULTIPLE_SOLUTIONS  # 多解
    SELECTOR_NEEDED = INSTANTIATE_SELECTOR_NEEDED  # 需要选择器
    PRECONDITION_FAILED = INSTANTIATE_PRECONDITION_FAILED  # 前置条件不满足
    OUT_OF_MEMORY = INSTANTIATE_OUT_OF_MEMORY    # 内存不足
    
    def __str__(self) -> str:
        """返回结果的中文描述。"""
        return {
            self.OK: "成功",
            self.NO_SOLUTION: "无解",
            self.MULTIPLE_SOLUTIONS: "多解",
            self.SELECTOR_NEEDED: "需要选择器",
            self.PRECONDITION_FAILED: "前置条件不满足",
            self.OUT_OF_MEMORY: "内存不足"
        }.get(self, "未知结果")
    
    @property
    def is_success(self) -> bool:
        """判断实例化操作是否成功。"""
        return self == self.OK


# ============================================================
# 异常类
# ============================================================

class FuncBlockError(lvBaseError):
    """函数块错误基类。"""
    pass


class FuncBlockPackError(FuncBlockError):
    """打包错误。"""
    pass


class FuncBlockInstantiateError(FuncBlockError):
    """实例化错误。"""
    pass


class FuncBlockDeterminismError(FuncBlockError):
    """确定性错误。"""
    pass


# ============================================================
# 数据类
# ============================================================

@dataclass(frozen=True)
class PortInfo:
    """
    端口信息数据类。
    
    Attributes:
        id: 端口 ID
        name: 端口名称
        direction: 端口方向（"input" 或 "output"）
    """
    id: int
    name: str
    direction: str


@dataclass
class FuncBlockInfo:
    """
    函数块信息数据类。
    
    Attributes:
        id: 函数块 ID
        name: 函数块名称
        description: 函数块描述
        input_count: 输入端口数量
        output_count: 输出端口数量
        determinism: 确定性状态
    """
    id: int
    name: Optional[str]
    description: Optional[str]
    input_count: int
    output_count: int
    determinism: DeterminismState
    
    @property
    def needs_selector(self) -> bool:
        """检查是否需要选择器。"""
        return self.determinism == DeterminismState.NON_DETERMINISTIC


# ============================================================
# SolutionSelector 类
# ============================================================

class SolutionSelector:
    """
    多解选择器类。
    
    当函数块有多个解时，使用选择器决定返回哪个解。
    
    Attributes:
        type: 选择器类型
        reference_node_id: 参考节点 ID（用于某些选择器）
        custom_func: 自定义选择函数
    
    Examples:
        >>> selector = SolutionSelector(SelectorType.POSITIVE_ROOT)
        >>> selector = SolutionSelector.with_reference(SelectorType.IN_REGION, region_node_id)
    """
    
    def __init__(self, selector_type: Union[SelectorType, int]) -> None:
        """
        创建选择器。
        
        Args:
            selector_type: 选择器类型
            
        Raises:
            FuncBlockError: 无法创建底层 C 选择器时抛出
        """
        self._type = selector_type if isinstance(selector_type, int) else selector_type.value
        self._reference_node_id: int = -1
        self._custom_func: Optional[Callable[..., int]] = None
        
        # 立即创建底层 C 选择器
        self._ptr = _lib.selector_create(self._type)
        if not self._ptr:
            raise FuncBlockError(f"创建选择器失败，类型={self._type}")
    
    @classmethod
    def with_reference(cls, selector_type: Union[SelectorType, int], 
                       reference_node_id: int) -> 'SolutionSelector':
        """
        创建带参考节点的选择器。
        
        Args:
            selector_type: 选择器类型
            reference_node_id: 参考节点 ID
            
        Returns:
            SolutionSelector: 创建的选择器
        """
        selector = cls(selector_type)
        selector._reference_node_id = reference_node_id
        # 重新创建带参考的选择器
        old_ptr = selector._ptr
        selector._ptr = _lib.selector_create_with_reference(selector._type, reference_node_id)
        if old_ptr:
            _lib.selector_destroy(old_ptr)
        return selector
    
    @classmethod
    def custom(cls, func: Callable[..., int]) -> 'SolutionSelector':
        """
        创建自定义选择器。
        
        Args:
            func: 自定义选择函数，签名为 (candidates, count) -> selected_index
            
        Returns:
            SolutionSelector: 创建的选择器
        """
        selector = cls(SelectorType.CUSTOM)
        selector._custom_func = func
        return selector
    
    @property
    def type(self) -> int:
        """获取选择器类型。"""
        return self._type
    
    @property
    def reference_node_id(self) -> int:
        """获取参考节点 ID。"""
        return self._reference_node_id
    
    def apply(self, candidates: List[Any]) -> int:
        """
        应用选择器从候选解中选取一个。
        
        Args:
            candidates: 候选解列表
            
        Returns:
            int: 选中的解的索引（从 0 开始）
            
        Raises:
            FuncBlockError: 选择器未初始化或选择操作失败
            ValueError: 候选解列表无效
        """
        if not candidates:
            raise ValueError("候选解列表不能为空")
        if not self._ptr:
            raise FuncBlockError("选择器未初始化")
        
        # 将候选解转换为 C 指针数组
        arr = (ctypes.POINTER(_FuncBlock) * len(candidates))(*candidates)
        selected = ctypes.c_int()
        
        if not _lib.selector_apply(self._ptr, arr, len(candidates), ctypes.byref(selected)):
            raise FuncBlockError("应用选择器失败")
        
        return selected.value
    
    def __del__(self) -> None:
        """析构函数：释放底层 C 选择器资源。"""
        try:
            if hasattr(self, '_ptr') and self._ptr:
                _lib.selector_destroy(self._ptr)
        except Exception:
            pass


# ============================================================
# FuncBlock 类
# ============================================================

class FuncBlock:
    """
    函数块类。
    
    表示一个可打包、实例化的几何函数。
    函数块将内部节点和端口封装为可重用的模块，
    支持跨不同的约束图进行实例化。
    
    函数块是 Lv-00 元编程能力的核心——它允许将几何构造过程
    抽象为可调用的"函数"，实现几何知识的复用和组合。
    
    Attributes:
        id: 函数块 ID（由系统分配的唯一标识符）
        determinism: 确定性状态
        selector: 多解选择器
    """
    
    def __init__(self, fb_id: int = -1) -> None:
        """
        创建函数块对象。
        
        Args:
            fb_id: 函数块 ID（若为 -1，则创建新的空函数块）
        """
        self._id: int = fb_id
        self._ptr: Optional[Any] = None
        self._owns_ptr: bool = False
        self._selector: Optional[SolutionSelector] = None
        self._determinism: DeterminismState = DeterminismState.UNVERIFIED
        self._input_count: int = 0
        self._output_count: int = 0
    
    @classmethod
    def from_ptr(cls, ptr: Any, owns_ptr: bool = False) -> 'FuncBlock':
        """
        从底层指针创建函数块对象。
        
        Args:
            ptr: 底层 C 指针
            owns_ptr: 是否拥有指针的所有权
            
        Returns:
            FuncBlock: 创建的函数块对象
        """
        fb = cls()
        fb._ptr = ptr
        fb._owns_ptr = owns_ptr
        return fb
    
    def __del__(self) -> None:
        """析构函数：释放底层 C 函数块资源。"""
        try:
            if hasattr(self, '_ptr') and self._ptr and self._owns_ptr:
                _lib.func_block_destroy(self._ptr)
        except Exception:
            pass
    
    def __repr__(self) -> str:
        """返回函数块的调试表示。"""
        return f"FuncBlock(id={self._id}, determinism={self.determinism})"
    
    # ========== 属性访问 ==========
    
    @property
    def id(self) -> int:
        """获取函数块 ID。"""
        if self._ptr:
            return _lib.func_block_get_id(self._ptr)
        return self._id
    
    @property
    def name(self) -> Optional[str]:
        """获取函数块名称。"""
        if not self._ptr:
            return None
        name_ptr = _lib.func_block_get_name(self._ptr)
        return name_ptr.decode('utf-8') if name_ptr else None
    
    @name.setter
    def name(self, value: str) -> None:
        """设置函数块名称。"""
        if not self._ptr:
            raise FuncBlockError("函数块未初始化")
        b = value.encode('utf-8')
        if not _lib.func_block_set_name(self._ptr, b):
            raise FuncBlockError("设置函数块名称失败")
    
    @property
    def description(self) -> Optional[str]:
        """获取函数块描述。"""
        if not self._ptr:
            return None
        desc_ptr = _lib.func_block_get_description(self._ptr)
        return desc_ptr.decode('utf-8') if desc_ptr else None
    
    @description.setter
    def description(self, value: str) -> None:
        """设置函数块描述。"""
        if not self._ptr:
            raise FuncBlockError("函数块未初始化")
        b = value.encode('utf-8')
        if not _lib.func_block_set_description(self._ptr, b):
            raise FuncBlockError("设置函数块描述失败")
    
    @property
    def input_count(self) -> int:
        """获取输入端口数量。"""
        if not self._ptr:
            return 0
        return _lib.func_block_get_input_count(self._ptr)
    
    @property
    def output_count(self) -> int:
        """获取输出端口数量。"""
        if not self._ptr:
            return 0
        return _lib.func_block_get_output_count(self._ptr)
    
    @property
    def internal_count(self) -> int:
        """获取内部节点数量。"""
        if not self._ptr:
            return 0
        return _lib.func_block_get_internal_count(self._ptr)
    
    @property
    def determinism(self) -> DeterminismState:
        """获取确定性状态。"""
        if self._ptr:
            state = _lib.func_block_get_determinism(self._ptr)
            self._determinism = DeterminismState(state)
        return self._determinism
    
    @determinism.setter
    def determinism(self, value: DeterminismState) -> None:
        """设置确定性状态。"""
        self._determinism = value
    
    @property
    def selector(self) -> Optional[SolutionSelector]:
        """获取多解选择器。"""
        return self._selector
    
    @selector.setter
    def selector(self, value: Optional[SolutionSelector]) -> None:
        """设置多解选择器。"""
        self._selector = value
    
    # ========== 确定性检查 ==========
    
    def check_determinism_static(self, graph: Any) -> DeterminismState:
        """
        执行静态确定性检查。
        
        Args:
            graph: 约束图对象
            
        Returns:
            DeterminismState: 确定性状态
        """
        if not self._ptr:
            return DeterminismState.UNVERIFIED
        
        status = _lib.func_block_determinism_check_static(self._ptr, graph._ptr)
        self._determinism = DeterminismState(status)
        return self._determinism
    
    def check_determinism_dynamic(self, graph: Any, 
                                   input_values: List[Any]) -> DeterminismState:
        """
        执行动态确定性检查。
        
        Args:
            graph: 约束图对象
            input_values: 输入值列表
            
        Returns:
            DeterminismState: 确定性状态
            
        Raises:
            TypeError: 参数类型无效
            ValueError: 输入值列表无效
        """
        if not self._ptr:
            return DeterminismState.UNVERIFIED
        
        # 验证 graph 对象
        if graph is None or not hasattr(graph, '_ptr'):
            raise TypeError("graph 必须具有 _ptr 属性")
        
        # 验证输入值列表
        if not input_values:
            raise ValueError("输入值列表不能为空")
        
        # 验证并提取每个输入值的 C 指针
        validated_ptrs = []
        for i, v in enumerate(input_values):
            if v is None:
                raise ValueError(f"输入值 #{i} 不能为 None")
            if not hasattr(v, '_ptr'):
                raise TypeError(
                    f"输入值 #{i} 缺少 _ptr 属性，"
                    f"期望 SymbolicCoord 类型但收到了 {type(v).__name__}"
                )
            if v._ptr is None:
                raise ValueError(f"输入值 #{i} 的 _ptr 为 None")
            validated_ptrs.append(v._ptr)
        
        # 构造 C 指针数组并调用底层函数
        ptr_array_type = ctypes.POINTER(_SymbolicCoord) * len(validated_ptrs)
        values_ptr = ptr_array_type(*validated_ptrs)
        
        status = _lib.func_block_determinism_check_dynamic(
            self._ptr, graph._ptr, values_ptr, len(validated_ptrs)
        )
        self._determinism = DeterminismState(status)
        return self._determinism
    
    def verify(self, graph: Any, step_limit: int = 100) -> DeterminismState:
        """
        执行完整的确定性验证流程。
        
        Args:
            graph: 约束图
            step_limit: 静态分析的最大步数
            
        Returns:
            DeterminismState: 最终确定性状态
        """
        if not self._ptr:
            return DeterminismState.UNVERIFIED
        
        status = _lib.func_block_verify_determinism(self._ptr, graph._ptr, step_limit)
        self._determinism = DeterminismState(status)
        return self._determinism
    
    # ========== 实例化 ==========
    
    def instantiate(self, graph: Any, arg_mappings: List[int]) -> Tuple[InstantiateResult, List[int]]:
        """
        实例化函数块。
        
        Args:
            graph: 约束图对象
            arg_mappings: 实参映射列表
            
        Returns:
            Tuple[InstantiateResult, List[int]]: (结果, 新创建的节点 ID 列表)
            
        Raises:
            FuncBlockInstantiateError: 实例化失败
        """
        if not self._ptr:
            raise FuncBlockInstantiateError("函数块未初始化")
        
        mappings_arr = (ctypes.c_int * len(arg_mappings))(*arg_mappings)
        count = ctypes.c_int()
        
        result_ptr = _lib.func_block_instantiate(
            self._ptr, graph._ptr,
            mappings_arr, len(arg_mappings),
            ctypes.byref(count)
        )
        
        if not result_ptr:
            raise FuncBlockInstantiateError("实例化失败: 无法获取新节点列表")
        
        try:
            new_node_ids = [result_ptr[i] for i in range(count.value)]
        finally:
            _lib.lv_free_ptr(result_ptr)
        
        return (InstantiateResult.INSTANTIATE_OK, new_node_ids)
    
    def safe_instantiate(self, graph: Any, arg_mappings: List[int]) -> Tuple[bool, List[int], str]:
        """
        安全实例化（不抛出异常）。
        
        Args:
            graph: 约束图对象
            arg_mappings: 实参映射列表
            
        Returns:
            Tuple[bool, List[int], str]: (是否成功, 新节点列表, 错误消息)
        """
        try:
            _, nodes = self.instantiate(graph, arg_mappings)
            return (True, nodes, "")
        except Exception as e:
            return (False, [], str(e))
    
    # ========== 便捷方法 ==========
    
    def is_deterministic(self) -> bool:
        """检查函数块是否确定性。"""
        return self.determinism == DeterminismState.VERIFIED
    
    def needs_selector(self) -> bool:
        """检查函数块是否需要选择器。"""
        return self.determinism == DeterminismState.NON_DETERMINISTIC
    
    def get_info(self) -> FuncBlockInfo:
        """获取函数块信息摘要。"""
        return FuncBlockInfo(
            id=self.id,
            name=self.name,
            description=self.description,
            input_count=self.input_count,
            output_count=self.output_count,
            determinism=self.determinism
        )
    
    # ========== Setter 方法 ==========
    
    def set_internal_nodes(self, node_ids: List[int]) -> None:
        """设置函数块的内部节点。"""
        if not self._ptr:
            raise FuncBlockError("函数块未初始化")
        arr = (ctypes.c_int * len(node_ids))(*node_ids)
        if not _lib.func_block_set_internal_nodes(self._ptr, arr, len(node_ids)):
            raise FuncBlockError("设置内部节点失败")
    
    def set_input_ports(self, port_ids: List[int]) -> None:
        """设置函数块的输入端口。"""
        if not self._ptr:
            raise FuncBlockError("函数块未初始化")
        arr = (ctypes.c_int * len(port_ids))(*port_ids)
        if not _lib.func_block_set_input_ports(self._ptr, arr, len(port_ids)):
            raise FuncBlockError("设置输入端口失败")
        self._input_count = len(port_ids)
    
    def set_output_ports(self, port_ids: List[int]) -> None:
        """设置函数块的输出端口。"""
        if not self._ptr:
            raise FuncBlockError("函数块未初始化")
        arr = (ctypes.c_int * len(port_ids))(*port_ids)
        if not _lib.func_block_set_output_ports(self._ptr, arr, len(port_ids)):
            raise FuncBlockError("设置输出端口失败")
        self._output_count = len(port_ids)
    
    def set_preconditions(self, region_ids: List[int]) -> None:
        """设置函数块的前置条件。"""
        if not self._ptr:
            raise FuncBlockError("函数块未初始化")
        arr = (ctypes.c_int * len(region_ids))(*region_ids)
        if not _lib.func_block_set_preconditions(self._ptr, arr, len(region_ids)):
            raise FuncBlockError("设置前置条件失败")
    
    def copy(self) -> 'FuncBlock':
        """
        深拷贝函数块。
        
        Returns:
            FuncBlock: 新的函数块副本
        """
        if not self._ptr:
            raise FuncBlockError("函数块未初始化")
        new_ptr = _lib.func_block_copy(self._ptr)
        if not new_ptr:
            raise FuncBlockError("拷贝函数块失败")
        fb = FuncBlock.from_ptr(new_ptr, owns_ptr=True)
        fb._determinism = self._determinism
        fb._selector = self._selector
        return fb


# ============================================================
# 打包函数
# ============================================================

def func_block_pack(
    graph: Any,
    internal_node_ids: List[int],
    input_port_ids: List[int],
    output_port_ids: List[int],
    cross_boundary_actions: Optional[List[int]] = None
) -> Tuple[PackResult, FuncBlock]:
    """
    打包函数块。
    
    将约束图中的一组内部节点和端口打包成可重用的函数块。
    
    Args:
        graph: 约束图对象
        internal_node_ids: 内部节点 ID 列表
        input_port_ids: 输入端口 ID 列表
        output_port_ids: 输出端口 ID 列表
        cross_boundary_actions: 跨边界约束处理动作（可选）
        
    Returns:
        Tuple[PackResult, FuncBlock]: (结果码, 函数块对象)
        
    Raises:
        FuncBlockPackError: 打包失败
    """
    internal_arr = (ctypes.c_int * len(internal_node_ids))(*internal_node_ids)
    input_arr = (ctypes.c_int * len(input_port_ids))(*input_port_ids)
    output_arr = (ctypes.c_int * len(output_port_ids))(*output_port_ids)
    
    fb_ptr = ctypes.POINTER(_FuncBlock)()
    
    if cross_boundary_actions:
        actions_arr = (ctypes.c_int * len(cross_boundary_actions))(*cross_boundary_actions)
        result = _lib.func_block_pack(
            graph._ptr,
            internal_arr, len(internal_node_ids),
            input_arr, len(input_port_ids),
            output_arr, len(output_port_ids),
            actions_arr, len(cross_boundary_actions),
            ctypes.byref(fb_ptr)
        )
    else:
        result = _lib.func_block_pack(
            graph._ptr,
            internal_arr, len(internal_node_ids),
            input_arr, len(input_port_ids),
            output_arr, len(output_port_ids),
            None, 0,
            ctypes.byref(fb_ptr)
        )
    
    if result != PACK_OK:
        raise FuncBlockPackError(f"打包失败: {PackResult(result)}")
    
    fb = FuncBlock.from_ptr(fb_ptr, owns_ptr=True)
    return (PackResult(result), fb)


# ============================================================
# 导出列表
# ============================================================

__all__ = [
    # 版本信息
    '__version__',
    
    # 枚举
    'DeterminismState',
    'SelectorType',
    'PackResult',
    'InstantiateResult',
    
    # 数据类
    'PortInfo',
    'FuncBlockInfo',
    
    # 核心类
    'FuncBlock',
    'SolutionSelector',
    
    # 异常类
    'FuncBlockError',
    'FuncBlockPackError',
    'FuncBlockInstantiateError',
    'FuncBlockDeterminismError',
    
    # 函数
    'func_block_pack',
    
    # 常量
    'SELECTOR_POSITIVE_ROOT',
    'SELECTOR_NEGATIVE_ROOT',
    'SELECTOR_IN_REGION',
    'SELECTOR_NEAREST_TO_POINT',
    'SELECTOR_CUSTOM',
]
