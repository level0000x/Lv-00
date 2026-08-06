/**
 * @mainpage Lv-00 几何元语言系统
 *
 * @section intro_sec 简介
 *
 * Lv-00 是一门以几何为唯一载体的双模数学元语言。
 * 几何体本身是计算的执行者、数据的承载者、证明的见证者。
 *
 * @section getting_started_sec 快速开始
 *
 * @code
 * // 1. 包含主头文件
 * #include "lv/lv.h"
 *
 * // 2. 初始化系统
 * if (!lv_init()) {
 *     fprintf(stderr, "Failed to initialize Lv-00\n");
 *     return 1;
 * }
 *
 * // 3. 创建引擎
 * lvEngine *engine = lv_engine_create();
 * if (!engine) {
 *     fprintf(stderr, "Failed to create engine\n");
 *     lv_cleanup();
 *     return 1;
 * }
 *
 * // 4. 构建几何问题
 * int p1 = lv_add_point(engine, 0, 1, 0, 1);   // 点 (0,0)
 * int p2 = lv_add_point(engine, 3, 1, 0, 1);   // 点 (3,0)
 * int p3 = lv_add_point(engine, 0, 1, 4, 1);   // 点 (0,4)
 * lv_add_line_segment(engine, p1, p2);
 * lv_add_line_segment(engine, p2, p3);
 * lv_add_line_segment(engine, p3, p1);
 * lv_add_constraint_incidence(engine, p1, 0);
 *
 * // 5. 求解
 * lv_normalize(engine, true);
 * EngineSolveResult result = lv_solve(engine);
 *
 * // 6. 获取结果
 * char info[1024];
 * lv_get_system_info(info, sizeof(info));
 * printf("Result: %s\n", info);
 *
 * // 7. 清理
 * lv_engine_destroy(engine);
 * lv_cleanup();
 * @endcode
 *
 * @section version_sec 版本
 * 当前版本: 3.3.0
 *
 * @section architecture_sec 架构
 *
 * Lv-00 采用严格的五层单向依赖架构:
 *   - 第1层: 输入解析层 (Parser)
 *   - 第2层: 资源管理层 (Resource)
 *   - 第3层: 几何拓扑层 (Geometry)
 *   - 第4层: 公理推理层 (Reasoning)
 *   - 第5层: 结果输出层 (Output)
 *
 * 详见 docs/ARCHITECTURE_v3.3.md
 *
 * @author Lv-00 Project
 * @version 1.1.0
 * @copyright Copyright (c) 2024-2026 Lv-00 Project
 */

/**
 * @file lv.h
 * @brief Lv-00 几何元语言系统 —— 公共 API 主入口
 *
 * @details 本文件是 Lv-00 的公共 API 入口，仅暴露对外接口。
 *          内部实现细节（如共享数据结构、JSON 序列化工具、内部宏等）
 *          集中定义在 lv_internal.h 中，该头文件不应被外部使用者直接引用。
 *
 * @note   所有公共函数通过 lv_PUBLIC_API 宏声明，以支持共享库（DLL/SO）构建。
 *         静态库构建时 lv_PUBLIC_API 展开为空。
 */

/* 头文件守卫：lv_lv_H = "lv"（项目名）+ "_" + "lv_H"（文件名 lv.h）
 * 采用双前缀格式避免与子模块头文件（如 solver.h → lv_SOLVER_H）冲突 */
#ifndef lv_lv_H
#define lv_lv_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ============================================================
 * 第零层：跨平台兼容层（必须在所有头文件之前）
 * ============================================================
 * 引入统一的跨平台类型系统和检测宏。
 * 所有平台差异在此集中处理，模块代码只使用统一宏接口。
 * ============================================================ */
#include "cross_platform.h"

