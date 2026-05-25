# PRISM 概率符号模型检测器——参考与借鉴文档

> 文档类型：外部项目参考分析
> 目标项目：Lv-00 几何约束求解器
> 撰写日期：2026-05-24
> 源项目网站：https://www.prismmodelchecker.org/

---

## 一、项目概述

### 1.1 项目背景

PRISM（Probabilistic Symbolic Model Checker）是由 Birmingham 大学和 Oxford 大学联合开发的概率符号模型检测器，由 Marta Kwiatkowska、Gethin Norman 和 David Parker 等人主导。自 2001 年首次发布以来，PRISM 已成为概率形式化验证领域最广泛使用的工具之一，累计被引用超过 10,000 次。

PRISM 的核心能力是对具有概率行为（随机性）的系统进行形式化建模和自动验证。它支持多种概率模型，包括：

- **DTMC**（Discrete-Time Markov Chain，离散时间马尔可夫链）：所有状态迁移是概率性的，没有非确定性。
- **MDP**（Markov Decision Process，马尔可夫决策过程）：同时包含概率性迁移和非确定性选择。
- **CTMC**（Continuous-Time Markov Chain，连续时间马尔可夫链）：迁移速率由指数分布控制。
- **PTA**（Probabilistic Timed Automata，概率时间自动机）：结合概率和时钟约束。
- **PEPA**（Performance Evaluation Process Algebra，性能评估进程代数）：随机进程代数模型。

实现语言为 Java，采用 GPL 许可。PRISM 提供命令行界面和图形用户界面（GUI）两种使用方式。

### 1.2 技术架构

```
PRISM 模型文件 (.pm / .prism)
    ↓
解析器 (parser) → 生成内部模型表示
    ↓
模型构建器 (model builder)
    ├── 显式引擎 (explicit)：直接构建稀疏矩阵表示
    ├── MTBDD 引擎 (symbolic)：使用多终端二叉决策图压缩状态空间
    └── 混合引擎 (hybrid)：结合显式和符号化方法
    ↓
模型检测引擎 (model checker)
    ├── PCTL / CSL 属性验证
    ├── 可达性分析（到达目标状态的概率）
    ├── 期望奖励分析（expected rewards）
    └── 稳态分析（steady-state probabilities）
    ↓
结果输出（概率值、反例生成、可视化）
```

### 1.3 核心工作流程

1. **建模阶段**：用户使用 PRISM 语言（一种基于 guarded command 的建模语言）描述系统。每个模块包含一组变量和一组命令（guarded commands），格式为 `[action] guard -> prob1 : update1 + prob2 : update2 + ...`。
2. **解析阶段**：PRISM 解析模型文件，生成内部模型表示（DTMC/MDP/CTMC 等）。
3. **构建阶段**：根据选择的引擎（显式/符号化/混合）构建模型的迁移矩阵。
4. **验证阶段**：用户指定 PCTL（概率计算树逻辑）或 CSL（连续随机逻辑）属性，PRISM 通过数值迭代（如 value iteration、Gauss-Seidel）或策略迭代计算概率值。
5. **输出阶段**：返回概率值、反例路径、或可视化结果。

PRISM 语言示例（一个简单的随机行走模型）：

```prism
dtmc

module walk
    x : [0..10] init 5;

    [] x > 0 & x < 10 -> 0.5 : (x' = x - 1) + 0.5 : (x' = x + 1);
    [] x = 0 -> 1.0 : (x' = x);
    [] x = 10 -> 1.0 : (x' = x);
endmodule

// PCTL 属性：从初始状态出发，最终到达 x=10 的概率是多少？
// P=? [ F x=10 ]
```

---

## 二、核心借鉴点

### 2.1 概率变迁系统与几何不确定性建模

PRISM 的概率变迁系统定义了一个从状态到状态分布的函数。在 Lv-00 的语境中，几何约束往往不是精确的，而是带有测量误差或公差范围。例如，工程制图中常见的标注"点 A 到点 B 的距离为 3.0 ± 0.1 mm"——这本质上是一个几何约束的"概率化"表示。

Lv-00 可以借鉴 PRISM 的建模思路，引入"概率几何约束"（probabilistic geometric constraint）概念：

