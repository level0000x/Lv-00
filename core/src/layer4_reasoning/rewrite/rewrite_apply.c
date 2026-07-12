/**
 * @file rewrite_apply.c
 * @brief 规则加载与重写应用
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
#include "lv00/rewrite.h"
#include "lv00/constraint_graph.h"
#include "debug.h"
#include "lv00_internal.h"
#include "lv00_utils.h"
#include "mpz_poly.h"

LV00_DECLARE_STREAM_CTX(rewrite);

/* 解析后的重写规则结构体 */
typedef struct {
    char name[256];
    int priority;
    /* 模式变量节点 ID 列表 */
    int *pattern_var_ids;
    int pattern_var_count;
    /* 模式约束 */
    struct {
        ConstraintType type;
        int participant_count;
        int participants[8];
    } *pattern_constraints;
    int pattern_constraint_count;
    /* 替换约束 */
    struct {
        ConstraintType type;
        int participant_count;
        int participants[8];
    } *replacement_constraints;
    int replacement_constraint_count;
    /* 替换节点绑定 */
    struct {
        int pattern_var_id;
        int target_id;
    } *node_bindings;
    int node_binding_count;
    /* 新节点 */
    int *new_nodes;
    int new_node_count;
    GeomType *new_node_types;     /* 新节点的几何类型 */
} ParsedRule;

/* 解析约束类型字符串 */
static ConstraintType parse_constraint_type(const char *str) {
    if (strcmp(str, "incidence") == 0) return INCIDENCE;
    if (strcmp(str, "betweenness") == 0) return BETWEENNESS;
    if (strcmp(str, "intersection") == 0) return INTERSECTION;
    if (strcmp(str, "containment") == 0) return CONTAINMENT;
    if (strcmp(str, "connection") == 0) return CONNECTION;
    return INCIDENCE; /* 默认 */
}

/**
 * @brief 跳过空白字符
 *
 * @param p 输入字符串指针
 * @return 跳过空白后的指针
 */
static const char *skip_whitespace(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\r') p++;
    return p;
}

/**
 * @brief 跳过一行
 *
 * @param p 输入字符串指针
 * @return 跳过当前行后的指针
 */
static const char *skip_line(const char *p) {
    while (*p && *p != '\n') p++;
    if (*p == '\n') p++;
    return p;
}

/* 读取一个整数 token。
 * 支持可选的正负号前缀。如果数值超出 int 范围，将设置 *out 为
 * INT_MAX 或 INT_MIN 并继续解析（不会崩溃，但值可能不精确）。
 * 返回指向解析后下一个字符的指针。 */
static const char *read_int(const char *p, int *out) {
    p = skip_whitespace(p);
    *out = 0;
    int sign = 1;
    if (*p == '-') { sign = -1; p++; }
    while (*p >= '0' && *p <= '9') {
        int digit = *p - '0';
        /* 溢出检查：在乘法前判断 value * 10 是否会超出 INT_MAX/10 */
        if (*out > INT_MAX / 10 ||
            (*out == INT_MAX / 10 && digit > INT_MAX % 10)) {
            /* 整数溢出，钳位到最大/最小值 */
            *out = (sign > 0) ? INT_MAX : INT_MIN;
            /* 跳过剩余数字字符 */
            while (*p >= '0' && *p <= '9') p++;
            return p;
        }
        *out = *out * 10 + digit;
        p++;
    }
    *out *= sign;
    return p;
}

/* 前向声明 */
static void parsed_rule_destroy(ParsedRule *rule);

/* 读取一个字符串 token（到空白或行尾） */
static const char *read_token(const char *p, char *buf, int buf_size) {
    p = skip_whitespace(p);
    int i = 0;
    while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && i < buf_size - 1) {
        buf[i++] = *p++;
    }
    buf[i] = '\0';
    return p;
}

/**
 * @brief 从 .lvz 规则文件解析重写规则
 *
 * .lvz 文件格式支持以下指令：
 * - rule <name> <priority>：定义规则名称和优先级
 * - pattern_vars <v1> <v2> ...：定义模式变量
 * - pattern_constraint <type> <p1> [p2] [p3]：定义模式约束
 * - replacement_constraint <type> <p1> [p2] [p3]：定义替换约束
 * - node_binding <pattern_var> <target_id>：定义节点绑定
 * - new_nodes <id1> <id2> ...：定义新节点
 * - new_node_types <type1> <type2> ...：定义新节点类型
 *
 * @param filepath  规则文件路径
 * @param out_count 输出：解析得到的规则数量
 * @return 解析后的规则数组，失败返回 NULL
 */
