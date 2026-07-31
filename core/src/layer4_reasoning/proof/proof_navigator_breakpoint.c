/*
 * @file proof_navigator_breakpoint.c
 * @brief Proof navigator module - breakpoint save/restore and storage
 * @details Split from proof_navigator.c
 */

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_platform.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_xmacro.h"
#include "lv/axiom_pkg.h"
#include "lv/constraint_graph.h"
#include "lv/engine.h"
#include "lv/proof.h"
#include "lv/proof_trace.h"
#include "lv/smt_backend.h"
#include "lv/trust_color.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "stream.h"
#include "stream_context_util.h"
#include "proof_navigator_internal.h"

/* ============== 证明断点保存/恢复 ============== */

/** 模块级唯一状态实例（替代原有的 6 个分散 static 变量） */

/** 模块级唯一状态实例（替代原有的 6 个分散 static 变量） */
ProofNavigatorState s_proof_state = {0};

/** 互斥锁惰性初始化标记 */
static volatile int g_breakpoint_mutex_inited = 0;

#define BREAKPOINT_LOCK()                                                         \
    do {                                                                          \
        if (!g_breakpoint_mutex_inited) {                                         \
            lv_MUTEX_INIT(&s_proof_state.breakpoint_mutex);                       \
            lv_ATOMIC_EXCHANGE(&g_breakpoint_mutex_inited, 1);                    \
        }                                                                         \
        lv_MUTEX_LOCK(&s_proof_state.breakpoint_mutex);                           \
    } while (0)
#define BREAKPOINT_UNLOCK() lv_MUTEX_UNLOCK(&s_proof_state.breakpoint_mutex)

bool proof_save_breakpoint(ProofNavigator *nav, int breakpoint_id) {
    if (!nav)
        return false;

    /* 检查断点ID是否有效 */
    if (breakpoint_id < 0)
        return false;

    BREAKPOINT_LOCK();

    /* 查找是否已有相同ID的快照，如果有则覆盖 */
    int slot = -1;
    for (int i = 0; i < s_proof_state.breakpoint_store_count; i++) {
        if (s_proof_state.breakpoint_store[i].breakpoint_id == breakpoint_id) {
            slot = i;
            break;
        }
    }

    /* 如果没有找到，分配新槽位 */
    if (slot < 0) {
        int current_count = (int) s_proof_state.breakpoint_store_count;
        if (current_count >= MAX_BREAKPOINT_SNAPSHOTS) {
            BREAKPOINT_UNLOCK();
            return false; /* 存储已满 */
        }
        slot = current_count;
        lv_ATOMIC_INC(&s_proof_state.breakpoint_store_count);
    }
    s_proof_state.breakpoint_store[slot].breakpoint_id = breakpoint_id;
    s_proof_state.breakpoint_store[slot].current_step = nav->current_step;
    s_proof_state.breakpoint_store[slot].step_count = nav->step_count;
    s_proof_state.breakpoint_store[slot].is_complete = nav->is_complete;
    s_proof_state.breakpoint_store[slot].final_color = nav->final_color;

    BREAKPOINT_UNLOCK();

    /* 将当前步骤标记为断点 */
    if (nav->current_step >= 0 && nav->current_step < nav->step_count) {
        ProofStep *step = nav->steps[nav->current_step];
        if (step) {
            proof_step_set_breakpoint(step, true);
        }
    }

    /* 流式事件：断点保存 */
    if (proof_stream_ctx != NULL) {
        lvStrBuf sb_11 = {0};
        lv_strbuf_printf(&sb_11, "断点保存: breakpoint_id=%d, step=%d", breakpoint_id, nav->current_step);
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_INFO, sb_11.data, 0);
        lv_strbuf_destroy(&sb_11);
    }

    return true;
}

