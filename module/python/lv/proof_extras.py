"""
Lv-00 扩展证明模块

提供证明搜索树、多策略引擎、交互式证明和高级证明功能的 Python 接口。

功能：
    - 证明搜索树可视化（Newclid 风格回溯树）
    - 多策略证明引擎（JGEX 六种证明方法并存）
    - 交互式证明步骤管理
    - 证明断点保存与恢复
    - 命题等价声明与查询
    - 依赖链验证与更新
    - 矛盾定义管理
    - 引理块折叠
    - 命题实例化（多态）
    - 不可构造性证明
    - Agda/Idris2/Isabelle/F* 借鉴功能

版本：3.3.0
作者：Lv-00 开发团队
"""

import ctypes
from typing import Any, List, Optional, Tuple

from ._ctypes_binding import (
    _lib, _ConstraintGraph, _ProofNavigator, _Proposition,
    c_int, c_char_p, c_void_p, c_bool, POINTER,
)
from .core import lvBaseError
from ._ptr_owner import _PtrOwner, _int_arr, _str_enc

__all__ = [
    "BacktrackNodeType", "ProofStrategyType", "ProofStrategyStatus",
    "SledgehammerMode", "IsarStructureLevel", "LemmaViewState",
    "ProofSearchTree", "ProofMultiStrategy",
    "ProofExtrasError",
    "proof_search_tree_create", "backtrack_node_create",
    "proof_multi_strategy_create",
    "proof_interactive_step",
    "proof_save_breakpoint", "proof_restore_breakpoint",
    "proof_check_unconstructibility",
    "proof_sledgehammer_dispatch",
    "proof_minimal_verify",
]


# ============================================================
# 枚举常量
# ============================================================

class BacktrackNodeType:
    """回溯点类型枚举。

    常量:
        CHOICE_POINT (0): 选择点——多个策略分支
        FAILURE (1): 失败点——此路径不可行
        SUCCESS (2): 成功点——此路径到达目标
        PRUNE (3): 剪枝点——启发式跳过
    """
    CHOICE_POINT = 0
    FAILURE = 1
    SUCCESS = 2
    PRUNE = 3


class ProofStrategyType:
    """证明策略类型枚举（借鉴 JGEX 六种方法）。

    常量:
        DIRECT_CONSTRUCTION (0): 直接构造法
        AREA_METHOD (1): 面积法
        GROEBNER_BASIS (2): Groebner 基法
        VECTOR_METHOD (3): 向量法
        FULL_ANGLE_METHOD (4): 全角法
        DEDUCTIVE_DATABASE (5): 演绎数据库法
        COORDINATE (6): 坐标法
        ORACLE (7): Oracle 法
        COUNT (8): 策略总数
    """
    DIRECT_CONSTRUCTION = 0
    AREA_METHOD = 1
    GROEBNER_BASIS = 2
    VECTOR_METHOD = 3
    FULL_ANGLE_METHOD = 4
    DEDUCTIVE_DATABASE = 5
    COORDINATE = 6
    ORACLE = 7
    COUNT = 8


class ProofStrategyStatus:
    """证明策略状态枚举。

    常量:
        AVAILABLE (0): 可用
        UNAVAILABLE (1): 不可用
        ACTIVE (2): 当前激活
        COMPLETED (3): 已完成
        FAILED (4): 失败
    """
    AVAILABLE = 0
    UNAVAILABLE = 1
    ACTIVE = 2
    COMPLETED = 3
    FAILED = 4


class SledgehammerMode:
    """Sledgehammer 调用模式枚举。

    常量:
        SYNC (0): 同步调用
        ASYNC (1): 异步调用
        TIMEOUT (2): 超时控制
    """
    SYNC = 0
    ASYNC = 1
    TIMEOUT = 2


class IsarStructureLevel:
    """Isar 结构化证明层级枚举。

    常量:
        LEMMA (0): 引理级
        HAVE (1): have 声明级
        SHOW (2): show 目标级
        QED (3): qed 完成级
    """
    LEMMA = 0
    HAVE = 1
    SHOW = 2
    QED = 3


class LemmaViewState:
    """引理视图状态枚举。

    常量:
        EXPANDED (0): 展开
        COLLAPSED (1): 折叠
    """
    EXPANDED = 0
    COLLAPSED = 1


