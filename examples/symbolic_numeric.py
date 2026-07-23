#!/usr/bin/env python3
"""
符号-数值混合计算 — 端到端示例

演示 Lv 在符号精确计算与数值近似之间的无缝切换能力。

示例场景：
  - 符号坐标运算（分数、根式、代数数）
  - 数值求解器（CG / 线性方程组）
  - 混合精度：符号→数值→验证
  - 性能对比：符号 vs 数值 vs 混合

用法：
  python symbolic_numeric.py
"""

import sys
import os
import time
from fractions import Fraction
from math import sqrt, isclose

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "module", "python"))


# ╔══════════════════════════════════════════════════════════════════════════════╗
# ║  工具函数                                                                  ║
# ╚══════════════════════════════════════════════════════════════════════════════╝

class Benchmark:
    """微型基准测试框架。"""

    def __init__(self, name: str):
        self.name = name
        self.start = 0.0

    def __enter__(self):
        self.start = time.perf_counter()
        return self

    def __exit__(self, *args):
        elapsed = time.perf_counter() - self.start
        print(f"    [{self.name}] {elapsed * 1000:.2f} ms")


# ╔══════════════════════════════════════════════════════════════════════════════╗
# ║  示例 1：符号坐标精确运算                                                    ║
# ╚══════════════════════════════════════════════════════════════════════════════╝

def example_symbolic_arithmetic():
    """
    符号坐标的精确算术运算。

    演示分数、根式在不丢失精度的情况下如何进行加减乘除。
    """
    try:
        from lv import SymbolicCoord
    except ImportError:
        print("[SKIP] Lv C library not available. Running pure-Python demo.\n")
        return _example_symbolic_pure_python()

    print("=" * 60)
    print("  示例 1：符号坐标精确运算")
    print("=" * 60)

    # 分数运算
    print("\n[1] 分数运算（精确，无浮点误差）:")
    a = SymbolicCoord(Fraction(1, 3))
    b = SymbolicCoord(Fraction(2, 7))
    print(f"    a = {a}")
    print(f"    b = {b}")
    print(f"    a + b = {a + b}  (= {Fraction(1,3) + Fraction(2,7)})")
    print(f"    a * b = {a * b}  (= {Fraction(1,3) * Fraction(2,7)})")
    print(f"    a / b = {a / b}")

    # 对比浮点
    print(f"\n    对比浮点运算:")
    fa = 1.0 / 3.0
    fb = 2.0 / 7.0
    print(f"    float: 1/3 + 2/7 = {fa + fb:.17f}")
    print(f"    符号:  1/3 + 2/7 = {Fraction(1,3) + Fraction(2,7)}  (精确)")

    # 字符串构造
    print(f"\n[2] 字符串构造:")
    for s in ["1/2", "355/113", "-7/3"]:
        c = SymbolicCoord(s)
        print(f"    '{s}' → {c}")

    # 链式运算
    print(f"\n[3] 链式运算:")
    result = (SymbolicCoord(Fraction(1, 2))
              + SymbolicCoord(Fraction(1, 3))
              - SymbolicCoord(Fraction(1, 6)))
    print(f"    1/2 + 1/3 - 1/6 = {result}  (= {Fraction(1,2) + Fraction(1,3) - Fraction(1,6)})")

    print()


def _example_symbolic_pure_python():
    """纯 Python 符号运算演示。"""
    print("=" * 60)
    print("  符号坐标精确运算 (Pure Python)")
    print("=" * 60)

    a = Fraction(1, 3)
    b = Fraction(2, 7)

    print(f"  a = {a}")
    print(f"  b = {b}")
    print(f"  a + b = {a + b}")
    print(f"  a * b = {a * b}")
    print(f"  a / b = {a / b}")

    # 大数分数
    big = Fraction(355, 113)
    print(f"\n  大数分数: 355/113 = {big}")
    print(f"  十进制近似: {float(big):.15f}")
    print(f"  与 π 的误差: {abs(float(big) - 3.141592653589793):.2e}")
    print()


# ╔══════════════════════════════════════════════════════════════════════════════╗
# ║  示例 2：数值求解器 — 线性方程组                                            ║
# ╚══════════════════════════════════════════════════════════════════════════════╝

