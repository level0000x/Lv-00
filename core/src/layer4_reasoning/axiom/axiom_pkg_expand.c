/*
 * @file axiom_pkg_expand.c
 * @brief Axiom package system - expansion cache, graph copy, lazy expansion
 * @details Split from axiom_pkg.c
 */

#include "axiom_pkg.h"
#include "axiom_pkg_internal.h"

#include "lv/lv_file.h"

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/sha256.h"

#include "debug.h"
#include "error_codes.h"
#include "lexer_shared.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "stream.h"

/* ============== 模板展开缓存 ============== */

/**
 * @brief 计算参数的简单哈希值（用于缓存键）
 */
static uint64_t compute_param_hash(SymbolicCoord **params, int param_count) {
    /* 使用 FNV-1a 基于序列化内容计算参数哈希（仅用于缓存键，非加密用途） */
    uint64_t hash = 14695981039346656037ULL; /* FNV offset basis */

    hash ^= (uint64_t) param_count;
    hash *= 1099511628211ULL; /* FNV prime */

    for (int i = 0; i < param_count && params && params[i]; i++) {
        char *ser = symbolic_coord_serialize(params[i]);
        if (ser) {
            for (const char *p = ser; *p; p++) {
                hash ^= (uint64_t) (unsigned char) *p;
                hash *= 1099511628211ULL; /* FNV prime */
            }
            lv_free((void **) &ser);
        }
    }

    return hash;
}

/**
 * @brief 在缓存中查找匹配的展开图
 *
 * 注意：返回的 ConstraintGraph 指针指向缓存内部持有的图对象。
 * 调用者不得修改、销毁或以其他方式变更返回的图，否则将破坏缓存一致性。
 * 如需修改展开结果，调用者应自行创建图的深拷贝后再操作。
 */
ConstraintGraph *axiom_package_lookup_expansion_cache(AxiomPackage *pkg, const char *template_name,
                                                      SymbolicCoord **params, int param_count) {
    if (!pkg)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "axiom_package_lookup_expansion_cache: pkg is NULL");
    if (!template_name)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "axiom_package_lookup_expansion_cache: template_name is NULL");

    uint64_t target_hash = compute_param_hash(params, param_count);

    for (int i = 0; i < pkg->expansion_cache.count; i++) {
        TemplateExpansionCache *c = (TemplateExpansionCache *)lv_darray_get(&pkg->expansion_cache, i);
        if (c->param_hash == target_hash && c->template_name &&
            strcmp(c->template_name, template_name) == 0) {
            return c->expanded_graph;
        }
    }

    return NULL;
}

/**
 * @brief 将展开结果存入缓存
 */
bool axiom_package_store_expansion_cache(AxiomPackage *pkg, const char *template_name, SymbolicCoord **params,
                                         int param_count, ConstraintGraph *expanded_graph) {
    if (!pkg)
        return false;

    TemplateExpansionCache c;
    c.param_hash = compute_param_hash(params, param_count);
    c.template_name = template_name ? lv_strdup_safe(template_name) : NULL;
    c.expanded_graph = expanded_graph;

    if (lv_darray_push(&pkg->expansion_cache, &c) < 0) {
        lv_free((void **) &c.template_name);
        return false;
    }

    return true;
}

/**
 * @brief 清空模板展开缓存
 */
void axiom_package_clear_expansion_cache(AxiomPackage *pkg) {
    if (!pkg)
        return;

    for (int i = 0; i < pkg->expansion_cache.count; i++) {
        TemplateExpansionCache *c = (TemplateExpansionCache *)lv_darray_get(&pkg->expansion_cache, i);
        lv_free((void **) &c->template_name);
        if (c->expanded_graph) {
            graph_destroy(c->expanded_graph);
        }
    }
    lv_darray_clear(&pkg->expansion_cache);
}

/* ============== graph_copy：约束图深拷贝 ============== */

