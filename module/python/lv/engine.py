"""
Lv-00 引擎模块

提供 Lv-00 主引擎的高级 Python 接口，用于：
    - 工作流编排
    - 模块/公理加载
    - 函数打包与实例化
    - 重写与求解协作
    - 位电路跳闸处理
    - 冻结点快照回滚
    - 合一检查（构造与命题匹配）

设计原则：
    1. 资源安全：支持上下文管理器，确保资源正确释放
    2. 错误处理：完整的异常层次结构，清晰的错误消息
    3. 类型安全：完整的类型提示和参数验证
    4. 可扩展性：支持自定义模块和公理包加载

版本：3.3.0
作者：Lv-00 开发团队
"""

import ctypes
import os
from dataclasses import dataclass, field
from enum import IntEnum
from typing import Any, Callable, Dict, List, Optional, Tuple, Union

from .core import lvBaseError

from ._ptr_owner import _PtrOwner, _int_arr, _c_arr_to_list

from ._ctypes_binding import (
    _lib, _lvEngine,
    ENGINE_OK, ENGINE_OUT_OF_MEMORY, ENGINE_INVALID_STATE,
    ENGINE_INVALID_ARGUMENT, ENGINE_CONSTRAINT_CONFLICT, ENGINE_MODULE_ERROR,
    ENGINE_ERROR_INTERNAL,
    ENGINE_SOLVE_OK, ENGINE_SOLVE_CONFLICT, ENGINE_SOLVE_TIMEOUT, ENGINE_SOLVE_ERROR,
    LOG_LEVEL_DEBUG, LOG_LEVEL_INFO, LOG_LEVEL_WARN, LOG_LEVEL_ERROR,
    # 重写相关常量
    UNIFY_OK, UNIFY_FAILED, UNIFY_TYPE_MISMATCH
)


# ============================================================
# 异常类
# ============================================================

class EngineError(lvBaseError):
    """
    引擎操作错误基类。

    所有引擎相关异常的父类，当引擎操作失败时抛出。
    继承 lvBaseError，复用统一的 message、error_code 属性和 __str__ 格式化逻辑。

    属性：
        message: 异常消息字符串
        error_code: 可选的错误码（整数，默认为 -1）
    """
    pass


class EngineMemoryError(EngineError):
    """内存不足错误。

    当引擎操作因系统内存不足而失败时抛出。
    """
    pass


class EngineStateError(EngineError):
    """引擎状态错误。

    当引擎处于无效状态（如未初始化、已销毁等）时抛出。
    """
    pass


class EngineConflictError(EngineError):
    """约束冲突错误。

    当约束图中存在不可解决的约束冲突时抛出。
    """
    pass


class EngineModuleError(EngineError):
    """模块加载错误。

    当模块或公理包文件加载、解析失败时抛出。
    """
    pass


# ============================================================
# 枚举定义（附加导出，供新代码使用）
# ============================================================
# 以下 IntEnum 为原 optimized 版增量，数值与 _ctypes_binding 导出的
# 常量保持一致（ENGINE_* / UNIFY_* 常量由 _ctypes_binding 统一修正）。

class EngineStatus(IntEnum):
    """引擎状态码枚举。"""
    OK = ENGINE_OK
    OUT_OF_MEMORY = ENGINE_OUT_OF_MEMORY
    INVALID_STATE = ENGINE_INVALID_STATE
    INVALID_ARGUMENT = ENGINE_INVALID_ARGUMENT
    CONSTRAINT_CONFLICT = ENGINE_CONSTRAINT_CONFLICT
    MODULE_ERROR = ENGINE_MODULE_ERROR
    ERROR_INTERNAL = ENGINE_ERROR_INTERNAL


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
# 数据类定义（附加导出，供新代码使用）
# ============================================================

