# Lv-00 v5.0.0 版本文档

**发布日期**: 2026-06-04
**版本号**: 5.0.0
**架构版本**: 十层单向依赖架构 v5.0
**许可证**: MIT

---

## 1. 项目概述

Lv-00 是一门以几何为唯一载体的双模数学元语言。几何体本身是计算的执行者、数据的承载者、证明的见证者。

**生态定位**:

```
上层应用（CGAL / CAD / AI求解器 / 教育工具）
        ↑ 它们需要精确语义
   Lv-00：几何元语言（提供精确语义）
        ↑ 它们提供形式化基础
底层框架（Lean / Coq / 一阶逻辑 / 约束求解）
```

- **不是** CGAL 那种供人调用的算法包
- **不是** AlphaGeometry 那种解题 AI
- **不是** LeanGeo 那种依附于外部证明器的数学库
- **而是** 一种同时完成构造、计算、证明的语言本身

---

## 2. 架构总览

### 2.1 十层单向依赖架构

项目采用严格的十层单向依赖架构，依赖方向为：Layer 10 → Layer 9 → ... → Layer 2，Layer 1 依赖 Layer 2。

| 层级 | 名称 | 职责 | 源文件数 |
|------|------|------|---------|
| Layer 1 | 输入解析层 (Parser) | 词法分析、公式解析、DSL 编译、数学输入 | 7 |
| Layer 2 | 资源管理层 (Resource) | 内存分配、错误码、调试、深拷贝、工具函数、内存池、运行时监控、测试框架、缓存管理、全局状态、中文本地化 | 15 |
| Layer 3 | 几何拓扑层 (Geometry) | 约束图、符号坐标、几何原语、高维结构、欧氏几何、交互几何、几何压缩、事件检测、几何变换、几何代数、区间算术、线程池、SIMD、拓扑、WFC 范式 | 35+ |
| Layer 4 | 公理推理层 (Reasoning) | 引擎、求解器、证明、重写、合一、规范化、类型系统、公理包、三值逻辑、模态算子、量词、SMT/SAT 后端、ATP 后端、BDD 编码、Groebner 引擎、自动微分、ODE 求解器、55+ 数学理论预设 | 100+ |
| Layer 5 | 结果输出层 (Output) | 流式输出、TikZ 导出、证明可视化、互操作、Magic 模拟器、几何可视化 | 6 |
| Layer 6 | 图形化编程层 (Visual) | 可视化编辑器、几何画布、节点图、块画布、控制流块、IO 块、数据结构块、运行时调度、增量执行、类型扩展、表示转换 | 17 |
| Layer 7 | 编排调度层 (Orchestration) | 会话编排、六阶段流水线调度 | 1 |
| Layer 8 | 元验证层 (Meta-Verification) | 类型一致性、完整性、健全性、非平凡性、往返可解析性验证 | 1 |
| Layer 9 | 应用入口层 (Application) | 批处理、交互式 REPL | 1 |
| Layer 10 | 外部集成层 (Interop) | Lean4 桥接、Coq 桥接、OPML 编解码 | 4 |

### 2.2 构建方式

每层编译为 `OBJECT` 库，通过 `target_link_libraries` 控制依赖方向，最终聚合为 `lv_static` 静态库（及可选的 `lv_shared` 共享库）。

---

## 3. 核心特性

### 3.1 几何能力

- **符号坐标系统**: 支持有理数、代数数、二次扩域和超越数（基于 GMP 任意精度算术）
- **约束图**: 表示几何对象（点、线段、区域）及其约束关系
- **归一化**: Weisfeiler-Lehman 图核迭代归一化，自动合并等价节点，保证幂等性
- **统一化**: 验证构造是否满足命题模式
- **几何变换**: 平移、旋转（Rodrigues 公式）、缩放、反射
- **几何代数**: GATr + GAALOP + GeoLogic 落地

### 3.2 推理与证明

