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

#include "geometry_compress.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constraint_graph.h"
#include "lv00_internal.h"
#include "lv00_utils.h"
#include "symbolic_coord.h"

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
    int v0; /**< 杈硅捣鐐硅妭鐐?ID */
    int v1; /**< 杈圭粓鐐硅妭鐐?ID */
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
    cfg.pred_mode = PREDICT_PARALLELOGRAM;
    cfg.entropy = ENTROPY_RANS;
    cfg.quantization_bits = 0;
    cfg.lossless = true;
    cfg.max_error = 0.0;
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
    if (!graph)
        return false;

    int node_count = graph->node_count;
    if (node_count < 3)
        return true; /* 灏戜簬 3 涓妭鐐癸紝鏃犳硶鏋勬垚涓夎褰?*/

    /* 浣跨敤绠€鍗曠殑宸茶闂爣璁版暟缁?*/
    bool *visited = (bool *) lv00_malloc(node_count * sizeof(bool));
    if (!visited)
        return false;
    memset(visited, 0, node_count * sizeof(bool));

    /* 閬嶅巻鑺傜偣锛氭煡鎵炬湁鍧愭爣鐨勫嚑浣曠偣 */
    for (int i = 0; i < node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || !node->symbolic_coords || node->coord_count < COORD_DIM)
            continue;
        if (visited[node->id])
            continue;
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
    if (!graph)
        return false;

    int node_count = graph->node_count;
    if (node_count < 2)
        return true;

    /* 淇濆瓨绗竴涓妭鐐圭殑鍧愭爣浣滀负鍙傝€冨€?*/
    GeomNode *prev = NULL;

    for (int i = 0; i < node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || !node->symbolic_coords || node->coord_count < COORD_DIM)
            continue;

        if (prev) {
            /* 璁＄畻宸€硷細node - prev锛屽瓨鍌ㄥ埌 node */
            for (int d = 0; d < COORD_DIM; d++) {
                SymbolicCoord *diff = symbolic_coord_subtract(node->symbolic_coords[d], prev->symbolic_coords[d]);
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
    if (!graph)
        return false;

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

bool edgebreaker_encode(const ConstraintGraph *graph, EdgebreakerMode **modes, int *seq_len) {
    if (!graph || !modes || !seq_len)
        return false;

    /* 鍒濆鍖?CLERS 搴忓垪缂撳啿鍖?*/
    int capacity = CLERS_SEQUENCE_INITIAL;
    EdgebreakerMode *seq = (EdgebreakerMode *) lv00_malloc(capacity * sizeof(EdgebreakerMode));
    if (!seq)
        return false;

    int len = 0;

    /* 杈圭晫鏍堬細浣跨敤绠€鍗曟暟缁勬ā鎷?*/
    Edge *boundary = (Edge *) lv00_malloc(BOUNDARY_STACK_INITIAL * sizeof(Edge));
    if (!boundary) {
        free(seq);
        return false;
    }
    int boundary_top = 0;
    int boundary_capacity = BOUNDARY_STACK_INITIAL;

    /* 鑺傜偣璁块棶鏍囪 */
    bool *visited = (bool *) lv00_malloc(graph->node_count * sizeof(bool));
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
        if (!node)
            continue;
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
            if (!c)
                continue;

            /* 鏌ユ壘鍚屾椂鍖呭惈 cur.v0 鍜?cur.v1 鐨勭害鏉?*/
            bool has_v0 = false, has_v1 = false;
            int opposite_id = -1;

            for (int pi = 0; pi < c->participant_count; pi++) {
                int pid = c->participants[pi];
                if (pid == cur.v0)
                    has_v0 = true;
                else if (pid == cur.v1)
                    has_v1 = true;
                else
                    opposite_id = pid;
            }

            if (!has_v0 || !has_v1)
                continue;

            /* 鎵惧埌浜嗗椤剁偣 */
            found_opposite = true;

            if (opposite_id >= 0 && !visited[opposite_id]) {
                /* 瀵归《鐐规湭璁块棶 鈫?C 妯″紡 */
                if (len >= capacity) {
                    capacity *= 2;
                    EdgebreakerMode *new_seq =
                        (EdgebreakerMode *) lv00_realloc(seq, capacity * sizeof(EdgebreakerMode));
                    if (!new_seq)
                        break;
                    seq = new_seq;
                }
                seq[len++] = EDGEBREAKER_C;
                visited[opposite_id] = true;

                /* 灏嗘柊杈瑰帇鍏ヨ竟鐣屾爤 */
                if (boundary_top >= boundary_capacity) {
                    boundary_capacity *= 2;
                    Edge *new_b = (Edge *) lv00_realloc(boundary, boundary_capacity * sizeof(Edge));
                    if (!new_b)
                        break;
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
                    EdgebreakerMode *new_seq =
                        (EdgebreakerMode *) lv00_realloc(seq, capacity * sizeof(EdgebreakerMode));
                    if (!new_seq)
                        break;
                    seq = new_seq;
                }
                seq[len++] = EDGEBREAKER_L;
            } else {
                /* 鏃犺竟鍙帹 鈫?E 妯″紡 */
                if (len >= capacity) {
                    capacity *= 2;
                    EdgebreakerMode *new_seq =
                        (EdgebreakerMode *) lv00_realloc(seq, capacity * sizeof(EdgebreakerMode));
                    if (!new_seq)
                        break;
                    seq = new_seq;
                }
                seq[len++] = EDGEBREAKER_E;
            }
        }

        if (!found_opposite) {
            /* 鏃犵浉鍏崇害鏉?鈫?鏍囪涓?E */
            if (len >= capacity) {
                capacity *= 2;
                EdgebreakerMode *new_seq = (EdgebreakerMode *) lv00_realloc(seq, capacity * sizeof(EdgebreakerMode));
                if (!new_seq)
                    break;
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
 * Huffman 鐔电紪鐮佸櫒
 * ======================================================================== */

/** Huffman鏍戞渶澶ц妭鐐规暟锛?56涓彾瀛?+ 鏈€澶?55涓唴閮ㄨ妭鐐?*/
#define HUFFMAN_MAX_NODES 511

/** Huffman缂栫爜鏈€澶ч暱搴︼紙鏈€鍧忔儏鍐碉細鎵€鏈夐鐜囩浉绛夋椂鐨勫亸鏂滄爲锛?*/
#define HUFFMAN_MAX_CODE_LEN 256

/* ========================================================================
 * 鍐呴儴鏁版嵁缁撴瀯
 * ======================================================================== */

/**
 * @brief Huffman鏍戣妭鐐? *
 * 鍖呭惈宸﹀彸瀛愯妭鐐圭储寮曘€佺埗鑺傜偣绱㈠紩銆侀鐜囨潈閲嶅強鍙跺瓙鑺傜偣瀵瑰簲鐨勫瓧鑺傚€笺€?*/
typedef struct {
    int left;           /**< 宸﹀瓙鑺傜偣绱㈠紩锛?-1琛ㄧず鏃?*/
    int right;          /**< 鍙冲瓙鑺傜偣绱㈠紩锛?-1琛ㄧず鏃?*/
    int parent;         /**< 鐖惰妭鐐圭储寮曪紝-1琛ㄧず鏍硅妭鐐?*/
    uint32_t freq;      /**< 鑺傜偣棰戠巼鏉冮噸 */
    uint8_t byte_val;   /**< 鍙跺瓙鑺傜偣瀵瑰簲鐨勫瓧鑺傚€硷紙鍐呴儴鑺傜偣鏃犳晥锛?*/
} HuffmanNode;

/**
 * @brief Huffman缂栫爜鏌ユ壘琛ㄦ潯鐩? *
 * 瀛樺偍姣忎釜瀛楄妭鍊煎搴旂殑鍙橀暱缂栫爜銆?
 * - code: 缂栫爜姣旂壒搴忓垪锛堜綆浣嶅榻愶紝鍗崇紪鐮佺殑绗竴涓瘮鐗瑰湪鏈€楂樹綅锛? * - length: 缂栫爜姣旂壒闀垮害
 */
typedef struct {
    uint32_t code;      /**< 缂栫爜姣旂壒搴忓垪 */
    int length;         /**< 缂栫爜姣旂壒闀垮害 */
} HuffmanCode;

/**
 * @brief 鏈€灏忓爢缁撴瀯锛堢敤浜庢瀯寤篐uffman鏍戯級
 *
 * 鍩轰簬鏁扮粍瀹炵幇鐨勪簩鍙夊爢锛岀敤浜庨噸澶嶆彁鍙栨渶灏忛鐜囪妭鐐广€? * 鍫嗕腑瀛樺偍鐨勬槸Huffman鑺傜偣鏁扮粍涓殑绱㈠紩銆?
 */
typedef struct {
    int *nodes;             /**< 鍫嗕腑瀛樺偍鐨凥uffman鑺傜偣绱㈠紩 */
    int size;               /**< 褰撳墠鍫嗗ぇ灏?*/
    int capacity;           /**< 鍫嗗閲?*/
    HuffmanNode *hnodes;    /**< 鎸囧悜Huffman鑺傜偣鏁扮粍鐨勬寚閽堬紙鐢ㄤ簬姣旇緝棰戠巼锛?*/
} MinHeap;

/**
 * @brief 浣嶅啓鍏ュ櫒 鈥斺€?鏀寔姣旂壒绾у啓鍏ヨ緭鍑虹紦鍐插尯
 *
 * 姣忎釜瀛楄妭鍐呬粠楂樹綅(bit 7)鍒颁綆浣?bit 0)渚濇濉厖銆?*/
typedef struct {
    uint8_t *buf;       /**< 杈撳嚭缂撳啿鍖?*/
    size_t capacity;    /**< 缂撳啿鍖哄閲忥紙瀛楄妭锛?*/
    size_t byte_pos;    /**< 褰撳墠鍐欏叆瀛楄妭浣嶇疆 */
    int bit_pos;        /**< 褰撳墠瀛楄妭鍐呯殑浣嶄綅缃紙7=鏈€楂樹綅锛?=鏈€浣庝綅锛?*/
} BitWriter;

/**
 * @brief 浣嶈鍙栧櫒 鈥斺€?鏀寔浠庤緭鍏ョ紦鍐插尯閫愪綅璇诲彇
 */
typedef struct {
    const uint8_t *buf; /**< 杈撳叆缂撳啿鍖?*/
    size_t size;        /**< 缂撳啿鍖哄ぇ灏忥紙瀛楄妭锛?*/
    size_t byte_pos;    /**< 褰撳墠璇诲彇瀛楄妭浣嶇疆 */
    int bit_pos;        /**< 褰撳墠瀛楄妭鍐呯殑浣嶄綅缃紙7=鏈€楂樹綅锛?=鏈€浣庝綅锛?*/
} BitReader;

/* ========================================================================
 * 浣嶅啓鍏ュ櫒鎿嶄綔
 * ======================================================================== */

/**
 * @brief 鍒濆鍖栦綅鍐欏叆鍣? *
 * 鍒嗛厤缂撳啿鍖哄苟灏嗗啓鍏ヤ綅缃綊闆躲€? *
 * @param[out] bw                浣嶅啓鍏ュ櫒鎸囬拡
 * @param[in]  initial_capacity  鍒濆缂撳啿鍖哄閲忥紙瀛楄妭锛? * @return true 鎴愬姛锛宖alse 鍐呭瓨涓嶈冻
 */
static bool bitwriter_init(BitWriter *bw, size_t initial_capacity) {
    bw->buf = (uint8_t *) lv00_malloc(initial_capacity);
    if (!bw->buf)
        return false;
    bw->capacity = initial_capacity;
    bw->byte_pos = 0;
    bw->bit_pos = 7;
    bw->buf[0] = 0;
    return true;
}

/**
 * @brief 鍚戜綅鍐欏叆鍣ㄥ啓鍏?涓瘮鐗? *
 * 灏嗘寚瀹氭瘮鐗瑰啓鍏ュ綋鍓嶅瓧鑺傜殑褰撳墠浣嶄綅缃紝骞舵洿鏂颁綅鎸囬拡銆? * 褰撳墠瀛楄妭鍐欐弧鍚庤嚜鍔ㄥ垏鎹㈠埌涓嬩竴涓瓧鑺傦紝蹇呰鏃惰嚜鍔ㄦ墿瀹广€? *
 * @param[in,out] bw   浣嶅啓鍏ュ櫒鎸囬拡
 * @param[in]     bit  瑕佸啓鍏ョ殑姣旂壒锛?-鎴?)
 * @return true 鎴愬姛锛宖alse 鎵╁澶辫触
 */
static bool bitwriter_write_bit(BitWriter *bw, int bit) {
    if (bit) {
        bw->buf[bw->byte_pos] |= (uint8_t) (1 << bw->bit_pos);
    }
    bw->bit_pos--;
    if (bw->bit_pos < 0) {
        bw->bit_pos = 7;
        bw->byte_pos++;
        if (bw->byte_pos >= bw->capacity) {
            size_t new_cap = bw->capacity * 2;
            uint8_t *new_buf = (uint8_t *) lv00_realloc(bw->buf, new_cap);
            if (!new_buf)
                return false;
            bw->buf = new_buf;
            bw->capacity = new_cap;
        }
        bw->buf[bw->byte_pos] = 0;
    }
    return true;
}

/**
 * @brief 鍚戜綅鍐欏叆鍣ㄥ啓鍏ュ涓瘮鐗? *
 * 鎸変粠楂樹綅鍒颁綆浣嶇殑椤哄簭渚濇鍐欏叆鎸囧畾鏁扮洰鐨勬瘮鐗广€? *
 * @param[in,out] bw         浣嶅啓鍏ュ櫒鎸囬拡
 * @param[in]     code       瑕佸啓鍏ョ殑姣旂壒搴忓垪
 * @param[in]     bit_count  瑕佸啓鍏ョ殑姣旂壒鏁伴噺
 * @return true 鎴愬姛锛宖alse 鎵╁澶辫触
 */
static bool bitwriter_write_bits(BitWriter *bw, uint32_t code, int bit_count) {
    for (int i = bit_count - 1; i >= 0; i--) {
        if (!bitwriter_write_bit(bw, (code >> i) & 1))
            return false;
    }
    return true;
}

/**
 * @brief 鍒锋柊浣嶅啓鍏ュ櫒骞惰繑鍥炲疄闄呰緭鍑哄瓧鑺傛暟
 *
 * 鏈€鍚庝竴涓瓧鑺傚鏋滄湭鍐欐弧锛屼粛璁″叆杈撳嚭锛坆it_pos < 7 鏃讹級銆? *
 * @param[in] bw 浣嶅啓鍏ュ櫒鎸囬拡
 * @return 瀹為檯鍐欏叆鐨勫瓧鑺傛暟
 */
static size_t bitwriter_flush(const BitWriter *bw) {
    return (bw->bit_pos < 7) ? bw->byte_pos + 1 : bw->byte_pos;
}

/* ========================================================================
 * 浣嶈鍙栧櫒鎿嶄綔
 * ======================================================================== */

/**
 * @brief 鍒濆鍖栦綅璇诲彇鍣? *
 * @param[out] br   浣嶈鍙栧櫒鎸囬拡
 * @param[in]  buf  杈撳叆缂撳啿鍖? * @param[in]  size 杈撳叆缂撳啿鍖哄ぇ灏忥紙瀛楄妭锛?*/
static void bitreader_init(BitReader *br, const uint8_t *buf, size_t size) {
    br->buf = buf;
    br->size = size;
    br->byte_pos = 0;
    br->bit_pos = 7;
}

/**
 * @brief 浠庝綅璇诲彇鍣ㄨ鍙?涓瘮鐗? *
 * 浠庡綋鍓嶅瓧鑺傜殑褰撳墠浣嶄綅缃鍙栦竴涓瘮鐗癸紝骞惰嚜鍔ㄦ洿鏂颁綅鎸囬拡銆? * 瀛楄妭璇诲畬鍚庤嚜鍔ㄥ垏鎹㈠埌涓嬩竴涓瓧鑺傘€? *
 * @param[in,out] br 浣嶈鍙栧櫒鎸囬拡
 * @return 璇诲彇鐨勬瘮鐗癸紙0鎴?锛夛紝宸茶揪缂撳啿鍖烘湯灏炬椂杩斿洖 -1
 */
static int bitreader_read_bit(BitReader *br) {
    if (br->byte_pos >= br->size)
        return -1;
    int bit = (br->buf[br->byte_pos] >> br->bit_pos) & 1;
    br->bit_pos--;
    if (br->bit_pos < 0) {
        br->bit_pos = 7;
        br->byte_pos++;
    }
    return bit;
}

/* ========================================================================
 * 鏈€灏忓爢鎿嶄綔锛堢敤浜庢瀯寤篐uffman鏍戯級
 * ======================================================================== */

/**
 * @brief 浜ゆ崲鍫嗕腑涓や釜鍏冪礌
 */
static void heap_swap(MinHeap *h, int i, int j) {
    int tmp = h->nodes[i];
    h->nodes[i] = h->nodes[j];
    h->nodes[j] = tmp;
}

/**
 * @brief 鍚戜笂璋冩暣鍫嗭紙涓婃护锛? *
 * 灏嗘寚瀹氫綅缃殑鍏冪礌鍚戜笂绉诲姩锛岀洿鍒版弧瓒虫渶灏忓爢鎬ц川銆? *
 * @param[in,out] h   鏈€灏忓爢鎸囬拡
 * @param[in]     idx 闇€瑕佽皟鏁寸殑鍏冪礌绱㈠紩
 */
static void heap_sift_up(MinHeap *h, int idx) {
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        if (h->hnodes[h->nodes[idx]].freq < h->hnodes[h->nodes[parent]].freq) {
            heap_swap(h, idx, parent);
            idx = parent;
        } else {
            break;
        }
    }
}

/**
 * @brief 鍚戜笅璋冩暣鍫嗭紙涓嬫护锛? *
 * 灏嗘寚瀹氫綅缃殑鍏冪礌鍚戜笅绉诲姩锛岀洿鍒版弧瓒虫渶灏忓爢鎬ц川銆? *
 * @param[in,out] h   鏈€灏忓爢鎸囬拡
 * @param[in]     idx 闇€瑕佽皟鏁寸殑鍏冪礌绱㈠紩
 */
static void heap_sift_down(MinHeap *h, int idx) {
    int size = h->size;
    while (1) {
        int smallest = idx;
        int left = 2 * idx + 1;
        int right = 2 * idx + 2;
        if (left < size && h->hnodes[h->nodes[left]].freq < h->hnodes[h->nodes[smallest]].freq)
            smallest = left;
        if (right < size && h->hnodes[h->nodes[right]].freq < h->hnodes[h->nodes[smallest]].freq)
            smallest = right;
        if (smallest == idx)
            break;
        heap_swap(h, idx, smallest);
        idx = smallest;
    }
}

/**
 * @brief 灏嗚妭鐐圭储寮曟帹鍏ユ渶灏忓爢
 *
 * @param[in,out] h        鏈€灏忓爢鎸囬拡
 * @param[in]     node_idx 瑕佹彃鍏ョ殑Huffman鑺傜偣绱㈠紩
 * @return true 鎴愬姛锛宖alse 鍫嗗凡婊?*/
static bool heap_push(MinHeap *h, int node_idx) {
    if (h->size >= h->capacity)
        return false;
    h->nodes[h->size] = node_idx;
    heap_sift_up(h, h->size);
    h->size++;
    return true;
}

/**
 * @brief 浠庢渶灏忓爢寮瑰嚭棰戠巼鏈€灏忕殑鑺傜偣绱㈠紩
 *
 * @param[in,out] h 鏈€灏忓爢鎸囬拡
 * @return 棰戠巼鏈€灏忕殑鑺傜偣绱㈠紩锛屽爢绌烘椂杩斿洖 -1
 */
static int heap_pop(MinHeap *h) {
    if (h->size <= 0)
        return -1;
    int result = h->nodes[0];
    h->size--;
    if (h->size > 0) {
        h->nodes[0] = h->nodes[h->size];
        heap_sift_down(h, 0);
    }
    return result;
}

/* ========================================================================
 * Huffman缂栫爜琛ㄧ敓鎴? * ======================================================================== */

/**
 * @brief 閫掑綊閬嶅巻Huffman鏍戯紝涓烘瘡涓彾瀛愯妭鐐圭敓鎴愮紪鐮? *
 * 浣跨敤鎵嬪姩鏍堢殑杩唬鏂瑰紡閬嶅巻鏍戯紝閬垮厤娣卞害閫掑綊瀵艰嚧鐨勬爤婧㈠嚭銆? * 閬嶅巻褰撲腑锛氬線宸︽椂缂栫爜鏈熬杩藉姞0锛屽線鍙虫椂杩藉姞1銆? *
 * @param[in]  hnodes Huffman鑺傜偣鏁扮粍
 * @param[in]  root   鏍硅妭鐐圭储寮? * @param[out] codes  杈撳嚭鐨凥uffman缂栫爜鏌ユ壘琛紙256椤癸紝姣忛」瀵瑰簲0-255瀛楄妭鍊硷級
 */
static void huffman_generate_codes(const HuffmanNode *hnodes, int root, HuffmanCode *codes) {
    /* 鎵嬪姩鏍堬細姣忎釜鍏冪礌瀛樺偍 (node_index, current_code, current_length) */
    int stack[HUFFMAN_MAX_NODES];
    uint32_t code_stack[HUFFMAN_MAX_NODES];
    int len_stack[HUFFMAN_MAX_NODES];
    int top = 0;

    stack[0] = root;
    code_stack[0] = 0;
    len_stack[0] = 0;
    top = 1;

    while (top > 0) {
        top--;
        int node = stack[top];
        uint32_t code = code_stack[top];
        int len = len_stack[top];

        if (hnodes[node].left < 0 && hnodes[node].right < 0) {
            /* 鍙跺瓙鑺傜偣锛氳褰曠紪鐮?*/
            codes[hnodes[node].byte_val].code = code;
            codes[hnodes[node].byte_val].length = len;
        } else {
            /* 鍐呴儴鑺傜偣锛氬帇鍏ュ乏鍙冲瓙鑺傜偣 */
            if (hnodes[node].left >= 0) {
                stack[top] = hnodes[node].left;
                code_stack[top] = (code << 1) | 0;
                len_stack[top] = len + 1;
                top++;
            }
            if (hnodes[node].right >= 0) {
                stack[top] = hnodes[node].right;
                code_stack[top] = (code << 1) | 1;
                len_stack[top] = len + 1;
                top++;
            }
        }
    }
}

/* ========================================================================
 * Huffman 缂栫爜锛堟浛鎹㈠師 entropy_encode_stub锛? * ======================================================================== */

/**
 * @brief Huffman鐔电紪鐮佸櫒
 *
 * 鍥涢亶鎵弿瀹炵幇瀹屾暣鐨凥uffman鍘嬬缉缂栫爜锛? *   1. 缁熻256涓瓧鑺傚€肩殑鍑虹幇棰戠巼
 *   2. 浣跨敤鏈€灏忓爢鏋勫缓Huffman鏍? *   3. 閬嶅巻Huffman鏍戠敓鎴愭瘡涓瓧鑺傚€肩殑鍙橀暱缂栫爜
 *   4. 瀵瑰師濮嬫暟鎹繘琛屾瘮鐗圭紪鐮佽緭鍑? *
 * 杈撳嚭鏍煎紡锛? *   [棰戠巼琛? 256 脳 4 瀛楄妭锛寀int32_t 灏忕搴廬 +
 *   [鍘熷澶у皬: 4 瀛楄妭锛寀int32_t 灏忕搴廬 +
 *   [缂栫爜鍚庢瘮鐗规祦: 鍙橀暱]
 *
 * 棰戠巼琛ㄥ缁堝寘鍚?56椤癸紝鍗充娇鏌愪簺瀛楄妭鍊奸鐜囦负0锛? * 浠ョ‘淇濊В鐮佺鑳藉姝ｇ‘閲嶅缓Huffman鏍戙€? *
 * @param[in]  raw_data   鍘熷鏁版嵁
 * @param[in]  raw_size   鍘熷鏁版嵁澶у皬锛堝瓧鑺傦級
 * @param[out] out_data   缂栫爜鍚庢暟鎹紙璋冪敤鑰呰礋璐ree锛? * @param[out] out_size   缂栫爜鍚庢暟鎹ぇ灏忥紙瀛楄妭锛? * @return true 鎴愬姛锛宖alse 澶辫触锛堝唴瀛樹笉瓒崇瓑锛?*/
static bool entropy_encode_stub(const uint8_t *raw_data, size_t raw_size, uint8_t **out_data, size_t *out_size) {
    if (!raw_data || raw_size == 0) {
        *out_data = NULL;
        *out_size = 0;
        return true;
    }

    /* 鈹€鈹€ 绗竴閬嶏細缁熻瀛楄妭棰戠巼 鈹€鈹€ */
    uint32_t freq[256];
    memset(freq, 0, sizeof(freq));
    for (size_t i = 0; i < raw_size; i++) {
        freq[raw_data[i]]++;
    }

    /* 鈹€鈹€ 绗簩閬嶏細鏋勫缓Huffman鏍?鈹€鈹€ */
    HuffmanNode hnodes[HUFFMAN_MAX_NODES];
    memset(hnodes, 0, sizeof(hnodes));
    int node_count = 0;

    /* 鍒濆鍖栧彾瀛愯妭鐐癸細姣忎釜鍑虹幇杩囩殑瀛楄妭鍊煎垱寤轰竴涓彾瀛愯妭鐐?*/
    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            hnodes[node_count].left = -1;
            hnodes[node_count].right = -1;
            hnodes[node_count].parent = -1;
            hnodes[node_count].freq = freq[i];
            hnodes[node_count].byte_val = (uint8_t) i;
            node_count++;
        }
    }

    /* 澶勭悊鐗规畩鎯呭喌锛氭暟鎹彧鏈変竴绉嶅瓧鑺傚€?*/
    if (node_count == 1) {
        /* 鍗曞瓧绗︾紪鐮侊細鍒涘缓涓€涓唴閮ㄨ妭鐐逛綔涓烘牴锛屽崟杈圭殑鏍?*/
        hnodes[node_count].left = 0;
        hnodes[node_count].right = -1;
        hnodes[node_count].parent = -1;
        hnodes[node_count].freq = hnodes[0].freq;
        hnodes[node_count].byte_val = 0;
        hnodes[0].parent = node_count;
        node_count++;
    }

    /* 浣跨敤鏈€灏忓爢鏋勫缓Huffman鏍?*/
    int heap_nodes[HUFFMAN_MAX_NODES];
    MinHeap heap;
    heap.nodes = heap_nodes;
    heap.size = 0;
    heap.capacity = HUFFMAN_MAX_NODES;
    heap.hnodes = hnodes;

    for (int i = 0; i < node_count; i++) {
        heap_push(&heap, i);
    }

    /* 鍚堝苟鑺傜偣鐩村埌鍫嗕腑鍙墿涓€涓妭鐐癸紙Huffman鏍戠殑鏍癸級 */
    while (heap.size > 1) {
        int left = heap_pop(&heap);
        int right = heap_pop(&heap);

        hnodes[node_count].left = left;
        hnodes[node_count].right = right;
        hnodes[node_count].parent = -1;
        hnodes[node_count].freq = hnodes[left].freq + hnodes[right].freq;
        hnodes[node_count].byte_val = 0;
        hnodes[left].parent = node_count;
        hnodes[right].parent = node_count;

        heap_push(&heap, node_count);
        node_count++;
    }

    int root = heap_pop(&heap);

    /* 鈹€鈹€ 绗笁閬嶏細鐢熸垚Huffman缂栫爜琛?鈹€鈹€ */
    HuffmanCode codes[256];
    memset(codes, 0, sizeof(codes));
    huffman_generate_codes(hnodes, root, codes);

    /* 鈹€鈹€ 绗洓閬嶏細缂栫爜杈撳嚭 鈹€鈹€ */

    /* 璁＄畻杈撳嚭澶у皬骞跺垎閰嶇紦鍐插尯 */
    /* 棰戠巼琛? 256 * 4, 鍘熷澶у皬: 4, 姣旂壒娴? 鏈€鍧忔儏鍐?raw_size * 8 bit + 7 bit 濉厖 */
    size_t header_size = 256 * sizeof(uint32_t) + sizeof(uint32_t); /* 1024 + 4 = 1028 */
    size_t bitstream_capacity = (raw_size * 8 + 7) / 8 + 16;       /* 棰濆16瀛楄妭瀹夊叏浣欓噺 */
    size_t total_capacity = header_size + bitstream_capacity;

    uint8_t *output = (uint8_t *) lv00_malloc(total_capacity);
    if (!output)
        return false;

    /* 鍐欏叆棰戠巼琛紙256涓猽int32_t锛屽皬绔簭锛?*/
    for (int i = 0; i < 256; i++) {
        uint32_t f = freq[i];
        output[i * 4 + 0] = (uint8_t) (f & 0xFF);
        output[i * 4 + 1] = (uint8_t) ((f >> 8) & 0xFF);
        output[i * 4 + 2] = (uint8_t) ((f >> 16) & 0xFF);
        output[i * 4 + 3] = (uint8_t) ((f >> 24) & 0xFF);
    }

    /* 鍐欏叆鍘熷澶у皬 */
    size_t offset = 256 * sizeof(uint32_t);
    uint32_t raw_sz = (uint32_t) raw_size;
    output[offset + 0] = (uint8_t) (raw_sz & 0xFF);
    output[offset + 1] = (uint8_t) ((raw_sz >> 8) & 0xFF);
    output[offset + 2] = (uint8_t) ((raw_sz >> 16) & 0xFF);
    output[offset + 3] = (uint8_t) ((raw_sz >> 24) & 0xFF);
    offset += sizeof(uint32_t);

    /* 浣跨敤浣嶅啓鍏ュ櫒缂栫爜鏁版嵁 */
    BitWriter bw;
    bw.buf = output + offset;
    bw.capacity = bitstream_capacity;
    bw.byte_pos = 0;
    bw.bit_pos = 7;
    bw.buf[0] = 0;

    for (size_t i = 0; i < raw_size; i++) {
        uint8_t byte_val = raw_data[i];
        HuffmanCode *hc = &codes[byte_val];
        if (hc->length == 0) {
            /* 鐞嗚涓婁笉浼氬彂鐢燂細鎵€鏈夊嚭鐜板湪鏁版嵁涓殑瀛楄妭閮芥湁缂栫爜 */
            free(output);
            return false;
        }
        if (!bitwriter_write_bits(&bw, hc->code, hc->length)) {
            free(output);
            return false;
        }
    }

    size_t bitstream_bytes = bitwriter_flush(&bw);
    *out_data = output;
    *out_size = offset + bitstream_bytes;
    return true;
}

/* ========================================================================
 * Huffman 瑙ｇ爜锛堟浛鎹㈠師 entropy_decode_stub锛? * ======================================================================== */

/**
 * @brief Huffman鐔佃В鐮佸櫒
 *
 * 浠庡帇缂╂瘮鐗规祦涓噸寤哄師濮嬫暟鎹細
 *   1. 璇诲彇棰戠巼琛紙256 脳 uint32_t锛夊苟閲嶅缓Huffman鏍? *   2. 璇诲彇鍘熷鏁版嵁澶у皬
 *   3. 浣跨敤Huffman鏍戦€愪綅瑙ｇ爜姣旂壒娴侊紝杈撳嚭鍘熷瀛楄妭
 *
 * 杈撳叆鏍煎紡涓?entropy_encode_stub 杈撳嚭鏍煎紡瀹屽叏鍖归厤锛? *   [棰戠巼琛? 256 脳 4 瀛楄妭] + [鍘熷澶у皬: 4 瀛楄妭] + [缂栫爜姣旂壒娴乚
 *
 * @param[in]  data      鍘嬬缉鏁版嵁
 * @param[in]  size      鍘嬬缉鏁版嵁澶у皬锛堝瓧鑺傦級
 * @param[out] out_data  瑙ｅ帇鍚庣殑鍘熷鏁版嵁锛堣皟鐢ㄨ€呰礋璐ree锛? * @param[out] out_size  瑙ｅ帇鍚庢暟鎹ぇ灏忥紙瀛楄妭锛? * @return true 鎴愬姛锛宖alse 澶辫触
 */
static bool entropy_decode_stub(const uint8_t *data, size_t size, uint8_t **out_data, size_t *out_size) {
    if (!data || size == 0) {
        *out_data = NULL;
        *out_size = 0;
        return true;
    }

    /* 鏈€灏忔湁鏁堝ぇ灏忥細棰戠巼琛?1024) + 鍘熷澶у皬(4) + 鑷冲皯1瀛楄妭姣旂壒娴?*/
    size_t min_size = 256 * sizeof(uint32_t) + sizeof(uint32_t);
    if (size < min_size)
        return false;

    /* 鈹€鈹€ 姝ラ1锛氳鍙栭鐜囪〃骞堕噸寤篐uffman鏍?鈹€鈹€ */
    uint32_t freq[256];
    for (int i = 0; i < 256; i++) {
        freq[i] = ((uint32_t) data[i * 4 + 0]) | ((uint32_t) data[i * 4 + 1] << 8)
                | ((uint32_t) data[i * 4 + 2] << 16) | ((uint32_t) data[i * 4 + 3] << 24);
    }

    /* 璇诲彇鍘熷澶у皬 */
    size_t offset = 256 * sizeof(uint32_t);
    uint32_t raw_sz = ((uint32_t) data[offset + 0]) | ((uint32_t) data[offset + 1] << 8)
                    | ((uint32_t) data[offset + 2] << 16) | ((uint32_t) data[offset + 3] << 24);
    offset += sizeof(uint32_t);

    if (raw_sz == 0) {
        *out_data = NULL;
        *out_size = 0;
        return true;
    }

    /* 鍒嗛厤杈撳嚭缂撳啿鍖?*/
    uint8_t *output = (uint8_t *) lv00_malloc(raw_sz);
    if (!output)
        return false;

    /* 閲嶅缓Huffman鏍?*/
    HuffmanNode hnodes[HUFFMAN_MAX_NODES];
    memset(hnodes, 0, sizeof(hnodes));
    int node_count = 0;

    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            hnodes[node_count].left = -1;
            hnodes[node_count].right = -1;
            hnodes[node_count].parent = -1;
            hnodes[node_count].freq = freq[i];
            hnodes[node_count].byte_val = (uint8_t) i;
            node_count++;
        }
    }

    /* 澶勭悊鍗曞瓧绗︽儏鍐?*/
    if (node_count == 1) {
        hnodes[node_count].left = 0;
        hnodes[node_count].right = -1;
        hnodes[node_count].parent = -1;
        hnodes[node_count].freq = hnodes[0].freq;
        hnodes[node_count].byte_val = 0;
        hnodes[0].parent = node_count;
        node_count++;
    }

    /* 浣跨敤鏈€灏忓爢閲嶅缓Huffman鏍?*/
    int heap_nodes[HUFFMAN_MAX_NODES];
    MinHeap heap;
    heap.nodes = heap_nodes;
    heap.size = 0;
    heap.capacity = HUFFMAN_MAX_NODES;
    heap.hnodes = hnodes;

    for (int i = 0; i < node_count; i++) {
        heap_push(&heap, i);
    }

    while (heap.size > 1) {
        int left = heap_pop(&heap);
        int right = heap_pop(&heap);
        hnodes[node_count].left = left;
        hnodes[node_count].right = right;
        hnodes[node_count].parent = -1;
        hnodes[node_count].freq = hnodes[left].freq + hnodes[right].freq;
        hnodes[node_count].byte_val = 0;
        hnodes[left].parent = node_count;
        hnodes[right].parent = node_count;
        heap_push(&heap, node_count);
        node_count++;
    }

    int root = heap_pop(&heap);

    /* 鈹€鈹€ 姝ラ2锛氶€愪綅瑙ｇ爜 鈹€鈹€ */
    BitReader br;
    bitreader_init(&br, data + offset, size - offset);

    size_t decoded = 0;

    if (node_count == 2 && hnodes[root].right < 0) {
        /* 鍗曞瓧绗︾壒娈婃儏鍐碉細鎵€鏈夋瘮鐗归兘鏄?锛岀洿鎺ュ～鍏呰瀛楃 */
        uint8_t byte_val = hnodes[hnodes[root].left].byte_val;
        for (size_t i = 0; i < raw_sz; i++) {
            output[i] = byte_val;
        }
    } else {
        while (decoded < raw_sz) {
            int node = root;
            /* 娌挎爲閬嶅巻鐩村埌鍙跺瓙 */
            while (hnodes[node].left >= 0 || hnodes[node].right >= 0) {
                int bit = bitreader_read_bit(&br);
                if (bit < 0) {
                    free(output);
                    return false;
                }
                if (bit == 0) {
                    node = hnodes[node].left;
                } else {
                    node = hnodes[node].right;
                }
                if (node < 0) {
                    free(output);
                    return false;
                }
            }
            output[decoded++] = hnodes[node].byte_val;
        }
    }

    *out_data = output;
    *out_size = raw_sz;
    return true;
}

