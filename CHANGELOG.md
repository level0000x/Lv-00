# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [3.3.0] - 2026-05-25

### 新增 (Added)
- **五层单向依赖架构**：Parser → Resource → Geometry → Reasoning → Output 层级划分
- **编译时层级边界验证**：ENABLE_LAYER_VALIDATION 宏，构建期检测跨层违规依赖
- **Groebner 基求解引擎增强**：多项式理想求解能力提升
- **SMT 后端集成**：支持 SMT 求解器作为证明后端
- **ATP（自动定理证明器）后端**：集成自动定理证明器接口
- **SAT/BDD 编码**：命题逻辑可满足性求解与二元决策图支持
- **环形日志缓冲区**：高性能固定大小日志系统，支持结构化日志输出
- **对象缓存系统（LRU）**：最近最少使用缓存，加速重复对象访问
- **集中化配置系统**：LV00_CONFIG_* 前缀统一配置项，替代散落的宏定义
- **统一错误码系统**：分层 0-999 错误码体系，覆盖全部模块
- **函数块确定性检查**：静态分析 + 动态运行时双重检查机制
- **函数块选择器**：5 种选择策略（优先级、轮询、随机、加权、自适应）
- **55+ 数学理论预设模块**：覆盖几何、代数、拓扑、逻辑等领域
- **47 种流式事件类型**：完整的流式处理事件分类体系
- **公理包系统**：SHA-256 完整性校验、模板展开机制
- 新增三值逻辑模块实现（`three_valued_logic.c`）：Kleene 强三值逻辑完整运算
- 新增模态算子模块实现（`modal_operators.c`）：Kripke 语义框架与模态推理
- 新增量词系统模块实现（`quantifier.c`）：全称/存在/唯一存在量词管理
- 新增 7 个公理包注册（INDEX.json）：Presburger 算术、量子信息论、二阶算术等
- 新增 `llm_coding_assistant/requirements.txt` 依赖声明
- 新增高级功能 C 示例：证明系统、类型系统、递归演示

### 修复 (Fixed)
- **版本号统一**：README.md、error_codes.h、web/index.html 版本号同步至 3.3.0
- **内存安全**：engine.c 中 `free()` 统一为 `lv00_free()`
- **内存安全**：src/core/ 下 100+ 处 `malloc()` 迁移至 `lv00_malloc()`
- **公理包完整性**：INDEX.json 补全 7 个未注册包及依赖关系
- **Web 前端**：github-demo.html 重复 class 属性修复
- **安全加固**：llm_coding_assistant 默认密码改为环境变量注入
- **安全修复**：WebSocket 认证机制完善、CSP 白名单配置、XSS 防护增强、密码管理安全加固
- **代码质量**：类型注解统一、ID 生成器统一、字符串安全处理、死代码清理
- **性能优化**：Canvas 渲染节流（throttle）、requestAnimationFrame 按需启动
- **架构改进**：FormulaPanel 组件拆分、头文件注释规范完善
- **文档更新**：SECURITY.md 安全策略、CONTRIBUTING.md 贡献指南、TODO_TRACKING.md 任务跟踪
- **工程规范**：README.md 占位符替换为实际项目地址、web-deploy.yml 路径修正（web/ → web-gui/）
- **CI 增强**：Python CI 新增 flake8 + mypy 静态检查步骤、requirements.txt 补全开发依赖
- **CLI 中文化**：llm_coding_assistant 命令行帮助信息改为中文
- **配置统一**：.editorconfig 行长度规则添加注释说明

### 变更 (Changed)
- **架构升级**：OCCT 风格 7 层架构重构为五层单向依赖架构
- **CI/CD**：python.yml 扩展覆盖 concurrent_monitor 和 llm_coding_assistant
- **架构文档**：ARCHITECTURE_v3.3.md 文件映射表补全
- **代码规范**：统一所有头文件版本标注

### 内部 (Internal)
- 全部头文件 @version 标签更新为 3.3.0
- 全部源文件 @version 标签更新为 3.3.0
- 预设模块目录扩展至 55+ 个 .c 文件
- 流式事件类型枚举扩展至 47 种
- 错误码头文件重构为分层编号体系

## [3.2.0] - 2026-05-24

### 新增
- 新增 `.clang-format` 代码格式化配置，统一 C11 代码风格
- 新增 `stream_context_util.c` 流式上下文注册分发系统，支持集中化管理
- 新增 `func_block_registry` FNV-1a 哈希表加速（O(1) 查找，含优雅降级）
- 新增 `src/_deprecated/README.md` 废弃文件说明文档
- 新增 `package.json` 中实用的构建/测试/格式化脚本命令
- 新增 `stream_context_register_builtins()` 内置模块自动注册
- 新增 `LV00_REGISTER_STREAM_CTX` 便捷注册宏

### 修复
- **版本号统一**：所有模块版本号统一为 `3.2.0`（CMake/lv00.h/Python/web-gui/concurrent_monitor）
- 修复 `engine.c` 中流式上下文手动逐模块注册的耦合问题，改用自动分发
- 修复 `solver.c` 中 18 处散落 TODO(设计决策) 注释，统一为 GMP 标准分配器说明
- 修复 `func_block_registry.c` 中 `unregister` 函数缺少锁保护的问题
- 修复 `stream_context_util.h` 中缺少注册机制的问题

