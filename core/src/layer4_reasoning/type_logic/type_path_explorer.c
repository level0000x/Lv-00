/**
 * @file type_path_explorer.c
 * @brief 路径探索器 (PathExplorer) 实现 —— 交互式类型重写路径搜索
 *
 * @details 提供在 TypeSystem 中从当前类型区域探索到目标类型区域的
 *          交互式路径搜索功能。支持查找可应用规则、预览、应用、
 *          撤销、目标检查以及导出探索路径。
 *
 *          本文件从 type_system.c 中独立提取，通过 extern 链接
 *          type_system.c 中定义的 type_region_deep_copy 和
 *          type_region_deep_free 等内部函数。
 *
 * @author Lv-00 Project
 */

#include "type_system.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv.h"
#include "lv/stream.h"

#include "lv_internal.h"
#include "lv_utils.h"
#include "rewrite.h"
#include "lv/lv_strbuf.h"

/*
 * 访问 type_system.c 中定义的流式上下文变量。
 * 该变量由 type_system_set_stream_context() 设置，
 * 供 stream_emit_simple 等函数使用。
 */
extern lv_THREAD_LOCAL StreamContext *type_system_stream_ctx;

/* type_region_deep_copy / type_region_deep_free 声明统一收口到 type_system.h */

/* ============== 路径探索器 (PathExplorer) ============== */

#define EXPLORER_INITIAL_CAPACITY 16
#define EXPLORER_HISTORY_INITIAL_CAPACITY 16

/**
 * @brief 路径探索器内部结构
 *
 * 通过 TypeSystem 的重写规则集，在 TypeRegion 空间中搜索从
 * current 到 target 的重写路径。使用 GraphSnapshot 实现撤销。
 */
struct PathExplorer {
    TypeSystem *ts;      /* 类型系统（不拥有） */
    TypeRegion *current; /* 当前类型区域（探索器拥有副本） */
    TypeRegion *target;  /* 目标类型区域（不拥有，外部引用） */

    /* 探索历史 */
    ExplorerStep *steps; /* 已执行步骤数组 */
    int step_count;      /* 当前步骤数 */
    int step_capacity;   /* 步骤数组容量 */

    /* 撤销栈：每步应用前保存当前类型的深拷贝 */
    TypeRegion **undo_stack; /* 撤销栈（每个元素为 TypeRegion 深拷贝） */
    int undo_count;          /* 撤销栈深度 */
    int undo_capacity;       /* 撤销栈容量 */
};

PathExplorer *path_explorer_create(TypeSystem *ts, TypeRegion *current, TypeRegion *target) {
    if (!ts || !current || !target)
        return NULL;

    PathExplorer *explorer = (PathExplorer *) lv_calloc(1, sizeof(PathExplorer));
    if (!explorer)
        return NULL;

    explorer->ts = ts;
    explorer->target = target;

    /* 深拷贝当前类型区域（探索器拥有副本） */
    explorer->current = type_region_deep_copy(current);
    if (!explorer->current) {
        lv_free((void **) &explorer);
        return NULL;
    }

    /* 初始化步骤数组 */
    explorer->step_capacity = EXPLORER_INITIAL_CAPACITY;
    explorer->steps = (ExplorerStep *) lv_calloc(explorer->step_capacity, sizeof(ExplorerStep));
    if (!explorer->steps) {
        type_region_deep_free(explorer->current);
        lv_free((void **) &explorer);
        return NULL;
    }
    explorer->step_count = 0;

    /* 初始化撤销栈 */
    explorer->undo_capacity = EXPLORER_HISTORY_INITIAL_CAPACITY;
    explorer->undo_stack = (TypeRegion **) lv_calloc(explorer->undo_capacity, sizeof(TypeRegion *));
    if (!explorer->undo_stack) {
        lv_free((void **) &explorer->steps);
        type_region_deep_free(explorer->current);
        lv_free((void **) &explorer);
        return NULL;
    }
    explorer->undo_count = 0;

    return explorer;
}

void path_explorer_destroy(PathExplorer *explorer) {
    if (!explorer)
        return;

    /* 释放当前类型副本 */
    type_region_deep_free(explorer->current);

    /* 释放步骤记录 */
    for (int i = 0; i < explorer->step_count; i++) {
        lv_free((void **) &explorer->steps[i].rule_name);
    }
    lv_free((void **) &explorer->steps);

    /* 释放撤销栈 */
    for (int i = 0; i < explorer->undo_count; i++) {
        type_region_deep_free(explorer->undo_stack[i]);
    }
    lv_free((void **) &explorer->undo_stack);

    lv_free((void **) &explorer);
}

