/**
 * @file axiom_pkg.h
 * @brief 公理包系统 —— 约束模板、不可构造性记录、依赖引用追踪
 *
 * 提供公理包的创建/销毁、序列化/反序列化、约束模板注册与验证、
 * 已知不可构造问题管理、双层测试集、模板展开缓存以及依赖引用追踪等功能。
 */

#ifndef lv_AXIOM_PKG_H
#define lv_AXIOM_PKG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "constraint_graph.h"
#include "lv_utils.h"
#include "stream.h"
#include "symbolic_coord.h"
#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */

/**
 * @brief 命题类型枚举 —— 标识公理包中命题的类型
 * 【枚举值命名规范】所有枚举值使用 UPPER_SNAKE_CASE
 */
typedef enum {
    PROPOSITION_KIND_CONSTRUCTIVE,            /**< 构造性命题：可直接构造证明 */
    PROPOSITION_KIND_NON_CONSTRUCTIVE_ORACLE, /**< 非构造性预言：需要外部Oracle */
    PROPOSITION_KIND_EXPLOSION_PRINCIPLE      /**< 爆炸原理：反证法适用 */
} PropositionKind;

typedef struct AxiomPackage AxiomPackage;

typedef struct KnownUnconstructible {
    char *name;
    char *reduces_to;        /* 目标问题名称 */
    lvDArray dependency_chain; /* 依赖项数组（lvDArray<char*>） */
    char *external_ref;      /* 外部引用URL或标识符 */
    bool green_verified;     /* 是否已验证 */
} KnownUnconstructible;

lv_PUBLIC_API AxiomPackage *lv_axiom_package_create(const char *name, const char *version);
lv_PUBLIC_API void axiom_package_destroy(AxiomPackage *pkg);

/* 流式上下文设置（由 lv_DECLARE_STREAM_CTX(axiom) 宏生成） */
lv_PUBLIC_API void axiom_set_stream_context(StreamContext *ctx);

lv_PUBLIC_API bool axiom_package_add_known_unconstructible(AxiomPackage *pkg, KnownUnconstructible *item);
lv_PUBLIC_API KnownUnconstructible *axiom_package_lookup_unconstructible(AxiomPackage *pkg, const char *name);

/**
 * @brief 不可构造性证明模板
 *
 * 一个几何构造，其输入是目标问题，其输出是一个已知不可构造问题的构造。
 * 用户在模板内完成归约构造，通过合一检查后，蓝色虚框转为已证不可构造。
 */
typedef struct {
    char *target_problem_name;                // 目标问题名称（如"三等分角"）
    char *known_unconstructible_name;         // 已知不可构造问题名称（如"倍立方"）
    ConstraintGraph *reduction_construction;  // 归约构造图
    bool verified;                            // 是否通过合一检查
    char *description;                        // 归约方法描述
} UnconstructibleTemplate;

/**
 * @brief 创建不可构造性证明模板
 *
 * @param pkg 公理包
 * @param target_name 目标问题名称
 * @param known_name 已知不可构造问题名称
 * @param construction 归约构造图（接过所有权）
 * @param description 归约描述
 * @return 0 成功
 */
lv_PUBLIC_API int axiom_package_add_unconstructible_template(AxiomPackage *pkg, const char *target_name,
                                                             const char *known_name, ConstraintGraph *construction,
                                                             const char *description);

/**
 * @brief 查找匹配的不可构造性证明模板
 *
 * @param pkg 公理包
 * @param target_name 目标问题名称
 * @return UnconstructibleTemplate* 匹配的模板，NULL 无匹配
 */
lv_PUBLIC_API UnconstructibleTemplate *axiom_package_lookup_unconstructible_template(AxiomPackage *pkg,
                                                                                     const char *target_name);

/**
 * @brief 执行不可构造性验证
 *
 * 尝试用可用模板将目标问题归约到已知不可构造问题。
 * 如果成功，将目标问题的 trust color 改为相应的颜色（绿/黄）。
 *
 * @param graph 约束图
 * @param target_node_id 目标问题节点 ID
 * @param pkg 公理包
 * @return true 验证通过
 */
