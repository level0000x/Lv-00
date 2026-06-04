# Lv-00 几何元语言

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Version](https://img.shields.io/badge/version-5.0.0-blue.svg?style=flat)](CHANGELOG.md)
[![CI](https://github.com/lv00-project/lv00/actions/workflows/ci.yml/badge.svg)](https://github.com/lv00-project/lv00/actions/workflows/ci.yml)
[![C11](https://img.shields.io/badge/C-11-green.svg)](https://en.cppreference.com/w/c)
[![Lean4](https://img.shields.io/badge/Lean-4-purple.svg)](https://lean-lang.org/)

> **Lv-00 是唯一将几何构造、代数计算、一阶逻辑证明三者统一于同一语法体系的元语言。**

```
上层应用（CGAL / CAD / AI求解器 / 教育工具）
        ↑ 它们需要精确语义
   Lv-00：几何元语言（提供精确语义）
        ↑ 它们提供形式化基础
底层框架（Lean / Coq / 一阶逻辑 / 约束求解）
```

Lv-00 不是一个几何工具库，不是一个自动证明器，也不是一个依附于外部证明器的数学库——它是一种**新的品类**：让你同时完成"画图、算数、证明"三件事的语言。几何体本身是程序的实体、数据的承载者、证明的见证者。

- **不是** CGAL 那种供人调用的算法包
- **不是** AlphaGeometry 那种解题 AI
- **不是** LeanGeo 那种依附于外部证明器的数学库
- **而是** 一种同时完成构造、计算、证明的语言本身

---

## 十层单向依赖架构

Lv-00 采用严格的**十层单向依赖架构**，每层只能依赖同层或更底层的层，反向依赖被编译时 `_Static_assert` 检查禁止。

```
┌─────────────────────────────────────────────────────────────┐
│  Layer 10 │ Interop        │ Coq/Lean4/OPML 桥接            │
│  Layer 9  │ Application    │ 批处理、交互式 REPL             │
│  Layer 8  │ Meta-Verify    │ 类型一致性、完备性、可靠性检查   │
│  Layer 7  │ Orchestration  │ 六阶段流水线编排                │
│  Layer 6  │ Visual         │ 节点图、几何画布、块调度器       │
│  Layer 5  │ Output         │ 流事件、TikZ、证明编译器、插件   │
│  Layer 4  │ Reasoning      │ 证明引擎、Groebner、SMT/SAT    │
│  Layer 3  │ Geometry       │ 约束图、符号坐标、代数数         │
│  Layer 2  │ Resource       │ 内存池、缓存、上下文、调试       │
│  Layer 1  │ Parser         │ 公式解析、DSL 编译              │
└─────────────────────────────────────────────────────────────┘
```

| 层级 | 名称 | 可依赖项 |
|:----:|:-----|:---------|
| L1 | Parser | L2 |
| L2 | Resource | 无（基础层）|
| L3 | Geometry | L2 |
| L4 | Reasoning | L2, L3 |
| L5 | Output | L2, L3, L4 |
| L6 | Visual | L2, L3, L5 |
| L7 | Orchestration | L2~L6 |
| L8 | Meta-Verification | L2, L3, L4 |
| L9 | Application | 所有层 |
| L10 | Interop | L2, L4, L5 |

编译选项 `ENABLE_LAYER_VALIDATION`（默认开启）会在编译时插入 `_Static_assert` 检查，任何层违规都会产生即时编译错误。

---

## 核心特性

### 精确计算
- **符号坐标系统**：有理数、代数数、二次扩域、超越数——零浮点误差
- **约束图**：O(1) 哈希索引，JSON/DOT 导出，冗余检测，冲突分析
- **信任颜色**：GREEN → BLUE → YELLOW → ORANGE → RED，计算谱系全程可见

### 推理引擎
- **多策略证明引擎**：向量法、全角法、演绎数据库、坐标法、面积法、Oracle 法
- **搜索算法**：DFS、BFS、最佳优先、MCTS（UCB1）
- **求解器后端**：Groebner 基（并行 Buchberger）、CDCL SAT、SMT、BDD ADD、SOS 分解
- **数值后端**：GMRES(m=30)、BiCGSTAB、共轭梯度法

### 形式化验证
- **Lean 4 形式化**：Hilbert 公理体系、约束图规范化证明、双向桥接
- **Coq 桥接**：Vernacular 生成/导入、38 种 tactic 验证
- **OPML 编解码器**：理论公理 + 证明步骤 JSON 导出

### 可视化与交互
- **节点图编辑器**：Fruchterman-Reingold 力导向布局
- **几何画布**：SVG 渲染、自适应视图
- **块调度器**：Kahn 拓扑排序 + 增量脏块执行
- **四视图同步**：文本 ↔ 节点图 ↔ 块图 ↔ 几何画布

### 可扩展性
- **55+ 公理预设**：欧几里得几何、代数、拓扑、数论等领域
- **函数块系统**：打包、实例化、部分应用（柯里化）、组合子
- **插件系统**：动态加载、通配符匹配、语义版本比较
- **Python DSL**：Workplane + AlgebraMode + 操作符变换链

---

## 快速开始

### 依赖

- CMake 3.15+
- C11 编译器 (GCC, Clang, MSVC)
- GMP 库（任意精度算术，核心依赖）

### Linux / macOS

```bash
# Ubuntu/Debian
sudo apt-get install cmake libgmp-dev

# macOS
brew install cmake gmp

# 构建
git clone https://github.com/lv00-project/lv00.git
cd lv00
mkdir build && cd build
cmake ..
cmake --build .

# 运行测试
ctest --output-on-failure
```

### Windows (MSYS2)

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-gmp
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
ctest --output-on-failure
```

### 构建选项

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release          # Release 模式
cmake .. -DCMAKE_BUILD_TYPE=Debug            # Debug 模式
cmake .. -DENABLE_COVERAGE=ON                # 代码覆盖率
cmake .. -DENABLE_SANITIZERS=ON              # ASan/UBSan
cmake .. -DBUILD_WASM=ON                     # WebAssembly 目标
cmake .. -DENABLE_LAYER_VALIDATION=OFF       # 关闭层级验证（加速编译）
```

---

## 使用示例

### C API：构造并证明等边三角形

```c
#include "lv00/lv00.h"
#include <stdio.h>

int main(void) {
    lv00_init();
    LV00Engine *engine = lv00_engine_create();

    // 定义基准点
    int A = lv00_add_point(engine, 0, 1, 0, 1);  // (0, 0)
    int B = lv00_add_point(engine, 2, 1, 0, 1);  // (2, 0)

    // 尺规作图：两圆交点构造等边三角形
    int c1 = lv00_add_circle(engine, A, B);
    int c2 = lv00_add_circle(engine, B, A);
    int C  = lv00_add_intersection(engine, c1, c2);

    // 求解并验证
    lv00_normalize(engine, true);
    EngineSolveResult result = lv00_solve(engine);

    printf("Proof valid: %s\n",
        result.status == ENGINE_STATUS_OK ? "YES" : "NO");

    lv00_engine_destroy(engine);
    lv00_cleanup();
    return 0;
}
```

### Lv-00 DSL：声明式几何证明

```lv00
// 定义三个顶点
point A(0, 0);
point B(6, 0);
point C(3, 3*sqrt(3));

// 构造三角形
triangle ABC(A, B, C);

// 证明三边相等
prove {
    length(AB) == length(BC) == length(CA)
}

// 证明三个角均等于 60°
prove {
    angle(A, B, C) == angle(B, C, A) == angle(C, A, B) == 60°
}

// 可视化
visualize {
    style ABC: { fill: "lightblue", stroke: "blue" };
    label A: "A"; label B: "B"; label C: "C";
}
```

更多示例见 `examples/` 目录：
- `triangle_construction.c` — 等边三角形构造与证明
- `circle_intersection.c` — 圆与线段相交
- `function_composition.c` — 函数块组合与类型系统

---

## 项目结构

```
Lv-00/
├── core/
│   ├── include/lv00/              # 公共 API 头文件 (170+)
│   │   ├── lv00.h                 # 主头文件
│   │   ├── symbolic_coord.h       # 符号坐标系
│   │   ├── constraint_graph.h     # 约束图
│   │   ├── proof.h                # 证明系统
│   │   ├── func_block.h           # 函数块系统
│   │   └── preset_*.h             # 55+ 数学理论预设
│   └── src/                       # 十层实现
│       ├── layer1_parser/         # 输入解析与 DSL 编译
│       ├── layer2_resource/       # 内存、缓存、上下文
│       ├── layer3_geometry/       # 约束图、符号坐标、拓扑
│       ├── layer4_reasoning/      # 引擎、求解器、证明
│       ├── layer5_output/         # 导出、可视化、插件
│       ├── layer6_visual/         # 节点图、几何画布、块调度
│       ├── layer7_orchestration/  # 六阶段流水线
│       ├── layer8_meta_verify/    # 元验证五维检查
│       ├── layer9_application/    # 批处理、REPL
│       └── layer10_interop/       # Coq/Lean4/OPML 桥接
├── formal/                        # Lean 4 形式化验证
│   ├── Lv00Formal.lean            # 主形式化入口
│   └── Lv00Formal/                # 理论与公理包证明
├── module/                        # 扩展模块
│   ├── axiom_packages/            # 公理包定义 (.lvz)
│   ├── python/                    # Python 绑定与 DSL
│   └── stream_bridge/             # 流式事件桥接
├── examples/                      # 用法示例与模板
├── web/                           # Web GUI 与流监视器
├── test/                          # 测试套件
├── doc/                           # 文档
│   ├── DOCUMENTATION.md           # 完整技术文档 (26章)
│   ├── docs/                      # 模块详细设计文档
│   └── reports/                   # 历史汇报文档
└── tool/                          # 脚本与报告生成器
```

---

## 测试

```bash
# 运行所有测试
ctest

# 运行特定测试
ctest -R func_block_test

# 详细输出
ctest --output-on-failure

# 并行测试
ctest -j4
```

**测试覆盖**：符号坐标、约束图、归一化、函数块、求解器、重写系统、类型系统、证明系统、递归系统、边界测试、复杂图测试。

---

## 文档

| 文档 | 描述 |
|:-----|:-----|
| [完整技术文档 v5.0](doc/DOCUMENTATION.md) | **最新**：26 章全面技术手册（十层架构）|
| [API 使用指南](doc/docs/API_USAGE_GUIDE.md) | 详细 API 参考和最佳实践 |
| [架构手册](doc/docs/ARCHITECTURE_MANUAL.md) | 十层架构设计详解 |
| [语言规范](doc/docs/LV00_LANGUAGE_SPEC.md) | Lv-00 DSL 语法规范 |
| [教程](doc/docs/TUTORIAL.md) | 逐步入门教程 |
| [竞品分析](doc/docs/competitive_analysis.md) | 22 个参考项目分析 |
| [实现路线图](IMPLEMENTATION_ROADMAP.md) | 开发计划和进度 |
| [CHANGELOG](CHANGELOG.md) | 版本变更历史 |

---

## 相关项目

Lv-00 的设计深受以下项目启发（详见[竞品分析](doc/docs/competitive_analysis.md)）：

| 项目 | 借鉴方向 |
|:-----|:---------|
| [LeanGeo](https://github.com/ahumenberger/LeanGeo) | 几何形式化的公理组织 |
| [GeoCoq](https://github.com/GeoCoq/GeoCoq) | Tarski 公理体系的模块化分层 |
| [AlphaGeometry](https://github.com/google-deepmind/alphageometry) | 神经符号推理 |
| [JGEX](https://github.com/kovzol/Java-Geometry-Expert) | 多证明方法并存引擎 |
| [CadQuery](https://github.com/CadQuery/cadquery) | Fluent API + Selector DSL |
| [OpenCASCADE](https://dev.opencascade.org) | 分层架构设计 |
| [CGAL](https://www.cgal.org) | API 文档组织 |
| [Penrose](https://penrose.cs.cmu.edu) | 数学关系可视化 |

---

## 贡献

欢迎贡献！请遵循以下步骤：

1. Fork 仓库
2. 创建特性分支 (`git checkout -b feature/amazing-feature`)
3. 提交更改 (`git commit -m 'feat: add amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 创建 Pull Request

### 代码规范

- 遵循 C11 标准
- 4 空格缩进
- 函数命名：`lowercase_with_underscores`
- 类型命名：`PascalCase`
- 宏命名：`UPPER_CASE`

详见 [CONTRIBUTING.md](CONTRIBUTING.md)。

---

## 许可证

本项目采用 [MIT](LICENSE) 许可证。

## 致谢

- [GMP](https://gmplib.org/) — 任意精度算术
- [CMake](https://cmake.org/) — 构建系统
- [Lean 4](https://lean-lang.org/) — 形式化验证
