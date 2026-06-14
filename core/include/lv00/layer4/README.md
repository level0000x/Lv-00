# Layer 4: Reasoning & Computation

推理引擎、求解器、证明系统的核心层。这是项目最复杂的层，因此进一步分为多个子功能域。

## 子功能域

### 1. Core (core/) — 核心引擎

```
core/engine.h              # 推理引擎主入口
core/solver.h              # 约束求解器
core/engine_scheduler.h    # 调度与流程控制
core/solver_core.h         # 求解核心算法
```

### 2. Proof (proof/) — 证明系统

```
proof/proof.h              # 证明构造与验证
proof/proof_priority.h     # 证明策略优先级
proof/proof_trace.h        # 证明路径追踪
```

### 3. Rewrite & Unify (rewrite_unify/) — 重写与合一

```
rewrite_unify/rewrite.h    # 项重写系统
rewrite_unify/unify.h      # 合一算法
rewrite_unify/recursion.h  # 递归处理
```

### 4. Type & Logic (type_logic/) — 类型与逻辑

```
type_logic/type_system.h   # 类型系统
type_logic/quantifier.h    # 量词处理
```

### 5. Axiom (axiom/) — 公理系统 [已存在]

```
axiom/axiom_pkg.h          # 公理包
axiom/axiom_rule_engine.h  # 公理规则引擎
```

### 6. Func Block (func_block/) — 函数块系统 [已存在]

```
func_block/func_block.h           # 函数块接口
func_block/func_block_registry.h  # 函数块注册表
```

### 7. Backends — 可选求解后端

#### a) Groebner Backend
```
backends/groebner_engine.h        # Groebner 基求解
```

#### b) SMT Backend
```
backends/smt_backend.h            # SMT 求解器接口
```

#### c) SAT/BDD Backend
```
backends/sat_encoding.h           # SAT 编码
backends/bdd_encoding.h           # BDD 编码
```

#### d) ATP Backend
```
backends/atp_backend.h            # 自动定理证明
```

### 8. Presets — 数学预设

预设已按数学领域分类：

```
presets/geometry/         # 几何预设 (15+ files)
presets/algebra/          # 代数预设 (12+ files)
presets/analysis/         # 分析预设 (10+ files)
presets/logic/            # 逻辑预设 (8+ files)
presets/advanced/         # 高级预设 (10+ files)
```

## 依赖关系

```
        Layer 4 (Reasoning)
             ↓ 依赖
        Layer 3 (Geometry)
        Layer 2 (Resource)
```

**上层依赖者**: Layer 5 (输出), Layer 6 (可视化), Layer 7+

## 内部架构

### 层间调用关系
```
[Engine (core/)] ← 主协调器
    ↓
[Solver] ← 约束求解
    ↓
[Backends: Groebner/SMT/SAT/ATP] ← 特定算法
    ↓
[Layer 3: Geometry] ← 基础对象
```

### 功能流程
```
输入表达式 (Layer 1)
    ↓
几何约束 (Layer 3)
    ↓
L4 求解 [选择合适的后端]
    ↓
证明生成
    ↓
L5 输出
```

## 使用指南

### 对于 Layer 5+ 的使用者

```c
// 推荐：通过主引擎接口
#include <lv00/engine.h>
LV00Engine *engine = lv00_engine_create();
lv00_engine_solve(engine, problem);

// 避免：直接调用后端
// #include <lv00/smt_backend.h>  ← 不建议
```

### 对于 Layer 4 内部开发者

```c
// 可以使用任何 Layer 4 的子模块
#include <lv00/layer4/core/solver.h>
#include <lv00/layer4/backends/smt_backend.h>
#include <lv00/layer4/proof/proof.h>
```

## 公开 vs 内部 API

### 公开 API (Layer 5+ 可使用)
- `engine.h` — 主入口
- `axiom_pkg.h` — 公理定义
- `proof.h` — 证明结果访问

### 内部 API (仅 Layer 4 内部)
- `backends/*` — 求解后端
- `rewrite_unify/*` — 内部算法
- `type_logic/*` — 类型检查

### 内部实现 (绝不导出)
- `logic_check.c` — 逻辑检查
- `circuit_breaker.c` — 中断机制
- `conflict_detector.c` — 冲突检测

## 设计原则

1. **可扩展性** — 易于添加新的求解后端
2. **清晰的职责** — 每个子模块有明确的功能
3. **后端独立** — 不同求解器独立实现
4. **策略模式** — 支持多个证明策略
5. **性能优化** — Groebner 并行、SMT 增量等

## 文件清单

```
core/include/lv00/layer4/
├── core/
│   ├── engine.h
│   ├── solver.h
│   ├── engine_scheduler.h
│   └── solver_core.h
├── proof/
│   ├── proof.h
│   ├── proof_priority.h
│   └── proof_trace.h
├── rewrite_unify/
│   ├── rewrite.h
│   ├── unify.h
│   └── recursion.h
├── type_logic/
│   ├── type_system.h
│   └── quantifier.h
├── axiom/       # 已存在
├── func_block/  # 已存在
├── backends/
│   ├── groebner_engine.h
│   ├── smt_backend.h
│   ├── sat_encoding.h
│   ├── bdd_encoding.h
│   └── atp_backend.h
├── presets/
│   ├── geometry/
│   ├── algebra/
│   ├── analysis/
│   ├── logic/
│   └── advanced/
└── ...
```

## 维护指南

- 新功能应选择合适的子功能域放置
- 跨子功能域的交互应通过明确的接口
- 定期审查是否需要新的子功能域
- 保留详细的设计文档