/* ========================================================================
 * 鍑犱綍鍘嬬缉涓?API
 * ======================================================================== */

/**
 * @brief 璁＄畻绾︽潫鍥句腑鍑犱綍鏁版嵁鐨勫師濮嬪瓧鑺傚ぇ灏忥紙浼板€硷級
 */
static size_t estimate_original_size(const ConstraintGraph *graph) {
    if (!graph)
        return 0;

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
    if (!graph || !out_size)
        return NULL;

    /* 鍏堣绠楀ぇ灏?*/
    size_t header = sizeof(int32_t); /* node_count */
    size_t body = 0;

    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node)
            continue;
        body += 2 * sizeof(int32_t); /* node_id, coord_count */
        if (node->symbolic_coords) {
            body += node->coord_count * sizeof(double);
        }
    }

    size_t total = header + body;
    uint8_t *buf = (uint8_t *) lv00_malloc(total);
    if (!buf)
        return NULL;

    /* 鍐欏叆鏁版嵁 */
    uint8_t *ptr = buf;
    int32_t count = (int32_t) graph->node_count;
    memcpy(ptr, &count, sizeof(int32_t));
    ptr += sizeof(int32_t);

    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node)
            continue;

        int32_t nid = (int32_t) node->id;
        int32_t cc = (int32_t) node->coord_count;
        memcpy(ptr, &nid, sizeof(int32_t));
        ptr += sizeof(int32_t);
        memcpy(ptr, &cc, sizeof(int32_t));
        ptr += sizeof(int32_t);

        for (int d = 0; d < node->coord_count; d++) {
            double val = symbolic_coord_to_double(node->symbolic_coords[d]);
            memcpy(ptr, &val, sizeof(double));
            ptr += sizeof(double);
        }
    }

    *out_size = (size_t) (ptr - buf);
    return buf;
}

