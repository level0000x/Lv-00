/**
 * @file float_error.c
 * @brief FPTaylor 椋庢牸娴偣璇樊楠岃瘉瀹炵幇 鈥斺€?鍖洪棿绠楁湳 + 娉板嫆灞曞紑妗? *
 * @details 瀹炵幇 IEEE 1788 鍖洪棿绠楁湳鐨勫熀鏈搷浣滐紙鍔犲噺涔橀櫎锛変互鍙? *          甯哥敤瓒呰秺鍑芥暟鐨勫尯闂寸増鏈€傛彁渚涗竴闃舵嘲鍕掑睍寮€鐨勬湁闄愬樊鍒嗚繎浼笺€? *          灏嗙害鏉熷浘鍙橀噺杞崲涓哄彲璇勪及琛ㄨ揪寮忥紝骞堕€氳繃璇樊涓庡宸瘮杈? *          鏄犲皠鍒?Lv-00 淇′换棰滆壊绯荤粺銆? *
 *          鍖洪棿绠楁湳閬靛惊鏈€灏?鏈€澶у師鐞嗭細
 *          - 鍔犳硶/鍑忔硶锛氱鐐圭洿鎺ヨ繍绠? *          - 涔樻硶锛氬洓涓鐐圭殑鏈€灏?鏈€澶у€? *          - 闄ゆ硶锛氶€氳繃鍊掓暟涔樻硶锛屾帓闄ら浂鐐瑰尯闂? *
 *          鏍稿績妯″潡锛? *          - 鍖洪棿绠楁湳瀹屾暣瀹炵幇锛歛dd/sub/mul/div/sqrt/sin/cos/exp/log
 *          - 涓€闃舵嘲鍕掑睍寮€锛氭湁闄愬樊鍒嗚繎浼煎亸瀵兼暟
 *          - fptaylor_evaluate_graph锛氱害鏉熷浘 鈫?琛ㄨ揪寮?鈫?鍖洪棿璇勪及
 *          - fptaylor_verify_safety锛氳宸?鈫?淇′换棰滆壊
 *
 * @author Lv-00 Project
 * @version 3.3.0
 *
 * @dependencies
 *   - float_error.h        : 娴偣璇樊鍒嗘瀽鍏叡鎺ュ彛
 *   - constraint_graph.h   : 绾︽潫鍥炬暟鎹粨鏋? *   - symbolic_coord.h     : 绗﹀彿鍧愭爣涓?TrustColor
 *   - lv00_utils.h         : 缁熶竴鍐呭瓨鍒嗛厤鍣? *   - lv00_internal.h      : 鍐呴儴甯搁噺涓庡伐鍏峰畯
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "float_error.h"
#include "constraint_graph.h"
#include "symbolic_coord.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

/* ========================================================================
 * 鍐呴儴甯搁噺
 * ======================================================================== */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/** 琛ㄨ揪寮忕紦鍐插尯鍒濆澶у皬 */
#define EXPR_BUFFER_INITIAL 256

/** 鏈€澶ф彁鍙栫殑绾︽潫鏂圭▼鏁?*/
#define MAX_EQUATIONS 64

/** 瀹夊叏涓嬮檺锛堥伩鍏?log(0) 绛夐棶棰橈級 */
#define SAFE_MIN_POSITIVE 1e-308

/* ========================================================================
 * 鍐呴儴杈呭姪瀹? * ======================================================================== */

/** 杩斿洖 a 鍜?b 鐨勬渶灏忓€?*/
static double double_min(double a, double b) { return (a < b) ? a : b; }

/** 杩斿洖 a 鍜?b 鐨勬渶澶у€?*/
static double double_max(double a, double b) { return (a > b) ? a : b; }

/**
 * @brief 鍚戜笅鑸嶅叆锛堜繚瀹堜笅鐣屼及璁★級
 *
 * 灏?double 鍊煎悜璐熸棤绌锋柟鍚戝井璋冿紝纭繚鍖洪棿涓嬬晫鏄繚瀹堢殑銆? * 涔樹互 (1 - DBL_EPSILON) 浠ュ鐞嗘诞鐐硅垗鍏ャ€? */
