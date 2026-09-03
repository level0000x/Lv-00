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
# v1.2.0: OpenMP 并行加速（可选，默认关闭）
option(lv_ENABLE_OPENMP "Enable OpenMP parallelization for numerical backend" OFF)
if(lv_ENABLE_OPENMP)
    find_package(OpenMP QUIET)
    if(OpenMP_C_FOUND)
        add_compile_options(${OpenMP_C_FLAGS})
        link_libraries(${OpenMP_C_LIBRARIES})
        add_compile_definitions(lv_HAS_OPENMP)
        message(STATUS "OpenMP enabled: ${OpenMP_C_FLAGS}")
    else()
        message(WARNING "lv_ENABLE_OPENMP is ON but OpenMP was not found. Compiling without OpenMP.")
    endif()
endif()
# v1.2.0: CUDA SDK 检测（可选，GPU 后端）
option(lv_ENABLE_CUDA "Enable CUDA GPU backend" OFF)
if(lv_ENABLE_CUDA)
    enable_language(CUDA)
    if(CMAKE_CUDA_COMPILER)
        add_compile_definitions(LV_HAS_CUDA)
        message(STATUS "CUDA enabled: ${CMAKE_CUDA_COMPILER}")
    else()
        message(WARNING "lv_ENABLE_CUDA is ON but CUDA compiler not found.")
    endif()
endif()

# v1.2.0: HIP SDK 检测（可选，AMD GPU 后端）
option(lv_ENABLE_HIP "Enable AMD HIP GPU backend" OFF)
if(lv_ENABLE_HIP)
    find_package(HIP QUIET)
    if(HIP_FOUND)
        add_compile_definitions(LV_HAS_HIP)
        message(STATUS "HIP enabled: ${HIP_VERSION}")
    else()
        message(WARNING "lv_ENABLE_HIP is ON but HIP not found.")
    endif()
endif()

# v1.2.0: Singular SDK 检测（可选，计算机代数后端）
option(lv_ENABLE_SINGULAR "Enable Singular computer algebra backend" OFF)
if(lv_ENABLE_SINGULAR)
    find_path(SINGULAR_INCLUDE_DIR singular/singular.h)
    find_library(SINGULAR_LIBRARY Singular)
    if(SINGULAR_INCLUDE_DIR AND SINGULAR_LIBRARY)
        add_compile_definitions(LV_HAS_SINGULAR)
        include_directories(${SINGULAR_INCLUDE_DIR})
        message(STATUS "Singular enabled: ${SINGULAR_LIBRARY}")
    else()
        message(WARNING "lv_ENABLE_SINGULAR is ON but Singular not found.")
    endif()
endif()
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
    # MinGW 需要 fms-extensions 以支持 __unaligned 等 Microsoft 扩展
    if(WIN32)
        add_compile_options(-fms-extensions)
    elseif(APPLE)
        # macOS (AppleClang): Apple SDK 的 <time.h> 在定义 _POSIX_C_SOURCE 时
        # 会隐藏 clock_gettime/CLOCK_REALTIME（__DARWIN_C_LEVEL 被降级），
        # 必须改用 _DARWIN_C_SOURCE（lv_platform.h 顶部已统一处理）。
        add_compile_definitions(_DARWIN_C_SOURCE=1)
    else()
        # POSIX 功能测试宏（编译行级，先于任何系统头生效）：
        # glibc 在严格 C11（-std=c11）下默认不定义 _POSIX_C_SOURCE，
        # CLOCK_REALTIME/CLOCK_MONOTONIC/strtok_r/strdup 不可见。
        # 统一在此定义，避免依赖各 .c 的 include 顺序（此前 lv.h/allocator.c
        # 等先 include 系统头导致 glibc 特性集锁定，Ubuntu/macOS 编译失败）。
        add_compile_definitions(_POSIX_C_SOURCE=200809L)
    endif()
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
    # -Wnull-dereference  : 空指针解引用警告
    # -Wduplicated-cond   : 重复条件检测
    # -Wlogical-op        : 可疑逻辑运算符检测
    # -fstack-clash-protection : 栈冲突保护
    #
    # 注：-Wconversion / -Wformat=2 / -Wduplicated-branches / -Wjump-misses-init
    #     曾在此启用，但被下方对应 -Wno-* 选项完全抵消（无效配置），已删除
    add_compile_options(
        -Wnull-dereference
        -Wduplicated-cond
        -Wlogical-op
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
    set(BUILD_TESTS OFF)
    message(STATUS "WASM: 禁用常规测试（使用 web/gui 测试）")
    
    # WASM 不需要 GMP（使用 JavaScript 大整数或内置实现）
    set(GMP_INCLUDE_DIR "")
    set(GMP_LIBRARIES "")
    add_definitions(-Dlv_WASM_BUILD=1)
    add_definitions(-Dlv_NO_GMP=1)
    add_definitions(-Dlv_NO_MPFR=1)
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
# MPFR / MPC / MPFI 库查找（默认底层依赖集：GMP 家族，D-0903-2）
# ============================================================
# 默认底层依赖集 = GMP / MPFR / MPFI / MPC（dependency-policy.md v1.0）。
# 非 WASM 构建四件套全部 REQUIRED（与 GMP 同策略：静态库优先，规避运行时
# DLL 依赖；查找目录 Hint 跟随 GMP 已定位目录 + MSYS2/Vcpkg 常用路径）。
# WASM 构建（lv_NO_GMP）下 MPFR/MPFI/MPC 一并跳过（依赖 GMP，见上分支）。
# 说明：本批仅查找 + 声明（代码层「装了 ≠ 用了」，D-0903-3）；链接在对应
# 消费者接入时经 MPFR_LIBRARIES / MPC_LIBRARIES / MPFI_LIBRARIES 追加。

# 目录 Hint 基底：跟随 GMP 已定位目录
set(_NUM_INC_HINTS)
set(_NUM_LIB_HINTS)
if(GMP_INCLUDE_DIR)
    list(APPEND _NUM_INC_HINTS "${GMP_INCLUDE_DIR}")
endif()
if(GMP_LIBRARIES AND EXISTS "${GMP_LIBRARIES}")
    get_filename_component(_gmp_lib_dir "${GMP_LIBRARIES}" DIRECTORY)
    list(APPEND _NUM_LIB_HINTS "${_gmp_lib_dir}")
elseif(GMP_INCLUDE_DIR)
    get_filename_component(_gmp_inc_dir "${GMP_INCLUDE_DIR}" DIRECTORY)
    list(APPEND _NUM_LIB_HINTS "${_gmp_inc_dir}/lib")
endif()
if(WIN32)
    list(APPEND _NUM_INC_HINTS "C:/msys64/mingw64/include" "C:/vcpkg/installed/x64-windows/include")
    list(APPEND _NUM_LIB_HINTS "C:/msys64/mingw64/lib" "C:/vcpkg/installed/x64-windows/lib")
endif()

if(NOT BUILD_WASM)
    # ---- MPFR（REQUIRED；静态优先，跟随 GMP 已定位目录）----
    find_path(MPFR_INCLUDE_DIR mpfr.h
        HINTS ${MPFR_ROOT}/include ENV MPFR_ROOT/include ${_NUM_INC_HINTS})
    if(EXISTS "${MPFR_ROOT}/lib/libmpfr.a")
        set(MPFR_LIBRARIES "${MPFR_ROOT}/lib/libmpfr.a")
    elseif(EXISTS "${_gmp_lib_dir}/libmpfr.a")
        set(MPFR_LIBRARIES "${_gmp_lib_dir}/libmpfr.a")
    else()
        find_library(MPFR_LIBRARIES mpfr
            HINTS ${MPFR_ROOT}/lib ENV MPFR_ROOT/lib ${GMP_ROOT}/lib ${_NUM_LIB_HINTS})
    endif()
    if(NOT MPFR_INCLUDE_DIR OR NOT MPFR_LIBRARIES)
        message(FATAL_ERROR "MPFR library not found (default bottom layer). "
            "Set -DMPFR_ROOT=<path> or install libmpfr-dev / mingw-w64-x86_64-mpfr / brew mpfr.")
    endif()
    message(STATUS "MPFR found: ${MPFR_LIBRARIES}")

    # ---- MPC（REQUIRED；静态优先，跟随 GMP 已定位目录）----
    find_path(MPC_INCLUDE_DIR mpc.h
        HINTS ${MPC_ROOT}/include ENV MPC_ROOT/include ${_NUM_INC_HINTS})
    if(EXISTS "${MPC_ROOT}/lib/libmpc.a")
        set(MPC_LIBRARIES "${MPC_ROOT}/lib/libmpc.a")
    elseif(EXISTS "${_gmp_lib_dir}/libmpc.a")
        set(MPC_LIBRARIES "${_gmp_lib_dir}/libmpc.a")
    else()
        find_library(MPC_LIBRARIES mpc
            HINTS ${MPC_ROOT}/lib ENV MPC_ROOT/lib ${GMP_ROOT}/lib ${_NUM_LIB_HINTS})
    endif()
    if(NOT MPC_INCLUDE_DIR OR NOT MPC_LIBRARIES)
        message(FATAL_ERROR "MPC library not found (default bottom layer). "
            "Set -DMPC_ROOT=<path> or install libmpc-dev / mingw-w64-x86_64-mpc / brew libmpc.")
    endif()
    message(STATUS "MPC found: ${MPC_LIBRARIES}")

    # ---- MPFI（可选：默认链成员；静态优先；msys2 mingw64 仓库缺包，首个消费者立项时转 REQUIRED）----
    find_path(MPFI_INCLUDE_DIR mpfi.h
        HINTS ${MPFI_ROOT}/include ENV MPFI_ROOT/include ${_NUM_INC_HINTS})
    if(EXISTS "${MPFI_ROOT}/lib/libmpfi.a")
        set(MPFI_LIBRARIES "${MPFI_ROOT}/lib/libmpfi.a")
    elseif(EXISTS "${_gmp_lib_dir}/libmpfi.a")
        set(MPFI_LIBRARIES "${_gmp_lib_dir}/libmpfi.a")
    else()
        find_library(MPFI_LIBRARIES mpfi
            HINTS ${MPFI_ROOT}/lib ENV MPFI_ROOT/lib ${GMP_ROOT}/lib ${_NUM_LIB_HINTS})
    endif()
    if(MPFI_INCLUDE_DIR AND MPFI_LIBRARIES)
        add_compile_definitions(lv_HAS_MPFI=1)
        message(STATUS "MPFI found: ${MPFI_LIBRARIES}")
    else()
        message(STATUS "MPFI not found (optional default-chain member; "
            "msys2 mingw64 repo lacks it — install libmpfi-dev/brew mpfi on other platforms, "
            "or build from source when first consumer lands)")
    endif()
else()
    message(STATUS "WASM: 跳过 MPFR/MPC/MPFI 库查找（lv_NO_MPFR）")
endif()

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
    # --- A ---
    core/include/lv/adaptive_threshold.h
    core/include/lv/algebra_mode.h
    core/include/lv/algebraic_number.h
    core/include/lv/allocator.h
    core/include/lv/application.h
    core/include/lv/approx_counter.h
    core/include/lv/atp_backend.h
    core/include/lv/autodiff.h
    core/include/lv/autodiff_vtable.h
    core/include/lv/axiom_grade.h
    core/include/lv/axiom_pkg.h
    core/include/lv/axiom_rule_engine.h
    # --- B ---
    core/include/lv/bdd_encoding.h
    core/include/lv/bicgstab_shared.h
    core/include/lv/bit_burning.h
    core/include/lv/block_graph_view.h
    core/include/lv/block_scheduler.h
    core/include/lv/blueprint_solver_incremental.h
    core/include/lv/bootstrap_test.h
    # --- C ---
    core/include/lv/circuit_breaker.h
    core/include/lv/coeff_pool.h
    core/include/lv/command_log.h
    core/include/lv/config.h
    core/include/lv/conflict_detector.h
    core/include/lv/constraint_graph.h
    core/include/lv/context.h
    core/include/lv/control_flow_blocks.h
    core/include/lv/critical_pair.h
    core/include/lv/cross_platform.h
    # --- D ---
    core/include/lv/data_structure_blocks.h
    core/include/lv/debug.h
    core/include/lv/default_host_ops.h
    core/include/lv/depth_limits.h
    core/include/lv/determinism_state.h
    core/include/lv/dsl_compiler.h
    core/include/lv/dsl_extension.h
    # --- E ---
    core/include/lv/ecosystem.h
    core/include/lv/effect_system.h
    core/include/lv/engine.h
    core/include/lv/engine_access.h
    core/include/lv/engine_scheduler.h
    core/include/lv/engine_status.h
    core/include/lv/equiv_class.h
    core/include/lv/error_codes.h
    core/include/lv/euclidean_geometry.h
    core/include/lv/exact_arithmetic.h
    core/include/lv/expr_canon.h
    core/include/lv/expr_canonical.h
    core/include/lv/expr_vtable.h
    core/include/lv/extended_types.h
    # --- F ---
    core/include/lv/fast_index.h
    core/include/lv/float_error.h
    core/include/lv/formula_converter.h
    core/include/lv/formula_parser.h
    core/include/lv/formula_renderer.h
    core/include/lv/func_block.h
    core/include/lv/func_block_custom.h
    core/include/lv/func_block_internal.h
    core/include/lv/func_block_preset.h
    core/include/lv/func_block_preset_ops.h
    core/include/lv/func_block_registry.h
    core/include/lv/func_block_template.h
    core/include/lv/func_block_utils.h
    # --- G ---
    core/include/lv/ga_codegen.h
    core/include/lv/ga_interface.h
    core/include/lv/ga_multivector.h
    core/include/lv/gappa_dsl.h
    core/include/lv/gappa_propagate.h
    core/include/lv/gc_language.h
    core/include/lv/geo_aabb_tree.h
    core/include/lv/geo_constraint_solver.h
    core/include/lv/geo_dynamic.h
    core/include/lv/geo_event_detect.h
    core/include/lv/geo_halfedge_mesh.h
    core/include/lv/geo_invariant_type.h
    core/include/lv/geo_predicate.h
    core/include/lv/geo_spec.h
    core/include/lv/geo_topology.h
    core/include/lv/geo_utils.h
    core/include/lv/geo_visual.h
    core/include/lv/geom_evol.h
    core/include/lv/geometric_primitives.h
    core/include/lv/geometry_compress.h
    core/include/lv/geometry_config.h
    core/include/lv/geometry_ops.h
    core/include/lv/geometry_transform.h
    core/include/lv/geometry_types.h
    core/include/lv/gmres_shared.h
    core/include/lv/graph_hash.h
    core/include/lv/groebner_engine.h
    core/include/lv/groebner_parallel.h
    # --- H ---
    core/include/lv/hash_history.h
    core/include/lv/high_dim.h
    core/include/lv/host_linalg.h
    # --- I ---
    core/include/lv/inequality_reasoning.h
    core/include/lv/interactive_geo.h
    core/include/lv/interop.h
    core/include/lv/interop_bridge_common.h
    core/include/lv/interop_step_type.h
    core/include/lv/interval_arith.h
    core/include/lv/interval_arithmetic.h
    core/include/lv/io_block.h
    core/include/lv/io_blocks.h
    # --- L ---
    core/include/lv/lambda_church.h
    core/include/lv/lambda_term.h
    core/include/lv/lambda_to_graph.h
    core/include/lv/lambda_type_check.h
    core/include/lv/lambda_unify.h
    core/include/lv/layer_validation.h
    core/include/lv/lexer_shared.h
    core/include/lv/logic_check.h
    core/include/lv/lv.h
    core/include/lv/lv_api_spec.h
    core/include/lv/lv_arena.h
    core/include/lv/lv_arith_safe.h
    core/include/lv/lv_ast.h
    core/include/lv/lv_backend_plugin.h
    core/include/lv/lv_bitset.h
    core/include/lv/lv_block_utils.h
    core/include/lv/lv_builtin_commands.h
    core/include/lv/lv_bytes.h
    core/include/lv/lv_callback_list.h
    core/include/lv/lv_check.h
    core/include/lv/lv_circuit_breaker.h
    core/include/lv/lv_config.h
    core/include/lv/lv_constraint_guard.h
    core/include/lv/lv_convenience.h
    core/include/lv/lv_dot_writer.h
    core/include/lv/lv_error.h
    core/include/lv/lv_event_bus.h
    core/include/lv/lv_export_common.h
    core/include/lv/lv_file.h
    core/include/lv/lv_graph_meta_verify.h
    core/include/lv/lv_graph_traversal.h
    core/include/lv/lv_hash.h
    core/include/lv/lv_hashtable.h
    core/include/lv/lv_heap.h
    core/include/lv/lv_internal.h
    core/include/lv/lv_json.h
    core/include/lv/lv_lexer.h
    core/include/lv/lv_lifecycle.h
    core/include/lv/lv_loader.h
    core/include/lv/lv_log.h
    core/include/lv/lv_lru_cache.h
    core/include/lv/lv_mempool.h
    core/include/lv/lv_mempool_utils.h
    core/include/lv/lv_number.h
    core/include/lv/lv_numeric.h
    core/include/lv/lv_parse_utils.h
    core/include/lv/lv_parser.h
    core/include/lv/lv_path.h
    core/include/lv/lv_platform.h
    core/include/lv/lv_process.h
    core/include/lv/lv_protocol.h
    core/include/lv/lv_reasoning_stack.h
    core/include/lv/lv_registry.h
    core/include/lv/lv_render_visitor.h
    core/include/lv/lv_ringbuf.h
    core/include/lv/lv_safety.h
    core/include/lv/lv_sema.h
    core/include/lv/lv_serialize_adapters.h
    core/include/lv/lv_storage.h
    core/include/lv/lv_str_utils.h
    core/include/lv/lv_strbuf.h
    core/include/lv/lv_stream_context.h
    core/include/lv/lv_thread.h
    core/include/lv/lv_upper_api.h
    core/include/lv/lv_utils.h
    core/include/lv/lv_vec3.h
    core/include/lv/lv_view.h
    core/include/lv/lv_xmacro.h
    # --- M ---
    core/include/lv/math_input.h
    core/include/lv/math_theory_guide_cn.h
    core/include/lv/memory_pool.h
    core/include/lv/meta_proof.h
    core/include/lv/meta_repr.h
    core/include/lv/meta_verify.h
    core/include/lv/mini_kernel.h
    core/include/lv/modal_operators.h
    core/include/lv/module.h
    core/include/lv/module_internal.h
    core/include/lv/mpz_poly.h
    # --- N ---
    core/include/lv/node_deep_copy.h
    core/include/lv/normalization.h
    core/include/lv/nt_number_theory.h
    core/include/lv/nt_polynomial.h
    core/include/lv/numerical_backend.h
    # --- O ---
    core/include/lv/ode_integrator.h
    core/include/lv/ode_solver.h
    core/include/lv/orchestrator.h
    # --- P ---
    core/include/lv/parametric_curves.h
    core/include/lv/parser_safety.h
    core/include/lv/path_type.h
    core/include/lv/performance_profiler.h
    core/include/lv/plugin_security.h
    core/include/lv/plugin_system.h
    core/include/lv/preset_abstract_algebra.h
    core/include/lv/preset_advanced_geometry.h
    core/include/lv/preset_algebraic.h
    core/include/lv/preset_algebraic_geometry.h
    core/include/lv/preset_algebraic_topology.h
    core/include/lv/preset_algebraic_topology_adv.h
    core/include/lv/preset_analysis.h
    core/include/lv/preset_arithmetic_geometry.h
    core/include/lv/preset_basic_geometry.h
    core/include/lv/preset_basic_math.h
    core/include/lv/preset_blocks.h
    core/include/lv/preset_calculus.h
    core/include/lv/preset_category.h
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
    core/include/lv/preset_name_defs.h
    core/include/lv/preset_number_theory.h
    core/include/lv/preset_numerical.h
    core/include/lv/preset_numerical_analysis.h
    core/include/lv/preset_optimization.h
    core/include/lv/preset_order_theory.h
    core/include/lv/preset_polygons.h
    core/include/lv/preset_polynomial.h
    core/include/lv/preset_probability.h
    core/include/lv/preset_probability_statistics.h
    core/include/lv/preset_representation_theory.h
    core/include/lv/preset_ring_theory.h
    core/include/lv/preset_set_theory.h
    core/include/lv/preset_special_functions.h
    core/include/lv/preset_statistics.h
    core/include/lv/preset_stochastic_processes.h
    core/include/lv/preset_topology.h
    core/include/lv/preset_transformations.h
    core/include/lv/preset_trigonometry.h
    core/include/lv/probabilistic_constraint.h
    core/include/lv/proof.h
    core/include/lv/proof_compiler.h
    core/include/lv/proof_engine_enhanced.h
    core/include/lv/proof_export_enhanced.h
    core/include/lv/proof_rule_engine.h
    core/include/lv/proof_rule_engine_internal.h
    core/include/lv/proof_session.h
    core/include/lv/proof_session_internal.h
    core/include/lv/proof_step_strategy.h
    core/include/lv/proof_trace.h
    core/include/lv/proof_version.h
    core/include/lv/proof_version_internal.h
    core/include/lv/proof_widget.h
    core/include/lv/prop_formula_ops.h
    core/include/lv/prop_formula_visitor.h
    core/include/lv/prop_verifier.h
    core/include/lv/propagation.h
    # --- Q ---
    core/include/lv/quantifier.h
    # --- R ---
    core/include/lv/rational.h
    core/include/lv/recursion.h
    core/include/lv/rel_atom_type.h
    core/include/lv/relation_model.h
    core/include/lv/representation_converter.h
    core/include/lv/rewrite.h
    core/include/lv/rewrite_strategy.h
    core/include/lv/runtime_guard.h
    core/include/lv/runtime_monitor.h
    # --- S ---
    core/include/lv/sat_encoding.h
    core/include/lv/sha256.h
    core/include/lv/simd_ops.h
    core/include/lv/smt_backend.h
    core/include/lv/smt_bitvector.h
    core/include/lv/smt_theory_combiner.h
    core/include/lv/smt_trigger_engine.h
    core/include/lv/solver.h
    core/include/lv/solver_core.h
    core/include/lv/solver_dirty_set.h
    core/include/lv/solver_types.h
    core/include/lv/sparse_linear_algebra.h
    core/include/lv/status_codes.h
    core/include/lv/stream.h
    core/include/lv/stream_context_util.h
    core/include/lv/sym_expr.h
    core/include/lv/symbolic_cache.h
    core/include/lv/symbolic_coord.h
    # --- T ---
    core/include/lv/test_framework.h
    core/include/lv/thread_pool.h
    core/include/lv/three_layer_arithmetic.h
    core/include/lv/three_valued_logic.h
    core/include/lv/tikz_export.h
    core/include/lv/trust_color.h
    core/include/lv/trust_color_x.h
    core/include/lv/type_equiv_explorer.h
    core/include/lv/type_system.h
    # --- U ---
    core/include/lv/unify.h
    # --- V ---
    core/include/lv/visual_editor.h
)

