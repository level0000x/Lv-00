/**
 * @file geometry_compress.c
 * @brief Draco 椋庢牸鍑犱綍鏁版嵁鍘嬬缉瀹炵幇 鈥斺€?Edgebreaker 鎷撴墤缂栫爜 + 棰勬祴缂栫爜妗? *
 * @details 瀹炵幇鍩轰簬 Edgebreaker CLERS 绠楁硶鐨勫嚑浣曟嫇鎵戝帇缂┿€佸钩琛屽洓杈瑰舰
 *          棰勬祴缂栫爜銆佷互鍙?.lvzd 鏍煎紡浜岃繘鍒?I/O銆傚綋鍓嶄负妗╁疄鐜扮増鏈紝
 *          鎻愪緵鍩烘湰鍘嬬缉/瑙ｅ帇妗嗘灦锛岀喌缂栫爜閮ㄥ垎鏍囨敞 TODO 寰呭悗缁凯浠ｅ畬鍠勩€? *
 *          鏍稿績妯″潡锛? *          - geometry_compress锛氬畬鏁村帇缂╃绾挎鏋? *          - geometry_decompress锛氶€嗗帇缂╃绾挎鏋? *          - edgebreaker_encode锛氭嫇鎵?CLERS 绗﹀彿搴忓垪鐢熸垚
 *          - predictive_encode_coords锛氬钩琛屽洓杈瑰舰鍧愭爣棰勬祴
 *          - .lvzd I/O锛氫簩杩涘埗鏂囦欢璇诲啓
 *
 * @author Lv-00 Project
 * @version 3.3.0
 *
 * @dependencies
 *   - geometry_compress.h  : 鍘嬬缉绠＄嚎鍏叡鎺ュ彛
 *   - constraint_graph.h   : 绾︽潫鍥炬暟鎹粨鏋? *   - symbolic_coord.h     : 绗﹀彿鍧愭爣绯荤粺
 *   - lv00_utils.h         : 缁熶竴鍐呭瓨鍒嗛厤鍣? *   - lv00_internal.h      : 鍐呴儴甯搁噺涓庡伐鍏峰畯
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "geometry_compress.h"
#include "constraint_graph.h"
#include "symbolic_coord.h"
#include "lv00_internal.h"
#include "lv00_utils.h"

/* ========================================================================
 * 鍐呴儴甯搁噺
 * ======================================================================== */

/** 杈圭晫鏍堝垵濮嬪閲?*/
#define BOUNDARY_STACK_INITIAL 64

/** CLERS 搴忓垪鍒濆瀹归噺 */
#define CLERS_SEQUENCE_INITIAL 256

/** .lvzd 璇荤紦鍐插尯鍒濆澶у皬 */
#define LVZD_READ_BUFFER_INITIAL 4096

/** 鍧愭爣缁村害锛?D 鎴?3D 寮犻噺锛?*/
#ifndef COORD_DIM
#define COORD_DIM 2
#endif

/* ========================================================================
 * 鍐呴儴杈呭姪缁撴瀯浣? * ======================================================================== */

/**
 * @brief 杈圭粨鏋勪綋 鈥斺€?鐢ㄤ簬 Edgebreaker 閬嶅巻
 */
typedef struct {
    int v0;       /**< 杈硅捣鐐硅妭鐐?ID */
    int v1;       /**< 杈圭粓鐐硅妭鐐?ID */
} Edge;

/**
 * @brief 涓夎褰㈤潰缁撴瀯浣?鈥斺€?閫氳繃绾︽潫鍏崇郴鎺ㄦ柇
 */
typedef struct {
    int verts[3]; /**< 涓変釜椤剁偣鑺傜偣 ID */
    bool visited; /**< 鏄惁宸插湪閬嶅巻涓闂?*/
} TriangleFace;

/* ========================================================================
 * 榛樿鍘嬬缉閰嶇疆宸ュ巶鍑芥暟锛堝唴閮級
 * ======================================================================== */

static CompressConfig compress_config_default(void) {
    CompressConfig cfg;
    cfg.pred_mode         = PREDICT_PARALLELOGRAM;
    cfg.entropy           = ENTROPY_RANS;
    cfg.quantization_bits = 0;
    cfg.lossless          = true;
    cfg.max_error         = 0.0;
    return cfg;
}

