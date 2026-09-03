# 贡献指南

感谢您对 Lv-00 几何元语言内核的关注！

## 项目简介

Lv-00 是一个纯 C11 从零自主研发的轻量化初等几何形式化自动证明内核。项目独创"几何拓扑约束图逻辑推演为核心，符号代数精密校验为辅"双层联动推理架构。

## 开发环境

### 前置依赖

- **编译器**: GCC ≥ 11 / Clang ≥ 14 / MSVC ≥ 2019
- **构建系统**: CMake ≥ 3.16
- **GMP 家族（默认底层依赖集）**: GMP ≥ 6.2（当前必需）/ MPFR / MPFI / MPC（标准底层成员，接入按[外部依赖策略](docs/architecture/dependency-policy.md)）
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

- 编码/格式标准由 `.clang-format` / `.editorconfig` 机器强制；命名规则（`lv_` 前缀族）见 `core/include/lv/lv_api_spec.h` 头注释
- 使用 4 空格缩进
- 公共函数使用 `lv_PUBLIC_API` 宏标记
- 所有公共 API 添加 Doxygen 风格注释
- 内存分配统一使用 `lv_malloc` / `lv_free` / `lv_calloc`

### 外部依赖规范

- 遵循项目 **[外部依赖策略](docs/architecture/dependency-policy.md)**（代码标准 v1.0，2026-09-03 批准）：默认底层依赖 GMP/MPFR/MPFI/MPC；外包边界判据、外部材料三级分类与许可红线见该文档

### 十层架构规范

项目严格遵循十层单向依赖架构（详见 [README 系统架构](README.md#系统架构)）：

1. **第 1 层** - 输入解析层 (Parser)：词法分析、公式解析
2. **第 2 层** - 资源管理层 (Resource)：内存、错误码、调试
3. **第 3 层** - 几何拓扑层 (Geometry)：约束图、符号坐标
4. **第 4 层** - 公理推理层 (Reasoning)：引擎、求解器、证明
5. **第 5 层** - 结果输出层 (Output)：流式事件、TikZ 导出

**规则**：只允许上层调用下层，禁止跨层反向依赖。

### 提交信息规范

使用 Conventional Commits 格式，并禁止无实际差异的空提交进入主分支：

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

提交要求：

- 每次提交必须对应明确的代码、测试、文档或配置变更。
- 禁止每日定时、机器人或脚本生成无实际内容的静默提交。
- 自动化任务如需保存产物，应优先使用 CI artifact，不应直接写回主分支。
- 涉及接口、构建路径或目录结构调整时，必须同步更新 README、CMake 和 CI 配置。

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

## Python 贡献指南

### Python 代码风格标准

- 遵循 **PEP 8** 编码规范（使用 `flake8` 或 `pylint` 检查）
- 行长度限制为 **100 字符**（非 PEP 8 默认的 79）
- 使用 `black` 自动格式化工具（配置 `line-length = 100`）
- 使用 `isort` 管理导入排序
- 类型存根文件遵循 PEP 484 规范

### 类型注解要求

- 所有公共函数和方法必须包含完整的类型注解（参数和返回值）
- 使用 `from __future__ import annotations` 启用延迟注解求值（Python 3.7+）
- 复杂类型使用 `typing` 模块（`Optional`, `Union`, `List`, `Dict`, `Tuple` 等）
- Python 3.9+ 优先使用内置泛型（`list[int]` 而非 `List[int]`）
- 回调函数类型使用 `Callable[[ArgType, ...], ReturnType]`
- CI 流水线集成 `mypy --strict` 进行静态类型检查

### Python 测试要求

- 测试框架使用 `pytest`
- 测试文件命名：`test_<module>.py`
- 每个公共函数至少一个正常路径测试和一个异常路径测试
- 使用 `pytest-asyncio` 测试异步代码
- Mock 外部依赖（DashScope API 等），不依赖真实网络请求
- 测试覆盖率目标：新增代码 >= 80%

### 中文注释要求

- Python 模块的 docstring 使用中文（与 C 头文件注释风格一致）
- 类和公共方法的 docstring 遵循 Google 风格或 NumPy 风格
- 行内注释使用中文，解释"为什么"而非"是什么"
- 示例：

```python
class GeometrySession:
    """几何计算会话 —— 管理用户的一次几何构造过程。

    每个会话包含独立的约束图和证明上下文，
    会话超时后自动清理资源。

    Attributes:
        session_id: 会话唯一标识符
        graph: 关联的约束图实例
        created_at: 创建时间戳
    """

    def add_point(self, x: float, y: float) -> int:
        """添加几何点到约束图。

        Args:
            x: 点的 x 坐标
            y: 点的 y 坐标

        Returns:
            新创建的节点 ID

        Raises:
            ValueError: 坐标值超出允许范围时
        """
        # 检查坐标范围，防止数值溢出
        ...
```

## 联系方式

请通过 Issue 或 PR 进行交流。

## 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](LICENSE) 文件。
