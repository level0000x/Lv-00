/**
 * @file geometry_compress.h
 * @brief Draco 风格几何数据压缩 —— Edgebreaker 拓扑编码与预测编码
 *
 * @details 实现借鉴 Google Draco 的几何压缩管线，将 Lv-00 约束图中
 *          的几何节点坐标和拓扑关系进行高压缩比编码。支持 Edgebreaker
 *          CLERS 五模式拓扑压缩、平行四边形/多阶平行四边形/差分预测
 *          编码、以及 RANS/算术/Huffman 三种熵编码后端。
 *
 *          Edgebreaker 算法（J. Rossignac, 1999）将三角网格的边遍历
 *          过程编码为 C（C 形）、L（左）、E（边结束）、R（右）、S（分割）
 *          五种符号的序列，重建时通过 CLERS 解码器恢复完整拓扑。
 *
 *          预测编码采用平行四边形法则：假设三角形三个顶点为 v0,v1,v2，
 *          当遍历到共享边 (v0,v1) 的对顶点 v2 时，预测值 = v0 + v1 - v_opposite，
 *          仅编码残差以降低熵。
 *
 *          压缩输出统一为 .lvzd 格式（二进制），魔数 "LVZD" 后跟
 *          版本号和压缩数据块。
 *
 *          设计借鉴：
 *          - Google Draco (github.com/google/draco) — Edgebreaker + 预测编码 + 熵编码
 *          - MPEG V-PCC / G-PCC — 点云几何压缩标准
 *          - Zstandard / LZ4 — 通用压缩后端可选集成
 *
 * @version v3.3.0
 * @date 2026-05-24
 */

#ifndef LV00_GEOMETRY_COMPRESS_H
#define LV00_GEOMETRY_COMPRESS_H

#include "lv00.h"
#include "constraint_graph.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Edgebreaker 模式枚举
 * ======================================================================== */

/**
 * @brief Edgebreaker CLERS 五模式
 *
 * 描述三角网格边遍历时的五种拓扑模式：
 * - C：新顶点，当前边闭合一个三角形，对顶点为未访问顶点
 * - L：左边界的顶点是已访问顶点，且在当前边界上
 * - E：对顶点已在边界上，边结束（不引入新三角形）
 * - R：右边界的顶点是已访问顶点，且在当前边界上
 * - S：对顶点是已访问顶点，但不在当前边界上（分割操作）
 */
typedef enum {
    EDGEBREAKER_C = 0,  /**< C 模式 —— 新顶点闭合三角形 */
    EDGEBREAKER_L = 1,  /**< L 模式 —— 左边界顶点 */
    EDGEBREAKER_E = 2,  /**< E 模式 —— 边结束，不生成新三角形 */
    EDGEBREAKER_R = 3,  /**< R 模式 —— 右边界顶点 */
    EDGEBREAKER_S = 4   /**< S 模式 —— 分割操作 */
} EdgebreakerMode;

/* ========================================================================
 * 预测模式枚举
 * ======================================================================== */

/**
 * @brief 坐标预测模式
 *
 * 几何压缩中用于减少坐标数据熵的预测策略。
 * 借鉴 Draco 的预测编码器分类。
 */
typedef enum {
    PREDICT_NONE                = 0,  /**< 无预测 —— 直接存储坐标原始值 */
    PREDICT_PARALLELOGRAM       = 1,  /**< 平行四边形预测 —— 基于单三角形对顶点推断 */
    PREDICT_MULTI_PARALLELOGRAM = 2,  /**< 多阶平行四边形预测 —— 利用多个邻面对顶点加权平均 */
    PREDICT_DELTA               = 3   /**< 差分预测 —— 按遍历顺序存储相邻节点坐标差 */
} PredictionMode;

/* ========================================================================
 * 熵编码类型
 * ======================================================================== */

