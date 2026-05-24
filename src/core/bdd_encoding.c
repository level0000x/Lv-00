/**
 * @file bdd_encoding.c
 * @brief CUDD 浜屽弶鍐崇瓥鍥剧紪鐮?鈥斺€?妗╁疄鐜? *
 * 鎻愪緵 BDD/ADD 鐨勫熀鏈搷浣滃疄鐜帮紝鍖呮嫭甯冨皵杩愮畻銆佸彉閲忓簭浼樺寲銆? * 绾︽潫鍥锯啋BDD 缂栫爜鍜屽潗鏍?bit-blasting銆? *
 * @version v3.3.0
 * @date 2026-05-24
 */

#include "bdd_encoding.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ========================================================================
 * 鍐呴儴锛氬敮涓€琛ㄥ搱甯? * ======================================================================== */

/** 鑺傜偣涓夊厓缁勫搱甯?(var_id, low, high) 鈫?鍞竴琛ㄧ储寮?*/
static int bdd_unique_hash(int var_id, BDDNode *low, BDDNode *high,
                            int table_size) {
    unsigned long h = (unsigned long)var_id;
    h = h * 31 + (unsigned long)(uintptr_t)low;
    h = h * 31 + (unsigned long)(uintptr_t)high;
    return (int)(h % (unsigned long)table_size);
}

/** 鍦ㄥ敮涓€琛ㄤ腑鏌ユ壘鎴栨彃鍏ヨ妭鐐?*/
static BDDNode *bdd_unique_lookup(BDDManager *mgr, int var_id,
                                   BDDNode *low, BDDNode *high) {
    /* 绠€鍖栧疄鐜帮細杩斿洖鏂拌妭鐐癸紙妗╋紝涓嶄娇鐢ㄧ湡姝ｅ敮涓€琛ㄧ紦瀛橈級 */
    BDDNode *node = (BDDNode *)malloc(sizeof(BDDNode));
    if (!node) return NULL;
    node->var_id = var_id;
    node->low = low;
    node->high = high;
    node->ref_count = 0;
    node->complemented = false;
    mgr->node_count++;
    return node;
}

/* ========================================================================
 * BDD 绠＄悊鍣ㄧ敓鍛藉懆鏈? * ======================================================================== */

BDDManager *bdd_manager_create(int var_count, int unique_table_size) {
    BDDManager *mgr = (BDDManager *)malloc(sizeof(BDDManager));
    if (!mgr) return NULL;

    /* 鍒涘缓缁堢 T 鑺傜偣 */
    mgr->true_node = (BDDNode *)malloc(sizeof(BDDNode));
    if (!mgr->true_node) { free(mgr); return NULL; }
    mgr->true_node->var_id = -1;
    mgr->true_node->low = NULL;
    mgr->true_node->high = NULL;
    mgr->true_node->ref_count = 1;  /* 鎸佷箙寮曠敤 */
    mgr->true_node->complemented = false;

    /* 鍒涘缓缁堢 F 鑺傜偣 */
    mgr->false_node = (BDDNode *)malloc(sizeof(BDDNode));
    if (!mgr->false_node) {
        free(mgr->true_node); free(mgr); return NULL;
    }
    mgr->false_node->var_id = -1;
    mgr->false_node->low = NULL;
    mgr->false_node->high = NULL;
    mgr->false_node->ref_count = 1;  /* 鎸佷箙寮曠敤 */
    mgr->false_node->complemented = false;

    /* 鍒嗛厤鍞竴琛紙妗╁疄鐜颁腑涓嶄娇鐢ㄥ搱甯岋紝浠呭崰浣嶏級 */
    if (unique_table_size < 1024) unique_table_size = 1024;
    mgr->unique_table = (BDDNode **)calloc((size_t)unique_table_size,
                                            sizeof(BDDNode *));
    if (!mgr->unique_table) {
        free(mgr->false_node); free(mgr->true_node); free(mgr);
        return NULL;
    }
    mgr->unique_table_size = unique_table_size;

    /* 鍙橀噺搴忔暟缁?*/
    mgr->var_order = (int *)malloc((size_t)var_count * sizeof(int));
    if (!mgr->var_order) {
        free(mgr->unique_table); free(mgr->false_node);
        free(mgr->true_node); free(mgr);
        return NULL;
    }
    for (int i = 0; i < var_count; i++) {
        mgr->var_order[i] = i;
    }
    mgr->var_count = var_count;
    mgr->node_count = 0;

    return mgr;
}

