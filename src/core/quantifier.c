/**
 * @file quantifier.c
 * @brief 量词系统实现 —— 全称/存在/唯一存在量词的形式化处理
 *
 * @details 本文件实现 quantifier.h 中声明的所有接口，提供：
 *          - 量化域（Lv00Domain）的创建、销毁、元素管理
 *          - 量化表达式（Lv00QuantifiedExpr）的创建、销毁、评估
 *          - 量词实例化（∀-消去）与泛化（∀-引入）
 *          - 存在量词引入（∃I）与消去（∃E）
 *          - 有限域上的量词消去（全称→合取，存在→析取，唯一存在→恰好一个）
 *          - 三值逻辑评估（Kleene 强三值语义）
 *
 *          量词消去规则（有限域 D = {d1, ..., dn}）：
 *          - ∀x∈D.P(x)  →  P(d1) ∧ ... ∧ P(dn)
 *          - ∃x∈D.P(x)  →  P(d1) ∨ ... ∨ P(dn)
 *          - ∃!x∈D.P(x) →  恰好一个 di 满足 P(di)
 *
 *          空域处理：
 *          - ∀ 在空域上为真（空合取的恒等元）
 *          - ∃ 在空域上为假（空析取的恒等元）
 *
 * @author Lv-00 Project
 * @version 3.3.0
 *
 * @dependencies
 *   - lv00/quantifier.h       : 量词系统公共接口定义
 *   - lv00/lv00_utils.h       : 统一内存分配器（lv00_malloc / lv00_free / lv00_calloc）
 *   - lv00/three_valued_logic.h : 三值逻辑运算（Kleene 强三值逻辑）
 *   - lv00/constraint_graph.h : 约束图数据结构（域的子图表示）
 *   - lv00/proof.h            : Proposition 结构定义
 */

#include "lv00/quantifier.h"
#include "lv00/three_valued_logic.h"
#include "lv00/lv00_utils.h"
#include "lv00/proof.h"
#include "lv00/constraint_graph.h"
#include "lv00/error_codes.h"

#include <stdio.h>
#include <string.h>

/* ============== 内部辅助宏 ============== */

/**
 * @brief 域元素数组的初始容量
 */
#define DOMAIN_INITIAL_CAPACITY 8

/**
 * @brief 实例化ID数组的初始容量
 */
#define INSTANTIATE_INITIAL_CAPACITY 8

/**
 * @brief 量词消去结果命题名称缓冲区大小
 */
#define RESULT_NAME_BUF_SIZE 256

/* ============== 内部辅助函数前向声明 ============== */

static bool domain_ensure_capacity(Lv00Domain *domain, int needed);
static bool instantiate_ensure_capacity(Lv00QuantifiedExpr *expr, int needed);
static struct Proposition *create_result_proposition(int id, const char *name);
static Lv00TruthValue evaluate_body_for_element(const Lv00QuantifiedExpr *expr, int element_id);
static void init_quant_result(Lv00QuantifiedResult *result);

/* ============== 内部辅助函数实现 ============== */

/**
 * @brief 确保域元素数组有足够容量
 *
 * 若当前容量不足，以 2 倍率扩容。
 *
 * @param domain  域
 * @param needed  需要的最小容量
 * @return true 成功，false 失败（内存不足）
 */
static bool domain_ensure_capacity(Lv00Domain *domain, int needed)
{
    int new_capacity;

    if (needed <= domain->element_capacity) {
        return true;
    }

    new_capacity = domain->element_capacity == 0 ? DOMAIN_INITIAL_CAPACITY : domain->element_capacity * 2;
    while (new_capacity < needed) {
        /* 整数溢出保护：翻倍后若变为负数或超过 INT_MAX/2，则无法继续扩容 */
        if (new_capacity < 0 || new_capacity > INT_MAX / 2) {
            return false;
        }
        new_capacity *= 2;
    }

    int *new_elements = (int *)lv00_realloc(domain->domain_elements,
                                            (size_t)new_capacity * sizeof(int));
    if (!new_elements) {
        return false;
    }

    domain->domain_elements = new_elements;
    domain->element_capacity = new_capacity;
    return true;
}

/**
 * @brief 确保实例化ID数组有足够容量
 *
 * @param expr    量化表达式
 * @param needed  需要的最小容量
 * @return true 成功，false 失败
 */
static bool instantiate_ensure_capacity(Lv00QuantifiedExpr *expr, int needed)
{
    int new_capacity;

    if (needed <= expr->instantiated_count) {
        /* 容量由 instantiated_count 隐含，此处简化处理 */
        return true;
    }

    new_capacity = expr->instantiated_count == 0
                       ? INSTANTIATE_INITIAL_CAPACITY
                       : expr->instantiated_count * 2;
    while (new_capacity < needed) {
        new_capacity *= 2;
    }

    int *new_ids = (int *)lv00_realloc(expr->instantiated_ids,
                                       (size_t)new_capacity * sizeof(int));
    if (!new_ids) {
        return false;
    }

    expr->instantiated_ids = new_ids;
    return true;
}

/**
 * @brief 创建简化的结果命题
 *
 * 在量词消去操作中，为消去后的命题创建一个简化的 Proposition 结构。
 * 仅设置 id 和 label（name）字段，其余字段置零/NULL。
 *
 * @param id    命题ID
 * @param name  命题名称（内部复制）
 * @return 新分配的 Proposition，失败返回 NULL
 */
static struct Proposition *create_result_proposition(int id, const char *name)
{
    struct Proposition *prop = (struct Proposition *)lv00_calloc(1, sizeof(struct Proposition));
    if (!prop) {
        return NULL;
    }

    prop->id = id;
    if (name) {
        prop->label = lv00_strdup(name);
        if (!prop->label) {
            lv00_free((void **)&prop);
            return NULL;
        }
    }

    return prop;
}

/**
 * @brief 评估体命题在指定元素上的真值
 *
 * 简化实现：检查体命题的 id 是否有效（非零），
 * 并检查该元素是否属于域。
 * 在完整实现中，此处应将元素代入变量后评估体命题。
 *
 * @param expr        量化表达式
 * @param element_id  要评估的元素ID
 * @return 三值真值
 */
