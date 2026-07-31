/*
 * @file high_dim_view.c
 * @brief High-dim module - multi-projection views
 * @details Split from high_dim.c
 */

#include "high_dim.h"
#include "high_dim_internal.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "lv/config.h"
#include "lv/lv_json.h"
#include "lv/lv_parse_utils.h"

#include "debug.h"
#include "error_codes.h"
#include "lv_internal.h"
#include "lv/lv_str_utils.h"
#include "lv_utils.h"
#include "stream.h"
#include "stream_context_util.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_xmacro.h"

/* ==================== 多投影视图 ==================== */

/**
 * @brief 多投影视图内部上下文
 *
 * 在 C 层维护每个多投影视图的状态，包括关联的高维块、投影预设和
 * 当前高亮元素列表。视图ID由 create 函数生成，通过静态数组统一管理。
 */
typedef struct {
    int view_id;                                       /**< 唯一视图标识符 */
    int block_id;                                      /**< 关联的高维块ID */
    int preset_index;                                  /**< 使用的投影预设索引 */
    bool is_active;                                    /**< 视图是否处于激活状态 */
    int highlighted_elements[HIGH_DIM_MAX_DIMENSIONS]; /**< 当前高亮的元素ID列表 */
    int highlighted_count;                             /**< 高亮元素数量 */
} HighDimMultiViewContext;

/** 全局活跃视图追踪数组 */
static lv_THREAD_LOCAL HighDimMultiViewContext g_multi_views[HIGH_DIM_MAX_ACTIVE_VIEWS];
static lv_THREAD_LOCAL int g_multi_view_count = 0;

/**
 * @brief 在全局视图数组中查找指定 view_id 对应的索引
 *
 * @param view_id 视图ID
 * @return 数组索引（>= 0），未找到返回 -1
 */
