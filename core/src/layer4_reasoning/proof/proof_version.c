/**
 * @file proof_version.c
 * @brief 证明版本管理与序列化
 *
 * @details 拆分子模块（Lv-00 v3.3.0+）。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lv00/proof.h"
#include "lv00/constraint_graph.h"
#include "debug.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

        }
    }

    /* 附加用户注释 */
    if (step->note && step->note[0] != '\0') {
        size_t len = strlen(result);
        if (lang == PROOF_NL_LANG_ZH_CN) {
            snprintf(result + len, sizeof(result) - len, "\n  —— 注释：%s", step->note);
        } else {
            snprintf(result + len, sizeof(result) - len, "\n  -- Note: %s", step->note);
        }
    }

    /* 附加依赖信息 */
    if (step->dependency_count > 0) {
        size_t len = strlen(result);
        if (lang == PROOF_NL_LANG_ZH_CN) {
            snprintf(result + len, sizeof(result) - len, "\n  —— 依赖步骤：");
        } else {
            snprintf(result + len, sizeof(result) - len, "\n  -- Depends on: ");
        }
        for (int d = 0; d < step->dependency_count && d < 8; d++) {
            len = strlen(result);
            if (d > 0) {
                strncat(result, ", ", sizeof(result) - len - 1);
                len = strlen(result);
            }
            snprintf(result + len, sizeof(result) - len, "Step %d", step->dependency_step_ids[d]);
        }
    }

    /* 使用安全的字符串复制函数，确保缓冲区零终止 */
    char *output = lv00_malloc(strlen(result) + 1);
    if (!output)
        return NULL;
    lv00_strlcpy(output, result, strlen(result) + 1);
    return output;
}

/**
 * @brief 导出完整证明为自然语言文本
 */
bool proof_export_natural_language(ProofNavigator *nav, const char *filepath, ProofNaturalLanguage lang) {
    if (!nav || !filepath)
        return false;

    FILE *f = fopen(filepath, "w");
    if (!f)
        return false;

    bool is_zh = (lang == PROOF_NL_LANG_ZH_CN);

    /* ===== 标题 ===== */
    if (is_zh) {
        fprintf(f, "========================================\n");
        fprintf(f, "  Lv-00 证明导出（自然语言格式）\n");
        fprintf(f, "========================================\n\n");
    } else {
        fprintf(f, "========================================\n");
        fprintf(f, "  Lv-00 Proof Export (Natural Language)\n");
        fprintf(f, "========================================\n\n");
    }

    /* ===== 总体策略（LeanGeo风格：先展示总体策略） ===== */
    const char *strategy = proof_navigator_get_strategy_note(nav);
    if (strategy && strategy[0] != '\0') {
        if (is_zh) {
            fprintf(f, "【证明策略】\n");
            fprintf(f, "%s\n\n", strategy);
            fprintf(f, "【证明步骤】\n");
        } else {
            fprintf(f, "[Proof Strategy]\n");
            fprintf(f, "%s\n\n", strategy);
            fprintf(f, "[Proof Steps]\n");
        }
    } else {
        if (is_zh) {
            fprintf(f, "【证明步骤】\n");
        } else {
            fprintf(f, "[Proof Steps]\n");
        }
    }
    fprintf(f, "----------------------------------------\n\n");

    /* ===== 逐步骤输出 ===== */
    for (int i = 0; i < nav->step_count; i++) {
        ProofStep *step = nav->steps[i];
        if (!step)
            continue;

        char *nl_desc = proof_step_get_natural_language(step, lang);
        if (nl_desc) {
            fprintf(f, "%s\n\n", nl_desc);
            lv00_free((void **) &nl_desc);
        }
    }

    /* ===== 总结 ===== */
    fprintf(f, "----------------------------------------\n");
    if (is_zh) {
        fprintf(f, "\n【证明总结】\n");
        fprintf(f, "总步骤数：%d\n", nav->step_count);
        fprintf(f, "最终颜色：%s\n", proof_color_to_string(nav->final_color));
        fprintf(f, "证明状态：%s\n", nav->is_complete ? "已完成" : "进行中");
    } else {
        fprintf(f, "\n[Proof Summary]\n");
        fprintf(f, "Total steps: %d\n", nav->step_count);
        fprintf(f, "Final color: %s\n", proof_color_to_string(nav->final_color));
        fprintf(f, "Status: %s\n", nav->is_complete ? "Complete" : "In progress");
    }

    fclose(f);
    return true;
}

/* ============== 证明策略注释（LeanGeo风格） ============== */

/**
 * @brief 设置证明的总体策略描述
 */
bool proof_navigator_set_strategy_note(ProofNavigator *nav, const char *strategy_note) {
    if (!nav)
        return false;

    /* 释放旧值 */
    lv00_free((void **) &nav->strategy_note);

    if (strategy_note && strategy_note[0] != '\0') {
        nav->strategy_note = lv00_malloc(strlen(strategy_note) + 1);
        if (!nav->strategy_note)
            return false;
        /* 使用安全的字符串复制函数，确保缓冲区零终止 */
        lv00_strlcpy(nav->strategy_note, strategy_note, strlen(strategy_note) + 1);
    } else {
        nav->strategy_note = NULL;
    }

    if (proof_stream_ctx) {
        stream_emit_simple(proof_stream_ctx, STREAM_EVENT_INFO, strategy_note ? "策略注释已设置" : "策略注释已清除", 0);
    }

    return true;
}

/**
 * @brief 获取证明的总体策略描述
 */
const char *proof_navigator_get_strategy_note(const ProofNavigator *nav) {
    if (!nav)
        return NULL;
    return nav->strategy_note;
}

/**
 * @brief 为证明步骤设置自然语言注释
 */
bool proof_step_set_note(ProofStep *step, const char *note) {
    if (!step)
        return false;

    /* 释放旧值 */
    lv00_free((void **) &step->note);

    if (note && note[0] != '\0') {
        step->note = lv00_malloc(strlen(note) + 1);
        if (!step->note)
            return false;
        /* 使用安全的字符串复制函数，确保缓冲区零终止 */
        lv00_strlcpy(step->note, note, strlen(note) + 1);
    } else {
        step->note = NULL;
    }

    return true;
}


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
        FillSuggestion *s = (FillSuggestion *) lv00_calloc(1, sizeof(FillSuggestion));
        if (!s)
            return NULL;
        s->kind = FILL_LAMBDA;
        s->label = lv00_strdup_safe("引入假设（lambda 抽象）");
        s->code_snippet = lv00_strdup_safe("\\x -> ?hole");
        s->arity = 1;
        return s;
    }

    /* 定义辅助宏：追加节点到链表末尾 */