bool proof_restore_breakpoint(ProofNavigator *nav, int breakpoint_id) {
    if (!nav)
        return false;

    /* 查找断点快照 */
    int slot = -1;
    for (int i = 0; i < s_proof_state.breakpoint_store_count; i++) {
        if (s_proof_state.breakpoint_store[i].breakpoint_id == breakpoint_id) {
            slot = i;
            break;
        }
    }

    if (slot < 0)
        return false; /* 未找到断点 */

    /* 验证快照中的 step_count 不超过当前步骤数量 */
    if (s_proof_state.breakpoint_store[slot].step_count > nav->step_count) {
        return false; /* 快照无效：保存时的步骤数多于当前 */
    }

    /* 恢复导航器状态 */
    nav->current_step = s_proof_state.breakpoint_store[slot].current_step;
    nav->is_complete = s_proof_state.breakpoint_store[slot].is_complete;
    nav->final_color = s_proof_state.breakpoint_store[slot].final_color;

    /* 确保 current_step 在有效范围内 */
    if (nav->current_step < -1) {
        nav->current_step = -1;
    }
    if (nav->current_step >= nav->step_count) {
        nav->current_step = nav->step_count - 1;
    }

    /* 流式事件：断点恢复 */
    if (proof_stream_ctx != NULL) {
        lvStrBuf sb_12 = {0};
        lv_strbuf_printf(&sb_12, "断点恢复: breakpoint_id=%d, step=%d", breakpoint_id, nav->current_step);
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_INFO, sb_12.data, 0);
        lv_strbuf_destroy(&sb_12);
    }

    return true;
}

/* ============== 断点存储管理实现（v3.4.1 新增） ============== */

/**
 * @brief 线程安全的断点存储初始化
 *
 * 使用静态局部变量确保初始化过程只执行一次（C++11 保证线程安全）。
 * 即使多个线程同时调用，也只有第一个会执行初始化。
 */
void proof_breakpoint_storage_init(void) {
    BREAKPOINT_LOCK();
    if (s_proof_state.breakpoint_store_count == 0 && s_proof_state.breakpoint_store[0].breakpoint_id == 0) {
        /* 重置计数器 */
        lv_ATOMIC_EXCHANGE(&s_proof_state.breakpoint_store_count, 0);
        /* 清空存储 */
        memset(s_proof_state.breakpoint_store, 0, sizeof(s_proof_state.breakpoint_store));
    }
    BREAKPOINT_UNLOCK();
}

void lv_proof_breakpoint_storage_cleanup(void) {
    proof_breakpoint_storage_reset();
}

/**
 * @brief 重置断点存储
 *
 * 清除所有已保存的断点快照，重置计数器。
 */
void proof_breakpoint_storage_reset(void) {
    BREAKPOINT_LOCK();
    /* 清空所有快照 */
    memset(s_proof_state.breakpoint_store, 0, sizeof(s_proof_state.breakpoint_store));
    lv_ATOMIC_EXCHANGE(&s_proof_state.breakpoint_store_count, 0);
    BREAKPOINT_UNLOCK();

    /* 流式事件 */
    if (proof_stream_ctx != NULL) {
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_INFO, "断点存储已重置", 0);
    }
}

/**
 * @brief 获取当前断点数量
 */
int proof_breakpoint_storage_count(void) {
    return s_proof_state.breakpoint_store_count;
}

/**
 * @brief 删除指定的断点快照
 */
bool proof_breakpoint_delete(int breakpoint_id) {
    if (breakpoint_id < 0) {
        return false;
    }

    BREAKPOINT_LOCK();

    /* 查找断点 */
    int slot = -1;
    for (int i = 0; i < s_proof_state.breakpoint_store_count; i++) {
        if (s_proof_state.breakpoint_store[i].breakpoint_id == breakpoint_id) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        BREAKPOINT_UNLOCK();
        /* 未找到断点 */
        return false;
    }

    /* 将最后一个元素移动到当前位置，然后减少计数 */
    if (slot < s_proof_state.breakpoint_store_count - 1) {
        s_proof_state.breakpoint_store[slot] = s_proof_state.breakpoint_store[s_proof_state.breakpoint_store_count - 1];
    }
    lv_ATOMIC_DEC(&s_proof_state.breakpoint_store_count);

    BREAKPOINT_UNLOCK();

    /* 流式事件 */
    if (proof_stream_ctx != NULL) {
        lvStrBuf sb_13 = {0};
        lv_strbuf_printf(&sb_13, "断点已删除: breakpoint_id=%d", breakpoint_id);
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_INFO, sb_13.data, 0);
        lv_strbuf_destroy(&sb_13);
    }

    return true;
}
