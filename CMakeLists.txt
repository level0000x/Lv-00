cmake_minimum_required(VERSION 3.15)
project(lv00 VERSION 3.3.0 LANGUAGES C)  # 版本号与 lv00.h 中 LV00_VERSION_MAJOR/MINOR/PATCH 保持一致

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
# v3.3.0: 层级验证开关（启用后在编译时检查跨层依赖违规）
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
# v3.3.0: 升级警告级别到 /W4（最高级别），确保代码质量
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
# v3.3.0: 添加额外的 GCC 警告和安全标志
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
# v3.3.0: 与 GCC 保持一致的警告级别，添加 Clang 特有的有用警告
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
    set(BUILD_TESTS OFF)
    message(STATUS "WASM: 禁用常规测试（使用 web-gui 测试）")
    
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
# 详见 docs/ARCHITECTURE_v3.3.md
# ============================================================

# ── 共享头文件列表（所有层共用）──
set(LV00_HEADERS
    include/lv00/lv00.h
    include/lv00/cross_platform.h
    include/lv00/error_codes.h
    include/lv00/symbolic_coord.h
    include/lv00/constraint_graph.h
    include/lv00/normalization.h
    include/lv00/graph_hash.h
    include/lv00/solver.h
    include/lv00/rewrite.h
    include/lv00/unify.h
    include/lv00/axiom_pkg.h
    include/lv00/module.h
    include/lv00/engine.h
    include/lv00/debug.h
    include/lv00/mpz_poly.h
    include/lv00/func_block.h
    include/lv00/type_system.h
    include/lv00/proof.h
    include/lv00/recursion.h
    include/lv00/lv00_utils.h
    include/lv00/high_dim.h
    include/lv00/interop.h
    include/lv00/formula_parser.h
    include/lv00/formula_renderer.h
    include/lv00/formula_converter.h
    include/lv00/prop_verifier.h
    include/lv00/lexer_shared.h
    include/lv00/stream.h
    include/lv00/func_block_utils.h
    include/lv00/stream_context_util.h
    include/lv00/func_block_registry.h
    include/lv00/func_block_preset_ops.h
    # v3.3.0 新增模块头文件（第一轮：核心计算模块）
    include/lv00/groebner_engine.h
    include/lv00/engine_scheduler.h
    include/lv00/euclidean_geometry.h
    include/lv00/geometry_compress.h
    include/lv00/sparse_linear_algebra.h
    include/lv00/float_error.h
    include/lv00/smt_backend.h
    include/lv00/bdd_encoding.h
    include/lv00/approx_counter.h
    # v3.3.0 新增模块头文件（第二轮：计算后端与数值模块）
    include/lv00/atp_backend.h
    include/lv00/solver_core.h
    include/lv00/sat_encoding.h
    include/lv00/relation_model.h
    include/lv00/numerical_backend.h
    include/lv00/geom_evol.h
    include/lv00/geo_event_detect.h
    include/lv00/lv00_numeric.h
    include/lv00/probabilistic_constraint.h
    # v3.3.0 新增模块头文件（第三轮：几何构造与交互模块）
    include/lv00/algebra_mode.h
    include/lv00/dsl_compiler.h
    include/lv00/interactive_geo.h
    include/lv00/tikz_export.h
    include/lv00/math_input.h
    include/lv00/math_protocol.h
    include/lv00/ecosystem.h
    # v3.3.0 新增模块头文件（第四轮：证明与规约模块）
    include/lv00/mini_kernel.h
    include/lv00/gc_language.h
    include/lv00/geo_spec.h
    include/lv00/proof_widget.h
    include/lv00/path_type.h
    # 模块化预设函数块系统
    include/lv00/preset_blocks.h
    include/lv00/preset_register_macros.h
    include/lv00/preset_basic_geometry.h
    include/lv00/preset_transformations.h
    include/lv00/preset_measurements.h
    include/lv00/preset_polygons.h
    include/lv00/preset_algebraic.h
    include/lv00/preset_number_theory.h
    include/lv00/preset_group_theory.h
    include/lv00/preset_topology.h
    include/lv00/preset_analysis.h
    include/lv00/preset_combinatorics.h
    include/lv00/preset_graph_theory.h
    include/lv00/preset_probability.h
    include/lv00/preset_numerical.h
    include/lv00/preset_optimization.h
    include/lv00/preset_advanced_geometry.h
    include/lv00/preset_geometry_3d.h
    include/lv00/preset_field_theory.h
    include/lv00/preset_ring_theory.h
    include/lv00/preset_linear_algebra.h
    include/lv00/preset_trigonometry.h
    include/lv00/preset_differential_equations.h
    include/lv00/preset_measure_theory.h
    include/lv00/preset_functional_analysis.h
    include/lv00/preset_set_theory.h
    include/lv00/preset_math_logic.h
    include/lv00/preset_category_theory.h
    include/lv00/preset_category_theory_adv.h
    include/lv00/preset_complex_analysis.h
    include/lv00/preset_differential_geometry.h
    include/lv00/preset_differential_geometry_adv.h
    include/lv00/preset_numerical_analysis.h
    include/lv00/preset_probability_statistics.h
    include/lv00/preset_statistics.h
    # v11.0 新增：理论数学研究模块
    include/lv00/preset_algebraic_geometry.h
    include/lv00/preset_homological_algebra.h
    include/lv00/preset_lie_theory_advanced.h
    # v12.0 新增：随机过程、博弈论、信息论、编码理论、差分方程模块
    include/lv00/preset_stochastic_processes.h
    include/lv00/preset_game_theory.h
    include/lv00/preset_information_theory.h
    include/lv00/preset_coding_theory.h
    include/lv00/preset_difference_equations.h
    # v3.3.0 性能优化模块
    include/lv00/thread_pool.h
    include/lv00/simd_ops.h
    include/lv00/fast_index.h
    include/lv00/benchmark.h
    # v3.3.0 新增核心模块（整改实施）
    include/lv00/memory_pool.h
    include/lv00/expr_canonical.h
    include/lv00/geometry_transform.h
    include/lv00/inequality_reasoning.h
    include/lv00/runtime_monitor.h
    include/lv00/axiom_rule_engine.h
    include/lv00/proof_engine_enhanced.h
    include/lv00/test_framework.h
    # Magic 模拟器模块
    include/lv00/magic.h
    # 内部/实现头文件（用于 IDE 索引和 OBJECT 库关联）
    include/lv00/lv00_internal.h
    include/lv00/config.h
    include/lv00/context.h
    include/lv00/geometry_types.h
    include/lv00/node_deep_copy.h
    include/lv00/parser_safety.h
    include/lv00/func_block_internal.h
    include/lv00/func_block_preset.h
    include/lv00/expr_canon.h
    include/lv00/modal_operators.h
    include/lv00/logic_check.h
    include/lv00/circuit_breaker.h
    include/lv00/axiom_grade.h
    include/lv00/three_valued_logic.h
    include/lv00/quantifier.h
    include/lv00/rational.h
    include/lv00/proof_priority.h
    include/lv00/proof_trace.h
    include/lv00/proof_score.h
    include/lv00/runtime_guard.h
    include/lv00/status_codes.h
    include/lv00/exact_arithmetic.h
    # func_block 内部预设头文件
    include/lv00/preset_basic_math.h
    include/lv00/preset_common.h
    include/lv00/preset_calculus.h
    include/lv00/preset_core.h
    include/lv00/preset_blocks.h
    # 预设模块头文件（内部实现）
    include/lv00/preset_algebraic_topology.h
    include/lv00/preset_algebraic_topology_adv.h
    include/lv00/preset_arithmetic_geometry.h
    include/lv00/preset_dynamical_systems.h
    include/lv00/preset_functional_analysis_adv.h
    include/lv00/preset_integral_transforms.h
    include/lv00/preset_lattice_theory.h
    include/lv00/preset_logic_advanced.h
    include/lv00/preset_mathematical_logic.h
    include/lv00/preset_mathematical_physics.h
    include/lv00/preset_matrix.h
    include/lv00/preset_order_theory.h
    include/lv00/preset_polynomial.h
    include/lv00/preset_representation_theory.h
    include/lv00/preset_special_functions.h
)