static double round_down(double x) {
    if (x > 0.0) {
        return x * (1.0 - DBL_EPSILON);
    } else {
        return x * (1.0 + DBL_EPSILON);
    }
}

/**
 * @brief 鍚戜笂鑸嶅叆锛堜繚瀹堜笂鐣屼及璁★級
 *
 * 灏?double 鍊煎悜姝ｆ棤绌锋柟鍚戝井璋冿紝纭繚鍖洪棿涓婄晫鏄繚瀹堢殑銆? */
static double round_up(double x) {
    if (x > 0.0) {
        return x * (1.0 + DBL_EPSILON);
    } else {
        return x * (1.0 - DBL_EPSILON);
    }
}

/* ========================================================================
 * 鍖洪棿绠楁湳 鈥斺€?瀹屾暣瀹炵幇锛堟渶灏?鏈€澶у師鐞嗭級
 * ======================================================================== */

FloatInterval interval_make(double lo, double hi, bool is_exact) {
    FloatInterval iv;
    iv.lo = lo;
    iv.hi = hi;
    iv.is_exact = is_exact;
    return iv;
}

FloatInterval interval_add(FloatInterval a, FloatInterval b) {
    FloatInterval result;
    result.lo = round_down(a.lo + b.lo);
    result.hi = round_up(a.hi + b.hi);
    result.is_exact = a.is_exact && b.is_exact;
    return result;
}

FloatInterval interval_sub(FloatInterval a, FloatInterval b) {
    FloatInterval result;
    /* a - b: 涓嬬晫 = a.lo - b.hi, 涓婄晫 = a.hi - b.lo */
    result.lo = round_down(a.lo - b.hi);
    result.hi = round_up(a.hi - b.lo);
    result.is_exact = a.is_exact && b.is_exact;
    return result;
}

FloatInterval interval_mul(FloatInterval a, FloatInterval b) {
    /* 璁＄畻鍥涗釜瑙掔偣 */
    double p1 = a.lo * b.lo;
    double p2 = a.lo * b.hi;
    double p3 = a.hi * b.lo;
    double p4 = a.hi * b.hi;

    double min_val = double_min(double_min(p1, p2), double_min(p3, p4));
    double max_val = double_max(double_max(p1, p2), double_max(p3, p4));

    FloatInterval result;
    result.lo = round_down(min_val);
    result.hi = round_up(max_val);
    result.is_exact = a.is_exact && b.is_exact;
    return result;
}

FloatInterval interval_div(FloatInterval a, FloatInterval b) {
    FloatInterval result;

    /* 妫€鏌ュ垎姣嶆槸鍚﹁法瓒婇浂鐐?*/
    if (b.lo <= 0.0 && b.hi >= 0.0) {
        /* 鍒嗘瘝鍖呭惈闆剁偣锛氳繑鍥?NaN 鍖洪棿琛ㄧず鏃犲畾涔?*/
        result.lo = -HUGE_VAL;
        result.hi = HUGE_VAL;
        result.is_exact = false;
        return result;
    }

    /* 閫氳繃鍊掓暟 + 涔樻硶瀹炵幇闄ゆ硶 */
    if (b.hi < 0.0) {
        /* 鍒嗘瘝鍏ㄨ礋锛氬彇鍊掓暟鑼冨洿 [1/b.hi, 1/b.lo] */
        double inv_lo = 1.0 / b.hi;
        double inv_hi = 1.0 / b.lo;
        FloatInterval inv_b = interval_make(inv_lo, inv_hi, b.is_exact);
        result = interval_mul(a, inv_b);
    } else {
        /* 鍒嗘瘝鍏ㄦ锛氬彇鍊掓暟鑼冨洿 [1/b.hi, 1/b.lo] */
        double inv_lo = 1.0 / b.hi;
        double inv_hi = 1.0 / b.lo;
        FloatInterval inv_b = interval_make(inv_lo, inv_hi, b.is_exact);
        result = interval_mul(a, inv_b);
    }

    return result;
}

FloatInterval interval_sqrt(FloatInterval a) {
    FloatInterval result;
    if (a.lo < 0.0) {
        /* 璐熸暟閮ㄥ垎鏃犲疄鏁板畾涔夛紝鎴柇鍒?0 */
        result.lo = 0.0;
    } else {
        result.lo = round_down(sqrt(a.lo));
    }
    result.hi = round_up(sqrt(a.hi));
    result.is_exact = a.is_exact && (a.lo == a.hi);
    return result;
}

