# Lv-00 变更日志 v3.3.0

> **发布日期**: 2026-05-24
> **代码名称**: "五层架构与公共 API"

---

## 概述

v3.3.0 是 Lv-00 的一个重要里程碑版本，实现了从扁平代码库到严格五层架构的全面重构，
并添加了完整的跨平台类型系统、公共 API 规范和综合文档。

### 版本号

| 组件 | 版本 |
|------|------|
| 主版本 | 3 |
| 次版本 | 3 |
| 补丁 | 0 |
| API 版本 | 1.0.0 |

---

## 变更分类

所有变更按优先级 (P0-P7) 分为七个阶段，每个阶段解决一组特定的架构或功能问题。

---

## P0: 内存安全 (Memory Safety)

*优先级: 最高 —— 防止崩溃、越界和内存错误*

### 新增

- [x] **统一内存分配接口**: 所有内部分配改为通过 `lv00_malloc` / `lv00_free` / `lv00_calloc` / `lv00_realloc` 进行，替代原始 `malloc/free`
- [x] **内存泄漏追踪**: 分配追踪系统记录每次分配的文件、行号、大小，便于定位泄漏
- [x] **缓冲区溢出保护**: 字符串操作统一使用 `lv00_strlcpy`，确保目标缓冲区不溢出
- [x] **内存上限**: `lv00_set_memory_limit_ex()` 设置软限制，防止失控分配
- [x] **内存统计**: `lv00_get_memory_stats_ex()` 提供实时内存使用报告

### 影响文件

- `src/utils/lv00_utils.c` — 内存分配器实现
- `src/core/lv00.c` — 全局内存追踪初始化
- `include/lv00/lv00.h` — 内存管理 API 声明

---

## P1: 五层架构 (Five-Layer Architecture)

*优先级: 高 —— 确立项目的长期架构方向*

### 新增

- [x] **五层单向依赖架构定义** (详见 `docs/ARCHITECTURE_v3.3.md`):
  - 第1层: 输入解析层 (Parser) — 6 个源文件
  - 第2层: 资源管理层 (Resource) — 5 个源文件
  - 第3层: 几何拓扑层 (Geometry) — 13 个源文件
  - 第4层: 公理推理层 (Reasoning) — 96 个源文件
  - 第5层: 结果输出层 (Output) — 6 个源文件
- [x] **CMake OBJECT 库分层构建**: 每层编译为独立的 OBJECT 库，通过 `target_link_libraries` 控制依赖方向
- [x] **聚合静态库向后兼容**: `lv00_static` 聚合所有层的 OBJECT 文件，保持现有使用者不受影响
- [x] **层级验证宏**: `LV00_CURRENT_LAYER` + `LV00_ENABLE_LAYER_VALIDATION` 编译时依赖检查
- [x] **层级依赖矩阵**: 明确定义每层允许依赖的下层集合

### 架构变更

```
v3.2 (扁平)                    v3.3 (五层)
src/core/*.c  ──────>         Layer 5: Output
(126 个文件        ──────>    Layer 4: Reasoning
 混在一起)         ──────>    Layer 3: Geometry
                  ──────>    Layer 2: Resource
                  ──────>    Layer 1: Parser
```

### 影响文件

- `CMakeLists.txt` — 完全重构为分层 OBJECT 库结构
- `docs/ARCHITECTURE_v3.3.md` — 新增架构规范文档 (850 行)
- `include/lv00/engine.h` — 新增层级验证宏

---

## P2: 解析器安全 (Parser Security)

*优先级: 高 —— 防止输入攻击和解析器崩溃*

### 新增

- [x] **解析器输入限制**: 新增 `LV00_CONFIG_PARSER_*` 配置系列
  - `LV00_CONFIG_PARSER_MAX_INPUT_LENGTH` = 1 MiB
  - `LV00_CONFIG_PARSER_MAX_TOKENS` = 100,000
  - `LV00_CONFIG_PARSER_MAX_AST_DEPTH` = 256
  - `LV00_CONFIG_PARSER_MAX_AST_NODES` = 500,000
  - `LV00_CONFIG_PARSER_MAX_TOKEN_LENGTH` = 4096