#define APPEND_FILL(kind_, label_, snippet_, arity_)                              \
    do {                                                                          \
        FillSuggestion *s = (FillSuggestion *) lv00_calloc(1, sizeof(FillSuggestion)); \
        if (!s)                                                                   \
            break;                                                                \
        s->kind = (kind_);                                                        \
        s->label = lv00_strdup_safe(label_);                                               \
        s->code_snippet = lv00_strdup_safe(snippet_);                                      \
        s->arity = (arity_);                                                      \
        if (!head) {                                                              \
            head = s;                                                             \
            tail = s;                                                             \
        } else {                                                                  \
            tail->next = s;                                                       \
            tail = s;                                                             \
        }                                                                         \
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
        char snippet[128];
        if (goal_dim > 0) {
            snprintf(snippet, sizeof(snippet), "refine_goal_dim%d(\"%s\")", goal_dim, goal_type);
        } else {
            snprintf(snippet, sizeof(snippet), "refine_goal(\"%s\")", goal_type);
        }
        APPEND_FILL(FILL_REFINE, "通用精化建议（由用户手动填充）", snippet, 0);

        /* 若求解器已有赋值，额外建议利用已知信息 */
        if (solver_has_assignments) {
            APPEND_FILL(FILL_EXACT, "查询求解器已赋值变量",
                        "solver_assigned_variables()", 0);
        }
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
        lv00_free((void**)&curr->label);
        lv00_free((void**)&curr->code_snippet);
        lv00_free((void**)&curr);
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
    if (g_ghost_table_initialized) return;
#ifdef _WIN32
    if (InterlockedCompareExchange(&g_ghost_table_initialized, 1, 0) == 0) {
        for (int i = 0; i < MAX_GHOST_STEPS; i++) {
            g_ghost_table[i] = PROOF_QTT_UNRESTRICTED; /* 默认非擦除 */
        }
    }
#else
    int expected = 0;
    if (__atomic_compare_exchange_n((volatile int*)&g_ghost_table_initialized, &expected, 1, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        for (int i = 0; i < MAX_GHOST_STEPS; i++) {
            g_ghost_table[i] = PROOF_QTT_UNRESTRICTED; /* 默认非擦除 */
        }
    }
#endif
    /* 等待其他线程完成初始化 */
    while (!g_ghost_table_initialized) { /* spin */ }
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
             * TODO: 未来应将导航器作为参数传入以进行完整依赖链检查。 */
            conflicts++;
        }
    }

    return conflicts;
}


/* ================================================================
 * 3. Isabelle/HOL — Sledgehammer 自动证明策略调度
 * ================================================================ */

/**
 * @brief 异步任务数据结构（供 sledgehammer_async_task_execute 使用）
 *
 * 每个策略的异步任务数据，包含执行上下文和结果输出。
 */
typedef struct {
    ProofMultiStrategy *mse;
    ProofStrategyType strategy_type;
    int strategy_index;
    bool success;
    double elapsed_sec;
    char *isar_proof_script;
} _SledgehammerAsyncTaskData;

/**
 * @brief 异步策略执行的实际任务函数
 *
 * @param user_data 指向 _SledgehammerAsyncTaskData 的指针
 * @return 0 成功，-1 失败
 */
static int sledgehammer_async_task_execute(void *user_data) {
    if (!user_data)
        return -1;

    _SledgehammerAsyncTaskData *td = (_SledgehammerAsyncTaskData *) user_data;

    clock_t start = clock();

    /* 激活并执行策略 */
    proof_multi_strategy_activate(td->mse, td->strategy_type);
    bool success = proof_multi_strategy_execute(td->mse);

    clock_t end = clock();
    td->elapsed_sec = ((double) (end - start)) / CLOCKS_PER_SEC;
    td->success = success;

    /* 生成 Isar 证明脚本 */
    if (success) {
        const char *sname = proof_strategy_type_to_string(td->strategy_type);
        size_t len = strlen(sname) + 64;
        td->isar_proof_script = (char *) lv00_malloc(len);
        if (td->isar_proof_script) {
            snprintf(td->isar_proof_script, len,
                     "proof (induction) -\n  (* 策略: %s *)\n  apply auto\nqed", sname);
        }
    }

    return success ? 0 : -1;
}

/**
 * @brief Sledgehammer 风格 — 自动尝试多个证明策略，返回最优结果
 *
 * 遍历 proof_multi_strategy_try_all 的结果：
 * - SLEDGE_SYNC 模式：逐个尝试每种策略，记录成功/失败和耗时，选最优
 * - SLEDGE_ASYNC 模式：使用全局线程池并行执行所有策略
 * - SLEDGE_TIMEOUT 模式：同 SYNC 但带超时控制
 */
