cmake_minimum_required(VERSION 3.15)
project(lv VERSION 1.1.0 LANGUAGES C)  # 版本号与 lv.h 中 lv_VERSION_MAJOR/MINOR/PATCH 保持一致

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
# 禁用编译器扩展，确保严格 C11 标准合规
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Clang 在 Windows 下对注释中的 Unicode 字符会产生错误，
# 放宽限制以允许中文注释。
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unicode-homoglyph -Wno-unicode")
endif()

# ============================================================
# 构建选项（集中声明）
# ============================================================
option(BUILD_TESTS       "Build tests"                            ON)
option(BUILD_EXAMPLES    "Build examples"                         ON)
option(BUILD_SHARED_LIBS "Build shared library (DLL/SO)"         OFF)
option(BUILD_FUZZERS     "Build fuzzing targets (requires Clang)" OFF)
option(ENABLE_COVERAGE   "Enable code coverage (GCC/Clang)"       OFF)
option(ENABLE_SANITIZERS "Enable ASan + UBSan (GCC/Clang)"       OFF)
option(BUILD_WASM        "Build WebAssembly target (Emscripten)"  OFF)
option(WASM_ES6_MODULE   "Generate ES6 module for WASM"          ON)
option(WASM_ALLOW_MEMORY_GROWTH "Allow WASM memory growth"       ON)
# v1.1.0: 层级验证开关（启用后在编译时检查跨层依赖违规）
option(ENABLE_LAYER_VALIDATION "Enable compile-time layer boundary checks" OFF)

# ============================================================
# 编译器标志
# ============================================================

# --- 通用编译器警告选项（GCC / Clang）---
# -Wall      : 启用大部分常见警告
# -Wextra    : 启用额外有用的警告（如未使用参数等）
# -Wpedantic : 严格 ISO C 标准合规性检查，禁用编译器扩展语法
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
# Windows Interlocked API 在 GCC 下产生 incompatible-pointer-types 警告，不影响正确性
add_compile_options(-Wno-incompatible-pointer-types)
endif()

# --- MSVC 编译器选项 ---
# v1.1.0: 升级警告级别到 /W4（最高级别），确保代码质量
if(MSVC)
    # /W4          : 最高警告级别（启用所有信息性警告）
    # /guard:cf    : 控制流保护（Control Flow Guard），防止 ROP 攻击
    # /CETCOMPAT   : 标记二进制为 CET（Control-flow Enforcement Technology）兼容
    # /sdl         : 启用 SDL 检查（额外安全代码生成）
    add_compile_options(/W4 /guard:cf /CETCOMPAT /sdl)

    # 选择性禁用 MSVC 的噪音警告（若产生大量误报）
    # C4201: 无名 struct/union（C11 匿名结构体，合法使用）
    # C4710: 函数未内联（信息性，非错误）
    # C4820: 填充字节（信息性，非错误）
    add_compile_options(/wd4201 /wd4710)

    # 定义 _CRT_SECURE_NO_WARNINGS 以启用 strdup/strcat 等 POSIX 函数
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)

    # 抑制函数弃用告警（strdup 等 POSIX 函数在 MSVC 中标记为弃用）
    add_compile_options(/wd4996)

    # VS 2022+ 需要显式启用 C11 atomics
    if(CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL "19.30")
        add_compile_options(/experimental:c11atomics)
    endif()

    # 抑制 GMP 库内部的无符号负数告警（GMP 自身代码的 MSVC 兼容性问题）
    add_compile_options(/wd4146)
endif()

# --- GCC 专用编译器选项 ---
# v1.1.0: 添加额外的 GCC 警告和安全标志
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    # -Wconversion        : 隐式类型转换警告（防止精度损失/符号变化）
    # -Wnull-dereference  : 空指针解引用警告
    # -Wformat=2          : 增强格式字符串检查
    # -Wduplicated-branches : 重复分支检测
    # -Wduplicated-cond   : 重复条件检测
    # -Wlogical-op        : 可疑逻辑运算符检测
    # -Wjump-misses-init  : 跳转跨越变量初始化
    # -fstack-clash-protection : 栈冲突保护
    add_compile_options(
        -Wconversion
        -Wnull-dereference
        -Wformat=2
        -Wduplicated-branches
        -Wduplicated-cond
        -Wlogical-op
        -Wjump-misses-init
        -fstack-clash-protection
    )

    # 选择性禁用 GCC 的噪音警告
    # -Wno-sign-conversion   : 符号转换警告过多（如 int → size_t）
    # -Wno-unused-parameter  : 未使用参数在回调 API 中常见
    # -Wno-missing-field-initializers : C 语言中合法且常见
    # 以下为大型代码库中常见的非关键性警告，逐文件修复成本过高：
    # -Wno-unused-function / -Wno-unused-variable / -Wno-unused-but-set-variable:
    #   debug/helper 函数与变量解耦后的残留声明
    # -Wno-pedantic : 可变参数宏无额外参数（##__VA_ARGS__ 合法使用）
    # -Wno-jump-misses-init : goto 跨越变量初始化（在错误处理路径中可控）
    # -Wno-enum-compare / -Wno-switch : 跨枚举类型的兼容性比较
    # -Wno-discarded-qualifiers : const 限定符在 C 回调 API 中的传递
    # -Wno-format-truncation / -Wno-format-nonliteral : snprintf 缓冲区足够大
    add_compile_options(
        -Wno-sign-conversion
        -Wno-unused-parameter
        -Wno-missing-field-initializers
        -Wno-unused-function
        -Wno-unused-variable
        -Wno-unused-but-set-variable
        -Wno-pedantic
        -Wno-jump-misses-init
        -Wno-enum-compare
        -Wno-switch
        -Wno-discarded-qualifiers
        -Wno-format-truncation
         -Wno-format-nonliteral
         -Wno-conversion
         -Wno-sign-compare
         -Wno-duplicated-branches
         -Wno-float-conversion
         -Wno-format
         -Wno-pointer-to-int-cast
         -Wno-cast-function-type
         -Wno-unused-label
     )
endif()

# --- Clang 专用编译器选项 ---
# v1.1.0: 与 GCC 保持一致的警告级别，添加 Clang 特有的有用警告
if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    add_compile_options(
        # 与 GCC/通用选项保持一致的基础警告
        -Wall -Wextra -Wpedantic

        # Clang 特有实用警告
        -Wconversion              # 隐式类型转换警告
        -Wnull-dereference        # 空指针解引用警告
        -Wformat=2                # 增强格式字符串检查
        -Wswitch-enum             # switch 未覆盖所有枚举值时警告
        -Wunreachable-code        # 不可达代码检测
        -Wshadow                  # 变量遮蔽警告
        -Wnonportable-include-path # 不可移植的 include 路径
    )

    # 选择性禁用 Clang 的噪音警告
    add_compile_options(
        # 代码风格相关（不影响正确性）
        -Wno-padded                     # 结构体填充字节
        -Wno-unused-macros              # 头文件中定义的未使用宏

        # C 标准兼容性
        -Wno-reserved-id-macro          # 保留 ID 宏（在第三方头文件中常见）
        -Wno-reserved-identifier        # 保留标识符

        # 实用性警告
        -Wno-float-equal                # 浮点数相等比较（在数学代码中可能是故意的）
        -Wno-covered-switch-default     # 覆盖所有 case 的 switch 语句

        # 文档与声明
        -Wno-documentation-unknown-command # 未知 doxygen 命令
    )

    # 额外安全标志
    add_compile_options(-fstack-clash-protection)
endif()

if(ENABLE_COVERAGE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    # 追加 -O0 前，先移除已有的优化标志，避免冲突
    string(REPLACE "-O0" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REPLACE "-O1" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REPLACE "-O2" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REPLACE "-O3" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REPLACE "-Os" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REPLACE "-Ofast" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage -O0 -g")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
endif()

if(ENABLE_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address,undefined -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address,undefined")
endif()

# 警告：覆盖率插桩和消毒器同时启用时可能互相干扰
if(ENABLE_COVERAGE AND ENABLE_SANITIZERS)
    message(WARNING "--coverage 和 -fsanitize 同时启用：覆盖率插桩可能与 AddressSanitizer 冲突，"
                    "导致性能下降或误报。建议分开构建（coverage 构建和 sanitizer 构建各用一个构建目录）。")
endif()

# ============================================================
# WebAssembly (Emscripten) 支持
# ============================================================
if(BUILD_WASM)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
        message(FATAL_ERROR "BUILD_WASM 需要 Emscripten 工具链。"
            "请使用: emcmake cmake -DBUILD_WASM=ON ...")
    endif()
    
    message(STATUS "配置 WebAssembly 构建目标")
    
    # WASM 特定编译选项
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s WASM=1")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap']")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s EXPORTED_FUNCTIONS=['_malloc','_free']")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s MODULARIZE=1")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s EXPORT_NAME='lvModule'")
    
    # ES6 模块支持
    if(WASM_ES6_MODULE)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s EXPORT_ES6=1")
        message(STATUS "WASM: 生成 ES6 模块")
    endif()
    
    # 内存增长支持
    if(WASM_ALLOW_MEMORY_GROWTH)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s ALLOW_MEMORY_GROWTH=1")
        message(STATUS "WASM: 允许内存增长")
    endif()
    
    # 初始内存 32MB，最大 256MB
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s INITIAL_MEMORY=33554432")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s MAXIMUM_MEMORY=268435456")
    
    # 禁用某些在 WASM 下不支持的特性
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s DISABLE_EXCEPTION_CATCHING=1")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s NODEJS_CATCH_EXIT=0")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s NODEJS_CATCH_REJECTION=0")
    
    # 启用优化
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
    
    # 禁用测试（WASM 需要特殊测试框架）
    set(BUILD_test/c OFF)
    message(STATUS "WASM: 禁用常规测试（使用 web/gui 测试）")
    
    # WASM 不需要 GMP（使用 JavaScript 大整数或内置实现）
    set(GMP_INCLUDE_DIR "")
    set(GMP_LIBRARIES "")
    add_definitions(-Dlv_WASM_BUILD=1)
    add_definitions(-Dlv_NO_GMP=1)
endif()

# --- 安全相关编译选项（GCC / Clang）---
# -fstack-protector-strong : 启用栈溢出保护（对含有局部数组/取地址操作的函数生效）
# -D_FORTIFY_SOURCE=2      : 启用 glibc 缓冲区溢出检测（需配合 -O1 及以上优化）
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" AND NOT ENABLE_SANITIZERS)
    add_compile_options(-fstack-protector-strong)
    # 仅在非调试优化级别下启用 _FORTIFY_SOURCE，避免与 -O0 冲突
    if(NOT (CMAKE_BUILD_TYPE STREQUAL "Debug" OR ENABLE_COVERAGE))
        add_compile_options(-D_FORTIFY_SOURCE=2)
    endif()
endif()

