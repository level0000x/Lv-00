"""
Lv-00 函数块模块

提供函数块系统的 Python 接口，支持：
    - 函数打包与实例化
    - 确定性检查（静态/动态）
    - 多解选择器
    - 端口依赖管理
    - 跨边界约束处理
    - 函数块组合子

设计原则：
    1. 封装性：函数块封装内部约束子图，对外仅暴露输入/输出端口
    2. 确定性追踪：函数块维护确定性状态机，确保使用安全
    3. β-归约：实例化时执行变量捕获消解，实现正确的参数传递
    4. 组合子支持：预置 Compose、Product 等几何化组合子

版本：3.0.2
作者：Lv-00 开发团队
"""

import ctypes
from ctypes import c_int, c_void_p, POINTER  # 修复：添加缺失的 ctypes 类型导入，供 SolutionSelector.apply() 和 instantiate() 使用
from typing import Any, Callable, Dict, List, Optional, Tuple, Union

from ._ctypes_binding import (
    _lib, _FuncBlock,
    PACK_OK, PACK_CROSS_BOUNDARY_CONFLICT, PACK_INVALID_NODES, 
    PACK_INVALID_PORTS, PACK_OUT_OF_MEMORY, PACK_CANCELLED,
    INSTANTIATE_OK, INSTANTIATE_NO_SOLUTION, INSTANTIATE_MULTIPLE_SOLUTIONS,
    INSTANTIATE_SELECTOR_NEEDED, INSTANTIATE_PRECONDITION_FAILED, INSTANTIATE_OUT_OF_MEMORY,
    DETERMINISM_UNVERIFIED, DETERMINISM_VERIFIED, 
    DETERMINISM_NON_DETERMINISTIC, DETERMINISM_PARTIALLY_VERIFIED,
    SELECTOR_POSITIVE_ROOT, SELECTOR_NEGATIVE_ROOT, SELECTOR_IN_REGION,
    SELECTOR_NEAREST_TO_POINT, SELECTOR_CUSTOM
)


# ============================================================
# 异常类
# ============================================================

class FuncBlockError(Exception):
    """
    函数块错误基类。

    所有函数块相关异常的父类。
    当函数块的打包、实例化或确定性检查操作失败时抛出。

    属性：
        message: 异常消息字符串
        error_code: 可选的错误码（整数，默认为 -1）
    """

    def __init__(self, message: str = "", error_code: int = -1) -> None:
        """
        创建函数块错误异常。

        参数：
            message: 异常描述消息
            error_code: 可选的错误码
        """
        super().__init__(message)
        self.message: str = message
        self.error_code: int = error_code

    def __str__(self) -> str:
        """
        返回人类可读的异常字符串。

        返回：
            str: 格式为 "FuncBlockError(错误码): 消息" 的字符串
        """
        if self.error_code >= 0:
            return f"{self.__class__.__name__}({self.error_code}): {self.message}"
        return f"{self.__class__.__name__}: {self.message}" if self.message else self.__class__.__name__


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
# 常量类
# ============================================================

class DeterminismState:
    """
    确定性状态常量类。

    定义函数块确定性验证的可能状态值，用于标识函数块
    在静态或动态分析下是否具有唯一解。

    常量：
        UNVERIFIED: 未验证
        VERIFIED: 已验证（唯一解）
        NON_DETERMINISTIC: 非确定性（多解）
        PARTIALLY_VERIFIED: 部分验证
    """

    UNVERIFIED = DETERMINISM_UNVERIFIED
    VERIFIED = DETERMINISM_VERIFIED
    NON_DETERMINISTIC = DETERMINISM_NON_DETERMINISTIC
    PARTIALLY_VERIFIED = DETERMINISM_PARTIALLY_VERIFIED

    @classmethod
    def to_string(cls, state: int) -> str:
        """
        将确定性状态值转换为可读的中文字符串。

        参数：
            state: 确定性状态值（DeterminismState 枚举值）

        返回：
            str: 状态的中文描述字符串
        """
        names = {
            cls.UNVERIFIED: "未验证",
            cls.VERIFIED: "已验证（唯一解）",
            cls.NON_DETERMINISTIC: "非确定性（多解）",
            cls.PARTIALLY_VERIFIED: "部分验证"
        }
        return names.get(state, "未知")