- 传统约束：`distance(A, B) == 3.0`（精确等式）
- 概率约束：`distance(A, B) ~ Normal(3.0, 0.05²)`（服从正态分布）
- 区间约束：`distance(A, B) ∈ [2.9, 3.1]`（精确区间）
- 概率区间约束：`P(distance(A, B) ∈ [2.9, 3.1]) > 0.95`（概率保证的区间约束）

这样一来，一个几何约束系统就变成了一个"概率约束图"（probabilistic constraint graph），其中的每个节点表示一个几何实体的可能位置分布，每条边表示一个带有不确定性的几何关系。

### 2.2 PCTL 与概率几何谓词

PCTL（Probabilistic Computation Tree Logic）是 CTL（计算树逻辑）的概率扩展。其基本语法为：

```
PCTL 状态公式：Φ ::= true | a | ¬Φ | Φ ∧ Φ | P~p [ψ]
PCTL 路径公式：ψ ::= X Φ | Φ U Φ | Φ U≤k Φ
```

其中 `P~p [ψ]` 表示"满足路径公式 ψ 的概率满足关系 ~p"（~ 可以是 ≥、>、<、≤）。

借鉴 PCTL 的设计，Lv-00 可以定义一套"概率几何谓词语言"（Probabilistic Geometric Predicate Language, PGPL）：

```
PGPL 语法（草案）：
    geom_prob_query ::= P~p [ geom_path_formula ]
    geom_path_formula ::=
        | F state_formula        // "最终（Eventually）"
        | G state_formula        // "始终（Globally）"
        | state_formula U state_formula  // "直到（Until）"
    state_formula ::=
        | collinear(A, B, C)     // 三点共线
        | concyclic(A, B, C, D)  // 四点共圆
        | parallel(L1, L2)       // 两线平行
        | perpendicular(L1, L2)  // 两线垂直
        | distance(A, B) < d     // 距离小于某值
        | angle(A, B, C) > θ     // 角度大于某值
        | ¬state_formula
        | state_formula ∧ state_formula
```

示例查询：

```
// "以大于 0.95 的概率，点 A、B、C 在当前公差范围内共线"
P>=0.95 [ F collinear_within_tolerance(A, B, C, 1e-6) ]

// "以大于 0.99 的概率，L1 始终不与 L2 相交"
P>=0.99 [ G ¬intersects(L1, L2) ]

// "点 A 到达目标位置之前，始终满足安全距离约束"
P>=0.90 [ distance(A, obstacle) > safety_margin U distance(A, target) < ε ]
```

### 2.3 MTBDD 符号化状态压缩

PRISM 最强大的特性之一是其 MTBDD（Multi-Terminal Binary Decision Diagram）引擎，能够符号化地表示和操作极大的概率状态空间。MTBDD 是 BDD（二叉决策图）的扩展，允许终端节点为任意数值（而不仅仅是 0 和 1），因此可以表示概率迁移矩阵。

MTBDD 的核心优势：

- **压缩共享子结构**：如果 100 万个状态有相同的迁移概率模式，BDD 可以将其表示为一个共享子图。
- **符号化矩阵-向量乘法**：在求解概率可达性时，不需要显式展开整个迁移矩阵，而是通过 BDD 操作递归计算。
- **变量序优化**：通过动态变量重排序（如 sifting 算法）最小化 BDD 节点数。

对于 Lv-00，MTBDD 提供了一种可能的"大规模概率约束图"存储和计算方案。当约束图中的几何实体数量达到数千甚至数万时（例如，一个复杂装配体的所有零件都有公差标注），显式地展开所有可能的位置组合将面临组合爆炸。MTBDD 的符号化表示可以：

1. 将每个几何实体的位置离散化为有限个可能值（通过量化公差范围）。
2. 用 BDD/MTBDD 表示所有可能的约束满足状态。
3. 在 MTBDD 上计算"约束满足概率"。

### 2.4 对照表：PRISM 概念与 Lv-00 对应关系