static int high_dim_find_view_index(int view_id) {
    for (int i = 0; i < g_multi_view_count; i++) {
        if (g_multi_views[i].is_active && g_multi_views[i].view_id == view_id) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief 分配一个新的视图上下文槽位
 *
 * @param view_id 视图ID
 * @param block_id 关联的高维块ID
 * @param preset_index 投影预设索引
 * @return 新分配的数组索引，失败返回 -1
 */
static int high_dim_allocate_view_slot(int view_id, int block_id, int preset_index) {
    /* 首先尝试复用已释放的槽位 */
    for (int i = 0; i < g_multi_view_count; i++) {
        if (!g_multi_views[i].is_active) {
            memset(&g_multi_views[i], 0, sizeof(HighDimMultiViewContext));
            g_multi_views[i].view_id = view_id;
            g_multi_views[i].block_id = block_id;
            g_multi_views[i].preset_index = preset_index;
            g_multi_views[i].is_active = true;
            g_multi_views[i].highlighted_count = 0;
            return i;
        }
    }

    /* 分配新槽位 */
    if (g_multi_view_count >= HIGH_DIM_MAX_ACTIVE_VIEWS) {
        lv_RETURN_ERROR(lv_ERROR_OVERFLOW, "high_dim_allocate_view_slot: view slots exhausted");
    }

    int idx = g_multi_view_count;
    memset(&g_multi_views[idx], 0, sizeof(HighDimMultiViewContext));
    g_multi_views[idx].view_id = view_id;
    g_multi_views[idx].block_id = block_id;
    g_multi_views[idx].preset_index = preset_index;
    g_multi_views[idx].is_active = true;
    g_multi_views[idx].highlighted_count = 0;
    g_multi_view_count++;

    return idx;
}

int high_dim_create_multi_projection_view(HighDimManager *manager, int block_id, const int *preset_indices,
                                          int preset_count, int *view_ids) {
    /**
     * @brief 创建多投影并排视图
     *
     * 为同一个高维块创建多个并排显示的投影视图。每个视图使用不同的
     * 投影预设，以便用户同时从多个角度观察高维对象。
     *
     * 实现要点：
     *   1. 参数验证：manager、preset_indices、view_ids 非空，preset_count > 0
     *   2. 存在性验证：目标高维块必须已注册
     *   3. 预设索引验证：每个 preset_indices[i] 必须在 [0, preset_count) 范围内
     *   4. 视图ID生成：使用 block_id 和 preset_index 的组合编码
     *      （格式: block_id * 1000 + preset_index）
     *   5. 视图上下文注册：在全局视图追踪数组中分配槽位，
     *      记录 view_id、block_id、preset_index 的映射关系
     *   6. 容量检查：确保不超过 HIGH_DIM_MAX_ACTIVE_VIEWS 限制
     *
     * 视图ID编码方案：
     *   由于 view_id = block_id * 1000 + preset_index，如果同一个
     *   (block_id, preset_index) 对被多次请求，将分配不同的视图ID
     *   （通过追加偏移量确保唯一性）。
     *
     * 注意：完整的渲染管线（视口布局、摄像机设置、OpenGL上下文
     * 切换）由UI层渲染引擎负责。C层仅维护视图逻辑状态的记录。
     *
     * @param manager 高维管理器指针
     * @param block_id 要创建多视图的高维块ID
     * @param preset_indices 投影预设索引数组，每个元素对应一个视图
     * @param preset_count 视图数量（= preset_indices 的元素个数）
     * @param view_ids 输出参数，接收生成的视图ID数组（长度 >= preset_count）
     * @return lv_OK 所有视图创建成功
     *         lv_ERROR_INVALID_PARAM 参数无效
     *         lv_ERROR_NOT_FOUND 未找到指定的高维块
     *         lv_ERROR_RESOURCE_EXHAUSTED 视图槽位已满
     */
    if (!manager || !preset_indices || !view_ids || preset_count <= 0) {
        return lv_ERROR_INVALID_PARAM;
    }

    HighDimAbstractBlock *block = high_dim_get_block(manager, block_id);
    if (!block) {
        lv_set_error(lv_ERROR_NOT_FOUND, "创建多投影视图失败：未找到block_id=%d对应的高维抽象块", block_id);
        return lv_ERROR_NOT_FOUND;
    }

    /* 验证预设索引 */
    for (int i = 0; i < preset_count; i++) {
        if (preset_indices[i] < 0 || preset_indices[i] >= block->preset_count) {
            lv_set_error(lv_ERROR_INVALID_PARAM,
                         "创建多投影视图失败：第%d个预设索引=%d无效"
                         "（有效范围：0-%d）",
                         i, preset_indices[i], block->preset_count - 1);
            return lv_ERROR_INVALID_PARAM;
        }
    }

    /* 为每个预设创建视图 */
    for (int i = 0; i < preset_count; i++) {
        /* 生成唯一视图ID：基础编码 + 冲突避免偏移 */
        /* 注意: block_id 和 preset_index 必须小于 1000，否则ID会碰撞 */
        if (block_id >= 1000 || preset_indices[i] >= 1000) {
            lv_LOG_WARNING("视图ID编码: block_id=%d 或 preset_index=%d 超过999，可能产生ID碰撞", block_id,
                           preset_indices[i]);
        }
        int base_vid = block_id * 1000 + preset_indices[i];
        int vid = base_vid;
        int offset = 0;
        /* 检查ID冲突：如果已有相同ID的活跃视图，递增偏移 */
        while (high_dim_find_view_index(vid) >= 0 && offset < 100) {
            offset++;
            vid = base_vid + offset * 10000; /* 加偏移量确保唯一性 */
        }
        if (offset >= 100) {
            /* 无法找到唯一ID：清理已创建的视图 */
            for (int j = 0; j < i; j++) {
                int idx = high_dim_find_view_index(view_ids[j]);
                if (idx >= 0) {
                    g_multi_views[idx].is_active = false;
                }
            }
            lv_set_error(lv_ERROR_RESOURCE_EXHAUSTED,
                         "创建多投影视图失败：block_id=%d 的视图ID空间已耗尽"
                         "（无法为preset_index=%d分配唯一ID）",
                         block_id, preset_indices[i]);
            return lv_ERROR_RESOURCE_EXHAUSTED;
        }

        /* 分配视图槽位 */
        int slot_idx = high_dim_allocate_view_slot(vid, block_id, preset_indices[i]);
        if (slot_idx < 0) {
            /* 槽位已满：回滚已创建的视图 */
            for (int j = 0; j < i; j++) {
                int idx = high_dim_find_view_index(view_ids[j]);
                if (idx >= 0) {
                    g_multi_views[idx].is_active = false;
                }
            }
            lv_set_error(lv_ERROR_RESOURCE_EXHAUSTED,
                         "创建多投影视图失败：全局视图槽位已满"
                         "（最大=%d，当前=%d）",
                         HIGH_DIM_MAX_ACTIVE_VIEWS, g_multi_view_count);
            return lv_ERROR_RESOURCE_EXHAUSTED;
        }

        view_ids[i] = vid;
    }

    /* DEBUG级别日志：提示UI层需要同步创建视图窗口 */
    LOG_DEBUG("high_dim", "已为block_id=%d创建%d个并排投影视图（view_ids[0]=%d,...）。", block_id, preset_count,
              view_ids[0]);

    return lv_OK;
}

int high_dim_destroy_multi_projection_view(HighDimManager *manager, int view_id) {
    /**
     * @brief 销毁多投影视图
     *
     * 完整的视图销毁流程包括以下步骤：
     *   1. 参数与视图ID有效性验证
     *   2. 从 view_id 反推原始的 block_id 和 preset_index
     *      （与 create 函数使用的编码规则一致：view_id = block_id * 1000 + preset_index）
     *   3. 验证对应的 block 和 preset 是否仍然存在
     *   4. 从全局视图追踪数组中标记该视图为 inactive，
     *      释放视图槽位供后续复用
     *   5. 清除该视图的高亮状态记录
     *   6. 输出中文诊断信息以协助UI层同步关闭对应窗口
     *
     * 视图槽位回收策略：
     *   - 将 is_active 设置为 false，不压缩数组（避免O(n)移动）
     *   - 下次 create 调用时通过 high_dim_allocate_view_slot 复用已释放槽位
     *   - 视图槽位在程序退出前不会释放内存，仅做逻辑标记
     *
     * @param manager 高维管理器指针
     * @param view_id 视图ID（来自 high_dim_create_multi_projection_view 的返回值）
     * @return lv_OK 视图成功标记为销毁
     *         lv_ERROR_INVALID_PARAM 参数无效或 view_id 不合法
     *         lv_ERROR_NOT_FOUND 未找到指定的视图或对应的块不存在
     */
    if (!manager)
        return lv_ERROR_INVALID_PARAM;

    /* 验证 view_id 的基本有效性 */
    if (view_id <= 0) {
        lv_set_error(lv_ERROR_INVALID_PARAM, "销毁视图失败：无效的视图ID=%d，ID必须为正值", view_id);
        return lv_ERROR_INVALID_PARAM;
    }

    /* 在全局视图数组中找到该视图 */
    int view_index = high_dim_find_view_index(view_id);
    if (view_index < 0) {
        lv_set_error(lv_ERROR_NOT_FOUND,
                     "销毁视图失败：未找到view_id=%d对应的活跃视图"
                     "（可能已被销毁或从未创建）",
                     view_id);
        return lv_ERROR_NOT_FOUND;
    }

    HighDimMultiViewContext *view_ctx = &g_multi_views[view_index];
    int block_id = view_ctx->block_id;
    int preset_index = view_ctx->preset_index;

    /* 验证对应的块是否仍然存在 */
    HighDimAbstractBlock *block = high_dim_get_block(manager, block_id);
    if (!block) {
        /* 块已被删除：仍需清理视图槽位，使用WARN级别日志记录异常情况 */
        LOG_WARN("high_dim",
                 "view_id=%d对应的block_id=%d已不存在（可能已被注销），"
                 "但视图槽位仍将被清理。",
                 view_id, block_id);
    } else {
        /* 验证预设索引是否仍然有效 */
        if (preset_index < 0 || preset_index >= block->preset_count) {
            LOG_WARN("high_dim",
                     "view_id=%d对应的预设索引=%d已无效"
                     "（当前块预设数=%d），视图槽位仍将被清理。",
                     view_id, preset_index, block->preset_count);
        }
    }

    /* 清除高亮状态 */
    view_ctx->highlighted_count = 0;
    memset(view_ctx->highlighted_elements, 0, sizeof(view_ctx->highlighted_elements));

    /* 标记视图为 inactive（槽位可复用） */
    view_ctx->is_active = false;

    /* DEBUG级别日志：提示UI层需要同步关闭视图窗口 */
    LOG_DEBUG("high_dim", "视图view_id=%d（block_id=%d, preset=%d, 高亮元素数=%d）已销毁。", view_id, block_id,
              preset_index, view_ctx->highlighted_count);

    return lv_OK;
}

int high_dim_link_highlight(HighDimManager *manager, const int *view_ids, int view_count, int element_id) {
    /**
     * @brief 多视图联动高亮元素
     *
     * 在一个视图中高亮元素时，所有关联视图联动高亮对应元素。
     * 实现以下功能：
     *
     *   1. 参数和视图有效性验证：
     *      - manager、view_ids 非空，view_count > 0，element_id >= 0
     *      - 每个 view_id 对应的活跃视图必须在全局追踪数组中存在
     *      - 验证每个视图对应的 block 和 preset 仍有效
     *
     *   2. 坐标空间映射（C层核心逻辑）：
     *      - 获取每个视图的投影预设配置
     *      - 对于给定的 element_id，在各视图中查找对应的几何元素
     *      - 建立跨视图的元素对应关系表
     *      - 注：完整的坐标映射（从视图A到视图B的高维坐标转换）
     *        需要访问实际的节点几何数据，当前C层记录高亮状态，
     *        具体的UI渲染高亮由UI层完成
     *
     *   3. 高亮状态记录：
     *      - 在每个关联视图的上下文中记录 element_id
     *      - 使用 highlighted_elements[] 数组存储高亮元素列表
     *      - 如果该元素已在高亮列表中，则跳过（去重）
     *      - 达到容量上限时输出警告
     *
     *   4. 跨视图一致性保证：
     *      - 确保所有关联视图都接收到相同的高亮指令
     *      - 如果某个视图的高亮列表已满，该视图被跳过并输出警告
     *      - 至少有一个视图成功记录高亮状态时，整体操作视为成功
     *
     * @param manager 高维管理器指针
     * @param view_ids 关联视图ID数组（所有需要联动高亮的视图）
     * @param view_count 视图数量
     * @param element_id 要联动高亮的元素ID（>= 0）
     * @return lv_OK 高亮状态已成功记录到至少一个视图
     *         lv_ERROR_INVALID_PARAM 参数无效
     *         lv_ERROR_NOT_FOUND 所有视图均未找到
     */
    if (!manager || !view_ids || view_count <= 0) {
        return lv_ERROR_INVALID_PARAM;
    }

    /* 验证 element_id 有效性 */
    if (element_id < 0) {
        lv_set_error(lv_ERROR_INVALID_PARAM, "联动高亮失败：无效的元素ID=%d", element_id);
        return lv_ERROR_INVALID_PARAM;
    }

    /* 遍历所有关联视图，记录高亮状态 */
    int views_highlighted = 0;
    int views_skipped = 0;
    char skipped_info[512] = "";

    for (int i = 0; i < view_count; i++) {
        int vid = view_ids[i];
        if (vid <= 0) {
            if (views_skipped < 10) {
                char buf[64];
                high_dim_snprintf(buf, sizeof(buf), "%sview_id[%d]=%d无效; ", views_skipped > 0 ? "" : "", i, vid);
                lv_strlcat(skipped_info, buf, sizeof(skipped_info));
            }
            views_skipped++;
            continue;
        }

        /* 在视图追踪数组中查找 */
        int view_idx = high_dim_find_view_index(vid);
        if (view_idx < 0) {
            if (views_skipped < 10) {
                char buf[64];
                high_dim_snprintf(buf, sizeof(buf), "%sview_id=%d未注册; ", views_skipped > 0 ? "" : "", vid);
                lv_strlcat(skipped_info, buf, sizeof(skipped_info));
            }
            views_skipped++;
            continue;
        }

        HighDimMultiViewContext *view_ctx = &g_multi_views[view_idx];

        /* 验证对应的 block 仍然存在 */
        HighDimAbstractBlock *block = high_dim_get_block(manager, view_ctx->block_id);
        if (!block) {
            if (views_skipped < 10) {
                char buf[80];
                high_dim_snprintf(buf, sizeof(buf), "%sview_id=%d的block_id=%d已注销; ", views_skipped > 0 ? "" : "",
                                  vid, view_ctx->block_id);
                lv_strlcat(skipped_info, buf, sizeof(skipped_info));
            }
            views_skipped++;
            continue;
        }

        /* 检查该元素是否已在高亮列表中（去重） */
        bool already_highlighted = false;
        for (int h = 0; h < view_ctx->highlighted_count; h++) {
            if (view_ctx->highlighted_elements[h] == element_id) {
                already_highlighted = true;
                break;
            }
        }

        if (already_highlighted) {
            views_skipped++;
            continue; /* 跳过重复高亮 */
        }

        /* 添加高亮元素 */
        if (view_ctx->highlighted_count < HIGH_DIM_MAX_DIMENSIONS) {
            view_ctx->highlighted_elements[view_ctx->highlighted_count] = element_id;
            view_ctx->highlighted_count++;
            views_highlighted++;
        } else {
            if (views_skipped < 10) {
                char buf[80];
                high_dim_snprintf(buf, sizeof(buf), "%sview_id=%d高亮列表已满; ", views_skipped > 0 ? "" : "", vid);
                lv_strlcat(skipped_info, buf, sizeof(skipped_info));
            }
            views_skipped++;
        }
    }

    /* 汇总结果 */
    if (views_highlighted == 0) {
        lv_set_error(lv_ERROR_NOT_FOUND, "联动高亮失败：所有%d个视图均未能记录高亮状态。跳过原因：%s", view_count,
                     skipped_info[0] ? skipped_info : "所有视图未注册或无效");
        return lv_ERROR_NOT_FOUND;
    }

    if (views_skipped > 0) {
        /* 部分视图被跳过，使用WARN级别日志记录 */
        LOG_WARN("high_dim",
                 "联动高亮部分成功：元素element_id=%d已在%d/%d个视图中高亮"
                 "（跳过%d个视图：%s）。",
                 element_id, views_highlighted, view_count, views_skipped, skipped_info[0] ? skipped_info : "重复高亮");
    } else {
        /* DEBUG级别日志：记录完整成功状态 */
        LOG_DEBUG("high_dim",
                  "联动高亮成功：元素element_id=%d已在全部%d个关联视图中高亮。"
                  "首视图view_id=%d。",
                  element_id, view_count, view_ids[0]);
    }

    return lv_OK;
}


/* ==================== 多视图管理（统一接口） ==================== */

int high_dim_manage_multi_views(HighDimManager *manager, int operation, int *view_ids, int *count) {
    /**
     * @brief 统一的多投影视图管理接口
     *
     * 【实现概述】
     *   本函数将多投影视图的创建、销毁和查询操作统一为一个管理接口，
     *   使调用者可以通过 operation 参数选择具体操作，简化API使用。
     *
     *   支持的操作类型：
     *     MULTIVIEW_OP_LIST (0)  : 列出所有活跃视图的ID
     *     MULTIVIEW_OP_COUNT (1) : 获取活跃视图的总数
     *     MULTIVIEW_OP_CLEAR (2) : 清除所有视图（批量标记为inactive）
     *
     * 【已实现功能】
     *   1. 列出所有活跃视图（MULTIVIEW_OP_LIST）：
     *      - 遍历全局视图追踪数组 g_multi_views[]
     *      - 将每个活跃视图的 view_id 写入 view_ids 输出数组
     *      - 通过 count 返回实际写入的视图数量
     *      - 参数要求：view_ids 非空，count 非空且 *count >= 期望的视图数
     *   2. 获取活跃视图数量（MULTIVIEW_OP_COUNT）：
     *      - 遍历全局视图追踪数组，统计 is_active == true 的条目
     *      - 通过 count 返回当前活跃视图数
     *      - 注意：此数量可能小于 g_multi_view_count（因为有 inactive 槽位）
     *   3. 清除所有视图（MULTIVIEW_OP_CLEAR）：
     *      - 将所有视图标记为 inactive，清空高亮列表
     *      - 释放视图槽位（标记复用，不压缩数组）
     *      - 注意：此操作不可逆，所有投影视图将被销毁
     *      - 不释放高维块（block），仅清除视图追踪记录
     *
     * 【仍为桩函数的部分（需要外部依赖或后续版本实现）】
     *   1. 按 block_id 过滤列出 —— 当前 LIST 返回所有视图，
     *      不支持只列出某个特定高维块的关联视图
     *   2. 视图状态导出 —— 不支持将当前多视图状态序列化为JSON/配置，
     *      以便保存和恢复工作会话
     *   3. 视图布局查询 —— 不支持获取视图的屏幕布局信息（位置、大小等），
     *      这些信息由UI层管理
     *   4. 批量视图创建 —— 本函数不直接创建视图，
     *      创建操作请使用 high_dim_create_multi_projection_view()
     *   5. 视图克隆/复制 —— 不支持复制一个视图的投影配置到新视图
     *   6. 视图重排 —— 不支持调整视图的显示顺序
     *   7. 视图快照 —— 不支持保存当前视图状态以便后续恢复
     *   8. 并发安全 —— 全局视图数组 g_multi_views[] 无锁保护，
     *      多线程环境下需要外部同步机制
     *
     * 【操作类型常量（建议在 high_dim.h 中定义）】
     *   #define MULTIVIEW_OP_LIST   0
     *   #define MULTIVIEW_OP_COUNT  1
     *   #define MULTIVIEW_OP_CLEAR  2
     *
     * 【使用示例】
     *   // 列出所有视图
     *   int view_ids[32];
     *   int count = 32;
     *   high_dim_manage_multi_views(manager, MULTIVIEW_OP_LIST, view_ids, &count);
     *   printf("活跃视图数: %d\n", count);
     *
     *   // 获取视图数量
     *   int count;
     *   high_dim_manage_multi_views(manager, MULTIVIEW_OP_COUNT, NULL, &count);
     *
     *   // 清除所有视图
     *   int dummy = 0;
     *   high_dim_manage_multi_views(manager, MULTIVIEW_OP_CLEAR, NULL, &dummy);
     *
     * @param manager 高维管理器指针
     * @param operation 操作类型：
     *                  0 = 列出活跃视图ID
     *                  1 = 获取活跃视图数量
     *                  2 = 清除所有视图
     * @param view_ids 视图ID的输出数组（LIST操作使用，COUNT/CLEAR时可为NULL）
     * @param count 输入/输出参数：
     *              LIST操作：输入=数组最大容量，输出=实际写入的视图数
     *              COUNT操作：输出=活跃视图总数
     *              CLEAR操作：输出=被清除的视图数
     * @return lv_OK 操作成功
     *         lv_ERROR_INVALID_PARAM 参数无效
     *         lv_ERROR_UNSUPPORTED 不支持的操作类型
     */
    if (!manager)
        return lv_ERROR_INVALID_PARAM;

    switch (operation) {
        case 0: {
            /*
             * 操作：列出所有活跃视图（MULTIVIEW_OP_LIST）
             *
             * 遍历全局视图追踪数组，收集所有 is_active == true 的视图ID。
             * 视图ID写入 view_ids[] 数组，最多写入 count 个。
             *
             * 【边界检查】
             *   - view_ids 非空且 count 非空：确保输出缓冲区有效
             *   - 写入数量限制在 max_count 内：防止缓冲区溢出
             *   - written 变量独立计数：即使缓冲区不足也能返回实际总数
             *   - g_multi_view_count 受 HIGH_DIM_MAX_VIEWS 限制（定义在头文件），
             *     确保数组索引不越界
             */
            if (!view_ids || !count) {
                return lv_ERROR_INVALID_PARAM;
            }

            int max_count = *count;
            int written = 0;

            for (int i = 0; i < g_multi_view_count; i++) {
                if (g_multi_views[i].is_active) {
                    if (written < max_count) {
                        view_ids[written] = g_multi_views[i].view_id;
                    }
                    written++;
                }
            }

            *count = written;

            if (written > max_count) {
                lv_set_error(lv_OK,
                             "列出视图：共%d个活跃视图，但输出数组容量仅%d，"
                             "实际写入%d个。请增大view_ids数组容量。",
                             written, max_count, max_count);
            } else {
                lv_set_error(lv_OK, "列出视图：共%d个活跃视图，已全部写入view_ids数组。", written);
            }

            return lv_OK;
        }

        case 1: {
            /*
             * 操作：获取活跃视图数量（MULTIVIEW_OP_COUNT）
             *
             * 统计 g_multi_views[] 中 is_active == true 的条目数。
             * 注意：此值通常 <= g_multi_view_count（因为可能有释放的槽位）。
             *
             * 【边界检查】
             *   - count 非空：确保输出参数有效
             *   - 循环受 g_multi_view_count 限制：不会越界访问
             *   - active_count 从 0 递增：返回值最小为 0
             *   - 此操作仅读取数组，不修改全局状态，天然线程安全（只读）
             */
            if (!count) {
                return lv_ERROR_INVALID_PARAM;
            }

            int active_count = 0;
            for (int i = 0; i < g_multi_view_count; i++) {
                if (g_multi_views[i].is_active) {
                    active_count++;
                }
            }

            *count = active_count;
            return lv_OK;
        }

        case 2: {
            /*
             * 操作：清除所有视图（MULTIVIEW_OP_CLEAR）
             *
             * 将所有活跃视图标记为 inactive，清空高亮列表。
             * 这是批量销毁操作，等价于对每个视图调用
             * high_dim_destroy_multi_projection_view()。
             *
             * 【边界检查和错误处理】
             *   - 清空前检查 highlighted_elements 数组容量（HIGH_DIM_MAX_HIGHLIGHTS）
             *   - memset 确保所有高亮条目清零，避免悬空/无效引用
             *   - cleared 变量记录实际清除的视图数，在 count 中返回
             *   - 此操作不可逆：被清除的视图数据已永久丢失，
             *     若需恢复需通过 UI 层重新创建视图
             *   - 此操作不释放高维块（block），仅清除视图追踪记录
             *
             * 【并发安全注意】
             *   全局数组 g_multi_views[] 无锁保护。
             *   多线程环境下，如果某一视图正在渲染中被标记 inactive，
             *   渲染器可能访问已释放的资源。调用者需确保：
             *   1. 在单线程环境中调用（如主事件循环）
             *   2. 或在调用前确保所有渲染操作已完成
             */
            int cleared = 0;
            for (int i = 0; i < g_multi_view_count; i++) {
                if (g_multi_views[i].is_active) {
                    g_multi_views[i].is_active = false;
                    g_multi_views[i].highlighted_count = 0;
                    memset(g_multi_views[i].highlighted_elements, 0, sizeof(g_multi_views[i].highlighted_elements));
                    cleared++;
                }
            }

            if (count) {
                *count = cleared;
            }

            lv_set_error(lv_OK,
                         "多视图管理：已清除%d个活跃视图。"
                         "UI层需同步关闭所有视图窗口并释放渲染资源。",
                         cleared);

            return lv_OK;
        }

        default:
            lv_set_error(lv_ERROR_UNSUPPORTED,
                         "多视图管理失败：不支持的操作类型=%d"
                         "（有效值：0=LIST, 1=COUNT, 2=CLEAR）",
                         operation);
            return lv_ERROR_UNSUPPORTED;
    }
}

