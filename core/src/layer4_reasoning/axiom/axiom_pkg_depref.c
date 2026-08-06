/*
 * @file axiom_pkg_depref.c
 * @brief Axiom package system - dependency refs, dep chain refs, lemma reverify
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

#include "lv/lv_hash.h"

#include "debug.h"
#include "error_codes.h"
#include "lexer_shared.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "stream.h"

/* ============== 依赖引用追踪（Section 11.5: 依赖链断裂自动降级） ============== */

/**
 * @brief 注册一个依赖引用到公理包
 *
 * 记录一个依赖引用及其内容哈希，以便后续升级时验证内容是否变化。
 * 如果内容哈希发生变化，依赖此引用的 GREEN 结论将被自动降级为 YELLOW。
 */
int axiom_package_register_dependency_ref(AxiomPackage *pkg, const char *ref_id, const char *content_hash,
                                          int dependent_node_id) {
    if (!pkg)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "axiom_package_register_dependency_ref: pkg is NULL");
    if (!ref_id)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "axiom_package_register_dependency_ref: ref_id is NULL");
    if (!content_hash)
        lv_RETURN_ERROR(lv_ERROR_NULL_POINTER, "axiom_package_register_dependency_ref: content_hash is NULL");

    DependencyRef ref;
    memset(&ref, 0, sizeof(DependencyRef));

    lv_strlcpy(ref.ref_id, ref_id, sizeof(ref.ref_id));
    lv_strlcpy(ref.content_hash, content_hash, sizeof(ref.content_hash));
    ref.dependent_node_id = dependent_node_id;
    ref.original_color = DEP_TRUST_GREEN;

    if (lv_darray_push(&pkg->dep_refs, &ref) < 0)
        return -2;

    if (axiom_stream_ctx) {
        stream_emit_simple(axiom_stream_ctx, STREAM_EVENT_INFO, "已注册依赖引用", 0);
    }

    return 0;
}

/**
 * @brief 验证所有依赖引用，返回失效的引用
 *
 * 通过重新计算包的内容哈希并与注册时存储的哈希进行比较，
 * 找出所有内容已发生变化的依赖引用。
 */
int axiom_package_validate_dependencies_with_hashes(AxiomPackage *pkg, DependencyRef **invalidated_refs,
                                                    int *invalidated_count) {
    if (!pkg || !invalidated_refs || !invalidated_count)
        return -1;

    *invalidated_refs = NULL;
    *invalidated_count = 0;

    if (pkg->dep_refs.count == 0)
        return 0;

    /* 重新计算当前包的内容哈希 */
    char *current_hash = axiom_package_compute_content_hash(pkg);
    if (!current_hash)
        return -1;

    /* 第一遍：统计失效引用数量（仅验证 REF_INTERNAL 类型的引用）
     * REF_EXTERNAL 为公认文献，永久有效，不参与自动重验
     * REF_AUTHOR 为基础黄色，无形式化支撑，不参与哈希验证 */
    int fail_count = 0;
    for (int i = 0; i < pkg->dep_refs.count; i++) {
        DependencyRef *ref = (DependencyRef *)lv_darray_get(&pkg->dep_refs, i);
        if (ref->ref_type != REF_INTERNAL)
            continue;
        if (strcmp(ref->content_hash, current_hash) != 0) {
            fail_count++;
        }
    }

    if (fail_count == 0) {
        lv_free((void **) &current_hash);
        return 0;
    }

    /* 分配输出数组 */
    DependencyRef *output = lv_calloc((size_t) fail_count, sizeof(DependencyRef));
    if (!output) {
        lv_free((void **) &current_hash);
        return -1;
    }

    /* 第二遍：填充失效引用 */
    int out_idx = 0;
    for (int i = 0; i < pkg->dep_refs.count; i++) {
        DependencyRef *ref = (DependencyRef *)lv_darray_get(&pkg->dep_refs, i);
        if (ref->ref_type != REF_INTERNAL)
            continue;
        if (strcmp(ref->content_hash, current_hash) != 0) {
            output[out_idx++] = *ref;
        }
    }

    lv_free((void **) &current_hash);
    *invalidated_refs = output;
    *invalidated_count = fail_count;
    return fail_count;
}