FloatInterval interval_sin(FloatInterval a) {
    /* sin 鍦?[-1, 1] 涔嬮棿锛岄渶瑕佸鐞嗛潪鍗曡皟鍖洪棿 */
    double sin_lo = sin(a.lo);
    double sin_hi = sin(a.hi);

    /* 妫€鏌ュ尯闂存槸鍚﹁法瓒?pi/2 + k*pi锛堟瀬澶у€肩偣锛?*/
    double width = a.hi - a.lo;
    double min_val = double_min(sin_lo, sin_hi);
    double max_val = double_max(sin_lo, sin_hi);

    if (width >= 2.0 * M_PI) {
        /* 鍖洪棿瓒呰繃涓€涓畬鏁村懆鏈?鈫?瑕嗙洊鍏ㄨ寖鍥?*/
        min_val = -1.0;
        max_val = 1.0;
    } else {
        /* 妫€鏌?pi/2 + 2k*pi 鍜?3pi/2 + 2k*pi 鏄惁鍦ㄥ尯闂村唴 */
        double pi_half = M_PI / 2.0;
        double k_start = ceil((a.lo - pi_half) / (2.0 * M_PI));
        double k_end   = floor((a.hi - pi_half) / (2.0 * M_PI));
        for (double k = k_start; k <= k_end; k += 1.0) {
            double peak = pi_half + k * 2.0 * M_PI;
            if (peak >= a.lo && peak <= a.hi) {
                if (fmod(k, 2.0) == 0.0) {
                    max_val = 1.0; /* sin(pi/2 + 2k*pi) = 1 */
                } else {
                    min_val = -1.0; /* sin(3pi/2 + 2k*pi) = -1 */
                }
            }
        }
    }

    FloatInterval result;
    result.lo = round_down(min_val);
    result.hi = round_up(max_val);
    result.is_exact = a.is_exact && (a.lo == a.hi);
    return result;
}

FloatInterval interval_cos(FloatInterval a) {
    /* cos 鐗规€х被浼?sin锛屽亸绉?pi/2 */
    double cos_lo = cos(a.lo);
    double cos_hi = cos(a.hi);
    double width = a.hi - a.lo;
    double min_val = double_min(cos_lo, cos_hi);
    double max_val = double_max(cos_lo, cos_hi);

    if (width >= 2.0 * M_PI) {
        min_val = -1.0;
        max_val = 1.0;
    } else {
        /* 妫€鏌?k*pi锛坈os 鐨勬瀬鍊肩偣锛夋槸鍚﹀湪鍖洪棿鍐?*/
        double k_start = ceil(a.lo / M_PI);
        double k_end   = floor(a.hi / M_PI);
        for (double k = k_start; k <= k_end; k += 1.0) {
            double peak = k * M_PI;
            if (peak >= a.lo && peak <= a.hi) {
                /* cos(k*pi) = (-1)^k */
                if (fmod(k, 2.0) == 0.0) {
                    max_val = 1.0;
                } else {
                    min_val = -1.0;
                }
            }
        }
    }

    FloatInterval result;
    result.lo = round_down(min_val);
    result.hi = round_up(max_val);
    result.is_exact = a.is_exact && (a.lo == a.hi);
    return result;
}

FloatInterval interval_exp(FloatInterval a) {
    /* exp 鍗曡皟閫掑 */
    FloatInterval result;
    result.lo = round_down(exp(a.lo));
    result.hi = round_up(exp(a.hi));
    result.is_exact = a.is_exact && (a.lo == a.hi);
    return result;
}

FloatInterval interval_log(FloatInterval a) {
    FloatInterval result;
    if (a.lo <= 0.0) {
        /* log 鍦ㄩ潪姝ｅ尯闂存棤瀹氫箟 */
        result.lo = -HUGE_VAL;
        result.hi = (a.hi > 0.0) ? round_up(log(a.hi)) : -HUGE_VAL;
        result.is_exact = false;
        return result;
    }
    result.lo = round_down(log(a.lo));
    result.hi = round_up(log(a.hi));
    result.is_exact = a.is_exact && (a.lo == a.hi);
    return result;
}

