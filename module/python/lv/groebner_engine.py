"""
Lv-00 Groebner 引擎模块

提供 Groebner 基计算的 Python 接口，借鉴 Singular/Macaulay2
的多项式理想与 Gröbner 基计算范式。

功能：
    - 多项式环管理（创建、销毁、查找）
    - 多项式操作（创建、四则运算、代入）
    - 理想与 Gröbner 基（计算、增量更新、成员判定）
    - 理想交/商
    - 代数簇计算
    - 约束图到多项式理想的转换

版本：3.3.0
作者：Lv-00 开发团队
"""

import ctypes
from typing import List, Optional, Tuple

from ._ctypes_binding import _lib, _ConstraintGraph, c_int, c_char_p, c_void_p, c_bool, POINTER
from .core import lvBaseError
from ._ptr_owner import (
    _PtrOwner, _call_truthy, _str_enc,
)

__all__ = [
    "RingFieldType", "MonomialOrder", "GroebnerAlgorithm",
    "RingRegistry", "PolynomialRing", "GroebnerEngineError",
    "ring_registry_create", "ring_create", "ring_find",
    "poly_create", "poly_add", "poly_multiply",
    "ideal_create", "ideal_add_generator",
    "groebner_compute", "ideal_membership",
    "ideal_intersection", "ideal_quotient",
    "variety_compute", "constraint_graph_to_ideal",
]


# ============================================================
# 枚举常量
# ============================================================

class RingFieldType:
    """系数域类型枚举。

    常量:
        RATIONAL (0): 有理数域 Q
        REAL (1): 实数域 R
        COMPLEX (2): 复数域 C
        FINITE (3): 有限域 GF(p)
        INTEGER (4): 整数环 Z
    """
    RATIONAL = 0
    REAL = 1
    COMPLEX = 2
    FINITE = 3
    INTEGER = 4


class MonomialOrder:
    """单项式序类型枚举。

    常量:
        LEX (0): 纯字典序 (lexicographic)
        GRLEX (1): 分次字典序 (graded lex)
        GREVLEX (2): 分次反字典序 (graded reverse lex，默认推荐)
        ELIM (3): 消去序 (elimination order)
        WEIGHT (4): 权重序 (用户自定义权重向量)
    """
    LEX = 0
    GRLEX = 1
    GREVLEX = 2
    ELIM = 3
    WEIGHT = 4


class GroebnerAlgorithm:
    """Gröbner 基算法枚举。

    常量:
        BUCHBERGER (0): 经典 Buchberger 算法
        F4 (1): Faugere F4 算法（矩阵化，工业标准）
        F5 (2): Faugere F5 算法（签名基）
        SIGNATURE (3): 基于签名的 Gröbner 基（GVW 等）
        AUTO (4): 自动选择最优算法（默认）
    """
    BUCHBERGER = 0
    F4 = 1
    F5 = 2
    SIGNATURE = 3
    AUTO = 4


# ============================================================
# 异常类
# ============================================================

class GroebnerEngineError(lvBaseError):
    """Groebner 引擎错误基类。

    所有 Groebner 引擎相关异常的父类。
    当环操作、多项式运算或 Gröbner 基计算出错时抛出。
    继承 lvBaseError，复用统一的 message、error_code 属性和 __str__ 格式化逻辑。

    属性:
        message: 异常消息字符串
        error_code: 可选的错误码（整数，默认为 -1）
    """
    pass


# ============================================================
# 环注册表
# ============================================================

class RingRegistry(_PtrOwner):
    """多项式环注册表。

    管理多个多项式环的全局注册表，借鉴 Singular 的"多环共存"范式。
    每个几何对象知道自己属于哪个环，环注册表确保对象的环归属始终正确。

    属性:
        _ptr: 底层 C 环注册表指针
    """

    def __init__(self, capacity: int = 16) -> None:
        """创建环注册表。

        参数:
            capacity: 环容量（建议 >= 8）

        异常:
            GroebnerEngineError: 创建失败
        """
        self._ptr = _lib.ring_registry_create(capacity)
        if not self._ptr:
            raise GroebnerEngineError("创建环注册表失败")
        # 登记生命周期管理（析构由 _PtrOwner 统一处理）
        _PtrOwner.__init__(self, self._ptr, _lib.ring_registry_destroy, True)

    def create_ring(self, var_names: List[str], field: int,
                    order: int, label: Optional[str] = None) -> int:
        """创建一个多项式环。

        借鉴 Singular 的"先声明环"范式。环定义了变量集、系数域和单项式序。

        参数:
            var_names: 变量名字符串列表，如 ["x", "y", "z"]
            field: 系数域（RingFieldType 枚举值）
            order: 单项式序（MonomialOrder 枚举值）
            label: 环标签（可为 None）

        返回:
            int: 环 ID（>= 0）

        异常:
            GroebnerEngineError: 创建失败
        """
        c_names = (c_char_p * len(var_names))()
        for i, name in enumerate(var_names):
            c_names[i] = _str_enc(name)
        c_label = _str_enc(label) if label else None
        ring_id = _lib.ring_create(self._ptr, c_names, len(var_names), field, order, c_label)
        if ring_id < 0:
            raise GroebnerEngineError(f"创建环失败（变量: {var_names}）")
        return ring_id

    def destroy_ring(self, ring_id: int) -> None:
        """销毁一个多项式环及其所有关联对象。

        参数:
            ring_id: 环 ID
        """
        _lib.ring_destroy(self._ptr, ring_id)

    def find_ring(self, ring_id: int) -> 'PolynomialRing':
        """按 ID 查找环。

        参数:
            ring_id: 环 ID

        返回:
            PolynomialRing: 环包装对象

        异常:
            GroebnerEngineError: 环不存在
        """
        ptr = _call_truthy(_lib.ring_find, self._ptr, ring_id,
                           exc_cls=GroebnerEngineError, msg=f"环 ID={ring_id} 不存在")
        return PolynomialRing(ptr, ring_id)


