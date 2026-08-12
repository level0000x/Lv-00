/**
 * @file type_check.c
 * @brief 类型检查实现 —— 等价检查、端口兼容性、循环检测、规范化、依赖类型检查
 *
 * @details 本文件从 type_system.c 中拆分而来，封装所有类型检查相关的操作。
 *          通过 extern 引用 type_system.c 中定义的流式上下文和深拷贝函数。
 *
 *          功能包括：
 *          - 类型等价检查（递归结构比较，支持重写引擎）
 *          - 端口类型兼容性检查
 *          - 谓词子类型值检查
 *          - 类型循环检测（DFS + 路径标记）
 *          - 非良基兼容性检查
 *          - 类型规范化（展开别名、实例化变量）
 *          - 依赖类型兼容性检查
 *
 * @author Lv-00 Project
 */

#include "type_system.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv.h"
#include "lv/lv_xmacro.h"
#include "lv/stream.h"

#include "lv_internal.h"
#include "lv_utils.h"
#include "rewrite.h"
#include "lv/lv_strbuf.h"

/* 访问 type_system.c 中定义的流式上下文变量 */
extern lv_THREAD_LOCAL StreamContext *type_system_stream_ctx;

/* type_region_deep_copy / type_region_deep_free 声明统一收口到 type_system.h */

/* ============== 内部辅助宏和前向声明 ============== */

/* visited set 最大容量，防止共享子类型导致指数级时间 */
#define TYPE_EQUIV_MAX_VISITED 256
#define MAX_VISITED TYPE_EQUIV_MAX_VISITED

/* visited set：记录已比较的类型对，防止共享子类型导致指数级时间
 * 使用线程局部存储确保多线程环境下类型等价检查的线程安全性 */
static lv_THREAD_LOCAL struct {
    const TypeRegion *a;
    const TypeRegion *b;
} s_equiv_visited[MAX_VISITED];
static lv_THREAD_LOCAL int s_equiv_visited_count = 0;

/**
 * @brief 检查是否已访问过此类型对，若已访问则直接返回等价
 *
 * 用于类型等价检查中的环路检测。在递归比较两个类型时，
 * 如果发现当前类型对已经被比较过，则直接返回 TYPE_EQUIV_OK
 * 以避免无限递归和指数级时间复杂度。
 */
#define VISITED_CHECK(ta, tb)                                                     \
    do {                                                                          \
        for (int _vi = 0; _vi < s_equiv_visited_count; _vi++) {                   \
            if (s_equiv_visited[_vi].a == (ta) && s_equiv_visited[_vi].b == (tb)) \
                return TYPE_EQUIV_OK;                                             \
        }                                                                         \
        if (s_equiv_visited_count < MAX_VISITED) {                                \
            s_equiv_visited[s_equiv_visited_count].a = (ta);                      \
            s_equiv_visited[s_equiv_visited_count].b = (tb);                      \
            s_equiv_visited_count++;                                              \
        }                                                                         \
    } while (0)

/* 前向声明 */
static TypeEquivResult type_check_equivalence_internal(TypeSystem *ts, TypeRegion *type1, TypeRegion *type2,
                                                       bool use_rewrite, int depth);
static bool type_detect_cycle_dfs(TypeSystem *ts, TypeRegion *current, bool *visited, bool *on_stack);
static bool type_normalize_internal(TypeSystem *ts, TypeRegion *type, TypeRegion **out_normalized, int depth);

/**
 * 递归检查两个类型的等价性（用于二元复合类型）
 * 消除 TYPE_KIND_FUNCTION/PRODUCT/SUM 中的重复代码
 *
 * @param ts       类型系统
 * @param type1    第一个类型
 * @param type2    第二个类型
 * @param sub1     第一个类型的第一个子类型
 * @param sub2     第一个类型的第二个子类型
 * @param other1   第二个类型的第一个子类型
 * @param other2   第二个类型的第二个子类型
 * @param use_rw   是否使用重写引擎
 * @param d        递归深度
 * @return 类型等价结果
 */
static inline TypeEquivResult check_binary_type_equiv(TypeSystem *ts, TypeRegion *type1, TypeRegion *type2,
                                                      TypeRegion *sub1, TypeRegion *sub2, TypeRegion *other1,
                                                      TypeRegion *other2, bool use_rw, int d) {
    (void)type1;
    (void)type2;
    VISITED_CHECK(sub1, other1);
    TypeEquivResult first = type_check_equivalence_internal(ts, sub1, other1, use_rw, d + 1);
    if (first == TYPE_EQUIV_NOT_EQUIV)
        return TYPE_EQUIV_NOT_EQUIV;
    if (first != TYPE_EQUIV_OK)
        return first;
    VISITED_CHECK(sub2, other2);
    return type_check_equivalence_internal(ts, sub2, other2, use_rw, d + 1);
}

/* ============== VTable 类型分发 ============== */

/* TYPE_KIND 枚举值数量（从 TYPE_KIND_POINT 到 TYPE_KIND_PREDICATE_SUBTYPE 共 10 个） */
#define TYPE_KIND_COUNT 10

/* ---------- 二元等价检查 VTable（switch #1：重写路径中的快捷比较） ---------- */
typedef TypeEquivResult (*BinaryEquivHandler)(TypeSystem *ts, TypeRegion *type1, TypeRegion *type2,
                                              bool use_rewrite, int depth);