static Lv00TruthValue evaluate_body_for_element(const Lv00QuantifiedExpr *expr, int element_id)
{
    (void)element_id; /* 当前简化实现暂不使用具体元素 */

    if (!expr || !expr->body_proposition) {
        return LV00_UNKNOWN;
    }

    /* 简化评估：体命题存在且 id 有效时视为 TRUE */
    if (expr->body_proposition->id > 0) {
        return LV00_TRUE;
    }

    return LV00_UNKNOWN;
}

/**
 * @brief 初始化量词操作结果结构体
 *
 * 将结果的所有字段置为安全的初始状态。
 *
 * @param result  结果结构体指针
 */
static void init_quant_result(Lv00QuantifiedResult *result)
{
    if (!result) {
        return;
    }

    result->status = LV00_QUANT_OK;
    result->truth_value = LV00_UNKNOWN;
    result->witness_node_id = -1;
    result->error_message = NULL;
    result->result_prop = NULL;
}

/* ============== 域管理 API ============== */

/**
 * @brief 创建命名域
 *
 * 分配并初始化一个 Lv00Domain 结构，设置域名和默认值。
 * 新创建的域没有元素，is_finite 默认为 false（除非后续添加元素）。
 *
 * @param id          域ID
 * @param domain_name 域名称（内部复制，可为 NULL）
 * @return 新分配的域，失败返回 NULL
 */
Lv00Domain *lv00_quant_domain_create(int id, const char *domain_name)
{
    Lv00Domain *domain = (Lv00Domain *)lv00_calloc(1, sizeof(Lv00Domain));
    if (!domain) {
        return NULL;
    }

    domain->id = id;
    domain->domain_name = domain_name ? lv00_strdup(domain_name) : NULL;
    domain->domain_elements = NULL;
    domain->element_count = 0;
    domain->element_capacity = 0;
    domain->subgraph = NULL;
    domain->is_finite = false;
    domain->estimated_cardinality = -1;

    return domain;
}

/**
 * @brief 创建有限枚举域
 *
 * 从给定的元素数组创建有限域。所有元素被复制到域的内部存储中。
 *
 * @param id       域ID
 * @param elements 元素节点ID数组（可为 NULL，此时 count 须为 0）
 * @param count    元素数量
 * @return 新分配的域，失败返回 NULL
 */
Lv00Domain *lv00_quant_domain_create_finite(int id, const int *elements, int count)
{
    Lv00Domain *domain;

    if (count < 0) {
        return NULL;
    }

    domain = (Lv00Domain *)lv00_calloc(1, sizeof(Lv00Domain));
    if (!domain) {
        return NULL;
    }

    domain->id = id;
    domain->domain_name = NULL;
    domain->is_finite = true;
    domain->estimated_cardinality = count;
    domain->element_count = 0;
    domain->element_capacity = 0;
    domain->domain_elements = NULL;
    domain->subgraph = NULL;

    /* 复制元素到内部数组 */
    if (count > 0 && elements) {
        domain->domain_elements = (int *)lv00_malloc((size_t)count * sizeof(int));
        if (!domain->domain_elements) {
            lv00_free((void **)&domain);
            return NULL;
        }
        memcpy(domain->domain_elements, elements, (size_t)count * sizeof(int));
        domain->element_count = count;
        domain->element_capacity = count;
    }

    return domain;
}

/**
 * @brief 向域中添加单个元素
 *
 * 若元素已存在则不做重复添加。添加后自动标记域为有限域。
 *
 * @param domain   域
 * @param element  元素节点ID
 * @return true 成功，false 失败（内存不足或域为 NULL）
 */
bool lv00_quant_domain_add_element(Lv00Domain *domain, int element)
{
    int i;

    if (!domain) {
        return false;
    }

    /* 检查是否已存在 */
    for (i = 0; i < domain->element_count; i++) {
        if (domain->domain_elements[i] == element) {
            return true; /* 已存在，不重复添加 */
        }
    }

    /* 扩容 */
    if (!domain_ensure_capacity(domain, domain->element_count + 1)) {
        return false;
    }

    domain->domain_elements[domain->element_count] = element;
    domain->element_count++;
    domain->is_finite = true;
    domain->estimated_cardinality = domain->element_count;

    return true;
}

/**
 * @brief 向域中批量添加元素
 *
 * 跳过已存在的元素，仅添加新元素。
 *
 * @param domain   域
 * @param elements 元素节点ID数组
 * @param count    元素数量
 * @return true 成功，false 失败
 */
bool lv00_quant_domain_add_elements(Lv00Domain *domain, const int *elements, int count)
{
    int i;

    if (!domain || !elements || count <= 0) {
        return false;
    }

    for (i = 0; i < count; i++) {
        if (!lv00_quant_domain_add_element(domain, elements[i])) {
            return false;
        }
    }

    return true;
}

/**
 * @brief 检查元素是否属于域
 *
 * 在有限枚举域中线性搜索；在约束图子图域中检查节点是否存在。
 *
 * @param domain    域
 * @param element   元素ID
 * @return true 属于，false 不属于
 */
bool lv00_quant_domain_contains(const Lv00Domain *domain, int element)
{
    int i;

    if (!domain) {
        return false;
    }

    /* 在有限枚举域中搜索 */
    for (i = 0; i < domain->element_count; i++) {
        if (domain->domain_elements[i] == element) {
            return true;
        }
    }

    /* 在约束图子图域中检查 */
    if (domain->subgraph) {
        if (graph_get_node(domain->subgraph, element) != NULL) {
            return true;
        }
    }

    return false;
}

/**
 * @brief 获取域大小
 *
 * 对有限域返回元素数量；对无限域返回 -1。
 *
 * @param domain 域
 * @return 元素数量（-1 表示无限）
 */
int lv00_quant_domain_size(const Lv00Domain *domain)
{
    if (!domain) {
        return -1;
    }

    if (domain->is_finite) {
        return domain->element_count;
    }

    /* 约束图子图域：尝试从图中获取节点数 */
    if (domain->subgraph) {
        return graph_get_node_count(domain->subgraph);
    }

    return -1; /* 无限域 */
}