/**
 * @brief 熵编码器类型
 *
 * 对 Edgebreaker 符号序列和坐标残差进行熵编码的后端。
 * - RANS：非对称数字系统，高吞吐量，接近算术编码压缩率
 * - 算术编码：理论最优，但编解码速度较慢
 * - Huffman：经典前缀编码，速度最快但压缩率最低
 */
typedef enum {
    ENTROPY_RANS       = 0,  /**< rANS 熵编码 —— 非对称数字系统 */
    ENTROPY_ARITHMETIC = 1,  /**< 算术编码 —— 区间编码 */
    ENTROPY_HUFFMAN    = 2   /**< Huffman 编码 —— 前缀树编码 */
} EntropyCoding;

/* ========================================================================
 * 压缩配置与元数据
 * ======================================================================== */

/**
 * @brief 几何压缩配置
 *
 * 控制压缩管线的各项参数。
 */
typedef struct {
    PredictionMode pred_mode;   /**< 坐标预测模式 */
    EntropyCoding  entropy;     /**< 熵编码器选择 */
    int   quantization_bits;    /**< 坐标量化位数（0 = 无损，否则为定点数位宽） */
    bool  lossless;             /**< 是否无损压缩 */
    double max_error;           /**< 有损压缩时的最大允许误差（lossless=false 时生效） */
} CompressConfig;

/**
 * @brief 几何压缩元数据
 *
 * 记录压缩前后的统计信息和 Edgebreaker 编码序列。
 * 压缩比 = original_size / compressed_size。
 */
typedef struct {
    uint64_t original_size;       /**< 压缩前数据大小（字节） */
    uint64_t compressed_size;     /**< 压缩后数据大小（字节） */
    double   compression_ratio;   /**< 压缩比（>= 1.0，越大压缩效果越好） */
    int      node_count;          /**< 压缩涉及的节点数量 */
    int      constraint_count;    /**< 压缩涉及的约束数量 */
    EdgebreakerMode *edgebreaker_sequence; /**< Edgebreaker CLERS 编码序列 */
    int      sequence_len;        /**< CLERS 序列长度 */
} CompressMetadata;

/* ========================================================================
 * 压缩主 API
 * ======================================================================== */

/**
 * @brief 压缩约束图的几何数据
 *
 * 完整压缩管线：
 *   1. 遍历图中所有几何节点提取坐标（点坐标 / 线段端点坐标）
 *   2. 对坐标应用预测编码（平行四边形 / 多阶 / 差分 / 无预测）
 *   3. 提取约束图拓扑，运行 Edgebreaker 编码生成 CLERS 序列
 *   4. 对预测残差和 CLERS 序列进行熵编码（rANS / 算术 / Huffman）
 *   5. 封装为二进制 buffer 输出
 *
 * @param[in]  graph     输入约束图（非 NULL）
 * @param[in]  config    压缩配置（非 NULL）
 * @param[out] out_data  输出的压缩数据 buffer（调用者负责 free）
 * @param[out] out_size  输出 buffer 大小
 * @param[out] out_meta  压缩元数据（可为 NULL 表示不关心元数据）
 * @return true 成功，false 失败
 */
bool geometry_compress(const ConstraintGraph *graph,
                       const CompressConfig *config,
                       uint8_t **out_data,
                       size_t *out_size,
                       CompressMetadata *out_meta);

/**
 * @brief 解压约束图的几何数据
 *
 * 逆压缩管线：熵解码 → Edgebreaker 解码重建拓扑 → 预测逆运算恢复坐标
 *
 * @param[in]  data      压缩数据 buffer
 * @param[in]  size      buffer 大小
 * @param[out] out_graph 重建的约束图（调用者负责 graph_destroy）
 * @return true 成功，false 失败
 */
bool geometry_decompress(const uint8_t *data,
                         size_t size,
                         ConstraintGraph **out_graph);

/* ========================================================================
 * Edgebreaker 编码器
 * ======================================================================== */