static TypeEquivResult binary_equiv_function(TypeSystem *ts, TypeRegion *type1, TypeRegion *type2,
                                             bool use_rewrite, int depth) {
    return check_binary_type_equiv(ts, type1, type2, type1->input_type, type1->output_type,
                                   type2->input_type, type2->output_type, use_rewrite, depth);
}
static TypeEquivResult binary_equiv_product(TypeSystem *ts, TypeRegion *type1, TypeRegion *type2,
                                            bool use_rewrite, int depth) {
    return check_binary_type_equiv(ts, type1, type2, type1->left_type, type1->right_type,
                                   type2->left_type, type2->right_type, use_rewrite, depth);
}
static TypeEquivResult binary_equiv_sum(TypeSystem *ts, TypeRegion *type1, TypeRegion *type2,
                                        bool use_rewrite, int depth) {
    return check_binary_type_equiv(ts, type1, type2, type1->first_type, type1->second_type,
                                   type2->first_type, type2->second_type, use_rewrite, depth);
}

static BinaryEquivHandler s_binary_equiv_handlers[TYPE_KIND_COUNT] = {
    [TYPE_KIND_FUNCTION] = binary_equiv_function,
    [TYPE_KIND_PRODUCT]  = binary_equiv_product,
    [TYPE_KIND_SUM]      = binary_equiv_sum,
};

/* ---------- 结构等价检查 VTable（switch #2：完整结构比较） ---------- */
typedef TypeEquivResult (*StructEquivHandler)(TypeSystem *ts, TypeRegion *type1, TypeRegion *type2,
                                              bool use_rewrite, int depth);

static TypeEquivResult struct_equiv_primitive(TypeSystem *ts, TypeRegion *type1, TypeRegion *type2,
                                              bool use_rewrite, int depth) {
    (void)ts; (void)type1; (void)type2; (void)use_rewrite; (void)depth;
    return TYPE_EQUIV_OK;
}

static TypeEquivResult struct_equiv_region(TypeSystem *ts, TypeRegion *type1, TypeRegion *type2,
                                           bool use_rewrite, int depth) {
    (void)use_rewrite; (void)depth;
    if (type1->contained_count != type2->contained_count) {
        return TYPE_EQUIV_NOT_EQUIV;
    }
    {
        if (type1->contained_node_ids && type2->contained_node_ids) {
            int count = type1->contained_count;
            int r = lv_int_multiset_equal(type1->contained_node_ids, count,
                                          type2->contained_node_ids, count);
            if (r < 0)
                return TYPE_EQUIV_ERROR;
            return r ? TYPE_EQUIV_OK : TYPE_EQUIV_NOT_EQUIV;
        }

        if (type1->constraint_ids && type2->constraint_ids) {
            if (type1->constraint_count != type2->constraint_count) {
                return TYPE_EQUIV_NOT_EQUIV;
            }

            int count = type1->constraint_count;
            int r = lv_int_multiset_equal(type1->constraint_ids, count,
                                          type2->constraint_ids, count);
            if (r < 0)
                return TYPE_EQUIV_ERROR;
            return r ? TYPE_EQUIV_OK : TYPE_EQUIV_NOT_EQUIV;
        }

        if (!type1->contained_node_ids && !type2->contained_node_ids && !type1->constraint_ids &&
            !type2->constraint_ids) {
            return type1->level == type2->level ? TYPE_EQUIV_OK : TYPE_EQUIV_NOT_EQUIV;
        }

        return TYPE_EQUIV_UNKNOWN;
    }
}

static TypeEquivResult struct_equiv_function(TypeSystem *ts, TypeRegion *type1, TypeRegion *type2,
                                             bool use_rewrite, int depth) {
    VISITED_CHECK(type1->input_type, type2->input_type);
    TypeEquivResult input_result =
        type_check_equivalence_internal(ts, type1->input_type, type2->input_type, use_rewrite, depth + 1);
    if (input_result != TYPE_EQUIV_OK)
        return input_result;
    VISITED_CHECK(type1->output_type, type2->output_type);
    return type_check_equivalence_internal(ts, type1->output_type, type2->output_type, use_rewrite, depth + 1);
}

static TypeEquivResult struct_equiv_product(TypeSystem *ts, TypeRegion *type1, TypeRegion *type2,
                                            bool use_rewrite, int depth) {
    VISITED_CHECK(type1->left_type, type2->left_type);
    TypeEquivResult left_result =
        type_check_equivalence_internal(ts, type1->left_type, type2->left_type, use_rewrite, depth + 1);
    if (left_result != TYPE_EQUIV_OK)
        return left_result;
    VISITED_CHECK(type1->right_type, type2->right_type);
    return type_check_equivalence_internal(ts, type1->right_type, type2->right_type, use_rewrite, depth + 1);
}

static TypeEquivResult struct_equiv_sum(TypeSystem *ts, TypeRegion *type1, TypeRegion *type2,
                                        bool use_rewrite, int depth) {
    VISITED_CHECK(type1->first_type, type2->first_type);
    TypeEquivResult first_result =
        type_check_equivalence_internal(ts, type1->first_type, type2->first_type, use_rewrite, depth + 1);
    if (first_result != TYPE_EQUIV_OK)
        return first_result;
    VISITED_CHECK(type1->second_type, type2->second_type);
    return type_check_equivalence_internal(ts, type1->second_type, type2->second_type, use_rewrite, depth + 1);
}

static TypeEquivResult struct_equiv_variable(TypeSystem *ts, TypeRegion *type1, TypeRegion *type2,
                                             bool use_rewrite, int depth) {
    (void)ts; (void)use_rewrite; (void)depth;
    if (type1->variable_id == type2->variable_id) {
        return TYPE_EQUIV_OK;
    }
    return TYPE_EQUIV_UNKNOWN;
}