bool geometry_compress(const ConstraintGraph *graph, const CompressConfig *config, uint8_t **out_data, size_t *out_size,
                       CompressMetadata *out_meta) {
    if (!graph || !out_data || !out_size)
        return false;

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
        out_meta->original_size = original_sz;
        out_meta->compressed_size = encoded_size;
        out_meta->compression_ratio = (encoded_size > 0) ? (double) original_sz / (double) encoded_size : 1.0;
        out_meta->node_count = graph->node_count;
        out_meta->constraint_count = graph->constraint_count;
        out_meta->edgebreaker_sequence = clers_seq;
        out_meta->sequence_len = clers_len;
    } else {
        free(clers_seq);
    }

    return true;
}

/* ========================================================================
 * 鍑犱綍瑙ｅ帇涓?API
 * ======================================================================== */

bool geometry_decompress(const uint8_t *data, size_t size, ConstraintGraph **out_graph) {
    if (!data || size == 0 || !out_graph)
        return false;

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
    buf[0] = (uint8_t) (val & 0xFF);
    buf[1] = (uint8_t) ((val >> 8) & 0xFF);
    buf[2] = (uint8_t) ((val >> 16) & 0xFF);
    buf[3] = (uint8_t) ((val >> 24) & 0xFF);
}

/**
 * @brief 灏?uint64 浠ュ皬绔簭鍐欏叆缂撳啿鍖? */
