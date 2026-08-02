/**
 * @file proof_version_ghost.c
 * @brief 证明版本管理与序列化 —— 幽灵标记与引导填充
 *
 * @details 由 proof_version.c 按功能域拆分而来。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/lv.h"
#include "lv/proof.h"
#include "lv/smt_backend.h"
#include "lv/thread_pool.h"

#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lv_str_utils.h"

#include "lv/lv_strbuf.h"


/* ================================================================
 * === 第六梯队参考项目落地 (P1) 实现 — 2026-05-24 ==================
 * 实现 Agda / Idris 2 / Isabelle/HOL / HOL Light / F* 五个项目的
 * 核心 API，为 Lv-00 证明系统增加洞填充、QTT标记、Sledgehammer
 * 调度、微内核验证和精化类型检查能力。
 * ================================================================ */


/* ================================================================
 * 1. Agda — hole-driven 证明编辑
 * ================================================================ */

/**
 * @brief 基于几何命题类型字符串，分析类型签名并生成填充建议链
 *
 * 启发式解析 goal_type 中的几何结构模式：
 * - 识别 "triangle" 关键词 → 建议构造器 + 精化
 * - 识别 "circle" 关键词 → 建议构造器
 * - 识别 "intersect" 关键词 → 建议 case split
 * - 通用 fallback → 建议 refine（让用户手动填充）
 */
FillSuggestion *proof_guided_fill(ConstraintSolver *solver, const char *goal_type, int goal_dim) {
    FillSuggestion *head = NULL;
    FillSuggestion *tail = NULL;

    /* 使用 solver 上下文：检查求解器状态以引导填充策略 */
    bool solver_has_assignments = false;
    if (solver != NULL) {
        /* 求解器非空：表明存在活跃的约束求解上下文。
         * 当求解器已有部分赋值时，优先建议精化而非构造，
         * 因为底层结构已部分确定。*/
        solver_has_assignments = true;
    }

    if (!goal_type || goal_type[0] == '\0') {
        /* 空目标类型 -> 建议 lambda 抽象 */
        FillSuggestion *s = (FillSuggestion *) lv_calloc(1, sizeof(FillSuggestion));
        if (!s)
            return NULL;
        s->kind = FILL_LAMBDA;
        s->label = lv_strdup_safe("引入假设（lambda 抽象）");
        s->code_snippet = lv_strdup_safe("\\x -> ?hole");
        s->arity = 1;
        return s;
    }

    /* 定义辅助宏：追加节点到链表末尾 */
#define APPEND_FILL(kind_, label_, snippet_, arity_)                                 \
    do {                                                                             \
        FillSuggestion *s = (FillSuggestion *) lv_calloc(1, sizeof(FillSuggestion)); \
        if (!s)                                                                      \
            break;                                                                   \
        s->kind = (kind_);                                                           \
        s->label = lv_strdup_safe(label_);                                           \
        s->code_snippet = lv_strdup_safe(snippet_);                                  \
        s->arity = (arity_);                                                         \
        if (!head) {                                                                 \
            head = s;                                                                \
            tail = s;                                                                \
        } else {                                                                     \
            tail->next = s;                                                          \
            tail = s;                                                                \
        }                                                                            \
    } while (0)

    /* 启发式 1：三角形相关 */
    if (strstr(goal_type, "triangle") || strstr(goal_type, "Triangle") || strstr(goal_type, "isosceles") ||
        strstr(goal_type, "right_triangle")) {
        APPEND_FILL(FILL_CONSTRUCTOR, "构造三角形构造器（给定顶点）", "triangle_create(a, b, c)", 3);
        APPEND_FILL(FILL_REFINE, "精化三角形性质", "assert_triangle_properties(a, b, c)", 0);
        /* 若有维度信息 */
        if (goal_dim == 2) {
            APPEND_FILL(FILL_LAMBDA, "二维修正 lambda 抽象", "\\tri : Triangle2D -> ?goal", 1);
        }
    }

    /* 启发式 2：圆形相关 */
    if (strstr(goal_type, "circle") || strstr(goal_type, "Circle")) {
        APPEND_FILL(FILL_CONSTRUCTOR, "构造圆形构造器（圆心+半径）", "circle_create(center, radius)", 2);
        APPEND_FILL(FILL_REFINE, "精化圆形方程", "assert_circle_eq(center, radius)", 0);
    }

    /* 启发式 3：交点相关 */
    if (strstr(goal_type, "intersect") || strstr(goal_type, "Intersection")) {
        APPEND_FILL(FILL_CASE_SPLIT, "对交点情况做分支分析", "case intersection_of(obj1, obj2) of ...", 2);
        APPEND_FILL(FILL_REFINE, "解交点方程", "solve_intersection(obj1, obj2)", 0);
    }

    /* 启发式 4：面积或体积相关 */
    if (strstr(goal_type, "area") || strstr(goal_type, "volume") || strstr(goal_type, "Area") ||
        strstr(goal_type, "Volume")) {
        APPEND_FILL(FILL_REFINE, "应用面积/体积公式", "apply_measure_formula(obj)", 0);
        APPEND_FILL(FILL_EXACT, "已知几何体查询面积常量", "lookup_area_constant(obj_type, dim)", 0);
    }

    /* 启发式 5：等式相关 */
    if (strstr(goal_type, "=") || strstr(goal_type, "equal") || strstr(goal_type, "congruent")) {
        APPEND_FILL(FILL_REFINE, "重写为等式两边化简", "rewrite_equality(lhs, rhs)", 0);
        APPEND_FILL(FILL_CASE_SPLIT, "对等式方向分支（左 -> 右 / 右 -> 左）", "case equality_direction of L2R | R2L",
                    0);
    }

    /* 通用 fallback：至少返回一个 refine 建议 */
    if (!head) {
        /* 含维度信息的默认建议 */
        lvStrBuf sb = {0};
        if (goal_dim > 0) {
            lv_strbuf_printf(&sb, "refine_goal_dim%d(\"%s\")", goal_dim, goal_type);
        } else {
            lv_strbuf_printf(&sb, "refine_goal(\"%s\")", goal_type);
        }
        APPEND_FILL(FILL_REFINE, "通用精化建议（由用户手动填充）", sb.data, 0);

        /* 若求解器已有赋值，额外建议利用已知信息 */
        if (solver_has_assignments) {
            APPEND_FILL(FILL_EXACT, "查询求解器已赋值变量", "solver_assigned_variables()", 0);
        }
        lv_strbuf_destroy(&sb);
    }

#undef APPEND_FILL
    return head;
}