void bdd_manager_destroy(BDDManager *mgr) {
    if (!mgr) return;
    /* 娉細妗╁疄鐜颁腑涓嶉亶鍘嗗洖鏀舵墍鏈夎妭鐐癸紙瀹屾暣瀹炵幇闇€瑕?GC锛?*/
    free(mgr->true_node);
    free(mgr->false_node);
    free(mgr->unique_table);
    free(mgr->var_order);
    free(mgr);
}

int bdd_new_var(BDDManager *mgr, const char *name, BDDVarType type) {
    (void)name;
    (void)type;
    if (!mgr) return -1;
    /* 妗╋細鐩存帴杩斿洖涓嬩竴涓彲鐢ㄥ彉閲?ID */
    int id = mgr->var_count;
    mgr->var_count++;
    return id;
}

/* ========================================================================
 * BDD 鑺傜偣鍒涘缓涓庡紩鐢ㄨ鏁? * ======================================================================== */

BDDNode *bdd_true(BDDManager *mgr) {
    return mgr ? mgr->true_node : NULL;
}

BDDNode *bdd_false(BDDManager *mgr) {
    return mgr ? mgr->false_node : NULL;
}

BDDNode *bdd_literal(BDDManager *mgr, int var_id) {
    if (!mgr) return NULL;
    if (var_id > 0) {
        /* 姝ｆ枃瀛楋細var 鈫?high=T, low=F */
        return bdd_unique_lookup(mgr, var_id, mgr->false_node, mgr->true_node);
    } else {
        /* 璐熸枃瀛楋細卢var 鈫?high=F, low=T */
        return bdd_unique_lookup(mgr, -var_id, mgr->true_node, mgr->false_node);
    }
}

void bdd_ref(BDDNode *node) {
    if (node) node->ref_count++;
}

void bdd_deref(BDDNode *node) {
    if (node && node->ref_count > 0) {
        node->ref_count--;
        /* 妗╋細缁堢鑺傜偣涓嶅洖鏀?*/
    }
}

/* ========================================================================
 * BDD ITE 鈥?鏍稿績閫掑綊绠楁硶
 *
 * ite(F, G, H) = (F 鈭?G) 鈭?(卢F 鈭?H)
 *
 * 閫掑綊缁堟鏉′欢锛? * - F = T 鈫?G
 * - F = F 鈫?H
 * - G = H 鈫?G
 * - G = T 鈭?H = F 鈫?F
 * - G = F 鈭?H = T 鈫?卢F
 *
 * 涓€鑸儏鍐碉細閫夋嫨 F, G, H 涓渶灏忕殑鍙橀噺锛岄€掑綊灞曞紑銆? * ======================================================================== */

BDDNode *bdd_ite(BDDManager *mgr, BDDNode *f, BDDNode *g, BDDNode *h) {
    if (!mgr || !f || !g || !h) return NULL;

    /* 缁堢鏉′欢 */
    if (f == mgr->true_node) {
        bdd_ref(g);
        return g;
    }
    if (f == mgr->false_node) {
        bdd_ref(h);
        return h;
    }
    if (g == h) {
        bdd_ref(g);
        return g;
    }
    if (g == mgr->true_node && h == mgr->false_node) {
        bdd_ref(f);
        return f;
    }
    if (g == mgr->false_node && h == mgr->true_node) {
        return bdd_not(mgr, f);
    }

    /* 纭畾椤堕儴鍙橀噺锛氬彇涓夎€呬腑鏈€灏忕殑鍙橀噺 ID */
    int top_var = f->var_id;
    if (g->var_id >= 0 && (top_var < 0 || g->var_id < top_var))
        top_var = g->var_id;
    if (h->var_id >= 0 && (top_var < 0 || h->var_id < top_var))
        top_var = h->var_id;

    /* 鑻ラ潪缁堢锛宑ofactor锛堢畝鍖栧疄鐜颁粎姣旇緝 var_id锛?*/
    BDDNode *f_low = (f->var_id == top_var) ? f->low : f;
    BDDNode *f_high = (f->var_id == top_var) ? f->high : f;
    BDDNode *g_low = (g->var_id == top_var) ? g->low : g;
    BDDNode *g_high = (g->var_id == top_var) ? g->high : g;
    BDDNode *h_low = (h->var_id == top_var) ? h->low : h;
    BDDNode *h_high = (h->var_id == top_var) ? h->high : h;

    /* 閫掑綊 */
    BDDNode *t = bdd_ite(mgr, f_low, g_low, h_low);
    BDDNode *e = bdd_ite(mgr, f_high, g_high, h_high);

    BDDNode *result = bdd_unique_lookup(mgr, top_var, t, e);
    bdd_deref(t);
    bdd_deref(e);
    return result;
}