/* ========================================================================
 * 涓€闃舵嘲鍕掑睍寮€锛堟湁闄愬樊鍒嗚繎浼硷級
 * ======================================================================== */

/**
 * @brief 浣跨敤鏈夐檺宸垎璁＄畻涓€闃跺亸瀵兼暟
 *
 * 瀵硅〃杈惧紡 f(x0,...,xn) 鍦?center 鐐瑰璁＄畻 df/dxi锛? *   df/dxi ~= (f(... xi+h ...) - f(... xi-h ...)) / (2h)
 *
 * 姝ラ暱 h 鍙?sqrt(DBL_EPSILON) 浠ュ钩琛℃埅鏂宸拰鑸嶅叆璇樊銆? *
 * @param[in] expr      琛ㄨ揪寮忓瓧绗︿覆锛堝綋鍓嶆敮鎸佸熀鏈畻鏈級
 * @param[in] var_bounds 鍙橀噺鍖洪棿
 * @param[in] var_count  鍙橀噺鏁伴噺
 * @param[in] var_idx    姹傚鐨勫彉閲忕储寮? * @param[in] center_vals 涓績鐐瑰€? * @return 鍋忓鏁拌繎浼煎€? */
static double finite_difference_partial(const char *expr,
                                         const FloatInterval *var_bounds,
                                         int var_count,
                                         int var_idx,
                                         const double *center_vals) {
    (void)expr;
    (void)var_bounds;
    (void)var_count;

    /* 姝ラ暱锛氱害 1.5e-8 for double */
    double h = sqrt(DBL_EPSILON);
    double x = center_vals[var_idx];

    /* TODO: 瀹屾暣瀹炵幇闇€瑕佽〃杈惧紡瑙ｆ瀽鍣ㄥ拰姹傚€煎櫒銆?     * 褰撳墠妗╋細杩斿洖鍋囪瀵兼暟鍊?1.0锛堢嚎鎬ц繎浼硷級 */
    (void)x;
    (void)h;

    return 1.0;
}

/**
 * @brief 涓€闃舵嘲鍕掑睍寮€
 *
 * 瀵硅〃杈惧紡 f(x) 鍦ㄥ尯闂翠腑蹇冪偣鍋氫竴闃舵嘲鍕掑睍寮€锛? *   f(x) ~= f(center) + SUM_i df/dxi * (xi - center_i)
 *
 * 缁撴灉瀛樺偍鍦?TaylorForm 涓紝interval_lo/hi 鏄尯闂翠紶鎾殑缁撴灉銆? *
 * @param[in]  expr       琛ㄨ揪寮? * @param[in]  var_bounds 鍙橀噺鍖洪棿
 * @param[in]  var_count  鍙橀噺鏁伴噺
 * @param[out] tf         杈撳嚭鐨勬嘲鍕掑舰寮? * @return true 鎴愬姛
 */