/**
 * @brief 对约束图拓扑进行 Edgebreaker CLERS 编码
 *
 * 遍历约束图的拓扑结构（将三角形约束视为三角面），
 * 生成 Edgebreaker 五模式符号序列。
 *
 * 算法概要：
 *   1. 以初始边界边为种子，维护边界栈
 *   2. 对每条当前边，查找其对顶点（通过约束关系）
 *   3. 根据对顶点状态分类为 C/L/E/R/S：
 *      - 未访问 → C
 *      - 在边界上，位于当前边左侧 → L
 *      - 在边界上，位于当前边右侧 → R
 *      - 不在边界上 → S
 *      - 边界耗尽 → E
 *   4. 更新边界栈，继续遍历
 *
 * @param[in]  graph   约束图
 * @param[out] modes   输出的 Edgebreaker 模式序列（调用者负责 free）
 * @param[out] seq_len 序列长度
 * @return true 成功，false 失败
 */
bool edgebreaker_encode(const ConstraintGraph *graph,
                        EdgebreakerMode **modes,
                        int *seq_len);

/* ========================================================================
 * 预测编码器
 * ======================================================================== */

/**
 * @brief 对几何节点坐标应用预测编码
 *
 * 根据指定预测模式，遍历约束图中的三角形面，
 * 用已知顶点预测对顶点坐标，仅存储预测残差。
 *
 * 平行四边形预测（PREDICT_PARALLELOGRAM）：
 *   给定三角形面 (v0, v1, v2)，当 v2 为待编码顶点时，
 *   利用已编码的邻接三角形对顶点 v_opp 计算：
 *     prediction = v0 + v1 - v_opp
 *     residual   = v2 - prediction
 *
 * 多阶平行四边形（PREDICT_MULTI_PARALLELOGRAM）：
 *   加权平均多个邻接三角形的预测值。
 *
 * 差分预测（PREDICT_DELTA）：
 *   按遍历顺序存储相邻节点的坐标差。
 *
 * @param[in,out] graph     约束图（节点的坐标值原地更新为残差）
 * @param[in]     mode      预测模式
 * @return true 成功，false 失败
 */
bool predictive_encode_coords(ConstraintGraph *graph,
                              PredictionMode mode);

/* ========================================================================
 * .lvzd 格式 I/O
 * ======================================================================== */

/** .lvzd 文件魔数 */
#define LVZD_MAGIC 0x445A564C  /**< "LVZD" 小端序：0x4C 0x56 0x5A 0x44 */

/** .lvzd 格式版本号 */
#define LVZD_VERSION_MAJOR 1
#define LVZD_VERSION_MINOR 0

/** .lvzd 文件头大小（字节）：魔数(4) + 主版本(2) + 次版本(2) + 原始大小(8) + 压缩大小(8) = 24 */
#define LVZD_HEADER_SIZE 24

/**
 * @brief 将压缩数据写入 .lvzd 文件
 *
 * 写入格式：
 *   [magic:4B] [version_major:2B] [version_minor:2B]
 *   [original_size:8B] [compressed_size:8B]
 *   [compressed_data:compressed_size bytes]
 *
 * 所有多字节整数使用小端序。
 *
 * @param[in] data     压缩数据 buffer
 * @param[in] size     buffer 大小
 * @param[in] filename 输出文件路径
 * @return true 成功，false 失败
 */
bool compress_write_lvzd(const uint8_t *data,
                         size_t size,
                         const char *filename);

/**
 * @brief 从 .lvzd 文件中读取压缩数据
 *
 * 解析文件头，验证魔数和版本，读取压缩数据块。
 *
 * @param[in]  filename 输入文件路径
 * @param[out] out_data 读取的压缩数据 buffer（调用者负责 free）
 * @param[out] out_size 读取的数据大小
 * @return true 成功，false 失败
 */
bool compress_read_lvzd(const char *filename,
                        uint8_t **out_data,
                        size_t *out_size);

#ifdef __cplusplus
}
#endif

#endif /* LV00_GEOMETRY_COMPRESS_H */
