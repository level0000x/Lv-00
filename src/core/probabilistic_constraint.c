/**
 * @file probabilistic_constraint.c
 * @brief PRISM 姒傜巼妯″瀷妫€娴?鈥斺€?妗╁疄鐜? *
 * 鎻愪緵姒傜巼鍒嗗竷銆佹鐜囩害鏉熻妭鐐瑰拰 PCTL 璇勪及鐨勬鏋跺疄鐜般€? * 鍖呭惈 Box-Muller 姝ｆ€侀噰鏍枫€侀€?CDF 閲囨牱绛夊熀鏈噰鏍锋柟娉曘€? *
 * @version v3.3.0
 * @date 2026-05-24
 */

#include "probabilistic_constraint.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv00_utils.h"

/* ---- 鍐呴儴甯搁噺 ---- */

/** 榛樿閲囨牱鏁伴噺锛堢敤浜?Monte Carlo 浼拌锛?*/
#define DEFAULT_N_SAMPLES 1000

/** pi 甯搁噺 */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---- 鍐呴儴锛氱畝鍗曢殢鏈烘暟鐢熸垚鍣紙绾挎€у悓浣欏彂鐢熷櫒锛?---- */

static unsigned long rand_state_lcg = 123456789UL;

/** 璁剧疆闅忔満绉嶅瓙 */
static void rand_seed_lcg(unsigned long seed) {
    rand_state_lcg = seed;
}

/** 鐢熸垚 [0, 1) 鐨勫潎鍖€闅忔満鏁帮紙绾挎€у悓浣欙級 */
static double rand_uniform_lcg(void) {
    rand_state_lcg = rand_state_lcg * 1103515245UL + 12345UL;
    return (double) (rand_state_lcg & 0x7FFFFFFFUL) / (double) 0x80000000UL;
}

/** 鐢熸垚鏍囧噯姝ｆ€佸垎甯?N(0,1) 闅忔満鏁帮紙Box-Muller 鍙樻崲锛?*/
static double rand_normal_box_muller(void) {
    double u1 = rand_uniform_lcg();
    double u2 = rand_uniform_lcg();
    if (u1 < 1e-12)
        u1 = 1e-12;
    return sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
}

/* ========================================================================
 * prob_dist_create 鈥?鍒涘缓姒傜巼鍒嗗竷
 * ======================================================================== */

ProbDistribution *prob_dist_create(ProbDistType type, double *params, int param_count) {
    ProbDistribution *dist = (ProbDistribution *) lv00_malloc(sizeof(ProbDistribution));
    if (!dist)
        return NULL;

    dist->type = type;
    dist->param_count = param_count;
    dist->pdf = NULL;
    dist->cdf = NULL;

    if (param_count > 0 && params) {
        dist->params = (double *) lv00_malloc((size_t) param_count * sizeof(double));
        if (!dist->params) {
            lv00_free((void **)&dist);
            return NULL;
        }
        memcpy(dist->params, params, (size_t) param_count * sizeof(double));
    } else {
        dist->params = NULL;
    }

    /* 璁剧疆鏀拺闆?*/
    switch (type) {
        case PROB_DIST_UNIFORM:
            dist->support_lo = (param_count >= 2) ? params[0] : 0.0;
            dist->support_hi = (param_count >= 2) ? params[1] : 1.0;
            break;
        case PROB_DIST_NORMAL:
            dist->support_lo = -1e308;
            dist->support_hi = 1e308;
            break;
        case PROB_DIST_BETA:
            dist->support_lo = 0.0;
            dist->support_hi = 1.0;
            break;
        default:
            dist->support_lo = -1e308;
            dist->support_hi = 1e308;
            break;
    }

    return dist;
}

/* ========================================================================
 * prob_dist_destroy 鈥?閿€姣佹鐜囧垎甯? * ======================================================================== */

void prob_dist_destroy(ProbDistribution *dist) {
    if (dist) {
        lv00_free((void **)&dist->params);
        lv00_free((void **)&dist);
    }
}

/* ========================================================================
 * prob_dist_pdf 鈥?璁＄畻姒傜巼瀵嗗害鍑芥暟鍊? * ======================================================================== */

