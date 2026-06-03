# CUDD 二叉决策图操作库——参考与借鉴文档

> 文档类型：外部项目参考分析
> 目标项目：Lv-00 几何约束求解器
> 撰写日期：2026-05-24
> 源项目 GitHub：https://github.com/ivmai/cudd

---

## 一、项目概述

### 1.1 项目背景

CUDD（Colorado University Decision Diagram）是最经典、最广泛使用的二叉决策图（BDD）操作库之一。最初由 Fabio Somenzi 教授在 University of Colorado Boulder 开发，自 1990 年代初期至今已有超过 30 年的历史。CUDD 在形式化验证、模型检测、逻辑综合等领域有深远影响，是 NuSMV、VIS、MVSIS 等工具的底层依赖。

CUDD 提供三种核心数据结构：

- **BDD**（Binary Decision Diagram，二叉决策图）：表示布尔函数。每个节点对应一个变量，两个子边分别对应该变量取 0 或 1 时的函数值。终端节点为常数 0 或 1。
- **ADD**（Algebraic Decision Diagram，代数决策图）：BDD 的推广。终端节点可以是任意实数（而非仅 0 和 1），因此可以表示从布尔域到实数域的映射。
- **ZDD**（Zero-suppressed BDD，零压缩 BDD）：另一种 BDD 变体，通过省略"取 0 分支"的节点来实现稀疏集合的高效表示。

实现语言为纯 C（ANSI C），采用 BSD 风格许可。CUDD 以其出色的性能和稳定性而闻名，当前的维护者为 Ivor Ma (ivmai)，仓库位于 https://github.com/ivmai/cudd。

### 1.2 技术架构

```
CUDD 库核心架构：

┌─────────────────────────────────────────────────┐
│                   用户 API 层                      │
│  Cudd_Init / Cudd_Quit                           │
│  Cudd_bddIthVar / Cudd_bddAnd / Cudd_bddOr / ... │
│  Cudd_addIthVar / Cudd_addApply / Cudd_addExist...│
│  Cudd_zddIthVar / Cudd_zddIntersect / ...        │
└──────────────────────┬──────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────┐
│              核心管理器 (DdManager)                │
│  - 唯一表 (Unique Table): 保证每个函数只有唯一的    │
│    BDD/ADD/ZDD 节点                                │
│  - 计算缓存 (Computed Table): 记忆化操作结果       │
│  - 引用计数 (Reference Counting): 自动垃圾回收     │
│  - 变量序管理: 动态重排序 (sifting)                │
└──────────────────────┬──────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────┐
│               节点存储与操作层                       │
│  - DdNode 结构 (节点)                              │
│  - ITE (if-then-else) 递归操作                     │
│  - Cudd_Apply (通用二元操作)                       │
│  - 量化操作 (存在量词/全称量词)                     │
│  - 变量重排序 (sifting / group sifting)            │
└─────────────────────────────────────────────────┘
```

CUDD 的核心设计原则包括：

1. **唯一表（Unique Table）**：每个布尔函数在 CUDD 中只有唯一的 BDD 节点表示。通过哈希表实现，检查 `(var, low_child, high_child)` 三元组是否已存在节点。这保证了强规范化（strong canonicity）。

2. **计算缓存（Computed Table）**：也称为记忆化缓存（memoization cache），存储二元操作（如 AND、OR）的结果。当执行 `f AND g` 时，先检查缓存中是否有 `(AND, f, g)` 的已有结果，避免重复计算。这使得 BDD 操作的时间复杂度与 BDD 节点数线性相关。

3. **引用计数与垃圾回收**：CUDD 使用引用计数来跟踪节点的使用情况。当节点不再被引用时，通过垃圾回收将其回收。

4. **动态变量重排序**：变量序对 BDD 大小有指数级影响。CUDD 的 sifting 算法会动态重新排列变量次序以最小化 BDD 节点数。

### 1.3 核心数据结构：BDD 与 ADD

**BDD（二叉决策图）示例**：

布尔函数 `f(a, b, c) = (a ∧ b) ∨ c` 的 BDD 表示（变量序 a < b < c）：