# ============================================================
# GMP 库查找（大整数运算依赖）
# ============================================================
# 查找优先级：
#   1. -DGMP_ROOT=<path> 命令行/缓存变量（最高优先级）
#   2. GMP_ROOT 环境变量
#   3. 默认系统路径（Linux/macOS: /usr, /usr/local; Windows: MSYS2/MinGW 常见路径）
#
# Windows 策略：优先查找静态库 libgmp.a，避免运行时 DLL 依赖。
#   如果找不到静态库，回退到 find_library 自动搜索。
# Linux/macOS：直接使用 CMake 内置的 find_path/find_library。
# MSVC 注意：MSVC 不能直接使用 MinGW 的头文件，需要专门的 MSVC 兼容 GMP 构建
if(WIN32)
    if(MSVC)
        # MSVC 需要专门的 GMP 构建，尝试查找预编译的 MSVC 版本
        find_path(GMP_INCLUDE_DIR gmp.h
            HINTS ${GMP_ROOT}/include ENV GMP_ROOT/include
                  "C:/gmp/include"
                  "C:/vcpkg/installed/x64-windows/include"
        )
        find_library(GMP_LIBRARIES gmp
            HINTS ${GMP_ROOT}/lib ENV GMP_ROOT/lib
                  "C:/gmp/lib"
                  "C:/vcpkg/installed/x64-windows/lib"
        )
        # 如果找不到 GMP，提示用户安装
        if(NOT GMP_INCLUDE_DIR OR NOT GMP_LIBRARIES)
            message(FATAL_ERROR "MSVC: GMP library not found. Please install GMP for MSVC:\n"
                "  Option 1: Use vcpkg: vcpkg install gmp\n"
                "  Option 2: Download from https://github.com/BrianGladman/mpir\n"
                "  Option 3: Set GMP_ROOT to your GMP installation path\n"
                "Or use MinGW/Clang compiler instead.")
        endif()
    else()
        # MinGW/Clang 可以使用 MSYS2 的 GMP
        set(_GMP_FALLBACK_DIR "C:/msys64/mingw64")
        find_path(GMP_INCLUDE_DIR gmp.h
            HINTS ${GMP_ROOT}/include ENV GMP_ROOT/include
                  "${_GMP_FALLBACK_DIR}/include"
        )
        if(EXISTS "${GMP_ROOT}/lib/libgmp.a")
            set(GMP_LIBRARIES "${GMP_ROOT}/lib/libgmp.a")
        elseif(EXISTS "${_GMP_FALLBACK_DIR}/lib/libgmp.a")
            set(GMP_LIBRARIES "${_GMP_FALLBACK_DIR}/lib/libgmp.a")
        else()
            find_library(GMP_LIBRARIES gmp
                HINTS ${GMP_ROOT}/lib ENV GMP_ROOT/lib
                      "${_GMP_FALLBACK_DIR}/lib"
            )
        endif()
    endif()
else()
    find_path(GMP_INCLUDE_DIR gmp.h)
    find_library(GMP_LIBRARIES gmp)
endif()

if(NOT BUILD_WASM)
    if(NOT GMP_INCLUDE_DIR OR NOT GMP_LIBRARIES)
        message(FATAL_ERROR "GMP library not found. Set -DGMP_ROOT=<path> or install libgmp-dev.")
    endif()
else()
    message(STATUS "WASM: 跳过 GMP 库查找（使用内置大整数实现）")
endif()

# GMP_LIBRARIES 已包含完整路径，无需 link_directories

# ============================================================
# 源文件 —— 五层架构组织（v3.3）
# ============================================================
# 设计原则：
#   1. 每层编译为 OBJECT 库，允许内部的函数调用
#   2. 层间通过 target_link_libraries 控制依赖方向
#   3. Layer 5 -> Layer 4 -> Layer 3 -> Layer 2 <- Layer 1
#   4. lv_static 聚合所有 OBJECT 库，保持向后兼容
# 详见 doc/docs/ARCHITECTURE_v1.1.md
# ============================================================

# ── 共享头文件列表（所有层共用）──
set(lv_HEADERS
    core/include/lv/lv.h
    core/include/lv/cross_platform.h
    core/include/lv/error_codes.h
    core/include/lv/symbolic_coord.h
    core/include/lv/constraint_graph.h
    core/include/lv/normalization.h
    core/include/lv/graph_hash.h
    core/include/lv/solver.h
    core/include/lv/rewrite.h
    core/include/lv/unify.h
    # v3.6.0 新增：WFC 范式头文件
    core/include/lv/propagation.h
    core/include/lv/equiv_class.h
    core/include/lv/meta_proof.h
    core/include/lv/axiom_pkg.h
    core/include/lv/module.h
    core/include/lv/engine.h
    core/include/lv/debug.h
    core/include/lv/mpz_poly.h
    core/include/lv/func_block.h
    core/include/lv/type_system.h
    core/include/lv/proof.h
    core/include/lv/recursion.h
    core/include/lv/lv_utils.h
    core/include/lv/high_dim.h
    core/include/lv/interop.h
    core/include/lv/formula_parser.h
    core/include/lv/formula_renderer.h
    core/include/lv/formula_converter.h
    core/include/lv/prop_verifier.h
    core/include/lv/lexer_shared.h
    core/include/lv/stream.h
    core/include/lv/func_block_utils.h
    core/include/lv/stream_context_util.h
    core/include/lv/func_block_registry.h
    core/include/lv/func_block_preset_ops.h
    # v1.1.0 新增模块头文件（第一轮：核心计算模块）
    core/include/lv/groebner_engine.h
    core/include/lv/engine_scheduler.h
    core/include/lv/euclidean_geometry.h
    core/include/lv/geometry_compress.h
    core/include/lv/float_error.h
    core/include/lv/smt_backend.h
    core/include/lv/bdd_encoding.h
    core/include/lv/approx_counter.h
    # 位数熔断系统
    core/include/lv/bit_burning.h
    core/include/lv/command_log.h
    # v1.1.0 新增模块头文件（第二轮：计算后端与数值模块）
    core/include/lv/atp_backend.h
    core/include/lv/solver_core.h
    core/include/lv/sat_encoding.h
    core/include/lv/relation_model.h
    core/include/lv/numerical_backend.h
    core/include/lv/geom_evol.h
    core/include/lv/geo_event_detect.h
    core/include/lv/geo_visual.h
    core/include/lv/lv_numeric.h
    core/include/lv/probabilistic_constraint.h
    # v1.1.0 新增模块头文件（第三轮：几何构造与交互模块）
    core/include/lv/algebra_mode.h
    core/include/lv/dsl_compiler.h
    core/include/lv/interactive_geo.h
    # v1.1.0 新增模块头文件（第四轮：证明与规约模块）
    core/include/lv/mini_kernel.h
    core/include/lv/proof_widget.h
    core/include/lv/path_type.h
    core/include/lv/three_layer_arithmetic.h
    core/include/lv/geometric_primitives.h
    # λ-演算：λ-项与约束图编译
    core/include/lv/lambda_to_graph.h
    # λ-演算：λ-项类型检查
    core/include/lv/lambda_type_check.h
    # λ-演算：Church 编码公共 API
    core/include/lv/lambda_church.h
    # 模块化预设函数块系统
    core/include/lv/preset_blocks.h
    # v1.1.0 性能优化模块
    core/include/lv/thread_pool.h
    core/include/lv/simd_ops.h
    core/include/lv/benchmark.h
    # v1.1.0 新增核心模块（整改实施）
    core/include/lv/memory_pool.h
    core/include/lv/expr_canonical.h
    core/include/lv/geometry_transform.h
    core/include/lv/inequality_reasoning.h
    core/include/lv/runtime_monitor.h
    core/include/lv/axiom_rule_engine.h
    core/include/lv/proof_engine_enhanced.h
    core/include/lv/test_framework.h
    # Magic 模拟器模块
    core/include/lv/magic.h
    # 内部/实现头文件（用于 IDE 索引和 OBJECT 库关联）
    core/include/lv/lv_internal.h
    core/include/lv/config.h
    core/include/lv/context.h
    core/include/lv/geometry_types.h
    core/include/lv/node_deep_copy.h
    core/include/lv/parser_safety.h
    core/include/lv/func_block_internal.h
    core/include/lv/func_block_preset.h
    core/include/lv/expr_canon.h
    core/include/lv/modal_operators.h
    core/include/lv/circuit_breaker.h
    core/include/lv/axiom_grade.h
    core/include/lv/three_valued_logic.h
    core/include/lv/quantifier.h
    core/include/lv/rational.h
    core/include/lv/proof_trace.h
    core/include/lv/runtime_guard.h
    core/include/lv/exact_arithmetic.h
    core/include/lv/sha256.h
    # v3.7.0 新增头文件（原 Pending header additions）
    core/include/lv/adaptive_pruning.h
    core/include/lv/autodiff.h
    core/include/lv/cache_manager.h
    core/include/lv/conflict_detector.h
    core/include/lv/ga_codegen.h
    core/include/lv/ga_interface.h
    core/include/lv/ga_multivector.h
    core/include/lv/groebner_parallel.h
    core/include/lv/math_theory_guide_cn.h
    core/include/lv/meta_repr.h
    core/include/lv/nt_number_theory.h
    core/include/lv/nt_polynomial.h
    core/include/lv/ode_solver.h
    core/include/lv/plugin_system.h
    core/include/lv/proof_compiler.h
    core/include/lv/proof_contradiction.h
    # v3.0.0 UI-Kernel 通信协议（替代 view_projection）
    core/include/lv/lv_protocol.h
    # v3.0.0 API 版式规范 + 层间通信
    core/include/lv/lv_api_spec.h
    core/include/lv/lv_layer_msg.h
    # stubs→headers: 17 semi-finished headers
    core/include/lv/adaptive_threshold.h
    core/include/lv/gappa_dsl.h
    core/include/lv/gappa_propagate.h
    core/include/lv/geo_aabb_tree.h
    core/include/lv/geo_constraint_solver.h
    core/include/lv/geo_dynamic.h
    core/include/lv/geo_predicate.h
    core/include/lv/geometry_config.h
    core/include/lv/performance_profiler.h
    core/include/lv/proof_export_enhanced.h
    core/include/lv/proof_rule_engine.h
    core/include/lv/proof_session.h
    core/include/lv/proof_version.h
    core/include/lv/reasoning_cache.h
    core/include/lv/rewrite_strategy.h
    core/include/lv/smt_bitvector.h
    core/include/lv/sym_expr.h
    # v3.5.0 stub→real: 12 non-preset modules
    core/include/lv/ecosystem.h
    core/include/lv/fast_index.h
    core/include/lv/gc_language.h
    core/include/lv/geo_spec.h
    core/include/lv/logic_check.h
    core/include/lv/math_input.h
    core/include/lv/math_protocol.h
    core/include/lv/proof_priority.h
    core/include/lv/proof_score.h
    core/include/lv/sparse_linear_algebra.h
    core/include/lv/status_codes.h
    core/include/lv/tikz_export.h
    # v3.5.0 stub→real: 61 preset module headers
    core/include/lv/preset_advanced_geometry.h
    core/include/lv/preset_algebraic.h
    core/include/lv/preset_algebraic_geometry.h
    core/include/lv/preset_algebraic_topology.h
    core/include/lv/preset_algebraic_topology_adv.h
    core/include/lv/preset_analysis.h
    core/include/lv/preset_arithmetic_geometry.h
    core/include/lv/preset_basic_geometry.h
    core/include/lv/preset_basic_math.h
    core/include/lv/preset_calculus.h
    core/include/lv/preset_category_theory.h
    core/include/lv/preset_category_theory_adv.h
    core/include/lv/preset_coding_theory.h
    core/include/lv/preset_combinatorics.h
    core/include/lv/preset_common.h
    core/include/lv/preset_complex_analysis.h
    core/include/lv/preset_core.h
    core/include/lv/preset_difference_equations.h
    core/include/lv/preset_differential_equations.h
    core/include/lv/preset_differential_geometry.h
    core/include/lv/preset_differential_geometry_adv.h
    core/include/lv/preset_dynamical_systems.h
    core/include/lv/preset_field_theory.h
    core/include/lv/preset_functional_analysis.h
    core/include/lv/preset_functional_analysis_adv.h
    core/include/lv/preset_game_theory.h
    core/include/lv/preset_geometry_3d.h
    core/include/lv/preset_graph_theory.h
    core/include/lv/preset_group_theory.h
    core/include/lv/preset_homological_algebra.h
    core/include/lv/preset_information_theory.h
    core/include/lv/preset_integral_transforms.h
    core/include/lv/preset_lattice_theory.h
    core/include/lv/preset_lie_theory_advanced.h
    core/include/lv/preset_linear_algebra.h
    core/include/lv/preset_logic_advanced.h
    core/include/lv/preset_math_logic.h
    core/include/lv/preset_mathematical_logic.h
    core/include/lv/preset_mathematical_physics.h
    core/include/lv/preset_matrix.h
    core/include/lv/preset_measure_theory.h
    core/include/lv/preset_measurements.h
    core/include/lv/preset_number_theory.h
    core/include/lv/preset_numerical.h
    core/include/lv/preset_numerical_analysis.h
    core/include/lv/preset_optimization.h
    core/include/lv/preset_order_theory.h
    core/include/lv/preset_polygons.h
    core/include/lv/preset_polynomial.h
    core/include/lv/preset_probability.h
    core/include/lv/preset_probability_statistics.h
    core/include/lv/preset_register_macros.h
    core/include/lv/preset_representation_theory.h
    core/include/lv/preset_ring_theory.h
    core/include/lv/preset_set_theory.h
    core/include/lv/preset_special_functions.h
    core/include/lv/preset_statistics.h
    core/include/lv/preset_stochastic_processes.h
    core/include/lv/preset_topology.h
    core/include/lv/preset_transformations.h
    core/include/lv/preset_trigonometry.h
    # v1.1.0 新增：关键对计算引擎 + 类型等价探索器
    core/include/lv/critical_pair.h
    core/include/lv/type_equiv_explorer.h
    # v1.9.0 micro-bootstrap: 语义分析与加载器
    core/include/lv/lv_sema.h
    core/include/lv/lv_loader.h
)