static TypeEquivResult struct_equiv_dependent(TypeSystem *ts, TypeRegion *type1, TypeRegion *type2,
                                              bool use_rewrite, int depth) {
    if (type1->param_node_id <= 0 || type2->param_node_id <= 0) {
        return TYPE_EQUIV_UNKNOWN;
    }
    if (!type1->body_type || !type2->body_type) {
        return TYPE_EQUIV_ERROR;
    }
    if (type1->param_node_id == type2->param_node_id) {
        VISITED_CHECK(type1->body_type, type2->body_type);
        return type_check_equivalence_internal(ts, type1->body_type, type2->body_type, use_rewrite, depth + 1);
    }
    {
        TypeRegion canonical_var;
        memset(&canonical_var, 0, sizeof(canonical_var));
        canonical_var.kind = TYPE_KIND_VARIABLE;
        canonical_var.variable_id = -1;

        TypeRegion *norm_body1 = NULL;
        bool sub1_ok = type_substitute_variable(ts, type1->body_type, type1->param_node_id, &canonical_var, &norm_body1);

        TypeRegion *norm_body2 = NULL;
        bool sub2_ok = type_substitute_variable(ts, type2->body_type, type2->param_node_id, &canonical_var, &norm_body2);

        if (sub1_ok && sub2_ok && norm_body1 && norm_body2) {
            VISITED_CHECK(norm_body1, norm_body2);
            TypeEquivResult body_result =
                type_check_equivalence_internal(ts, norm_body1, norm_body2, use_rewrite, depth + 1);
            type_region_deep_free(norm_body1);
            type_region_deep_free(norm_body2);
            return body_result;
        }

        if (norm_body1) type_region_deep_free(norm_body1);
        if (norm_body2) type_region_deep_free(norm_body2);

        if (type1->body_type->kind != type2->body_type->kind) {
            return TYPE_EQUIV_NOT_EQUIV;
        }

        VISITED_CHECK(type1->body_type, type2->body_type);
        TypeEquivResult body_result =
            type_check_equivalence_internal(ts, type1->body_type, type2->body_type, use_rewrite, depth + 1);
        if (body_result == TYPE_EQUIV_OK) {
            return TYPE_EQUIV_OK;
        }
        return TYPE_EQUIV_UNKNOWN;
    }
}

static StructEquivHandler s_struct_equiv_handlers[TYPE_KIND_COUNT] = {
    [TYPE_KIND_POINT]          = struct_equiv_primitive,
    [TYPE_KIND_LINE_SEGMENT]   = struct_equiv_primitive,
    [TYPE_KIND_BOTTOM]         = struct_equiv_primitive,
    [TYPE_KIND_REGION]         = struct_equiv_region,
    [TYPE_KIND_FUNCTION]       = struct_equiv_function,
    [TYPE_KIND_PRODUCT]        = struct_equiv_product,
    [TYPE_KIND_SUM]            = struct_equiv_sum,
    [TYPE_KIND_VARIABLE]       = struct_equiv_variable,
    [TYPE_KIND_DEPENDENT]      = struct_equiv_dependent,
};

/* ---------- 循环检测 VTable（switch #4） ---------- */
typedef bool (*CycleDetectHandler)(TypeSystem *ts, TypeRegion *current, bool *visited, bool *on_stack);

static bool cycle_detect_function(TypeSystem *ts, TypeRegion *current, bool *visited, bool *on_stack) {
    bool has_cycle = false;
    if (current->input_type) {
        has_cycle = type_detect_cycle_dfs(ts, current->input_type, visited, on_stack);
    }
    if (!has_cycle && current->output_type) {
        has_cycle = type_detect_cycle_dfs(ts, current->output_type, visited, on_stack);
    }
    return has_cycle;
}

static bool cycle_detect_product(TypeSystem *ts, TypeRegion *current, bool *visited, bool *on_stack) {
    bool has_cycle = false;
    if (current->left_type) {
        has_cycle = type_detect_cycle_dfs(ts, current->left_type, visited, on_stack);
    }
    if (!has_cycle && current->right_type) {
        has_cycle = type_detect_cycle_dfs(ts, current->right_type, visited, on_stack);
    }
    return has_cycle;
}

static bool cycle_detect_sum(TypeSystem *ts, TypeRegion *current, bool *visited, bool *on_stack) {
    bool has_cycle = false;
    if (current->first_type) {
        has_cycle = type_detect_cycle_dfs(ts, current->first_type, visited, on_stack);
    }
    if (!has_cycle && current->second_type) {
        has_cycle = type_detect_cycle_dfs(ts, current->second_type, visited, on_stack);
    }
    return has_cycle;
}

static bool cycle_detect_dependent(TypeSystem *ts, TypeRegion *current, bool *visited, bool *on_stack) {
    bool has_cycle = false;
    if (current->body_type) {
        has_cycle = type_detect_cycle_dfs(ts, current->body_type, visited, on_stack);
    }
    return has_cycle;
}

static bool cycle_detect_region(TypeSystem *ts, TypeRegion *current, bool *visited, bool *on_stack) {
    bool has_cycle = false;
    if (current->aliased_type) {
        has_cycle = type_detect_cycle_dfs(ts, current->aliased_type, visited, on_stack);
    }
    return has_cycle;
}

static bool cycle_detect_variable(TypeSystem *ts, TypeRegion *current, bool *visited, bool *on_stack) {
    bool has_cycle = false;
    if (current->variable_id > 0) {
        for (int i = 0; i < ts->type_var_count; i++) {
            if (ts->type_vars[i] && ts->type_vars[i]->id == current->variable_id) {
                if (ts->type_vars[i]->bound_type) {
                    has_cycle = type_detect_cycle_dfs(ts, ts->type_vars[i]->bound_type, visited, on_stack);
                }
                break;
            }
        }
    }
    return has_cycle;
}

static bool cycle_detect_none(TypeSystem *ts, TypeRegion *current, bool *visited, bool *on_stack) {
    (void)ts; (void)current; (void)visited; (void)on_stack;
    return false;
}