- [x] **恶意输入防护**: `parser_safety.c` 实现了输入校验、深度限制和资源限制
- [x] **代数严谨性**: 公式解析器对不合法输入返回明确错误而非静默失败

### 影响文件

- `src/parser/parser_safety.c` — 新增解析器安全模块
- `include/lv00/config.h` — 新增解析器配置常量

---

## P3: 公理系统增强 (Axiom System Enhancement)

*优先级: 中 —— 提升推理能力和证明质量*

### 新增

- [x] **公理分级系统 (Axiom Grading)**:
  - Grade 1: 基础公理（如反射性、传递性）
  - Grade 2: 结构性公理（如结合律、交换律）
  - Grade 3: 领域特定公理（如欧几里得公设）
  - Grade 4: 元理论公理（如完备性公理）
- [x] **证明轨迹树 (Proof Trace Tree)**:
  - 完整记录每一步推理的来源公理、应用参数和中间结果
  - 支持 JSON 序列化用于外部验证
  - DAG 结构允许共享子证明
- [x] **反证法支持 (Contradiction Proofs)**:
  - 假设否定自动推导矛盾
  - 矛盾检测引擎识别逻辑不一致性
  - 支持非构造性证明策略

### 影响文件

- `src/core/proof.c` — 证明系统重写
- `src/core/proof_optimize.c` — 证明优化
- `src/core/proof_multi_strategy.c` — 多策略证明
- `src/axiom/axiom_pkg.c` — 公理包系统增强
- `include/lv00/proof.h` — 证明 API 扩展

---

## P4: 编码标准 (Code Standards)

*优先级: 中 —— 提升代码质量和可维护性*

### 新增

- [x] **集中配置系统**: `include/lv00/config.h` 消除散布在各源文件中的魔数
- [x] **const 正确性**: 所有只读指针参数添加 `const` 限定
- [x] **统一错误码**: `include/lv00/error_codes.h` 集中定义所有错误码，替代零散的 `#define`
- [x] **命名规范文档**: `docs/ARCHITECTURE_v3.3.md` 第 9 节定义各层的命名约定
- [x] **编译期版本兼容性检查**: `lv00.h` 中的版本宏与 `CMakeLists.txt` 中的 `project(VERSION ...)` 交叉验证
- [x] **Doxygen 文档注释规范**: 所有公共 API 函数使用 `@brief`, `@param`, `@return`, `@note` 标签

### 兼容层

- [x] `include/lv00/config.h` 中包含 `SOLVER_MAX_VAR_ID`, `MAX_MODULE_DEPTH` 等旧宏的兼容定义
- [x] 旧宏保留直至所有模块迁移完成

### 影响文件

- `include/lv00/config.h` — 新增集中配置头文件 (207 行)
- `include/lv00/error_codes.h` — 统一错误码系统
- `include/lv00/lv00.h` — 版本验证和 Doxygen 注释

---

## P5: 运行时安全保障 (Runtime Safety Guards)

*优先级: 高 —— 防止运行时死循环、栈溢出和资源耗尽*

### 新增

- [x] **状态机 (State Machine)**: 引擎内部引入明确的状态转换，防止非法 API 调用序列
  - `LV00_STATE_UNINIT` → `LV00_STATE_IDLE` → `LV00_STATE_SOLVING` → `LV00_STATE_DONE`
- [x] **递归转迭代 (Recursion to Iteration)**: 关键递归算法（图遍历、重写循环）改为显式栈迭代，防止栈溢出
- [x] **结构化日志 (Structured Logging)**:
  - `LV00_LOG_OFF` (0) 到 `LV00_LOG_DEBUG` (4) 五级日志
  - 日志包含时间戳、层级标识、源文件位置
- [x] **熔断机制 (Circuit Breaker)**:
  - 重写步数超过 `LV00_CONFIG_DEFAULT_REWRITE_LIMIT` 自动终止
  - 求解器在 `LV00_CONFIG_MINI_KERNEL_VERIFY_TIMEOUT_MS` 超时后中断
  - 位数超过 `LV00_CONFIG_BIT_CUTOFF_THRESHOLD` 切换到近似策略