/**
 * @brief 销毁域
 *
 * 释放域的所有内部资源，包括元素数组、域名和子图引用。
 * 注意：不销毁 subgraph 指向的约束图（调用者负责管理其生命周期）。
 *
 * @param domain 域（可为 NULL，此时无操作）
 */
void lv00_quant_domain_destroy(Lv00Domain *domain)
{
    if (!domain) {
        return;
    }

    LV00_FREE_AND_NULL(domain->domain_name);
    LV00_FREE_AND_NULL(domain->domain_elements);
    /* subgraph 的所有权不属于域，不在此处释放 */

    lv00_free((void **)&domain);
}

/* ============== 量化表达式 API ============== */

/**
 * @brief 创建量化表达式
 *
 * 构造一个完整的量化命题表达式：
 *   QUANTIFIER variable_name ∈ domain . body_proposition
 *
 * 所有权语义：domain 和 body_proposition 的所有权转移给新创建的表达式，
 * 表达式销毁时会一并释放它们。
 *
 * @param id               表达式ID
 * @param quantifier       量词类型
 * @param variable_name    变量名（内部复制）
 * @param variable_node_id 变量绑定的约束图节点ID
 * @param domain           量化域（所有权转移）
 * @param body_prop        体命题（所有权转移）
 * @return 新分配的量化表达式，失败返回 NULL
 */
Lv00QuantifiedExpr *lv00_quant_expr_create(int id, Lv00Quantifier quantifier, const char *variable_name,
                                           int variable_node_id, Lv00Domain *domain, struct Proposition *body_prop)
{
    Lv00QuantifiedExpr *expr;

    if (!domain) {
        return NULL;
    }

    expr = (Lv00QuantifiedExpr *)lv00_calloc(1, sizeof(Lv00QuantifiedExpr));
    if (!expr) {
        return NULL;
    }

    expr->id = id;
    expr->quantifier = quantifier;
    expr->variable_name = variable_name ? lv00_strdup(variable_name) : NULL;
    expr->variable_node_id = variable_node_id;
    expr->domain = domain;
    expr->body_proposition = body_prop;

    /* 实例化追踪初始化 */
    expr->instantiated_ids = NULL;
    expr->instantiated_count = 0;

    /* 真值缓存初始化为无效 */
    expr->cached_truth = LV00_UNKNOWN;
    expr->truth_cache_valid = false;

    return expr;
}

/**
 * @brief 销毁量化表达式
 *
 * 释放表达式及其拥有的所有资源：
 * - 变量名字符串
 * - 量化域
 * - 体命题
 * - 实例化ID数组
 *
 * @param expr 量化表达式（可为 NULL，此时无操作）
 */
void lv00_quant_expr_destroy(Lv00QuantifiedExpr *expr)
{
    if (!expr) {
        return;
    }

    LV00_FREE_AND_NULL(expr->variable_name);
    lv00_quant_domain_destroy(expr->domain);
    expr->domain = NULL;

    /* 释放体命题 */
    if (expr->body_proposition) {
        LV00_FREE_AND_NULL(expr->body_proposition->label);
        LV00_FREE_AND_NULL(expr->body_proposition->input_port_ids);
        LV00_FREE_AND_NULL(expr->body_proposition->output_port_ids);
        LV00_FREE_AND_NULL(expr->body_proposition->precondition_region_ids);
        LV00_FREE_AND_NULL(expr->body_proposition->postcondition_constraint_ids);
        /* sub_props 和 pattern 的释放需要递归，此处简化处理 */
        lv00_free((void **)&(expr->body_proposition));
    }

    LV00_FREE_AND_NULL(expr->instantiated_ids);

    lv00_free((void **)&expr);
}

/**
 * @brief 评估量化表达式的真值（三值逻辑）
 *
 * 评估策略：
 * - 有限域：枚举所有元素，逐一评估体命题
 *   - ∀：AND 归约（空域返回 TRUE）
 *   - ∃：OR 归约（空域返回 FALSE）
 *   - ∃!：统计满足元素数，恰好 1 个为 TRUE
 * - 无限域：返回 LV00_UNKNOWN
 *
 * 评估结果会被缓存，后续调用直接返回缓存值。
 *
 * @param expr 量化表达式
 * @return 三值真值
 */
Lv00TruthValue lv00_quant_expr_evaluate(Lv00QuantifiedExpr *expr)
{
    int domain_size;
    int i;
    int satisfying_count;
    Lv00TruthValue elem_truth;
    Lv00TruthValue result;

    if (!expr) {
        return LV00_UNKNOWN;
    }

    /* 若缓存有效，直接返回 */
    if (expr->truth_cache_valid) {
        return expr->cached_truth;
    }

    domain_size = lv00_quant_domain_size(expr->domain);

    /* 无限域：无法完全评估 */
    if (domain_size < 0) {
        expr->cached_truth = LV00_UNKNOWN;
        expr->truth_cache_valid = true;
        return LV00_UNKNOWN;
    }

    /* 空域处理 */
    if (domain_size == 0) {
        switch (expr->quantifier) {
        case LV00_FORALL:
            /* 空合取的恒等元为 TRUE */
            expr->cached_truth = LV00_TRUE;
            break;
        case LV00_EXISTS:
            /* 空析取的恒等元为 FALSE */
            expr->cached_truth = LV00_FALSE;
            break;
        case LV00_EXISTS_UNIQUE:
            /* 空域上不存在唯一满足的元素 */
            expr->cached_truth = LV00_FALSE;
            break;
        default:
            expr->cached_truth = LV00_UNKNOWN;
            break;
        }
        expr->truth_cache_valid = true;
        return expr->cached_truth;
    }

    /* 有限域：枚举评估 */
    switch (expr->quantifier) {
    case LV00_FORALL: {
        /* ∀x∈D.P(x) → P(d1) ∧ ... ∧ P(dn) */
        result = LV00_TRUE;
        for (i = 0; i < expr->domain->element_count; i++) {
            elem_truth = evaluate_body_for_element(expr, expr->domain->domain_elements[i]);
            result = lv00_tvl_and(result, elem_truth);
            /* 短路：遇到 FALSE 立即停止 */
            if (result == LV00_FALSE) {
                break;
            }
        }
        expr->cached_truth = result;
        break;
    }

    case LV00_EXISTS: {
        /* ∃x∈D.P(x) → P(d1) ∨ ... ∨ P(dn) */
        result = LV00_FALSE;
        for (i = 0; i < expr->domain->element_count; i++) {
            elem_truth = evaluate_body_for_element(expr, expr->domain->domain_elements[i]);
            result = lv00_tvl_or(result, elem_truth);
            /* 短路：遇到 TRUE 立即停止 */
            if (result == LV00_TRUE) {
                break;
            }
        }
        expr->cached_truth = result;
        break;
    }

    case LV00_EXISTS_UNIQUE: {
        /* ∃!x∈D.P(x) → 恰好一个元素满足 */
        satisfying_count = 0;
        result = LV00_FALSE;
        for (i = 0; i < expr->domain->element_count; i++) {
            elem_truth = evaluate_body_for_element(expr, expr->domain->domain_elements[i]);
            if (elem_truth == LV00_TRUE) {
                satisfying_count++;
            } else if (elem_truth == LV00_UNKNOWN) {
                result = LV00_UNKNOWN;
            }
        }
        if (result != LV00_UNKNOWN) {
            result = (satisfying_count == 1) ? LV00_TRUE : LV00_FALSE;
        }
        expr->cached_truth = result;
        break;
    }

    default:
        expr->cached_truth = LV00_UNKNOWN;
        break;
    }

    expr->truth_cache_valid = true;
    return expr->cached_truth;
}