/* ============================================================
 * lv_PUBLIC_API —— 共享库导出/导入控制
 * ============================================================
 *
 * 用途：控制符号在共享库（DLL/SO）中的可见性。
 *
 * 行为:
 *   - 构建 lv 共享库时:
 *       CMake 定义 lv_BUILD_SHARED → lv_PUBLIC_API 展开为 __declspec(dllexport) (MSVC)
 *       或 __attribute__((visibility("default"))) (GCC/Clang)
 *   - 使用者链接 lv 共享库时:
 *       不定义 lv_BUILD_SHARED → lv_PUBLIC_API 展开为 __declspec(dllimport) (MSVC)
 *       或空 (GCC/Clang，默认可见性即可)
 *   - 构建静态库时: BUILD_SHARED_LIBS=OFF → lv_PUBLIC_API 展开为空
 *
 * 使用示例:
 *   lv_PUBLIC_API bool lv_init(void);
 *   lv_PUBLIC_API void lv_cleanup(void);
 * ============================================================ */
#if defined(_WIN32) || defined(_MSC_VER)
/* Windows DLL 导出/导入 */
#ifdef lv_BUILD_SHARED
#define lv_PUBLIC_API __declspec(dllexport)
#else
#ifdef lv_USE_SHARED
#define lv_PUBLIC_API __declspec(dllimport)
#else
#define lv_PUBLIC_API
#endif
#endif
#elif defined(__GNUC__) || defined(__clang__)
/* GCC/Clang 符号可见性 */
#ifdef lv_BUILD_SHARED
#define lv_PUBLIC_API __attribute__((visibility("default")))
#else
#define lv_PUBLIC_API
#endif
#else
#define lv_PUBLIC_API
#endif

/* ============================================================
 * 线程局部存储宏（跨平台统一，避免各模块各自定义）
 * ============================================================ */
#if defined(_MSC_VER)
#define lv_THREAD_LOCAL __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
#define lv_THREAD_LOCAL __thread
#else
#if __STDC_VERSION__ >= 201112L
#define lv_THREAD_LOCAL _Thread_local
#else
#define lv_THREAD_LOCAL /* 不支持：回退到全局变量 */
/* 注意：当前编译器不支持线程局部存储（thread-local storage），
 * lv_THREAD_LOCAL 回退为全局变量。多线程场景下需外部同步保护。 */
#endif
#endif

/* strdup 兼容性（非标准C函数）
 * 使用 lv_strdup_safe 确保与 lv_malloc/lv_free 内存分配器兼容。
 * 如果 memory_pool.h 中声明了 lv_strdup 函数，则不使用宏定义，
 * 以避免宏吞掉函数声明导致编译错误。 */
#ifndef lv_STRDUP_AS_FUNCTION
#define lv_strdup lv_strdup_safe
#endif /* lv_STRDUP_AS_FUNCTION */

/* localtime 线程安全包装 */
#ifndef lv_LOCALTIME
#if defined(_WIN32)
#define lv_LOCALTIME(time_ptr, tm_buf) localtime_s(tm_buf, time_ptr)
#else
#define lv_LOCALTIME(time_ptr, tm_buf) localtime_r(time_ptr, tm_buf)
#endif
#endif

/* 函数废弃标记（跨编译器统一） */
#ifndef lv_DEPRECATED
#if defined(__GNUC__) || defined(__clang__)
#define lv_DEPRECATED(msg) __attribute__((deprecated(msg)))
#elif defined(_MSC_VER)
#define lv_DEPRECATED(msg) __declspec(deprecated(msg))
#else
#define lv_DEPRECATED(msg) /* 不支持废弃标记 */
#endif
#endif

/* ── 路径分隔符（跨平台） ── */
#ifdef _WIN32
#define lv_PATH_SEPARATOR_CHAR '\\'
#define lv_PATH_SEPARATOR_STR "\\"
#else
#define lv_PATH_SEPARATOR_CHAR '/'
#define lv_PATH_SEPARATOR_STR "/"
#endif
/* 向后兼容：旧宏名 lv_PATH_SEPARATOR 保留
 * （权威定义在 config.h，此处 include 以保证单一定义来源；若 config.h
 *   已先定义则下方 #ifndef 兜底不生效） */
#include "config.h"
#ifndef lv_PATH_SEPARATOR
#define lv_PATH_SEPARATOR lv_PATH_SEPARATOR_CHAR
#endif

