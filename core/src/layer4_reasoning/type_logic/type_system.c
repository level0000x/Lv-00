/**
 * @file type_system.c
 * @brief 类型系统实现 —— 宇宙层级类型论与等价检查
 *
 * @details 实现宇宙层级类型系统，支持 9 种类型和等价检查。
 *          提供类型推断、非良基模式检测和规则表驱动推断。
 *
 *          九种类型（TypeKind）：
 *          - TYPE_KIND_POINT        : 点类型（宇宙基础层）
 *          - TYPE_KIND_LINE_SEGMENT : 线段类型（宇宙基础层）
 *          - TYPE_KIND_REGION       : 区域类型（宇宙第一层，线的幂集）
 *          - TYPE_KIND_FUNCTION     : 函数类型 A->B（层级 = max(A,B)+1）
 *          - TYPE_KIND_PRODUCT      : 积类型 A*B（层级 = max(A,B)）
 *          - TYPE_KIND_SUM          : 和类型 A+B（层级 = max(A,B)）
 *          - TYPE_KIND_VARIABLE     : 类型变量（多态，可实例化为任意类型）
 *          - TYPE_KIND_DEPENDENT    : 依赖类型 Pi(x:A).B(x)
 *          - TYPE_KIND_BOTTOM       : 底类型（空类型，所有类型的子类型）
 *
 *          宇宙层级系统：
 *          - UNIVERSE_BASE (0): 点、线段等基本几何类型
 *          - UNIVERSE_TYPE_1 (1): 区域类型（线的幂集）
 *          - UNIVERSE_TYPE_2 (2): 区域类型的函数等
 *          - 整数层级支持无限提升（V_{omega+n}）
 *
 *          类型推断规则表（rule_table）：
 *          将几何节点类型（GEOM_POINT 等）映射到类型种类（TYPE_KIND_POINT 等）。
 *          支持用户注册自定义推理规则以扩展推断能力。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 *
 * @dependencies
 *   - type_system.h        : 类型系统公共接口定义
 *   - lv_internal.h      : 内部数据结构与常量
 *   - lv_utils.h         : 统一内存分配器
 *   - rewrite.h            : 图重写引擎（类型等价检查的重写路径）
 *   - stream.h             : 流式事件输出
 */

#include "type_system.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv.h"
#include "lv/stream.h"

#include "lv_internal.h"
#include "lv_utils.h"
#include "rewrite.h"

lv_DECLARE_STREAM_CTX(type_system);

void type_system_set_stream_context(StreamContext *ctx) {
    type_system_stream_ctx = ctx;
}

/* ============== 内部辅助宏和前向声明 ============== */

/* 推断规则表初始容量 */
#define INFERENCE_RULE_INITIAL_CAPACITY 8

/**
 * 类型推断与等价检查的递归深度限制已迁移至 lvConfig 运行时系统。
 * 通过 lv_config_get_int("type_infer_max_depth", 100) 和
 * lv_config_get_int("type_equiv_max_depth", 16) 读取。
 */

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

/* 前向声明：用于辅助函数 */
static TypeEquivResult type_check_equivalence_internal(TypeSystem *ts, TypeRegion *type1, TypeRegion *type2,
                                                       bool use_rewrite, int depth);

/**
 * @brief 类型推断的内部递归实现
 *
 * 根据约束图中节点的几何类型（GEOM_POINT / GEOM_LINE_SEGMENT / GEOM_REGION /
 * GEOM_PORT / GEOM_FUNCTION_BLOCK）递归推断其类型区域（TypeRegion）。
 * 对于基本几何类型直接创建对应类型；对于端口类型通过 type_infer_port 从连接关系推断；
 * 对于函数块类型则递归推断所有输入/输出端口类型，并组合为乘积类型。
 *
 * @param ts        类型系统上下文，用于创建和管理类型对象，不可为 NULL
 * @param graph     约束图，提供节点和连接信息，不可为 NULL
 * @param node_id   待推断节点的 ID（在 graph 中的索引）
 * @param out_type  输出参数，成功时指向新创建的 TypeRegion 对象；
 *                  调用者需通过类型系统的释放接口管理其生命周期，不可为 NULL
 * @param depth     当前递归深度，初始调用应传 0；每次递归调用时递增
 *
 * @return true  类型推断成功，*out_type 已设置
 * @return false 参数无效、节点不存在、递归深度超过 TYPE_INFER_MAX_DEPTH 或推断失败
 *
 * @note 递归深度限制（TYPE_INFER_MAX_DEPTH）用于防止在循环依赖的约束图中无限递归，
 *       例如函数块 A 的输出连接到函数块 B 的输入，而 B 的输出又连接到 A 的输入。
 *       多个输入/输出端口会组合为乘积类型（type_create_product）。
 */
static bool type_infer_node_internal(TypeSystem *ts, ConstraintGraph *graph, int node_id, TypeRegion **out_type,
                                     int depth);

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
    VISITED_CHECK(sub1, other1);
    TypeEquivResult first = type_check_equivalence_internal(ts, sub1, other1, use_rw, d + 1);
    if (first == TYPE_EQUIV_NOT_EQUIV)
        return TYPE_EQUIV_NOT_EQUIV;
    if (first != TYPE_EQUIV_OK)
        return first;
    VISITED_CHECK(sub2, other2);
    return type_check_equivalence_internal(ts, sub2, other2, use_rw, d + 1);
}

/* ============== 类型系统管理API ============== */

/**
 * @brief 创建类型系统实例
 *
 * 分配并初始化一个 TypeSystem 结构体。默认启用良基模式和累积性，
 * 最大宇宙层级设为 UNIVERSE_TYPE_1 + 1 并注册五种基本几何类型的推断规则。
 *
 * @return 新分配的类型系统指针，失败返回 NULL
 */
TypeSystem *type_system_create(void) {
    TypeSystem *ts = lv_calloc(1, sizeof(TypeSystem));
    if (!ts)
        return NULL;

    ts->well_founded = true;                      /* 默认启用良基模式 */
    ts->cumulative = true;                        /* 默认启用累积性 */
    ts->max_universe_level = UNIVERSE_TYPE_1 + 1; /* 默认最大层级为2，但整数层级无上限 */

    /* 初始化重写路径 */
    ts->rewrite_path = type_rewrite_path_create();
    if (!ts->rewrite_path) {
        lv_free((void **) &ts);
        return NULL;
    }

    /* 注册默认类型推断规则 */
    type_system_register_inference_rule(ts, GEOM_POINT, TYPE_KIND_POINT, 0, "Point node -> Point type");
    type_system_register_inference_rule(ts, GEOM_LINE_SEGMENT, TYPE_KIND_LINE_SEGMENT, 0,
                                        "LineSegment node -> LineSegment type");
    type_system_register_inference_rule(ts, GEOM_REGION, TYPE_KIND_REGION, 0, "Region node -> Region type");
    type_system_register_inference_rule(ts, GEOM_FUNCTION_BLOCK, TYPE_KIND_FUNCTION, 0,
                                        "FunctionBlock node -> Function type");
    type_system_register_inference_rule(ts, GEOM_PORT, TYPE_KIND_VARIABLE, 0,
                                        "Port node -> Variable type (unconnected)");

    return ts;
}

/**
 * @brief 销毁类型系统并释放所有资源
 *
 * 安全释放类型系统中的所有类型区域、类型变量、节点映射、
 * 重写路径和推断规则表。先清理类型变量的绑定关系再释放类型区域，
 * 避免悬挂指针导致 double-free。
 *
 * @param ts 类型系统指针（可为 NULL）
 */
void type_system_destroy(TypeSystem *ts) {
    if (!ts)
        return;

    /* 在释放 type_regions 之前，先扫描所有 type_vars 的 bound_type，
     * 将指向已注册 type_regions 的 bound_type 置 NULL，避免后续 double-free。
     * 原因：type_instantiate_variable 将 concrete_type 指针直接赋给 bound_type，
     * 而 concrete_type 本身已注册在 type_regions 数组中，两者指向同一块内存。 */
    for (int i = 0; i < ts->type_var_count; i++) {
        if (ts->type_vars[i] && ts->type_vars[i]->bound_type) {
            for (int j = 0; j < ts->type_region_count; j++) {
                if (ts->type_vars[i]->bound_type == ts->type_regions[j]) {
                    ts->type_vars[i]->bound_type = NULL;
                    break;
                }
            }
        }
    }

    for (int i = 0; i < ts->type_region_count; i++) {
        type_region_destroy(ts->type_regions[i]);
    }
    lv_free((void **) &ts->type_regions);

    for (int i = 0; i < ts->type_var_count; i++) {
        if (ts->type_vars[i]) {
            /* 释放 bound_type（仅限非 type_regions 注册的外部类型） */
            if (ts->type_vars[i]->bound_type) {
                type_region_destroy(ts->type_vars[i]->bound_type);
                ts->type_vars[i]->bound_type = NULL;
            }
            lv_free((void **) &ts->type_vars[i]->name);
            lv_free((void **) &ts->type_vars[i]);
        }
    }
    lv_free((void **) &ts->type_vars);

    /* 释放节点-类型映射 */
    lv_free((void **) &ts->node_type_mappings);

    /* 释放重写路径 */
    type_rewrite_path_destroy(ts->rewrite_path);
    ts->rewrite_path = NULL;

    /* 释放推断规则 */
    lv_free((void **) &ts->inference_rules);

    lv_free((void **) &ts);
}

/**
 * @brief 设置类型系统的良基性标志
 *
 * 良基模式下，宇宙层级检查会强制执行层级约束（区域只能包含
 * 严格低于其层级的几何体）。非良基模式下跳过层级检查。
 *
 * @param ts           类型系统指针（可为 NULL）
 * @param well_founded 是否启用良基模式
 */
void type_system_set_well_founded(TypeSystem *ts, bool well_founded) {
    if (ts)
        ts->well_founded = well_founded;
}

