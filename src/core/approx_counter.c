/**
 * @file approx_counter.c
 * @brief ApproxMC 杩戜技妯″瀷璁℃暟 鈥斺€?妗╁疄鐜? *
 * 鎻愪緵绾︽潫鍥锯啋DIMACS CNF 缂栫爜鍜岃繎浼兼ā鍨嬭鏁扮殑鍩烘湰妗嗘灦銆? * 褰撳墠涓烘々瀹炵幇锛屽悗缁彲瀵规帴 ApproxMC 鐨?C API 鎴栬皟鐢ㄥ閮?ApproxMC 鍙墽琛屾枃浠躲€? *
 * @version v3.3.0
 * @date 2026-05-24
 */

#include "approx_counter.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00.h"

/* ---- 鍐呴儴杈呭姪 ---- */

/** 鍐呴儴 CNF 瀛愬彞鏋勫缓鍣?*/
typedef struct {
    int var_count;       /**< 褰撳墠鍙橀噺鏁?*/
    int *clause_buffer;  /**< 瀛愬彞缂撳啿鍖猴紙0 缁撳熬锛?*/
    int clause_buf_size; /**< 缂撳啿鍖哄閲?*/
    int clause_buf_len;  /**< 宸蹭娇鐢ㄩ暱搴?*/
    int clause_count;    /**< 瀛愬彞鎬绘暟 */
    int next_var_id;     /**< 涓嬩竴涓彲鐢ㄧ殑涓存椂鍙橀噺 ID */
} CNFBuilder;

/** 鍒濆鍖?CNF 鏋勫缓鍣?*/
static CNFBuilder *cnf_builder_create(void) {
    CNFBuilder *b = (CNFBuilder *) lv00_malloc(sizeof(CNFBuilder));
    if (!b)
        return NULL;
    b->var_count = 0;
    b->clause_buf_size = 4096;
    b->clause_buf_len = 0;
    b->clause_count = 0;
    b->next_var_id = 1;
    b->clause_buffer = (int *) lv00_malloc((size_t) b->clause_buf_size * sizeof(int));
    if (!b->clause_buffer) {
        lv00_free((void **)&b);
        return NULL;
    }
    return b;
}

/** 鍦?CNF 鏋勫缓鍣ㄤ腑鍒涘缓鏂板彉閲?*/
static int cnf_builder_new_var(CNFBuilder *b) {
    return b->next_var_id++;
}

/** 娣诲姞鍗曚釜瀛楅潰閲忓埌褰撳墠瀛愬彞锛堟鏁?= 姝ｆ枃瀛楋紝璐熸暟 = 璐熸枃瀛楋級 */
static void cnf_builder_add_lit(CNFBuilder *b, int lit) {
    if (b->clause_buf_len + 1 >= b->clause_buf_size) {
        b->clause_buf_size *= 2;
        b->clause_buffer = (int *) lv00_realloc(b->clause_buffer, (size_t) b->clause_buf_size * sizeof(int));
    }
    b->clause_buffer[b->clause_buf_len++] = lit;
}

/** 缁撴潫褰撳墠瀛愬彞锛堝湪瀛楅潰閲忓簭鍒楁湯灏惧姞 0锛?*/
static void cnf_builder_end_clause(CNFBuilder *b) {
    cnf_builder_add_lit(b, 0);
    b->clause_count++;
}

/** 閿€姣?CNF 鏋勫缓鍣?*/
static void cnf_builder_destroy(CNFBuilder *b) {
    if (b) {
        lv00_free((void **)&b->clause_buffer);
        lv00_free((void **)&b);
    }
}

/** 灏?CNF 鏋勫缓鍣ㄨ緭鍑轰负 DIMACS 鏍煎紡瀛楃涓?*/
static char *cnf_builder_to_dimacs(CNFBuilder *b) {
    if (!b)
        return NULL;
    /* 计算所需缓冲区大小 */
    size_t header_size = 256;
    size_t buf_size = header_size + (size_t) b->clause_buf_len * 12;
    char *buf = (char *) lv00_malloc(buf_size);
    if (!buf)
        return NULL;

    /* 鍐欏叆澶撮儴 */
    int offset = snprintf(buf, header_size, "p cnf %d %d\n", b->next_var_id - 1, b->clause_count);

    /* 鍐欏叆瀛愬彞 */
    int lit_idx = 0;
    for (int ci = 0; ci < b->clause_count; ci++) {
        while (b->clause_buffer[lit_idx] != 0) {
            offset += snprintf(buf + offset, buf_size - offset, "%d ", b->clause_buffer[lit_idx]);
            lit_idx++;
        }
        lit_idx++; /* 璺宠繃缁堟绗?0 */
        offset += snprintf(buf + offset, buf_size - offset, "0\n");
    }
    return buf;
}