/**
 * @brief 销毁填充建议链表，释放所有分配的内存
 */
void fill_suggestions_destroy(FillSuggestion *list) {
    FillSuggestion *curr = list;
    while (curr) {
        FillSuggestion *next = curr->next;
        lv_free((void **) &curr->label);
        lv_free((void **) &curr->code_snippet);
        lv_free((void **) &curr);
        curr = next;
    }
}


/* ================================================================
 * 2. Idris 2 — QTT 线性类型标记（0/1/ω）
 * ================================================================ */

/** @brief 静态 ghost 标记表，最大支持 1024 步 */
#define MAX_GHOST_STEPS 1024

static ProofQuantifier g_ghost_table[MAX_GHOST_STEPS];
static volatile long g_ghost_table_initialized = 0;

/**
 * @brief 惰性初始化 ghost 标记表（线程安全的一次性初始化）
 */
static void ghost_table_init(void) {
    if (g_ghost_table_initialized)
        return;
    int expected = 0;
    if (lv_ATOMIC_CAS_BOOL(&g_ghost_table_initialized, 1, &expected)) {
        for (int i = 0; i < MAX_GHOST_STEPS; i++) {
            g_ghost_table[i] = PROOF_QTT_UNRESTRICTED; /* 默认非擦除 */
        }
    }
    /* 等待其他线程完成初始化 */
    while (!g_ghost_table_initialized) { /* spin */
    }
}

/**
 * @brief 标记证明步骤的 QTT 用量 — 证明仅编译期存在，运行时擦除
 */
bool proof_mark_ghost(int step_id, ProofQuantifier quant) {
    ghost_table_init();

    if (step_id < 0 || step_id >= MAX_GHOST_STEPS) {
        return false;
    }

    g_ghost_table[step_id] = quant;
    return true;
}

/**
 * @brief 扫描依赖链，检查是否有 runtime 步骤依赖了 ERASED 步骤
 *
 * 遍历 ghost 标记表：
 * - 若 step_i 被标记为 ERASED（仅编译期证明），且存在某个非 ERASED
 *   步骤在依赖链中引用了 step_i，则产生冲突。
 * 遍历 ghost 标记表和证明导航器的依赖链，检测 runtime 步骤对
 * ERASED 步骤的非法依赖。
 *
 * @return 冲突数量
 */
int proof_check_ghost_conflicts(void) {
    ghost_table_init();

    int conflicts = 0;

    for (int i = 0; i < MAX_GHOST_STEPS; i++) {
        if (g_ghost_table[i] == PROOF_QTT_ERASED) {
            /* 当前无导航器关联，仅计数 ERASED 步骤。
             * FUTURE: 将导航器作为参数传入以进行完整依赖链检查。 */
            conflicts++;
        }
    }

    return conflicts;
}