@dataclass(frozen=True)
class EngineConfig:
    """
    引擎配置数据类。

    属性：
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

    属性：
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

class Engine(_PtrOwner):
    """
    Lv-00 主引擎类。
    
    引擎是系统的核心，协调所有子系统的工作。
    提供统一的接口来管理约束图、模块、公理包和求解流程。
    
    示例：
        >>> engine = Engine()
        >>> engine.load_module("my_module.lv")
        >>> engine.load_axiom_package("geometry_axioms.lv")
        >>> result = engine.solve()
    """
    
    def __init__(self) -> None:
        """
        创建新的引擎实例。
        
        异常：
            EngineError: 创建失败
        """
        self._ptr = _lib.engine_create()
        if not self._ptr:
            raise EngineError("创建引擎失败")
        # 登记生命周期管理（析构由 _PtrOwner 统一处理；
        # 原实现以 logging.debug 静默记录，故保持 silent 策略）
        _PtrOwner.__init__(self, self._ptr, _lib.engine_destroy, True)
    
    def __enter__(self) -> 'Engine':
        """
        上下文管理器入口，支持 with 语句。

        使用 with 语句可以确保引擎资源在使用完毕后自动释放，
        无需手动调用 destroy_frozen_point 等清理方法。

        返回：
            Engine: 引擎自身

        示例：
            >>> with Engine() as engine:
            ...     engine.load_module("module.lv")
            ...     engine.solve()
            # 退出 with 块后自动释放引擎资源
        """
        return self

    def __exit__(self, exc_type: Any, exc_val: Any, exc_tb: Any) -> None:
        """
        上下文管理器出口，自动释放引擎资源。

        无论 with 块中是否发生异常，都会确保引擎资源被释放。

        参数：
            exc_type: 异常类型（无异常时为 None）
            exc_val: 异常值（无异常时为 None）
            exc_tb: 异常回溯（无异常时为 None）

        返回：
            None: 不抑制异常，让异常正常传播
        """
        self._release()
        # 不抑制异常，返回 None 让异常继续传播
        return None

    def _check_status(self, status: int) -> None:
        """
        检查引擎状态码，异常时抛出对应错误。
        
        参数：
            status: 状态码
        
        异常：
            EngineMemoryError: 内存不足
            EngineStateError: 状态错误
            EngineConflictError: 约束冲突
            EngineModuleError: 模块错误
        """
        if status == ENGINE_OK:
            return
        # 状态码 -> 异常映射表（引用 _ctypes_binding 常量，数值自动跟随修正）
        status_map = {
            ENGINE_OUT_OF_MEMORY: EngineMemoryError("内存不足"),
            ENGINE_INVALID_STATE: EngineStateError("引擎状态无效"),
            ENGINE_INVALID_ARGUMENT: EngineError("无效参数"),
            ENGINE_CONSTRAINT_CONFLICT: EngineConflictError("约束冲突"),
            ENGINE_MODULE_ERROR: EngineModuleError("模块错误"),
            ENGINE_ERROR_INTERNAL: EngineError("内部错误"),
        }
        error = status_map.get(status)
        if error:
            raise error
        raise EngineError(f"未知错误: {status}")
    
    # ============================================================
    # 求解
    # ============================================================
    
    def solve(self) -> int:
        """
        执行完整的求解流程。

        流程包括：重写 -> 求解 -> 冲突检查 -> 自由度更新。
        求解完成后，可通过 get_last_status() 获取详细状态码，
        通过 get_last_error() 获取错误消息。

        返回：
            int: 求解结果状态码，取值为：
                - ENGINE_SOLVE_OK (0): 求解成功
                - ENGINE_SOLVE_CONFLICT (1): 检测到约束冲突
                - ENGINE_SOLVE_TIMEOUT (2): 求解超时

        异常：
            EngineMemoryError: 引擎内存不足
            EngineStateError: 引擎状态无效（未初始化必要组件）
            EngineConflictError: 约束图存在不可解决的冲突
        """
        status = _lib.engine_solve(self._ptr)
        if status not in (ENGINE_SOLVE_OK, ENGINE_SOLVE_CONFLICT, ENGINE_SOLVE_TIMEOUT):
            self._check_status(status)
        return status
    
    def rewrite_and_solve(self, max_rewrite_steps: int = 1000, 
                          max_solve_steps: int = 1000) -> int:
        """
        执行重写-求解协作流程。

        实现协作协议：重写优先 -> 停滞时求解 -> 暴露冲突。
        这是 Lv-00 引擎的核心工作流，将重写规则的化简能力
        与代数求解的定量计算能力有机结合。

        参数：
            max_rewrite_steps: 最大重写步数（默认 1000）
            max_solve_steps: 最大求解步数（默认 1000）

        返回：
            int: 总步数（正数），或负数表示错误码

        异常：
            EngineMemoryError: 引擎内存不足
            EngineStateError: 引擎状态无效
            EngineConflictError: 约束冲突无法解决
        """
        return _lib.engine_rewrite_and_solve(self._ptr, max_rewrite_steps, max_solve_steps)
    
    def get_last_status(self) -> int:
        """
        获取最后的状态码。
        
        返回：
            int: 状态码
        """
        return _lib.engine_get_last_status(self._ptr)
    
    def get_last_error(self) -> str:
        """
        获取最后的错误消息。
        
        返回：
            str: 错误消息
        """
        msg = _lib.engine_get_last_error(self._ptr)
        if msg:
            return msg.decode('utf-8')
        return ""
    
    # ============================================================
    # 配置
    # ============================================================
    
    def set_rewrite_step_limit(self, limit: int) -> None:
        """
        设置重写步数限制。
        
        参数：
            limit: 最大步数（必须 > 0）
        """
        if limit <= 0:
            raise ValueError("步数限制必须大于0")
        _lib.engine_set_rewrite_step_limit(self._ptr, limit)
    
    def get_rewrite_step_limit(self) -> int:
        """
        获取当前的重写步数限制。
        
        返回：
            int: 当前限制（默认1000）
        """
        return _lib.engine_get_rewrite_step_limit(self._ptr)
    
    # ============================================================
    # 模块加载
    # ============================================================

    def _load_file(self, filepath: str, operation_name: str, c_func) -> bool:
        """
        加载文件的内部通用方法。

        编码文件路径为 UTF-8 字节串，调用对应的 C 函数加载，
        并在失败时抛出统一格式的 EngineModuleError。

        参数：
            filepath: 文件路径字符串
            operation_name: 操作名称（用于错误消息，如 "模块"、"公理包"）
            c_func: C 加载函数（如 _lib.engine_load_module）

        返回：
            bool: 加载成功返回 True

        异常：
            EngineModuleError: C 函数返回非 OK 状态时抛出
        """
        # 使用 os.fsencode() 处理文件路径编码（兼容非 UTF-8 路径）
        b = os.fsencode(filepath)
        status = c_func(self._ptr, b)
        if status != ENGINE_OK:
            raise EngineModuleError(f"加载{operation_name}失败: {filepath}")
        return True

    def load_module(self, filepath: str) -> bool:
        """
        加载模块文件。

        参数：
            filepath: 模块文件路径

        返回：
            bool: 是否成功

        异常：
            EngineModuleError: 加载失败
        """
        return self._load_file(filepath, "模块", _lib.engine_load_module)

    def load_axiom_package(self, filepath: str) -> bool:
        """
        加载公理包文件。

        参数：
            filepath: 公理包文件路径

        返回：
            bool: 是否成功

        异常：
            EngineModuleError: 加载失败
        """
        return self._load_file(filepath, "公理包", _lib.engine_load_axiom_package)
    
    # ============================================================
    # 函数块操作
    # ============================================================
    
    def pack_function(self, internal_node_ids: List[int],
                      input_port_ids: List[int],
                      output_port_ids: List[int]) -> int:
        """
        打包函数块。

        将一组内部节点和端口打包成一个可重用的函数块。
        打包后的函数块可在相同或不同的约束图中实例化。

        参数：
            internal_node_ids: 内部节点 ID 列表，打包进函数块的节点
            input_port_ids: 输入端口 ID 列表，函数块的参数入口
            output_port_ids: 输出端口 ID 列表，函数块的结果出口

        返回：
            int: 打包后的函数块 ID（正数），可用于后续的实例化操作

        异常：
            EngineError: 打包失败，可通过 get_last_error() 获取详细原因
        """
        internal_arr = _int_arr(internal_node_ids)
        input_arr = _int_arr(input_port_ids)
        output_arr = _int_arr(output_port_ids)
        
        fb_id = ctypes.c_int()
        success = _lib.engine_pack_function(
            self._ptr,
            internal_arr, len(internal_node_ids),
            input_arr, len(input_port_ids),
            output_arr, len(output_port_ids),
            ctypes.byref(fb_id)
        )
        
        if not success:
            raise EngineError(f"打包函数块失败: {self.get_last_error()}")
        
        return fb_id.value
    
    def instantiate_function(self, func_block_id: int, 
                              arg_mappings: List[int]) -> List[int]:
        """
        实例化函数块。

        使用实参替换形式参数，在引擎的约束图中创建函数块的新实例。
        实例化后的节点会被添加到引擎的约束图中。

        参数：
            func_block_id: 函数块 ID（由 pack_function 返回）
            arg_mappings: 实参映射列表，将函数块的输入端口 ID 
                          映射到约束图中的节点 ID

        返回：
            List[int]: 新创建的节点 ID 列表（实例化产生的几何节点）

        异常：
            EngineError: 实例化失败，可通过 get_last_error() 获取详细原因
        """
        mappings_arr = _int_arr(arg_mappings)
        count = ctypes.c_int()
        
        result_ptr = _lib.engine_instantiate_function(
            self._ptr, func_block_id,
            mappings_arr, len(arg_mappings),
            ctypes.byref(count)
        )
        
        if not result_ptr:
            raise EngineError(f"实例化函数块失败: {self.get_last_error()}")
        
        return _c_arr_to_list(result_ptr, count.value, _lib.lv_free_ptr)
    
    # ============================================================
    # 重写规则管理
    # ============================================================
    
    def add_rewrite_rule(self, rule: Union[Any, 'ctypes.c_void_p']) -> bool:
        """
        向引擎添加重写规则。

        重写规则用于几何图的等价变换，是 Lv-00 重写系统的核心。
        规则由模式（pattern）和替换（replacement）组成。

        参数：
            rule: RewriteRule 对象或底层 C 指针（c_void_p）

        返回：
            bool: 添加成功返回 True

        异常：
            EngineError: 添加规则失败
        """
        # 支持 Python RewriteRule 对象和底层 C 指针
        if hasattr(rule, '_ptr'):
            rule_ptr = rule._ptr
        else:
            rule_ptr = rule

        success = _lib.engine_add_rewrite_rule(self._ptr, rule_ptr)
        if not success:
            raise EngineError(f"添加重写规则失败: {self.get_last_error()}")
        return True

    # ============================================================
    # 流式输出管理
    # ============================================================

    def get_stream_context(self) -> Optional['ctypes.c_void_p']:
        """
        获取引擎的流式上下文。

        引擎创建时自动创建流式上下文，可通过此方法获取其指针，
        用于注册流式事件回调。

        返回：
            c_void_p: 流式上下文指针（不透明），引擎未初始化时返回 None
        """
        ctx = _lib.engine_get_stream_context(self._ptr)
        return ctx

    def set_streaming_enabled(self, enabled: bool) -> None:
        """
        设置流式输出开关。

        启用后，引擎在执行 solve/rewrite/normalize 等操作时会持续
        发射流式事件。默认启用。

        参数：
            enabled: True 启用流式输出，False 禁用
        """
        _lib.engine_set_streaming_enabled(self._ptr, enabled)

    def is_streaming_enabled(self) -> bool:
        """
        查询流式输出是否启用。

        返回：
            bool: 启用返回 True，禁用返回 False
        """
        return _lib.engine_is_streaming_enabled(self._ptr)

    def emit_stream_event(self, event_type: int, description: str,
                          step_number: int = 0, node_id: int = -1,
                          constraint_id: int = -1) -> None:
        """
        发射引擎流式事件。

        手动发射一个流式事件，事件将通过流式上下文推送给已注册的回调。
        如果流式上下文为 NULL，则为空操作。

        参数：
            event_type: 事件类型编码（StreamEventType）
            description: 事件描述文本
            step_number: 步骤编号（默认 0）
            node_id: 相关节点 ID（默认 -1 表示无）
            constraint_id: 相关约束 ID（默认 -1 表示无）
        """
        _lib.engine_emit_stream_event(
            self._ptr, event_type,
            _str_enc(description),
            step_number, node_id, constraint_id
        )

    # ============================================================
    # 合一检查 (Unification Check)
    # ============================================================
    # 合一检查是 Lv-00 证明系统的核心操作之一。将构造图与命题模式图进行匹配，
    # 判断构造是否实现了命题所描述的几何结构。
    # 引擎内置三层优化：哈希快速过滤 -> 拓扑特征匹配 -> 端口精化。

    def unify(self, construction: Any, proposition: Any) -> int:
        """
        执行合一检查（检查构造图是否满足命题模式）。

        C 层 engine_unify() 接受两个 ConstraintGraph* 参数，本方法
        从 Python 层的 Graph 对象提取底层 C 指针后传入。

        参数：
            construction: 构造图对象（Graph 实例），必须已初始化且包含有效的 _ptr
                （类型约定：Any，实际运行时支持任何具有 _ptr 属性的对象）
            proposition: 命题模式图对象（Graph 实例或具有 _ptr 的兼容对象）
                （类型约定：Any，实际运行时支持任何具有 _ptr 属性的对象）

        返回：
            int: 合一状态码，取值为：
                - UNIFY_OK (0): 构造满足命题模式，合一成功
                - UNIFY_FAILED (1): 构造不满足命题模式
                - UNIFY_TYPE_MISMATCH (2): 图类型不匹配，无法执行合一

        异常：
            TypeError: construction 或 proposition 没有有效的 _ptr 属性
            EngineError: 底层 C 合一引擎调用失败
        """
        # ---- 参数验证 ----
        if not hasattr(construction, '_ptr') or construction._ptr is None:
            raise TypeError(
                f"construction 必须具有有效的 _ptr 属性（C 约束图指针），"
                f"收到类型: {type(construction).__name__}"
            )
        if not hasattr(proposition, '_ptr') or proposition._ptr is None:
            raise TypeError(
                f"proposition 必须具有有效的 _ptr 属性（C 约束图指针），"
                f"收到类型: {type(proposition).__name__}"
            )

        # ---- 调用 C 层合一检查 ----
        # C 函数签名: proof_unify(ConstraintGraph*, Proposition*, bool) -> int
        # 注意：原 unify_check 函数已从 C 库移除，改用 proof_unify 实现
        # proof_unify 内部执行三层匹配：哈希过滤 -> 拓扑特征 -> 端口精化
        result = _lib.proof_unify(construction._ptr, proposition._ptr, True)
        return result

    def unify_detailed(self, construction: Any, proposition: Any) -> Tuple[int, str]:
        """
        执行详细的合一检查（带失败原因报告）。

        与 unify() 功能相同，但在合一失败时额外返回人类可读的失败原因。
        适用于调试和开发场景，帮助研究者理解为什么两个图不能合一。

        参数：
            construction: 构造图对象（Graph 实例）
            proposition: 命题模式图对象（Graph 实例）

        返回：
            Tuple[int, str]: (合一状态码, 详细失败原因描述)，
            成功时失败原因为空字符串，失败时包含 C 引擎的诊断信息

        异常：
            TypeError: 参数没有有效的 C 指针
        """
        # ---- 参数验证 ----
        if not hasattr(construction, '_ptr') or construction._ptr is None:
            raise TypeError("construction 必须具有有效的 C 指针")
        if not hasattr(proposition, '_ptr') or proposition._ptr is None:
            raise TypeError("proposition 必须具有有效的 C 指针")

        # ---- 调用基础合一检查，附带模拟的详细报告 ----
        # unify_detailed 和 proof_unify_detailed 均已从 C 库移除，
        # 改用 proof_unify 并在失败时提供基本的诊断信息
        result = _lib.proof_unify(construction._ptr, proposition._ptr, True)
        if result == UNIFY_OK:
            return (result, "")
        elif result == UNIFY_FAILED:
            return (result, "构造图不满足命题模式")
        elif result == UNIFY_TYPE_MISMATCH:
            return (result, "图类型不匹配，无法执行合一")
        else:
            return (result, f"未知合一错误 (code={result})")


    # ============================================================
    # 位电路处理
    # ============================================================
    
    def handle_circuit_trip(self) -> int:
        """
        处理位电路跳闸事件。

        当计算发生溢出时调用此方法，引擎将执行默认的跳闸恢复策略。
        位电路溢出是 Lv-00 安全模型的核心特性之一。

        返回：
            int: 处理结果状态码：
                - 0: 恢复成功
                - 1: 需要降级操作
                - 负数: 错误码

        异常：
            EngineStateError: 引擎状态无效，无法处理跳闸
        """
        return _lib.engine_handle_circuit_trip(self._ptr)
    
    def handle_circuit_trip_with_action(self, action: int) -> int:
        """
        使用指定动作处理位电路跳闸。

        参数：
            action: 处理动作：
                - 0: 忽略跳闸，继续计算
                - 1: 回滚到最近的冻结点
                - 2: 降级精度继续计算

        返回：
            int: 处理结果状态码，与 handle_circuit_trip() 相同
        """
        return _lib.engine_handle_circuit_trip_with_action(self._ptr, action)
    
    # ============================================================
    # 冻结点管理
    # ============================================================
    
    def create_frozen_point(self) -> Optional['ctypes.c_void_p']:
        """
        创建冻结点快照。

        保存当前引擎状态的完整快照，用于在位电路跳闸时回滚状态。
        这是 Lv-00 安全回滚机制的核心接口。

        返回：
            Any: 快照句柄（不透明指针），后续用于 restore_frozen_point()

        异常：
            EngineMemoryError: 内存不足，无法创建快照
            EngineStateError: 引擎状态无效
        """
        frozen = _lib.engine_create_frozen_point(self._ptr)
        if not frozen:
            raise EngineError("创建冻结点失败")
        return frozen
    
    def restore_frozen_point(self, frozen_point: 'ctypes.c_void_p') -> bool:
        """
        恢复到冻结点状态。

        将引擎状态回滚到 create_frozen_point() 创建快照时的状态。
        恢复后会丢弃快照之后的所有修改。

        参数：
            frozen_point: 冻结点句柄（由 create_frozen_point() 返回）

        返回：
            bool: 恢复成功返回 True

        异常：
            EngineStateError: 冻结点句柄无效或已释放
            EngineError: 恢复过程中发生其他错误
        """
        success = _lib.engine_restore_frozen_point(self._ptr, frozen_point)
        if not success:
            raise EngineError("恢复冻结点失败")
        return True
    
    def destroy_frozen_point(self, frozen_point: 'ctypes.c_void_p') -> None:
        """
        销毁冻结点快照。

        释放冻结点占用的内存资源。快照一旦销毁，无法再用于恢复。

        参数：
            frozen_point: 冻结点句柄（由 create_frozen_point() 返回）

        注意：
            调用此方法后，frozen_point 句柄将失效。
        """
        _lib.engine_destroy_frozen_point(frozen_point)
    
    # ============================================================
    # 批量操作
    # ============================================================
    
    def _load_from_directory(self, filepaths: List[str], loader: Callable[[str], Any]) -> int:
        """
        批量加载文件的通用方法。

        依次对每个文件路径调用指定的加载函数，返回成功加载的数量。
        如果某个文件加载失败，会抛出异常并停止后续加载。

        参数：
            filepaths: 文件路径列表
            loader: 加载函数，接受文件路径，执行加载操作

        返回：
            int: 成功加载的文件数量

        异常：
            EngineModuleError: 任一文件加载失败时抛出
        """
        success_count = 0
        for filepath in filepaths:
            loader(filepath)
            success_count += 1
        return success_count

    def load_modules(self, filepaths: List[str]) -> int:
        """
        批量加载模块文件。

        依次加载多个模块文件，返回成功加载的数量。
        如果某个模块加载失败，会抛出异常并停止后续加载。

        参数：
            filepaths: 模块文件路径列表

        返回：
            int: 成功加载的模块数量

        异常：
            EngineModuleError: 任一模块加载失败时抛出

        示例：
            >>> engine.load_modules(["module1.lv", "module2.lv"])
        """
        return self._load_from_directory(filepaths, self.load_module)
    
    def load_axiom_packages(self, filepaths: List[str]) -> int:
        """
        批量加载公理包文件。

        依次加载多个公理包文件，返回成功加载的数量。

        参数：
            filepaths: 公理包文件路径列表

        返回：
            int: 成功加载的公理包数量

        异常：
            EngineModuleError: 任一公理包加载失败时抛出
        """
        return self._load_from_directory(filepaths, self.load_axiom_package)
    
    # ============================================================
    # 状态查询
    # ============================================================
    
    def is_healthy(self) -> bool:
        """
        检查引擎是否处于健康状态。

        健康状态意味着引擎已正确初始化，可以执行求解操作。

        返回：
            bool: 引擎健康返回 True，否则返回 False
        """
        try:
            # 尝试获取状态，如果引擎指针无效会抛出异常
            status = self.get_last_status()
            return status >= 0
        except Exception:
            return False
    
    def get_statistics(self) -> Dict[str, Any]:
        """
        获取引擎运行统计信息。

        返回引擎的运行状态和性能指标。

        返回：
            Dict[str, Any]: 包含统计信息的字典，包括：
                - rewrite_step_limit: 重写步数限制
                - last_status: 最后的状态码
                - is_healthy: 健康状态
        """
        return {
            "rewrite_step_limit": self.get_rewrite_step_limit(),
            "last_status": self.get_last_status(),
            "is_healthy": self.is_healthy(),
        }
    
    # ============================================================
    # 便捷方法
    # ============================================================
    
    def solve_with_retry(self, max_retries: int = 3) -> int:
        """
        带重试机制的求解。

        当求解超时时自动重试，直到成功或达到最大重试次数。

        参数：
            max_retries: 最大重试次数（默认 3）

        返回：
            int: 最终的求解结果状态码

        异常：
            EngineError: 所有重试都失败时抛出
        """
        for attempt in range(max_retries):
            result = self.solve()
            if result == ENGINE_SOLVE_OK:
                return result
            if result == ENGINE_SOLVE_CONFLICT:
                # 冲突不需要重试
                return result
        
        raise EngineError(f"求解失败，已重试 {max_retries} 次")
    
    def safe_solve(self) -> Tuple[int, str]:
        """
        安全求解（不抛出异常）。

        执行求解操作，但不会抛出异常。适合需要静默处理的场景。

        返回：
            Tuple[int, str]: (状态码, 错误消息)
            成功时错误消息为空字符串

        示例：
            >>> status, msg = engine.safe_solve()
            >>> if status != 0:
            ...     print(f"求解失败: {msg}")
        """
        try:
            result = self.solve()
            return (result, "")
        except Exception as e:
            return (-1, str(e))


# ============================================================
# 常量导出
# ============================================================

__all__ = [
    'Engine',
    'EngineError',
    'EngineMemoryError', 
    'EngineStateError',
    'EngineConflictError',
    'EngineModuleError',
    # 附加导出：枚举与数据类（合并自 optimized 版）
    'EngineStatus',
    'SolveResult',
    'UnifyResult',
    'CircuitAction',
    'EngineConfig',
    'FunctionBlockSpec',
    'ENGINE_OK',
    'ENGINE_OUT_OF_MEMORY',
    'ENGINE_INVALID_STATE',
    'ENGINE_INVALID_ARGUMENT',
    'ENGINE_CONSTRAINT_CONFLICT',
    'ENGINE_MODULE_ERROR',
    'ENGINE_ERROR_INTERNAL',
    'ENGINE_SOLVE_OK',
    'ENGINE_SOLVE_CONFLICT',
    'ENGINE_SOLVE_TIMEOUT',
    'ENGINE_SOLVE_ERROR',
    'UNIFY_OK',
    'UNIFY_FAILED',
    'UNIFY_TYPE_MISMATCH',
]