/**
 * @brief 设置类型系统的累积性标志
 *
 * 累积性模式下，第 n 层对象可以出现在第 n+1 层区域中。
 * 非累积模式下，层级约束更严格。
 *
 * @param ts         类型系统指针（可为 NULL）
 * @param cumulative 是否启用累积性模式
 */
void type_system_set_cumulative(TypeSystem *ts, bool cumulative) {
    if (ts)
        ts->cumulative = cumulative;
}

/* ============== 类型区域管理 ============== */

/**
 * @brief 创建类型区域（内部辅助函数）
 *
 * 分配 TypeRegion 结构体并将其注册到类型系统的类型区域数组中。
 * 检查容量溢出，通过 realloc 扩容。
 *
 * @param ts   类型系统指针
 * @param kind 类型种类（点/线段/区域/函数/积/和/变量/依赖/底）
 * @return 新分配的类型区域指针，失败返回 NULL
 */
static TypeRegion *type_region_create(TypeSystem *ts, TypeKind kind) {
    TypeRegion *tr = lv_calloc(1, sizeof(TypeRegion));
    if (!tr)
        return NULL;

    tr->kind = kind;

    /* 添加到类型系统 */
    if (ts->type_region_count >= INT_MAX) {
        lv_free((void **) &tr);
        return NULL;
    }
    /* 确保容量 */
    if (!lv_ensure_capacity((void **)&ts->type_regions, ts->type_region_count,
                            &ts->type_region_capacity, sizeof(TypeRegion *), 1)) {
        lv_free((void **)&tr);
        return NULL;
    }
    ts->type_regions[ts->type_region_count++] = tr;

    tr->id = ts->type_region_count;
    return tr;
}

/**
 * @brief 创建点类型
 *
 * 点类型属于基础宇宙层级 UNIVERSE_BASE，
 * 是所有几何对象中最底层的类型。
 *
 * @param ts 类型系统指针
 * @return 新分配的点类型区域指针，失败返回 NULL
 */
TypeRegion *type_create_point(TypeSystem *ts) {
    TypeRegion *tr = type_region_create(ts, TYPE_KIND_POINT);
    if (tr) {
        tr->level = UNIVERSE_BASE;
    }
    return tr;
}

/**
 * @brief 创建线段类型
 *
 * 线段类型属于基础宇宙层级 UNIVERSE_BASE。
 *
 * @param ts 类型系统指针
 * @return 新分配的线段类型区域指针，失败返回 NULL
 */
TypeRegion *type_create_line_segment(TypeSystem *ts) {
    TypeRegion *tr = type_region_create(ts, TYPE_KIND_LINE_SEGMENT);
    if (tr) {
        tr->level = UNIVERSE_BASE;
    }
    return tr;
}

/**
 * @brief 创建区域类型
 *
 * 区域类型属于 UNIVERSE_TYPE_1 层级（线的幂集）。
 * 可包含边界节点列表。
 *
 * @param ts            类型系统指针
 * @param contained_ids 区域内包含的节点 ID 数组（可为 NULL）
 * @param count         包含节点数量
 * @return 新分配的区域类型区域指针，失败返回 NULL
 */
TypeRegion *type_create_region(TypeSystem *ts, const int *contained_ids, int count) {
    TypeRegion *tr = type_region_create(ts, TYPE_KIND_REGION);
    if (!tr)
        return NULL;

    tr->level = UNIVERSE_TYPE_1;

    if (contained_ids && count > 0) {
        tr->contained_node_ids = lv_malloc((size_t) count * sizeof(int));
        if (tr->contained_node_ids) {
            memcpy(tr->contained_node_ids, contained_ids, count * sizeof(int));
            tr->contained_count = count;
        }
    }

    return tr;
}

/**
 * @brief 创建函数类型
 *
 * 函数类型 A->B 的宇宙层级 = max(input_level, output_level) + 1。
 *
 * @param ts     类型系统指针
 * @param input  输入类型（可为 NULL 表示未指定）
 * @param output 输出类型（可为 NULL 表示未指定）
 * @return 新分配的函数类型区域指针，失败返回 NULL
 */
TypeRegion *type_create_function(TypeSystem *ts, TypeRegion *input, TypeRegion *output) {
    TypeRegion *tr = type_region_create(ts, TYPE_KIND_FUNCTION);
    if (!tr)
        return NULL;

    tr->input_type = input;
    tr->output_type = output;

    /* 函数类型的层级 = max(输入层级, 输出层级) + 1 */
    UniverseLevel input_level = input ? input->level : UNIVERSE_BASE;
    UniverseLevel output_level = output ? output->level : UNIVERSE_BASE;
    tr->level = (input_level > output_level ? input_level : output_level) + 1;

    return tr;
}

/**
 * @brief 创建积类型
 *
 * 积类型 A*B 的宇宙层级 = max(left_level, right_level)。
 *
 * @param ts    类型系统指针
 * @param left  左分量类型（可为 NULL）
 * @param right 右分量类型（可为 NULL）
 * @return 新分配的积类型区域指针，失败返回 NULL
 */
TypeRegion *type_create_product(TypeSystem *ts, TypeRegion *left, TypeRegion *right) {
    TypeRegion *tr = type_region_create(ts, TYPE_KIND_PRODUCT);
    if (!tr)
        return NULL;

    tr->left_type = left;
    tr->right_type = right;

    /* 乘积类型的层级 = max(左层级, 右层级) */
    UniverseLevel left_level = left ? left->level : UNIVERSE_BASE;
    UniverseLevel right_level = right ? right->level : UNIVERSE_BASE;
    tr->level = left_level > right_level ? left_level : right_level;

    return tr;
}

/**
 * @brief 创建和类型
 *
 * 和类型 A+B 的宇宙层级 = max(first_level, second_level)。
 *
 * @param ts     类型系统指针
 * @param first  第一分量类型（可为 NULL）
 * @param second 第二分量类型（可为 NULL）
 * @return 新分配的和类型区域指针，失败返回 NULL
 */
TypeRegion *type_create_sum(TypeSystem *ts, TypeRegion *first, TypeRegion *second) {
    TypeRegion *tr = type_region_create(ts, TYPE_KIND_SUM);
    if (!tr)
        return NULL;

    tr->first_type = first;
    tr->second_type = second;

    /* 和类型的层级 = max(第一层级, 第二层级) */
    UniverseLevel first_level = first ? first->level : UNIVERSE_BASE;
    UniverseLevel second_level = second ? second->level : UNIVERSE_BASE;
    tr->level = first_level > second_level ? first_level : second_level;

    return tr;
}

/**
 * @brief 创建类型变量
 *
 * 分配一个可变类型区域并创建对应的 TypeVariable 条目。
 * 类型变量始终是多态的，可被实例化为任意具体类型。
 * 宇宙层级设为 UNIVERSE_BASE 作为占位符。
 *
 * @param ts   类型系统指针
 * @param name 变量名称（可为 NULL）
 * @return 新分配的类型变量区域指针，失败返回 NULL
 */
TypeRegion *type_create_variable(TypeSystem *ts, const char *name) {
    TypeRegion *tr = type_region_create(ts, TYPE_KIND_VARIABLE);
    if (!tr)
        return NULL;

    if (name) {
        tr->variable_name = lv_strdup(name);
    }

    /* 创建类型变量 */
    TypeVariable *tv = lv_calloc(1, sizeof(TypeVariable));
    if (tv) {
        int new_count = ts->type_var_count + 1;
        TypeVariable **new_arr = (TypeVariable **) lv_realloc(ts->type_vars, new_count * sizeof(TypeVariable *));
        if (!new_arr) {
            /* 修复：realloc 失败时，需清理已分配的 TypeVariable 和已创建的 TypeRegion，
             * 防止内存泄漏 */
            lv_free((void **) &tv);
            lv_free((void **) &tr->variable_name);
            /* 从类型系统的 type_regions 数组中移除 tr，避免悬空指针 */
            if (ts->type_region_count > 0) {
                ts->type_regions[ts->type_region_count - 1] = NULL;
                ts->type_region_count--;
            }
            lv_free((void **) &tr);
            return NULL;
        }
        ts->type_vars = new_arr;
        ts->type_var_count = new_count;
        ts->type_vars[new_count - 1] = tv;
        tv->id = new_count;
        tv->name = name ? lv_strdup(name) : NULL;
        tv->is_polymorphic = true;
        tr->variable_id = tv->id;
    }

    tr->level = UNIVERSE_BASE; /* 类型变量可以是任意层级 */

    return tr;
}

/**
 * @brief 创建依赖类型
 *
 * 依赖类型 \Pi(x:A).B(x)，其中 B 的类型依赖于参数 x（由 param_id 标识）。
 * 宇宙层级与 body 类型相同。
 *
 * @param ts       类型系统指针
 * @param param_id 依赖参数的节点 ID
 * @param body     依赖类型体（可为 NULL）
 * @return 新分配的依赖类型区域指针，失败返回 NULL
 */
TypeRegion *type_create_dependent(TypeSystem *ts, int param_id, TypeRegion *body) {
    TypeRegion *tr = type_region_create(ts, TYPE_KIND_DEPENDENT);
    if (!tr)
        return NULL;

    tr->param_node_id = param_id;
    tr->body_type = body;

    /* 依赖类型的层级 = 体类型层级 + 1 */
    tr->level = body ? body->level + 1 : UNIVERSE_TYPE_1;

    return tr;
}

/**
 * @brief 创建底类型（空类型）
 *
 * 底类型是所有类型的子类型，代表空集。宇宙层级设为 UNIVERSE_BASE。
 * 在非良基类型系统中通过反基础公理引入。
 *
 * @param ts 类型系统指针
 * @return 新分配的底类型区域指针，失败返回 NULL
 */
TypeRegion *type_create_bottom(TypeSystem *ts) {
    TypeRegion *tr = type_region_create(ts, TYPE_KIND_BOTTOM);
    if (tr) {
        tr->level = UNIVERSE_BASE;
    }
    return tr;
}