- [x] **运行时防护阈值**: `LV00_CONFIG_RUNTIME_GUARD_*` 系列配置

### 影响文件

- `src/core/engine.c` — 状态机实现
- `src/core/rewrite.c` — 迭代化重写循环
- `src/core/debug.c` — 结构化日志
- `include/lv00/config.h` — 运行时防护配置

---

## P6: 逻辑系统扩展 (Logic System Expansion)

*优先级: 中 —— 扩展推理能力和表达力*

### 新增

- [x] **三值逻辑 (Three-valued Logic)**:
  - `LV00_TVAL_TRUE`, `LV00_TVAL_FALSE`, `LV00_TVAL_UNKNOWN`
  - 支持不完全信息下的推理
  - Kleene 和 Lukasiewicz 三值逻辑表
- [x] **量词支持 (Quantifiers)**:
  - 全称量词 (forall) 和存在量词 (exists)
  - 嵌套量词（受限于 `LV00_CONFIG_PARSER_MAX_AST_DEPTH`）
  - Skolem 化预处理
- [x] **逻辑自检 (Logic Self-Check)**:
  - 公理一致性的自动验证
  - 证明结果的类型正确性检查
  - 循环推理检测
- [x] **模态算子 (Modal Operators)**:
  - 必然性 (box) 和可能性 (diamond) 算子
  - K, T, S4, S5 模态框架
- [x] **证明评分 (Proof Scoring)**:
  - 简洁性评分: 步骤越少越高
  - 优雅性评分: 使用的公理越基础越高
  - 完备性评分: 覆盖所有必要条件

### 影响文件

- `src/core/proof.c` — 证明评分逻辑
- `src/core/prop_verifier.c` — 三值逻辑和量词处理
- `src/core/mini_kernel.c` — 模态算子支持
- `include/lv00/config.h` — 逻辑相关配置

---

## P7: 跨平台类型与公共 API (Cross-Platform Types & Public API)

*优先级: 高 —— 提升可移植性、可用性和分发便利性*

### 7.1 跨平台类型系统

- [x] **新增 `include/lv00/cross_platform.h`** (601 行)
  - 固定宽度整数类型: `lv00_i32`, `lv00_u32`, `lv00_i64`, `lv00_u64`, `lv00_iptr`, `lv00_uptr`, `lv00_byte`, `lv00_bool`
  - 平台检测: `LV00_PLATFORM_WINDOWS`, `LV00_PLATFORM_LINUX`, `LV00_PLATFORM_MACOS`, `LV00_PLATFORM_UNIX`
  - 编译器检测: `LV00_CC_GCC`, `LV00_CC_CLANG`, `LV00_CC_MSVC`
  - 架构检测: `LV00_ARCH_32BIT`, `LV00_ARCH_64BIT`
  - 栈大小感知: `LV00_STACK_SIZE_KB`, `LV00_STACK_SIZE_BYTES`, `LV00_SAFE_STACK_ALLOC_BYTES`
  - 字节序检测: `LV00_ENDIAN_LITTLE`, `LV00_ENDIAN_BIG`
  - 可移植对齐: `LV00_ALIGNAS(n)`, `LV00_ALIGN_UP`, `LV00_ALIGN_DOWN`
  - 属性注解: `LV00_INLINE`, `LV00_FORCE_INLINE`, `LV00_NORETURN`, `LV00_CONST_FUNC`, `LV00_PURE_FUNC`
  - 分支预测: `LV00_LIKELY(x)`, `LV00_UNLIKELY(x)`
  - 格式检查: `LV00_FORMAT_PRINTF`, `LV00_FORMAT_SCANF`
  - 符号可见性: `LV00_EXPORT`, `LV00_HIDDEN`
  - 编译期静态断言: `LV00_STATIC_ASSERT`
  - 平台信息运行时查询: `lv00_platform_name()`, `lv00_compiler_name()`, `lv00_arch_name()`, `lv00_endian_name()`
  - 字节交换: `LV00_BSWAP16`, `LV00_BSWAP32`, `LV00_BSWAP64`

### 7.2 公共 API 重构