/* ---- 版本信息（统一版本号 v3.3.0，所有模块引用此宏） ---- */
#define lv_VERSION_MAJOR 1
#define lv_VERSION_MINOR 1
#define lv_VERSION_PATCH 0
#define lv_VERSION_STRING_EXPAND(maj, min, pat) #maj "." #min "." #pat
#define lv_VERSION_STRING_MACRO(maj, min, pat) lv_VERSION_STRING_EXPAND(maj, min, pat)
#ifndef lv_VERSION_STRING
#define lv_VERSION_STRING lv_VERSION_STRING_MACRO(lv_VERSION_MAJOR, lv_VERSION_MINOR, lv_VERSION_PATCH)
#endif

/* 基础模块（必须在其他模块之前） */
#include "error_codes.h"   /* 统一错误码系统 */
#include "runtime_guard.h" /* 运行时安全守卫 */

/* 隔离上下文系统 —— 统一状态容器、分支推理与熔断机制 */
#include "context.h"

/* 核心模块 */
#include "constraint_graph.h" /* 约束图核心 */
#include "critical_pair.h"    /* 关键对计算引擎 */
#include "graph_hash.h"       /* 图结构哈希 */
#include "normalization.h"    /* 图规范化遍引擎 */
#include "rewrite.h"          /* 图重写引擎 */
#include "solver.h"           /* 符号代数求解器 */
#include "symbolic_coord.h"   /* 符号坐标系统 */
#include "unify.h"            /* 合一检查 */

/* 公理系统 */
#include "axiom_pkg.h" /* 公理系统包 */
#include "module.h"    /* 模块系统 */

/* 高级功能 */
#include "func_block.h"          /* 函数块系统 */
#include "func_block_preset.h"   /* 预设函数块库 */
#include "func_block_registry.h" /* 预设函数块注册系统 */

/* 模块化预设函数块系统 */
#include "modal_operators.h"        /* 模态逻辑算子 */
#include "preset_algebraic.h"       /* 代数运算模块 */
#include "preset_basic_geometry.h"  /* 基础几何构造模块 */
#include "preset_blocks.h"          /* 模块化预设函数块主系统 */
#include "preset_measurements.h"    /* 度量计算模块 */
#include "preset_polygons.h"        /* 多边形构造模块 */
#include "preset_transformations.h" /* 几何变换模块 */
#include "proof.h"                  /* 命题与证明系统 */
#include "proof_engine_enhanced.h"  /* 增强证明引擎 */
#include "quantifier.h"             /* 量词系统 */
#include "recursion.h"              /* 递归与条件 */
#include "three_valued_logic.h"     /* 三值逻辑系统 */
#include "type_equiv_explorer.h"    /* 类型等价探索器 */
#include "type_system.h"            /* 类型系统 */

/* 引擎 */
#include "engine.h" /* 主引擎 */
#include "magic.h"  /* Magic 模拟器模块 */

/* 调试 */
#include "debug.h" /* 调试工具 */

/* 工具函数库 */
#include "lv_utils.h" /* 通用工具函数 */

/* 流式输出 */
#include "stream.h"              /* 流式事件系统 */
#include "stream_context_util.h" /* 流式上下文工具宏 */

/* λ-项数据结构 */
#include "lambda_term.h"   /* λ-项（tagged union） */
#include "lambda_church.h" /* Church 编码公共 API */

/* ============================================================
 * === 版本信息 API ===
 * ============================================================ */

/**
 * @brief 获取编译期版本号（主版本号）
 * @return 主版本号（如 3）
 * @note   编译期常量，可在 #if 预处理指令中使用
 */
#define lv_version_major() lv_VERSION_MAJOR

/**
 * @brief 获取编译期版本号（次版本号）
 * @return 次版本号（如 3）
 * @note   编译期常量，可在 #if 预处理指令中使用
 */
#define lv_version_minor() lv_VERSION_MINOR

/**
 * @brief 获取编译期版本号（补丁版本号）
 * @return 补丁版本号（如 0）
 * @note   编译期常量，可在 #if 预处理指令中使用
 */
#define lv_version_patch() lv_VERSION_PATCH

/**
 * @brief 获取版本信息结构体
 *
 * 提供运行时可查询的版本信息，包含主版本号、次版本号、补丁版本号、
 * 版本字符串以及编译时的平台和编译器信息。
 */