TypeRegion *type_create_predicate_subtype(TypeSystem *ts, TypeRegion *base_type, const char *predicate_name,
                                          const char *predicate_expr) {
    if (!ts || !base_type || !predicate_name)
        return NULL;

    TypeRegion *tr = type_region_create(ts, TYPE_KIND_PREDICATE_SUBTYPE);
    if (!tr)
        return NULL;

    tr->base_type = base_type;
    tr->predicate_name = lv_strdup(predicate_name);
    tr->predicate_expr = predicate_expr ? lv_strdup(predicate_expr) : NULL;
    tr->predicate_constraint_id = -1; /* 稍后通过约束系统关联 */
    tr->level = base_type->level;     /* 子类型与基类型同层级 */

    if (!tr->predicate_name) {
        /* 从 type_regions 数组中移除，避免悬空指针导致 type_system_destroy 时 double-free */
        ts->type_region_count--;
        ts->type_regions[ts->type_region_count] = NULL;
        lv_free((void **) &tr);
        return NULL;
    }

    return tr;
}

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

TypeRegion *type_predicate_subtype_get_base(TypeRegion *subtype) {
    if (!subtype || subtype->kind != TYPE_KIND_PREDICATE_SUBTYPE)
        return NULL;
    return subtype->base_type;
}

/**
 * @brief 销毁类型区域并释放其资源
 *
 * 释放类型区域内部的动态数组（包含节点 ID、变量名、别名、约束 ID）。
 * 注意：不递归销毁关联的类型（input/output/left/right/body 等），
 * 因为它们可能被多个类型区域共享。
 *
 * @param tr 类型区域指针（可为 NULL）
 */
void type_region_destroy(TypeRegion *tr) {
    if (!tr)
        return;

    lv_free((void **) &tr->contained_node_ids);
    lv_free((void **) &tr->variable_name);
    lv_free((void **) &tr->alias_name);
    lv_free((void **) &tr->constraint_ids);
    lv_free((void **) &tr->predicate_name);
    lv_free((void **) &tr->predicate_expr);

    /* 注意：不递归销毁关联的类型，因为它们可能被共享 */
    lv_free((void **) &tr);
}

/**
 * @brief 为类型区域添加别名
 *
 * 替换现有的别名。别名用于在错误消息和调试输出中提供更友好的类型名称。
 *
 * @param tr    类型区域指针
 * @param alias 别名字符串
 * @return true 设置成功，false 参数无效或内存分配失败
 */
bool type_add_alias(TypeRegion *tr, const char *alias) {
    if (!tr || !alias)
        return false;

    lv_free((void **) &tr->alias_name);
    tr->alias_name = lv_strdup(alias);
    return tr->alias_name != NULL;
}

/* ============== 宇宙层级检查 ============== */

/**
 * @brief 获取类型区域的宇宙层级
 *
 * @param tr 类型区域指针
 * @return 类型区域的宇宙层级，NULL 时返回 UNIVERSE_BASE
 */
UniverseLevel type_get_level(TypeRegion *tr) {
    return tr ? tr->level : UNIVERSE_BASE;
}

/**
 * @brief 检查宇宙层级有效性
 *
 * 验证 contained 类型是否可以合法地出现在 container 类型中。
 * 非良基模式下跳过层级检查直接返回 true。
 * 累积性模式下允许第 n 层对象出现在第 n+1 层区域中。
 *
 * @param ts        类型系统指针
 * @param container 容器类型（通常是区域类型）
 * @param contained 被包含类型
 * @return true 层级合法，false 层级违规或参数无效
 */
bool type_check_level_validity(TypeSystem *ts, TypeRegion *container, TypeRegion *contained) {
    if (!ts || !container || !contained)
        return false;

    /* 非良基模式下跳过层级检查 */
    if (!ts->well_founded)
        return true;

    /* 区域只能包含严格低于其层级的几何体 */
    if (container->kind == TYPE_KIND_REGION) {
        if (contained->level >= container->level) {
            /* 流式事件：层级错误 */
            if (type_system_stream_ctx != NULL) {
                stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_WARNING, "宇宙层级检查失败: 包含层级不合法", 0);
            }
            return false;
        }
    }

    /* 累积性：第n层对象可以出现在第n+1层区域 */
    if (ts->cumulative) {
        if (contained->level <= container->level) {
            return true;
        }
    }

    return contained->level < container->level;
}

/**
 * @brief 检查累积性层级兼容性
 *
 * 在累积性模式下，第 n 层类型可以出现在第 n+1 层上下文中。
 * 对函数类型和乘积类型进行递归检查。
 *
 * @param ts     类型系统指针
 * @param lower  较低层级的类型
 * @param higher 较高层级的类型
 * @return true 兼容，false 不兼容或参数无效
 */
bool type_check_cumulative(TypeSystem *ts, TypeRegion *lower, TypeRegion *higher) {
    if (!ts || !lower || !higher)
        return false;

    if (!ts->cumulative) {
        /* 非累积模式：层级必须严格相等 */
        return type_get_level(lower) == type_get_level(higher);
    }

    /* 累积模式：层级 n 的类型自动属于层级 n+1 */
    int lower_level = type_get_level(lower);
    int higher_level = type_get_level(higher);

    if (lower_level <= higher_level)
        return true;

    /* 函数类型的递归检查：
     * (A -> B) : (i+1) 要求 A : i, B : (i+1)
     * 在累积模式下，A 可以在更高的层级 */
    if (lower->kind == TYPE_KIND_FUNCTION && higher->kind == TYPE_KIND_FUNCTION) {
        bool input_ok = true, output_ok = true;
        if (lower->input_type && higher->input_type) {
            input_ok = type_check_cumulative(ts, lower->input_type, higher->input_type);
        }
        if (input_ok && lower->output_type && higher->output_type) {
            output_ok = type_check_cumulative(ts, lower->output_type, higher->output_type);
        }
        return input_ok && output_ok;
    }

    /* 乘积类型的递归检查 */
    if (lower->kind == TYPE_KIND_PRODUCT && higher->kind == TYPE_KIND_PRODUCT) {
        bool left_ok = true, right_ok = true;
        if (lower->left_type && higher->left_type) {
            left_ok = type_check_cumulative(ts, lower->left_type, higher->left_type);
        }
        if (left_ok && lower->right_type && higher->right_type) {
            right_ok = type_check_cumulative(ts, lower->right_type, higher->right_type);
        }
        return left_ok && right_ok;
    }

    return false;
}

/* ============== 宇宙层级类型系统（6.1节） ============== */

/**
 * 全局层级检查开关（默认开启）
 * type_set_universe_checking() / type_is_universe_checking_enabled() 控制此标志
 */
static bool s_universe_checking_enabled = true;

/**
 * @brief 获取几何节点的宇宙层级
 *
 * GEOM_POINT / GEOM_LINE_SEGMENT / GEOM_PORT → UNIVERSE_LEVEL_0
 * GEOM_REGION → UNIVERSE_LEVEL_1（由基本几何体构成的集合）
 * GEOM_FUNCTION_BLOCK → UNIVERSE_LEVEL_1（函数空间）
 *
 * @param node 几何节点
 * @return 宇宙层级；node 为 NULL 或未知类型时返回 UNIVERSE_LEVEL_0
 */
UniverseLevel type_get_universe_level(const GeomNode *node) {
    if (!node)
        return UNIVERSE_LEVEL_0;

    switch (node->type) {
        case GEOM_POINT:
        case GEOM_LINE_SEGMENT:
        case GEOM_PORT:
            return UNIVERSE_LEVEL_0;
        case GEOM_REGION:
        case GEOM_CIRCLE:
        case GEOM_FUNCTION_BLOCK:
            return UNIVERSE_LEVEL_1;
        default:
            return UNIVERSE_LEVEL_0;
    }
}

/**
 * @brief 检查区域包含关系是否满足宇宙层级约束
 *
 * 规则：外层的宇宙层级必须严格高于内层的宇宙层级。
 * 如果层级检查已关闭（type_set_universe_checking(false)），不做检查返回 true。
 *
 * @param outer 外部对象节点（通常是区域）
 * @param inner 内部对象节点
 * @return true 层级检查通过或已关闭；false 违反层级约束
 */
bool type_check_universe_constraint(const GeomNode *outer, const GeomNode *inner) {
    /* 层级检查已关闭时跳过检查 */
    if (!s_universe_checking_enabled)
        return true;

    if (!outer || !inner)
        return false;

    UniverseLevel outer_level = type_get_universe_level(outer);
    UniverseLevel inner_level = type_get_universe_level(inner);

    /* 外层必须严格高于内层（外层 > 内层） */
    return outer_level > inner_level;
}

/**
 * @brief 设置/关闭层级检查
 *
 * @param enabled true 开启层级检查（默认），false 关闭
 */
void type_set_universe_checking(bool enabled) {
    s_universe_checking_enabled = enabled;
}

/**
 * @brief 查询当前层级检查状态
 *
 * @return true 层级检查开启，false 已关闭
 */
bool type_is_universe_checking_enabled(void) {
    return s_universe_checking_enabled;
}

/* ============== 类型等价检查 ============== */

/* qsort 比较函数：按 int 升序排列 */
static int compare_ints(const void *a, const void *b) {
    int ia = *(const int *) a;
    int ib = *(const int *) b;
    return (ia > ib) - (ia < ib);
}