static ParsedRule *parse_lvz_file(const char *filepath, int *out_count) {
    FILE *f = fopen(filepath, "r");
    if (!f) return NULL;

    /* 读取整个文件 */
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize <= 0) { fclose(f); return NULL; }

    char *content = lv00_malloc((size_t)fsize + 1);
    if (!content) { fclose(f); return NULL; }
    size_t nread = fread(content, 1, (size_t)fsize, f);
    content[nread] = '\0';
    fclose(f);

    /* 第一遍：计算规则数量 */
    int rule_count = 0;
    const char *p = content;
    while (*p) {
        if (*p == '#') { p = skip_line(p); continue; } /* 注释 */
        char token[64];
        p = read_token(p, token, sizeof(token));
        if (strcmp(token, "rule") == 0) {
            rule_count++;
        }
        p = skip_line(p);
    }

    if (rule_count == 0) { lv00_free((void**)&content); return NULL; }

    /* 分配规则数组 */
    ParsedRule *rules = lv00_malloc((size_t)rule_count * sizeof(ParsedRule));
    if (!rules) { lv00_free((void**)&content); return NULL; }
    memset(rules, 0, (size_t)rule_count * sizeof(ParsedRule));

    /* 第二遍：解析规则 */
    p = content;
    int current_rule = -1;
    while (*p) {
        if (*p == '#') { p = skip_line(p); continue; }
        if (*p == '\n') { p++; continue; }

        char token[256];
        p = read_token(p, token, sizeof(token));

        if (strcmp(token, "rule") == 0) {
            current_rule++;
            /* 读取规则名和优先级 */
            p = read_token(p, rules[current_rule].name, sizeof(rules[current_rule].name));
            p = read_int(p, &rules[current_rule].priority);
        } else if (current_rule >= 0) {
            if (strcmp(token, "pattern_vars") == 0) {
                /* pattern_vars: v1 v2 v3 ... */
                int count = 0;
                int vars[64];
                while (*p && *p != '\n') {
                    int v;
                    p = read_int(p, &v);
                    if (count < 64) vars[count++] = v;
                }
                rules[current_rule].pattern_var_ids = lv00_malloc((size_t)count * sizeof(int));
                if (rules[current_rule].pattern_var_ids) {
                    memcpy(rules[current_rule].pattern_var_ids, vars, (size_t)count * sizeof(int));
                    rules[current_rule].pattern_var_count = count;
                }
            } else if (strcmp(token, "pattern_constraint") == 0) {
                /* pattern_constraint: type p1 p2 [p3] */
                char type_str[32];
                p = read_token(p, type_str, sizeof(type_str));
                int parts[8];
                int pcount = 0;
                while (*p && *p != '\n') {
                    int v;
                    const char *next = read_int(p, &v);
                    if (next == p) break; /* 没有读到数字 */
                    p = next;
                    if (pcount < 8) parts[pcount++] = v;
                }
                int idx = rules[current_rule].pattern_constraint_count;
                void *new_pc = lv00_realloc(
                    rules[current_rule].pattern_constraints,
                    (size_t)(idx + 1) * sizeof(rules[current_rule].pattern_constraints[0]));
                if (!new_pc) {
                    for (int r = 0; r <= current_rule; r++) parsed_rule_destroy(&rules[r]);
                    lv00_free((void**)&rules);
                    lv00_free((void**)&content);
                    return NULL;
                }
                rules[current_rule].pattern_constraints = new_pc;
                rules[current_rule].pattern_constraints[idx].type = parse_constraint_type(type_str);
                rules[current_rule].pattern_constraints[idx].participant_count = pcount;
                memcpy(rules[current_rule].pattern_constraints[idx].participants, parts,
                       (size_t)pcount * sizeof(int));
                rules[current_rule].pattern_constraint_count++;
            } else if (strcmp(token, "replacement_constraint") == 0) {
                /* replacement_constraint: type p1 p2 [p3] */
                char type_str[32];
                p = read_token(p, type_str, sizeof(type_str));
                int parts[8];
                int pcount = 0;
                while (*p && *p != '\n') {
                    int v;
                    const char *next = read_int(p, &v);
                    if (next == p) break;
                    p = next;
                    if (pcount < 8) parts[pcount++] = v;
                }
                int idx = rules[current_rule].replacement_constraint_count;
                void *new_rc = lv00_realloc(
                    rules[current_rule].replacement_constraints,
                    (size_t)(idx + 1) * sizeof(rules[current_rule].replacement_constraints[0]));
                if (!new_rc) {
                    for (int r = 0; r <= current_rule; r++) parsed_rule_destroy(&rules[r]);
                    lv00_free((void**)&rules);
                    lv00_free((void**)&content);
                    return NULL;
                }
                rules[current_rule].replacement_constraints = new_rc;
                rules[current_rule].replacement_constraints[idx].type = parse_constraint_type(type_str);
                rules[current_rule].replacement_constraints[idx].participant_count = pcount;
                memcpy(rules[current_rule].replacement_constraints[idx].participants, parts,
                       (size_t)pcount * sizeof(int));
                rules[current_rule].replacement_constraint_count++;
            } else if (strcmp(token, "node_binding") == 0) {
                /* node_binding: pattern_var target_id */
                int var_id, target;
                p = read_int(p, &var_id);
                p = read_int(p, &target);
                int idx = rules[current_rule].node_binding_count;
                void *new_nb = lv00_realloc(
                    rules[current_rule].node_bindings,
                    (size_t)(idx + 1) * sizeof(rules[current_rule].node_bindings[0]));
                if (!new_nb) {
                    for (int r = 0; r <= current_rule; r++) parsed_rule_destroy(&rules[r]);
                    lv00_free((void**)&rules);
                    lv00_free((void**)&content);
                    return NULL;
                }
                rules[current_rule].node_bindings = new_nb;
                rules[current_rule].node_bindings[idx].pattern_var_id = var_id;
                rules[current_rule].node_bindings[idx].target_id = target;
                rules[current_rule].node_binding_count++;
            } else if (strcmp(token, "new_nodes") == 0) {
                /* new_nodes: id1 id2 ... */
                int nodes[64];
                int ncount = 0;
                while (*p && *p != '\n') {
                    int v;
                    const char *next = read_int(p, &v);
                    if (next == p) break;
                    p = next;
                    if (ncount < 64) nodes[ncount++] = v;
                }
                rules[current_rule].new_nodes = lv00_malloc((size_t)ncount * sizeof(int));
                if (rules[current_rule].new_nodes) {
                    memcpy(rules[current_rule].new_nodes, nodes, (size_t)ncount * sizeof(int));
                    rules[current_rule].new_node_count = ncount;
                }
            } else if (strcmp(token, "new_node_types") == 0) {
                /* new_node_types: type1 type2 ... (0=POINT, 1=LINE_SEGMENT, 2=REGION) */
                GeomType types[64];
                int tcount = 0;
                while (*p && *p != '\n') {
                    int v;
                    const char *next = read_int(p, &v);
                    if (next == p) break;
                    p = next;
                    if (tcount < 64) {
                        /* 验证类型值合法性 */
                        if (v >= GEOM_POINT && v <= GEOM_FUNCTION_BLOCK) {
                            types[tcount++] = (GeomType)v;
                        } else {
                            types[tcount++] = GEOM_POINT; /* 默认为 POINT */
                        }
                    }
                }
                rules[current_rule].new_node_types = lv00_malloc((size_t)tcount * sizeof(GeomType));
                if (rules[current_rule].new_node_types) {
                    memcpy(rules[current_rule].new_node_types, types, (size_t)tcount * sizeof(GeomType));
                }
            }
        }
        p = skip_line(p);
    }

    lv00_free((void**)&content);
    *out_count = rule_count;
    return rules;
}

