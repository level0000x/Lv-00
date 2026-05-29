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

## 分支管理

本项目采用 [Git Flow](https://nvie.com/posts/a-successful-git-branching-model/) 风格的分支管理策略。详细规范请参阅 [.github/BRANCHING_STRATEGY.md](.github/BRANCHING_STRATEGY.md)。

### 分支类型

| 分支类型 | 命名规则 | 基于 | 合并目标 | 用途 |
|---------|---------|------|---------|------|
| main | `main` | - | - | 生产就绪代码 |
| develop | `develop` | main | - | 日常开发集成 |
| 功能分支 | `feature/*` | develop | develop | 新功能开发 |
| 修复分支 | `fix/*` | develop | develop | Bug 修复 |
| 文档分支 | `docs/*` | develop | develop | 文档更新 |
| 重构分支 | `refactor/*` | develop | develop | 代码重构 |
| 稳定分支 | `stable` | main | - | 长期稳定版本 |
| 发行分支 | `release/*` | develop | main, develop | 版本发布准备 |
| 热修复分支 | `hotfix/*` | main | main, develop | 紧急修复 |

### 快速开始

```bash
# 1. Fork 项目后克隆到本地
git clone <your-fork-url>
cd Lv-00

# 2. 添加上游仓库
git remote add upstream <original-repo-url>

# 3. 创建功能分支（从 develop）
git checkout develop
git pull upstream develop
git checkout -b feature/my-feature

# 4. 开发并提交
git add .
git commit -m "feat(scope): description"

# 5. 保持分支同步
git fetch upstream
git rebase upstream/develop

# 6. 推送到你的 Fork
git push -u origin feature/my-feature

# 7. 创建 Pull Request 到上游 develop 分支
```

### 分支保护规则

- **main**: 禁止直接推送，需 2 人审查，CI 通过
- **develop**: 禁止直接推送，需 1 人审查，CI 通过
- **release/***: 禁止直接推送，需 2 人审查

## 开发流程

1. **创建功能分支**从 `develop` 分支（详见分支管理）
2. **编写代码**并确保编译通过（零警告）
3. **添加测试**覆盖新增功能
4. **运行全部测试**确保回归通过
5. **更新文档**（如有 API 变更）
6. **提交 PR** 到 `develop` 分支并描述变更内容
7. **审查通过**后由维护者合并

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