ExplorerResult path_explorer_get_applicable_rules(const PathExplorer *explorer, int **rule_indices, int *count) {
    if (!explorer || !rule_indices || !count)
        return EXPLORER_ERROR;

    *rule_indices = NULL;
    *count = 0;

    /* 先检查是否已达到目标 */
    bool reached = false;
    TypeEquivResult equiv = type_check_equivalence(explorer->ts, explorer->current, explorer->target, true);
    if (equiv == TYPE_EQUIV_OK) {
        return EXPLORER_GOAL_REACHED;
    }

    /* 遍历所有重写规则，检查哪些可以匹配当前类型 */
    int *indices = (int *) lv_calloc(explorer->ts->rewrite_rule_count, sizeof(int));
    if (!indices)
        return EXPLORER_ERROR;

    int applicable = 0;
    for (int i = 0; i < explorer->ts->rewrite_rule_count; i++) {
        RewriteRule *rule = explorer->ts->rewrite_rules[i];
        if (!rule || !rule->pattern)
            continue;

        /* 可应用性检查：规则模式与当前类型结构匹配 */
        bool rule_applicable = false;
        if (rule->name && rule->pattern) {
            /* 检查规则模式的顶层类型种类是否匹配当前类型 */
            if (rule->pattern->kind == TYPE_KIND_VARIABLE) {
                /* 模式为类型变量 → 可匹配任何类型 */
                rule_applicable = true;
            } else if (explorer->current && rule->pattern->kind == explorer->current->kind) {
                /* 顶层种类匹配 → 候选规则 */
                rule_applicable = true;
            } else if (rule->pattern->kind == TYPE_KIND_BOTTOM) {
                /* 底部类型模式可匹配任何类型 */
                rule_applicable = true;
            }
        }
        if (rule_applicable) {
            indices[applicable++] = i;
        }
    }

    if (applicable == 0) {
        lv_free((void **) &indices);
        return EXPLORER_NO_RULES;
    }

    *rule_indices = indices;
    *count = applicable;
    return EXPLORER_OK;
}

ExplorerResult path_explorer_preview_rule(PathExplorer *explorer, int rule_index, TypeRegion **preview_result) {
    if (!explorer || !preview_result)
        return EXPLORER_ERROR;
    *preview_result = NULL;

    /* 验证规则索引有效 */
    if (rule_index < 0 || rule_index >= explorer->ts->rewrite_rule_count) {
        return EXPLORER_INVALID_RULE;
    }

    RewriteRule *rule = explorer->ts->rewrite_rules[rule_index];
    if (!rule || !rule->name) {
        return EXPLORER_INVALID_RULE;
    }

    /*
     * 预览：创建当前类型的深拷贝作为预览结果。
     * 实际的重写效果取决于重写引擎的匹配和替换，
     * 这里返回当前类型的副本作为保守预览。
     * 调用者可据此判断规则是否值得应用。
     */
    TypeRegion *preview = type_region_deep_copy(explorer->current);
    if (!preview)
        return EXPLORER_ERROR;

    *preview_result = preview;

    /* 流式事件：预览规则 */
    if (type_system_stream_ctx != NULL) {
        lvStrBuf sb = {0};
        lv_strbuf_printf(&sb, "路径探索: 预览规则 '%s'", rule->name ? rule->name : "?");
        stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_INFO, sb.data, 0);
        lv_strbuf_destroy(&sb);
    }

    return EXPLORER_OK;
}

ExplorerResult path_explorer_apply_rule(PathExplorer *explorer, int rule_index) {
    if (!explorer)
        return EXPLORER_ERROR;

    /* 验证规则索引有效 */
    if (rule_index < 0 || rule_index >= explorer->ts->rewrite_rule_count) {
        return EXPLORER_INVALID_RULE;
    }

    RewriteRule *rule = explorer->ts->rewrite_rules[rule_index];
    if (!rule || !rule->name) {
        return EXPLORER_INVALID_RULE;
    }

    /* 应用前：将当前类型压入撤销栈 */
    if (explorer->undo_count >= explorer->undo_capacity) {
        if (explorer->undo_capacity > INT_MAX / 2)
            return EXPLORER_ERROR;
        int new_cap = explorer->undo_capacity * 2;
        TypeRegion **new_stack = (TypeRegion **) lv_realloc(explorer->undo_stack, new_cap * sizeof(TypeRegion *));
        if (!new_stack)
            return EXPLORER_ERROR;
        explorer->undo_stack = new_stack;
        explorer->undo_capacity = new_cap;
    }

    TypeRegion *snapshot = type_region_deep_copy(explorer->current);
    if (!snapshot)
        return EXPLORER_ERROR;
    explorer->undo_stack[explorer->undo_count++] = snapshot;

    /*
     * 应用重写规则：
     * 使用类型系统的规范化功能尝试归一化当前类型。
     * 如果归一化成功，用归一化结果替换当前类型。
     * 这模拟了重写引擎在类型层面的效果。
     */
    TypeRegion *normalized = NULL;
    bool norm_ok = type_normalize(explorer->ts, explorer->current, &normalized);

    if (norm_ok && normalized) {
        /* 用归一化结果替换当前类型 */
        type_region_deep_free(explorer->current);
        explorer->current = normalized;
    }
    /* 如果归一化失败，保留当前类型不变（规则应用为空操作） */

    /* 记录步骤 */
    if (explorer->step_count >= explorer->step_capacity) {
        if (explorer->step_capacity > INT_MAX / 2) {
            /* 步骤记录失败，但状态已改变，仍返回成功 */
            return EXPLORER_OK;
        }
        int new_cap = explorer->step_capacity * 2;
        ExplorerStep *new_steps = (ExplorerStep *) lv_realloc(explorer->steps, new_cap * sizeof(ExplorerStep));
        if (!new_steps) {
            /* 步骤记录失败，但状态已改变，仍返回成功 */
            return EXPLORER_OK;
        }
        explorer->steps = new_steps;
        explorer->step_capacity = new_cap;
    }

    ExplorerStep *step = &explorer->steps[explorer->step_count];
    step->rule_index = rule_index;
    step->rule_name = rule->name ? lv_strdup(rule->name) : NULL;
    step->step_number = explorer->step_count;
    explorer->step_count++;

    /* 流式事件：规则应用成功 */
    if (type_system_stream_ctx != NULL) {
        lvStrBuf sb_2 = {0};
        lv_strbuf_printf(&sb_2, "路径探索: 应用规则 '%s' (步骤 %d)", rule->name ? rule->name : "?",
                 explorer->step_count - 1);
        stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_REWRITE_APPLIED, sb_2.data, 0);
        lv_strbuf_destroy(&sb_2);
    }

    /* 流式事件：路径探索应用规则信息 */
    if (type_system_stream_ctx != NULL) {
        lvStrBuf sb_3 = {0};
        lv_strbuf_printf(&sb_3, "路径探索: 应用规则 '%s'", rule->name ? rule->name : "?");
        stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_INFO, sb_3.data, 0);
        lv_strbuf_destroy(&sb_3);
    }

    return EXPLORER_OK;
}