/* 内部递归辅助函数，带深度限制 */
static TypeEquivResult type_check_equivalence_internal(TypeSystem *ts, TypeRegion *type1, TypeRegion *type2,
                                                       bool use_rewrite, int depth) {
    if (!ts || !type1 || !type2)
        return TYPE_EQUIV_ERROR;

    /* 递归深度限制检查 */
    int equiv_max = lv_config_get_int("type_equiv_max_depth", 16);
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
                    qsort(sorted1, count, sizeof(int), compare_ints);
                    qsort(sorted2, count, sizeof(int), compare_ints);

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
                    qsort(sorted1, count, sizeof(int), compare_ints);
                    qsort(sorted2, count, sizeof(int), compare_ints);

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
            /* FUNCTION, PRODUCT, SUM, VARIABLE, DEPENDENT types need structural comparison */
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
        char buf[128];
        snprintf(buf, sizeof(buf), "类型等价检查完成: %s", result_str);
        if (result == TYPE_EQUIV_NOT_EQUIV) {
            stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_WARNING, buf, 0);
        } else {
            stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_INFO, buf, 0);
        }
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
        char buf[128];
        snprintf(buf, sizeof(buf), "端口兼容性检查完成: %s", result_str);
        if (result == TYPE_CHECK_MISMATCH || result == TYPE_CHECK_INCOMPATIBLE) {
            stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_WARNING, buf, 0);
        } else {
            stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_INFO, buf, 0);
        }
    }

    return result;
}

/* ============== 类型推断 ============== */

/**
 * @brief 类型推断的内部递归实现
 *
 * 根据约束图中节点的几何类型（GEOM_POINT / GEOM_LINE_SEGMENT / GEOM_REGION /
 * GEOM_PORT / GEOM_FUNCTION_BLOCK）递归推断其类型区域（TypeRegion）。
 * 对于基本几何类型直接创建对应类型；对于端口类型通过 type_infer_port 从连接关系推断；
 * 对于函数块类型则递归推断所有输入/输出端口类型，并组合为乘积类型。
 *
 * @param ts        类型系统上下文，用于创建和管理类型对象，不可为 NULL
 * @param graph     约束图，提供节点和连接信息，不可为 NULL
 * @param node_id   待推断节点的 ID（在 graph 中的索引）
 * @param out_type  输出参数，成功时指向新创建的 TypeRegion 对象；
 *                  调用者需通过类型系统的释放接口管理其生命周期，不可为 NULL
 * @param depth     当前递归深度，初始调用应传 0；每次递归调用时递增
 *
 * @return true  类型推断成功，*out_type 已设置
 * @return false 参数无效、节点不存在、递归深度超过 TYPE_INFER_MAX_DEPTH 或推断失败
 *
 * @note 递归深度限制（TYPE_INFER_MAX_DEPTH）用于防止在循环依赖的约束图中无限递归，
 *       例如函数块 A 的输出连接到函数块 B 的输入，而 B 的输出又连接到 A 的输入。
 *       多个输入/输出端口会组合为乘积类型（type_create_product）。
 */
static bool type_infer_node_internal(TypeSystem *ts, ConstraintGraph *graph, int node_id, TypeRegion **out_type,
                                     int depth) {
    if (!ts || !graph || !out_type)
        return false;

    /* 递归深度限制检查。
     * 超过最大深度时返回 false，避免在循环依赖的约束图中无限递归。 */
    int infer_max = lv_config_get_int("type_infer_max_depth", 100);
    if (depth >= infer_max) {
        return false;
    }

    GeomNode *node = graph_get_node(graph, node_id);
    if (!node)
        return false;

    switch (node->type) {
        case GEOM_POINT:
            *out_type = type_create_point(ts);
            return true;

        case GEOM_LINE_SEGMENT:
            *out_type = type_create_line_segment(ts);
            return true;

        case GEOM_REGION:
            *out_type = type_create_region(ts, NULL, 0);
            return true;
        case GEOM_CIRCLE:
            *out_type = type_create_region(ts, NULL, 0);
            return true;

        case GEOM_PORT:
            /* 端口类型需要从连接推断 */
            return type_infer_port(ts, graph, node_id, out_type);

        case GEOM_FUNCTION_BLOCK:
            /* 函数块类型：从输入输出端口推断 */
            {
                TypeRegion *input_type = NULL;
                TypeRegion *output_type = NULL;

                /* 从输入端口推断输入类型 */
                if (node->data.func_block.input_port_ids && node->data.func_block.input_count > 0) {
                    /*
                     * 多个输入端口：构建乘积类型作为输入
                     * 单个输入端口：直接使用该端口类型
                     */
                    for (int i = 0; i < node->data.func_block.input_count; i++) {
                        TypeRegion *port_type = NULL;
                        if (type_infer_port(ts, graph, node->data.func_block.input_port_ids[i], &port_type)) {
                            if (input_type == NULL) {
                                input_type = port_type;
                            } else {
                                /* 将多个输入组合为乘积类型 */
                                input_type = type_create_product(ts, input_type, port_type);
                            }
                        }
                    }
                }

                /* 从输出端口推断输出类型 */
                if (node->data.func_block.output_port_ids && node->data.func_block.output_count > 0) {
                    /*
                     * 多个输出端口：构建乘积类型作为输出
                     * 单个输出端口：直接使用该端口类型
                     */
                    for (int i = 0; i < node->data.func_block.output_count; i++) {
                        TypeRegion *port_type = NULL;
                        if (type_infer_port(ts, graph, node->data.func_block.output_port_ids[i], &port_type)) {
                            if (output_type == NULL) {
                                output_type = port_type;
                            } else {
                                /* 将多个输出组合为乘积类型 */
                                output_type = type_create_product(ts, output_type, port_type);
                            }
                        }
                    }
                }

                /* 构建函数类型 (domain -> codomain) */
                *out_type = type_create_function(ts, input_type, output_type);
                return true;
            }

        default:
            break;
    }

    /* ===== 新增推断规则：沿集合包含链推断 ===== */
    /*
     * 若节点 A 包含在区域 R 中，且 R 的类型已知，
     * 则 A 的类型可从 R 的元素类型推断。
     *
     * 遍历约束图中的 CONTAINMENT 约束，查找包含当前节点的区域。
     */
    {
        int constraint_indices[64];
        int found_count = graph_find_constraints_involving(graph, node_id, constraint_indices, 64);

        for (int i = 0; i < found_count; i++) {
            Constraint *c = graph_get_constraint(graph, constraint_indices[i]);
            if (!c)
                continue;

            if (c->type == CONTAINMENT && c->participant_count >= 2) {
                /* 查找包含此节点的区域 */
                int container_id = -1;
                for (int j = 0; j < c->participant_count; j++) {
                    if (c->participants[j] != node_id) {
                        GeomNode *other = graph_get_node(graph, c->participants[j]);
                        if (other && other->type == GEOM_REGION) {
                            container_id = c->participants[j];
                            break;
                        }
                    }
                }

                if (container_id > 0) {
                    /* 尝试从外部映射获取容器区域的类型 */
                    TypeRegion *container_type = type_get_node_type(ts, container_id);
                    if (container_type) {
                        /*
                         * 容器类型已知，推断节点类型为容器类型的元素类型。
                         * 对于区域类型，元素类型即区域所包含的几何体类型。
                         * 这里创建一个与容器层级兼容的类型变量作为推断结果。
                         */
                        *out_type = type_create_variable(ts, NULL);
                        if (*out_type) {
                            (*out_type)->level =
                                container_type->level > UNIVERSE_BASE ? container_type->level - 1 : UNIVERSE_BASE;
                        }
                        return (*out_type) != NULL;
                    }
                }
            }
        }
    }

    /* ===== 新增推断规则：沿函数块输入输出关系推断 ===== */
    /*
     * 若函数块 F 的输出连接到节点 N，且 F 的输出类型已知，
     * 则 N 的类型可推断为 F 的输出类型的分量。
     *
     * 遍历约束图中的 CONNECTION 约束，查找连接到此节点的端口。
     */
    {
        int constraint_indices[64];
        int found_count = graph_find_constraints_involving(graph, node_id, constraint_indices, 64);

        for (int i = 0; i < found_count; i++) {
            Constraint *c = graph_get_constraint(graph, constraint_indices[i]);
            if (!c)
                continue;

            if (c->type == CONNECTION && c->participant_count >= 2) {
                /* 查找连接到此节点的源端口 */
                int src_port_id = -1;
                for (int j = 0; j < c->participant_count; j++) {
                    if (c->participants[j] != node_id) {
                        src_port_id = c->participants[j];
                        break;
                    }
                }

                if (src_port_id > 0) {
                    GeomNode *src_port = graph_get_node(graph, src_port_id);
                    if (src_port && src_port->type == GEOM_PORT && src_port->data.port) {
                        /* 查找源端口所属的函数块 */
                        int parent_block_id = src_port->data.port->parent_block_id;
                        if (parent_block_id > 0) {
                            /* 尝试推断函数块的类型，从中提取输出类型 */
                            TypeRegion *func_type = NULL;
                            if (type_infer_node_internal(ts, graph, parent_block_id, &func_type, depth + 1) &&
                                func_type && func_type->kind == TYPE_KIND_FUNCTION) {
                                /* 函数块的输出类型即为连接目标节点的推断类型 */
                                *out_type = func_type->output_type;
                                return (*out_type) != NULL;
                            }
                        }
                    }
                }
            }
        }
    }

    /* 所有推断规则均未匹配 */
    return false;
}

/**
 * @brief 公共 API 包装器，调用内部实现并传入初始深度 0
 *
 * @param ts      类型系统指针
 * @param graph   约束图指针
 * @param node_id 节点 ID
 * @param out_type 输出参数，接收推断的类型
 * @return true 推断成功，false 推断失败
 */
bool type_infer_node(TypeSystem *ts, ConstraintGraph *graph, int node_id, TypeRegion **out_type) {
    /* 流式事件：入口 */
    if (type_system_stream_ctx != NULL) {
        stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_INFO, "类型推断开始", 0);
    }

    bool result = type_infer_node_internal(ts, graph, node_id, out_type, 0);

    /* 流式事件：结果 */
    if (type_system_stream_ctx != NULL) {
        if (result && out_type && *out_type) {
            char buf[128];
            snprintf(buf, sizeof(buf), "类型推断完成: %s", type_kind_to_string((*out_type)->kind));
            stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_INFO, buf, 0);
        } else {
            stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_WARNING, "类型推断失败", 0);
        }
    }

    return result;
}