# ── Layer 1: 输入解析层 (Parser) ──
# 职责：词法分析、公式解析、DSL 编译
# 依赖：Layer 2
set(lv_LAYER1_SOURCES
    # .lv 语言解析管线（纯文本 → AST，仅依赖 L2）
    core/src/layer1_parser/lv_lexer.c
    core/src/layer1_parser/lv_ast.c
    core/src/layer1_parser/lv_parser.c
    # v1.9.0 micro-bootstrap: 语义分析与加载器
    core/src/layer1_parser/lv_sema.c
    core/src/layer1_parser/lv_loader.c
    core/src/layer1_parser/math_input.c
    # F24/I5：parser_safety.c 已下沉 L2（输入安全校验设施被 L1/L3 共享，读 lvConfig）
    # 注：formula 家族（parser/converter/renderer）已整体迁 L3——其转换产物为
    #     ConstraintGraph（L3），formula_converter.h 直接 include constraint_graph.h，
    #     L1 依赖模型（仅 L2）无法承载；消费方仅 L0（lv.c）。
    #     dsl 家族（dsl_compiler*.c/dsl_lexer.c）已整体迁 L4——load 阶段注册公理包
    #     AxiomPackage（L4）且 IR→ConstraintGraph（L3），消费方仅 L0（orchestrator）。
)

# ── Layer 2: 资源管理层 (Resource) ──
# 职责：内存分配、错误码、调试、深拷贝、工具函数
# 依赖：GMP，C 标准库（仅 Layer 2 能直接依赖 GMP 链接）
set(lv_LAYER2_SOURCES
    core/src/layer2_resource/lv_utils.c
    core/src/layer2_resource/allocator.c
    core/src/layer2_resource/lv_utils_str.c
    core/src/layer2_resource/lv_utils_array.c
    core/src/layer2_resource/lv_utils_config.c
    core/src/layer2_resource/lv_utils_misc.c
    core/src/layer2_resource/lv_numeric.c
    core/src/layer2_resource/lv_json.c
    core/src/layer2_resource/lv_event_bus.c
    core/src/layer2_resource/lv_callback_list.c
    core/src/layer2_resource/lv_config.c
    # 系统入口协调者 lv.c 已移至 lv_core（L0 便利层，可协调各层生命周期，消除 L2 反向依赖）
    core/src/layer2_resource/context.c
    # F24/I5：parser_safety.c 自 L1 下沉（输入安全校验设施，L1/L3 共享，读 lvConfig）
    core/src/layer2_resource/parser_safety.c
    # v4.0.0 新增：熔断器独立模块（从 lvContext God Object 提取）
    core/src/layer2_resource/lv_circuit_breaker.c
    # v4.0.0 新增：推理分支栈独立模块（从 lvContext God Object 提取）
    # F24/I5：已从 L3 归位 L2（快照释放经 LvContextResourceOps 回调不透明解耦，
    # 不依赖 ConstraintGraph 类型）
    core/src/layer2_resource/lv_reasoning_stack.c
    # v4.0.0 新增：流式输出上下文独立模块（从 lvContext God Object 提取）
    core/src/layer2_resource/lv_stream_context.c
    core/src/layer2_resource/error_codes.c
    core/src/layer2_resource/lv_error.c
    core/src/layer2_resource/debug.c
    core/src/layer2_resource/debug_state.c
    core/src/layer2_resource/debug_mempool.c
    core/src/layer2_resource/debug_refcount.c
    core/src/layer2_resource/debug_emergency.c
    core/src/layer2_resource/debug_trace_session.c
    core/src/layer2_resource/debug_ringbuf.c
    core/src/layer2_resource/debug_log_ctx.c
    # node_deep_copy.c 已迁 L3（GeomNode/symbolic_coord/graph_node_internal 均为 L3 域）
    # v1.1.0 新增：内存池、运行时监控、测试框架
    core/src/layer2_resource/memory_pool.c
    # v3.3.0 新增：内存池公共 API 包装
    core/src/layer2_resource/lv_mempool.c
    # v3.3.0 新增：内存池静态单例工具（消除各模块重复的 init+alloc+clear 模式）
    core/src/layer2_resource/lv_mempool_utils.c
    core/src/layer2_resource/runtime_monitor.c
    core/src/layer2_resource/test_framework.c
    # 流式基础设施（自 L4 下沉：仅依赖 L2，被 L1/L2/L3 大面积消费）
    core/src/layer2_resource/stream/stream.c
    core/src/layer2_resource/stream/stream_async.c
    core/src/layer2_resource/stream/stream_buffer.c
    core/src/layer2_resource/stream/stream_context.c
    core/src/layer2_resource/stream/stream_emit.c
    core/src/layer2_resource/stream/stream_filter.c
    core/src/layer2_resource/stream/stream_json.c
    core/src/layer2_resource/stream/stream_lazy.c
    core/src/layer2_resource/stream/stream_stats.c
    core/src/layer2_resource/stream/stream_utils.c
    # 词法共享设施（自 L1 下沉：被 L1 与 L4 module/axiom_pkg 消费）
    core/src/layer2_resource/lexer_shared.c
    # 补全遗漏文件：全局状态、几何配置、调试追踪、中文预设/理论/错误/结果消息
    # 泛型容器：二叉堆；统一路径工具族（lv_path）
    core/src/layer2_resource/lv_path.c
    core/src/layer2_resource/lv_heap.c
    core/src/layer2_resource/global_state.c
    core/src/layer2_resource/geometry_config.c
    core/src/layer2_resource/debug_trace.c
    core/src/layer2_resource/math_theory_guide_cn.c
    # v3.5.0 stub→real: 8 new resource modules
    core/src/layer2_resource/fast_index.c
    # geo_spec.c / math_input.c 与上层重复，规范实现位于 layer3_geometry / layer1_parser
    core/src/layer2_resource/status_codes.c
    # v1.1.0 补全遗漏：运行时守卫、性能分析、表示转换、自适应阈值、元表示
    core/src/layer2_resource/runtime_guard.c
    core/src/layer2_resource/performance_profiler.c
    # 表示转换器已移至 layer6_visual/converter（与 block_to_* 三兄弟同域，消除 L2→L4/L6 越权）
    # adaptive_threshold.c 已迁 L3（依赖 lv_graph_traversal L3）
    # meta_repr.c 已迁 L4（编码 ConstraintGraph L3 + FuncBlock L4）
    # 补全遗漏文件：引导测试
    # bootstrap_test.c 已移至 L4（依赖 L3+L4 类型）
    # SHA-256 共享实现（从 axiom_pkg.c 提取）
    core/src/layer2_resource/sha256.c
    # 统一内容哈希抽象（SHA-256 / FNV-1a 字段级哈希 + 统一 hex 输出）
    core/src/layer2_resource/lv_hash.c
    # v6.0.0 统一哈希表设施（int/string 双形态，收敛 graph/geo/preset 手写哈希）
    core/src/layer2_resource/lv_hashtable.c
    # 蓝图 LRU 缓存（PERFORMANCE_OPTIMIZATION.md §2.3：哈希表 + 双向链表）
    core/src/layer2_resource/lv_lru_cache.c
    # 蓝图 DSL 扩展注册表与版本语义（§4.1.5）
    core/src/layer2_resource/dsl_extension.c
    # 蓝图安全宏与泄漏检测（§16.2/16.3）
    core/src/layer2_resource/lv_safety.c
    # v1.1.0 线程池（原在 L3，v2.0.0 移入 L2 作为基础设施）
    core/src/layer2_resource/thread_pool.c
    # v3.8.0 统一日志抽象层
    core/src/layer2_resource/lv_log.c
    # v3.8.0 安全字符串构建器
    core/src/layer2_resource/lv_strbuf.c
    # 泛型环形缓冲区（从 debug.c 提取）
    core/src/layer2_resource/lv_ringbuf.c
    # v3.8.0 统一字符串工具函数集
    core/src/layer2_resource/lv_str_utils.c
    # 导出公共工具已移至 layer5_output（调用者仅 L5：tikz_export / interop_export_svg）
    # v5.x 公共 DOT（Graphviz）写入器（统一 digraph 样板与 label 转义；仅依赖 L2 基础设施，供 L2~L5 复用）
    core/src/layer2_resource/lv_dot_writer.c
    # v3.8.0 统一文件 I/O 抽象层
    core/src/layer2_resource/lv_file.c
    # 统一外部进程执行器（子进程调用基础设施，ATP/SMT 后端共用）
    core/src/layer2_resource/lv_process.c
    # v1.1.0 通用注册表（统一 ATP/SMT/Groebner 后端注册表模式）
    core/src/layer2_resource/lv_registry.c
    # v1.1.0 统一后端插件系统抽象层
    core/src/layer2_resource/lv_backend_plugin.c
    # v5.0.0 统一存储抽象层
    core/src/layer2_resource/lv_storage.c
    # 内置序列化适配器（graph JSON 等业务序列化对接入统一序列化注册表）
    # lv_serialize_adapters.c 已迁 L4（graph 序列化 + meta_repr 验证注册，均 L3/L4 域）
    # v1.0.0 竞技场分配器抽象层
    core/src/layer2_resource/lv_arena.c
    # 字节流编解码基础设施（lvByteWriter/lvByteReader：endian/varint/zigzag，收敛 msgpack/压缩引擎手写位移）
    core/src/layer2_resource/lv_bytes.c
    # 复合对象生命周期管理（lv_obj_destroy_fields 字段销毁描述表 + lv_DEFER/lv_SCOPE_EXIT 作用域守卫）
    core/src/layer2_resource/lv_lifecycle.c
)

