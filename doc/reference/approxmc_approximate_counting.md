# ApproxMC 近似模型计数——参考与借鉴文档

> 文档类型：外部项目参考分析
> 目标项目：Lv-00 几何约束求解器
> 撰写日期：2026-05-24
> 源项目 GitHub：https://github.com/meelgroup/approxmc

---

## 一、项目概述

### 1.1 项目背景

ApproxMC 是 Meel Group（由 Kuldeep S. Meel 教授领导，现任教于 National University of Singapore）开发的近似模型计数器。它解决的是 #SAT 问题（Sharp-SAT，或称模型计数问题）：给定一个命题逻辑公式（通常以 CNF 形式表示），计算有多少个变量赋值使公式为真。

精确的 #SAT 是 #P-complete 问题（比 NP-complete 的 SAT 更难），对于大规模公式来说精确求解不现实。ApproxMC 通过引入近似技术，提供 PAC（Probably Approximately Correct，概率近似正确）保证的模型计数。它在 SAT 竞赛和模型计数竞赛中长期位居前列。

技术栈方面，ApproxMC 的核心底层依赖两个关键组件：

- **CryptoMiniSat**：Mate Soos 开发的高性能 SAT 求解器，支持 XOR 子句（异或子句），这对基于哈希的近似计数至关重要。
- **Arjun**：独立支持（Independent Support）计算工具，自动识别和消除公式中的冗余变量。

实现语言为 C++，采用 MIT 许可。

### 1.2 技术架构

ApproxMC 的整体架构遵循"哈希基近似计数"（Hash-based Approximate Counting）范式：

```
输入 CNF 公式
    ↓
Arjun：独立支持计算
    │ 识别非独立变量，只在独立支持变量上计数
    │ 输出约简后的变量集合
    ↓
ApproxMC 核心循环（Algorithm 1）：
    ├── 1. 随机选择哈希函数 h ∈ H(n, m, 3)
    ├── 2. 构造 CNF + XOR 哈希约束的合取公式
    ├── 3. CryptoMiniSat 求解：是否有解？
    │       - 有解：增加哈希约束（缩小解空间）
    │       - 无解：记录当前阈值
    ├── 4. 通过多个独立哈希实验估算解的规模
    └── 5. 取所有实验的中位数作为最终估计
    ↓
输出：满足赋值的近似数量（含 ε-δ 保证）
```

核心算法是 ApproxMC 的 Algorithm 1（来自 Chakraborty, Meel, Vardi 2013, 2016）：

```
Algorithm 1: ApproxMC(C, ε, δ)
Input: CNF formula C, tolerance ε, confidence δ
Output: Approximate count with PAC guarantee

1. counter ← 0; pivot ← compute_pivot(ε, δ)
2. repeat ⌈17 log₂(3/δ)⌉ times:
3.     Y ← IndependentSupport(C)      // Arjun
4.     m ← |Y|  // number of independent variables
5.     Choose m random XOR constraints (each with 3 variables)
6.     nSol ← CryptoMiniSat(C ∧ XORs) // count solutions via SAT calls
7.     counter ← counter + log₂(nSol)
8. return median_cell_count(counter, pivot)
```

ApproxMC 6 和 ApproxMC 7 是目前使用最广泛的两个版本，其中 ApproxMC 7 通过改进哈希函数选择和优化 pivot 计算进一步提升了精度和效率。

### 1.3 PAC 保证

ApproxMC 的核心卖点之一是其 PAC（Probably Approximately Correct）保证。默认参数为：

- **ε = 0.8**（近似比 tolerance）：估计值在真实值的 `[1/(1+ε), 1+ε]` 倍数范围内。
- **δ = 0.2**（置信度 confidence）：估计值以 `1-δ` 的概率落在上述范围内。

这意味着：ApproxMC 返回的计数 `c` 满足 `P(1/(1+ε) ≤ c/true_count ≤ 1+ε) ≥ 1-δ`，即至少有 80% 的概率，估计值在真实值的 0.56 到 1.8 倍之间。

