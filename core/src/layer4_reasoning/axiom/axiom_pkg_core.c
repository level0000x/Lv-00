/*
 * @file axiom_pkg_core.c
 * @brief Axiom package system - create/destroy, unconstructible mgmt, template mgmt
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

/* ============== 创建和销毁 ============== */

AxiomPackage *axiom_package_create(const char *name, const char *version) {
    AxiomPackage *pkg = lv_calloc(1, sizeof(AxiomPackage));
    if (!pkg)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "axiom_package_create: lv_calloc failed");

    pkg->name = safe_lv_strdup_safe(name);
    pkg->version = safe_lv_strdup_safe(version);
    lv_darray_init(&pkg->templates, sizeof(ConstraintTemplate));
    lv_darray_init(&pkg->known_unconstructibles, sizeof(KnownUnconstructible));
    lv_darray_init(&pkg->unconstructible_templates, sizeof(UnconstructibleTemplate));
    pkg->bottom_geometry = NULL;
    pkg->negation_encoding = NULL;
    pkg->contradiction_behavior = EXPLOSION_PRINCIPLE;
    lv_darray_init(&pkg->expansion_cache, sizeof(TemplateExpansionCache));
    pkg->max_expansion_depth = AXIOM_MAX_EXPANSION_DEPTH; /* 默认递归深度 */
    lv_darray_init(&pkg->dep_refs, sizeof(DependencyRef));

    if (axiom_stream_ctx) {
        stream_emit_simple(axiom_stream_ctx, STREAM_EVENT_INFO, "公理包创建成功", 0);
    }

    return pkg;
}

int axiom_package_get_template_count(const AxiomPackage *pkg) {
    if (!pkg) return 0;
    return (int)pkg->templates.count;
}

int axiom_package_get_unconstructible_count(const AxiomPackage *pkg) {
    if (!pkg) return 0;
    return (int)pkg->known_unconstructibles.count;
}

const KnownUnconstructible *axiom_package_get_unconstructible(const AxiomPackage *pkg, int index) {
    if (!pkg || index < 0 || index >= (int)pkg->known_unconstructibles.count) return NULL;
    lvDArray *arr = (lvDArray *)&pkg->known_unconstructibles;
    return (const KnownUnconstructible *)lv_darray_get(arr, index);
}

const ConstraintTemplate *axiom_package_get_template_by_index(const AxiomPackage *pkg, int index) {
    if (!pkg || index < 0 || index >= (int)pkg->templates.count) return NULL;
    lvDArray *arr = (lvDArray *)&pkg->templates;
    return (const ConstraintTemplate *)lv_darray_get(arr, index);
}

void axiom_package_destroy(AxiomPackage *pkg) {
    if (!pkg)
        return;

    lv_free((void **) &pkg->name);
    lv_free((void **) &pkg->version);

    /* 释放模板 */
    for (int i = 0; i < pkg->templates.count; i++) {
        ConstraintTemplate *t = (ConstraintTemplate *)lv_darray_get(&pkg->templates, i);
        lv_free((void **) &t->name);
        lv_free((void **) &t->params);
        if (t->compressed_subgraph) {
            graph_destroy(t->compressed_subgraph);
        }
    }
    lv_darray_free(&pkg->templates);

    /* 释放不可构造问题 */
    for (int i = 0; i < pkg->known_unconstructibles.count; i++) {
        KnownUnconstructible *uc = (KnownUnconstructible *)lv_darray_get(&pkg->known_unconstructibles, i);
        lv_free((void **) &uc->name);
        lv_free((void **) &uc->reduces_to);
        lv_free((void **) &uc->external_ref);

        /* 释放依赖链 */
        for (int j = 0; j < uc->dependency_chain.count; j++) {
            lv_free((void **) lv_darray_get(&uc->dependency_chain, j));
        }
        lv_darray_free(&uc->dependency_chain);
    }
    lv_darray_free(&pkg->known_unconstructibles);

    /* 释放不可构造性证明模板 */
    for (int i = 0; i < pkg->unconstructible_templates.count; i++) {
        UnconstructibleTemplate *tmpl = (UnconstructibleTemplate *)lv_darray_get(&pkg->unconstructible_templates, i);
        lv_free((void **) &tmpl->target_problem_name);
        lv_free((void **) &tmpl->known_unconstructible_name);
        if (tmpl->reduction_construction) {
            graph_destroy(tmpl->reduction_construction);
        }
        lv_free((void **) &tmpl->description);
    }
    lv_darray_free(&pkg->unconstructible_templates);

    lv_free((void **) &pkg->bottom_geometry);
    lv_free((void **) &pkg->negation_encoding);

    /* 释放模板展开缓存 */
    for (int i = 0; i < pkg->expansion_cache.count; i++) {
        TemplateExpansionCache *c = (TemplateExpansionCache *)lv_darray_get(&pkg->expansion_cache, i);
        lv_free((void **) &c->template_name);
        if (c->expanded_graph) {
            graph_destroy(c->expanded_graph);
        }
    }
    lv_darray_free(&pkg->expansion_cache);

    /* 释放依赖引用数组 */
    lv_darray_free(&pkg->dep_refs);

    lv_free((void **) &pkg);
}

