"""
Lv-00 引擎模块 (优化版 v3.5.1)
==========================

模块功能概述：
    提供 Lv-00 主引擎的高级 Python 接口，是系统的核心协调层。
    引擎负责工作流编排、模块/公理加载、函数打包与实例化、重写与求解协作、
    位电路跳闸处理、冻结点快照回滚以及合一检查（构造与命题匹配）等操作。

主要类和异常：
    - Engine: 主引擎类，协调所有子系统的工作，支持上下文管理器
    - EngineError: 引擎操作错误基类
    - EngineMemoryError: 内存不足错误
    - EngineStateError: 引擎状态错误（未初始化、已销毁等）
    - EngineConflictError: 约束冲突错误
    - EngineModuleError: 模块/公理包加载错误

使用示例：
    >>> from lv.engine import Engine
    >>>
    >>> # 基本用法：创建引擎并加载模块
    >>> engine = Engine()
    >>> engine.load_module("my_module.lv")
    >>> engine.load_axiom_package("geometry_axioms.lv")
    >>> result = engine.solve()
    >>>
    >>> # 使用上下文管理器确保资源释放
    >>> with Engine() as engine:
    ...     engine.load_module("basic_geometry.lv")
    ...     result = engine.solve()
    ...     # 离开 with 块后自动释放引擎资源

版本：3.5.1 (优化版)
作者：Lv-00 开发团队
"""

from __future__ import annotations

import ctypes
import logging
import os
from dataclasses import dataclass, field
from enum import IntEnum
from typing import Any, Callable, Dict, Final, List, Optional, Protocol, Tuple, TypeVar, Union

from .core import lvBaseError