lv_PUBLIC_API bool axiom_package_verify_unconstructible(ConstraintGraph *graph, int target_node_id, AxiomPackage *pkg);

typedef enum {
    AXIOM_LOAD_OK,
    AXIOM_LOAD_FILE_NOT_FOUND,
    AXIOM_LOAD_PARSE_ERROR,
    AXIOM_LOAD_CIRCULAR_DEPENDENCY,
    AXIOM_LOAD_DEPTH_EXCEEDED,
    AXIOM_LOAD_VALIDATION_ERROR,
    AXIOM_LOAD_NULL_POINTER, /* 空指针参数 */
    AXIOM_LOAD_MEMORY_ERROR  /* 内存分配失败 */
} AxiomLoadStatus;

typedef enum { AXIOM_SAVE_OK, AXIOM_SAVE_FILE_ERROR, AXIOM_SAVE_WRITE_ERROR } AxiomSaveStatus;

/* 获取最后一次加载错误的详细信息 */
lv_PUBLIC_API const char *axiom_package_get_last_error(void);

lv_PUBLIC_API AxiomLoadStatus axiom_package_load(AxiomPackage *pkg, const char *filepath);
lv_PUBLIC_API AxiomSaveStatus axiom_package_save(const AxiomPackage *pkg, const char *filepath);

/**
 * @brief 计算公理包的内容哈希
 *
 * 将公理包的名称、版本、模板、已知不可构造问题等信息
 * 计算 SHA-256 哈希值，生成 64 字符的十六进制字符串。
 * 用于检测公理包内容是否发生变化。
 *
 * @param[in] pkg  要计算哈希的公理包
 * @return 新分配的 65 字符十六进制字符串（[take] 调用者负责 lv_free）。
 *         如果 pkg 为 NULL 或内存分配失败，返回 NULL。
 */
lv_PUBLIC_API char *axiom_package_compute_content_hash(AxiomPackage *pkg);
lv_PUBLIC_API bool axiom_package_validate_dependencies(AxiomPackage *pkg, AxiomPackage **loaded_packages,
                                                       int package_count);

/* ============== ConstraintTemplate 增强 ============== */

/* 参数类型
 * 【枚举值命名规范】所有枚举值使用 UPPER_SNAKE_CASE
 */
typedef enum {
    TEMPLATE_PARAM_TYPE_POINT,        /**< 点参数 */
    TEMPLATE_PARAM_TYPE_LINE_SEGMENT, /**< 线段参数 */
    TEMPLATE_PARAM_TYPE_REGION,       /**< 区域参数 */
    TEMPLATE_PARAM_TYPE_SCALAR        /**< 标量参数 */
} TemplateParamType;

typedef struct {
    TemplateParamType type;
    char name[64];
} TemplateParam;

/**
 * @brief 模板级别
 */
typedef enum {
    TEMPLATE_LEVEL_ONE, /**< 一级模板（原子度量，通过测试集验证） */
    TEMPLATE_LEVEL_TWO  /**< 二级模板（用户复合体） */
} TemplateLevel;

/* 正则形式描述 */
typedef struct {
    int expected_node_types[8];       /* 期望的节点类型序列 */
    int expected_constraint_types[8]; /* 期望的约束类型序列 */
    int node_type_count;
    int constraint_type_count;
} NormalFormDesc;

typedef struct ConstraintTemplate {
    char *name;
    int param_count;
    void (*expand)(SymbolicCoord **params, ConstraintGraph *target);
    bool verified;

    /* 增强字段 */
    TemplateParam *params;      /* 参数描述数组 */
    int param_desc_count;       /* 参数描述数量 */
    NormalFormDesc normal_form; /* 正则形式描述 */

    /* v3.6.0: 模板分级管理与惰性展开 */
    TemplateLevel level;                  /**< 模板级别（一级/二级） */
    bool is_compressed;                   /**< 当前是否处于压缩态 */
    ConstraintGraph *compressed_subgraph; /**< 压缩态的内部子图（二级模板用） */
} ConstraintTemplate;

