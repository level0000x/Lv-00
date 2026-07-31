/**
 * @file type_system.c
 * @brief 类型系统核心实现 —— 类型系统管理、类型区域创建与管理、宇宙层级检查、
 *        节点-类型映射、辅助函数、重写路径记录、规则表驱动推断、深拷贝
 *
 * @details 本文件为类型系统核心模块，管理 TypeSystem 上下文的创建与销毁、
 *          所有 TypeRegion 工厂函数的实现、宇宙层级检查机制、类型与几何节点的
 *          外部映射、类型重写路径的记录与回放、规则表驱动的类型推断以及深拷贝工具。
 *
 *          与之配合的子模块：
 *          - type_check.c   : 类型等价检查、端口兼容性、循环检测、规范化、依赖检查
 *          - type_infer.c   : 节点/端口类型推断、变量实例化与替换
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
 *   - lv/lv.h              : Lv-00 核心头文件
 *   - lv_internal.h        : 内部数据结构与常量
 *   - lv_utils.h           : 统一内存分配器
 *   - rewrite.h            : 图重写引擎（类型等价检查的重写路径）
 *   - lv/stream.h          : 流式事件输出
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

/* 流式上下文（非 static，供 type_path_explorer.c 通过 extern 访问） */
lv_UNUSED_ATTR lv_THREAD_LOCAL StreamContext *type_system_stream_ctx = NULL;

void type_system_set_stream_context(StreamContext *ctx) {
    type_system_stream_ctx = ctx;
}

/* ============== 内部辅助宏 ============== */

/* 推断规则表初始容量 */
#define INFERENCE_RULE_INITIAL_CAPACITY 8

/**
 * 类型推断与等价检查的递归深度限制已迁移至 lvConfig 运行时系统。
 * 通过 lv_config_get_int("type_infer_max_depth", 100) 和
 * lv_config_get_int("type_equiv_max_depth", 16) 读取。
 */

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
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "type_system_create: lv_calloc failed");

    ts->well_founded = true;                      /* 默认启用良基模式 */
    ts->cumulative = true;                        /* 默认启用累积性 */
    ts->max_universe_level = UNIVERSE_TYPE_1 + 1; /* 默认最大层级为2，但整数层级无上限 */

    /* 初始化重写路径 */
    ts->rewrite_path = type_rewrite_path_create();
    if (!ts->rewrite_path) {
        lv_free((void **) &ts);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "type_system_create: type_rewrite_path_create failed");
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
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "type_region_create: lv_calloc failed");

    tr->kind = kind;

    /* 添加到类型系统 */
    if (ts->type_region_count >= INT_MAX) {
        lv_free((void **) &tr);
        lv_RETURN_ERROR_NULL(lv_ERROR_OVERFLOW, "type_region_create: type_region_count overflow");
    }
    /* 确保容量 */
    if (!lv_ensure_capacity((void **)&ts->type_regions, ts->type_region_count,
                            &ts->type_region_capacity, sizeof(TypeRegion *), 1)) {
        lv_free((void **)&tr);
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "type_region_create: lv_ensure_capacity failed");
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
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "type_create_region: type_region_create failed");

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
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "type_create_function: type_region_create failed");

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
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "type_create_product: type_region_create failed");

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
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "type_create_sum: type_region_create failed");

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
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "type_create_variable: type_region_create failed");

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
            lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "type_create_variable: lv_realloc type_vars failed");
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
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "type_create_dependent: type_region_create failed");

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
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "type_create_predicate_subtype: NULL parameter");

    TypeRegion *tr = type_region_create(ts, TYPE_KIND_PREDICATE_SUBTYPE);
    if (!tr)
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "type_create_predicate_subtype: type_region_create failed");

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
        lv_RETURN_ERROR_NULL(lv_ERROR_ALLOCATION_FAILED, "type_create_predicate_subtype: lv_strdup predicate_name failed");
    }

    return tr;
}

