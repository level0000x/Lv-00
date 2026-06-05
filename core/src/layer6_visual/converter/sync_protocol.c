#include "lv00/representation_converter.h"
#include "lv00/visual_editor.h"
#include <stdlib.h>
#include <string.h>

/* 双向同步协议 */
/* 确保所有4个视图保持语义等价 */

typedef struct Lv00SyncProtocol {
    int enabled;
    void *core_graph;
    int conflict_count;
    char conflicts[16][512];
} Lv00SyncProtocol;

Lv00SyncProtocol *lv00_sync_protocol_create(void *graph) {
    Lv00SyncProtocol *proto = calloc(1, sizeof(Lv00SyncProtocol));
    if (!proto) return NULL;
    proto->enabled = 1;
    proto->core_graph = graph;
    return proto;
}

void lv00_sync_protocol_destroy(Lv00SyncProtocol *proto) {
    free(proto);
}

/* 视图类型常量（与 Lv00ViewType 对应） */
#define SYNC_VIEW_GEOMETRY  0
#define SYNC_VIEW_NODE      1
#define SYNC_VIEW_BLOCK     2
#define SYNC_VIEW_TEXT      3

/* 传播变更到所有其他视图 */
/* 根据源视图类型，调用对应的转换器将变更同步到其余视图 */
/* max_depth 防止循环依赖导致无限递归，默认最大深度 32 */
int lv00_sync_propagate(Lv00SyncProtocol *proto, int source_view, void *change, int max_depth) {
    if (!proto || !proto->enabled) return -1;
    if (max_depth <= 0) {
        /* 达到递归深度上限，可能存在循环依赖 */
        if (proto->conflict_count < 16) {
            snprintf(proto->conflicts[proto->conflict_count],
                     sizeof(proto->conflicts[0]),
                     "sync recursion depth exceeded (circular dependency?)");
            proto->conflict_count++;
        }
        return -1;
    }

    /* 检查同步是否启用 */
    if (!proto->enabled) {
        return 0;
    }

    int prop_count = 0; /* 成功传播计数 */

    /* 根据源视图类型，确定需要执行的转换方向 */
    switch (source_view) {
    case SYNC_VIEW_BLOCK:
        /* 块视图变更 → 同步到文本、节点图、几何画布 */
        {
            Lv00ConvertResult r;

            /* BLOCK → TEXT */
            r = lv00_convert_block_to_text(change);
            if (r.success) prop_count++;

            /* BLOCK → NODE */
            r = lv00_convert_block_to_node(change);
            if (r.success) prop_count++;

            /* BLOCK → GEOMETRY */
            r = lv00_convert_block_to_geometry(change);
            if (r.success) prop_count++;
        }
        break;

    case SYNC_VIEW_TEXT:
        /* 文本视图变更 → 同步到块视图（其余视图通过块视图间接同步） */
        {
            Lv00ConvertResult r = lv00_convert_text_to_block((const char *)change);
            if (r.success) {
                prop_count++;
                /* 通过块视图继续传播到其他视图 */
                prop_count += lv00_sync_propagate(proto, SYNC_VIEW_BLOCK, r.output, max_depth - 1);
            }
        }
        break;

    case SYNC_VIEW_NODE:
        /* 节点图视图变更 → 同步到块视图 */
        {
            Lv00ConvertResult r = lv00_convert_node_to_block(change);
            if (r.success) {
                prop_count++;
                prop_count += lv00_sync_propagate(proto, SYNC_VIEW_BLOCK, r.output, max_depth - 1);
            }
        }
        break;

    case SYNC_VIEW_GEOMETRY:
        /* 几何画布变更 → 同步到块视图 */
        {
            Lv00ConvertResult r = lv00_convert_geometry_to_block(change);
            if (r.success) {
                prop_count++;
                prop_count += lv00_sync_propagate(proto, SYNC_VIEW_BLOCK, r.output, max_depth - 1);
            }
        }
        break;

    default:
        /* 未知视图类型，记录冲突 */
        if (proto->conflict_count < 16) {
            snprintf(proto->conflicts[proto->conflict_count],
                     sizeof(proto->conflicts[0]),
                     "unknown source view type: %d", source_view);
            proto->conflict_count++;
        }
        break;
    }

    return prop_count;
}