typedef struct lvVersionInfo {
    int major;                  /**< 主版本号 */
    int minor;                  /**< 次版本号 */
    int patch;                  /**< 补丁版本号 */
    const char *version_string; /**< 完整版本字符串（如 "3.3.0"） */
    const char *platform;       /**< 编译平台名称 */
    const char *compiler;       /**< 编译器名称 */
    const char *arch;           /**< 目标架构 */
    const char *build_date;     /**< 构建日期（__DATE__） */
    const char *build_time;     /**< 构建时间（__TIME__） */
} lvVersionInfo;

/**
 * @brief 获取版本字符串（编译期常量）
 *
 * 返回编译期确定的版本字符串，零开销。
 * 格式为 "major.minor.patch"，例如 "3.3.0"。
 *
 * @return 版本字符串（静态常量，无需释放）
 *
 * @note   此函数始终可用，无需调用 lv_init()。
 *         等价于直接使用 lv_VERSION_STRING 宏。
 *
 * 示例:
 * @code
 *   printf("Lv-00 version: %s\n", lv_get_version_string());
 *   // 输出: Lv-00 version: 3.3.0
 * @endcode
 */
lv_PUBLIC_API const char *lv_get_version_string(void);

/**
 * @brief 获取详细版本信息（运行时）
 *
 * 填充 lvVersionInfo 结构体，包含版本号、平台、编译器、构建时间等详细信息。
 * 适用于日志记录、调试输出和系统信息报告。
 *
 * @param[out] info 指向 lvVersionInfo 结构体的指针（调用者分配）
 * @return true 成功，false 失败（info 为 NULL 时）
 *
 * @note   返回的字符串指针（version_string, platform 等）指向静态内存，无需释放。
 *
 * 示例:
 * @code
 *   lvVersionInfo info;
 *   if (lv_get_version_info(&info)) {
 *       printf("Lv-00 v%d.%d.%d (%s, %s, %s)\n",
 *              info.major, info.minor, info.patch,
 *              info.platform, info.compiler, info.arch);
 *   }
 * @endcode
 */
lv_PUBLIC_API bool lv_get_version_info(lvVersionInfo *info);

/**
 * @brief 检查运行时版本与编译时头文件版本的兼容性
 *
 * 如果运行时库的主版本号与编译时头文件的主版本号不匹配，返回 false。
 * 次版本号不同仅产生警告但不阻止运行（向后兼容的 API 添加）。
 *
 * @return true 兼容（主版本号匹配），false 不兼容
 *
 * @note   建议在 lv_init() 之后调用此函数验证版本兼容性。
 *
 * 示例:
 * @code
 *   lv_init();
 *   if (!lv_check_version_compat()) {
 *       fprintf(stderr, "Version mismatch: header v%d vs library v%d\n",
 *              lv_VERSION_MAJOR, lv_get_version_string());
 *   }
 * @endcode
 */
lv_PUBLIC_API bool lv_check_version_compat(void);

/* ============================================================
 * === 初始化与清理 ===
 * ============================================================ */

/**
 * @brief 初始化 Lv-00 系统
 *
 * 初始化所有子系统，包括内存管理、错误码系统、调试基础设施。
 * 必须在调用任何其他 Lv-00 API（除 lv_get_version_string() 外）之前调用。
 *
 * @return true 初始化成功，false 初始化失败
 *
 * @note   线程安全：应在主线程中调用，在创建任何工作线程之前完成初始化。
 * @note   可以多次调用 lv_init()，后续调用若系统已初始化则直接返回 true。
 * @note   每次成功的 lv_init() 必须对应一次 lv_cleanup() 调用。
 *
 * 示例:
 * @code
 *   if (!lv_init()) {
 *       fprintf(stderr, "Lv-00 initialization failed\n");
 *       return EXIT_FAILURE;
 *   }
 * @endcode
 */
lv_PUBLIC_API bool lv_init(void);

/**
 * @brief 清理 Lv-00 系统，释放所有资源
 *
 * 释放 Lv-00 分配的所有全局资源。
 * 调用后不应再使用任何 Lv-00 API（除非重新调用 lv_init()）。
 *
 * @note   必须先销毁所有引擎实例（lv_engine_destroy）再调用此函数。
 * @note   线程安全：应在所有工作线程结束后在主线程中调用。
 *
 * 示例:
 * @code
 *   lv_engine_destroy(engine);
 *   lv_cleanup();
 * @endcode
 */