class PolynomialRing:
    """多项式环包装类。

    表示一个多项式环，包含变量集、系数域和单项式序。

    属性:
        ring_id: 环的唯一标识符
        _ptr: 底层 C 环指针（只读）
    """

    def __init__(self, ptr, ring_id: int) -> None:
        self._ptr = ptr
        self.ring_id: int = ring_id


# ============================================================
# 便捷函数
# ============================================================

def ring_registry_create(capacity: int = 16) -> RingRegistry:
    """创建环注册表（便捷函数）。

    参数:
        capacity: 环容量

    返回:
        RingRegistry: 新创建的环注册表
    """
    return RingRegistry(capacity)


def ring_create(registry: RingRegistry, var_names: List[str],
                field: int = RingFieldType.RATIONAL,
                order: int = MonomialOrder.GREVLEX,
                label: Optional[str] = None) -> int:
    """在注册表中创建多项式环（便捷函数）。

    参数:
        registry: 环注册表
        var_names: 变量名字符串列表
        field: 系数域类型（默认有理数域）
        order: 单项式序（默认分次反字典序）
        label: 环标签（可选）

    返回:
        int: 环 ID
    """
    return registry.create_ring(var_names, field, order, label)


def ring_find(registry: RingRegistry, ring_id: int) -> PolynomialRing:
    """查找环（便捷函数）。

    参数:
        registry: 环注册表
        ring_id: 环 ID

    返回:
        PolynomialRing: 环对象
    """
    return registry.find_ring(ring_id)


# ============================================================
# 多项式操作
# ============================================================

def poly_create(registry: RingRegistry, ring_id: int,
                capacity: int = 16, label: Optional[str] = None) -> int:
    """创建多项式。

    在指定环中创建一个多项式，初始化为零多项式。

    参数:
        registry: 环注册表
        ring_id: 所属环 ID
        capacity: 项容量预分配（默认 16）
        label: 多项式标签（可选）

    返回:
        int: 多项式 ID（>= 0）

    异常:
        GroebnerEngineError: 创建失败
    """
    c_label = _str_enc(label) if label else None
    poly_id = _lib.poly_create(registry._ptr, ring_id, capacity, c_label)
    if poly_id < 0:
        raise GroebnerEngineError(f"创建多项式失败（环 ID={ring_id}）")
    return poly_id


def poly_add(registry: RingRegistry, poly_id_f: int, poly_id_g: int,
             label: Optional[str] = None) -> int:
    """多项式加法: h = f + g。

    参数:
        registry: 环注册表
        poly_id_f: 被加多项式 ID
        poly_id_g: 加多项式 ID
        label: 结果标签（可选）

    返回:
        int: 结果多项式 ID
    """
    c_label = _str_enc(label) if label else None
    result = _lib.poly_add(registry._ptr, poly_id_f, poly_id_g, c_label)
    if result < 0:
        raise GroebnerEngineError("多项式加法失败")
    return result


def poly_multiply(registry: RingRegistry, poly_id_f: int, poly_id_g: int,
                  label: Optional[str] = None) -> int:
    """多项式乘法: h = f * g。

    参数:
        registry: 环注册表
        poly_id_f: 被乘多项式 ID
        poly_id_g: 乘多项式 ID
        label: 结果标签（可选）

    返回:
        int: 结果多项式 ID
    """
    c_label = _str_enc(label) if label else None
    result = _lib.poly_multiply(registry._ptr, poly_id_f, poly_id_g, c_label)
    if result < 0:
        raise GroebnerEngineError("多项式乘法失败")
    return result


# ============================================================
# 理想与 Gröbner 基
# ============================================================

def ideal_create(registry: RingRegistry, ring_id: int,
                 label: Optional[str] = None) -> int:
    """创建理想。

    由一组生成元定义理想 I = <f_1, ..., f_k>。

    参数:
        registry: 环注册表
        ring_id: 所属环 ID
        label: 理想标签（可选）

    返回:
        int: 理想 ID（>= 0）
    """
    c_label = _str_enc(label) if label else None
    ideal_id = _lib.ideal_create(registry._ptr, ring_id, c_label)
    if ideal_id < 0:
        raise GroebnerEngineError(f"创建理想失败（环 ID={ring_id}）")
    return ideal_id