```
        a
       / \
      0   1
     /     \
    b       b
   / \     / \
  0   1   0   1
  |   |   |   |
  c   c   c   c
 / \ / \ / \ / \
0  1 0 1 0  1 0  1
```

在实际的简化 BDD（ROBDD）中，相同子树会被合并，冗余节点会被消除。

**ADD（代数决策图）示例**：

一个简单的分段函数 `g(a, b) = 2.5 if a ∧ b else 1.0 if a else 0.3` 的 ADD 表示：

```
        a
       / \
      0   1
     /     \
   0.3     b
          / \
         0   1
        /     \
       1.0    2.5
```

ADD 特别适合表示多项式、权重函数和概率分布，这些在 Lv-00 的符号坐标计算中非常有用。

---

## 二、核心借鉴点

### 2.1 BDD 变量序优化

变量序对 BDD 大小的影响是决定性的。同一个布尔函数，在好的变量序下可能只需要 O(n) 个节点，在差的变量序下可能需要 O(2^n) 个节点。例如，n 位加法器的 BDD 大小在最差序下为指数级，在最佳序（位交错排列）下为线性。

CUDD 提供了两种主要的变量重排序算法：

- **Sifting（筛选法）**：针对单个变量，依次尝试将其移动到所有其他位置，找到使 BDD 节点数最小的位置。类似于插入排序。
- **Group Sifting（分组筛选法）**：将多个相关变量捆绑为一组，整体移动。适用于对称变量较多的函数。

CUDD 的变量重排序可以**动态触发**（当节点数增长超过阈值时自动执行），也可以在用户请求时**手动触发**。

Lv-00 借鉴场景：当使用 BDD 编码几何约束系统时，变量序直接对应几何实体的"编码顺序"。不同的编码顺序会产生数量级差异的 BDD 大小。借鉴 CUDD 的 sifting 思想，Lv-00 可以实现几何约束专用的变量序自适应优化，例如：

- 将几何上相邻的实体分配相邻的变量位。
- 将独立变量放在决策图的高层，依赖变量放在低层。
- 识别约束图中的"瓶颈变量"（这些变量的赋值影响最大），将它们放在更靠前的位置。

### 2.2 ADD 的数值编码能力

ADD 相比于 BDD 的核心优势是：它可以自然地表示数值函数和权重分布。在 Lv-00 的语境中，ADD 可以用于：

1. **符号坐标表示**：将几何实体的坐标编码为 ADD 而非显式的浮点数。ADD 中每条从根到叶子的路径对应一种可能的坐标赋值，叶子存储该赋值下的坐标值。

2. **约束满足度评分**：不是简单的 0/1（满足/不满足），而是在不同构型上给出连续值评分。例如，对于距离约束 `|d(A,B) - 3.0| < 0.1`，ADD 可以存储该构型下的实际误差值。

3. **概率密度函数表示**：使用 ADD 在整个约束解空间上表示概率密度函数，支持不确定性传播和灵敏度分析。

示例：用 ADD 表示 Lv-00 中一个点的 x 坐标值（假设用 3 位编码）：

```python
# 伪代码：用 ADD 表示点 p 的 x 坐标值
# px_add =    x2 * 4 + x1 * 2 + x0 * 1   (3-bit 编码，范围 [0, 7])
# 对于每个变量赋值 (x2, x1, x0)，叶子节点存储对应的坐标值

# (x2=1, x1=1, x0=0) → 1*4 + 1*2 + 0*1 = 6.0
# (x2=0, x1=1, x0=1) → 0*4 + 1*2 + 1*1 = 3.0
# ...
```

### 2.3 ZDD 的稀疏表示

ZDD（零压缩 BDD）是一种专门为表示稀疏集合族而优化的决策图。它与 BDD 的关键区别在于"省略规则":

- BDD 中：如果 then-边和 else-边指向同一子节点，则该节点冗余，可被消除。
- ZDD 中：如果 then-边指向终端 0，则该节点及其 else-子节点被省略。

这使得 ZDD 在表示"仅含少量元素的集合"时特别高效。对于 Lv-00，ZDD 可以用于：

- **约束集合的稀疏表示**：当约束系统仅活跃在少数几何实体上时，ZDD 可以比 BDD 更紧凑地存储活跃约束的组合。
- **冲突约束集合的枚举**：在所有约束中找到冲突的最小集合（MUS, Minimal Unsatisfiable Subset），ZDD 可以高效存储所有 MUS。