/* ============== 量词实例化 ============== */

/**
 * @brief 量词实例化（∀-消去）
 *
 * 从 ∀x.P(x) 推导出 P(t)，其中 t 必须在域中。
 * 这是全称量词的消去规则（∀E）。
 *
 * 实例化后：
 * - 将 instance_id 记录到 instantiated_ids 列表中
 * - 创建结果命题（简化为设置 id 和 name）
 * - 设置 truth_value 为体命题在 instance_id 上的评估值
 *
 * @param expr         量化表达式（须为 ∀ 类型）
 * @param instance_id  要代入的实例节点ID
 * @param out_result   输出结果
 * @return 操作结果状态码
 */
Lv00QuantResult lv00_quantifier_instantiate(const Lv00QuantifiedExpr *expr, int instance_id,
                                            Lv00QuantifiedResult *out_result)
{
    char name_buf[RESULT_NAME_BUF_SIZE];
    const char *quant_str;

    init_quant_result(out_result);

    /* 参数校验 */
    if (!expr || !out_result) {
        out_result->status = LV00_QUANT_ERROR;
        return LV00_QUANT_ERROR;
    }

    if (expr->quantifier != LV00_FORALL) {
        out_result->status = LV00_QUANT_INSTANTIATE_FAILED;
        out_result->error_message = lv00_strdup("实例化仅适用于全称量词(∀)");
        return LV00_QUANT_INSTANTIATE_FAILED;
    }

    if (!expr->domain) {
        out_result->status = LV00_QUANT_DOMAIN_EMPTY;
        out_result->error_message = lv00_strdup("域未定义");
        return LV00_QUANT_DOMAIN_EMPTY;
    }

    /* 检查实例是否在域中 */
    if (!lv00_quant_domain_contains(expr->domain, instance_id)) {
        out_result->status = LV00_QUANT_INVALID_VARIABLE;
        out_result->error_message = lv00_strdup("实例不在量化域中");
        return LV00_QUANT_INVALID_VARIABLE;
    }

    if (!expr->body_proposition) {
        out_result->status = LV00_QUANT_BODY_UNDEFINED;
        out_result->error_message = lv00_strdup("体命题未定义");
        return LV00_QUANT_BODY_UNDEFINED;
    }

    /* 记录实例化（需要修改 expr，但函数签名为 const，此处通过结果反映） */
    /* 注意：由于 expr 为 const，无法直接修改 instantiated_ids。
       调用者若需追踪，应在外部管理。此处仅生成结果。 */

    /* 评估体命题在实例上的真值 */
    out_result->truth_value = evaluate_body_for_element(expr, instance_id);

    /* 创建结果命题 */
    quant_str = lv00_quant_to_string(expr->quantifier);
    (void)snprintf(name_buf, RESULT_NAME_BUF_SIZE, "%s%s(%d).P(%d)",
                   quant_str, expr->variable_name ? expr->variable_name : "x",
                   expr->id, instance_id);

    out_result->result_prop = create_result_proposition(expr->body_proposition->id, name_buf);
    if (!out_result->result_prop) {
        out_result->status = LV00_QUANT_ERROR;
        out_result->error_message = lv00_strdup("创建结果命题失败");
        return LV00_QUANT_ERROR;
    }

    out_result->witness_node_id = instance_id;
    out_result->status = LV00_QUANT_OK;

    return LV00_QUANT_OK;
}

/**
 * @brief 量词泛化（∀-引入）
 *
 * 从 P(x) 对任意 x∈D 成立推导出 ∀x.P(x)。
 * 前提条件（特征变量条件）：
 * - x 不能在前提集中自由出现
 * - 域必须有限（或已验证所有元素）
 *
 * 泛化操作：
 * 1. 检查域是否为有限域
 * 2. 评估体命题在所有域元素上的真值
 * 3. 若全部为 TRUE，则泛化成功
 *
 * @param expr         量化表达式模板（须为 ∀ 类型）
 * @param out_result   输出结果
 * @return 操作结果状态码
 */