/**
 * @brief 执行失效依赖的自动降级
 *
 * 当公理包升级后，如果内部引用哈希发生变化（即引用内容被修改），
 * 所有依赖这些引用的 GREEN 结论将被自动降级为 YELLOW
 * "conditional unconstructible -- dependency invalidated"。
 *
 * 这是 design_v2.9.md Section 11.5 要求的依赖链断裂自动降级机制。
 */
int axiom_package_auto_degrade_invalidated(AxiomPackage *pkg, ConstraintGraph *graph) {
    if (!pkg || !graph)
        return 0;

    /* 步骤1：验证所有依赖引用，找出失效的 */
    DependencyRef *invalidated = NULL;
    int invalidated_count = 0;

    int result = axiom_package_validate_dependencies_with_hashes(pkg, &invalidated, &invalidated_count);

    if (result < 0 || invalidated_count == 0) {
        return 0;
    }

    /* 步骤2-4：对每个失效引用，降级依赖节点 */
    int degraded_count = 0;

    for (int i = 0; i < invalidated_count; i++) {
        DependencyRef *ref = &invalidated[i];

        /* 在约束图中查找依赖节点 */
        GeomNode *node = graph_get_node(graph, ref->dependent_node_id);
        if (!node) {
            lv_LOG_WARNING("[WARNING] axiom_package_auto_degrade_invalidated: "
                           "依赖节点 %d 未在约束图中找到 (ref_id='%s')\n",
                           ref->dependent_node_id, ref->ref_id);
            continue;
        }

        /* 仅降级 GREEN 节点 */
        if (node->trust == TRUST_GREEN) {
            node->trust = TRUST_YELLOW;
            degraded_count++;

            lv_LOG_WARNING("[WARNING] axiom_package_auto_degrade_invalidated: "
                           "节点 %d 已从 GREEN 降级为 YELLOW "
                           "(conditional unconstructible -- dependency invalidated, "
                           "ref_id='%s')\n",
                           ref->dependent_node_id, ref->ref_id);
        }
    }

    /* 释放验证结果数组 */
    lv_free((void **) &invalidated);

    /* 步骤3：处理作者断言引用 —— 确保依赖节点保持 YELLOW */
    for (int i = 0; i < pkg->dep_refs.count; i++) {
        DependencyRef *ref = (DependencyRef *)lv_darray_get(&pkg->dep_refs, i);
        if (ref->ref_type != REF_AUTHOR)
            continue;

        GeomNode *node = graph_get_node(graph, ref->dependent_node_id);
        if (!node) {
            lv_LOG_WARNING("[WARNING] axiom_package_auto_degrade_invalidated: "
                           "作者断言依赖节点 %d 未在约束图中找到 (ref_id='%s')\n",
                           ref->dependent_node_id, ref->ref_id);
            continue;
        }

        /* 作者断言基础即为 YELLOW，确保未被错误提升为 GREEN */
        if (node->trust == TRUST_GREEN) {
            node->trust = TRUST_YELLOW;
            degraded_count++;
            lv_LOG_WARNING("[WARNING] axiom_package_auto_degrade_invalidated: "
                           "节点 %d 从 GREEN 降级为 YELLOW "
                           "(author assertion -- no formal proof, ref_id='%s')\n",
                           ref->dependent_node_id, ref->ref_id);
        }
    }

    return degraded_count;
}

/* ============== 不可构造性证明依赖链引用 ============== */

/**
 * @brief 计算引理块的内容哈希
 *
 * 基于当前公理包的已知不可构造问题和模板数据计算哈希。
 * 用于内引用的内容验证。
 */
static char *compute_lemma_block_hash(AxiomPackage *pkg, int lemma_block_id) {
    if (!pkg)
        return NULL;

    lvHashCtx ctx;
    lv_hash_init(&ctx, LV_HASH_SHA256);

    /* 哈希引理块 ID 作为标识 */
    lv_hash_update(&ctx, &lemma_block_id, sizeof(lemma_block_id));

    /* 哈希所有已知不可构造问题中的依赖链（这些构成引理块的约束逻辑） */
    for (int i = 0; i < pkg->known_unconstructibles.count; i++) {
        KnownUnconstructible *uc = (KnownUnconstructible *)lv_darray_get(&pkg->known_unconstructibles, i);
        lv_hash_str(&ctx, uc->name);
        lv_hash_str(&ctx, uc->reduces_to);
        lv_hash_bool(&ctx, uc->green_verified);

        /* 哈希依赖链 */
        for (int j = 0; j < uc->dependency_chain.count; j++) {
            lv_hash_str(&ctx, *(char **)lv_darray_get(&uc->dependency_chain, j));
        }

        lv_hash_str(&ctx, uc->external_ref);
    }

    /* 哈希所有模板名称和参数（构成构造性基础） */
    for (int i = 0; i < pkg->templates.count; i++) {
        ConstraintTemplate *t = (ConstraintTemplate *)lv_darray_get(&pkg->templates, i);
        lv_hash_str(&ctx, t->name);
        lv_hash_int32(&ctx, t->param_count);
        lv_hash_bool(&ctx, t->verified);
    }

    /* 哈希几何信息和矛盾行为 */
    lv_hash_str(&ctx, pkg->bottom_geometry);
    lv_hash_str(&ctx, pkg->negation_encoding);
    lv_hash_int32(&ctx, pkg->contradiction_behavior);

    /* 计算最终哈希并转换为十六进制字符串 */
    return lv_hash_to_hex_alloc(&ctx);
}