### 2.4 唯一表与引用计数机制

CUDD 的引用计数和唯一表机制是保证 BDD 操作正确性和性能的基础。Lv-00 可以借鉴这一机制来管理几何约束图中的"不变式子图"（invariant subgraph）：

- **唯一表 → 不变式子图缓存**：在约束传播过程中，某些子图的约束满足状态可能被重复计算。通过唯一表缓存（canonical cache），可以避免对相同子图的重复求解。
- **引用计数 → 自动垃圾回收**：当约束图的一部分不再活跃时（例如，一个局部约束被删除），自动释放相关的缓存数据。
- **计算缓存 → 约束传播记忆化**：类似于 CUDD 的 Computed Table，Lv-00 可以在约束传播过程中缓存中间结果，加速增量式约束求解。

### 2.5 对照表：CUDD 概念与 Lv-00 对应关系

| CUDD 概念 | Lv-00 对应 | 借鉴价值 |
|-----------|-----------|----------|
| BDD (二叉决策图) | 布尔化约束编码 (bit-blasted constraint encoding) | 将连续几何约束编码为有限布尔变量上的函数 |
| ADD (代数决策图) | 符号坐标表示、约束满足度评分 | 在约束解空间上表示数值函数 |
| ZDD (零压缩 BDD) | 稀疏约束集合、MUS 枚举 | 高效存储约束组合的稀疏集合 |
| ROBDD (简化有序 BDD) | 规范化约束表示 (canonical constraint form) | 强规范化保证等价性判断的高效 |
| DdManager | 约束图符号化后端管理器 | 统一管理 BDD/ADD/ZDD 的创建和操作 |
| 唯一表 (Unique Table) | 不变式子图缓存 | 避免对相同约束子图的重复求解 |
| 计算缓存 (Computed Table) | 约束传播记忆化 | 缓存中间约束求解结果 |
| 引用计数 (Ref Counting) | 约束子图生命周期管理 | 自动管理缓存数据的生命周期 |
| Sifting 变量重排序 | 几何约束 BDD 变量序优化 | 自适应优化编码顺序，减少 BDD 节点数 |
| Group Sifting | 按几何关联性分组重排序 | 将空间上相邻的实体变量归为一组 |
| ITE (if-then-else) | 条件约束的符号化表示 | 条件分支约束的高效 BDD 表示 |
| Cudd_addApply | 符号化约束传播操作 | 在 ADD 上进行约束的合取/析取/量化 |
| Exist/Forall Abstraction | 变量消除（符号化投影） | 消除中间变量，简化约束系统 |

---

## 三、Lv-00 映射方案

### 3.1 BDD 编码几何约束系统 (Bit-blasting)