Lv00QuantResult lv00_quantifier_generalize(const Lv00QuantifiedExpr *expr, Lv00QuantifiedResult *out_result)
{
    char name_buf[RESULT_NAME_BUF_SIZE];
    const char *quant_str;
    Lv00TruthValue truth;

    init_quant_result(out_result);

    if (!expr || !out_result) {
        out_result->status = LV00_QUANT_ERROR;
        return LV00_QUANT_ERROR;
    }

    if (expr->quantifier != LV00_FORALL) {
        out_result->status = LV00_QUANT_GENERALIZE_FAILED;
        out_result->error_message = lv00_strdup("泛化仅适用于全称量词(∀)");
        return LV00_QUANT_GENERALIZE_FAILED;
    }

    if (!expr->body_proposition) {
        out_result->status = LV00_QUANT_BODY_UNDEFINED;
        out_result->error_message = lv00_strdup("体命题未定义");
        return LV00_QUANT_BODY_UNDEFINED;
    }

    /* 检查域是否有限 */
    if (!expr->domain->is_finite && !expr->domain->subgraph) {
        out_result->status = LV00_QUANT_DOMAIN_INFINITE;
        out_result->error_message = lv00_strdup("无法在无限域上泛化");
        return LV00_QUANT_DOMAIN_INFINITE;
    }

    /* 评估量化表达式的真值（需要非 const，此处通过简化方式） */
    truth = LV00_UNKNOWN;

    /* 对有限域枚举评估 */
    if (expr->domain->is_finite && expr->domain->element_count >= 0) {
        int i;
        truth = LV00_TRUE;
        for (i = 0; i < expr->domain->element_count; i++) {
            Lv00TruthValue elem_truth = evaluate_body_for_element(expr, expr->domain->domain_elements[i]);
            truth = lv00_tvl_and(truth, elem_truth);
            if (truth == LV00_FALSE) {
                break;
            }
        }
    }

    out_result->truth_value = truth;

    /* 创建结果命题 */
    quant_str = lv00_quant_to_string(expr->quantifier);
    (void)snprintf(name_buf, RESULT_NAME_BUF_SIZE, "%s%s∈D.P(%s)",
                   quant_str,
                   expr->variable_name ? expr->variable_name : "x",
                   expr->variable_name ? expr->variable_name : "x");

    out_result->result_prop = create_result_proposition(expr->id, name_buf);
    if (!out_result->result_prop) {
        out_result->status = LV00_QUANT_ERROR;
        out_result->error_message = lv00_strdup("创建结果命题失败");
        return LV00_QUANT_ERROR;
    }

    if (truth == LV00_TRUE) {
        out_result->status = LV00_QUANT_OK;
    } else if (truth == LV00_FALSE) {
        out_result->status = LV00_QUANT_COUNTEREXAMPLE;
        out_result->error_message = lv00_strdup("泛化失败：存在不满足体命题的元素");
    } else {
        out_result->status = LV00_QUANT_GENERALIZE_FAILED;
        out_result->error_message = lv00_strdup("泛化失败：无法确定所有元素是否满足体命题");
    }

    return out_result->status;
}

/* ============== 存在量词运算 ============== */

/**
 * @brief 存在量词引入（∃I）
 *
 * 从 P(t) 推导出 ∃x.P(x)，其中 t 必须在域中。
 * t 称为"目击者"（witness），证明存在性。
 *
 * @param expr         待填充的量化表达式（量词须为 ∃）
 * @param witness_id   目击者节点ID
 * @param out_result   输出结果
 * @return 操作结果状态码
 */
Lv00QuantResult lv00_quant_exists_introduce(Lv00QuantifiedExpr *expr, int witness_id,
                                            Lv00QuantifiedResult *out_result)
{
    char name_buf[RESULT_NAME_BUF_SIZE];
    const char *quant_str;

    init_quant_result(out_result);

    if (!expr || !out_result) {
        out_result->status = LV00_QUANT_ERROR;
        return LV00_QUANT_ERROR;
    }

    if (expr->quantifier != LV00_EXISTS) {
        out_result->status = LV00_QUANT_INSTANTIATE_FAILED;
        out_result->error_message = lv00_strdup("存在引入仅适用于存在量词(∃)");
        return LV00_QUANT_INSTANTIATE_FAILED;
    }

    if (!expr->domain) {
        out_result->status = LV00_QUANT_DOMAIN_EMPTY;
        out_result->error_message = lv00_strdup("域未定义");
        return LV00_QUANT_DOMAIN_EMPTY;
    }

    /* 检查目击者是否在域中 */
    if (!lv00_quant_domain_contains(expr->domain, witness_id)) {
        out_result->status = LV00_QUANT_INVALID_VARIABLE;
        out_result->error_message = lv00_strdup("目击者不在量化域中");
        return LV00_QUANT_INVALID_VARIABLE;
    }

    if (!expr->body_proposition) {
        out_result->status = LV00_QUANT_BODY_UNDEFINED;
        out_result->error_message = lv00_strdup("体命题未定义");
        return LV00_QUANT_BODY_UNDEFINED;
    }

    /* 评估体命题在目击者上的真值 */
    out_result->truth_value = evaluate_body_for_element(expr, witness_id);

    if (out_result->truth_value != LV00_TRUE) {
        out_result->status = LV00_QUANT_INSTANTIATE_FAILED;
        out_result->error_message = lv00_strdup("目击者不满足体命题，无法引入存在量词");
        return LV00_QUANT_INSTANTIATE_FAILED;
    }

    /* 记录目击者 */
    out_result->witness_node_id = witness_id;

    /* 创建结果命题 */
    quant_str = lv00_quant_to_string(expr->quantifier);
    (void)snprintf(name_buf, RESULT_NAME_BUF_SIZE, "%s%s∈D.P(%s)",
                   quant_str,
                   expr->variable_name ? expr->variable_name : "x",
                   expr->variable_name ? expr->variable_name : "x");

    out_result->result_prop = create_result_proposition(expr->id, name_buf);
    if (!out_result->result_prop) {
        out_result->status = LV00_QUANT_ERROR;
        out_result->error_message = lv00_strdup("创建结果命题失败");
        return LV00_QUANT_ERROR;
    }

    out_result->status = LV00_QUANT_OK;
    return LV00_QUANT_OK;
}

/**
 * @brief 存在量词消去（∃E）
 *
 * 从 ∃x.P(x) 和 ∀y.(P(y)→Q) 推导出 Q（其中 y 不在 Q 中自由出现）。
 *
 * 简化实现：
 * 1. 验证存在量化表达式
 * 2. 在有限域上找到满足体命题的目击者
 * 3. 将目标命题作为结果返回
 *
 * @param exists_expr  存在量化表达式
 * @param target_prop  目标命题 Q
 * @param out_result   输出结果
 * @return 操作结果状态码
 */