# ============================================================
# 异常类
# ============================================================

class ProofExtrasError(lvBaseError):
    """扩展证明错误基类。

    所有扩展证明相关异常的父类。
    继承 lvBaseError，复用统一的 message、error_code 属性和 __str__ 格式化逻辑。

    属性:
        message: 异常消息字符串
        error_code: 可选的错误码（整数，默认为 -1）
    """
    pass


# ============================================================
# 证明搜索树
# ============================================================

class ProofSearchTree(_PtrOwner):
    """证明搜索树（Newclid 风格）。

    展示证明搜索过程中尝试了哪些路径，标注回溯节点，
    支持在不同搜索策略之间切换观察效果。

    属性:
        _ptr: 底层 C ProofSearchTree 指针
    """

    def __init__(self) -> None:
        """创建证明搜索树。

        异常:
            ProofExtrasError: 创建失败
        """
        self._ptr = _lib.proof_search_tree_create()
        if not self._ptr:
            raise ProofExtrasError("创建证明搜索树失败")
        # 登记生命周期管理（析构由 _PtrOwner 统一处理）
        _PtrOwner.__init__(self, self._ptr, _lib.proof_search_tree_destroy, True)

    def add_child(self, parent: Any, child: Any) -> bool:
        """向搜索树添加子节点。

        参数:
            parent: 父节点指针（传 None 则设为根节点）
            child: 子节点指针

        返回:
            bool: 成功返回 True
        """
        p_ptr = parent._ptr if hasattr(parent, '_ptr') else parent
        c_ptr = child._ptr if hasattr(child, '_ptr') else child
        return _lib.proof_search_tree_add_child(self._ptr, p_ptr, c_ptr)

    def register_strategy(self, strategy_name: str) -> None:
        """注册可用策略。

        参数:
            strategy_name: 策略名称
        """
        _lib.proof_search_tree_register_strategy(self._ptr, _str_enc(strategy_name))

    def set_strategy(self, strategy_name: str) -> None:
        """设置当前策略。

        参数:
            strategy_name: 策略名称
        """
        _lib.proof_search_tree_set_strategy(self._ptr, _str_enc(strategy_name))

    def export_json(self, filepath: str) -> bool:
        """导出搜索树为 JSON（用于 Web GUI 可视化）。

        参数:
            filepath: 输出文件路径

        返回:
            bool: 成功返回 True
        """
        return _lib.proof_search_tree_export_json(self._ptr, _str_enc(filepath))

    def export_dot(self, filepath: str) -> bool:
        """导出搜索树为 DOT 格式（Graphviz）。

        参数:
            filepath: 输出文件路径

        返回:
            bool: 成功返回 True
        """
        return _lib.proof_search_tree_export_dot(self._ptr, _str_enc(filepath))


def backtrack_node_create(node_type: int, label: str) -> Any:
    """创建回溯节点。

    参数:
        node_type: 节点类型（BacktrackNodeType 枚举值）
        label: 节点标签

    返回:
        Any: 底层 C 节点指针

    .. warning::
        此函数返回裸 C 指针，调用者负责在适当时机调用对应的 _destroy 函数释放内存。
        建议使用 try/finally 确保资源释放。
    """
    return _lib.backtrack_node_create(node_type, _str_enc(label))


def proof_search_tree_create() -> ProofSearchTree:
    """创建证明搜索树（便捷函数）。

    返回:
        ProofSearchTree: 新创建的搜索树
    """
    return ProofSearchTree()


# ============================================================
# 多策略证明引擎
# ============================================================

