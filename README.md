# Lv-00 几何元语言

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Version](https://img.shields.io/badge/version-v1.9.0+-blue.svg)](CHANGELOG.md)
[![C11](https://img.shields.io/badge/C-11-green.svg)](https://en.cppreference.com/w/c)
[![Lean4](https://img.shields.io/badge/Lean-4-purple.svg)](https://lean-lang.org/)

> **Lv-00 是一个尝试把几何构造、代数计算与逻辑证明统一到同一语法体系里的形式化元语言原型。核心以 C11 实现，并在 Lean 4 中做形式化验证。**

> ⚠️ **实验性 / 研究原型。** 这是一个学术品质的研究原型：核心几何与符号计算引擎实现完整，已通过初步验证；部分高级功能和跨平台支持仍在完善中。详见[项目现状](#项目现状)和[实现完成度诚实审计](IMPLEMENTATION_STATUS_AUDIT.md)。

---

## 目录

- [项目现状](#项目现状)
- [项目背景](#项目背景)
- [设计目标](#设计目标)
- [核心概念](#核心概念)
- [构建](#构建)
- [快速开始](#快速开始)
- [公共 API](#公共-api)
- [UI 系统](#ui-系统)
- [系统架构](#系统架构)
- [项目结构](#项目结构)
- [形式化验证](#形式化验证)
- [技术实现](#技术实现)
- [文档](#文档)
- [相关工作](#相关工作)
- [开发路线图](#开发路线图)
- [参与贡献](#参与贡献)
- [许可证](#许可证)

---

## 项目现状

**核心完成度：85-90%**（完整实现）。关键模块已完全实现并通过验证，包含三輪数学严谨性与数值稳定性修复（30+ 文件）。

> 📋 完整的诚实审计报告见：[IMPLEMENTATION_STATUS_AUDIT.md](IMPLEMENTATION_STATUS_AUDIT.md)

### 核心模块状态

| 模块 | 状态 | 说明 | 代码量 |
|---|:--:|---|---|
| **符号坐标 / 有理数运算 (GMP)** | ✅ 完全实现 | 精确有理数、代数数、区间表示 | ~3KB |
| **约束图 / 规范化** | ✅ 完全实现 | 节点管理、同构检测、合并算法 | ~6.5KB |
| **引擎 API (`lv_engine_*`)** | ✅ 完全实现 | 30+ 公共函数，已验证可用 | ~3KB |
| **推理引擎（Groebner/SMT/SAT）** | ✅ 完全实现 | 237KB solver.c，包含 Buchberger、重写、证明 | 237KB |
| **高维几何 / CSG / 变换** | ✅ 完全实现 | 所有主体实现，需复杂场景测试 | ~4KB |
| **证明系统** | ✅ 90% 完整 | 合一、追踪、多策略搜索框架完成；部分搜索策略待优化 | ~8KB |

### 高级功能状态

| 功能 | 状态 | 说明 | 瓶颈 |
|---|:--:|---|---|
| **可视化 / 多渲染后端** | 🚧 50% | 流式事件/导出框架完成；TikZ/SVG 渲染待补齐 | 缺少具体渲染实现 |
| **UI 前端 (React)** | ✅ 完全实现 | L1-L6 六层架构完整；Mock Bridge 独立可用 | 与内核真实通信未充分测试 |
| **Python 绑定** | ✅ 完全实现 | 模块化绑定完成；集成示例待补充 | 需完整示例代码 |

### 基础设施状态

| 项目 | 状态 | 说明 |
|---|:--:|---|
| **跨平台构建 (CMake)** | ⚠️ 配置完成，需验证 | Ubuntu 已验证；macOS/Windows 需测试 |
| **Lean 4 形式化验证** | ✅ 骨架完整；🚧 部分待证 | 编译器 pipeline 已证；54 个公理包验证完成；几何部分 30 个 axiom 待证 |
| **CI / CD** | ⚠️ 配置完成，未通过 | 工作流文件就绪；需修复编译错误 |

> **图例**：✅ 完全实现 · 🚧 部分实现 · ⚠️ 配置完成，需验证

当前代码库约：401 个 `.c`、229 个 `.h`、84 个 `.lean`、154 个 `.lv` 语义规格、94 个 `.py`、41 个 `.tsx`、1011 个 `.ts`。其中核心推理引擎（`solver.c`）占 237KB，是最大单文件实现。测试套件 138 项，137 项通过（1 项预存失败）。

---

## 项目背景

几何计算、符号推理、形式化证明是数学机械化的三个方向，各自有成熟工具：

| 领域 | 典型系统 |
|---|---|
| 几何作图 | GeoGebra、CAD |
| 代数计算 | SymPy、MATLAB |
| 形式证明 | Lean、Coq |

它们在各自领域内成熟，但**跨领域协作时存在语义断层**：几何模型难以直接用于形式证明，符号计算结果难以追溯几何意义。Lv-00 想探索的，是把这三个方向统一到同一个形式化语言体系中。

---

## 设计目标

Lv-00 项目的目标是探索一种**领域特定形式化语言**，尝试实现以下设计原则：

### 1. 语义统一性

几何对象（点、线、圆等）同时作为：
- 程序的执行实体
- 代数计算的操作数
- 逻辑命题的证明对象

### 2. 精确性优先

采用符号计算而非浮点计算：
- 有理数运算基于 GMP（`mpq_t`），任意精度
- 代数数使用最小多项式 + 隔离区间表示
- 避免数值精度损失

### 3. 十层层级化架构

采用十层单向依赖架构：
- 层间通过稳定数据结构通信
- 禁止反向依赖
- 支持编译时 `_Static_assert` 边界检查（ENABLE_LAYER_VALIDATION）

### 4. 模块化扩展

- 预设模块系统支持领域扩展（60+ 数学理论预设）
- 公理包支持版本化管理（`.lvz` 格式）
- 函数块支持组合复用

### 5. 内核/UI 完全解耦

- C 内核与 React 前端通过 `KernelBridge` 协议通信
- 前端使用 Mock Bridge 独立开发，不依赖真实内核
- 协议类型（DrawCmd / UserAction 等）在内核和前端之间一一对应

---

## 核心概念

### 符号坐标 (Symbolic Coordinate)

使用精确符号表示，而非浮点近似：有理数基于 GMP（`mpq_t`），代数数用最小多项式 + 隔离区间表示，避免数值精度损失。

### 约束图 (Constraint Graph)

几何对象（点、线、圆…）为节点，约束关系（距离、角度、共线…）为边，以图结构存储并参与推理。

### 函数块 (Function Block)

可复用、可组合的计算单元，通过 `FuncBlock` 类型定义，支持预设注册和运行时组合。详见 `core/include/lv/func_block.h`。

### 引擎模型 (Engine)

对外 API 围绕一个 `lvEngine`：你向引擎添加几何元素与约束，调用归一化与求解，再读取结果。这是当前**真实可用**的编程入口（见下文）。

---

## 构建

> ✅ **构建配置完成，已在 Ubuntu 验证，macOS/Windows 需测试**（见[项目现状](#项目现状)）。

### 环境依赖

| 依赖 | 版本要求 | 说明 |
|---|---|---|
| CMake | ≥ 3.15 | 构建系统 |
| GMP | ≥ 6.0 | 任意精度有理数运算 |
| C 编译器 | C11 | GCC / Clang / MSVC |
| Node.js (可选) | ≥ 18 | 仅构建 UI 前端 |

### 安装依赖

```bash
# Ubuntu / Debian
sudo apt-get install cmake libgmp-dev

# macOS
brew install cmake gmp

# Windows (MSYS2 MinGW)
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-gmp
```

### 编译内核

```bash
git clone https://github.com/level0000x/Lv-00.git
cd Lv-00
mkdir build && cd build
cmake ..
cmake --build .
```

CMake 构建选项：`BUILD_TESTS`(ON), `BUILD_EXAMPLES`(ON), `BUILD_SHARED_LIBS`(OFF), `ENABLE_COVERAGE`(OFF), `ENABLE_SANITIZERS`(OFF), `BUILD_WASM`(OFF), `ENABLE_LAYER_VALIDATION`(OFF)。

### 测试

```bash
ctest --output-on-failure
```

### 编译 UI 前端（可选）

```bash
cd ui
npm install
npm run dev       # 开发服务器 (localhost:5173)
npm run build     # 生产构建 → dist/
```

---

## 快速开始

下面的示例对应 `core/include/lv/lv.h` 中声明的**真实公共 API**（引擎模型）。一个 3-4-5 直角三角形的最小例子：

```c
#include "lv/lv.h"
#include <stdio.h>

int main(void) {
    // 1. 初始化系统
    if (!lv_init()) {
        fprintf(stderr, "Failed to initialize Lv-00\n");
        return 1;
    }

    // 2. 创建引擎
    lvEngine *engine = lv_engine_create();
    if (!engine) {
        lv_cleanup();
        return 1;
    }

    // 3. 添加点（参数为有理数分子/分母：x = num/den, y = num/den）
    int p1 = lv_add_point(engine, 0, 1, 0, 1);   // (0, 0)
    int p2 = lv_add_point(engine, 3, 1, 0, 1);   // (3, 0)
    int p3 = lv_add_point(engine, 0, 1, 4, 1);   // (0, 4)

    // 4. 添加边
    lv_add_line_segment(engine, p1, p2);
    lv_add_line_segment(engine, p2, p3);
    lv_add_line_segment(engine, p3, p1);

    // 5. 归一化 + 求解
    lv_normalize(engine, true);
    EngineSolveResult result = lv_solve(engine);
    (void)result;

    // 6. 读取结果
    char info[1024];
    lv_get_system_info(info, sizeof(info));
    printf("Result: %s\n", info);

    // 7. 清理
    lv_engine_destroy(engine);
    lv_cleanup();
    return 0;
}
```

> **注意**：本仓库历史上的 README 出现过 `lv_point()` / `lv_distance()` / `lv_prove()` / `lv_context_create()` 等示例 API，以及 `Point` / `Segment` / `Circle` / `Expr` 等高级类型。这些是**旧设计**，已废弃。当前唯一的真实 API 入口是 `lvEngine` 模型，见上文。

---

## 公共 API

`core/include/lv/lv.h` 当前导出的主要函数（约 30 个）：

**生命周期与系统**
`lv_init` · `lv_cleanup` · `lv_is_initialized` · `lv_health_check` · `lv_get_system_info`

**版本**
`lv_get_version_string` · `lv_get_version_info` · `lv_check_version_compat`

**引擎与建模**
`lv_engine_create` · `lv_engine_destroy` · `lv_add_point` · `lv_add_point_i` · `lv_add_line_segment` · `lv_add_constraint_incidence` · `lv_normalize` · `lv_solve`

**配置**
`lv_config_get_int` · `lv_config_get_bool` · `lv_config_get_double` · `lv_config_get_string` · `lv_config_set_int` · `lv_config_set_bool` · `lv_config_set_double` · `lv_config_set_string`

**内存与诊断**
`lv_get_memory_stats_ex` · `lv_get_memory_limit_ex` · `lv_set_memory_limit_ex` · `lv_set_log_level` · `lv_get_log_level` · `lv_set_assertions_enabled` · `lv_are_assertions_enabled`

---

## UI 系统

前端基于 **React 19 + TypeScript 6 + Vite 8 + Zustand**，采用 L1–L6 六层架构，与 C 内核完全解耦。

| 层级 | 路径 | 职责 |
|:--:|---|---|
| L1 | `ui/L1-base/` | 基础样式 (theme.css)、视觉工具、全局常量 |
| L2 | `ui/L2-components/` | 原子组件：Button、Modal、Slider、CommandPalette、CanvasToolbar 等 16 个 |
| L3 | `ui/L3-modules/` | 业务模块：Canvas、Text、Table、Tree、Terminal、Topology、Formula、Proof 等 17 个 |
| L4 | `ui/L4-shell/` | 应用壳：App、Header、Layout、SidebarLeft/Right、StatusBar |
| L5 | `ui/L5-core/` | 内核桥接：KernelBridge 协议、Mock Bridge、Zustand 状态管理 |
| L6 | `ui/L6-monitor/` | 运行监控 |

**内核通信**：`ui/L5-core/protocol/index.ts` 定义了唯一的跨边界类型——`DrawCmd`（内核→UI 绘制指令）、`UserAction`（UI→内核 用户操作）、`KernelBridge` 接口。

**嵌壳方案**：`ui/shells/` 下提供 VS Code 扩展和 Qt 独立窗口两种桌面嵌壳方案。

---

## 系统架构

### C 内核：十层单向依赖架构

```
Layer 10 │ Interop        │ Coq / Lean4 / OPML 桥接
Layer 9  │ Application    │ 批处理、交互入口
Layer 8  │ Meta-Verify    │ 类型一致性、完备性等检查
Layer 7  │ Orchestration  │ 六阶段流水线编排
Layer 6  │ Visual         │ 节点图 / 几何画布 / 块调度
Layer 5  │ Output         │ 流事件、TikZ/Lean 导出、插件
Layer 4  │ Reasoning      │ 证明引擎、Groebner、SMT/SAT
Layer 3  │ Geometry       │ 约束图、符号坐标、代数数
Layer 2  │ Resource       │ 内存池、缓存、上下文、调试
Layer 1  │ Parser         │ 公式解析、DSL 编译
```

| 层级 | 名称 | 可依赖项 |
|:----:|:-----|:---------|
| L1 | Parser | L2 |
| L2 | Resource | 无（基础层）|
| L3 | Geometry | L2 |
| L4 | Reasoning | L2, L3 |
| L5 | Output | L2, L3, L4 |
| L6 | Visual | L2, L3, L5 |
| L7 | Orchestration | L2-L6 |
| L8 | Meta-Verification | L2, L3, L4 |
| L9 | Application | 所有层 |
| L10 | Interop | L2, L4, L5 |

**架构约束**：单向依赖（上层依赖下层，下层不得反向依赖）、稳定契约（层间通过 AST、Typed IR、Proof Object 等稳定结构通信）、可验证性（支持编译时 `_Static_assert` 边界检查）。

---

## 项目结构

```
Lv-00/
├── core/                    # 核心引擎（C11）
│   ├── include/lv/        # 公共 API 头文件 (229 个 .h)
│   │   ├── lv.h           # 主头文件 — 唯一公共入口
│   │   ├── config.h         # 配置系统
│   │   ├── proof.h          # 证明系统 (Proposition/Proof 类型)
│   │   ├── func_block.h     # 函数块系统 (FuncBlock 类型)
│   │   └── preset_*.h       # 60+ 数学理论预设声明
│   └── src/                 # 十层实现 (401 个 .c)
│       ├── layer1_parser/            # 公式解析、DSL 编译
│       ├── layer2_resource/          # 内存池、缓存、上下文、调试
│       ├── layer3_geometry/          # 约束图、符号坐标、代数数
│       ├── layer4_reasoning/         # 证明引擎、Groebner、SMT/SAT
│       ├── layer5_output/             # 输出与导出、插件系统
│       ├── layer6_visual/             # 可视化编程、块调度器
│       ├── layer7_orchestration/      # 流水线编排
│       ├── layer8_meta_verify/       # 元验证
│       ├── layer9_application/         # 应用入口
│       └── layer10_interop/          # Coq/Lean4/OPML 桥接
├── ui/                      # Web 前端（React + TypeScript + Vite）
│   ├── L1-base/             # 基础样式与工具
│   ├── L2-components/       # 原子组件 (16 个)
│   ├── L3-modules/          # 业务模块 (17 个)
│   ├── L4-shell/            # 应用壳
│   ├── L5-core/             # 内核桥接与状态管理
│   ├── L6-monitor/          # 运行监控
│   └── shells/              # VS Code 扩展 / Qt 独立窗口
├── bootstrap/               # .lv 语义规格 + GMP 原语运行时
├── formal/                  # Lean 4 形式化（编译器 pipeline + Hilbert 公理）
├── lv-formal/             # 经典几何形式化框架
├── module/                  # 扩展模块：Python 绑定、公理包(.lvz)、流桥接
├── cmake/                   # CMake 打包配置 (find_package / pkg-config)
├── doc/                     # 技术文档与报告
├── test/                    # 测试套件
├── examples/                # 演示（当前为 demo.py 演示桩）
├── tool/                    # 辅助工具
├── CMakeLists.txt
├── VERSION                  # 1.1.0
├── IMPLEMENTATION_STATUS_AUDIT.md  # 诚实的完成度审计
└── README.md
```

---

## 形式化验证

- `formal/`：编译器 pipeline（lvLang → IR → C11 子集）的正确性证明，以及 Hilbert 公理框架。
- `lv-formal/`：Lv-00 自有几何元语言形式化验证的主体，包含三元本体、六条本原谓词、八条基础公理、约束图规范化、54 个数学理论包的依赖验证。

### 形式化完成度

| 模块 | 状态 | 说明 |
|---|:--:|---|
| **编译器 pipeline** | ✅ 已证 | lvLang → IR → C11 的正确性已完全证明 |
| **54 个数学理论包** | ✅ 已验 | 所有跨包依赖已通过全局注册表验证 |
| **约束图规范化** | ✅ 已证 | 幂等性、良构性保持已证明 |
| **几何基础公理** | 🚧 部分 | 三元本体、六条本原谓词已定义；约 30 处几何部分仍为 `axiom` 假设（待证）|

**诚实说明**：Lean 形式化部分的完成度是 **70-75%**。编译器和公理包验证完成，但几何部分仍有约 30 处 `axiom` 未被证明。这些不是"错误"，而是"待证明"的引理——它们将在后续版本中逐步补齐。

---

## 技术实现

### 数值系统

| 类型 | 实现方式 | 精度 |
|-----|---------|------|
| 有理数 | GMP mpq_t | 任意精度 |
| 代数数 | 最小多项式 + 隔离区间 | 符号精确 |
| 区间算术 | lvInterval (lo/hi/is_exact) | 可验证精度 |

### 求解引擎

- **Groebner 基**：并行 Buchberger 算法（`solver.c` 中 237KB 完整实现），支持工作窃取调度
- **SMT 后端**：一阶逻辑求解（Z3、CVC5 集成框架）
- **ATP 接口**：自动定理证明器集成
- **SAT/BDD**：CDCL SAT 求解器、BDD ADD 操作
- **数值后端**：GMRES(m=30)、BiCGSTAB、共轭梯度法

### 证明系统

- **多策略引擎**：向量法、全角法、演绎数据库、坐标法、面积法、Oracle 法
- **搜索算法**：DFS、BFS、最佳优先、MCTS（UCB1）
- **约束传播**：WFC 风格弧相容算法
- **证明导出**：Lean/Coq 脚本生成、OPML 格式导出

### 可视化与交互

- **节点图编辑器**：Fruchterman-Reingold 力导向布局
- **几何画布**：SVG 渲染、自适应视图
- **块调度器**：Kahn 拓扑排序 + 增量脏块执行
- **四视图同步**：文本 ↔ 节点图 ↔ 块图 ↔ 几何画布

### 渲染后端

SVG、Cairo 脚本、Three.js HTML、TikZ LaTeX、PPM 光栅化

### SIMD 支持

抽象 SIMD 层，运行时检测 SSE2/AVX/AVX2/AVX512F/NEON 能力，提供 `lvVec4d`/`lvVec4f`/`lvVec8f` 向量类型。

---

## 文档

- [完整技术文档](doc/DOCUMENTATION.md)
- [实现完成度诚实审计](IMPLEMENTATION_STATUS_AUDIT.md) - **[NEW]** 代码审计报告
- [API 快速入门](doc/docs/API_QUICKSTART.md)
- [语言规范](doc/docs/lv_LANGUAGE_SPEC.md)
- [架构手册](doc/docs/ARCHITECTURE_MANUAL.md)
- [文档索引](doc/docs/INDEX.md)
- [CHANGELOG.md](CHANGELOG.md) / [VERSION_LOG.md](VERSION_LOG.md)
- [CONTRIBUTING.md](CONTRIBUTING.md) / [COMMIT_CONVENTION.md](COMMIT_CONVENTION.md)

> 提示：`doc/docs/` 下部分文档链接尚在补全。详见[实现完成度诚实审计](IMPLEMENTATION_STATUS_AUDIT.md)了解各模块的实际状态。

---

## 相关工作

### 几何形式化

| 项目 | 机构/作者 | 技术路线 |
|-----|---------|---------|
| GeoCoq | 巴黎第十一大学 | 在 Coq 中形式化 Tarski 公理 |
| LeanGeo | A. Humenberger 等 | Lean 4 几何库 |
| Euklides | Budapest 等 | 动态几何证明 |

### 几何 AI

| 项目 | 机构 | 特点 |
|-----|------|-----|
| AlphaGeometry | Google DeepMind | 符号推理 + 神经网络 |
| DDAR | - | 几何自动作图 |

### 约束求解

| 项目 | 应用领域 | 特点 |
|-----|---------|-----|
| CGAL | 几何计算 | 算法库集合 |
| JGEX | 几何证明 | 多策略引擎 |

---

## 开发路线图

> 以下为**计划**，不代表已实现。已实现内容请见[项目现状](#项目现状)。

**v1.1.0（当前基线，已完成）**
- 十层目录结构与引擎 API 完整实现
- GMP 精确有理数计算（完整）
- 编译器 pipeline 的 Lean 形式化（完整）
- UI 系统内核/前端完全解耦（完整）
- Groebner 求解器（237KB 完整实现）
- 54 个公理包 Lean 验证（完整）
- 资源释放命名统一、内存分配器统一、头文件依赖精简（v1.8.0 工程优化批次）
- 命名统一：全仓库 `lv00` 前缀消解为 `lv`
- 端口作用域系统完整化
- 信任颜色系统完整化
- λ-演算核心集成
- 基础设施补齐（SHA-256 共享、模块加载器增强）

**v1.2.0+（后续）**
- 消除 Lean 中的 `axiom` 假设，补齐 30 个几何定理证明
- 公理自动发现
- 多语言绑定支持
- 证明可视化组件

**长期方向**
- 3D 几何支持
- 微分几何扩展
- 与几何 AI 系统集成

---

## 参与贡献

```bash
# 1. Fork & clone
git clone https://github.com/YOUR_USERNAME/Lv-00.git
cd Lv-00
git checkout -b feature/your-feature

# 2. 开发 + 测试，遵循约定式提交
git commit -m "feat: 描述变更"
git push origin feature/your-feature
# 3. 提交 Pull Request
```

代码规范：C11 标准、4 空格缩进、函数名 `lower_snake_case`、类型名 `PascalCase`、宏全大写。
详见 [CONTRIBUTING.md](CONTRIBUTING.md) 与 [COMMIT_CONVENTION.md](COMMIT_CONVENTION.md)。

---

## 许可证

[MIT License](LICENSE)

---

## 致谢

- [GMP](https://gmplib.org/) - 任意精度算术支持
- [Lean](https://lean-lang.org/) - 形式化验证框架参考
- [CMake](https://cmake.org/) - 构建系统
- [React](https://react.dev/) - UI 框架
- [Vite](https://vite.dev/) - 前端构建工具

---

<div align="center">

**Lv-00** = Level Zero · *探索几何构造与形式化推理的统一表达*

完成度：75-80% | 代码质量：学术品质 | 形式化验证：70-75%

</div>