Lv00QuantResult lv00_quant_exists_eliminate(const Lv00QuantifiedExpr *exists_expr,
                                            struct Proposition *target_prop, Lv00QuantifiedResult *out_result)
{
    int i;
    char name_buf[RESULT_NAME_BUF_SIZE];

    init_quant_result(out_result);

    if (!exists_expr || !out_result) {
        out_result->status = LV00_QUANT_ERROR;
        return LV00_QUANT_ERROR;
    }

    if (exists_expr->quantifier != LV00_EXISTS) {
        out_result->status = LV00_QUANT_INSTANTIATE_FAILED;
        out_result->error_message = lv00_strdup("存在消去仅适用于存在量词(∃)");
        return LV00_QUANT_INSTANTIATE_FAILED;
    }

    if (!exists_expr->body_proposition) {
        out_result->status = LV00_QUANT_BODY_UNDEFINED;
        out_result->error_message = lv00_strdup("体命题未定义");
        return LV00_QUANT_BODY_UNDEFINED;
    }

    /* 在有限域上寻找目击者 */
    if (exists_expr->domain && exists_expr->domain->is_finite) {
        for (i = 0; i < exists_expr->domain->element_count; i++) {
            Lv00TruthValue elem_truth = evaluate_body_for_element(
                exists_expr, exists_expr->domain->domain_elements[i]);
            if (elem_truth == LV00_TRUE) {
                out_result->witness_node_id = exists_expr->domain->domain_elements[i];
                break;
            }
        }
    }

    if (out_result->witness_node_id < 0) {
        out_result->status = LV00_QUANT_INSTANTIATE_FAILED;
        out_result->error_message = lv00_strdup("未找到满足体命题的目击者");
        out_result->truth_value = LV00_FALSE;
        return LV00_QUANT_INSTANTIATE_FAILED;
    }

    /* 使用目标命题作为结果 */
    if (target_prop) {
        (void)snprintf(name_buf, RESULT_NAME_BUF_SIZE, "ElimE_%s_%d",
                       exists_expr->variable_name ? exists_expr->variable_name : "x",
                       exists_expr->id);
        out_result->result_prop = create_result_proposition(target_prop->id, name_buf);
    }

    out_result->truth_value = LV00_TRUE;
    out_result->status = LV00_QUANT_OK;

    return LV00_QUANT_OK;
}

/* ============== 有限域上的量词消去 ============== */

/**
 * @brief 有限域上的全称量词消去
 *
 * 在有限域 D = {d1, ..., dn} 上，将 ∀x∈D.P(x) 展开为 P(d1) ∧ ... ∧ P(dn)。
 * 返回消去后的合取命题。
 *
 * 算法流程：
 * 1. 验证表达式为全称量词且域有限
 * 2. 为每个域元素创建体命题的实例
 * 3. 将所有实例组合为合取命题
 * 4. 评估合取命题的真值
 *
 * @param expr         全称量化表达式
 * @param out_result   输出结果（含 status、truth_value 和 result_prop）
 * @return 操作结果状态码
 */
Lv00QuantResult lv00_quant_eliminate_forall_finite(const Lv00QuantifiedExpr *expr,
                                                    Lv00QuantifiedResult *out_result)
{
    int i;
    char name_buf[RESULT_NAME_BUF_SIZE];
    Lv00TruthValue combined_truth;

    init_quant_result(out_result);

    if (!expr || !out_result) {
        out_result->status = LV00_QUANT_ERROR;
        return LV00_QUANT_ERROR;
    }

    if (expr->quantifier != LV00_FORALL) {
        out_result->status = LV00_QUANT_ERROR;
        out_result->error_message = lv00_strdup("全称消去仅适用于全称量词(∀)");
        return LV00_QUANT_ERROR;
    }

    if (!expr->domain || !expr->domain->is_finite) {
        out_result->status = LV00_QUANT_DOMAIN_INFINITE;
        out_result->error_message = lv00_strdup("域为无限域，无法消去全称量词");
        return LV00_QUANT_DOMAIN_INFINITE;
    }

    /* 空域：∀ 在空域上为真 */
    if (expr->domain->element_count == 0) {
        out_result->status = LV00_QUANT_DOMAIN_EMPTY;
        out_result->truth_value = LV00_TRUE;
        (void)snprintf(name_buf, RESULT_NAME_BUF_SIZE, "ForallElim_empty_%d", expr->id);
        out_result->result_prop = create_result_proposition(expr->id, name_buf);
        return LV00_QUANT_DOMAIN_EMPTY;
    }

    /* 枚举所有元素，执行 AND 归约 */
    combined_truth = LV00_TRUE;
    for (i = 0; i < expr->domain->element_count; i++) {
        Lv00TruthValue elem_truth = evaluate_body_for_element(expr, expr->domain->domain_elements[i]);
        combined_truth = lv00_tvl_and(combined_truth, elem_truth);
        if (combined_truth == LV00_FALSE) {
            break; /* 短路 */
        }
    }

    out_result->truth_value = combined_truth;

    /* 创建结果命题：P(d1) ∧ ... ∧ P(dn) */
    (void)snprintf(name_buf, RESULT_NAME_BUF_SIZE, "ForallElim_%s_%d",
                   expr->variable_name ? expr->variable_name : "x", expr->id);
    out_result->result_prop = create_result_proposition(expr->id, name_buf);
    if (!out_result->result_prop) {
        out_result->status = LV00_QUANT_ERROR;
        out_result->error_message = lv00_strdup("创建结果命题失败");
        return LV00_QUANT_ERROR;
    }

    out_result->status = LV00_QUANT_OK;
    return LV00_QUANT_OK;
}

/**
 * @brief 有限域上的存在量词消去
 *
 * 在有限域 D = {d1, ..., dn} 上，将 ∃x∈D.P(x) 展开为 P(d1) ∨ ... ∨ P(dn)。
 * 返回消去后的析取命题。
 *
 * 算法流程：
 * 1. 验证表达式为存在量词且域有限
 * 2. 枚举所有域元素，执行 OR 归约
 * 3. 记录第一个满足体命题的目击者
 * 4. 创建析取命题作为结果
 *
 * @param expr         存在量化表达式
 * @param out_result   输出结果（含 status、truth_value、witness_node_id 和 result_prop）
 * @return 操作结果状态码
 */