/* ========================================================================
 * 棰勬祴缂栫爜瀹炵幇
 * ======================================================================== */

/**
 * @brief 骞宠鍥涜竟褰㈤娴嬬紪鐮? *
 * 閬嶅巻绾︽潫鍥句腑浠庣害鏉熷叧绯绘帹鏂殑涓夎褰㈤潰锛屽姣忎釜鏈闂殑瀵归《鐐? * 鐢ㄥ钩琛屽洓杈瑰舰娉曞垯棰勬祴骞跺瓨鍌ㄦ畫宸€? *
 * 绠楁硶锛氬浜庝笁瑙掑舰 (v0, v1, v2)锛岃 v2 涓哄緟棰勬祴椤剁偣锛? * 鏌ユ壘鍏变韩杈?(v0, v1) 鐨勭浉閭讳笁瑙掑舰瀵归《鐐?v_opp锛? * 棰勬祴鍊硷細pred = coord(v0) + coord(v1) - coord(v_opp)
 * 娈嬪樊锛歝oord(v2) = coord(v2) - pred锛堝師鍦颁慨鏀癸級
 *
 * @param[in,out] graph 绾︽潫鍥? * @return true 鎴愬姛锛宖alse 澶辫触
 */
static bool predictive_encode_parallelogram(ConstraintGraph *graph) {
    if (!graph) return false;

    int node_count = graph->node_count;
    if (node_count < 3) return true; /* 灏戜簬 3 涓妭鐐癸紝鏃犳硶鏋勬垚涓夎褰?*/

    /* 浣跨敤绠€鍗曠殑宸茶闂爣璁版暟缁?*/
    bool *visited = (bool *)lv00_malloc(node_count * sizeof(bool));
    if (!visited) return false;
    memset(visited, 0, node_count * sizeof(bool));

    /* 閬嶅巻鑺傜偣锛氭煡鎵炬湁鍧愭爣鐨勫嚑浣曠偣 */
    for (int i = 0; i < node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || !node->symbolic_coords || node->coord_count < COORD_DIM) continue;
        if (visited[node->id]) continue;
        visited[node->id] = true;
    }

    /* TODO: 瀹屾暣瀹炵幇闇€瑕佸厛鎻愬彇涓夎褰㈤潰鎷撴墤锛堜粠绾︽潫鍏崇郴涓帹鏂級銆?     * 褰撳墠妗╁疄鐜帮細鏍囪宸茶闂絾涓嶄慨鏀瑰潗鏍囥€?*/
    free(visited);
    return true;
}

/**
 * @brief 宸垎棰勬祴缂栫爜
 *
 * 鎸夎妭鐐?ID 椤哄簭閬嶅巻锛屽皢姣忎釜鑺傜偣鐨勫潗鏍囨浛鎹负涓庡墠涓€涓妭鐐圭殑宸€笺€? *
 * @param[in,out] graph 绾︽潫鍥? * @return true 鎴愬姛锛宖alse 澶辫触
 */
static bool predictive_encode_delta(ConstraintGraph *graph) {
    if (!graph) return false;

    int node_count = graph->node_count;
    if (node_count < 2) return true;

    /* 淇濆瓨绗竴涓妭鐐圭殑鍧愭爣浣滀负鍙傝€冨€?*/
    GeomNode *prev = NULL;

    for (int i = 0; i < node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || !node->symbolic_coords || node->coord_count < COORD_DIM) continue;

        if (prev) {
            /* 璁＄畻宸€硷細node - prev锛屽瓨鍌ㄥ埌 node */
            for (int d = 0; d < COORD_DIM; d++) {
                SymbolicCoord *diff = symbolic_coord_subtract(
                    node->symbolic_coords[d],
                    prev->symbolic_coords[d]);
                if (diff) {
                    symbolic_coord_destroy(node->symbolic_coords[d]);
                    node->symbolic_coords[d] = diff;
                }
            }
        }
        prev = node;
    }

    return true;
}

/* ========================================================================
 * 鍏叡棰勬祴缂栫爜鎺ュ彛
 * ======================================================================== */