# ── Layer 1: 输入解析层 (Parser) ──
# 职责：词法分析、公式解析、DSL 编译
# 依赖：Layer 2
set(lv_LAYER1_SOURCES
    core/src/layer1_parser/formula_parser.c
    core/src/layer1_parser/formula_converter.c
    core/src/layer1_parser/formula_renderer.c
    core/src/layer1_parser/lexer_shared.c
    core/src/layer1_parser/parser_safety.c
    core/src/layer1_parser/dsl_compiler.c
    core/src/layer1_parser/math_input.c
    # parser 子目录：公式 AST、DSL、LaTeX、Python 表达式
    core/src/layer1_parser/parser/formula_ast.c
    core/src/layer1_parser/parser/formula_dsl.c
    core/src/layer1_parser/parser/formula_latex.c
    core/src/layer1_parser/parser/formula_python.c
    # .lv 语言解析管线
    core/src/layer1_parser/lv_lexer.c
    core/src/layer1_parser/lv_ast.c
    core/src/layer1_parser/lv_parser.c
    # v1.9.0 micro-bootstrap: 语义分析与加载器
    core/src/layer1_parser/lv_sema.c
    core/src/layer1_parser/lv_loader.c
)

# ── Layer 2: 资源管理层 (Resource) ──
# 职责：内存分配、错误码、调试、深拷贝、工具函数
# 依赖：GMP，C 标准库（仅 Layer 2 能直接依赖 GMP 链接）
set(lv_LAYER2_SOURCES
    core/src/layer2_resource/lv_utils.c
    core/src/layer2_resource/lv_numeric.c
    core/src/layer2_resource/lv_config.c
    core/src/layer2_resource/lv.c
    core/src/layer2_resource/context.c
    core/src/layer2_resource/error_codes.c
    core/src/layer2_resource/debug.c
    core/src/layer2_resource/node_deep_copy.c
    # v1.1.0 新增：内存池、运行时监控、测试框架
    core/src/layer2_resource/memory_pool.c
    core/src/layer2_resource/runtime_monitor.c
    core/src/layer2_resource/test_framework.c
    # 补全遗漏文件：缓存管理、全局状态、几何配置、调试追踪、中文预设/理论/错误/结果消息
    core/src/layer2_resource/cache_manager.c
    core/src/layer2_resource/global_state.c
    core/src/layer2_resource/geometry_config.c
    core/src/layer2_resource/debug_trace.c
    core/src/layer2_resource/preset_helper_cn.c
    core/src/layer2_resource/math_theory_guide_cn.c
    core/src/layer2_resource/error_messages_cn.c
    core/src/layer2_resource/result_messages_cn.c
    # v3.5.0 stub→real: 8 new resource modules
    core/src/layer2_resource/fast_index.c
    core/src/layer2_resource/gc_language.c
    core/src/layer2_resource/geo_spec.c
    core/src/layer2_resource/math_input.c
    core/src/layer2_resource/math_protocol.c
    core/src/layer2_resource/proof_priority.c
    core/src/layer2_resource/proof_score.c
    core/src/layer2_resource/status_codes.c
    # v1.1.0 补全遗漏：运行时守卫、基准测试、性能分析、表示转换、自适应阈值、元表示
    core/src/layer2_resource/runtime_guard.c
    core/src/layer2_resource/benchmark.c
    core/src/layer2_resource/performance_profiler.c
    core/src/layer2_resource/representation_converter.c
    core/src/layer2_resource/adaptive_threshold.c
    core/src/layer2_resource/meta_repr.c
    # 补全遗漏文件：引导测试
    core/src/layer2_resource/bootstrap_test.c
    # SHA-256 共享实现（从 axiom_pkg.c 提取）
    core/src/layer2_resource/sha256.c
)

# ── Core: 核心便利层 (Core Convenience) ──
# 职责：便捷 API、快捷函数、对外包装
set(lv_CORE_SOURCES
    core/src/core/lv_convenience.c
    # 补全遗漏文件：原生实现
    core/src/lv_impl_native.c
    # 上层接口实现
    core/src/lv_impl_upper.c
)

# ── Layer 3: 几何拓扑层 (Geometry) ──
# 职责：约束图、符号坐标、几何原语、高维结构
# 依赖：Layer 2，GMP（通过 Layer 2 传递）
set(lv_LAYER3_SOURCES
    core/src/layer3_geometry/constraint_graph.c
    core/src/layer3_geometry/symbolic_coord.c
    core/src/layer3_geometry/euclidean_geometry.c
    core/src/layer3_geometry/interactive_geo.c
    core/src/layer3_geometry/high_dim.c
    core/src/layer3_geometry/geometry_compress.c
    core/src/layer3_geometry/geo_event_detect.c
    core/src/layer3_geometry/geo_spec.c
    core/src/layer3_geometry/geometry_csg.c
    core/src/layer3_geometry/geom_evol.c
    core/src/layer3_geometry/float_error.c
    core/src/layer3_geometry/mpz_poly_resultant.c
    # v1.1.0 新增：几何变换推理
    core/src/layer3_geometry/geometry_transform.c
    # v3.4.0 新增：几何代数模块（GATr + GAALOP + GeoLogic 落地）
    core/src/layer3_geometry/ga_multivector.c
    core/src/layer3_geometry/ga_interface.c
    core/src/layer3_geometry/ga_codegen.c
    core/src/layer3_geometry/geo_invariant_type.c
    # v3.4.0 新增：区间/数值验证模块（MPFI + Gappa + FPTaylor + Herbie + FLINT/Arb 落地）
    core/src/layer3_geometry/interval_arithmetic.c
    core/src/layer3_geometry/gappa_dsl.c
    core/src/layer3_geometry/gappa_propagate.c
    core/src/layer3_geometry/fptaylor_eval.c
    core/src/layer3_geometry/herbie_eval.c
    # v1.1.0 性能优化模块
    core/src/layer3_geometry/thread_pool.c
    # v1.1.0 新增：几何拓扑
    core/src/layer3_geometry/geo_topology.c
    # v3.6.0 新增：WFC 范式 - 等价类管理器（Layer 3 约束拓扑规约）
    core/src/layer3_geometry/equiv_class.c
    # v3.6.0 新增：WFC 范式 - 约束传播引擎（Layer 3 约束拓扑规约）
    core/src/layer3_geometry/propagation.c
    # 补全遗漏文件：代数数、半边网格、约束求解器、几何工具、动态几何、AABB树、几何谓词
    core/src/layer3_geometry/algebraic_number.c
    core/src/layer3_geometry/geo_halfedge_mesh.c
    core/src/layer3_geometry/geo_constraint_solver.c
    core/src/layer3_geometry/geo_utils.c
    core/src/layer3_geometry/geo_dynamic.c
    core/src/layer3_geometry/geo_aabb_tree.c
    core/src/layer3_geometry/geo_predicate.c
    # v1.1.0 补全遗漏：几何原语、参数曲线、路径类型
    core/src/layer3_geometry/geometric_primitives.c
    core/src/layer3_geometry/parametric_curves.c
    core/src/layer3_geometry/path_type.c
    # 约束图子模块：冲突检测、索引、内存、节点、序列化
    core/src/layer3_geometry/constraint_graph/graph_conflict.c
    core/src/layer3_geometry/constraint_graph/graph_index.c
    core/src/layer3_geometry/constraint_graph/graph_memory.c
    core/src/layer3_geometry/constraint_graph/graph_node.c
    core/src/layer3_geometry/constraint_graph/graph_serialize.c
    # 符号计算子模块：代数、二次、有理、坐标运算、超越函数
    core/src/layer3_geometry/symbolics/algebraic.c
    core/src/layer3_geometry/symbolics/quadratic.c
    core/src/layer3_geometry/symbolics/rational.c
    core/src/layer3_geometry/symbolics/symbolic_coord_ops.c
    core/src/layer3_geometry/symbolics/transcendental.c
    # 稀疏线性代数
    core/src/layer3_geometry/sparse_linear_algebra.c
)