```python
# lv00/symbolic/bdd_encoder.py

import ctypes
from ctypes import c_void_p, c_int, c_double, POINTER, byref
from enum import IntEnum
from typing import Dict, List, Tuple, Optional


class CUDDManager:
    """
    CUDD 管理器的 Python 包装。
    封装 CUDD 的 DdManager，提供 Pythonic 的接口。

    在实际实现中，建议使用 pybind11 或 cffi 进行 CUDD 的 Python 绑定。
    此处展示概念接口。
    """

    def __init__(self, num_vars: int = 256):
        """
        初始化 CUDD 管理器。

        Args:
            num_vars: 最大变量数量。
                对于几何约束编码，建议预留充足的变量空间。
                每个 2D 点需要 2 * num_bit_vars 个 BDD 变量。
        """
        self._num_vars = num_vars
        self._manager = None  # CUDD DdManager 的指针
        self._var_to_bdd: Dict[int, c_void_p] = {}  # 变量 ID → BDD 节点

    def new_var(self) -> int:
        """创建一个新的 BDD 变量"""
        var_id = len(self._var_to_bdd)
        if var_id >= self._num_vars:
            raise RuntimeError("Exceeded maximum variable count")
        # self._var_to_bdd[var_id] = Cudd_bddIthVar(self._manager, var_id)
        return var_id

    def make_literal(self, var_id: int, polarity: bool) -> c_void_p:
        """创建文字的 BDD（正文字或负文字）"""
        if polarity:
            return self._var_to_bdd[var_id]
        else:
            return self._not(self._var_to_bdd[var_id])

    def and_op(self, f: c_void_p, g: c_void_p) -> c_void_p:
        """BDD 合取"""
        # return Cudd_bddAnd(self._manager, f, g)
        pass

    def or_op(self, f: c_void_p, g: c_void_p) -> c_void_p:
        """BDD 析取"""
        # return Cudd_bddOr(self._manager, f, g)
        pass

    def _not(self, f: c_void_p) -> c_void_p:
        """BDD 否定（使用 complemented edge）"""
        # return Cudd_Not(f)
        pass

    def read_node_count(self) -> int:
        """返回当前 BDD 的节点数量"""
        # return Cudd_ReadNodeCount(self._manager)
        pass

    def reorder(self, method: str = "sift"):
        """触发变量重排序"""
        # Cudd_ReduceHeap(self._manager, CUDD_REORDER_SIFT, 1)
        pass

    def get_statistics(self) -> dict:
        """获取管理器统计信息"""
        return {
            "num_vars": self._num_vars,
            "num_nodes": self.read_node_count(),
            "num_reorderings": 0,  # Cudd_ReadReorderings(self._manager)
        }


class GeometricBDDEncoder:
    """
    将几何约束系统编码为 BDD。

    核心编码策略：
    1. 将连续坐标范围离散化为 2^k 级（k = num_bits）。
    2. 每个点坐标 (X, Y) 用 (X_bit_0, ..., X_bit_{k-1}, Y_bit_0, ..., Y_bit_{k-1}) 表示。
    3. 每个约束（距离、角度、共线等）编码为 BDD。
    4. 所有约束的 BDD 进行合取 → 得到满足所有约束的构型的 BDD。
    """

    def __init__(self, manager: CUDDManager, num_bits: int = 8,
                 coord_min: float = -100.0, coord_max: float = 100.0):
        self.mgr = manager
        self.num_bits = num_bits
        self.coord_min = coord_min
        self.coord_max = coord_max
        self.num_levels = 2 ** num_bits
        self.step = (coord_max - coord_min) / self.num_levels

        # 预计算表：离散值 i 对应的位向量
        self._bit_vectors: Dict[int, List[int]] = {}
        for i in range(self.num_levels):
            bits = [(i >> b) & 1 for b in range(num_bits)]
            self._bit_vectors[i] = bits

    def encode_point(self, point_name: str) -> dict:
        """
        为一个 2D 点分配 BDD 变量。

        返回:
            {
                "x_vars": [var_id_0, ..., var_id_{k-1}],
                "y_vars": [var_id_0, ..., var_id_{k-1}],
            }
        """
        x_vars = [self.mgr.new_var() for _ in range(self.num_bits)]
        y_vars = [self.mgr.new_var() for _ in range(self.num_bits)]
        return {"x_vars": x_vars, "y_vars": y_vars, "name": point_name}

    def encode_assignment_to_bdd(self, point_vars: dict,
                                  x_val: float,
                                  y_val: float) -> c_void_p:
        """
        将特定的坐标赋值编码为 BDD（一个单一的模型）。

        例如，将"点 A 位于 (3.0, 5.0)"编码为 BDD。
        """
        x_bits = self._float_to_bits(x_val)
        y_bits = self._float_to_bits(y_val)

        # 构建该赋值的 BDD：所有变量的"位等于"的合取
        bdd = self._make_true_bdd()
        for i in range(self.num_bits):
            lit_x = self.mgr.make_literal(
                point_vars["x_vars"][i], x_bits[i])
            lit_y = self.mgr.make_literal(
                point_vars["y_vars"][i], y_bits[i])
            bdd = self.mgr.and_op(bdd, lit_x)
            bdd = self.mgr.and_op(bdd, lit_y)
        return bdd

    def encode_distance_constraint(self,
                                     point_a_vars: dict,
                                     point_b_vars: dict,
                                     target: float,
                                     tolerance: float) -> c_void_p:
        """
        将距离约束编码为 BDD。

        策略：对每一个满足 |dist - target| < tolerance 的离散赋值，
        构建其对应 BDD，然后对所有满足赋值的 BDD 取 OR。

        概念实现。生产环境中应使用更高效的比较器电路编码。
        """
        sat_bdd = self._make_false_bdd()
        satisfied_count = 0

        for ax_val in range(self.num_levels):
            ax = self.coord_min + ax_val * self.step
            for ay_val in range(self.num_levels):
                ay = self.coord_min + ay_val * self.step
                for bx_val in range(self.num_levels):
                    bx = self.coord_min + bx_val * self.step
                    for by_val in range(self.num_levels):
                        by = self.coord_min + by_val * self.step

                        dist = ((ax - bx) ** 2 + (ay - by) ** 2) ** 0.5
                        if abs(dist - target) <= tolerance:
                            # 构建该赋值的 BDD 并加到 sat_bdd
                            assign_bdd = self._build_assignment_bdd(
                                point_a_vars, ax_val, ay_val,
                                point_b_vars, bx_val, by_val)
                            sat_bdd = self.mgr.or_op(sat_bdd, assign_bdd)
                            satisfied_count += 1

        return sat_bdd

    def _float_to_bits(self, value: float) -> List[int]:
        """浮点值 → 位向量"""
        clamped = max(self.coord_min,
                      min(self.coord_max - self.step, value))
        discrete = int((clamped - self.coord_min) / self.step)
        return self._bit_vectors[discrete]

    def _build_assignment_bdd(self, a_vars, ax_val, ay_val,
                                b_vars, bx_val, by_val):
        """为特定赋值构建 BDD（概念方法）"""
        # 实际实现：构建 (ax_bits AND ay_bits AND bx_bits AND by_bits)
        pass

    def _make_true_bdd(self) -> c_void_p:
        """返回恒真 BDD"""
        pass

    def _make_false_bdd(self) -> c_void_p:
        """返回恒假 BDD"""
        pass
```