用户可以通过命令行参数调整 ε 和 δ，以获得更高精度（更小的 ε）或更高置信度（更大的 `1-δ`）。

---

## 二、核心借鉴点

### 2.1 哈希基近似计数

ApproxMC 的哈希基近似计数思想可以直观地理解为一个"随机二分搜索"的过程：

1. 假设公式 F 有 `S` 个解。
2. 添加一个随机 XOR 哈希约束 `h(x) = b`（其中 `h` 是一个随机的 3-XOR 函数）。
3. 该约束以约 1/2 的概率将任意一个解保留下来。
4. 添加 `m` 个独立随机 XOR 约束后，每个解以约 `1/2^m` 的概率被保留。
5. 如果添加了 `m` 个约束后公式仍有解，说明原解空间至少为 `2^m`；如果无解，说明原解空间小于 `2^m`。
6. 通过二分查找合适的 `m` 值，就可以逼近真实解数。

将这个思想映射到 Lv-00 的几何约束上下文：

- 几何约束系统的"解"对应满足所有几何关系的一组几何实体位置。
- "模型计数"对应约束解空间的"体积"或"维度"——有多少个不同的构型满足约束。
- 哈希约束对应：对解空间施加额外的随机限制（例如，随机固定某些坐标），观察约束系统是否仍然可解。

借鉴价值在于：Lv-00 可以引入"近似可构造性"（approximate constructability）概念——不仅回答"这个约束系统有解吗？"（SAT 的 yes/no），而且回答"大约有多少种方案可以满足这些约束？"（#SAT 的计数）。这在实际工程中非常有用，例如评估设计的灵活性：一个只有 1 种解的设计可能过于刚性，而有数千种解的设计则提供了更大的优化空间。

### 2.2 Arjun 独立支持

Arjun 是 ApproxMC 的核心辅助工具，用于计算 CNF 公式的"独立支持"（Independent Support, IS）。其核心思想是：

- 给定一个 CNF 公式 F，它的全体变量集合为 V。
- **独立支持** IS ⊆ V 是一个最小子集，使得对于任意两个变量赋值 τ₁ 和 τ₂，如果它们在 IS 上的赋值相同，那么它们在 V 上的可满足性也相同。
- 换句话说，不在 IS 中的变量是"冗余的"——它们的值可以由 IS 变量的值唯一确定。

Arjun 通过迭代的 SAT 查询和变量依赖图分析来自动发现独立支持。这对于模型计数的性能至关重要：计数复杂度只与独立支持变量数 |IS| 有关，而不是所有变量数 |V|。

对应到 Lv-00 的几何约束系统：

- 约束图中的变量（几何实体的坐标、角度等）并非全部独立。
- 例如，给定三角形的两个顶点坐标和三条边长，第三个顶点的坐标是确定的（最多两种可能）——第三个顶点的坐标不是独立的。
- Arjun 风格的分析可以帮助 Lv-00 自动发现约束系统中的"独立自由度"（independent degrees of freedom）和"过度约束"（over-constraint）。

### 2.3 PAC 保证与近似可构造性

ApproxMC 的 PAC 保证形式为 Lv-00 引入"近似可构造性"（approximate constructability）概念提供了数学基础。传统几何求解器通常给出 yes/no 的二值判断，但在实际工程中：

- **刚性设计问题**：一个约束系统可能"几乎无解"——虽然有解，但解的数量非常少（例如只有 1-2 个），导致制造和装配非常困难。
- **过度自由问题**：一个约束系统可能有过多解，说明设计不够充分约束。
- **PAC 保证的应用**：Lv-00 可以报告"约束系统的解空间体积约为 V，此估计在 0.56V 到 1.8V 之间的置信度为 80%"。

### 2.4 模型计数在约束分析中的应用

比 SAT 的 yes/no 判断更丰富的约束分析能力：