TypeRegion *type_predicate_subtype_get_base(TypeRegion *subtype) {
    if (!subtype || subtype->kind != TYPE_KIND_PREDICATE_SUBTYPE)
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "type_predicate_subtype_get_base: NULL or wrong kind");
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
        lvStrBuf sb_5 = {0};
        lv_strbuf_printf(&sb_5, "类型附加到节点: node_id=%d, type=%s", node_id, type_kind_to_string(type->kind));
        stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_NODE_ADDED, sb_5.data, 0);
        lv_strbuf_destroy(&sb_5);
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
        lv_RETURN_ERROR_NULL(lv_ERROR_INVALID_PARAM, "type_get_node_type: NULL ts or invalid node_id");

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

/* ============== 辅助函数 ============== */

const char *type_kind_to_string(TypeKind kind) {
    switch (kind) {
        LV_TYPE_KIND_X(LV_X_TO_STR_CASE)
        default: return "Unknown";
    }
}

/**
 * Convert a UniverseLevel to its string representation.
 *
 * NOTE: 使用线程局部 scratch 缓冲区（lv_utils.h 的 lv_fmt_tmp）。
 * 调用者不得在后续调用后继续使用返回的指针（scratch 语义）。
 */
const char *universe_level_to_string(UniverseLevel level) {
    if (level == UNIVERSE_BASE)
        return "Base";
    if (level == UNIVERSE_TYPE_1)
        return "Type1";
    return lv_fmt_tmp("Type%d", level);
}

static const char *s_equiv_result_names[] = {
    [TYPE_EQUIV_OK]                = "Equivalent",
    [TYPE_EQUIV_NOT_EQUIV]         = "NotEquivalent",
    [TYPE_EQUIV_UNKNOWN]           = "Unknown",
    [TYPE_EQUIV_ERROR]             = "Error",
    [TYPE_EQUIV_NEEDS_INTERACTION] = "NeedsInteraction",
};

const char *type_equiv_result_to_string(TypeEquivResult result) {
    if ((unsigned)result >= sizeof(s_equiv_result_names) / sizeof(s_equiv_result_names[0]))
        return "Unknown";
    return s_equiv_result_names[result];
}

static const char *s_check_result_names[] = {
    [TYPE_CHECK_OK]          = "OK",
    [TYPE_CHECK_MISMATCH]   = "Mismatch",
    [TYPE_CHECK_LEVEL_ERROR]= "LevelError",
    [TYPE_CHECK_CYCLE]      = "Cycle",
    [TYPE_CHECK_INFERRED]   = "Inferred",
    [TYPE_CHECK_ERROR]      = "Error",
};

const char *type_check_result_to_string(TypeCheckResult result) {
    if ((unsigned)result >= sizeof(s_check_result_names) / sizeof(s_check_result_names[0]))
        return "Unknown";
    return s_check_result_names[result];
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
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "type_system_register_inference_rule: ts is NULL");

    /* 需要扩容 */
    if (!lv_ensure_capacity((void **)&ts->inference_rules, ts->inference_rule_count,
                            &ts->inference_rule_capacity, sizeof(TypeInferenceRule), 1))
        lv_RETURN_ERROR(lv_ERROR_ALLOCATION_FAILED, "type_system_register_inference_rule: lv_ensure_capacity failed");

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
            lvStrBuf sb_6 = {0};
            lv_strbuf_printf(&sb_6, "规则推断成功: 节点 %d -> %s", node_id, type_kind_to_string(type->kind));
            stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_INFO, sb_6.data, 0);
            lv_strbuf_destroy(&sb_6);
        }

        return TYPE_EQUIV_OK;
    }

    /* 无规则匹配 */
    /* 流式事件：规则表推断完成，无匹配规则 */
    if (type_system_stream_ctx != NULL) {
        lvStrBuf sb_7 = {0};
        lv_strbuf_printf(&sb_7, "规则表推断完成: 节点 %d 无匹配规则", node_id);
        stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_INFO, sb_7.data, 0);
        lv_strbuf_destroy(&sb_7);
    }

    return TYPE_EQUIV_NOT_EQUIV;
}

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