/**
 * @brief 释放解析后的规则数据
 *
 * 释放 ParsedRule 中所有动态分配的资源：
 * pattern_var_ids、pattern_constraints、replacement_constraints、
 * node_bindings 和 new_nodes 数组。
 *
 * @param rule 待销毁的解析规则指针（可为 NULL）
 */
static void parsed_rule_destroy(ParsedRule *rule) {
    if (!rule) return;
    lv00_free((void**)&rule->pattern_var_ids);
    lv00_free((void**)&rule->pattern_constraints);
    lv00_free((void**)&rule->replacement_constraints);
    lv00_free((void**)&rule->node_bindings);
    lv00_free((void**)&rule->new_nodes);
    lv00_free((void**)&rule->new_node_types);
}

/**
 * @brief 将 ParsedRule 转换为 RewriteRule
 *
 * 从解析后的规则数据构建完整的 RewriteRule 结构体：
 * - 构建 RewritePattern（变量 ID 列表和模式约束）
 * - 构建 RewriteReplacement（替换约束和节点绑定）
 * 分配失败时进行回滚清理，返回 NULL。
 *
 * @param pr 解析后的规则数据（只读）
 * @return 新分配的 RewriteRule 指针，失败返回 NULL
 */
static RewriteRule *parsed_rule_to_rewrite_rule(const ParsedRule *pr) {
    /* 构建模式 */
    RewritePattern *pattern = lv00_malloc(sizeof(RewritePattern));
    if (!pattern) return NULL;
    pattern->var_count = pr->pattern_var_count;
    pattern->variable_node_ids = NULL;
    if (pr->pattern_var_count > 0 && pr->pattern_var_ids) {
        pattern->variable_node_ids = lv00_malloc((size_t)pr->pattern_var_count * sizeof(int));
        if (pattern->variable_node_ids) {
            memcpy(pattern->variable_node_ids, pr->pattern_var_ids,
                   (size_t)pr->pattern_var_count * sizeof(int));
        }
    }

    /* 构建模式约束 */
    pattern->pattern_constraint_count = pr->pattern_constraint_count;
    pattern->pattern_constraints = NULL;
    if (pr->pattern_constraint_count > 0 && pr->pattern_constraints) {
        pattern->pattern_constraints = lv00_malloc(
            (size_t)pr->pattern_constraint_count * sizeof(Constraint *));
        if (pattern->pattern_constraints) {
            for (int i = 0; i < pr->pattern_constraint_count; i++) {
                Constraint *c = lv00_malloc(sizeof(Constraint));
                if (c) {
                    memset(c, 0, sizeof(Constraint));
                    c->type = pr->pattern_constraints[i].type;
                    c->participant_count = pr->pattern_constraints[i].participant_count;
                    c->participants = lv00_malloc(
                        (size_t)c->participant_count * sizeof(int));
                    if (c->participants) {
                        memcpy(c->participants,
                               pr->pattern_constraints[i].participants,
                               (size_t)c->participant_count * sizeof(int));
                    }
                }
                pattern->pattern_constraints[i] = c;
            }
        }
    }

    /* 构建替换 */
    RewriteReplacement *replacement = lv00_malloc(sizeof(RewriteReplacement));
    if (!replacement) {
        /* 简化清理 */
        lv00_free((void**)&pattern->variable_node_ids);
        if (pattern->pattern_constraints) {
            for (int i = 0; i < pattern->pattern_constraint_count; i++) {
                if (pattern->pattern_constraints[i]) {
                    lv00_free((void**)&pattern->pattern_constraints[i]->participants);
                    lv00_free((void**)&pattern->pattern_constraints[i]);
                }
            }
            lv00_free((void**)&pattern->pattern_constraints);
        }
        lv00_free((void**)&pattern);
        return NULL;
    }

    /* 替换节点绑定 */
    replacement->binding_count = pr->node_binding_count;
    replacement->node_bindings = NULL;
    if (pr->node_binding_count > 0 && pr->node_bindings) {
        replacement->node_bindings = lv00_malloc(
            (size_t)pr->node_binding_count * sizeof(int *));
        if (replacement->node_bindings) {
            for (int i = 0; i < pr->node_binding_count; i++) {
                replacement->node_bindings[i] = lv00_malloc(2 * sizeof(int));
                if (replacement->node_bindings[i]) {
                    replacement->node_bindings[i][0] = pr->node_bindings[i].pattern_var_id;
                    replacement->node_bindings[i][1] = pr->node_bindings[i].target_id;
                }
            }
        }
    }

    /* 替换约束 */
    replacement->replacement_constraint_count = pr->replacement_constraint_count;
    replacement->replacement_constraints = NULL;
    if (pr->replacement_constraint_count > 0 && pr->replacement_constraints) {
        replacement->replacement_constraints = lv00_malloc(
            (size_t)pr->replacement_constraint_count * sizeof(Constraint *));
        if (replacement->replacement_constraints) {
            for (int i = 0; i < pr->replacement_constraint_count; i++) {
                Constraint *c = lv00_malloc(sizeof(Constraint));
                if (c) {
                    memset(c, 0, sizeof(Constraint));
                    c->type = pr->replacement_constraints[i].type;
                    c->participant_count = pr->replacement_constraints[i].participant_count;
                    c->participants = lv00_malloc(
                        (size_t)c->participant_count * sizeof(int));
                    if (c->participants) {
                        memcpy(c->participants,
                               pr->replacement_constraints[i].participants,
                               (size_t)c->participant_count * sizeof(int));
                    }
                }
                replacement->replacement_constraints[i] = c;
            }
        }
    }

    /* 新节点 */
    replacement->new_node_count = pr->new_node_count;
    replacement->new_nodes = NULL;
    replacement->new_node_types = NULL;
    if (pr->new_node_count > 0 && pr->new_nodes) {
        replacement->new_nodes = lv00_malloc((size_t)pr->new_node_count * sizeof(int));
        if (replacement->new_nodes) {
            memcpy(replacement->new_nodes, pr->new_nodes,
                   (size_t)pr->new_node_count * sizeof(int));
        }
    }
    /* 新节点类型 */
    if (pr->new_node_count > 0 && pr->new_node_types) {
        replacement->new_node_types = lv00_malloc((size_t)pr->new_node_count * sizeof(GeomType));
        if (replacement->new_node_types) {
            memcpy(replacement->new_node_types, pr->new_node_types,
                   (size_t)pr->new_node_count * sizeof(GeomType));
        }
    }

    RewriteRule *rule = rewrite_rule_create(pr->name, pattern, replacement, pr->priority);
    return rule;
}