double prob_dist_pdf(ProbDistribution *dist, double x) {
    if (!dist)
        return 0.0;

    /* 璋冪敤鑷畾涔?PDF锛堝鏋滄彁渚涳級 */
    if (dist->type == PROB_DIST_CUSTOM && dist->pdf) {
        return dist->pdf(x, dist->params, dist->param_count);
    }

    switch (dist->type) {
        case PROB_DIST_UNIFORM: {
            double a = (dist->param_count >= 2) ? dist->params[0] : 0.0;
            double b = (dist->param_count >= 2) ? dist->params[1] : 1.0;
            if (x < a || x > b)
                return 0.0;
            return 1.0 / (b - a);
        }
        case PROB_DIST_NORMAL: {
            double mu = (dist->param_count >= 2) ? dist->params[0] : 0.0;
            double sigma = (dist->param_count >= 2) ? dist->params[1] : 1.0;
            double z = (x - mu) / sigma;
            return exp(-0.5 * z * z) / (sigma * sqrt(2.0 * M_PI));
        }
        case PROB_DIST_BETA: {
            double alpha = (dist->param_count >= 2) ? dist->params[0] : 1.0;
            double beta = (dist->param_count >= 2) ? dist->params[1] : 1.0;
            if (x < 0.0 || x > 1.0)
                return 0.0;
            /* 绠€鍖栵細浣跨敤 pow 杩戜技锛堝畬鏁村疄鐜伴渶瑕?Gamma 鍑芥暟锛?*/
            return pow(x, alpha - 1.0) * pow(1.0 - x, beta - 1.0);
        }
        default:
            return 0.0;
    }
}

/* ========================================================================
 * prob_dist_cdf 鈥?璁＄畻绱Н鍒嗗竷鍑芥暟鍊? * ======================================================================== */

double prob_dist_cdf(ProbDistribution *dist, double x) {
    if (!dist)
        return 0.0;

    if (dist->type == PROB_DIST_CUSTOM && dist->cdf) {
        return dist->cdf(x, dist->params, dist->param_count);
    }

    switch (dist->type) {
        case PROB_DIST_UNIFORM: {
            double a = (dist->param_count >= 2) ? dist->params[0] : 0.0;
            double b = (dist->param_count >= 2) ? dist->params[1] : 1.0;
            if (x < a)
                return 0.0;
            if (x > b)
                return 1.0;
            return (x - a) / (b - a);
        }
        case PROB_DIST_NORMAL: {
            double mu = (dist->param_count >= 2) ? dist->params[0] : 0.0;
            double sigma = (dist->param_count >= 2) ? dist->params[1] : 1.0;
            /* 浣跨敤 erf 杩戜技 */
            double z = (x - mu) / (sigma * sqrt(2.0));
            return 0.5 * (1.0 + erf(z));
        }
        default:
            return 0.0;
    }
}

/* ========================================================================
 * prob_dist_sample 鈥?浠庡垎甯冧腑閲囨牱
 *
 * 鏍规嵁鍒嗗竷绫诲瀷浣跨敤涓嶅悓鐨勯噰鏍锋柟娉曪細
 * - UNIFORM: 閫?CDF 绾挎€у彉鎹? * - NORMAL: Box-Muller 鍙樻崲
 * - BETA: 鎷掔粷閲囨牱
 * - DISCRETE: 绂绘暎閫?CDF
 * ======================================================================== */

int prob_dist_sample(ProbDistribution *dist, int n_samples, double **out_samples) {
    if (!dist || n_samples <= 0 || !out_samples)
        return -1;

    double *samples = (double *) lv00_malloc((size_t) n_samples * sizeof(double));
    if (!samples)
        return -1;

    for (int i = 0; i < n_samples; i++) {
        switch (dist->type) {
            case PROB_DIST_UNIFORM: {
                double a = (dist->param_count >= 2) ? dist->params[0] : 0.0;
                double b = (dist->param_count >= 2) ? dist->params[1] : 1.0;
                samples[i] = a + rand_uniform_lcg() * (b - a);
                break;
            }
            case PROB_DIST_NORMAL: {
                double mu = (dist->param_count >= 2) ? dist->params[0] : 0.0;
                double sigma = (dist->param_count >= 2) ? dist->params[1] : 1.0;
                samples[i] = mu + sigma * rand_normal_box_muller();
                break;
            }
            case PROB_DIST_BETA: {
                /* 绠€鍖栭噰鏍凤細浣跨敤闅忔満娓歌蛋 Metropolis-Hastings
             * 妗╁疄鐜帮細鐢熸垚鍧囧寑闅忔満鏁颁綔涓鸿繎浼?*/
                samples[i] = rand_uniform_lcg();
                break;
            }
            case PROB_DIST_DISCRETE: {
                /* 绂绘暎鍒嗗竷锛氶€?CDF 鏂规硶 */
                double r = rand_uniform_lcg();
                double cum = 0.0;
                int k = 0;
                for (; k < dist->param_count; k++) {
                    cum += dist->params[k];
                    if (r <= cum)
                        break;
                }
                samples[i] = (double) k;
                break;
            }
            case PROB_DIST_CUSTOM:
            default:
                samples[i] = 0.0;
                break;
        }
    }

    *out_samples = samples;
    return n_samples;
}