/* ============== 不可构造问题管理 ============== */

bool axiom_package_add_known_unconstructible(AxiomPackage *pkg, KnownUnconstructible *item) {
    if (!pkg)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "axiom_package_add_known_unconstructible: pkg is NULL");
    if (!item)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "axiom_package_add_known_unconstructible: item is NULL");

    KnownUnconstructible target_item;
    memset(&target_item, 0, sizeof(KnownUnconstructible));

    /* 深拷贝语义：对所有字符串字段进行独立拷贝，
     * 确保包内部持有独立的内存副本。
     * 调用者可以安全地释放或修改原始 item 的字符串字段。 */
    target_item.name = safe_lv_strdup_safe(item->name);
    target_item.reduces_to = safe_lv_strdup_safe(item->reduces_to);
    target_item.external_ref = safe_lv_strdup_safe(item->external_ref);
    target_item.green_verified = item->green_verified;

    /* 深拷贝依赖链中的每个字符串 */
    lv_darray_init(&target_item.dependency_chain, sizeof(char *));
    for (int i = 0; i < item->dependency_chain.count; i++) {
        char *s = safe_lv_strdup_safe(*(char **)lv_darray_get(&item->dependency_chain, i));
        if (lv_darray_push(&target_item.dependency_chain, &s) < 0) {
            /* 分配失败时回滚已拷贝的字段 */
            lv_free_many((void **) &target_item.name, (void **) &target_item.reduces_to,
                         (void **) &target_item.external_ref, NULL);
            lv_darray_free(&target_item.dependency_chain);
            memset(&target_item, 0, sizeof(KnownUnconstructible));
            return false;
        }
    }

    /* 推入包数组 */
    if (lv_darray_push(&pkg->known_unconstructibles, &target_item) < 0) {
        lv_free_many((void **) &target_item.name, (void **) &target_item.reduces_to,
                     (void **) &target_item.external_ref, NULL);
        lv_darray_free(&target_item.dependency_chain);
        memset(&target_item, 0, sizeof(KnownUnconstructible));
        return false;
    }

    if (axiom_stream_ctx) {
        stream_emit_simple(axiom_stream_ctx, STREAM_EVENT_INFO, "已注册不可构造问题", 0);
    }

    return true;
}

KnownUnconstructible *axiom_package_lookup_unconstructible(AxiomPackage *pkg, const char *name) {
    if (!pkg || !name)
        return NULL;

    for (int i = 0; i < pkg->known_unconstructibles.count; i++) {
        KnownUnconstructible *uc = (KnownUnconstructible *)lv_darray_get(&pkg->known_unconstructibles, i);
        if (strcmp(uc->name, name) == 0) {
            return uc;
        }
    }
    return NULL;
}

/* ============== 模板管理 ============== */

bool axiom_package_register_template(AxiomPackage *pkg, ConstraintTemplate *tmpl) {
    if (!pkg)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "axiom_package_register_template: pkg is NULL");
    if (!tmpl)
        lv_RETURN_ERROR_BOOL(lv_ERROR_NULL_POINTER, "axiom_package_register_template: tmpl is NULL");

    ConstraintTemplate slot = *tmpl;
    /* 深拷贝 name（调用者可能释放原始字符串） */
    if (slot.name) {
        slot.name = lv_strdup_safe(slot.name);
    }
    /* 安全初始化：浅拷贝后 params 指针指向调用者的内存（或未初始化），
     * pkg 不应持有该指针的所有权。无条件置 NULL 以避免 free() 未初始化
     * 指针或调用者内存导致 bad-free / double-free。
     * 若调用者需要注册参数描述，应使用独立的 API 设置。 */
    slot.params = NULL;
    slot.param_desc_count = 0;
    /* v3.6.0: 模板分级管理初始化 */
    slot.level = TEMPLATE_LEVEL_ONE; /* 默认为一级模板 */
    slot.is_compressed = false;
    slot.compressed_subgraph = NULL;

    if (lv_darray_push(&pkg->templates, &slot) < 0) {
        lv_free((void **) &slot.name);
        return false;
    }

    if (axiom_stream_ctx) {
        stream_emit_simple(axiom_stream_ctx, STREAM_EVENT_INFO, "已注册约束模板", 0);
    }

    return true;
}

ConstraintTemplate *axiom_package_get_template(AxiomPackage *pkg, const char *name) {
    if (!pkg)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "axiom_package_get_template: pkg is NULL");
    if (!name)
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "axiom_package_get_template: name is NULL");

    for (int i = 0; i < pkg->templates.count; i++) {
        ConstraintTemplate *t = (ConstraintTemplate *)lv_darray_get(&pkg->templates, i);
        if (strcmp(t->name, name) == 0) {
            return t;
        }
    }
    return NULL;
}