int axiom_package_add_internal_ref(AxiomPackage *pkg, int lemma_block_id, int dependent_node_id) {
    if (!pkg)
        return -1;
    if (lemma_block_id < 0 || dependent_node_id < 0)
        return -1;

    /* 计算引理块的内容哈希 */
    char *hash = compute_lemma_block_hash(pkg, lemma_block_id);
    if (!hash)
        return -2;

    /* 生成引用标识符 */
    char ref_id[64];
    snprintf(ref_id, sizeof(ref_id), "internal:lemma:%d", lemma_block_id);

    DependencyRef ref;
    memset(&ref, 0, sizeof(DependencyRef));

    lv_strlcpy(ref.ref_id, ref_id, sizeof(ref.ref_id));
    lv_strlcpy(ref.content_hash, hash, sizeof(ref.content_hash));
    lv_free((void **) &hash);

    ref.dependent_node_id = dependent_node_id;
    ref.original_color = DEP_TRUST_GREEN;
    ref.ref_type = REF_INTERNAL;
    ref.hash_valid = true;

    if (lv_darray_push(&pkg->dep_refs, &ref) < 0)
        return -2;

    if (axiom_stream_ctx) {
        stream_emit_simple(axiom_stream_ctx, STREAM_EVENT_INFO, "已注册内引用（内容哈希验证）", 0);
    }

    return 0;
}

int axiom_package_add_external_ref(AxiomPackage *pkg, const char *ref_string, int dependent_node_id,
                                   const char *trust_comment) {
    if (!pkg || !ref_string)
        return -1;
    if (dependent_node_id < 0)
        return -1;

    /* 生成引用标识符 */
    char ref_id[64];
    snprintf(ref_id, sizeof(ref_id), "external:%.48s", ref_string);

    DependencyRef ref;
    memset(&ref, 0, sizeof(DependencyRef));

    lv_strlcpy(ref.ref_id, ref_id, sizeof(ref.ref_id));
    ref.content_hash[0] = '\0';
    ref.dependent_node_id = dependent_node_id;
    ref.original_color = DEP_TRUST_GREEN;
    ref.ref_type = REF_EXTERNAL;
    ref.hash_valid = false;
    lv_strlcpy(ref.external_ref, ref_string, sizeof(ref.external_ref));
    if (trust_comment && trust_comment[0] != '\0') {
        lv_strlcpy(ref.trust_comment, trust_comment, sizeof(ref.trust_comment));
    }

    if (lv_darray_push(&pkg->dep_refs, &ref) < 0)
        return -2;

    if (axiom_stream_ctx) {
        stream_emit_simple(axiom_stream_ctx, STREAM_EVENT_INFO, "已注册外引用（公认文献，永久有效）", 0);
    }

    return 0;
}

int axiom_package_add_author_assertion(AxiomPackage *pkg, int dependent_node_id) {
    if (!pkg)
        return -1;
    if (dependent_node_id < 0)
        return -1;

    /* 生成引用标识符 */
    char ref_id[64];
    snprintf(ref_id, sizeof(ref_id), "author:node:%d", dependent_node_id);

    DependencyRef ref;
    memset(&ref, 0, sizeof(DependencyRef));

    lv_strlcpy(ref.ref_id, ref_id, sizeof(ref.ref_id));
    ref.content_hash[0] = '\0';
    ref.dependent_node_id = dependent_node_id;
    ref.original_color = DEP_TRUST_YELLOW;
    ref.ref_type = REF_AUTHOR;
    ref.hash_valid = false;

    if (lv_darray_push(&pkg->dep_refs, &ref) < 0)
        return -2;

    if (axiom_stream_ctx) {
        stream_emit_simple(axiom_stream_ctx, STREAM_EVENT_INFO, "已注册作者断言（无形式化支撑，黄色基础）", 0);
    }

    return 0;
}

