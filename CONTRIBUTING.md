# 贡献指南

感谢您对 Lv-00 几何元语言内核的关注！

## 项目简介

Lv-00 是一个纯 C11 从零自主研发的轻量化初等几何形式化自动证明内核。项目独创"几何拓扑约束图逻辑推演为核心，符号代数精密校验为辅"双层联动推理架构。

## 开发环境

### 前置依赖

- **编译器**: GCC ≥ 11 / Clang ≥ 14 / MSVC ≥ 2019
- **构建系统**: CMake ≥ 3.16
- **GMP**: GNU Multiple Precision Arithmetic Library ≥ 6.2
- **Python** (可选): ≥ 3.8（用于 Python 绑定和辅助工具）
- **Node.js** (可选): ≥ 18（用于 Web GUI）

### 环境搭建

```bash
# 克隆仓库
git clone <repo-url>
cd Lv-00

# 配置构建
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 编译
cmake --build build

# 运行测试
cd build && ctest
```

## 代码规范

### C 语言规范

- 遵循项目 **[编码标准](docs/CODING_STANDARD.md)**
- 遵循项目 **[命名规范](docs/NAMING_CONVENTION.md)**
- 使用 4 空格缩进
- 公共函数使用 `LV00_PUBLIC_API` 宏标记
- 所有公共 API 添加 Doxygen 风格注释
- 内存分配统一使用 `lv00_malloc` / `lv00_free` / `lv00_calloc`

### 五层架构规范

项目严格遵循五层单向依赖架构（详见 [ARCHITECTURE_v3.3.md](docs/ARCHITECTURE_v3.3.md)）：

1. **第 1 层** - 输入解析层 (Parser)：词法分析、公式解析
2. **第 2 层** - 资源管理层 (Resource)：内存、错误码、调试
3. **第 3 层** - 几何拓扑层 (Geometry)：约束图、符号坐标
4. **第 4 层** - 公理推理层 (Reasoning)：引擎、求解器、证明
5. **第 5 层** - 结果输出层 (Output)：流式事件、TikZ 导出

**规则**：只允许上层调用下层，禁止跨层反向依赖。

### 提交信息规范

使用 Conventional Commits 格式：

```
<type>(<scope>): <description>

type: feat, fix, refactor, docs, test, chore, perf
scope: solver, proof, stream, gui, docs, build 等
```

示例：
```
feat(solver): 新增 Groebner 基增量求解
fix(proof): 修复反证法数据回滚异常
refactor(constraint_graph): 按功能拆分模块
```

## 开发流程

1. **Fork 项目**并创建功能分支
2. **编写代码**并确保编译通过（零警告）
3. **添加测试**覆盖新增功能
4. **运行全部测试**确保回归通过
5. **更新文档**（如有 API 变更）
6. **提交 PR** 并描述变更内容

## 测试要求

- 每个新增模块必须包含独立测试文件（命名 `test_<module>.c`）
- 测试使用项目统一测试框架（`test_helpers.h`）
- PR 必须通过全部现有测试

## 联系方式

请通过 Issue 或 PR 进行交流。

## 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE) 文件。