| 分析维度 | SAT (yes/no) | #SAT (模型计数) | Lv-00 借鉴 |
|----------|-------------|----------------|-----------|
| 有解/无解 | 是 | 是 | 基础约束满足检查 |
| 解的数量 | 否 | 是 | 设计自由度评估 |
| 解的分布 | 否 | 间接（通过加权计数） | 公差敏感度分析 |
| 过度约束检测 | 否 | 是（0 解） | 冲突约束集合 |
| 欠约束检测 | 否 | 是（过多解） | 缺失约束建议 |
| 约束冗余 | 否 | 是（移除约束后计数不变） | 冗余约束移除 |
| 近似保证 | N/A | 是（PAC） | 可信度报告 |

### 2.5 对照表：ApproxMC 概念与 Lv-00 对应关系

| ApproxMC 概念 | Lv-00 对应 | 借鉴价值 |
|--------------|-----------|----------|
| CNF 公式 F | 几何约束系统 G | 将几何约束系统编码为逻辑公式 |
| #SAT (模型计数) | 约束解空间分析 (graph_solution_count) | 从 yes/no 升级到量化分析 |
| 模型 (model) | 约束解（一组满足所有约束的几何实体位置） | 解构造与枚举 |
| 哈希函数 (XOR) | 随机附加约束（随机固定某些坐标/角度） | 通过随机限制估计解空间大小 |
| 独立支持 (IS) | 独立自由度（Independent DOF） | 自动发现约束系统中的冗余变量 |
| PAC 保证 (ε, δ) | 近似可构造性保证 | 提供数学上可证明的估算精度保证 |
| CryptoMiniSat | Lv-00 几何 SAT 求解器 | 集成高性能求解器后端 |
| Arjun | Lv-00 自由度分析器 | 自动发现过度约束和欠约束 |
| pivot 参数 | 解空间大小的阈值参数 | 控制估算精度的超参数 |

---

## 三、Lv-00 映射方案

### 3.1 CNF 编码管道

将 Lv-00 的几何约束系统编码为 SAT/CNF 格式，这是借鉴 ApproxMC 进行约束计数的基础步骤：