lv_PUBLIC_API void lv_cleanup(void);

/**
 * @brief 获取系统信息字符串
 *
 * 返回包含版本、平台、编译器、内存使用等信息的格式化字符串。
 *
 * @param[out] buf  输出缓冲区（调用者分配）
 * @param[in]  size 缓冲区大小（字节），建议至少 1024
 * @return 实际写入的字符数（不含终止符），缓冲区不足时返回所需大小
 *
 * 示例:
 * @code
 *   char info[1024];
 *   lv_get_system_info(info, sizeof(info));
 *   printf("%s\n", info);
 * @endcode
 */
lv_PUBLIC_API int lv_get_system_info(char *buf, size_t size);

/**
 * @brief 检查系统健康状况
 *
 * 返回 0~100 的评分，反映系统的整体健康状态。
 * 评分依据: 内存碎片率、错误累积计数、子系统状态等。
 *
 * @return 健康评分（0~100），未初始化时返回 0
 */
lv_PUBLIC_API int lv_health_check(void);

/**
 * @brief 检查系统是否已初始化
 *
 * @return true 已初始化，false 未初始化
 *
 * @note   在调用除 lv_get_version_string() 外的任何 Lv-00 API 之前，
 *          建议先调用此函数检查状态。
 */
lv_PUBLIC_API bool lv_is_initialized(void);

/* ============================================================
 * === 引擎生命周期 ===
 * ============================================================ */

/**
 * @brief 创建并初始化引擎实例（便捷函数）
 *
 * 创建一个新的 lvEngine 实例。引擎是 Lv-00 的核心工作单元，
 * 持有约束图、符号坐标、重写规则、证明树等所有状态。
 *
 * @return 引擎指针，失败返回 NULL（可通过 lv_get_last_error() 获取错误详情）
 *
 * @note   调用者负责通过 lv_engine_destroy() 释放引擎。
 * @note   可以在同一进程中创建多个独立引擎实例（线程安全取决于具体使用方式）。
 *
 * 示例:
 * @code
 *   lvEngine *engine = lv_engine_create();
 *   if (!engine) {
 *       fprintf(stderr, "Engine creation failed: %s\n", lv_get_last_error());
 *       return EXIT_FAILURE;
 *   }
 * @endcode
 */
lv_PUBLIC_API lvEngine *lv_engine_create(void);

/**
 * @brief 销毁引擎实例（便捷函数）
 *
 * 释放引擎及其所有关联资源（约束图、坐标、重写规则、证明树等）。
 *
 * @param[in] engine 引擎指针（可为 NULL，此时函数无操作）
 *
 * @note   销毁引擎后，所有从该引擎获取的指针（节点 ID、坐标引用等）均失效。
 *
 * 示例:
 * @code
 *   lv_engine_destroy(engine);
 *   engine = NULL;  // 防止悬空指针
 * @endcode
 */
lv_PUBLIC_API void lv_engine_destroy(lvEngine *engine);

/* ============================================================
 * === 几何构造便捷 API ===
 * ============================================================ */

/**
 * @brief 快速创建一个有理数坐标的点（便捷函数）
 *
 * 在引擎的约束图中创建一个新点节点，坐标为精确有理数 (x_num/x_den, y_num/y_den)。
 * 使用有理数而非浮点数确保几何计算的精确性。
 *
 * @param[in,out] engine 引擎实例
 * @param[in]     x_num  X 坐标分子（可为负数）
 * @param[in]     x_den  X 坐标分母（必须 > 0）
 * @param[in]     y_num  Y 坐标分子（可为负数）
 * @param[in]     y_den  Y 坐标分母（必须 > 0）
 * @return 新节点的 ID（>= 0），失败返回 -1
 *
 * @note   分母为 0 时函数将失败并设置错误码 lv_ERROR_INVALID_PARAM。
 *
 * 示例:
 * @code
 *   // 创建点 (0, 0)
 *   int origin = lv_add_point(engine, 0, 1, 0, 1);
 *   // 创建点 (1.5, 2.75) = (3/2, 11/4)
 *   int p = lv_add_point(engine, 3, 2, 11, 4);
 * @endcode
 */
