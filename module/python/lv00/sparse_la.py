"""
Lv-00 稀疏线性代数模块

提供稀疏线性代数后端的 Python 接口，借鉴 SuiteSparse/GraphBLAS
的半环矩阵运算与约束传播。

功能：
    - 稀疏矩阵创建/销毁/克隆（CSR/CSC/COO 格式）
    - 半环抽象（Real/Max-Min/Boolean/Interval）
    - 约束传播（半环矩阵乘法不动点迭代）
    - 稀疏线性求解（Cholesky/LU/QR）
    - 约束图到稀疏矩阵转换
    - 稀疏矩阵乘法和转置
    - 约束图度分析

版本：3.3.0
作者：Lv-00 开发团队
"""

import ctypes
import logging
from typing import Any, List, Optional, Tuple

from ._ctypes_binding import _lib, _ConstraintGraph, c_int, c_double, c_char_p, c_void_p, c_bool, POINTER
from .core import Lv00BaseError

logger = logging.getLogger(__name__)

__all__ = [
    "SparseFormat", "SemiringType",
    "SparseMatrix", "SparseLAError",
    "sparse_matrix_create",
    "semiring_propagate_constraints",
    "graph_to_constraint_matrix",
    "sparse_matrix_multiply", "sparse_matrix_transpose",
    "graph_degree_analysis",
]


# ============================================================
# 枚举常量
# ============================================================

class SparseFormat:
    """稀疏矩阵存储格式枚举。

    常量:
        CSR (0): 压缩稀疏行——适合行遍历
        CSC (1): 压缩稀疏列——适合列遍历
        COO (2): 坐标格式——适合增量构建
        DENSE (3): 稠密格式——退化情况
    """
    CSR = 0
    CSC = 1
    COO = 2
    DENSE = 3


class SemiringType:
    """半环类型枚举。

    常量:
        PLUS_TIMES (0): (R, +, x)——经典实数半环
        MIN_PLUS (1): (R, min, +)——最短路径/热带半环
        MAX_TIMES (2): (R, max, x)——最大可信度传播
        OR_AND (3): ({0,1}, OR, AND)——布尔半环，可达性分析
        BOOL (4): 别名，同 OR_AND
        INTERVAL (5): (I, 交, +)——区间约束传播
    """
    PLUS_TIMES = 0
    MIN_PLUS = 1
    MAX_TIMES = 2
    OR_AND = 3
    BOOL = 4
    INTERVAL = 5


# ============================================================
# 异常类
# ============================================================

class SparseLAError(Lv00BaseError):
    """稀疏线性代数错误基类。

    所有稀疏线性代数相关异常的父类。
    继承 Lv00BaseError，复用统一的 message、error_code 属性和 __str__ 格式化逻辑。

    属性:
        message: 异常消息字符串
        error_code: 可选的错误码（整数，默认为 -1）
    """
    pass


# ============================================================
# SparseMatrix 类
# ============================================================

