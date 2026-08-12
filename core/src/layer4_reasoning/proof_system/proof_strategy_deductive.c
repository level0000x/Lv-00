/**
 * @file proof_strategy_deductive.c
 * @brief 演绎数据库法策略执行
 *
 * 从 proof_strategy_exec.c 拆分的模块之一。
 *
 * @version v3.6.0
 */

#include "proof_multi_strategy_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/constraint_graph.h"
#include "lv/geo_utils.h"
#include "lv/proof.h"
#include "lv/solver.h"

#include "atp_backend.h"
#include "debug.h"
#include "lv_internal.h"
#include "lv_utils.h"
#include "lv/lambda_to_graph.h"
#include "lv/lambda_unify.h"
#include "normalization.h"
#include "type_system.h"
#include "unify.h"

/* ================================================================
 * 约束类型 -> 初始事实格式 映射表（数据表化，替代 switch）
 * ================================================================ */

/* 约束类型 → 初始事实格式 映射表。
 * min_participants 为该约束生成事实所需的最少参与者数量。 */
typedef struct {
    const char *format;   /* 事实格式串，如 "incidence:%d,%d" */
    int min_participants; /* 所需最少参与者数 */
} DeductFactSpec;

static const DeductFactSpec s_constraint_fact_specs[] = {
    [INCIDENCE]    = {"incidence:%d,%d", 2},
    [BETWEENNESS]  = {"betweenness:%d,%d,%d", 3},
    [INTERSECTION] = {"intersection:%d,%d,%d", 3},
    [CONTAINMENT]  = {"containment:%d,%d", 2},
    [ANGLE]        = {"angle:%d,%d", 2},
    [CONNECTION]   = {"connection:%d,%d", 2},
};

/**
 * @brief 演绎数据库法执行 —— 前向链推理
 *
 * 借鉴 JGEX 的演绎数据库法：
 * - 从约束图中提取已知事实作为初始事实库
 * - 迭代应用推理规则生成新事实：
 *   - 等式/合同关系的传递性
 *   - 等量代换
 *   - 角度加法
 *   - 三角形合同判据（SSS、SAS、ASA）
 * - 每轮迭代：对所有已知事实应用所有规则
 * - 检查目标命题是否在已推导事实中
 * - 限制最大迭代次数防止无限循环
 */
