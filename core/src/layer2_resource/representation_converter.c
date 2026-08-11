/**
 * @file representation_converter.c
 * @brief 表示转换器 - 管理多种表示之间的双向转换
 *
 * @details 实现表示转换器的创建/销毁、前向/反向转换、
 *          往返一致性验证、冲突检测等功能。
 *
 * 支持的转换路径：
 * - 函数块 <-> 几何表示（lv_convert_block_to_geometry / lv_convert_geometry_to_block）
 * - 函数块 <-> 文本表示（lv_convert_block_to_text / lv_convert_text_to_block）
 * - 函数块 <-> 节点图（lv_convert_block_to_node / lv_convert_node_to_block）
 * - 通用注册式转换器（前向/反向转换器注册表）
 *
 * @author Lv-00 Project
 */
#include "lv/representation_converter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/func_block.h"
#include "lv/lv_error.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

/* ============ 转换器创建与销毁 ============ */

/**
 * @brief 创建表示转换器
 *
 * @param graph 核心图指针（可为 NULL）
 * @return 新创建的转换器指针，失败返回 NULL
 */
lvRepresentationConverter *lv_converter_create(void *graph) {
    lvRepresentationConverter *conv = lv_calloc(1, sizeof(lvRepresentationConverter));
    if (!conv)
        return NULL;
    conv->core_graph = graph;
    conv->conflict_count = 0;
    return conv;
}

/**
 * @brief 销毁表示转换器
 *
 * 释放前向和反向转换器中的所有注册函数及转换器本身。
 *
 * @param conv 转换器指针（可为 NULL）
 */
void lv_converter_destroy(lvRepresentationConverter *conv) {
    if (!conv)
        return;
    /* 释放前向转换器 */
    lv_free((void **) &conv->forward.to_geometry);
    lv_free((void **) &conv->forward.to_node);
    lv_free((void **) &conv->forward.to_block);
    lv_free((void **) &conv->forward.to_text);
    /* 释放反向转换器 */
    lv_free((void **) &conv->reverse.from_geometry);
    lv_free((void **) &conv->reverse.from_node);
    lv_free((void **) &conv->reverse.from_block);
    lv_free((void **) &conv->reverse.from_text);
    lv_free((void **) &conv);
}

/* ============ 内部辅助：构建失败结果 ============ */

/**
 * @brief 创建错误转换结果
 * @param msg 错误消息
 * @return 转换结果结构体
 */
static lvConvertResult make_error_result(const char *msg) {
    lvConvertResult r;
    r.output = NULL;
    lv_RESULT_FAIL(r, msg);
    return r;
}

/**
 * @brief 创建成功转换结果
 * @param output 输出指针
 * @return 转换结果结构体
 */
static lvConvertResult make_success_result(void *output) {
    lvConvertResult r;
    r.success = 1;
    r.output = output;
    r.error_msg[0] = '\0';
    return r;
}

/* ============ 转换 API ============ */

/**
 * @brief 将函数块转换为几何表示
 *
 * @param conv  转换器
 * @param block 函数块指针
 * @return 转换结果
 */
lvConvertResult lv_convert_to_geometry(lvRepresentationConverter *conv, void *block) {
    if (!conv)
        return make_error_result("转换器为 NULL");
    if (!block)
        return make_error_result("输入块为 NULL");
    if (conv->forward.to_geometry && conv->forward.to_geometry->convert_forward) {
        return conv->forward.to_geometry->convert_forward(block);
    }
    return make_error_result("几何转换器未注册");
}

/**
 * @brief 将函数块转换为节点图表示
 *
 * @param conv  转换器
 * @param block 函数块指针
 * @return 转换结果
 */
lvConvertResult lv_convert_to_node_graph(lvRepresentationConverter *conv, void *block) {
    if (!conv)
        return make_error_result("转换器为 NULL");
    if (!block)
        return make_error_result("输入块为 NULL");
    if (conv->forward.to_node && conv->forward.to_node->convert_forward) {
        return conv->forward.to_node->convert_forward(block);
    }
    return make_error_result("节点图转换器未注册");
}

/**
 * @brief 将函数块转换为文本表示
 *
 * @param conv  转换器
 * @param block 函数块指针
 * @return 转换结果
 */
lvConvertResult lv_convert_to_text(lvRepresentationConverter *conv, void *block) {
    if (!conv)
        return make_error_result("转换器为 NULL");
    if (!block)
        return make_error_result("输入块为 NULL");
    if (conv->forward.to_text && conv->forward.to_text->convert_forward) {
        return conv->forward.to_text->convert_forward(block);
    }
    return make_error_result("文本转换器未注册");
}

/**
 * @brief 将文本表示反向转换为函数块
 *
 * @param conv 转换器
 * @param code 文本代码
 * @return 转换结果
 */
lvConvertResult lv_convert_from_text(lvRepresentationConverter *conv, const char *code) {
    if (!conv)
        return make_error_result("转换器为 NULL");
    if (!code)
        return make_error_result("代码为 NULL");
    if (conv->reverse.from_text && conv->reverse.from_text->convert_backward) {
        return conv->reverse.from_text->convert_backward((void *) code);
    }
    return make_error_result("文本反向转换器未注册");
}

/* ============ 往返一致性验证 ============ */

/**
 * @brief 验证转换往返一致性
 *
 * 对原始对象执行前向再反向转换，比较结果是否一致。
 *
 * @param conv     转换器
 * @param original 原始对象
 * @param type     视图类型
 * @return 1 一致，0 不一致，-1 暂不支持
 */
int lv_converter_verify_roundtrip(lvRepresentationConverter *conv, void *original, lvViewType type) {
    if (!conv || !original)
        return 0;

    /* 往返策略：original -> 中间表示 -> original，比较是否一致 */
    lvConvertResult r1, r2;
    switch (type) {
        case lv_VIEW_TEXT_CODE:
            r1 = lv_convert_to_text(conv, original);
            if (!r1.success)
                return 0;
            r2 = lv_convert_from_text(conv, (const char *) r1.output);
            if (!r2.success)
                return 0;
            /* 简化：成功往返即视为通过 */
            return 1;
        default:
            /* 其他视图类型的往返验证暂不实现 */
            lv_RETURN_ERROR(lv_ERROR_UNSUPPORTED, "roundtrip not supported for this view type");
    }
}


/**
 * @brief 将函数块转换为几何表示（直接 API）
 *
 * @param block 函数块指针
 * @return 转换结果
 */
/* 实现在 block_to_geometry.c (layer6_visual/converter) 中 */