bool predictive_encode_coords(ConstraintGraph *graph, PredictionMode mode) {
    if (!graph) return false;

    switch (mode) {
    case PREDICT_PARALLELOGRAM:
        return predictive_encode_parallelogram(graph);

    case PREDICT_MULTI_PARALLELOGRAM:
        /* TODO: 澶氶樁骞宠鍥涜竟褰㈤娴?鈥斺€?鍔犳潈骞冲潎澶氫釜閭婚潰 */
        return predictive_encode_parallelogram(graph);

    case PREDICT_DELTA:
        return predictive_encode_delta(graph);

    case PREDICT_NONE:
        /* 鏃犻娴嬶細鐩存帴淇濈暀鍘熷鍧愭爣 */
        return true;

    default:
        return false;
    }
}

/* ========================================================================
 * Edgebreaker 缂栫爜瀹炵幇
 * ======================================================================== */

bool edgebreaker_encode(const ConstraintGraph *graph,
                        EdgebreakerMode **modes,
                        int *seq_len) {
    if (!graph || !modes || !seq_len) return false;

    /* 鍒濆鍖?CLERS 搴忓垪缂撳啿鍖?*/
    int capacity = CLERS_SEQUENCE_INITIAL;
    EdgebreakerMode *seq = (EdgebreakerMode *)lv00_malloc(
        capacity * sizeof(EdgebreakerMode));
    if (!seq) return false;

    int len = 0;

    /* 杈圭晫鏍堬細浣跨敤绠€鍗曟暟缁勬ā鎷?*/
    Edge *boundary = (Edge *)lv00_malloc(
        BOUNDARY_STACK_INITIAL * sizeof(Edge));
    if (!boundary) {
        free(seq);
        return false;
    }
    int boundary_top = 0;
    int boundary_capacity = BOUNDARY_STACK_INITIAL;

    /* 鑺傜偣璁块棶鏍囪 */
    bool *visited = (bool *)lv00_malloc(
        graph->node_count * sizeof(bool));
    if (!visited) {
        free(seq);
        free(boundary);
        return false;
    }
    memset(visited, 0, graph->node_count * sizeof(bool));

    /* 鏌ユ壘鍒濆杈癸細鍙栧墠涓や釜鐐圭被鍨嬬殑鑺傜偣 */
    int start_v0 = -1, start_v1 = -1;
    for (int i = 0; i < graph->node_count && start_v1 < 0; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node) continue;
        if (node->type == GEOM_POINT) {
            if (start_v0 < 0) {
                start_v0 = node->id;
                visited[node->id] = true;
            } else if (start_v1 < 0) {
                start_v1 = node->id;
                visited[node->id] = true;
            }
        }
    }

    if (start_v0 < 0 || start_v1 < 0) {
        /* 娌℃湁瓒冲鐨勫嚑浣曠偣锛岀敓鎴愮┖搴忓垪 */
        free(seq);
        free(boundary);
        free(visited);
        *modes = NULL;
        *seq_len = 0;
        return true;
    }

    /* 灏嗗垵濮嬭竟鍘嬪叆杈圭晫鏍?*/
    boundary[0].v0 = start_v0;
    boundary[0].v1 = start_v1;
    boundary_top = 1;

    /* 涓婚亶鍘嗗惊鐜細浣跨敤闃熷垪鏂瑰紡閬嶅巻绾︽潫鍥?*/
    while (boundary_top > 0) {
        boundary_top--;
        Edge cur = boundary[boundary_top];

        /* 鏌ユ壘涓庡綋鍓嶈竟鍏宠仈鐨勭害鏉燂紙INCIDENCE/CONTAINMENT锛?*/
        int constr_count = graph->constraint_count;
        bool found_opposite = false;

        for (int ci = 0; ci < constr_count && !found_opposite; ci++) {
            Constraint *c = graph->constraints[ci];
            if (!c) continue;

            /* 鏌ユ壘鍚屾椂鍖呭惈 cur.v0 鍜?cur.v1 鐨勭害鏉?*/
            bool has_v0 = false, has_v1 = false;
            int opposite_id = -1;

            for (int pi = 0; pi < c->participant_count; pi++) {
                int pid = c->participants[pi];
                if (pid == cur.v0) has_v0 = true;
                else if (pid == cur.v1) has_v1 = true;
                else opposite_id = pid;
            }

            if (!has_v0 || !has_v1) continue;

            /* 鎵惧埌浜嗗椤剁偣 */
            found_opposite = true;

            if (opposite_id >= 0 && !visited[opposite_id]) {
                /* 瀵归《鐐规湭璁块棶 鈫?C 妯″紡 */
                if (len >= capacity) {
                    capacity *= 2;
                    EdgebreakerMode *new_seq = (EdgebreakerMode *)
                        lv00_realloc(seq, capacity * sizeof(EdgebreakerMode));
                    if (!new_seq) break;
                    seq = new_seq;
                }
                seq[len++] = EDGEBREAKER_C;
                visited[opposite_id] = true;

                /* 灏嗘柊杈瑰帇鍏ヨ竟鐣屾爤 */
                if (boundary_top >= boundary_capacity) {
                    boundary_capacity *= 2;
                    Edge *new_b = (Edge *)lv00_realloc(
                        boundary, boundary_capacity * sizeof(Edge));
                    if (!new_b) break;
                    boundary = new_b;
                }
                boundary[boundary_top].v0 = cur.v1;
                boundary[boundary_top].v1 = opposite_id;
                boundary_top++;
                boundary[boundary_top].v0 = opposite_id;
                boundary[boundary_top].v1 = cur.v0;
                boundary_top++;

            } else if (opposite_id >= 0) {
                /* 瀵归《鐐瑰凡璁块棶 鈫?闇€瑕佽繘涓€姝ュ垎绫?*/
                /* TODO: 鏍规嵁瀵归《鐐瑰湪杈圭晫鏍堜腑鐨勪綅缃垽鏂?L/R/S */
                /* 妗╁疄鐜帮細榛樿褰掔被涓?L */
                if (len >= capacity) {
                    capacity *= 2;
                    EdgebreakerMode *new_seq = (EdgebreakerMode *)
                        lv00_realloc(seq, capacity * sizeof(EdgebreakerMode));
                    if (!new_seq) break;
                    seq = new_seq;
                }
                seq[len++] = EDGEBREAKER_L;
            } else {
                /* 鏃犺竟鍙帹 鈫?E 妯″紡 */
                if (len >= capacity) {
                    capacity *= 2;
                    EdgebreakerMode *new_seq = (EdgebreakerMode *)
                        lv00_realloc(seq, capacity * sizeof(EdgebreakerMode));
                    if (!new_seq) break;
                    seq = new_seq;
                }
                seq[len++] = EDGEBREAKER_E;
            }
        }

        if (!found_opposite) {
            /* 鏃犵浉鍏崇害鏉?鈫?鏍囪涓?E */
            if (len >= capacity) {
                capacity *= 2;
                EdgebreakerMode *new_seq = (EdgebreakerMode *)
                    lv00_realloc(seq, capacity * sizeof(EdgebreakerMode));
                if (!new_seq) break;
                seq = new_seq;
            }
            seq[len++] = EDGEBREAKER_E;
        }
    }

    free(boundary);
    free(visited);

    *modes = seq;
    *seq_len = len;
    return true;
}