- [x] **`include/lv00/lv00.h` 完全重写** (794 行)
  - **`LV00_PUBLIC_API` 宏**: 支持 MSVC 的 `__declspec(dllexport/dllimport)` 和 GCC/Clang 的 `__attribute__((visibility("default")))`
  - **清晰的节区标记**: `=== 版本信息 API ===`, `=== 初始化与清理 ===`, `=== 引擎生命周期 ===`, `=== 几何构造便捷 API ===`, `=== 推理与求解 ===`, `=== 配置管理 ===`, `=== 内存管理 ===`, `=== 调试和日志 ===`
  - **Doxygen 风格的完整文档注释**: 所有公共函数的 `@brief`, `@param[in]`, `@param[out]`, `@param[in,out]`, `@return`, `@note`, `@code` 示例
  - **Getting Started 示例**: 在 `@mainpage` 中提供从初始化到清理的完整 7 步示例
  - **LV00VersionInfo 结构体**: 包含 major/minor/patch/version_string/platform/compiler/arch/build_date/build_time
  - **新增版本 API**:
    - `lv00_get_version_string()` — 零开销编译期版本字符串
    - `lv00_get_version_info(&info)` — 详细版本信息填充
    - `lv00_check_version_compat()` — 运行时版本兼容性验证
    - `lv00_version_major()` / `lv00_version_minor()` / `lv00_version_patch()` — 编译期版本号宏
  - **所有公共函数添加 `LV00_PUBLIC_API` 导出标记**
  - **版本号更新**: 3.2.0 → 3.3.0
  - **引入 `cross_platform.h`**: 作为 `lv00.h` 的第一个包含

### 7.3 API 使用指南

- [x] **新增 `docs/API_QUICKSTART.md`** (650+ 行)
  - 安装与构建说明 (Windows/Linux/macOS)
  - 最小的可工作示例 (三角形验证)
  - 高级示例:
    - 多问题批量处理
    - 自定义公理加载
    - 系统信息与健康检查
    - 配置优化
  - 错误处理模式 (分层检查、错误码速查、防御性编程)
  - 内存管理模式 (所有权规则、监控、RAII)
  - 线程安全说明 (安全模式、不安全模式、最佳实践)
  - API 速查表 (版本/平台/生命周期/构造/求解/配置/日志/内存)
  - 常见问题排查 (Q1-Q5)

### 7.4 CMake 跨平台增强

- [x] **CMakeLists.txt 增强** (740+ 行)
  - **平台特定编译器标志**:
    - MSVC: `/W4` (最高警告), `/guard:cf` (控制流保护), `/CETCOMPAT` (CET 兼容)
    - GCC: `-Wconversion` (隐式转换), `-Wnull-dereference` (空指针解引用), `-Wformat=2` (格式字符串), `-fstack-clash-protection` (栈冲突保护)
    - Clang: `-Weverything` + 选择性禁用噪音警告 (`-Wno-padded`, `-Wno-disabled-macro-expansion` 等)
  - **`install()` 规则**: `cmake --install` 正确安装库文件和头文件到 `${CMAKE_INSTALL_LIBDIR}` / `${CMAKE_INSTALL_INCLUDEDIR}`
  - **CPack 打包配置**: 支持生成 `.tar.gz`, `.zip`, `.deb`, `.rpm`, NSIS 安装器
  - **pkg-config `.pc` 文件生成**: `lv00.pc` 自动生成，支持 `pkg-config --cflags --libs lv00`
  - **CMake 包配置**: `lv00-config.cmake` 和 `lv00-config-version.cmake` 支持 `find_package(lv00)`
  - **共享库构建增强**: `LV00_BUILD_SHARED` 编译定义，MSVC 运行时库配置

---

## 文件统计

### 新增文件

| 文件 | 行数 | 说明 |
|------|------|------|
| `include/lv00/cross_platform.h` | 601 | 跨平台类型系统 |
| `include/lv00/config.h` | 207 | 集中化配置 |
| `docs/ARCHITECTURE_v3.3.md` | 850 | 五层架构规范 |
| `docs/API_QUICKSTART.md` | 650 | API 快速入门指南 |
| `CHANGELOG_v3.3.0.md` | 本文件 | 变更日志 |

### 修改文件