# ── Layer 4: 公理推理层 (Reasoning) ──
# 职责：引擎、求解器、证明、重写、合一、规范化、类型系统、公理包
# 依赖：Layer 3，Layer 2
set(lv_LAYER4_SOURCES
    # 核心引擎与求解
    core/src/layer4_reasoning/engine/engine.c
    core/src/layer4_reasoning/solver.c
    core/src/layer4_reasoning/solver/solver_core.c
    # 证明系统
    core/src/layer4_reasoning/proof.c
    core/src/layer4_reasoning/proof_system/proof_optimize.c
    core/src/layer4_reasoning/proof_system/proof_multi_strategy.c
    core/src/layer4_reasoning/proof_trace.c
    core/src/layer4_reasoning/proof/proof_tree.c
    core/src/layer4_reasoning/proof_system/proof_priority.c
    core/src/layer4_reasoning/proof_system/logic_check.c
    core/src/layer4_reasoning/circuit_breaker.c
    # 三值逻辑、模态算子、量词系统
    core/src/layer4_reasoning/type_logic/three_valued_logic.c
    core/src/layer4_reasoning/type_logic/modal_operators.c
    core/src/layer4_reasoning/type_logic/quantifier.c
    # 增强证明引擎
    core/src/layer4_reasoning/engine/proof_engine_enhanced.c
    # 重写与合一
    core/src/layer4_reasoning/rewrite.c
    core/src/layer4_reasoning/unify/unify.c
    core/src/layer4_reasoning/unify/normalization.c
    core/src/layer4_reasoning/unify/graph_hash.c
    core/src/layer4_reasoning/model/recursion.c
    # 类型与属性
    core/src/layer4_reasoning/type_logic/type_system.c
    core/src/layer4_reasoning/type_logic/type_equiv_explorer.c
    core/src/layer4_reasoning/proof_system/prop_verifier.c
    # 后端求解引擎
    core/src/layer4_reasoning/backends/groebner_engine.c
    core/src/layer4_reasoning/backends/smt_backend_impl.c
    core/src/layer4_reasoning/backends/atp_backend.c
    core/src/layer4_reasoning/backends/sat_encoding.c
    core/src/layer4_reasoning/backends/bdd_encoding.c
    core/src/layer4_reasoning/backends/probabilistic_constraint.c
    core/src/layer4_reasoning/expr/rational.c
    core/src/layer4_reasoning/expr/expr_canon.c
    core/src/layer4_reasoning/exact_arithmetic.c
    # v1.1.0 新增：表达式规范化、不等式推理
    core/src/layer4_reasoning/expr/expr_canonical.c
    # 模型与模式
    core/src/layer4_reasoning/model/relation_model.c
    core/src/layer4_reasoning/model/algebra_mode.c
    core/src/layer4_reasoning/numeric/numerical_backend.c
    # 内核与协议
    core/src/layer4_reasoning/module.c
    core/src/layer4_reasoning/kernel/mini_kernel.c
    core/src/layer4_reasoning/kernel/gc_language.c
    core/src/layer4_reasoning/kernel/ecosystem.c
    core/src/layer4_reasoning/kernel/math_protocol.c
    core/src/layer4_reasoning/stream/stream.c
    core/src/layer4_reasoning/stream/stream_context_util.c
    # SMT/SAT 后端（从 Layer 3 迁移）
    core/src/layer4_reasoning/backends/smt_theory_combiner.c
    core/src/layer4_reasoning/backends/smt_bitvector.c
    core/src/layer4_reasoning/backends/smt_trigger_engine.c
    # 证明策略（从 Layer 3 迁移）
    core/src/layer4_reasoning/proof_system/proof_rule_engine.c
    core/src/layer4_reasoning/proof_system/proof_session.c
    # 数论/代数（从 Layer 3 迁移）
    core/src/layer4_reasoning/numeric/nt_number_theory.c
    core/src/layer4_reasoning/numeric/nt_polynomial.c
    core/src/layer4_reasoning/expr/sym_expr.c
    # 重写引擎（从 Layer 3 迁移）
    core/src/layer4_reasoning/unify/rewrite_strategy.c
    # 版本控制（从 Layer 3 迁移）
    core/src/layer4_reasoning/proof_system/proof_version.c
    # 自动微分（从 Layer 3 迁移）
    core/src/layer4_reasoning/numeric/autodiff.c
    # ODE 求解器（从 Layer 3 迁移）
    core/src/layer4_reasoning/numeric/ode_solver.c
    # 位数熔断系统
    core/src/layer4_reasoning/numeric/bit_burning.c
    # v1.1.0 新增：不等式推理系统（桩实现）
    core/src/layer4_reasoning/type_logic/inequality_reasoning.c
    # v4.0.0 新增：自适应剪枝策略（动态阈值 + 启发式评分）
    core/src/layer4_reasoning/engine/adaptive_pruning.c
    # v5.0.0 新增：并行 Groebner 引擎（并发优化）
    core/src/layer4_reasoning/backends/groebner_parallel.c
    # 公理包
    core/src/layer4_reasoning/axiom/axiom_pkg.c
    core/src/layer4_reasoning/axiom/axiom_grade.c
    core/src/layer4_reasoning/axiom/axiom_template_test.c
    # func_block 函数块系统
    core/src/layer4_reasoning/func_block/func_block.c
    core/src/layer4_reasoning/func_block/func_block_compose.c
    core/src/layer4_reasoning/func_block/func_block_determinism.c
    core/src/layer4_reasoning/func_block/func_block_instantiate.c
    core/src/layer4_reasoning/func_block/func_block_preset.c
    core/src/layer4_reasoning/func_block/func_block_preset_ops.c
    core/src/layer4_reasoning/func_block/func_block_registry.c
    core/src/layer4_reasoning/func_block/func_block_selector.c
    core/src/layer4_reasoning/func_block/func_block_serialize.c
    core/src/layer4_reasoning/func_block/func_block_utils.c
    core/src/layer4_reasoning/func_block/preset_basic_math.c
    core/src/layer4_reasoning/func_block/preset_blocks.c
    core/src/layer4_reasoning/func_block/preset_calculus.c
    core/src/layer4_reasoning/func_block/preset_common.c
    core/src/layer4_reasoning/func_block/preset_manager.c
    # 数学理论预设 (55 个文件)
    core/src/layer4_reasoning/preset/preset_advanced_geometry.c
    core/src/layer4_reasoning/preset/preset_algebraic.c
    core/src/layer4_reasoning/preset/preset_algebraic_geometry.c
    core/src/layer4_reasoning/preset/preset_algebraic_topology.c
    core/src/layer4_reasoning/preset/preset_algebraic_topology_adv.c
    core/src/layer4_reasoning/preset/preset_analysis.c
    core/src/layer4_reasoning/preset/preset_arithmetic_geometry.c
    core/src/layer4_reasoning/preset/preset_basic_geometry.c
    core/src/layer4_reasoning/preset/preset_category_theory.c
    core/src/layer4_reasoning/preset/preset_category_theory_adv.c
    core/src/layer4_reasoning/preset/preset_coding_theory.c
    core/src/layer4_reasoning/preset/preset_combinatorics.c
    core/src/layer4_reasoning/preset/preset_complex_analysis.c
    core/src/layer4_reasoning/preset/preset_difference_equations.c
    core/src/layer4_reasoning/preset/preset_differential_equations.c
    core/src/layer4_reasoning/preset/preset_differential_geometry.c
    core/src/layer4_reasoning/preset/preset_differential_geometry_adv.c
    core/src/layer4_reasoning/preset/preset_dynamical_systems.c
    core/src/layer4_reasoning/preset/preset_field_theory.c
    core/src/layer4_reasoning/preset/preset_functional_analysis.c
    core/src/layer4_reasoning/preset/preset_functional_analysis_adv.c
    core/src/layer4_reasoning/preset/preset_game_theory.c
    core/src/layer4_reasoning/preset/preset_geometry_3d.c
    core/src/layer4_reasoning/preset/preset_graph_theory.c
    core/src/layer4_reasoning/preset/preset_group_theory.c
    core/src/layer4_reasoning/preset/preset_homological_algebra.c
    core/src/layer4_reasoning/preset/preset_information_theory.c
    core/src/layer4_reasoning/preset/preset_integral_transforms.c
    core/src/layer4_reasoning/preset/preset_lattice_theory.c
    core/src/layer4_reasoning/preset/preset_lie_theory_advanced.c
    core/src/layer4_reasoning/preset/preset_linear_algebra.c
    core/src/layer4_reasoning/preset/preset_logic_advanced.c
    core/src/layer4_reasoning/preset/preset_math_logic.c
    core/src/layer4_reasoning/preset/preset_mathematical_logic.c
    core/src/layer4_reasoning/preset/preset_mathematical_physics.c
    core/src/layer4_reasoning/preset/preset_matrix.c
    core/src/layer4_reasoning/preset/preset_measure_theory.c
    core/src/layer4_reasoning/preset/preset_measurements.c
    core/src/layer4_reasoning/preset/preset_number_theory.c
    core/src/layer4_reasoning/preset/preset_numerical.c
    core/src/layer4_reasoning/preset/preset_numerical_analysis.c
    core/src/layer4_reasoning/preset/preset_optimization.c
    core/src/layer4_reasoning/preset/preset_order_theory.c
    core/src/layer4_reasoning/preset/preset_polygons.c
    core/src/layer4_reasoning/preset/preset_polynomial.c
    core/src/layer4_reasoning/preset/preset_probability.c
    core/src/layer4_reasoning/preset/preset_probability_statistics.c
    core/src/layer4_reasoning/preset/preset_representation_theory.c
    core/src/layer4_reasoning/preset/preset_ring_theory.c
    core/src/layer4_reasoning/preset/preset_set_theory.c
    core/src/layer4_reasoning/preset/preset_special_functions.c
    core/src/layer4_reasoning/preset/preset_statistics.c
    core/src/layer4_reasoning/preset/preset_stochastic_processes.c
    core/src/layer4_reasoning/preset/preset_topology.c
    core/src/layer4_reasoning/preset/preset_transformations.c
    core/src/layer4_reasoning/preset/preset_trigonometry.c
    # v3.6.0 新增：WFC 范式 - 剪枝合法性元证明（Layer 4 推理层）
    core/src/layer4_reasoning/meta_proof.c
    # 补全遗漏文件：冲突检测、矛盾证明、抽象代数预设
    core/src/layer4_reasoning/engine/conflict_detector.c
    core/src/layer4_reasoning/proof_contradiction.c
    core/src/layer4_reasoning/func_block/preset_abstract_algebra.c
    # v1.1.0 补全遗漏：公理规则引擎、推理缓存、SIMD 运算
    core/src/layer4_reasoning/axiom_rule_engine.c
    # reasoning_cache is in model/reasoning_cache.c (below)
    # simd_ops.c 在 unify/simd_ops.c（已统一）
    # core/src/layer4_reasoning/simd_ops.c（已删除，使用 unify/ 版本）
    # 求解器子模块：坐标提取、方程组、线性、快照、符号
    core/src/layer4_reasoning/solver/solver_coord_extract.c
    core/src/layer4_reasoning/solver/solver_conflict.c
    core/src/layer4_reasoning/solver/solver_dirty_set.c
    core/src/layer4_reasoning/solver/solver_eq_system.c
    core/src/layer4_reasoning/solver/solver_feedback.c
    core/src/layer4_reasoning/solver/solver_incremental.c
    core/src/layer4_reasoning/solver/solver_linear.c
    core/src/layer4_reasoning/solver/solver_order.c
    core/src/layer4_reasoning/solver/solver_result.c
    core/src/layer4_reasoning/solver/solver_snapshot.c
    core/src/layer4_reasoning/solver/solver_stats.c
    core/src/layer4_reasoning/solver/solver_symbolic.c
    # 求解器扩展（新提取模块）
    core/src/layer4_reasoning/solver/solver_eliminate.c
    core/src/layer4_reasoning/solver/solver_engine.c
    core/src/layer4_reasoning/solver/solver_equation_extract.c
    core/src/layer4_reasoning/solver/solver_geom_templates.c
    core/src/layer4_reasoning/solver/solver_groebner.c
    core/src/layer4_reasoning/solver/solver_multibranch.c
    # 求解器扩展：脏集合、快照
    # solver_dirty_set.c 和 solver_snapshot.c 已移至 solver/ 子目录
    # 旧路径 solver_ext/ 已清理
    # 证明子模块：依赖、导航、命题、版本、信任颜色
    core/src/layer4_reasoning/proof/proof_dependency.c
    core/src/layer4_reasoning/proof/proof_navigator.c
    core/src/layer4_reasoning/proof/proof_proposition.c
    core/src/layer4_reasoning/proof/proof_version.c
    core/src/layer4_reasoning/proof/trust_color.c
    core/src/layer4_reasoning/proof/meta_verify.c
    # 证明系统子模块：矛盾、追踪
    core/src/layer4_reasoning/proof_system/proof_contradiction.c
    core/src/layer4_reasoning/proof_system/proof_trace.c
    # 重写子模块：应用、匹配、VF2、WL
    core/src/layer4_reasoning/rewrite/rewrite_apply.c
    core/src/layer4_reasoning/rewrite/rewrite_match.c
    core/src/layer4_reasoning/rewrite/rewrite_vf2.c
    core/src/layer4_reasoning/rewrite/rewrite_wl.c
    core/src/layer4_reasoning/rewrite/critical_pair.c
    core/src/layer4_reasoning/rewrite/beta_reduce.c
    core/src/layer4_reasoning/rewrite/rewrite_strategy_impl.c
    # λ-项：数据类型与编译
    core/src/layer4_reasoning/lambda/lambda_term.c
    core/src/layer4_reasoning/lambda/lambda_to_graph.c
    core/src/layer4_reasoning/lambda/lambda_type_check.c
    core/src/layer4_reasoning/lambda/lambda_church.c
    # 引擎调度
    core/src/layer4_reasoning/engine/engine_scheduler.c
    # 精确算术（定义在 layer4_reasoning/exact_arithmetic.c）
    # core/src/layer4_reasoning/expr/exact_arithmetic.c (已删除，与根目录版本重复)
    # 近似计数器
    core/src/layer4_reasoning/backends/approx_counter.c
    # 命令日志系统
    core/src/layer4_reasoning/command_log.c
    # 内核子模块：熔断器、元证明
    core/src/layer4_reasoning/kernel/circuit_breaker.c
    core/src/layer4_reasoning/kernel/meta_proof.c
    # 推理缓存
    core/src/layer4_reasoning/model/reasoning_cache.c
    # 模块子模块：增量、LVZ、序列化
    core/src/layer4_reasoning/module/module_delta.c
    core/src/layer4_reasoning/module/module_lvz.c
    core/src/layer4_reasoning/module/module_serialize.c
    # 多项式
    core/src/layer4_reasoning/numeric/mv_polynomial.c
    # 统一子模块：SIMD 运算
    core/src/layer4_reasoning/unify/simd_ops.c
)