from ._ctypes_binding import (
    _lib, _lvEngine,
    ENGINE_OK, ENGINE_OUT_OF_MEMORY, ENGINE_INVALID_STATE,
    ENGINE_CONSTRAINT_CONFLICT, ENGINE_MODULE_ERROR,
    ENGINE_SOLVE_OK, ENGINE_SOLVE_CONFLICT, ENGINE_SOLVE_TIMEOUT, ENGINE_SOLVE_ERROR,
    LOG_LEVEL_DEBUG, LOG_LEVEL_INFO, LOG_LEVEL_WARN, LOG_LEVEL_ERROR,
    UNIFY_OK, UNIFY_FAILED, UNIFY_TYPE_MISMATCH
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

class EngineStatus(IntEnum):
    """引擎状态码枚举。"""
    OK = ENGINE_OK
    OUT_OF_MEMORY = ENGINE_OUT_OF_MEMORY
    INVALID_STATE = ENGINE_INVALID_STATE
    CONSTRAINT_CONFLICT = ENGINE_CONSTRAINT_CONFLICT
    MODULE_ERROR = ENGINE_MODULE_ERROR


class SolveResult(IntEnum):
    """求解结果枚举。"""
    OK = ENGINE_SOLVE_OK
    CONFLICT = ENGINE_SOLVE_CONFLICT
    TIMEOUT = ENGINE_SOLVE_TIMEOUT
    ERROR = ENGINE_SOLVE_ERROR


class UnifyResult(IntEnum):
    """合一检查结果枚举。"""
    OK = UNIFY_OK
    FAILED = UNIFY_FAILED
    TYPE_MISMATCH = UNIFY_TYPE_MISMATCH


class CircuitAction(IntEnum):
    """位电路跳闸处理动作枚举。"""
    IGNORE = 0
    ROLLBACK = 1
    DEGRADE = 2


# ============================================================
# 异常类
# ============================================================

class EngineError(lvBaseError):
    """引擎操作错误基类。"""
    pass


class EngineMemoryError(EngineError):
    """内存不足错误。"""
    pass


class EngineStateError(EngineError):
    """引擎状态错误。"""
    pass


class EngineConflictError(EngineError):
    """约束冲突错误。"""
    pass


class EngineModuleError(EngineError):
    """模块加载错误。"""
    pass


# ============================================================
# 数据类
# ============================================================

@dataclass(frozen=True)
class EngineConfig:
    """
    引擎配置数据类。
    
    Attributes:
        rewrite_step_limit: 最大重写步数
        solve_step_limit: 最大求解步数
        streaming_enabled: 是否启用流式输出
    """
    rewrite_step_limit: int = 1000
    solve_step_limit: int = 1000
    streaming_enabled: bool = True
    
    def __post_init__(self) -> None:
        """验证配置值。"""
        if self.rewrite_step_limit <= 0:
            raise ValueError("rewrite_step_limit 必须大于 0")
        if self.solve_step_limit <= 0:
            raise ValueError("solve_step_limit 必须大于 0")


@dataclass
class FunctionBlockSpec:
    """
    函数块规格数据类。
    
    Attributes:
        internal_nodes: 内部节点 ID 列表
        input_ports: 输入端口 ID 列表
        output_ports: 输出端口 ID 列表
    """
    internal_nodes: List[int] = field(default_factory=list)
    input_ports: List[int] = field(default_factory=list)
    output_ports: List[int] = field(default_factory=list)


# ============================================================
# 引擎类
# ============================================================

class Engine:
    """
    Lv-00 主引擎类。
    
    引擎是系统的核心，协调所有子系统的工作。
    提供统一的接口来管理约束图、模块、公理包和求解流程。
    
    Attributes:
        _ptr: 底层 C 引擎指针
        _owns_ptr: 是否拥有指针所有权
        _config: 引擎配置
    """
    
    # 默认配置
    _DEFAULT_CONFIG: Final[EngineConfig] = EngineConfig()
    
    def __init__(self, config: Optional[EngineConfig] = None) -> None:
        """
        创建新的引擎实例。
        
        Args:
            config: 引擎配置，使用默认配置如果为 None
            
        Raises:
            EngineError: 创建失败
        """
        self._config = config or self._DEFAULT_CONFIG
        self._ptr = _lib.engine_create()
        if not self._ptr:
            raise EngineError("创建引擎失败")
        self._owns_ptr = True
        
        # 应用配置
        self._apply_config()

        # 冻结点存储
        self._frozen_points: Dict[str, Dict[str, Any]] = {}
    
    def _apply_config(self) -> None:
        """应用配置到引擎。"""
        _lib.engine_set_rewrite_step_limit(self._ptr, self._config.rewrite_step_limit)
        _lib.engine_set_streaming_enabled(self._ptr, self._config.streaming_enabled)

    def _snapshot_state(self) -> Dict[str, Any]:
        """捕获引擎当前状态快照（用于冻结点）。"""
        return {
            'rewrite_step_limit': self.get_rewrite_step_limit(),
            'streaming_enabled': self.is_streaming_enabled(),
            'last_status': self.get_last_status().value,
            'last_error': self.get_last_error(),
        }
    
    def _cleanup(self) -> None:
        """释放引擎资源的内部方法。"""
        if hasattr(self, '_ptr') and self._ptr and getattr(self, '_owns_ptr', True):
            try:
                _lib.engine_destroy(self._ptr)
            except Exception as e:
                logger.debug("引擎清理时发生异常: %s", e)
            self._ptr = None
    
    def __del__(self) -> None:
        """析构函数：释放引擎资源。"""
        self._cleanup()
    
    def __enter__(self) -> 'Engine':
        """上下文管理器入口。"""
        return self
    
    def __exit__(self, exc_type: Any, exc_val: Any, exc_tb: Any) -> None:
        """上下文管理器出口。"""
        self._cleanup()
    
    def _check_status(self, status: int) -> None:
        """
        检查引擎状态码，异常时抛出对应错误。
        
        Args:
            status: 状态码
            
        Raises:
            EngineMemoryError: 内存不足
            EngineStateError: 状态错误
            EngineConflictError: 约束冲突
            EngineModuleError: 模块错误
        """
        status_map = {
            ENGINE_OUT_OF_MEMORY: EngineMemoryError("内存不足"),
            ENGINE_INVALID_STATE: EngineStateError("引擎状态无效"),
            ENGINE_CONSTRAINT_CONFLICT: EngineConflictError("约束冲突"),
            ENGINE_MODULE_ERROR: EngineModuleError("模块错误"),
        }
        
        if status == ENGINE_OK:
            return
        
        error = status_map.get(status)
        if error:
            raise error
        raise EngineError(f"未知错误: {status}")
    
    # ========== 求解 ==========
    
    def solve(self) -> SolveResult:
        """
        执行完整的求解流程。
        
        Returns:
            SolveResult: 求解结果
            
        Raises:
            EngineMemoryError: 引擎内存不足
            EngineStateError: 引擎状态无效
            EngineConflictError: 约束图存在不可解决的冲突
        """
        status = _lib.engine_solve(self._ptr)
        if status not in (ENGINE_SOLVE_OK, ENGINE_SOLVE_CONFLICT, ENGINE_SOLVE_TIMEOUT):
            self._check_status(status)
        return SolveResult(status)
    
    def rewrite_and_solve(self, max_rewrite_steps: Optional[int] = None,
                          max_solve_steps: Optional[int] = None) -> int:
        """
        执行重写-求解协作流程。
        
        Args:
            max_rewrite_steps: 最大重写步数，使用配置值如果为 None
            max_solve_steps: 最大求解步数，使用配置值如果为 None
            
        Returns:
            int: 总步数（正数），或负数表示错误码
            
        Raises:
            EngineMemoryError: 引擎内存不足
            EngineStateError: 引擎状态无效
            EngineConflictError: 约束冲突无法解决
        """
        rewrite_steps = max_rewrite_steps or self._config.rewrite_step_limit
        solve_steps = max_solve_steps or self._config.solve_step_limit
        return _lib.engine_rewrite_and_solve(self._ptr, rewrite_steps, solve_steps)
    
    def get_last_status(self) -> EngineStatus:
        """获取最后的状态码。"""
        return EngineStatus(_lib.engine_get_last_status(self._ptr))
    
    def get_last_error(self) -> str:
        """获取最后的错误消息。"""
        msg = _lib.engine_get_last_error(self._ptr)
        return msg.decode('utf-8') if msg else ""
    
    # ========== 配置 ==========
    
    def set_rewrite_step_limit(self, limit: int) -> None:
        """
        设置重写步数限制。
        
        Args:
            limit: 最大步数（必须 > 0）
            
        Raises:
            ValueError: 步数限制无效
        """
        if limit <= 0:
            raise ValueError("步数限制必须大于 0")
        _lib.engine_set_rewrite_step_limit(self._ptr, limit)
        # 更新配置
        object.__setattr__(self._config, 'rewrite_step_limit', limit)
    
    def get_rewrite_step_limit(self) -> int:
        """获取当前的重写步数限制。"""
        return _lib.engine_get_rewrite_step_limit(self._ptr)
    
    @property
    def config(self) -> EngineConfig:
        """获取当前配置。"""
        return self._config
    
    # ========== 模块加载 ==========
    
    def _load_file(self, filepath: str, operation_name: str, c_func: Callable) -> bool:
        """
        加载文件的内部通用方法。
        
        Args:
            filepath: 文件路径字符串
            operation_name: 操作名称（用于错误消息）
            c_func: C 加载函数
            
        Returns:
            bool: 加载成功返回 True
            
        Raises:
            EngineModuleError: 加载失败
        """
        # 使用 os.fsencode() 处理文件路径编码
        b = os.fsencode(filepath)
        status = c_func(self._ptr, b)
        if status != ENGINE_OK:
            raise EngineModuleError(f"加载{operation_name}失败: {filepath}")
        return True
    
    def load_module(self, filepath: str) -> bool:
        """
        加载模块文件。
        
        Args:
            filepath: 模块文件路径
            
        Returns:
            bool: 是否成功
            
        Raises:
            EngineModuleError: 加载失败
        """
        return self._load_file(filepath, "模块", _lib.engine_load_module)
    
    def load_axiom_package(self, filepath: str) -> bool:
        """
        加载公理包文件。
        
        Args:
            filepath: 公理包文件路径
            
        Returns:
            bool: 是否成功
            
        Raises:
            EngineModuleError: 加载失败
        """
        return self._load_file(filepath, "公理包", _lib.engine_load_axiom_package)
    
    # ========== 函数块操作 ==========
    
    def pack_function(self, spec: FunctionBlockSpec) -> int:
        """
        打包函数块。
        
        Args:
            spec: 函数块规格
            
        Returns:
            int: 打包后的函数块 ID
            
        Raises:
            EngineError: 打包失败
        """
        internal_arr = (ctypes.c_int * len(spec.internal_nodes))(*spec.internal_nodes)
        input_arr = (ctypes.c_int * len(spec.input_ports))(*spec.input_ports)
        output_arr = (ctypes.c_int * len(spec.output_ports))(*spec.output_ports)
        
        fb_id = ctypes.c_int()
        success = _lib.engine_pack_function(
            self._ptr,
            internal_arr, len(spec.internal_nodes),
            input_arr, len(spec.input_ports),
            output_arr, len(spec.output_ports),
            ctypes.byref(fb_id)
        )
        
        if not success:
            raise EngineError(f"打包函数块失败: {self.get_last_error()}")
        
        return fb_id.value
    
    def instantiate_function(self, func_block_id: int, 
                              arg_mappings: List[int]) -> List[int]:
        """
        实例化函数块。
        
        Args:
            func_block_id: 函数块 ID
            arg_mappings: 实参映射列表
            
        Returns:
            List[int]: 新创建的节点 ID 列表
            
        Raises:
            EngineError: 实例化失败
        """
        mappings_arr = (ctypes.c_int * len(arg_mappings))(*arg_mappings)
        count = ctypes.c_int()
        
        result_ptr = _lib.engine_instantiate_function(
            self._ptr, func_block_id,
            mappings_arr, len(arg_mappings),
            ctypes.byref(count)
        )
        
        if not result_ptr:
            raise EngineError(f"实例化函数块失败: {self.get_last_error()}")
        
        try:
            result = [result_ptr[i] for i in range(count.value)]
        finally:
            _lib.lv_free_ptr(result_ptr)
        
        return result
    
    # ========== 重写规则管理 ==========
    
    def add_rewrite_rule(self, rule: Any) -> bool:
        """
        向引擎添加重写规则。
        
        Args:
            rule: RewriteRule 对象或底层 C 指针
            
        Returns:
            bool: 添加成功返回 True
            
        Raises:
            EngineError: 添加规则失败
        """
        rule_ptr = rule._ptr if hasattr(rule, '_ptr') else rule
        
        success = _lib.engine_add_rewrite_rule(self._ptr, rule_ptr)
        if not success:
            raise EngineError(f"添加重写规则失败: {self.get_last_error()}")
        return True
    
    # ========== 流式输出管理 ==========
    
    def get_stream_context(self) -> Optional['ctypes.c_void_p']:
        """获取引擎的流式上下文。"""
        return _lib.engine_get_stream_context(self._ptr)
    
    def set_streaming_enabled(self, enabled: bool) -> None:
        """设置流式输出开关。"""
        _lib.engine_set_streaming_enabled(self._ptr, enabled)
        object.__setattr__(self._config, 'streaming_enabled', enabled)
    
    def is_streaming_enabled(self) -> bool:
        """查询流式输出是否启用。"""
        return _lib.engine_is_streaming_enabled(self._ptr)
    
    def emit_stream_event(self, event_type: int, description: str,
                          step_number: int = 0, node_id: int = -1,
                          constraint_id: int = -1) -> None:
        """发射引擎流式事件。"""
        _lib.engine_emit_stream_event(
            self._ptr, event_type,
            description.encode('utf-8'),
            step_number, node_id, constraint_id
        )
    
    # ========== 合一检查 ==========
    
    def unify(self, construction: Any, proposition: Any) -> UnifyResult:
        """
        执行合一检查（检查构造图是否满足命题模式）。
        
        Args:
            construction: 构造图对象（Graph 实例）
            proposition: 命题模式图对象（Graph 实例）
            
        Returns:
            UnifyResult: 合一结果
            
        Raises:
            TypeError: 参数没有有效的 _ptr 属性
        """
        self._validate_graph_pointer(construction, "construction")
        self._validate_graph_pointer(proposition, "proposition")
        
        result = _lib.proof_unify(construction._ptr, proposition._ptr, True)
        return UnifyResult(result)
    
    def unify_detailed(self, construction: Any, proposition: Any) -> Tuple[UnifyResult, str]:
        """
        执行详细的合一检查（带失败原因报告）。
        
        Args:
            construction: 构造图对象
            proposition: 命题模式图对象
            
        Returns:
            Tuple[UnifyResult, str]: (合一结果, 详细失败原因描述)
            
        Raises:
            TypeError: 参数没有有效的 C 指针
        """
        self._validate_graph_pointer(construction, "construction")
        self._validate_graph_pointer(proposition, "proposition")
        
        result = _lib.proof_unify(construction._ptr, proposition._ptr, True)
        
        reason_map = {
            UNIFY_OK: "",
            UNIFY_FAILED: "构造图不满足命题模式",
            UNIFY_TYPE_MISMATCH: "图类型不匹配，无法执行合一",
        }
        
        reason = reason_map.get(result, f"未知合一错误 (code={result})")
        return UnifyResult(result), reason
    
    def _validate_graph_pointer(self, obj: Any, name: str) -> None:
        """验证对象具有有效的 C 指针。"""
        if not hasattr(obj, '_ptr') or obj._ptr is None:
            raise TypeError(
                f"{name} 必须具有有效的 _ptr 属性（C 约束图指针），"
                f"收到类型: {type(obj).__name__}"
            )
    
    # ========== 位电路处理 ==========
    
    def handle_circuit_trip(self) -> int:
        """处理位电路跳闸事件。"""
        return _lib.engine_handle_circuit_trip(self._ptr)
    
    def handle_circuit_trip_with_action(self, action: CircuitAction) -> int:
        """
        使用指定动作处理位电路跳闸。
        
        Args:
            action: 处理动作
            
        Returns:
            int: 处理结果状态码
        """
        return _lib.engine_handle_circuit_trip_with_action(self._ptr, action.value)

    # ========== 冻结点管理 ==========

    def create_frozen_point(self, point_name: str) -> bool:
        """
        创建冻结点快照。

        捕获引擎当前状态并存储为命名冻结点，用于后续回滚。

        Args:
            point_name: 冻结点名称

        Returns:
            bool: 创建成功返回 True

        Raises:
            EngineStateError: 引擎指针无效
            ValueError: 冻结点名称已存在
        """
        if not self._ptr:
            raise EngineStateError("引擎未初始化，无法创建冻结点")
        if point_name in self._frozen_points:
            raise ValueError(f"冻结点 '{point_name}' 已存在，请先销毁或使用其他名称")

        self._frozen_points[point_name] = self._snapshot_state()
        logger.debug("冻结点 '%s' 已创建", point_name)
        return True

    def restore_frozen_point(self, point_name: str) -> bool:
        """
        恢复到指定冻结点。

        将引擎状态恢复到冻结点创建时的状态。

        Args:
            point_name: 冻结点名称

        Returns:
            bool: 恢复成功返回 True

        Raises:
            EngineStateError: 引擎指针无效
            KeyError: 冻结点不存在
        """
        if not self._ptr:
            raise EngineStateError("引擎未初始化，无法恢复冻结点")
        if point_name not in self._frozen_points:
            raise KeyError(f"冻结点 '{point_name}' 不存在")

        state = self._frozen_points[point_name]
        self.set_rewrite_step_limit(state['rewrite_step_limit'])
        self.set_streaming_enabled(state['streaming_enabled'])
        logger.debug("冻结点 '%s' 已恢复", point_name)
        return True

    def destroy_frozen_point(self, point_name: str) -> bool:
        """
        销毁指定冻结点。

        Args:
            point_name: 冻结点名称

        Returns:
            bool: 销毁成功返回 True

        Raises:
            KeyError: 冻结点不存在
        """
        if point_name not in self._frozen_points:
            raise KeyError(f"冻结点 '{point_name}' 不存在")

        del self._frozen_points[point_name]
        logger.debug("冻结点 '%s' 已销毁", point_name)
        return True

    # ========== 健康检查与统计 ==========

    def is_healthy(self) -> bool:
        """
        检查引擎是否健康可用。

        通过尝试获取引擎状态来验证底层 C 引擎是否正常工作。

        Returns:
            bool: 引擎健康返回 True，否则返回 False
        """
        try:
            if not self._ptr:
                return False
            # 尝试一个轻量级操作来验证引擎可用性
            _lib.engine_get_last_status(self._ptr)
            return True
        except Exception as e:
            logger.debug("引擎健康检查失败: %s", e)
            return False

    def get_statistics(self) -> Dict[str, Any]:
        """
        获取引擎统计信息。

        Returns:
            Dict[str, Any]: 包含引擎基本统计信息的字典
        """
        return {
            'status': self.get_last_status().name,
            'last_error': self.get_last_error(),
            'rewrite_step_limit': self.get_rewrite_step_limit(),
            'streaming_enabled': self.is_streaming_enabled(),
            'frozen_point_count': len(self._frozen_points),
            'frozen_point_names': list(self._frozen_points.keys()),
            'healthy': self.is_healthy(),
        }

    # ========== 重试求解 ==========

    def solve_with_retry(self, max_retries: int = 3,
                          backoff_factor: float = 1.0) -> SolveResult:
        """
        带重试的求解。

        当求解失败时自动重试，每次重试之间有指数退避间隔。

        Args:
            max_retries: 最大重试次数
            backoff_factor: 退避因子（秒）

        Returns:
            SolveResult: 最终求解结果

        Raises:
            ValueError: 重试次数无效
        """
        if max_retries < 1:
            raise ValueError("max_retries 必须大于等于 1")

        import time
        last_result = None
        for attempt in range(max_retries):
            result = self.solve()
            last_result = result
            if result == SolveResult.OK:
                return result
            if attempt < max_retries - 1:
                wait_time = backoff_factor * (2 ** attempt)
                logger.debug("求解失败 (尝试 %d/%d)，%.1f 秒后重试",
                             attempt + 1, max_retries, wait_time)
                time.sleep(wait_time)
        return last_result  # type: ignore[return-value]

    def safe_solve(self) -> Optional[SolveResult]:
        """
        安全求解（不抛出异常）。

        捕获所有异常并返回 None 表示失败。

        Returns:
            Optional[SolveResult]: 求解结果，失败返回 None
        """
        try:
            return self.solve()
        except Exception as e:
            logger.warning("安全求解失败: %s", e)
            return None


# ============================================================
# 导出列表
# ============================================================

__all__ = [
    # 版本信息
    '__version__',
    
    # 枚举
    'EngineStatus',
    'SolveResult',
    'UnifyResult',
    'CircuitAction',
    
    # 数据类
    'EngineConfig',
    'FunctionBlockSpec',
    
    # 引擎类
    'Engine',
    
    # 异常类
    'EngineError',
    'EngineMemoryError',
    'EngineStateError',
    'EngineConflictError',
    'EngineModuleError',
    
    # 常量
    'ENGINE_OK',
    'ENGINE_OUT_OF_MEMORY',
    'ENGINE_INVALID_STATE',
    'ENGINE_CONSTRAINT_CONFLICT',
    'ENGINE_MODULE_ERROR',
    'ENGINE_SOLVE_OK',
    'ENGINE_SOLVE_CONFLICT',
    'ENGINE_SOLVE_TIMEOUT',
    'ENGINE_SOLVE_ERROR',
    'UNIFY_OK',
    'UNIFY_FAILED',
    'UNIFY_TYPE_MISMATCH',
    'LOG_LEVEL_DEBUG',
    'LOG_LEVEL_INFO',
    'LOG_LEVEL_WARN',
    'LOG_LEVEL_ERROR',
]
