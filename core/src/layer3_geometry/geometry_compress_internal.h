/**
 * @file geometry_compress_internal.h
 * @brief Internal shared definitions for geometry compression engine.
 */

#ifndef lv_GEOMETRY_COMPRESS_INTERNAL_H
#define lv_GEOMETRY_COMPRESS_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "geometry_compress.h"

/* ---- constants ---- */
#define BOUNDARY_STACK_INITIAL 64
#define CLERS_SEQUENCE_INITIAL 256
#define LVZD_READ_BUFFER_INITIAL 4096

#ifndef COORD_DIM
#define COORD_DIM 2
#endif

#define LVZD_COMPRESS_MAGIC 0x4C564300 /* "LVZC" */
#define MAX_ADJACENT_FACES 16
#define HUFFMAN_MAX_NODES 511
#define HUFFMAN_MAX_CODE_LEN 256

/* ---- internal structures ---- */
typedef struct {
    int v0;
    int v1;
} Edge;

typedef struct {
    int verts[3];
    bool visited;
} TriangleFace;

typedef struct {
    int left;
    int right;
    int parent;
    uint32_t freq;
    uint8_t byte_val;
} HuffmanNode;

typedef struct {
    int node_index;
    uint32_t freq;
} HuffHeapElem;

typedef struct {
    uint32_t code;
    int length;
} HuffmanCode;

typedef struct {
    uint8_t *buf;
    size_t capacity;
    size_t byte_pos;
    int bit_pos;
} BitWriter;

typedef struct {
    const uint8_t *buf;
    size_t size;
    size_t byte_pos;
    int bit_pos;
} BitReader;

/* ---- cross-file helpers ---- */
bool extract_triangle_faces(const ConstraintGraph *graph, TriangleFace **faces, int *face_count);
int find_adjacent_face(const TriangleFace *faces, int face_count, int v0, int v1, int exclude);
int find_all_adjacent_faces(const TriangleFace *faces, int face_count, int v0, int v1, int *adj_indices, int max_count);
int get_opposite_vertex(const TriangleFace *face, int v0, int v1);
double triangle_face_area(const ConstraintGraph *graph, const TriangleFace *face);

bool predictive_encode_parallelogram(ConstraintGraph *graph);
bool predictive_encode_multi_parallelogram(ConstraintGraph *graph);
bool predictive_encode_delta(ConstraintGraph *graph);

bool rle_encode(const uint8_t *data, size_t size, uint8_t **out_data, size_t *out_size);
bool rle_decode(const uint8_t *data, size_t size, uint8_t **out_data, size_t *out_size);

bool entropy_encode_huffman(const uint8_t *raw_data, size_t raw_size, uint8_t **out_data, size_t *out_size);
bool entropy_decode_huffman(const uint8_t *data, size_t size, uint8_t **out_data, size_t *out_size);

bool bitwriter_write_bit(BitWriter *bw, int bit);
bool bitwriter_write_bits(BitWriter *bw, uint32_t code, int bit_count);
size_t bitwriter_flush(const BitWriter *bw);
/* 初始化位写入器：统一初始化内部字段（禁止调用方手工赋值内部结构） */
void bitwriter_init(BitWriter *bw, uint8_t *buf, size_t capacity);
void bitreader_init(BitReader *br, const uint8_t *buf, size_t size);
int bitreader_read_bit(BitReader *br);

bool entropy_encode_real(const uint8_t *raw_data, size_t raw_size, uint8_t **out_data, size_t *out_size);
bool entropy_decode_real(const uint8_t *data, size_t size, uint8_t **out_data, size_t *out_size);

/* ---- cross-section helpers (defined in geometry_compress.c) ---- */
int huff_heap_compare(const void *a, const void *b);
CompressConfig compress_config_default(void);
int huffman_tree_build(const uint32_t freq[256], HuffmanNode hnodes[HUFFMAN_MAX_NODES]);
void huffman_generate_codes(const HuffmanNode *hnodes, int root, HuffmanCode *codes);

#ifdef __cplusplus
}
#endif

#endif /* lv_GEOMETRY_COMPRESS_INTERNAL_H */