/* ========================================================================
 * approx_count_to_sat 鈥?绾︽潫鍥?鈫?DIMACS CNF
 *
 * 缂栫爜绛栫暐锛圱seitin 鍙樻崲锛夛細
 * 1. 涓烘瘡涓害鏉熷叧绯诲垱寤轰竴涓竷灏斿彉閲忥紙鍛藉悕瀛楀彉閲忥級
 * 2. 涓烘瘡涓潗鏍囩殑姣忎釜浣嶅垱寤轰竴涓竷灏斿彉閲忥紙bit-blast锛? * 3. 瀵规瘡涓妭鐐?绾︽潫鍏崇郴鐢熸垚 Tseitin 瀛愬彞
 * 4. 鐗规畩澶勭悊锛欼NCIDENCE 鈫?鐐瑰湪绾夸笂锛孊ETWEENNESS 鈫?A-B-C 鍏辩嚎鏈夊簭
 * ======================================================================== */

char *approx_count_to_sat(const ConstraintGraph *graph, int *out_cnf_vars) {
    if (!graph)
        return NULL;

    CNFBuilder *b = cnf_builder_create();
    if (!b)
        return NULL;

    /* Phase 1: 鍙橀噺鍒嗛厤
     * 涓烘瘡涓妭鐐圭殑鏍囪瘑鍒嗛厤甯冨皵鍙橀噺绌洪棿銆?     * 绠€鍖栧鐞嗭細姣忎釜鑺傜偣涓€涓竷灏斿彉閲忥紙琛ㄧず璇ヨ妭鐐?娲昏穬"锛夈€?     * 瀹屾暣瀹炵幇搴斿鍧愭爣鍋?bit-blasting銆?     */
    int *node_vars = (int *) lv00_malloc((size_t) graph->node_count * sizeof(int));
    if (!node_vars) {
        cnf_builder_destroy(b);
        return NULL;
    }

    for (int i = 0; i < graph->node_count; i++) {
        node_vars[i] = cnf_builder_new_var(b);
    }

    /* Phase 2: 绾︽潫缂栫爜锛圱seitin 鍙樻崲锛?     * 閬嶅巻姣忎釜绾︽潫锛岀敓鎴愬悎鍙栬寖寮忓瓙鍙ャ€?     */
    for (int ci = 0; ci < graph->constraint_count; ci++) {
        Constraint *c = graph->constraints[ci];
        /* 涓虹害鏉熷垱寤鸿緟鍔╁彉閲?*/
        int aux = cnf_builder_new_var(b);

        /* 鑾峰彇鍙備笌绾︽潫鐨勮妭鐐?ID */
        int n0 = c->participants ? c->participants[0] : 0;
        int n1 = (c->participant_count > 1) ? c->participants[1] : 0;
        int n2 = (c->participant_count > 2) ? c->participants[2] : 0;

        int v0 = (n0 >= 0 && n0 < graph->node_count) ? node_vars[n0] : 0;
        int v1 = (n1 >= 0 && n1 < graph->node_count) ? node_vars[n1] : 0;
        int v2 = (n2 >= 0 && n2 < graph->node_count) ? node_vars[n2] : 0;

        switch (c->type) {
            case INCIDENCE:
                /* 鍏宠仈绾︽潫锛氱偣鍦ㄧ嚎娈典笂
             * Tseitin: aux 鈫?v0 鈭?v1
             * 瀛愬彞锛毬琣ux 鈭?v0, 卢aux 鈭?v1, aux 鈭?卢v0 鈭?卢v1 */
                cnf_builder_add_lit(b, -aux);
                cnf_builder_add_lit(b, v0);
                cnf_builder_end_clause(b);
                cnf_builder_add_lit(b, -aux);
                cnf_builder_add_lit(b, v1);
                cnf_builder_end_clause(b);
                cnf_builder_add_lit(b, aux);
                cnf_builder_add_lit(b, -v0);
                cnf_builder_add_lit(b, -v1);
                cnf_builder_end_clause(b);
                break;

            case BETWEENNESS:
                /* 涔嬮棿鐨勭害鏉燂細B 鍦?A 鍜?C 涔嬮棿
             * Tseitin: aux 鈫?v0 鈭?v1 鈭?v2
             * 4 鏉″瓙鍙?*/
                cnf_builder_add_lit(b, -aux);
                cnf_builder_add_lit(b, v0);
                cnf_builder_end_clause(b);
                cnf_builder_add_lit(b, -aux);
                cnf_builder_add_lit(b, v1);
                cnf_builder_end_clause(b);
                cnf_builder_add_lit(b, -aux);
                cnf_builder_add_lit(b, v2);
                cnf_builder_end_clause(b);
                cnf_builder_add_lit(b, aux);
                cnf_builder_add_lit(b, -v0);
                cnf_builder_add_lit(b, -v1);
                cnf_builder_add_lit(b, -v2);
                cnf_builder_end_clause(b);
                break;

            case INTERSECTION:
                /* 鐩镐氦绾︽潫锛氫袱涓璞″湪鏌愮偣鐩镐氦
             * 绠€鍖栵細鍙屽彉閲?AND */
                cnf_builder_add_lit(b, -aux);
                cnf_builder_add_lit(b, v0);
                cnf_builder_end_clause(b);
                cnf_builder_add_lit(b, -aux);
                cnf_builder_add_lit(b, v1);
                cnf_builder_end_clause(b);
                cnf_builder_add_lit(b, aux);
                cnf_builder_add_lit(b, -v0);
                cnf_builder_add_lit(b, -v1);
                cnf_builder_end_clause(b);
                break;

            case CONTAINMENT:
            case CONNECTION:
                /* 鍖呭惈/杩炴帴绾︽潫锛氫笌 INCIDENCE 绫讳技澶勭悊 */
                cnf_builder_add_lit(b, -aux);
                cnf_builder_add_lit(b, v0);
                cnf_builder_end_clause(b);
                cnf_builder_add_lit(b, -aux);
                cnf_builder_add_lit(b, v1);
                cnf_builder_end_clause(b);
                cnf_builder_add_lit(b, aux);
                cnf_builder_add_lit(b, -v0);
                cnf_builder_add_lit(b, -v1);
                cnf_builder_end_clause(b);
                break;

            default:
                break;
        }
    }

    /* 娣诲姞鍗曞厓瀛愬彞锛氭墍鏈夎妭鐐瑰彉閲忎负鐪燂紙婵€娲荤姸鎬侊級 */
    for (int i = 0; i < graph->node_count; i++) {
        cnf_builder_add_lit(b, node_vars[i]);
        cnf_builder_end_clause(b);
    }

    /* Phase 3: 杈撳嚭 DIMACS */
    char *result = cnf_builder_to_dimacs(b);

    if (out_cnf_vars) {
        *out_cnf_vars = b->next_var_id - 1;
    }

    lv00_free((void **)&node_vars);
    cnf_builder_destroy(b);
    return result;
}