class ProofMultiStrategy(_PtrOwner):
    """多策略证明引擎（借鉴 JGEX 架构）。

    管理多种证明方法的注册、切换、组合执行。

    属性:
        _ptr: 底层 C ProofMultiStrategy 指针
    """

    def __init__(self, navigator: Any = None) -> None:
        """创建多策略证明引擎。

        参数:
            navigator: 共享的证明导航器（可为 None）

        异常:
            ProofExtrasError: 创建失败
        """
        nav_ptr = navigator._ptr if hasattr(navigator, '_ptr') else navigator
        self._ptr = _lib.proof_multi_strategy_create(nav_ptr)
        if not self._ptr:
            raise ProofExtrasError("创建多策略引擎失败")
        # 登记生命周期管理（析构由 _PtrOwner 统一处理）
        _PtrOwner.__init__(self, self._ptr, _lib.proof_multi_strategy_destroy, True)

    def activate(self, strategy_type: int) -> bool:
        """激活指定策略。

        参数:
            strategy_type: 要激活的策略类型（ProofStrategyType 枚举值）

        返回:
            bool: 成功返回 True
        """
        return _lib.proof_multi_strategy_activate(self._ptr, strategy_type)

    def get_active(self) -> Any:
        """获取当前激活的策略。

        返回:
            Any: 策略描述符指针（不可修改），无激活策略返回 None
        """
        return _lib.proof_multi_strategy_get_active(self._ptr)

    def evaluate_applicability(self, graph: Any, proposition: Any,
                                max_count: int = 8) -> List[int]:
        """评估所有可用策略的适用性。

        参数:
            graph: 目标构造图对象
            proposition: 目标命题对象
            max_count: 最多返回数量

        返回:
            List[int]: 适用的策略类型列表
        """
        g_ptr = graph._ptr if hasattr(graph, '_ptr') else graph
        p_ptr = proposition._ptr if hasattr(proposition, '_ptr') else proposition
        out_arr = (c_int * max_count)()
        count = _lib.proof_multi_strategy_evaluate_applicability(
            self._ptr, g_ptr, p_ptr, out_arr, max_count)
        return [out_arr[i] for i in range(count)] if count > 0 else []

    def execute(self) -> bool:
        """使用当前策略执行证明。

        返回:
            bool: 成功返回 True
        """
        return _lib.proof_multi_strategy_execute(self._ptr)

    def try_all(self) -> int:
        """尝试所有可用策略（竞争模式）。

        按回退顺序依次尝试每个可用策略。

        返回:
            int: 成功的策略类型，失败返回 ProofStrategyType.COUNT
        """
        return _lib.proof_multi_strategy_try_all(self._ptr)

    def pipeline(self, pipeline_types: List[int]) -> bool:
        """使用多个策略组合证明（流水线模式）。

        参数:
            pipeline_types: 策略类型流水线（按顺序执行）

        返回:
            bool: 全部成功返回 True
        """
        arr = _int_arr(pipeline_types)
        return _lib.proof_multi_strategy_pipeline(self._ptr, arr, len(pipeline_types))

    def set_fallback_order(self, fallback_order: List[int]) -> None:
        """设置回退顺序。

        参数:
            fallback_order: 策略索引数组（按优先级排序）
        """
        arr = _int_arr(fallback_order)
        _lib.proof_multi_strategy_set_fallback_order(self._ptr, arr, len(fallback_order))

    def switch(self, strategy_type: int) -> bool:
        """切换策略（保存当前策略状态后切换）。

        参数:
            strategy_type: 目标策略类型

        返回:
            bool: 成功返回 True
        """
        return _lib.proof_multi_strategy_switch(self._ptr, strategy_type)

    def get_stats(self) -> Tuple[int, int]:
        """获取策略执行统计。

        返回:
            Tuple[int, int]: (总尝试次数, 成功次数)
        """
        total = c_int()
        success = c_int()
        _lib.proof_multi_strategy_get_stats(self._ptr, ctypes.byref(total), ctypes.byref(success))
        return (total.value, success.value)


def proof_multi_strategy_create(navigator: Any = None) -> ProofMultiStrategy:
    """创建多策略证明引擎（便捷函数）。

    参数:
        navigator: 共享的证明导航器

    返回:
        ProofMultiStrategy: 新创建的多策略引擎
    """
    return ProofMultiStrategy(navigator)


# ============================================================
# 交互式证明
# ============================================================

def proof_interactive_step(navigator: Any, step_type: int, step_data: Any = None) -> bool:
    """交互式证明步骤。

    允许用户引导证明构建。

    参数:
        navigator: 证明导航器对象
        step_type: 步骤类型（ProofStepType 枚举值）
        step_data: 步骤数据（类型取决于 step_type）

    返回:
        bool: 成功返回 True

    异常:
        TypeError: navigator 没有有效的 _ptr 属性
    """
    if not hasattr(navigator, '_ptr') or not navigator._ptr:
        raise TypeError("navigator 必须具有有效的 _ptr 属性")
    data_ptr = step_data._ptr if hasattr(step_data, '_ptr') else step_data
    return _lib.proof_interactive_step(navigator._ptr, step_type, data_ptr)


