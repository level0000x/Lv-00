#ifndef lv_REPRESENTATION_CONVERTER_H
#define lv_REPRESENTATION_CONVERTER_H

#include "lv/visual_editor.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Conversion result */
typedef struct lvConvertResult {
    int success;
    void *output;
    char error_msg[512];
} lvConvertResult;

/* Converter interface */
typedef struct lvConverter {
    int source_type;
    int target_type;
    lvConvertResult (*convert_forward)(void *input);
    lvConvertResult (*convert_backward)(void *input);
} lvConverter;

/* Representation converter (manages all converters) */
typedef struct lvRepresentationConverter {
    void *core_graph;

    /* Forward converters */
    struct {
        lvConverter *to_geometry;
        lvConverter *to_node;
        lvConverter *to_block;
        lvConverter *to_text;
    } forward;

    /* Reverse converters */
    struct {
        lvConverter *from_geometry;
        lvConverter *from_node;
        lvConverter *from_block;
        lvConverter *from_text;
    } reverse;

    /* Conflict detection */
    int conflict_count;
} lvRepresentationConverter;

/* Lifecycle */
lvRepresentationConverter *lv_converter_create(void *graph);
void lv_converter_destroy(lvRepresentationConverter *conv);

/* Conversion API */
lvConvertResult lv_convert_to_geometry(lvRepresentationConverter *conv, void *block);
lvConvertResult lv_convert_to_node_graph(lvRepresentationConverter *conv, void *block);
lvConvertResult lv_convert_to_text(lvRepresentationConverter *conv, void *block);
lvConvertResult lv_convert_from_text(lvRepresentationConverter *conv, const char *code);

/* Roundtrip verification */
int lv_converter_verify_roundtrip(lvRepresentationConverter *conv, void *original, lvViewType type);

/* Direct conversion functions (legacy API) */
lvConvertResult lv_convert_block_to_text(void *block);
lvConvertResult lv_convert_text_to_block(const char *code);
lvConvertResult lv_convert_block_to_node(void *block);
lvConvertResult lv_convert_node_to_block(void *node);
lvConvertResult lv_convert_block_to_geometry(void *block);
lvConvertResult lv_convert_geometry_to_block(void *entity);

/* Destroy geometry encoding (internal type, use void* at API boundary) */
void lv_geometry_encoding_destroy(void *enc);

/* SimpleBlockGraph 失败路径守卫（判据 G：收敛 converter 域三文件逐字同构的
 * simple_block_graph_guard_cleanup 本地 static——销毁已建函数块并释放结构）。
 * 签名取 void* 以兼容 lv_DEFER(cleanup, &sg)；NULL 安全。 */
void lv_simple_block_graph_guard_cleanup(void *p);

#ifdef __cplusplus
}
#endif

#endif /* lv_REPRESENTATION_CONVERTER_H */