| PRISM 概念 | Lv-00 对应 | 借鉴价值 |
|-----------|-----------|----------|
| 概率模型 (DTMC/MDP/CTMC) | 概率几何约束图 (Probabilistic Constraint Graph) | 将带公差的几何约束形式化为概率模型 |
| 状态 (state) | 几何构型（所有几何实体的一种可能位置组合） | 每个状态对应一种可能的装配/定位方案 |
| 概率迁移 (transition) | 几何约束的不确定性传播（如公差链的累积） | 从当前构型出发，因公差导致的位置偏差分布 |
| PCTL 属性 | 概率几何谓词 (PGPL) | 声明式的概率几何查询语言 |
| MTBDD 引擎 | 约束图符号化后端 | 大规模约束系统的压缩存储与高效求解 |
| Value Iteration | 约束传播迭代 | 通过迭代计算满足概率的收敛值 |
| 反例生成 | 公差敏感度分析 | 找到导致约束失败的最敏感公差链 |
| Guarded Commands | 约束触发条件 | 条件式的约束激活（某些约束仅在特定条件下生效） |
| 奖励 (rewards) | 约束满足的"代价值" | 将约束违规映射为惩罚代价，支持最优公差分配 |

---

## 三、Lv-00 映射方案

### 3.1 概率约束节点类型定义

在 Lv-00 中引入概率约束节点类型：

```python
# lv00/constraints/prob_constraint.py

from dataclasses import dataclass, field
from enum import Enum
from typing import Optional, Union, List, Callable
import numpy as np

class ProbDistType(Enum):
    """概率分布类型"""
    NORMAL = "normal"           # 正态分布 N(μ, σ²)
    UNIFORM = "uniform"         # 均匀分布 U(a, b)
    TRIANGULAR = "triangular"   # 三角分布 Tri(a, b, c)
    TRUNCATED_NORMAL = "truncated_normal"  # 截断正态分布

@dataclass
class ProbConstraint:
    """
    概率几何约束节点。

    表示一个带有不确定性的几何关系。例如：
    - 距离约束：distance(A, B) ∼ Normal(3.0, 0.05²)
    - 角度约束：angle(A, B, C) ∼ Uniform(89.5°, 90.5°)
    """
    constraint_id: str
    constraint_type: str     # "distance" | "angle" | "collinearity" | "parallelism" ...
    entities: List[str]       # 涉及的几何实体 ID 列表
    nominal_value: float      # 名义值（分布的均值/中心）
    dist_type: ProbDistType   # 分布类型
    dist_params: dict         # 分布参数，如 {"mu": 3.0, "sigma": 0.05}
    confidence_level: float = 0.95  # 置信水平

    def sample(self, n_samples: int = 1) -> np.ndarray:
        """
        从约束的不确定性分布中采样。
        返回 n_samples 个约束值的样本。
        """
        if self.dist_type == ProbDistType.NORMAL:
            mu = self.dist_params["mu"]
            sigma = self.dist_params["sigma"]
            return np.random.normal(mu, sigma, n_samples)
        elif self.dist_type == ProbDistType.UNIFORM:
            a = self.dist_params["a"]
            b = self.dist_params["b"]
            return np.random.uniform(a, b, n_samples)
        elif self.dist_type == ProbDistType.TRIANGULAR:
            a = self.dist_params["a"]
            b = self.dist_params["b"]
            c = self.dist_params["c"]
            return np.random.triangular(a, c, b, n_samples)
        elif self.dist_type == ProbDistType.TRUNCATED_NORMAL:
            mu = self.dist_params["mu"]
            sigma = self.dist_params["sigma"]
            lo = self.dist_params.get("lo", mu - 3 * sigma)
            hi = self.dist_params.get("hi", mu + 3 * sigma)
            from scipy.stats import truncnorm
            a_norm = (lo - mu) / sigma
            b_norm = (hi - mu) / sigma
            return truncnorm.rvs(a_norm, b_norm, loc=mu, scale=sigma, size=n_samples)
        else:
            raise ValueError(f"Unknown distribution type: {self.dist_type}")

    def to_interval(self) -> tuple:
        """将概率约束转换为置信区间"""
        if self.dist_type == ProbDistType.NORMAL:
            mu = self.dist_params["mu"]
            sigma = self.dist_params["sigma"]
            # 95% 置信区间: μ ± 1.96σ
            z = 1.96
            return (mu - z * sigma, mu + z * sigma)
        elif self.dist_type == ProbDistType.UNIFORM:
            return (self.dist_params["a"], self.dist_params["b"])
        else:
            # 使用采样估计置信区间
            samples = self.sample(10000)
            alpha = (1 - self.confidence_level) / 2
            return (np.percentile(samples, 100 * alpha),
                    np.percentile(samples, 100 * (1 - alpha)))

@dataclass
class ProbConstraintGraph:
    """
    概率约束图：包含带不确定性的几何约束集合。
    借鉴 PRISM 的概率模型概念进行设计。
    """
    nodes: dict = field(default_factory=dict)    # {entity_id: ProbGeomEntity}
    constraints: List[ProbConstraint] = field(default_factory=list)

    def build_state_space(self, discretization_steps: int = 100):
        """
        构建离散化状态空间。
        将每个几何实体的连续位置离散化为有限个可能值，
        类似于 PRISM 中将连续时间离散化为 DTMC 状态。
        """
        state_variables = []
        for entity_id, entity in self.nodes.items():
            # 对每个实体的位置（x, y, z）按照概率约束进行离散化
            x_range = entity.get_x_range(self.constraints)
            y_range = entity.get_y_range(self.constraints)
            z_range = entity.get_z_range(self.constraints)

            # 在范围内均匀离散化
            x_vals = np.linspace(x_range[0], x_range[1], discretization_steps)
            y_vals = np.linspace(y_range[0], y_range[1], discretization_steps)
            z_vals = np.linspace(z_range[0], z_range[1], discretization_steps)

            state_variables.append({
                "entity_id": entity_id,
                "x_vals": x_vals,
                "y_vals": y_vals,
                "z_vals": z_vals,
            })
        return state_variables

    def compute_sat_probability(self, target_constraint: ProbConstraint,
                                 n_monte_carlo: int = 10000) -> float:
        """
        蒙特卡洛法计算目标约束的满足概率。
        对应 PRISM 中的 P=? [ F target ] 类查询。
        """
        satisfied_count = 0
        for _ in range(n_monte_carlo):
            # 为每个几何实体采样一个位置
            entity_positions = {}
            for entity_id, entity in self.nodes.items():
                entity_positions[entity_id] = entity.sample_position(
                    self.constraints)

            # 检查目标约束是否满足
            if target_constraint.check(entity_positions):
                satisfied_count += 1

        return satisfied_count / n_monte_carlo
```

