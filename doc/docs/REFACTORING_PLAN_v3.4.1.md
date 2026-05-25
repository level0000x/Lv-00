# Lv-00 大文件拆分重构计划

版本: v3.4.1
日期: 2026-05-25
状态: 计划中

## 一、概述

本文档记录 Lv-00 项目中超过 64KB 的大型源文件的拆分计划。拆分目标是将超大型文件分解为职责单一的小型模块，提升可维护性和可测试性。

## 二、需要拆分的文件

### 2.1 solver.c (337KB) — 高优先级

**当前问题**：
- 文件超过 337KB，包含过多功能
- Groebner 基算法、方程系统管理、多项式操作、自由度计算混在一起

**拆分方案**：

| 子模块 | 职责 | 建议文件名 |
| groebner_base | Groebner 基计算核心算法 | solver_groebner.c |
| equation_system | 线性/非线性方程组管理 | solver_equation.c |
| freedom_degree | 自由度计算与分析 | solver_freedom.c |
| polynomial_ops | 多项式基本操作 | solver_polynomial.c |
| solver_core | 主求解器入口与调度 | solver_main.c |

### 2.2 proof.c (216KB) — 高优先级

**当前问题**：
- 文件超过 216KB
- 命题管理、证明导航、爆炸原理、导出功能混杂

**拆分方案**：

| 子模块 | 职责 | 建议文件名 |
| proposition | 命题结构管理与销毁 | proof_proposition.c |
| proof_nav | 证明树导航与遍历 | proof_navigation.c |
| explosion | 爆炸原理实现 | proof_explosion.c |
| export | 多种格式导出(LaTeX/JSON等) | proof_export.c |
| proof_main | 证明器主入口 | proof_main.c |

### 2.3 stream.c (78KB) — 中优先级

**当前问题**：
- 文件超过 78KB
- 事件发射、回调管理、JSON序列化混在一起

**拆分方案**：

| 子模块 | 职责 | 建议文件名 |
| stream_core | 核心事件系统 | stream_core.c |
| stream_async | 异步队列实现 | stream_async.c |
| stream_json | JSON序列化 | stream_json.c |
| stream_lazy | 惰性求值 | stream_lazy.c |

### 2.4 rewrite.c (超大) — 中优先级

**当前问题**：
- 文件超大
- VF2子图同构、WL图核哈希、图快照/回滚混杂

**拆分方案**：

| 子模块 | 职责 | 建议文件名 |
| rewrite_vf2 | VF2子图同构算法 | rewrite_vf2.c |
| rewrite_wl | Weisfeiler-Lehman图核 | rewrite_wl.c |
| rewrite_snapshot | 图快照与回滚 | rewrite_snapshot.c |
| rewrite_rules | 规则加载与解析 | rewrite_rules.c |

### 2.5 symbolic_coord.c (107KB) — 中优先级

**拆分方案**：

| 子模块 | 职责 | 建议文件名 |
| coord_rational | 有理数坐标 | coord_rational.c |
| coord_algebraic | 代数数坐标 | coord_algebraic.c |
| coord_transcendental | 超越数坐标 | coord_transcendental.c |
| coord_circuit | 位电路A/B计划 | coord_circuit.c |

### 2.6 其他需要拆分的文件

| 文件 | 当前大小 | 建议拆分数 |
| debug.c (83KB) | 83KB | 4 |
| recursion.c (78KB) | 78KB | 3 |
| unify.c (70KB) | 70KB | 4 |
| engine.c (67KB) | 67KB | 4 |
| type_system.c (大) | 大 | 4 |

## 三、拆分原则

1. **单一职责**：每个子模块只负责一个明确的功能域
2. **最小依赖**：子模块之间的依赖关系应尽量简单
3. **向后兼容**：拆分后应保持原有API不变
4. **增量实施**：每次只拆分1-2个子模块，充分测试后再继续

## 四、实施步骤

### 阶段1: 准备（1-2天）
1. 创建新的 .c/.h 文件骨架
2. 移动代码到新文件
3. 更新 CMakeLists.txt 添加新源文件
4. 编译验证

### 阶段2: solver.c 拆分（3-5天）
1. 提取 solver_groebner.c
2. 提取 solver_equation.c
3. 提取 solver_freedom.c
4. 清理 solver.c 保留主入口

### 阶段3: proof.c 拆分（3-5天）
1. 提取 proof_proposition.c
2. 提取 proof_navigation.c
3. 提取 proof_export.c
4. 清理 proof.c 保留主入口

### 阶段4: 其他文件（持续）
按照优先级逐步处理其他文件

## 五、风险评估

| 风险 | 影响 | 缓解措施 |
| 循环依赖 | 高 | 拆分前绘制依赖图，确保单向依赖 |
| API破坏 | 高 | 保持头文件不变，仅移动实现 |
| 编译错误 | 中 | 增量编译，每步验证 |
| 性能下降 | 低 | 性能测试对比 |

## 六、预期收益

- 提升代码可读性
- 改善编译时间（增量编译）
- 便于单元测试
- 支持并行开发
- 降低bug引入风险