lv_PUBLIC_API int lv_add_point(lvEngine *engine, int64_t x_num, uint64_t x_den, int64_t y_num, uint64_t y_den);

/**
 * @brief 添加整数坐标点（便捷函数）
 * @param engine 引擎实例
 * @param x x 坐标（整数）
 * @param y y 坐标（整数）
 * @return 新节点 ID，失败返回 -1
 *
 * @note 等价于 lv_add_point(engine, x, 1, y, 1)
 */
static inline int lv_add_point_i(lvEngine *engine, long long x, long long y) {
    return lv_add_point(engine, x, 1, y, 1);
}

/**
 * @brief 快速创建线段（便捷函数）
 *
 * 在引擎的约束图中创建一条连接两个已有点的线段。
 *
 * @param[in,out] engine      引擎实例
 * @param[in]     point1_id   端点1节点ID
 * @param[in]     point2_id   端点2节点ID
 * @return 新线段的节点 ID（>= 0），失败返回 -1
 *
 * 示例:
 * @code
 *   int segment = lv_add_line_segment(engine, p1, p2);
 * @endcode
 */
lv_PUBLIC_API int lv_add_line_segment(lvEngine *engine, int point1_id, int point2_id);

/**
 * @brief 快速添加关联约束（便捷函数）
 *
 * 声明一个点属于某条线段或区域（关联约束）。
 *
 * @param[in,out] engine    引擎实例
 * @param[in]     point_id  点节点ID
 * @param[in]     line_id   线段/区域节点ID
 * @return true 成功，false 失败
 *
 * 示例:
 * @code
 *   // 声明点 p 在线段 L 上
 *   lv_add_constraint_incidence(engine, p, L);
 * @endcode
 */
lv_PUBLIC_API bool lv_add_constraint_incidence(lvEngine *engine, int point_id, int line_id);

/* ============================================================
 * === 推理与求解 ===
 * ============================================================ */

/**
 * @brief 执行图归一化（便捷函数）
 *
 * 对引擎中的约束图执行 Weisfeiler-Lehman 图核迭代归一化，
 * 合并同构图结构，简化后续推理。
 *
 * @param[in,out] engine       引擎实例
 * @param[in]     scope_aware  是否考虑命名空间范围
 * @return 归一化结果（调用者负责通过 normalization_result_destroy 释放），失败返回 NULL
 *
 * 示例:
 * @code
 *   NormalizationResult *nr = lv_normalize(engine, true);
 *   if (nr) {
 *       printf("Graph normalized: %d iterations\n", nr->iterations);
 *       normalization_result_destroy(nr);
 *   }
 * @endcode
 */
lv_PUBLIC_API NormalizationResult *lv_normalize(lvEngine *engine, bool scope_aware);

/**
 * @brief 执行求解流水线（便捷函数）
 *
 * 运行完整的求解流水线：归一化 → 重写 → 约束求解 → 验证。
 * 这是 Lv-00 的核心推理入口。
 *
 * @param[in,out] engine 引擎实例
 * @return 求解结果状态码（lv_SOLVE_SUCCESS 等）
 *
 * @note   求解可能耗时较长，对于复杂问题建议设置超时（通过配置 API）。
 *
 * 示例:
 * @code
 *   EngineSolveResult result = lv_solve(engine);
 *   switch (result) {
 *       case lv_SOLVE_SUCCESS:
 *           printf("Solved successfully\n");
 *           break;
 *       case lv_SOLVE_TIMEOUT:
 *           printf("Solver timed out\n");
 *           break;
 *       default:
 *           printf("Solver failed: %s\n", lv_get_last_error());
 *   }
 * @endcode
 */
lv_PUBLIC_API EngineSolveResult lv_solve(lvEngine *engine);

/**
 * @brief 将节点永久降级为数值假设（SetNumericAssumption 命令）
 *
 * 当位数熔断触发且用户选择"永久降级"时调用此函数。
 * 将节点的信任颜色设为 TRUST_AMBER，存储精度阈值和声明文本。
 * 所有下游依赖节点自动继承 TRUST_AMBER。
 *
 * @param engine     引擎实例
 * @param node_id    要降级的节点 ID
 * @param precision  数值精度阈值（如 1e-15）
 * @param declaration 数值假设声明文本（如"该点坐标在10^{-15}精度下近似为1.4142"）
 * @return 成功返回 0（lv_OK），失败返回负错误码
 */
