# Herbie 浮点精度自动改进工具——参考与借鉴文档

> 文档类型：外部项目参考分析
> 目标项目：Lv-00 几何约束求解器
> 撰写日期：2026-05-24
> 源项目网站：https://herbie.uwplse.org/

---

## 一、项目概述

### 1.1 项目背景

Herbie 是由 Washington 大学 PLSE（Programming Languages and Software Engineering）实验室开发的一款自动化浮点精度改进工具。该项目由 Pavel Panchekha 主导，Zachary Tatlock 教授指导，自 2014 年起持续维护至今，已发展到 v2.0 以上版本。

浮点运算的一个基本困境是：数学上等价的表达式，在 IEEE 754 浮点标准下可能产生截然不同的数值误差。例如，二次方程求根公式 `(-b ± sqrt(b² - 4ac)) / (2a)` 在判别式接近零时，会因为"灾难性抵消"（catastrophic cancellation）而产生严重误差。Herbie 正是为解决这一类问题而设计的——它能自动检测数值不稳定的表达式，并通过搜索重写规则找到更稳定的等价形式。

### 1.2 技术架构

Herbie 的整体架构如下：

```
用户输入表达式 → 解析（FPCore 格式）
    → 随机采样（在不同输入域上采样，检测浮点误差）
    → 重写搜索（应用预定义和自动发现的代数规则，生成候选等价表达式）
    → 局部误差评估（每个采样点上的相对误差 / 绝对误差）
    → 全局误差评估（汇总所有采样点的误差分布）
    → Pareto 最优选择（精度-速度的 Pareto 前沿）
    → 输出精确表达式 + 精度改进报告
```

实现语言为 Racket（一种 Scheme 方言），利用了 Racket 生态中强大的符号计算和模式匹配能力。核心模块包括：

| 模块 | 职责 |
|------|------|
| `core/` | 表达式解析、表示、求值引擎 |
| `syntax/` | 代数重写规则的定义和应用 |
| `float/` | 高精度浮点运算（基于 MPFR）以计算"真实值" |
| `points/` | 采样策略，包括均匀采样、对数采样、边界感知采样 |
| `regimes/` | 输入域划分（将输入空间划分为不同"制度"，分别分析） |
| `pareto/` | Pareto 前沿计算，多目标优化（精度 vs. 速度） |
| `reports/` | 生成精度改进的 HTML/文本报告 |

### 1.3 核心工作流程

Herbie 收到一个浮点表达式后，执行以下步骤：

1. **解析阶段**：将输入表达式转换为内部 AST（抽象语法树）。
2. **采样检测阶段**：在输入域上生成大量随机采样点（默认 256 个），同时在每个采样点上用高精度算术（MPFR，默认 80 位精度）计算"基准真实值"，再用标准 IEEE 754 双精度计算"近似值"。比较两者得出每个采样点的位误差（bits of error）。
3. **域划分阶段**：如果采样点之间的误差分布不均匀，使用 k-means 聚类或等深度分箱将输入域划分为多个"制度"（regimes）。不同制度可能对应不同的最优重写。
4. **重写搜索阶段**：在每个制度内，对原始表达式应用一系列代数重写规则（结合律重组、分配律展开/收缩、多项式因式分解、分数通分/约分、三角函数恒等式等），生成大量候选等价表达式。
5. **误差评估阶段**：对每个候选表达式重新采样并计算误差分布，生成"平均误差"和"最大误差"两个关键指标。
6. **Pareto 筛选阶段**：在所有候选中，找出精度最高和速度最快的 Pareto 前沿解。速度通过操作数数量来近似。
7. **输出阶段**：为每个制度生成条件分支的改进表达式（即分段函数），并附带精度改进的可视化报告。

---

## 二、核心借鉴点

### 2.1 随机采样驱动的误差检测

Herbie 的核心洞察之一是：数值不稳定并非均匀分布在整个输入域上，而是集中在某些"危险区域"（例如除零附近、相消点附近、函数拐点附近）。因此，均匀采样是不够的——需要主动探测这些危险区域。

Herbie 的采样策略包括以下几个层面：

- **均匀采样**：覆盖整个输入域的基线采样。
- **边界采样**：特别关注输入域的边界点、间断点、奇点附近。
- **自适应采样**：在误差较大的区域增加采样密度，逐步逼近真正的"最差点"。
- **对数值采样**：对于数量级跨度大的输入域，使用对数均匀采样，避免只在数值大的区域采样。

