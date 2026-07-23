# Lv-00 几何元语言系统 v1.1.0 - 模块文档索引

## 概述

Lv-00 是一门以几何为唯一载体的双模数学元语言。几何体本身是计算的执行者、数据的承载者、证明的见证者。

**核心特性**：
- **几何即符号**：点、线、区域本身就是程序的实体
- **数形不二**：数值只能是几何量
- **构造即证明**：构造是否构成证明取决于合一检查
- **公理中立**：内核不内建距离、角度概念
- **可演进**：公理系统可升级，定理可固化为新规则

## 文档结构

本文档目录包含 Lv-00 v1.1.0 全部核心模块的详细描述：

### 核心模块

| 文档 | 模块 | 功能描述 |
|------|------|----------|
| [01_symbolic_coord.md](01_symbolic_coord.md) | 符号坐标系统 | 有理数、代数数、二次根式、超越常数的精确表示和运算 |
| [02_constraint_graph.md](02_constraint_graph.md) | 约束图核心 | 点、线段、区域、端口、函数块节点及约束管理 |
| [03_normalization.md](03_normalization.md) | 图规范化遍引擎 | 点合并、线段合并、区域合并、幂等性保证 |
| [04_solver.md](04_solver.md) | 符号代数求解器 | 代数方程转化、Gröbner基求解、多解处理 |
| [05_rewrite.md](05_rewrite.md) | 图重写引擎 | 匹配算法、替换操作、循环检测 |
| [06_unify.md](06_unify.md) | 合一检查 | 端口匹配、约束匹配、坐标判等 |

### 高级功能

| 文档 | 模块 | 功能描述 |
|------|------|----------|
| [07_func_block.md](07_func_block.md) | 函数块系统 | 打包操作、确定性检查、多解选择器、组合子 |
| [08_type_system.md](08_type_system.md) | 类型系统 | 宇宙层级、类型等价检查、类型推断 |
| [09_proof.md](09_proof.md) | 命题与证明系统 | 命题模式、证明导航器、爆炸原理 |
| [10_recursion.md](10_recursion.md) | 递归与条件 | 测度系统、选择器块、递归深度监控 |
| [11_wfc_paradigm.md](11_wfc_paradigm.md) | 波函数坍缩范式 | WFC 约束生成与几何构造 |

### 架构与工程文档

| 文档 | 模块 | 功能描述 |
|------|------|----------|
| [12_context_and_lifecycle.md](12_context_and_lifecycle.md) | 上下文与生命周期 | 引擎上下文管理、资源生命周期 |
| [13_error_handling.md](13_error_handling.md) | 错误处理 | 错误码体系、错误传播、恢复策略 |
| [14_memory_management.md](14_memory_management.md) | 内存管理 | 内存池、分配策略、泄漏检测 |

### 十层架构详细文档

| 文档 | 模块 | 功能描述 |
|------|------|----------|
| [15_parsing_layer.md](15_parsing_layer.md) | 解析层 (Layer 1) | 词法分析、语法解析、DSL 编译、AST 生成 |
| [16_geometry_layer.md](16_geometry_layer.md) | 几何层 (Layer 3) | 约束图、符号坐标、规范化、几何拓扑 |
| [17_reasoning_layer.md](17_reasoning_layer.md) | 推理层 (Layer 4) | 求解器、重写、证明、SMT/ATP 后端 |
| [18_output_layer.md](18_output_layer.md) | 输出层 (Layer 5) | TikZ 导出、跨语言互操作、证明格式化 |
| [19_numerical_backends.md](19_numerical_backends.md) | 数值后端 | 区间算术、浮点误差、Herbie/FPTaylor/Gappa |
| [20_preset_registry.md](20_preset_registry.md) | 预设函数块注册表 | 63个领域预设函数块完整索引 |

## 模块依赖关系

```
符号坐标系统 (01)
    ↓
约束图核心 (02)
    ↓
    ├─→ 图规范化遍引擎 (03)
    ├─→ 符号代数求解器 (04)
    ├─→ 图重写引擎 (05)
    └─→ 合一检查 (06)
            ↓
    ├─→ 函数块系统 (07)
    ├─→ 类型系统 (08)
    ├─→ 命题与证明系统 (09)
    └─→ 递归与条件 (10)
```

## 快速参考

### 信任颜色图例

| 颜色 | 含义 |
|------|------|
| 🟢 绿色 | 全构造，无任何非常规依赖 |
| 🔵 蓝色 | 待完成的证明义务（未探索/资源受限/超出范围） |
| 🟢 绿色实框 | 已证不可构造 |
| 🟡 黄色虚线框 | 条件性不可构造 |
| 🟠 浅橙色实心 | 依赖非构造性 oracle |
| 🟠 浅橙色虚线 | 爆炸原理步骤 |
| 🟡 橙黄色 | 含数值假设 |
| 🟠 深橙色 | 非构造性依赖与数值假设叠加 |

### 核心 API 速查

**上下文创建**:
```c
lvContext *ctx = engine_create();
engine_load_axiom_package(ctx, "euclidean.lvz");
```

**创建几何体**:
```c
// 创建点
AddNodeResult result = graph_add_point(graph, x, y);
int point_id = graph->next_node_id - 1;

// 创建线段
int endpoints[] = {point1_id, point2_id};
result = graph_add_line_segment(graph, endpoints, 2);
```