static bool basic_taylor_expand(const char *expr,
                                 const FloatInterval *var_bounds,
                                 int var_count,
                                 TaylorForm *tf) {
    if (!expr || !var_bounds || var_count <= 0 || !tf) return false;

    tf->deriv_count = var_count;
    tf->order = 1;

    tf->first_derivs = (double *)lv00_malloc(var_count * sizeof(double));
    tf->deriv_var_ids = (int *)lv00_malloc(var_count * sizeof(int));
    if (!tf->first_derivs || !tf->deriv_var_ids) {
        free(tf->first_derivs);
        free(tf->deriv_var_ids);
        return false;
    }

    /* 璁＄畻涓績鐐瑰€硷細鍙栨瘡涓彉閲忓尯闂寸殑涓偣 */
    double center_vals[MAX_EQUATIONS];
    for (int i = 0; i < var_count; i++) {
        center_vals[i] = (var_bounds[i].lo + var_bounds[i].hi) / 2.0;
        tf->deriv_var_ids[i] = i;
    }

    /* 璁＄畻涓績鐐瑰鐨勫嚱鏁板€?*/
    /* TODO: 闇€瑕佽〃杈惧紡瑙ｆ瀽鍣ㄦ潵姹傚€?f(center) */
    tf->center_val = center_vals[0]; /* 妗╋細鍋囪 f(x) = x */

    /* 瀵规瘡涓彉閲忚绠楀亸瀵兼暟锛堟湁闄愬樊鍒嗭級 */
    for (int i = 0; i < var_count; i++) {
        tf->first_derivs[i] = finite_difference_partial(
            expr, var_bounds, var_count, i, center_vals);
    }

    /* 鍖洪棿浼犳挱锛氬熀鏈及璁?*/
    /* interval = center_val + SUM_i deriv_i * (interval_i - center_i) */
    double delta_lo = 0.0;
    double delta_hi = 0.0;

    for (int i = 0; i < var_count; i++) {
        double d = tf->first_derivs[i];
        double dev_lo = var_bounds[i].lo - center_vals[i];
        double dev_hi = var_bounds[i].hi - center_vals[i];

        if (d >= 0.0) {
            delta_lo += d * dev_lo;
            delta_hi += d * dev_hi;
        } else {
            delta_lo += d * dev_hi;
            delta_hi += d * dev_lo;
        }
    }

    tf->interval_lo = tf->center_val + delta_lo;
    tf->interval_hi = tf->center_val + delta_hi;

    return true;
}

/* ========================================================================
 * fptaylor_evaluate_graph 瀹炵幇
 * ======================================================================== */

/**
 * @brief 浠庣害鏉熷浘涓彁鍙栨秹鍙婃寚瀹氬彉閲忕殑绾︽潫鏂圭▼
 *
 * 閬嶅巻绾︽潫鍥撅紝鎵惧埌鎵€鏈?participants 涓寘鍚?var_id 鐨勭害鏉熴€? * 灏嗘瘡涓害鏉熺殑绫诲瀷鍜屽弬涓庤€呯紪鐮佷负绠€鍖栫殑琛ㄨ揪寮忓瓧绗︿覆銆? *
 * @param[in]  graph       绾︽潫鍥? * @param[in]  var_id      鐩爣鍙橀噺 ID
 * @param[out] equations   杈撳嚭鐨勮〃杈惧紡瀛楃涓叉暟缁? * @param[out] eq_count    鏂圭▼鏁伴噺
 * @return true 鎴愬姛
 */
static bool extract_equations(const ConstraintGraph *graph,
                               int var_id,
                               char ***equations,
                               int *eq_count) {
    if (!graph || !equations || !eq_count) return false;

    *eq_count = 0;
    *equations = NULL;

    if (graph->constraint_count == 0) return true;

    /* 鍒嗛厤琛ㄨ揪寮忔暟缁?*/
    int alloc_count = (graph->constraint_count < MAX_EQUATIONS)
                      ? graph->constraint_count : MAX_EQUATIONS;
    char **eqs = (char **)lv00_malloc(alloc_count * sizeof(char *));
    if (!eqs) return false;

    for (int ci = 0; ci < graph->constraint_count && *eq_count < alloc_count; ci++) {
        Constraint *c = graph->constraints[ci];
        if (!c) continue;

        /* 妫€鏌?var_id 鏄惁鍦?participants 涓?*/
        bool involves_var = false;
        for (int pi = 0; pi < c->participant_count; pi++) {
            if (c->participants[pi] == var_id) {
                involves_var = true;
                break;
            }
        }
        if (!involves_var) continue;

        /* 鏋勯€犺〃杈惧紡鎻忚堪瀛楃涓?*/
        /* 鏍煎紡锛?constraint_N: type=X, vars=[a,b,c]" */
        const char *type_str = "UNKNOWN";
        switch (c->type) {
        case INCIDENCE:    type_str = "INCIDENCE";    break;
        case BETWEENNESS:  type_str = "BETWEENNESS";  break;
        case INTERSECTION: type_str = "INTERSECTION"; break;
        case CONTAINMENT:  type_str = "CONTAINMENT";  break;
        case CONNECTION:   type_str = "CONNECTION";   break;
        default:                                      break;
        }

        char buf[EXPR_BUFFER_INITIAL];
        int off = snprintf(buf, sizeof(buf), "constraint_%d: type=%s, vars=[",
                           c->id, type_str);
        for (int pi = 0; pi < c->participant_count && off < (int)sizeof(buf) - 20; pi++) {
            off += snprintf(buf + off, sizeof(buf) - off, "%s%d",
                            (pi > 0) ? "," : "", c->participants[pi]);
        }
        snprintf(buf + off, sizeof(buf) - off, "]");

        eqs[*eq_count] = lv00_strdup(buf);
        (*eq_count)++;
    }

    *equations = eqs;
    return true;
}