lv_PUBLIC_API bool axiom_package_register_template(AxiomPackage *pkg, ConstraintTemplate *tmpl);
lv_PUBLIC_API ConstraintTemplate *axiom_package_get_template(AxiomPackage *pkg, const char *name);

/* ============== 模板分级管理与惰性展开 ============== */

/**
 * @brief 设置模板级别
 *
 * @param tmpl  模板指针
 * @param level 模板级别
 */
lv_PUBLIC_API void axiom_template_set_level(ConstraintTemplate *tmpl, TemplateLevel level);

/**
 * @brief 惰性展开模板
 *
 * 一级模板：直接调用 expand() 展开。
 * 二级模板：如果处于压缩态，先获取内部子图，再展开。
 * 展开后标记为非压缩态。
 *
 * @param pkg           公理包
 * @param template_name 模板名称
 * @param params        参数
 * @param param_count   参数数量
 * @return 展开后的约束图（缓存中查找或新展开）
 */
/** 获取可构造模板数量 */
lv_PUBLIC_API int axiom_package_get_template_count(const AxiomPackage *pkg);
/** 按索引获取可构造模板 */
lv_PUBLIC_API const ConstraintTemplate *axiom_package_get_template_by_index(const AxiomPackage *pkg, int index);
/** 获取不可构造问题数量 */
lv_PUBLIC_API int axiom_package_get_unconstructible_count(const AxiomPackage *pkg);
/** 按索引获取不可构造问题 */
lv_PUBLIC_API const KnownUnconstructible *axiom_package_get_unconstructible(const AxiomPackage *pkg, int index);

lv_PUBLIC_API ConstraintGraph *axiom_template_expand_lazy(AxiomPackage *pkg, const char *template_name,
                                                          SymbolicCoord **params, int param_count);

/**
 * @brief 将模板重新压缩为压缩态
 *
 * 释放展开后的约束图，恢复到压缩态，节省内存。
 *
 * @param tmpl 模板
 */
lv_PUBLIC_API void axiom_template_compress(ConstraintTemplate *tmpl);

lv_PUBLIC_API bool axiom_template_validate_normal_form(const ConstraintTemplate *tmpl,
                                                       const ConstraintGraph *expanded_graph,
                                                       const char *canonical_form);

/* ============== 双层测试集 ============== */

/**
 * @brief 模板测试用例类型
 */
typedef enum {
    TEST_CASE_FACTORY, /**< 出厂测试（内核开发者编写） */
    TEST_CASE_USER     /**< 用户测试（公理包作者添加） */
} TestCaseType;

/**
 * @brief 模板测试用例
 */
typedef struct {
    char *template_name;                    /**< 模板名称 */
    TestCaseType type;                      /**< 测试类型 */
    int param_count;                        /**< 参数数量 */
    SymbolicCoord **params;                 /**< 参数值 */
    struct ConstraintGraph *expected_graph; /**< 预期的展开后图结构（仅用于正则形式验证） */
    bool expected_result;                   /**< 期望结果（true=通过，false=失败） */
    char *description;                      /**< 测试描述 */
} TemplateTestCase;

/**
 * @brief 烟测用例结果枚举
 */
typedef enum {
    TEST_RESULT_PASSED,  /**< 通过 */
    TEST_RESULT_FAILED,  /**< 失败 */
    TEST_RESULT_TIMEOUT, /**< 超时（步骤数超限） */
    TEST_RESULT_SKIPPED, /**< 未运行（总时间超限） */
    TEST_RESULT_ERROR    /**< 执行错误 */
} TestCaseResult;

/**
 * @brief 烟测用例详细记录
 */
typedef struct {
    char *test_name;       /**< 测试用例名称 */
    TestCaseResult result; /**< 结果 */
    char *message;         /**< 失败/超时原因描述 */
} TemplateTestRecord;

/**
 * @brief 模板测试结果
 */
typedef struct {
    int total;
    int passed;
    int failed;
    char **failure_messages;
    int timed_out;               /**< 超时数量 */
    int skipped;                 /**< 未运行数量 */
    TemplateTestRecord *records; /**< 详细记录数组 */
    int record_count;            /**< 记录数量 */
} TemplateTestResult;