/* ========================================================================
 * prob_constraint_create 鈥?鍒涘缓姒傜巼绾︽潫鑺傜偣
 * ======================================================================== */

ProbConstraintNode *prob_constraint_create(int node_id, ProbDistribution *dist) {
    ProbConstraintNode *node = (ProbConstraintNode *) lv00_malloc(sizeof(ProbConstraintNode));
    if (!node)
        return NULL;

    node->base_node_id = node_id;
    node->coord_dist = dist;
    node->is_soft = (dist != NULL);
    node->probability = 1.0;
    node->pctl_formula = NULL;

    return node;
}

/* ========================================================================
 * prob_constraint_destroy 鈥?閿€姣佹鐜囩害鏉熻妭鐐? * ======================================================================== */

void prob_constraint_destroy(ProbConstraintNode *node) {
    if (node) {
        prob_dist_destroy(node->coord_dist);
        lv00_free((void **)&node->pctl_formula);
        lv00_free((void **)&node);
    }
}

/* ========================================================================
 * prob_constraint_sample 鈥?浠庢鐜囩害鏉熻妭鐐归噰鏍峰潗鏍? * ======================================================================== */

int prob_constraint_sample(ProbConstraintNode *node, int n_samples, double **out_samples) {
    if (!node || n_samples <= 0 || !out_samples)
        return -1;

    if (!node->coord_dist) {
        /* 无分布：返回 0.0（确定性坐标） */
        double *samples = (double *) lv00_malloc((size_t) n_samples * sizeof(double));
        if (!samples)
            return -1;
        for (int i = 0; i < n_samples; i++)
            samples[i] = 0.0;
        *out_samples = samples;
        return n_samples;
    }

    return prob_dist_sample(node->coord_dist, n_samples, out_samples);
}

/* ========================================================================
 * pctl_evaluate 鈥?鍦ㄧ害鏉熷浘涓婅瘎浼?PCTL 鍏紡
 *
 * 妗╁疄鐜帮細鏍规嵁 PCTL 鍏紡绫诲瀷鍋氭鏋惰瘎浼? * ======================================================================== */

bool pctl_evaluate(const ConstraintGraph *graph, const PCTLFormula *formula, double *out_probability) {
    if (!graph || !formula || !out_probability)
        return false;

    *out_probability = 0.0;

    switch (formula->type) {
        case PCTL_PROB_BOUND:
            /* 姒傜巼杈圭晫锛氭鏌ユ槸鍚︽弧瓒?P~p [ phi ]
         * 妗╋細浼板€间负 p_bound 鏈韩 */
            *out_probability = formula->p_bound;
            break;

        case PCTL_NEXT:
            /* 涓嬩竴鐘舵€侊細X phi
         * 妗╋細濡傛灉瀛樺湪閭绘帴鑺傜偣 鈫?1.0锛屽惁鍒?0.0 */
            *out_probability = (graph->node_count > 1) ? 1.0 : 0.0;
            break;

        case PCTL_UNTIL:
            /* phi U psi
         * 妗╋細鍩虹浼板€?0.5 */
            *out_probability = 0.5;
            break;

        case PCTL_EVENTUALLY:
            /* F phi锛氭渶缁堟弧瓒?         * 妗╋細濡傜姸鎬佽皳璇嶉潪绌?鈫?1.0锛屽惁鍒?0.0 */
            *out_probability = (formula->state_predicate && strlen(formula->state_predicate) > 0) ? 1.0 : 0.0;
            break;

        case PCTL_ALWAYS:
            /* G phi锛氭€绘槸婊¤冻
         * 妗╋細浼扮畻涓?1.0锛堜箰瑙傚亣璁撅級 */
            *out_probability = 1.0;
            break;

        case PCTL_STEADY_STATE:
            /* S~p [ phi ]锛氱ǔ鎬佹鐜?         * 妗╋細褰撹妭鐐规暟 > 0 鏃惰繑鍥炲潎鍖€鍒嗗竷绋虫€?*/
            *out_probability = (graph->node_count > 0) ? (1.0 / (double) graph->node_count) : 0.0;
            break;

        default:
            return false;
    }

    return true;
}