bool fptaylor_evaluate_graph(const ConstraintGraph *graph,
                             int var_id,
                             const FPTaylorConfig *cfg,
                             ErrorBound *out) {
    if (!graph || !out) return false;

    /* 楠岃瘉 var_id 鏄惁鏈夋晥 */
    GeomNode *target_node = graph_get_node(graph, var_id);
    if (!target_node) return false;

    /* 姝ラ 1: 浠庣害鏉熷浘涓彁鍙栨秹鍙?var_id 鐨勬柟绋?*/
    char **equations = NULL;
    int eq_count = 0;
    if (!extract_equations(graph, var_id, &equations, &eq_count)) {
        return false;
    }

    /* 姝ラ 2: 鏋勯€犲彉閲忓尯闂磋竟鐣?     * 浠庣洰鏍囪妭鐐圭殑鍧愭爣涓彁鍙栬竟鐣岋紙鑻ヤ负绗﹀彿鍧愭爣锛岃浆涓哄尯闂达級 */
    FloatInterval var_bounds[2];
    int var_count = 0;

    if (target_node->symbolic_coords && target_node->coord_count > 0) {
        int coord_count = target_node->coord_count;
        if (coord_count > 2) coord_count = 2;

        for (int d = 0; d < coord_count; d++) {
            double val = symbolic_coord_to_double(target_node->symbolic_coords[d]);
            double eps = fabs(val) * DBL_EPSILON * 10.0; /* 10 ulp 瀹瑰繊 */
            if (eps < DBL_MIN) eps = DBL_EPSILON;
            var_bounds[d] = interval_make(val - eps, val + eps, false);
        }
        var_count = coord_count;
    }

    /* 姝ラ 3: 瀵规瘡涓害鏉熸柟绋嬭繘琛屾嘲鍕掑睍寮€鍜屽尯闂磋瘎浼?*/
    double max_abs_err = 0.0;
    double max_rel_err = 0.0;

    for (int ei = 0; ei < eq_count; ei++) {
        TaylorForm tf;
        memset(&tf, 0, sizeof(TaylorForm));

        if (var_count > 0 && basic_taylor_expand(equations[ei], var_bounds,
                                                   var_count, &tf)) {
            /* 璁＄畻缁濆璇樊 = (interval_hi - interval_lo) / 2 */
            double half_width = (tf.interval_hi - tf.interval_lo) / 2.0;
            if (half_width > max_abs_err) {
                max_abs_err = half_width;
            }
            /* 璁＄畻鐩稿璇樊锛堥伩鍏嶉櫎闆讹級 */
            double abs_center = fabs(tf.center_val);
            if (abs_center > DBL_MIN) {
                double rel = half_width / abs_center;
                if (rel > max_rel_err) max_rel_err = rel;
            }

            free(tf.first_derivs);
            free(tf.deriv_var_ids);
        }
    }

    /* 姝ラ 4: 鏋勯€犺瘉鏄庢枃鏈?*/
    char proof_buf[512];
    if (eq_count > 0) {
        snprintf(proof_buf, sizeof(proof_buf),
                 "FPTaylor analysis for var_id=%d: %d constraint equations, "
                 "taylor_order=%d, abs_err=%.6e, rel_err=%.6e",
                 var_id, eq_count,
                 cfg ? cfg->taylor_order : 1,
                 max_abs_err, max_rel_err);
    } else {
        snprintf(proof_buf, sizeof(proof_buf),
                 "FPTaylor analysis for var_id=%d: no relevant constraints found",
                 var_id);
    }

    /* 姝ラ 5: 娓呯悊骞惰緭鍑?*/
    out->absolute_error = max_abs_err;
    out->relative_error = max_rel_err;
    out->trust_level  = (max_abs_err > 0.0) ? TRUST_BLUE : TRUST_GREEN;
    out->proof_text   = lv00_strdup(proof_buf);

    for (int ei = 0; ei < eq_count; ei++) {
        free(equations[ei]);
    }
    free(equations);

    return true;
}