- **多策略证明引擎**: 8 种证明方法并存（直接构造、面积法、Groebner 基法、向量法、全角法、演绎数据库法、坐标法、Oracle 法）
- **搜索算法**: DFS 回溯、BFS 队列、最佳优先启发式、MCTS UCB1
- **三值逻辑系统**: 支持真、假、未知三种真值
- **模态逻辑算子**: 支持模态推理
- **量词系统**: 全称量词与存在量词，含元素代入评估
- **信任颜色系统**:
  - Green（纯构造性）
  - Blue（未探索/资源受限/超出范围）
  - Yellow（条件性不可构造）
  - Amber（含数值假设）
  - Orange Oracle（依赖非构造性 oracle）
  - Orange Ex Falso（爆炸原理步骤）
  - Dark Orange（非构造性依赖与数值假设叠加）

### 3.3 后端求解引擎

| 引擎 | 状态 | 说明 |
|------|------|------|
| CDCL SAT 求解器 | 已实现 | 冲突驱动子句学习，含传播/冲突分析/回跳/学习/重启 |
| SMT 后端 | 部分实现 | Groebner 基后端已实现；Z3/cvc5/Singular 需外部安装 |
| ATP 后端 | 已实现 | Vampire/EProver/iProver 子进程集成，SZS 状态解析 |
| BDD 编码 | 已实现 | 唯一表、计算表、Tseitin CNF 变换、Shannon 展开 ADD 运算 |
| Groebner 基 | 已实现 | 并行 Buchberger 算法，work-stealing 负载均衡 |
| 数值后端 | 已实现 | GMRES(m=30)、BiCGSTAB、共轭梯度 |
| 不等式推理 | 已实现 | AM-GM、Cauchy-Schwarz、Jensen、SOS 分解、符号传播 |
| 概率约束 | 已实现 | DTMC 稀疏矩阵 + PCTL 评估（EVENTUALLY/ALWAYS/UNTIL/NEXT/STEADY_STATE） |

### 3.4 函数块与预设模块

- **函数块系统**: 支持打包、实例化、部分应用和组合子
- **55+ 数学理论预设模块**: 涵盖几何、代数、拓扑、逻辑、分析、数论、概率统计、微分方程、范畴论、代数几何、同调代数、李理论、随机过程、博弈论、信息论、编码理论等
- **预设函数块注册系统**: 模块化加载与管理

### 3.5 运行时基础设施

- **内存池管理**: 自定义内存分配器
- **环形日志缓冲区**: 运行时日志系统（支持分级过滤、时间戳、文件输出）
- **对象缓存系统（LRU）**: 性能优化
- **集中化配置系统**: `lv_CONFIG_*` 前缀的配置键
- **统一错误码系统**: 分层 0-999 错误码体系
- **运行时监控**: 健康检查、性能统计

### 3.6 互操作与输出

| 格式/后端 | 状态 | 说明 |
|-----------|------|------|
| OPML | 已实现 | 开放数学证明交换格式（JSON 编码） |
| Lean 4 | 已实现 | 双向桥接：Lv-00 ↔ Lean 4 tactic 脚本 |
| Coq | 已实现 | 双向桥接：Lv-00 ↔ Coq vernacular |
| TikZ | 已实现 | LaTeX 图形输出 |
| SVG | 已实现 | 几何可视化 SVG 渲染 |
| Cairo | 已实现 | Cairo 脚本生成 |
| Three.js | 已实现 | HTML + Three.js 3D 场景 |
| PPM | 已实现 | 光栅化像素输出（Bresenham 算法） |

### 3.7 可视化编程

- **四视图同步**: 节点图、几何画布、积木画布、文本代码
- **力导向布局**: Fruchterman-Reingold 算法（50 次迭代）
- **块调度器**: Kahn 拓扑排序 + 增量脏块执行
- **类型推断**: Hindley-Milner 风格统一算法
- **效果追踪**: Pure/IO/State 效果类型系统