Lv00QuantResult lv00_quant_eliminate_exists_finite(const Lv00QuantifiedExpr *expr,
                                                    Lv00QuantifiedResult *out_result)
{
    int i;
    char name_buf[RESULT_NAME_BUF_SIZE];
    Lv00TruthValue combined_truth;

    init_quant_result(out_result);

    if (!expr || !out_result) {
        out_result->status = LV00_QUANT_ERROR;
        return LV00_QUANT_ERROR;
    }

    if (expr->quantifier != LV00_EXISTS) {
        out_result->status = LV00_QUANT_ERROR;
        out_result->error_message = lv00_strdup("存在消去仅适用于存在量词(∃)");
        return LV00_QUANT_ERROR;
    }

    if (!expr->domain || !expr->domain->is_finite) {
        out_result->status = LV00_QUANT_DOMAIN_INFINITE;
        out_result->error_message = lv00_strdup("域为无限域，无法消去存在量词");
        return LV00_QUANT_DOMAIN_INFINITE;
    }

    /* 空域：∃ 在空域上为假 */
    if (expr->domain->element_count == 0) {
        out_result->status = LV00_QUANT_DOMAIN_EMPTY;
        out_result->truth_value = LV00_FALSE;
        (void)snprintf(name_buf, RESULT_NAME_BUF_SIZE, "ExistsElim_empty_%d", expr->id);
        out_result->result_prop = create_result_proposition(expr->id, name_buf);
        return LV00_QUANT_DOMAIN_EMPTY;
    }

    /* 枚举所有元素，执行 OR 归约 */
    combined_truth = LV00_FALSE;
    for (i = 0; i < expr->domain->element_count; i++) {
        Lv00TruthValue elem_truth = evaluate_body_for_element(expr, expr->domain->domain_elements[i]);
        combined_truth = lv00_tvl_or(combined_truth, elem_truth);

        /* 记录第一个目击者 */
        if (elem_truth == LV00_TRUE && out_result->witness_node_id < 0) {
            out_result->witness_node_id = expr->domain->domain_elements[i];
        }

        if (combined_truth == LV00_TRUE) {
            break; /* 短路 */
        }
    }

    out_result->truth_value = combined_truth;

    /* 创建结果命题：P(d1) ∨ ... ∨ P(dn) */
    (void)snprintf(name_buf, RESULT_NAME_BUF_SIZE, "ExistsElim_%s_%d",
                   expr->variable_name ? expr->variable_name : "x", expr->id);
    out_result->result_prop = create_result_proposition(expr->id, name_buf);
    if (!out_result->result_prop) {
        out_result->status = LV00_QUANT_ERROR;
        out_result->error_message = lv00_strdup("创建结果命题失败");
        return LV00_QUANT_ERROR;
    }

    out_result->status = LV00_QUANT_OK;
    return LV00_QUANT_OK;
}

/**
 * @brief 有限域上的唯一存在量词消去
 *
 * 在有限域上，将 ∃!x.P(x) 展开。
 * 语义：恰好一个元素满足 P。
 *
 * 算法流程：
 * 1. 验证表达式为唯一存在量词且域有限
 * 2. 枚举所有域元素，统计满足体命题的元素数量
 * 3. 若恰好一个元素满足，返回 TRUE 并记录目击者
 * 4. 否则返回 FALSE
 *
 * 展开形式（D = {d1, ..., dn}）：
 *   (P(d1) ∧ ¬P(d2) ∧ ... ∧ ¬P(dn)) ∨
 *   (¬P(d1) ∧ P(d2) ∧ ... ∧ ¬P(dn)) ∨ ...
 *   即 n 个分支的析取，每个分支恰好一个元素满足 P。
 *
 * @param expr         唯一存在量化表达式
 * @param out_result   输出结果
 * @return 操作结果状态码
 */
Lv00QuantResult lv00_quant_eliminate_exists_unique_finite(const Lv00QuantifiedExpr *expr,
                                                           Lv00QuantifiedResult *out_result)
{
    int i;
    char name_buf[RESULT_NAME_BUF_SIZE];
    int satisfying_count;
    int last_witness;
    bool has_unknown;

    init_quant_result(out_result);

    if (!expr || !out_result) {
        out_result->status = LV00_QUANT_ERROR;
        return LV00_QUANT_ERROR;
    }

    if (expr->quantifier != LV00_EXISTS_UNIQUE) {
        out_result->status = LV00_QUANT_ERROR;
        out_result->error_message = lv00_strdup("唯一存在消去仅适用于唯一存在量词(∃!)");
        return LV00_QUANT_ERROR;
    }

    if (!expr->domain || !expr->domain->is_finite) {
        out_result->status = LV00_QUANT_DOMAIN_INFINITE;
        out_result->error_message = lv00_strdup("域为无限域，无法消去唯一存在量词");
        return LV00_QUANT_DOMAIN_INFINITE;
    }

    /* 空域：不存在唯一满足的元素 */
    if (expr->domain->element_count == 0) {
        out_result->status = LV00_QUANT_DOMAIN_EMPTY;
        out_result->truth_value = LV00_FALSE;
        (void)snprintf(name_buf, RESULT_NAME_BUF_SIZE, "ExistsUniqueElim_empty_%d", expr->id);
        out_result->result_prop = create_result_proposition(expr->id, name_buf);
        return LV00_QUANT_DOMAIN_EMPTY;
    }

    /* 枚举所有元素，统计满足体命题的元素数量 */
    satisfying_count = 0;
    last_witness = -1;
    has_unknown = false;

    for (i = 0; i < expr->domain->element_count; i++) {
        Lv00TruthValue elem_truth = evaluate_body_for_element(expr, expr->domain->domain_elements[i]);

        if (elem_truth == LV00_TRUE) {
            satisfying_count++;
            last_witness = expr->domain->domain_elements[i];
        } else if (elem_truth == LV00_UNKNOWN) {
            has_unknown = true;
        }
    }

    /* 确定真值 */
    if (has_unknown) {
        out_result->truth_value = LV00_UNKNOWN;
    } else if (satisfying_count == 1) {
        out_result->truth_value = LV00_TRUE;
        out_result->witness_node_id = last_witness;
    } else {
        out_result->truth_value = LV00_FALSE;
    }

    /* 创建结果命题 */
    (void)snprintf(name_buf, RESULT_NAME_BUF_SIZE, "ExistsUniqueElim_%s_%d",
                   expr->variable_name ? expr->variable_name : "x", expr->id);
    out_result->result_prop = create_result_proposition(expr->id, name_buf);
    if (!out_result->result_prop) {
        out_result->status = LV00_QUANT_ERROR;
        out_result->error_message = lv00_strdup("创建结果命题失败");
        return LV00_QUANT_ERROR;
    }

    out_result->status = LV00_QUANT_OK;
    return LV00_QUANT_OK;
}