static CycleDetectHandler s_cycle_detect_handlers[TYPE_KIND_COUNT] = {
    [TYPE_KIND_FUNCTION]     = cycle_detect_function,
    [TYPE_KIND_PRODUCT]      = cycle_detect_product,
    [TYPE_KIND_SUM]          = cycle_detect_sum,
    [TYPE_KIND_DEPENDENT]    = cycle_detect_dependent,
    [TYPE_KIND_REGION]       = cycle_detect_region,
    [TYPE_KIND_VARIABLE]     = cycle_detect_variable,
    [TYPE_KIND_POINT]        = cycle_detect_none,
    [TYPE_KIND_LINE_SEGMENT] = cycle_detect_none,
    [TYPE_KIND_BOTTOM]       = cycle_detect_none,
};

/* ---------- 类型规范化 VTable（switch #5） ---------- */
typedef bool (*NormalizeHandler)(TypeSystem *ts, TypeRegion *type, TypeRegion **out_normalized, int depth);

static bool normalize_function(TypeSystem *ts, TypeRegion *type, TypeRegion **out_normalized, int depth) {
    TypeRegion *norm_input = NULL;
    TypeRegion *norm_output = NULL;

    if (type->input_type) {
        type_normalize_internal(ts, type->input_type, &norm_input, depth + 1);
    }
    if (type->output_type) {
        type_normalize_internal(ts, type->output_type, &norm_output, depth + 1);
    }

    if (norm_input || norm_output) {
        *out_normalized = type_create_function(ts, norm_input ? norm_input : type->input_type,
                                               norm_output ? norm_output : type->output_type);
    } else {
        *out_normalized = type;
    }
    return true;
}

static bool normalize_product(TypeSystem *ts, TypeRegion *type, TypeRegion **out_normalized, int depth) {
    TypeRegion *norm_left = NULL;
    TypeRegion *norm_right = NULL;

    if (type->left_type) {
        type_normalize_internal(ts, type->left_type, &norm_left, depth + 1);
    }
    if (type->right_type) {
        type_normalize_internal(ts, type->right_type, &norm_right, depth + 1);
    }

    if (norm_left || norm_right) {
        *out_normalized = type_create_product(ts, norm_left ? norm_left : type->left_type,
                                              norm_right ? norm_right : type->right_type);
    } else {
        *out_normalized = type;
    }
    return true;
}

static bool normalize_dependent(TypeSystem *ts, TypeRegion *type, TypeRegion **out_normalized, int depth) {
    TypeRegion *norm_body = NULL;

    if (type->body_type) {
        type_normalize_internal(ts, type->body_type, &norm_body, depth + 1);
    }

    if (norm_body) {
        *out_normalized = type_create_dependent(ts, type->param_node_id, norm_body);
    } else {
        *out_normalized = type;
    }
    return true;
}

static bool normalize_identity(TypeSystem *ts, TypeRegion *type, TypeRegion **out_normalized, int depth) {
    (void)ts; (void)depth;
    *out_normalized = type;
    return true;
}

static NormalizeHandler s_normalize_handlers[TYPE_KIND_COUNT] = {
    [TYPE_KIND_FUNCTION]   = normalize_function,
    [TYPE_KIND_PRODUCT]    = normalize_product,
    [TYPE_KIND_DEPENDENT]  = normalize_dependent,
    [TYPE_KIND_POINT]      = normalize_identity,
    [TYPE_KIND_LINE_SEGMENT] = normalize_identity,
    [TYPE_KIND_REGION]     = normalize_identity,
    [TYPE_KIND_SUM]        = normalize_identity,
    [TYPE_KIND_VARIABLE]   = normalize_identity,
    [TYPE_KIND_BOTTOM]     = normalize_identity,
};

/* ---------- 依赖类型检查 VTable（switch #6） ---------- */
typedef bool (*DependentCheckHandler)(const TypeSystem *ts, const TypeRegion *output_type,
                                      const TypeRegion *input_type, const SymbolicCoord **input_values);

static bool dep_check_primitive(const TypeSystem *ts, const TypeRegion *output_type,
                                const TypeRegion *input_type, const SymbolicCoord **input_values) {
    (void)ts; (void)output_type; (void)input_type; (void)input_values;
    return true;
}

static bool dep_check_function(const TypeSystem *ts, const TypeRegion *output_type,
                               const TypeRegion *input_type, const SymbolicCoord **input_values) {
    bool input_ok = true, output_ok = true;
    if (output_type->input_type && input_type->input_type) {
        input_ok = type_check_dependent(ts, output_type->input_type, input_type->input_type, input_values);
    }
    if (input_ok && output_type->output_type && input_type->output_type) {
        output_ok = type_check_dependent(ts, output_type->output_type, input_type->output_type, input_values);
    }
    return (input_ok && output_ok);
}

static bool dep_check_product(const TypeSystem *ts, const TypeRegion *output_type,
                              const TypeRegion *input_type, const SymbolicCoord **input_values) {
    bool left_ok = true, right_ok = true;
    if (output_type->left_type && input_type->left_type) {
        left_ok = type_check_dependent(ts, output_type->left_type, input_type->left_type, input_values);
    }
    if (left_ok && output_type->right_type && input_type->right_type) {
        right_ok = type_check_dependent(ts, output_type->right_type, input_type->right_type, input_values);
    }
    return (left_ok && right_ok);
}

static bool dep_check_sum(const TypeSystem *ts, const TypeRegion *output_type,
                          const TypeRegion *input_type, const SymbolicCoord **input_values) {
    bool first_ok = true, second_ok = true;
    if (output_type->first_type && input_type->first_type) {
        first_ok = type_check_dependent(ts, output_type->first_type, input_type->first_type, input_values);
    }
    if (first_ok && output_type->second_type && input_type->second_type) {
        second_ok = type_check_dependent(ts, output_type->second_type, input_type->second_type, input_values);
    }
    return (first_ok && second_ok);
}