对应到 Lv-00 项目，这一方法可以启发我们设计几何约束的"脆弱性采样器"：

```
# Lv-00 借鉴思路：几何约束脆弱性采样
def lv00_fragility_sampler(constraint_graph, num_samples=256):
    """
    在约束图上采样，检测哪些区域对浮点误差敏感。
    重点关注：
    1. 退化构型附近（三点共线、四点共圆、平行线等）
    2. 判别式接近零时（如二次方程的双根情况）
    3. 极小夹角处（夹角 → 0° 或 180° 的三角形）
    """
    samples = []
    # 1. 均匀采样
    samples += uniform_sampl(constraint_graph, num_samples // 2)
    # 2. 退化附近的扰动采样
    degenerate_configs = find_degenerate_neighbors(constraint_graph)
    samples += perturb_around(degenerate_configs, num_samples // 4)
    # 3. 奇点附近的自适应采样
    singular_regions = detect_singularities(constraint_graph)
    samples += adaptive_sample(singular_regions, num_samples // 4)
    return samples
```

### 2.2 Pareto 最优重写搜索

Herbie 不仅仅寻找"精度最高"的表达式，而是在精度和速度之间寻找 Pareto 前沿。这一思路对于 Lv-00 的数值路径优化有直接参考价值。

在 Lv-00 中，约束求解的数值计算路径有多种选择（例如，是用消元法还是迭代法解线性方程组、是用解析求根还是 Newton-Raphson 迭代）。不同的数值路径在"解的质量"和"计算耗时"之间存在 trade-off。借鉴 Herbie 的 Pareto 搜索思想，Lv-00 可以：

1. 为每个约束类型注册多个可选的数值求解路径。
2. 对每个路径计算"可信度分数 TrustColor"（精度维度）和"操作计数"（速度维度）。
3. 在 Pareto 前沿上选择最优路径。

### 2.3 局部误差与全局误差的双层评估

Herbie 的误差评估包含两个层次：

- **局部误差**：每个采样点上的位误差（bits of error），公式为 `bits_error = |log₂(approx/exact)|`。
- **全局误差**：汇总所有采样点的误差，输出平均位误差、最大位误差、误差分布直方图。

双层评估的优点是：局部误差可以帮助定位问题表达式中的具体"弱点"；全局误差则提供整体精度保证。对应到 Lv-00：

- **局部 AMBER 评分**：对约束图中每个节点/边的 TrustColor AMBER 评级。
- **全局 AMBER 评分**：整个约束图的整体可信度。

### 2.4 表达式重写规则库

Herbie 内置了一个丰富的代数重写规则库，部分规则如下：

| 规则类别 | 示例 |
|----------|------|
| 结合律重组 | `(a+b)+c → a+(b+c)` —— 不同分组可能影响浮点舍入 |
| 提前除零避免 | `1/(x+ε)-1/x → -ε/(x·(x+ε))` —— 避免两个接近量相减 |
| 因式分解 | `x²-y² → (x+y)(x-y)` —— 避免大数相减 |
| 平方根有理化 | `sqrt(x+1)-sqrt(x) → 1/(sqrt(x+1)+sqrt(x))` |
| 三角函数恒等式 | `1-cos(x) → 2·sin²(x/2)` —— 避免小角度的减性抵消 |
| 多项式Horner化 | `ax³+bx²+cx+d → ((a·x+b)·x+c)·x+d` —— 减少乘法次数 |
| 对数性质 | `log(a·b) → log(a)+log(b)` —— 避免大数乘积溢出 |

Lv-00 可借鉴的几何表达式重写规则：

```
几何化简规则库：
- 点距公式稳定性：sqrt((x1-x2)²+(y1-y2)²) → hypot(x1-x2, y1-y2)
- 夹角计算稳定性：acos(dot(a,b)/(|a|·|b|)) → 2·atan2(|a×b|, |a|·|b|+dot(a,b))
- 面积公式：0.5·|cross(v1,v2)| → 避免除以2后用abs
- 共线性检测：cross(p2-p1, p3-p1) ≈ 0 → 标准化后用相对阈值
```

### 2.5 对照表：Herbie 概念与 Lv-00 对应关系