### 3.2 ADD 符号坐标表示

```python
# lv00/symbolic/add_coordinates.py

class SymbolicCoordinate:
    """
    使用 ADD（代数决策图）表示几何实体的符号坐标。

    与显式浮点数不同，符号坐标将坐标值存储在 ADD 的终端节点中，
    可以高效表示坐标在所有可能赋值下的值。

    示例：
    - 一个点的 x 坐标 = ADD( x2*4 + x1*2 + x0*1 )
    - 在赋值 (x2=1, x1=0, x0=1) 下，ADD 求值为 5.0
    """

    def __init__(self, manager, bit_vars: List[int],
                 coord_min: float, coord_max: float):
        self.mgr = manager
        self.bit_vars = bit_vars  # BDD 变量列表（低位 → 高位）
        self.num_bits = len(bit_vars)
        self.coord_min = coord_min
        self.coord_max = coord_max
        self.num_levels = 2 ** self.num_bits
        self.step = (coord_max - coord_min) / self.num_levels

        # 构建 ADD：对每个位，权重为 2^i
        # coord_add = Σ (bit_i * 2^i * step) + coord_min
        self.add = self._build_coordinate_add()

    def _build_coordinate_add(self):
        """构建坐标 ADD"""
        # 从最低位开始，递归构建
        # coord = coord_min + step * Σ(bit_i * 2^i)
        #
        # 每个位变量对应一个 ADD 内部节点：
        #   如果 bit_i = 0，贡献 0
        #   如果 bit_i = 1，贡献 step * 2^i
        add_result = None
        for i in range(self.num_bits):
            # 构建 2^i * step 的常量 ADD
            const_add = self._make_constant_add(self.step * (2 ** i))

            # 构建"如果 bit_i 为真则 const_add 否则 0"的 ADD
            var_node = self._get_var_add(self.bit_vars[i])
            bit_add = self._add_ite(var_node, const_add,
                                     self._make_constant_add(0.0))

            # 累加到结果
            if add_result is None:
                add_result = bit_add
            else:
                add_result = self._add_apply("+", add_result, bit_add)

        # 添加偏移 coord_min
        offset = self._make_constant_add(self.coord_min)
        add_result = self._add_apply("+", add_result, offset)

        return add_result

    def evaluate(self, assignment: Dict[int, int]) -> float:
        """
        在给定的变量赋值下评估坐标值。
        """
        val = 0.0
        for i in range(self.num_bits):
            if assignment.get(self.bit_vars[i], 0):
                val += self.step * (2 ** i)
        return self.coord_min + val

    def _make_constant_add(self, value: float):
        """创建常量 ADD"""
        pass

    def _get_var_add(self, var_id: int):
        """获取变量的 ADD 节点"""
        pass

    def _add_ite(self, cond, then_add, else_add):
        """ADD 的 if-then-else 操作"""
        pass

    def _add_apply(self, op: str, a, b):
        """ADD 的二元 Apply 操作 (+/-/*/min/max)"""
        pass


class SymbolicDistance:
    """
    使用 ADD 计算两点间距离的符号表示。

    利用 ADD 的代数操作，可以构建
    distance_add = sqrt((x_a_add - x_b_add)² + (y_a_add - y_b_add)²)
    的 ADD 表示。

    这使得可以在符号层面比较距离约束：例如判断 distance_add < target
    等价于检查 ADD 的每个叶子节点值是否小于 target。
    """

    def __init__(self, point_a_coords: Tuple[SymbolicCoordinate, SymbolicCoordinate],
                 point_b_coords: Tuple[SymbolicCoordinate, SymbolicCoordinate]):
        self.ax, self.ay = point_a_coords
        self.bx, self.by = point_b_coords

        # 构建距离的 ADD 表示
        # dx² = (ax - bx)²
        dx = self._add_subtract(self.ax.add, self.bx.add)
        dx_sq = self._add_multiply(dx, dx)

        dy = self._add_subtract(self.ay.add, self.by.add)
        dy_sq = self._add_multiply(dy, dy)

        # distance = sqrt(dx² + dy²)
        sum_sq = self._add_apply("+", dx_sq, dy_sq)
        self.distance_add = self._add_sqrt(sum_sq)

    def check_constraint(self, target: float, tolerance: float) -> bool:
        """
        检查距离是否在 [target - tolerance, target + tolerance] 内。
        通过遍历 ADD 的叶子节点来完成。
        """
        # 构建 |distance - target| < tolerance 的 BDD
        pass

    def _add_subtract(self, a, b):
        pass

    def _add_multiply(self, a, b):
        pass

    def _add_apply(self, op, a, b):
        pass

    def _add_sqrt(self, add):
        pass
```