static bool dep_check_false(const TypeSystem *ts, const TypeRegion *output_type,
                            const TypeRegion *input_type, const SymbolicCoord **input_values) {
    (void)ts; (void)output_type; (void)input_type; (void)input_values;
    return false;
}

static DependentCheckHandler s_dependent_check_handlers[TYPE_KIND_COUNT] = {
    [TYPE_KIND_POINT]          = dep_check_primitive,
    [TYPE_KIND_LINE_SEGMENT]   = dep_check_primitive,
    [TYPE_KIND_REGION]         = dep_check_primitive,
    [TYPE_KIND_FUNCTION]       = dep_check_function,
    [TYPE_KIND_PRODUCT]        = dep_check_product,
    [TYPE_KIND_SUM]            = dep_check_sum,
    [TYPE_KIND_VARIABLE]       = dep_check_primitive,
    [TYPE_KIND_BOTTOM]         = dep_check_primitive,
    [TYPE_KIND_DEPENDENT]      = dep_check_false,
    [TYPE_KIND_PREDICATE_SUBTYPE] = dep_check_false,
};

/* ============== 谓词子类型检查 ============== */

bool type_check_predicate_subtype_value(TypeSystem *ts, TypeRegion *subtype, int node_id) {
    if (!ts || !subtype || subtype->kind != TYPE_KIND_PREDICATE_SUBTYPE)
        return false;

    /* 首先检查值是否属于基类型 */
    TypeRegion *base = subtype->base_type;
    if (!base)
        return false;

    /* 获取节点的当前类型 */
    TypeRegion *node_type = type_get_node_type(ts, node_id);
    if (!node_type)
        return false;

    /* 检查节点类型是否与基类型兼容 */
    TypeEquivResult equiv = type_check_equivalence(ts, node_type, base, false);
    if (equiv != TYPE_EQUIV_OK)
        return false;

    /* 如果有关联的约束ID，检查约束是否满足 */
    if (subtype->predicate_constraint_id >= 0) {
        /* 通过约束ID列表检查约束是否已被验证满足 */
        bool constraint_satisfied = false;
        if (subtype->constraint_ids && subtype->constraint_count > 0) {
            for (int ci = 0; ci < subtype->constraint_count; ci++) {
                if (subtype->constraint_ids[ci] == subtype->predicate_constraint_id) {
                    constraint_satisfied = true;
                    break;
                }
            }
        }
        /* 同时检查节点的类型区域是否包含该约束ID */
        if (!constraint_satisfied && node_type->constraint_ids && node_type->constraint_count > 0) {
            for (int ci = 0; ci < node_type->constraint_count; ci++) {
                if (node_type->constraint_ids[ci] == subtype->predicate_constraint_id) {
                    constraint_satisfied = true;
                    break;
                }
            }
        }
        return constraint_satisfied;
    }

    return true; /* 基类型兼容即可 */
}

/* ============== 类型等价检查 ============== */

