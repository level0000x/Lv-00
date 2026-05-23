cmake_minimum_required(VERSION 3.15)
project(lv00 VERSION 3.0.1 LANGUAGES C)  # 版本号与 lv00.h 中 LV00_VERSION_MAJOR/MINOR/PATCH 保持一致

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

# --- MSVC 警告选项 ---
if(MSVC)
    # /W3 : 标准警告级别（/W4 过于严格，可能产生大量噪音）
    add_compile_options(/W3)
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
if(WIN32)
    # 优先使用 GMP_ROOT 环境变量，回退到常见 MSYS2/MinGW 路径
    set(_GMP_FALLBACK_DIR "C:/msys64/mingw64")
    find_path(GMP_INCLUDE_DIR gmp.h
        HINTS ${GMP_ROOT}/include ENV GMP_ROOT/include
              "${_GMP_FALLBACK_DIR}/include"
    )
    # Windows 上优先使用静态库以避免 DLL 依赖
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
else()
    find_path(GMP_INCLUDE_DIR gmp.h)
    find_library(GMP_LIBRARIES gmp)
endif()

if(NOT GMP_INCLUDE_DIR OR NOT GMP_LIBRARIES)
    message(FATAL_ERROR "GMP library not found. Set -DGMP_ROOT=<path> or install libgmp-dev.")
endif()

# GMP_LIBRARIES 已包含完整路径，无需 link_directories

# ============================================================
# 源文件
# ============================================================
set(LV00_HEADERS
    include/lv00/lv00.h
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
    # 模块化预设函数块系统
    include/lv00/preset_blocks.h
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
    include/lv00/preset_numerical_analysis.h
    include/lv00/preset_statistics.h
)

set(LV00_SOURCES)
file(GLOB_RECURSE LV00_SOURCES
    src/*.c
)
# Exclude test files and deprecated
list(FILTER LV00_SOURCES EXCLUDE REGEX "tests/")
list(FILTER LV00_SOURCES EXCLUDE REGEX "preset_basic_geometry_v2\\.c$")
list(FILTER LV00_SOURCES EXCLUDE REGEX "\\.deprecated$")
# Exclude duplicate source files (root-level copies superseded by subdir versions)
list(FILTER LV00_SOURCES EXCLUDE REGEX "^src/symbolic_coord\\.c$")
# Exclude incomplete/legacy files that use incompatible type systems
list(FILTER LV00_SOURCES EXCLUDE REGEX "magic/magic\\.c$")
list(FILTER LV00_SOURCES EXCLUDE REGEX "symbolic_coord_fixed\\.c$")


# ============================================================
# 库目标
# ============================================================
add_library(lv00_static STATIC ${LV00_SOURCES} ${LV00_HEADERS})
# 修复 2：使用 target_include_directories 替代全局 include_directories，
# 避免目录属性污染所有目标
# 源文件使用 #include "xxx.h" 模式（直接引用头文件名），
# 因此 include 目录设为 ${CMAKE_SOURCE_DIR}/include/lv00。
# 这样编译器会在 include/lv00/ 下找到对应的头文件。
target_include_directories(lv00_static PUBLIC
    ${GMP_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/include/lv00
)
target_link_libraries(lv00_static ${GMP_LIBRARIES})
set_target_properties(lv00_static PROPERTIES OUTPUT_NAME lv00)

if(BUILD_SHARED_LIBS)
    add_library(lv00_shared SHARED ${LV00_SOURCES} ${LV00_HEADERS})
    target_include_directories(lv00_shared PUBLIC
        ${GMP_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}/include/lv00
    )
    target_link_libraries(lv00_shared ${GMP_LIBRARIES})
    if(WIN32)
        target_link_libraries(lv00_shared ws2_32)
    endif()
    # 共享库构建时跳过有编译错误的预设
    target_compile_definitions(lv00_shared PRIVATE LV00_EXCLUDE_BROKEN_PRESETS)
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
    # 抑制 test_edge_cases 中的 return-mismatch 警告
    if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(test_edge_cases PRIVATE -Wno-return-mismatch)
    endif()
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
    add_lv00_test_and_register(test_utils
        tests/test_utils.c utils_test)
    add_lv00_test_and_register(test_stream
        tests/test_stream.c stream_test)
    add_lv00_test_and_register(test_stream_extended
        tests/test_stream_extended.c stream_extended_test)
    add_lv00_test_and_register(test_func_block_utils
        tests/test_func_block_utils.c func_block_utils_test)
    # 暂时排除：依赖 preset_blocks.c
    # add_lv00_test_and_register(test_func_block_preset
    #     tests/test_func_block_preset.c func_block_preset_test)
    add_lv00_test_and_register(test_error_codes
        tests/test_error_codes.c error_codes_test)
    # 新增：互操作模块测试
    add_lv00_test_and_register(test_interop
        tests/test_interop.c interop_test)
    # test_interop 使用 Winsock，需要链接 ws2_32
    if(WIN32)
        target_link_libraries(test_interop ws2_32)
    endif()
    # 新增：高维结构模块测试
    add_lv00_test_and_register(test_high_dim
        tests/test_high_dim.c high_dim_test)
    # 新增：符号坐标系统测试
    add_lv00_test_and_register(test_symbolic_coord
        tests/test_symbolic_coord.c symbolic_coord_test)
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
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()