**添加约束**:
```c
int participants[] = {point_id, line_id};
graph_add_constraint(graph, CONSTRAINT_INCIDENCE, participants, 2);
```

**打包函数块**:
```c
FuncBlock *fb;
PackResult result = func_block_pack(
    graph,
    internal_nodes, 2,
    input_ports, 1,
    output_ports, 1,
    NULL, 0,
    &fb
);
```

**合一检查**:
```c
Proposition *prop = proposition_create(1, PROPOSITION_ATOMIC);
proposition_set_pattern(prop, pattern_graph);

UnifyResult result = proof_unify(construction_graph, prop, true);
if (result == UNIFY_OK) {
    printf("命题得证！\n");
}
```

## 文件结构

```
Lv-00/
├── core/                     # 核心引擎（C11）
│   ├── include/lv/         # 公共 API 头文件（229 个 .h）
│   │   ├── lv.h            # 主头文件 — 唯一公共入口
│   │   ├── config.h          # 配置系统
│   │   ├── proof.h           # 证明系统
│   │   ├── func_block.h      # 函数块系统
│   │   ├── lambda_term.h     # λ-演算数据结构
│   │   └── preset_*.h        # 60+ 数学理论预设声明
│   └── src/                  # 十层实现（401 个 .c）
│       ├── layer1_parser/            # 公式解析、DSL 编译
│       ├── layer2_resource/          # 内存池、缓存、上下文、调试
│       ├── layer3_geometry/          # 约束图、符号坐标、代数数
│       ├── layer4_reasoning/         # 证明引擎、Groebner、SMT/SAT、λ-演算
│       ├── layer5_output/            # 输出与导出、插件系统
│       ├── layer6_visual/            # 可视化编程、块调度器
│       ├── layer7_orchestration/     # 流水线编排
│       ├── layer8_meta_verify/      # 元验证
│       ├── layer9_application/       # 应用入口
│       └── layer10_interop/         # Coq/Lean4/OPML 桥接
├── ui/                       # Web 前端（React + TypeScript + Vite）
│   ├── L1-base/              # 基础样式与工具
│   ├── L2-components/        # 原子组件（16 个）
│   ├── L3-modules/           # 业务模块（17 个）
│   ├── L4-shell/             # 应用壳
│   ├── L5-core/              # 内核桥接与状态管理
│   ├── L6-monitor/           # 运行监控
│   └── shells/               # VS Code 扩展 / Qt 独立窗口
├── bootstrap/                # .lv 语义规格 + GMP 原语运行时
├── formal/                   # Lean 4 形式化（编译器 pipeline + Hilbert 公理）
├── lv-formal/              # 经典几何形式化框架
├── module/                   # 扩展模块：Python 绑定、公理包（.lvz）、流桥接
├── cmake/                    # CMake 打包配置
├── doc/                      # 技术文档与报告
├── test/                     # 测试套件
├── examples/                 # 演示示例
├── tool/                     # 辅助工具
├── CMakeLists.txt
├── VERSION                   # 1.1.0
├── IMPLEMENTATION_STATUS_AUDIT.md
└── README.md
```

## 编译说明

### 依赖

- C11 编译器（GCC / Clang / MSVC）
- GMP 库 ≥ 6.0
- CMake ≥ 3.15

### 编译步骤

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### 运行测试

```bash
ctest --output-on-failure
```

## 设计原则

### 公理中立

内核只提供关联几何引擎（点、线、区域的关联、之间、相交、包含、连接关系），不内建距离、角度概念。欧氏几何、双曲几何、集合论、类型论全都是可加载的"公理系统包"。

### 信任边界

- **第一可信基**：C内核（关联几何引擎、符号坐标系统、求解器、重写引擎）
- **第二可信基**：核心公理包的安全重写规则集（需外部汇合性证明）

### 自举路线

1. ✅ C实现内核（基础约束图+推理引擎完整）
2. ✅ 公理系统编辑器
3. ✅ 微自举A：几何可表达性验证（lv 解析自身 .lv 文件）
4. ✅ λ-演算核心集成（Church 编码、Y 组合子、β-归约）
5. ⏳ 微自举B：几何证明能力验证
6. ⏳ 首次自举：命题逻辑验证器

## 版本历史

- **v1.1.0**（当前版本）
  - 十层单向依赖架构，C11 实现
  - 符号坐标系统（GMP 精确有理数）、约束图核心、图规范化
  - 推理引擎：Groebner 基求解器（237KB）、SMT/ATP/SAT/BDD 多后端
  - λ-演算核心集成（β-归约、Church 编码、Y 组合子）
  - 端口作用域系统 + 信任颜色 8 色体系
  - 证明系统：多策略合一、证明导航器、证明导出
  - UI 前端 L1–L6 分层架构（React 19 + TypeScript + Vite）
  - Lean 4 形式化验证：编译器 pipeline 已证，54 个公理包验证
  - 函数块系统、类型系统、递归与条件
  - 60+ 数学理论预设，模块加载器（SHA-256 + DFS 循环检测）
  - 401 个 .c、229 个 .h、84 个 .lean、154 个 .lv、41 个 .tsx、1011 个 .ts

## 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](../LICENSE) 文件

## 参考文献

- 设计文档：`设计.txt`
- 规划文档：`规划.txt`
- 系统描述：`Lv-00系统描述文档.md`