bool type_infer_port(TypeSystem *ts, ConstraintGraph *graph, int port_id, TypeRegion **out_type) {
    /* 流式事件：入口 */
    if (type_system_stream_ctx != NULL) {
        stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_INFO, "端口类型推断开始", 0);
    }

    if (!ts || !graph || !out_type)
        return false;

    GeomNode *port = graph_get_node(graph, port_id);
    if (!port || port->type != GEOM_PORT)
        return false;

    bool result;

    /* 如果端口有连接，从连接的另一端推断类型 */
    if (port->data.port && port->data.port->connected_to) {
        /* 使用内部版本并传入 depth + 1 以跟踪递归深度。
         * 注意：type_infer_port 自身不直接跟踪深度，但通过调用
         * type_infer_node_internal 间接利用深度限制机制。 */
        result = type_infer_node_internal(ts, graph, port->data.port->connected_to->id, out_type, 1);
    } else {
        /* 否则，创建类型变量 */
        *out_type = type_create_variable(ts, NULL);
        result = true;
    }

    /* 流式事件：完成 */
    if (type_system_stream_ctx != NULL && result) {
        stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_INFO, "端口类型推断完成", 0);
    }

    return result;
}

/* ============== 类型变量实例化 ============== */

bool type_instantiate_variable(TypeSystem *ts, int var_id, TypeRegion *concrete_type) {
    if (!ts || !concrete_type)
        return false;

    /* 查找类型变量 */
    TypeVariable *var = NULL;
    for (int i = 0; i < ts->type_var_count; i++) {
        if (ts->type_vars[i] && ts->type_vars[i]->id == var_id) {
            var = ts->type_vars[i];
            break;
        }
    }

    if (!var)
        return false;

    var->bound_type = concrete_type;
    var->is_polymorphic = false;

    /* 流式事件：变量实例化完成 */
    if (type_system_stream_ctx != NULL) {
        char buf[128];
        snprintf(buf, sizeof(buf), "类型变量实例化: var_id=%d -> %s", var_id, type_kind_to_string(concrete_type->kind));
        stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_INFO, buf, 0);
    }

    return true;
}

bool type_substitute_variable(TypeSystem *ts, TypeRegion *type, int var_id, TypeRegion *replacement,
                              TypeRegion **out_result) {
    if (!ts || !type || !replacement || !out_result)
        return false;

    /* 引用语义说明。
     * 当 type 是目标类型变量时，此函数直接将 replacement 指针赋值给
     * *out_result，而不创建 replacement 的深拷贝。这意味着：
     * 1. 调用者不能在返回后释放 replacement，否则 out_result 将成为悬垂指针。
     * 2. 多次调用可能返回指向同一 TypeRegion 的指针，修改一处会影响所有引用。
     * 3. 调用者需要自行管理 replacement 的生命周期。
     * 对于复合类型（FUNCTION、PRODUCT、SUM），此函数会创建新的 TypeRegion
     * 容器，但其中的子类型仍可能共享引用。 */

    /* 如果是类型变量 */
    if (type->kind == TYPE_KIND_VARIABLE && type->variable_id == var_id) {
        *out_result = replacement;
        return true;
    }

    /* 递归替换 */
    switch (type->kind) {
        case TYPE_KIND_FUNCTION: {
            TypeRegion *new_input = NULL;
            TypeRegion *new_output = NULL;

            if (type->input_type) {
                type_substitute_variable(ts, type->input_type, var_id, replacement, &new_input);
            }
            if (type->output_type) {
                type_substitute_variable(ts, type->output_type, var_id, replacement, &new_output);
            }

            *out_result = type_create_function(ts, new_input, new_output);
            return true;
        }

        case TYPE_KIND_PRODUCT: {
            TypeRegion *new_left = NULL;
            TypeRegion *new_right = NULL;

            if (type->left_type) {
                type_substitute_variable(ts, type->left_type, var_id, replacement, &new_left);
            }
            if (type->right_type) {
                type_substitute_variable(ts, type->right_type, var_id, replacement, &new_right);
            }

            *out_result = type_create_product(ts, new_left, new_right);
            return true;
        }

        case TYPE_KIND_SUM: {
            TypeRegion *new_first = NULL;
            TypeRegion *new_second = NULL;

            if (type->first_type) {
                type_substitute_variable(ts, type->first_type, var_id, replacement, &new_first);
            }
            if (type->second_type) {
                type_substitute_variable(ts, type->second_type, var_id, replacement, &new_second);
            }

            *out_result = type_create_sum(ts, new_first, new_second);
            return true;
        }

        case TYPE_KIND_DEPENDENT:
            /* 依赖类型：递归替换体类型中的变量 */
            {
                TypeRegion *new_body = NULL;

                if (type->body_type) {
                    type_substitute_variable(ts, type->body_type, var_id, replacement, &new_body);
                }

                *out_result = type_create_dependent(ts, type->param_node_id, new_body ? new_body : type->body_type);
                return true;
            }

        case TYPE_KIND_REGION:
            /* 区域类型：递归替换被别名类型的变量 */
            if (type->aliased_type) {
                TypeRegion *new_aliased = NULL;
                if (type_substitute_variable(ts, type->aliased_type, var_id, replacement, &new_aliased)) {
                    if (new_aliased != type->aliased_type) {
                        /* 创建新的区域类型，保留原有属性但替换被别名类型 */
                        TypeRegion *new_region =
                            type_create_region(ts, type->contained_node_ids, type->contained_count);
                        if (new_region) {
                            new_region->aliased_type = new_aliased;
                            if (type->alias_name) {
                                new_region->alias_name = lv_strdup(type->alias_name);
                            }
                            *out_result = new_region;
                            return true;
                        }
                    }
                }
            }
            /* 无需替换 */
            *out_result = type;
            return true;

        default:
            /* 其他类型不变 */
            *out_result = type;
            return true;
    }
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

/* ============== 类型附加到节点 ============== */

/* 映射表初始容量 */
#define NODE_TYPE_MAPPING_INITIAL_CAPACITY 16

/**
 * @brief 将类型附加到约束图节点
 *
 * 在类型系统的节点-类型映射表中建立节点 ID 与类型的关联。
 * 若该节点已存在映射，则更新类型；否则添加新映射条目。
 *
 * @param ts      类型系统指针
 * @param node_id 约束图节点 ID（必须 > 0）
 * @param type    待附加的类型区域指针
 * @return true 附加成功，false 参数无效或内存不足
 */
bool type_attach_to_node(TypeSystem *ts, int node_id, TypeRegion *type) {
    if (!ts || !type || node_id <= 0)
        return false;

    /* 检查是否已存在该节点的映射，若存在则更新 */
    for (int i = 0; i < ts->node_type_mapping_count; i++) {
        if (ts->node_type_mappings[i].node_id == node_id) {
            ts->node_type_mappings[i].type = type;
            return true;
        }
    }

    /* 需要扩容 */
    if (!lv_ensure_capacity((void **)&ts->node_type_mappings, ts->node_type_mapping_count,
                            &ts->node_type_mapping_capacity, sizeof(NodeTypeMapping), 1))
        return false;

    /* 添加新映射 */
    ts->node_type_mappings[ts->node_type_mapping_count].node_id = node_id;
    ts->node_type_mappings[ts->node_type_mapping_count].type = type;
    ts->node_type_mapping_count++;

    /* 流式事件：类型附加到节点 */
    if (type_system_stream_ctx != NULL) {
        char buf[128];
        snprintf(buf, sizeof(buf), "类型附加到节点: node_id=%d, type=%s", node_id, type_kind_to_string(type->kind));
        stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_NODE_ADDED, buf, 0);
    }

    return true;
}

/**
 * @brief 获取约束图节点关联的类型
 *
 * @param ts      类型系统指针
 * @param node_id 约束图节点 ID
 * @return 关联的类型区域指针，未找到返回 NULL
 */
TypeRegion *type_get_node_type(const TypeSystem *ts, int node_id) {
    if (!ts || node_id <= 0)
        return NULL;

    for (int i = 0; i < ts->node_type_mapping_count; i++) {
        if (ts->node_type_mappings[i].node_id == node_id) {
            return ts->node_type_mappings[i].type;
        }
    }

    return NULL;
}

/**
 * @brief 解除约束图节点与类型的关联
 *
 * 从节点-类型映射表中移除指定节点的映射条目。
 * 使用交换删除策略保持数组紧凑。
 *
 * @param ts      类型系统指针
 * @param node_id 约束图节点 ID
 * @return true 解除成功，false 未找到或参数无效
 */