/* ========================================================================
 * BDD 甯冨皵杩愮畻
 * ======================================================================== */

BDDNode *bdd_and(BDDManager *mgr, BDDNode *f, BDDNode *g) {
    /* f 鈭?g = ite(f, g, F) */
    return bdd_ite(mgr, f, g, mgr->false_node);
}

BDDNode *bdd_or(BDDManager *mgr, BDDNode *f, BDDNode *g) {
    /* f 鈭?g = ite(f, T, g) */
    return bdd_ite(mgr, f, mgr->true_node, g);
}

BDDNode *bdd_not(BDDManager *mgr, BDDNode *f) {
    /* 卢f = ite(f, F, T) */
    return bdd_ite(mgr, f, mgr->false_node, mgr->true_node);
}

BDDNode *bdd_xor(BDDManager *mgr, BDDNode *f, BDDNode *g) {
    /* f 鈯?g = ite(f, 卢g, g) */
    BDDNode *not_g = bdd_not(mgr, g);
    BDDNode *result = bdd_ite(mgr, f, not_g, g);
    bdd_deref(not_g);
    return result;
}

BDDNode *bdd_nand(BDDManager *mgr, BDDNode *f, BDDNode *g) {
    /* 卢(f 鈭?g) = ite(f, 卢g, T) */
    BDDNode *not_g = bdd_not(mgr, g);
    BDDNode *result = bdd_ite(mgr, f, not_g, mgr->true_node);
    bdd_deref(not_g);
    return result;
}

/* ========================================================================
 * Sifting 鍙橀噺搴忎紭鍖? *
 * 绠楁硶锛? * 1. 瀵逛簬姣忎釜鍙橀噺 i锛? *    a. 璁板綍褰撳墠浣嶇疆鍜屽綋鍓嶈妭鐐规暟
 *    b. 灏嗗彉閲?i 浠庡彉閲忓簭涓Щ鍑? *    c. 灏濊瘯灏嗗彉閲?i 鎻掑叆鍒版瘡涓綅缃?j
 *    d. 璁板綍浣胯妭鐐规暟鏈€灏戠殑浣嶇疆
 *    e. 鍥哄畾鍙橀噺 i 鍦ㄨ浣嶇疆
 * 2. 杩斿洖鏈€缁堢殑鑺傜偣鏁? * ======================================================================== */

int bdd_reorder_sift(BDDManager *mgr) {
    if (!mgr || mgr->var_count <= 0) return -1;

    int n = mgr->var_count;
    int *best_order = (int *)malloc((size_t)n * sizeof(int));
    if (!best_order) return -1;
    memcpy(best_order, mgr->var_order, (size_t)n * sizeof(int));

    int improved = 0;

    for (int var = 0; var < n; var++) {
        int orig_pos = -1;
        /* 鏌ユ壘鍙橀噺 var 鍦?var_order 涓殑褰撳墠浣嶇疆 */
        for (int p = 0; p < n; p++) {
            if (mgr->var_order[p] == var) {
                orig_pos = p;
                break;
            }
        }
        if (orig_pos < 0) continue;

        /* 璁板綍褰撳墠浣嶇疆鐨勮妭鐐规暟 */
        uint64_t orig_nodes = mgr->node_count;

        /* 绉诲嚭鍙橀噺 var */
        for (int p = orig_pos; p < n - 1; p++) {
            mgr->var_order[p] = mgr->var_order[p + 1];
        }

        /* 灏濊瘯姣忎釜鎻掑叆浣嶇疆 */
        int best_pos = 0;
        uint64_t best_nodes = UINT64_MAX;

        for (int insert_pos = 0; insert_pos < n; insert_pos++) {
            /* 鍦?insert_pos 澶勪复鏃舵彃鍏?var */
            for (int p = n - 1; p > insert_pos; p--) {
                mgr->var_order[p] = mgr->var_order[p - 1];
            }
            mgr->var_order[insert_pos] = var;

            /* 妗╋細浣跨敤鍚彂寮忚繎浼艰瘎浼帮紙瀹屾暣瀹炵幇闇€閲嶅缓 BDD锛?*/
            uint64_t est_nodes = orig_nodes;
            if (insert_pos == orig_pos) {
                est_nodes = orig_nodes;
            } else {
                /* 绂诲師浣嶇疆瓒婅繙锛屾儵缃氳秺澶э紙绠€鍖栧惎鍙戝紡锛?*/
                int dist = (insert_pos > orig_pos) ?
                    (insert_pos - orig_pos) : (orig_pos - insert_pos);
                est_nodes = orig_nodes + (uint64_t)dist * 2;
            }

            if (est_nodes < best_nodes) {
                best_nodes = est_nodes;
                best_pos = insert_pos;
            }

            /* 鎭㈠锛氱Щ鍑?var */
            for (int p = insert_pos; p < n - 1; p++) {
                mgr->var_order[p] = mgr->var_order[p + 1];
            }
        }

        /* 灏嗗彉閲?var 鍥哄畾鍒版渶浣充綅缃?*/
        for (int p = n - 1; p > best_pos; p--) {
            mgr->var_order[p] = mgr->var_order[p - 1];
        }
        mgr->var_order[best_pos] = var;

        if (best_nodes < orig_nodes) {
            improved++;
        }
    }

    free(best_order);
    return improved;
}