/* 内部递归辅助函数，带深度限制 */
static TypeEquivResult type_check_equivalence_internal(TypeSystem *ts, TypeRegion *type1, TypeRegion *type2,
                                                       bool use_rewrite, int depth) {
    if (!ts || !type1 || !type2)
        return TYPE_EQUIV_ERROR;

    /* 递归深度限制检查 */
    int equiv_max = lv_config_get_int(LV_CFG_TYPE_EQUIV_MAX_DEPTH, 16);
    if (depth >= equiv_max) {
        return TYPE_EQUIV_UNKNOWN;
    }

    /* 首次进入时重置 visited set（depth == 0 表示顶层调用） */
    if (depth == 0) {
        s_equiv_visited_count = 0;
    }

    /* 对入口类型对执行 visited 检查 */
    VISITED_CHECK(type1, type2);

    /* 相同指针 */
    if (type1 == type2)
        return TYPE_EQUIV_OK;

    /* 使用重写引擎进行归一化比较 */
    if (use_rewrite && ts->rewrite_rules && ts->rewrite_rule_count > 0) {
        /*
         * 重写引擎集成策略：
         * 尝试通过重写规则将两个类型归一化。
         * 由于重写引擎操作于约束图层面，而类型等价检查操作于类型层面，
         * 这里我们检查类型是否可以通过重写规则推导为等价。
         *
         * 当类型结构无法直接比较时，标记为需要交互式证明。
         */

        /* 检查类型种类 */
        if (type1->kind == type2->kind) {
            /* 类型种类相同：通过 VTable 对复合类型进行递归的深度受限归一化比较 */
            if (type1->kind >= 0 && type1->kind < TYPE_KIND_COUNT) {
                BinaryEquivHandler handler = s_binary_equiv_handlers[type1->kind];
                if (handler) {
                    return handler(ts, type1, type2, use_rewrite, depth);
                }
            }
            /* 非复合类型，种类相同，继续执行下面的结构比较逻辑 */
        } else if (type1->kind == TYPE_KIND_VARIABLE || type2->kind == TYPE_KIND_VARIABLE) {
            /* 类型变量的等价性检查。
             * 当类型变量已被实例化（bound_type != NULL）时，应递归检查
             * bound_type 与目标类型的等价性，而非无条件返回 TYPE_EQUIV_OK。
             * 只有在变量未被实例化时（自由类型变量），才返回 TYPE_EQUIV_OK，
             * 因为自由变量可以与任意类型统一。 */
            TypeRegion *var_type = (type1->kind == TYPE_KIND_VARIABLE) ? type1 : type2;
            TypeRegion *other_type = (type1->kind == TYPE_KIND_VARIABLE) ? type2 : type1;

            /* 查找类型变量对应的 TypeVariable，检查是否已实例化 */
            if (var_type->variable_id >= 0 && var_type->variable_id < ts->type_var_count) {
                TypeVariable *tv = ts->type_vars[var_type->variable_id];
                if (tv && tv->bound_type) {
                    /* 变量已实例化，递归检查 bound_type 与目标类型的等价性 */
                    VISITED_CHECK(tv->bound_type, other_type);
                    return type_check_equivalence_internal(ts, tv->bound_type, other_type, use_rewrite, depth + 1);
                }
            }
            /* 变量未被实例化（自由类型变量），可以匹配任意类型 */
            return TYPE_EQUIV_OK;
        } else {
            /* 类型种类不同（非变量情况），无法通过重写引擎归一化 */
            return TYPE_EQUIV_NOT_EQUIV;
        }

        /* 继续执行下面的结构比较逻辑 */
    }

    /* 检查别名 */
    if (type1->alias_name && type2->alias_name) {
        if (strcmp(type1->alias_name, type2->alias_name) == 0) {
            return TYPE_EQUIV_OK;
        }
    }

    /* 检查类型种类 */
    if (type1->kind != type2->kind) {
        /* 类型变量可以匹配任意类型，但若已实例化则需递归检查。 */
        if (type1->kind == TYPE_KIND_VARIABLE || type2->kind == TYPE_KIND_VARIABLE) {
            TypeRegion *var_type = (type1->kind == TYPE_KIND_VARIABLE) ? type1 : type2;
            TypeRegion *other_type = (type1->kind == TYPE_KIND_VARIABLE) ? type2 : type1;

            if (var_type->variable_id >= 0 && var_type->variable_id < ts->type_var_count) {
                TypeVariable *tv = ts->type_vars[var_type->variable_id];
                if (tv && tv->bound_type) {
                    /* 变量已实例化，递归检查 bound_type 与目标类型的等价性 */
                    VISITED_CHECK(tv->bound_type, other_type);
                    return type_check_equivalence_internal(ts, tv->bound_type, other_type, use_rewrite, depth + 1);
                }
            }
            /* 变量未被实例化（自由类型变量），可以匹配任意类型 */
            return TYPE_EQUIV_OK;
        }
        return TYPE_EQUIV_NOT_EQUIV;
    }

    /* 通过 VTable 进行结构等价检查 */
    if (type1->kind >= 0 && type1->kind < TYPE_KIND_COUNT) {
        StructEquivHandler handler = s_struct_equiv_handlers[type1->kind];
        if (handler) {
            return handler(ts, type1, type2, use_rewrite, depth);
        }
    }
    /* 未知类型种类 */
    return TYPE_EQUIV_NOT_EQUIV;
}

/**
 * @brief 类型等价检查（公共 API 入口函数）
 *
 * 检查两个类型区域是否结构等价。可选启用重写引擎进行归一化比较。
 * 内部使用带递归深度限制的 type_check_equivalence_internal 函数。
 *
 * @param ts         类型系统指针
 * @param type1      第一个类型区域
 * @param type2      第二个类型区域
 * @param use_rewrite 是否使用重写引擎进行归一化
 * @return 类型等价结果（OK/NOT_EQUIV/UNKNOWN/ERROR）
 */
TypeEquivResult type_check_equivalence(TypeSystem *ts, TypeRegion *type1, TypeRegion *type2, bool use_rewrite) {
    /* 流式事件：等价检查开始 */
    if (type_system_stream_ctx != NULL) {
        stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_INFO, "类型等价检查开始", 0);
    }

    TypeEquivResult result = type_check_equivalence_internal(ts, type1, type2, use_rewrite, 0);

    /* 流式事件：等价检查结果 */
    if (type_system_stream_ctx != NULL) {
        const char *result_str = type_equiv_result_to_string(result);
        lvStrBuf sb = {0};
        lv_strbuf_printf(&sb, "类型等价检查完成: %s", result_str);
        if (result == TYPE_EQUIV_NOT_EQUIV) {
            stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_WARNING, sb.data, 0);
        } else {
            stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_INFO, sb.data, 0);
        }
        lv_strbuf_destroy(&sb);
    }

    return result;
}

/**
 * @brief 端口类型兼容性检查
 *
 * 检查源端口类型与目标端口类型是否兼容。
 * 内部调用 type_check_equivalence 进行等价检查，采用保守策略：
 * 当无法证明等价时返回 TYPE_CHECK_INCOMPATIBLE，确保类型安全。
 *
 * @param ts           类型系统指针
 * @param source_type  源端口类型
 * @param target_type  目标端口类型
 * @return 类型检查结果（OK/MISMATCH/INCOMPATIBLE/ERROR）
 */
