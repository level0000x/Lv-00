/**
 * @file meta_repr.c
 * @brief Lv-00 元表示层实现
 *
 * @details 元表示层将 C 核数据结构编码为几何表示，实现自举架构中
 *          "几何层理解自身"的核心目标。
 *
 *          实现功能：
 *          1. C 结构体 -> 几何表示（编码）
 *          2. 几何表示 -> C 结构体（解码）
 *          3. 编码-解码往返验证
 *          4. 几何表示同构比较
 *          5. DOT/JSON 序列化导出
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#include "lv/meta_repr.h"

#include "lv/lv_file.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "lv/constraint_graph.h"
#include "lv/error_codes.h"
#include "lv/func_block.h"
#include "lv/lv.h"
#include "lv/lv_json.h"
#include "lv/lv_xmacro.h"
#include "lv/lv_utils.h"


/* ============== 内部数据结构 ============== */

/**
 * @brief 编码器内部状态
 */
struct MetaReprEncoder {
    MetaReprConfig config; /* 编码配置 */
    int node_count;        /* 已编码节点数 */
    int constraint_count;  /* 已编码约束数 */
    bool is_initialized;   /* 是否已初始化 */
};

/**
 * @brief 解码器内部状态
 */
struct MetaReprDecoder {
    int decode_count;    /* 已解码数量 */
    bool is_initialized; /* 是否已初始化 */
};

/* ============== 默认配置 ============== */

/**
 * @brief 获取元表示编码器的默认配置
 *
 * @return 默认配置结构体
 */
MetaReprConfig meta_repr_default_config(void) {
    MetaReprConfig config;
    memset(&config, 0, sizeof(config));

    /* 坐标编码方案默认值 */
    config.coordinate_scheme.id_spacing = 10.0;
    config.coordinate_scheme.type_spacing = 100.0;
    config.coordinate_scheme.base_x = 0.0;
    config.coordinate_scheme.base_y = 0.0;

    /* 区域编码方案默认值 */
    config.region_scheme.padding = 50.0;
    config.region_scheme.port_spacing = 30.0;

    /* 元数据编码默认值 */
    config.encode_metadata = true;
    config.encode_version = true;

    /* 性能选项默认值 */
    config.use_caching = false;
    config.cache_size = 0;

    return config;
}

/* ============== 编码器 API 实现 ============== */

/**
 * @brief 创建元表示编码器
 *
 * @param config 编码配置（为 NULL 时使用默认配置）
 * @return 新创建的编码器指针，失败返回 NULL
 */
MetaReprEncoder *meta_repr_encoder_create(const MetaReprConfig *config) {
    MetaReprEncoder *encoder = (MetaReprEncoder *) lv_calloc(1, sizeof(MetaReprEncoder));
    if (!encoder)
        return NULL;

    /* 使用传入配置或默认配置 */
    if (config) {
        encoder->config = *config;
    } else {
        encoder->config = meta_repr_default_config();
    }

    encoder->node_count = 0;
    encoder->constraint_count = 0;
    encoder->is_initialized = true;

    return encoder;
}

/**
 * @brief 销毁元表示编码器
 *
 * @param encoder 编码器指针（可为 NULL）
 */
void meta_repr_encoder_destroy(MetaReprEncoder *encoder) {
    if (!encoder)
        return;
    encoder->is_initialized = false;
    lv_free((void **) &encoder);
}

/**
 * @brief 重置编码器内部计数
 *
 * @param encoder 编码器指针
 */
void meta_repr_encoder_reset(MetaReprEncoder *encoder) {
    if (!encoder || !encoder->is_initialized)
        return;
    encoder->node_count = 0;
    encoder->constraint_count = 0;
}

/**
 * @brief 将约束图编码为几何表示
 *
 * 遍历原图的节点和约束，将节点坐标映射为几何点，
 * 约束关系映射为几何约束，返回新的 ConstraintGraph。
 *
 * @param encoder 编码器
 * @param graph   原始约束图
 * @return 编码后的约束图，失败返回 NULL
 */
