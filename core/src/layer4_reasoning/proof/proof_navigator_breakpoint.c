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
#include "lv/lv_thread.h"  /* lv_MUTEX_* 兼容宏依赖 lv_mutex_* 实现 */
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
ProofNavigatorState s_proof_state = {0};

/** 断点互斥锁一次性初始化守卫（项目惯例：lv_once 消除 check-then-init 竞态） */
static lv_once_t g_breakpoint_mutex_once = lv_ONCE_INIT;

static void breakpoint_mutex_init_once(void) {
    lv_mutex_init(&s_proof_state.breakpoint_mutex);
}

#define BREAKPOINT_LOCK()                                                         \
    do {                                                                          \
        lv_once(&g_breakpoint_mutex_once, breakpoint_mutex_init_once);            \
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
    nav_emit(proof_stream_ctx, STREAM_EVENT_INFO, "断点保存: breakpoint_id=%d, step=%d", breakpoint_id,
             nav->current_step);

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
    nav_emit(proof_stream_ctx, STREAM_EVENT_INFO, "断点恢复: breakpoint_id=%d, step=%d", breakpoint_id,
             nav->current_step);

    return true;
}