# ── Layer 5: 结果输出层 (Output) ──
# 职责：流式输出、TikZ 导出、证明可视化、模块、互操作
# 依赖：Layer 4，Layer 3，Layer 2
set(lv_LAYER5_SOURCES
    core/src/layer5_output/proof_widget.c
    core/src/layer5_output/proof_export_enhanced.c
    core/src/layer5_output/interop/interop.c
    # Magic 模拟器模块
    core/src/layer5_output/magic/magic.c
    # 几何可视化（完整版）
    core/src/layer5_output/geo_visual_complete.c
    # UI-Kernel 通信协议
    core/src/layer5_output/lv_protocol.c
    # 层间通信消息
    core/src/layer5_output/lv_layer_msg.c
    # v1.1.0 补全遗漏：插件系统、证明编译器
    core/src/layer5_output/plugin_system.c
    core/src/layer5_output/proof_compiler.c
    core/src/layer5_output/tikz_export.c
    # 互操作子模块：命令、导出、导入、服务器、定理
    core/src/layer5_output/interop/interop_command.c
    core/src/layer5_output/interop/interop_export.c
    core/src/layer5_output/interop/interop_import.c
    core/src/layer5_output/interop/interop_server.c
    core/src/layer5_output/interop/interop_theorem.c
)

# ============================================================
# 库目标 —— 五层 OBJECT 库 + 聚合静态库
# ============================================================

# ── 辅助宏：为每个层设置公共编译属性 ──
macro(lv_setup_layer target layer_id)
    target_include_directories(${target} PUBLIC
        ${GMP_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/core/include
        ${CMAKE_SOURCE_DIR}/core/include/lv
        ${CMAKE_SOURCE_DIR}/core/src
        ${CMAKE_SOURCE_DIR}/core/src/preset
    )
    # 层级标识（用于编译时诊断和层级验证）
    target_compile_definitions(${target} PRIVATE lv_CURRENT_LAYER=${layer_id})
    if(ENABLE_LAYER_VALIDATION)
        target_compile_definitions(${target} PRIVATE lv_ENABLE_LAYER_VALIDATION)
    endif()
endmacro()

# Layer 2: 资源管理层（基础层，不依赖任何上层）
add_library(lv_layer2_resource OBJECT ${lv_LAYER2_SOURCES} ${lv_HEADERS})
lv_setup_layer(lv_layer2_resource 2)
target_link_libraries(lv_layer2_resource ${GMP_LIBRARIES})

# Core: 核心便利层（基础层，不依赖任何上层）
add_library(lv_core OBJECT ${lv_CORE_SOURCES} ${lv_HEADERS})
lv_setup_layer(lv_core 0)

# GCC/Clang 优化标志
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lv_core PRIVATE
        -O2
        -fstrict-aliasing
        -fno-omit-frame-pointer
    )
endif()

# Layer 1: 输入解析层（依赖 Layer 2）
add_library(lv_layer1_parser OBJECT ${lv_LAYER1_SOURCES} ${lv_HEADERS})
lv_setup_layer(lv_layer1_parser 1)
target_link_libraries(lv_layer1_parser lv_layer2_resource)

# Layer 3: 几何拓扑层（依赖 Layer 2）
add_library(lv_layer3_geometry OBJECT ${lv_LAYER3_SOURCES} ${lv_HEADERS})
lv_setup_layer(lv_layer3_geometry 3)
target_link_libraries(lv_layer3_geometry lv_layer2_resource)
# v1.1.0: 线程库链接（用于 thread_pool.c）
find_package(Threads REQUIRED)
target_link_libraries(lv_layer3_geometry Threads::Threads)

# Layer 4: 公理推理层（依赖 Layer 3，Layer 2）
add_library(lv_layer4_reasoning OBJECT ${lv_LAYER4_SOURCES} ${lv_HEADERS})
lv_setup_layer(lv_layer4_reasoning 4)
target_link_libraries(lv_layer4_reasoning lv_layer3_geometry lv_layer2_resource)

# Layer 5: 结果输出层（依赖 Layer 4，Layer 3，Layer 2）
add_library(lv_layer5_output OBJECT ${lv_LAYER5_SOURCES} ${lv_HEADERS})
lv_setup_layer(lv_layer5_output 5)
target_link_libraries(lv_layer5_output lv_layer4_reasoning lv_layer3_geometry lv_layer2_resource)
# v3.5.0: 互操作层需要 ws2_32（Windows socket）
if(WIN32)
    target_link_libraries(lv_layer5_output ws2_32)
endif()

# ── Layer 6: 图形化编程层 (Visual Programming Layer) ──
# 设计文档: doc/docs/35_layer6_visual_programming.md
# 四大子系统: 可视化编辑器、函数块运行时扩展、类型系统扩展、表示转换层

set(lv_LAYER6_SOURCES
    # 核心视图
    core/src/layer6_visual/visual_editor.c
    core/src/layer6_visual/geometry_canvas.c
    core/src/layer6_visual/node_graph.c
    core/src/layer6_visual/block_canvas.c
    core/src/layer6_visual/text_code.c
    core/src/layer6_visual/view_synchronizer.c
    # 控制流块
    core/src/layer6_visual/control_flow/if_block.c
    core/src/layer6_visual/control_flow/while_block.c
    core/src/layer6_visual/control_flow/match_block.c
    # IO 交互块
    core/src/layer6_visual/io/file_block.c
    core/src/layer6_visual/io/network_block.c
    core/src/layer6_visual/io/ui_block.c
    # 数据结构块
    core/src/layer6_visual/data/list_block.c
    core/src/layer6_visual/data/map_block.c
    core/src/layer6_visual/data/record_block.c
    # 运行时
    core/src/layer6_visual/runtime/block_scheduler.c
    core/src/layer6_visual/runtime/incremental_exec.c
    core/src/layer6_visual/runtime/effect_tracker.c
    # 类型扩展
    core/src/layer6_visual/types/extended_types.c
    core/src/layer6_visual/types/effect_types.c
    core/src/layer6_visual/types/type_inference.c
    # 表示转换
    core/src/layer6_visual/converter/block_to_node.c
    core/src/layer6_visual/converter/block_to_text.c
    core/src/layer6_visual/converter/block_to_geometry.c
    core/src/layer6_visual/converter/sync_protocol.c
)

set(lv_LAYER6_HEADERS
    core/include/lv/visual_editor.h
    core/include/lv/control_flow_blocks.h
    core/include/lv/io_blocks.h
    core/include/lv/data_structure_blocks.h
    core/include/lv/extended_types.h
    core/include/lv/effect_system.h
    core/include/lv/block_scheduler.h
    core/include/lv/representation_converter.h
)

add_library(lv_layer6_visual OBJECT ${lv_LAYER6_SOURCES} ${lv_LAYER6_HEADERS})
lv_setup_layer(lv_layer6_visual 6)
target_link_libraries(lv_layer6_visual lv_layer5_output lv_layer4_reasoning lv_layer3_geometry lv_layer2_resource)

# ── Layer 7: 编排调度层 (Orchestration Layer) ──
set(lv_LAYER7_SOURCES
    core/src/layer7_orchestration/orchestrator.c
)
set(lv_LAYER7_HEADERS
    core/include/lv/orchestrator.h
)
add_library(lv_layer7_orchestration OBJECT ${lv_LAYER7_SOURCES} ${lv_LAYER7_HEADERS})
lv_setup_layer(lv_layer7_orchestration 7)
target_link_libraries(lv_layer7_orchestration lv_layer6_visual lv_layer5_output lv_layer4_reasoning lv_layer3_geometry lv_layer2_resource)

# ── Layer 8: 元验证层 (Meta-Verification Layer) ──
set(lv_LAYER8_SOURCES
    core/src/layer8_meta_verify/meta_verify.c
)
set(lv_LAYER8_HEADERS
    core/include/lv/meta_verify.h
)
add_library(lv_layer8_meta_verify OBJECT ${lv_LAYER8_SOURCES} ${lv_LAYER8_HEADERS})
lv_setup_layer(lv_layer8_meta_verify 8)
target_link_libraries(lv_layer8_meta_verify lv_layer7_orchestration)

# ── Layer 9: 应用入口层 (Application Layer) ──
set(lv_LAYER9_SOURCES
    core/src/layer9_application/application.c
)
set(lv_LAYER9_HEADERS
    core/include/lv/application.h
)
add_library(lv_layer9_application OBJECT ${lv_LAYER9_SOURCES} ${lv_LAYER9_HEADERS})
lv_setup_layer(lv_layer9_application 9)
target_link_libraries(lv_layer9_application lv_layer8_meta_verify)