/* ========================================================================
 * 鐔电紪鐮侊紙妗╁疄鐜帮級
 * ======================================================================== */

/**
 * @brief 妗╃喌缂栫爜鍣?鈥斺€?鍦ㄥ畬鏁村疄鐜板墠鐩存帴澶嶅埗鍘熷鏁版嵁
 *
 * TODO: 瀹炵幇鐪熸鐨?rANS / 绠楁湳 / Huffman 缂栫爜鍣ㄣ€? *
 * @param[in]  raw_data   鍘熷鏁版嵁
 * @param[in]  raw_size   鍘熷鏁版嵁澶у皬
 * @param[out] out_data   缂栫爜鍚庢暟鎹紙褰撳墠涓虹洿鎺ュ鍒讹級
 * @param[out] out_size   杈撳嚭澶у皬
 * @return true 鎴愬姛
 */
static bool entropy_encode_stub(const uint8_t *raw_data,
                                 size_t raw_size,
                                 uint8_t **out_data,
                                 size_t *out_size) {
    if (!raw_data || raw_size == 0) {
        *out_data = NULL;
        *out_size = 0;
        return true;
    }

    *out_data = (uint8_t *)lv00_malloc(raw_size);
    if (!*out_data) return false;

    memcpy(*out_data, raw_data, raw_size);
    *out_size = raw_size;
    return true;
}