```python
# lv00/counting/cnf_encoder.py

from dataclasses import dataclass, field
from typing import List, Dict, Tuple, Optional
import math

@dataclass
class CNFClause:
    """CNF 子句：一个文字列表的析取"""
    literals: List[int]  # 正整数 = 正文字, 负整数 = 负文字

    def to_dimacs(self) -> str:
        return " ".join(str(lit) for lit in self.literals) + " 0"

@dataclass
class CNFFormula:
    """CNF 公式"""
    num_vars: int
    clauses: List[CNFClause] = field(default_factory=list)
    var_mapping: Dict[str, Tuple[int, int]] = field(default_factory=dict)

    def to_dimacs(self) -> str:
        lines = [f"p cnf {self.num_vars} {len(self.clauses)}"]
        for clause in self.clauses:
            lines.append(clause.to_dimacs())
        return "\n".join(lines)


class GeometricCNFEncoder:
    """
    将几何约束系统编码为 CNF 公式。

    核心策略：
    1. 离散化：将连续坐标范围离散化为有限个可能值。
    2. Bit-blasting：每个坐标分量用 k 位二进制表示。
    3. 约束编码：将几何约束（距离、角度等）编码为 CNF 子句。
    """

    def __init__(self, num_bits_per_coord: int = 8,
                 coord_range: Tuple[float, float] = (-100.0, 100.0)):
        """
        Args:
            num_bits_per_coord: 每个坐标分量的位宽。
                8 bits → 256 个离散值，精度 = range/256。
            coord_range: 坐标的有效范围。
        """
        self.num_bits = num_bits_per_coord
        self.coord_min, self.coord_max = coord_range
        self.num_values = 2 ** num_bits
        self.step = (coord_range[1] - coord_range[0]) / self.num_values

    def float_to_bits(self, value: float) -> List[int]:
        """将浮点值离散化为位向量"""
        clamped = max(self.coord_min, min(self.coord_max - self.step, value))
        discrete = int((clamped - self.coord_min) // self.step)
        bits = []
        for i in range(self.num_bits):
            bits.append((discrete >> i) & 1)
        return bits

    def bits_to_float(self, bits: List[int]) -> float:
        """将位向量还原为浮点值"""
        discrete = sum(b << i for i, b in enumerate(bits))
        return self.coord_min + discrete * self.step

    def encode_point(self, point_name: str) -> dict:
        """
        为一个 2D 点分配 CNF 变量。
        返回: {"x_vars": [(var_id, bit_index), ...], "y_vars": [...]}
        """
        x_vars = []
        y_vars = []
        for i in range(self.num_bits):
            x_var = self._alloc_var(f"{point_name}_x_{i}")
            y_var = self._alloc_var(f"{point_name}_y_{i}")
            x_vars.append(x_var)
            y_vars.append(y_var)
        return {"x_vars": x_vars, "y_vars": y_vars}

    def encode_distance_constraint(self, point_a_vars: dict,
                                    point_b_vars: dict,
                                    target: float,
                                    tolerance: float) -> List[CNFClause]:
        """
        将距离约束编码为 CNF。

        策略：|distance(A, B) - target| < tolerance
        → 枚举所有满足条件的 (v_a, v_b) 组合
        → 将不满足的组合构造成"禁止子句"（blocking clauses）

        此实现是一个概念原型。在生产环境中，
        建议使用更高效的 Tseitin 变换或 ADP 编码。
        """
        clauses = []
        # 枚举所有可能的 (A_x, A_y, B_x, B_y) 组合
        for ax_val in range(self.num_values):
            for ay_val in range(self.num_values):
                for bx_val in range(self.num_values):
                    for by_val in range(self.num_values):
                        # 检查是否满足约束
                        dist = math.hypot(
                            self.bits_to_float(
                                [(ax_val >> i) & 1 for i in range(self.num_bits)]
                            ) - self.bits_to_float(
                                [(bx_val >> i) & 1 for i in range(self.num_bits)]
                            ),
                            self.bits_to_float(
                                [(ay_val >> i) & 1 for i in range(self.num_bits)]
                            ) - self.bits_to_float(
                                [(by_val >> i) & 1 for i in range(self.num_bits)]
                            ),
                        )
                        if abs(dist - target) > tolerance:
                            # 这个组合不满足约束，添加禁止子句
                            blocking_lits = []
                            # 枚举该赋值的否定
                            for i in range(self.num_bits):
                                ax_bit = (ax_val >> i) & 1
                                ay_bit = (ay_val >> i) & 1
                                bx_bit = (bx_val >> i) & 1
                                by_bit = (by_val >> i) & 1
                                blocking_lits.append(
                                    -point_a_vars["x_vars"][i] if ax_bit
                                    else point_a_vars["x_vars"][i])
                                blocking_lits.append(
                                    -point_a_vars["y_vars"][i] if ay_bit
                                    else point_a_vars["y_vars"][i])
                                blocking_lits.append(
                                    -point_b_vars["x_vars"][i] if bx_bit
                                    else point_b_vars["x_vars"][i])
                                blocking_lits.append(
                                    -point_b_vars["y_vars"][i] if by_bit
                                    else point_b_vars["y_vars"][i])
                            clauses.append(CNFClause(literals=blocking_lits))
        return clauses

    def encode(self, constraint_graph) -> CNFFormula:
        """
        将整个约束图编码为 CNF 公式，供 ApproxMC 使用。
        """
        formula = CNFFormula(num_vars=0)

        # 第一步：为所有几何实体分配变量
        entity_vars = {}
        for entity in constraint_graph.entities:
            if entity.type == "Point2D":
                entity_vars[entity.id] = self.encode_point(entity.id)

        # 第二步：编码所有约束
        for constraint in constraint_graph.constraints:
            if constraint.type == "DistanceConstraint":
                a_vars = entity_vars[constraint.entity_a]
                b_vars = entity_vars[constraint.entity_b]
                clauses = self.encode_distance_constraint(
                    a_vars, b_vars,
                    constraint.value, constraint.tolerance)
                formula.clauses.extend(clauses)

            # 其他约束类型的编码（AngleConstraint, Collinearity 等）
            # 原理类似，省略以保持示例简洁

        formula.num_vars = self._next_var_id - 1
        return formula

    # 内部变量管理
    _next_var_id: int = 1

    def _alloc_var(self, name: str) -> int:
        """分配一个新的 CNF 变量"""
        var_id = self._next_var_id
        self._next_var_id += 1
        return var_id
```