### 3.2 PCTL 样式的概率几何查询

```python
# lv00/constraints/prob_geom_query.py

from enum import Enum
from typing import List, Callable, Optional
import math

class PathQuantifier(Enum):
    """路径量词 — 对应 PCTL 的路径公式"""
    EVENTUALLY = "F"   # 最终（Future / Eventually）
    GLOBALLY = "G"     # 始终（Globally）
    UNTIL = "U"        # 直到（Until）

class ProbRel(Enum):
    """概率关系"""
    GEQ = ">="
    LEQ = "<="
    GT = ">"
    LT = "<"
    EQ = "="

class ProbGeomQuery:
    """
    概率几何查询 — 借鉴 PCTL 设计。
    示例: "以 >0.95 概率，A, B, C 最终共线"
    """

    def __init__(self,
                 prob_rel: ProbRel,
                 prob_bound: float,
                 path_quantifier: PathQuantifier,
                 state_formula: Callable[..., bool],
                 entities: List[str],
                 until_formula: Optional[Callable[..., bool]] = None):
        self.prob_rel = prob_rel
        self.prob_bound = prob_bound
        self.path_quantifier = path_quantifier
        self.state_formula = state_formula
        self.entities = entities
        self.until_formula = until_formula

    def __repr__(self):
        formula_str = f"P{self.prob_rel.value}{self.prob_bound} ["
        if self.path_quantifier == PathQuantifier.UNTIL:
            formula_str += f" {self.state_formula.__name__} U {self.until_formula.__name__} ]"
        else:
            formula_str += f" {self.path_quantifier.value} {self.state_formula.__name__} ]"
        formula_str += f" on {self.entities}"
        return formula_str


# 预定义的几何状态公式

def collinear_check(pts: dict, entities: List[str], tolerance: float = 1e-9) -> bool:
    """检查三点是否共线"""
    a, b, c = entities
    pa, pb, pc = pts[a], pts[b], pts[c]
    cross = abs((pb[0] - pa[0]) * (pc[1] - pa[1]) -
                (pb[1] - pa[1]) * (pc[0] - pa[0]))
    return cross < tolerance

def concyclic_check(pts: dict, entities: List[str], tolerance: float = 1e-9) -> bool:
    """检查四点是否共圆（使用 Ptolemy 定理检验）"""
    a, b, c, d = entities
    pa, pb, pc, pd = pts[a], pts[b], pts[c], pts[d]

    def dist(p1, p2):
        return math.hypot(p1[0] - p2[0], p1[1] - p2[1])

    ab, bc, cd, da = dist(pa, pb), dist(pb, pc), dist(pc, pd), dist(pd, pa)
    ac, bd = dist(pa, pc), dist(pb, pd)
    # Ptolemy: AC * BD = AB * CD + BC * AD (对共圆四点)
    lhs = ac * bd
    rhs = ab * cd + bc * da
    return abs(lhs - rhs) / max(rhs, 1e-10) < tolerance

def parallel_check(pts: dict, entities: List[str], tolerance: float = 1e-9) -> bool:
    """检查两线是否平行"""
    l1, l2 = entities
    # 假设 entities 是 [[a1, a2], [b1, b2]] 格式
    a1, a2 = pts[l1[0]], pts[l1[1]]
    b1, b2 = pts[l2[0]], pts[l2[1]]

    v1 = (a2[0] - a1[0], a2[1] - a1[1])
    v2 = (b2[0] - b1[0], b2[1] - b1[1])
    cross = abs(v1[0] * v2[1] - v1[1] * v2[0])
    # 规范化后比较
    len1 = math.hypot(v1[0], v1[1])
    len2 = math.hypot(v2[0], v2[1])
    if len1 < 1e-10 or len2 < 1e-10:
        return False
    return cross / (len1 * len2) < tolerance


class ProbGeomQueryEvaluator:
    """
    概率几何查询评估器。
    使用蒙特卡洛模拟评估 PCTL 风格的查询。
    """

    def __init__(self, constraint_graph, n_simulations: int = 10000):
        self.graph = constraint_graph
        self.n_simulations = n_simulations

    def evaluate(self, query: ProbGeomQuery) -> dict:
        """
        评估概率几何查询，返回满足概率及相关统计信息。
        """
        satisfied = 0
        path_lengths = []
        total_sims = 0

        for sim in range(self.n_simulations):
            # 构建一次随机采样下的几何构型
            config = self.graph.sample_configuration()
            total_sims += 1

            if query.path_quantifier == PathQuantifier.EVENTUALLY:
                # "最终"语义：在多次迭代中检查是否有一刻满足
                satisfied_in_sim = self._simulate_until_hit(
                    config, query.state_formula, query.entities)
                if satisfied_in_sim:
                    satisfied += 1

            elif query.path_quantifier == PathQuantifier.GLOBALLY:
                # "始终"语义：在整个模拟路径中是否一直满足
                satisfied_in_sim = self._simulate_globally(
                    config, query.state_formula, query.entities)
                if satisfied_in_sim:
                    satisfied += 1

            elif query.path_quantifier == PathQuantifier.UNTIL:
                # "直到"语义
                satisfied_in_sim, steps = self._simulate_until(
                    config,
                    query.state_formula,
                    query.until_formula,
                    query.entities)
                if satisfied_in_sim:
                    satisfied += 1
                    path_lengths.append(steps)

        # 计算满足概率
        prob = satisfied / total_sims if total_sims > 0 else 0.0

        # 判断查询结果
        result = self._check_prob_bound(prob, query.prob_rel, query.prob_bound)

        return {
            "query": repr(query),
            "estimated_probability": prob,
            "bound": (query.prob_rel.value, query.prob_bound),
            "result": result,  # True = 查询成立
            "num_simulations": total_sims,
            "num_satisfied": satisfied,
            "confidence_interval": self._binomial_ci(prob, total_sims),
            "avg_path_length": (sum(path_lengths) / len(path_lengths)
                                if path_lengths else None),
        }

    def _simulate_until_hit(self, config, state_formula, entities):
        """模拟直到状态公式满足（最多 100 步）"""
        current = config
        for _ in range(100):
            if state_formula(current, entities):
                return True
            current = self.graph.step(current)
        return False

    def _simulate_globally(self, config, state_formula, entities):
        """模拟是否总是满足状态公式（最多 100 步）"""
        current = config
        for _ in range(100):
            if not state_formula(current, entities):
                return False
            current = self.graph.step(current)
        return True

    def _simulate_until(self, config, before_formula, after_formula, entities):
        """模拟 until 语义"""
        current = config
        for step in range(100):
            if after_formula(current, entities):
                return True, step
            if not before_formula(current, entities):
                return False, step
            current = self.graph.step(current)
        return False, 100

    def _check_prob_bound(self, prob, rel, bound):
        """检查概率是否满足关系"""
        if rel == ProbRel.GEQ:
            return prob >= bound
        elif rel == ProbRel.LEQ:
            return prob <= bound
        elif rel == ProbRel.GT:
            return prob > bound
        elif rel == ProbRel.LT:
            return prob < bound
        elif rel == ProbRel.EQ:
            return abs(prob - bound) < 1e-6
        return False

    @staticmethod
    def _binomial_ci(p, n, z=1.96):
        """二项式比例的 Wald 置信区间"""
        if n == 0:
            return (0, 0)
        se = math.sqrt(p * (1 - p) / n)
        return (max(0, p - z * se), min(1, p + z * se))
```