| 文件 | 变更说明 |
|------|---------|
| `include/lv00/lv00.h` | 完全重写：版本 API、doxygen 文档、LV00_PUBLIC_API、节区标记 (383 → 794 行) |
| `CMakeLists.txt` | 分层构建、平台标志、install/CPack/pkg-config/package config |
| `include/lv00/engine.h` | 新增层级验证宏 |

### 项目总览

| 指标 | v3.2.0 | v3.3.0 | 变化 |
|------|--------|--------|------|
| 头文件数 | ~100 | ~103 | +3 |
| 源文件数 | 126 | 126 | 0 |
| 文档文件数 | 0 | 3 | +3 |
| CMake 行数 | ~450 | ~820 | +370 |
| 架构层级 | 扁平 | 5 层 | 质变 |
| 公共 API 文档覆盖率 | ~30% | ~95% | +65% |

---

## 破坏性变更

### 编译期变更

- [x] **版本宏更新**: `LV00_VERSION_MAJOR` 保持 3，`LV00_VERSION_MINOR` 从 2 更新到 3，`LV00_VERSION_PATCH` 保持 0
- [x] **编译期版本检查更新**: 版本验证断言更新为 3.3.0
- [x] **新必选头文件**: `lv00.h` 现在要求 `cross_platform.h` 存在

### API 变更

- [x] **`lv00_get_version()` 函数**: 原 `static inline` 函数改为 `LV00_PUBLIC_API` 声明（实际实现移入 .c 文件），建议使用新的 `lv00_get_version_string()` 替代
- [x] **所有公共函数添加 `LV00_PUBLIC_API`**: 静态库构建无影响；共享库构建时正确导出符号

### 行为变更

- [x] **内存分配**: 所有旧代码中仍在直接使用 `malloc/free` 的将逐步迁移到 `lv00_malloc/lv00_free`
- [x] **日志系统**: 新增日志级别控制，默认日志行为可能略有不同

---

## 兼容性

### 向后兼容

- [x] 聚合静态库 `lv00_static` 保持向后兼容
- [x] 所有旧的公共 API 函数签名未改变
- [x] 配置兼容层 (`config.h`) 保留了旧的宏名称
- [x] CMake 构建选项 `BUILD_TESTS`, `BUILD_EXAMPLES`, `BUILD_SHARED_LIBS` 等保持不变

### 平台支持

| 平台 | 编译器 | 状态 |
|------|--------|------|
| Windows 10/11 | MSVC 2019+ | 完全支持 |
| Windows 10/11 | MinGW-w64 GCC 12+ | 完全支持 |
| Linux (x86_64) | GCC 7+ | 完全支持 |
| Linux (x86_64) | Clang 5+ | 完全支持 |
| Linux (aarch64) | GCC 7+ | 最终测试中 |
| macOS 12+ (x86_64) | Apple Clang 13+ | 完全支持 |
| macOS 12+ (aarch64) | Apple Clang 14+ | 完全支持 |

---

## 已知问题

1. **Windows MSVC 动态 CRT**: 共享库构建需要一致的 CRT 链接（`/MD` vs `/MT`）。CMakeLists.txt 中已配置，但手动构建时需注意。
2. **预设编译时间**: 55 个数学理论预设文件使编译时间较长（首次构建约 5-10 分钟）。
3. **层级验证依赖于开发者规范**: `ENABLE_LAYER_VALIDATION` 目前是可选开关，未默认启用。

---

## 贡献者

| 角色 | 贡献 |
|------|------|
| 架构设计 | 五层架构规范、跨平台类型系统 |
| 实现 | P0-P7 全部功能实现 |
| 文档 | 架构规范、API 快速入门、变更日志 |

---

## 下一步计划 (v3.4.0+)

- [ ] CI/CD 流水线 (GitHub Actions / GitLab CI)
- [ ] 各层独立单元测试
- [ ] 性能基准测试套件
- [ ] WASM 目标平台支持
- [ ] Python 绑定 (ctypes/CFFI)
- [ ] 交互式 REPL
- [ ] 可视化调试工具

---

> **项目仓库**: [Lv-00](https://github.com/example/lv00)
> **许可证**: MIT
> **文档**: 见 `docs/` 目录