# ── Core: 核心便利层 (Core Convenience) ──
# 职责：便捷 API、快捷函数、对外包装
set(lv_CORE_SOURCES
    core/src/core/lv_convenience.c
    # 系统入口协调者（init/cleanup 各层生命周期，原在 layer2_resource，归 L0 便利层）
    core/src/lv.c
    # 补全遗漏文件：原生实现
    core/src/lv_impl_native.c
    # 上层接口实现
    core/src/lv_impl_upper.c
    core/src/lv_impl_upper_geom.c
    core/src/lv_impl_upper_algebra.c
    core/src/lv_impl_upper_visual.c
    core/src/lv_impl_upper_orchestrator.c
    # upper_meta.c 已删（无代码空壳：仅重复 internal.h 的 extern；L4 图级/l8 会话级实现均在各自层）
    core/src/lv_impl_upper_app.c
    core/src/lv_impl_upper_interop.c
    core/src/lv_impl_upper_preset.c
    core/src/lv_impl_upper_utils.c
    # F24/I5 P0-②：.lv 装载入图（lv_apply_parse_result + 名称映射表 + 声明处理）
    # 自 lv_loader.c（L1）拆出——L0 编排层可调 L3/L4，解除 lv_layer1_parser 补链
    core/src/lv_loader_engine.c
    # 提取的预设实现（从 lv_impl_upper.c 拆分）
    core/src/impl_preset_transformations.c
)

# ── Layer 3: 几何拓扑层 (Geometry) ──
# 职责：约束图、符号坐标、几何原语、高维结构
# 依赖：Layer 2，GMP（通过 Layer 2 传递）
set(lv_LAYER3_SOURCES
    # formula 家族（自 L1 整体迁入：转换产物为 ConstraintGraph，formula_converter.h
    # 直接 include constraint_graph.h；消费方仅 L0 lv.c。含 parser/ 子目录与 renderer）
    core/src/layer3_geometry/formula/formula_parser.c
    core/src/layer3_geometry/formula/formula_converter.c
    core/src/layer3_geometry/formula/formula_converter_util.c
    core/src/layer3_geometry/formula/formula_converter_geom.c
    core/src/layer3_geometry/formula/formula_converter_complex.c
    core/src/layer3_geometry/formula/formula_converter_constraint.c
    core/src/layer3_geometry/formula/formula_converter_stmt.c
    core/src/layer3_geometry/formula/formula_converter_export.c
    core/src/layer3_geometry/formula/formula_eval.c
    core/src/layer3_geometry/formula/formula_string.c
    core/src/layer3_geometry/formula/formula_curve.c
    core/src/layer3_geometry/formula/formula_renderer.c
    core/src/layer3_geometry/formula/formula_renderer_internal.c
    core/src/layer3_geometry/formula/formula_renderer_latex.c
    core/src/layer3_geometry/formula/formula_renderer_python.c
    core/src/layer3_geometry/formula/formula_renderer_dsl.c
    core/src/layer3_geometry/formula/formula_renderer_mathml.c
    core/src/layer3_geometry/formula/formula_renderer_ascii.c
    core/src/layer3_geometry/formula/formula_renderer_html.c
    core/src/layer3_geometry/formula/parser/formula_ast.c
    core/src/layer3_geometry/formula/parser/formula_dsl.c
    core/src/layer3_geometry/formula/parser/formula_dsl_lex.c
    core/src/layer3_geometry/formula/parser/formula_dsl_parse.c
    core/src/layer3_geometry/formula/parser/formula_latex.c
    core/src/layer3_geometry/formula/parser/formula_python.c
    # 深拷贝（GeomNode 域）与自适应阈值（图遍历域）自 L2 迁入
    core/src/layer3_geometry/node_deep_copy.c
    core/src/layer3_geometry/adaptive_threshold.c
    # SIMD 运算（依赖 geo_utils L3）自 L2 迁入
    core/src/layer3_geometry/simd_ops.c
    core/src/layer3_geometry/simd_batch.c
    core/src/layer3_geometry/simd_geo_matrix.c
    core/src/layer3_geometry/simd_v4d.c
    core/src/layer3_geometry/simd_v4f.c
    core/src/layer3_geometry/simd_v8f.c
    core/src/layer3_geometry/constraint_graph.c
    core/src/layer3_geometry/symbolic_coord.c
    core/src/layer3_geometry/euclidean_geometry.c
    # interactive_geo.c 已迁 L4（接受 lvEngine*，依赖 engine.h L4）
    core/src/layer3_geometry/high_dim.c
    core/src/layer3_geometry/high_dim_core.c
    core/src/layer3_geometry/high_dim_preset.c
    core/src/layer3_geometry/high_dim_project.c
    core/src/layer3_geometry/high_dim_fidelity.c
    core/src/layer3_geometry/high_dim_zoom.c
    core/src/layer3_geometry/high_dim_view.c
    core/src/layer3_geometry/high_dim_serialize.c
    core/src/layer3_geometry/high_dim_utils.c
    core/src/layer3_geometry/geometry_compress.c
    core/src/layer3_geometry/geometry_compress_triangle.c
    core/src/layer3_geometry/geometry_compress_predict.c
    core/src/layer3_geometry/geometry_compress_coords.c
    core/src/layer3_geometry/geometry_compress_edgebreaker.c
    core/src/layer3_geometry/geometry_compress_bitio.c
    core/src/layer3_geometry/geometry_compress_huffman.c
    core/src/layer3_geometry/geometry_compress_rle.c
    core/src/layer3_geometry/geometry_compress_entropy.c
    core/src/layer3_geometry/geometry_compress_main.c
    core/src/layer3_geometry/geometry_compress_decompress.c
    core/src/layer3_geometry/geometry_compress_io.c
    core/src/layer3_geometry/geo_event_detect.c
    core/src/layer3_geometry/geo_spec.c
    core/src/layer3_geometry/geometry_csg.c
    core/src/layer3_geometry/geometry_csg_node.c
    core/src/layer3_geometry/geometry_csg_bbox.c
    core/src/layer3_geometry/geometry_csg_primitive.c
    core/src/layer3_geometry/geometry_csg_mesh.c
    core/src/layer3_geometry/geometry_csg_bsp.c
    core/src/layer3_geometry/geometry_csg_hull.c
    core/src/layer3_geometry/geometry_csg_eval.c
    core/src/layer3_geometry/geometry_csg_export.c
    # geom_evol.c 已迁 L4（ODE 积分/数值后端属 L4 numeric 域）
    core/src/layer3_geometry/float_error.c
    core/src/layer3_geometry/mpz_poly_resultant.c
    # v1.1.0 新增：几何变换推理
    core/src/layer3_geometry/geometry_transform.c
    core/src/layer3_geometry/geometry_transform_analysis.c
    core/src/layer3_geometry/geometry_transform_apply.c
    core/src/layer3_geometry/geometry_transform_group.c
    # v3.4.0 新增：几何代数模块（GATr + GAALOP + GeoLogic 落地）
    core/src/layer3_geometry/ga_multivector.c
    core/src/layer3_geometry/ga_interface.c
    core/src/layer3_geometry/ga_codegen.c
    core/src/layer3_geometry/geo_invariant_type.c
    # v3.4.0 新增：区间/数值验证模块（MPFI + Gappa + FPTaylor + Herbie + FLINT/Arb 落地）
    # v1.0.0 公共区间算术库（数值验证策略的统一区间底座；gappa/float_error 收敛于此。
    # interval_arithmetic.c 已删除：其 interval_* 兼容层并入 interval_arith.c 末尾，
    # geo_predicate.c 已迁移到 lv_interval_*）
    core/src/layer3_geometry/interval_arith.c
    core/src/layer3_geometry/gappa_dsl.c
    core/src/layer3_geometry/gappa_propagate.c
    core/src/layer3_geometry/fptaylor_eval.c
    core/src/layer3_geometry/herbie_eval.c
    # v1.1.0 新增：几何拓扑
    core/src/layer3_geometry/geo_topology.c
    # v3.6.0 新增：WFC 范式 - 等价类管理器（Layer 3 约束拓扑规约）
    core/src/layer3_geometry/equiv_class.c
    # v3.6.0 新增：WFC 范式 - 约束传播引擎（Layer 3 约束拓扑规约）
    core/src/layer3_geometry/propagation.c
    # 补全遗漏文件：代数数、半边网格、约束求解器、几何工具、动态几何、AABB树、几何谓词
    core/src/layer3_geometry/algebraic_number.c
    core/src/layer3_geometry/algebraic_number_util.c
    core/src/layer3_geometry/algebraic_number_rational.c
    core/src/layer3_geometry/algebraic_number_quadratic.c
    core/src/layer3_geometry/algebraic_number_interval.c
    core/src/layer3_geometry/algebraic_number_poly.c
    core/src/layer3_geometry/algebraic_number_io.c
    core/src/layer3_geometry/geo_halfedge_mesh.c
    core/src/layer3_geometry/geo_constraint_solver.c
    core/src/layer3_geometry/geo_constraint_solver_hash.c
    core/src/layer3_geometry/geo_constraint_solver_dof.c
    core/src/layer3_geometry/geo_constraint_solver_system.c
    core/src/layer3_geometry/geo_constraint_solver_residual.c
    core/src/layer3_geometry/geo_constraint_solver_linear.c
    core/src/layer3_geometry/geo_constraint_solver_newton.c
    core/src/layer3_geometry/geo_constraint_solver_fast.c
    core/src/layer3_geometry/geo_utils.c
    core/src/layer3_geometry/geo_dynamic.c
    core/src/layer3_geometry/geo_aabb_tree.c
    core/src/layer3_geometry/aabb_box.c
    core/src/layer3_geometry/aabb_common.c
    core/src/layer3_geometry/aabb_tree_2d.c
    core/src/layer3_geometry/aabb_tree_3d.c
    core/src/layer3_geometry/geo_predicate.c
    # 位数熔断系统（自 L4 numeric 归位：依赖 L3 图内部结构，被 L3/L4 消费）
    core/src/layer3_geometry/bit_burning.c
    # 蓝图几何运算（§12.3/12.5/12.6/12.7）
    core/src/layer3_geometry/geometry_ops.c
    # 蓝图符号坐标缓存（§15.2）
    core/src/layer3_geometry/symbolic_cache.c
    # v1.1.0 补全遗漏：几何原语、参数曲线、路径类型
    core/src/layer3_geometry/parametric_curves.c
    core/src/layer3_geometry/path_type.c
    # F24/I5：lv_reasoning_stack.c 已归位 L2（快照释放经 LvContextResourceOps 回调，见 layer2 列表）
    # 约束图子模块：冲突检测、索引、内存、节点、序列化
    core/src/layer3_geometry/constraint_graph/graph_conflict.c
    core/src/layer3_geometry/constraint_graph/graph_index.c
    core/src/layer3_geometry/constraint_graph/graph_memory.c
    core/src/layer3_geometry/constraint_graph/graph_node.c
    core/src/layer3_geometry/constraint_graph/graph_node_alloc.c
    core/src/layer3_geometry/constraint_graph/graph_node_copy.c
    core/src/layer3_geometry/constraint_graph/graph_node_emit.c
    core/src/layer3_geometry/constraint_graph/graph_node_vtable.c
    core/src/layer3_geometry/constraint_graph/graph_node_hash.c
    core/src/layer3_geometry/constraint_graph/graph_node_conflict.c
    core/src/layer3_geometry/constraint_graph/graph_node_stub.c
    core/src/layer3_geometry/constraint_graph/graph_rank.c
    core/src/layer3_geometry/constraint_graph/graph_serialize.c
    core/src/layer3_geometry/constraint_graph/graph_dot_export.c
    core/src/layer3_geometry/constraint_graph/blueprint_graph_api.c
    core/src/layer3_geometry/constraint_graph/blueprint_solver_incremental.c
    # 图/树遍历抽象层实现
    core/src/layer3_geometry/constraint_graph/lv_graph_traversal.c
    core/src/layer3_geometry/constraint_graph/graph_traversal_bfs.c
    core/src/layer3_geometry/constraint_graph/graph_traversal_dfs.c
    core/src/layer3_geometry/constraint_graph/graph_traversal_tree.c
    core/src/layer3_geometry/constraint_graph/graph_traversal_util.c
    # 符号计算子模块：代数、二次、有理、坐标运算、超越函数
    core/src/layer3_geometry/symbolics/algebraic.c
    core/src/layer3_geometry/symbolics/quadratic.c
    core/src/layer3_geometry/symbolics/rational.c
    core/src/layer3_geometry/symbolics/lv_rational.c
    core/src/layer3_geometry/symbolics/symbolic_coord_ops.c
    core/src/layer3_geometry/symbolics/symbolic_coord_lifecycle.c
    core/src/layer3_geometry/symbolics/symbolic_coord_compare.c
    core/src/layer3_geometry/symbolics/symbolic_coord_transform.c
    core/src/layer3_geometry/symbolics/symbolic_coord_trust.c
    core/src/layer3_geometry/symbolics/transcendental.c
    core/src/layer3_geometry/symbolics/lv_number.c
    # 稀疏线性代数
    core/src/layer3_geometry/sparse_linear_algebra.c
)

# ── Layer 4: 公理推理层 (Reasoning) ──
# 职责：引擎、求解器、证明、重写、合一、规范化、类型系统、公理包
# 依赖：Layer 3，Layer 2

