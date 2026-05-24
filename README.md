# Lv-00 几何元语言

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Version](https://img.shields.io/badge/version-3.2.0-blue.svg?style=flat)](CHANGELOG.md)
[![CI](https://github.com/yourusername/lv00/actions/workflows/ci.yml/badge.svg)](https://github.com/yourusername/lv00/actions/workflows/ci.yml)

> **Lv-00 是唯一将几何构造、计算程序、一阶逻辑证明三者统一于同一语法体系的元语言。**

Lv-00 不是一个几何工具库，不是一个自动证明器，也不是一个依附于外部证明器的数学库——它是一种**新的品类**：让你同时完成"画图、算数、证明"三件事的语言。几何体本身是程序的实体、数据的承载者、证明的见证者。

### 生态定位

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

## 特性

- **符号坐标系统**：支持有理数、代数数、二次扩域和超越数
- **约束图**：表示几何对象（点、线段、区域）及其约束关系
- **归一化**：自动合并等价节点，保证幂等性
- **统一化**：验证构造是否满足命题模式
- **函数块系统**：支持打包、实例化、部分应用和组合子
- **证明系统**：支持命题创建、证明导航和爆炸原理
- **多策略证明引擎 (v3.2)**：8 种证明方法并存（借鉴 JGEX 架构）
- **类型系统**：宇宙层级、类型等价检查和类型推断
- **递归系统**：测度系统、递归深度监控和终止检查
- **Python DSL (v3.2)**：Workplane 工作平面 + AlgebraMode 代数模式 + 操作符变换链（借鉴 CadQuery/build123d/GAlgebra）
- **几何实体类型层次 (v3.2)**：借鉴 SymPy GeometryEntity 继承体系
- **扁平数组存储 (v3.2)**：借鉴 clifford flat array，SIMD 友好的紧凑数值存储

## 快速开始

### 依赖

- CMake 3.15+
- C11 编译器 (GCC, Clang, MSVC)
- GMP 库 (任意精度算术)

### Linux/macOS

```bash
# 安装依赖
# Ubuntu/Debian
sudo apt-get install cmake libgmp-dev

# macOS
brew install cmake gmp

# 构建
git clone https://github.com/USERNAME/lv00.git

cd lv00
mkdir build && cd build
cmake ..
cmake --build .

# 运行测试
ctest --output-on-failure
```

### Windows (MSYS2)

```bash
# 安装 MSYS2 和依赖
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-gmp

# 构建
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
cmake --build .

# 运行测试
ctest --output-on-failure
```

## 构建选项

```bash
# Debug 模式
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Release 模式
cmake .. -DCMAKE_BUILD_TYPE=Release

# 启用代码覆盖率 (GCC/Clang)
cmake .. -DENABLE_COVERAGE=ON

# 启用 Sanitizers (检测内存错误)
cmake .. -DENABLE_SANITIZERS=ON

# 仅构建库，不构建测试和示例
cmake .. -DBUILD_TESTS=OFF -DBUILD_EXAMPLES=OFF
```

## 使用示例

```c
#include "lv00.h"
#include <stdio.h>

int main() {
    // 创建约束图
    ConstraintGraph *g = graph_create();
    
    // 创建两个点
    SymbolicCoord *x1 = symbolic_coord_create_rational(0, 1);
    SymbolicCoord *y1 = symbolic_coord_create_rational(0, 1);
    SymbolicCoord *coords1[] = {x1, y1};
    graph_add_point(g, coords1, 2);
    
    SymbolicCoord *x2 = symbolic_coord_create_rational(3, 1);
    SymbolicCoord *y2 = symbolic_coord_create_rational(4, 1);
    SymbolicCoord *coords2[] = {x2, y2};
    graph_add_point(g, coords2, 2);
    
    // 创建线段
    graph_add_line_segment(g, 0, 1);
    
    // 归一化
    NormalizationResult *result = graph_normalize(g, false);
    printf("合并了 %d 个节点\n", result->merged_count);
    normalization_result_destroy(result);
    
    // 清理
    graph_destroy(g);
    return 0;
}
```

更多示例见 `examples/` 目录：
- `triangle_construction.c` - 等边三角形构造与证明
- `circle_intersection.c` - 圆与线段相交
- `function_composition.c` - 函数块组合与类型系统

## 测试

项目包含全面的测试套件：

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

### 测试覆盖

- **基础测试**：符号坐标、约束图、归一化
- **边界测试**：空图、单节点、重复约束
- **复杂图测试**：大规模图、约束冲突
- **函数块测试**：打包、实例化、组合子
- **求解器测试**：自由度计算、冲突检测
- **重写系统测试**：模式匹配、规则应用
- **类型系统测试**：类型创建、等价检查
- **证明系统测试**：命题、证明导航
- **递归系统测试**：测度、终止检查

## 项目结构

```
lv00/
├── include/lv00/      # 公共头文件
│   ├── lv00.h         # 主头文件
│   ├── lv00_internal.h # 内部宏定义
│   ├── symbolic_coord.h / constraint_graph.h / ...
│   └── preset_*.h     # 预设模块头文件
├── src/
│   ├── core/          # 核心引擎（求解器、归一化、重写、类型系统等）
│   ├── func_block/    # 函数块系统（打包、实例化、组合、确定性检查等）
│   ├── preset/        # 预设函数块模块（几何、代数、拓扑、逻辑等42个模块）
│   ├── parser/        # 公式解析、转换、渲染
│   ├── utils/         # 工具函数
│   ├── axiom/         # 公理包
│   ├── interop/       # 互操作
│   ├── magic/         # Magic 模块
│   └── _deprecated/   # 废弃文件归档
├── tests/             # 测试文件
├── examples/          # 示例程序
├── docs/              # 文档
│   ├── reports/       # 历史汇报文档
│   └── *.md           # 设计文档
├── scripts/           # 工具脚本
├── .github/workflows/ # CI 配置
├── CMakeLists.txt
└── README.md
```

## 文档

- [API使用指南](docs/API_USAGE_GUIDE.md) - 详细的API参考和最佳实践
- [分层架构设计 v3.2](docs/architecture_v3.2.md) - **新增：OCCT 风格 7 层架构（2026-05-24）**
- [竞品分析](docs/competitive_analysis.md) - **已更新：22 个参考项目（2026-05-24）**
- [模块文档](docs/) - 各模块的详细设计文档
- [实现路线图](IMPLEMENTATION_ROADMAP.md) - 开发计划和进度

## 贡献

欢迎贡献！请遵循以下步骤：

1. Fork 仓库
2. 创建特性分支 (`git checkout -b feature/amazing-feature`)
3. 提交更改 (`git commit -m 'Add amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 创建 Pull Request

### 代码规范

- 遵循 C11 标准
- 使用 4 空格缩进
- 函数命名使用小写加下划线
- 类型命名使用 PascalCase
- 宏命名使用全大写

## 相关项目

Lv-00 的设计深受以下项目的启发（详见[竞品分析](docs/competitive_analysis.md)，共 22 个参考项目）：

### 第一梯队：直接相关
| 项目 | 借鉴方向 |
|:---|:---|
| [LeanGeo](https://github.com/ahumenberger/LeanGeo) | 几何形式化的公理组织 |
| [GeoCoq](https://github.com/GeoCoq/GeoCoq) | Tarski 公理体系的模块化分层 |
| [AlphaGeometry](https://github.com/google-deepmind/alphageometry) | 自然语言证明输出 |
| [Newclid](https://github.com/leomlopes/newclid) | 证明搜索与回溯可视化 |

### v3.2 新增借鉴
| 项目 | 借鉴方向 |
|:---|:---|
| [JGEX](https://github.com/kovzol/Java-Geometry-Expert) | 多证明方法并存引擎（8 种策略） |
| [CadQuery](https://github.com/CadQuery/cadquery) | Fluent API + Selector DSL + Workplane |
| [build123d](https://github.com/gumyr/build123d) | 代数模式无状态设计 + 操作符变换链 |
| [GAlgebra](https://github.com/pygae/galgebra) | 操作符重载数学映射 |
| [Ganja.js](https://github.com/enkimute/ganja.js) | inline AST 转译 DSL 技术 |
| [OpenCASCADE](https://dev.opencascade.org) | 7 模块分层架构 |
| [SymPy Geometry](https://github.com/sympy/sympy) | GeometryEntity 类型层次 |
| [clifford](https://github.com/pygae/clifford) | flat array 数值存储策略 |
| [Grassmann.jl](https://github.com/chakravala/Grassmann.jl) | 编译期类型级代数 |
| [OpenGeometry](https://opengeometry.cn) | 国产开源几何内核生态 |

### 其他参考
| 项目 | 借鉴方向 |
|:---|:---|
| [CGAL](https://www.cgal.org) | API 文档组织与模块分类 |
| [Penrose](https://penrose.cs.cmu.edu) | 数学关系→可视化的叙事方式 |
| [GeoGebra](https://www.geogebra.org) | 几何对象的命名与引用体系 |
| [GAP](https://www.gap-system.org) | 包管理与生态建设 |

## 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE) 文件

## 致谢

- [GMP](https://gmplib.org/) - 任意精度算术库
- [CMake](https://cmake.org/) - 构建系统