/* ========================================================================
 * pctl_check_constructibility 鈥?PCTL 鏋勯€犳€ф鏌? *
 * 閫氳繃瀵规鐜囧垎甯冭繘琛?Monte Carlo 閲囨牱锛堥粯璁?N=1000 娆★級锛? * 缁熻婊¤冻绾︽潫鐨勬湁鏁堟瀯閫犳瘮渚嬨€? * ======================================================================== */

bool pctl_check_constructibility(const ConstraintGraph *graph, double confidence) {
    if (!graph)
        return false;
    if (confidence < 0.0 || confidence > 1.0)
        return false;

    /* 涓哄浘涓瘡涓妭鐐硅繘琛?Monte Carlo 妯℃嫙 */
    int n = DEFAULT_N_SAMPLES;
    int valid_count = 0;

    /* 閬嶅巻姣忎釜绾︽潫锛屾鏌ュ彲婊¤冻鎬?*/
    for (int ci = 0; ci < graph->constraint_count; ci++) {
        Constraint *c = graph->constraints[ci];
        (void) c; /* 妗╋細鍋囪鎵€鏈夌害鏉熼兘鏈夎В */

        /* 绠€鍖栵細姣忕绾︽潫绫诲瀷鏈夌壒瀹氭湁鏁堟鐜?*/
        double valid_prob = 1.0;
        if (c->type == INTERSECTION) {
            valid_prob = 0.95; /* 鐩镐氦绾︽潫閫氬父鍙弧瓒?*/
        } else if (c->type == BETWEENNESS) {
            valid_prob = 0.90; /* 涔嬮棿绾︽潫鐣ュ井涓ユ牸 */
        }

        for (int sample = 0; sample < n; sample++) {
            if (rand_uniform_lcg() < valid_prob) {
                valid_count++;
            }
        }
    }

    /* 璁＄畻鏈夋晥鏋勯€犳瘮渚?*/
    int total_trials = n * graph->constraint_count;
    if (total_trials <= 0)
        total_trials = 1;
    double proportion = (double) valid_count / (double) total_trials;

    return proportion >= confidence;
}

/* ========================================================================
 * prob_constraint_infer 鈥?姒傜巼绾︽潫鎺ㄧ悊
 *
 * 浣跨敤璐濆彾鏂綉缁滈鏍肩殑淇″康浼犳挱锛屼粠涓€缁勬鐜囩害鏉熸帹鏂洰鏍囧彉閲忕殑缃俊搴︺€? * ======================================================================== */

bool prob_constraint_infer(const ConstraintGraph *graph, int target_var, ProbConstraintNode **constraints, int n,
                           double *out_conf) {
    if (!graph || !constraints || n <= 0 || !out_conf)
        return false;

    /* 妗╁疄鐜帮細浣跨敤鏈寸礌璐濆彾鏂帹鐞?     * 瀵规瘡涓害鏉熺嫭绔嬮噰鏍凤紝姹囨€诲悗璁＄畻缃俊鍖洪棿
     */
    double total_confidence = 0.0;
    int valid_constraints = 0;

    for (int i = 0; i < n; i++) {
        ProbConstraintNode *cn = constraints[i];
        if (!cn)
            continue;

        /* 瀵瑰綋鍓嶇害鏉熼噰鏍?*/
        int n_samples = 100;
        double *samples = NULL;
        int count = prob_constraint_sample(cn, n_samples, &samples);
        if (count <= 0 || !samples)
            continue;

        /* 璁＄畻鏍锋湰缃俊搴︼紙绠€鍖栵細浣跨敤杞害鏉熸鐜囷級 */
        double conf = cn->is_soft ? cn->probability : 1.0;
        total_confidence += conf;
        valid_constraints++;

        lv00_free((void **)&samples);
    }

    if (valid_constraints > 0) {
        *out_conf = total_confidence / (double) valid_constraints;
        if (*out_conf > 1.0)
            *out_conf = 1.0;
        if (*out_conf < 0.0)
            *out_conf = 0.0;
    } else {
        *out_conf = 0.0;
    }

    (void) target_var; /* 妗╁疄鐜颁腑鏈娇鐢ㄧ洰鏍囧彉閲?*/
    return true;
}
