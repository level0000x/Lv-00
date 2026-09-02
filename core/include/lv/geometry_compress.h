#ifndef lv_GEOMETRY_COMPRESS_H
#define lv_GEOMETRY_COMPRESS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "constraint_graph.h"

/* ── LVZD magic / version ── */
#define LVZD_MAGIC 0x4C565A44
#define LVZD_VERSION_MAJOR 1
#define LVZD_VERSION_MINOR 0
/* K39 修复：头部 28 字节 = magic(4) + major(4) + minor(4) + comp_size(8) + orig_size(8)。
 * 原值 16 致 geometry_compress_io.c 写 header+20 / 读 header+20 越界 12 字节。 */
#define LVZD_HEADER_SIZE 28

/* K39 编译期对拍：头部布局与 LVZD_HEADER_SIZE 一致（防再改回越界） */
lv_STATIC_ASSERT(LVZD_HEADER_SIZE == 4 + 4 + 4 + 8 + 8, "LVZD header size mismatch");

/* ── Prediction mode ── */
typedef enum {
    PREDICT_NONE = -1,
    PREDICT_PARALLELOGRAM = 0,
    PREDICT_MULTI_PARALLELOGRAM,
    PREDICT_DELTA
} PredictionMode;

/* ── Entropy coding mode ── */
typedef enum { ENTROPY_NONE = 0, ENTROPY_RANS, ENTROPY_HUFFMAN, ENTROPY_ARITHMETIC } EntropyMode;

/* ── Edgebreaker mode ── */
typedef enum { EDGEBREAKER_C = 0, EDGEBREAKER_L, EDGEBREAKER_E, EDGEBREAKER_R, EDGEBREAKER_S } EdgebreakerMode;

/* ── Compress config ── */
typedef struct {
    PredictionMode pred_mode;
    EntropyMode entropy;
    int quantization_bits;
    bool lossless;
    double max_error;
} CompressConfig;

/* ── Compress metadata ── */
typedef struct {
    int original_vertex_count;
    int original_face_count;
    int compressed_byte_count;
    int original_size;
    int compressed_size;
    int node_count;
    int constraint_count;
    EdgebreakerMode *edgebreaker_sequence;
    int sequence_len;
    double compression_ratio;
    PredictionMode pred_mode;
    EntropyMode entropy;
    bool lossless;
} CompressMetadata;

/* ── API ── */
bool geometry_compress(const ConstraintGraph *graph, const CompressConfig *config, uint8_t **out_data, size_t *out_size,
                       CompressMetadata *out_meta);
lv_PUBLIC_API bool geometry_decompress(const uint8_t *data, size_t size, ConstraintGraph **out_graph);
lv_PUBLIC_API bool predictive_encode_coords(ConstraintGraph *graph, PredictionMode mode);
lv_PUBLIC_API bool edgebreaker_encode(const ConstraintGraph *graph, EdgebreakerMode **modes, int *seq_len);

/* ── LVZD 容器文件 I/O（K20：补头声明，原实现无头声明） ── */
lv_PUBLIC_API bool compress_write_lvzd(const uint8_t *data, size_t size, const char *filename);
lv_PUBLIC_API bool compress_read_lvzd(const char *filename, uint8_t **out_data, size_t *out_size);

#ifdef __cplusplus
}
#endif
#endif