### 3.2 近似解计数

```python
# lv00/counting/approx_counter.py

import subprocess
import tempfile
import os
import re
from dataclasses import dataclass
from typing import Optional


@dataclass
class CountResult:
    """近似计数结果"""
    estimated_count: float       # 估计的解数量
    lower_bound: float           # 置信区间下界
    upper_bound: float           # 置信区间上界
    confidence: float            # 置信度 (1-δ)
    epsilon: float               # 近似比 ε
    num_independent_vars: int    # 独立变量数
    elapsed_sec: float           # 耗时


class ApproxMCCounter:
    """
    通过调用外部 ApproxMC 命令行工具来进行近似模型计数。

    工作流程：
    1. 使用 GeometricCNFEncoder 将约束图编码为 CNF
    2. 写入临时 DIMACS 文件
    3. 调用 ApproxMC 命令行
    4. 解析输出，返回计数结果
    """

    def __init__(self, approxmc_path: str = "approxmc",
                 epsilon: float = 0.8, delta: float = 0.2):
        """
        Args:
            approxmc_path: ApproxMC 可执行文件的路径。
            epsilon: 近似比参数，默认 0.8。
            delta: 置信度参数，默认 0.2（即置信度 80%）。
        """
        self.approxmc_path = approxmc_path
        self.epsilon = epsilon
        self.delta = delta

    def count(self, cnf_formula) -> CountResult:
        """
        对 CNF 公式进行近似模型计数。
        """
        # 1. 将公式写入临时 DIMACS 文件
        with tempfile.NamedTemporaryFile(
            mode='w', suffix='.cnf', delete=False
        ) as f:
            f.write(cnf_formula.to_dimacs())
            cnf_path = f.name

        try:
            # 2. 调用 ApproxMC
            cmd = [
                self.approxmc_path,
                f"--epsilon={self.epsilon}",
                f"--delta={self.delta}",
                cnf_path,
            ]
            result = subprocess.run(
                cmd, capture_output=True, text=True, timeout=300)

            # 3. 解析输出
            count = self._parse_approxmc_output(result.stdout)

        finally:
            os.unlink(cnf_path)

        return count

    def _parse_approxmc_output(self, output: str) -> CountResult:
        """
        解析 ApproxMC 的标准输出。

        示例输出：
        c ApproxMC version 7
        c -------------------------------------------------------
        s mc 1234567
        v 1230000 1300000
        c Total time: 12.345
        """
        estimated = 0
        lower = 0
        upper = 0
        num_indep = 0
        elapsed = 0.0

        for line in output.split("\n"):
            line = line.strip()
            # 解析计数结果
            m = re.match(r"^s\s+mc\s+(\d+)", line)
            if m:
                estimated = int(m.group(1))
            # 解析置信区间
            m = re.match(r"^v\s+(\d+)\s+(\d+)", line)
            if m:
                lower = int(m.group(1))
                upper = int(m.group(2))
            # 解析独立变量数
            m = re.match(r"^c\s+independent support:\s+(\d+)", line)
            if m:
                num_indep = int(m.group(1))
            # 解析耗时
            m = re.match(r"^c\s+Total time:\s+([\d.]+)", line)
            if m:
                elapsed = float(m.group(1))

        return CountResult(
            estimated_count=estimated,
            lower_bound=lower,
            upper_bound=upper,
            confidence=1.0 - self.delta,
            epsilon=self.epsilon,
            num_independent_vars=num_indep,
            elapsed_sec=elapsed,
        )


class GraphSolutionCounter:
    """
    Lv-00 的约束图解计数入口。
    封装了 CNF 编码 + ApproxMC 调用 + 结果解释的完整流程。
    """

    def __init__(self, num_bits: int = 8,
                 epsilon: float = 0.8, delta: float = 0.2):
        self.encoder = GeometricCNFEncoder(num_bits_per_coord=num_bits)
        self.counter = ApproxMCCounter(epsilon=epsilon, delta=delta)

    def graph_solution_count_approx(self, constraint_graph) -> CountResult:
        """
        近似统计约束图的解数量。

        对应 ApproxMC 的 use case：
        encode_to_cnf(graph) -> ApproxMC -> graph_solution_count_approx()
        """
        # 第一步：编码
        cnf = self.encoder.encode(constraint_graph)

        # 第二步：近似计数
        result = self.counter.count(cnf)

        # 第三步：附加解释信息
        result.constraint_graph_complexity = self._assess_complexity(result)

        return result

    def _assess_complexity(self, result: CountResult) -> str:
        """
        根据计数结果评估约束图的复杂度。

        这是比 SAT yes/no 更丰富的约束分析维度。
        """
        if result.estimated_count == 0:
            return "OVER_CONSTRAINED"  # 过度约束：无解
        elif result.estimated_count == 1:
            return "FULLY_CONSTRAINED"  # 完全约束：唯一解
        elif result.estimated_count < 10:
            return "TIGHTLY_CONSTRAINED"  # 紧约束：极少数解
        elif result.estimated_count < 1000:
            return "MODERATELY_CONSTRAINED"  # 适度约束
        elif result.estimated_count < 100000:
            return "LOOSELY_CONSTRAINED"  # 松约束
        else:
            return "UNDER_CONSTRAINED"  # 欠约束：过多解
```