# ── Layer 1: 输入解析层 (Parser) ──
# 职责：词法分析、公式解析、DSL 编译
# 依赖：Layer 2
set(LV00_LAYER1_SOURCES
    src/parser/formula_parser.c
    src/parser/formula_converter.c
    src/parser/formula_renderer.c
    src/parser/lexer_shared.c
    src/parser/parser_safety.c
    src/core/dsl_compiler.c
    src/core/math_input.c
)

# ── Layer 2: 资源管理层 (Resource) ──
# 职责：内存分配、错误码、调试、深拷贝、工具函数
# 依赖：GMP，C 标准库（仅 Layer 2 能直接依赖 GMP 链接）
set(LV00_LAYER2_SOURCES
    src/utils/lv00_utils.c
    src/core/lv00.c
    src/core/context.c
    src/core/error_codes.c
    src/core/debug.c
    src/core/node_deep_copy.c
    # v3.3.0 新增：内存池、运行时监控、测试框架
    src/core/memory_pool.c
    src/core/runtime_monitor.c
    src/core/test_framework.c
)

# ── Layer 3: 几何拓扑层 (Geometry) ──
# 职责：约束图、符号坐标、几何原语、高维结构
# 依赖：Layer 2，GMP（通过 Layer 2 传递）
set(LV00_LAYER3_SOURCES
    src/core/constraint_graph.c
    src/core/symbolic_coord.c
    src/core/euclidean_geometry.c
    src/core/interactive_geo.c
    src/core/high_dim.c
    src/core/geometry_compress.c
    src/core/geo_event_detect.c
    src/core/geo_spec.c
    src/core/geometry_csg.c
    src/core/sparse_linear_algebra.c
    src/core/geom_evol.c
    src/core/float_error.c
    src/core/mpz_poly_resultant.c
    # v3.3.0 新增：几何变换推理
    src/core/geometry_transform.c
    # v3.4.0 新增：几何代数模块（GATr + GAALOP + GeoLogic 落地）
    src/core/ga_multivector.c
    src/core/ga_interface.c
    src/core/ga_codegen.c
    src/core/geo_invariant_type.c
    # v3.4.0 新增：区间/数值验证模块（MPFI + Gappa + FPTaylor + Herbie + FLINT/Arb 落地）
    src/core/interval_arithmetic.c
    src/core/gappa_dsl.c
    src/core/gappa_propagate.c
    src/core/fptaylor_eval.c
    src/core/herbie_eval.c
    # v3.4.0 新增：SMT/SAT 模块（Alt-Ergo + STP + Boolector + CaDiCaL + Yices2 落地）
    src/core/smt_theory_combiner.c
    src/core/smt_bitvector.c
    src/core/smt_trigger_engine.c
    # v3.4.0 新增：证明策略模块（Aesop + Seed-Prover + MiniF2F 落地）
    src/core/proof_rule_engine.c
    src/core/proof_session.c
    # v3.4.0 新增：数论/代数模块（NTL + SymEngine + GiNaC 落地）
    src/core/nt_number_theory.c
    src/core/nt_polynomial.c
    src/core/sym_expr.c
    # v3.4.0 新增：重写引擎模块（egg + Maude + K Framework 落地）
    src/core/rewrite_strategy.c
    # v3.4.0 新增：版本控制模块（libgit2 落地）
    src/core/proof_version.c
    # v3.4.0 新增：自动微分模块（Enzyme 落地）
    src/core/autodiff.c
    # v3.3.0 性能优化模块
    src/core/thread_pool.c
    src/core/simd_ops.c
)