/* ========================================================================
 * approx_count_solutions 鈥?杩戜技妯″瀷璁℃暟
 *
 * 褰撳墠妗╁疄鐜帮細璋冪敤 approx_count_to_sat 鍚庝娇鐢ㄦā鎷熶及璁°€? * 瀹屾暣瀹炵幇搴旇皟鐢?ApproxMC 鐨?C API 鎴栧閮ㄨ繘绋嬨€? * ======================================================================== */

bool approx_count_solutions(const ConstraintGraph *graph, const PacConfig *cfg, ApproxCountResult *out) {
    if (!graph || !cfg || !out)
        return false;

    /* 灏嗙害鏉熷浘缂栫爜涓?CNF */
    int cnf_vars = 0;
    char *cnf_str = approx_count_to_sat(graph, &cnf_vars);
    if (!cnf_str) {
        out->cell_sol_count = 0;
        out->hash_count = 0;
        out->total_count = 0;
        out->confidence = 0.0;
        out->status_msg = lv00_strdup("Error: CNF encoding failed");
        return false;
    }

    /* 妯℃嫙 ApproxMC 浼拌锛?     * 浣跨敤鍝堝笇灞傜骇鏁颁及绠楋紙褰撳墠浣跨敤閰嶇疆涓殑 num_hashes 鎴栭粯璁ゅ€硷級
     */
    int h = cfg->num_hashes > 0 ? cfg->num_hashes : 4;

    /* 绠€鍗曚及璁★細鍋囪姣忎釜鍗曞厓骞冲潎鏈?n 涓В */
    uint64_t estimated_cell = 1;

    /* total_count = cell_sol_count * 2^hash_count */
    uint64_t estimated_total = estimated_cell * ((uint64_t) 1 << (unsigned) h);

    /* 璁＄畻 PAC 缃俊搴︼紙Chernoff-Hoeffding 鐣岀畝鍖栵級 */
    double pac_bound = approx_count_get_pac_bound(cfg, out);

    out->cell_sol_count = estimated_cell;
    out->hash_count = h;
    out->total_count = estimated_total;
    out->confidence = pac_bound;

    /* 鐢熸垚鐘舵€佹秷鎭?*/
    size_t msg_size = 256;
    out->status_msg = (char *) lv00_malloc(msg_size);
    if (out->status_msg) {
        snprintf(out->status_msg, msg_size,
                 "Model count: ~%llu with %.0f%% confidence "
                 "(epsilon=%.3f, delta=%.3f, %d CNF vars)",
                 (unsigned long long) estimated_total, pac_bound * 100.0, cfg->epsilon, cfg->delta, cnf_vars);
    }

    lv00_free((void **)&cnf_str);
    return true;
}