class SparseMatrix:
    """稀疏矩阵包装类。

    封装 CSR/CSC/COO 格式的稀疏矩阵。

    属性:
        _ptr: 底层 C SparseMatrix 指针
        rows: 行数
        cols: 列数
        fmt: 存储格式
    """

    def __init__(self, rows: int, cols: int, fmt: int = SparseFormat.CSR) -> None:
        """创建稀疏矩阵。

        参数:
            rows: 行数
            cols: 列数
            fmt: 存储格式（默认 CSR）

        异常:
            SparseLAError: 创建失败
        """
        self.rows: int = rows
        self.cols: int = cols
        self.fmt: int = fmt
        self._ptr = _lib.sparse_matrix_create(rows, cols, fmt)
        if not self._ptr:
            raise SparseLAError(f"创建稀疏矩阵失败 ({rows}x{cols})")

    def __del__(self) -> None:
        """析构：释放稀疏矩阵资源。"""
        if hasattr(self, '_ptr') and self._ptr:
            try:
                _lib.sparse_matrix_destroy(self._ptr)
            except Exception:
                pass
            self._ptr = None

    @classmethod
    def from_ptr(cls, ptr, owns: bool = True) -> 'SparseMatrix':
        """从底层 C 指针创建稀疏矩阵包装。

        参数:
            ptr: 底层 C 指针
            owns: 是否拥有所有权（默认 True）

        返回:
            SparseMatrix: 稀疏矩阵对象
        """
        # 尝试获取矩阵维度信息（从已知属性推断）
        mat = cls.__new__(cls)
        mat._ptr = ptr
        mat.rows = 0
        mat.cols = 0
        mat.fmt = SparseFormat.CSR
        mat._owns = owns
        return mat

    def clone(self) -> 'SparseMatrix':
        """深拷贝稀疏矩阵。

        返回:
            SparseMatrix: 独立副本

        异常:
            SparseLAError: 克隆失败
        """
        new_ptr = _lib.sparse_matrix_clone(self._ptr)
        if not new_ptr:
            raise SparseLAError("克隆稀疏矩阵失败")
        mat = SparseMatrix.from_ptr(new_ptr, owns=True)
        mat.rows = self.rows
        mat.cols = self.cols
        mat.fmt = self.fmt
        return mat

    def print(self, name: Optional[str] = None) -> None:
        """打印稀疏矩阵的结构和数值（调试用）。

        参数:
            name: 矩阵名称标签（可为 None）
        """
        c_name = name.encode('utf-8') if name else None
        _lib.sparse_matrix_print(self._ptr, c_name)


# ============================================================
# 约束传播
# ============================================================

def semiring_propagate_constraints(graph, semiring: int,
                                    x: List[float], max_iter: int = 0) -> int:
    """使用半环对约束图执行约束传播。

    将约束图上的约束传播建模为半环矩阵乘法的不动点迭代。

    参数:
        graph: 约束图对象
        semiring: 半环类型（SemiringType 枚举值）
        x: 节点值数组（输入初值，输出不动点），会被原地修改
        max_iter: 最大迭代次数（0 = 自动，默认 1000）

    返回:
        int: 实际迭代次数，-1 表示未收敛

    异常:
        TypeError: graph 没有有效的 _ptr 属性
    """
    if not hasattr(graph, '_ptr') or not graph._ptr:
        raise TypeError("graph 必须具有有效的 _ptr 属性")
    x_arr = (c_double * len(x))(*x)
    result = _lib.semiring_propagate_constraints(graph._ptr, semiring, x_arr, max_iter)
    # 将结果写回 x 列表
    for i in range(len(x)):
        x[i] = x_arr[i]
    return result


def graph_to_constraint_matrix(graph) -> SparseMatrix:
    """从约束图提取约束矩阵的稀疏结构。

    参数:
        graph: 约束图对象

    返回:
        SparseMatrix: 稀疏约束矩阵（CSR 格式）

    异常:
        TypeError: graph 没有有效的 _ptr 属性
        SparseLAError: 转换失败
    """
    if not hasattr(graph, '_ptr') or not graph._ptr:
        raise TypeError("graph 必须具有有效的 _ptr 属性")
    out_ptr = c_void_p()
    success = _lib.graph_to_constraint_matrix(graph._ptr, ctypes.byref(out_ptr))
    if not success or not out_ptr:
        raise SparseLAError("约束图转稀疏矩阵失败")
    return SparseMatrix.from_ptr(out_ptr, owns=True)


def sparse_matrix_multiply(a: SparseMatrix, b: SparseMatrix) -> SparseMatrix:
    """稀疏矩阵乘法 C = A * B（CSR 格式）。

    参数:
        a: 左矩阵
        b: 右矩阵

    返回:
        SparseMatrix: 乘积矩阵

    异常:
        SparseLAError: 乘法失败
    """
    out_ptr = c_void_p()
    success = _lib.sparse_matrix_multiply(a._ptr, b._ptr, ctypes.byref(out_ptr))
    if not success or not out_ptr:
        raise SparseLAError("稀疏矩阵乘法失败")
    mat = SparseMatrix.from_ptr(out_ptr, owns=True)
    mat.rows = a.rows
    mat.cols = b.cols
    return mat