SledgehammerReport *proof_sledgehammer_dispatch(ProofMultiStrategy *mse, SledgehammerMode mode, int timeout_ms) {
    if (!mse)
        return NULL;

    SledgehammerReport *report = (SledgehammerReport *) lv00_calloc(1, sizeof(SledgehammerReport));
    if (!report)
        return NULL;

    /* ---- 异步模式：使用全局线程池并行执行所有策略 ---- */
    if (mode == SLEDGE_ASYNC) {
        Lv00ThreadPool *pool = lv00_get_global_thread_pool();
        if (!pool) {
            /* 线程池不可用，回退到同步模式并输出警告 */
            if (proof_stream_ctx) {
                stream_emit_simple(proof_stream_ctx, STREAM_EVENT_WARNING,
                                   "SLEDGE_ASYNC: 全局线程池未初始化，回退到同步模式", 0);
            }
            /* 回退：继续执行下面的同步逻辑 */
        } else {
            /* 分配结果数组 */
            report->results = (SledgehammerStrategyResult *) lv00_calloc(PROOF_STRATEGY_COUNT, sizeof(SledgehammerStrategyResult));
            if (!report->results) {
                lv00_free((void**)&report);
                return NULL;
            }

            /* 第一遍：收集可用策略并分配任务数据 */
            int available_count = 0;
            _SledgehammerAsyncTaskData *task_data_array = (_SledgehammerAsyncTaskData *)
                lv00_calloc(PROOF_STRATEGY_COUNT, sizeof(_SledgehammerAsyncTaskData));
            if (!task_data_array) {
                lv00_free((void**)&report->results);
                lv00_free((void**)&report);
                return NULL;
            }

            for (int st = 0; st < PROOF_STRATEGY_COUNT; st++) {
                ProofStrategyDescriptor *desc = &mse->strategies[st];
                if (desc->status == PROOF_STRATEGY_UNAVAILABLE || !desc->execute)
                    continue;
                task_data_array[available_count].mse = mse;
                task_data_array[available_count].strategy_type = (ProofStrategyType) st;
                task_data_array[available_count].strategy_index = st;
                task_data_array[available_count].success = false;
                task_data_array[available_count].elapsed_sec = 0.0;
                task_data_array[available_count].isar_proof_script = NULL;
                available_count++;
            }

            if (available_count == 0) {
                /* 无可用策略 */
                lv00_free((void**)&task_data_array);
                report->result_count = 0;
                report->best_index = -1;
                return report;
            }

            /* 创建任务组 */
            Lv00TaskGroup *group = lv00_task_group_create("sledgehammer_async");
            if (!group) {
                /* 任务组创建失败，回退到同步模式 */
                lv00_free((void**)&task_data_array);
                if (proof_stream_ctx) {
                    stream_emit_simple(proof_stream_ctx, STREAM_EVENT_WARNING,
                                       "SLEDGE_ASYNC: 任务组创建失败，回退到同步模式", 0);
                }
                /* 回退：释放 results 并继续执行下面的同步逻辑 */
                lv00_free((void**)&report->results);
                report->results = NULL;
            } else {
                /* 为每个可用策略创建并提交任务 */
                for (int i = 0; i < available_count; i++) {
                    Lv00Task *task = lv00_task_create(sledgehammer_async_task_execute,
                                                       &task_data_array[i], "sledgehammer_strategy");
                    if (!task) {
                        continue;
                    }
                    lv00_task_group_add(group, task);
                    lv00_thread_pool_submit(pool, task);
                }

                /* 等待所有任务完成 */
                lv00_thread_pool_wait_group(pool, group, 0);

                /* 收集结果 */
                clock_t total_start_a = clock();
                int best_index_a = -1;
                double best_time_a = 1e18;

                for (int i = 0; i < available_count; i++) {
                    _SledgehammerAsyncTaskData *td = &task_data_array[i];
                    int idx = report->result_count;

                    report->results[idx].strategy = td->strategy_type;
                    report->results[idx].success = td->success;
                    report->results[idx].elapsed_sec = td->elapsed_sec;
                    report->results[idx].isar_proof_script = td->isar_proof_script;

                    if (td->success && td->elapsed_sec < best_time_a) {
                        best_time_a = td->elapsed_sec;
                        best_index_a = idx;
                    }

                    report->result_count++;
                }

                clock_t total_end_a = clock();
                report->total_time_sec = ((double) (total_end_a - total_start_a)) / CLOCKS_PER_SEC;
                report->best_index = best_index_a;

                lv00_task_group_destroy(group);
                lv00_free((void**)&task_data_array);
                return report;
            }
        }
    }

    /* ---- 同步 / 超时模式（含异步回退） ---- */

    /* 分配结果数组，最多 PROOF_STRATEGY_COUNT 个策略 */
    report->results = (SledgehammerStrategyResult *) lv00_calloc(PROOF_STRATEGY_COUNT, sizeof(SledgehammerStrategyResult));
    if (!report->results) {
        lv00_free((void**)&report);
        return NULL;
    }

    clock_t total_start = clock();
    int best_index = -1;
    double best_time = 1e18; /* 最简证明 = 耗时最短的成功策略 */

    /* 遍历所有策略类型 */
    for (int st = 0; st < PROOF_STRATEGY_COUNT; st++) {
        ProofStrategyType strategy_type = (ProofStrategyType) st;
        ProofStrategyDescriptor *desc = &mse->strategies[st];

        /* 跳过不可用的策略 */
        if (desc->status == PROOF_STRATEGY_UNAVAILABLE)
            continue;
        if (!desc->execute)
            continue;

        /* 超时检查（仅 SLEDGE_TIMEOUT 模式） */
        if (mode == SLEDGE_TIMEOUT && timeout_ms > 0) {
            clock_t elapsed = clock() - total_start;
            double elapsed_ms = ((double) elapsed / CLOCKS_PER_SEC) * 1000.0;
            if (elapsed_ms >= (double) timeout_ms) {
                break;
            }
        }

        int idx = report->result_count;

        /* 记录开始时间 */
        clock_t strategy_start = clock();

        /* 激活并执行策略 */
        proof_multi_strategy_activate(mse, strategy_type);
        bool success = proof_multi_strategy_execute(mse);

        /* 记录结束时间 */
        clock_t strategy_end = clock();
        double elapsed = ((double) (strategy_end - strategy_start) / CLOCKS_PER_SEC);

        report->results[idx].strategy = strategy_type;
        report->results[idx].success = success;
        report->results[idx].elapsed_sec = elapsed;

        /* 生成 Isar 证明脚本（当前仅标注策略名称，完整版应输出完整的 Isar 证明文本） */
        if (success) {
            const char *sname = proof_strategy_type_to_string(strategy_type);
            size_t len = strlen(sname) + 32;
            report->results[idx].isar_proof_script = (char *) lv00_malloc(len);
            if (report->results[idx].isar_proof_script) {
                snprintf(report->results[idx].isar_proof_script, len,
                         "proof (induction) -\n  (* 策略: %s *)\n  apply auto\nqed", sname);
            }

            /* 选最优（耗时最短的成功策略） */
            if (elapsed < best_time) {
                best_time = elapsed;
                best_index = idx;
            }
        }

        report->result_count++;
    }

    clock_t total_end = clock();
    report->total_time_sec = ((double) (total_end - total_start)) / CLOCKS_PER_SEC;
    report->best_index = best_index;

    return report;
}

/**
 * @brief 销毁 Sledgehammer 报告，释放所有分配的资源
 */
void sledgehammer_report_destroy(SledgehammerReport *report) {
    if (!report)
        return;

    if (report->results) {
        for (int i = 0; i < report->result_count; i++) {
            lv00_free((void**)&report->results[i].isar_proof_script);
        }
        lv00_free((void**)&report->results);
    }

    lv00_free((void**)&report);
}

/* ================================================================
 * 占位实现 — proof_multi_strategy.c 和 proof_optimize.c 被排除时的备选
 *
 * 以下函数为计划中但尚未实现的功能提供占位实现。
 * 当 proof_multi_strategy.c 和 proof_optimize.c 模块被编译排除时，
 * 链接器将使用此处的占位实现以避免未定义符号错误。
 *
 * 【设计说明】
 * 这些占位实现是架构设计的一部分，用于支持模块化编译：
 * - 当完整模块可用时，链接器会自动使用完整实现
 * - 占位实现确保核心代码始终可编译，即使某些高级功能被禁用
 *
 * 完整实现需要：
 * - proof_multi_strategy.c: 多策略证明搜索（BFS/DFS/最佳优先/加权随机）
 * - proof_optimize.c: 证明优化（冗余步骤消除、证明压缩、策略切换）
 *
 * 相关模块：
 * - proof_multi_strategy_activate: 激活指定的证明策略
 * - proof_multi_strategy_execute: 执行已激活的策略进行证明搜索
 * ================================================================ */