# ── Layer 10: 外部集成层 (Interop Layer) ──
set(lv_LAYER10_SOURCES
    core/src/layer10_interop/lean4_bridge.c
    core/src/layer10_interop/coq_bridge.c
    core/src/layer10_interop/opml_codec.c
)
set(lv_LAYER10_HEADERS
    core/include/lv/interop.h
)
add_library(lv_layer10_interop OBJECT ${lv_LAYER10_SOURCES} ${lv_LAYER10_HEADERS})
lv_setup_layer(lv_layer10_interop 10)
target_link_libraries(lv_layer10_interop lv_layer9_application)

# ── 聚合静态库（向后兼容 lv_static）──
# 将所有层的 OBJECT 文件聚合到一个静态库中
add_library(lv_static STATIC
    $<TARGET_OBJECTS:lv_core>
    $<TARGET_OBJECTS:lv_layer1_parser>
    $<TARGET_OBJECTS:lv_layer2_resource>
    $<TARGET_OBJECTS:lv_layer3_geometry>
    $<TARGET_OBJECTS:lv_layer4_reasoning>
    $<TARGET_OBJECTS:lv_layer5_output>
    $<TARGET_OBJECTS:lv_layer6_visual>
    $<TARGET_OBJECTS:lv_layer7_orchestration>
    $<TARGET_OBJECTS:lv_layer8_meta_verify>
    $<TARGET_OBJECTS:lv_layer9_application>
    $<TARGET_OBJECTS:lv_layer10_interop>
)
target_include_directories(lv_static PUBLIC
    ${GMP_INCLUDE_DIR}
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/core/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/core/include/lv>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(lv_static ${GMP_LIBRARIES} Threads::Threads)
set_target_properties(lv_static PROPERTIES OUTPUT_NAME lv)

# preset_differential_geometry, preset_functional_analysis 已修复，预设计数已同步

if(BUILD_SHARED_LIBS)
    add_library(lv_shared SHARED
        $<TARGET_OBJECTS:lv_core>
        $<TARGET_OBJECTS:lv_layer1_parser>
        $<TARGET_OBJECTS:lv_layer2_resource>
        $<TARGET_OBJECTS:lv_layer3_geometry>
        $<TARGET_OBJECTS:lv_layer4_reasoning>
        $<TARGET_OBJECTS:lv_layer5_output>
        $<TARGET_OBJECTS:lv_layer6_visual>
        $<TARGET_OBJECTS:lv_layer7_orchestration>
        $<TARGET_OBJECTS:lv_layer8_meta_verify>
        $<TARGET_OBJECTS:lv_layer9_application>
        $<TARGET_OBJECTS:lv_layer10_interop>
    )
    target_include_directories(lv_shared PUBLIC
        ${GMP_INCLUDE_DIR}
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/core/include>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/core/include/lv>
        $<INSTALL_INTERFACE:include>
    )
    # [P0 修复] 共享库也需要链接 Threads::Threads（与静态库保持一致）
    target_link_libraries(lv_shared ${GMP_LIBRARIES} Threads::Threads)
    if(WIN32)
        target_link_libraries(lv_shared ws2_32)
    endif()
    # v1.1.0: 定义 lv_BUILD_SHARED 以启用 lv.h 中的 dllexport/visibility
    target_compile_definitions(lv_shared PRIVATE lv_BUILD_SHARED)

    set_target_properties(lv_shared PROPERTIES
        OUTPUT_NAME lv
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
    if(WIN32)
        set_target_properties(lv_shared PROPERTIES
            WINDOWS_EXPORT_ALL_SYMBOLS ON
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
            ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        )
        # MSVC 运行时库处理：共享库应使用动态 CRT（/MD 或 /MDd）
        # 避免静态 CRT（/MT, /MTd）导致的跨模块内存管理问题
        if(MSVC)
            set_target_properties(lv_shared PROPERTIES
                MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
            )
        endif()
    endif()
endif()

add_library(lv ALIAS lv_static)

# ============================================================
# 辅助函数：添加测试可执行文件
# ============================================================
function(add_lv_test name source)
    add_executable(${name} ${source})
    target_include_directories(${name} PRIVATE
        ${CMAKE_SOURCE_DIR}/test/c
    )
    target_link_libraries(${name} lv ${GMP_LIBRARIES})
    if(WIN32)
        target_link_libraries(${name} ws2_32)
    endif()
endfunction()

function(add_lv_test_and_register test_name source ctest_name)
    add_lv_test(${test_name} ${source})
    add_test(NAME ${ctest_name} COMMAND ${test_name}
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endfunction()

# ============================================================
# 测试
# ============================================================
if(BUILD_TESTS)
    enable_testing()

    # 正式模块测试（注册到 CTest）
    add_lv_test_and_register(test_basic
        test/c/test_basic.c basic_test)
    add_lv_test_and_register(test_edge_cases
        test/c/test_edge_cases.c edge_cases_test)
    add_lv_test_and_register(test_complex_graph
        test/c/test_complex_graph.c complex_graph_test)
    add_lv_test_and_register(test_normalization
        test/c/test_normalization.c normalization_test)
    add_lv_test_and_register(test_constraint_graph_ops
        test/c/test_constraint_graph_ops.c constraint_graph_ops_test)
    # 图哈希模块测试
    add_lv_test_and_register(test_graph_hash
        test/c/test_graph_hash.c graph_hash_test)
    add_lv_test_and_register(test_constraint_compatibility
        test/c/test_constraint_compatibility.c constraint_compatibility_test)
    add_lv_test_and_register(test_unify
        test/c/test_unify.c unify_test)
    add_lv_test_and_register(test_func_block
        test/c/test_func_block.c func_block_test)
    add_lv_test_and_register(test_solver
        test/c/test_solver.c solver_test)
    add_lv_test_and_register(test_solver_enhanced
        test/c/test_solver_enhanced.c solver_enhanced_test)
    add_lv_test_and_register(test_groebner_basis
        test/c/test_groebner_basis.c groebner_basis_test)
    add_lv_test_and_register(test_rewrite
        test/c/test_rewrite.c rewrite_test)
    add_lv_test_and_register(test_critical_pair
        test/c/test_critical_pair.c critical_pair_test)
    add_lv_test_and_register(test_type_system
        test/c/test_type_system.c type_system_test)
    add_lv_test_and_register(test_type_equiv_explorer
        test/c/test_type_equiv_explorer.c type_equiv_explorer_test)
    add_lv_test_and_register(test_proof
        test/c/test_proof.c proof_test)
    add_lv_test_and_register(test_axiom_pkg
        test/c/test_axiom_pkg.c axiom_pkg_test)
    add_lv_test_and_register(test_template_test
        test/c/test_template_test.c template_test_test)
    add_lv_test_and_register(test_module
        test/c/test_module.c module_test)
    add_lv_test_and_register(test_graph_serialize
        test/c/test_graph_serialize.c graph_serialize_test)
    add_lv_test_and_register(test_recursion
        test/c/test_recursion.c recursion_test)
    add_lv_test_and_register(test_implementation
        test/c/test_implementation.c implementation_test)
    add_lv_test_and_register(test_minimal_parse
        test/c/test_minimal_parse.c minimal_parse_test)
    add_lv_test_and_register(test_axiom_euclidean_plane
        test/c/test_axiom_euclidean_plane.c euclidean_plane_axiom_test)
    add_lv_test_and_register(test_axiom_boolean_algebra
        test/c/test_axiom_boolean_algebra.c boolean_algebra_axiom_test)
    add_lv_test_and_register(test_axiom_classical_propositional_logic
        test/c/test_axiom_classical_propositional_logic.c classical_propositional_logic_axiom_test)
    add_lv_test_and_register(test_axiom_group_theory
        test/c/test_axiom_group_theory.c group_theory_axiom_test)
    add_lv_test_and_register(test_axiom_zfc_set_theory
        test/c/test_axiom_zfc_set_theory.c zfc_set_theory_axiom_test)
    add_lv_test_and_register(test_axiom_ring_theory
        test/c/test_axiom_ring_theory.c ring_theory_axiom_test)
    add_lv_test_and_register(test_axiom_intuitionistic_logic
        test/c/test_axiom_intuitionistic_logic.c intuitionistic_logic_axiom_test)
    add_lv_test_and_register(test_axiom_intuitionistic_propositional_logic
        test/c/test_axiom_intuitionistic_propositional_logic.c intuitionistic_propositional_logic_axiom_test)
    add_lv_test_and_register(test_axiom_category_theory
        test/c/test_axiom_category_theory.c category_theory_axiom_test)
    add_lv_test_and_register(test_axiom_point_set_topology
        test/c/test_axiom_point_set_topology.c point_set_topology_axiom_test)
    add_lv_test_and_register(test_axiom_hyperbolic_geometry
        test/c/test_axiom_hyperbolic_geometry.c hyperbolic_geometry_axiom_test)
    add_lv_test_and_register(test_axiom_elliptic_geometry
        test/c/test_axiom_elliptic_geometry.c elliptic_geometry_axiom_test)
    add_lv_test_and_register(test_axiom_simple_type_theory
        test/c/test_axiom_simple_type_theory.c simple_type_theory_axiom_test)
    add_lv_test_and_register(test_axiom_field_theory
        test/c/test_axiom_field_theory.c field_theory_axiom_test)
    add_lv_test_and_register(test_axiom_nbg_set_theory
        test/c/test_axiom_nbg_set_theory.c nbg_set_theory_axiom_test)
    add_lv_test_and_register(test_axiom_real_analysis
        test/c/test_axiom_real_analysis.c real_analysis_axiom_test)
    add_lv_test_and_register(test_axiom_combinatorics
        test/c/test_axiom_combinatorics.c combinatorics_axiom_test)
    add_lv_test_and_register(test_axiom_algebraic_topology
        test/c/test_axiom_algebraic_topology.c algebraic_topology_axiom_test)
    add_lv_test_and_register(test_axiom_model_theory
        test/c/test_axiom_model_theory.c model_theory_axiom_test)
    add_lv_test_and_register(test_axiom_dependent_type_theory
        test/c/test_axiom_dependent_type_theory.c dependent_type_theory_axiom_test)
    add_lv_test_and_register(test_axiom_homological_algebra
        test/c/test_axiom_homological_algebra.c homological_algebra_axiom_test)
    add_lv_test_and_register(test_axiom_universal_algebra
        test/c/test_axiom_universal_algebra.c universal_algebra_axiom_test)
    add_lv_test_and_register(test_axiom_differential_geometry
        test/c/test_axiom_differential_geometry.c differential_geometry_axiom_test)
    add_lv_test_and_register(test_axiom_number_theory
        test/c/test_axiom_number_theory.c number_theory_axiom_test)
    add_lv_test_and_register(test_axiom_functional_analysis
        test/c/test_axiom_functional_analysis.c functional_analysis_axiom_test)
    add_lv_test_and_register(test_axiom_algebraic_geometry
        test/c/test_axiom_algebraic_geometry.c algebraic_geometry_axiom_test)
    add_lv_test_and_register(test_axiom_proof_theory
        test/c/test_axiom_proof_theory.c proof_theory_axiom_test)
    add_lv_test_and_register(test_axiom_homotopy_type_theory
        test/c/test_axiom_homotopy_type_theory.c homotopy_type_theory_axiom_test)
    add_lv_test_and_register(test_axiom_affine_geometry
        test/c/test_axiom_affine_geometry.c affine_geometry_axiom_test)
    add_lv_test_and_register(test_axiom_projective_geometry
        test/c/test_axiom_projective_geometry.c projective_geometry_axiom_test)
    add_lv_test_and_register(test_axiom_lattice_theory
        test/c/test_axiom_lattice_theory.c lattice_theory_axiom_test)
    add_lv_test_and_register(test_axiom_lie_theory
        test/c/test_axiom_lie_theory.c lie_theory_axiom_test)
    add_lv_test_and_register(test_axiom_modal_logic
        test/c/test_axiom_modal_logic.c modal_logic_axiom_test)
    add_lv_test_and_register(test_axiom_linear_logic
        test/c/test_axiom_linear_logic.c linear_logic_axiom_test)
    add_lv_test_and_register(test_axiom_metric_space
        test/c/test_axiom_metric_space.c metric_space_axiom_test)
    add_lv_test_and_register(test_axiom_game_theory
        test/c/test_axiom_game_theory.c game_theory_axiom_test)
    add_lv_test_and_register(test_axiom_information_theory
        test/c/test_axiom_information_theory.c information_theory_axiom_test)
    add_lv_test_and_register(test_axiom_descriptive_set_theory
        test/c/test_axiom_descriptive_set_theory.c descriptive_set_theory_axiom_test)
    add_lv_test_and_register(test_axiom_domain_theory
        test/c/test_axiom_domain_theory.c domain_theory_axiom_test)
    add_lv_test_and_register(test_axiom_computability_theory
        test/c/test_axiom_computability_theory.c computability_theory_axiom_test)
    add_lv_test_and_register(test_axiom_computational_complexity_theory
        test/c/test_axiom_computational_complexity_theory.c computational_complexity_theory_axiom_test)
    add_lv_test_and_register(test_axiom_presburger_arithmetic
        test/c/test_axiom_presburger_arithmetic.c presburger_arithmetic_axiom_test)
    add_lv_test_and_register(test_utils
        test/c/test_utils.c utils_test)
    add_lv_test_and_register(test_stream
        test/c/test_stream.c stream_test)
    add_lv_test_and_register(test_stream_extended
        test/c/test_stream_extended.c stream_extended_test)
    add_lv_test_and_register(test_func_block_utils
        test/c/test_func_block_utils.c func_block_utils_test)
    # 恢复：preset_blocks.c 已加入 Layer 4 构建列表，依赖已满足
    add_lv_test_and_register(test_func_block_preset
        test/c/test_func_block_preset.c func_block_preset_test)
    add_lv_test_and_register(test_error_codes
        test/c/test_error_codes.c error_codes_test)
    # 恢复：interop.c 已在 Layer 5 构建列表中，依赖已满足
    add_lv_test_and_register(test_interop
        test/c/test_interop.c interop_test)
    # 新增：高维结构模块测试
    add_lv_test_and_register(test_high_dim
        test/c/test_high_dim.c high_dim_test)
    # 新增：符号坐标系统测试
    add_lv_test_and_register(test_symbolic_coord
        test/c/test_symbolic_coord.c symbolic_coord_test)
    # 新增：v1.1.0 新模块集成测试
    add_lv_test_and_register(test_rational
        test/c/test_rational.c rational_test)
    add_lv_test_and_register(test_expr_canon
        test/c/test_expr_canon.c expr_canon_test)
    add_lv_test_and_register(test_proof_trace
        test/c/test_proof_trace.c proof_trace_test)
    # v1.1.0 新模块集成测试
    add_lv_test_and_register(test_new_modules
        test/c/test_new_modules.c new_modules_test)
    add_lv_test_and_register(test_circuit_breaker
        test/c/test_circuit_breaker.c circuit_breaker_test)
    # v3.4.0 新增：几何代数模块测试
    add_lv_test_and_register(test_ga_multivector
        test/c/test_ga_multivector.c test_ga_multivector)
    add_lv_test_and_register(test_geo_invariant
        test/c/test_geo_invariant.c test_geo_invariant)
    # v3.4.0 新增：区间/数值验证模块测试
    add_lv_test_and_register(test_interval_arithmetic
        test/c/test_interval_arithmetic.c test_interval_arithmetic)
    add_lv_test_and_register(test_gappa_dsl
        test/c/test_gappa_dsl.c test_gappa_dsl)
    # v3.4.0 新增：SMT/SAT 模块测试
    add_lv_test_and_register(test_smt_bitvector
        test/c/test_smt_bitvector.c test_smt_bitvector)
    # v3.4.0 新增：证明策略模块测试
    add_lv_test_and_register(test_proof_rule_engine
        test/c/test_proof_rule_engine.c test_proof_rule_engine)
    # v3.4.0 新增：数论/代数模块测试
    add_lv_test_and_register(test_nt_number_theory
        test/c/test_nt_number_theory.c test_nt_number_theory)
    add_lv_test_and_register(test_sym_expr
        test/c/test_sym_expr.c test_sym_expr)
    # v3.4.0 新增：重写引擎测试
    add_lv_test_and_register(test_rewrite_strategy
        test/c/test_rewrite_strategy.c test_rewrite_strategy)
    add_lv_test_and_register(test_rewrite_strategy_impl
        test/c/test_rewrite_strategy_impl.c rewrite_strategy_impl_test)
    # v3.4.0 新增：版本控制测试
    add_lv_test_and_register(test_proof_version
        test/c/test_proof_version.c test_proof_version)
    # v3.4.0 新增：自动微分测试
    add_lv_test_and_register(test_autodiff
        test/c/test_autodiff.c test_autodiff)
    # prop_verifier 测试：链接 lv 库以获取 lv_malloc/lv_free 等工具函数
    # [QA] Also listed in lv_LAYER4_SOURCES; test target requires separate compilation
    add_executable(test_prop_verifier test/c/test_prop_verifier.c core/src/layer4_reasoning/proof_system/prop_verifier.c)
    target_include_directories(test_prop_verifier PRIVATE core/include core/include/lv)
    target_link_libraries(test_prop_verifier lv ${GMP_LIBRARIES})
    add_test(NAME prop_verifier_test COMMAND test_prop_verifier
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    # v3.6.0 新增：WFC 范式三模块集成测试
    add_lv_test_and_register(test_wfc_modules
        test/c/test_wfc_modules.c wfc_modules_test)

    # v3.5.0 新增：几何约束求解器模块测试
    add_lv_test_and_register(test_geo_constraint_solver
        test/c/test_geo_constraint_solver.c geo_constraint_solver_test)
    add_lv_test_and_register(test_geo_constraint_solver_ext
        test/c/test_geo_constraint_solver_ext.c geo_constraint_solver_ext_test)
    # ODE 求解器模块测试
    add_lv_test_and_register(test_ode_solver
        test/c/test_ode_solver.c ode_solver_test)
    add_lv_test_and_register(test_proof_export_enhanced
        test/c/test_proof_export_enhanced.c proof_export_enhanced_test)
    add_lv_test_and_register(test_performance
        test/c/test_performance.c performance_test)
    # 内存管理综合测试
    add_lv_test_and_register(test_memory_management
        test/c/test_memory_management.c memory_management_test)
    # 最小化群论公理加载测试
    add_lv_test_and_register(test_group_minimal
        test/c/test_group_minimal.c group_minimal_test)
    # 几何拓扑模块测试
    add_lv_test_and_register(test_geo_topology
        test/c/test_geo_topology.c geo_topology_test)
    # 几何谓词模块测试
    add_lv_test_and_register(test_geo_predicate
        test/c/test_geo_predicate.c geo_predicate_test)
    # Halfedge 网格拓扑模块测试
    add_lv_test_and_register(test_geo_halfedge_mesh
        test/c/test_geo_halfedge_mesh.c geo_halfedge_mesh_test)
    # 动态几何依赖图模块测试
    add_lv_test_and_register(test_geo_dynamic
        test/c/test_geo_dynamic.c geo_dynamic_test)
    # 几何可视化渲染模块测试
    add_lv_test_and_register(test_geo_visual
        test/c/test_geo_visual.c geo_visual_test)
    # v3.6.0 WFC 约束传播引擎测试
    add_lv_test_and_register(test_propagation
        test/c/test_propagation.c propagation_test)
    # v3.6.0 等价类管理器测试
    add_lv_test_and_register(test_equiv_class
        test/c/test_equiv_class.c equiv_class_test)
    # v3.6.0 交互几何系统测试
    add_lv_test_and_register(test_interactive_geo
        test/c/test_interactive_geo.c interactive_geo_test)
    # v3.7.0 几何变换预设测试
    add_lv_test_and_register(test_geom_transform_presets
        test/c/test_geom_transform_presets.c geom_transform_presets_test)
    # AABB 树空间索引模块测试
    add_lv_test_and_register(test_geo_aabb_tree
        test/c/test_geo_aabb_tree.c geo_aabb_tree_test)
    # 错误处理与边界条件测试
    add_lv_test_and_register(test_error_handling
        test/c/test_error_handling.c error_handling_test)
    # 矛盾约束检测器测试
    add_lv_test_and_register(test_conflict_detector
        test/c/test_conflict_detector.c conflict_detector_test)
    add_lv_test_and_register(test_adaptive_threshold
        test/c/test_adaptive_threshold.c adaptive_threshold_test)
    # 引擎核心操作测试
    add_lv_test_and_register(test_engine_ops
        test/c/test_engine_ops.c engine_ops_test)
    # 引擎调度器测试
    add_lv_test_and_register(test_engine_scheduler
        test/c/test_engine_scheduler.c engine_scheduler_test)
    # 信任颜色映射系统测试（Gap 1）
    add_lv_test_and_register(test_trust_color
        test/c/test_trust_color.c trust_color_test)
    # Layer5 输出层操作测试
    add_lv_test_and_register(test_layer5_output_ops
        test/c/test_layer5_output_ops.c layer5_output_ops_test)
    # 求解器调试测试
    add_lv_test_and_register(test_solve_debug
        test/c/test_solve_debug.c solve_debug_test)
    # .lv 词法分析器测试
    add_lv_test_and_register(test_lv_lexer
        test/c/test_lv_lexer.c lv_lexer_test)

    # .lv 解析器测试
    add_lv_test_and_register(test_lv_parser
        test/c/test_lv_parser.c lv_parser_test)

    # v1.9.0 micro-bootstrap: 集成测试
    add_lv_test_and_register(test_lv_bootstrap
        test/c/test_lv_bootstrap.c lv_bootstrap_test)

    # 元验证逻辑测试
    add_lv_test_and_register(test_meta_verify
        test/c/test_meta_verify.c meta_verify_test)

    # λ-演算端到端测试（Church 数字编码、β-归约、Y 组合子）
    add_lv_test_and_register(test_lambda_church
        test/c/test_lambda_church.c lambda_church_test)

    # λ-演算 β-归约结果验证（图结构指纹比对）
    add_lv_test_and_register(test_lambda_eval
        test/c/test_lambda_eval.c lambda_eval_test)

    # 新增公理包测试
    add_lv_test_and_register(test_axiom_topos_theory
        test/c/test_axiom_topos_theory.c topos_theory_axiom_test)
    add_lv_test_and_register(test_axiom_synthetic_differential_geometry
        test/c/test_axiom_synthetic_differential_geometry.c synthetic_differential_geometry_axiom_test)
    add_lv_test_and_register(test_axiom_second_order_arithmetic
        test/c/test_axiom_second_order_arithmetic.c second_order_arithmetic_axiom_test)
    add_lv_test_and_register(test_axiom_robin_arithmetic
        test/c/test_axiom_robin_arithmetic.c robin_arithmetic_axiom_test)
    add_lv_test_and_register(test_axiom_quantum_information_theory
        test/c/test_axiom_quantum_information_theory.c quantum_information_theory_axiom_test)
    add_lv_test_and_register(test_axiom_probability_theory
        test/c/test_axiom_probability_theory.c probability_theory_axiom_test)
    add_lv_test_and_register(test_axiom_peano_arithmetic
        test/c/test_axiom_peano_arithmetic.c peano_arithmetic_axiom_test)
    add_lv_test_and_register(test_axiom_order_theory
        test/c/test_axiom_order_theory.c order_theory_axiom_test)
    add_lv_test_and_register(test_axiom_non_well_founded_set_theory
        test/c/test_axiom_non_well_founded_set_theory.c non_well_founded_set_theory_axiom_test)
    add_lv_test_and_register(test_axiom_measure_theory
        test/c/test_axiom_measure_theory.c measure_theory_axiom_test)
    add_lv_test_and_register(test_axiom_linear_algebra
        test/c/test_axiom_linear_algebra.c linear_algebra_axiom_test)
    add_lv_test_and_register(test_axiom_graph_theory
        test/c/test_axiom_graph_theory.c graph_theory_axiom_test)
    add_lv_test_and_register(test_axiom_galois_theory
        test/c/test_axiom_galois_theory.c galois_theory_axiom_test)
    add_lv_test_and_register(test_axiom_ergodic_theory
        test/c/test_axiom_ergodic_theory.c ergodic_theory_axiom_test)
    add_lv_test_and_register(test_axiom_cartesian_closed_category
        test/c/test_axiom_cartesian_closed_category.c cartesian_closed_category_axiom_test)

    # === New tests: Layer 3 geometry core ===
    add_lv_test_and_register(test_symbolic_coord_ops
        test/c/test_symbolic_coord_ops.c symbolic_coord_ops_test)
    add_lv_test_and_register(test_geometry_core
        test/c/test_geometry_core.c geometry_core_test)

    # === New tests: Layer 4 backends ===
    add_lv_test_and_register(test_smt_backend
        test/c/test_smt_backend.c smt_backend_test)
    add_lv_test_and_register(test_bdd_sat_atp
        test/c/test_bdd_sat_atp.c bdd_sat_atp_test)

    # === New tests: Layer 4 solver submodules ===
    add_lv_test_and_register(test_solver_submodules
        test/c/test_solver_submodules.c solver_submodules_test)

    # === New tests: Layer 4 misc (mini_kernel / relation_model / algebra_mode / rewrite) ===
    add_lv_test_and_register(test_layer4_misc
        test/c/test_layer4_misc.c layer4_misc_test)

    # === New tests: Layer 5 output (magic / plugin / tikz / proof_compiler / protocol) ===
    add_lv_test_and_register(test_layer5_output
        test/c/test_layer5_output.c layer5_output_test)
    add_lv_test_and_register(test_layer5_core
        test/c/test_layer5_core.c layer5_core_test)
    add_lv_test_and_register(test_output_export
        test/c/test_output_export.c output_export_test)

    # === New tests: Layer 6 visual (canvas / scheduler / type / converter) ===
    add_lv_test_and_register(test_layer6_visual
        test/c/test_layer6_visual.c layer6_visual_test)

    # === New tests: Proof infrastructure (command_log / proof_navigator / dependency) ===
    add_lv_test_and_register(test_proof_infra
        test/c/test_proof_infra.c proof_infra_test)

    # 基准测试（构建但不注册到 CTest，手动运行）
    add_lv_test(test_benchmark test/c/test_benchmark.c)
endif()

# ============================================================
# 模糊测试
# ============================================================
if(BUILD_FUZZERS AND CMAKE_C_COMPILER_ID MATCHES "Clang")
    if(WIN32)
        message(WARNING "BUILD_FUZZERS is enabled, but libFuzzer is not supported by this Windows Clang toolchain; skipping fuzz targets.")
    else()
        add_executable(fuzz_constraint_graph test/fuzz/fuzz_constraint_graph.c)
        target_link_libraries(fuzz_constraint_graph lv ${GMP_LIBRARIES})
        target_compile_options(fuzz_constraint_graph PRIVATE -fsanitize=fuzzer,address)
        target_link_options(fuzz_constraint_graph PRIVATE -fsanitize=fuzzer,address)

        add_executable(fuzz_symbolic_coord test/fuzz/fuzz_symbolic_coord.c)
        target_link_libraries(fuzz_symbolic_coord lv ${GMP_LIBRARIES})
        target_compile_options(fuzz_symbolic_coord PRIVATE -fsanitize=fuzzer,address)
        target_link_options(fuzz_symbolic_coord PRIVATE -fsanitize=fuzzer,address)
    endif()
endif()

# ============================================================
# 示例程序
# ============================================================
if(BUILD_EXAMPLES)
    function(add_lv_example name source)
        add_executable(${name} ${source})
        target_link_libraries(${name} lv ${GMP_LIBRARIES})
    endfunction()

    add_lv_example(example_triangle    test/examples/triangle_construction.c)
    add_lv_example(example_circle      test/examples/circle_intersection.c)
    add_lv_example(example_composition test/examples/function_composition.c)
    add_lv_example(example_high_dim    test/examples/high_dim_demo.c)
    add_lv_example(example_streaming      test/examples/streaming_demo.c)
    add_lv_example(example_stream_advanced test/examples/stream_advanced_demo.c)
    add_lv_example(example_type_system     test/examples/type_system_demo.c)
    add_lv_example(example_recursion       test/examples/recursion_demo.c)
    # example_proof 使用已移除的旧 API (PROPOSITION_ATOMIC, lvLogicContext 等)，暂不构建
    # add_lv_example(example_proof           test/examples/proof_demo.c)
endif()

# 修复 1：安装规则 —— 支持 cmake --install 将库文件和头文件安装到系统目录
include(GNUInstallDirs)

install(TARGETS lv_static
    EXPORT lv-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY core/include/lv
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
    PATTERN ".gitkeep" EXCLUDE
)

if(BUILD_SHARED_LIBS)
    install(TARGETS lv_shared
        EXPORT lv-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

# [P0 修复] 导出 CMake 目标文件，供 find_package(lv) 使用
install(EXPORT lv-targets
    FILE lv-targets.cmake
    NAMESPACE lv::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/build/build/cmake/lv
)

# ============================================================
# v1.1.0: pkg-config .pc 文件生成
#
# 安装后，使用者可以通过以下命令获取编译和链接标志:
#   pkg-config --cflags lv
#   pkg-config --libs lv
# ============================================================

# 准备 pkg-config 变量
set(lv_PC_REQUIRES "gmp")            # 依赖的 pkg-config 包

# 计算 libdir（相对于 prefix）
# 若 CMAKE_INSTALL_LIBDIR 是绝对路径则使用，否则添加 ${prefix}/
if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
    set(lv_PC_LIBDIR "${CMAKE_INSTALL_LIBDIR}")
else()
    set(lv_PC_LIBDIR "\${prefix}/${CMAKE_INSTALL_LIBDIR}")
endif()

if(IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
    set(lv_PC_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}")
else()
    set(lv_PC_INCLUDEDIR "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
endif()

# 生成 lv.pc
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/lv.pc.in
    ${CMAKE_BINARY_DIR}/lv.pc
    @ONLY
)

install(FILES ${CMAKE_BINARY_DIR}/lv.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# ============================================================
# v1.1.0: CMake 包配置文件生成
#
# 安装后，其他 CMake 项目可以通过以下方式引用:
#   find_package(lv 1.1.0 REQUIRED)
#   target_link_libraries(myapp lv::lv)
# ============================================================

include(CMakePackageConfigHelpers)

# 包安装路径
set(lv_CMAKE_CONFIG_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/lv")

# 生成 lv-config.cmake 和 lv-config-version.cmake
configure_package_config_file(
    ${CMAKE_SOURCE_DIR}/cmake/lv-config.cmake.in
    ${CMAKE_BINARY_DIR}/lv-config.cmake
    INSTALL_DESTINATION ${lv_CMAKE_CONFIG_DIR}
)

write_basic_package_version_file(
    ${CMAKE_BINARY_DIR}/lv-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# 安装 CMake 包配置文件
install(FILES
    ${CMAKE_BINARY_DIR}/lv-config.cmake
    ${CMAKE_BINARY_DIR}/lv-config-version.cmake
    DESTINATION ${lv_CMAKE_CONFIG_DIR}
)

# ============================================================
# v1.1.0: CPack 打包配置
#
# 生成可分发的归档包（.tar.gz, .zip）和安装器包（.deb, .rpm, NSIS）。
#
# 用法:
#   cmake --build . --target package          # 生成二进制包
#   cmake --build . --target package_source   # 生成源码包
# ============================================================

set(CPACK_PACKAGE_NAME            "lv")
set(CPACK_PACKAGE_VENDOR          "Lv-00 Project")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Lv-00: A geometric metalanguage for dual-mode mathematics")
set(CPACK_PACKAGE_VERSION_MAJOR   ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR   ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH   ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_FILE_NAME       "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT         "Lv-00 Project <lv@example.com>")
set(CPACK_RESOURCE_FILE_LICENSE   "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README    "${CMAKE_SOURCE_DIR}/README.md")

# 包生成器
if(WIN32)
    set(CPACK_GENERATOR "ZIP")
    # 如果安装了 NSIS，也可以生成 Windows 安装器
    find_program(NSIS_FOUND makensis)
    if(NSIS_FOUND)
        list(APPEND CPACK_GENERATOR "NSIS")
    endif()
elseif(APPLE)
    set(CPACK_GENERATOR "TGZ")
    # macOS 可选: productbuild, DragNDrop
else()
    set(CPACK_GENERATOR "TGZ;DEB;RPM")
endif()

# DEB 包特定设置
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Lv-00 Project")
set(CPACK_DEBIAN_PACKAGE_SECTION     "libs")
set(CPACK_DEBIAN_PACKAGE_DEPENDS     "libgmp10 (>= 6.0.0)")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE    "https://github.com/example/lv")

# RPM 包特定设置
set(CPACK_RPM_PACKAGE_LICENSE        "MIT")
set(CPACK_RPM_PACKAGE_GROUP          "Development/Libraries")
set(CPACK_RPM_PACKAGE_REQUIRES       "gmp >= 6.0.0")
set(CPACK_RPM_PACKAGE_URL            "https://github.com/example/lv")

# 排除不需要的文件
set(CPACK_SOURCE_IGNORE_FILES
    "/build/"
    "/.git/"
    "/.vs/"
    "\\\\.gitignore$"
    "\\\\.vscode/"
    ".*~$"
)

include(CPack)