### 3.3 近似-精确混合计数策略

```python
# lv00/counting/hybrid_counter.py

class HybridCounter:
    """
    近似-精确混合计数策略。

    对于小规模约束系统使用精确计数（通过精确 SAT 枚举），
    对于大规模系统回退到 ApproxMC 近似计数。
    """

    EXACT_THRESHOLD = 10000   # 解数量超过此阈值时切换为近似
    EXACT_TIMEOUT_SEC = 30    # 精确计数的超时时间

    def count(self, constraint_graph) -> CountResult:
        """
        混合计数策略：
        1. 先尝试精确计数（适用于小规模系统）
        2. 如果超时或解数量过大，回退到近似计数
        """
        # 评估约束图规模
        num_entities = len(constraint_graph.entities)
        num_constraints = len(constraint_graph.constraints)
        estimated_complexity = num_entities * num_constraints

        if estimated_complexity < 50:
            # 小规模，尝试精确计数
            try:
                exact_result = self._exact_count(constraint_graph)
                if exact_result.estimated_count <= self.EXACT_THRESHOLD:
                    exact_result.method = "EXACT"
                    return exact_result
            except TimeoutError:
                pass  # 回退到近似

        # 回退到近似计数
        approx_counter = GraphSolutionCounter(
            num_bits=8,
            epsilon=0.8,
            delta=0.2,
        )
        result = approx_counter.graph_solution_count_approx(constraint_graph)
        result.method = "APPROXMC"
        return result

    def _exact_count(self, constraint_graph) -> CountResult:
        """
        精确枚举所有解（适用于极小规模的约束系统）。
        """
        # 使用精确方法枚举（如基于回溯的完整搜索）
        # 此处仅展示接口
        pass
```

---

## 四、实现路线图

### 4.1 分阶段实施计划

| 阶段 | 名称 | 目标 | 预计工作量 | 交付物 |
|------|------|------|-----------|--------|
| **阶段 1** | CNF 编码管道 | 实现几何约束系统的 CNF 编码，支持距离/角度/共线约束的 bit-blasting | 3-4 周 | `cnf_encoder.py`、DIMACS 输出、编码正确性测试 |
| **阶段 2** | ApproxMC 集成 | 集成 ApproxMC 命令行工具，实现解析器和计数结果解释 | 2-3 周 | `approx_counter.py`、CountResult 数据结构、复杂度评估 |
| **阶段 3** | 近似-精确混合计数 | 实现混合计数策略，支持小规模精确 + 大规模近似 | 2-3 周 | `hybrid_counter.py`、自动策略选择、性能基准 |