### 3.8 形式化验证

- **Lean 4 框架**: Lake 构建系统，mathlib4 v4.14.0 依赖
- **Hilbert 公理体系**: 五大公理组（关联、顺序、全等、平行、连续）
- **欧氏平面**: 基础定义和定理框架
- **元验证**: 类型一致性、完整性、健全性、非平凡性、往返可解析性

---

## 4. 文件统计

| 层级 | 源文件数 | 头文件数 | 测试数 |
|------|---------|---------|--------|
| Layer 1 (Parser) | 7 | 共享 | - |
| Layer 2 (Resource) | 15 | 共享 | - |
| Layer 3 (Geometry) | 35+ | 共享 | - |
| Layer 4 (Reasoning) | 100+ | 共享 | - |
| Layer 5 (Output) | 6 | 共享 | - |
| Layer 6 (Visual) | 17 | 8 | - |
| Layer 7 (Orchestration) | 1 | 1 | - |
| Layer 8 (Meta-Verify) | 1 | 1 | - |
| Layer 9 (Application) | 1 | 1 | - |
| Layer 10 (Interop) | 4 | 1 | - |
| **formal/** | 13 | - | 1 |
| **test/** | - | - | 70+ |
| **总计** | **200+** | **120+** | **70+** |

---

## 5. 已知限制

### 5.1 外部依赖限制

| 依赖 | 影响 | 说明 |
|------|------|------|
| GMP | 构建必需 | 非 WASM 构建必须依赖 GMP 库 |
| Z3/cvc5 | SMT 求解 | 需外部安装，未安装时优雅降级为 UNKNOWN |
| SuiteSparse | 稀疏求解 | CHOLMOD/UMFPACK/SPQR 集成待实现，当前为稠密回退 |
| OpenMP/CUDA/HIP | 并行后端 | 需硬件和 SDK，当前仅 SERIAL 后端 |

### 5.2 硬编码阈值

| 参数 | 当前值 | 说明 |
|------|--------|------|
| VF2_MAX_DEPTH | 100 | 图匹配深度限制 |
| BUCHBERGER_MAX_STEPS | 50000 | Groebner 基计算步数限制 |
| POLY_REDUCE_MAX_STEPS | 10000 | 多项式约化步数限制 |
| REWRITE_SOLVE_MAX_ITERATIONS | 10000 | 重写求解迭代限制 |
| CDCL_MAX_STEPS | 1000 | SAT 求解步数限制 |
| CDCL_MAX_DECISIONS | 1000 | SAT 决策次数限制 |

### 5.3 形式化理论

- 角度度量系统尚未形式化（EuclideanPlane.lean 中 angle_sum_180 为占位）
- Hilbert 公理体系的机器可检验证明仍在推进中
- 核心算法正确性证明（归一化幂等性、VF2 匹配、Groebner 基）待完成

### 5.4 模块依赖

- `lv_EXCLUDE_BROKEN_PRESETS` 排除了部分有深层依赖问题的预设模块（如微分几何、泛函分析）
- Windows Clang 工具链不支持 libFuzzer
- 覆盖率与 Sanitizer 同时启用可能互相干扰

---

## 6. 路线图

### 6.1 时间线

| 阶段 | 时间 | 重点 |
|------|------|------|
| 2026 Q3-Q4 | 6 个月 | Lean 4 框架完善、Hilbert 公理形式化、C API 接口层、动态阈值框架 |
| 2027 Q1-Q2 | 6 个月 | 推理规则完备性、信任颜色系统形式化、策略调度优化、基准测试 |
| 2027 Q3-Q4 | 6 个月 | 论文撰写投稿、v4.0-alpha、社区反馈、v4.0-stable |

### 6.2 关键里程碑

| 里程碑 | 日期 | 交付物 |
|--------|------|--------|
| M1 | 2026.07 | Lean 4 项目框架完善 |
| M2 | 2026.08 | C API 接口层完成 |
| M3 | 2026.09 | Hilbert 公理形式化完成 |
| M4 | 2026.10 | Lean 4 插件 v0.1 |
| M5 | 2026.11 | 动态阈值框架（简单问题提速 30%） |
| M6 | 2026.12 | 中期评审 |
| M9 | 2027.03 | 推理规则完备性 |
| M12 | 2027.06 | v4.0-alpha |

### 6.3 改进维度

| 维度 | 当前等级 | 目标等级 | 关键任务 |
|------|---------|---------|---------|
| 形式化理论深度 | B- | A- | Hilbert 公理体系形式化证明、算法正确性验证 |
| 学术互通生态 | C+ | B+ | Lean/Coq 双向接口完善、OPML 生态建设 |
| 核心算法性能 | B | A- | 自适应剪枝策略、Groebner 引擎并发优化、推理效率提升 50%+ |

---

## 7. 变更历史

### v5.0.0 (2026-06-04)

**架构扩展**:
- 从五层架构扩展为十层架构（新增 Visual、Orchestration、Meta-Verification、Application、Interop）

**L4 推理层**:
- 实现 CDCL SAT 求解器（传播、冲突分析、回跳、学习、重启）
- 实现 SMT 后端（Groebner 基、Z3/cvc5 子进程集成）
- 实现 ATP 后端（Vampire/EProver/iProver 子进程集成）
- 实现 BDD 编码（唯一表、计算表、Tseitin CNF 变换）
- 实现 GMRES/BiCGSTAB/CG 迭代求解器
- 实现不等式推理（AM-GM、Cauchy-Schwarz、Jensen、SOS）
- 实现概率约束（DTMC + PCTL 评估）
- 实现多策略证明引擎（8 种策略 + 4 种搜索算法）
- 实现自适应剪枝框架
- 实现 Groebner 并行引擎（Buchberger + work-stealing）

**L3 几何层**:
- 实现 Adams-Bashforth-Moulton 预测校正（1-5 阶）
- 实现 BDF 隐式多步（1-5 阶，Newton 迭代）
- 实现 CSG 操作（变换、凸包、Minkowski 和、线性/旋转拉伸）

**L5 输出层**:
- 修复 Lean 4 导出中的 sorry 输出
- 实现 4 种渲染后端（Cairo、Three.js、TikZ、PPM）
- 实现插件系统（通配符匹配、目录扫描、语义版本）

**L6 可视化层**:
- 实现节点图（力导向布局）
- 实现几何画布和积木画布（SVG 渲染）
- 实现块调度器（拓扑排序 + 增量执行）
- 实现 4 种视图转换器（block↔text↔node↔geometry）
- 实现同步协议

**L7-L10**:
- 实现编排器六阶段流水线
- 实现元验证五维检查
- 实现批处理和 REPL
- 实现 Lean 4/Coq/OPML 双向桥接

**formal/**:
- Lean 4 项目框架（Lake + mathlib4）
- Hilbert 公理体系形式化
- 欧氏平面基础定义

---

## 8. 构建指南

### 依赖

- CMake >= 3.15
- C11 编译器 (GCC/Clang/MSVC)
- GMP 库（非 WASM 构建）
- Python 3（可选，用于 DSL）

### 基本构建

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### 选项

| 选项 | 默认 | 说明 |
|------|------|------|
| BUILD_TESTS | ON | 构建测试 |
| BUILD_EXAMPLES | ON | 构建示例 |
| ENABLE_WASM | OFF | WebAssembly 构建 |
| ENABLE_LAYER_VALIDATION | ON | 层级边界验证 |

---

## 9. 许可证

MIT License - 详见 [LICENSE](LICENSE) 文件。

---

*本文档由 Lv-00 项目自动生成，最后更新: 2026-06-04*