/**
 * @brief 妗╃喌瑙ｇ爜鍣? *
 * TODO: 瀹炵幇鐪熸鐨?rANS / 绠楁湳 / Huffman 瑙ｇ爜鍣ㄣ€? */
static bool entropy_decode_stub(const uint8_t *data, size_t size,
                                 uint8_t **out_data, size_t *out_size) {
    if (!data || size == 0) {
        *out_data = NULL;
        *out_size = 0;
        return true;
    }

    *out_data = (uint8_t *)lv00_malloc(size);
    if (!*out_data) return false;

    memcpy(*out_data, data, size);
    *out_size = size;
    return true;
}

/* ========================================================================
 * 鍑犱綍鍘嬬缉涓?API
 * ======================================================================== */

/**
 * @brief 璁＄畻绾︽潫鍥句腑鍑犱綍鏁版嵁鐨勫師濮嬪瓧鑺傚ぇ灏忥紙浼板€硷級
 */
static size_t estimate_original_size(const ConstraintGraph *graph) {
    if (!graph) return 0;

    size_t total = 0;
    /* 姣忎釜鑺傜偣锛歩d(4) + type(4) + coord_count(4) + coords(N * sizeof(SymbolicCoord*)) */
    total += graph->node_count * (sizeof(int) * 3);
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (node && node->symbolic_coords) {
            total += node->coord_count * sizeof(SymbolicCoord *);
        }
    }
    total += graph->constraint_count * sizeof(Constraint);

    return total;
}

/**
 * @brief 灏嗚妭鐐瑰潗鏍囧簭鍒楀寲涓哄師濮嬪瓧鑺傛祦
 *
 * 鏍煎紡锛歯ode_count(4B) + [node_id(4B) + coord_count(4B) + coord_doubles(8B*coord_count*dim)]*
 */
static uint8_t *serialize_coords_raw(const ConstraintGraph *graph, size_t *out_size) {
    if (!graph || !out_size) return NULL;

    /* 鍏堣绠楀ぇ灏?*/
    size_t header = sizeof(int32_t); /* node_count */
    size_t body = 0;

    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node) continue;
        body += 2 * sizeof(int32_t); /* node_id, coord_count */
        if (node->symbolic_coords) {
            body += node->coord_count * sizeof(double);
        }
    }

    size_t total = header + body;
    uint8_t *buf = (uint8_t *)lv00_malloc(total);
    if (!buf) return NULL;

    /* 鍐欏叆鏁版嵁 */
    uint8_t *ptr = buf;
    int32_t count = (int32_t)graph->node_count;
    memcpy(ptr, &count, sizeof(int32_t)); ptr += sizeof(int32_t);

    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node) continue;

        int32_t nid = (int32_t)node->id;
        int32_t cc  = (int32_t)node->coord_count;
        memcpy(ptr, &nid, sizeof(int32_t)); ptr += sizeof(int32_t);
        memcpy(ptr, &cc,  sizeof(int32_t)); ptr += sizeof(int32_t);

        for (int d = 0; d < node->coord_count; d++) {
            double val = symbolic_coord_to_double(node->symbolic_coords[d]);
            memcpy(ptr, &val, sizeof(double)); ptr += sizeof(double);
        }
    }

    *out_size = (size_t)(ptr - buf);
    return buf;
}

