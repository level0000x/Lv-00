/**
 * @file quantifier.c
 * @brief 量词系统实现 —— 全称/存在/唯一存在量词的形式化处理
 *
 * @details 本文件实现 quantifier.h 中声明的所有接口，提供：
 *          - 量化域（lvDomain）的创建、销毁、元素管理
 *          - 量化表达式（lvQuantifiedExpr）的创建、销毁、评估
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
 *   - lv/quantifier.h       : 量词系统公共接口定义
 *   - lv/lv_utils.h       : 统一内存分配器（lv_malloc / lv_free / lv_calloc）
 *   - lv/three_valued_logic.h : 三值逻辑运算（Kleene 强三值逻辑）
 *   - lv/constraint_graph.h : 约束图数据结构（域的子图表示）
 *   - lv/proof.h            : Proposition 结构定义
 */

#include "lv/quantifier.h"
#include "lv/lv_xmacro.h"
#include "lv/lv_lifecycle.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/proof.h"

#include "lv/error_codes.h"
#include "lv/lv_utils.h"
#include "lv/three_valued_logic.h"
#include "quantifier_internal.h"

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

static bool domain_ensure_capacity(lvDomain *domain, int needed);
static bool instantiate_ensure_capacity(lvQuantifiedExpr *expr, int needed);
struct Proposition *create_result_proposition(int id, const char *name);
lvTruthValue evaluate_body_for_element(const lvQuantifiedExpr *expr, int element_id);
void init_quant_result(lvQuantifiedResult *result);

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
static bool domain_ensure_capacity(lvDomain *domain, int needed) {
    /* 统一迁移至 lv_ensure_capacity（倍增策略/溢出检查/失败语义一致） */
    return lv_ensure_capacity((void **) &domain->domain_elements, needed, &domain->element_capacity, sizeof(int), 0);
}

/**
 * @brief 确保实例化ID数组有足够容量
 *
 * @param expr    量化表达式
 * @param needed  需要的最小容量
 * @return true 成功，false 失败
 */