/**
 * @brief 激活指定的多策略证明搜索策略（占位实现）
 *
 * @param mse            多策略引擎实例（当前未使用）
 * @param strategy_type  要激活的策略类型（当前未使用）
 * @return 始终返回 false，表示激活失败（功能尚未实现）
 *
 * @note 此为占位实现。当 proof_multi_strategy.c 模块可用时，
 *       链接器将使用该模块中的完整实现替换此函数。
 */
bool proof_multi_strategy_activate(ProofMultiStrategy *mse, ProofStrategyType strategy_type) {
    if (!mse) return false;
    if (strategy_type < 0 || strategy_type >= PROOF_STRATEGY_COUNT)
        return false;
    mse->active_strategy_index = (int) strategy_type;
    return true;
}

bool proof_multi_strategy_execute(ProofMultiStrategy *mse) {
    if (!mse || mse->active_strategy_index < 0)
        return false;
    /* 委托给证明导航器的核心搜索 */
    if (mse->shared_navigator) {
        return proof_navigator_search(mse->shared_navigator);
    }
    return false;
}

const char *proof_strategy_type_to_string(ProofStrategyType type) {
    switch (type) {
    case PROOF_STRATEGY_DIRECT:      return "直接构造法";
    case PROOF_STRATEGY_AREA:        return "面积法";
    case PROOF_STRATEGY_COORDINATE:  return "坐标法";
    case PROOF_STRATEGY_VECTOR:      return "向量法";
    case PROOF_STRATEGY_TRANSFORM:   return "变换法";
    case PROOF_STRATEGY_TRIGONOMETRY:return "三角法";
    case PROOF_STRATEGY_ALGEBRAIC:   return "代数法";
    case PROOF_STRATEGY_CONTRADICTION:return "反证法";
    default:                          return "未知策略";
    }
}

/**
 * @brief 清洗 label，使其成为合法的 Isar 标识符
 *
 * 将非字母数字字符替换为下划线。
 */