/**
 * @brief 运行模板的测试集
 *
 * @param pkg           公理包
 * @param template_name 模板名称
 * @param factory_tests 工厂测试用例数组
 * @param factory_count 工厂测试用例数量
 * @param user_tests    用户测试用例数组
 * @param user_count    用户测试用例数量
 * @return 测试结果
 */
lv_PUBLIC_API TemplateTestResult axiom_template_run_tests(AxiomPackage *pkg, const char *template_name,
                                                          TemplateTestCase *factory_tests, int factory_count,
                                                          TemplateTestCase *user_tests, int user_count);

/**
 * @brief 释放测试结果
 */
lv_PUBLIC_API void axiom_template_test_result_destroy(TemplateTestResult *result);

/**
 * @brief 模板测试执行器
 *
 * 运行一组测试用例，比对实际结果与预期结果。
 * 测试执行器是纯比对函数，不执行任何推理。
 *
 * @param pkg 公理包
 * @param test_cases 测试用例数组
 * @param count 测试用例数量
 * @param out_passed 输出：通过的测试数
 * @param out_failed 输出：失败的测试数
 * @param out_failures 输出：失败的测试名称数组（调用者需 free）
 * @return int 失败数量，<0 表示错误
 */
lv_PUBLIC_API int axiom_template_test_run(AxiomPackage *pkg, TemplateTestCase **test_cases, int count, int *out_passed,
                                          int *out_failed, char ***out_failures);

/**
 * @brief 模板正则形式验证
 *
 * 验证模板展开后的图结构是否符合该模板的正则形式规范。
 * 正则形式由约束图的结构定义（节点类型、约束类型组合）。
 *
 * @param pkg 公理包
 * @param template_name 模板名称
 * @return true 通过正则形式验证
 */
lv_PUBLIC_API bool axiom_template_verify_normal_form(AxiomPackage *pkg, const char *template_name);

/**
 * @brief 创建测试用例
 *
 * @param name 测试用例名称
 * @param type 测试类型（TEST_CASE_FACTORY 或 TEST_CASE_USER）
 * @param param_count 参数数量
 * @param expected 期望结果
 * @return 新创建的 TemplateTestCase，失败返回 NULL
 */
lv_PUBLIC_API TemplateTestCase *axiom_template_test_case_create(const char *name, TestCaseType type, int param_count,
                                                                bool expected);

/**
 * @brief 销毁测试用例
 *
 * @param tc 要销毁的测试用例（可为 NULL）
 */
lv_PUBLIC_API void axiom_template_test_case_destroy(TemplateTestCase *tc);

/**
 * @brief 深拷贝测试用例（[copy] 语义，memory-ownership.md K10/F39）
 *
 * 完整拷贝 template_name、description、params 和 expected_graph。
 * [copy] 返回的拷贝由调用者负责销毁（axiom_template_test_case_destroy）。
 *
 * @param src 源测试用例
 * @return 深拷贝副本，失败返回 NULL
 */
lv_PUBLIC_API TemplateTestCase *axiom_template_test_case_copy(const TemplateTestCase *src);

/* ============== 模板展开缓存 ============== */

/**
 * @brief 模板展开缓存条目
 */
typedef struct {
    uint64_t param_hash;
    char *template_name; /* 关联的模板名称 */
    ConstraintGraph *expanded_graph;
} TemplateExpansionCache;

/**
 * @brief 在缓存中查找匹配的展开图
 */
lv_PUBLIC_API ConstraintGraph *axiom_package_lookup_expansion_cache(AxiomPackage *pkg, const char *template_name,
                                                                    SymbolicCoord **params, int param_count);

/**
 * @brief 将展开结果存入缓存
 */
lv_PUBLIC_API bool axiom_package_store_expansion_cache(AxiomPackage *pkg, const char *template_name,
                                                       SymbolicCoord **params, int param_count,
                                                       ConstraintGraph *expanded_graph);

/**
 * @brief 清空模板展开缓存
 */
lv_PUBLIC_API void axiom_package_clear_expansion_cache(AxiomPackage *pkg);

/* ============== 依赖引用追踪 ============== */