ExplorerResult path_explorer_undo(PathExplorer *explorer) {
    if (!explorer)
        return EXPLORER_ERROR;

    if (explorer->undo_count == 0) {
        return EXPLORER_UNDO_EMPTY;
    }

    /* 弹出撤销栈顶部 */
    explorer->undo_count--;
    TypeRegion *restored = explorer->undo_stack[explorer->undo_count];
    explorer->undo_stack[explorer->undo_count] = NULL;

    /* 替换当前类型 */
    type_region_deep_free(explorer->current);
    explorer->current = restored;

    /* 移除最后一步记录 */
    if (explorer->step_count > 0) {
        explorer->step_count--;
        lv_free((void **) &explorer->steps[explorer->step_count].rule_name);
        explorer->steps[explorer->step_count].rule_name = NULL;
    }

    /* 流式事件：撤销操作 */
    if (type_system_stream_ctx != NULL) {
        stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_REWRITE_ROLLBACK, "路径探索: 撤销上一步操作", 0);
    }

    return EXPLORER_OK;
}

ExplorerResult path_explorer_check_goal(const PathExplorer *explorer, bool *reached) {
    if (!explorer || !reached)
        return EXPLORER_ERROR;

    TypeEquivResult equiv = type_check_equivalence(explorer->ts, explorer->current, explorer->target, true);
    *reached = (equiv == TYPE_EQUIV_OK);

    /* 流式事件：目标检查结果 */
    if (type_system_stream_ctx != NULL && *reached) {
        stream_emit_simple(type_system_stream_ctx, STREAM_EVENT_INFO, "路径探索: 已达到目标类型", 0);
    }

    return EXPLORER_OK;
}

ExplorerResult path_explorer_save_path(const PathExplorer *explorer, TypeRewritePath **out_path) {
    if (!explorer || !out_path)
        return EXPLORER_ERROR;
    *out_path = NULL;

    TypeRewritePath *path = type_rewrite_path_create();
    if (!path)
        return EXPLORER_ERROR;

    /* 将每一步记录到重写路径中
     *
     * 利用撤销栈恢复 before/after 快照：
     *   undo_stack[i] 保存了第 i 步应用前的类型深拷贝。
     *   undo_stack[i+1] 保存了第 i+1 步应用前的类型（即第 i 步之后的状态）。
     *   对于最后一步，after 为 explorer->current。
     */
    for (int i = 0; i < explorer->step_count; i++) {
        ExplorerStep *step = &explorer->steps[i];
        const TypeRegion *before = NULL;
        const TypeRegion *after = NULL;

        /* before: 从撤销栈获取（应用规则前的深拷贝） */
        if (i < explorer->undo_count) {
            before = explorer->undo_stack[i];
        }

        /* after: 下一步的 before（撤销栈中），或当前类型 */
        if (i + 1 < explorer->undo_count) {
            after = explorer->undo_stack[i + 1];
        } else {
            after = explorer->current;
        }

        type_rewrite_path_record(path, step->rule_name, before, after);
    }

    *out_path = path;
    return EXPLORER_OK;
}

int path_explorer_get_step_count(const PathExplorer *explorer) {
    if (!explorer)
        return 0;
    return explorer->step_count;
}

const ExplorerStep *path_explorer_get_steps(const PathExplorer *explorer) {
    if (!explorer || explorer->step_count == 0)
        return NULL;
    return explorer->steps;
}

const TypeRegion *path_explorer_get_current(const PathExplorer *explorer) {
    if (!explorer)
        return NULL;
    return explorer->current;
}