lv_PUBLIC_API int lv_set_numeric_assumption(lvEngine *engine, int node_id, double precision, const char *declaration);

/* ============================================================
 * === 配置管理 ===
 * ============================================================ */

/*
 * 配置系统说明（三套收敛为两套，系统 C 已消除）：
 *   - 系统 A（lvConfig）：config.h / lv_config.c。X-macro 注册表键
 *     （LV_CONFIG_INT_KEYS / LV_CONFIG_DOUBLE_KEYS）+ 类型安全
 *     lv_config_get_<key>() / lv_config_set_<key>()，JSON 持久化；
 *   - 系统 B（ConfigManager）：lv_utils_config.c。字符串键存储，
 *     公共 API 即本节的 lv_config_get_*(key, default) 等。
 *
 * 本节的 lv_config_get_int/bool/double/string(key, default) 与
 * lv_config_set_bool/set_string 为系统 B 的公共 API；lv_config_set_int /
 * lv_config_set_double 为系统 A 的通用 setter（与 config.h 中同一符号，
 * 定义于 lv_config.c），仅接受 A 注册表内的键，未知键返回 false。
 *
 * config.h 的 LV_CFG_* 字符串键宏不再构成独立配置系统：与 A 同名的键
 * 经 lv.c 的统一分发读取 lvConfig 单例，其余键归属 B 的键空间。
 */

/**
 * @brief 获取整数配置值（便捷函数）
 *
 * @param[in] key         配置键名（如 "rewrite.step_limit"）
 * @param[in] default_val 默认值（当键不存在时返回）
 * @return 配置值
 *
 * 示例:
 * @code
 *   int limit = lv_config_get_int("rewrite.step_limit", 1000);
 * @endcode
 */
lv_PUBLIC_API int lv_config_get_int(const char *key, int default_val);

/**
 * @brief 获取布尔配置值（便捷函数）
 *
 * @param[in] key         配置键名
 * @param[in] default_val 默认值
 * @return 配置值
 */
lv_PUBLIC_API bool lv_config_get_bool(const char *key, bool default_val);

/**
 * @brief 获取浮点配置值（便捷函数）
 *
 * @param[in] key         配置键名
 * @param[in] default_val 默认值
 * @return 配置值
 */
lv_PUBLIC_API double lv_config_get_double(const char *key, double default_val);

/**
 * @brief 获取字符串配置值（便捷函数）
 *
 * @param[in] key         配置键名
 * @param[in] default_val 默认值
 * @return 配置值（指向内部存储的指针，勿释放）
 */
lv_PUBLIC_API const char *lv_config_get_string(const char *key, const char *default_val);

/**
 * @brief 设置整数配置值
 *
 * 系统 A 通用 setter（与 config.h 声明为同一符号，定义于 lv_config.c）：
 * 仅接受 lvConfig 注册表（LV_CONFIG_INT_KEYS）内的键，未知键返回 false。
 *
 * @param[in] key   配置键名
 * @param[in] value 配置值
 * @return true 成功，false 失败（键名无效或值超出范围）
 */
lv_PUBLIC_API bool lv_config_set_int(const char *key, int value);

/**
 * @brief 设置布尔配置值
 *
 * @param[in] key   配置键名
 * @param[in] value 配置值
 * @return true 成功，false 失败
 */
lv_PUBLIC_API bool lv_config_set_bool(const char *key, bool value);

/**
 * @brief 设置浮点配置值
 *
 * 系统 A 通用 setter（与 config.h 声明为同一符号，定义于 lv_config.c）：
 * 仅接受 lvConfig 注册表（LV_CONFIG_DOUBLE_KEYS）内的键，未知键返回 false。
 *
 * @param[in] key   配置键名
 * @param[in] value 配置值
 * @return true 成功，false 失败
 */
lv_PUBLIC_API bool lv_config_set_double(const char *key, double value);

