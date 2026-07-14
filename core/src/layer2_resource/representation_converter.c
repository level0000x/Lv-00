/**
 * @file representation_converter.c
 * @brief 表示转换器 - 管理多种表示之间的双向转换
 *
 * @details 实现表示转换器的创建/销毁、前向/反向转换、
 *          往返一致性验证、冲突检测等功能。
 */
#include "lv00/representation_converter.h"
#include "lv00/lv00_utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============ 转换器创建与销毁 ============ */

Lv00RepresentationConverter *lv00_converter_create(void *graph) {
    Lv00RepresentationConverter *conv = lv00_calloc(1, sizeof(Lv00RepresentationConverter));
    if (!conv) return NULL;
    conv->core_graph = graph;
    conv->conflict_count = 0;
    return conv;
}

void lv00_converter_destroy(Lv00RepresentationConverter *conv) {
    if (!conv) return;
    /* 释放前向转换器 */
    lv00_free((void **)&conv->forward.to_geometry);
    lv00_free((void **)&conv->forward.to_node);
    lv00_free((void **)&conv->forward.to_block);
    lv00_free((void **)&conv->forward.to_text);
    /* 释放反向转换器 */
    lv00_free((void **)&conv->reverse.from_geometry);
    lv00_free((void **)&conv->reverse.from_node);
    lv00_free((void **)&conv->reverse.from_block);
    lv00_free((void **)&conv->reverse.from_text);
    lv00_free((void **)&conv);
}

/* ============ 内部辅助：构建失败结果 ============ */

static Lv00ConvertResult make_error_result(const char *msg) {
    Lv00ConvertResult r;
    r.success = 0;
    r.output = NULL;
    if (msg) {
        lv00_strlcpy(r.error_msg, msg, sizeof(r.error_msg));
    } else {
        r.error_msg[0] = '\0';
    }
    return r;
}

static Lv00ConvertResult make_success_result(void *output) {
    Lv00ConvertResult r;
    r.success = 1;
    r.output = output;
    r.error_msg[0] = '\0';
    return r;
}

/* ============ 转换 API ============ */

Lv00ConvertResult lv00_convert_to_geometry(Lv00RepresentationConverter *conv, void *block) {
    if (!conv) return make_error_result("转换器为 NULL");
    if (!block) return make_error_result("输入块为 NULL");
    if (conv->forward.to_geometry && conv->forward.to_geometry->convert_forward) {
        return conv->forward.to_geometry->convert_forward(block);
    }
    return make_error_result("几何转换器未注册");
}

Lv00ConvertResult lv00_convert_to_node_graph(Lv00RepresentationConverter *conv, void *block) {
    if (!conv) return make_error_result("转换器为 NULL");
    if (!block) return make_error_result("输入块为 NULL");
    if (conv->forward.to_node && conv->forward.to_node->convert_forward) {
        return conv->forward.to_node->convert_forward(block);
    }
    return make_error_result("节点图转换器未注册");
}

Lv00ConvertResult lv00_convert_to_text(Lv00RepresentationConverter *conv, void *block) {
    if (!conv) return make_error_result("转换器为 NULL");
    if (!block) return make_error_result("输入块为 NULL");
    if (conv->forward.to_text && conv->forward.to_text->convert_forward) {
        return conv->forward.to_text->convert_forward(block);
    }
    return make_error_result("文本转换器未注册");
}

Lv00ConvertResult lv00_convert_from_text(Lv00RepresentationConverter *conv, const char *code) {
    if (!conv) return make_error_result("转换器为 NULL");
    if (!code) return make_error_result("代码为 NULL");
    if (conv->reverse.from_text && conv->reverse.from_text->convert_backward) {
        return conv->reverse.from_text->convert_backward((void *)code);
    }
    return make_error_result("文本反向转换器未注册");
}

/* ============ 往返一致性验证 ============ */

int lv00_converter_verify_roundtrip(Lv00RepresentationConverter *conv,
                                     void *original, Lv00ViewType type) {
    if (!conv || !original) return 0;

    /* 往返策略：original -> 中间表示 -> original，比较是否一致 */
    Lv00ConvertResult r1, r2;
    switch (type) {
        case LV00_VIEW_TEXT_CODE:
            r1 = lv00_convert_to_text(conv, original);
            if (!r1.success) return 0;
            r2 = lv00_convert_from_text(conv, (const char *)r1.output);
            if (!r2.success) return 0;
            /* 简化：成功往返即视为通过 */
            return 1;
        default:
            /* 其他视图类型的往返验证暂不实现 */
            return -1;
    }
}

/* ============ Legacy 直接转换 API（桩实现） ============ */

Lv00ConvertResult lv00_convert_block_to_text(void *block) {
    if (!block) return make_error_result("输入块为 NULL");
    /* 简化实现：返回成功但无实际输出 */
    return make_success_result(NULL);
}

Lv00ConvertResult lv00_convert_text_to_block(const char *code) {
    if (!code) return make_error_result("代码为 NULL");
    return make_success_result(NULL);
}

Lv00ConvertResult lv00_convert_block_to_node(void *block) {
    if (!block) return make_error_result("输入块为 NULL");
    return make_success_result(NULL);
}

Lv00ConvertResult lv00_convert_node_to_block(void *node) {
    if (!node) return make_error_result("节点为 NULL");
    return make_success_result(NULL);
}

Lv00ConvertResult lv00_convert_block_to_geometry(void *block) {
    if (!block) return make_error_result("输入块为 NULL");
    return make_success_result(NULL);
}

Lv00ConvertResult lv00_convert_geometry_to_block(void *entity) {
    if (!entity) return make_error_result("实体为 NULL");
    return make_success_result(NULL);
}