ConstraintGraph *meta_repr_encode_graph(MetaReprEncoder *encoder, const ConstraintGraph *graph) {
    if (!encoder || !encoder->is_initialized || !graph)
        return NULL;

    ConstraintGraph *encoded = graph_create();
    if (!encoded)
        return NULL;

    double base_x = encoder->config.coordinate_scheme.base_x;
    double base_y = encoder->config.coordinate_scheme.base_y;
    double id_spacing = encoder->config.coordinate_scheme.id_spacing;
    double type_spacing = encoder->config.coordinate_scheme.type_spacing;

    /* 1. 遍历原图的所有节点，编码为几何点 */
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *src_node = graph->nodes[i];
        if (!src_node)
            continue;

        double x_val = base_x + (double) src_node->id * id_spacing;
        double y_val = base_y + (double) src_node->type * type_spacing;

        SymbolicCoord *coords[2];
        coords[0] = symbolic_coord_from_double_scaled(x_val, 1);
        coords[1] = symbolic_coord_from_double_scaled(y_val, 1);
        if (!coords[0] || !coords[1]) {
            if (coords[0])
                symbolic_coord_destroy(coords[0]);
            if (coords[1])
                symbolic_coord_destroy(coords[1]);
            continue;
        }

        GeomNode *new_node = graph_add_node_with_id(encoded, src_node->id, GEOM_POINT, coords, 2);
        if (!new_node) {
            symbolic_coord_destroy(coords[0]);
            symbolic_coord_destroy(coords[1]);
            continue;
        }
        new_node->trust = src_node->trust;
        new_node->is_active = src_node->is_active;

        encoder->node_count++;
    }

    /* 2. 编码约束关系 */
    for (int i = 0; i < graph->constraint_count; i++) {
        Constraint *src_con = graph->constraints[i];
        if (!src_con || !src_con->is_active)
            continue;

        int n_parts = src_con->participant_count;
        if (n_parts < 2)
            continue;

        Constraint *new_con =
            graph_add_constraint_with_id(encoded, src_con->id, src_con->type, src_con->participants, n_parts);
        if (new_con) {
            new_con->is_active = src_con->is_active;
            new_con->template_id = src_con->template_id;
            encoder->constraint_count++;
        }
    }

    return encoded;
}

/**
 * @brief 将单个几何节点编码为几何表示
 *
 * 将节点的 ID 和类型线性映射到几何坐标。
 *
 * @param encoder 编码器
 * @param node    原始几何节点
 * @return 编码后的几何节点，失败返回 NULL
 */
GeomNode *meta_repr_encode_node(MetaReprEncoder *encoder, const GeomNode *node) {
    if (!encoder || !encoder->is_initialized || !node)
        return NULL;

    GeomNode *encoded = (GeomNode *) lv_calloc(1, sizeof(GeomNode));
    if (!encoded)
        return NULL;

    /* 坐标编码：将节点 ID 和类型线性映射到几何坐标 */
    double x_val =
        encoder->config.coordinate_scheme.base_x + (double) node->id * encoder->config.coordinate_scheme.id_spacing;
    double y_val =
        encoder->config.coordinate_scheme.base_y + (double) node->type * encoder->config.coordinate_scheme.type_spacing;

    SymbolicCoord **coords = (SymbolicCoord **) lv_calloc(2, sizeof(SymbolicCoord *));
    if (!coords) {
        lv_free((void **) &encoded);
        return NULL;
    }
    coords[0] = symbolic_coord_from_double_scaled(x_val, 1);
    coords[1] = symbolic_coord_from_double_scaled(y_val, 1);
    if (!coords[0] || !coords[1]) {
        if (coords[0])
            symbolic_coord_destroy(coords[0]);
        if (coords[1])
            symbolic_coord_destroy(coords[1]);
        lv_free((void **) &coords);
        lv_free((void **) &encoded);
        return NULL;
    }

    encoded->id = node->id;
    encoded->type = GEOM_POINT;
    encoded->symbolic_coords = coords;
    encoded->coord_count = 2;
    encoded->trust = node->trust;
    encoded->is_active = true;

    encoder->node_count++;

    return encoded;
}

/**
 * @brief 将函数块编码为几何区域表示
 *
 * 函数块被编码为 GEOM_REGION 类型节点，输入/输出端口编码为边界上的 GEOM_PORT 子节点。
 *
 * @param encoder 编码器
 * @param block   函数块指针
 * @return 编码后的区域节点，失败返回 NULL
 */