static void write_uint64_le(uint8_t *buf, uint64_t val) {
    for (int i = 0; i < 8; i++) {
        buf[i] = (uint8_t) ((val >> (i * 8)) & 0xFF);
    }
}

/**
 * @brief 浠庣紦鍐插尯浠ュ皬绔簭璇诲彇 uint32
 */
static uint32_t read_uint32_le(const uint8_t *buf) {
    return ((uint32_t) buf[0]) | ((uint32_t) buf[1] << 8) | ((uint32_t) buf[2] << 16) | ((uint32_t) buf[3] << 24);
}

/**
 * @brief 浠庣紦鍐插尯浠ュ皬绔簭璇诲彇 uint64
 */
static uint64_t read_uint64_le(const uint8_t *buf) {
    uint64_t val = 0;
    for (int i = 0; i < 8; i++) {
        val |= ((uint64_t) buf[i]) << (i * 8);
    }
    return val;
}

bool compress_write_lvzd(const uint8_t *data, size_t size, const char *filename) {
    if (!data || size == 0 || !filename)
        return false;

    FILE *fp = fopen(filename, "wb");
    if (!fp)
        return false;

    /* 鏋勫缓鏂囦欢澶?*/
    uint8_t header[LVZD_HEADER_SIZE];
    memset(header, 0, LVZD_HEADER_SIZE);

    write_uint32_le(header, LVZD_MAGIC);
    write_uint32_le(header + 4, LVZD_VERSION_MAJOR);
    write_uint32_le(header + 8, LVZD_VERSION_MINOR);
    write_uint64_le(header + 12, (uint64_t) size); /* original_size = compressed_size for stub */
    write_uint64_le(header + 20, (uint64_t) size); /* compressed_size */

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

bool compress_read_lvzd(const char *filename, uint8_t **out_data, size_t *out_size) {
    if (!filename || !out_data || !out_size)
        return false;

    FILE *fp = fopen(filename, "rb");
    if (!fp)
        return false;

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
    (void) ver_minor; /* 娆＄増鏈悜鍓嶅吋瀹?*/

    /* 璇诲彇鍘嬬缉鏁版嵁澶у皬 */
    uint64_t comp_size = read_uint64_le(header + 20);
    if (comp_size == 0) {
        fclose(fp);
        *out_data = NULL;
        *out_size = 0;
        return true;
    }

    /* 鍒嗛厤缂撳啿鍖哄苟璇诲彇鍘嬬缉鏁版嵁 */
    uint8_t *buf = (uint8_t *) lv00_malloc((size_t) comp_size);
    if (!buf) {
        fclose(fp);
        return false;
    }

    read_bytes = fread(buf, 1, (size_t) comp_size, fp);
    fclose(fp);

    if (read_bytes != (size_t) comp_size) {
        free(buf);
        return false;
    }

    *out_data = buf;
    *out_size = (size_t) comp_size;
    return true;
}
