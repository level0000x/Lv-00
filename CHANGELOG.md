# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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

[3.0.0]: https://github.com/yourusername/lv00/releases/tag/v3.0.0