### 3.3 MTBDD 符号化后端（概念设计）

```python
# lv00/constraints/symbolic/mtbdd_backend.py

class MTBDDNode:
    """
    多终端二叉决策图 (MTBDD) 节点。
    用于符号化表示概率约束的状态空间。

    这是一个概念原型。在生产环境中，
    建议通过 pybind11 绑定 CUDD 的 ADD（代数决策图）来获取性能。
    """

    def __init__(self, var_index: int = -1, low=None, high=None,
                 value: float = None):
        self.var_index = var_index
        self.low = low
        self.high = high
        self.value = value

    def is_terminal(self) -> bool:
        return self.var_index == -1

    def evaluate(self, assignment: dict) -> float:
        """递归计算给定变量赋值下的 MTBDD 值"""
        if self.is_terminal():
            return self.value
        bit = assignment.get(self.var_index, 0)
        if bit == 0:
            return self.low.evaluate(assignment)
        else:
            return self.high.evaluate(assignment)


class SymbolicConstraintEncoder:
    """
    符号化约束编码器。
    将连续几何域上的约束满足问题编码为 MTBDD。
    """

    def __init__(self, num_bits_per_var: int = 8):
        """
        Args:
            num_bits_per_var: 每个变量的位宽。
            例如 8 bits 意味着将连续变量离散化为 2^8 = 256 个可能值。
        """
        self.num_bits = num_bits_per_var

    def encode_distance_constraint(self, var_a_x, var_a_y,
                                    var_b_x, var_b_y,
                                    target: float, tolerance: float) -> MTBDDNode:
        """
        将距离约束编码为 MTBDD。
        例如：|distance(A, B) - target| < tolerance
        返回一个 MTBDD，对满足约束的赋值求值为 1.0，否则为 0.0。
        """
        # 概念实现：构建 MTBDD 表示
        # 实际实现需要：bit-blasting → 构建比较器电路 → BDD 编码
        # 此处展示概念
        pass

    def encode_angle_constraint(self, var_a_x, var_a_y, var_b_x, var_b_y,
                                 var_c_x, var_c_y,
                                 target: float, tolerance: float) -> MTBDDNode:
        """
        将角度约束编码为 MTBDD。
        例如：|angle(ABC) - target| < tolerance
        """
        pass

    def build_sat_mtbdd(self, constraint_graph) -> MTBDDNode:
        """
        构建约束满足的 MTBDD。
        将所有约束的 MTBDD 做合取（AND），得到满足所有约束的赋值集合。
        """
        # 对所有约束的 MTBDD 做 AND 操作
        result = None
        for constraint in constraint_graph.constraints:
            constraint_bdd = self.encode_constraint(constraint)
            if result is None:
                result = constraint_bdd
            else:
                result = self.mtbdd_and(result, constraint_bdd)
        return result

    def count_sat_assignments(self, mtbdd: MTBDDNode) -> float:
        """
        统计满足约束的赋值数量。
        通过遍历 MTBDD 计算所有路径上的"概率质量"之和。
        """
        pass

    @staticmethod
    def mtbdd_and(a: MTBDDNode, b: MTBDDNode) -> MTBDDNode:
        """MTBDD 合取操作（概念实现）"""
        if a.is_terminal() and b.is_terminal():
            return MTBDDNode(value=min(a.value, b.value))  # 用 min 模拟 AND
        # 实际实现需要 Apply 算法的递归处理
        pass
```