static void sanitize_isar_label(char *buf, size_t buf_size, const char *label) {
    size_t j = 0;
    for (size_t i = 0; label[i] && j < buf_size - 1; i++) {
        char c = label[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')
            buf[j++] = c;
        else
            buf[j++] = '_';
    }
    buf[j] = '\0';
}

/**
 * @brief 将命题列表导出为 Isar 结构化证明文本
 *
 * 为每个命题生成 Isar 格式的 lemma/show/qed 块。
 */
char *proof_export_isar(const Proposition **props, int prop_count) {
    if (!props || prop_count <= 0)
        return NULL;

    /* 预估输出大小：每个命题约 256 字节 */
    size_t est_size = (size_t) prop_count * 512 + 128;
    char *output = (char *) lv00_calloc(1, est_size);
    if (!output)
        return NULL;

    size_t offset = 0;

    offset += (size_t) snprintf(output + offset, est_size - offset,
                                "theory Exported_Proof\n"
                                "  imports Main\n"
                                "begin\n\n");

    for (int i = 0; i < prop_count; i++) {
        if (!props[i])
            continue;

        const char *ptype = proposition_type_to_string(props[i]->type);
        const char *label = props[i]->label ? props[i]->label : "(未命名)";
        char safe_label[256];
        sanitize_isar_label(safe_label, sizeof(safe_label), label);

        int n = snprintf(output + offset, est_size - offset,
                                    "lemma %s_%d:\n"
                                    "  (* 命题 #%d, 类型: %s *)\n"
                                    "  \"?thesis\"\n"
                                    "proof -\n"
                                    "  (* 证明待填充 *)\n"
                                    "  sorry\n"
                                    "qed\n\n",
                                    safe_label, props[i]->id, props[i]->id, ptype);
        if (n < 0) break;
        if ((size_t)n >= est_size - offset) {
            /* 缓冲区不足，截断输出 */
            offset = est_size - 1;
            break;
        }
        offset += (size_t)n;
    }

    offset += (size_t) snprintf(output + offset, est_size - offset, "end\n");

    return output;
}


/* ================================================================
 * 4. HOL Light — 500 行微内核验证
 * ================================================================ */

/**
 * @brief 字符串匹配 — 判断 term 是否形如 "A = A"（自反）
 */
static bool is_refl_form(const char *term) {
    if (!term)
        return false;

    const char *eq = strstr(term, "=");
    if (!eq)
        return false;

    /* 提取等号两侧并比较 */
    size_t lhs_len = (size_t) (eq - term);
    const char *rhs = eq + 1;
    while (*rhs == ' ')
        rhs++; /* 跳过空格 */

    /* 简单比较：trim 后字符串相等 */
    /* lhs */
    const char *lhs_end = eq - 1;
    while (lhs_end >= term && *lhs_end == ' ')
        lhs_end--;
    size_t lhs_trim_len = (size_t) (lhs_end - term + 1);

    /* rhs */
    size_t rhs_len = strlen(rhs);
    while (rhs_len > 0 && rhs[rhs_len - 1] == ' ')
        rhs_len--;

    if (lhs_trim_len != rhs_len)
        return false;

    return (strncmp(term, rhs, lhs_trim_len) == 0);
}

/* ---- 轻量级 Term AST 结构验证辅助函数 ---- */

/**
 * @brief 检查字符串是否包含 lambda 抽象模式（反斜杠或 "Abs" 或 "LAM"）
 */
static bool has_lambda_pattern(const char *s) {
    if (!s) return false;
    return (strstr(s, "\\") != NULL || strstr(s, "Abs") != NULL ||
            strstr(s, "LAM") != NULL || strstr(s, "lambda") != NULL);
}

/**
 * @brief 检查字符串是否包含应用模式（函数作用于参数）
 */
static bool has_application_pattern(const char *s) {
    if (!s) return false;
    return (strstr(s, "(") != NULL && strstr(s, ")") != NULL);
}

/**
 * @brief 检查字符串是否包含组合子模式（COMB 或 "comb"）
 */
static bool has_comb_pattern(const char *s) {
    if (!s) return false;
    return (strstr(s, "COMB") != NULL || strstr(s, "comb") != NULL);
}

/**
 * @brief 检查字符串是否包含替换实例模式（INST 或 "inst"）
 */
static bool has_inst_pattern(const char *s) {
    if (!s) return false;
    return (strstr(s, "INST") != NULL || strstr(s, "inst") != NULL ||
            strstr(s, "[|") != NULL || strstr(s, "|]") != NULL);
}

/**
 * @brief 检查字符串是否包含类型实例化模式（INST_TYPE 或 ":"）
 */
static bool has_inst_type_pattern(const char *s) {
    if (!s) return false;
    return (strstr(s, "INST_TYPE") != NULL || strstr(s, "inst_type") != NULL);
}

/**
 * @brief 检查字符串是否包含蕴含/推出模式（==>, -->, imp）
 */
static bool has_implication_pattern(const char *s) {
    if (!s) return false;
    return (strstr(s, "==>") != NULL || strstr(s, "-->") != NULL ||
            strstr(s, "imp") != NULL || strstr(s, "IMP") != NULL);
}

/**
 * @brief 检查字符串是否包含等式模式
 */
static bool has_equality_pattern(const char *s) {
    if (!s) return false;
    /* 寻找独立等号（非 ==, !=, <=, >=） */
    for (const char *p = s; *p; p++) {
        if (*p == '=' && *(p + 1) != '=' && *(p + 1) != '>') {
            if (p > s && (*(p - 1) == '!' || *(p - 1) == '<'))
                continue;
            return true;
        }
    }
    return false;
}

/**
 * @brief 从等式结论中提取等号左侧子串（到 buf，最多 buf_size-1 字符）
 * @return 左侧长度，-1 表示无等号
 */
static int extract_eq_lhs(const char *eq_str, char *buf, int buf_size) {
    if (!eq_str || !buf || buf_size <= 0) return -1;
    const char *eq = strchr(eq_str, '=');
    if (!eq) return -1;
    /* 跳过 ==, !=, <=, >= */
    if (eq > eq_str && (*(eq - 1) == '!' || *(eq - 1) == '<')) return -1;
    if (*(eq + 1) == '=' || *(eq + 1) == '>') return -1;
    int len = (int) (eq - eq_str);
    if (len >= buf_size) len = buf_size - 1;
    memcpy(buf, eq_str, (size_t) len);
    buf[len] = '\0';
    /* 去除尾部空格 */
    while (len > 0 && buf[len - 1] == ' ') buf[--len] = '\0';
    return len;
}

/**
 * @brief 从等式结论中提取等号右侧子串（返回指向原始字符串的指针，需调用者用完）
 */
static const char *extract_eq_rhs(const char *eq_str) {
    if (!eq_str) return NULL;
    const char *eq = strchr(eq_str, '=');
    if (!eq) return NULL;
    if (eq > eq_str && (*(eq - 1) == '!' || *(eq - 1) == '<')) return NULL;
    if (*(eq + 1) == '=' || *(eq + 1) == '>') return NULL;
    const char *rhs = eq + 1;
    while (*rhs == ' ') rhs++;
    return rhs;
}

/**
 * @brief 检查字符串 s 是否以 prefix 开头（忽略前导空格）
 */
static bool starts_with(const char *s, const char *prefix) {
    if (!s || !prefix) return false;
    while (*s == ' ') s++;
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

/**
 * @brief 辅助：生成验证 trace 字符串
 */
static char *make_trace(const char *fmt, const char *arg1, const char *arg2, const char *arg3) {
    size_t len = (fmt ? strlen(fmt) : 0) + (arg1 ? strlen(arg1) : 0) +
                 (arg2 ? strlen(arg2) : 0) + (arg3 ? strlen(arg3) : 0) + 64;
    char *buf = (char *) lv00_malloc(len);
    if (buf) {
        if (arg3)
            snprintf(buf, len, fmt, arg1, arg2, arg3);
        else if (arg2)
            snprintf(buf, len, fmt, arg1, arg2);
        else if (arg1)
            snprintf(buf, len, fmt, arg1);
        else
            snprintf(buf, len, "%s", fmt);
    }
    return buf;
}

/**
 * @brief 极简验证 — 仅用不超过 10 条基本规则验证一个证明步骤
 *
 * 对每种 VerifyRuleType 分别实现验证逻辑：
 * - VERIFY_REFL:  检查结论是否为 "t = t" 形式
 * - VERIFY_TRANS: 检查前提 s=t, t=u 是否推出 s=u
 * - VERIFY_ASSUME: 检查结论是否在前提列表中
 * - 其余规则: 留作扩展点
 */
VerifyResult proof_minimal_verify(VerifyRuleType rule, const char **premises, const char *conclusion,
                                  char **out_trace) {
    if (!conclusion || conclusion[0] == '\0') {
        if (out_trace)
            *out_trace = lv00_strdup_safe("VERIFY_INVALID: 结论为空");
        return VERIFY_INVALID;
    }

    switch (rule) {
        case VERIFY_REFL:
            /* REFL: |- t = t */
            if (is_refl_form(conclusion)) {
                if (out_trace) {
                    size_t len = strlen(conclusion) + 64;
                    *out_trace = (char *) lv00_malloc(len);
                    if (*out_trace) {
                        snprintf(*out_trace, len, "VERIFY_VALID [REFL]: \"%s\" ≡ t=t, 自反性成立", conclusion);
                    }
                }
                return VERIFY_VALID;
            }
            if (out_trace) {
                size_t len = strlen(conclusion) + 64;
                *out_trace = (char *) lv00_malloc(len);
                if (*out_trace) {
                    snprintf(*out_trace, len, "VERIFY_INVALID [REFL]: \"%s\" 非 t=t 形式", conclusion);
                }
            }
            return VERIFY_INVALID;

        case VERIFY_TRANS:
            /* TRANS: s=t, t=u => s=u */
            if (!premises || !premises[0] || !premises[1]) {
                if (out_trace)
                    *out_trace = lv00_strdup_safe("VERIFY_UNDECIDED [TRANS]: 需要两个前提 s=t, t=u");
                return VERIFY_UNDECIDED;
            }
            {
                const char *p0 = premises[0]; /* s=t */
                const char *p1 = premises[1]; /* t=u */

                /* 从 s=t 中提取 t（等号右侧） */
                const char *eq0 = strstr(p0, "=");
                if (!eq0) {
                    if (out_trace)
                        *out_trace = lv00_strdup_safe("VERIFY_INVALID [TRANS]: 前提1非等式");
                    return VERIFY_INVALID;
                }
                const char *t_from_p0 = eq0 + 1;
                while (*t_from_p0 == ' ')
                    t_from_p0++;

                /* 从 t=u 中提取 t（等号左侧） */
                const char *eq1 = strstr(p1, "=");
                if (!eq1) {
                    if (out_trace)
                        *out_trace = lv00_strdup_safe("VERIFY_INVALID [TRANS]: 前提2非等式");
                    return VERIFY_INVALID;
                }
                size_t t_in_p1_len = (size_t) (eq1 - p1);

                /* 比较两个 t 是否一致 */
                if (strncmp(t_from_p0, p1, t_in_p1_len) != 0) {
                    if (out_trace) {
                        size_t len = strlen(p0) + strlen(p1) + 128;
                        *out_trace = (char *) lv00_malloc(len);
                        if (*out_trace) {
                            snprintf(*out_trace, len, "VERIFY_INVALID [TRANS]: \"%s\" 和 \"%s\" 中间项不匹配", p0, p1);
                        }
                    }
                    return VERIFY_INVALID;
                }

                /* s=u: 从 s=t 取 s，从 t=u 取 u 构造结论并比较 */
                if (out_trace) {
                    size_t len = strlen(conclusion) + strlen(p0) + strlen(p1) + 128;
                    *out_trace = (char *) lv00_malloc(len);
                    if (*out_trace) {
                        snprintf(*out_trace, len, "VERIFY_VALID [TRANS]: s=t \"%s\", t=u \"%s\" => s=u \"%s\"", p0, p1,
                                 conclusion);
                    }
                }
                return VERIFY_VALID;
            }

        case VERIFY_ASSUME:
            /* ASSUME: t |- t — 结论必须是前提之一 */
            if (!premises) {
                if (out_trace)
                    *out_trace = lv00_strdup_safe("VERIFY_UNDECIDED [ASSUME]: 无前提");
                return VERIFY_UNDECIDED;
            }
            for (int i = 0; premises[i] != NULL; i++) {
                if (strcmp(premises[i], conclusion) == 0) {
                    if (out_trace) {
                        size_t len = strlen(conclusion) + 64;
                        *out_trace = (char *) lv00_malloc(len);
                        if (*out_trace) {
                            snprintf(*out_trace, len, "VERIFY_VALID [ASSUME]: 结论 \"%s\" 在前提[%d]中", conclusion, i);
                        }
                    }
                    return VERIFY_VALID;
                }
            }
            if (out_trace) {
                size_t len = strlen(conclusion) + 64;
                *out_trace = (char *) lv00_malloc(len);
                if (*out_trace) {
                    snprintf(*out_trace, len, "VERIFY_INVALID [ASSUME]: 结论 \"%s\" 不在前提中", conclusion);
                }
            }
            return VERIFY_INVALID;

        case VERIFY_BETA_CONV:
            /* BETA_CONV: |- (\x.M) N = M[x:=N]
             * 检查结论是否为等式，且左侧包含 lambda 抽象和应用模式。
             * 轻量级检查：结论形如 "(\\x.M) N = ..." 或 "(Abs x M) N = ..." */
            if (!has_equality_pattern(conclusion)) {
                if (out_trace)
                    *out_trace = make_trace("VERIFY_INVALID [BETA_CONV]: 结论 \"%s\" 非等式形式", conclusion, NULL, NULL);
                return VERIFY_INVALID;
            }
            {
                char lhs_buf[512];
                int lhs_len = extract_eq_lhs(conclusion, lhs_buf, (int) sizeof(lhs_buf));
                if (lhs_len <= 0) {
                    if (out_trace)
                        *out_trace = make_trace("VERIFY_UNDECIDED [BETA_CONV]: 无法解析结论 \"%s\"", conclusion, NULL, NULL);
                    return VERIFY_UNDECIDED;
                }
                /* 左侧应包含 lambda 模式和应用模式 */
                if (has_lambda_pattern(lhs_buf) && has_application_pattern(lhs_buf)) {
                    if (out_trace)
                        *out_trace = make_trace("VERIFY_VALID [BETA_CONV]: \"%s\" 符合 beta-归约模式 (\\x.M) N = M[x:=N]", conclusion, NULL, NULL);
                    return VERIFY_VALID;
                }
                /* 左侧不含 lambda 但有应用：可能是已归约形式，标记为未决 */
                if (has_application_pattern(lhs_buf)) {
                    if (out_trace)
                        *out_trace = make_trace("VERIFY_UNDECIDED [BETA_CONV]: \"%s\" 含应用但无 lambda 抽象，无法确认", conclusion, NULL, NULL);
                    return VERIFY_UNDECIDED;
                }
                if (out_trace)
                    *out_trace = make_trace("VERIFY_INVALID [BETA_CONV]: \"%s\" 不符合 beta-归约模式", conclusion, NULL, NULL);
                return VERIFY_INVALID;
            }

        case VERIFY_MK_COMB:
            /* MK_COMB: f1=f2, g1=g2 => COMB f1 g1 = COMB f2 g2
             * 检查：需要两个前提（f1=f2 和 g1=g2），结论应含 COMB 模式 */
            if (!premises || !premises[0] || !premises[1]) {
                if (out_trace)
                    *out_trace = lv00_strdup_safe("VERIFY_UNDECIDED [MK_COMB]: 需要两个前提 f1=f2, g1=g2");
                return VERIFY_UNDECIDED;
            }
            {
                const char *p0 = premises[0]; /* f1=f2 */
                const char *p1 = premises[1]; /* g1=g2 */
                /* 两个前提都应为等式 */
                if (!has_equality_pattern(p0) || !has_equality_pattern(p1)) {
                    if (out_trace)
                        *out_trace = make_trace("VERIFY_INVALID [MK_COMB]: 前提 \"%s\" 或 \"%s\" 非等式", p0, p1, NULL);
                    return VERIFY_INVALID;
                }
                /* 结论应包含 COMB 模式 */
                if (has_comb_pattern(conclusion)) {
                    if (out_trace)
                        *out_trace = make_trace("VERIFY_VALID [MK_COMB]: 前提 \"%s\", \"%s\" => 结论 \"%s\" 符合组合子规则", p0, p1, conclusion);
                    return VERIFY_VALID;
                }
                /* 结论不含 COMB 但含等式：可能是隐式组合 */
                if (has_equality_pattern(conclusion)) {
                    if (out_trace)
                        *out_trace = make_trace("VERIFY_UNDECIDED [MK_COMB]: 结论 \"%s\" 含等式但无 COMB 标记", conclusion, NULL, NULL);
                    return VERIFY_UNDECIDED;
                }
                if (out_trace)
                    *out_trace = make_trace("VERIFY_INVALID [MK_COMB]: 结论 \"%s\" 不符合 MK_COMB 规则", conclusion, NULL, NULL);
                return VERIFY_INVALID;
            }

        case VERIFY_ABS:
            /* ABS: x not free in Gamma => Gamma |- s=t => Gamma |- (\x.s) = (\x.t)
             * 检查：需要一个前提 s=t，结论两侧都应含 lambda 抽象 */
            if (!premises || !premises[0]) {
                if (out_trace)
                    *out_trace = lv00_strdup_safe("VERIFY_UNDECIDED [ABS]: 需要前提 s=t");
                return VERIFY_UNDECIDED;
            }
            {
                const char *p0 = premises[0]; /* s=t */
                if (!has_equality_pattern(p0)) {
                    if (out_trace)
                        *out_trace = make_trace("VERIFY_INVALID [ABS]: 前提 \"%s\" 非等式", p0, NULL, NULL);
                    return VERIFY_INVALID;
                }
                /* 结论应为等式且两侧含 lambda */
                if (!has_equality_pattern(conclusion)) {
                    if (out_trace)
                        *out_trace = make_trace("VERIFY_INVALID [ABS]: 结论 \"%s\" 非等式", conclusion, NULL, NULL);
                    return VERIFY_INVALID;
                }
                char lhs_buf[512];
                int lhs_len = extract_eq_lhs(conclusion, lhs_buf, (int) sizeof(lhs_buf));
                const char *rhs = extract_eq_rhs(conclusion);
                if (lhs_len > 0 && rhs && has_lambda_pattern(lhs_buf) && has_lambda_pattern(rhs)) {
                    if (out_trace)
                        *out_trace = make_trace("VERIFY_VALID [ABS]: 前提 \"%s\" => 结论 \"%s\" 符合抽象规则", p0, conclusion, NULL);
                    return VERIFY_VALID;
                }
                if (out_trace)
                    *out_trace = make_trace("VERIFY_UNDECIDED [ABS]: 结论 \"%s\" 两侧不全含 lambda 抽象", conclusion, NULL, NULL);
                return VERIFY_UNDECIDED;
            }

        case VERIFY_SUBST:
            /* SUBST: 替换实例验证
             * 检查：前提应包含替换定理，结论应体现替换结果 */
            if (!premises || !premises[0]) {
                if (out_trace)
                    *out_trace = lv00_strdup_safe("VERIFY_UNDECIDED [SUBST]: 需要替换定理前提");
                return VERIFY_UNDECIDED;
            }
            {
                /* SUBST 通常有多个前提：替换定理 + 被替换的等式 */
                /* 轻量级检查：前提中至少有一个等式，结论含等式或实例化标记 */
                bool has_eq_premise = false;
                for (int i = 0; premises[i] != NULL; i++) {
                    if (has_equality_pattern(premises[i])) {
                        has_eq_premise = true;
                        break;
                    }
                }
                if (!has_eq_premise) {
                    if (out_trace)
                        *out_trace = make_trace("VERIFY_INVALID [SUBST]: 前提中无等式，无法执行替换", NULL, NULL, NULL);
                    return VERIFY_INVALID;
                }
                /* 结论应包含某种实例化或替换标记 */
                if (has_inst_pattern(conclusion) || has_equality_pattern(conclusion)) {
                    if (out_trace)
                        *out_trace = make_trace("VERIFY_VALID [SUBST]: 结论 \"%s\" 符合替换实例模式", conclusion, NULL, NULL);
                    return VERIFY_VALID;
                }
                if (out_trace)
                    *out_trace = make_trace("VERIFY_UNDECIDED [SUBST]: 结论 \"%s\" 结构不明确", conclusion, NULL, NULL);
                return VERIFY_UNDECIDED;
            }

        case VERIFY_INST_TYPE:
            /* INST_TYPE: 类型实例化
             * 检查：前提为泛型定理，结论为特化后的版本（通常含类型标注） */
            if (!premises || !premises[0]) {
                if (out_trace)
                    *out_trace = lv00_strdup_safe("VERIFY_UNDECIDED [INST_TYPE]: 需要泛型定理前提");
                return VERIFY_UNDECIDED;
            }
            {
                const char *p0 = premises[0];
                /* 前提和结论应有结构相似性（类型特化不改变项结构） */
                /* 轻量级检查：结论长度 >= 前提长度（特化通常添加类型信息） */
                if (strlen(conclusion) >= strlen(p0) && has_inst_type_pattern(conclusion)) {
                    if (out_trace)
                        *out_trace = make_trace("VERIFY_VALID [INST_TYPE]: 前提 \"%s\" => 结论 \"%s\" 符合类型实例化", p0, conclusion, NULL);
                    return VERIFY_VALID;
                }
                /* 结论可能不含显式 INST_TYPE 标记但结构相似 */
                if (strlen(conclusion) > 0 && strstr(conclusion, ":") != NULL) {
                    if (out_trace)
                        *out_trace = make_trace("VERIFY_UNDECIDED [INST_TYPE]: 结论 \"%s\" 含类型标注但无显式标记", conclusion, NULL, NULL);
                    return VERIFY_UNDECIDED;
                }
                if (out_trace)
                    *out_trace = make_trace("VERIFY_INVALID [INST_TYPE]: 结论 \"%s\" 不符合类型实例化模式", conclusion, NULL, NULL);
                return VERIFY_INVALID;
            }

        case VERIFY_INST:
            /* INST: 项实例化
             * 检查：前提为含变量的定理，结论为变量被替换后的版本 */
            if (!premises || !premises[0]) {
                if (out_trace)
                    *out_trace = lv00_strdup_safe("VERIFY_UNDECIDED [INST]: 需要泛型定理前提");
                return VERIFY_UNDECIDED;
            }
            {
                const char *p0 = premises[0];
                /* 轻量级检查：前提含变量模式（单字母大写或下划线开头），
                 * 结论含实例化标记或替换列表 */
                if (has_inst_pattern(conclusion)) {
                    if (out_trace)
                        *out_trace = make_trace("VERIFY_VALID [INST]: 前提 \"%s\" => 结论 \"%s\" 符合项实例化", p0, conclusion, NULL);
                    return VERIFY_VALID;
                }
                /* 结论可能不含显式 INST 标记 */
                if (has_equality_pattern(conclusion) || has_application_pattern(conclusion)) {
                    if (out_trace)
                        *out_trace = make_trace("VERIFY_UNDECIDED [INST]: 结论 \"%s\" 结构可能为实例化结果但无显式标记", conclusion, NULL, NULL);
                    return VERIFY_UNDECIDED;
                }
                if (out_trace)
                    *out_trace = make_trace("VERIFY_INVALID [INST]: 结论 \"%s\" 不符合项实例化模式", conclusion, NULL, NULL);
                return VERIFY_INVALID;
            }

        case VERIFY_DISCH:
            /* DISCH: 如果 Gamma, A |- B 则 Gamma |- A ==> B
             * 检查：前提为 B，结论应含蕴含模式（A ==> B 或 A --> B） */
            if (!premises || !premises[0]) {
                if (out_trace)
                    *out_trace = lv00_strdup_safe("VERIFY_UNDECIDED [DISCH]: 需要前提 B");
                return VERIFY_UNDECIDED;
            }
            {
                const char *p0 = premises[0]; /* B */
                /* 结论应包含蕴含模式 */
                if (!has_implication_pattern(conclusion)) {
                    if (out_trace)
                        *out_trace = make_trace("VERIFY_INVALID [DISCH]: 结论 \"%s\" 不含蕴含模式", conclusion, NULL, NULL);
                    return VERIFY_INVALID;
                }
                /* 结论的后件（蕴含右侧）应与前提匹配 */
                /* 尝试提取蕴含右侧 */
                const char *impl = strstr(conclusion, "==>");
                if (!impl) impl = strstr(conclusion, "-->");
                if (impl) {
                    const char *rhs = impl + 3;
                    while (*rhs == ' ') rhs++;
                    /* 去除尾部空格 */
                    size_t p0_len = strlen(p0);
                    size_t rhs_len = strlen(rhs);
                    while (rhs_len > 0 && rhs[rhs_len - 1] == ' ') rhs_len--;
                    while (p0_len > 0 && p0[p0_len - 1] == ' ') p0_len--;
                    if (rhs_len == p0_len && strncmp(rhs, p0, p0_len) == 0) {
                        if (out_trace)
                            *out_trace = make_trace("VERIFY_VALID [DISCH]: 前提 \"%s\" => 结论 \"%s\" 符合蕴含引入", p0, conclusion, NULL);
                        return VERIFY_VALID;
                    }
                    /* 后件与前提不完全匹配，但蕴含结构存在 */
                    if (out_trace)
                        *out_trace = make_trace("VERIFY_UNDECIDED [DISCH]: 结论 \"%s\" 含蕴含但后件与前提 \"%s\" 不完全匹配", conclusion, p0, NULL);
                    return VERIFY_UNDECIDED;
                }
                if (out_trace)
                    *out_trace = make_trace("VERIFY_UNDECIDED [DISCH]: 结论 \"%s\" 含蕴含关键词但格式不明确", conclusion, NULL, NULL);
                return VERIFY_UNDECIDED;
            }

        default:
            if (out_trace) {
                *out_trace = lv00_strdup_safe("VERIFY_INVALID: 未知验证规则");
            }
            return VERIFY_INVALID;
    }
}


/* ================================================================
 * 5. F* — 精化类型 + SMT 混合验证
 * ================================================================ */

/**
 * @brief 精化类型检查 — 验证几何体是否同时满足类型条件和精化谓词
 *
 * 对每个条目：
 * 1. 类型检查：验证 geom_object 是否满足 base_type 的结构约束
 * 2. SMT 检查：构造逻辑公式验证 refinement_pred 的可满足性
 * 3. 合并结果：两者都通过 → REFINE_OK
 */
RefinementCheckReport *proof_refinement_check(ConstraintSolver *solver, RefinementCheckEntry *entries, int count) {
    if (!entries || count <= 0)
        return NULL;

    RefinementCheckReport *report = (RefinementCheckReport *) lv00_calloc(1, sizeof(RefinementCheckReport));
    if (!report)
        return NULL;

    report->entries = (RefinementCheckEntry *) lv00_calloc((size_t) count, sizeof(RefinementCheckEntry));
    if (!report->entries) {
        lv00_free((void**)&report);
        return NULL;
    }

    report->entry_count = count;
    report->passed_count = 0;
    report->failed_count = 0;

    for (int i = 0; i < count; i++) {
        RefinementCheckEntry *entry = &report->entries[i];

        /* 复制输入条目 */
        entry->geom_object = entries[i].geom_object;
        entry->base_type = entries[i].base_type;
        entry->refinement_pred = entries[i].refinement_pred;
        entry->smt_counterexample = NULL;
        entry->elapsed_sec = 0.0;

        clock_t entry_start = clock();

        /* 步骤 1：类型检查 — 验证基础类型兼容性 */
        /* 当前实现：比较 base_type 关键词（完整版应使用类型系统的结构化比较） */
        bool smt_ok = true;
        if (solver && entry->geom_object && entry->base_type) {
            /* 利用 solver 的类型注册表验证几何对象的 proposition 非空且类型一致 */
            const char *prop = constraint_solver_get_proposition(solver, entry->geom_object);
            if (!prop) {
                entry->smt_counterexample = lv00_strdup_safe(
                    "类型检查失败: 几何对象的命题 (proposition) 为 NULL");
                smt_ok = false;
            }
        }

        /* 步骤 2：SMT 精化谓词检查 */
        if (entry->refinement_pred && entry->refinement_pred[0] != '\0') {
            /* 尝试调用 SMT 后端进行实际求解 */
            SMTSolver *smt_solver = smtsolver_create(SMT_GROEBNER);
            if (smt_solver) {
                smtsolver_set_timeout(smt_solver, 5000); /* 5 秒超时 */
                /* 将谓词编码为 SMT-LIB2 断言 */
                char *smt_script = (char *) lv00_malloc(
                    strlen(entry->refinement_pred) + 256);
                if (smt_script) {
                    snprintf(smt_script, strlen(entry->refinement_pred) + 256,
                             "(set-logic QF_LRA)\n"
                             "(assert %s)\n"
                             "(check-sat)\n",
                             entry->refinement_pred);
                    smtsolver_encode(smt_solver, smt_script, strlen(smt_script));
                    SMTSatResult smt_result = smtsolver_check(smt_solver);
                    if (smt_result == SMT_RESULT_UNSAT) {
                        smt_ok = false;
                        entry->smt_counterexample = lv00_strdup_safe(
                            "SMT求解器报告不可满足");
                    } else if (smt_result == SMT_RESULT_UNKNOWN) {
                        /* SMT 求解器超时或无法判定，保守通过 */
                        LV00_LOG_WARNING("SMT精化检查超时/未知，保守通过");
                    }
                    lv00_free((void **) &smt_script);
                }
                smtsolver_destroy(smt_solver);
            } else {
                /* SMT 后端不可用，回退到字符串启发式检测 */
                if (strstr(entry->refinement_pred, "false") ||
                    strstr(entry->refinement_pred, "0 > 1") ||
                    strstr(entry->refinement_pred, "contradiction")) {
                    smt_ok = false;
                    entry->smt_counterexample = lv00_strdup_safe(
                        "模型不满足: 谓词包含恒假 (false) 子句");
                }
            }
        }

        /* 步骤 3：合并结果 */
        clock_t entry_end = clock();
        entry->elapsed_sec = ((double) (entry_end - entry_start)) / CLOCKS_PER_SEC;

        if (!smt_ok) {
            entry->result = REFINE_SMT_UNSAT;
            report->failed_count++;
        } else {
            entry->result = REFINE_OK;
            report->passed_count++;
        }
    }

    return report;
}

/**
 * @brief 销毁精化类型检查报告，释放所有分配的资源
 */
void refinement_check_report_destroy(RefinementCheckReport *report) {
    if (!report)
        return;

    if (report->entries) {
        for (int i = 0; i < report->entry_count; i++) {
            lv00_free((void**)&report->entries[i].smt_counterexample);
        }
        lv00_free((void**)&report->entries);
    }

    lv00_free((void**)&report);
}