/* 信任颜色常量（用于 original_color 字段） */
#define DEP_TRUST_GREEN 0
#define DEP_TRUST_BLUE_UNEXPLORED 1
#define DEP_TRUST_BLUE_EXCEEDED 2
#define DEP_TRUST_BLUE_OUT_OF_SCOPE 3
#define DEP_TRUST_YELLOW 4
#define DEP_TRUST_LIGHT_ORANGE_ORACLE 5
#define DEP_TRUST_LIGHT_ORANGE_EXPLOSION 6
#define DEP_TRUST_AMBER 7
#define DEP_TRUST_DEEP_ORANGE 8
#define DEP_TRUST_RED 9

/* 引用类型枚举 */
typedef enum {
    REF_INTERNAL, /**< 内引用（内容哈希验证） */
    REF_EXTERNAL, /**< 外引用（公认文献，永久有效） */
    REF_AUTHOR    /**< 作者断言（无形式化支撑，基础即为黄色） */
} RefType;

/* 追踪一个依赖引用（内部或外部） */
typedef struct {
    char ref_id[64];       /* 引用标识符 */
    char content_hash[65]; /* 引用内容的 SHA-256 哈希（十六进制字符串）
                               * 注意：调用者须确保传入的哈希字符串长度恰好为 64 字符
                               *（不含终止符），超出部分将被截断，不足则未定义行为。 */
    int dependent_node_id; /* 依赖此引用的节点 ID */
    int original_color;    /* 原始信任颜色 (0=GREEN 等) */
    /* === v3.5.0 新增字段：不可构造性证明依赖链 === */
    RefType ref_type;        /**< 引用类型 */
    char external_ref[256];  /**< 外部引用字符串（外引用用） */
    char trust_comment[256]; /**< 信任注释（外引用用，如"截至 2025 年公认有效"） */
    bool hash_valid;         /**< 内容哈希是否有效（内引用用） */
} DependencyRef;

/* ============== AxiomPackage 结构体 ============== */

struct AxiomPackage {
    char *name;
    char *version;
    lvDArray templates;                   /* lvDArray<ConstraintTemplate> */
    lvDArray known_unconstructibles;      /* lvDArray<KnownUnconstructible> */
    lvDArray unconstructible_templates;   /* lvDArray<UnconstructibleTemplate> */
    char *bottom_geometry;                /* 底层几何类型 */
    char *negation_encoding;              /* 否定编码方法 */
    int contradiction_behavior;           /* 矛盾行为 */

    /* 模板展开缓存 */
    lvDArray expansion_cache;             /* lvDArray<TemplateExpansionCache> */
    int max_expansion_depth;              /* 默认 8 */

    /* 依赖引用追踪 */
    lvDArray dep_refs;                    /* lvDArray<DependencyRef> */
};

/* ============== 依赖引用管理 ============== */

/**
 * @brief 注册一个依赖引用到公理包
 *
 * 追踪一个依赖引用（内部或外部）的存在。
 * 当公理包升级后，可以通过内容哈希验证引用是否失效。
 *
 * @param[in] pkg              公理包
 * @param[in] ref_id           引用标识符
 * @param[in] content_hash     引用内容的 SHA-256 哈希（64字符十六进制字符串）
 * @param[in] dependent_node_id 依赖此引用的节点 ID
 * @return 0 成功，-1 参数无效，-2 内存分配失败
 */
lv_PUBLIC_API int axiom_package_register_dependency_ref(AxiomPackage *pkg, const char *ref_id, const char *content_hash,
                                                        int dependent_node_id);

/**
 * @brief 验证所有依赖引用，返回失效的引用
 *
 * 重新计算每个依赖引用的内容哈希并与存储的哈希比较。
 * 哈希不匹配的引用被视为失效。
 *
 * @param[in]  pkg              公理包
 * @param[out] invalidated_refs  输出失效引用数组（调用者需释放）
 * @param[out] invalidated_count 输出失效引用数量
 * @return 失效引用数量，-1 表示参数无效
 */
lv_PUBLIC_API int axiom_package_validate_dependencies_with_hashes(AxiomPackage *pkg, DependencyRef **invalidated_refs,
                                                                  int *invalidated_count);