int rewrite_rules_load_from_file(const char *filepath,
                                  RewriteRule ***out_rules,
                                  int *out_count)
{
    if (!filepath || !out_rules || !out_count) return -1;

    *out_rules = NULL;
    *out_count = 0;

    int parsed_count = 0;
    ParsedRule *parsed = parse_lvz_file(filepath, &parsed_count);
    if (!parsed || parsed_count <= 0) {
        if (parsed) lv00_free((void**)&parsed);
        return -1;
    }

    RewriteRule **rules = lv00_malloc((size_t)parsed_count * sizeof(RewriteRule *));
    if (!rules) {
        for (int i = 0; i < parsed_count; i++) parsed_rule_destroy(&parsed[i]);
        lv00_free((void**)&parsed);
        return -1;
    }
    memset(rules, 0, (size_t)parsed_count * sizeof(RewriteRule *));

    int loaded = 0;
    for (int i = 0; i < parsed_count; i++) {
        rules[loaded] = parsed_rule_to_rewrite_rule(&parsed[i]);
        if (rules[loaded]) {
            loaded++;
            if (rewrite_stream_ctx) {
                stream_emit_simple(rewrite_stream_ctx, STREAM_EVENT_REWRITE_RULE_LOADED,
                                   rules[loaded - 1]->name ? rules[loaded - 1]->name : "(unnamed)", 0);
            }
        }
        parsed_rule_destroy(&parsed[i]);
    }
    lv00_free((void**)&parsed);

    if (loaded == 0) {
        lv00_free((void**)&rules);
        return 0;
    }

    /* 压缩数组 */
    if (loaded < parsed_count) {
        RewriteRule **compressed = lv00_realloc(rules, (size_t)loaded * sizeof(RewriteRule *));
        if (compressed) rules = compressed;
    }

    *out_rules = rules;
    *out_count = loaded;
    return loaded;
}

bool rewrite_rule_unload(RewriteRule ***rules, int *count,
                          const char *rule_name)
{
    if (!rules || !*rules || !count || !rule_name) return false;

    int found_idx = -1;
    for (int i = 0; i < *count; i++) {
        if ((*rules)[i] && (*rules)[i]->name &&
            strcmp((*rules)[i]->name, rule_name) == 0) {
            found_idx = i;
            break;
        }
    }

    if (found_idx < 0) return false;

    /* 销毁该规则 */
    RewriteRule *rule = (*rules)[found_idx];
    if (rule) {
        /* 销毁模式 */
        if (rule->pattern) {
            lv00_free((void**)&rule->pattern->variable_node_ids);
            if (rule->pattern->pattern_constraints) {
                for (int i = 0; i < rule->pattern->pattern_constraint_count; i++) {
                    if (rule->pattern->pattern_constraints[i]) {
                        lv00_free((void**)&rule->pattern->pattern_constraints[i]->participants);
                        lv00_free((void**)&rule->pattern->pattern_constraints[i]);
                    }
                }
                lv00_free((void**)&rule->pattern->pattern_constraints);
            }
            lv00_free((void**)&rule->pattern);
        }
        /* 销毁替换 */
        if (rule->replacement) {
            if (rule->replacement->node_bindings) {
                for (int i = 0; i < rule->replacement->binding_count; i++) {
                    lv00_free((void**)&rule->replacement->node_bindings[i]);
                }
                lv00_free((void**)&rule->replacement->node_bindings);
            }
            if (rule->replacement->replacement_constraints) {
                for (int i = 0; i < rule->replacement->replacement_constraint_count; i++) {
                    if (rule->replacement->replacement_constraints[i]) {
                        lv00_free((void**)&rule->replacement->replacement_constraints[i]->participants);
                        lv00_free((void**)&rule->replacement->replacement_constraints[i]);
                    }
                }
                lv00_free((void**)&rule->replacement->replacement_constraints);
            }
            lv00_free((void**)&rule->replacement->new_nodes);
            lv00_free((void**)&rule->replacement);
        }
        lv00_free((void**)&rule->name);
        lv00_free((void**)&rule);
    }

    /* 从数组中移除并压缩 */
    for (int i = found_idx; i < *count - 1; i++) {
        (*rules)[i] = (*rules)[i + 1];
    }
    (*count)--;

    /* 缩小数组 */
    if (*count > 0) {
        RewriteRule **compressed = lv00_realloc(*rules, (size_t)*count * sizeof(RewriteRule *));
        if (compressed) *rules = compressed;
    } else {
        lv00_free((void**)&*rules);
        *rules = NULL;
    }

    if (rewrite_stream_ctx) {
        stream_emit_simple(rewrite_stream_ctx, STREAM_EVENT_INFO,
                           rule_name, 0);
    }

    return true;
}

