/**
 * @file graph_hash.c
 * @brief 图哈希模块实现 —— FNV-1a 基约束图指纹计算
 *
 * 提供约束图的完整哈希计算、比较和销毁操作，用于重写历史
 * 循环检测和规范化幂等性验证。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 *
 * @dependencies
 *   - graph_hash.h          : 图哈希公共接口定义
 *   - lv_internal.h         : 内部工具宏与 FNV 常量
 *   - symbolic_coord.h      : 符号坐标序列化
 */

#include "lv/graph_hash.h"
#include "lv/lv_utils.h"
#include "lv/lv_internal.h"     /* FNV 常量：lv_FNV64_OFFSET_BASIS, lv_FNV64_PRIME */
#include "lv/symbolic_coord.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ==================== 局部常量 ==================== */

/** 节点描述缓冲区大小 —— 用于 snprintf 构造 "id:type" 字符串 */
#define GRAPH_HASH_DESC_BUF 256

/** 约束描述缓冲区大小 —— 用于 snprintf 构造 "C:id:type" 字符串 */
#define GRAPH_HASH_CONSTRAINT_DESC 64

/* ==================== 局部工具函数 ==================== */

/**
 * @brief FNV-1a 哈希函数
 *
 * 对输入字符串计算 64 位 FNV-1a 哈希值。
 * 输入为 NULL 或空字符串时返回偏移基值。
 *
 * @param s 输入字符串（允许 NULL）
 * @return uint64_t 64 位哈希值
 */
static uint64_t fnv1a_hash(const char *s) {
    uint64_t hash = lv_FNV64_OFFSET_BASIS;
    while (s && *s) {
        hash ^= (uint8_t)(*s);
        hash *= lv_FNV64_PRIME;
        s++;
    }
    return hash;
}

/* ==================== 公共 API 实现 ==================== */

GraphHash *compute_complete_graph_hash(const ConstraintGraph *graph) {
    if (!graph) return NULL;

    GraphHash *gh = (GraphHash *)lv_calloc(1, sizeof(GraphHash));
    if (!gh) return NULL;

    gh->node_count = graph->node_count;

    /* 分配节点哈希数组 */
    if (graph->node_count > 0) {
        gh->node_hashes = (uint64_t *)lv_calloc((size_t)graph->node_count, sizeof(uint64_t));
        if (!gh->node_hashes) {
            lv_free((void **)&gh);
            return NULL;
        }
    } else {
        gh->node_hashes = NULL;
    }

    /* 逐节点计算哈希：id:type[:coord_serialize] */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];

        /* 构造基础描述 "id:type" */
        char *desc = (char *)lv_malloc(GRAPH_HASH_DESC_BUF);
        if (!desc) {
            desc = (char *)lv_malloc(1);
            if (desc) desc[0] = '\0';
        } else {
            snprintf(desc, GRAPH_HASH_DESC_BUF, "%d:%d",
                     node->id, (int)node->type);
        }

        /* 对于 POINT 类型，拼接符号坐标序列化 */
        if (node->type == GEOM_POINT && node->coord_count > 0 && node->symbolic_coords) {
            char *coord_str = symbolic_coord_serialize(node->symbolic_coords[0]);
            if (coord_str) {
                size_t desc_len = strlen(desc);
                size_t coord_len = strlen(coord_str);

                /* 检查溢出 */
                if (desc_len > SIZE_MAX - coord_len - 2) {
                    lv_free((void **)&coord_str);
                } else {
                    char *new_desc = (char *)lv_malloc(desc_len + coord_len + 2);
                    if (new_desc) {
                        snprintf(new_desc, desc_len + coord_len + 2, "%s:%s",
                                 desc, coord_str);
                        lv_free((void **)&desc);
                        desc = new_desc;
                    }
                    lv_free((void **)&coord_str);
                }
            }
        }

        gh->node_hashes[i] = fnv1a_hash(desc);
        lv_free((void **)&desc);
    }

    /* 合并节点哈希为整体哈希（XOR-and-MULTIPLY 组合） */
    gh->hash = lv_FNV64_OFFSET_BASIS;
    for (int i = 0; i < graph->node_count; i++) {
        gh->hash ^= gh->node_hashes[i];
        gh->hash *= lv_FNV64_PRIME;
    }

    /* 将约束信息混入整体哈希 */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        char desc[GRAPH_HASH_CONSTRAINT_DESC];
        snprintf(desc, GRAPH_HASH_CONSTRAINT_DESC, "C:%d:%d",
                 c->id, (int)c->type);
        uint64_t chash = fnv1a_hash(desc);
        gh->hash ^= chash;
        gh->hash *= lv_FNV64_PRIME;
    }

    return gh;
}

bool graph_hash_equal(const GraphHash *a, const GraphHash *b) {
    if (!a || !b) return false;
    if (a->hash != b->hash) return false;
    if (a->node_count != b->node_count) return false;

    for (int i = 0; i < a->node_count; i++) {
        if (a->node_hashes[i] != b->node_hashes[i]) return false;
    }
    return true;
}

void graph_hash_destroy(GraphHash *hash) {
    if (hash) {
        lv_free((void **)&hash->node_hashes);
        lv_free((void **)&hash);
    }
}

/**
 * @brief 计算快速图哈希（轻量级，用于循环检测）
 *
 * 仅产生单一 uint64_t 值，不分配堆内存。遍历所有节点和约束，
 * 将 id、type 和坐标信息（若有）逐项混入哈希。适用于快速
 * 比较和循环检测场景，无需 per-node 详细哈希数组。
 *
 * @param graph 约束图指针
 * @return uint64_t 图哈希值，输入为 NULL 时返回 0
 */
uint64_t compute_quick_graph_hash(const ConstraintGraph *graph) {
    if (!graph) return 0;

    uint64_t hash = lv_FNV64_OFFSET_BASIS;

    /* 混入节点数量 */
    hash ^= (uint64_t)graph->node_count;
    hash *= lv_FNV64_PRIME;

    /* 逐节点混入 id 和 type */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node) continue;

        hash ^= (uint64_t)node->id;
        hash *= lv_FNV64_PRIME;
        hash ^= (uint64_t)node->type;
        hash *= lv_FNV64_PRIME;

        /* 对于 POINT 节点，混入坐标哈希 */
        if (node->type == GEOM_POINT && node->coord_count > 0 && node->symbolic_coords) {
            char *coord_str = symbolic_coord_serialize(node->symbolic_coords[0]);
            if (coord_str) {
                uint64_t ch = fnv1a_hash(coord_str);
                hash ^= ch;
                hash *= lv_FNV64_PRIME;
                lv_free((void **)&coord_str);
            }
        }
    }

    /* 混入约束数量 */
    hash ^= (uint64_t)graph->constraint_count;
    hash *= lv_FNV64_PRIME;

    /* 逐约束混入 id 和 type */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (!c) continue;

        hash ^= (uint64_t)c->id;
        hash *= lv_FNV64_PRIME;
        hash ^= (uint64_t)c->type;
        hash *= lv_FNV64_PRIME;
    }

    return hash;
}