# prop_verifier 模块源文件（lv_L4_PROOF_SYSTEM_SOURCES 与 test_prop_verifier EXTRA_SOURCES 共用，
# 避免同一清单维护两处）
set(lv_PROP_VERIFIER_SOURCES
    core/src/layer4_reasoning/proof_system/prop_verifier.c
    core/src/layer4_reasoning/proof_system/prop_verifier_formula.c
    core/src/layer4_reasoning/proof_system/prop_verifier_compare.c
    core/src/layer4_reasoning/proof_system/prop_verifier_serialize.c
    core/src/layer4_reasoning/proof_system/prop_verifier_context.c
    core/src/layer4_reasoning/proof_system/prop_verifier_hash.c
    core/src/layer4_reasoning/proof_system/prop_verifier_memo.c
    core/src/layer4_reasoning/proof_system/prop_verifier_premises.c
    core/src/layer4_reasoning/proof_system/prop_verifier_forward.c
    core/src/layer4_reasoning/proof_system/prop_verifier_engine.c
    core/src/layer4_reasoning/proof_system/prop_verifier_api.c
    core/src/layer4_reasoning/proof_system/prop_verifier_checks.c
    core/src/layer4_reasoning/proof_system/prop_verifier_analysis.c
    core/src/layer4_reasoning/proof_system/prop_formula_visitor.c
    core/src/layer4_reasoning/proof_system/prop_verifier_bhk.c
    core/src/layer4_reasoning/proof_system/prop_verifier_trust.c
    core/src/layer4_reasoning/proof_system/prop_verifier_equivalence.c
    core/src/layer4_reasoning/proof_system/prop_formula_ops.c
)

# ────────────────────────────────────────────────────────────
# Layer 4 子域 SOURCES（2026-08-14 按子域拆分为独立 OBJECT 库：
# 增量构建 + 子域边界显式化；L4 五个 include 环已于 C-⑩ 全破）
# ────────────────────────────────────────────────────────────

# L4 根级公共/聚合域（跨子域的门面与调试设施）
set(lv_L4_CORE_SOURCES
    core/src/layer4_reasoning/solver.c
    core/src/layer4_reasoning/proof.c
    core/src/layer4_reasoning/rewrite.c
    core/src/layer4_reasoning/module.c
    core/src/layer4_reasoning/circuit_breaker.c
    # 几何演化引擎（ODE 积分器/数值后端属 L4 numeric 域）自 L3 迁入
    core/src/layer4_reasoning/geom_evol.c
    # 交互式几何（接受 lvEngine*，依赖 engine.h L4）自 L3 迁入
    core/src/layer4_reasoning/interactive_geo.c
    # 元表示层（编码 ConstraintGraph L3 + FuncBlock L4）自 L2 迁入
    core/src/layer4_reasoning/meta_repr.c
    # 序列化适配器（graph 序列化 L3 + meta_repr 验证注册 L4）自 L2 迁入
    core/src/layer4_reasoning/lv_serialize_adapters.c
    # 几何原语与推理引擎混合体（自 L3 归位：geo_prove/geo_export 依赖 L4 proof/rewrite/unify）
    core/src/layer4_reasoning/geometric_primitives.c
    # 引擎状态验证器（自 L2 归位：依赖 lvEngine/TypeSystem，debug 模式下被 engine_solve 调用）
    core/src/layer4_reasoning/debug_normalize_assert.c
    core/src/layer4_reasoning/debug_invariants.c
    # 精确算术（定义在 layer4_reasoning/exact_arithmetic.c）
    core/src/layer4_reasoning/exact_arithmetic.c
    # 命令日志系统
    core/src/layer4_reasoning/command_log.c
    # v2.0.0 自举差分测试（原在 L2，依赖 L3+L4 类型，故归位 L4）
    core/src/layer4_reasoning/bootstrap_test.c
    core/src/layer4_reasoning/bootstrap_test_init.c
    core/src/layer4_reasoning/bootstrap_test_diff.c
    core/src/layer4_reasoning/bootstrap_test_random.c
    core/src/layer4_reasoning/bootstrap_test_iso.c
    core/src/layer4_reasoning/bootstrap_test_primitive.c
    core/src/layer4_reasoning/bootstrap_test_oracle.c
    core/src/layer4_reasoning/bootstrap_test_report.c
    # v3.6.0 新增：WFC 范式 - 剪枝合法性元证明（Layer 4 推理层）
    core/src/layer4_reasoning/meta_proof.c
    # v1.1.0 补全遗漏：公理规则引擎
    core/src/layer4_reasoning/axiom_rule_engine.c
)

# axiom 公理包域
set(lv_L4_AXIOM_SOURCES
    core/src/layer4_reasoning/axiom/axiom_pkg.c
    core/src/layer4_reasoning/axiom/axiom_pkg_core.c
    core/src/layer4_reasoning/axiom/axiom_pkg_parser.c
    core/src/layer4_reasoning/axiom/axiom_pkg_serialize.c
    core/src/layer4_reasoning/axiom/axiom_pkg_verify.c
    core/src/layer4_reasoning/axiom/axiom_pkg_expand.c
    core/src/layer4_reasoning/axiom/axiom_pkg_depref.c
    core/src/layer4_reasoning/axiom/axiom_grade.c
    core/src/layer4_reasoning/axiom/axiom_template_test.c
)

# backends 后端求解引擎域
set(lv_L4_BACKENDS_SOURCES
    core/src/layer4_reasoning/backends/groebner_engine.c
    core/src/layer4_reasoning/backends/groebner_engine_core.c
    core/src/layer4_reasoning/backends/groebner_engine_ring.c
    core/src/layer4_reasoning/backends/groebner_engine_poly.c
    core/src/layer4_reasoning/backends/groebner_engine_ideal.c
    core/src/layer4_reasoning/backends/groebner_engine_variety.c
    core/src/layer4_reasoning/backends/groebner_mono.c
    core/src/layer4_reasoning/backends/groebner_poly.c
    core/src/layer4_reasoning/backends/smt_backend_impl.c
    core/src/layer4_reasoning/backends/smt_backend_impl_smtlib2.c
    core/src/layer4_reasoning/backends/smt_backend_impl_groebner.c
    core/src/layer4_reasoning/backends/smt_backend_impl_external.c
    core/src/layer4_reasoning/backends/atp_backend.c
    core/src/layer4_reasoning/backends/bdd_encoding.c
    core/src/layer4_reasoning/backends/bdd_encoding_add.c
    core/src/layer4_reasoning/backends/bdd_encoding_cnf.c
    core/src/layer4_reasoning/backends/bdd_encoding_encode.c
    core/src/layer4_reasoning/backends/bdd_encoding_sift.c
    core/src/layer4_reasoning/backends/probabilistic_constraint.c
    core/src/layer4_reasoning/backends/smt_theory_combiner.c
    core/src/layer4_reasoning/backends/smt_bitvector.c
    core/src/layer4_reasoning/backends/smt_trigger_engine.c
    # v5.0.0 新增：并行 Groebner 引擎（并发优化）
    core/src/layer4_reasoning/backends/groebner_parallel.c
    core/src/layer4_reasoning/backends/approx_counter.c
)

# dsl 域（自 L1 整体迁入：load 阶段 IR→ConstraintGraph（L3）并注册公理包
# AxiomPackage（L4）；消费方仅 L0 orchestrator/app/convenience）
set(lv_L4_DSL_SOURCES
    core/src/layer4_reasoning/dsl/dsl_compiler.c
    core/src/layer4_reasoning/dsl/dsl_compiler_parse.c
    core/src/layer4_reasoning/dsl/dsl_compiler_ir.c
    core/src/layer4_reasoning/dsl/dsl_compiler_load.c
    core/src/layer4_reasoning/dsl/dsl_lexer.c
)

# engine 引擎域（核心引擎 + 增强证明引擎 + 调度）
set(lv_L4_ENGINE_SOURCES
    core/src/layer4_reasoning/engine/engine.c
    core/src/layer4_reasoning/engine/engine_circuit.c
    core/src/layer4_reasoning/engine/engine_error.c
    core/src/layer4_reasoning/engine/engine_frozen.c
    core/src/layer4_reasoning/engine/engine_function.c
    core/src/layer4_reasoning/engine/engine_lifecycle.c
    core/src/layer4_reasoning/engine/engine_resource.c
    core/src/layer4_reasoning/engine/engine_solve.c
    core/src/layer4_reasoning/engine/engine_state.c
    core/src/layer4_reasoning/engine/engine_status.c
    core/src/layer4_reasoning/engine/engine_stream.c
    # 增强证明引擎
    core/src/layer4_reasoning/engine/proof_engine_enhanced.c
    core/src/layer4_reasoning/engine/proof_engine.c
    core/src/layer4_reasoning/engine/proof_strategy.c
    core/src/layer4_reasoning/engine/proof_verify.c
    core/src/layer4_reasoning/engine/proof_optimize.c
    core/src/layer4_reasoning/engine/proof_export.c
    core/src/layer4_reasoning/engine/proof_trace_tree.c
    core/src/layer4_reasoning/engine/proof_contradiction.c
    # 引擎调度
    core/src/layer4_reasoning/engine/engine_scheduler.c
)

# expr 表达式域
set(lv_L4_EXPR_SOURCES
    core/src/layer4_reasoning/expr/expr_canon.c
    core/src/layer4_reasoning/expr/expr_canonical.c
    core/src/layer4_reasoning/expr/sym_expr.c
)

# func_block 函数块域（含预设运行时；lv_PRESETS_DIR 编译定义随库）
set(lv_L4_FUNC_BLOCK_SOURCES
    core/src/layer4_reasoning/func_block/func_block.c
    core/src/layer4_reasoning/func_block/func_block_compose.c
    core/src/layer4_reasoning/func_block/func_block_determinism.c
    core/src/layer4_reasoning/func_block/func_block_instantiate.c
    core/src/layer4_reasoning/func_block/func_block_preset.c
    core/src/layer4_reasoning/func_block/func_block_preset_advanced.c
    core/src/layer4_reasoning/func_block/func_block_preset_data.c
    core/src/layer4_reasoning/func_block/func_block_preset_doc.c
    core/src/layer4_reasoning/func_block/func_block_preset_internal.c
    core/src/layer4_reasoning/func_block/func_block_preset_ops.c
    core/src/layer4_reasoning/func_block/func_block_preset_query.c
    core/src/layer4_reasoning/func_block/func_block_registry.c
    core/src/layer4_reasoning/func_block/func_block_custom.c
    core/src/layer4_reasoning/func_block/func_block_template.c
    core/src/layer4_reasoning/func_block/blueprint_preset_api.c
    core/src/layer4_reasoning/func_block/func_block_selector.c
    core/src/layer4_reasoning/func_block/func_block_serialize.c
    core/src/layer4_reasoning/func_block/func_block_utils.c
    core/src/layer4_reasoning/func_block/preset_basic_math.c
    core/src/layer4_reasoning/func_block/preset_blocks.c
    core/src/layer4_reasoning/func_block/preset_calculus.c
    core/src/layer4_reasoning/func_block/preset_common.c
    core/src/layer4_reasoning/func_block/preset_manager.c
    core/src/layer4_reasoning/func_block/preset_manager_register.c
    core/src/layer4_reasoning/func_block/preset_manager_query.c
    core/src/layer4_reasoning/func_block/preset_manager_instantiate.c
    core/src/layer4_reasoning/func_block/preset_manager_compose.c
    core/src/layer4_reasoning/func_block/preset_manager_doc.c
    core/src/layer4_reasoning/func_block/preset_manager_serialize.c
    core/src/layer4_reasoning/func_block/preset_abstract_algebra.c
)

# kernel 内核域
set(lv_L4_KERNEL_SOURCES
    core/src/layer4_reasoning/kernel/mini_kernel.c
    core/src/layer4_reasoning/kernel/gc_language.c
    core/src/layer4_reasoning/kernel/ecosystem.c
)

# lambda λ-演算域（数据类型与编译）
set(lv_L4_LAMBDA_SOURCES
    core/src/layer4_reasoning/lambda/lambda_term.c
    core/src/layer4_reasoning/lambda/lambda_to_graph.c
    core/src/layer4_reasoning/lambda/lambda_type_check.c
    core/src/layer4_reasoning/lambda/lambda_church.c
)

# model 模型/递归域
set(lv_L4_MODEL_SOURCES
    core/src/layer4_reasoning/model/recursion.c
    core/src/layer4_reasoning/model/recursion_context.c
    core/src/layer4_reasoning/model/recursion_measure.c
    core/src/layer4_reasoning/model/recursion_mutual.c
    core/src/layer4_reasoning/model/recursion_selector.c
    core/src/layer4_reasoning/model/recursion_symbolic_measure.c
    core/src/layer4_reasoning/model/recursion_test.c
    core/src/layer4_reasoning/model/recursion_utils.c
    core/src/layer4_reasoning/model/recursion_validation.c
    core/src/layer4_reasoning/model/recursion_test_suite.c
    core/src/layer4_reasoning/model/relation_model.c
    core/src/layer4_reasoning/model/algebra_mode.c
)

# module 模块域：增量、LVZ、序列化
set(lv_L4_MODULE_SOURCES
    core/src/layer4_reasoning/module/module_delta.c
    core/src/layer4_reasoning/module/module_export.c
    core/src/layer4_reasoning/module/module_lvz.c
    core/src/layer4_reasoning/module/module_serialize.c
    core/src/layer4_reasoning/module/module_serialize_msgpack.c
    core/src/layer4_reasoning/module/module_serialize_json.c
    core/src/layer4_reasoning/module/module_serialize_autosave.c
)

# numeric 数值域（含后端：SERIAL/CUDA/HIP/Singular）
set(lv_L4_NUMERIC_SOURCES
    core/src/layer4_reasoning/numeric/numerical_backend.c
    # BiCGSTAB 共享迭代内核（SERIAL/CUDA/HIP 三个后端复用）
    core/src/layer4_reasoning/numeric/bicgstab_shared.c
    # GMRES(m) 共享迭代内核（SERIAL/CUDA/HIP 三个后端复用）
    core/src/layer4_reasoning/numeric/gmres_shared.c
    core/src/layer4_reasoning/numeric/host_linalg.c
    # 主机端通用 ops 默认实现（消除后端重复的数据搬移实现）
    core/src/layer4_reasoning/numeric/backends/default_host_ops.c
    core/src/layer4_reasoning/numeric/backends/cuda_backend.c
    core/src/layer4_reasoning/numeric/backends/hip_backend.c
    core/src/layer4_reasoning/numeric/backends/singular_backend.c
    # 数论/代数（从 Layer 3 迁移）
    core/src/layer4_reasoning/numeric/nt_number_theory.c
    core/src/layer4_reasoning/numeric/nt_polynomial.c
    # 自动微分（从 Layer 3 迁移）
    core/src/layer4_reasoning/numeric/autodiff.c
    # ODE 求解器（从 Layer 3 迁移）
    core/src/layer4_reasoning/numeric/ode_solver.c
    core/src/layer4_reasoning/numeric/ode_integrator.c
    # 多项式
    core/src/layer4_reasoning/numeric/mv_polynomial.c
)

# proof 证明子域：依赖、导航、命题、版本、信任颜色
set(lv_L4_PROOF_SOURCES
    # 证明追踪（layer4_reasoning 根级文件）
    core/src/layer4_reasoning/proof_trace.c
    core/src/layer4_reasoning/proof/proof_tree.c
    core/src/layer4_reasoning/proof/proof_dependency.c
    core/src/layer4_reasoning/proof/proof_navigator.c
    core/src/layer4_reasoning/proof/proof_navigator_dependency.c
    core/src/layer4_reasoning/proof/proof_navigator_exfalso.c
    core/src/layer4_reasoning/proof/proof_navigator_interactive.c
    core/src/layer4_reasoning/proof/proof_navigator_breakpoint.c
    core/src/layer4_reasoning/proof/proof_navigator_export.c
    core/src/layer4_reasoning/proof/proof_navigator_equivalence.c
    core/src/layer4_reasoning/proof/proof_navigator_degrade.c
    core/src/layer4_reasoning/proof/proof_navigator_misc.c
    core/src/layer4_reasoning/proof/proof_navigator_utils.c
    core/src/layer4_reasoning/proof/proof_navigator_instantiate.c
    core/src/layer4_reasoning/proof/proof_proposition.c
    core/src/layer4_reasoning/proof/proof_step_registry.c
    core/src/layer4_reasoning/proof/proof_version_nl.c
    core/src/layer4_reasoning/proof/proof_version_ghost.c
    core/src/layer4_reasoning/proof/proof_version_sledge.c
    core/src/layer4_reasoning/proof/proof_version_task.c
    core/src/layer4_reasoning/proof/proof_version_isar.c
    core/src/layer4_reasoning/proof/proof_version_refine.c
    core/src/layer4_reasoning/proof/trust_color.c
    # 冲突检测（自 engine/ 归位：语义属推理辅助，proof 域 meta_verify 消费；断 engine↔proof 环）
    core/src/layer4_reasoning/proof/conflict_detector.c
    # L4 内建的证明元验证（与 L8 layer8_meta_verify/meta_verify.c 为两个独立模块）
    core/src/layer4_reasoning/proof/meta_verify.c
)