# ── Layer 4: 公理推理层 (Reasoning) ──
# 职责：引擎、求解器、证明、重写、合一、规范化、类型系统、公理包
# 依赖：Layer 3，Layer 2
set(LV00_LAYER4_SOURCES
    # 核心引擎与求解
    src/core/engine.c
    src/core/engine_scheduler.c
    src/core/solver.c
    src/core/solver_core.c
    # 证明系统
    src/core/proof.c
    src/core/proof_optimize.c
    src/core/proof_multi_strategy.c
    src/core/proof_trace.c
    src/core/proof_priority.c
    src/core/logic_check.c
    src/core/circuit_breaker.c
    # 三值逻辑、模态算子、量词系统
    src/core/three_valued_logic.c
    src/core/modal_operators.c
    src/core/quantifier.c
    # 增强证明引擎
    src/core/proof_engine_enhanced.c
    # 重写与合一
    src/core/rewrite.c
    src/core/unify.c
    src/core/normalization.c
    src/core/recursion.c
    # 类型与属性
    src/core/type_system.c
    src/core/prop_verifier.c
    # 后端求解引擎
    src/core/groebner_engine.c
    src/core/smt_backend_impl.c
    src/core/atp_backend.c
    src/core/sat_encoding.c
    src/core/bdd_encoding.c
    src/core/approx_counter.c
    src/core/probabilistic_constraint.c
    src/core/rational.c
    src/core/expr_canon.c
    src/core/exact_arithmetic.c
    # v3.3.0 新增：表达式规范化、不等式推理
    src/core/expr_canonical.c
    # 模型与模式
    src/core/relation_model.c
    src/core/algebra_mode.c
    src/core/numerical_backend.c
    # 内核与协议
    src/core/module.c
    src/core/mini_kernel.c
    src/core/gc_language.c
    src/core/ecosystem.c
    src/core/math_protocol.c
    src/core/stream.c
    src/core/stream_context_util.c
    # 公理包
    src/axiom/axiom_pkg.c
    src/axiom/axiom_grade.c
    # func_block 函数块系统
    src/func_block/func_block.c
    src/func_block/func_block_compose.c
    src/func_block/func_block_determinism.c
    src/func_block/func_block_instantiate.c
    src/func_block/func_block_preset.c
    src/func_block/func_block_preset_ops.c
    src/func_block/func_block_registry.c
    src/func_block/func_block_selector.c
    src/func_block/func_block_serialize.c
    src/func_block/func_block_utils.c
    src/func_block/preset_basic_math.c
    src/func_block/preset_blocks.c
    src/func_block/preset_calculus.c
    src/func_block/preset_common.c
    src/func_block/preset_manager.c
    # 数学理论预设 (55 个文件)
    src/preset/preset_advanced_geometry.c
    src/preset/preset_algebraic.c
    src/preset/preset_algebraic_geometry.c
    src/preset/preset_algebraic_topology.c
    src/preset/preset_algebraic_topology_adv.c
    src/preset/preset_analysis.c
    src/preset/preset_arithmetic_geometry.c
    src/preset/preset_basic_geometry.c
    src/preset/preset_category_theory.c
    src/preset/preset_category_theory_adv.c
    src/preset/preset_coding_theory.c
    src/preset/preset_combinatorics.c
    src/preset/preset_complex_analysis.c
    src/preset/preset_difference_equations.c
    src/preset/preset_differential_equations.c
    src/preset/preset_differential_geometry.c
    src/preset/preset_differential_geometry_adv.c
    src/preset/preset_dynamical_systems.c
    src/preset/preset_field_theory.c
    src/preset/preset_functional_analysis.c
    src/preset/preset_functional_analysis_adv.c
    src/preset/preset_game_theory.c
    src/preset/preset_geometry_3d.c
    src/preset/preset_graph_theory.c
    src/preset/preset_group_theory.c
    src/preset/preset_homological_algebra.c
    src/preset/preset_information_theory.c
    src/preset/preset_integral_transforms.c
    src/preset/preset_lattice_theory.c
    src/preset/preset_lie_theory_advanced.c
    src/preset/preset_linear_algebra.c
    src/preset/preset_logic_advanced.c
    src/preset/preset_math_logic.c
    src/preset/preset_mathematical_logic.c
    src/preset/preset_mathematical_physics.c
    src/preset/preset_matrix.c
    src/preset/preset_measure_theory.c
    src/preset/preset_measurements.c
    src/preset/preset_number_theory.c
    src/preset/preset_numerical.c
    src/preset/preset_numerical_analysis.c
    src/preset/preset_optimization.c
    src/preset/preset_order_theory.c
    src/preset/preset_polygons.c
    src/preset/preset_polynomial.c
    src/preset/preset_probability.c
    src/preset/preset_probability_statistics.c
    src/preset/preset_representation_theory.c
    src/preset/preset_ring_theory.c
    src/preset/preset_set_theory.c
    src/preset/preset_special_functions.c
    src/preset/preset_statistics.c
    src/preset/preset_stochastic_processes.c
    src/preset/preset_topology.c
    src/preset/preset_transformations.c
    src/preset/preset_trigonometry.c
)