/* ============== 引理自动重验循环（Section 11） ============== */

/**
 * @brief 重新验证引理块
 *
 * 尝试重新验证引理块的内容。
 * 通过重新计算公理包的当前内容哈希并与存储的哈希对比来判断内容是否发生变化。
 *
 * @param pkg 公理包
 * @param ref 要重验的依赖引用
 * @return true 重验通过（哈希匹配）
 */
static bool lemma_reverify(AxiomPackage *pkg, DependencyRef *ref) {
    if (!pkg || !ref)
        return false;

    /* 计算当前内容哈希 */
    char *current_hash = axiom_package_compute_content_hash(pkg);
    if (!current_hash)
        return false;

    /* 对比哈希 */
    bool match = (strcmp(ref->content_hash, current_hash) == 0);
    lv_free((void **) &current_hash);
    return match;
}

int axiom_package_reverify_lemmas(AxiomPackage *pkg, int *out_stale, char ***out_stale_names) {
    if (!pkg)
        return 0;

    int total = 0;
    int stale_count = 0;

    /* 第一遍：统计需要处理的 REF_INTERNAL 引用总数和失效数 */
    for (int i = 0; i < pkg->dep_refs.count; i++) {
        DependencyRef *ref = (DependencyRef *)lv_darray_get(&pkg->dep_refs, i);
        if (ref->ref_type != REF_INTERNAL)
            continue;
        total++;
        if (!lemma_reverify(pkg, ref)) {
            stale_count++;
        }
    }

    /* 分配输出数组 */
    char **stale_names = NULL;
    if (stale_count > 0) {
        stale_names = (char **) lv_calloc((size_t) stale_count, sizeof(char *));
        if (!stale_names) {
            if (out_stale)
                *out_stale = 0;
            if (out_stale_names)
                *out_stale_names = NULL;
            return total;
        }
    }

    /* 第二遍：标记失效引用并记录名称 */
    int idx = 0;
    for (int i = 0; i < pkg->dep_refs.count; i++) {
        DependencyRef *ref = (DependencyRef *)lv_darray_get(&pkg->dep_refs, i);
        if (ref->ref_type != REF_INTERNAL)
            continue;
        if (!lemma_reverify(pkg, ref)) {
            axiom_package_mark_lemma_stale(pkg, ref->ref_id);
            if (stale_names && idx < stale_count) {
                stale_names[idx] = lv_strdup_safe(ref->ref_id);
                idx++;
            }
        }
    }

    if (out_stale)
        *out_stale = stale_count;
    if (out_stale_names) {
        *out_stale_names = stale_names;
    } else if (stale_names) {
        /* 调用者不需要名称数组，释放分配的内存 */
        for (int i = 0; i < stale_count; i++) {
            lv_free((void **) &stale_names[i]);
        }
        lv_free((void **) &stale_names);
    }

    if (axiom_stream_ctx) {
        stream_emit_simple(axiom_stream_ctx, STREAM_EVENT_INFO, "引理自动重验完成", 0);
    }

    return total;
}

int axiom_package_mark_lemma_stale(AxiomPackage *pkg, const char *ref_id) {
    if (!pkg || !ref_id)
        return -1;

    for (int i = 0; i < pkg->dep_refs.count; i++) {
        DependencyRef *ref = (DependencyRef *)lv_darray_get(&pkg->dep_refs, i);
        if (strcmp(ref->ref_id, ref_id) != 0)
            continue;

        /* 设置信任注释，标识为遗留状态 */
        lv_strlcpy(ref->trust_comment, "遗留 - 在旧版本下得证，未验证兼容性", sizeof(ref->trust_comment));

        /* 将信任颜色设为黄色，表示需人工介入 */
        ref->original_color = DEP_TRUST_YELLOW;

        if (axiom_stream_ctx) {
            stream_emit_simple(axiom_stream_ctx, STREAM_EVENT_WARNING, "引理标记为遗留状态", 0);
        }

        return 0;
    }

    return -1;
}
