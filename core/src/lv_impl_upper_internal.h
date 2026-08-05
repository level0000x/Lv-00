/**
 * @file lv_impl_upper_internal.h
 * @brief Internal shared definitions for upper unified implementation.
 */

#ifndef lv_IMPL_UPPER_INTERNAL_H
#define lv_IMPL_UPPER_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "lv/atp_backend.h"
#include "lv/engine.h"
#include "lv/geom_evol.h"
#include "lv/meta_verify.h"
#include "lv/visual_editor.h"

#include "constraint_graph.h"

/* ---- 通用对象表抽象:{id, ptr} 槽位 + 线性查找/插入/删除 ----
 * 消除上层 create/find/destroy 中重复的对象表样板代码。
 * 约定: 空槽 ptr==NULL; 删除仅置空不压缩(槽位顺序与 ID 分配顺序均不变);
 * 因已占用槽位可能位于 count 之外, 查找/删除必须按容量扫描。 */
typedef struct {
    int64_t id;  /**< 对象唯一 ID(upper_id 语义) */
    void *ptr;   /**< 对象指针; NULL 表示空槽 */
} lvObjSlot;

/** 在表中线性查找 id 对应的槽位, 返回槽指针, 未找到返回 NULL。
 *  count 为扫描上界(通常传表容量 MAX_*)。 */
static inline lvObjSlot *lv_obj_table_find(lvObjSlot *table, int count, int64_t id) {
    for (int i = 0; i < count; i++) {
        if (table[i].ptr && table[i].id == id)
            return &table[i];
    }
    return NULL;
}

/** 在表中找第一个空槽写入 {id, ptr} 并递增 *count, 返回槽位下标。
 *  表满返回 -1(调用方应先以 count>=capacity 预检, 保证必然有空槽)。 */
static inline int lv_obj_table_add(lvObjSlot *table, int *count, int capacity, int64_t id, void *ptr) {
    for (int i = 0; i < capacity; i++) {
        if (!table[i].ptr) {
            table[i].id = id;
            table[i].ptr = ptr;
            (*count)++;
            return i;
        }
    }
    return -1;
}

/** 移除表中 id 对应的槽位(置空并递减 *count)。返回 1 表示找到并移除, 0 表示未找到。 */
static inline int lv_obj_table_remove(lvObjSlot *table, int *count, int capacity, int64_t id) {
    for (int i = 0; i < capacity; i++) {
        if (table[i].ptr && table[i].id == id) {
            table[i].ptr = NULL;
            table[i].id = 0;
            (*count)--;
            return 1;
        }
    }
    return 0;
}

/* ---- internal tables (defined in lv_impl_upper.c) ---- */
#define MAX_EVOL_TABLE 256
#define MAX_ATP_BACKEND_TABLE 256

typedef struct {
    int64_t task_id;           /* task unique id */
    int64_t backend_id;        /* index into atp_backend_table */
    ATPResultInfo result_info;
    int8_t completed;          /* 0=pending, 1=done */
} ATPTask;

#define MAX_ATP_TASK_TABLE 512
#define MAX_VISUAL_EDITOR_TABLE 64
#define MAX_VIEW_SYNC_TABLE 64
#define MAX_TEXT_CODE_TABLE 64

typedef struct {
    int64_t upper_id;
    lvObjSlot evol_table[MAX_EVOL_TABLE];
    int evol_count;
    lvObjSlot atp_backend_table[MAX_ATP_BACKEND_TABLE];
    int atp_backend_count;
    ATPTask atp_task_table[MAX_ATP_TASK_TABLE];
    int atp_task_count;
    lvObjSlot visual_editor_table[MAX_VISUAL_EDITOR_TABLE];
    int visual_editor_count;
    lvObjSlot view_sync_table[MAX_VIEW_SYNC_TABLE];
    int view_sync_count;
    lvObjSlot text_code_table[MAX_TEXT_CODE_TABLE];
    int text_code_count;
    lvMetaVerifier *meta_verifier;
} UpperState;

extern UpperState s_upper_state;

/* ---- cross-section internal APIs ---- */
typedef struct lvOrchestrator lvOrchestrator;
lvOrchestrator *lv_orchestrator_create(lvEngine *ctx);

int meta_verify_completeness(const ConstraintGraph *graph);
int meta_verify_soundness(const ConstraintGraph *graph);
int meta_verify_differential(const ConstraintGraph *graph_a, const ConstraintGraph *graph_b);

#ifdef __cplusplus
}
#endif

#endif /* lv_IMPL_UPPER_INTERNAL_H */