# proof_system 证明系统域（策略/会话/规则 + prop_verifier 命题验证器）
set(lv_L4_PROOF_SYSTEM_SOURCES
    core/src/layer4_reasoning/proof_system/proof_multi_strategy.c
    core/src/layer4_reasoning/proof_system/proof_search_algo.c
    core/src/layer4_reasoning/proof_system/proof_strategy_exec.c
    core/src/layer4_reasoning/proof_system/proof_strategy_core.c
    core/src/layer4_reasoning/proof_system/proof_strategy_vector.c
    core/src/layer4_reasoning/proof_system/proof_strategy_angle.c
    core/src/layer4_reasoning/proof_system/proof_strategy_deductive.c
    core/src/layer4_reasoning/proof_system/proof_strategy_coordinate.c
    core/src/layer4_reasoning/proof_system/proof_strategy_hol_oracle.c
    core/src/layer4_reasoning/proof_system/proof_strategy_numeric.c
    core/src/layer4_reasoning/proof_system/logic_check.c
    # 证明策略（从 Layer 3 迁移）
    core/src/layer4_reasoning/proof_system/proof_rule_engine.c
    core/src/layer4_reasoning/proof_system/proof_session.c
    # 版本控制（从 Layer 3 迁移）
    core/src/layer4_reasoning/proof_system/proof_version.c
    ${lv_PROP_VERIFIER_SOURCES}
)

# rewrite 重写域
set(lv_L4_REWRITE_SOURCES
    # 重写引擎（从 Layer 3 迁移）
    core/src/layer4_reasoning/rewrite/rewrite_strategy.c
    core/src/layer4_reasoning/rewrite/rewrite_apply.c
    core/src/layer4_reasoning/rewrite/rewrite_match.c
    core/src/layer4_reasoning/rewrite/rewrite_binding.c
    core/src/layer4_reasoning/rewrite/rewrite_snapshot.c
    core/src/layer4_reasoning/rewrite/rewrite_hash.c
    core/src/layer4_reasoning/rewrite/rewrite_rule.c
    core/src/layer4_reasoning/rewrite/rewrite_match_search.c
    core/src/layer4_reasoning/rewrite/rewrite_vf2.c
    core/src/layer4_reasoning/rewrite/rewrite_wl.c
    core/src/layer4_reasoning/rewrite/critical_pair.c
    core/src/layer4_reasoning/rewrite/beta_reduce.c
    core/src/layer4_reasoning/rewrite/rewrite_strategy_impl.c
)

# solver 求解器域（CDCL 核心 + 子模块 + SAT 编码管线）
set(lv_L4_SOLVER_SOURCES
    core/src/layer4_reasoning/solver/solver_core.c
    core/src/layer4_reasoning/solver/solver_coord_extract.c
    core/src/layer4_reasoning/solver/solver_equation_extract.c
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
    core/src/layer4_reasoning/solver/solver_eliminate.c
    core/src/layer4_reasoning/solver/solver_engine.c
    core/src/layer4_reasoning/solver/solver_geom_templates.c
    core/src/layer4_reasoning/solver/solver_groebner.c
    core/src/layer4_reasoning/solver/solver_multibranch.c
    # SAT 编码管线（从 backends/ 移入 solver/：编码 + 驱动 CDCL 求解，断 solver↔backends 环）
    core/src/layer4_reasoning/solver/sat_encoding.c
    core/src/layer4_reasoning/solver/sat_encoding_decode.c
    core/src/layer4_reasoning/solver/sat_encoding_formula.c
    core/src/layer4_reasoning/solver/sat_encoding_geom.c
)

# stream 流式基础设施域（自 L4 下沉：仅依赖 L2，L1/L2/L3 均消费）
set(lv_L4_STREAM_SOURCES
    core/src/layer4_reasoning/stream/stream_context_util.c
)

# type_logic 类型逻辑域（三值/模态/量词/类型系统/不等式推理）
set(lv_L4_TYPE_LOGIC_SOURCES
    core/src/layer4_reasoning/type_logic/three_valued_logic.c
    core/src/layer4_reasoning/type_logic/modal_operators.c
    core/src/layer4_reasoning/type_logic/quantifier.c
    core/src/layer4_reasoning/type_logic/quantifier_elim.c
    core/src/layer4_reasoning/type_logic/quantifier_expr.c
    core/src/layer4_reasoning/type_logic/quantifier_inst.c
    core/src/layer4_reasoning/type_logic/type_system.c
    core/src/layer4_reasoning/type_logic/type_check.c
    core/src/layer4_reasoning/type_logic/type_infer.c
    core/src/layer4_reasoning/type_logic/type_path_explorer.c
    core/src/layer4_reasoning/type_logic/type_equiv_explorer.c
    core/src/layer4_reasoning/type_logic/inequality_reasoning.c
    core/src/layer4_reasoning/type_logic/inequality_reasoning_core.c
    core/src/layer4_reasoning/type_logic/inequality_reasoning_prove.c
    core/src/layer4_reasoning/type_logic/inequality_reasoning_classic.c
    core/src/layer4_reasoning/type_logic/inequality_reasoning_transform.c
    core/src/layer4_reasoning/type_logic/inequality_reasoning_sign.c
    core/src/layer4_reasoning/type_logic/inequality_reasoning_geo.c
    core/src/layer4_reasoning/type_logic/inequality_reasoning_serialize.c
)

# unify 合一域
set(lv_L4_UNIFY_SOURCES
    core/src/layer4_reasoning/unify/unify.c
    core/src/layer4_reasoning/unify/unify_basic.c
    core/src/layer4_reasoning/unify/unify_equivalence.c
    core/src/layer4_reasoning/unify/unify_fine.c
    core/src/layer4_reasoning/unify/unify_hash.c
    core/src/layer4_reasoning/unify/unify_helpers.c
    core/src/layer4_reasoning/unify/unify_instantiate.c
    core/src/layer4_reasoning/unify/unify_report.c
    core/src/layer4_reasoning/unify/unify_simplify.c
    core/src/layer4_reasoning/unify/normalization.c
    core/src/layer4_reasoning/unify/graph_hash.c
    core/src/layer4_reasoning/unify/lambda_unify.c
)

# ── Layer 5: 结果输出层 (Output) ──
# 职责：流式输出、TikZ 导出、证明可视化、模块、互操作
# 依赖：Layer 4，Layer 3，Layer 2
set(lv_LAYER5_SOURCES
    core/src/layer5_output/proof_widget.c
    core/src/layer5_output/proof_export_enhanced.c
    core/src/layer5_output/interop/interop.c
    # 几何可视化（完整版）
    core/src/layer5_output/geo_visual_complete.c
    # 渲染访问器（多后端统一遍历抽象）
    core/src/layer5_output/lv_render_visitor.c
    # TikZ 渲染后端（基于 visitor 接口）
    core/src/layer5_output/lv_render_visitor_tikz.c
    # UI-Kernel 通信协议
    core/src/layer5_output/lv_protocol.c
    # v1.1.0 补全遗漏：插件系统、证明编译器
    core/src/layer5_output/plugin_system.c
    core/src/layer5_output/plugin_system_core.c
    # 蓝图插件安全层（§16.1/§16.2.1）
    core/src/layer5_output/plugin_security.c
    # 导出公共工具（XML 转义 / 写文件；原在 layer2，调用者仅 L5，归位输出层）
    core/src/layer5_output/lv_export_common.c
    core/src/layer5_output/plugin_system_load.c
    core/src/layer5_output/plugin_system_state.c
    core/src/layer5_output/plugin_system_query.c
    core/src/layer5_output/plugin_system_interface.c
    core/src/layer5_output/plugin_system_config.c
    core/src/layer5_output/plugin_system_event.c
    core/src/layer5_output/plugin_system_deps.c
    core/src/layer5_output/plugin_system_autoload.c
    core/src/layer5_output/plugin_system_version.c
    core/src/layer5_output/proof_compiler.c
    core/src/layer5_output/tikz_export.c
    # 互操作子模块：命令、导出、导入、服务器、定理
    core/src/layer5_output/interop/interop_command.c
    core/src/layer5_output/interop/interop_command_export.c
    core/src/layer5_output/interop/interop_command_stream.c
    core/src/layer5_output/interop/interop_export.c
    core/src/layer5_output/interop/interop_export_canonical.c
    core/src/layer5_output/interop/interop_export_coq.c
    core/src/layer5_output/interop/interop_export_geojson.c
    core/src/layer5_output/interop/interop_export_html.c
    core/src/layer5_output/interop/interop_export_lean.c
    core/src/layer5_output/interop/interop_export_pdf.c
    core/src/layer5_output/interop/interop_export_svg.c
    core/src/layer5_output/interop/interop_import.c
    core/src/layer5_output/interop/interop_import_ggb_zip.c
    core/src/layer5_output/interop/interop_import_ggb_xml.c
    core/src/layer5_output/interop/interop_import_json.c
    core/src/layer5_output/interop/interop_import_svg.c
    core/src/layer5_output/interop/interop_server.c
    core/src/layer5_output/interop/interop_server_ws.c
    core/src/layer5_output/interop/interop_theorem.c
)

# ============================================================
# 库目标 —— 五层 OBJECT 库 + 聚合静态库
# ============================================================

