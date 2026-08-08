/*
 * @file axiom_pkg_core.c
 * @brief Axiom package system - create/destroy, unconstructible mgmt, template mgmt
 * @details Split from axiom_pkg.c
 */

#include "axiom_pkg.h"
#include "axiom_pkg_internal.h"

#include "lv/lv_file.h"
#include "lv/lv_lifecycle.h"
#include "lv/lv_registry.h"
#include "lv/lv_thread.h"

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

/* ============== 名称索引注册表 ============== */

/**
 * @brief 公理包名称索引注册表（通用注册表设施）。
 *
 * key  = "<pkg指针>:<kind>:<name>"（kind: T = 模板，U = 不可构造问题），
 * value = boxed int 元素索引（指向 pkg->templates / pkg->known_unconstructibles
 *        lvDArray 中的元素；darray 扩容 realloc 不改变元素索引，故索引保持有效）。
 *
 * 模板/不可构造问题数据仍存储于各 pkg 的 lvDArray（公共 API get_by_index/
 * get_count 及 serialize/verify 等按索引/直接遍历访问），注册表仅承担
 * name→索引 的 strcmp 查重与查找；重复注册时首次映射生效（与原线性查找
 * "首个匹配优先"语义一致）。文件级单例（lv_once 惰性初始化，线程安全）。
 */
lv_REGISTRY_STATIC(axiom_name_registry, 32);

/** @brief 注册表 key 缓冲区大小（pkg 指针 + kind + 名称） */
#define AXIOM_REGKEY_MAX 512

/** @brief 装箱元素索引（注册表 value） */
static void *axiom_box_index(int idx) {
    int *p = (int *) lv_malloc(sizeof(int));
    if (p) {
        *p = idx;
    }
    return p;
}

/** @brief boxed 索引的注册表 destroy 回调适配器（void(*)(void*) 形态） */
static void axiom_box_destroy(void *value) {
    lv_free((void **) &value);
}

/** @brief 解箱元素索引 */
static int axiom_unbox_index(void *value) {
    return value ? *(int *) value : -1;
}

/** @brief 构造注册表 key（"<pkg>:<kind>:<name>"，kind: T/U） */
static void axiom_build_key(const AxiomPackage *pkg, char kind, const char *name, char *buf, size_t bufsz) {
    snprintf(buf, bufsz, "%p:%c:%s", (const void *) pkg, kind, name);
}

/** @brief 移除指定 pkg 的全部注册条目（pkg 销毁时调用，防止残留悬垂索引） */
static void axiom_registry_remove_pkg(const AxiomPackage *pkg) {
    if (!pkg) {
        return;
    }
    /* 可能从未注册过任何条目：确保注册表已初始化（lv_once 幂等），
     * 避免对未初始化互斥锁加锁 */
    axiom_name_registry_ensure();
    char prefix[64];
    snprintf(prefix, sizeof(prefix), "%p:", (const void *) pkg);
    size_t prefix_len = strlen(prefix);
    int total = lv_registry_count(&g_axiom_name_registry);
    for (int i = total - 1; i >= 0; i--) {
        const char *reg_name = NULL;
        void *reg_value = NULL;
        if (!lv_registry_get_at(&g_axiom_name_registry, i, &reg_name, &reg_value)) {
            continue;
        }
        if (strncmp(reg_name, prefix, prefix_len) == 0) {
            lv_registry_remove(&g_axiom_name_registry, reg_name);
        }
    }
}

/* ============== 创建和销毁 ============== */