### 3.3 变量序自适应优化

```python
# lv00/symbolic/variable_ordering.py

from typing import List, Dict, Set, Tuple
from collections import defaultdict
import itertools


class GeometricVariableOrderOptimizer:
    """
    借鉴 CUDD 的 sifting 思想，
    为几何约束 BDD 编码优化变量序。

    核心策略：
    1. 构建几何实体依赖图。
    2. 拓扑排序 → 将独立变量放在前面的决策层。
    3. 空间邻近性 → 将几何上靠近的实体变量交错排列。
    4. 约束权重 → 将参与更多约束的变量放在更高层。
    """

    def __init__(self, constraint_graph):
        self.graph = constraint_graph

    def compute_dependency_graph(self) -> Dict[str, Set[str]]:
        """
        计算几何实体之间的依赖关系图。
        如果两个实体由同一个约束关联，则认为它们存在依赖。
        """
        deps = defaultdict(set)
        for constraint in self.graph.constraints:
            entities = constraint.involved_entities()
            for e1, e2 in itertools.combinations(entities, 2):
                deps[e1].add(e2)
                deps[e2].add(e1)
        return deps

    def compute_variable_weights(self) -> Dict[str, float]:
        """
        计算每个几何实体的"约束参与度"权重。
        参与约束数量越多 → 权重越大 → 变量序更靠前。
        """
        weights = defaultdict(float)
        for constraint in self.graph.constraints:
            entities = constraint.involved_entities()
            for entity in entities:
                weights[entity] += 1.0 / len(entities)
        return dict(weights)

    def optimize_ordering(self) -> Tuple[List[str], Dict[str, int]]:
        """
        优化变量序，返回 (ordered_entities, var_position_map)。

        算法：
        1. 构建依赖图。
        2. 按约束参与度降序排列实体。
        3. 贪心地将高权重实体放在前面的决策层。
        4. 对空间邻近的实体进行分组交错（类似 group sifting）。
        """
        deps = self.compute_dependency_graph()
        weights = self.compute_variable_weights()

        # 按权重降序排列
        sorted_entities = sorted(
            weights.keys(),
            key=lambda e: (-weights[e], e)
        )

        # 空间分组：将空间邻近的实体归为一组
        groups = self._cluster_by_proximity(sorted_entities)

        # 组内按依赖关系排序（最小化交叉依赖）
        ordered = []
        for group in groups:
            ordered_in_group = self._topological_sort_within_group(
                group, deps)
            ordered.extend(ordered_in_group)

        # 构建位置映射
        var_positions = {}
        for pos, entity in enumerate(ordered):
            var_positions[entity] = pos

        return ordered, var_positions

    def _cluster_by_proximity(self, entities: List[str]) -> List[List[str]]:
        """
        根据空间邻近性将实体分为若干组。
        同一组内的实体在 BDD 变量序中会相邻排列（类似 group sifting）。
        """
        # 概念实现：基于各实体的边界框进行空间聚类
        # 生产环境中可以使用 k-means 或 DBSCAN
        if len(entities) <= 4:
            return [entities]

        # 简化：根据约束中的距离提示分组
        groups = []
        remaining = set(entities)

        while remaining:
            seed = remaining.pop()
            cluster = {seed}
            # 将与 seed 距离最近的几个实体加入同一组
            for constraint in self.graph.constraints:
                involved = constraint.involved_entities()
                if seed in involved:
                    for e in involved:
                        if e in remaining:
                            cluster.add(e)
                            remaining.discard(e)
            groups.append(list(cluster))

        return groups if groups else [entities]

    def _topological_sort_within_group(self, group, deps):
        """组内的拓扑排序：减少交叉依赖"""
        visited = set()
        order = []

        def dfs(entity):
            if entity in visited:
                return
            visited.add(entity)
            for neighbor in deps.get(entity, set()):
                if neighbor in group:
                    dfs(neighbor)
            order.append(entity)

        for entity in group:
            dfs(entity)
        return list(reversed(order))

    def evaluate_ordering_quality(self, ordering: List[str]) -> dict:
        """
        评估变量序质量。

        返回指标：
        - cross_dependencies: 变量序中不相邻但存在依赖的实体对数量。
            越少越好（类似于 BDD 的交叉边）。
        - avg_span: 依赖实体对在序中的平均跨度。
            越小越好，表示相关变量更靠近。
        """
        deps = self.compute_dependency_graph()
        pos = {e: i for i, e in enumerate(ordering)}

        cross_deps = 0
        total_span = 0
        dep_count = 0

        for e1, neighbors in deps.items():
            for e2 in neighbors:
                if e1 < e2:  # 防止重复计数
                    span = abs(pos[e1] - pos[e2])
                    if span > 5:  # 跨度超过阈值视为交叉依赖
                        cross_deps += 1
                    total_span += span
                    dep_count += 1

        return {
            "num_entities": len(ordering),
            "num_dependencies": dep_count,
            "cross_dependencies": cross_deps,
            "avg_dependency_span": total_span / max(dep_count, 1),
            "cross_dependency_ratio": cross_deps / max(dep_count, 1),
        }
```