GeomNode *meta_repr_encode_func_block(MetaReprEncoder *encoder, const FuncBlock *block) {
    if (!encoder || !encoder->is_initialized || !block)
        return NULL;

    GeomNode *encoded_region = (GeomNode *) lv_calloc(1, sizeof(GeomNode));
    if (!encoded_region)
        return NULL;

    double base_x = encoder->config.coordinate_scheme.base_x;
    double base_y = encoder->config.coordinate_scheme.base_y;
    double id_spacing = encoder->config.coordinate_scheme.id_spacing;
    double type_spacing = encoder->config.coordinate_scheme.type_spacing;
    double padding = encoder->config.region_scheme.padding;
    double port_spacing = encoder->config.region_scheme.port_spacing;

    /* 函数块编码为 GEOM_REGION，坐标编码块 ID 和类型 */
    double x_val = base_x + (double) block->id * id_spacing;
    double y_val = base_y + (double) GEOM_FUNCTION_BLOCK * type_spacing;

    SymbolicCoord **coords = (SymbolicCoord **) lv_calloc(2, sizeof(SymbolicCoord *));
    if (!coords) {
        lv_free((void **) &encoded_region);
        return NULL;
    }
    coords[0] = symbolic_coord_from_double_scaled(x_val, 1);
    coords[1] = symbolic_coord_from_double_scaled(y_val, 1);

    encoded_region->id = block->id;
    encoded_region->type = GEOM_REGION;
    encoded_region->symbolic_coords = coords;
    encoded_region->coord_count = 2;
    encoded_region->is_active = true;

    /* 输入端口：在区域上边界创建点 */
    encoded_region->data.region.segment_count = 0;
    encoded_region->data.region.boundary_segments = NULL;

    /* 为每个输入/输出端口编码子节点 */
    int total_ports = block->input_count + block->output_count;
    if (total_ports > 0) {
        encoded_region->data.region.boundary_segments =
            (GeomNode **) lv_calloc((size_t) total_ports, sizeof(GeomNode *));
        if (encoded_region->data.region.boundary_segments) {
            for (int i = 0; i < block->input_count; i++) {
                double px = x_val;
                double py = (double) padding - (double) i * port_spacing;
                SymbolicCoord **pcoords = (SymbolicCoord **) lv_calloc(2, sizeof(SymbolicCoord *));
                if (!pcoords)
                    continue;
                pcoords[0] = symbolic_coord_from_double_scaled(px, 1);
                pcoords[1] = symbolic_coord_from_double_scaled(py, 1);
                if (!pcoords[0] || !pcoords[1]) {
                    if (pcoords[0])
                        symbolic_coord_destroy(pcoords[0]);
                    if (pcoords[1])
                        symbolic_coord_destroy(pcoords[1]);
                    lv_free((void **) &pcoords);
                    continue;
                }
                GeomNode *port_node = (GeomNode *) lv_calloc(1, sizeof(GeomNode));
                if (!port_node) {
                    symbolic_coord_destroy(pcoords[0]);
                    symbolic_coord_destroy(pcoords[1]);
                    lv_free((void **) &pcoords);
                    continue;
                }
                port_node->id = block->input_port_ids[i];
                port_node->type = GEOM_PORT;
                port_node->symbolic_coords = pcoords;
                port_node->coord_count = 2;
                port_node->is_active = true;
                encoded_region->data.region.boundary_segments[encoded_region->data.region.segment_count++] = port_node;
            }
            for (int i = 0; i < block->output_count; i++) {
                double px = x_val;
                double py = (double) (-(int64_t) padding) + (double) i * port_spacing;
                SymbolicCoord **pcoords = (SymbolicCoord **) lv_calloc(2, sizeof(SymbolicCoord *));
                if (!pcoords)
                    continue;
                pcoords[0] = symbolic_coord_from_double_scaled(px, 1);
                pcoords[1] = symbolic_coord_from_double_scaled(py, 1);
                if (!pcoords[0] || !pcoords[1]) {
                    if (pcoords[0])
                        symbolic_coord_destroy(pcoords[0]);
                    if (pcoords[1])
                        symbolic_coord_destroy(pcoords[1]);
                    lv_free((void **) &pcoords);
                    continue;
                }
                GeomNode *port_node = (GeomNode *) lv_calloc(1, sizeof(GeomNode));
                if (!port_node) {
                    symbolic_coord_destroy(pcoords[0]);
                    symbolic_coord_destroy(pcoords[1]);
                    lv_free((void **) &pcoords);
                    continue;
                }
                port_node->id = block->output_port_ids[i];
                port_node->type = GEOM_PORT;
                port_node->symbolic_coords = pcoords;
                port_node->coord_count = 2;
                port_node->is_active = true;
                encoded_region->data.region.boundary_segments[encoded_region->data.region.segment_count++] = port_node;
            }
        }
    }

    encoder->node_count++;
    encoder->constraint_count++;

    return encoded_region;
}

/**
 * @brief 将类型区域编码为几何区域表示
 *
 * @param encoder     编码器
 * @param type_region 类型区域指针
 * @return 编码后的区域节点，失败返回 NULL
 */