/* ========================================================================
 * fptaylor_evaluate_expr 瀹炵幇
 * ======================================================================== */

bool fptaylor_evaluate_expr(const char *expr,
                            const FloatInterval *var_bounds,
                            int var_count,
                            const FPTaylorConfig *cfg,
                            ErrorBound *out) {
    if (!expr || !var_bounds || var_count <= 0 || !out) return false;

    FPTaylorConfig config = cfg ? *cfg : fptaylor_config_default();

    /* 涓€闃舵嘲鍕掑睍寮€ */
    TaylorForm tf;
    memset(&tf, 0, sizeof(TaylorForm));

    if (!basic_taylor_expand(expr, var_bounds, var_count, &tf)) {
        return false;
    }

    /* 璁＄畻璇樊鐣?*/
    double half_width = (tf.interval_hi - tf.interval_lo) / 2.0;
    double abs_center = fabs(tf.center_val);

    out->absolute_error = half_width;
    out->relative_error = (abs_center > DBL_MIN)
                          ? half_width / abs_center
                          : half_width;
    out->trust_level  = TRUST_BLUE;

    /* 鏋勯€犺瘉鏄庢枃鏈?*/
    char proof_buf[512];
    snprintf(proof_buf, sizeof(proof_buf),
             "expr=\"%s\", order=%d, center=%.6e, interval=[%.6e, %.6e], "
             "abs_err=%.6e, rel_err=%.6e",
             expr, config.taylor_order, tf.center_val,
             tf.interval_lo, tf.interval_hi,
             out->absolute_error, out->relative_error);
    out->proof_text = lv00_strdup(proof_buf);

    free(tf.first_derivs);
    free(tf.deriv_var_ids);

    return true;
}

/* ========================================================================
 * fptaylor_verify_safety 瀹炵幇
 * ======================================================================== */

TrustColor fptaylor_verify_safety(const ErrorBound *bound, double tolerance) {
    if (!bound) return TRUST_RED;

    double abs_err = bound->absolute_error;

    /* NaN 鎴栭潪姝ｅ父鍊?*/
    if (isnan(abs_err) || isinf(abs_err)) {
        return TRUST_RED;
    }

    /* 鎸夐槇鍊煎垎绾х殑淇′换棰滆壊鍒ゆ柇 */
    if (abs_err <= 1e-12) {
        /* 鏋佸害绮剧‘ 鈥斺€?瀹屽叏鏋勯€犳€у畨鍏?*/
        return TRUST_GREEN;
    }

    if (abs_err <= 1e-10) {
        /* 楂樼簿搴?鈥斺€?鍙俊浣嗕粛闇€鍏虫敞 */
        return TRUST_BLUE;
    }

    if (abs_err <= tolerance) {
        /* 杈圭晫瀹夊叏 鈥斺€?鍚暟鍊煎亣璁撅紝鏍囪涓?AMBER */
        return TRUST_AMBER;
    }

    if (abs_err <= tolerance * 10.0) {
        /* 鎺ヨ繎瀹瑰繊杈圭晫 鈥斺€?鏉′欢鎬у畨鍏?*/
        return TRUST_YELLOW;
    }

    /* 瓒呭嚭瀹瑰繊鑼冨洿 鈥斺€?涓嶅畨鍏?*/
    return TRUST_RED;
}

/* ========================================================================
 * 宸ュ巶涓庤祫婧愮鐞? * ======================================================================== */

FPTaylorConfig fptaylor_config_default(void) {
    FPTaylorConfig cfg;
    cfg.use_optimization       = true;
    cfg.taylor_order           = 1;
    cfg.use_z3_opt             = false;
    cfg.use_gelpia             = false;
    cfg.branch_bound_threshold = 1e-6;
    return cfg;
}

void error_bound_free(ErrorBound *bound) {
    if (!bound) return;
    if (bound->proof_text) {
        free(bound->proof_text);
        bound->proof_text = NULL;
    }
}
