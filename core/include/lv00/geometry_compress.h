#ifndef LV00_GEOMETRY_COMPRESS_H
#define LV00_GEOMETRY_COMPRESS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "constraint_graph.h"

/* ── LVZD magic / version ── */
#define LVZD_MAGIC            0x4C565A44
#define LVZD_VERSION_MAJOR    1
#define LVZD_VERSION_MINOR    0
#define LVZD_HEADER_SIZE      16

/* ── Prediction mode ── */
typedef enum {
    PREDICT_NONE        = -1,
    PREDICT_PARALLELOGRAM = 0,
    PREDICT_MULTI_PARALLELOGRAM,
    PREDICT_DELTA
} PredictionMode;

/* ── Entropy coding mode ── */
typedef enum {
    ENTROPY_NONE    = 0,
    ENTROPY_RANS,
    ENTROPY_HUFFMAN,
    ENTROPY_ARITHMETIC
} EntropyMode;

/* ── Edgebreaker mode ── */
typedef enum {
    EDGEBREAKER_C = 0,
    EDGEBREAKER_L,
    EDGEBREAKER_E,
    EDGEBREAKER_R,
    EDGEBREAKER_S
} EdgebreakerMode;

/* ── Compress config ── */
typedef struct {
    PredictionMode  pred_mode;
    EntropyMode     entropy;
    int             quantization_bits;
    bool            lossless;
    double          max_error;
} CompressConfig;

/* ── Compress metadata ── */
typedef struct {
    int             original_vertex_count;
    int             original_face_count;
    int             compressed_byte_count;
    int             original_size;
    int             compressed_size;
    int             node_count;
    int             constraint_count;
    EdgebreakerMode *edgebreaker_sequence;
    int             sequence_len;
    double          compression_ratio;
    PredictionMode  pred_mode;
    EntropyMode     entropy;
    bool            lossless;
} CompressMetadata;

/* ── API ── */
int geometry_compress(const ConstraintGraph *graph, const CompressConfig *config,
                        uint8_t **out_data, size_t *out_size,
                        CompressMetadata *out_meta);
int geometry_decompress(const uint8_t *data, size_t size,
                          ConstraintGraph **out_graph);
int predictive_encode_coords(ConstraintGraph *graph, PredictionMode mode);
int edgebreaker_encode(const ConstraintGraph *graph, EdgebreakerMode **modes, int *seq_len);

#ifdef __cplusplus
}
#endif
#endif