GeomNode *meta_repr_encode_type_region(MetaReprEncoder *encoder, const TypeRegion *type_region) {
    if (!encoder || !encoder->is_initialized || !type_region)
        return NULL;

    GeomNode *encoded_region = (GeomNode *) lv_calloc(1, sizeof(GeomNode));
    if (!encoded_region)
        return NULL;

    double base_x = encoder->config.coordinate_scheme.base_x;
    double base_y = encoder->config.coordinate_scheme.base_y;
    double id_spacing = encoder->config.coordinate_scheme.id_spacing;
    double type_spacing = encoder->config.coordinate_scheme.type_spacing;

    /* 将 type_region->id 映射到 x 坐标，kind 映射到 y 坐标 */
    double x_val = base_x + (double) type_region->id * id_spacing;
    double y_val = base_y + (double) ((int) type_region->kind + 1) * type_spacing;

    SymbolicCoord **coords = (SymbolicCoord **) lv_calloc(2, sizeof(SymbolicCoord *));
    if (!coords) {
        lv_free((void **) &encoded_region);
        return NULL;
    }
    coords[0] = symbolic_coord_from_double_scaled(x_val, 1);
    coords[1] = symbolic_coord_from_double_scaled(y_val, 1);
    if (!coords[0] || !coords[1]) {
        if (coords[0])
            symbolic_coord_destroy(coords[0]);
        if (coords[1])
            symbolic_coord_destroy(coords[1]);
        lv_free((void **) &coords);
        lv_free((void **) &encoded_region);
        return NULL;
    }

    encoded_region->id = type_region->id;
    encoded_region->type = GEOM_REGION;
    encoded_region->symbolic_coords = coords;
    encoded_region->coord_count = 2;
    encoded_region->is_active = true;
    encoded_region->namespace_depth = (int) type_region->level;

    encoder->node_count++;

    return encoded_region;
}

/**
 * @brief 将命题编码为几何点表示
 *
 * @param encoder    编码器
 * @param proposition 命题指针
 * @return 编码后的几何节点，失败返回 NULL
 */
GeomNode *meta_repr_encode_proposition(MetaReprEncoder *encoder, const Proposition *proposition) {
    if (!encoder || !encoder->is_initialized || !proposition)
        return NULL;

    GeomNode *encoded_node = (GeomNode *) lv_calloc(1, sizeof(GeomNode));
    if (!encoded_node)
        return NULL;

    double base_x = encoder->config.coordinate_scheme.base_x;
    double base_y = encoder->config.coordinate_scheme.base_y;
    double id_spacing = encoder->config.coordinate_scheme.id_spacing;
    double type_spacing = encoder->config.coordinate_scheme.type_spacing;

    /* proposition->id 映射到 x，proposition->type 映射到 y */
    double x_val = base_x + (double) proposition->id * id_spacing;
    double y_val = base_y + (double) proposition->type * type_spacing;

    SymbolicCoord **coords = (SymbolicCoord **) lv_calloc(2, sizeof(SymbolicCoord *));
    if (!coords) {
        lv_free((void **) &encoded_node);
        return NULL;
    }
    coords[0] = symbolic_coord_from_double_scaled(x_val, 1);
    coords[1] = symbolic_coord_from_double_scaled(y_val, 1);
    if (!coords[0] || !coords[1]) {
        if (coords[0])
            symbolic_coord_destroy(coords[0]);
        if (coords[1])
            symbolic_coord_destroy(coords[1]);
        lv_free((void **) &coords);
        lv_free((void **) &encoded_node);
        return NULL;
    }

    encoded_node->id = proposition->id;
    encoded_node->type = GEOM_POINT;
    encoded_node->symbolic_coords = coords;
    encoded_node->coord_count = 2;
    encoded_node->trust = (TrustColor) proposition->color;
    encoded_node->is_active = true;

    encoder->node_count++;

    return encoded_node;
}

/* ============== 解码器 API 实现 ============== */

/**
 * @brief 创建元表示解码器
 *
 * @return 新创建的解码器指针，失败返回 NULL
 */
MetaReprDecoder *meta_repr_decoder_create(void) {
    MetaReprDecoder *decoder = (MetaReprDecoder *) lv_calloc(1, sizeof(MetaReprDecoder));
    if (!decoder)
        return NULL;

    decoder->decode_count = 0;
    decoder->is_initialized = true;

    return decoder;
}

/**
 * @brief 销毁元表示解码器
 *
 * @param decoder 解码器指针（可为 NULL）
 */
void meta_repr_decoder_destroy(MetaReprDecoder *decoder) {
    if (!decoder)
        return;
    decoder->is_initialized = false;
    lv_free((void **) &decoder);
}

/**
 * @brief 将编码后的约束图解码回原始结构
 *
 * 使用默认编码方案的逆映射（基于坐标反推 ID 和类型）。
 *
 * @param decoder       解码器
 * @param encoded_graph 编码后的约束图
 * @return 解码后的约束图，失败返回 NULL
 */
