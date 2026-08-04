#include <string.h>

#include "lv/lv_internal.h"
#include "lv/lv_utils.h"
#include "lv/representation_converter.h"
#include "lv/visual_editor.h"

/* 双向同步协议 */
/* 确保所有4个视图保持语义等价 */

typedef struct lvSyncProtocol {
    int enabled;
    void *core_graph;
    int conflict_count;
    char conflicts[16][512];
} lvSyncProtocol;

lvSyncProtocol *lv_sync_protocol_create(void *graph) {
    lvSyncProtocol *proto = lv_calloc(1, sizeof(lvSyncProtocol));
    if (!proto)
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "failed to allocate sync protocol");
    proto->enabled = 1;
    proto->core_graph = graph;
    return proto;
}

void lv_sync_protocol_destroy(lvSyncProtocol *proto) {
    lv_free((void **) &proto);
}

/* ================================================================
 * 源视图 -> 块视图 转换注册表
 *
 * 使用项目正式 lvViewType 枚举（lv/lv_view.h）替代原私有 SYNC_VIEW_* 宏，
 * 消除两套同义不同值枚举可漂移的隐患（原宏与 lvViewType 语义平行但值不同）。
 * ================================================================ */

/** @brief 视图同步转换函数签名（输入变更数据，输出转换结果） */
typedef lvConvertResult (*lvSyncConvertFn)(void *change);

/** @brief 文本视图转换包装：统一函数签名（文本输入为 const char*，其余为 void*） */
static lvConvertResult sync_convert_text_to_block(void *change) {
    return lv_convert_text_to_block((const char *) change);
}

/** @brief 源视图 -> 块视图 转换注册表
 *
 * 指定初始化器按 lvViewType 枚举对齐，编译器可校验枚举与函数对应关系。
 * 块视图是同步枢纽（直接向其余视图分发），对应项为 NULL。 */
static const lvSyncConvertFn kSyncConvertToBlock[] = {
    [lv_VIEW_BLOCK_CANVAS] = NULL,
    [lv_VIEW_TEXT_CODE] = sync_convert_text_to_block,
    [lv_VIEW_NODE_GRAPH] = lv_convert_node_to_block,
    [lv_VIEW_GEOMETRY_CANVAS] = lv_convert_geometry_to_block,
};

/* 传播变更到所有其他视图 */
/* 根据源视图类型，调用对应的转换器将变更同步到其余视图 */
/* max_depth 防止循环依赖导致无限递归，默认最大深度 32 */
int lv_sync_propagate(lvSyncProtocol *proto, lvViewType source_view, void *change, int max_depth) {
    if (!proto || !proto->enabled)
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "sync protocol disabled or NULL");
    if (max_depth <= 0) {
        /* 达到递归深度上限，可能存在循环依赖 */
        if (proto->conflict_count < 16) {
            snprintf(proto->conflicts[proto->conflict_count], sizeof(proto->conflicts[0]),
                     "sync recursion depth exceeded (circular dependency?)");
            proto->conflict_count++;
        }
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "sync recursion depth exceeded");
    }

    /* 检查同步是否启用 */
    if (!proto->enabled) {
        return 0;
    }

    int prop_count = 0; /* 成功传播计数 */

    /* 源视图即块视图：将变更直接同步到文本、节点图、几何画布 */
    if (source_view == lv_VIEW_BLOCK_CANVAS) {
        lvConvertResult r;

        /* BLOCK → TEXT */
        r = lv_convert_block_to_text(change);
        if (r.success)
            prop_count++;

        /* BLOCK → NODE */
        r = lv_convert_block_to_node(change);
        if (r.success)
            prop_count++;

        /* BLOCK → GEOMETRY */
        r = lv_convert_block_to_geometry(change);
        if (r.success)
            prop_count++;

        return prop_count;
    }

    /* 其余视图：查注册表获取转换为块视图的函数 */
    const lvSyncConvertFn convert = ((unsigned) source_view < lv_ARRAY_SIZE(kSyncConvertToBlock))
                                        ? kSyncConvertToBlock[source_view]
                                        : NULL;
    if (!convert) {
        /* 未知视图类型，记录冲突 */
        if (proto->conflict_count < 16) {
            snprintf(proto->conflicts[proto->conflict_count], sizeof(proto->conflicts[0]),
                     "unknown source view type: %d", (int) source_view);
            proto->conflict_count++;
        }
        return prop_count;
    }

    /* 转换为块视图后，通过块视图继续传播到其他视图 */
    lvConvertResult r = convert(change);
    if (r.success) {
        prop_count++;
        prop_count += lv_sync_propagate(proto, lv_VIEW_BLOCK_CANVAS, r.output, max_depth - 1);
    }

    return prop_count;
}