static bool instantiate_ensure_capacity(lvQuantifiedExpr *expr, int needed) {
    /* 统一迁移至 lv_ensure_capacity（倍增策略/溢出检查/失败语义一致） */
    return lv_ensure_capacity((void **) &expr->instantiated_ids, needed, &expr->instantiated_capacity, sizeof(int), 0);
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
struct Proposition *create_result_proposition(int id, const char *name) {
    struct Proposition *prop = (struct Proposition *) lv_calloc(1, sizeof(struct Proposition));
    if (!prop) {
        return NULL;
    }

    prop->id = id;
    if (name) {
        prop->label = lv_strdup(name);
        if (!prop->label) {
            lv_free((void **) &prop);
            return NULL;
        }
    }

    return prop;
}

/**
 * @brief 评估体命题在指定元素上的真值
 *
 * 评估体命题在指定元素上的真值：
 * 检查体命题的 id 有效性、前置条件区域、输出端口、
 * 子命题以及变量节点 ID 是否与 element_id 关联。
 *
 * @param expr        量化表达式
 * @param element_id  要评估的元素ID
 * @return 三值真值
 */
lvTruthValue evaluate_body_for_element(const lvQuantifiedExpr *expr, int element_id) {
    if (!expr || !expr->body_proposition) {
        return lv_UNKNOWN;
    }

    /* 将元素代入体命题进行评估：
     * 检查体命题是否包含对量化变量的引用，
     * 如果 variable_node_id 与 element_id 匹配，则视为满足。
     * 完整实现应执行真正的变量替换和命题评估。 */
    if (expr->body_proposition->id <= 0) {
        return lv_UNKNOWN;
    }

    /* 检查体命题的 precondition_region_ids 或 postcondition_constraint_ids
     * 中是否包含与 element_id 相关的约束 */
    if (expr->body_proposition->precondition_region_ids) {
        /* 检查前置条件区域中是否包含 element_id */
        for (int i = 0; i < (int) expr->body_proposition->precondition_region_count; i++) {
            if (expr->body_proposition->precondition_region_ids[i] == element_id) {
                return lv_TRUE;
            }
        }
    }

    /* 检查体命题的 output_port_ids 是否与 element_id 关联 */
    if (expr->body_proposition->output_port_ids) {
        for (int i = 0; i < (int) expr->body_proposition->output_port_count; i++) {
            if (expr->body_proposition->output_port_ids[i] == element_id) {
                return lv_TRUE;
            }
        }
    }

    /* 检查子命题：递归检查子命题是否引用了 element_id */
    if (expr->body_proposition->sub_props && expr->body_proposition->sub_prop_count > 0) {
        for (int i = 0; i < expr->body_proposition->sub_prop_count; i++) {
            struct Proposition *sub = expr->body_proposition->sub_props[i];
            if (sub && sub->id == element_id) {
                return lv_TRUE;
            }
        }
    }

    /* 如果变量节点 ID 与 element_id 匹配，且体命题有效，视为 TRUE */
    if (expr->variable_node_id == element_id) {
        return lv_TRUE;
    }

    /* 默认：体命题有效但无法确定元素代入结果 */
    return lv_UNKNOWN;
}

/**
 * @brief 初始化量词操作结果结构体
 *
 * 将结果的所有字段置为安全的初始状态。
 *
 * @param result  结果结构体指针
 */
void init_quant_result(lvQuantifiedResult *result) {
    if (!result) {
        return;
    }

    result->status = lv_QUANT_OK;
    result->truth_value = lv_UNKNOWN;
    result->witness_node_id = -1;
    result->error_message = NULL;
    result->result_prop = NULL;
}

/* ============== 域管理 API ============== */

/**
 * @brief 创建命名域
 *
 * 分配并初始化一个 lvDomain 结构，设置域名和默认值。
 * 新创建的域没有元素，is_finite 默认为 false（除非后续添加元素）。
 *
 * @param id          域ID
 * @param domain_name 域名称（内部复制，可为 NULL）
 * @return 新分配的域，失败返回 NULL
 */
lvDomain *lv_quant_domain_create(int id, const char *domain_name) {
    lvDomain *domain = (lvDomain *) lv_calloc(1, sizeof(lvDomain));
    if (!domain) {
        return NULL;
    }

    domain->id = id;
    domain->domain_name = domain_name ? lv_strdup(domain_name) : NULL;
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
lvDomain *lv_quant_domain_create_finite(int id, const int *elements, int count) {
    lvDomain *domain;

    if (count < 0) {
        return NULL;
    }

    domain = (lvDomain *) lv_calloc(1, sizeof(lvDomain));
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
        domain->domain_elements = (int *) lv_malloc((size_t) count * sizeof(int));
        if (!domain->domain_elements) {
            lv_free((void **) &domain);
            return NULL;
        }
        memcpy(domain->domain_elements, elements, (size_t) count * sizeof(int));
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
bool lv_quant_domain_add_element(lvDomain *domain, int element) {
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
bool lv_quant_domain_add_elements(lvDomain *domain, const int *elements, int count) {
    int i;

    if (!domain || !elements || count <= 0) {
        return false;
    }

    for (i = 0; i < count; i++) {
        if (!lv_quant_domain_add_element(domain, elements[i])) {
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
bool lv_quant_domain_contains(const lvDomain *domain, int element) {
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
int lv_quant_domain_size(const lvDomain *domain) {
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
void lv_quant_domain_destroy(lvDomain *domain) {
    if (!domain) {
        return;
    }

    lv_FREE_AND_NULL(domain->domain_name);
    lv_FREE_AND_NULL(domain->domain_elements);
    /* subgraph 的所有权不属于域，不在此处释放 */

    lv_free((void **) &domain);
}