| Herbie 概念 | Lv-00 对应 | 借鉴价值 |
|-------------|-----------|----------|
| 浮点表达式 | 几何约束的数值求解路径 | 为每条数值路径建立精度评估模型 |
| FPCore 格式 | Lv-00 的 Constraint IR（中间表示） | 统一的约束表达式表示格式 |
| 随机采样 | 约束图脆弱性采样 | 在退化附近密集采样，检测 AMBER 降级风险 |
| 高精度基准值 (MPFR) | 高精度几何计算（如 Boost.Multiprecision） | 获取约束解的"正确基准" |
| 位误差 (bits of error) | AMBER 可信度评分 | 将浮点误差映射到 TrustColor 体系的量化标准 |
| 制度划分 (regimes) | 约束图分区（不同子图可能有不同的数值行为） | 对约束图的不同部分采用不同的精度策略 |
| 重写规则库 | 几何数值优化的等价变换规则 | 自动化数值路径改进 |
| Pareto 前沿 | 精度-速度最优数值路径选择 | 在多个可行方案中找到最优权衡 |
| 误差报告 | AMBER 降级诊断报告 | 提供可操作的精度问题诊断 |

---

## 三、Lv-00 映射方案

### 3.1 herbie_eval 风格表达式评估器

在 Lv-00 中引入类似 Herbie 的表达式采样评估器，可以对每条数值计算路径进行精度评分。

```python
# lv00/numeric/herbie_eval.py

from dataclasses import dataclass
from typing import List, Tuple, Callable
import numpy as np
from mpmath import mp  # 高精度数学库（类比 Herbie 的 MPFR）

mp.dps = 50  # 设置 50 位十进制精度作为基准

@dataclass
class EvalResult:
    """单次评估结果"""
    avg_bits_error: float   # 平均位误差
    max_bits_error: float   # 最大位误差
    amber_score: float      # Lv-00 的 AMBER 可信度评分 (0-1)
    num_danger_points: int  # 危险点（误差 > 4 bits）的数量
    total_samples: int

class HerbieStyleEvaluator:
    """
    Herbie 风格的数值路径精度评估器。
    用于评估 Lv-00 中每条数值计算路径的浮点稳定性。
    """

    def __init__(self, high_precision_fn: Callable, standard_fn: Callable):
        """
        Args:
            high_precision_fn: 高精度版本的函数（使用 mpmath）
            standard_fn: 标准双精度的函数（使用 numpy）
        """
        self.high_precision_fn = high_precision_fn
        self.standard_fn = standard_fn

    def sample_input_domain(self, constraint_node, num_samples=256):
        """
        根据约束节点的类型，生成采样点。
        对于几何约束，重点关注退化附近的区域。
        """
        samples = []
        node_type = constraint_node.node_type

        if node_type == "DistanceConstraint":
            # 距离约束：在 [ε, L] 范围内对数均匀采样
            samples = np.logspace(-8, 2, num_samples)
        elif node_type == "AngleConstraint":
            # 角度约束：覆盖 [0, π]，特别关注 0 和 π 附近
            base = np.linspace(0, np.pi, num_samples // 2)
            near_degen = np.concatenate([
                np.logspace(-8, -1, num_samples // 4),  # 接近 0
                np.pi - np.logspace(-8, -1, num_samples // 4)  # 接近 π
            ])
            samples = np.concatenate([base, near_degen])
        elif node_type == "CollinearityCheck":
            # 共线性检测：关注 cross_product ≈ 0
            samples = np.linspace(-1e-6, 1e-6, num_samples)
        else:
            samples = np.linspace(-10, 10, num_samples)

        return samples

    def evaluate(self, constraint_node, num_samples=256) -> EvalResult:
        """
        评估数值路径的精度。
        返回包含位误差和 AMBER 评分的 EvalResult。
        """
        inputs = self.sample_input_domain(constraint_node, num_samples)
        bits_errors = []

        for x in inputs:
            # 高精度基准值
            exact_val = float(self.high_precision_fn(x))
            # 标准双精度近似值
            approx_val = float(self.standard_fn(x))

            if exact_val == 0:
                # 避免除以零
                if approx_val == 0:
                    bits_error = 0.0
                else:
                    bits_error = 53.0  # 双精度尾数的最大位数
            else:
                relative_error = abs((approx_val - exact_val) / exact_val)
                if relative_error > 0:
                    bits_error = abs(np.log2(relative_error))
                else:
                    bits_error = 0.0

            bits_errors.append(bits_error)

        bits_errors = np.array(bits_errors)
        avg_error = float(np.mean(bits_errors))
        max_error = float(np.max(bits_errors))
        danger_count = int(np.sum(bits_errors > 4))

        # 将位误差映射到 AMBER 评分
        # 0 bits → AMBER 1.0（完全可信）
        # >8 bits → AMBER 0.0（不可信）
        amber_score = max(0.0, min(1.0, 1.0 - avg_error / 8.0))

        return EvalResult(
            avg_bits_error=avg_error,
            max_bits_error=max_error,
            amber_score=amber_score,
            num_danger_points=danger_count,
            total_samples=num_samples,
        )

    def partition_regimes(self, constraint_node, min_samples=32):
        """
        制度划分：如果误差分布不均匀，将输入域划分为多个制度。
        对每个制度分别评估。
        """
        # 简化实现：使用基于误差的等深分箱
        inputs = self.sample_input_domain(constraint_node, num_samples=512)
        bits_errors = []
        for x in inputs:
            exact_val = float(self.high_precision_fn(x))
            approx_val = float(self.standard_fn(x))
            if exact_val == 0:
                be = 0.0 if approx_val == 0 else 53.0
            else:
                re = abs((approx_val - exact_val) / exact_val)
                be = abs(np.log2(re)) if re > 0 else 0.0
            bits_errors.append(be)
        bits_errors = np.array(bits_errors)

        # 简单分段：低误差区和高误差区
        median_error = np.median(bits_errors)
        low_mask = bits_errors <= median_error
        high_mask = ~low_mask

        regimes = []
        if np.sum(low_mask) >= min_samples:
            regimes.append({
                "name": "low_error_regime",
                "mask": low_mask,
                "avg_error": float(np.mean(bits_errors[low_mask])),
                "amber": max(0.0, 1.0 - float(np.mean(bits_errors[low_mask])) / 8.0),
            })
        if np.sum(high_mask) >= min_samples:
            regimes.append({
                "name": "high_error_regime",
                "mask": high_mask,
                "avg_error": float(np.mean(bits_errors[high_mask])),
                "amber": max(0.0, 1.0 - float(np.mean(bits_errors[high_mask])) / 8.0),
            })
        return regimes
```

