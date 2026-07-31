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

/*
 * type_region_deep_copy 和 type_region_deep_free 在 type_system.c 中定义，
 * 通过 extern 链接到本文件。
 * type_region_deep_free 已在 type_system.h 中声明。
 */
extern TypeRegion *type_region_deep_copy(const TypeRegion *src);

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
            /* 类型种类相同：对复合类型进行递归的深度受限归一化比较 */
            switch (type1->kind) {
                case TYPE_KIND_FUNCTION:
                    /* 函数类型：递归比较输入和输出类型的等价性 */
                    return check_binary_type_equiv(ts, type1, type2, type1->input_type, type1->output_type,
                                                   type2->input_type, type2->output_type, use_rewrite, depth);

                case TYPE_KIND_PRODUCT:
                    /* 乘积类型：递归比较各分量类型的等价性 */
                    return check_binary_type_equiv(ts, type1, type2, type1->left_type, type1->right_type,
                                                   type2->left_type, type2->right_type, use_rewrite, depth);

                case TYPE_KIND_SUM:
                    /* 和类型：递归比较各分量类型的等价性 */
                    return check_binary_type_equiv(ts, type1, type2, type1->first_type, type1->second_type,
                                                   type2->first_type, type2->second_type, use_rewrite, depth);

                default:
                    /* 非复合类型，种类相同，继续执行下面的结构比较逻辑 */
                    break;
            }
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

    switch (type1->kind) {
        case TYPE_KIND_POINT:
        case TYPE_KIND_LINE_SEGMENT:
        case TYPE_KIND_BOTTOM:
            /* 基本类型：种类相同即等价 */
            return TYPE_EQUIV_OK;

        case TYPE_KIND_REGION:
            /* 区域类型：检查包含的节点 */
            if (type1->contained_count != type2->contained_count) {
                return TYPE_EQUIV_NOT_EQUIV;
            }
            /* 区域等价检查：两个区域等价当且仅当它们有相同的边界
             * 边界由形成闭合边界的线段集合定义
             *
             * 检查策略：
             * 1. 如果两个区域都有 contained_node_ids，检查它们是否包含相同的节点集合
             * 2. 如果两个区域都有约束条件，检查约束是否等价
             * 3. 如果都没有额外信息，则认为相同类型的区域等价
             */
            {
                /* 情况1：都有包含节点，检查节点集合是否相同 */
                if (type1->contained_node_ids && type2->contained_node_ids) {
                    /* 排序+双指针 O(n log n) 优化 */
                    int count = type1->contained_count;
                    int *sorted1 = lv_malloc((size_t) count * sizeof(int));
                    int *sorted2 = lv_malloc((size_t) count * sizeof(int));
                    if (!sorted1 || !sorted2) {
                        lv_free((void **) &sorted1);
                        lv_free((void **) &sorted2);
                        return TYPE_EQUIV_ERROR;
                    }
                    memcpy(sorted1, type1->contained_node_ids, count * sizeof(int));
                    memcpy(sorted2, type2->contained_node_ids, count * sizeof(int));
                    qsort(sorted1, count, sizeof(int), lv_cmp_int);
                    qsort(sorted2, count, sizeof(int), lv_cmp_int);

                    /* 双指针线性扫描 */
                    bool equiv = true;
                    int i = 0, j = 0;
                    while (i < count && j < count) {
                        if (sorted1[i] == sorted2[j]) {
                            i++;
                            j++;
                        } else if (sorted1[i] < sorted2[j]) {
                            equiv = false;
                            break;
                        } else {
                            equiv = false;
                            break;
                        }
                    }
                    if (equiv && (i < count || j < count)) {
                        equiv = false;
                    }

                    lv_free((void **) &sorted1);
                    lv_free((void **) &sorted2);
                    return equiv ? TYPE_EQUIV_OK : TYPE_EQUIV_NOT_EQUIV;
                }

                /* 情况2：检查约束条件是否等价 */
                if (type1->constraint_ids && type2->constraint_ids) {
                    if (type1->constraint_count != type2->constraint_count) {
                        return TYPE_EQUIV_NOT_EQUIV;
                    }

                    /* 排序+双指针 O(n log n) 优化 */
                    int count = type1->constraint_count;
                    int *sorted1 = lv_malloc((size_t) count * sizeof(int));
                    int *sorted2 = lv_malloc((size_t) count * sizeof(int));
                    if (!sorted1 || !sorted2) {
                        lv_free((void **) &sorted1);
                        lv_free((void **) &sorted2);
                        return TYPE_EQUIV_ERROR;
                    }
                    memcpy(sorted1, type1->constraint_ids, count * sizeof(int));
                    memcpy(sorted2, type2->constraint_ids, count * sizeof(int));
                    qsort(sorted1, count, sizeof(int), lv_cmp_int);
                    qsort(sorted2, count, sizeof(int), lv_cmp_int);

                    /* 双指针线性扫描 */
                    bool equiv = true;
                    int i = 0, j = 0;
                    while (i < count && j < count) {
                        if (sorted1[i] == sorted2[j]) {
                            i++;
                            j++;
                        } else if (sorted1[i] < sorted2[j]) {
                            equiv = false;
                            break;
                        } else {
                            equiv = false;
                            break;
                        }
                    }
                    if (equiv && (i < count || j < count)) {
                        equiv = false;
                    }

                    lv_free((void **) &sorted1);
                    lv_free((void **) &sorted2);
                    return equiv ? TYPE_EQUIV_OK : TYPE_EQUIV_NOT_EQUIV;
                }

                /* 情况3：都没有额外信息，检查层级是否相同 */
                if (!type1->contained_node_ids && !type2->contained_node_ids && !type1->constraint_ids &&
                    !type2->constraint_ids) {
                    /* 两个空区域，层级相同则等价 */
                    return type1->level == type2->level ? TYPE_EQUIV_OK : TYPE_EQUIV_NOT_EQUIV;
                }

                /* 情况4：一个有信息一个没有，无法确定 */
                return TYPE_EQUIV_UNKNOWN;
            }

        case TYPE_KIND_FUNCTION:
            /* 函数类型：递归检查输入和输出 */
            {
                VISITED_CHECK(type1->input_type, type2->input_type);
                TypeEquivResult input_result =
                    type_check_equivalence_internal(ts, type1->input_type, type2->input_type, use_rewrite, depth + 1);
                if (input_result != TYPE_EQUIV_OK)
                    return input_result;

                VISITED_CHECK(type1->output_type, type2->output_type);
                return type_check_equivalence_internal(ts, type1->output_type, type2->output_type, use_rewrite,
                                                       depth + 1);
            }

        case TYPE_KIND_PRODUCT:
            /* 乘积类型：递归检查左右类型 */
            {
                VISITED_CHECK(type1->left_type, type2->left_type);
                TypeEquivResult left_result =
                    type_check_equivalence_internal(ts, type1->left_type, type2->left_type, use_rewrite, depth + 1);
                if (left_result != TYPE_EQUIV_OK)
                    return left_result;

                VISITED_CHECK(type1->right_type, type2->right_type);
                return type_check_equivalence_internal(ts, type1->right_type, type2->right_type, use_rewrite,
                                                       depth + 1);
            }

        case TYPE_KIND_SUM:
            /* 和类型：递归检查两个分支 */
            {
                VISITED_CHECK(type1->first_type, type2->first_type);
                TypeEquivResult first_result =
                    type_check_equivalence_internal(ts, type1->first_type, type2->first_type, use_rewrite, depth + 1);
                if (first_result != TYPE_EQUIV_OK)
                    return first_result;

                VISITED_CHECK(type1->second_type, type2->second_type);
                return type_check_equivalence_internal(ts, type1->second_type, type2->second_type, use_rewrite,
                                                       depth + 1);
            }

        case TYPE_KIND_VARIABLE:
            /* 类型变量：检查变量ID */
            if (type1->variable_id == type2->variable_id) {
                return TYPE_EQUIV_OK;
            }
            /* 不同变量可能被实例化为相同类型 */
            return TYPE_EQUIV_UNKNOWN;

        case TYPE_KIND_DEPENDENT:
            /* 依赖类型 Pi(x:A).B(x) 的等价检查
             *
             * 两个依赖类型 Pi(x:A1).B1(x) 和 Pi(y:A2).B2(y) 等价当且仅当：
             * 1. A1 和 A2 等价（参数类型相同）
             * 2. B1 和 B2 在参数替换后等价（alpha等价）
             *
             * Alpha等价实现策略：
             * - 使用 De Bruijn 索引方法：将两个依赖类型的参数统一重命名为
             *   同一个规范变量（canonical_var），然后在体类型中替换各自的参数
             *   为该规范变量，最后比较替换后的体类型。
             */
            {
                /* 首先检查参数节点是否有效 */
                if (type1->param_node_id <= 0 || type2->param_node_id <= 0) {
                    /* 参数节点无效，无法进行完整检查 */
                    return TYPE_EQUIV_UNKNOWN;
                }

                /* 检查体类型是否存在 */
                if (!type1->body_type || !type2->body_type) {
                    return TYPE_EQUIV_ERROR;
                }

                /* 策略1：相同参数节点ID，直接比较体类型 */
                if (type1->param_node_id == type2->param_node_id) {
                    VISITED_CHECK(type1->body_type, type2->body_type);
                    return type_check_equivalence_internal(ts, type1->body_type, type2->body_type, use_rewrite,
                                                           depth + 1);
                }

                /*
                 * 策略2：Alpha等价 - 使用 De Bruijn 索引规范化
                 *
                 * 步骤：
                 * 1. 创建一个规范类型变量（使用固定ID -1 表示内部规范变量）
                 * 2. 将 type1 的体类型中的 param_node_id 替换为规范变量
                 * 3. 将 type2 的体类型中的 param_node_id 替换为规范变量
                 * 4. 比较两个替换后的体类型
                 *
                 * 由于 type_substitute_variable 需要一个 TypeRegion* 作为替换目标，
                 * 我们创建一个简单的 TYPE_KIND_VARIABLE 节点作为规范变量。
                 */

                /* 创建规范变量节点（De Bruijn index 0 → variable_id = -1） */
                TypeRegion canonical_var;
                memset(&canonical_var, 0, sizeof(canonical_var));
                canonical_var.kind = TYPE_KIND_VARIABLE;
                canonical_var.variable_id = -1; /* 规范变量标记 */

                /* 替换 type1 体类型中的参数为规范变量 */
                TypeRegion *norm_body1 = NULL;
                bool sub1_ok =
                    type_substitute_variable(ts, type1->body_type, type1->param_node_id, &canonical_var, &norm_body1);

                /* 替换 type2 体类型中的参数为规范变量 */
                TypeRegion *norm_body2 = NULL;
                bool sub2_ok =
                    type_substitute_variable(ts, type2->body_type, type2->param_node_id, &canonical_var, &norm_body2);

                if (sub1_ok && sub2_ok && norm_body1 && norm_body2) {
                    /* 两个替换都成功，比较规范化后的体类型 */
                    VISITED_CHECK(norm_body1, norm_body2);
                    TypeEquivResult body_result =
                        type_check_equivalence_internal(ts, norm_body1, norm_body2, use_rewrite, depth + 1);

                    /* 释放临时规范化类型（仅释放容器，不释放共享的 canonical_var） */
                    type_region_deep_free(norm_body1);
                    type_region_deep_free(norm_body2);

                    return body_result;
                }

                /* 替换失败，清理并回退到结构比较 */
                if (norm_body1)
                    type_region_deep_free(norm_body1);
                if (norm_body2)
                    type_region_deep_free(norm_body2);

                /* 回退：检查体类型的种类是否相同 */
                if (type1->body_type->kind != type2->body_type->kind) {
                    return TYPE_EQUIV_NOT_EQUIV;
                }

                /* 对于简单情况，直接比较体类型（忽略参数差异） */
                VISITED_CHECK(type1->body_type, type2->body_type);
                TypeEquivResult body_result =
                    type_check_equivalence_internal(ts, type1->body_type, type2->body_type, use_rewrite, depth + 1);

                if (body_result == TYPE_EQUIV_OK) {
                    return TYPE_EQUIV_OK;
                }

                return TYPE_EQUIV_UNKNOWN;
            }

        default:
            /* 未知类型种类 */
            return TYPE_EQUIV_NOT_EQUIV;
    }
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

    TypeCheckResult result;

    switch (equiv) {
        case TYPE_EQUIV_OK:
            result = TYPE_CHECK_OK;
            break;

        case TYPE_EQUIV_NOT_EQUIV:
            result = TYPE_CHECK_MISMATCH;
            break;

        case TYPE_EQUIV_UNKNOWN:
            /* 端口兼容性检查采用保守策略。
             * 之前 TYPE_EQUIV_UNKNOWN 时返回 TYPE_CHECK_OK（允许连接），
             * 这可能导致类型不安全的连接通过检查。
             * 改为返回 TYPE_CHECK_INCOMPATIBLE，在无法证明等价时拒绝连接，
             * 确保类型安全性。如果后续需要更宽松的策略，可以在此处添加
             * 交互式证明请求机制。 */
            result = TYPE_CHECK_INCOMPATIBLE;
            break;

        default:
            result = TYPE_CHECK_ERROR;
            break;
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

    /* 根据类型种类递归检查子类型引用 */
    switch (current->kind) {
        case TYPE_KIND_FUNCTION:
            /* 函数类型：检查输入和输出类型 */
            if (current->input_type) {
                has_cycle = type_detect_cycle_dfs(ts, current->input_type, visited, on_stack);
            }
            if (!has_cycle && current->output_type) {
                has_cycle = type_detect_cycle_dfs(ts, current->output_type, visited, on_stack);
            }
            break;

        case TYPE_KIND_PRODUCT:
            /* 乘积类型：检查左右类型 */
            if (current->left_type) {
                has_cycle = type_detect_cycle_dfs(ts, current->left_type, visited, on_stack);
            }
            if (!has_cycle && current->right_type) {
                has_cycle = type_detect_cycle_dfs(ts, current->right_type, visited, on_stack);
            }
            break;

        case TYPE_KIND_SUM:
            /* 和类型：检查两个分支 */
            if (current->first_type) {
                has_cycle = type_detect_cycle_dfs(ts, current->first_type, visited, on_stack);
            }
            if (!has_cycle && current->second_type) {
                has_cycle = type_detect_cycle_dfs(ts, current->second_type, visited, on_stack);
            }
            break;

        case TYPE_KIND_DEPENDENT:
            /* 依赖类型：检查体类型 */
            if (current->body_type) {
                has_cycle = type_detect_cycle_dfs(ts, current->body_type, visited, on_stack);
            }
            break;

        case TYPE_KIND_REGION:
            /* 区域类型：检查包含的类型（通过aliased_type或约束引用） */
            if (current->aliased_type) {
                has_cycle = type_detect_cycle_dfs(ts, current->aliased_type, visited, on_stack);
            }
            break;

        case TYPE_KIND_VARIABLE:
            /* 类型变量：检查绑定的类型 */
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
            break;

        default:
            /* 基本类型（点、线段、底部）无子类型引用 */
            break;
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

    /* 递归规范化复合类型 */
    switch (type->kind) {
        case TYPE_KIND_FUNCTION: {
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

        case TYPE_KIND_PRODUCT:
            /* 乘积类型：规范化每个分量 */
            {
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

        case TYPE_KIND_DEPENDENT:
            /* 依赖类型：规范化主体和依赖体 */
            {
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

        default:
            *out_normalized = type;
            return true;
    }
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

    /* 相同类型种别的递归结构检查 */
    switch (output_type->kind) {
        case TYPE_KIND_POINT:
        case TYPE_KIND_LINE_SEGMENT:
            /* 基本类型：种类相同即兼容 */
            return true;

        case TYPE_KIND_FUNCTION:
            /* 函数类型：递归检查输入和输出 */
            {
                bool input_ok = true, output_ok = true;
                if (output_type->input_type && input_type->input_type) {
                    input_ok = type_check_dependent(ts, output_type->input_type, input_type->input_type, input_values);
                }
                if (input_ok && output_type->output_type && input_type->output_type) {
                    output_ok =
                        type_check_dependent(ts, output_type->output_type, input_type->output_type, input_values);
                }
                return (input_ok && output_ok);
            }

        case TYPE_KIND_PRODUCT:
            /* 乘积类型：递归检查左右 */
            {
                bool left_ok = true, right_ok = true;
                if (output_type->left_type && input_type->left_type) {
                    left_ok = type_check_dependent(ts, output_type->left_type, input_type->left_type, input_values);
                }
                if (left_ok && output_type->right_type && input_type->right_type) {
                    right_ok = type_check_dependent(ts, output_type->right_type, input_type->right_type, input_values);
                }
                return (left_ok && right_ok);
            }

        case TYPE_KIND_SUM:
            /* 和类型：递归检查两个分支 */
            {
                bool first_ok = true, second_ok = true;
                if (output_type->first_type && input_type->first_type) {
                    first_ok = type_check_dependent(ts, output_type->first_type, input_type->first_type, input_values);
                }
                if (first_ok && output_type->second_type && input_type->second_type) {
                    second_ok =
                        type_check_dependent(ts, output_type->second_type, input_type->second_type, input_values);
                }
                return (first_ok && second_ok);
            }

        case TYPE_KIND_REGION:
            /* 区域类型：层级已检查，视为兼容 */
            return true;

        default:
            return false;
    }
}
