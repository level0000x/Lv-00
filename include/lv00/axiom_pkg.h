/**
 * @file axiom_pkg.h
 * @brief 公理包系统 —— 约束模板、不可构造性记录、依赖引用追踪
 *
 * 提供公理包的创建/销毁、序列化/反序列化、约束模板注册与验证、
 * 已知不可构造问题管理、双层测试集、模板展开缓存以及依赖引用追踪等功能。
 */

#ifndef LV00_AXIOM_PKG_H
#define LV00_AXIOM_PKG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "constraint_graph.h"
#include "stream.h"
#include "symbolic_coord.h"

typedef enum { CONSTRUCTIVE, NON_CONSTRUCTIVE_ORACLE, EXPLOSION_PRINCIPLE } PropositionKind;

typedef struct AxiomPackage AxiomPackage;

typedef struct KnownUnconstructible {
    char *name;
    char *reduces_to;        /* 目标问题名称 */
    char **dependency_chain; /* 依赖项数组 */
    int dependency_count;    /* 依赖项数量 */
    char *external_ref;      /* 外部引用URL或标识符 */
    bool green_verified;     /* 是否已验证 */
} KnownUnconstructible;

AxiomPackage *axiom_package_create(const char *name, const char *version);
void axiom_package_destroy(AxiomPackage *pkg);

/* 流式上下文设置（由 LV00_DECLARE_STREAM_CTX(axiom) 宏生成） */
void axiom_set_stream_context(StreamContext *ctx);

bool axiom_package_add_known_unconstructible(AxiomPackage *pkg, KnownUnconstructible *item);
KnownUnconstructible *axiom_package_lookup_unconstructible(AxiomPackage *pkg, const char *name);

typedef enum {
    AXIOM_LOAD_OK,
    AXIOM_LOAD_FILE_NOT_FOUND,
    AXIOM_LOAD_PARSE_ERROR,
    AXIOM_LOAD_CIRCULAR_DEPENDENCY,
    AXIOM_LOAD_DEPTH_EXCEEDED,
    AXIOM_LOAD_VALIDATION_ERROR
} AxiomLoadStatus;

typedef enum { AXIOM_SAVE_OK, AXIOM_SAVE_FILE_ERROR, AXIOM_SAVE_WRITE_ERROR } AxiomSaveStatus;

/* 获取最后一次加载错误的详细信息 */
const char *axiom_package_get_last_error(void);

AxiomLoadStatus axiom_package_load(AxiomPackage *pkg, const char *filepath);
AxiomSaveStatus axiom_package_save(const AxiomPackage *pkg, const char *filepath);

/**
 * @brief 计算公理包的内容哈希
 *
 * 将公理包的名称、版本、模板、已知不可构造问题等信息
 * 计算 SHA-256 哈希值，生成 64 字符的十六进制字符串。
 * 用于检测公理包内容是否发生变化。
 *
 * @param[in] pkg  要计算哈希的公理包
 * @return 新分配的 65 字符十六进制字符串（调用者负责 free）。
 *         如果 pkg 为 NULL 或内存分配失败，返回 NULL。
 */
char *axiom_package_compute_content_hash(AxiomPackage *pkg);
bool axiom_package_validate_dependencies(AxiomPackage *pkg, AxiomPackage **loaded_packages, int package_count);

/* ============== ConstraintTemplate 增强 ============== */

/* 参数类型 */
typedef enum { PARAM_POINT, PARAM_LINE_SEGMENT, PARAM_REGION, PARAM_SCALAR } TemplateParamType;

typedef struct {
    TemplateParamType type;
    char name[64];
} TemplateParam;

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
} ConstraintTemplate;

bool axiom_package_register_template(AxiomPackage *pkg, ConstraintTemplate *tmpl);
ConstraintTemplate *axiom_package_get_template(AxiomPackage *pkg, const char *name);

bool axiom_template_validate_normal_form(const ConstraintTemplate *tmpl, const ConstraintGraph *expanded_graph,
                                         const char *canonical_form);

/* ============== 双层测试集 ============== */

/**
 * @brief 模板测试用例
 */
typedef struct {
    char name[128];
    SymbolicCoord **param_values; /* 参数值 */
    int param_count;
    bool expected_pass; /* 预期结果 */
} TemplateTestCase;

/**
 * @brief 模板测试结果
 */
typedef struct {
    int total;
    int passed;
    int failed;
    char **failure_messages;
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
TemplateTestResult axiom_template_run_tests(AxiomPackage *pkg, const char *template_name,
                                            TemplateTestCase *factory_tests, int factory_count,
                                            TemplateTestCase *user_tests, int user_count);

/**
 * @brief 释放测试结果
 */
void axiom_template_test_result_destroy(TemplateTestResult *result);

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
ConstraintGraph *axiom_package_lookup_expansion_cache(AxiomPackage *pkg, const char *template_name,
                                                      SymbolicCoord **params, int param_count);

/**
 * @brief 将展开结果存入缓存
 */
bool axiom_package_store_expansion_cache(AxiomPackage *pkg, const char *template_name, SymbolicCoord **params,
                                         int param_count, ConstraintGraph *expanded_graph);

/**
 * @brief 清空模板展开缓存
 */
void axiom_package_clear_expansion_cache(AxiomPackage *pkg);

/* ============== 依赖引用追踪 ============== */

/* 信任颜色常量（用于 original_color 字段） */
#define DEP_TRUST_GREEN 0
#define DEP_TRUST_BLUE 1
#define DEP_TRUST_YELLOW 2

/* 追踪一个依赖引用（内部或外部） */
typedef struct {
    char ref_id[64];       /* 引用标识符 */
    char content_hash[65]; /* 引用内容的 SHA-256 哈希（十六进制字符串）
                               * 注意：调用者须确保传入的哈希字符串长度恰好为 64 字符
                               *（不含终止符），超出部分将被截断，不足则未定义行为。 */
    int dependent_node_id; /* 依赖此引用的节点 ID */
    int original_color;    /* 原始信任颜色 (0=GREEN 等) */
} DependencyRef;

/* ============== AxiomPackage 结构体 ============== */

struct AxiomPackage {
    char *name;
    char *version;
    ConstraintTemplate *templates;
    int template_count;
    KnownUnconstructible *known_unconstructibles;
    int unconstructible_count;
    char *bottom_geometry;      /* 底层几何类型 */
    char *negation_encoding;    /* 否定编码方法 */
    int contradiction_behavior; /* 矛盾行为 */

    /* 模板展开缓存 */
    TemplateExpansionCache *expansion_cache;
    int expansion_cache_count;
    int expansion_cache_capacity;
    int max_expansion_depth; /* 默认 8 */

    /* 依赖引用追踪 */
    DependencyRef *dep_refs;
    int dep_ref_count;
    int dep_ref_capacity;
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
int axiom_package_register_dependency_ref(AxiomPackage *pkg, const char *ref_id, const char *content_hash,
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
int axiom_package_validate_dependencies_with_hashes(AxiomPackage *pkg, DependencyRef **invalidated_refs,
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
int axiom_package_auto_degrade_invalidated(AxiomPackage *pkg, ConstraintGraph *graph);

#ifdef __cplusplus
}
#endif

#endif /* LV00_AXIOM_PKG_H */