TypeCheckResult type_check_port_compatibility(TypeSystem *ts, TypeRegion *source_type, TypeRegion *target_type) {
    if (!ts || !source_type || !target_type)
        return TYPE_CHECK_ERROR;

    /* 流式事件：端口兼容性检查开始 */
    if (type_system_stream_ctx != NULL) {
        stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_INFO, "端口兼容性检查开始", 0);
    }

    /* 检查类型等价 */
    TypeEquivResult equiv = type_check_equivalence(ts, source_type, target_type, true);

    /* 等价结果到检查结果的映射表 */
    static const TypeCheckResult s_equiv_to_check_result[] = {
        [TYPE_EQUIV_OK] = TYPE_CHECK_OK,
        [TYPE_EQUIV_NOT_EQUIV] = TYPE_CHECK_MISMATCH,
        [TYPE_EQUIV_UNKNOWN] = TYPE_CHECK_INCOMPATIBLE,
        [TYPE_EQUIV_ERROR] = TYPE_CHECK_ERROR,
        [TYPE_EQUIV_NEEDS_INTERACTION] = TYPE_CHECK_ERROR,
    };
    TypeCheckResult result;

    if ((int)equiv >= 0 && (int)equiv < (int)(sizeof(s_equiv_to_check_result) / sizeof(s_equiv_to_check_result[0]))) {
        result = s_equiv_to_check_result[equiv];
    } else {
        result = TYPE_CHECK_ERROR;
    }

    /* 流式事件：端口兼容性检查结果 */
    if (type_system_stream_ctx != NULL) {
        const char *result_str = type_check_result_to_string(result);
        lvStrBuf sb_2 = {0};
        lv_strbuf_printf(&sb_2, "端口兼容性检查完成: %s", result_str);
        if (result == TYPE_CHECK_MISMATCH || result == TYPE_CHECK_INCOMPATIBLE) {
            stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_WARNING, sb_2.data, 0);
        } else {
            stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_INFO, sb_2.data, 0);
        }
        lv_strbuf_destroy(&sb_2);
    }

    return result;
}

/* ============== 非良基模式 ============== */

/**
 * DFS辅助函数：递归检测类型循环
 * @param ts 类型系统
 * @param current 当前访问的类型
 * @param visited 已访问标记数组
 * @param on_stack 当前DFS路径上的类型（用于检测回边）
 * @return 是否检测到循环
 *
 * @note 评估（不迁移）：本函数是递归三色 DFS（visited / on_stack 分离），但与
 *       lv_cycle_detect（lv_graph_traversal.h）不可直接替代：
 *       1) 节点是 TypeRegion* 指针而非整数 id，visited/on_stack 按 current->id
 *          索引，且 id 有越界防御；
 *       2) 出边集合不是统一的"后继枚举"，而是经 VTable（s_cycle_detect_handlers）
 *          按类型种类分发的递归 handler，由各 handler 决定子类型引用；
 *       3) visited 语义含"已验证子图剪枝"（visited 且不在 on_stack 即返回 false）。
 *       若未来在 lv_cycle_detect 上封装"节点 id + 出边回调"的薄层且 TypeRegion
 *       循环检测改为显式栈 + 邻接回调，方可迁移；当前保持递归实现。
 */
static bool type_detect_cycle_dfs(TypeSystem *ts, TypeRegion *current, bool *visited, bool *on_stack) {
    if (!current)
        return false;

    /* 边界检查：防止 id 越界访问 visited/on_stack 数组 */
    if (current->id < 0 || current->id >= ts->type_region_count) {
        return false;
    }

    /* 如果当前类型已在DFS路径上，则检测到循环 */
    if (on_stack[current->id]) {
        return true;
    }

    /* 如果已经访问过且不在当前路径上，则无循环（已验证过的子图） */
    if (visited[current->id]) {
        return false;
    }

    /* 标记当前类型为已访问，并加入当前路径 */
    visited[current->id] = true;
    on_stack[current->id] = true;

    bool has_cycle = false;

    /* 通过 VTable 根据类型种类递归检查子类型引用 */
    if (current->kind >= 0 && current->kind < TYPE_KIND_COUNT) {
        CycleDetectHandler handler = s_cycle_detect_handlers[current->kind];
        if (handler) {
            has_cycle = handler(ts, current, visited, on_stack);
        }
    }

    /* 从当前路径移除 */
    on_stack[current->id] = false;

    return has_cycle;
}

bool type_detect_cycle(TypeSystem *ts, TypeRegion *type) {
    if (!ts || !type)
        return false;

    /* 分配访问标记数组 */
    bool *visited = lv_calloc(ts->type_region_count + 1, sizeof(bool));
    if (!visited)
        return false;

    /* 分配当前路径标记数组 */
    bool *on_stack = lv_calloc(ts->type_region_count + 1, sizeof(bool));
    if (!on_stack) {
        lv_free((void **) &visited);
        return false;
    }

    /* 执行DFS检测循环 */
    bool has_cycle = type_detect_cycle_dfs(ts, type, visited, on_stack);

    lv_free((void **) &visited);
    lv_free((void **) &on_stack);

    /* 流式事件：循环检测结果 */
    if (type_system_stream_ctx != NULL && has_cycle) {
        stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_WARNING, "类型循环检测: 发现循环依赖", 0);
    }

    return has_cycle;
}

/**
 * @brief 检查非良基类型兼容性
 *
 * 在良基模式下检测类型中的循环依赖，非良基模式下允许循环包含。
 *
 * @param ts   类型系统指针
 * @param type 待检查的类型区域
 * @return true 兼容，false 存在循环依赖或参数无效
 */
bool type_check_non_well_founded_compatibility(TypeSystem *ts, TypeRegion *type) {
    if (!ts || !type)
        return false;

    /* 非良基模式下允许循环包含 */
    if (ts->well_founded) {
        /* 检测循环 */
        if (type_detect_cycle(ts, type)) {
            return false;
        }
    }

    return true;
}

/* ============== 类型规范化 ============== */

/** @brief 类型规范化递归深度上限（防止循环引用导致无限递归） */
#define TYPE_NORMALIZE_MAX_DEPTH 4096

/**
 * @brief 内部：规范化类型（带递归深度保护）
 *
 * 对类型进行规范化处理：展开类型别名、实例化类型变量、
 * 规范化复合类型的子类型。
 *
 * @param ts             类型系统指针
 * @param type           待规范化的类型区域
 * @param out_normalized 输出参数，接收规范化后的类型
 * @param depth          当前递归深度
 * @return true 规范化成功，false 参数无效或失败
 */