bool geometry_compress(const ConstraintGraph *graph,
                       const CompressConfig *config,
                       uint8_t **out_data,
                       size_t *out_size,
                       CompressMetadata *out_meta) {
    if (!graph || !out_data || !out_size) return false;

    CompressConfig cfg = config ? *config : compress_config_default();

    /* 姝ラ 1: 璁＄畻鍘熷澶у皬 */
    size_t original_sz = estimate_original_size(graph);

    /* 姝ラ 2: 澶嶅埗绾︽潫鍥剧敤浜庡師鍦颁慨鏀癸紙棰勬祴缂栫爜浼氫慨鏀瑰潗鏍囷級 */
    /* TODO: 瀹炵幇 graph_clone() 杩涜娣辨嫹璐?鈥斺€?褰撳墠妗╀粎鍋氭祬鍙傝€?*/

    /* 姝ラ 3: 棰勬祴缂栫爜 鈥斺€?灏嗗潗鏍囨浛鎹负娈嬪樊 */
    /* 娉ㄦ剰锛氭澶勯渶瑕佸湪鍓湰涓婃搷浣滐紝閬垮厤淇敼鍘熷鍥?*/
    /* TODO: 瀹炵幇 ConstraintGraph 娣辨嫹璐濆悗鍦ㄦ鎿嶄綔 */

    /* 姝ラ 4: Edgebreaker 缂栫爜 鈥斺€?鐢熸垚 CLERS 鎷撴墤搴忓垪 */
    EdgebreakerMode *clers_seq = NULL;
    int clers_len = 0;
    edgebreaker_encode(graph, &clers_seq, &clers_len);

    /* 姝ラ 5: 搴忓垪鍖栧潗鏍囨暟鎹负鍘熷瀛楄妭娴?*/
    size_t raw_size = 0;
    uint8_t *raw_buf = serialize_coords_raw(graph, &raw_size);
    if (!raw_buf) {
        free(clers_seq);
        return false;
    }

    /* 姝ラ 6: 鐔电紪鐮?*/
    /* TODO: 灏?CLERS 搴忓垪鍜屽潗鏍囨畫宸悎骞跺悗鍋氱湡姝ｇ殑鐔电紪鐮?*/
    uint8_t *encoded = NULL;
    size_t encoded_size = 0;
    bool enc_ok = entropy_encode_stub(raw_buf, raw_size, &encoded, &encoded_size);

    free(raw_buf);

    if (!enc_ok) {
        free(clers_seq);
        return false;
    }

    *out_data = encoded;
    *out_size = encoded_size;

    /* 濉厖鍏冩暟鎹?*/
    if (out_meta) {
        out_meta->original_size        = original_sz;
        out_meta->compressed_size      = encoded_size;
        out_meta->compression_ratio    = (encoded_size > 0)
            ? (double)original_sz / (double)encoded_size
            : 1.0;
        out_meta->node_count           = graph->node_count;
        out_meta->constraint_count     = graph->constraint_count;
        out_meta->edgebreaker_sequence = clers_seq;
        out_meta->sequence_len         = clers_len;
    } else {
        free(clers_seq);
    }

    return true;
}

/* ========================================================================
 * 鍑犱綍瑙ｅ帇涓?API
 * ======================================================================== */

bool geometry_decompress(const uint8_t *data,
                         size_t size,
                         ConstraintGraph **out_graph) {
    if (!data || size == 0 || !out_graph) return false;

    /* 姝ラ 1: 鐔佃В鐮?*/
    uint8_t *decoded = NULL;
    size_t decoded_size = 0;
    if (!entropy_decode_stub(data, size, &decoded, &decoded_size)) {
        return false;
    }

    /* 姝ラ 2: 浠庤В鐮佹暟鎹噸寤虹害鏉熷浘 */
    ConstraintGraph *graph = graph_create();
    if (!graph) {
        free(decoded);
        return false;
    }

    /* TODO: 浠?decoded 瀛楄妭娴佷腑鍙嶅簭鍒楀寲鑺傜偣鍧愭爣鍜屾嫇鎵?     * 褰撳墠妗╋細浠呭垱寤虹┖鍥?*/
    free(decoded);

    *out_graph = graph;
    return true;
}

/* ========================================================================
 * .lvzd 鏍煎紡 I/O
 * ======================================================================== */

/**
 * @brief 灏?uint32 浠ュ皬绔簭鍐欏叆缂撳啿鍖? */
static void write_uint32_le(uint8_t *buf, uint32_t val) {
    buf[0] = (uint8_t)(val & 0xFF);
    buf[1] = (uint8_t)((val >> 8) & 0xFF);
    buf[2] = (uint8_t)((val >> 16) & 0xFF);
    buf[3] = (uint8_t)((val >> 24) & 0xFF);
}

/**
 * @brief 灏?uint64 浠ュ皬绔簭鍐欏叆缂撳啿鍖? */