bool type_detach_node_type(TypeSystem *ts, int node_id) {
    if (!ts || node_id <= 0)
        return false;

    for (int i = 0; i < ts->node_type_mapping_count; i++) {
        if (ts->node_type_mappings[i].node_id == node_id) {
            /* 将最后一个条目移到当前位置，保持数组紧凑 */
            ts->node_type_mappings[i] = ts->node_type_mappings[ts->node_type_mapping_count - 1];
            ts->node_type_mapping_count--;
            return true;
        }
    }

    return false; /* 未找到 */
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

/* ============== 辅助函数 ============== */

const char *type_kind_to_string(TypeKind kind) {
    switch (kind) {
        case TYPE_KIND_POINT:
            return "Point";
        case TYPE_KIND_LINE_SEGMENT:
            return "LineSegment";
        case TYPE_KIND_REGION:
            return "Region";
        case TYPE_KIND_FUNCTION:
            return "Function";
        case TYPE_KIND_PRODUCT:
            return "Product";
        case TYPE_KIND_SUM:
            return "Sum";
        case TYPE_KIND_VARIABLE:
            return "Variable";
        case TYPE_KIND_DEPENDENT:
            return "Dependent";
        case TYPE_KIND_BOTTOM:
            return "Bottom";
        default:
            return "Unknown";
    }
}

/**
 * Convert a UniverseLevel to its string representation.
 *
 * NOTE: This function is NOT thread-safe because it uses a static
 * internal buffer.  Callers must not rely on the returned pointer
 * remaining valid after a subsequent call from another thread.
 */
const char *universe_level_to_string(UniverseLevel level) {
    /* 由于层级现在是任意整数，使用静态缓冲区格式化 */
    static __thread char buf[32];
    if (level == UNIVERSE_BASE)
        return "Base";
    if (level == UNIVERSE_TYPE_1)
        return "Type1";
    snprintf(buf, sizeof(buf), "Type%d", level);
    return buf;
}

const char *type_equiv_result_to_string(TypeEquivResult result) {
    switch (result) {
        case TYPE_EQUIV_OK:
            return "Equivalent";
        case TYPE_EQUIV_NOT_EQUIV:
            return "NotEquivalent";
        case TYPE_EQUIV_UNKNOWN:
            return "Unknown";
        case TYPE_EQUIV_ERROR:
            return "Error";
        case TYPE_EQUIV_NEEDS_INTERACTION:
            return "NeedsInteraction";
        default:
            return "Unknown";
    }
}

const char *type_check_result_to_string(TypeCheckResult result) {
    switch (result) {
        case TYPE_CHECK_OK:
            return "OK";
        case TYPE_CHECK_MISMATCH:
            return "Mismatch";
        case TYPE_CHECK_LEVEL_ERROR:
            return "LevelError";
        case TYPE_CHECK_CYCLE:
            return "Cycle";
        case TYPE_CHECK_INFERRED:
            return "Inferred";
        case TYPE_CHECK_ERROR:
            return "Error";
        default:
            return "Unknown";
    }
}

/**
 * @brief 打印类型结构（调试用）
 *
 * 递归打印类型区域的种类、层级、别名和子类型信息。
 *
 * @param tr     类型区域指针（可为 NULL）
 * @param indent 缩进层级（空格数）
 */
void type_print(const TypeRegion *tr, int indent) {
    if (!tr)
        return;

    for (int i = 0; i < indent; i++)
        printf("  ");

    printf("Type[%d]: %s (Level: %s)", tr->id, type_kind_to_string(tr->kind), universe_level_to_string(tr->level));

    if (tr->alias_name) {
        printf(" (alias: %s)", tr->alias_name);
    }

    printf("\n");

    /* 递归打印子类型 */
    if (tr->input_type) {
        for (int i = 0; i < indent + 1; i++)
            printf("  ");
        printf("Input:\n");
        type_print(tr->input_type, indent + 2);
    }
    if (tr->output_type) {
        for (int i = 0; i < indent + 1; i++)
            printf("  ");
        printf("Output:\n");
        type_print(tr->output_type, indent + 2);
    }
    if (tr->left_type) {
        for (int i = 0; i < indent + 1; i++)
            printf("  ");
        printf("Left:\n");
        type_print(tr->left_type, indent + 2);
    }
    if (tr->right_type) {
        for (int i = 0; i < indent + 1; i++)
            printf("  ");
        printf("Right:\n");
        type_print(tr->right_type, indent + 2);
    }
}

/* ============== 重写路径记录与回放 ============== */

#define REWRITE_PATH_INITIAL_CAPACITY 8

TypeRewritePath *type_rewrite_path_create(void) {
    TypeRewritePath *path = lv_calloc(1, sizeof(TypeRewritePath));
    if (!path)
        return NULL;

    path->steps = lv_calloc(REWRITE_PATH_INITIAL_CAPACITY, sizeof(TypeRewriteStep));
    if (!path->steps) {
        lv_free((void **) &path);
        return NULL;
    }
    path->step_count = 0;
    path->capacity = REWRITE_PATH_INITIAL_CAPACITY;

    return path;
}

/**
 * @brief 销毁类型重写路径
 *
 * 释放路径中的步骤数组和规则名称字符串。
 * 注意：不销毁 before/after 指向的 TypeRegion，因为它们由类型系统管理。
 *
 * @param path 重写路径指针（可为 NULL）
 */
void type_rewrite_path_destroy(TypeRewritePath *path) {
    if (!path)
        return;

    for (int i = 0; i < path->step_count; i++) {
        lv_free((void **) &path->steps[i].rule_name);
        /* 注意：不销毁 before/after 指向的 TypeRegion，
         * 因为它们由类型系统管理，可能被共享引用 */
    }
    lv_free((void **) &path->steps);
    lv_free((void **) &path);
}

/**
 * @brief 记录类型重写步骤
 *
 * 向重写路径中追加一步重写记录，包含规则名称和重写前后的类型。
 *
 * @param path      重写路径指针
 * @param rule_name 应用的重写规则名称
 * @param before    重写前的类型
 * @param after     重写后的类型
 */
void type_rewrite_path_record(TypeRewritePath *path, const char *rule_name, const TypeRegion *before,
                              const TypeRegion *after) {
    if (!path)
        return;

    /* 检查是否需要扩容 */
    if (!lv_ensure_capacity((void **)&path->steps, path->step_count,
                            &path->capacity, sizeof(TypeRewriteStep), 1))
        return;

    /* 记录新步骤 */
    TypeRewriteStep *step = &path->steps[path->step_count];
    step->step_number = path->step_count;
    step->rule_name = rule_name ? lv_strdup(rule_name) : NULL;
    step->before = (TypeRegion *) before;
    step->after = (TypeRegion *) after;
    path->step_count++;
}

/**
 * @brief 回放类型重写路径到指定步骤
 *
 * 按顺序应用重写路径中的步骤，直到达到目标步骤号。
 *
 * @param path         重写路径指针
 * @param target_step  目标步骤号（从 0 开始）
 * @return true 回放成功，false 参数无效或目标步骤超出范围
 */
bool type_rewrite_path_replay(TypeRewritePath *path, int target_step) {
    if (!path)
        return false;

    /* 验证目标步骤在有效范围内 */
    if (target_step < 0 || target_step >= path->step_count) {
        return false;
    }

    /*
     * 回放机制：
     * 从步骤0开始，依次验证每一步的 before 类型与上一步的 after 类型匹配。
     * 对于 target_step 之前的所有步骤，验证重写链的连续性。
     *
     * 回放不会修改类型系统状态，仅验证路径的连续性。
     * 实际的类型回放需要外部调用者根据路径信息执行。
     */
    for (int i = 1; i <= target_step; i++) {
        TypeRewriteStep *prev = &path->steps[i - 1];
        TypeRewriteStep *curr = &path->steps[i];

        /* 验证连续性：当前步骤的 before 应与上一步骤的 after 相同 */
        /* 按结构比较类型区域，而非通过指针比较 */
        bool types_match = (prev->after == curr->before); /* fast path: same pointer */
        if (!types_match && prev->after && curr->before) {
            types_match = (prev->after->kind == curr->before->kind && prev->after->level == curr->before->level);
            /* 如果两者都有别名则比较别名 */
            if (types_match && prev->after->alias_name && curr->before->alias_name) {
                types_match = (strcmp(prev->after->alias_name, curr->before->alias_name) == 0);
            } else if (types_match && (prev->after->alias_name || curr->before->alias_name)) {
                types_match = false; /* one has alias_name, other doesn't */
            }
        }
        if (!types_match) {
            /* 类型不匹配，路径不连续 */
            return false;
        }
    }

    return true;
}

const TypeRewritePath *type_system_get_rewrite_path(const TypeSystem *ts) {
    if (!ts)
        return NULL;
    return ts->rewrite_path;
}

/* ============== 规则表驱动的类型推断 ============== */

/**
 * 内部辅助：对推断规则数组按优先级升序排序（插入排序，规则数量通常很少）
 */
static void inference_rules_sort_by_priority(TypeInferenceRule *rules, int count) {
    for (int i = 1; i < count; i++) {
        TypeInferenceRule key = rules[i];
        int j = i - 1;
        while (j >= 0 && rules[j].priority > key.priority) {
            rules[j + 1] = rules[j];
            j--;
        }
        rules[j + 1] = key;
    }
}

/**
 * @brief 注册类型推断规则
 *
 * 向类型系统的推断规则表中添加一条新规则。规则按优先级排序，
 * 优先级数值越小越先执行。
 *
 * @param ts               类型系统指针
 * @param source_node_type 源节点类型（GEOM_POINT 等）
 * @param target_type_kind 目标类型种类（TYPE_KIND_POINT 等）
 * @param priority         优先级（数值越小越先执行）
 * @param description      规则描述（可为 NULL）
 * @return 规则 ID（>= 0），失败返回 -1
 */
int type_system_register_inference_rule(TypeSystem *ts, int source_node_type, int target_type_kind, int priority,
                                        const char *description) {
    if (!ts)
        return -1;

    /* 需要扩容 */
    if (!lv_ensure_capacity((void **)&ts->inference_rules, ts->inference_rule_count,
                            &ts->inference_rule_capacity, sizeof(TypeInferenceRule), 1))
        return -1;

    /* 添加新规则 */
    TypeInferenceRule *rule = &ts->inference_rules[ts->inference_rule_count];
    rule->source_node_type = source_node_type;
    rule->target_type_kind = target_type_kind;
    rule->priority = priority;
    rule->description = description;
    ts->inference_rule_count++;

    /* 按优先级重新排序 */
    inference_rules_sort_by_priority(ts->inference_rules, ts->inference_rule_count);

    /* 流式事件：注册成功 */
    if (type_system_stream_ctx != NULL) {
        stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_INFO, "推理规则注册成功", 0);
    }

    return 0;
}

/**
 * @brief 获取类型系统的推断规则表
 *
 * @param ts         类型系统指针
 * @param rule_count 输出参数，接收规则数量
 * @return 规则数组指针（只读），失败返回 NULL
 */
const TypeInferenceRule *type_system_get_inference_rules(TypeSystem *ts, int *rule_count) {
    if (!ts) {
        if (rule_count)
            *rule_count = 0;
        return NULL;
    }
    if (rule_count)
        *rule_count = ts->inference_rule_count;
    return ts->inference_rules;
}

/**
 * @brief 清空类型推断规则表
 *
 * @param ts 类型系统指针（可为 NULL）
 */
void type_system_clear_inference_rules(TypeSystem *ts) {
    if (!ts)
        return;
    ts->inference_rule_count = 0;
    /* 不释放内存，保留容量以供后续注册使用 */
}