/* ========================================================================
 * constraint_graph_to_bdd 鈥?绾︽潫鍥?鈫?BDD 缂栫爜
 *
 * 鏋氫妇鎵€鏈夊竷灏旂粍鍚堬紝鏋勫缓 BDD銆? * 瀵逛簬绾︽潫鍥句腑 n 涓妭鐐癸紝鏈?2^n 绉嶅竷灏旇祴鍊笺€? * 姣忕璧嬪€煎搴?BDD 鐨勪竴涓弧瓒宠矾寰勩€? * ======================================================================== */

BDDNode *constraint_graph_to_bdd(const ConstraintGraph *graph,
                                  BDDManager *mgr) {
    if (!graph || !mgr) return NULL;

    int n = graph->node_count;
    if (n <= 0) return bdd_true(mgr);

    /* 绠€鍖栨々锛氬鎵€鏈夎妭鐐瑰彉閲忓仛 AND 鐨?BDD */
    BDDNode *result = bdd_true(mgr);

    for (int i = 0; i < n; i++) {
        int var_id = i + 1;  /* 鍙橀噺 ID 浠?1 寮€濮?*/
        BDDNode *lit = bdd_literal(mgr, var_id);
        if (!lit) continue;

        BDDNode *new_result = bdd_and(mgr, result, lit);
        bdd_deref(result);
        bdd_deref(lit);
        result = new_result;
    }

    return result;
}

/* ========================================================================
 * coord_to_bdd_var 鈥?鍧愭爣 bit-blasting
 *
 * IEEE 754 鍙岀簿搴︿綅琛ㄧず锛? 浣嶇鍙?+ 11 浣嶆寚鏁?+ 52 浣嶅熬鏁?= 64 浣嶃€? * 姣忎綅缂栫爜涓轰竴涓?BDD 鍙橀噺銆? * ======================================================================== */

int coord_to_bdd_var(const SymbolicCoord *coord,
                      BDDManager *mgr,
                      int base_var) {
    if (!coord || !mgr) return -1;

    /* 64 浣?IEEE 754 鍙岀簿搴︾紪鐮?*/
    #define IEEE754_DOUBLE_BITS 64

    /* 鎻愬彇鍧愭爣鐨勬暟鍊艰繎浼硷紙浣跨敤 double锛?*/
    double value = 0.0;
    if (coord->type == RATIONAL && coord->data.rational) {
        value = mpq_get_d(coord->data.rational->value);
    }

    /* 灏?double 鐨?64 浣嶅垎鍒紪鐮佷负 BDD 鍙橀噺 */
    union {
        double d;
        uint64_t u;
    } ieee;
    ieee.d = value;

    /* 涓烘瘡涓€浣嶆敞鍐屼竴涓?BDD 鍙橀噺 */
    for (int bit = 0; bit < IEEE754_DOUBLE_BITS; bit++) {
        int var_id = base_var + bit;
        /* 鑾峰彇鎴栧垱寤哄彉閲?*/
        if (var_id >= mgr->var_count) {
            /* 鎵╁睍鍙橀噺搴?*/
            mgr->var_count = var_id + 1;
        }
    }

    return IEEE754_DOUBLE_BITS;

    #undef IEEE754_DOUBLE_BITS
}

/* ========================================================================
 * bdd_to_cnf 鈥?BDD 鈫?DIMACS CNF
 *
 * 浣跨敤 Tseitin 鍙樻崲锛氬 BDD 涓瘡涓潪缁堢鑺傜偣寮曞叆杈呭姪鍙橀噺銆? * 鑺傜偣 v = ITE(var, low, high) 鐨?Tseitin 缂栫爜锛? *   (卢v 鈭?卢var 鈭?high) 鈭?(卢v 鈭?var 鈭?low) 鈭?(v 鈭?卢var 鈭?卢high) 鈭?(v 鈭?var 鈭?卢low)
 * ======================================================================== */

