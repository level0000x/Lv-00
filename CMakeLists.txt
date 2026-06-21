cmake_minimum_required(VERSION 3.15)
project(lv00 VERSION 1.1.0 LANGUAGES C)  # 版本号与 lv00.h 中 LV00_VERSION_MAJOR/MINOR/PATCH 保持一致

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
    add_compile_options(
        -Wno-sign-conversion
        -Wno-unused-parameter
        -Wno-missing-field-initializers
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
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s EXPORT_NAME='Lv00Module'")
    
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
    add_definitions(-DLV00_WASM_BUILD=1)
    add_definitions(-DLV00_NO_GMP=1)
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
#   4. lv00_static 聚合所有 OBJECT 库，保持向后兼容
# 详见 doc/docs/ARCHITECTURE_v1.1.md
# ============================================================

# ── 共享头文件列表（所有层共用）──
set(LV00_HEADERS
    core/include/lv00/lv00.h
    core/include/lv00/cross_platform.h
    core/include/lv00/error_codes.h
    core/include/lv00/symbolic_coord.h
    core/include/lv00/constraint_graph.h
    core/include/lv00/normalization.h
    core/include/lv00/graph_hash.h
    core/include/lv00/solver.h
    core/include/lv00/rewrite.h
    core/include/lv00/unify.h
    # v3.6.0 新增：WFC 范式头文件
    core/include/lv00/propagation.h
    core/include/lv00/equiv_class.h
    core/include/lv00/meta_proof.h
    core/include/lv00/axiom_pkg.h
    core/include/lv00/module.h
    core/include/lv00/engine.h
    core/include/lv00/debug.h
    core/include/lv00/mpz_poly.h
    core/include/lv00/func_block.h
    core/include/lv00/type_system.h
    core/include/lv00/proof.h
    core/include/lv00/recursion.h
    core/include/lv00/lv00_utils.h
    core/include/lv00/high_dim.h
    core/include/lv00/interop.h
    core/include/lv00/formula_parser.h
    core/include/lv00/formula_renderer.h
    core/include/lv00/formula_converter.h
    core/include/lv00/prop_verifier.h
    core/include/lv00/lexer_shared.h
    core/include/lv00/stream.h
    core/include/lv00/func_block_utils.h
    core/include/lv00/stream_context_util.h
    core/include/lv00/func_block_registry.h
    core/include/lv00/func_block_preset_ops.h
    # v1.1.0 新增模块头文件（第一轮：核心计算模块）
    core/include/lv00/groebner_engine.h
    core/include/lv00/engine_scheduler.h
    core/include/lv00/euclidean_geometry.h
    core/include/lv00/geometry_compress.h
    core/include/lv00/sparse_linear_algebra.h
    core/include/lv00/float_error.h
    core/include/lv00/smt_backend.h
    core/include/lv00/bdd_encoding.h
    core/include/lv00/approx_counter.h
    # v1.1.0 新增模块头文件（第二轮：计算后端与数值模块）
    core/include/lv00/atp_backend.h
    core/include/lv00/solver_core.h
    core/include/lv00/sat_encoding.h
    core/include/lv00/relation_model.h
    core/include/lv00/numerical_backend.h
    core/include/lv00/geom_evol.h
    core/include/lv00/geo_event_detect.h
    core/include/lv00/lv00_numeric.h
    core/include/lv00/probabilistic_constraint.h
    # v1.1.0 新增模块头文件（第三轮：几何构造与交互模块）
    core/include/lv00/algebra_mode.h
    core/include/lv00/dsl_compiler.h
    core/include/lv00/interactive_geo.h
    core/include/lv00/tikz_export.h
    core/include/lv00/math_input.h
    core/include/lv00/math_protocol.h
    core/include/lv00/ecosystem.h
    # v1.1.0 新增模块头文件（第四轮：证明与规约模块）
    core/include/lv00/mini_kernel.h
    core/include/lv00/gc_language.h
    core/include/lv00/geo_spec.h
    core/include/lv00/proof_widget.h
    core/include/lv00/path_type.h
    # 模块化预设函数块系统
    core/include/lv00/preset_blocks.h
    core/include/lv00/preset_register_macros.h
    core/include/lv00/preset_basic_geometry.h
    core/include/lv00/preset_transformations.h
    core/include/lv00/preset_measurements.h
    core/include/lv00/preset_polygons.h
    core/include/lv00/preset_algebraic.h
    core/include/lv00/preset_number_theory.h
    core/include/lv00/preset_group_theory.h
    core/include/lv00/preset_topology.h
    core/include/lv00/preset_analysis.h
    core/include/lv00/preset_combinatorics.h
    core/include/lv00/preset_graph_theory.h
    core/include/lv00/preset_probability.h
    core/include/lv00/preset_numerical.h
    core/include/lv00/preset_optimization.h
    core/include/lv00/preset_advanced_geometry.h
    core/include/lv00/preset_geometry_3d.h
    core/include/lv00/preset_field_theory.h
    core/include/lv00/preset_ring_theory.h
    core/include/lv00/preset_linear_algebra.h
    core/include/lv00/preset_trigonometry.h
    core/include/lv00/preset_differential_equations.h
    core/include/lv00/preset_measure_theory.h
    core/include/lv00/preset_functional_analysis.h
    core/include/lv00/preset_set_theory.h
    core/include/lv00/preset_math_logic.h
    core/include/lv00/preset_category_theory.h
    core/include/lv00/preset_category_theory_adv.h
    core/include/lv00/preset_complex_analysis.h
    core/include/lv00/preset_differential_geometry.h
    core/include/lv00/preset_differential_geometry_adv.h
    core/include/lv00/preset_numerical_analysis.h
    core/include/lv00/preset_probability_statistics.h
    core/include/lv00/preset_statistics.h
    # v11.0 新增：理论数学研究模块
    core/include/lv00/preset_algebraic_geometry.h
    core/include/lv00/preset_homological_algebra.h
    core/include/lv00/preset_lie_theory_advanced.h
    # v12.0 新增：随机过程、博弈论、信息论、编码理论、差分方程模块
    core/include/lv00/preset_stochastic_processes.h
    core/include/lv00/preset_game_theory.h
    core/include/lv00/preset_information_theory.h
    core/include/lv00/preset_coding_theory.h
    core/include/lv00/preset_difference_equations.h
    # v1.1.0 性能优化模块
    core/include/lv00/thread_pool.h
    core/include/lv00/simd_ops.h
    core/include/lv00/fast_index.h
    core/include/lv00/benchmark.h
    # v1.1.0 新增核心模块（整改实施）
    core/include/lv00/memory_pool.h
    core/include/lv00/expr_canonical.h
    core/include/lv00/geometry_transform.h
    core/include/lv00/inequality_reasoning.h
    core/include/lv00/runtime_monitor.h
    core/include/lv00/axiom_rule_engine.h
    core/include/lv00/proof_engine_enhanced.h
    core/include/lv00/test_framework.h
    # Magic 模拟器模块
    core/include/lv00/magic.h
    # 内部/实现头文件（用于 IDE 索引和 OBJECT 库关联）
    core/include/lv00/lv00_internal.h
    core/include/lv00/config.h
    core/include/lv00/context.h
    core/include/lv00/geometry_types.h
    core/include/lv00/node_deep_copy.h
    core/include/lv00/parser_safety.h
    core/include/lv00/func_block_internal.h
    core/include/lv00/func_block_preset.h
    core/include/lv00/expr_canon.h
    core/include/lv00/modal_operators.h
    core/include/lv00/logic_check.h
    core/include/lv00/circuit_breaker.h
    core/include/lv00/axiom_grade.h
    core/include/lv00/three_valued_logic.h
    core/include/lv00/quantifier.h
    core/include/lv00/rational.h
    core/include/lv00/proof_priority.h
    core/include/lv00/proof_trace.h
    core/include/lv00/proof_score.h
    core/include/lv00/runtime_guard.h
    core/include/lv00/status_codes.h
    core/include/lv00/exact_arithmetic.h
    # func_block 内部预设头文件
    core/include/lv00/preset_basic_math.h
    core/include/lv00/preset_common.h
    core/include/lv00/preset_calculus.h
    core/include/lv00/preset_core.h
    core/include/lv00/preset_blocks.h
    # 预设模块头文件（内部实现）
    core/include/lv00/preset_algebraic_topology.h
    core/include/lv00/preset_algebraic_topology_adv.h
    core/include/lv00/preset_arithmetic_geometry.h
    core/include/lv00/preset_dynamical_systems.h
    core/include/lv00/preset_functional_analysis_adv.h
    core/include/lv00/preset_integral_transforms.h
    core/include/lv00/preset_lattice_theory.h
    core/include/lv00/preset_logic_advanced.h
    core/include/lv00/preset_mathematical_logic.h
    core/include/lv00/preset_mathematical_physics.h
    core/include/lv00/preset_matrix.h
    core/include/lv00/preset_order_theory.h
    core/include/lv00/preset_polynomial.h
    core/include/lv00/preset_representation_theory.h
    core/include/lv00/preset_special_functions.h
    # [QA] Pending header additions: adaptive_pruning, autodiff, cache_manager,
    # conflict_detector, ga_codegen, ga_interface, ga_multivector, groebner_parallel,
    # math_theory_guide_cn, meta_repr, nt_number_theory, nt_polynomial, ode_solver,
    # plugin_system, proof_compiler, proof_contradiction (see TASK_CONTEXT_QA.md R6)
)

# ── Layer 1: 输入解析层 (Parser) ──
# 职责：词法分析、公式解析、DSL 编译
# 依赖：Layer 2
set(LV00_LAYER1_SOURCES
    core/src/layer1_parser/formula_parser.c
    core/src/layer1_parser/formula_converter.c
    core/src/layer1_parser/formula_renderer.c
    core/src/layer1_parser/lexer_shared.c
    core/src/layer1_parser/parser_safety.c
    core/src/layer1_parser/dsl_compiler.c
    core/src/layer1_parser/math_input.c
)

# ── Layer 2: 资源管理层 (Resource) ──
# 职责：内存分配、错误码、调试、深拷贝、工具函数
# 依赖：GMP，C 标准库（仅 Layer 2 能直接依赖 GMP 链接）
set(LV00_LAYER2_SOURCES
    core/src/layer2_resource/lv00_utils.c
    core/src/layer2_resource/lv00.c
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
)

# ── Layer 3: 几何拓扑层 (Geometry) ──
# 职责：约束图、符号坐标、几何原语、高维结构
# 依赖：Layer 2，GMP（通过 Layer 2 传递）
set(LV00_LAYER3_SOURCES
    core/src/layer3_geometry/constraint_graph.c
    core/src/layer3_geometry/symbolic_coord.c
    core/src/layer3_geometry/euclidean_geometry.c
    core/src/layer3_geometry/interactive_geo.c
    core/src/layer3_geometry/high_dim.c
    core/src/layer3_geometry/geometry_compress.c
    core/src/layer3_geometry/geo_event_detect.c
    core/src/layer3_geometry/geo_spec.c
    core/src/layer3_geometry/geometry_csg.c
    core/src/layer3_geometry/sparse_linear_algebra.c
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
    # [已移至 Layer 4] SMT/SAT 模块
    # core/src/layer3_geometry/smt_theory_combiner.c
    # core/src/layer3_geometry/smt_bitvector.c
    # core/src/layer3_geometry/smt_trigger_engine.c
    # [已移至 Layer 4] 证明策略模块
    # core/src/layer3_geometry/proof_rule_engine.c
    # core/src/layer3_geometry/proof_session.c
    # [已移至 Layer 4] 数论/代数模块
    # core/src/layer3_geometry/nt_number_theory.c
    # core/src/layer3_geometry/nt_polynomial.c
    # core/src/layer3_geometry/sym_expr.c
    # [已移至 Layer 4] 重写引擎模块
    # core/src/layer3_geometry/rewrite_strategy.c
    # [已移至 Layer 4] 版本控制模块
    # core/src/layer3_geometry/proof_version.c
    # [已移至 Layer 4] 自动微分模块
    # core/src/layer3_geometry/autodiff.c
    # v1.1.0 性能优化模块
    core/src/layer3_geometry/thread_pool.c
    core/src/layer3_geometry/simd_ops.c
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
    # [已移至 Layer 4] ODE 求解器
    # core/src/layer3_geometry/ode_solver.c
)

# ── Layer 4: 公理推理层 (Reasoning) ──
# 职责：引擎、求解器、证明、重写、合一、规范化、类型系统、公理包
# 依赖：Layer 3，Layer 2
set(LV00_LAYER4_SOURCES
    # 核心引擎与求解
    core/src/layer4_reasoning/engine.c
    core/src/layer4_reasoning/engine_scheduler.c
    core/src/layer4_reasoning/solver.c
    core/src/layer4_reasoning/solver_core.c
    # 证明系统
    core/src/layer4_reasoning/proof.c
    core/src/layer4_reasoning/proof_optimize.c
    core/src/layer4_reasoning/proof_multi_strategy.c
    core/src/layer4_reasoning/proof_trace.c
    core/src/layer4_reasoning/proof_priority.c
    core/src/layer4_reasoning/logic_check.c
    core/src/layer4_reasoning/circuit_breaker.c
    # 三值逻辑、模态算子、量词系统
    core/src/layer4_reasoning/three_valued_logic.c
    core/src/layer4_reasoning/modal_operators.c
    core/src/layer4_reasoning/quantifier.c
    # 增强证明引擎
    core/src/layer4_reasoning/proof_engine_enhanced.c
    # 重写与合一
    core/src/layer4_reasoning/rewrite.c
    core/src/layer4_reasoning/unify.c
    core/src/layer4_reasoning/normalization.c
    core/src/layer4_reasoning/recursion.c
    # 类型与属性
    core/src/layer4_reasoning/type_system.c
    core/src/layer4_reasoning/prop_verifier.c
    # 后端求解引擎
    core/src/layer4_reasoning/groebner_engine.c
    core/src/layer4_reasoning/smt_backend_impl.c
    core/src/layer4_reasoning/atp_backend.c
    core/src/layer4_reasoning/sat_encoding.c
    core/src/layer4_reasoning/bdd_encoding.c
    core/src/layer4_reasoning/approx_counter.c
    core/src/layer4_reasoning/probabilistic_constraint.c
    core/src/layer4_reasoning/rational.c
    core/src/layer4_reasoning/expr_canon.c
    core/src/layer4_reasoning/exact_arithmetic.c
    # v1.1.0 新增：表达式规范化、不等式推理
    core/src/layer4_reasoning/expr_canonical.c
    # 模型与模式
    core/src/layer4_reasoning/relation_model.c
    core/src/layer4_reasoning/algebra_mode.c
    core/src/layer4_reasoning/numerical_backend.c
    # 内核与协议
    core/src/layer4_reasoning/module.c
    core/src/layer4_reasoning/mini_kernel.c
    core/src/layer4_reasoning/gc_language.c
    core/src/layer4_reasoning/ecosystem.c
    core/src/layer4_reasoning/math_protocol.c
    core/src/layer4_reasoning/stream.c
    core/src/layer4_reasoning/stream_context_util.c
    # SMT/SAT 后端（从 Layer 3 迁移）
    core/src/layer4_reasoning/smt_theory_combiner.c
    core/src/layer4_reasoning/smt_bitvector.c
    core/src/layer4_reasoning/smt_trigger_engine.c
    # 证明策略（从 Layer 3 迁移）
    core/src/layer4_reasoning/proof_rule_engine.c
    core/src/layer4_reasoning/proof_session.c
    # 数论/代数（从 Layer 3 迁移）
    core/src/layer4_reasoning/nt_number_theory.c
    core/src/layer4_reasoning/nt_polynomial.c
    core/src/layer4_reasoning/sym_expr.c
    # 重写引擎（从 Layer 3 迁移）
    core/src/layer4_reasoning/rewrite_strategy.c
    # 版本控制（从 Layer 3 迁移）
    core/src/layer4_reasoning/proof_version.c
    # 自动微分（从 Layer 3 迁移）
    core/src/layer4_reasoning/autodiff.c
    # ODE 求解器（从 Layer 3 迁移）
    core/src/layer4_reasoning/ode_solver.c
    # v1.1.0 新增：不等式推理系统（桩实现）
    core/src/layer4_reasoning/inequality_reasoning.c
    # v4.0.0 新增：自适应剪枝策略（动态阈值 + 启发式评分）
    core/src/layer4_reasoning/adaptive_pruning.c
    # v5.0.0 新增：并行 Groebner 引擎（并发优化）
    core/src/layer4_reasoning/groebner_parallel.c
    # 公理包
    core/src/layer4_reasoning/axiom/axiom_pkg.c
    core/src/layer4_reasoning/axiom/axiom_grade.c
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
    core/src/layer4_reasoning/conflict_detector.c
    core/src/layer4_reasoning/proof_contradiction.c
    core/src/layer4_reasoning/func_block/preset_abstract_algebra.c
)

# ── Layer 5: 结果输出层 (Output) ──
# 职责：流式输出、TikZ 导出、证明可视化、模块、互操作
# 依赖：Layer 4，Layer 3，Layer 2
set(LV00_LAYER5_SOURCES
    core/src/layer5_output/proof_widget.c
    core/src/layer5_output/tikz_export.c
    core/src/layer5_output/proof_export_enhanced.c
    core/src/layer5_output/interop/interop.c
    # Magic 模拟器模块
    core/src/layer5_output/magic/magic.c
    # 补全遗漏文件：几何可视化（基础版 + 完整版）
    core/src/layer5_output/geo_visual.c
    core/src/layer5_output/geo_visual_complete.c
)

# ============================================================
# 库目标 —— 五层 OBJECT 库 + 聚合静态库
# ============================================================

# ── 辅助宏：为每个层设置公共编译属性 ──
macro(lv00_setup_layer target layer_id)
    target_include_directories(${target} PUBLIC
        ${GMP_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/core/include
        ${CMAKE_SOURCE_DIR}/core/include/lv00
        ${CMAKE_SOURCE_DIR}/core/src
        ${CMAKE_SOURCE_DIR}/core/src/preset
    )
    # 层级标识（用于编译时诊断和层级验证）
    target_compile_definitions(${target} PRIVATE LV00_CURRENT_LAYER=${layer_id})
    if(ENABLE_LAYER_VALIDATION)
        target_compile_definitions(${target} PRIVATE LV00_ENABLE_LAYER_VALIDATION)
    endif()
endmacro()

# Layer 2: 资源管理层（基础层，不依赖任何上层）
add_library(lv00_layer2_resource OBJECT ${LV00_LAYER2_SOURCES} ${LV00_HEADERS})
lv00_setup_layer(lv00_layer2_resource 2)
target_link_libraries(lv00_layer2_resource ${GMP_LIBRARIES})

# Layer 1: 输入解析层（依赖 Layer 2）
add_library(lv00_layer1_parser OBJECT ${LV00_LAYER1_SOURCES} ${LV00_HEADERS})
lv00_setup_layer(lv00_layer1_parser 1)
target_link_libraries(lv00_layer1_parser lv00_layer2_resource)

# Layer 3: 几何拓扑层（依赖 Layer 2）
add_library(lv00_layer3_geometry OBJECT ${LV00_LAYER3_SOURCES} ${LV00_HEADERS})
lv00_setup_layer(lv00_layer3_geometry 3)
target_link_libraries(lv00_layer3_geometry lv00_layer2_resource)
# v1.1.0: 线程库链接（用于 thread_pool.c）
find_package(Threads REQUIRED)
target_link_libraries(lv00_layer3_geometry Threads::Threads)

# Layer 4: 公理推理层（依赖 Layer 3，Layer 2）
add_library(lv00_layer4_reasoning OBJECT ${LV00_LAYER4_SOURCES} ${LV00_HEADERS})
lv00_setup_layer(lv00_layer4_reasoning 4)
target_link_libraries(lv00_layer4_reasoning lv00_layer3_geometry lv00_layer2_resource)

# Layer 5: 结果输出层（依赖 Layer 4，Layer 3，Layer 2）
add_library(lv00_layer5_output OBJECT ${LV00_LAYER5_SOURCES} ${LV00_HEADERS})
lv00_setup_layer(lv00_layer5_output 5)
target_link_libraries(lv00_layer5_output lv00_layer4_reasoning lv00_layer3_geometry lv00_layer2_resource)

# ── Layer 6: 图形化编程层 (Visual Programming Layer) ──
# 设计文档: doc/docs/35_layer6_visual_programming.md
# 四大子系统: 可视化编辑器、函数块运行时扩展、类型系统扩展、表示转换层

set(LV00_LAYER6_SOURCES
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
    core/src/layer6_visual/converter/block_to_geometry.c
    core/src/layer6_visual/converter/block_to_node.c
    core/src/layer6_visual/converter/block_to_text.c
    core/src/layer6_visual/converter/sync_protocol.c
)

set(LV00_LAYER6_HEADERS
    core/include/lv00/visual_editor.h
    core/include/lv00/control_flow_blocks.h
    core/include/lv00/io_blocks.h
    core/include/lv00/data_structure_blocks.h
    core/include/lv00/extended_types.h
    core/include/lv00/effect_system.h
    core/include/lv00/block_scheduler.h
    core/include/lv00/representation_converter.h
)

add_library(lv00_layer6_visual OBJECT ${LV00_LAYER6_SOURCES} ${LV00_LAYER6_HEADERS})
lv00_setup_layer(lv00_layer6_visual 6)
target_link_libraries(lv00_layer6_visual lv00_layer5_output lv00_layer4_reasoning lv00_layer3_geometry lv00_layer2_resource)

# ── Layer 7: 编排调度层 (Orchestration Layer) ──
set(LV00_LAYER7_SOURCES
    core/src/layer7_orchestration/orchestrator.c
)
set(LV00_LAYER7_HEADERS
    core/include/lv00/orchestrator.h
)
add_library(lv00_layer7_orchestration OBJECT ${LV00_LAYER7_SOURCES} ${LV00_LAYER7_HEADERS})
lv00_setup_layer(lv00_layer7_orchestration 7)
target_link_libraries(lv00_layer7_orchestration lv00_layer6_visual lv00_layer5_output lv00_layer4_reasoning lv00_layer3_geometry lv00_layer2_resource)

# ── Layer 8: 元验证层 (Meta-Verification Layer) ──
set(LV00_LAYER8_SOURCES
    core/src/layer8_meta_verify/meta_verify.c
)
set(LV00_LAYER8_HEADERS
    core/include/lv00/meta_verify.h
)
add_library(lv00_layer8_meta_verify OBJECT ${LV00_LAYER8_SOURCES} ${LV00_LAYER8_HEADERS})
lv00_setup_layer(lv00_layer8_meta_verify 8)
target_link_libraries(lv00_layer8_meta_verify lv00_layer7_orchestration)

# ── Layer 9: 应用入口层 (Application Layer) ──
set(LV00_LAYER9_SOURCES
    core/src/layer9_application/application.c
)
set(LV00_LAYER9_HEADERS
    core/include/lv00/application.h
)
add_library(lv00_layer9_application OBJECT ${LV00_LAYER9_SOURCES} ${LV00_LAYER9_HEADERS})
lv00_setup_layer(lv00_layer9_application 9)
target_link_libraries(lv00_layer9_application lv00_layer8_meta_verify)

# ── Layer 10: 外部集成层 (Interop Layer) ──
set(LV00_LAYER10_SOURCES
    core/src/layer10_interop/interop.c
    core/src/layer10_interop/lean4_bridge.c
    core/src/layer10_interop/coq_bridge.c
    core/src/layer10_interop/opml_codec.c
)
set(LV00_LAYER10_HEADERS
    core/include/lv00/interop.h
)
add_library(lv00_layer10_interop OBJECT ${LV00_LAYER10_SOURCES} ${LV00_LAYER10_HEADERS})
lv00_setup_layer(lv00_layer10_interop 10)
target_link_libraries(lv00_layer10_interop lv00_layer9_application)

# ── 聚合静态库（向后兼容 lv00_static）──
# 将所有层的 OBJECT 文件聚合到一个静态库中
add_library(lv00_static STATIC
    $<TARGET_OBJECTS:lv00_layer1_parser>
    $<TARGET_OBJECTS:lv00_layer2_resource>
    $<TARGET_OBJECTS:lv00_layer3_geometry>
    $<TARGET_OBJECTS:lv00_layer4_reasoning>
    $<TARGET_OBJECTS:lv00_layer5_output>
    $<TARGET_OBJECTS:lv00_layer6_visual>
    $<TARGET_OBJECTS:lv00_layer7_orchestration>
    $<TARGET_OBJECTS:lv00_layer8_meta_verify>
    $<TARGET_OBJECTS:lv00_layer9_application>
    $<TARGET_OBJECTS:lv00_layer10_interop>
)
target_include_directories(lv00_static PUBLIC
    ${GMP_INCLUDE_DIR}
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/core/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/core/include/lv00>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(lv00_static ${GMP_LIBRARIES} Threads::Threads)
set_target_properties(lv00_static PROPERTIES OUTPUT_NAME lv00)

# 跳过有深层依赖问题的预设注册（preset_differential_geometry, preset_functional_analysis）
# 此定义对静态库始终生效，不依赖于 BUILD_SHARED_LIBS 选项
target_compile_definitions(lv00_static PRIVATE LV00_EXCLUDE_BROKEN_PRESETS)

if(BUILD_SHARED_LIBS)
    add_library(lv00_shared SHARED
        $<TARGET_OBJECTS:lv00_layer1_parser>
        $<TARGET_OBJECTS:lv00_layer2_resource>
        $<TARGET_OBJECTS:lv00_layer3_geometry>
        $<TARGET_OBJECTS:lv00_layer4_reasoning>
        $<TARGET_OBJECTS:lv00_layer5_output>
        $<TARGET_OBJECTS:lv00_layer6_visual>
        $<TARGET_OBJECTS:lv00_layer7_orchestration>
        $<TARGET_OBJECTS:lv00_layer8_meta_verify>
        $<TARGET_OBJECTS:lv00_layer9_application>
        $<TARGET_OBJECTS:lv00_layer10_interop>
    )
    target_include_directories(lv00_shared PUBLIC
        ${GMP_INCLUDE_DIR}
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/core/include>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/core/include/lv00>
        $<INSTALL_INTERFACE:include>
    )
    # [P0 修复] 共享库也需要链接 Threads::Threads（与静态库保持一致）
    target_link_libraries(lv00_shared ${GMP_LIBRARIES} Threads::Threads)
    if(WIN32)
        target_link_libraries(lv00_shared ws2_32)
    endif()
    # v1.1.0: 定义 LV00_BUILD_SHARED 以启用 lv00.h 中的 dllexport/visibility
    target_compile_definitions(lv00_shared PRIVATE LV00_BUILD_SHARED)

    set_target_properties(lv00_shared PROPERTIES
        OUTPUT_NAME lv00
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
    if(WIN32)
        set_target_properties(lv00_shared PROPERTIES
            WINDOWS_EXPORT_ALL_SYMBOLS ON
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
            ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
        )
        # MSVC 运行时库处理：共享库应使用动态 CRT（/MD 或 /MDd）
        # 避免静态 CRT（/MT, /MTd）导致的跨模块内存管理问题
        if(MSVC)
            set_target_properties(lv00_shared PROPERTIES
                MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
            )
        endif()
    endif()
endif()

add_library(lv00 ALIAS lv00_static)

# ============================================================
# 辅助函数：添加测试可执行文件
# ============================================================
function(add_lv00_test name source)
    add_executable(${name} ${source})
    target_include_directories(${name} PRIVATE
        ${CMAKE_SOURCE_DIR}/test/c
    )
    target_link_libraries(${name} lv00 ${GMP_LIBRARIES})
endfunction()

function(add_lv00_test_and_register test_name source ctest_name)
    add_lv00_test(${test_name} ${source})
    add_test(NAME ${ctest_name} COMMAND ${test_name}
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endfunction()

# ============================================================
# 测试
# ============================================================
if(BUILD_TESTS)
    enable_testing()

    # 正式模块测试（注册到 CTest）
    add_lv00_test_and_register(test_basic
        test/c/test_basic.c basic_test)
    add_lv00_test_and_register(test_edge_cases
        test/c/test_edge_cases.c edge_cases_test)
    add_lv00_test_and_register(test_complex_graph
        test/c/test_complex_graph.c complex_graph_test)
    add_lv00_test_and_register(test_normalization
        test/c/test_normalization.c normalization_test)
    add_lv00_test_and_register(test_constraint_compatibility
        test/c/test_constraint_compatibility.c constraint_compatibility_test)
    add_lv00_test_and_register(test_unify
        test/c/test_unify.c unify_test)
    add_lv00_test_and_register(test_func_block
        test/c/test_func_block.c func_block_test)
    add_lv00_test_and_register(test_solver
        test/c/test_solver.c solver_test)
    add_lv00_test_and_register(test_solver_enhanced
        test/c/test_solver_enhanced.c solver_enhanced_test)
    add_lv00_test_and_register(test_rewrite
        test/c/test_rewrite.c rewrite_test)
    add_lv00_test_and_register(test_type_system
        test/c/test_type_system.c type_system_test)
    add_lv00_test_and_register(test_proof
        test/c/test_proof.c proof_test)
    add_lv00_test_and_register(test_axiom_pkg
        test/c/test_axiom_pkg.c axiom_pkg_test)
    add_lv00_test_and_register(test_module
        test/c/test_module.c module_test)
    add_lv00_test_and_register(test_graph_serialize
        test/c/test_graph_serialize.c graph_serialize_test)
    add_lv00_test_and_register(test_recursion
        test/c/test_recursion.c recursion_test)
    add_lv00_test_and_register(test_implementation
        test/c/test_implementation.c implementation_test)
    add_lv00_test_and_register(test_minimal_parse
        test/c/test_minimal_parse.c minimal_parse_test)
    add_lv00_test_and_register(test_axiom_euclidean_plane
        test/c/test_axiom_euclidean_plane.c euclidean_plane_axiom_test)
    add_lv00_test_and_register(test_axiom_boolean_algebra
        test/c/test_axiom_boolean_algebra.c boolean_algebra_axiom_test)
    add_lv00_test_and_register(test_axiom_classical_propositional_logic
        test/c/test_axiom_classical_propositional_logic.c classical_propositional_logic_axiom_test)
    add_lv00_test_and_register(test_axiom_group_theory
        test/c/test_axiom_group_theory.c group_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_zfc_set_theory
        test/c/test_axiom_zfc_set_theory.c zfc_set_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_ring_theory
        test/c/test_axiom_ring_theory.c ring_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_intuitionistic_logic
        test/c/test_axiom_intuitionistic_logic.c intuitionistic_logic_axiom_test)
    add_lv00_test_and_register(test_axiom_intuitionistic_propositional_logic
        test/c/test_axiom_intuitionistic_propositional_logic.c intuitionistic_propositional_logic_axiom_test)
    add_lv00_test_and_register(test_axiom_category_theory
        test/c/test_axiom_category_theory.c category_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_point_set_topology
        test/c/test_axiom_point_set_topology.c point_set_topology_axiom_test)
    add_lv00_test_and_register(test_axiom_hyperbolic_geometry
        test/c/test_axiom_hyperbolic_geometry.c hyperbolic_geometry_axiom_test)
    add_lv00_test_and_register(test_axiom_elliptic_geometry
        test/c/test_axiom_elliptic_geometry.c elliptic_geometry_axiom_test)
    add_lv00_test_and_register(test_axiom_simple_type_theory
        test/c/test_axiom_simple_type_theory.c simple_type_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_field_theory
        test/c/test_axiom_field_theory.c field_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_nbg_set_theory
        test/c/test_axiom_nbg_set_theory.c nbg_set_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_real_analysis
        test/c/test_axiom_real_analysis.c real_analysis_axiom_test)
    add_lv00_test_and_register(test_axiom_combinatorics
        test/c/test_axiom_combinatorics.c combinatorics_axiom_test)
    add_lv00_test_and_register(test_axiom_algebraic_topology
        test/c/test_axiom_algebraic_topology.c algebraic_topology_axiom_test)
    add_lv00_test_and_register(test_axiom_model_theory
        test/c/test_axiom_model_theory.c model_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_dependent_type_theory
        test/c/test_axiom_dependent_type_theory.c dependent_type_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_homological_algebra
        test/c/test_axiom_homological_algebra.c homological_algebra_axiom_test)
    add_lv00_test_and_register(test_axiom_universal_algebra
        test/c/test_axiom_universal_algebra.c universal_algebra_axiom_test)
    add_lv00_test_and_register(test_axiom_differential_geometry
        test/c/test_axiom_differential_geometry.c differential_geometry_axiom_test)
    add_lv00_test_and_register(test_axiom_number_theory
        test/c/test_axiom_number_theory.c number_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_functional_analysis
        test/c/test_axiom_functional_analysis.c functional_analysis_axiom_test)
    add_lv00_test_and_register(test_axiom_algebraic_geometry
        test/c/test_axiom_algebraic_geometry.c algebraic_geometry_axiom_test)
    add_lv00_test_and_register(test_axiom_proof_theory
        test/c/test_axiom_proof_theory.c proof_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_homotopy_type_theory
        test/c/test_axiom_homotopy_type_theory.c homotopy_type_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_affine_geometry
        test/c/test_axiom_affine_geometry.c affine_geometry_axiom_test)
    add_lv00_test_and_register(test_axiom_projective_geometry
        test/c/test_axiom_projective_geometry.c projective_geometry_axiom_test)
    add_lv00_test_and_register(test_axiom_lattice_theory
        test/c/test_axiom_lattice_theory.c lattice_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_lie_theory
        test/c/test_axiom_lie_theory.c lie_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_modal_logic
        test/c/test_axiom_modal_logic.c modal_logic_axiom_test)
    add_lv00_test_and_register(test_axiom_linear_logic
        test/c/test_axiom_linear_logic.c linear_logic_axiom_test)
    add_lv00_test_and_register(test_axiom_metric_space
        test/c/test_axiom_metric_space.c metric_space_axiom_test)
    add_lv00_test_and_register(test_axiom_game_theory
        test/c/test_axiom_game_theory.c game_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_information_theory
        test/c/test_axiom_information_theory.c information_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_descriptive_set_theory
        test/c/test_axiom_descriptive_set_theory.c descriptive_set_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_domain_theory
        test/c/test_axiom_domain_theory.c domain_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_computability_theory
        test/c/test_axiom_computability_theory.c computability_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_computational_complexity_theory
        test/c/test_axiom_computational_complexity_theory.c computational_complexity_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_presburger_arithmetic
        test/c/test_axiom_presburger_arithmetic.c presburger_arithmetic_axiom_test)
    add_lv00_test_and_register(test_utils
        test/c/test_utils.c utils_test)
    add_lv00_test_and_register(test_stream
        test/c/test_stream.c stream_test)
    add_lv00_test_and_register(test_stream_extended
        test/c/test_stream_extended.c stream_extended_test)
    add_lv00_test_and_register(test_func_block_utils
        test/c/test_func_block_utils.c func_block_utils_test)
    # 恢复：preset_blocks.c 已加入 Layer 4 构建列表，依赖已满足
    add_lv00_test_and_register(test_func_block_preset
        test/c/test_func_block_preset.c func_block_preset_test)
    add_lv00_test_and_register(test_error_codes
        test/c/test_error_codes.c error_codes_test)
    # 恢复：interop.c 已在 Layer 5 构建列表中，依赖已满足
    add_lv00_test_and_register(test_interop
        test/c/test_interop.c interop_test)
    if(WIN32)
        target_link_libraries(test_interop ws2_32)
    endif()
    # 新增：高维结构模块测试
    add_lv00_test_and_register(test_high_dim
        test/c/test_high_dim.c high_dim_test)
    # 新增：符号坐标系统测试
    add_lv00_test_and_register(test_symbolic_coord
        test/c/test_symbolic_coord.c symbolic_coord_test)
    # 新增：v1.1.0 新模块集成测试
    add_lv00_test_and_register(test_rational
        test/c/test_rational.c rational_test)
    add_lv00_test_and_register(test_expr_canon
        test/c/test_expr_canon.c expr_canon_test)
    add_lv00_test_and_register(test_proof_trace
        test/c/test_proof_trace.c proof_trace_test)
    # v1.1.0 新模块集成测试
    add_lv00_test_and_register(test_new_modules
        test/c/test_new_modules.c new_modules_test)
    add_lv00_test_and_register(test_circuit_breaker
        test/c/test_circuit_breaker.c circuit_breaker_test)
    # v3.4.0 新增：几何代数模块测试
    add_lv00_test_and_register(test_ga_multivector
        test/c/test_ga_multivector.c test_ga_multivector)
    add_lv00_test_and_register(test_geo_invariant
        test/c/test_geo_invariant.c test_geo_invariant)
    # v3.4.0 新增：区间/数值验证模块测试
    add_lv00_test_and_register(test_interval_arithmetic
        test/c/test_interval_arithmetic.c test_interval_arithmetic)
    add_lv00_test_and_register(test_gappa_dsl
        test/c/test_gappa_dsl.c test_gappa_dsl)
    # v3.4.0 新增：SMT/SAT 模块测试
    add_lv00_test_and_register(test_smt_bitvector
        test/c/test_smt_bitvector.c test_smt_bitvector)
    # v3.4.0 新增：证明策略模块测试
    add_lv00_test_and_register(test_proof_rule_engine
        test/c/test_proof_rule_engine.c test_proof_rule_engine)
    # v3.4.0 新增：数论/代数模块测试
    add_lv00_test_and_register(test_nt_number_theory
        test/c/test_nt_number_theory.c test_nt_number_theory)
    add_lv00_test_and_register(test_sym_expr
        test/c/test_sym_expr.c test_sym_expr)
    # v3.4.0 新增：重写引擎测试
    add_lv00_test_and_register(test_rewrite_strategy
        test/c/test_rewrite_strategy.c test_rewrite_strategy)
    # v3.4.0 新增：版本控制测试
    add_lv00_test_and_register(test_proof_version
        test/c/test_proof_version.c test_proof_version)
    # v3.4.0 新增：自动微分测试
    add_lv00_test_and_register(test_autodiff
        test/c/test_autodiff.c test_autodiff)
    # prop_verifier 测试：链接 lv00 库以获取 lv00_malloc/lv00_free 等工具函数
    # [QA] Also listed in LV00_LAYER4_SOURCES; test target requires separate compilation
    add_executable(test_prop_verifier test/c/test_prop_verifier.c core/src/layer4_reasoning/prop_verifier.c)
    target_include_directories(test_prop_verifier PRIVATE core/include core/include/lv00)
    target_link_libraries(test_prop_verifier lv00 ${GMP_LIBRARIES})
    add_test(NAME prop_verifier_test COMMAND test_prop_verifier
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    # v3.6.0 新增：WFC 范式三模块集成测试
    add_lv00_test_and_register(test_wfc_modules
        test/c/test_wfc_modules.c wfc_modules_test)

    # 基准测试（构建但不注册到 CTest，手动运行）
    add_lv00_test(test_benchmark test/c/test_benchmark.c)
endif()

# ============================================================
# 模糊测试
# ============================================================
if(BUILD_FUZZERS AND CMAKE_C_COMPILER_ID MATCHES "Clang")
    if(WIN32)
        message(WARNING "BUILD_FUZZERS is enabled, but libFuzzer is not supported by this Windows Clang toolchain; skipping fuzz targets.")
    else()
        add_executable(fuzz_constraint_graph test/fuzz/fuzz_constraint_graph.c)
        target_link_libraries(fuzz_constraint_graph lv00 ${GMP_LIBRARIES})
        target_compile_options(fuzz_constraint_graph PRIVATE -fsanitize=fuzzer,address)
        target_link_options(fuzz_constraint_graph PRIVATE -fsanitize=fuzzer,address)

        add_executable(fuzz_symbolic_coord test/fuzz/fuzz_symbolic_coord.c)
        target_link_libraries(fuzz_symbolic_coord lv00 ${GMP_LIBRARIES})
        target_compile_options(fuzz_symbolic_coord PRIVATE -fsanitize=fuzzer,address)
        target_link_options(fuzz_symbolic_coord PRIVATE -fsanitize=fuzzer,address)
    endif()
endif()

# ============================================================
# 示例程序
# ============================================================
if(BUILD_EXAMPLES)
    function(add_lv00_example name source)
        add_executable(${name} ${source})
        target_link_libraries(${name} lv00 ${GMP_LIBRARIES})
    endfunction()

    add_lv00_example(example_triangle    test/examples/triangle_construction.c)
    add_lv00_example(example_circle      test/examples/circle_intersection.c)
    add_lv00_example(example_composition test/examples/function_composition.c)
    add_lv00_example(example_high_dim    test/examples/high_dim_demo.c)
    add_lv00_example(example_streaming      test/examples/streaming_demo.c)
    add_lv00_example(example_stream_advanced test/examples/stream_advanced_demo.c)
endif()

# 修复 1：安装规则 —— 支持 cmake --install 将库文件和头文件安装到系统目录
include(GNUInstallDirs)

install(TARGETS lv00_static
    EXPORT lv00-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY core/include/lv00
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
    PATTERN ".gitkeep" EXCLUDE
)

if(BUILD_SHARED_LIBS)
    install(TARGETS lv00_shared
        EXPORT lv00-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

# [P0 修复] 导出 CMake 目标文件，供 find_package(lv00) 使用
install(EXPORT lv00-targets
    FILE lv00-targets.cmake
    NAMESPACE lv00::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/build/build/cmake/lv00
)

# ============================================================
# v1.1.0: pkg-config .pc 文件生成
#
# 安装后，使用者可以通过以下命令获取编译和链接标志:
#   pkg-config --cflags lv00
#   pkg-config --libs lv00
# ============================================================

# 准备 pkg-config 变量
set(LV00_PC_REQUIRES "gmp")            # 依赖的 pkg-config 包

# 计算 libdir（相对于 prefix）
# 若 CMAKE_INSTALL_LIBDIR 是绝对路径则使用，否则添加 ${prefix}/
if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
    set(LV00_PC_LIBDIR "${CMAKE_INSTALL_LIBDIR}")
else()
    set(LV00_PC_LIBDIR "\${prefix}/${CMAKE_INSTALL_LIBDIR}")
endif()

if(IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
    set(LV00_PC_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}")
else()
    set(LV00_PC_INCLUDEDIR "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
endif()

# 生成 lv00.pc
configure_file(
    ${CMAKE_SOURCE_DIR}/build/cmake/lv00.pc.in
    ${CMAKE_BINARY_DIR}/lv00.pc
    @ONLY
)

install(FILES ${CMAKE_BINARY_DIR}/lv00.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# ============================================================
# v1.1.0: CMake 包配置文件生成
#
# 安装后，其他 CMake 项目可以通过以下方式引用:
#   find_package(lv00 1.1.0 REQUIRED)
#   target_link_libraries(myapp lv00::lv00)
# ============================================================

include(CMakePackageConfigHelpers)

# 包安装路径
set(LV00_CMAKE_CONFIG_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/lv00")

# 生成 lv00-config.cmake 和 lv00-config-version.cmake
configure_package_config_file(
    ${CMAKE_SOURCE_DIR}/build/cmake/lv00-config.cmake.in
    ${CMAKE_BINARY_DIR}/lv00-config.cmake
    INSTALL_DESTINATION ${LV00_CMAKE_CONFIG_DIR}
)

write_basic_package_version_file(
    ${CMAKE_BINARY_DIR}/lv00-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# 安装 CMake 包配置文件
install(FILES
    ${CMAKE_BINARY_DIR}/lv00-config.cmake
    ${CMAKE_BINARY_DIR}/lv00-config-version.cmake
    DESTINATION ${LV00_CMAKE_CONFIG_DIR}
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

set(CPACK_PACKAGE_NAME            "lv00")
set(CPACK_PACKAGE_VENDOR          "Lv-00 Project")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Lv-00: A geometric metalanguage for dual-mode mathematics")
set(CPACK_PACKAGE_VERSION_MAJOR   ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR   ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH   ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_FILE_NAME       "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT         "Lv-00 Project <lv00@example.com>")
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
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE    "https://github.com/example/lv00")

# RPM 包特定设置
set(CPACK_RPM_PACKAGE_LICENSE        "MIT")
set(CPACK_RPM_PACKAGE_GROUP          "Development/Libraries")
set(CPACK_RPM_PACKAGE_REQUIRES       "gmp >= 6.0.0")
set(CPACK_RPM_PACKAGE_URL            "https://github.com/example/lv00")

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