---

## 四、实现路线图

### 4.1 分阶段实施计划

| 阶段 | 名称 | 目标 | 预计工作量 | 交付物 |
|------|------|------|-----------|--------|
| **阶段 1** | BDD 约束编码器 | 实现几何约束的 BDD 编码（bit-blasting），绑定 CUDD 的 Python 接口 | 4-5 周 | `bdd_encoder.py`、CUDD Python 绑定、基础编码正确性测试 |
| **阶段 2** | ADD 符号坐标表示 | 实现基于 ADD 的符号坐标计算，支持多项式操作和约束满足度评分 | 3-4 周 | `add_coordinates.py`、ADD 操作封装、符号距离计算 |
| **阶段 3** | 变量序自适应优化 | 实现几何约束专用的变量序优化算法，包括依赖分析和空间分组 | 2-3 周 | `variable_ordering.py`、sifting 风格优化器、性能基准 |

### 4.2 阶段 1 详细任务

1. **CUDD 绑定**
   - 使用 pybind11 或 ctypes 封装 CUDD 的核心 API。
   - 实现 DdManager、BDD 节点、基本操作（AND/OR/NOT）的 Python 接口。
   - 处理 complemented edge 和引用计数的 Python 内存安全。

2. **约束编码**
   - 实现浮点坐标的位向量编码。
   - 实现基本几何约束（距离、角度、共线）的 BDD 编码。
   - 实现约束合取（AND），得到约束系统的完整 BDD。