bool bdd_to_cnf(BDDNode *bdd, char **out_cnf) {
    if (!bdd || !out_cnf) return false;

    /* 妗╁疄鐜帮細鐢熸垚妗嗘灦 CNF */
    size_t buf_size = 4096;
    char *buf = (char *)malloc(buf_size);
    if (!buf) return false;

    /* DIMACS 澶撮儴 */
    int offset = snprintf(buf, buf_size,
                          "c BDD-to-CNF conversion (stub)\n"
                          "p cnf 0 0\n");

    *out_cnf = buf;
    return true;
}

/* ========================================================================
 * ADD 绠＄悊鍣紙妗╁疄鐜帮級
 * ======================================================================== */

ADDManager *add_manager_create(int var_count, int unique_table_size) {
    ADDManager *mgr = (ADDManager *)malloc(sizeof(ADDManager));
    if (!mgr) return NULL;

    mgr->zero_node = (ADDNode *)malloc(sizeof(ADDNode));
    if (!mgr->zero_node) { free(mgr); return NULL; }
    mgr->zero_node->var_id = -1;
    mgr->zero_node->low = NULL;
    mgr->zero_node->high = NULL;
    mgr->zero_node->constant = 0.0;
    mgr->zero_node->is_constant = true;

    mgr->one_node = (ADDNode *)malloc(sizeof(ADDNode));
    if (!mgr->one_node) {
        free(mgr->zero_node); free(mgr); return NULL;
    }
    mgr->one_node->var_id = -1;
    mgr->one_node->low = NULL;
    mgr->one_node->high = NULL;
    mgr->one_node->constant = 1.0;
    mgr->one_node->is_constant = true;

    mgr->unique_table = NULL;
    mgr->unique_table_size = unique_table_size;
    mgr->var_count = var_count;
    mgr->node_count = 0;

    mgr->var_order = (int *)malloc((size_t)var_count * sizeof(int));
    if (mgr->var_order) {
        for (int i = 0; i < var_count; i++) mgr->var_order[i] = i;
    }

    return mgr;
}

void add_manager_destroy(ADDManager *mgr) {
    if (!mgr) return;
    free(mgr->zero_node);
    free(mgr->one_node);
    free(mgr->unique_table);
    free(mgr->var_order);
    free(mgr);
}

ADDNode *add_constant(ADDManager *mgr, double value) {
    if (!mgr) return NULL;
    ADDNode *node = (ADDNode *)malloc(sizeof(ADDNode));
    if (!node) return NULL;
    node->var_id = -1;
    node->low = NULL;
    node->high = NULL;
    node->constant = value;
    node->is_constant = true;
    return node;
}

/* ADD 杩愮畻鈥斺€旀々瀹炵幇 */
ADDNode *add_add(ADDManager *mgr, ADDNode *a, ADDNode *b) {
    if (!mgr || !a || !b) return NULL;
    if (a->is_constant && b->is_constant) {
        return add_constant(mgr, a->constant + b->constant);
    }
    return add_constant(mgr, 0.0);  /* 妗?*/
}

ADDNode *add_sub(ADDManager *mgr, ADDNode *a, ADDNode *b) {
    if (!mgr || !a || !b) return NULL;
    if (a->is_constant && b->is_constant) {
        return add_constant(mgr, a->constant - b->constant);
    }
    return add_constant(mgr, 0.0);
}

ADDNode *add_mul(ADDManager *mgr, ADDNode *a, ADDNode *b) {
    if (!mgr || !a || !b) return NULL;
    if (a->is_constant && b->is_constant) {
        return add_constant(mgr, a->constant * b->constant);
    }
    return add_constant(mgr, 0.0);
}

ADDNode *add_div(ADDManager *mgr, ADDNode *a, ADDNode *b) {
    if (!mgr || !a || !b) return NULL;
    if (a->is_constant && b->is_constant && b->constant != 0.0) {
        return add_constant(mgr, a->constant / b->constant);
    }
    return add_constant(mgr, 0.0);
}

ADDNode *add_max(ADDManager *mgr, ADDNode *a, ADDNode *b) {
    if (!mgr || !a || !b) return NULL;
    if (a->is_constant && b->is_constant) {
        return add_constant(mgr,
            (a->constant > b->constant) ? a->constant : b->constant);
    }
    return add_constant(mgr, 0.0);
}

ADDNode *add_min(ADDManager *mgr, ADDNode *a, ADDNode *b) {
    if (!mgr || !a || !b) return NULL;
    if (a->is_constant && b->is_constant) {
        return add_constant(mgr,
            (a->constant < b->constant) ? a->constant : b->constant);
    }
    return add_constant(mgr, 0.0);
}