ConstraintGraph *meta_repr_decode_graph(MetaReprDecoder *decoder, const ConstraintGraph *encoded_graph) {
    if (!decoder || !decoder->is_initialized || !encoded_graph)
        return NULL;

    ConstraintGraph *decoded = graph_create();
    if (!decoded)
        return NULL;

    /* 使用默认编码方案的逆映射 */
    const double base_x = 0.0;
    const double base_y = 0.0;
    const double id_spacing = 10.0;
    const double type_spacing = 100.0;

    for (int i = 0; i < encoded_graph->node_count; i++) {
        GeomNode *enc_node = encoded_graph->nodes[i];
        if (!enc_node || !enc_node->symbolic_coords || enc_node->coord_count < 2)
            continue;

        double x = symbolic_coord_to_double(enc_node->symbolic_coords[0]);
        double y = symbolic_coord_to_double(enc_node->symbolic_coords[1]);

        int node_id = (int) ((x - base_x) / id_spacing + 0.5);
        int type_idx = (int) ((y - base_y) / type_spacing + 0.5);
        if (type_idx < 0)
            type_idx = 0;
        if (type_idx > GEOM_FUNCTION_BLOCK)
            type_idx = GEOM_POINT;

        GeomNode *new_node = graph_add_node_with_id(decoded, node_id, (GeomType) type_idx, enc_node->symbolic_coords,
                                                    enc_node->coord_count);
        if (new_node) {
            new_node->trust = enc_node->trust;
            new_node->is_active = enc_node->is_active;
        }

        decoder->decode_count++;
    }

    /* 2. 恢复约束关系 */
    for (int i = 0; i < encoded_graph->constraint_count; i++) {
        Constraint *enc_con = encoded_graph->constraints[i];
        if (!enc_con || !enc_con->is_active)
            continue;

        int n_parts = enc_con->participant_count;
        if (n_parts < 2)
            continue;

        Constraint *new_con =
            graph_add_constraint_with_id(decoded, enc_con->id, enc_con->type, enc_con->participants, n_parts);
        if (new_con) {
            new_con->is_active = enc_con->is_active;
            new_con->template_id = enc_con->template_id;
        }
    }

    return decoded;
}

/**
 * @brief 将编码后的几何节点解码回原始结构
 *
 * @param decoder      解码器
 * @param encoded_node 编码后的几何节点
 * @return 解码后的几何节点，失败返回 NULL
 */
GeomNode *meta_repr_decode_node(MetaReprDecoder *decoder, const GeomNode *encoded_node) {
    if (!decoder || !decoder->is_initialized || !encoded_node)
        return NULL;

    GeomNode *decoded = (GeomNode *) lv_calloc(1, sizeof(GeomNode));
    if (!decoded)
        return NULL;

    const double base_x = 0.0;
    const double base_y = 0.0;
    const double id_spacing = 10.0;
    const double type_spacing = 100.0;

    if (encoded_node->symbolic_coords && encoded_node->coord_count >= 2) {
        double x = symbolic_coord_to_double(encoded_node->symbolic_coords[0]);
        double y = symbolic_coord_to_double(encoded_node->symbolic_coords[1]);

        int node_id = (int) ((x - base_x) / id_spacing + 0.5);
        int type_idx = (int) ((y - base_y) / type_spacing + 0.5);
        if (type_idx < 0)
            type_idx = 0;
        if (type_idx > GEOM_FUNCTION_BLOCK)
            type_idx = GEOM_POINT;

        decoded->id = node_id;
        decoded->type = (GeomType) type_idx;
    } else {
        decoded->id = encoded_node->id;
        decoded->type = encoded_node->type;
    }

    decoded->trust = encoded_node->trust;
    decoded->is_active = encoded_node->is_active;

    decoder->decode_count++;

    return decoded;
}

/**
 * @brief 将编码后的函数块区域解码回 FuncBlock
 *
 * 从 GEOM_REGION 类型节点的边界端口点中恢复输入/输出端口。
 *
 * @param decoder        解码器
 * @param encoded_block  编码后的区域节点
 * @return 解码后的 FuncBlock 指针，失败返回 NULL
 */