# ── Layer 5: 结果输出层 (Output) ──
# 职责：流式输出、TikZ 导出、证明可视化、模块、互操作
# 依赖：Layer 4，Layer 3，Layer 2
set(LV00_LAYER5_SOURCES
    src/core/proof_widget.c
    src/core/tikz_export.c
    src/interop/interop.c
    # Magic 模拟器模块
    src/magic/magic.c
)

# ============================================================
# 库目标 —— 五层 OBJECT 库 + 聚合静态库
# ============================================================

# ── 辅助宏：为每个层设置公共编译属性 ──
macro(lv00_setup_layer target layer_id)
    target_include_directories(${target} PUBLIC
        ${GMP_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/include/lv00
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
# v3.3.0: 线程库链接（用于 thread_pool.c）
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

# ── 聚合静态库（向后兼容 lv00_static）──
# 将所有层的 OBJECT 文件聚合到一个静态库中
add_library(lv00_static STATIC
    $<TARGET_OBJECTS:lv00_layer1_parser>
    $<TARGET_OBJECTS:lv00_layer2_resource>
    $<TARGET_OBJECTS:lv00_layer3_geometry>
    $<TARGET_OBJECTS:lv00_layer4_reasoning>
    $<TARGET_OBJECTS:lv00_layer5_output>
)
target_include_directories(lv00_static PUBLIC
    ${GMP_INCLUDE_DIR}
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include/lv00>
    $<INSTALL_INTERFACE:include/lv00>
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
    )
    target_include_directories(lv00_shared PUBLIC
        ${GMP_INCLUDE_DIR}
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include/lv00>
        $<INSTALL_INTERFACE:include/lv00>
    )
    # [P0 修复] 共享库也需要链接 Threads::Threads（与静态库保持一致）
    target_link_libraries(lv00_shared ${GMP_LIBRARIES} Threads::Threads)
    if(WIN32)
        target_link_libraries(lv00_shared ws2_32)
    endif()
    # v3.3.0: 定义 LV00_BUILD_SHARED 以启用 lv00.h 中的 dllexport/visibility
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
        tests/test_basic.c basic_test)
    add_lv00_test_and_register(test_edge_cases
        tests/test_edge_cases.c edge_cases_test)
    add_lv00_test_and_register(test_complex_graph
        tests/test_complex_graph.c complex_graph_test)
    add_lv00_test_and_register(test_normalization
        tests/test_normalization.c normalization_test)
    add_lv00_test_and_register(test_unify
        tests/test_unify.c unify_test)
    add_lv00_test_and_register(test_func_block
        tests/test_func_block.c func_block_test)
    add_lv00_test_and_register(test_solver
        tests/test_solver.c solver_test)
    add_lv00_test_and_register(test_solver_enhanced
        tests/test_solver_enhanced.c solver_enhanced_test)
    add_lv00_test_and_register(test_rewrite
        tests/test_rewrite.c rewrite_test)
    add_lv00_test_and_register(test_type_system
        tests/test_type_system.c type_system_test)
    add_lv00_test_and_register(test_proof
        tests/test_proof.c proof_test)
    add_lv00_test_and_register(test_axiom_pkg
        tests/test_axiom_pkg.c axiom_pkg_test)
    add_lv00_test_and_register(test_module
        tests/test_module.c module_test)
    add_lv00_test_and_register(test_graph_serialize
        tests/test_graph_serialize.c graph_serialize_test)
    add_lv00_test_and_register(test_recursion
        tests/test_recursion.c recursion_test)
    add_lv00_test_and_register(test_implementation
        tests/test_implementation.c implementation_test)
    add_lv00_test_and_register(test_minimal_parse
        tests/test_minimal_parse.c minimal_parse_test)
    add_lv00_test_and_register(test_axiom_euclidean_plane
        tests/test_axiom_euclidean_plane.c euclidean_plane_axiom_test)
    add_lv00_test_and_register(test_axiom_boolean_algebra
        tests/test_axiom_boolean_algebra.c boolean_algebra_axiom_test)
    add_lv00_test_and_register(test_axiom_classical_propositional_logic
        tests/test_axiom_classical_propositional_logic.c classical_propositional_logic_axiom_test)
    add_lv00_test_and_register(test_axiom_group_theory
        tests/test_axiom_group_theory.c group_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_zfc_set_theory
        tests/test_axiom_zfc_set_theory.c zfc_set_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_ring_theory
        tests/test_axiom_ring_theory.c ring_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_intuitionistic_logic
        tests/test_axiom_intuitionistic_logic.c intuitionistic_logic_axiom_test)
    add_lv00_test_and_register(test_axiom_intuitionistic_propositional_logic
        tests/test_axiom_intuitionistic_propositional_logic.c intuitionistic_propositional_logic_axiom_test)
    add_lv00_test_and_register(test_axiom_category_theory
        tests/test_axiom_category_theory.c category_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_point_set_topology
        tests/test_axiom_point_set_topology.c point_set_topology_axiom_test)
    add_lv00_test_and_register(test_axiom_hyperbolic_geometry
        tests/test_axiom_hyperbolic_geometry.c hyperbolic_geometry_axiom_test)
    add_lv00_test_and_register(test_axiom_elliptic_geometry
        tests/test_axiom_elliptic_geometry.c elliptic_geometry_axiom_test)
    add_lv00_test_and_register(test_axiom_simple_type_theory
        tests/test_axiom_simple_type_theory.c simple_type_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_field_theory
        tests/test_axiom_field_theory.c field_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_nbg_set_theory
        tests/test_axiom_nbg_set_theory.c nbg_set_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_real_analysis
        tests/test_axiom_real_analysis.c real_analysis_axiom_test)
    add_lv00_test_and_register(test_axiom_combinatorics
        tests/test_axiom_combinatorics.c combinatorics_axiom_test)
    add_lv00_test_and_register(test_axiom_algebraic_topology
        tests/test_axiom_algebraic_topology.c algebraic_topology_axiom_test)
    add_lv00_test_and_register(test_axiom_model_theory
        tests/test_axiom_model_theory.c model_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_dependent_type_theory
        tests/test_axiom_dependent_type_theory.c dependent_type_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_homological_algebra
        tests/test_axiom_homological_algebra.c homological_algebra_axiom_test)
    add_lv00_test_and_register(test_axiom_universal_algebra
        tests/test_axiom_universal_algebra.c universal_algebra_axiom_test)
    add_lv00_test_and_register(test_axiom_differential_geometry
        tests/test_axiom_differential_geometry.c differential_geometry_axiom_test)
    add_lv00_test_and_register(test_axiom_number_theory
        tests/test_axiom_number_theory.c number_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_functional_analysis
        tests/test_axiom_functional_analysis.c functional_analysis_axiom_test)
    add_lv00_test_and_register(test_axiom_algebraic_geometry
        tests/test_axiom_algebraic_geometry.c algebraic_geometry_axiom_test)
    add_lv00_test_and_register(test_axiom_proof_theory
        tests/test_axiom_proof_theory.c proof_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_homotopy_type_theory
        tests/test_axiom_homotopy_type_theory.c homotopy_type_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_affine_geometry
        tests/test_axiom_affine_geometry.c affine_geometry_axiom_test)
    add_lv00_test_and_register(test_axiom_projective_geometry
        tests/test_axiom_projective_geometry.c projective_geometry_axiom_test)
    add_lv00_test_and_register(test_axiom_lattice_theory
        tests/test_axiom_lattice_theory.c lattice_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_lie_theory
        tests/test_axiom_lie_theory.c lie_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_modal_logic
        tests/test_axiom_modal_logic.c modal_logic_axiom_test)
    add_lv00_test_and_register(test_axiom_linear_logic
        tests/test_axiom_linear_logic.c linear_logic_axiom_test)
    add_lv00_test_and_register(test_axiom_metric_space
        tests/test_axiom_metric_space.c metric_space_axiom_test)
    add_lv00_test_and_register(test_axiom_game_theory
        tests/test_axiom_game_theory.c game_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_information_theory
        tests/test_axiom_information_theory.c information_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_descriptive_set_theory
        tests/test_axiom_descriptive_set_theory.c descriptive_set_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_domain_theory
        tests/test_axiom_domain_theory.c domain_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_computability_theory
        tests/test_axiom_computability_theory.c computability_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_computational_complexity_theory
        tests/test_axiom_computational_complexity_theory.c computational_complexity_theory_axiom_test)
    add_lv00_test_and_register(test_axiom_presburger_arithmetic
        tests/test_axiom_presburger_arithmetic.c presburger_arithmetic_axiom_test)
    add_lv00_test_and_register(test_utils
        tests/test_utils.c utils_test)
    add_lv00_test_and_register(test_stream
        tests/test_stream.c stream_test)
    add_lv00_test_and_register(test_stream_extended
        tests/test_stream_extended.c stream_extended_test)
    add_lv00_test_and_register(test_func_block_utils
        tests/test_func_block_utils.c func_block_utils_test)
    # 恢复：preset_blocks.c 已加入 Layer 4 构建列表，依赖已满足
    add_lv00_test_and_register(test_func_block_preset
        tests/test_func_block_preset.c func_block_preset_test)
    add_lv00_test_and_register(test_error_codes
        tests/test_error_codes.c error_codes_test)
    # 恢复：interop.c 已在 Layer 5 构建列表中，依赖已满足
    add_lv00_test_and_register(test_interop
        tests/test_interop.c interop_test)
    if(WIN32)
        target_link_libraries(test_interop ws2_32)
    endif()
    # 新增：高维结构模块测试
    add_lv00_test_and_register(test_high_dim
        tests/test_high_dim.c high_dim_test)
    # 新增：符号坐标系统测试
    add_lv00_test_and_register(test_symbolic_coord
        tests/test_symbolic_coord.c symbolic_coord_test)
    # 新增：v3.3.0 新模块集成测试
    add_lv00_test_and_register(test_rational
        tests/test_rational.c rational_test)
    add_lv00_test_and_register(test_expr_canon
        tests/test_expr_canon.c expr_canon_test)
    add_lv00_test_and_register(test_proof_trace
        tests/test_proof_trace.c proof_trace_test)
    # v3.3.0 新模块集成测试
    add_lv00_test_and_register(test_new_modules
        tests/test_new_modules.c new_modules_test)
    add_lv00_test_and_register(test_circuit_breaker
        tests/test_circuit_breaker.c circuit_breaker_test)
    # v3.4.0 新增：几何代数模块测试
    add_lv00_test_and_register(test_ga_multivector
        tests/test_ga_multivector.c test_ga_multivector)
    add_lv00_test_and_register(test_geo_invariant
        tests/test_geo_invariant.c test_geo_invariant)
    # v3.4.0 新增：区间/数值验证模块测试
    add_lv00_test_and_register(test_interval_arithmetic
        tests/test_interval_arithmetic.c test_interval_arithmetic)
    add_lv00_test_and_register(test_gappa_dsl
        tests/test_gappa_dsl.c test_gappa_dsl)
    # v3.4.0 新增：SMT/SAT 模块测试
    add_lv00_test_and_register(test_smt_bitvector
        tests/test_smt_bitvector.c test_smt_bitvector)
    # v3.4.0 新增：证明策略模块测试
    add_lv00_test_and_register(test_proof_rule_engine
        tests/test_proof_rule_engine.c test_proof_rule_engine)
    # v3.4.0 新增：数论/代数模块测试
    add_lv00_test_and_register(test_nt_number_theory
        tests/test_nt_number_theory.c test_nt_number_theory)
    add_lv00_test_and_register(test_sym_expr
        tests/test_sym_expr.c test_sym_expr)
    # v3.4.0 新增：重写引擎测试
    add_lv00_test_and_register(test_rewrite_strategy
        tests/test_rewrite_strategy.c test_rewrite_strategy)
    # v3.4.0 新增：版本控制测试
    add_lv00_test_and_register(test_proof_version
        tests/test_proof_version.c test_proof_version)
    # v3.4.0 新增：自动微分测试
    add_lv00_test_and_register(test_autodiff
        tests/test_autodiff.c test_autodiff)
    # prop_verifier 测试：链接 lv00 库以获取 lv00_malloc/lv00_free 等工具函数
    add_executable(test_prop_verifier tests/test_prop_verifier.c src/core/prop_verifier.c)
    target_include_directories(test_prop_verifier PRIVATE include/lv00)
    target_link_libraries(test_prop_verifier lv00 ${GMP_LIBRARIES})
    add_test(NAME prop_verifier_test COMMAND test_prop_verifier
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    # 基准测试（构建但不注册到 CTest，手动运行）
    add_lv00_test(test_benchmark tests/test_benchmark.c)
endif()

# ============================================================
# 模糊测试
# ============================================================
if(BUILD_FUZZERS AND CMAKE_C_COMPILER_ID MATCHES "Clang")
    add_executable(fuzz_constraint_graph fuzz/fuzz_constraint_graph.c)
    target_link_libraries(fuzz_constraint_graph lv00 ${GMP_LIBRARIES})
    target_compile_options(fuzz_constraint_graph PRIVATE -fsanitize=fuzzer,address)
    target_link_options(fuzz_constraint_graph PRIVATE -fsanitize=fuzzer,address)

    add_executable(fuzz_symbolic_coord fuzz/fuzz_symbolic_coord.c)
    target_link_libraries(fuzz_symbolic_coord lv00 ${GMP_LIBRARIES})
    target_compile_options(fuzz_symbolic_coord PRIVATE -fsanitize=fuzzer,address)
    target_link_options(fuzz_symbolic_coord PRIVATE -fsanitize=fuzzer,address)
endif()

# ============================================================
# 示例程序
# ============================================================
if(BUILD_EXAMPLES)
    function(add_lv00_example name source)
        add_executable(${name} ${source})
        target_link_libraries(${name} lv00 ${GMP_LIBRARIES})
    endfunction()

    add_lv00_example(example_triangle    examples/triangle_construction.c)
    add_lv00_example(example_circle      examples/circle_intersection.c)
    add_lv00_example(example_composition examples/function_composition.c)
    add_lv00_example(example_high_dim    examples/high_dim_demo.c)
    add_lv00_example(example_streaming      examples/streaming_demo.c)
    add_lv00_example(example_stream_advanced examples/stream_advanced_demo.c)
endif()

# 修复 1：安装规则 —— 支持 cmake --install 将库文件和头文件安装到系统目录
include(GNUInstallDirs)

install(TARGETS lv00_static
    EXPORT lv00-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/lv00
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
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lv00
)

# ============================================================
# v3.3.0: pkg-config .pc 文件生成
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
    ${CMAKE_SOURCE_DIR}/cmake/lv00.pc.in
    ${CMAKE_BINARY_DIR}/lv00.pc
    @ONLY
)

install(FILES ${CMAKE_BINARY_DIR}/lv00.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# ============================================================
# v3.3.0: CMake 包配置文件生成
#
# 安装后，其他 CMake 项目可以通过以下方式引用:
#   find_package(lv00 3.3.0 REQUIRED)
#   target_link_libraries(myapp lv00::lv00)
# ============================================================

include(CMakePackageConfigHelpers)

# 包安装路径
set(LV00_CMAKE_CONFIG_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/lv00")

# 生成 lv00-config.cmake 和 lv00-config-version.cmake
configure_package_config_file(
    ${CMAKE_SOURCE_DIR}/cmake/lv00-config.cmake.in
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
# v3.3.0: CPack 打包配置
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