def proof_save_breakpoint(navigator: Any, breakpoint_id: int) -> bool:
    """保存证明断点。

    在指定断点 ID 处保存当前证明状态，以便后续继续。

    参数:
        navigator: 证明导航器对象
        breakpoint_id: 断点 ID

    返回:
        bool: 成功返回 True
    """
    if not hasattr(navigator, '_ptr') or not navigator._ptr:
        raise TypeError("navigator 必须具有有效的 _ptr 属性")
    return _lib.proof_save_breakpoint(navigator._ptr, breakpoint_id)


def proof_restore_breakpoint(navigator: Any, breakpoint_id: int) -> bool:
    """恢复证明断点。

    从指定断点 ID 处恢复之前保存的证明状态。

    参数:
        navigator: 证明导航器对象
        breakpoint_id: 断点 ID

    返回:
        bool: 成功返回 True
    """
    if not hasattr(navigator, '_ptr') or not navigator._ptr:
        raise TypeError("navigator 必须具有有效的 _ptr 属性")
    return _lib.proof_restore_breakpoint(navigator._ptr, breakpoint_id)


# ============================================================
# 不可构造性证明
# ============================================================

def proof_check_unconstructibility(navigator: Any, graph: Any,
                                    proposition: Any = None) -> int:
    """检查构造是否已知不可构造。

    在已加载的公理包中搜索匹配的不可构造性问题。

    参数:
        navigator: 证明导航器对象
        graph: 要检查的构造图对象
        proposition: 相关命题（可为 None）

    返回:
        int: UnconstructResult 枚举值
    """
    if not hasattr(navigator, '_ptr') or not navigator._ptr:
        raise TypeError("navigator 必须具有有效的 _ptr 属性")
    g_ptr = graph._ptr if hasattr(graph, '_ptr') else graph
    p_ptr = proposition._ptr if (proposition and hasattr(proposition, '_ptr')) else None
    return _lib.proof_check_unconstructibility(navigator._ptr, g_ptr, p_ptr, None)


# ============================================================
# Sledgehammer 自动证明
# ============================================================

def proof_sledgehammer_dispatch(mse: ProofMultiStrategy, mode: int = 0,
                                 timeout_ms: int = 0) -> Any:
    """Sledgehammer 风格自动证明调度。

    自动尝试多个证明策略，返回最优结果。

    参数:
        mse: 多策略引擎
        mode: 调度模式（SledgehammerMode 枚举值，默认 SYNC）
        timeout_ms: 超时毫秒（0 = 不限）

    返回:
        Any: 调度报告指针

    .. warning::
        此函数返回裸 C 指针，调用者负责在适当时机调用对应的 _destroy 函数释放内存。
        建议使用 try/finally 确保资源释放。
    """
    return _lib.proof_sledgehammer_dispatch(mse._ptr, mode, timeout_ms)


# ============================================================
# HOL Light 微内核验证
# ============================================================

def proof_minimal_verify(rule: int, premises: List[str],
                          conclusion: str) -> Tuple[int, Optional[str]]:
    """极简验证——仅用不超过 10 条基本规则验证一个证明步骤。

    参数:
        rule: 应用的推理规则（VerifyRuleType 枚举值）
        premises: 前提列表
        conclusion: 结论

    返回:
        Tuple[int, str]: (验证结果枚举值, 验证追溯字符串)
    """
    # 将前提列表转换为 C 字符串数组（NULL-terminated）
    c_premises = (c_char_p * (len(premises) + 1))()
    for i, p in enumerate(premises):
        c_premises[i] = _str_enc(p)
    c_premises[len(premises)] = None  # NULL terminator
    c_conclusion = _str_enc(conclusion)
    out_trace = c_char_p()
    result = _lib.proof_minimal_verify(
        rule, c_premises, c_conclusion, ctypes.byref(out_trace))
    trace_str = out_trace.value.decode('utf-8') if out_trace.value else None
    return (result, trace_str)