---

## 四、实现路线图

### 4.1 分阶段实施计划

| 阶段 | 名称 | 目标 | 预计工作量 | 交付物 |
|------|------|------|-----------|--------|
| **阶段 1** | 概率约束类型定义 | 定义 ProbConstraint 和 ProbConstraintGraph 数据结构，支持正态/均匀/三角分布 | 2-3 周 | `prob_constraint.py`、概率分布采样器、置信区间计算 |
| **阶段 2** | PCTL 几何谓词实现 | 实现概率几何查询语言（PGPL）的解析器和评估器 | 3-4 周 | `prob_geom_query.py`、蒙特卡洛评估器、查询解析器 |
| **阶段 3** | MTBDD 符号化后端 | 实现基于 MTBDD 的大规模状态空间压缩与约束满足概率计算 | 4-6 周 | `mtbdd_backend.py`、CUDD Python 绑定、符号化编码器 |

### 4.2 阶段 1 详细任务

1. **概率约束数据结构**
   - 定义 `ProbDistType` 枚举和 `ProbConstraint` 数据类。
   - 支持正态分布、均匀分布、三角分布、截断正态分布。
   - 实现从约束到置信区间的转换函数。

2. **概率约束图**
   - 定义 `ProbConstraintGraph` 类。
   - 实现状态空间离散化（用于与 MTBDD 后端交互）。
   - 实现蒙特卡洛采样评估。