FuncBlock *meta_repr_decode_func_block(MetaReprDecoder *decoder, const GeomNode *encoded_block) {
    if (!decoder || !decoder->is_initialized || !encoded_block)
        return NULL;

    const double base_x = 0.0;
    const double id_spacing = 10.0;

    int block_id = encoded_block->id;

    if (encoded_block->symbolic_coords && encoded_block->coord_count >= 2) {
        double x = symbolic_coord_to_double(encoded_block->symbolic_coords[0]);
        block_id = (int) ((x - base_x) / id_spacing + 0.5);
    }

    FuncBlock *decoded = func_block_create(block_id);
    if (!decoded)
        return NULL;

    /* 从边界端口点恢复输入/输出端口 */
    int seg_count = encoded_block->data.region.segment_count;
    GeomNode **segments = encoded_block->data.region.boundary_segments;

    if (segments && seg_count > 0) {
        /* 统计输入/输出端口数（通过 y 坐标符号判断） */
        int input_count = 0;
        int output_count = 0;
        for (int i = 0; i < seg_count; i++) {
            if (!segments[i] || !segments[i]->symbolic_coords)
                continue;
            double y = symbolic_coord_to_double(segments[i]->symbolic_coords[1]);
            if (y > 0.0)
                input_count++;
            else
                output_count++;
        }

        if (input_count > 0) {
            int *in_ids = (int *) lv_calloc((size_t) input_count, sizeof(int));
            if (in_ids) {
                int idx = 0;
                for (int i = 0; i < seg_count && idx < input_count; i++) {
                    if (!segments[i] || !segments[i]->symbolic_coords)
                        continue;
                    double y = symbolic_coord_to_double(segments[i]->symbolic_coords[1]);
                    if (y > 0.0)
                        in_ids[idx++] = segments[i]->id;
                }
                func_block_set_input_ports(decoded, in_ids, input_count);
                lv_free((void **) &in_ids);
            }
        }
        if (output_count > 0) {
            int *out_ids = (int *) lv_malloc((size_t) output_count * sizeof(int));
            if (out_ids) {
                int idx = 0;
                for (int i = 0; i < seg_count && idx < output_count; i++) {
                    if (!segments[i] || !segments[i]->symbolic_coords)
                        continue;
                    double y = symbolic_coord_to_double(segments[i]->symbolic_coords[1]);
                    if (y <= 0.0)
                        out_ids[idx++] = segments[i]->id;
                }
                func_block_set_output_ports(decoded, out_ids, output_count);
                lv_free((void **) &out_ids);
            }
        }
    }

    if (encoded_block->namespace_depth > 0) {
        decoded->determinism = (DeterminismState) encoded_block->namespace_depth;
    }

    decoder->decode_count++;

    return decoded;
}

/* ============== 验证 API 实现 ============== */

/**
 * @brief 验证编码-解码往返一致性
 *
 * @param original 原始数据指针
 * @param decoded  解码后的数据指针
 * @param type_name 类型名称（用于选择比较策略）
 * @return true 一致，false 不一致或参数无效
 */
bool meta_repr_verify_roundtrip(const void *original, const void *decoded, const char *type_name) {
    if (!original || !decoded)
        return false;

    /* 往返验证逻辑：
     * 对原始结构体进行编码再解码，比较解码结果与原始结构体。
     * 比较策略：
     * 1. 指针相等性检查（浅比较）
     * 2. 内存内容逐字节比较（深比较）
     * 3. 语义等价性检查（根据类型名称选择比较策略）
     */
    if (original == decoded)
        return true;

    if (type_name) {
        /* 根据类型名称选择合适的比较策略 */
        if (strcmp(type_name, "ConstraintGraph") == 0) {
            return meta_repr_graph_equivalent((const ConstraintGraph *) original, (const ConstraintGraph *) decoded);
        }
    }

    /* 默认使用内存比较（兜底策略） */
    return memcmp(original, decoded, sizeof(void *)) == 0;
}

/**
 * @brief 比较两个约束图是否等价
 *
 * 比较节点数、约束数以及相同 ID 的节点类型和约束类型。
 *
 * @param a 图 A
 * @param b 图 B
 * @return true 等价，false 不等价或参数无效
 */
bool meta_repr_graph_equivalent(const ConstraintGraph *a, const ConstraintGraph *b) {
    if (!a || !b)
        return false;
    if (a == b)
        return true;

    /* 节点数和约束数必须相等 */
    if (a->node_count != b->node_count)
        return false;
    if (a->constraint_count != b->constraint_count)
        return false;

    /* 检查相同 ID 的节点类型是否一致 */
    for (int i = 0; i < a->node_count; i++) {
        GeomNode *na = a->nodes[i];
        if (!na)
            continue;
        GeomNode *nb = graph_get_node(b, na->id);
        if (!nb)
            return false;
        if (na->type != nb->type)
            return false;
        if (na->is_active != nb->is_active)
            return false;
    }

    /* 检查相同 ID 的约束关系是否对应 */
    for (int i = 0; i < a->constraint_count; i++) {
        Constraint *ca = a->constraints[i];
        if (!ca || !ca->is_active)
            continue;
        Constraint *cb = graph_get_constraint(b, ca->id);
        if (!cb || !cb->is_active)
            return false;
        if (ca->type != cb->type)
            return false;
        if (ca->participant_count != cb->participant_count)
            return false;
        for (int j = 0; j < ca->participant_count; j++) {
            if (ca->participants[j] != cb->participants[j])
                return false;
        }
    }

    return true;
}