# ── 辅助宏：为每个层设置公共编译属性 ──
macro(lv_setup_layer target layer_id)
    # C9：内部 OBJECT 库的 include 路径（含 core/src / core/src/preset——
    # 源文件裸 include 内部头如 graph_node_internal.h / debug_internal.h 需要）
    # 仅本库编译需要，PRIVATE 不外泄；消费者接口由聚合库 lv_static/lv_shared
    # 的 PUBLIC 3 路径（core/include + core/include/lv + GMP）承担。
    target_include_directories(${target} PRIVATE
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
    # 位置无关代码：OBJECT 库可能被聚合进 lv_shared（共享库）。
    # 若无 -fPIC，TLS 变量（如 lv.c 的 s_lv_state）在 x86-64 下触发
    # "R_X86_64_TPOFF32 relocation can not be used when making a shared
    # object" 链接错误（Ubuntu python.yml 实测）。
    set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
endmacro()

# Layer 2: 资源管理层（基础层，不依赖任何上层）
add_library(lv_layer2_resource OBJECT ${lv_LAYER2_SOURCES} ${lv_HEADERS})
lv_setup_layer(lv_layer2_resource 2)
# layer6_visual/converter 已实现同名直接转换 API（block_to_text/node/geometry），
# 用宏守卫排除 layer2 旧桩，避免重复符号及 FuncBlock* 语义误读
target_compile_definitions(lv_layer2_resource PRIVATE LV_HAS_LAYER6_CONVERTER)
target_link_libraries(lv_layer2_resource ${GMP_LIBRARIES})

# Core: 核心便利层（基础层，不依赖任何上层）
add_library(lv_core OBJECT ${lv_CORE_SOURCES} ${lv_HEADERS})
lv_setup_layer(lv_core 0)

# GCC/Clang 优化标志
# 仅非 Debug / 非 coverage 时启用 -O2：Debug（断点）与 coverage（行覆盖）
# 需 -O0，无条件 -O2 会覆盖 CMAKE_BUILD_TYPE=Debug 的 -O0（K 登记 C8）。
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" AND NOT (CMAKE_BUILD_TYPE STREQUAL "Debug" OR ENABLE_COVERAGE))
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

# ── Layer 4: 公理推理层 —— 按子域拆分为独立 OBJECT 库（2026-08-14）──
# 依赖：Layer 3，Layer 2。所有 L4 子域库统一基础依赖；L4 五个 include 环
# 已于 C-⑩ 全破，子域间依赖具备无环性，跨子域链接的逐域精化留作后续批次。

# R2：L2+L3 基础依赖三元组单源（原宏内/手写/高层库 4 处逐字重复）
set(lv_L23_LIBS lv_layer3_geometry lv_layer2_resource)

macro(lv_l4_subdomain lib)
    add_library(${lib} OBJECT ${ARGN})
    lv_setup_layer(${lib} 4)
    target_link_libraries(${lib} ${lv_L23_LIBS})
endmacro()

lv_l4_subdomain(lv_l4_core ${lv_L4_CORE_SOURCES})
lv_l4_subdomain(lv_l4_axiom ${lv_L4_AXIOM_SOURCES})
lv_l4_subdomain(lv_l4_backends ${lv_L4_BACKENDS_SOURCES})
lv_l4_subdomain(lv_l4_dsl ${lv_L4_DSL_SOURCES})
lv_l4_subdomain(lv_l4_engine ${lv_L4_ENGINE_SOURCES})
lv_l4_subdomain(lv_l4_expr ${lv_L4_EXPR_SOURCES})
# func_block 域：额外携带运行时预设目录编译定义（preset_blocks.c 使用）
add_library(lv_l4_func_block OBJECT ${lv_L4_FUNC_BLOCK_SOURCES})
lv_setup_layer(lv_l4_func_block 4)
target_link_libraries(lv_l4_func_block ${lv_L23_LIBS})
target_compile_definitions(lv_l4_func_block PRIVATE
    lv_PRESETS_DIR="${CMAKE_SOURCE_DIR}/module/presets")
lv_l4_subdomain(lv_l4_kernel ${lv_L4_KERNEL_SOURCES})
lv_l4_subdomain(lv_l4_lambda ${lv_L4_LAMBDA_SOURCES})
lv_l4_subdomain(lv_l4_model ${lv_L4_MODEL_SOURCES})
lv_l4_subdomain(lv_l4_module ${lv_L4_MODULE_SOURCES})
lv_l4_subdomain(lv_l4_numeric ${lv_L4_NUMERIC_SOURCES})
lv_l4_subdomain(lv_l4_proof ${lv_L4_PROOF_SOURCES})
lv_l4_subdomain(lv_l4_proof_system ${lv_L4_PROOF_SYSTEM_SOURCES})
lv_l4_subdomain(lv_l4_rewrite ${lv_L4_REWRITE_SOURCES})
lv_l4_subdomain(lv_l4_solver ${lv_L4_SOLVER_SOURCES})
lv_l4_subdomain(lv_l4_stream ${lv_L4_STREAM_SOURCES})
lv_l4_subdomain(lv_l4_type_logic ${lv_L4_TYPE_LOGIC_SOURCES})
lv_l4_subdomain(lv_l4_unify ${lv_L4_UNIFY_SOURCES})

# L4 子域库聚合（供上层 L5/L6 与 lv_static/lv_shared 引用）
set(lv_L4_LIBS
    lv_l4_core lv_l4_axiom lv_l4_backends lv_l4_dsl lv_l4_engine
    lv_l4_expr lv_l4_func_block lv_l4_kernel lv_l4_lambda lv_l4_model
    lv_l4_module lv_l4_numeric lv_l4_proof lv_l4_proof_system lv_l4_rewrite
    lv_l4_solver lv_l4_stream lv_l4_type_logic lv_l4_unify
)

# F24/I5 P0-②：lv_loader.c 已只产 AST（纯解析，仅依赖 L2）；装载入图
# lv_apply_parse_result 迁至 lv_core（L0），L1 补链解除（原 L3/L4 链接删除）

# Layer 5: 结果输出层（依赖 Layer 4，Layer 3，Layer 2）
add_library(lv_layer5_output OBJECT ${lv_LAYER5_SOURCES} ${lv_HEADERS})
lv_setup_layer(lv_layer5_output 5)
target_link_libraries(lv_layer5_output ${lv_L4_LIBS} ${lv_L23_LIBS})
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
    core/src/layer6_visual/converter/representation_converter.c
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
target_link_libraries(lv_layer6_visual lv_layer5_output ${lv_L4_LIBS} ${lv_L23_LIBS})

# ── Layer 7: 编排调度层 (Orchestration Layer) ──
# 注: orchestrator.c / lv_pipeline.c 已随死代码清理移除；orchestrator.h 保留在
# core/include/lv/ 供 meta_verify.h include（其类型被 meta_verify.c 消费）。

# ── Layer 8: 元验证层 (Meta-Verification Layer) ──
set(lv_LAYER8_SOURCES
    core/src/layer8_meta_verify/meta_verify.c
)
set(lv_LAYER8_HEADERS
    core/include/lv/meta_verify.h
)
add_library(lv_layer8_meta_verify OBJECT ${lv_LAYER8_SOURCES} ${lv_LAYER8_HEADERS})
lv_setup_layer(lv_layer8_meta_verify 8)
target_link_libraries(lv_layer8_meta_verify lv_layer3_geometry ${lv_L4_LIBS} lv_layer2_resource)
# 注：原 lv_layer6_visual 链接为历史遗留——meta_verify.c 仅消费 L2/L3/L4
# 符号（lv_strbuf/lv_graph_traversal lv_bfs_run/proof.h 类型），无 L6 依赖。

# ── Layer 9: 应用入口层 (Application Layer) ──
# 注: application.c / application.h 已随死代码清理移除，本层暂无内容。

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

# ── 聚合对象清单（lv_static / lv_shared 共用单一事实源，B6：消除 27×2 逐字重复）──
# 新增 OBJECT 库只需在此追加一行，静态/共享两聚合库自动同步
set(lv_AGGREGATE_OBJECTS
    $<TARGET_OBJECTS:lv_core>
    $<TARGET_OBJECTS:lv_layer1_parser>
    $<TARGET_OBJECTS:lv_layer2_resource>
    $<TARGET_OBJECTS:lv_layer3_geometry>
    $<TARGET_OBJECTS:lv_layer5_output>
    $<TARGET_OBJECTS:lv_layer6_visual>
    # L4 子域 OBJECT 库（2026-08-14 拆分）
    $<TARGET_OBJECTS:lv_l4_core>
    $<TARGET_OBJECTS:lv_l4_axiom>
    $<TARGET_OBJECTS:lv_l4_backends>
    $<TARGET_OBJECTS:lv_l4_dsl>
    $<TARGET_OBJECTS:lv_l4_engine>
    $<TARGET_OBJECTS:lv_l4_expr>
    $<TARGET_OBJECTS:lv_l4_func_block>
    $<TARGET_OBJECTS:lv_l4_kernel>
    $<TARGET_OBJECTS:lv_l4_lambda>
    $<TARGET_OBJECTS:lv_l4_model>
    $<TARGET_OBJECTS:lv_l4_module>
    $<TARGET_OBJECTS:lv_l4_numeric>
    $<TARGET_OBJECTS:lv_l4_proof>
    $<TARGET_OBJECTS:lv_l4_proof_system>
    $<TARGET_OBJECTS:lv_l4_rewrite>
    $<TARGET_OBJECTS:lv_l4_solver>
    $<TARGET_OBJECTS:lv_l4_stream>
    $<TARGET_OBJECTS:lv_l4_type_logic>
    $<TARGET_OBJECTS:lv_l4_unify>
    $<TARGET_OBJECTS:lv_layer8_meta_verify>
    $<TARGET_OBJECTS:lv_layer10_interop>
)

# ── 聚合静态库（向后兼容 lv_static）──
# 将所有层的 OBJECT 文件聚合到一个静态库中
add_library(lv_static STATIC ${lv_AGGREGATE_OBJECTS})
target_include_directories(lv_static PUBLIC
    ${GMP_INCLUDE_DIR}
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/core/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/core/include/lv>
    # 裸 include（lv.h 内 #include "xxx.h"）安装消费需要 include/lv
    $<INSTALL_INTERFACE:include>
    $<INSTALL_INTERFACE:include/lv>
)
target_link_libraries(lv_static ${GMP_LIBRARIES} Threads::Threads)
if(WIN32)
    # interop_server.c 使用 winsock API，静态库需传递 ws2_32 依赖
    target_link_libraries(lv_static ws2_32)
else()
    # Linux/macOS: sqrt/round 等数学函数位于 libm（glibc 不随 libc 提供），
    # 需显式链接 -lm 并传递给消费者（此前 Ubuntu/macOS 链接阶段报
    # "undefined reference to `sqrt'/'round'"）
    target_link_libraries(lv_static m)
endif()
set_target_properties(lv_static PROPERTIES OUTPUT_NAME lv)

# preset_differential_geometry, preset_functional_analysis 已修复，预设计数已同步

if(BUILD_SHARED_LIBS)
    add_library(lv_shared SHARED ${lv_AGGREGATE_OBJECTS})
    target_include_directories(lv_shared PUBLIC
        ${GMP_INCLUDE_DIR}
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/core/include>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/core/include/lv>
        # 裸 include 安装消费需要 include/lv
        $<INSTALL_INTERFACE:include>
        $<INSTALL_INTERFACE:include/lv>
    )
    # [P0 修复] 共享库也需要链接 Threads::Threads（与静态库保持一致）
    target_link_libraries(lv_shared ${GMP_LIBRARIES} Threads::Threads)
    if(WIN32)
        target_link_libraries(lv_shared ws2_32)
    else()
        # 与 lv_static 一致：非 Windows 平台显式链接 libm
        target_link_libraries(lv_shared m)
    endif()
    # v1.1.0: 定义 lv_BUILD_SHARED 以启用 lv.h 中的 dllexport/visibility
    target_compile_definitions(lv_shared PRIVATE lv_BUILD_SHARED)

    # K18/F96：导出机制统一——GCC/Clang（非 Windows）下未装饰符号隐藏
    # （lv_PUBLIC_API 装饰全覆盖 K59 后安全）；Windows 走 __declspec(dllexport)
    # + WINDOWS_EXPORT_ALL_SYMBOLS，visibility hidden 会与之冲突（实测 0 导出）
    if(NOT WIN32)
        set_target_properties(lv_shared PROPERTIES
            C_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN ON
        )
    endif()

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
# 辅助函数：添加可执行程序（测试/示例公共脚手架，C10 收敛）
# ============================================================
function(add_lv_program name)
    add_executable(${name} ${ARGN})
    # 测试与示例均复用 test/c/test_helpers.h（含 add_point 等辅助函数）
    target_include_directories(${name} PRIVATE
        ${CMAKE_SOURCE_DIR}/test/c
    )
    target_link_libraries(${name} lv ${GMP_LIBRARIES})
    if(WIN32)
        # interop_server 等使用 winsock API，可执行文件需显式链接 ws2_32
        target_link_libraries(${name} ws2_32)
    endif()
endfunction()

# 辅助函数：添加测试可执行文件（委托 add_lv_program 公共脚手架）
function(add_lv_test name)
    add_lv_program(${name} ${ARGN})
endfunction()

# 显式注册：源路径默认 test/c/<name>.c；CTEST_NAME 指定 ctest 注册名（缺省 = target 名）。
# 调用处如需与 target 名不同的 ctest 名，必须显式传 CTEST_NAME 保持既有注册名不变。
# EXTRA_SOURCES 供多源特例使用（如 test_prop_verifier 直接编译 prop_verifier 各源文件）
function(add_lv_test_auto name)
    cmake_parse_arguments(_lvta "" "CTEST_NAME" "EXTRA_SOURCES" ${ARGN})
    add_lv_test(${name} test/c/${name}.c ${_lvta_EXTRA_SOURCES})
    if(_lvta_CTEST_NAME)
        set(_lvta_ctest "${_lvta_CTEST_NAME}")
    else()
        set(_lvta_ctest "${name}")
    endif()
    add_test(NAME "${_lvta_ctest}" COMMAND "${name}"
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endfunction()

# ============================================================
# 测试
# ============================================================
if(BUILD_TESTS)
    enable_testing()

    # 正式模块测试（注册到 CTest）
    add_lv_test_auto(test_geometric_primitives CTEST_NAME geometric_primitives_test)
    add_lv_test_auto(test_basic CTEST_NAME basic_test)
    add_lv_test_auto(test_edge_cases CTEST_NAME edge_cases_test)
    add_lv_test_auto(test_complex_graph CTEST_NAME complex_graph_test)
    add_lv_test_auto(test_normalization CTEST_NAME normalization_test)
    add_lv_test_auto(test_formula_ext CTEST_NAME formula_ext_test)
    add_lv_test_auto(test_graph_traversal_ext CTEST_NAME graph_traversal_ext_test)
    add_lv_test_auto(test_json_ext CTEST_NAME json_ext_test)
    add_lv_test_auto(test_three_valued_logic_ext CTEST_NAME three_valued_logic_ext_test)
    add_lv_test_auto(test_inequality_ext CTEST_NAME inequality_ext_test)
    add_lv_test_auto(test_lv_number_ext CTEST_NAME lv_number_ext_test)
    add_lv_test_auto(test_recursion_ext CTEST_NAME recursion_ext_test)
    add_lv_test_auto(test_lv_numeric_ext CTEST_NAME lv_numeric_ext_test)
    add_lv_test_auto(test_runtime_monitor_ext CTEST_NAME runtime_monitor_ext_test)
    add_lv_test_auto(test_axiom_rule_ext CTEST_NAME axiom_rule_ext_test)
    add_lv_test_auto(test_geometry_transform_ext CTEST_NAME geometry_transform_ext_test)
    add_lv_test_auto(test_thread_ext CTEST_NAME thread_ext_test)
    add_lv_test_auto(test_storage_ext CTEST_NAME storage_ext_test)
    add_lv_test_auto(test_algebraic_number_ext CTEST_NAME algebraic_number_ext_test)
    add_lv_test_auto(test_alg_quadratic_ext CTEST_NAME alg_quadratic_ext_test)
    add_lv_test_auto(test_alg_interval_ext CTEST_NAME alg_interval_ext_test)
    add_lv_test_auto(test_alg_poly_ext CTEST_NAME alg_poly_ext_test)
    add_lv_test_auto(test_geo_halfedge_mesh_ext CTEST_NAME geo_halfedge_mesh_ext_test)
    add_lv_test_auto(test_debug_log_ext CTEST_NAME debug_log_ext_test)
    add_lv_test_auto(test_lv_utils_array_ext CTEST_NAME lv_utils_array_ext_test)
    add_lv_test_auto(test_simd_ops_ext CTEST_NAME simd_ops_ext_test)
    add_lv_test_auto(test_modal_operators_ext CTEST_NAME modal_operators_ext_test)
    add_lv_test_auto(test_lv_error_ext CTEST_NAME lv_error_ext_test)
    add_lv_test_auto(test_memory_pool_ext CTEST_NAME memory_pool_ext_test)
    add_lv_test_auto(test_parametric_curves_ext CTEST_NAME parametric_curves_ext_test)
    add_lv_test_auto(test_config_ext CTEST_NAME config_ext_test)
    add_lv_test_auto(test_control_flow_blocks_ext CTEST_NAME control_flow_blocks_ext_test)
    add_lv_test_auto(test_lv_number_ops_ext CTEST_NAME lv_number_ops_ext_test)
    add_lv_test_auto(test_effect_system_ext CTEST_NAME effect_system_ext_test)
    add_lv_test_auto(test_axiom_grade_ext CTEST_NAME axiom_grade_ext_test)
    add_lv_test_auto(test_axiom_rule_engine_ext CTEST_NAME axiom_rule_engine_ext_test)
    add_lv_test_auto(test_lv_ast_ext CTEST_NAME lv_ast_ext_test)
    add_lv_test_auto(test_lv_ringbuf_ext CTEST_NAME lv_ringbuf_ext_test)
    add_lv_test_auto(test_lv_reasoning_stack_ext CTEST_NAME lv_reasoning_stack_ext_test)
    add_lv_test_auto(test_lv_bitset_ext CTEST_NAME lv_bitset_ext_test)  # K63/F89 位图容器
    add_lv_test_auto(test_lv_heap_ext CTEST_NAME lv_heap_ext_test)
    add_lv_test_auto(test_lv_event_bus_ext CTEST_NAME lv_event_bus_ext_test)
    add_lv_test_auto(test_lv_circuit_breaker_ext CTEST_NAME lv_circuit_breaker_ext_test)
    add_lv_test_auto(test_lv_arena_ext CTEST_NAME lv_arena_ext_test)
    add_lv_test_auto(test_lv_file_ext CTEST_NAME lv_file_ext_test)
    add_lv_test_auto(test_lv_path_ext CTEST_NAME lv_path_ext_test)
    add_lv_test_auto(test_lv_hashtable_ext CTEST_NAME lv_hashtable_ext_test)
    add_lv_test_auto(test_error_codes_ext CTEST_NAME error_codes_ext_test)
    add_lv_test_auto(test_dsl_extension CTEST_NAME dsl_extension_test)
    add_lv_test_auto(test_lru_cache CTEST_NAME lru_cache_test)
    add_lv_test_auto(test_geometry_ops CTEST_NAME geometry_ops_test)
    add_lv_test_auto(test_blueprint_g4 CTEST_NAME blueprint_g4_test)
    add_lv_test_auto(test_lv_safety CTEST_NAME lv_safety_test)
    add_lv_test_auto(test_blueprint_solver_incremental CTEST_NAME blueprint_solver_incremental_test)
    add_lv_test_auto(test_plugin_security CTEST_NAME plugin_security_test)
    add_lv_test_auto(test_sha256_ext CTEST_NAME sha256_ext_test)
    add_lv_test_auto(test_lv_strbuf_ext CTEST_NAME lv_strbuf_ext_test)
    add_lv_test_auto(test_data_structure_blocks_ext CTEST_NAME data_structure_blocks_ext_test)
    add_lv_test_auto(test_constraint_graph_ops CTEST_NAME constraint_graph_ops_test)
    add_lv_test_auto(test_constraint_graph_ext CTEST_NAME constraint_graph_ext_test)
    # 图哈希模块测试
    add_lv_test_auto(test_graph_hash CTEST_NAME graph_hash_test)
    add_lv_test_auto(test_constraint_compatibility CTEST_NAME constraint_compatibility_test)
    add_lv_test_auto(test_unify CTEST_NAME unify_test)
    add_lv_test_auto(test_unify_ext CTEST_NAME unify_ext_test)
    add_lv_test_auto(test_func_block CTEST_NAME func_block_test)
    add_lv_test_auto(test_solver CTEST_NAME solver_test)
    add_lv_test_auto(test_solver_enhanced CTEST_NAME solver_enhanced_test)
    add_lv_test_auto(test_solver_ext CTEST_NAME solver_ext_test)
    add_lv_test_auto(test_groebner_basis CTEST_NAME groebner_basis_test)
    add_lv_test_auto(test_rewrite CTEST_NAME rewrite_test)
    add_lv_test_auto(test_critical_pair CTEST_NAME critical_pair_test)
    add_lv_test_auto(test_type_system CTEST_NAME type_system_test)
    add_lv_test_auto(test_type_system_ext CTEST_NAME type_system_ext_test)
    add_lv_test_auto(test_type_equiv_explorer CTEST_NAME type_equiv_explorer_test)
    add_lv_test_auto(test_proof CTEST_NAME proof_test)
    add_lv_test_auto(test_axiom_pkg CTEST_NAME axiom_pkg_test)
    add_lv_test_auto(test_axiom_pkg_ext CTEST_NAME axiom_pkg_ext_test)
    add_lv_test_auto(test_template_test CTEST_NAME template_test_test)
    add_lv_test_auto(test_module CTEST_NAME module_test)
    add_lv_test_auto(test_module_ext CTEST_NAME module_ext_test)
    add_lv_test_auto(test_graph_serialize CTEST_NAME graph_serialize_test)
    add_lv_test_auto(test_serialize_registry CTEST_NAME serialize_registry_test)
    add_lv_test_auto(test_recursion CTEST_NAME recursion_test)
    add_lv_test_auto(test_implementation CTEST_NAME implementation_test)
    add_lv_test_auto(test_minimal_parse CTEST_NAME minimal_parse_test)
    add_lv_test_auto(test_axiom_euclidean_plane CTEST_NAME euclidean_plane_axiom_test)
    add_lv_test_auto(test_axiom_boolean_algebra CTEST_NAME boolean_algebra_axiom_test)
    add_lv_test_auto(test_axiom_classical_propositional_logic CTEST_NAME classical_propositional_logic_axiom_test)
    add_lv_test_auto(test_axiom_group_theory CTEST_NAME group_theory_axiom_test)
    add_lv_test_auto(test_axiom_zfc_set_theory CTEST_NAME zfc_set_theory_axiom_test)
    add_lv_test_auto(test_axiom_ring_theory CTEST_NAME ring_theory_axiom_test)
    add_lv_test_auto(test_axiom_intuitionistic_logic CTEST_NAME intuitionistic_logic_axiom_test)
    add_lv_test_auto(test_axiom_intuitionistic_propositional_logic CTEST_NAME intuitionistic_propositional_logic_axiom_test)
    add_lv_test_auto(test_axiom_category_theory CTEST_NAME category_theory_axiom_test)
    add_lv_test_auto(test_axiom_point_set_topology CTEST_NAME point_set_topology_axiom_test)
    add_lv_test_auto(test_axiom_hyperbolic_geometry CTEST_NAME hyperbolic_geometry_axiom_test)
    add_lv_test_auto(test_axiom_elliptic_geometry CTEST_NAME elliptic_geometry_axiom_test)
    add_lv_test_auto(test_axiom_simple_type_theory CTEST_NAME simple_type_theory_axiom_test)
    add_lv_test_auto(test_axiom_field_theory CTEST_NAME field_theory_axiom_test)
    add_lv_test_auto(test_axiom_nbg_set_theory CTEST_NAME nbg_set_theory_axiom_test)
    add_lv_test_auto(test_axiom_real_analysis CTEST_NAME real_analysis_axiom_test)
    add_lv_test_auto(test_axiom_combinatorics CTEST_NAME axiom_combinatorics_test)
    add_lv_test_auto(test_axiom_algebraic_topology CTEST_NAME algebraic_topology_axiom_test)
    add_lv_test_auto(test_axiom_model_theory CTEST_NAME model_theory_axiom_test)
    add_lv_test_auto(test_axiom_dependent_type_theory CTEST_NAME dependent_type_theory_axiom_test)
    add_lv_test_auto(test_axiom_homological_algebra CTEST_NAME homological_algebra_axiom_test)
    add_lv_test_auto(test_axiom_universal_algebra CTEST_NAME universal_algebra_axiom_test)
    add_lv_test_auto(test_axiom_differential_geometry CTEST_NAME differential_geometry_axiom_test)
    add_lv_test_auto(test_axiom_number_theory CTEST_NAME number_theory_axiom_test)
    add_lv_test_auto(test_axiom_functional_analysis CTEST_NAME functional_analysis_axiom_test)
    add_lv_test_auto(test_axiom_algebraic_geometry CTEST_NAME algebraic_geometry_axiom_test)
    add_lv_test_auto(test_axiom_proof_theory CTEST_NAME proof_theory_axiom_test)
    add_lv_test_auto(test_axiom_homotopy_type_theory CTEST_NAME homotopy_type_theory_axiom_test)
    add_lv_test_auto(test_axiom_affine_geometry CTEST_NAME affine_geometry_axiom_test)
    add_lv_test_auto(test_axiom_projective_geometry CTEST_NAME projective_geometry_axiom_test)
    add_lv_test_auto(test_axiom_lattice_theory CTEST_NAME lattice_theory_axiom_test)
    add_lv_test_auto(test_axiom_lie_theory CTEST_NAME lie_theory_axiom_test)
    add_lv_test_auto(test_axiom_modal_logic CTEST_NAME modal_logic_axiom_test)
    add_lv_test_auto(test_axiom_linear_logic CTEST_NAME linear_logic_axiom_test)
    add_lv_test_auto(test_axiom_metric_space CTEST_NAME metric_space_axiom_test)
    add_lv_test_auto(test_axiom_game_theory CTEST_NAME game_theory_axiom_test)
    add_lv_test_auto(test_axiom_information_theory CTEST_NAME information_theory_axiom_test)
    add_lv_test_auto(test_axiom_descriptive_set_theory CTEST_NAME descriptive_set_theory_axiom_test)
    add_lv_test_auto(test_axiom_domain_theory CTEST_NAME domain_theory_axiom_test)
    add_lv_test_auto(test_axiom_computability_theory CTEST_NAME computability_theory_axiom_test)
    add_lv_test_auto(test_axiom_computational_complexity_theory CTEST_NAME computational_complexity_theory_axiom_test)
    add_lv_test_auto(test_axiom_presburger_arithmetic CTEST_NAME presburger_arithmetic_axiom_test)
    add_lv_test_auto(test_utils CTEST_NAME utils_test)
    # 抽象化重构（枚举映射/字节流/生命周期/注册表）新增测试
    add_lv_test_auto(test_enum_maps CTEST_NAME enum_maps_test)
    add_lv_test_auto(test_bytes CTEST_NAME bytes_test)
    add_lv_test_auto(test_json_buf CTEST_NAME json_buf_test)
    add_lv_test_auto(test_hashtable CTEST_NAME hashtable_test)
    add_lv_test_auto(test_module_serialize_unify CTEST_NAME module_serialize_unify_test)
    add_lv_test_auto(test_lifecycle CTEST_NAME lifecycle_test)
    # 新增：L7 编排层 + L9 应用层测试
    add_lv_test_auto(test_orchestrator CTEST_NAME orchestrator_test)
    add_lv_test_auto(test_registry CTEST_NAME registry_test)
    add_lv_test_auto(test_stream CTEST_NAME stream_test)
    add_lv_test_auto(test_stream_extended CTEST_NAME stream_extended_test)
    # 线程池并行 for（lv_parallel_for）测试
    add_lv_test_auto(test_parallel_for CTEST_NAME parallel_for_test)
    add_lv_test_auto(test_func_block_utils CTEST_NAME func_block_utils_test)
    # 恢复：preset_blocks.c 已加入 Layer 4 构建列表，依赖已满足
    add_lv_test_auto(test_func_block_preset CTEST_NAME func_block_preset_test)
    add_lv_test_auto(test_error_codes CTEST_NAME error_codes_test)
    # 恢复：interop.c 已在 Layer 5 构建列表中，依赖已满足
    add_lv_test_auto(test_interop CTEST_NAME interop_test)
    # 新增：高维结构模块测试
    add_lv_test_auto(test_high_dim CTEST_NAME high_dim_test)
    # 新增：符号坐标系统测试
    add_lv_test_auto(test_symbolic_coord CTEST_NAME symbolic_coord_test)
    # 新增：v1.1.0 新模块集成测试
    add_lv_test_auto(test_rational CTEST_NAME rational_test)
    add_lv_test_auto(test_expr_canon CTEST_NAME expr_canon_test)
    # 新增：不等式推理表达式符号判定（P1-3 桩补齐）
    add_lv_test_auto(test_inequality_reasoning CTEST_NAME inequality_reasoning_test)
    # 新增：模态算子对偶转换 UNSUPPORTED 行为（P1-4）
    add_lv_test_auto(test_modal_operators CTEST_NAME modal_operators_test)
    add_lv_test_auto(test_proof_trace CTEST_NAME proof_trace_test)
    # v1.1.0 新模块集成测试
    add_lv_test_auto(test_new_modules CTEST_NAME new_modules_test)
    add_lv_test_auto(test_circuit_breaker CTEST_NAME circuit_breaker_test)
    # 批次 C-㉙：隔离上下文零覆盖 API 契约测试
    add_lv_test_auto(test_context CTEST_NAME context_test)
    # 批次 C-㉚：量词系统 + CDCL 求解器核心零覆盖契约测试
    add_lv_test_auto(test_quantifier CTEST_NAME quantifier_test)
    add_lv_test_auto(test_solver_core CTEST_NAME solver_core_test)
    # 批次 C-㉛：lv_utils.h 零覆盖 L2 基础设施契约测试
    add_lv_test_auto(test_lv_utils CTEST_NAME lv_utils_test)
    # 批次 C-㉜：lv_utils 文件 IO + 批量释放族 + 内存检查契约测试
    add_lv_test_auto(test_lv_utils_ext CTEST_NAME lv_utils_ext_test)
    # 批次 C-㉞：proof.h 引擎依赖设施专项契约测试
    add_lv_test_auto(test_proof_version_ext CTEST_NAME proof_version_ext_test)
    # 批次 C-㉟：反证法作用域（scope API）契约测试
    add_lv_test_auto(test_proof_scope CTEST_NAME proof_scope_test)
    # 批次 C-㊱：增强证明引擎零覆盖 API 契约测试
    add_lv_test_auto(test_proof_engine_enhanced CTEST_NAME proof_engine_enhanced_test)
    # v3.6.0 新增：上层统一接口（lv_upper_api.h）测试
    add_lv_test_auto(test_upper_api CTEST_NAME upper_api_test)
    # v3.4.0 新增：几何代数模块测试
    add_lv_test_auto(test_ga_multivector)
    add_lv_test_auto(test_geo_invariant)
    # v3.4.0 新增：区间/数值验证模块测试
    add_lv_test_auto(test_interval_arithmetic)
    add_lv_test_auto(test_gappa_dsl)
    # v3.4.0 新增：SMT/SAT 模块测试
    add_lv_test_auto(test_smt_bitvector)
    # v3.4.0 新增：证明策略模块测试
    add_lv_test_auto(test_proof_rule_engine)
    # v3.4.0 新增：数论/代数模块测试
    add_lv_test_auto(test_nt_number_theory)
    add_lv_test_auto(test_sym_expr)
    # 新增：稀疏线性代数（CSR + Jacobi 求解）与数论多项式模块测试
    add_lv_test_auto(test_sparse_linear_algebra CTEST_NAME sparse_linear_algebra_test)
    add_lv_test_auto(test_sparse_python_bindings_ext CTEST_NAME sparse_python_bindings_ext_test)
    add_lv_test_auto(test_nt_polynomial CTEST_NAME nt_polynomial_test)
    # v3.4.0 新增：重写引擎测试
    add_lv_test_auto(test_rewrite_strategy)
    add_lv_test_auto(test_rewrite_strategy_impl CTEST_NAME rewrite_strategy_impl_test)
    # v3.4.0 新增：版本控制测试
    add_lv_test_auto(test_proof_version)
    # v3.4.0 新增：自动微分测试
    add_lv_test_auto(test_autodiff)
    # prop_verifier 测试：直接编译 prop_verifier 各源文件（链接 lv 库以获取 lv_malloc/lv_free 等工具函数）
    # [QA] Also listed in lv_L4_PROOF_SYSTEM_SOURCES; test target requires separate compilation
    add_lv_test_auto(test_prop_verifier CTEST_NAME prop_verifier_test EXTRA_SOURCES
        ${lv_PROP_VERIFIER_SOURCES})

    # v3.6.0 新增：WFC 范式三模块集成测试
    add_lv_test_auto(test_wfc_modules CTEST_NAME wfc_modules_test)

    # v3.5.0 新增：几何约束求解器模块测试
    add_lv_test_auto(test_geo_constraint_solver CTEST_NAME geo_constraint_solver_test)
    add_lv_test_auto(test_geo_constraint_solver_ext CTEST_NAME geo_constraint_solver_ext_test)
    # ODE 求解器模块测试
    add_lv_test_auto(test_ode_solver CTEST_NAME ode_solver_test)
    add_lv_test_auto(test_proof_export_enhanced CTEST_NAME proof_export_enhanced_test)
    add_lv_test_auto(test_performance CTEST_NAME performance_test)
    # 内存管理综合测试
    add_lv_test_auto(test_memory_management CTEST_NAME memory_management_test)
    # 最小化群论公理加载测试
    add_lv_test_auto(test_group_minimal CTEST_NAME group_minimal_test)
    # 几何拓扑模块测试
    add_lv_test_auto(test_geo_topology CTEST_NAME geo_topology_test)
    # 几何谓词模块测试
    add_lv_test_auto(test_geo_predicate CTEST_NAME geo_predicate_test)
    # Halfedge 网格拓扑模块测试
    add_lv_test_auto(test_geo_halfedge_mesh CTEST_NAME geo_halfedge_mesh_test)
    # 动态几何依赖图模块测试
    add_lv_test_auto(test_geo_dynamic CTEST_NAME geo_dynamic_test)
    # 几何可视化渲染模块测试
    add_lv_test_auto(test_geo_visual CTEST_NAME geo_visual_test)
    # v3.6.0 WFC 约束传播引擎测试
    add_lv_test_auto(test_propagation CTEST_NAME propagation_test)
    # v3.6.0 等价类管理器测试
    add_lv_test_auto(test_equiv_class CTEST_NAME equiv_class_test)
    # v3.6.0 交互几何系统测试
    add_lv_test_auto(test_interactive_geo CTEST_NAME interactive_geo_test)
    # v3.7.0 几何变换预设测试
    add_lv_test_auto(test_geom_transform_presets CTEST_NAME geom_transform_presets_test)
    # AABB 树空间索引模块测试
    add_lv_test_auto(test_geo_aabb_tree CTEST_NAME geo_aabb_tree_test)
    # 错误处理与边界条件测试
    add_lv_test_auto(test_error_handling CTEST_NAME error_handling_test)
    # 矛盾约束检测器测试
    add_lv_test_auto(test_conflict_detector CTEST_NAME conflict_detector_test)
    add_lv_test_auto(test_adaptive_threshold CTEST_NAME adaptive_threshold_test)
    # 引擎核心操作测试
    add_lv_test_auto(test_engine_ops CTEST_NAME engine_ops_test)
    # 引擎调度器测试
    add_lv_test_auto(test_engine_scheduler CTEST_NAME engine_scheduler_test)
    add_lv_test_auto(test_engine_ext CTEST_NAME engine_ext_test)
    # 信任颜色映射系统测试（Gap 1）
    add_lv_test_auto(test_trust_color CTEST_NAME trust_color_test)
    # Layer5 输出层操作测试
    add_lv_test_auto(test_layer5_output_ops CTEST_NAME layer5_output_ops_test)
    # 求解器调试测试
    add_lv_test_auto(test_solve_debug CTEST_NAME solve_debug_test)
    # .lv 词法分析器测试
    add_lv_test_auto(test_lv_lexer CTEST_NAME lv_lexer_test)

    # .lv 解析器测试
    add_lv_test_auto(test_lv_parser CTEST_NAME lv_parser_test)

    # v1.9.0 micro-bootstrap: 集成测试
    add_lv_test_auto(test_lv_bootstrap CTEST_NAME lv_bootstrap_test)

    # 元验证逻辑测试
    add_lv_test_auto(test_meta_verify CTEST_NAME meta_verify_test)

    # λ-演算端到端测试（Church 数字编码、β-归约、Y 组合子）
    add_lv_test_auto(test_lambda_church CTEST_NAME lambda_church_test)

    # λ-演算 β-归约结果验证（图结构指纹比对）
    add_lv_test_auto(test_lambda_eval CTEST_NAME lambda_eval_test)

    # λ-演算句法合一 + Miller 模式合一测试
    add_lv_test_auto(test_lambda_unify CTEST_NAME lambda_unify_test)

    # 新增公理包测试
    add_lv_test_auto(test_axiom_topos_theory CTEST_NAME topos_theory_axiom_test)
    add_lv_test_auto(test_axiom_synthetic_differential_geometry CTEST_NAME synthetic_differential_geometry_axiom_test)
    add_lv_test_auto(test_axiom_second_order_arithmetic CTEST_NAME second_order_arithmetic_axiom_test)
    add_lv_test_auto(test_axiom_robin_arithmetic CTEST_NAME robin_arithmetic_axiom_test)
    add_lv_test_auto(test_axiom_quantum_information_theory CTEST_NAME quantum_information_theory_axiom_test)
    add_lv_test_auto(test_axiom_probability_theory CTEST_NAME probability_theory_axiom_test)
    add_lv_test_auto(test_axiom_peano_arithmetic CTEST_NAME peano_arithmetic_axiom_test)
    add_lv_test_auto(test_axiom_order_theory CTEST_NAME order_theory_axiom_test)
    add_lv_test_auto(test_axiom_non_well_founded_set_theory CTEST_NAME non_well_founded_set_theory_axiom_test)
    add_lv_test_auto(test_axiom_measure_theory CTEST_NAME measure_theory_axiom_test)
    add_lv_test_auto(test_axiom_linear_algebra CTEST_NAME linear_algebra_axiom_test)
    add_lv_test_auto(test_axiom_graph_theory CTEST_NAME graph_theory_axiom_test)
    add_lv_test_auto(test_axiom_galois_theory CTEST_NAME galois_theory_axiom_test)
    add_lv_test_auto(test_axiom_ergodic_theory CTEST_NAME ergodic_theory_axiom_test)
    add_lv_test_auto(test_axiom_cartesian_closed_category CTEST_NAME cartesian_closed_category_axiom_test)

    # === New tests: Layer 3 geometry core ===
    add_lv_test_auto(test_symbolic_coord_ops CTEST_NAME symbolic_coord_ops_test)
    add_lv_test_auto(test_geometry_core CTEST_NAME geometry_core_test)

    # === New tests: Layer 4 backends ===
    add_lv_test_auto(test_smt_backend CTEST_NAME smt_backend_test)
    add_lv_test_auto(test_bdd_sat_atp CTEST_NAME bdd_sat_atp_test)

    # === New tests: Layer 4 solver submodules ===
    add_lv_test_auto(test_solver_submodules CTEST_NAME solver_submodules_test)

    # === New tests: Layer 4 misc (mini_kernel / relation_model / algebra_mode / rewrite) ===
    add_lv_test_auto(test_layer4_misc CTEST_NAME layer4_misc_test)

    # === New tests: Layer 5 output (plugin / tikz / proof_compiler / protocol) ===
    add_lv_test_auto(test_layer5_output CTEST_NAME layer5_output_test)
    add_lv_test_auto(test_layer5_core CTEST_NAME layer5_core_test)
    add_lv_test_auto(test_output_export CTEST_NAME output_export_test)
    add_lv_test_auto(test_interop_ext CTEST_NAME interop_ext_test)
    add_lv_test_auto(test_interop_svg_ext CTEST_NAME interop_svg_ext_test)

    # === New tests: Layer 6 visual (canvas / scheduler / type / converter) ===
    add_lv_test_auto(test_layer6_visual CTEST_NAME layer6_visual_test)

    # === New tests: Proof infrastructure (command_log / proof_navigator / dependency) ===
    add_lv_test_auto(test_proof_infra CTEST_NAME proof_infra_test)

    # v3.7.0 新增：数值验证策略测试（proof_strategy_numeric.c）与
    # 区间算术公共库扩展函数测试（interval_arith.c）
    add_lv_test_auto(test_proof_strategy_numeric CTEST_NAME proof_strategy_numeric_test)
    add_lv_test_auto(test_interval_arith CTEST_NAME interval_arith_test)

    # Herbie 浮点表达式优化（herbie_eval.c）测试
    add_lv_test_auto(test_herbie CTEST_NAME herbie_test)

    # 公共设施与规则/桥接守护测试：lvCallbackList、并行 Groebner 忠实
    # 布尔编码、数值验证规则入口、经典策略桥接
    add_lv_test_auto(test_callback_list CTEST_NAME callback_list_test)
    add_lv_test_auto(test_groebner_parallel CTEST_NAME groebner_parallel_test)
    add_lv_test_auto(test_proof_rule_numeric CTEST_NAME proof_rule_numeric_test)
    add_lv_test_auto(test_proof_strategy_legacy CTEST_NAME proof_strategy_legacy_test)

    # 批次 C-㊺续30：零覆盖契约测试
    add_lv_test_auto(test_interval_arith_ext CTEST_NAME interval_arith_ext_test)
    add_lv_test_auto(test_gappa_propagate_ext CTEST_NAME gappa_propagate_ext_test)
    add_lv_test_auto(test_io_blocks_ext CTEST_NAME io_blocks_ext_test)
    add_lv_test_auto(test_lv_log_ext CTEST_NAME lv_log_ext_test)
    add_lv_test_auto(test_lv_registry_ext CTEST_NAME lv_registry_ext_test)
    add_lv_test_auto(test_proof_trace_ext CTEST_NAME proof_trace_ext_test)

    # 批次 C-㊺续31：零覆盖契约测试
    add_lv_test_auto(test_representation_converter_ext CTEST_NAME representation_converter_ext_test)
    add_lv_test_auto(test_geo_predicate_ext CTEST_NAME geo_predicate_ext_test)
    add_lv_test_auto(test_status_codes_ext CTEST_NAME status_codes_ext_test)
    add_lv_test_auto(test_lv_stream_context_ext CTEST_NAME lv_stream_context_ext_test)
    add_lv_test_auto(test_logic_check_ext CTEST_NAME logic_check_ext_test)
    add_lv_test_auto(test_equiv_class_ext CTEST_NAME equiv_class_ext_test)
    add_lv_test_auto(test_conflict_detector_ext CTEST_NAME conflict_detector_ext_test)

    # 批次 C-㊺续32：零覆盖契约测试（geometry_transform_ext 已注册于 C-㊺续9，本批追加）
    add_lv_test_auto(test_lv_dot_writer_ext CTEST_NAME lv_dot_writer_ext_test)
    add_lv_test_auto(test_inequality_reasoning_ext CTEST_NAME inequality_reasoning_ext_test)
    add_lv_test_auto(test_expr_canon_ext CTEST_NAME expr_canon_ext_test)
    add_lv_test_auto(test_fast_index_ext CTEST_NAME fast_index_ext_test)
    add_lv_test_auto(test_lv_lexer_ext CTEST_NAME lv_lexer_ext_test)
    add_lv_test_auto(test_ode_integrator_ext CTEST_NAME ode_integrator_ext_test)

    # 批次 C-㊺续33：零覆盖契约测试
    add_lv_test_auto(test_meta_verify_ext CTEST_NAME meta_verify_ext_test)
    add_lv_test_auto(test_engine_scheduler_ext CTEST_NAME engine_scheduler_ext_test)
    add_lv_test_auto(test_lv_mempool_ext CTEST_NAME lv_mempool_ext_test)
    add_lv_test_auto(test_geo_aabb_tree_ext CTEST_NAME geo_aabb_tree_ext_test)
    add_lv_test_auto(test_gc_language_ext CTEST_NAME gc_language_ext_test)
    add_lv_test_auto(test_ecosystem_ext CTEST_NAME ecosystem_ext_test)
    add_lv_test_auto(test_circuit_breaker_ext CTEST_NAME circuit_breaker_ext_test)

    # 批次 C-㊺续34：零覆盖契约测试
    add_lv_test_auto(test_allocator_ext CTEST_NAME allocator_ext_test)
    add_lv_test_auto(test_geo_dynamic_ext CTEST_NAME geo_dynamic_ext_test)
    add_lv_test_auto(test_expr_canonical_ext CTEST_NAME expr_canonical_ext_test)
    add_lv_test_auto(test_lv_str_utils_ext CTEST_NAME lv_str_utils_ext_test)
    add_lv_test_auto(test_lv_convenience_ext CTEST_NAME lv_convenience_ext_test)
    add_lv_test_auto(test_smt_bitvector_ext CTEST_NAME smt_bitvector_ext_test)
    add_lv_test_auto(test_geo_topology_ext CTEST_NAME geo_topology_ext_test)

    # 批次 C-㊺续35：零覆盖契约测试
    add_lv_test_auto(test_lv_backend_plugin_ext CTEST_NAME lv_backend_plugin_ext_test)
    add_lv_test_auto(test_lv_render_visitor_ext CTEST_NAME lv_render_visitor_ext_test)
    add_lv_test_auto(test_math_input_ext CTEST_NAME math_input_ext_test)
    add_lv_test_auto(test_lv_process_ext CTEST_NAME lv_process_ext_test)
    add_lv_test_auto(test_lv_export_common_ext CTEST_NAME lv_export_common_ext_test)
    add_lv_test_auto(test_lambda_term_ext CTEST_NAME lambda_term_ext_test)
    add_lv_test_auto(test_lv_version_ext CTEST_NAME lv_version_ext_test)

    # 批次 C-㊺续36：零覆盖契约测试（gappa_propagate_ext 已注册，本批追加）
    add_lv_test_auto(test_autodiff_ext CTEST_NAME autodiff_ext_test)
    add_lv_test_auto(test_lv_top_api_ext CTEST_NAME lv_top_api_ext_test)
    add_lv_test_auto(test_gappa_dsl_ext CTEST_NAME gappa_dsl_ext_test)
    add_lv_test_auto(test_exact_arithmetic_ext CTEST_NAME exact_arithmetic_ext_test)
    add_lv_test_auto(test_three_layer_arithmetic_ext CTEST_NAME three_layer_arithmetic_ext_test)
    add_lv_test_auto(test_bootstrap_test_ext CTEST_NAME bootstrap_test_ext_test)

    # 批次 C-㊺续37：零覆盖契约测试（error_codes_ext / geo_constraint_solver_ext 已注册，本批追加）
    add_lv_test_auto(test_rational_ext CTEST_NAME rational_ext_test)
    # 0b/0c：数值抽象层强制池化 + 跨类型精确提升契约（number-abstraction-layer-design）
    add_lv_test_auto(test_lv_number_pool_ext CTEST_NAME lv_number_pool_ext_test)
    # ND-5 / 批次 243：池连续段原语契约（roadmap §4.2）
    add_lv_test_auto(test_lv_number_segment_ext CTEST_NAME lv_number_segment_ext_test)
    # B0 / 批次 245：MPFR 编译+静态链接冒烟（MPFR/MPC REQUIRED 依赖地基验证）
    if(MPFR_INCLUDE_DIR AND MPFR_LIBRARIES)
        add_lv_test_auto(test_mpfr_smoke CTEST_NAME mpfr_smoke_test)
        target_include_directories(test_mpfr_smoke PRIVATE ${MPFR_INCLUDE_DIR})
        # mpfr 引用 GMP 符号：GMP 需排在 mpfr 之后（静态链接顺序）
        target_link_libraries(test_mpfr_smoke ${MPFR_LIBRARIES} ${GMP_LIBRARIES})
    endif()
    # 批次 246：MPC 编译+静态链接冒烟（复数依赖链 mpc → mpfr → gmp）
    if(MPC_INCLUDE_DIR AND MPC_LIBRARIES)
        add_lv_test_auto(test_mpc_smoke CTEST_NAME mpc_smoke_test)
        target_include_directories(test_mpc_smoke PRIVATE ${MPC_INCLUDE_DIR} ${MPFR_INCLUDE_DIR})
        target_link_libraries(test_mpc_smoke ${MPC_LIBRARIES} ${MPFR_LIBRARIES} ${GMP_LIBRARIES})
    endif()
    add_lv_test_auto(test_lexer_shared_ext CTEST_NAME lexer_shared_ext_test)
    add_lv_test_auto(test_geo_spec_ext CTEST_NAME geo_spec_ext_test)
    add_lv_test_auto(test_gmres_shared_ext CTEST_NAME gmres_shared_ext_test)
    add_lv_test_auto(test_bicgstab_shared_ext CTEST_NAME bicgstab_shared_ext_test)
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
        # 委托 add_lv_program 公共脚手架（含 ws2_32 链接，与测试一致——C10）
        add_lv_program(${name} ${source})
    endfunction()

    add_lv_example(example_triangle    test/examples/triangle_construction.c)
    add_lv_example(example_circle      test/examples/circle_intersection.c)
    add_lv_example(example_composition test/examples/function_composition.c)
    add_lv_example(example_high_dim    test/examples/high_dim_demo.c)
    add_lv_example(example_streaming      test/examples/streaming_demo.c)
    add_lv_example(example_stream_advanced test/examples/stream_advanced_demo.c)
    add_lv_example(example_type_system     test/examples/type_system_demo.c)
    add_lv_example(example_recursion       test/examples/recursion_demo.c)
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
# 与 lv-config.cmake 同目录（${libdir}/cmake/lv），保证 find_package 能找到 targets
install(EXPORT lv-targets
    FILE lv-targets.cmake
    NAMESPACE lv::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lv
)

# ============================================================
# v1.1.0: pkg-config .pc 文件生成
#
# 安装后，使用者可以通过以下命令获取编译和链接标志:
#   pkg-config --cflags lv
#   pkg-config --libs lv
# ============================================================

# 准备 pkg-config 变量
set(lv_PC_REQUIRES "gmp mpfr mpc")    # 依赖的 pkg-config 包（默认底层集 GMP/MPFR/MPC；MPFI 待消费者接入后追加）

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
set(CPACK_DEBIAN_PACKAGE_DEPENDS     "libgmp10 (>= 6.0.0), libmpfr6 (>= 4.0.0), libmpc3 (>= 1.1.0)")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE    "https://github.com/example/lv")

# RPM 包特定设置
set(CPACK_RPM_PACKAGE_LICENSE        "MIT")
set(CPACK_RPM_PACKAGE_GROUP          "Development/Libraries")
set(CPACK_RPM_PACKAGE_REQUIRES       "gmp >= 6.0.0, mpfr >= 3.1.0, libmpc >= 1.0.0")
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
