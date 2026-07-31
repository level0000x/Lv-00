/* ============================================================================
 * 模块名称：几何数据压缩引擎 (geometry_compress)
 *
 * 功能概述：
 *   类 Draco 风格的几何网格数据压缩——基于 Edgebreaker CLERS 算法的
 *   拓扑编码 + 平行四边形预测编码 + 熵编码 + .lvzd 二进制文件 I/O。
 *
 * 核心流水线：
 *   geometry_compress()   压缩流水线：拓扑编码 → 坐标预测 → 熵编码 → 二进制打包
 *   geometry_decompress()  解压流水线：二进制解析 → 熵解码 → 坐标还原 → 拓扑重建
 *
 * 内部模块：
 *   - edgebreaker_encode()           CLERS 符号序列生成（网格拓扑编码）
 *   - predictive_encode_coords()     坐标预测编码（平行四边形 / 多平行四边形 / 增量）
 *   - 熵编码器                         Huffman + RLE 自适应选择
 *   - .lvzd I/O                       二进制文件读写（小端序）
 *
 * 数据结构：
 *   - TriangleFace                    三角面片（从约束图中提取 3-participant 约束）
 *   - HuffmanNode/HuffmanCode  Huffman 编码基础设施
 *   - BitWriter/BitReader             位级 I/O 工具
 *
 * 设计文档参考：Section 3.5 几何内核 · 网格压缩
 *
 * ============================================================================ */

#include "geometry_compress.h"

#include "lv/lv_file.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "lv/constraint_graph.h"
#include "lv/lv_heap.h"


#include "lv_internal.h"
#include "lv_utils.h"
#include "node_deep_copy.h"
#include "symbolic_coord.h"


#include "geometry_compress_internal.h"

int huff_heap_compare(const void *a, const void *b) {
    const HuffHeapElem *ea = (const HuffHeapElem *) a;
    const HuffHeapElem *eb = (const HuffHeapElem *) b;
    return (ea->freq > eb->freq) - (ea->freq < eb->freq);
}

/* ========================================================================
 * Default compression config factory (internal)
 * ======================================================================== */

CompressConfig compress_config_default(void) {
    CompressConfig cfg;
    cfg.pred_mode = PREDICT_PARALLELOGRAM;
    cfg.entropy = ENTROPY_RANS;
    cfg.quantization_bits = 0;
    cfg.lossless = true;
    cfg.max_error = 0.0;
    return cfg;
}