/**
 * @brief 设置字符串配置值
 *
 * @param[in] key   配置键名
 * @param[in] value 配置值（内部会复制该字符串）
 * @return true 成功，false 失败
 */
lv_PUBLIC_API bool lv_config_set_string(const char *key, const char *value);

/* ============================================================
 * === 内存管理 ===
 * ============================================================ */

/**
 * @brief 获取当前内存使用统计
 *
 * 获取 Lv-00 内存管理器的统计信息，包括当前分配量、峰值、
 * 分配次数和释放次数等。
 *
 * @param[out] stats 输出统计信息结构体（调用者分配）
 * @return true 成功，false 失败（stats 为 NULL 或系统未初始化）
 *
 * 示例:
 * @code
 *   MemoryStats stats;
 *   if (lv_get_memory_stats_ex(&stats)) {
 *       printf("Memory: %zu bytes current, %zu peak\n",
 *              stats.current_bytes, stats.peak_bytes);
 *   }
 * @endcode
 */
lv_PUBLIC_API bool lv_get_memory_stats_ex(MemoryStats *stats);

/**
 * @brief 设置内存使用上限
 *
 * 限制 Lv-00 可分配的总内存。达到上限后，新的分配请求将失败
 * 并返回 NULL，同时设置错误码 lv_ERROR_OUT_OF_MEMORY。
 *
 * @param[in] limit_bytes 内存上限（字节），0 表示无限制
 *
 * @note   该限制是软限制，仅在通过 lv_malloc/lv_calloc 分配时生效。
 *          外部通过标准 malloc 分配的内存不计入此限制。
 */
lv_PUBLIC_API void lv_set_memory_limit_ex(size_t limit_bytes);

/**
 * @brief 获取当前内存使用上限
 *
 * @return 内存上限（字节），0 表示无限制
 */
lv_PUBLIC_API size_t lv_get_memory_limit_ex(void);

/* ============================================================
 * === 调试和日志 ===
 * ============================================================ */

/**
 * @brief 设置日志级别
 *
 * 控制 Lv-00 内部日志的输出详细程度。
 *
 * @param[in] level 日志级别:
 *                   0 = lv_LOG_OFF    (禁用所有日志)
 *                   1 = lv_LOG_ERROR  (仅错误)
 *                   2 = lv_LOG_WARN   (错误 + 警告)
 *                   3 = lv_LOG_INFO   (错误 + 警告 + 信息)
 *                   4 = lv_LOG_DEBUG  (所有日志，含调试信息)
 *
 * 示例:
 * @code
 *   lv_set_log_level(3);  // 启用 INFO 级别日志
 * @endcode
 */
lv_PUBLIC_API void lv_set_log_level(int level);

/**
 * @brief 获取当前日志级别
 *
 * @return 当前日志级别（0~4）
 */
lv_PUBLIC_API int lv_get_log_level(void);

/**
 * @brief 启用/禁用断言
 *
 * 控制 Lv-00 内部的运行时断言检查。
 * 禁用断言可略微提升性能，但会降低错误检测能力。
 *
 * @param[in] enabled true 启用断言，false 禁用
 *
 * @note   发布构建中建议禁用断言以获得最佳性能。
 */
lv_PUBLIC_API void lv_set_assertions_enabled(bool enabled);

/**
 * @brief 检查断言是否启用
 *
 * @return true 启用，false 禁用
 */
lv_PUBLIC_API bool lv_are_assertions_enabled(void);

/* ============================================================
 * === 编译期版本兼容性检查 ===
 *
 * 确保 lv.h 中的版本宏与 CMakeLists.txt 的 project(VERSION ...)
 * 保持一致。版本不匹配时触发编译错误，防止 API 兼容性问题。
 * ============================================================ */
#if lv_VERSION_MAJOR != 1 || lv_VERSION_MINOR != 1 || lv_VERSION_PATCH != 0
#error \
    "[Lv-00] 版本宏不匹配：lv.h 中 lv_VERSION_MAJOR/MINOR/PATCH 与 CMakeLists.txt 的 project(VERSION ...) 不一致，请同步后重新编译。"
#endif

#ifdef __cplusplus
}
#endif

#endif /* lv_lv_H */