/**
 * @brief 基于规则表的类型推断
 *
 * 遍历已注册的推断规则，按优先级顺序尝试推断指定节点的类型。
 *
 * @param ts      类型系统指针
 * @param graph   约束图指针
 * @param node_id 待推断的节点 ID
 * @return 类型等价结果
 */
TypeEquivResult type_infer_by_rules(TypeSystem *ts, ConstraintGraph *graph, int node_id) {
    if (!ts || !graph)
        return TYPE_EQUIV_NOT_EQUIV;

    GeomNode *node = graph_get_node(graph, node_id);
    if (!node)
        return TYPE_EQUIV_NOT_EQUIV;

    /* 遍历已排序的规则表（按优先级升序） */
    for (int i = 0; i < ts->inference_rule_count; i++) {
        const TypeInferenceRule *rule = &ts->inference_rules[i];

        /* 检查规则是否匹配当前节点类型 */
        if (rule->source_node_type != (int) node->type) {
            continue;
        }

        /* 匹配成功：根据 target_type_kind 创建对应类型 */
        TypeRegion *type = NULL;

        switch (rule->target_type_kind) {
            case TYPE_KIND_POINT:
                type = type_create_point(ts);
                break;

            case TYPE_KIND_LINE_SEGMENT:
                type = type_create_line_segment(ts);
                break;

            case TYPE_KIND_REGION:
                type = type_create_region(ts, NULL, 0);
                break;

            case TYPE_KIND_FUNCTION:
                type = type_create_function(ts, NULL, NULL);
                break;

            case TYPE_KIND_VARIABLE:
                type = type_create_variable(ts, NULL);
                break;

            case TYPE_KIND_BOTTOM:
                type = type_create_bottom(ts);
                break;

            default:
                /* 不支持的类型种类，跳过此规则 */
                continue;
        }

        if (!type)
            return TYPE_EQUIV_NOT_EQUIV;

        /* 将推断出的类型附加到节点 */
        if (!type_attach_to_node(ts, node_id, type)) {
            return TYPE_EQUIV_NOT_EQUIV;
        }

        /* 流式事件：规则推断成功 */
        if (type_system_stream_ctx != NULL) {
            char buf[128];
            snprintf(buf, sizeof(buf), "规则推断成功: 节点 %d -> %s", node_id, type_kind_to_string(type->kind));
            stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_INFO, buf, 0);
        }

        return TYPE_EQUIV_OK;
    }

    /* 无规则匹配 */
    /* 流式事件：规则表推断完成，无匹配规则 */
    if (type_system_stream_ctx != NULL) {
        char buf[128];
        snprintf(buf, sizeof(buf), "规则表推断完成: 节点 %d 无匹配规则", node_id);
        stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_INFO, buf, 0);
    }

    return TYPE_EQUIV_NOT_EQUIV;
}

/* ============== 路径探索器 (PathExplorer) ============== */

/**
 * @brief 路径探索器内部结构
 *
 * 通过 TypeSystem 的重写规则集，在 TypeRegion 空间中搜索从
 * current 到 target 的重写路径。使用 GraphSnapshot 实现撤销。
 */
#define EXPLORER_INITIAL_CAPACITY 16
#define EXPLORER_HISTORY_INITIAL_CAPACITY 16

struct PathExplorer {
    TypeSystem *ts;      /* 类型系统（不拥有） */
    TypeRegion *current; /* 当前类型区域（探索器拥有副本） */
    TypeRegion *target;  /* 目标类型区域（不拥有，外部引用） */

    /* 探索历史 */
    ExplorerStep *steps; /* 已执行步骤数组 */
    int step_count;      /* 当前步骤数 */
    int step_capacity;   /* 步骤数组容量 */

    /* 撤销栈：每步应用前保存当前类型的深拷贝 */
    TypeRegion **undo_stack; /* 撤销栈（每个元素为 TypeRegion 深拷贝） */
    int undo_count;          /* 撤销栈深度 */
    int undo_capacity;       /* 撤销栈容量 */
};

/**
 * @brief 深拷贝类型区域
 *
 * 创建 TypeRegion 的递归深拷贝，用于撤销栈。
 * 仅拷贝关键字段（kind, level, id, alias_name, variable_name 等）。
 *
 * @param src 源类型区域
 * @return 新分配的深拷贝，失败返回 NULL
 */
TypeRegion *type_region_deep_copy(const TypeRegion *src) {
    if (!src)
        return NULL;

    TypeRegion *dst = (TypeRegion *) lv_calloc(1, sizeof(TypeRegion));
    if (!dst)
        return NULL;

    /* 复制基本字段 */
    dst->id = src->id;
    dst->kind = src->kind;
    dst->level = src->level;
    dst->variable_id = src->variable_id;

    /* 复制 contained_node_ids */
    if (src->contained_count > 0 && src->contained_node_ids) {
        dst->contained_node_ids = (int *) lv_calloc(src->contained_count, sizeof(int));
        if (dst->contained_node_ids) {
            memcpy(dst->contained_node_ids, src->contained_node_ids, src->contained_count * sizeof(int));
            dst->contained_count = src->contained_count;
        }
    }

    /* 复制 constraint_ids */
    if (src->constraint_count > 0 && src->constraint_ids) {
        dst->constraint_ids = (int *) lv_calloc(src->constraint_count, sizeof(int));
        if (dst->constraint_ids) {
            memcpy(dst->constraint_ids, src->constraint_ids, src->constraint_count * sizeof(int));
            dst->constraint_count = src->constraint_count;
        }
    }

    /* 复制字符串字段 */
    if (src->alias_name) {
        dst->alias_name = lv_strdup(src->alias_name);
    }
    if (src->variable_name) {
        dst->variable_name = lv_strdup(src->variable_name);
    }

    /* 递归复制子类型（仅一层，避免循环引用） */
    if (src->input_type) {
        dst->input_type = type_region_deep_copy(src->input_type);
    }
    if (src->output_type) {
        dst->output_type = type_region_deep_copy(src->output_type);
    }
    if (src->left_type) {
        dst->left_type = type_region_deep_copy(src->left_type);
    }
    if (src->right_type) {
        dst->right_type = type_region_deep_copy(src->right_type);
    }
    if (src->first_type) {
        dst->first_type = type_region_deep_copy(src->first_type);
    }
    if (src->second_type) {
        dst->second_type = type_region_deep_copy(src->second_type);
    }
    if (src->body_type) {
        dst->body_type = type_region_deep_copy(src->body_type);
    }
    if (src->aliased_type) {
        dst->aliased_type = type_region_deep_copy(src->aliased_type);
    }

    return dst;
}

/**
 * @brief 释放深拷贝的类型区域
 *
 * 与 type_region_deep_copy 配对使用，递归释放所有子类型和字符串。
 *
 * @param tr 要释放的类型区域
 */
void type_region_deep_free(TypeRegion *tr) {
    if (!tr)
        return;

    /* 递归释放子类型 */
    type_region_deep_free(tr->input_type);
    type_region_deep_free(tr->output_type);
    type_region_deep_free(tr->left_type);
    type_region_deep_free(tr->right_type);
    type_region_deep_free(tr->first_type);
    type_region_deep_free(tr->second_type);
    type_region_deep_free(tr->body_type);
    type_region_deep_free(tr->aliased_type);

    /* 释放数组 */
    lv_free((void **) &tr->contained_node_ids);
    lv_free((void **) &tr->constraint_ids);

    /* 释放字符串 */
    lv_free((void **) &tr->alias_name);
    lv_free((void **) &tr->variable_name);

    lv_free((void **) &tr);
}

PathExplorer *path_explorer_create(TypeSystem *ts, TypeRegion *current, TypeRegion *target) {
    if (!ts || !current || !target)
        return NULL;

    PathExplorer *explorer = (PathExplorer *) lv_calloc(1, sizeof(PathExplorer));
    if (!explorer)
        return NULL;

    explorer->ts = ts;
    explorer->target = target;

    /* 深拷贝当前类型区域（探索器拥有副本） */
    explorer->current = type_region_deep_copy(current);
    if (!explorer->current) {
        lv_free((void **) &explorer);
        return NULL;
    }

    /* 初始化步骤数组 */
    explorer->step_capacity = EXPLORER_INITIAL_CAPACITY;
    explorer->steps = (ExplorerStep *) lv_calloc(explorer->step_capacity, sizeof(ExplorerStep));
    if (!explorer->steps) {
        type_region_deep_free(explorer->current);
        lv_free((void **) &explorer);
        return NULL;
    }
    explorer->step_count = 0;

    /* 初始化撤销栈 */
    explorer->undo_capacity = EXPLORER_HISTORY_INITIAL_CAPACITY;
    explorer->undo_stack = (TypeRegion **) lv_calloc(explorer->undo_capacity, sizeof(TypeRegion *));
    if (!explorer->undo_stack) {
        lv_free((void **) &explorer->steps);
        type_region_deep_free(explorer->current);
        lv_free((void **) &explorer);
        return NULL;
    }
    explorer->undo_count = 0;

    return explorer;
}

void path_explorer_destroy(PathExplorer *explorer) {
    if (!explorer)
        return;

    /* 释放当前类型副本 */
    type_region_deep_free(explorer->current);

    /* 释放步骤记录 */
    for (int i = 0; i < explorer->step_count; i++) {
        lv_free((void **) &explorer->steps[i].rule_name);
    }
    lv_free((void **) &explorer->steps);

    /* 释放撤销栈 */
    for (int i = 0; i < explorer->undo_count; i++) {
        type_region_deep_free(explorer->undo_stack[i]);
    }
    lv_free((void **) &explorer->undo_stack);

    lv_free((void **) &explorer);
}