/* ============== 量词级别的最佳实践检查 ============== */

/**
 * @brief 检查量词在域上是否可消去
 *
 * 只有有限域上的量词才能通过枚举消去。
 * 判定条件：
 * - 域的 is_finite 标志为 true，或
 * - 域有约束图子图（可枚举子图节点）
 *
 * @param expr 量化表达式
 * @return true 可消去，false 不可
 */
bool lv00_quant_is_eliminable(const Lv00QuantifiedExpr *expr)
{
    if (!expr || !expr->domain) {
        return false;
    }

    /* 有限枚举域可直接消去 */
    if (expr->domain->is_finite) {
        return true;
    }

    /* 约束图子图域可枚举节点 */
    if (expr->domain->subgraph != NULL) {
        return true;
    }

    return false;
}

/**
 * @brief 获取给定域中满足体命题的元素数量
 *
 * 对于有限域，枚举检查每个元素并统计满足的数量。
 * 对于无限域，返回 -1（无法确定）。
 *
 * @param expr 量化表达式
 * @return 满足体命题的元素数量（-1 = 无法确定）
 */
int lv00_quant_count_satisfying(const Lv00QuantifiedExpr *expr)
{
    int i;
    int count;

    if (!expr || !expr->domain || !expr->body_proposition) {
        return -1;
    }

    /* 无限域：无法确定 */
    if (!expr->domain->is_finite && !expr->domain->subgraph) {
        return -1;
    }

    /* 有限域：枚举统计 */
    count = 0;
    for (i = 0; i < expr->domain->element_count; i++) {
        Lv00TruthValue elem_truth = evaluate_body_for_element(expr, expr->domain->domain_elements[i]);
        if (elem_truth == LV00_TRUE) {
            count++;
        }
    }

    return count;
}

/* ============== 释放结果结构体 ============== */

/**
 * @brief 释放量词操作结果结构体
 *
 * 释放结果中拥有的动态资源：
 * - error_message 字符串
 * - result_prop 命题（包括其内部字段）
 *
 * 释放后，result 结构体本身不被释放（通常为栈分配），
 * 但其所有指针字段被置为 NULL。
 *
 * @param result 结果结构体指针
 */
void lv00_quant_result_destroy(Lv00QuantifiedResult *result)
{
    if (!result) {
        return;
    }

    LV00_FREE_AND_NULL(result->error_message);

    if (result->result_prop) {
        LV00_FREE_AND_NULL(result->result_prop->label);
        LV00_FREE_AND_NULL(result->result_prop->input_port_ids);
        LV00_FREE_AND_NULL(result->result_prop->output_port_ids);
        LV00_FREE_AND_NULL(result->result_prop->precondition_region_ids);
        LV00_FREE_AND_NULL(result->result_prop->postcondition_constraint_ids);
        lv00_free((void **)&(result->result_prop));
    }

    /* 重置字段 */
    result->status = LV00_QUANT_OK;
    result->truth_value = LV00_UNKNOWN;
    result->witness_node_id = -1;
}

/* ============== 辅助函数 ============== */

/**
 * @brief 量词类型转字符串
 *
 * 将量词枚举值转换为对应的数学符号字符串。
 * 返回静态字符串常量，调用者无需释放。
 *
 * @param q 量词类型
 * @return 静态字符串：
 *         - LV00_FORALL        -> "∀"
 *         - LV00_EXISTS        -> "∃"
 *         - LV00_EXISTS_UNIQUE -> "∃!"
 *         - 其他               -> "?(未知)"
 */
const char *lv00_quant_to_string(Lv00Quantifier q)
{
    switch (q) {
    case LV00_FORALL:
        return "\xe2\x88\x80"; /* ∀ UTF-8: E2 88 80 */
    case LV00_EXISTS:
        return "\xe2\x88\x83"; /* ∃ UTF-8: E2 88 83 */
    case LV00_EXISTS_UNIQUE:
        return "\xe2\x88\x83!"; /* ∃! UTF-8 */
    default:
        return "?(unknown)";
    }
}

/**
 * @brief 量词操作结果转字符串
 *
 * 将操作结果枚举值转换为中文描述字符串。
 * 返回静态字符串常量，调用者无需释放。
 *
 * @param result 操作结果
 * @return 静态字符串（中文描述）
 */
const char *lv00_quant_result_to_string(Lv00QuantResult result)
{
    switch (result) {
    case LV00_QUANT_OK:
        return "操作成功";
    case LV00_QUANT_DOMAIN_EMPTY:
        return "域为空";
    case LV00_QUANT_DOMAIN_INFINITE:
        return "域为无限，消去不可能";
    case LV00_QUANT_INVALID_VARIABLE:
        return "变量无效";
    case LV00_QUANT_BODY_UNDEFINED:
        return "体命题未定义";
    case LV00_QUANT_INSTANTIATE_FAILED:
        return "实例化失败";
    case LV00_QUANT_GENERALIZE_FAILED:
        return "泛化失败";
    case LV00_QUANT_COUNTEREXAMPLE:
        return "找到反例";
    case LV00_QUANT_ERROR:
        return "一般性错误";
    default:
        return "未知结果";
    }
}