### 3.2 集成 TrustColor AMBER 降级

Herbie 的误差评估输出可直接集成到 Lv-00 的 TrustColor AMBER 降级机制中：

```python
# lv00/trustcolor/herbie_integration.py

class HerbieAmberIntegrator:
    """
    将 Herbie 风格的精度评估集成到 TrustColor AMBER 降级系统。
    """

    AMBER_THRESHOLDS = {
        "GREEN":  0.95,   # 位误差 < 0.4 → 几乎无误差
        "AMBER":  0.70,   # 位误差 0.4-2.4 → 可接受误差
        "RED":    0.30,   # 位误差 2.4-5.6 → 高风险
        "BLACK":  0.0,    # 位误差 > 5.6 → 不可信
    }

    def evaluate_and_assign(self, constraint_graph, evaluator):
        """
        对约束图中的每条数值路径进行评估，
        并分配 TrustColor AMBER 等级。
        """
        color_map = {}
        for node in constraint_graph.numerical_nodes():
            result = evaluator.evaluate(node)

            # 根据 AMBER 评分确定颜色
            if result.amber_score >= self.AMBER_THRESHOLDS["GREEN"]:
                color = "GREEN"
            elif result.amber_score >= self.AMBER_THRESHOLDS["AMBER"]:
                color = "AMBER"
            elif result.amber_score >= self.AMBER_THRESHOLDS["RED"]:
                color = "RED"
            else:
                color = "BLACK"

            color_map[node.id] = {
                "color": color,
                "amber_score": result.amber_score,
                "avg_bits_error": result.avg_bits_error,
                "max_bits_error": result.max_bits_error,
                "danger_points": result.num_danger_points,
            }

        # AMBER 降级逻辑：如果上游为 RED/BLACK，下游自动降级
        for node, info in color_map.items():
            upstream_colors = [
                color_map[n.id]["color"]
                for n in constraint_graph.upstream_of(node)
                if n.id in color_map
            ]
            if "BLACK" in upstream_colors and info["color"] not in ("BLACK",):
                info["color"] = "BLACK"
                info["degraded_by"] = "upstream_BLACK"
            elif "RED" in upstream_colors and info["color"] == "GREEN":
                info["color"] = "AMBER"
                info["degraded_by"] = "upstream_RED"

        return color_map
```

### 3.3 几何表达式重写规则