AxiomPackage *lv_axiom_package_create(const char *name, const char *version) {
    AxiomPackage *pkg = lv_calloc(1, sizeof(AxiomPackage));
    if (!pkg)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_axiom_package_create: lv_calloc failed");

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

/* ── axiom_package_destroy 逐元素销毁适配 ── */

/* ConstraintTemplate 元素：name / params 指针 + compressed_subgraph 图 */
static void destroy_axiom_template_elem(void *elem) {
    ConstraintTemplate *t = (ConstraintTemplate *) elem;
    lv_free((void **) &t->name);
    lv_free((void **) &t->params);
    if (t->compressed_subgraph) {
        graph_destroy(t->compressed_subgraph);
        t->compressed_subgraph = NULL;
    }
}

/* KnownUnconstructible 元素：三个字符串 + dependency_chain（lvDArray<char*>） */
static void destroy_axiom_unconstructible_elem(void *elem) {
    KnownUnconstructible *uc = (KnownUnconstructible *) elem;
    lv_free((void **) &uc->name);
    lv_free((void **) &uc->reduces_to);
    lv_free((void **) &uc->external_ref);

    /* 释放依赖链（元素为 char*，逐个释放后释放数组） */
    for (int j = 0; j < uc->dependency_chain.count; j++) {
        lv_free((void **) lv_darray_get(&uc->dependency_chain, j));
    }
    lv_darray_free(&uc->dependency_chain);
}

/* UnconstructibleTemplate 元素：两个名称字符串 + reduction_construction 图 + description */
static void destroy_axiom_uctemplate_elem(void *elem) {
    UnconstructibleTemplate *tmpl = (UnconstructibleTemplate *) elem;
    lv_free((void **) &tmpl->target_problem_name);
    lv_free((void **) &tmpl->known_unconstructible_name);
    if (tmpl->reduction_construction) {
        graph_destroy(tmpl->reduction_construction);
        tmpl->reduction_construction = NULL;
    }
    lv_free((void **) &tmpl->description);
}

/* TemplateExpansionCache 元素：template_name 字符串 + expanded_graph 图 */
static void destroy_axiom_expansion_cache_elem(void *elem) {
    TemplateExpansionCache *c = (TemplateExpansionCache *) elem;
    lv_free((void **) &c->template_name);
    if (c->expanded_graph) {
        graph_destroy(c->expanded_graph);
        c->expanded_graph = NULL;
    }
}

/* axiom_package_destroy 字段描述表：释放顺序与原实现一致
 * （name/version → 6 个 darray → bottom_geometry/negation_encoding → 外壳），
 * 全部置 NULL 安全 */
static const lvFieldDesc s_axiom_package_destroy_fields[] = {
    lv_FIELD_PLAIN(AxiomPackage, name),
    lv_FIELD_PLAIN(AxiomPackage, version),
    lv_FIELD_DARRAY_ELEMS(AxiomPackage, templates, destroy_axiom_template_elem),
    lv_FIELD_DARRAY_ELEMS(AxiomPackage, known_unconstructibles, destroy_axiom_unconstructible_elem),
    lv_FIELD_DARRAY_ELEMS(AxiomPackage, unconstructible_templates, destroy_axiom_uctemplate_elem),
    lv_FIELD_PLAIN(AxiomPackage, bottom_geometry),
    lv_FIELD_PLAIN(AxiomPackage, negation_encoding),
    lv_FIELD_DARRAY_ELEMS(AxiomPackage, expansion_cache, destroy_axiom_expansion_cache_elem),
    lv_FIELD_DARRAY(AxiomPackage, dep_refs),
};

void axiom_package_destroy(AxiomPackage *pkg) {
    if (!pkg)
        return;

    /* 移除该包在名称索引注册表中的全部条目（防止残留悬垂索引） */
    axiom_registry_remove_pkg(pkg);

    lv_obj_destroy_fields(pkg, s_axiom_package_destroy_fields,
                          sizeof(s_axiom_package_destroy_fields) / sizeof(s_axiom_package_destroy_fields[0]));
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

    /* 登记名称索引（value = boxed 元素索引；重复注册保留首次映射，与线性查找"首个匹配优先"语义一致） */
    axiom_name_registry_ensure();
    char regkey[AXIOM_REGKEY_MAX];
    axiom_build_key(pkg, 'U', target_item.name, regkey, sizeof(regkey));
    void *boxed = axiom_box_index((int) pkg->known_unconstructibles.count - 1);
    if (boxed && !lv_registry_put_ex(&g_axiom_name_registry, regkey, boxed, axiom_box_destroy)) {
        lv_free((void **) &boxed);
    }

    if (axiom_stream_ctx) {
        stream_emit_simple(axiom_stream_ctx, STREAM_EVENT_INFO, "已注册不可构造问题", 0);
    }

    return true;
}

KnownUnconstructible *axiom_package_lookup_unconstructible(AxiomPackage *pkg, const char *name) {
    if (!pkg || !name)
        return NULL;

    /* 委托注册表按名称定位索引（strcmp 由 lv_registry 承担），再经 darray 取元素指针 */
    axiom_name_registry_ensure();
    char regkey[AXIOM_REGKEY_MAX];
    axiom_build_key(pkg, 'U', name, regkey, sizeof(regkey));
    void *boxed = lv_registry_get(&g_axiom_name_registry, regkey);
    if (!boxed)
        return NULL;
    int idx = axiom_unbox_index(boxed);
    if (idx < 0 || idx >= (int) pkg->known_unconstructibles.count)
        return NULL;
    return (KnownUnconstructible *) lv_darray_get(&pkg->known_unconstructibles, idx);
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

    /* 登记名称索引（value = boxed 元素索引；重复注册保留首次映射，与线性查找"首个匹配优先"语义一致） */
    axiom_name_registry_ensure();
    char regkey[AXIOM_REGKEY_MAX];
    axiom_build_key(pkg, 'T', slot.name, regkey, sizeof(regkey));
    void *boxed = axiom_box_index((int) pkg->templates.count - 1);
    if (boxed && !lv_registry_put_ex(&g_axiom_name_registry, regkey, boxed, axiom_box_destroy)) {
        lv_free((void **) &boxed);
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

    /* 委托注册表按名称定位索引（strcmp 由 lv_registry 承担），再经 darray 取元素指针 */
    axiom_name_registry_ensure();
    char regkey[AXIOM_REGKEY_MAX];
    axiom_build_key(pkg, 'T', name, regkey, sizeof(regkey));
    void *boxed = lv_registry_get(&g_axiom_name_registry, regkey);
    if (!boxed)
        return NULL;
    int idx = axiom_unbox_index(boxed);
    if (idx < 0 || idx >= (int) pkg->templates.count)
        return NULL;
    return (ConstraintTemplate *) lv_darray_get(&pkg->templates, idx);
}