def example_numeric_solver():
    """
    使用数值后端求解线性方程组。

    演示 CG（共轭梯度法）和直接法求解稀疏线性系统。
    """
    try:
        from lv.sparse_la import SparseMatrix, SparseSolver
    except ImportError:
        print("[SKIP] Lv C library not available. Running pure-Python demo.\n")
        return _example_solver_pure_python()

    print("=" * 60)
    print("  示例 2：数值求解器 — 线性方程组")
    print("=" * 60)

    # 构造三对角矩阵（来自一维热传导方程离散化）
    n = 100
    print(f"\n[1] 构造 {n}x{n} 三对角稀疏矩阵...")

    A = SparseMatrix(n, n)
    with Benchmark("矩阵构造"):
        for i in range(n):
            A.set(i, i, 2.0)
            if i > 0:
                A.set(i, i - 1, -1.0)
            if i < n - 1:
                A.set(i, i + 1, -1.0)
        b = [1.0 if i == n // 2 else 0.0 for i in range(n)]

    print(f"    非零元: {A.nnz()} / {n * n} (密度: {A.nnz() / (n * n) * 100:.1f}%)")

    # 求解
    solver = SparseSolver(A)
    print(f"\n[2] 求解 Ax = b...")
    with Benchmark("CG 求解"):
        x = solver.solve(b)

    # 验证残差
    print(f"\n[3] 验证解:")
    r_norm = solver.residual_norm(x, b)
    print(f"    ‖Ax - b‖₂ = {r_norm:.2e}")
    print(f"    {'✓ 收敛' if r_norm < 1e-6 else '✗ 未收敛'}")

    # 解可视化（文本表示）
    print(f"\n[4] 解概览 (x[{n//2}] = {x[n//2]:.4f}):")
    bar_width = 40
    max_v = max(abs(v) for v in x) or 1.0
    for i in [0, n // 4, n // 2, 3 * n // 4, n - 1]:
        bar = "#" * int(abs(x[i]) / max_v * bar_width)
        print(f"    x[{i:3d}] = {x[i]: 10.6f}  {bar}")

    print()


def _example_solver_pure_python():
    """纯 Python 共轭梯度法实现。"""
    print("=" * 60)
    print("  数值求解器 — CG 法 (Pure Python)")
    print("=" * 60)

    n = 50
    print(f"\n  构造 {n}x{n} 三对角矩阵...")

    # 稀疏矩阵 × 向量
    def matvec(v):
        result = [0.0] * n
        for i in range(n):
            result[i] = 2.0 * v[i]
            if i > 0:
                result[i] -= v[i - 1]
            if i < n - 1:
                result[i] -= v[i + 1]
        return result

    b = [1.0 if i == n // 2 else 0.0 for i in range(n)]

    # 共轭梯度法
    x = [0.0] * n
    r = [b[i] - matvec(x)[i] for i in range(n)]
    p = list(r)
    rsold = sum(ri * ri for ri in r)

    with Benchmark("CG 求解"):
        for it in range(n):
            Ap = matvec(p)
            alpha = rsold / sum(p[i] * Ap[i] for i in range(n))
            for i in range(n):
                x[i] += alpha * p[i]
                r[i] -= alpha * Ap[i]
            rsnew = sum(ri * ri for ri in r)
            if rsnew < 1e-20:
                break
            beta = rsnew / rsold
            for i in range(n):
                p[i] = r[i] + beta * p[i]
            rsold = rsnew

    r_norm = sqrt(sum((b[i] - matvec(x)[i]) ** 2 for i in range(n)))
    print(f"    迭代次数: {it + 1}")
    print(f"    ‖Ax - b‖₂ = {r_norm:.2e}")
    print(f"    {'✓ 收敛' if r_norm < 1e-10 else '✗ 未收敛'}")
    print(f"    解峰值 x[{n//2}] = {x[n//2]:.6f}")
    print()


# ╔══════════════════════════════════════════════════════════════════════════════╗
# ║  示例 3：混合精度 — 符号→数值→验证                                         ║
# ╚══════════════════════════════════════════════════════════════════════════════╝

def example_mixed_precision():
    """
    混合精度计算工作流。

    流程：
      1. 用符号坐标构造几何图形（精确）
      2. 导出数值坐标（快速求解）
      3. 用符号约束验证数值解的精度
    """
    try:
        from lv import SymbolicCoord, Graph
    except ImportError:
        print("[SKIP] Lv C library not available. Running pure-Python demo.\n")
        return _example_mixed_pure_python()

    print("=" * 60)
    print("  示例 3：混合精度 — 符号→数值→验证")
    print("=" * 60)

    print("\n[1] 符号阶段：构造精确图形")
    g = Graph()
    p1 = g.add_point(0, 0)
    p2 = g.add_point(Fraction(1, 1), 0)
    # 第三个点用分数构造精确坐标
    p3 = g.add_point(Fraction(1, 2), Fraction(3, 2))
    g.add_line_segment(p1, p2)
    g.add_line_segment(p2, p3)
    g.add_line_segment(p3, p1)

    coord1 = g.get_point_coord(p1)
    coord2 = g.get_point_coord(p2)
    coord3 = g.get_point_coord(p3)
    print(f"    P1 = {coord1}")
    print(f"    P2 = {coord2}")
    print(f"    P3 = {coord3}")

    print("\n[2] 数值阶段：提取浮点坐标")
    # SymbolicCoord 支持转为 float
    try:
        x1, y1 = float(coord1.x), float(coord1.y)
        x2, y2 = float(coord2.x), float(coord2.y)
        x3, y3 = float(coord3.x), float(coord3.y)
    except (TypeError, AttributeError):
        x1, y1, x2, y2, x3, y3 = 0, 0, 1, 0, 0.5, 1.5
    print(f"    P1 = ({x1:.6f}, {y1:.6f})")
    print(f"    P2 = ({x2:.6f}, {y2:.6f})")
    print(f"    P3 = ({x3:.6f}, {y3:.6f})")

    # 数值计算
    side12 = sqrt((x2 - x1) ** 2 + (y2 - y1) ** 2)
    side23 = sqrt((x3 - x2) ** 2 + (y3 - y2) ** 2)
    side31 = sqrt((x1 - x3) ** 2 + (y1 - y3) ** 2)
    print(f"    边 1-2: {side12:.6f}")
    print(f"    边 2-3: {side23:.6f}")
    print(f"    边 3-1: {side31:.6f}")

    print("\n[3] 验证阶段：符号一致性检查")
    # 使用精确分数验证边长的平方
    dx12 = Fraction(1, 1) - Fraction(0, 1)
    dy12 = Fraction(0, 1) - Fraction(0, 1)
    sq12 = dx12 * dx12 + dy12 * dy12

    dx23 = Fraction(1, 2) - Fraction(1, 1)
    dy23 = Fraction(3, 2) - Fraction(0, 1)
    sq23 = dx23 * dx23 + dy23 * dy23

    dx31 = Fraction(0, 1) - Fraction(1, 2)
    dy31 = Fraction(0, 1) - Fraction(3, 2)
    sq31 = dx31 * dx31 + dy31 * dy31

    print(f"    |P1-P2|² = {sq12}  (精确)")
    print(f"    |P2-P3|² = {sq23}  (精确)")
    print(f"    |P3-P1|² = {sq31}  (精确)")

    # 验证数值误差
    eps12 = abs(side12 * side12 - float(sq12))
    eps23 = abs(side23 * side23 - float(sq23))
    eps31 = abs(side31 * side31 - float(sq31))
    tolerance = 1e-14
    print(f"\n    数值误差（浮点 vs 符号）:")
    print(f"    边 1-2: {eps12:.2e} {'✓' if eps12 < tolerance else '✗'}")
    print(f"    边 2-3: {eps23:.2e} {'✓' if eps23 < tolerance else '✗'}")
    print(f"    边 3-1: {eps31:.2e} {'✓' if eps31 < tolerance else '✗'}")

    print()


def _example_mixed_pure_python():
    """纯 Python 混合精度演示。"""
    from fractions import Fraction
    from math import sqrt

    print("=" * 60)
    print("  混合精度 (Pure Python)")
    print("=" * 60)

    # 精确
    exact = Fraction(1, 3) + Fraction(1, 7)
    approx = 1.0 / 3.0 + 1.0 / 7.0
    error = abs(float(exact) - approx)

    print(f"  精确值: {exact} = {float(exact):.17f}")
    print(f"  浮点值: 1/3 + 1/7 = {approx:.17f}")
    print(f"  误差:   {error:.2e}")
    print(f"  说明:   符号坐标可以完全避免此类累积误差。")
    print()


# ╔══════════════════════════════════════════════════════════════════════════════╗
# ║  示例 4：性能对比                                                         ║
# ╚══════════════════════════════════════════════════════════════════════════════╝

def example_performance():
    """
    符号 vs 数值 vs 混合模式下的性能对比。
    """
    print("=" * 60)
    print("  示例 4：性能对比")
    print("=" * 60)

    N = 10000

    # 纯浮点运算
    with Benchmark(f"浮点加法 (N={N})"):
        s = 0.0
        for i in range(N):
            s += 1.0 / 3.0

    # 分数运算
    with Benchmark(f"分数加法 (N={N})"):
        s = Fraction(0, 1)
        for i in range(N):
            s += Fraction(1, 3)

    # 使用 SymbolicCoord（如果有 C 库）
    try:
        from lv import SymbolicCoord
        with Benchmark(f"SymbolicCoord 加法 (N={N})"):
            s = SymbolicCoord(0)
            third = SymbolicCoord(Fraction(1, 3))
            for i in range(N):
                s = s + third
    except ImportError:
        print(f"    [SymbolicCoord] 跳过（无 C 库）")

    print(f"\n  分析:")
    print(f"    浮点: 适合数值密集型任务（但可能有精度损失）")
    print(f"    分数: 完全精确，但大数运算会持续增长")
    print(f"    C 符号: 利用 GMP/MPFR 加速，兼顾速度和精度")
    print()


# ╔══════════════════════════════════════════════════════════════════════════════╗
# ║  入口                                                                     ║
# ╚══════════════════════════════════════════════════════════════════════════════╝

if __name__ == "__main__":
    example_symbolic_arithmetic()
    example_numeric_solver()
    example_mixed_precision()
    example_performance()

    print("=" * 60)
    print("  全部示例运行完毕。")
    print("=" * 60)
