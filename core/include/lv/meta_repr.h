/**
 * @file meta_repr.h
 * @brief Lv-00 元表示层公共接口
 *
 * @details 元表示层用于将 C 核数据结构编码为几何表示，
 *          实现自举架构中"几何层理解自身"的核心目标。
 *
 *          核心功能：
 *          1. C 结构体 → 几何表示（编码）
 *          2. 几何表示 → C 结构体（解码）
 *          3. 编码-解码往返验证
 *          4. 几何表示同构比较
 *
 * @author Lv-00 Project
 * @version 1.1.0
 * @date 2026-05-29
 */
#ifndef lv_META_REPR_H
#define lv_META_REPR_H
#ifdef __cplusplus
extern "C" {
#endif
#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include <stdbool.h>
#include <stdint.h>
/* ============== 前向声明 ============== */
typedef struct ConstraintGraph ConstraintGraph;
typedef struct GeomNode GeomNode;
typedef struct FuncBlock FuncBlock;
typedef struct TypeRegion TypeRegion;
typedef struct Proposition Proposition;
typedef struct MetaReprEncoder MetaReprEncoder;
typedef struct MetaReprDecoder MetaReprDecoder;
/* ============== 编码配置 ============== */
/**
 * @brief 编码器配置选项
 */
typedef struct MetaReprConfig {
    /* 坐标编码方案 */
    struct {
        double id_spacing;   /**< 节点 ID 间距（默认 10.0） */
        double type_spacing; /**< 类型间距（默认 100.0） */
        double base_x;       /**< 基础 X 坐标（默认 0.0） */
        double base_y;       /**< 基础 Y 坐标（默认 0.0） */
    } coordinate_scheme;

    /* 区域编码方案 */
    struct {
        double padding;      /**< 区域内部边距（默认 50.0） */
        double port_spacing; /**< 端口间距（默认 30.0） */
    } region_scheme;

    /* 元数据编码 */
    bool encode_metadata; /**< 是否编码元数据（count、capacity 等） */
    bool encode_version;  /**< 是否编码版本信息 */

    /* 性能选项 */
    bool use_caching;    /**< 是否启用缓存 */
    uint32_t cache_size; /**< 缓存大小 */
} MetaReprConfig;
/**
 * @brief 获取默认编码配置
 * @return 默认配置
 */
MetaReprConfig meta_repr_default_config(void);
/* ============== 编码器 API ============== */
/**
 * @brief 创建编码器
 * @param config 编码配置（NULL 使用默认配置）
 * @return 编码器实例
 */
MetaReprEncoder *meta_repr_encoder_create(const MetaReprConfig *config);
/**
 * @brief 销毁编码器
 * @param encoder 编码器实例
 */
lv_PUBLIC_API void meta_repr_encoder_destroy(MetaReprEncoder *encoder);
/**
 * @brief 重置编码器状态
 * @param encoder 编码器实例
 */
lv_PUBLIC_API void meta_repr_encoder_reset(MetaReprEncoder *encoder);
/**
 * @brief 编码 ConstraintGraph 为几何表示
 * @param encoder 编码器实例
 * @param graph 约束图
 * @return 编码后的几何表示（新的 ConstraintGraph 实例）
 */
ConstraintGraph *meta_repr_encode_graph(MetaReprEncoder *encoder, const ConstraintGraph *graph);
/**
 * @brief 编码 GeomNode 为几何点
 * @param encoder 编码器实例
 * @param node 几何节点
 * @return 编码后的点（新的 GeomNode 实例）
 */
GeomNode *meta_repr_encode_node(MetaReprEncoder *encoder, const GeomNode *node);
/**
 * @brief 编码 FuncBlock 为几何区域
 * @param encoder 编码器实例
 * @param block 函数块
 * @return 编码后的区域（新的 GeomNode 实例，类型为 REGION）
 */
GeomNode *meta_repr_encode_func_block(MetaReprEncoder *encoder, const FuncBlock *block);
/**
 * @brief 编码 TypeRegion 为几何区域
 * @param encoder 编码器实例
 * @param type_region 类型区域
 * @return 编码后的区域（新的 GeomNode 实例）
 */
GeomNode *meta_repr_encode_type_region(MetaReprEncoder *encoder, const TypeRegion *type_region);
/**
 * @brief 编码 Proposition 为几何节点
 * @param encoder 编码器实例
 * @param proposition 命题
 * @return 编码后的节点（新的 GeomNode 实例）
 */
GeomNode *meta_repr_encode_proposition(MetaReprEncoder *encoder, const Proposition *proposition);
/* ============== 解码器 API ============== */
/**
 * @brief 创建解码器
 * @return 解码器实例
 */
MetaReprDecoder *meta_repr_decoder_create(void);
/**
 * @brief 销毁解码器
 * @param decoder 解码器实例
 */
lv_PUBLIC_API void meta_repr_decoder_destroy(MetaReprDecoder *decoder);
/**
 * @brief 从几何表示解码为 ConstraintGraph
 * @param decoder 解码器实例
 * @param encoded_graph 编码后的几何表示
 * @return 解码后的约束图
 */
ConstraintGraph *meta_repr_decode_graph(MetaReprDecoder *decoder, const ConstraintGraph *encoded_graph);
/**
 * @brief 从几何点解码为 GeomNode
 * @param decoder 解码器实例
 * @param encoded_node 编码后的点
 * @return 解码后的节点
 */
GeomNode *meta_repr_decode_node(MetaReprDecoder *decoder, const GeomNode *encoded_node);
/**
 * @brief 从几何区域解码为 FuncBlock
 * @param decoder 解码器实例
 * @param encoded_block 编码后的区域
 * @return 解码后的函数块
 */
FuncBlock *meta_repr_decode_func_block(MetaReprDecoder *decoder, const GeomNode *encoded_block);
/* ============== 验证 API ============== */
/**
 * @brief 验证编码-解码往返正确性
 * @param original 原始结构体
 * @param decoded 解码后的结构体
 * @param type_name 结构体类型名称（用于日志）
 * @return 是否等价
 */
lv_PUBLIC_API bool meta_repr_verify_roundtrip(const void *original, const void *decoded, const char *type_name);
/**
 * @brief 比较两个约束图是否语义等价
 * @param a 约束图 A
 * @param b 约束图 B
 * @return 是否等价
 */
lv_PUBLIC_API bool meta_repr_graph_equivalent(const ConstraintGraph *a, const ConstraintGraph *b);
/**
 * @brief 比较两个几何表示是否同构
 * @param a 几何表示 A
 * @param b 几何表示 B
 * @return 是否同构等价
 */
lv_PUBLIC_API bool meta_repr_isomorphic(const ConstraintGraph *a, const ConstraintGraph *b);
/* ============== 工具 API ============== */
/**
 * @brief 获取编码统计信息
 * @param encoder 编码器实例
 * @param out_node_count 输出节点数量
 * @param out_constraint_count 输出约束数量
 */
lv_PUBLIC_API void meta_repr_get_stats(MetaReprEncoder *encoder, int *out_node_count, int *out_constraint_count);
/**
 * @brief 导出几何表示为 DOT 格式（用于可视化）
 * @param encoded_graph 编码后的几何表示
 * @param filepath 输出文件路径
 * @return 是否成功
 */
lv_PUBLIC_API bool meta_repr_export_dot(const ConstraintGraph *encoded_graph, const char *filepath);
/**
 * @brief 导出几何表示为 JSON 格式
 * @param encoded_graph 编码后的几何表示
 * @return JSON 字符串（需由调用者释放）
 */
lv_PUBLIC_API char *meta_repr_export_json(const ConstraintGraph *encoded_graph);
#ifdef __cplusplus
}
#endif
#endif /* lv_META_REPR_H */