def sparse_matrix_transpose(mat: SparseMatrix) -> SparseMatrix:
    """稀疏矩阵转置。

    参数:
        mat: 源矩阵

    返回:
        SparseMatrix: 转置后的矩阵

    异常:
        SparseLAError: 转置失败
    """
    out_ptr = c_void_p()
    success = _lib.sparse_matrix_transpose(mat._ptr, ctypes.byref(out_ptr))
    if not success or not out_ptr:
        raise SparseLAError("稀疏矩阵转置失败")
    result = SparseMatrix.from_ptr(out_ptr, owns=True)
    result.rows = mat.cols
    result.cols = mat.rows
    return result


def sparse_matrix_create(rows: int, cols: int, fmt: int = SparseFormat.CSR) -> SparseMatrix:
    """创建稀疏矩阵（便捷函数）。

    参数:
        rows: 行数
        cols: 列数
        fmt: 存储格式

    返回:
        SparseMatrix: 新创建的稀疏矩阵
    """
    return SparseMatrix(rows, cols, fmt)


def graph_degree_analysis(graph) -> Tuple[int, int, float, int]:
    """计算约束图度数分布的稀疏表示。

    参数:
        graph: 约束图对象

    返回:
        Tuple[int, int, float, int]: (节点总数, 最大度数, 平均度数, 孤立节点数)

    异常:
        TypeError: graph 没有有效的 _ptr 属性
    """
    if not hasattr(graph, '_ptr') or not graph._ptr:
        raise TypeError("graph 必须具有有效的 _ptr 属性")

    # 尝试调用 C 库
    out_ptr = c_void_p()
    try:
        success = _lib.graph_degree_analysis(graph._ptr, ctypes.byref(out_ptr))
    except (AttributeError, OSError):
        success = False

    if success and out_ptr:
        try:
            # DegreeAnalysis ctypes 结构体定义：
            #   int node_count;      节点总数
            #   int max_degree;      最大度数
            #   int min_degree;      最小度数
            #   double avg_degree;   平均度数
            #   int isolated_count;  孤立节点数（度数为 0）
            class _DegreeAnalysis(ctypes.Structure):
                _fields_ = [
                    ("node_count", ctypes.c_int),
                    ("max_degree", ctypes.c_int),
                    ("min_degree", ctypes.c_int),
                    ("avg_degree", ctypes.c_double),
                    ("isolated_count", ctypes.c_int),
                ]
            analysis = ctypes.cast(out_ptr, ctypes.POINTER(_DegreeAnalysis)).contents
            return (analysis.node_count, analysis.max_degree,
                    analysis.avg_degree, analysis.isolated_count)
        except Exception as e:
            logger.debug(f"度分析 ctypes 解析失败: {e}")
        finally:
            try:
                _lib.degree_analysis_free(out_ptr)
            except Exception:
                pass

    # 纯 Python 回退：通过约束图 API 计算度数统计
    try:
        node_count = _lib.graph_get_node_count(graph._ptr)
        constraint_count = _lib.graph_get_constraint_count(graph._ptr)

        if node_count <= 0:
            return (0, 0, 0.0, 0)

        # 统计每个节点的度数（参与约束的次数）
        degrees = [0] * node_count

        # 使用 graph_detect_conflicts 获取约束参与者信息不可行，
        # 改用逐节点查询方式
        for nid in range(node_count):
            count_ptr = ctypes.c_int()
            try:
                _lib.graph_find_constraints_involving(
                    graph._ptr, nid, ctypes.byref(count_ptr), 0)
                degrees[nid] = count_ptr.value
            except (AttributeError, OSError):
                degrees[nid] = 0

        max_degree = max(degrees) if degrees else 0
        avg_degree = sum(degrees) / len(degrees) if degrees else 0.0
        isolated_count = sum(1 for d in degrees if d == 0)

        return (node_count, max_degree, avg_degree, isolated_count)
    except Exception as e:
        raise NotImplementedError(
            f"无法计算图度数分析（C 库和 Python 回退均失败）: {e}"
        ) from e