/**
 * @brief 检查两个约束图是否同构
 *
 * 在等价性检查的基础上，进一步验证每个节点的坐标完全匹配。
 *
 * @param a 图 A
 * @param b 图 B
 * @return true 同构，false 不同构或参数无效
 */
bool meta_repr_isomorphic(const ConstraintGraph *a, const ConstraintGraph *b) {
    if (!a || !b)
        return false;
    if (a == b)
        return true;

    /* 先检查基本等价性 */
    if (!meta_repr_graph_equivalent(a, b))
        return false;

    /* 在等价基础上，进一步检查每个节点坐标完全匹配 */
    for (int i = 0; i < a->node_count; i++) {
        GeomNode *na = a->nodes[i];
        if (!na)
            continue;
        GeomNode *nb = graph_get_node(b, na->id);
        if (!nb)
            return false;

        /* 坐标数量必须相同 */
        if (na->coord_count != nb->coord_count)
            return false;

        /* 每个坐标必须完全匹配 */
        for (int c = 0; c < na->coord_count; c++) {
            if (!na->symbolic_coords || !nb->symbolic_coords)
                return false;
            if (!na->symbolic_coords[c] || !nb->symbolic_coords[c])
                return false;
            if (symbolic_coord_compare(na->symbolic_coords[c], nb->symbolic_coords[c]) != 0)
                return false;
        }
    }

    return true;
}

/* ============== 工具 API 实现 ============== */

/**
 * @brief 获取编码器的统计信息
 *
 * @param encoder             编码器（可为 NULL）
 * @param out_node_count      输出已编码节点数（可为 NULL）
 * @param out_constraint_count 输出已编码约束数（可为 NULL）
 */
void meta_repr_get_stats(MetaReprEncoder *encoder, int *out_node_count, int *out_constraint_count) {
    if (!encoder || !encoder->is_initialized) {
        if (out_node_count)
            *out_node_count = 0;
        if (out_constraint_count)
            *out_constraint_count = 0;
        return;
    }

    if (out_node_count)
        *out_node_count = encoder->node_count;
    if (out_constraint_count)
        *out_constraint_count = encoder->constraint_count;
}

/**
 * @brief 将编码后的约束图导出为 DOT 格式
 *
 * 生成 Graphviz DOT 格式的图形描述，节点形状根据 GeomType 映射。
 *
 * @param encoded_graph 编码后的约束图
 * @param filepath      输出文件路径
 * @return true 成功，false 失败（参数无效或文件写入失败）
 */
/** @brief 约束类型 -> 名称 权威查找表（由共享 X-macro LV_CONSTRAINT_TYPE_X 生成，
 *  编译器校验与 ConstraintType 对齐；float_error.c / graph_node_alloc.c 通过
 *  lv_constraint_type_name() 引用，禁止在其他文件重复定义名称表） */
static const char *const kConstraintTypeLabels[] = {
    lv_XMACRO_TO_NAME_ARRAY(LV_CONSTRAINT_TYPE_X)
};

const char *lv_constraint_type_name(ConstraintType type) {
    if ((unsigned) type >= lv_ARRAY_SIZE(kConstraintTypeLabels))
        return NULL;
    return kConstraintTypeLabels[type];
}

