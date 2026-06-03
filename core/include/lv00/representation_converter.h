#ifndef LV00_REPRESENTATION_CONVERTER_H
#define LV00_REPRESENTATION_CONVERTER_H

#include "lv00/visual_editor.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Conversion result */
typedef struct Lv00ConvertResult {
    int success;
    void *output;
    char error_msg[512];
} Lv00ConvertResult;

/* Converter interface */
typedef struct Lv00Converter {
    int source_type;
    int target_type;
    Lv00ConvertResult (*convert_forward)(void *input);
    Lv00ConvertResult (*convert_backward)(void *input);
} Lv00Converter;

/* Representation converter (manages all converters) */
typedef struct Lv00RepresentationConverter {
    void *core_graph;

    /* Forward converters */
    struct {
        Lv00Converter *to_geometry;
        Lv00Converter *to_node;
        Lv00Converter *to_block;
        Lv00Converter *to_text;
    } forward;

    /* Reverse converters */
    struct {
        Lv00Converter *from_geometry;
        Lv00Converter *from_node;
        Lv00Converter *from_block;
        Lv00Converter *from_text;
    } reverse;

    /* Conflict detection */
    int conflict_count;
} Lv00RepresentationConverter;

/* Lifecycle */
Lv00RepresentationConverter *lv00_converter_create(void *graph);
void lv00_converter_destroy(Lv00RepresentationConverter *conv);

/* Conversion API */
Lv00ConvertResult lv00_convert_to_geometry(Lv00RepresentationConverter *conv, void *block);
Lv00ConvertResult lv00_convert_to_node_graph(Lv00RepresentationConverter *conv, void *block);
Lv00ConvertResult lv00_convert_to_text(Lv00RepresentationConverter *conv, void *block);
Lv00ConvertResult lv00_convert_from_text(Lv00RepresentationConverter *conv, const char *code);

/* Roundtrip verification */
int lv00_converter_verify_roundtrip(Lv00RepresentationConverter *conv,
                                     void *original, Lv00ViewType type);

#ifdef __cplusplus
}
#endif

#endif /* LV00_REPRESENTATION_CONVERTER_H */