static void write_uint64_le(uint8_t *buf, uint64_t val) {
    for (int i = 0; i < 8; i++) {
        buf[i] = (uint8_t)((val >> (i * 8)) & 0xFF);
    }
}

/**
 * @brief 浠庣紦鍐插尯浠ュ皬绔簭璇诲彇 uint32
 */
static uint32_t read_uint32_le(const uint8_t *buf) {
    return ((uint32_t)buf[0])
        | ((uint32_t)buf[1] << 8)
        | ((uint32_t)buf[2] << 16)
        | ((uint32_t)buf[3] << 24);
}

/**
 * @brief 浠庣紦鍐插尯浠ュ皬绔簭璇诲彇 uint64
 */
static uint64_t read_uint64_le(const uint8_t *buf) {
    uint64_t val = 0;
    for (int i = 0; i < 8; i++) {
        val |= ((uint64_t)buf[i]) << (i * 8);
    }
    return val;
}

bool compress_write_lvzd(const uint8_t *data,
                         size_t size,
                         const char *filename) {
    if (!data || size == 0 || !filename) return false;

    FILE *fp = fopen(filename, "wb");
    if (!fp) return false;

    /* 鏋勫缓鏂囦欢澶?*/
    uint8_t header[LVZD_HEADER_SIZE];
    memset(header, 0, LVZD_HEADER_SIZE);

    write_uint32_le(header,      LVZD_MAGIC);
    write_uint32_le(header + 4,  LVZD_VERSION_MAJOR);
    write_uint32_le(header + 8,  LVZD_VERSION_MINOR);
    write_uint64_le(header + 12, (uint64_t)size); /* original_size = compressed_size for stub */
    write_uint64_le(header + 20, (uint64_t)size); /* compressed_size */

    /* 鍐欏叆鏂囦欢澶?*/
    size_t written = fwrite(header, 1, LVZD_HEADER_SIZE, fp);
    if (written != LVZD_HEADER_SIZE) {
        fclose(fp);
        return false;
    }

    /* 鍐欏叆鍘嬬缉鏁版嵁 */
    written = fwrite(data, 1, size, fp);
    fclose(fp);
    return (written == size);
}

bool compress_read_lvzd(const char *filename,
                        uint8_t **out_data,
                        size_t *out_size) {
    if (!filename || !out_data || !out_size) return false;

    FILE *fp = fopen(filename, "rb");
    if (!fp) return false;

    /* 璇诲彇鏂囦欢澶?*/
    uint8_t header[LVZD_HEADER_SIZE];
    size_t read_bytes = fread(header, 1, LVZD_HEADER_SIZE, fp);
    if (read_bytes != LVZD_HEADER_SIZE) {
        fclose(fp);
        return false;
    }

    /* 楠岃瘉榄旀暟 */
    uint32_t magic = read_uint32_le(header);
    if (magic != LVZD_MAGIC) {
        fclose(fp);
        return false;
    }

    /* 楠岃瘉鐗堟湰鍙?*/
    uint32_t ver_major = read_uint32_le(header + 4);
    uint32_t ver_minor = read_uint32_le(header + 8);
    if (ver_major > LVZD_VERSION_MAJOR) {
        /* 涓荤増鏈笉鍏煎 */
        fclose(fp);
        return false;
    }
    (void)ver_minor; /* 娆＄増鏈悜鍓嶅吋瀹?*/

    /* 璇诲彇鍘嬬缉鏁版嵁澶у皬 */
    uint64_t comp_size = read_uint64_le(header + 20);
    if (comp_size == 0) {
        fclose(fp);
        *out_data = NULL;
        *out_size = 0;
        return true;
    }

    /* 鍒嗛厤缂撳啿鍖哄苟璇诲彇鍘嬬缉鏁版嵁 */
    uint8_t *buf = (uint8_t *)lv00_malloc((size_t)comp_size);
    if (!buf) {
        fclose(fp);
        return false;
    }

    read_bytes = fread(buf, 1, (size_t)comp_size, fp);
    fclose(fp);

    if (read_bytes != (size_t)comp_size) {
        free(buf);
        return false;
    }

    *out_data = buf;
    *out_size = (size_t)comp_size;
    return true;
}