3. **验证**
   - 对小规模示例进行 BDD 编码正确性测试。
   - 对比 BDD 结果与直接数值方法的结果。

### 4.3 阶段 2 详细任务

1. **ADD 操作实现**
   - 实现 ADD 的创建、Apply（+/−/×/min/max 等）、ITE 操作。
   - 实现 ADD 的叶节点遍历（用于评估具体赋值下的值）。

2. **符号坐标计算**
   - 实现 SymbolicCoordinate 类。
   - 实现 SymbolicDistance 类。
   - 支持在符号层面计算约束满足度评分。

3. **多项式表示**
   - 实现多项式在 ADD 上的加法和乘法。
   - 探索 ADD 在公差链分析中的应用。

### 4.4 阶段 3 详细任务

1. **依赖图分析**
   - 实现几何实体依赖图的构建和分析。
   - 计算约束参与度权重。

2. **变量序优化**
   - 实现贪心排序算法。
   - 实现空间分组排序（Group Sifting 的几何模拟）。
   - 实现排序质量评估指标。

3. **性能基准测试**
   - 测试不同变量序对 BDD 节点数的影响。
   - 对比默认序和优化后的性能差异。
   - 在真实几何约束基准上进行评估。

---

## 五、附录

### 5.1 关键参考

- **GitHub 仓库**：https://github.com/ivmai/cudd
- **CUDD 手册**：https://github.com/ivmai/cudd/blob/main/cudd/cudd.pdf
- **核心论文**：Somenzi, "CUDD: CU Decision Diagram Package", 1998–2015.
- **BDD 基础**：Bryant, "Graph-Based Algorithms for Boolean Function Manipulation", IEEE TC 1986.
- **动态重排序**：Rudell, "Dynamic Variable Ordering for Ordered Binary Decision Diagrams", ICCAD 1993.

### 5.2 术语表

| 术语 | 英文 | 说明 |
|------|------|------|
| BDD | Binary Decision Diagram | 二叉决策图，布尔函数的规范有向无环图表示 |
| ROBDD | Reduced Ordered BDD | 简化有序 BDD，等价节点的合并和冗余消除 |
| ADD | Algebraic Decision Diagram | 代数决策图，终端节点为实数的 BDD 推广 |
| MTBDD | Multi-Terminal BDD | CUDD 中 ADD 的别称 |
| ZDD | Zero-suppressed BDD | 零压缩 BDD，优化稀疏集合族的表示 |
| Sifting | Sifting | 单变量动态重排序算法 |
| Group Sifting | Group Sifting | 多变量分组重排序算法 |
| 唯一表 | Unique Table | 保证每个函数有唯一 BDD 表示的哈希表 |
| 计算缓存 | Computed Table | 缓存二元操作结果以加速递归计算 |
| Complemented Edge | Complemented Edge | 通过边上的取反标志避免创建 NOT 节点 |
| ITE | if-then-else | BDD/ADD 的基本三元操作 |
| Bit-blasting | Bit-blasting | 将高层次数据路径"炸开"为位级别的布尔约束 |

### 5.3 Lv-00 项目中的相关文件路径

| 模块 | 计划路径 |
|------|----------|
| CUDD Python 绑定 | `lv00/symbolic/cudd_binding.py` / `lv00/symbolic/_cudd.cpp` |
| BDD 编码器 | `lv00/symbolic/bdd_encoder.py` |
| ADD 符号坐标 | `lv00/symbolic/add_coordinates.py` |
| 变量序优化器 | `lv00/symbolic/variable_ordering.py` |
| 测试 | `tests/symbolic/test_bdd_encoder.py` |

---

*文档版本：v1.0 | 最后更新：2026-05-24 | 维护者：Lv-00 项目组*