### 改进
- **代码风格**：统一 include 风格、添加 `.clang-format` 配置
- **内存安全**：CI 新增 macOS sanitizer 构建步骤（Linux 已有）
- **文档完整性**：废弃目录添加说明文档、CHANGELOG 补全
- **工程规范性**：`package.json` 添加 cmake 构建脚本、格式化命令
- **注释质量**：多处 TODO → 正式设计决策文档注释

### 内部变更
- `include/lv00/` 全部头文件 @version 标签更新为 3.2.0
- `src/` 全部源文件 @version 标签更新为 3.2.0
- `python/` 全部 Python 绑定版本字符串更新为 3.2.0
- `concurrent_monitor/` 版本号同步为 3.2.0
- `web-gui/` Tauri 和前端版本号同步

## [3.0.1] - 2026-05-20
### 修复
- 修复 interop.c 和 high_dim.c 中桩函数返回 LV00_OK 的静默失败问题
- 修复 GeoJSON 导出硬编码占位数据问题
- 修复 proof.c setter 函数 malloc 失败时状态不一致
- 修复 symbolic_coord.c 中浮点比较和类型转换安全风险
- 修复 mpz_poly_div 除零检查缺失
- 消除 axiom_pkg.c/module.c 词法分析器重复代码
- 合并 func_block.c 确定性检查函数重复逻辑
### 改进
- 添加 CMake install() 规则
- 完善 .gitignore
- 统一 LV00_THREAD_LOCAL 宏定义
- 优化 unify.c 哈希过滤版本端口匹配

## [3.0.0] - 2026-05-18

### Added

#### Core Features
- **Symbolic Coordinate System** - Support for rational numbers, algebraic numbers, quadratic extensions, and transcendental numbers
- **Constraint Graph** - Geometric objects (points, line segments, regions) with constraint relationships
- **Normalization Engine** - Automatic merging of equivalent nodes with idempotency guarantee
- **Unification System** - Pattern matching for verifying constructions against propositions
- **Function Block System** - Functional abstraction for geometric constructions with packing, instantiation, and partial application
- **Proof System** - Proposition creation, proof navigation, and ex falso principle
- **Type System** - Universe levels, type equivalence checking, and type inference
- **Recursion System** - Measure system, recursion depth monitoring, and termination checking

#### Language Bindings
- **Python Bindings** - Complete Python API via ctypes with SymbolicCoord, Graph, Point, LineSegment classes
- **WebAssembly Support** - Browser-compatible build with JavaScript bindings

#### Testing & Quality
- **13 C Unit Tests** - Comprehensive test coverage for all modules
- **35+ Python Tests** - pytest-based test suite
- **Fuzzing Targets** - libFuzzer-based fuzz testing for constraint graphs and symbolic coordinates
- **Benchmark Suite** - Performance tracking with 15 test cases
- **CI/CD Pipeline** - 6 GitHub Actions workflows for multi-platform testing

#### Documentation
- **API Usage Guide** - Comprehensive API reference
- **10 Module Design Docs** - Detailed design documentation for each subsystem
- **Project Showcase** - High-level project overview and highlights
- **Web Demo** - Interactive browser-based visualization

### Changed

#### Build System
- Refactored CMakeLists.txt to eliminate repetition (200+ lines → 170 lines)
- Added `add_lv00_test()` helper function for consistent test registration
- Improved GMP library detection with `GMP_ROOT` environment variable support
- Centralized all `option()` declarations at file beginning

#### Code Quality
- Unified version number definitions across all headers
- Added `extern "C"` protection to all 16 public headers for C++ compatibility
- Resolved `CrossBoundaryConstraint` type conflict between `func_block.h` and `constraint_graph.h`
- Fixed `func_block.c` to properly include its own header instead of manual type redeclaration

#### Project Structure
- Moved temporary/debug tests to `tests/manual/` directory
- Relocated design documents to `docs/` directory
- Removed empty and duplicate files from repository root

### Fixed

- **Memory Safety** - Fixed double-free issues in test files (proof.c, recursion.c)
- **Type Safety** - Resolved type redefinition conflicts
- **Build Reliability** - Fixed GMP static library linking on Windows
- **CI/CD** - All 13 tests now properly registered and passing

### Removed

- Empty files: `design1.txt`, `design2.txt`
- Duplicate documents: `设计`, `设计.txt`, `规划`, `规划.txt`
- Temporary test files moved to `tests/manual/`: `test_gmp.c`, `test_custom.c`, `test_debug.c`, `test_simple.c`, `test_lv00_direct.c`, `test_debug_basic.c`, `test_comprehensive.c`

---

## Release Statistics

- **Source Files**: 14 C source files (~15,000 lines)
- **Header Files**: 16 public headers
- **Test Files**: 13 C tests + 2 Python test files
- **Test Cases**: 100+ test cases
- **CI Workflows**: 6 GitHub Actions workflows
- **Documentation**: 15+ documentation files
- **Examples**: 3 C examples + 1 Python example + 1 Web demo

## Compatibility

- **C Standard**: C11
- **Supported Platforms**: Linux, macOS, Windows (MSYS2/MinGW)
- **Dependencies**: GMP (GNU Multiple Precision Arithmetic Library)
- **Python**: 3.10+
- **Browsers**: Modern browsers with WebAssembly support

## Contributors

Thanks to all contributors who made this release possible!

---

[3.0.0]: https://github.com/lv00-project/lv00/releases/tag/v3.0.0