### 4.2 阶段 1 详细任务

1. **离散化方案设计**
   - 确定坐标范围和位宽（初始使用 8 bits → 256 级）。
   - 实现浮点 ↔ 位向量的双向转换。
   - 分析离散化引入的误差：`step = range / 256 ≈ 0.78`（当 range=200 时）。

2. **约束编码实现**
   - 距离约束：枚举所有组合 → 禁止不满足的赋值。
   - 角度约束：使用更高效的比较器电路编码。
   - 共线性/共圆约束：编码为代数关系的 CNF。

3. **DIMACS 输出**
   - 实现标准 DIMACS CNF 格式输出。
   - 验证生成的 CNF 可以被标准 SAT 求解器读取。

### 4.3 阶段 2 详细任务

1. **ApproxMC 安装集成**
   - 编译/安装 ApproxMC 和其依赖（CryptoMiniSat、Arjun）。
   - 编写 Python 包装器。

2. **输出解析器**
   - 解析 ApproxMC 的标准输出。
   - 提取估计值、置信区间、独立变量数。

3. **结果解释**
   - 实现复杂度评估逻辑。
   - 生成人类可读的约束分析报告。

### 4.4 阶段 3 详细任务

1. **精确枚举器**
   - 实现基于 SAT 求解器的全解枚举（all-solutions SAT）。
   - 设置超时和阈值保护。

2. **自动策略选择**
   - 基于约束图规模（实体数 × 约束数）自动选择计数方法。
   - 考虑问题类型：刚性图 vs. 欠约束图。

3. **性能基准**
   - 在标准几何约束基准上测试精确 vs. 近似计数。
   - 分析 ε 和 δ 参数对精度的影响。

---

## 五、附录

### 5.1 关键参考

- **GitHub 仓库**：https://github.com/meelgroup/approxmc
- **核心论文**：Chakraborty, Meel, Vardi, "A Scalable Approximate Model Counter", CP 2013.
- **ApproxMC 7 论文**：Soos, Meel, "Arjun: An Efficient Independent Support Computation Technique and its Applications to Counting and Sampling", ICCAD 2022.
- **模型计数竞赛**：https://mccompetition.org/

### 5.2 术语表

| 术语 | 英文 | 说明 |
|------|------|------|
| #SAT | Sharp-SAT / Model Counting | 计算 SAT 公式的满足赋值数量 |
| PAC | Probably Approximately Correct | 概率近似正确——提供统计保证的近似算法 |
| CNF | Conjunctive Normal Form | 合取范式，子句的合取，每个子句是文字的析取 |
| 独立支持 | Independent Support | 变量集的最小子集，使得其赋值唯一确定整个公式的可满足性 |
| XOR 哈希 | XOR Hashing | 利用随机异或约束将解空间随机划分的技术 |
| Bit-blasting | Bit-blasting | 将高层次约束"炸开"为位级别的布尔约束 |
| ε | epsilon | 近似比参数，控制估计的乘法误差范围 |
| δ | delta | 置信度参数，控制估计失败的概率 |

### 5.3 Lv-00 项目中的相关文件路径

| 模块 | 计划路径 |
|------|----------|
| CNF 编码器 | `lv00/counting/cnf_encoder.py` |
| ApproxMC 集成 | `lv00/counting/approx_counter.py` |
| 混合计数器 | `lv00/counting/hybrid_counter.py` |
| 复杂度评估 | `lv00/counting/complexity.py` |
| 测试 | `tests/counting/test_cnf_encoder.py` |

---

*文档版本：v1.0 | 最后更新：2026-05-24 | 维护者：Lv-00 项目组*