/* ---- apply_rewrite (THE MAIN IMPLEMENTATION) ---- */

RewriteStatus apply_rewrite(ConstraintGraph *graph, RewriteRule *rule, RewriteMatch *match) {
    if (!graph || !rule || !match || !rule->replacement) {
        return REWRITE_NO_MATCH;
    }
    if (rule->reduction_measure < 0) {
        return REWRITE_NO_MATCH;
    }

    /* ================================================================
     * GRAPH SNAPSHOT — 用于替换操作的真正事务性回滚
     * 在执行任何修改前创建图的深拷贝快照。
     * 如果替换后检测到冲突，使用快照完整恢复图状态。
     * ================================================================ */
    GraphSnapshot *snapshot = graph_snapshot_create(graph);
    if (!snapshot) {
        return REWRITE_NO_MATCH;
    }

    const RewriteReplacement *repl = rule->replacement;
    const RewritePattern *pat = rule->pattern;

    /* ================================================================
     * TRANSACTION LOG
     * We record every mutation so we can roll back on failure.
     * ================================================================ */
    struct TxnEntry {
        enum { TXN_ADD_NODE, TXN_ADD_CONSTRAINT, TXN_REMOVE_NODE, TXN_REMOVE_CONSTRAINT } kind;
        int id;              /* node or constraint id */
        ConstraintType ctype; /* for added constraints */
        int *participants;   /* copy of participant array for added constraints */
        int participant_count;
    };

    int txn_cap = 64;
    int txn_count = 0;
    struct TxnEntry *txn = lv00_malloc((size_t)txn_cap * sizeof(struct TxnEntry));
    if (!txn) {
        graph_snapshot_destroy(snapshot);
        return REWRITE_NO_MATCH;
    }

    #define TXN_PUSH(entry) do { \
        if (txn_count >= txn_cap) { \
            txn_cap *= 2; \
            struct TxnEntry *_tmp = lv00_realloc(txn, (size_t)txn_cap * sizeof(struct TxnEntry)); \
            if (!_tmp) goto txn_rollback; \
            txn = _tmp; \
        } \
        txn[txn_count++] = (entry); \
    } while(0)

    RewriteStatus result = REWRITE_NO_MATCH;

    /* ----------------------------------------------------------------
     * Step a: Create new nodes from replacement.new_nodes
     *
     * new_nodes[i] is a placeholder id.  We create a real node in the
     * graph and record the mapping from placeholder -> actual id.
     *
     * If replacement specifies new_node_types[i], use that type.
     * Otherwise, infer from replacement constraint context:
     *   - If the new node participates in a constraint that implies
     *     LINE_SEGMENT endpoints, create POINT nodes.
     *   - Default to GEOM_POINT for backward compatibility.
     *
     * Supported types: GEOM_POINT, GEOM_LINE_SEGMENT, GEOM_REGION.
     * ---------------------------------------------------------------- */
    int *new_node_map = NULL;
    if (repl->new_node_count > 0) {
        new_node_map = lv00_malloc((size_t)repl->new_node_count * sizeof(int));
        if (!new_node_map) goto txn_cleanup;

        for (int i = 0; i < repl->new_node_count; i++) {
            GeomType node_type = GEOM_POINT; /* 默认类型 */

            /* 优先使用规则中显式指定的类型 */
            if (repl->new_node_types && i < repl->new_node_count) {
                node_type = repl->new_node_types[i];
            } else {
                /* 推断类型：扫描替换约束，检查新节点参与的约束类型 */
                int placeholder_id = repl->new_nodes[i];
                for (int c = 0; c < repl->replacement_constraint_count; c++) {
                    Constraint *rc = repl->replacement_constraints[c];
                    bool involves_new_node = false;
                    for (int p = 0; p < rc->participant_count; p++) {
                        if (rc->participants[p] == placeholder_id) {
                            involves_new_node = true;
                            break;
                        }
                    }
                    if (involves_new_node) {
                        /* 如果约束类型是 INCIDENCE 且参与者数量为 2，
                           新节点可能是线段端点 -> 保持 POINT */
                        if (rc->type == INCIDENCE && rc->participant_count == 2) {
                            node_type = GEOM_POINT;
                        }
                        /* 如果约束类型暗示线段参与，且新节点是线段本身 */
                        if (rc->type == BETWEENNESS && rc->participant_count == 3) {
                            /* BETWEENNESS 的三个参与者可能是 (p1, p2, p3)，
                               如果新节点不是端点，保持 POINT */
                        }
                    }
                }
            }

            AddNodeResult nr = ADD_NODE_OK;
            int actual_id = -1;

            switch (node_type) {
            case GEOM_LINE_SEGMENT: {
                /* 创建线段需要两个端点。如果替换约束中有 INCIDENCE
                   关联到此线段的端点，使用已解析的端点 ID。
                   否则创建两个占位点作为端点。 */
                int ep1_id = -1, ep2_id = -1;
                int placeholder_id = repl->new_nodes[i];

                /* 尝试从替换约束中找到关联的端点 */
                for (int c = 0; c < repl->replacement_constraint_count && ep1_id < 0; c++) {
                    Constraint *rc = repl->replacement_constraints[c];
                    if (rc->type == INCIDENCE && rc->participant_count == 2) {
                        for (int p = 0; p < rc->participant_count; p++) {
                            if (rc->participants[p] == placeholder_id) {
                                int other_idx = 1 - p;
                                int other_id = rc->participants[other_idx];
                                if (other_id < 0) {
                                    /* 模式变量 -> 查找匹配绑定 */
                                    other_id = resolve_binding(
                                        match->node_bindings, match->binding_count, other_id);
                                } else if (other_id != placeholder_id) {
                                    /* 检查是否是另一个新节点 */
                                    bool is_other_new = false;
                                    for (int nn = 0; nn < i; nn++) {
                                        if (repl->new_nodes[nn] == other_id) {
                                            other_id = new_node_map[nn];
                                            is_other_new = true;
                                            break;
                                        }
                                    }
                                }
                                if (ep1_id < 0) ep1_id = other_id;
                                else if (ep2_id < 0) ep2_id = other_id;
                            }
                        }
                    }
                }

                /* 如果没有找到端点，创建占位点 */
                if (ep1_id < 0) {
                    SymbolicCoord *zc = symbolic_coord_create_rational(0, 1);
                    SymbolicCoord *coords[] = { zc };
                    nr = graph_add_point(graph, coords, 1);
                    symbolic_coord_destroy(zc);
                    if (nr != ADD_NODE_OK) goto txn_rollback;
                    ep1_id = graph->next_node_id - 1;
                    struct TxnEntry ep_e;
                    ep_e.kind = TXN_ADD_NODE;
                    ep_e.id = ep1_id;
                    ep_e.participants = NULL;
                    ep_e.participant_count = 0;
                    TXN_PUSH(ep_e);
                }
                if (ep2_id < 0) {
                    SymbolicCoord *zc = symbolic_coord_create_rational(0, 1);
                    SymbolicCoord *coords[] = { zc };
                    nr = graph_add_point(graph, coords, 1);
                    symbolic_coord_destroy(zc);
                    if (nr != ADD_NODE_OK) goto txn_rollback;
                    ep2_id = graph->next_node_id - 1;
                    struct TxnEntry ep_e;
                    ep_e.kind = TXN_ADD_NODE;
                    ep_e.id = ep2_id;
                    ep_e.participants = NULL;
                    ep_e.participant_count = 0;
                    TXN_PUSH(ep_e);
                }

                nr = graph_add_line_segment(graph, ep1_id, ep2_id);
                if (nr != ADD_NODE_OK) goto txn_rollback;
                actual_id = graph->next_node_id - 1;
                break;
            }

            case GEOM_REGION: {
                /* 创建区域需要边界线段 ID。
                   尝试从替换约束中找到 CONTAINMENT 关联的线段。 */
                int seg_ids[64];
                int seg_count = 0;
                int placeholder_id = repl->new_nodes[i];

                for (int c = 0; c < repl->replacement_constraint_count && seg_count < 64; c++) {
                    Constraint *rc = repl->replacement_constraints[c];
                    if (rc->type == CONTAINMENT && rc->participant_count == 2) {
                        for (int p = 0; p < rc->participant_count; p++) {
                            if (rc->participants[p] == placeholder_id) {
                                int other_idx = 1 - p;
                                int other_id = rc->participants[other_idx];
                                if (other_id < 0) {
                                    other_id = resolve_binding(
                                        match->node_bindings, match->binding_count, other_id);
                                } else {
                                    /* 检查是否是另一个新节点 */
                                    for (int nn = 0; nn < i; nn++) {
                                        if (repl->new_nodes[nn] == other_id) {
                                            other_id = new_node_map[nn];
                                            break;
                                        }
                                    }
                                }
                                if (other_id >= 0) {
                                    seg_ids[seg_count++] = other_id;
                                }
                            }
                        }
                    }
                }

                /* 如果没有找到边界线段，创建一个空区域（使用空数组） */
                nr = graph_add_region(graph, seg_ids, seg_count);
                if (nr != ADD_NODE_OK) goto txn_rollback;
                actual_id = graph->next_node_id - 1;
                break;
            }

            case GEOM_POINT:
            default: {
                /* 创建 POINT 节点（原有逻辑） */
                SymbolicCoord *zero_coord = symbolic_coord_create_rational(0, 1);
                SymbolicCoord *coords[] = { zero_coord };
                nr = graph_add_point(graph, coords, 1);
                symbolic_coord_destroy(zero_coord);

                if (nr != ADD_NODE_OK) {
                    goto txn_rollback;
                }
                actual_id = graph->next_node_id - 1;
                break;
            }
            }

            if (nr != ADD_NODE_OK) {
                goto txn_rollback;
            }

            new_node_map[i] = actual_id;

            struct TxnEntry e;
            e.kind = TXN_ADD_NODE;
            e.id = actual_id;
            e.participants = NULL;
            e.participant_count = 0;
            TXN_PUSH(e);
        }
    }

    /* ----------------------------------------------------------------
     * Step b: Create new constraints from replacement.replacement_constraints
     *
     * For each replacement constraint, resolve every participant id:
     *   - negative id  -> pattern variable -> look up in match bindings
     *   - new node id  -> look up in new_node_map
     *   - positive id not in pattern -> external node, keep as-is
     * ---------------------------------------------------------------- */
    for (int c = 0; c < repl->replacement_constraint_count; c++) {
        Constraint *rc = repl->replacement_constraints[c];
        int *resolved = lv00_malloc((size_t)rc->participant_count * sizeof(int));
        if (!resolved) goto txn_rollback;

        bool all_ok = true;
        for (int p = 0; p < rc->participant_count; p++) {
            int rid = resolve_replacement_participant(
                rc->participants[p],
                match->node_bindings, match->binding_count,
                new_node_map, repl->new_node_count,
                repl->new_nodes, repl->new_node_count);
            if (rid < 0) {
                all_ok = false;
                break;
            }
            resolved[p] = rid;
        }

        if (!all_ok) {
            lv00_free((void**)&resolved);
            goto txn_rollback;
        }

        /* 验证所有引用的节点确实存在 */
        for (int p = 0; p < rc->participant_count; p++) {
            if (!graph_get_node(graph, resolved[p])) {
                lv00_free((void**)&resolved);
                goto txn_rollback;
            }
        }

        bool added = add_constraint_generic(graph, rc->type, resolved, rc->participant_count);
        if (!added) {
            lv00_free((void**)&resolved);
            goto txn_rollback;
        }

        /* 记录添加的约束，以便可能回滚 */
        int new_con_id = graph->next_constraint_id - 1;
        struct TxnEntry e;
        e.kind = TXN_ADD_CONSTRAINT;
        e.id = new_con_id;
        e.ctype = rc->type;
        e.participants = resolved;
        e.participant_count = rc->participant_count;
        TXN_PUSH(e);
    }

    /* ----------------------------------------------------------------
     * Step c: Remove old matched constraints
     *
     * Remove every constraint that was matched by the pattern.
     * ---------------------------------------------------------------- */
    for (int i = 0; i < match->binding_count; i++) {
        int con_id = match->constraint_bindings[i];
        if (graph_get_constraint(graph, con_id)) {
            struct TxnEntry e;
            e.kind = TXN_REMOVE_CONSTRAINT;
            e.id = con_id;
            e.participants = NULL;
            e.participant_count = 0;
            TXN_PUSH(e);

            if (graph_remove_constraint(graph, con_id) != REMOVE_CONSTRAINT_OK) {
                goto txn_rollback;
            }
        }
    }

    /* ----------------------------------------------------------------
     * Step d: Remove old matched pattern nodes that are NOT referenced
     * by the replacement.
     *
     * A matched node (bound to a negative pattern var id) should be
     * removed if:
     *   1. It does NOT appear in any replacement constraint, AND
     *   2. It does NOT appear in the replacement node_bindings
     * ---------------------------------------------------------------- */
    for (int i = 0; i < match->binding_count; i++) {
        int pattern_var_id = match->node_bindings[i * 2];
        int graph_node_id  = match->node_bindings[i * 2 + 1];

        /* 只考虑模式变量（负 ID） */
        if (pattern_var_id >= 0) continue;

        /* 检查替换结果是否仍需要此节点 */
        bool used = pattern_var_used_in_replacement(repl, pattern_var_id);
        if (used) continue;

        /* 检查替换结果是否重新绑定了此变量 */
        bool rebound = pattern_var_in_replacement_bindings(repl, pattern_var_id);
        if (rebound) continue;

        /* 还需检查：此节点是否被任何未匹配的约束引用？
           如果是，删除它会破坏这些约束。
           graph_remove_node 函数已处理移除引用，但我们应仅在
           该节点没有剩余约束引用时才删除它。 */
        bool has_external_refs = false;
        for (int c = 0; c < graph->constraint_count; c++) {
            Constraint *con = graph->constraints[c];
            if (is_matched_constraint(match, con->id)) continue;
            for (int p = 0; p < con->participant_count; p++) {
                if (con->participants[p] == graph_node_id) {
                    has_external_refs = true;
                    break;
                }
            }
            if (has_external_refs) break;
        }
        if (has_external_refs) continue;

        /* 可以安全移除 */
        GeomNode *node = graph_get_node(graph, graph_node_id);
        if (node && node->type != GEOM_REGION) {
            struct TxnEntry e;
            e.kind = TXN_REMOVE_NODE;
            e.id = graph_node_id;
            e.participants = NULL;
            e.participant_count = 0;
            TXN_PUSH(e);

            if (graph_remove_node(graph, graph_node_id) != REMOVE_NODE_OK) {
                goto txn_rollback;
            }
        }
    }

    /* ----------------------------------------------------------------
     * Step e: Boundary reconnection is implicit.
     *
     * Any replacement constraint that references external nodes (positive
     * ids not in the pattern) already keeps those references because
     * resolve_replacement_participant passes them through unchanged.
     * No additional work is needed here.
     * ---------------------------------------------------------------- */

    result = REWRITE_APPLIED;

    /* 验证约束图一致性：若产生冲突，使用快照回滚 */
    if (!check_graph_consistency(graph)) {
        graph_snapshot_restore(snapshot, graph);
        graph_snapshot_destroy(snapshot);
        result = REWRITE_CONFLUENCE_ISSUE;
        goto txn_cleanup;
    }

    goto txn_cleanup;