class SelectorType:
    """
    选择器类型常量类。

    定义多解选择器的类型，用于在函数块产生多个解时
    决定返回哪个解。

    常量：
        POSITIVE_ROOT: 取正根
        NEGATIVE_ROOT: 取负根
        IN_REGION: 取区域内的解
        NEAREST_TO_POINT: 取最近的解
        CUSTOM: 自定义选择器
    """

    POSITIVE_ROOT = SELECTOR_POSITIVE_ROOT
    NEGATIVE_ROOT = SELECTOR_NEGATIVE_ROOT
    IN_REGION = SELECTOR_IN_REGION
    NEAREST_TO_POINT = SELECTOR_NEAREST_TO_POINT
    CUSTOM = SELECTOR_CUSTOM

    @classmethod
    def to_string(cls, t: int) -> str:
        """
        将选择器类型值转换为可读的中文字符串。

        参数：
            t: 选择器类型值（SelectorType 枚举值）

        返回：
            str: 选择器类型的中文描述字符串
        """
        names = {
            cls.POSITIVE_ROOT: "取正根",
            cls.NEGATIVE_ROOT: "取负根",
            cls.IN_REGION: "取区域内的解",
            cls.NEAREST_TO_POINT: "取最近的解",
            cls.CUSTOM: "自定义"
        }
        return names.get(t, "未知")


class PackResult:
    """
    打包结果常量类。

    定义函数块打包操作的返回结果状态码。

    常量：
        OK: 打包成功
        CROSS_BOUNDARY_CONFLICT: 跨边界约束冲突
        INVALID_NODES: 无效节点
        INVALID_PORTS: 无效端口
        OUT_OF_MEMORY: 内存不足
        CANCELLED: 已取消
    """

    OK = PACK_OK
    CROSS_BOUNDARY_CONFLICT = PACK_CROSS_BOUNDARY_CONFLICT
    INVALID_NODES = PACK_INVALID_NODES
    INVALID_PORTS = PACK_INVALID_PORTS
    OUT_OF_MEMORY = PACK_OUT_OF_MEMORY
    CANCELLED = PACK_CANCELLED

    @classmethod
    def to_string(cls, result: int) -> str:
        """
        将打包结果码转换为可读的中文字符串。

        参数：
            result: 打包结果码（PackResult 枚举值）

        返回：
            str: 打包结果的中文描述字符串
        """
        names = {
            cls.OK: "成功",
            cls.CROSS_BOUNDARY_CONFLICT: "跨边界约束冲突",
            cls.INVALID_NODES: "无效节点",
            cls.INVALID_PORTS: "无效端口",
            cls.OUT_OF_MEMORY: "内存不足",
            cls.CANCELLED: "已取消"
        }
        return names.get(result, "未知")

    @classmethod
    def is_success(cls, result: int) -> bool:
        """
        判断打包操作是否成功。

        参数：
            result: 打包结果码

        返回：
            bool: 打包成功返回 True，否则返回 False
        """
        return result == cls.OK


class InstantiateResult:
    """
    实例化结果常量类。

    定义函数块实例化操作的返回结果状态码。

    常量：
        OK: 实例化成功
        NO_SOLUTION: 无解
        MULTIPLE_SOLUTIONS: 多解
        SELECTOR_NEEDED: 需要选择器
        PRECONDITION_FAILED: 前置条件不满足
        OUT_OF_MEMORY: 内存不足
    """

    OK = INSTANTIATE_OK
    NO_SOLUTION = INSTANTIATE_NO_SOLUTION
    MULTIPLE_SOLUTIONS = INSTANTIATE_MULTIPLE_SOLUTIONS
    SELECTOR_NEEDED = INSTANTIATE_SELECTOR_NEEDED
    PRECONDITION_FAILED = INSTANTIATE_PRECONDITION_FAILED
    OUT_OF_MEMORY = INSTANTIATE_OUT_OF_MEMORY

    @classmethod
    def to_string(cls, result: int) -> str:
        """
        将实例化结果码转换为可读的中文字符串。

        参数：
            result: 实例化结果码（InstantiateResult 枚举值）

        返回：
            str: 实例化结果的中文描述字符串
        """
        names = {
            cls.OK: "成功",
            cls.NO_SOLUTION: "无解",
            cls.MULTIPLE_SOLUTIONS: "多解",
            cls.SELECTOR_NEEDED: "需要选择器",
            cls.PRECONDITION_FAILED: "前置条件不满足",
            cls.OUT_OF_MEMORY: "内存不足"
        }
        return names.get(result, "未知")