3. **基础测试**
   - 单元测试：各种分布的采样正确性。
   - 集成测试：在简单二实体系统上的概率计算。

### 4.3 阶段 2 详细任务

1. **PGPL 语法定义**
   - 定义概率几何查询的语法规则。
   - 实现 ANTLR 或手写解析器。

2. **几何状态公式库**
   - 实现共线、共圆、平行、垂直等基础公式。
   - 支持自定义公式注册。

3. **蒙特卡洛评估器**
   - 实现 `ProbGeomQueryEvaluator` 类。
   - 支持 EVENTUALLY、GLOBALLY、UNTIL 三种路径量词。
   - 输出置信区间和路径统计。

### 4.4 阶段 3 详细任务

1. **CUDD Python 绑定**
   - 通过 pybind11 或 SWIG 绑定 CUDD 库的 ADD 操作。
   - 实现基本的 ADD 操作：create, apply (AND/OR/MAX), 变量重排序。

2. **约束编码器**
   - 实现 bit-blasting：将浮点坐标编码为定点位向量。
   - 构建距离/角度比较器的 BDD 编码。
   - 实现约束合取（AND）的符号化操作。

3. **性能评估**
   - 对比显式方法和符号化方法的性能。
   - 测试变量序优化对性能的影响。

---

## 五、附录

### 5.1 关键参考

- **项目网站**：https://www.prismmodelchecker.org/
- **PRISM 手册**：https://www.prismmodelchecker.org/manual/
- **核心论文**：Kwiatkowska, Norman, Parker, "PRISM: Probabilistic Symbolic Model Checker", TOOLS 2002.
- **PRISM 案例分析**：https://www.prismmodelchecker.org/casestudies/

### 5.2 术语表

| 术语 | 英文 | 说明 |
|------|------|------|
| DTMC | Discrete-Time Markov Chain | 离散时间马尔可夫链，所有迁移是概率性的 |
| MDP | Markov Decision Process | 马尔可夫决策过程，包含非确定性选择 |
| CTMC | Continuous-Time Markov Chain | 连续时间马尔可夫链，迁移速率由指数分布控制 |
| MTBDD | Multi-Terminal BDD | 多终端二叉决策图，终端节点为任意数值 |
| PCTL | Probabilistic CTL | 概率计算树逻辑，CTL 的概率扩展 |
| CSL | Continuous Stochastic Logic | 连续随机逻辑，CTMC 的时序逻辑 |
| PGPL | Probabilistic Geometric Predicate Language | 概率几何谓词语言（Lv-00 新定义） |
| Guarded Command | Guarded Command | PRISM 语言中的条件动作命令 |

### 5.3 Lv-00 项目中的相关文件路径

| 模块 | 计划路径 |
|------|----------|
| 概率约束类型 | `lv00/constraints/prob_constraint.py` |
| 概率几何查询 | `lv00/constraints/prob_geom_query.py` |
| MTBDD 后端 | `lv00/constraints/symbolic/mtbdd_backend.py` |
| 几何状态公式库 | `lv00/constraints/geom_formulas.py` |
| 测试 | `tests/constraints/test_prob_constraint.py` |

---

*文档版本：v1.0 | 最后更新：2026-05-24 | 维护者：Lv-00 项目组*