/**
 * @brief 深拷贝约束图
 *
 * 遍历源图中的所有节点和约束，在新图中创建完全独立的副本。
 * 高级类型（Port、Region、FunctionBlock）通过 graph_add_node_with_id
 * 复制基础字段，调用者可后续设置特定字段。
 */
ConstraintGraph *graph_copy(const ConstraintGraph *graph) {
    if (!graph)
        return NULL;

    ConstraintGraph *new_graph = graph_create();
    if (!new_graph)
        return NULL;

    /* 复制所有节点 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *src = graph->nodes[i];
        if (!src)
            continue;

        /* 使用带ID接口添加节点，保持ID一致 */
        GeomNode *dst = graph_add_node_with_id(new_graph, src->id, src->type, src->symbolic_coords, src->coord_count);
        if (!dst) {
            graph_destroy(new_graph);
            return NULL;
        }

        /* 复制增强字段 */
        dst->trust = src->trust;
        dst->is_active = src->is_active;
        dst->lo_subtype = src->lo_subtype;
        dst->namespace_depth = src->namespace_depth;
        dst->parent_block_id = src->parent_block_id;
        dst->numeric_precision = src->numeric_precision;

        /* 深拷贝 numeric_assumption_declaration */
        if (src->numeric_assumption_declaration) {
            dst->numeric_assumption_declaration = lv_strdup_safe(src->numeric_assumption_declaration);
        }
    }

    /* 复制所有约束 */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *src = graph->constraints[i];
        if (!src)
            continue;

        Constraint *dst =
            graph_add_constraint_with_id(new_graph, src->id, src->type, src->participants, src->participant_count);
        if (!dst) {
            graph_destroy(new_graph);
            return NULL;
        }

        /* 复制增强字段 */
        dst->template_id = src->template_id;
        dst->is_active = src->is_active;
        dst->numeric_value = src->numeric_value;
        dst->satisfaction = src->satisfaction;
    }

    /* 复制高级图属性 */
    new_graph->dirty = graph->dirty;

    return new_graph;
}

/* ============== 模板分级管理与惰性展开 ============== */

void axiom_template_set_level(ConstraintTemplate *tmpl, TemplateLevel level) {
    if (!tmpl)
        return;
    tmpl->level = level;
    /* 二级模板默认标记为压缩态 */
    if (level == TEMPLATE_LEVEL_TWO) {
        tmpl->is_compressed = true;
    }
}

ConstraintGraph *axiom_template_expand_lazy(AxiomPackage *pkg, const char *template_name, SymbolicCoord **params,
                                            int param_count) {
    if (!pkg)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "axiom_template_expand_lazy: pkg is NULL");
    if (!template_name)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "axiom_template_expand_lazy: template_name is NULL");

    ConstraintTemplate *tmpl = axiom_package_get_template(pkg, template_name);
    if (!tmpl)
        return NULL;

    /* 先查展开缓存 */
    ConstraintGraph *cached = axiom_package_lookup_expansion_cache(pkg, template_name, params, param_count);
    if (cached)
        return cached;

    ConstraintGraph *result = NULL;

    /* 二级模板：从压缩态展开 */
    if (tmpl->level == TEMPLATE_LEVEL_TWO && tmpl->is_compressed && tmpl->compressed_subgraph) {
        result = graph_copy(tmpl->compressed_subgraph);
    } else {
        /* 正常展开 */
        result = graph_create();
        if (result && tmpl->expand) {
            tmpl->expand(params, result);
        }
    }

    /* 存入缓存并标记为非压缩态 */
    if (result) {
        axiom_package_store_expansion_cache(pkg, template_name, params, param_count, result);
        tmpl->is_compressed = false;
    }

    return result;
}

void axiom_template_compress(ConstraintTemplate *tmpl) {
    if (!tmpl || tmpl->level != TEMPLATE_LEVEL_TWO)
        return;
    tmpl->is_compressed = true;
    /* 展开缓存由缓存管理器自行处理，此处仅恢复压缩态标记 */
}