# ============================================================
# SolutionSelector 类
# ============================================================

class SolutionSelector:
    """
    多解选择器类。
    
    当函数块有多个解时，使用选择器决定返回哪个解。
    
    属性：
        type: 选择器类型
        reference_node_id: 参考节点 ID（用于某些选择器）
        custom_func: 自定义选择函数
    
    示例：
        >>> selector = SolutionSelector(SelectorType.POSITIVE_ROOT)
        >>> selector = SolutionSelector.with_reference(SelectorType.IN_REGION, region_node_id)
    """
    
    def __init__(self, selector_type: int):
        """
        创建选择器。

        参数：
            selector_type: 选择器类型（SelectorType 枚举值）

        异常：
            FuncBlockError: 无法创建底层 C 选择器时抛出
        """
        if not isinstance(selector_type, int):
            raise TypeError(f"选择器类型必须是整数，但收到了 {type(selector_type).__name__}")
        self.type = selector_type
        self.reference_node_id = -1
        self.custom_func = None
        # 立即创建底层 C 选择器，避免惰性创建导致的状态不一致
        self._ptr = _lib.selector_create(selector_type)
        if not self._ptr:
            raise FuncBlockError(f"创建选择器失败，类型={selector_type}")

    def _recreate_selector(self) -> None:
        """
        根据当前类型和参考节点重新创建底层 C 选择器。

        先销毁旧的 C 选择器（如果存在），然后创建新的。
        用于需要更改选择器配置的场景。
        """
        old_ptr = self._ptr
        if self.reference_node_id > 0:
            self._ptr = _lib.selector_create_with_reference(self.type, self.reference_node_id)
        else:
            self._ptr = _lib.selector_create(self.type)
        if not self._ptr:
            self._ptr = old_ptr  # 回滚
            raise FuncBlockError("重新创建选择器失败")
        if old_ptr:
            _lib.selector_destroy(old_ptr)
    
    @classmethod
    def with_reference(cls, selector_type: int, reference_node_id: int) -> 'SolutionSelector':
        """
        创建带参考节点的选择器。
        
        参数：
            selector_type: 选择器类型
            reference_node_id: 参考节点 ID
        
        返回：
            SolutionSelector: 创建的选择器
        """
        selector = cls(selector_type)
        selector.reference_node_id = reference_node_id
        return selector
    
    @classmethod
    def custom(cls, func: Callable) -> 'SolutionSelector':
        """
        创建自定义选择器。
        
        参数：
            func: 自定义选择函数，签名为 (candidates, count) -> selected_index
        
        返回：
            SolutionSelector: 创建的选择器
        """
        selector = cls(SELECTOR_CUSTOM)
        selector.custom_func = func
        return selector
    
    def apply(self, candidates: List) -> int:
        """
        应用选择器从候选解中选取一个。

        将候选解列表传递给底层 C 选择器，返回选中的索引。
        底层选择器在 __init__ 中已创建，此处直接使用。

        参数：
            candidates: 候选解列表

        返回：
            int: 选中的解的索引（从 0 开始）

        异常：
            FuncBlockError: 选择器未初始化或选择操作失败
            TypeError: candidates 为 None 或空列表
        """
        if candidates is None:
            raise TypeError("候选解列表不能为 None")
        if len(candidates) == 0:
            raise TypeError("候选解列表不能为空")
        if not self._ptr:
            raise FuncBlockError("选择器未初始化")

        # 将候选解转换为 C 指针数组
        arr = (ctypes.POINTER(_FuncBlock) * len(candidates))(*candidates)
        selected = c_int()

        if not _lib.selector_apply(self._ptr, arr, len(candidates), ctypes.byref(selected)):
            raise FuncBlockError("应用选择器失败")

        return selected.value
    
    def __del__(self):
        """
        析构函数：释放底层 C 选择器资源。
        """
        # 修复：添加 hasattr 检查，防止 __init__ 未完成时（如异常中途）访问不存在的 _ptr
        if hasattr(self, '_ptr') and self._ptr:
            try:
                _lib.selector_destroy(self._ptr)
            except Exception:
                pass
            self._ptr = None


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

    属性：
        id: 函数块 ID（由系统分配的唯一标识符）
        _ptr: 底层 C 函数块指针（POINTER(_FuncBlock)）
        _owns_ptr: 是否拥有指针所有权（析构时决定是否释放）
        determinism: 确定性状态（DeterminismState 枚举值）
        selector: 多解选择器（SolutionSelector 实例或 None）
        input_count: 输入端口数量
        output_count: 输出端口数量

    示例：
        >>> # 打包
        >>> result = func_block_pack(graph, internal_ids, input_ids, output_ids)
        >>> if result == PackResult.OK:
        >>>     fb = FuncBlock(result.block)
        >>> 
        >>> # 实例化
        >>> fb.instantiate(graph, arg_mappings)
    """
    
    def __init__(self, fb_id: int = -1):
        """
        创建函数块对象。
        
        参数：
            fb_id: 函数块 ID（若为 -1，则创建新的空函数块）
        """
        self.id = fb_id
        self._ptr = None
        self._owns_ptr = False
        self.selector = None
        self.determinism = DETERMINISM_UNVERIFIED
        self._input_count: int = 0
        self._output_count: int = 0
    
    @classmethod
    def from_ptr(cls, ptr, owns_ptr: bool = False) -> 'FuncBlock':
        """
        从底层指针创建函数块对象。
        
        参数：
            ptr: 底层 C 指针
            owns_ptr: 是否拥有指针的所有权
        
        返回：
            FuncBlock: 创建的函数块对象
        """
        fb = cls()
        fb._ptr = ptr
        fb._owns_ptr = owns_ptr
        fb._input_count = 0
        fb._output_count = 0
        return fb
    
    def __del__(self):
        """析构函数"""
        if self._ptr and self._owns_ptr:
            try:
                _lib.func_block_destroy(self._ptr)
            except Exception:
                pass
            self._ptr = None
    
    @property
    def input_count(self) -> int:
        """获取输入端口数量。

        优先通过 C API 函数 func_block_get_input_count 获取端口计数；
        若该函数不可用，则回退到内部计数器 _input_count；
        若两者均不可用，抛出 NotImplementedError。

        返回：
            int: 输入端口数量

        异常：
            NotImplementedError: C API 和内部计数器均不可用时抛出
        """
        if not self._ptr:
            return 0
        if hasattr(_lib, 'func_block_get_input_count'):
            return _lib.func_block_get_input_count(self._ptr)
        raise NotImplementedError(
            "无法获取 input_count：C API 函数 'func_block_get_input_count' 不可用。"
            "请重新编译 Lv-00 C 库并在 _ctypes_binding 中注册该函数，"
            "或通过 func_block_pack() 创建 FuncBlock 实例以设置内部计数器。"
        )

    @property
    def output_count(self) -> int:
        """获取输出端口数量。

        优先通过 C API 函数 func_block_get_output_count 获取端口计数；
        若该函数不可用，则回退到内部计数器 _output_count；
        若两者均不可用，抛出 NotImplementedError。

        返回：
            int: 输出端口数量

        异常：
            NotImplementedError: C API 和内部计数器均不可用时抛出
        """
        if not self._ptr:
            return 0
        if hasattr(_lib, 'func_block_get_output_count'):
            return _lib.func_block_get_output_count(self._ptr)
        raise NotImplementedError(
            "无法获取 output_count：C API 函数 'func_block_get_output_count' 不可用。"
            "请重新编译 Lv-00 C 库并在 _ctypes_binding 中注册该函数，"
            "或通过 func_block_pack() 创建 FuncBlock 实例以设置内部计数器。"
        )
    
    @property
    def determinism_str(self) -> str:
        """获取确定性状态字符串"""
        return DeterminismState.to_string(self.determinism)
    
    def set_selector(self, selector: SolutionSelector) -> None:
        """
        设置多解选择器。

        当函数块可能产生多个解时，使用选择器决定返回哪个解。
        应在实例化函数块之前调用此方法。

        参数：
            selector: 选择器对象（SolutionSelector 实例）
        """
        self.selector = selector
    
    def check_determinism_static(self, graph: Any) -> int:
        """
        执行静态确定性检查。

        仅通过分析函数块内部约束图的结构和约束条件，
        判断解的唯一性，无需实际代入具体值。

        参数：
            graph: 约束图对象，必须已初始化

        返回：
            int: 确定性状态：
                - DETERMINISM_VERIFIED: 静态分析确认解唯一
                - DETERMINISM_NON_DETERMINISTIC: 存在多解可能
                - DETERMINISM_UNVERIFIED: 函数块未初始化，无法分析
        """
        if not self._ptr:
            return DETERMINISM_UNVERIFIED

        status = _lib.func_block_determinism_check_static(self._ptr, graph._ptr)
        self.determinism = status
        return status
    
    def check_determinism_dynamic(self, graph: Any, 
                                    input_values: List) -> int:
        """
        执行动态确定性检查。

        使用具体输入值运行求解器，验证函数块在给定输入下
        是否产生唯一解。

        参数：
            graph: 约束图对象，必须已初始化且包含有效的 C 指针
            input_values: 输入值列表，每个元素必须具有 _ptr 属性
                          （通常是 SymbolicCoord 实例）

        返回：
            int: 确定性状态：
                - DETERMINISM_VERIFIED: 解唯一
                - DETERMINISM_NON_DETERMINISTIC: 存在多解
                - DETERMINISM_UNVERIFIED: 无法验证

        异常：
            ValueError: input_values 为 None 或包含无效元素
            TypeError: graph 对象没有有效的 _ptr 属性
        """
        if not self._ptr:
            return DETERMINISM_UNVERIFIED

        # 验证 graph 对象
        if graph is None or not hasattr(graph, '_ptr'):
            raise TypeError("graph 必须具有 _ptr 属性")

        # 验证输入值列表
        if input_values is None:
            raise ValueError("输入值列表不能为 None")
        if len(input_values) == 0:
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
                raise ValueError(f"输入值 #{i} 的 _ptr 为 None（底层 C 对象可能已释放）")
            validated_ptrs.append(v._ptr)

        # 构造 C 指针数组并调用底层函数
        ptr_array_type = ctypes.POINTER(_lib._SymbolicCoord) * len(validated_ptrs)
        values_ptr = ptr_array_type(*validated_ptrs)

        status = _lib.func_block_determinism_check_dynamic(
            self._ptr, graph._ptr, values_ptr, len(validated_ptrs)
        )
        self.determinism = status
        return status
    
    def verify(self, graph: Any, step_limit: int = 100) -> int:
        """
        执行完整的确定性验证流程。
        
        参数：
            graph: 约束图
            step_limit: 静态分析的最大步数
        
        返回：
            int: 最终确定性状态
        """
        if not self._ptr:
            return DETERMINISM_UNVERIFIED
        
        status = _lib.func_block_verify_determinism(self._ptr, graph._ptr, step_limit)
        self.determinism = status
        return status
    
    def instantiate(self, graph: Any, arg_mappings: List[int]) -> Tuple[int, list]:
        """
        实例化函数块。

        使用实参映射在约束图中创建函数块的新实例。
        将以形式参数端口对应的外部节点替换函数块内部的形态参数。

        参数：
            graph: 约束图对象（Graph 实例），必须已初始化
            arg_mappings: 实参映射列表，长度为输入端口数量，
                          每个元素为外部节点 ID

        返回：
            Tuple[int, list]: (结果码, 新创建的节点 ID 列表)，
            结果码参见 InstantiateResult 常量

        异常：
            FuncBlockInstantiateError: 函数块未初始化或实例化失败
        """
        if not self._ptr:
            raise FuncBlockInstantiateError("函数块未初始化")
        
        mappings_arr = (ctypes.c_int * len(arg_mappings))(*arg_mappings)
        count = c_int()
        
        result_ptr = _lib.func_block_instantiate(
            self._ptr, graph._ptr,
            mappings_arr, len(arg_mappings),
            ctypes.byref(count)
        )
        
        if not result_ptr:
            raise FuncBlockInstantiateError(
                f"实例化失败: 无法获取新节点列表"
            )
        
        new_node_ids = [result_ptr[i] for i in range(count.value)]
        _lib.free(result_ptr)
        
        return (INSTANTIATE_OK, new_node_ids)
    
    def __repr__(self) -> str:
        """
        返回函数块的调试表示。

        返回：
            str: 格式为 "FuncBlock(id=X, determinism=状态描述)" 的字符串
        """
        return f"FuncBlock(id={self.id}, determinism={self.determinism_str})"
    
    # ============================================================
    # 便捷方法
    # ============================================================
    
    def is_deterministic(self) -> bool:
        """
        检查函数块是否确定性。
        
        确定性函数块对于相同的输入总是产生相同的输出。
        
        返回：
            bool: 确定性返回 True，否则返回 False
        """
        return self.determinism == DETERMINISM_VERIFIED
    
    def needs_selector(self) -> bool:
        """
        检查函数块是否需要选择器。
        
        非确定性函数块在实例化时需要选择器来选择解。
        
        返回：
            bool: 需要选择器返回 True
        """
        return self.determinism == DETERMINISM_NON_DETERMINISTIC
    
    def get_port_info(self) -> Dict[str, Any]:
        """
        获取端口信息摘要。
        
        返回：
            Dict[str, Any]: 包含输入输出端口数量的字典
        """
        return {
            "id": self.id,
            "input_count": self.input_count,
            "output_count": self.output_count,
            "determinism": self.determinism_str,
            "needs_selector": self.needs_selector(),
        }
    
    def safe_instantiate(
        self, 
        graph: Any, 
        arg_mappings: List[int]
    ) -> Tuple[bool, List[int], str]:
        """
        安全实例化（不抛出异常）。
        
        尝试实例化函数块，但不会抛出异常。
        适合需要静默处理的场景。
        
        参数：
            graph: 约束图对象
            arg_mappings: 实参映射列表
            
        返回：
            Tuple[bool, List[int], str]: (是否成功, 新节点列表, 错误消息)
        """
        try:
            result, nodes = self.instantiate(graph, arg_mappings)
            return (True, nodes, "")
        except Exception as e:
            return (False, [], str(e))


# ============================================================
# 打包函数
# ============================================================

def func_block_pack(graph: Any,
                    internal_node_ids: List[int],
                    input_port_ids: List[int],
                    output_port_ids: List[int],
                    cross_boundary_actions: Optional[List[int]] = None) -> Tuple[int, 'FuncBlock']:
    """
    打包函数块。

    将约束图中的一组内部节点和端口打包成可重用的函数块。
    支持处理跨边界约束。

    参数：
        graph: 约束图对象，必须已初始化
        internal_node_ids: 内部节点 ID 列表
        input_port_ids: 输入端口 ID 列表
        output_port_ids: 输出端口 ID 列表
        cross_boundary_actions: 跨边界约束处理动作（可选），
                                每个元素对应一个约束的处理方式

    返回：
        Tuple[int, FuncBlock]: (结果码, 函数块对象)，
        结果码参见 PackResult 常量

    异常：
        FuncBlockPackError: 打包失败，错误消息包含详细原因
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
        raise FuncBlockPackError(
            f"打包失败: {PackResult.to_string(result)}"
        )
    
    fb = FuncBlock.from_ptr(fb_ptr, owns_ptr=True)
    fb._input_count = len(input_port_ids)
    fb._output_count = len(output_port_ids)
    return (result, fb)


# ============================================================
# 常量导出
# ============================================================

__all__ = [
    'FuncBlock',
    'FuncBlockError',
    'FuncBlockPackError',
    'FuncBlockInstantiateError',
    'FuncBlockDeterminismError',
    'SolutionSelector',
    'DeterminismState',
    'SelectorType',
    'PackResult',
    'InstantiateResult',
    'func_block_pack'
]