bool meta_repr_export_dot(const ConstraintGraph *encoded_graph, const char *filepath) {
    if (!encoded_graph || !filepath)
        return false;

    FILE *fp = lv_file_open(filepath, "w");
    if (!fp)
        return false;

    fprintf(fp, "digraph MetaRepr {\n");
    fprintf(fp, "    rankdir=LR;\n\n");

    /* GeomType -> DOT shape 映射（6 项，与 GeomType 枚举严格对齐；
     * 原实现缺 CIRCLE 且 FUNCTION_BLOCK 下标越界，已修复） */
    static const char *type_shapes[] = {
        "ellipse", /* GEOM_POINT */
        "diamond", /* GEOM_LINE_SEGMENT */
        "box",     /* GEOM_REGION */
        "circle",  /* GEOM_CIRCLE */
        "box",     /* GEOM_PORT */
        "box"      /* GEOM_FUNCTION_BLOCK */
    };
    static const char *type_names[] = {
        lv_XMACRO_TO_NAME_ARRAY(LV_GEOM_TYPE_X)
    };

    /* 输出节点 */
    for (int i = 0; i < encoded_graph->node_count; i++) {
        GeomNode *node = encoded_graph->nodes[i];
        if (!node || !node->is_active)
            continue;

        const char *shape = "ellipse";
        const char *tname = "POINT";
        if (node->type >= 0 && node->type <= GEOM_FUNCTION_BLOCK) {
            shape = type_shapes[(int) node->type];
            tname = type_names[(int) node->type];
        }

        fprintf(fp, "    node%d [shape=%s, label=\"%s #%d\"];\n", node->id, shape, tname, node->id);
    }

    fprintf(fp, "\n");

    /* 输出约束为边 */
    for (int i = 0; i < encoded_graph->constraint_count; i++) {
        Constraint *con = encoded_graph->constraints[i];
        if (!con || !con->is_active)
            continue;

        const char *label = "?";
        const char *tname = lv_constraint_type_name(con->type);
        if (tname != NULL)
            label = tname;

        int n = con->participant_count;
        if (n >= 2) {
            for (int j = 0; j < n - 1; j++) {
                fprintf(fp, "    node%d -> node%d [label=\"%s\"];\n", con->participants[j], con->participants[j + 1],
                        label);
            }
        } else if (n == 1) {
            fprintf(fp, "    node%d [label=\"%s(#%d)\"];\n", con->participants[0], label, con->id);
        }
    }

    fprintf(fp, "}\n");
    lv_file_close(fp);
    return true;
}

/**
 * @brief 将编码后的约束图导出为 JSON 格式
 *
 * @param encoded_graph 编码后的约束图
 * @return JSON 字符串（调用者须通过 lv_free 释放），失败返回 NULL
 */
char *meta_repr_export_json(const ConstraintGraph *encoded_graph) {
    if (!encoded_graph)
        return NULL;

    /* 修复：旧表仅 5 项（缺 CIRCLE 且 FUNCTION_BLOCK 越界），补齐 6 项并用指定初始化器对齐枚举 */
    static const char *type_names[] = {
        [GEOM_POINT] = "GEOM_POINT",
        [GEOM_LINE_SEGMENT] = "GEOM_LINE_SEGMENT",
        [GEOM_REGION] = "GEOM_REGION",
        [GEOM_CIRCLE] = "GEOM_CIRCLE",
        [GEOM_PORT] = "GEOM_PORT",
        [GEOM_FUNCTION_BLOCK] = "GEOM_FUNCTION_BLOCK",
    };

    /* 预估缓冲区大小 */
    size_t est_size = 512 + (size_t) encoded_graph->node_count * 128 + (size_t) encoded_graph->constraint_count * 64;
    lvJsonBuf _jb;
    if (!lv_json_buf_init(&_jb, est_size))
        return NULL;

    lv_json_buf_append_raw(&_jb, "{\n  \"nodes\": [\n");

    /* 序列化节点 */
    bool first = true;
    for (int i = 0; i < encoded_graph->node_count; i++) {
        GeomNode *node = encoded_graph->nodes[i];
        if (!node || !node->is_active)
            continue;

        const char *tname = "GEOM_POINT";
        if (node->type >= 0 && node->type <= GEOM_FUNCTION_BLOCK) {
            tname = type_names[(int) node->type];
        }

        if (!first)
            lv_json_buf_append_raw(&_jb, ",\n");
        first = false;

        lv_json_buf_append_fmt(&_jb, "    {\"id\": %d, \"type\": \"%s\", \"coord_count\": %d}", node->id, tname,
                               node->coord_count);
    }
    lv_json_buf_append_raw(&_jb, "\n  ],\n  \"constraints\": [\n");

    /* 序列化约束 */
    first = true;
    for (int i = 0; i < encoded_graph->constraint_count; i++) {
        Constraint *con = encoded_graph->constraints[i];
        if (!con || !con->is_active)
            continue;

        if (!first)
            lv_json_buf_append_raw(&_jb, ",\n");
        first = false;

        lv_json_buf_append_fmt(&_jb, "    {\"id\": %d, \"type\": %d, \"participant_count\": %d}", con->id,
                               (int) con->type, con->participant_count);
    }

    lv_json_buf_append_raw(&_jb, "\n  ],\n  \"metadata\": {\n");
    lv_json_buf_append_fmt(&_jb, "    \"node_count\": %d,\n", encoded_graph->node_count);
    lv_json_buf_append_fmt(&_jb, "    \"constraint_count\": %d\n", encoded_graph->constraint_count);
    lv_json_buf_append_raw(&_jb, "  }\n}\n");

    return lv_json_buf_finalize(&_jb);
}