```python
# lv00/numeric/geo_rewrite_rules.py

class GeoRewriteRule:
    """几何数值优化重写规则"""

    rule_id: str
    name: str
    description: str
    condition: callable   # 判断规则是否适用
    transform: callable   # 执行表达式变换

# 示例规则库
GEO_REWRITE_RULES = [
    GeoRewriteRule(
        rule_id="GR001",
        name="距离计算-hypot替换",
        description="将 sqrt(dx²+dy²) 替换为 hypot(dx, dy)，避免中间溢出和不必要的舍入",
        condition=lambda expr: is_pattern(expr, "sqrt($a**2 + $b**2)"),
        transform=lambda expr: {
            "new_expr": "hypot($a, $b)",
            "speed_impact": +0,      # hypot 可能由硬件实现，速度相当
            "precision_gain": "high",  # 避免中间平方的溢出
        },
    ),
    GeoRewriteRule(
        rule_id="GR002",
        name="夹角计算-atan2替换",
        description="将 acos(dot/(|a||b|)) 替换为 atan2(|cross|, |a||b|)，避免接近 0 或 π 的精度丢失",
        condition=lambda expr: is_pattern(expr, "acos($dot/($lena*$lenb))"),
        transform=lambda expr: {
            "new_expr": "2*atan2(|$a × $b|, $lena*$lenb+$dot)",
            "speed_impact": -1,       # atan2 可能略慢于 acos
            "precision_gain": "high",
        },
    ),
    GeoRewriteRule(
        rule_id="GR003",
        name="共线性检测-相对阈值",
        description="将 |cross| < ε 替换为 |cross|/(|v1||v2|) < ε_rel，使用相对而非绝对阈值",
        condition=lambda expr: is_pattern(expr, "abs($cross) < $eps"),
        transform=lambda expr: {
            "new_expr": "abs($cross)/($len1*$len2) < $eps_rel",
            "speed_impact": -1,
            "precision_gain": "medium",
        },
    ),
    GeoRewriteRule(
        rule_id="GR004",
        name="三角形面积-稳定公式",
        description="使用 Heron 公式的稳定形式计算三角形面积",
        condition=lambda expr: is_triangle_area_expr(expr),
        transform=lambda expr: {
            "new_expr": "0.25*sqrt((a+(b+c))*(c-(a-b))*(c+(a-b))*(a+(b-c)))",
            "speed_impact": -2,
            "precision_gain": "high",
        },
    ),
    GeoRewriteRule(
        rule_id="GR005",
        name="二次求根-稳定公式",
        description="对于 ax²+bx+c=0，使用稳定的求根公式避免灾难性消去",
        condition=lambda expr: is_quadratic_root_expr(expr),
        transform=lambda expr: {
            "new_expr": (
                "if b > 0: r1=(-b-sqrt(b²-4ac))/(2a), r2=(2c)/(-b-sqrt(b²-4ac)); "
                "else: r1=(2c)/(-b+sqrt(b²-4ac)), r2=(-b+sqrt(b²-4ac))/(2a)"
            ),
            "speed_impact": -3,
            "precision_gain": "high",
        },
    ),
]
```

### 3.4 Pareto 最优数值路径选择

```python
# lv00/numeric/pareto_selector.py

class ParetoPathSelector:
    """
    借鉴 Herbie 的 Pareto 前沿思想，为 Lv-00 选择精度-速度最优的数值路径。
    """

    @staticmethod
    def is_dominated(candidate_a, candidate_b):
        """
        判断 candidate_a 是否被 candidate_b 支配。
        支配条件：b 在所有维度上都不差于 a，且至少在一个维度上严格更好。
        """
        better_or_equal = (
            candidate_b.amber_score >= candidate_a.amber_score and
            candidate_b.speed_score >= candidate_a.speed_score
        )
        strictly_better = (
            candidate_b.amber_score > candidate_a.amber_score or
            candidate_b.speed_score > candidate_a.speed_score
        )
        return better_or_equal and strictly_better

    def compute_pareto_front(self, candidates):
        """
        计算 Pareto 前沿。
        返回不被任何其他候选支配的候选集合。
        """
        pareto_front = []
        for i, candidate in enumerate(candidates):
            dominated = False
            for j, other in enumerate(candidates):
                if i != j and self.is_dominated(candidate, other):
                    dominated = True
                    break
            if not dominated:
                pareto_front.append(candidate)
        return pareto_front

    def select_optimal_path(self, constraint_node, candidate_paths,
                            accuracy_weight=0.5):
        """
        在 Pareto 前沿上选择最优数值路径。
        使用加权得分在精度和速度之间做最终选择。
        """
        # 先评估所有候选路径的精度
        for path in candidate_paths:
            evaluator = HerbieStyleEvaluator(
                high_precision_fn=path.high_precision_impl,
                standard_fn=path.standard_impl,
            )
            result = evaluator.evaluate(constraint_node)
            path.amber_score = result.amber_score
            # 速度得分：操作数越少越快（简化版）
            path.speed_score = 1.0 / max(1, path.op_count)

        # 计算 Pareto 前沿
        pareto = self.compute_pareto_front(candidate_paths)

        # 在 Pareto 前沿上用加权得分选出最优
        best_path = max(pareto, key=lambda p:
            accuracy_weight * p.amber_score +
            (1 - accuracy_weight) * p.speed_score
        )
        return best_path
```