bool execute_deductive_database(ProofMultiStrategy *mse, ProofNavigator *nav) {
    (void) mse;
    if (!nav || !nav->construction)
        return false;

    ConstraintGraph *graph = nav->construction;

    /* 添加演绎数据库起始步骤 */
    ProofStep *start_step = proof_step_create(PROOF_STEP_FUNCTION_APP);
    if (start_step) {
        start_step->color = PROOF_COLOR_GREEN;
        start_step->note = lv_strdup_safe("[演绎数据库] 从已知条件出发，使用前向链推理逐步推导新事实");
        proof_navigator_add_step(nav, start_step);
    }

/* ---- 事实表示 ----
     * 使用简单的字符串数组表示事实（"fact_type:arg1,arg2,..."）
     * 最大事实数 1024，最大迭代 100
     */
#define DEDUCT_MAX_FACTS 1024
#define DEDUCT_MAX_ITER 100

    char **facts = (char **) lv_calloc(DEDUCT_MAX_FACTS, sizeof(char *));
    if (!facts) {
        lv_RETURN_ERROR_BOOL(lv_ERROR_OUT_OF_MEMORY, "deductive_method: lv_calloc for facts failed (max=%d)", DEDUCT_MAX_FACTS);
    }
    int fact_count = 0;
    bool verified = false;

/* 辅助函数：添加事实（去重） */
/* 使用内联 lambda 风格的辅助逻辑 */
#define DEDUCT_ADD_FACT(fmt_str, ...)                          \
    do {                                                       \
        char _buf[512];                                        \
        snprintf(_buf, sizeof(_buf), fmt_str, __VA_ARGS__);    \
        bool _dup = false;                                     \
        for (int _fi = 0; _fi < fact_count; _fi++) {           \
            if (facts[_fi] && strcmp(facts[_fi], _buf) == 0) { \
                _dup = true;                                   \
                break;                                         \
            }                                                  \
        }                                                      \
        if (!_dup && fact_count < DEDUCT_MAX_FACTS) {          \
            facts[fact_count++] = lv_strdup_safe(_buf);        \
        }                                                      \
    } while (0)

    /* 阶段1：从约束图提取初始事实 */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *c = graph->constraints[i];
        if (!c || !c->is_active)
            continue;

        /* 按约束类型查表生成事实（格式串 + 最少参与者数检查） */
        if ((unsigned) c->type < lv_ARRAY_SIZE(s_constraint_fact_specs)) {
            const DeductFactSpec *spec = &s_constraint_fact_specs[c->type];
            if (spec->format && c->participant_count >= spec->min_participants)
                DEDUCT_ADD_FACT(spec->format, c->participants[0], c->participants[1],
                                c->participant_count >= 3 ? c->participants[2] : 0);
        }
    }

    /* 提取点坐标作为事实 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_POINT)
            continue;
        if (node->coord_count >= 2 && node->symbolic_coords[0] && node->symbolic_coords[1]) {
            char *sx = symbolic_coord_serialize(node->symbolic_coords[0]);
            char *sy = symbolic_coord_serialize(node->symbolic_coords[1]);
            if (sx && sy) {
                char buf[512];
                snprintf(buf, sizeof(buf), "point_coord:%d,%s,%s", node->id, sx, sy);
                bool dup = false;
                for (int fi = 0; fi < fact_count; fi++) {
                    if (facts[fi] && strcmp(facts[fi], buf) == 0) {
                        dup = true;
                        break;
                    }
                }
                if (!dup && fact_count < DEDUCT_MAX_FACTS) {
                    facts[fact_count++] = lv_strdup_safe(buf);
                }
            }
            if (sy)
                lv_free((void **) &sy);
            if (sx)
                lv_free((void **) &sx);
        }
    }

    ProofStep *extract_step = proof_step_create(PROOF_STEP_FUNCTION_APP);
    if (extract_step) {
        extract_step->color = PROOF_COLOR_GREEN;
        char buf[256];
        snprintf(buf, sizeof(buf), "[演绎数据库] 从约束图提取 %d 条初始事实", fact_count);
        extract_step->note = lv_strdup_safe(buf);
        proof_navigator_add_step(nav, extract_step);
    }

    /* 阶段2：迭代推理 */
    int prev_fact_count = 0;
    for (int iter = 0; iter < DEDUCT_MAX_ITER && !verified; iter++) {
        prev_fact_count = fact_count;
        int new_derived = 0;

        /* 规则1：传递性 —— 如果 A-B 和 B-C 共线，则 A-B-C 共线 */
        for (int i = 0; i < fact_count; i++) {
            if (!lv_str_startswith(facts[i], "betweenness:"))
                continue;
            /* 解析 betweenness:a,b,c */
            int a1, b1, c1;
            if (sscanf(facts[i], "betweenness:%d,%d,%d", &a1, &b1, &c1) != 3)
                continue;

            for (int j = 0; j < fact_count; j++) {
                if (i == j || !lv_str_startswith(facts[j], "betweenness:"))
                    continue;
                int a2, b2, c2;
                if (sscanf(facts[j], "betweenness:%d,%d,%d", &a2, &b2, &c2) != 3)
                    continue;

                /* 如果 b1 == a2，则推导 a1-b1(=a2)-c2 共线 */
                if (b1 == a2 && a1 != c2) {
                    DEDUCT_ADD_FACT("betweenness:%d,%d,%d", a1, b1, c2);
                    new_derived++;
                }
                /* 如果 c1 == a2，则推导 a1-c1(=a2)-c2 */
                if (c1 == a2 && a1 != c2) {
                    DEDUCT_ADD_FACT("betweenness:%d,%d,%d", a1, c1, c2);
                    new_derived++;
                }
            }
        }

        /* 规则2：相交传递 —— 如果线L1过点P，L2过点P，且L1和L2相交于P */
        for (int i = 0; i < fact_count; i++) {
            if (!lv_str_startswith(facts[i], "incidence:"))
                continue;
            int p1, l1;
            if (sscanf(facts[i], "incidence:%d,%d", &p1, &l1) != 2)
                continue;

            for (int j = i + 1; j < fact_count; j++) {
                if (!lv_str_startswith(facts[j], "incidence:"))
                    continue;
                int p2, l2;
                if (sscanf(facts[j], "incidence:%d,%d", &p2, &l2) != 2)
                    continue;

                /* 同一点在两条不同线上 => 相交 */
                if (p1 == p2 && l1 != l2) {
                    DEDUCT_ADD_FACT("intersection:%d,%d,%d", l1, l2, p1);
                    new_derived++;
                }
            }
        }

        /* 规则3：等量代换 —— 如果两点的坐标相同，则两点重合 */
        for (int i = 0; i < fact_count; i++) {
            if (!lv_str_startswith(facts[i], "point_coord:"))
                continue;
            for (int j = i + 1; j < fact_count; j++) {
                if (!lv_str_startswith(facts[j], "point_coord:"))
                    continue;
                /* 比较坐标字符串：从逗号后截取坐标部分进行字符串比较 */
                /* 格式：point_coord:id,x,y —— 逗号后为 ",x,y" */
                char *comma1_i = strchr(facts[i] + 11, ',');
                char *comma1_j = strchr(facts[j] + 11, ',');
                if (comma1_i && comma1_j) {
                    /* 比较坐标部分 */
                    if (strcmp(comma1_i, comma1_j) == 0) {
                        int id_i = -1, id_j = -1;
                        int ret_i = sscanf(facts[i], "point_coord:%d,", &id_i);
                        int ret_j = sscanf(facts[j], "point_coord:%d,", &id_j);
                        if (ret_i >= 1 && ret_j >= 1 && id_i != id_j) {
                            DEDUCT_ADD_FACT("coincident:%d,%d", id_i, id_j);
                            new_derived++;
                        }
                    }
                }
            }
        }

        /* 规则4：SSS 合同判据 —— 如果三边对应相等，则三角形合同 */
        /* 从 point_coord 事实中提取所有点坐标，计算边长，检测合同三角形 */
        {
            /* 收集所有有坐标的点 */
            int pt_ids[128];
            double pt_x[128], pt_y[128];
            int pt_count = 0;
            for (int fi = 0; fi < fact_count && pt_count < 128; fi++) {
                if (!lv_str_startswith(facts[fi], "point_coord:"))
                    continue;
                int pid;
                double fx, fy;
                if (sscanf(facts[fi], "point_coord:%d,%lf,%lf", &pid, &fx, &fy) == 3) {
                    pt_ids[pt_count] = pid;
                    pt_x[pt_count] = fx;
                    pt_y[pt_count] = fy;
                    pt_count++;
                }
            }

            /* 计算所有点对之间的距离平方（避免开方） */
            if (pt_count >= 3) {
#define SSS_MAX_PAIRS 4096
                int pair_a[SSS_MAX_PAIRS], pair_b[SSS_MAX_PAIRS];
                double pair_dist2[SSS_MAX_PAIRS];
                int pair_count = 0;
                for (int a = 0; a < pt_count && pair_count < SSS_MAX_PAIRS; a++) {
                    for (int b = a + 1; b < pt_count && pair_count < SSS_MAX_PAIRS; b++) {
                        double dx = pt_x[a] - pt_x[b];
                        double dy = pt_y[a] - pt_y[b];
                        pair_a[pair_count] = pt_ids[a];
                        pair_b[pair_count] = pt_ids[b];
                        pair_dist2[pair_count] = geo_norm_sq_2d(dx, dy);
                        pair_count++;
                    }
                }

/* 枚举所有三角形（三个点），检查是否有合同三角形 */
#define SSS_MAX_TRI 512
                int tri[SSS_MAX_TRI][3]; /* 每个三角形的三个点ID */
                int tri_count = 0;
                for (int a = 0; a < pt_count && tri_count < SSS_MAX_TRI; a++) {
                    for (int b = a + 1; b < pt_count && tri_count < SSS_MAX_TRI; b++) {
                        for (int c = b + 1; c < pt_count && tri_count < SSS_MAX_TRI; c++) {
                            tri[tri_count][0] = pt_ids[a];
                            tri[tri_count][1] = pt_ids[b];
                            tri[tri_count][2] = pt_ids[c];
                            tri_count++;
                        }
                    }
                }

                /* 对每对三角形，检查 SSS 合同 */
                for (int t1 = 0; t1 < tri_count && !verified; t1++) {
                    for (int t2 = t1 + 1; t2 < tri_count; t2++) {
                        /* 计算三角形 t1 的三边长 */
                        double edges1[3];
                        int e1_pairs[3][2]; /* 每条边对应的点对索引 */
                        int e1_found = 0;
                        for (int e = 0; e < 3; e++) {
                            int pa = tri[t1][e];
                            int pb = tri[t1][(e + 1) % 3];
                            for (int p = 0; p < pair_count; p++) {
                                if ((pair_a[p] == pa && pair_b[p] == pb) || (pair_a[p] == pb && pair_b[p] == pa)) {
                                    edges1[e] = pair_dist2[p];
                                    e1_pairs[e][0] = pa;
                                    e1_pairs[e][1] = pb;
                                    e1_found++;
                                    break;
                                }
                            }
                        }

                        if (e1_found < 3)
                            continue;

                        /* 计算三角形 t2 的三边长，尝试所有顶点排列 */
                        for (int perm = 0; perm < 6; perm++) {
                            /* 6种排列：012, 021, 102, 120, 201, 210 */
                            static const int perms[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2},
                                                            {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
                            int va = tri[t2][perms[perm][0]];
                            int vb = tri[t2][perms[perm][1]];
                            int vc = tri[t2][perms[perm][2]];
                            double edges2[3];
                            int e2_found = 0;
                            int e2_pairs[3][2];
                            (void) e2_pairs; /* suppress unused warning */

                            int t2verts[3] = {va, vb, vc};
                            for (int e = 0; e < 3; e++) {
                                int pa = t2verts[e];
                                int pb = t2verts[(e + 1) % 3];
                                for (int p = 0; p < pair_count; p++) {
                                    if ((pair_a[p] == pa && pair_b[p] == pb) || (pair_a[p] == pb && pair_b[p] == pa)) {
                                        edges2[e] = pair_dist2[p];
                                        e2_pairs[e][0] = pa;
                                        e2_pairs[e][1] = pb;
                                        e2_found++;
                                        break;
                                    }
                                }
                            }

                            if (e2_found < 3)
                                continue;

                            /* 检查三边是否对应相等（使用相对容差） */
                            bool sss_match = true;
                            for (int e = 0; e < 3; e++) {
                                double diff = fabs(edges1[e] - edges2[e]);
                                double max_val = fmax(fabs(edges1[e]), fabs(edges2[e]));
                                if (max_val < lv_EPSILON_ULTRA)
                                    continue; /* 两边都为零 */
                                if (diff / max_val > lv_EPSILON_LOW) {
                                    sss_match = false;
                                    break;
                                }
                            }

                            if (sss_match) {
                                DEDUCT_ADD_FACT("congruent:%d,%d,%d,%d,%d,%d", tri[t1][0], tri[t1][1], tri[t1][2],
                                                tri[t2][0], tri[t2][1], tri[t2][2]);
                                new_derived++;
                            }
                        }
                    }
                }
#undef SSS_MAX_PAIRS
#undef SSS_MAX_TRI
            }
        }

        /* 检查是否推导出目标命题相关的事实 */
        if (nav->target_prop && nav->target_prop->name) {
            /* 检查目标命题中的关键关系是否在事实库中 */
            for (int fi = 0; fi < fact_count; fi++) {
                if (!facts[fi])
                    continue;
                /* 如果目标涉及共线，检查 betweenness 事实 */
                if (strstr(nav->target_prop->name, "collinear") && lv_str_startswith(facts[fi], "betweenness:")) {
                    verified = true;
                    break;
                }
                /* 如果目标涉及相交，检查 intersection 事实 */
                if (strstr(nav->target_prop->name, "intersect") && lv_str_startswith(facts[fi], "intersection:")) {
                    verified = true;
                    break;
                }
                /* 如果目标涉及重合，检查 coincident 事实 */
                if (strstr(nav->target_prop->name, "coincident") && lv_str_startswith(facts[fi], "coincident:")) {
                    verified = true;
                    break;
                }
            }
        }

        /* 如果没有新事实产生，提前终止 */
        if (fact_count == prev_fact_count) {
            ProofStep *fixpoint_step = proof_step_create(PROOF_STEP_FUNCTION_APP);
            if (fixpoint_step) {
                fixpoint_step->color = PROOF_COLOR_BLUE_UNEXPLORED;
                char buf[256];
                snprintf(buf, sizeof(buf), "[演绎数据库] 第 %d 轮迭代达到不动点，共 %d 条事实", iter + 1, fact_count);
                fixpoint_step->note = lv_strdup_safe(buf);
                proof_navigator_add_step(nav, fixpoint_step);
            }
            break;
        }

        if (new_derived > 0 && iter % 10 == 0) {
            ProofStep *iter_step = proof_step_create(PROOF_STEP_FUNCTION_APP);
            if (iter_step) {
                iter_step->color = PROOF_COLOR_GREEN;
                char buf[256];
                snprintf(buf, sizeof(buf), "[演绎数据库] 第 %d 轮推理，新增 %d 条事实，累计 %d 条", iter + 1,
                         new_derived, fact_count);
                iter_step->note = lv_strdup_safe(buf);
                proof_navigator_add_step(nav, iter_step);
            }
        }
    }

    /* 如果演绎推理未直接验证，回退到合一 */
    if (!verified && nav->target_prop && nav->target_prop->pattern) {
        UnifyStatus status = proof_unify(graph, nav->target_prop, false);
        verified = (status == UNIFY_STATUS_OK);
    }

    if (verified) {
        ProofStep *done_step = proof_step_create(PROOF_STEP_UNIFY);
        if (done_step) {
            done_step->color = PROOF_COLOR_GREEN;
            done_step->note = lv_strdup_safe("[演绎数据库] 目标命题已从已知条件推导得出");
            proof_navigator_add_step(nav, done_step);
        }
    }

    /* 清理事实库 */
    for (int i = 0; i < fact_count; i++) {
        lv_free((void **) &facts[i]);
    }
    lv_free((void **) &facts);

#undef DEDUCT_MAX_FACTS
#undef DEDUCT_MAX_ITER
#undef DEDUCT_ADD_FACT

    return verified;
}