ExplorerResult path_explorer_get_applicable_rules(const PathExplorer *explorer, int **rule_indices, int *count) {
    if (!explorer || !rule_indices || !count)
        return EXPLORER_ERROR;

    *rule_indices = NULL;
    *count = 0;

    /* 先检查是否已达到目标 */
    bool reached = false;
    TypeEquivResult equiv = type_check_equivalence(explorer->ts, explorer->current, explorer->target, true);
    if (equiv == TYPE_EQUIV_OK) {
        return EXPLORER_GOAL_REACHED;
    }

    /* 遍历所有重写规则，检查哪些可以匹配当前类型 */
    int *indices = (int *) lv_calloc(explorer->ts->rewrite_rule_count, sizeof(int));
    if (!indices)
        return EXPLORER_ERROR;

    int applicable = 0;
    for (int i = 0; i < explorer->ts->rewrite_rule_count; i++) {
        RewriteRule *rule = explorer->ts->rewrite_rules[i];
        if (!rule || !rule->pattern)
            continue;

        /* 可应用性检查：规则模式与当前类型结构匹配 */
        bool rule_applicable = false;
        if (rule->name && rule->pattern) {
            /* 检查规则模式的顶层类型种类是否匹配当前类型 */
            if (rule->pattern->kind == TYPE_KIND_VARIABLE) {
                /* 模式为类型变量 → 可匹配任何类型 */
                rule_applicable = true;
            } else if (explorer->current && rule->pattern->kind == explorer->current->kind) {
                /* 顶层种类匹配 → 候选规则 */
                rule_applicable = true;
            } else if (rule->pattern->kind == TYPE_KIND_BOTTOM) {
                /* 底部类型模式可匹配任何类型 */
                rule_applicable = true;
            }
        }
        if (rule_applicable) {
            indices[applicable++] = i;
        }
    }

    if (applicable == 0) {
        lv_free((void **) &indices);
        return EXPLORER_NO_RULES;
    }

    *rule_indices = indices;
    *count = applicable;
    return EXPLORER_OK;
}

ExplorerResult path_explorer_preview_rule(PathExplorer *explorer, int rule_index, TypeRegion **preview_result) {
    if (!explorer || !preview_result)
        return EXPLORER_ERROR;
    *preview_result = NULL;

    /* 验证规则索引有效 */
    if (rule_index < 0 || rule_index >= explorer->ts->rewrite_rule_count) {
        return EXPLORER_INVALID_RULE;
    }

    RewriteRule *rule = explorer->ts->rewrite_rules[rule_index];
    if (!rule || !rule->name) {
        return EXPLORER_INVALID_RULE;
    }

    /*
     * 预览：创建当前类型的深拷贝作为预览结果。
     * 实际的重写效果取决于重写引擎的匹配和替换，
     * 这里返回当前类型的副本作为保守预览。
     * 调用者可据此判断规则是否值得应用。
     */
    TypeRegion *preview = type_region_deep_copy(explorer->current);
    if (!preview)
        return EXPLORER_ERROR;

    *preview_result = preview;

    /* 流式事件：预览规则 */
    if (type_system_stream_ctx != NULL) {
        char buf[128];
        snprintf(buf, sizeof(buf), "路径探索: 预览规则 '%s'", rule->name ? rule->name : "?");
        stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_INFO, buf, 0);
    }

    return EXPLORER_OK;
}

ExplorerResult path_explorer_apply_rule(PathExplorer *explorer, int rule_index) {
    if (!explorer)
        return EXPLORER_ERROR;

    /* 验证规则索引有效 */
    if (rule_index < 0 || rule_index >= explorer->ts->rewrite_rule_count) {
        return EXPLORER_INVALID_RULE;
    }

    RewriteRule *rule = explorer->ts->rewrite_rules[rule_index];
    if (!rule || !rule->name) {
        return EXPLORER_INVALID_RULE;
    }

    /* 应用前：将当前类型压入撤销栈 */
    if (explorer->undo_count >= explorer->undo_capacity) {
        if (explorer->undo_capacity > INT_MAX / 2)
            return EXPLORER_ERROR;
        int new_cap = explorer->undo_capacity * 2;
        TypeRegion **new_stack = (TypeRegion **) lv_realloc(explorer->undo_stack, new_cap * sizeof(TypeRegion *));
        if (!new_stack)
            return EXPLORER_ERROR;
        explorer->undo_stack = new_stack;
        explorer->undo_capacity = new_cap;
    }

    TypeRegion *snapshot = type_region_deep_copy(explorer->current);
    if (!snapshot)
        return EXPLORER_ERROR;
    explorer->undo_stack[explorer->undo_count++] = snapshot;

    /*
     * 应用重写规则：
     * 使用类型系统的规范化功能尝试归一化当前类型。
     * 如果归一化成功，用归一化结果替换当前类型。
     * 这模拟了重写引擎在类型层面的效果。
     */
    TypeRegion *normalized = NULL;
    bool norm_ok = type_normalize(explorer->ts, explorer->current, &normalized);

    if (norm_ok && normalized) {
        /* 用归一化结果替换当前类型 */
        type_region_deep_free(explorer->current);
        explorer->current = normalized;
    }
    /* 如果归一化失败，保留当前类型不变（规则应用为空操作） */

    /* 记录步骤 */
    if (explorer->step_count >= explorer->step_capacity) {
        if (explorer->step_capacity > INT_MAX / 2) {
            /* 步骤记录失败，但状态已改变，仍返回成功 */
            return EXPLORER_OK;
        }
        int new_cap = explorer->step_capacity * 2;
        ExplorerStep *new_steps = (ExplorerStep *) lv_realloc(explorer->steps, new_cap * sizeof(ExplorerStep));
        if (!new_steps) {
            /* 步骤记录失败，但状态已改变，仍返回成功 */
            return EXPLORER_OK;
        }
        explorer->steps = new_steps;
        explorer->step_capacity = new_cap;
    }

    ExplorerStep *step = &explorer->steps[explorer->step_count];
    step->rule_index = rule_index;
    step->rule_name = rule->name ? lv_strdup(rule->name) : NULL;
    step->step_number = explorer->step_count;
    explorer->step_count++;

    /* 流式事件：规则应用成功 */
    if (type_system_stream_ctx != NULL) {
        char buf[128];
        snprintf(buf, sizeof(buf), "路径探索: 应用规则 '%s' (步骤 %d)", rule->name ? rule->name : "?",
                 explorer->step_count - 1);
        stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_REWRITE_APPLIED, buf, 0);
    }

    /* 流式事件：路径探索应用规则信息 */
    if (type_system_stream_ctx != NULL) {
        char buf[128];
        snprintf(buf, sizeof(buf), "路径探索: 应用规则 '%s'", rule->name ? rule->name : "?");
        stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_INFO, buf, 0);
    }

    return EXPLORER_OK;
}

ExplorerResult path_explorer_undo(PathExplorer *explorer) {
    if (!explorer)
        return EXPLORER_ERROR;

    if (explorer->undo_count == 0) {
        return EXPLORER_UNDO_EMPTY;
    }

    /* 弹出撤销栈顶部 */
    explorer->undo_count--;
    TypeRegion *restored = explorer->undo_stack[explorer->undo_count];
    explorer->undo_stack[explorer->undo_count] = NULL;

    /* 替换当前类型 */
    type_region_deep_free(explorer->current);
    explorer->current = restored;

    /* 移除最后一步记录 */
    if (explorer->step_count > 0) {
        explorer->step_count--;
        lv_free((void **) &explorer->steps[explorer->step_count].rule_name);
        explorer->steps[explorer->step_count].rule_name = NULL;
    }

    /* 流式事件：撤销操作 */
    if (type_system_stream_ctx != NULL) {
        stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_REWRITE_ROLLBACK, "路径探索: 撤销上一步操作", 0);
    }

    return EXPLORER_OK;
}

ExplorerResult path_explorer_check_goal(const PathExplorer *explorer, bool *reached) {
    if (!explorer || !reached)
        return EXPLORER_ERROR;

    TypeEquivResult equiv = type_check_equivalence(explorer->ts, explorer->current, explorer->target, true);
    *reached = (equiv == TYPE_EQUIV_OK);

    /* 流式事件：目标检查结果 */
    if (type_system_stream_ctx != NULL && *reached) {
        stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_INFO, "路径探索: 已达到目标类型", 0);
    }

    return EXPLORER_OK;
}

ExplorerResult path_explorer_save_path(const PathExplorer *explorer, TypeRewritePath **out_path) {
    if (!explorer || !out_path)
        return EXPLORER_ERROR;
    *out_path = NULL;

    TypeRewritePath *path = type_rewrite_path_create();
    if (!path)
        return EXPLORER_ERROR;

    /* 将每一步记录到重写路径中
     *
     * 利用撤销栈恢复 before/after 快照：
     *   undo_stack[i] 保存了第 i 步应用前的类型深拷贝。
     *   undo_stack[i+1] 保存了第 i+1 步应用前的类型（即第 i 步之后的状态）。
     *   对于最后一步，after 为 explorer->current。
     */
    for (int i = 0; i < explorer->step_count; i++) {
        ExplorerStep *step = &explorer->steps[i];
        const TypeRegion *before = NULL;
        const TypeRegion *after = NULL;

        /* before: 从撤销栈获取（应用规则前的深拷贝） */
        if (i < explorer->undo_count) {
            before = explorer->undo_stack[i];
        }

        /* after: 下一步的 before（撤销栈中），或当前类型 */
        if (i + 1 < explorer->undo_count) {
            after = explorer->undo_stack[i + 1];
        } else {
            after = explorer->current;
        }

        type_rewrite_path_record(path, step->rule_name, before, after);
    }

    *out_path = path;
    return EXPLORER_OK;
}

int path_explorer_get_step_count(const PathExplorer *explorer) {
    if (!explorer)
        return 0;
    return explorer->step_count;
}

const ExplorerStep *path_explorer_get_steps(const PathExplorer *explorer) {
    if (!explorer || explorer->step_count == 0)
        return NULL;
    return explorer->steps;
}

const TypeRegion *path_explorer_get_current(const PathExplorer *explorer) {
    if (!explorer)
        return NULL;
    return explorer->current;
}