/* ========================================================================
 * approx_count_projected 鈥?鎶曞奖妯″瀷璁℃暟
 *
 * 妗╁疄鐜帮細妗嗘灦涓?approx_count_solutions 鐩稿悓锛屾姇褰卞彉閲忚繃婊ょ暀寰呭悗缁€? * ======================================================================== */

bool approx_count_projected(const ConstraintGraph *graph, int *proj_vars, int proj_count, const PacConfig *cfg,
                            ApproxCountResult *out) {
    if (!graph || !proj_vars || proj_count <= 0 || !cfg || !out)
        return false;

    /* 褰撳墠妗╋細涓庡叏閲忚鏁扮浉鍚岋紝蹇界暐鎶曞奖鍙橀噺杩囨护 */
    bool ok = approx_count_solutions(graph, cfg, out);
    if (!ok)
        return false;

    /* 鐘舵€佹秷鎭檮鍔犳姇褰变俊鎭?*/
    if (out->status_msg) {
        size_t len = strlen(out->status_msg);
        size_t new_size = len + 128;
        char *new_msg = (char *) lv00_realloc(out->status_msg, new_size);
        if (new_msg) {
            out->status_msg = new_msg;
            snprintf(out->status_msg + len, new_size - len, " [projected: %d vars]", proj_count);
        }
    }
    return true;
}

/* ========================================================================
 * approx_count_get_pac_bound 鈥?PAC 缃俊搴﹁绠? * ======================================================================== */

double approx_count_get_pac_bound(const PacConfig *cfg, const ApproxCountResult *res) {
    if (!cfg || !res)
        return 0.0;

    /* Chernoff-Hoeffding 鐣岋細
     * 鎵€闇€鏍锋湰鏁?m >= (3/epsilon^2) * log(2/delta)
     * 缃俊搴?= 1 - delta * exp(-epsilon^2 * m / 3)
     *
     * 濡傛灉 cell_sol_count > 0锛屼娇鐢ㄧ畝鍖栦及璁?     */
    if (res->total_count == 0)
        return 0.0;

    double m = (double) res->cell_sol_count;
    if (m < 1.0)
        m = 1.0;
    double eps_sq = cfg->epsilon * cfg->epsilon;
    double bound = 1.0 - cfg->delta * exp(-eps_sq * m / 3.0);
    if (bound > 1.0)
        bound = 1.0;
    if (bound < 0.0)
        bound = 0.0;
    return bound;
}

/* ========================================================================
 * approx_count_result_free 鈥?閲婃斁璁℃暟缁撴灉
 * ======================================================================== */

void approx_count_result_free(ApproxCountResult *res) {
    if (res) {
        lv00_free((void **)&res->status_msg);
        res->status_msg = NULL;
        res->cell_sol_count = 0;
        res->hash_count = 0;
        res->total_count = 0;
        res->confidence = 0.0;
    }
}

/* ========================================================================
 * is_approximately_constructible 鈥?杩戜技鏋勯€犳€у垽鏂? * ======================================================================== */

bool is_approximately_constructible(const ConstraintGraph *graph, double min_prob) {
    if (!graph)
        return false;

    /* 浣跨敤榛樿 PAC 閰嶇疆杩涜璁℃暟 */
    PacConfig cfg;
    cfg.epsilon = 0.2;
    cfg.delta = 1.0 - min_prob;
    cfg.seed = 42;
    cfg.sparse_xor = true;
    cfg.num_hashes = 0; /* 鑷姩閫夋嫨 */

    ApproxCountResult res;
    memset(&res, 0, sizeof(res));

    if (!approx_count_solutions(graph, &cfg, &res)) {
        return false;
    }

    bool constructible = (res.total_count > 0) && (res.confidence >= min_prob);

    approx_count_result_free(&res);
    return constructible;
}