---

## 四、实现路线图

### 4.1 分阶段实施计划

| 阶段 | 名称 | 目标 | 预计工作量 | 交付物 |
|------|------|------|-----------|--------|
| **阶段 1** | 表达式采样评估器 | 实现 Herbie 风格的浮点误差检测，能对 Lv-00 中每条数值路径生成误差报告 | 2-3 周 | `herbie_eval.py`、采样策略模块、AMBER 评分集成 |
| **阶段 2** | 重写规则搜索 | 构建几何表达式重写规则库，实现自动规则匹配和应用 | 3-4 周 | `geo_rewrite_rules.py`、规则匹配引擎、Pareto 选择器 |
| **阶段 3** | 自动精度改进管线 | 端到端的"检测→重写→评估→选择"精度改进管线 | 2-3 周 | 管线编排器、改进报告生成、与 CI 集成 |

### 4.2 阶段 1 详细任务

1. **高精度基准实现**
   - 选择高精度后端（推荐 `mpmath` 或 `gmpy2`）。
   - 为 Lv-00 的几何计算函数编写高精度版本。

2. **采样器设计**
   - 实现均匀采样、边界采样、对数均匀采样。
   - 添加退化检测模块，自动在退化构型附近密集采样。

3. **误差量化**
   - 计算位误差（bits of error）。
   - 映射到位误差 → AMBER 评分的转换函数。
   - 生成误差分布报告。

### 4.3 阶段 2 详细任务

1. **规则库构建**
   - 从数值分析文献中收集几何计算的稳定性技巧。
   - 编写可匹配的规则模板（支持模式匹配）。

2. **规则搜索与应用**
   - 对表达式 AST 进行自底向上的规则匹配。
   - 生成所有合法的重写候选。

3. **Pareto 筛选**
   - 对每个候选进行精度评估。
   - 计算 Pareto 前沿。
   - 在 Pareto 前沿上用可配置的权重选择最终方案。

### 4.4 阶段 3 详细任务

1. **管线编排**
   - 统一的入口函数，接受约束图和精度级别参数。
   - 自动遍历所有数值路径。

2. **报告生成**
   - 生成可视化误差报告。
   - AMBER 降级标记和原因解释。

3. **CI 集成**
   - 在 CI 中自动运行精度检查。
   - 当 AMBER 评分低于阈值时，CI 发出警告。

---

## 五、附录

### 5.1 关键参考

- **项目网站**：https://herbie.uwplse.org/
- **论文**：Panchekha et al., "Automatically Improving Accuracy for Floating Point Expressions", PLDI 2015.
- **在线演示**：https://herbie.uwplse.org/demo/ —— 可以直接在浏览器中测试 Herbie。

### 5.2 术语表

| 术语 | 英文 | 说明 |
|------|------|------|
| FPCore | FPCore | Herbie 使用的表达式交换格式 |
| 位误差 | bits of error | 浮点近似值与精确值之间的二进制位数差异 |
| 制度 | regime | 输入域的子分区，在该分区内误差行为相对一致 |
| 灾难性抵消 | catastrophic cancellation | 两个接近的浮点数相减导致有效数字大量丢失 |
| Pareto 前沿 | Pareto front | 多目标优化中所有非支配解的集合 |

### 5.3 Lv-00 项目中的相关文件路径

| 模块 | 计划路径 |
|------|----------|
| 表达式评估器 | `lv00/numeric/herbie_eval.py` |
| 重写规则库 | `lv00/numeric/geo_rewrite_rules.py` |
| Pareto 选择器 | `lv00/numeric/pareto_selector.py` |
| AMBER 集成 | `lv00/trustcolor/herbie_integration.py` |
| 测试 | `tests/numeric/test_herbie_eval.py` |

---

*文档版本：v1.0 | 最后更新：2026-05-24 | 维护者：Lv-00 项目组*