static bool type_normalize_internal(TypeSystem *ts, TypeRegion *type, TypeRegion **out_normalized, int depth) {
    if (!ts || !type || !out_normalized)
        return false;

    /* 递归深度保护：超过上限时直接返回原始类型 */
    if (depth > TYPE_NORMALIZE_MAX_DEPTH) {
        *out_normalized = type;
        return true;
    }

    /* 流式事件：规范化开始 */
    if (type_system_stream_ctx != NULL) {
        stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_NORMALIZE_START, "类型规范化开始", 0);
    }

    /* 规范化规则：
     * 1. 展开类型别名
     * 2. 实例化类型变量
     * 3. 规范化复合类型的子类型
     */

    /* 展开别名 */
    if (type->alias_name && type->aliased_type) {
        return type_normalize_internal(ts, type->aliased_type, out_normalized, depth + 1);
    }

    /* 实例化变量 */
    if (type->kind == TYPE_KIND_VARIABLE && type->variable_id > 0) {
        for (int i = 0; i < ts->type_var_count; i++) {
            if (ts->type_vars[i] && ts->type_vars[i]->id == type->variable_id) {
                if (ts->type_vars[i]->bound_type) {
                    return type_normalize_internal(ts, ts->type_vars[i]->bound_type, out_normalized, depth + 1);
                }
            }
        }
    }

    /* 通过 VTable 递归规范化复合类型 */
    if (type->kind >= 0 && type->kind < TYPE_KIND_COUNT) {
        NormalizeHandler handler = s_normalize_handlers[type->kind];
        if (handler) {
            return handler(ts, type, out_normalized, depth);
        }
    }
    *out_normalized = type;
    return true;
}

/**
 * @brief 规范化类型（公开 API）
 *
 * 对类型进行规范化处理：展开类型别名、实例化类型变量、
 * 规范化复合类型的子类型。添加递归深度保护防止循环引用导致无限递归。
 *
 * @param ts             类型系统指针
 * @param type           待规范化的类型区域
 * @param out_normalized 输出参数，接收规范化后的类型
 * @return true 规范化成功，false 参数无效或失败
 */
bool type_normalize(TypeSystem *ts, TypeRegion *type, TypeRegion **out_normalized) {
    return type_normalize_internal(ts, type, out_normalized, 0);
}

/* ============== 依赖类型检查 ============== */

/**
 * @brief 依赖类型检查
 *
 * 检查输出类型与输入类型在依赖类型语义下的兼容性。
 * 类型变量视为通配，与任何类型兼容。
 *
 * @param ts           类型系统指针
 * @param output_type  输出类型
 * @param input_type   输入类型
 * @param input_values 输入值的符号坐标数组
 * @return true 兼容，false 不兼容或参数无效
 */
bool type_check_dependent(const TypeSystem *ts, const TypeRegion *output_type, const TypeRegion *input_type,
                          const SymbolicCoord **input_values) {
    if (!ts || !output_type || !input_type)
        return false;

    /*
     * 依赖类型检查实现：
     *
     * 对于依赖类型 Π(x:A).B(x)，检查：
     * 1. output_type 和 input_type 的宇宙层级兼容性
     * 2. 结构兼容性（递归检查子类型）
     * 3. 若提供 input_values，执行参数替换后检查
     */

    /* 类型变量与任何类型兼容 */
    if (output_type->kind == TYPE_KIND_VARIABLE || input_type->kind == TYPE_KIND_VARIABLE) {
        return true;
    }

    /* 底部类型与任何类型兼容 */
    if (output_type->kind == TYPE_KIND_BOTTOM || input_type->kind == TYPE_KIND_BOTTOM) {
        return true;
    }

    /* 如果 input_type 是依赖类型，检查其体类型与 output_type 的兼容性 */
    if (input_type->kind == TYPE_KIND_DEPENDENT) {
        if (input_type->body_type) {
            /* 如果提供了 input_values，在比较前替换体类型中的参数 */
            if (input_values && input_type->param_node_id >= 0) {
                /* 创建体类型的替换版本，将参数变量替换为实际输入值的类型 */
                TypeRegion *substituted = NULL;
                bool sub_ok = type_substitute_variable((TypeSystem *) ts, (TypeRegion *) input_type->body_type,
                                                       input_type->param_node_id, NULL, &substituted);
                if (sub_ok && substituted) {
                    bool result = type_check_dependent(ts, output_type, substituted, NULL);
                    type_region_destroy(substituted);
                    return result;
                }
            }
            return type_check_dependent(ts, output_type, input_type->body_type, input_values);
        }
        return false;
    }

    /* 如果 output_type 是依赖类型，检查 input_type 与其体类型的兼容性 */
    if (output_type->kind == TYPE_KIND_DEPENDENT) {
        if (output_type->body_type) {
            return type_check_dependent(ts, output_type->body_type, input_type, input_values);
        }
        return false;
    }

    /* 层级兼容性检查：利用累积性 */
    if (ts->cumulative) {
        /* 累积模式下，output_type 的层级只需 >= input_type 的层级 */
        if (output_type->level < input_type->level) {
            return false;
        }
    } else {
        /* 非累积模式下，层级必须严格相等 */
        if (output_type->level != input_type->level) {
            return false;
        }
    }

    /* 类型种类不同（非变量、非底部），不兼容 */
    if (output_type->kind != input_type->kind) {
        return false;
    }

    /* 通过 VTable 进行相同类型种别的递归结构检查 */
    if (output_type->kind >= 0 && output_type->kind < TYPE_KIND_COUNT) {
        DependentCheckHandler handler = s_dependent_check_handlers[output_type->kind];
        if (handler) {
            return handler(ts, output_type, input_type, input_values);
        }
    }
    return false;
}