def ideal_add_generator(registry: RingRegistry, ideal_id: int, poly_id: int) -> int:
    """向理想添加生成元。

    参数:
        registry: 环注册表
        ideal_id: 理想 ID
        poly_id: 生成元多项式 ID

    返回:
        int: 0 表示成功
    """
    result = _lib.ideal_add_generator(registry._ptr, ideal_id, poly_id)
    if result < 0:
        raise GroebnerEngineError("添加理想生成元失败")
    return result


def groebner_compute(registry: RingRegistry, ideal_id: int,
                     algorithm: int = GroebnerAlgorithm.AUTO) -> int:
    """计算 Gröbner 基（核心函数）。

    为理想 I 计算 Gröbner 基，结果缓存在理想内部。

    参数:
        registry: 环注册表
        ideal_id: 理想 ID
        algorithm: 算法选择（默认 AUTO 自动选择）

    返回:
        int: 0 表示成功
    """
    result = _lib.groebner_compute(registry._ptr, ideal_id, algorithm)
    if result < 0:
        raise GroebnerEngineError(f"Gröbner 基计算失败（理想 ID={ideal_id}）")
    return result


def ideal_membership(registry: RingRegistry, ideal_id: int, poly_id: int) -> bool:
    """理想成员判定。

    检查多项式 f 是否属于理想 I。
    等价于检查 f 的 Gröbner 基约化余式是否为 0。

    参数:
        registry: 环注册表
        ideal_id: 理想 ID
        poly_id: 待判定多项式 ID

    返回:
        bool: 属于理想返回 True
    """
    return _lib.ideal_membership(registry._ptr, ideal_id, poly_id)


def ideal_intersection(registry: RingRegistry, ideal_id_a: int, ideal_id_b: int,
                       label: Optional[str] = None) -> int:
    """理想交: 计算 I ∩ J。

    参数:
        registry: 环注册表
        ideal_id_a: 理想 I 的 ID
        ideal_id_b: 理想 J 的 ID
        label: 结果理想标签（可选）

    返回:
        int: 结果理想 ID（>= 0）
    """
    c_label = _str_enc(label) if label else None
    result = _lib.ideal_intersection(registry._ptr, ideal_id_a, ideal_id_b, c_label)
    if result < 0:
        raise GroebnerEngineError("理想交计算失败")
    return result


def ideal_quotient(registry: RingRegistry, ideal_id_a: int, ideal_id_b: int,
                   label: Optional[str] = None) -> int:
    """理想商: 计算 I : J。

    参数:
        registry: 环注册表
        ideal_id_a: 理想 I 的 ID
        ideal_id_b: 理想 J 的 ID
        label: 结果理想标签（可选）

    返回:
        int: 结果理想 ID（>= 0）
    """
    c_label = _str_enc(label) if label else None
    result = _lib.ideal_quotient(registry._ptr, ideal_id_a, ideal_id_b, c_label)
    if result < 0:
        raise GroebnerEngineError("理想商计算失败")
    return result


# ============================================================
# 代数簇
# ============================================================

def variety_compute(registry: RingRegistry, ideal_id: int,
                    label: Optional[str] = None) -> int:
    """计算代数簇（求解多项式方程组）。

    给定理想 I，计算其代数簇 V(I)。

    参数:
        registry: 环注册表
        ideal_id: 理想 ID
        label: 簇标签（可选）

    返回:
        int: 簇 ID（>= 0）
    """
    c_label = _str_enc(label) if label else None
    variety_id = _lib.variety_compute(registry._ptr, ideal_id, c_label)
    if variety_id < 0:
        raise GroebnerEngineError(f"代数簇计算失败（理想 ID={ideal_id}）")
    return variety_id


# ============================================================
# 约束图转换
# ============================================================

def constraint_graph_to_ideal(registry: RingRegistry, graph, ring_id: int,
                               label: Optional[str] = None) -> int:
    """将约束图转换为多项式理想。

    将 Lv-00 的几何约束图编码为多项式理想。

    参数:
        registry: 环注册表
        graph: 约束图对象（Graph 实例）
        ring_id: 目标环 ID
        label: 结果理想标签（可选）

    返回:
        int: 理想 ID（>= 0）

    异常:
        TypeError: graph 没有有效的 C 指针
        GroebnerEngineError: 转换失败
    """
    if not hasattr(graph, '_ptr') or not graph._ptr:
        raise TypeError("graph 必须具有有效的 _ptr 属性")
    c_label = _str_enc(label) if label else None
    ideal_id = _lib.constraint_graph_to_ideal(registry._ptr, graph._ptr, ring_id, c_label)
    if ideal_id < 0:
        raise GroebnerEngineError("约束图转理想失败")
    return ideal_id