txn_rollback:
    /* 使用图快照进行真正的回滚，替代原来的逐操作撤销 */
    graph_snapshot_restore(snapshot, graph);
    result = REWRITE_NO_MATCH;

txn_cleanup:
    /* 销毁快照（成功路径或回滚路径都需要销毁） */
    graph_snapshot_destroy(snapshot);

    for (int i = 0; i < txn_count; i++) {
        if (txn[i].kind == TXN_ADD_CONSTRAINT && txn[i].participants) {
            lv00_free((void**)&txn[i].participants);
        }
    }
    lv00_free((void**)&txn);
    lv00_free((void**)&new_node_map);

    #undef TXN_PUSH

    return result;
}

/**
 * @brief qsort 比较函数：按 reduction_measure 降序排序，
 *        相同则按注册顺序（原始索引）升序排列
 *
 * @param a 规则指针 a
 * @param b 规则指针 b
 * @return 比较结果
 */
typedef struct {
    RewriteRule *rule;
    int original_index;
} SortedRule;

static int sorted_rule_cmp(const void *a, const void *b) {
    const SortedRule *sa = (const SortedRule *)a;
    const SortedRule *sb = (const SortedRule *)b;
    if (sa->rule->reduction_measure != sb->rule->reduction_measure) {
        /* 度量值高的优先 */
        return (sb->rule->reduction_measure > sa->rule->reduction_measure) ? 1 : -1;
    }
    /* 相同度量值：保持注册顺序 */
    return (sa->original_index - sb->original_index);
}