/**
 * @brief 执行失效依赖的自动降级
 *
 * 1. 调用 axiom_package_validate_dependencies_with_hashes() 查找失效引用
 * 2. 对每个失效引用，在约束图中找到依赖节点
 * 3. 将节点的信任颜色从 GREEN 降级为 YELLOW (AMBER)
 * 4. 记录警告日志
 *
 * @param[in] pkg   公理包
 * @param[in] graph 约束图（用于更新节点颜色）
 * @return 被降级的节点数量
 */
lv_PUBLIC_API int axiom_package_auto_degrade_invalidated(AxiomPackage *pkg, ConstraintGraph *graph);

/* ============== 不可构造性证明依赖链引用 ============== */

/**
 * @brief 创建内引用
 *
 * 指向公理包内的一个引理块，通过内容哈希验证。
 * 内容哈希基于约束图的规范表示（Canonical Graph Representation）计算。
 *
 * @param[in] pkg              公理包
 * @param[in] lemma_block_id   引理块 ID
 * @param[in] dependent_node_id 依赖此引理的节点 ID
 * @return 0 成功，-1 参数无效，-2 内存分配失败
 */
lv_PUBLIC_API int axiom_package_add_internal_ref(AxiomPackage *pkg, int lemma_block_id, int dependent_node_id);

/**
 * @brief 创建外引用
 *
 * 指向公认文献或形式化证明。
 * 外引用被视为永久有效的形式化支撑，不参与自动重验。
 *
 * @param[in] pkg              公理包
 * @param[in] ref_string       规范引用字符串（如 "Wantzel 1837"）
 * @param[in] dependent_node_id 依赖此引用的节点 ID
 * @param[in] trust_comment    信任注释（可选，如 "截至 2025 年公认有效"）
 * @return 0 成功，-1 参数无效，-2 内存分配失败
 */
lv_PUBLIC_API int axiom_package_add_external_ref(AxiomPackage *pkg, const char *ref_string, int dependent_node_id,
                                                 const char *trust_comment);

/**
 * @brief 创建作者断言引用
 *
 * 无形式化支撑，由公理包作者声明。
 * 系统立即识别为黄色基础（TRUST_YELLOW）。
 *
 * @param[in] pkg              公理包
 * @param[in] dependent_node_id 依赖此断言的节点 ID
 * @return 0 成功，-1 参数无效，-2 内存分配失败
 */
lv_PUBLIC_API int axiom_package_add_author_assertion(AxiomPackage *pkg, int dependent_node_id);

/* ============== 引理自动重验循环 ============== */

/**
 * @brief 引理自动重验循环
 *
 * 遍历公理包中的所有依赖引用，对每个 REF_INTERNAL 类型的引用：
 * 1. 计算当前内容哈希
 * 2. 与存储的哈希对比
 * 3. 如果哈希不匹配，标记为"遗留"并记录
 * 4. 如果哈希匹配，保留绿色状态
 *
 * @param[in]  pkg              公理包
 * @param[out] out_stale        输出：过期的引理块数量（可为 NULL）
 * @param[out] out_stale_names  输出：过期引理块名称数组（调用者需 free，可为 NULL）
 * @return 处理的总引用数
 */
lv_PUBLIC_API int axiom_package_reverify_lemmas(AxiomPackage *pkg, int *out_stale, char ***out_stale_names);

/**
 * @brief 标记引理为遗留状态
 *
 * 当自动重验失败时调用。将引理标记为"遗留"，
 * 即"在旧版本下得证，未验证兼容性"。
 * 用户可手动重证或保留为遗留块。
 *
 * @param[in] pkg    公理包
 * @param[in] ref_id 引理块引用 ID
 * @return 0 成功，-1 未找到或参数无效
 */
lv_PUBLIC_API int axiom_package_mark_lemma_stale(AxiomPackage *pkg, const char *ref_id);

#ifdef __cplusplus
}
#endif

/* ============================================================
 * 向后兼容别名（旧名称 → lv_ 前缀新名称）
 * ============================================================ */
#define axiom_package_create lv_axiom_package_create
#endif /* lv_AXIOM_PKG_H */