RewriteStatus rewrite_with_rules(ConstraintGraph *graph, RewriteRule **rules,
                                 int rule_count, int step_limit,
                                 bool normalize_between_steps)
{
    if (rule_count <= 0) return REWRITE_OK;

    if (rewrite_stream_ctx) {
        stream_emit_simple(rewrite_stream_ctx, STREAM_EVENT_REWRITE_START,
                           "rewrite phase started", 0);
    }

    /* 按规则优先级排序 */
    SortedRule *sorted = lv00_malloc((size_t)rule_count * sizeof(SortedRule));
    if (!sorted) return REWRITE_TERMINATED;
    for (int i = 0; i < rule_count; i++) {
        sorted[i].rule = rules[i];
        sorted[i].original_index = i;
    }
    qsort(sorted, (size_t)rule_count, sizeof(SortedRule), sorted_rule_cmp);

    int steps = 0;
    int *history_hashes = lv00_malloc((size_t)step_limit * sizeof(uint32_t));
    if (!history_hashes) { lv00_free((void**)&sorted); return REWRITE_TERMINATED; }
    int history_count = 0;

    RewriteStatus final_status = REWRITE_OK;

    while (steps < step_limit) {
        /* 通过图哈希检测重写循环 */
        uint32_t current_hash = compute_graph_hash(graph);
        bool loop_detected = false;
        for (int i = 0; i < history_count; i++) {
            if (history_hashes[i] == current_hash) {
                loop_detected = true;
                break;
            }
        }
        if (loop_detected) {
            if (rewrite_stream_ctx) {
                stream_emit_simple(rewrite_stream_ctx, STREAM_EVENT_ERROR,
                                   "rewrite loop detected, terminating", steps);
            }
            final_status = REWRITE_TERMINATED;
            break;
        }
        if (history_count < step_limit) {
            history_hashes[history_count++] = current_hash;
        }

        /* 按优先级依次尝试每条规则 */
        bool applied = false;
        for (int i = 0; i < rule_count; i++) {
            RewriteRule *rule = sorted[i].rule;
            RewriteMatch *match = find_rewrite_match(graph, rule, false);
            if (!match) continue;

            if (rewrite_stream_ctx) {
                stream_emit_simple(rewrite_stream_ctx, STREAM_EVENT_REWRITE_MATCH_FOUND,
                                   rule->name ? rule->name : "rule matched", steps);
            }

            RewriteStatus status = apply_rewrite(graph, rule, match);
            if (status == REWRITE_APPLIED) {
                if (rewrite_stream_ctx) {
                    stream_emit_simple(rewrite_stream_ctx, STREAM_EVENT_REWRITE_APPLIED,
                                       rule->name ? rule->name : "rule applied", steps);
                }
                /* apply_rewrite 内部已通过快照机制处理冲突检测和回滚，
                 * 返回 REWRITE_APPLIED 表示替换成功且图一致。 */

                /* 根据 design_v2.9.md Section 6.4，可在重写步骤之间
                 * 选择性进行规范化，以防止冗余节点干扰后续匹配。 */
                if (normalize_between_steps) {
                    NormalizationResult *norm_result = graph_normalize(graph, false);
                    if (norm_result) normalization_result_destroy(norm_result);
                }

                applied = true;
                lv00_free((void**)&match);
                break;
            } else {
                if (rewrite_stream_ctx) {
                    stream_emit_simple(rewrite_stream_ctx, STREAM_EVENT_REWRITE_ROLLBACK,
                                       rule->name ? rule->name : "rule rolled back", steps);
                }
            }
            lv00_free((void**)&match);
        }

        if (!applied) break;
        steps++;
    }

    if (steps >= step_limit && final_status == REWRITE_OK) {
        final_status = REWRITE_TERMINATED;
    }

    if (rewrite_stream_ctx) {
        stream_emit_simple(rewrite_stream_ctx, STREAM_EVENT_REWRITE_DONE,
                           "rewrite phase done", steps);
    }

done:
    lv00_free((void**)&history_hashes);
    lv00_free((void**)&sorted);
    return final_status;
}

/* ===========================================================================
 * 循环检测
 * ===========================================================================
 */

/**
 * @brief 检测重写循环：判断当前图哈希是否在历史中出现过
 *
 * 计算当前约束图的结构哈希值，与历史哈希记录逐一比对。
 * 若匹配则说明图状态已出现过，形成重写循环。
 *
 * @param graph          当前约束图指针
 * @param history_hashes 历史哈希值数组
 * @param history_count  历史记录数量
 * @return true 表示检测到循环，false 表示未检测到
 */
static bool detect_rewrite_loop(ConstraintGraph *graph, int *history_hashes, int history_count) {
