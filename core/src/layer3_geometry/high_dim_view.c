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
#include "lv/lv_thread.h"

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
 * 在 C 层维护每个多投影视图的状态，包括关联的高维块、投影预设、
 * 当前高亮元素列表，以及显示顺序/布局元数据。视图ID由 create 函数
 * 生成，通过静态数组统一管理。
 *
 * 布局与显示顺序字段由 C 层维护逻辑状态，供 UI 层消费：
 *   - z_order        ：显示顺序（数值越小越靠前），重排操作修改该值
 *   - layout_x/y     ：视口左上角像素坐标（默认瀑布流网格分配）
 *   - layout_width/h ：视口尺寸（像素）
 *   - camera_zoom    ：摄像机缩放系数（1.0 = 原始大小）
 *   - camera_angle   ：摄像机旋转角（弧度）
 */
typedef struct {
    int view_id;                                       /**< 唯一视图标识符 */
    int block_id;                                      /**< 关联的高维块ID */
    int preset_index;                                  /**< 使用的投影预设索引 */
    bool is_active;                                    /**< 视图是否处于激活状态 */
    int highlighted_elements[HIGH_DIM_MAX_DIMENSIONS]; /**< 当前高亮的元素ID列表 */
    int highlighted_count;                             /**< 高亮元素数量 */
    int z_order;                                       /**< 显示顺序（越小越靠前） */
    int layout_x;                                      /**< 视口左上角 x（像素） */
    int layout_y;                                      /**< 视口左上角 y（像素） */
    int layout_width;                                  /**< 视口宽度（像素） */
    int layout_height;                                 /**< 视口高度（像素） */
    double camera_zoom;                                /**< 摄像机缩放系数 */
    double camera_angle;                               /**< 摄像机旋转角（弧度） */
} HighDimMultiViewContext;

/** 全局活跃视图追踪数组（线程本地：每个线程维护独立视图集合） */
static lv_THREAD_LOCAL HighDimMultiViewContext g_multi_views[HIGH_DIM_MAX_ACTIVE_VIEWS];
static lv_THREAD_LOCAL int g_multi_view_count = 0;

/* ==================== 并发安全（全局锁） ==================== */

/**
 * @brief 全局视图数组互斥锁
 *
 * 尽管 g_multi_views[] 是线程本地数组，同一线程内的重入调用以及
 * 未来可能引入的共享状态仍需要互斥保护。所有公开入口
 * （create/destroy/link_highlight/manage_multi_views）在访问全局
 * 数组前必须持有该锁，保证多线程环境下视图状态的一致性。
 */
static lv_mutex_t g_multi_views_lock;
static lv_once_t g_multi_views_lock_once = lv_ONCE_INIT;

static void high_dim_views_lock_init(void) {
    lv_mutex_init(&g_multi_views_lock);
}

static void high_dim_views_lock(void) {
    lv_once(&g_multi_views_lock_once, high_dim_views_lock_init);
    lv_mutex_lock(&g_multi_views_lock);
}

static void high_dim_views_unlock(void) {
    lv_mutex_unlock(&g_multi_views_lock);
}

/* ==================== 视图快照存储 ==================== */

/** 视图快照存储：保存/恢复用（线程本地，与视图数组同线程语义） */
static lv_THREAD_LOCAL HighDimMultiViewContext g_multi_view_snapshot[HIGH_DIM_MAX_ACTIVE_VIEWS];
static lv_THREAD_LOCAL int g_multi_view_snapshot_count = 0;

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 在全局视图数组中查找指定 view_id 对应的索引
 *
 * @param view_id 视图ID
 * @return 数组索引（>= 0），未找到返回 -1
 * @note 调用者必须持有 g_multi_views_lock
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
 * @brief 计算下一个可用的显示顺序（当前最大 z_order + 1）
 *
 * @return 新的 z_order 值
 * @note 调用者必须持有 g_multi_views_lock
 */
static int high_dim_next_z_order(void) {
    int max_z = 0;
    for (int i = 0; i < g_multi_view_count; i++) {
        if (g_multi_views[i].is_active && g_multi_views[i].z_order > max_z) {
            max_z = g_multi_views[i].z_order;
        }
    }
    return max_z + 1;
}

/**
 * @brief 为新分配的视图初始化默认布局（瀑布流网格）
 *
 * 默认布局：每行 4 个视口，网格尺寸 320x240（视口 300x220）。
 * 显示顺序自动追加到当前所有视图之后。
 *
 * @param ctx 视图上下文
 * @param slot_index 槽位索引（决定网格位置）
 * @note 调用者必须持有 g_multi_views_lock
 */
static void high_dim_init_view_layout(HighDimMultiViewContext *ctx, int slot_index) {
    int col = slot_index % 4;
    int row = slot_index / 4;
    ctx->z_order = high_dim_next_z_order();
    ctx->layout_x = col * 320;
    ctx->layout_y = row * 240;
    ctx->layout_width = 300;
    ctx->layout_height = 220;
    ctx->camera_zoom = 1.0;
    ctx->camera_angle = 0.0;
}

/**
 * @brief 分配一个新的视图上下文槽位
 *
 * @param view_id 视图ID
 * @param block_id 关联的高维块ID
 * @param preset_index 投影预设索引
 * @return 新分配的数组索引，失败返回 -1
 * @note 调用者必须持有 g_multi_views_lock
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
            high_dim_init_view_layout(&g_multi_views[i], i);
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
    high_dim_init_view_layout(&g_multi_views[idx], idx);
    g_multi_view_count++;

    return idx;
}

/**
 * @brief 在 JSON 字符串缓冲区末尾追加一段格式化内容
 *
 * 基于 vsnprintf 的截断检测：若剩余空间不足则返回 -1，
 * 否则将写入长度累加到 *pos。
 *
 * @param buf 目标缓冲区
 * @param size 缓冲区总容量
 * @param pos 当前写入位置（输入/输出）
 * @param fmt 格式化串
 * @return 0 成功，-1 缓冲区空间不足
 */
static int high_dim_json_append(char *buf, size_t size, size_t *pos, const char *fmt, ...) {
    if (!buf || size == 0 || !pos) {
        return -1;
    }
    if (*pos >= size) {
        return -1;
    }
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf + *pos, size - *pos, fmt, args);
    va_end(args);
    if (n < 0) {
        return -1;
    }
    if ((size_t)n >= size - *pos) {
        return -1; /* 截断 */
    }
    *pos += (size_t)n;
    return 0;
}

/**
 * @brief 将当前全部活跃视图状态序列化为 JSON 字符串
 *
 * 序列化格式（真实 JSON）：
 *   {"view_count":N,"views":[{"view_id":..,"block_id":..,"preset_index":..,
 *    "z_order":..,"layout":{"x":..,"y":..,"width":..,"height":..},
 *    "camera":{"zoom":..,"angle":..},"highlights":[...]},...]}
 *
 * @param buffer 输出缓冲区（UTF-8）
 * @param buffer_size 缓冲区容量（字节）
 * @return 写入的字节数（不含终止符），容量不足返回 -1
 * @note 调用者必须持有 g_multi_views_lock
 */
static int high_dim_views_export_json(char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size < 2) {
        return -1;
    }
    size_t pos = 0;
    /* 先统计活跃视图数，并预写根节点 */
    int active_count = 0;
    for (int i = 0; i < g_multi_view_count; i++) {
        if (g_multi_views[i].is_active) {
            active_count++;
        }
    }

    if (high_dim_json_append(buffer, buffer_size, &pos, "{\"view_count\":%d,\"views\":[", active_count) != 0) {
        return -1;
    }

    int written_views = 0;
    for (int i = 0; i < g_multi_view_count; i++) {
        if (!g_multi_views[i].is_active) {
            continue;
        }
        HighDimMultiViewContext *v = &g_multi_views[i];
        if (written_views > 0) {
            if (high_dim_json_append(buffer, buffer_size, &pos, ",") != 0) {
                return -1;
            }
        }
        if (high_dim_json_append(buffer, buffer_size, &pos,
                                 "{\"view_id\":%d,\"block_id\":%d,\"preset_index\":%d,\"z_order\":%d,"
                                 "\"layout\":{\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d},"
                                 "\"camera\":{\"zoom\":%.3f,\"angle\":%.4f},\"highlights\":[",
                                 v->view_id, v->block_id, v->preset_index, v->z_order,
                                 v->layout_x, v->layout_y, v->layout_width, v->layout_height,
                                 v->camera_zoom, v->camera_angle) != 0) {
            return -1;
        }
        for (int h = 0; h < v->highlighted_count; h++) {
            if (h > 0) {
                if (high_dim_json_append(buffer, buffer_size, &pos, ",") != 0) {
                    return -1;
                }
            }
            if (high_dim_json_append(buffer, buffer_size, &pos, "%d", v->highlighted_elements[h]) != 0) {
                return -1;
            }
        }
        if (high_dim_json_append(buffer, buffer_size, &pos, "]}") != 0) {
            return -1;
        }
        written_views++;
    }

    if (high_dim_json_append(buffer, buffer_size, &pos, "]}") != 0) {
        return -1;
    }
    return (int)pos;
}

/**
 * @brief 将当前全部活跃视图状态序列化为 JSON 字符串（公开接口）
 *
 * @param manager 高维管理器指针（仅用于非空校验）
 * @param buffer 输出缓冲区（UTF-8）
 * @param buffer_size 缓冲区容量（字节）
 * @return 写入的字节数（不含终止符），参数无效或容量不足返回负值
 */
int high_dim_export_views_json(HighDimManager *manager, char *buffer, size_t buffer_size) {
    if (!manager || !buffer || buffer_size == 0) {
        return -1;
    }
    high_dim_views_lock();
    int n = high_dim_views_export_json(buffer, buffer_size);
    high_dim_views_unlock();
    return n;
}

static int high_dim_create_multi_projection_view_locked(HighDimManager *manager, int block_id,
                                                        const int *preset_indices, int preset_count, int *view_ids) {
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
     * @note 调用者必须持有 g_multi_views_lock
     */
    if (!manager || !preset_indices || !view_ids || preset_count <= 0) {
        return lv_ERROR_INVALID_PARAM;
    }

    HighDimAbstractBlock *block = high_dim_get_block(manager, block_id);
    if (!block) {
        lv_RETURN_ERROR_VAL(lv_ERROR_NOT_FOUND, lv_ERROR_NOT_FOUND,
                            "创建多投影视图失败：未找到block_id=%d对应的高维抽象块", block_id);
    }

    /* 验证预设索引 */
    for (int i = 0; i < preset_count; i++) {
        if (preset_indices[i] < 0 || preset_indices[i] >= block->preset_count) {
            lv_RETURN_ERROR_VAL(lv_ERROR_INVALID_PARAM, lv_ERROR_INVALID_PARAM,
                                "创建多投影视图失败：第%d个预设索引=%d无效"
                                "（有效范围：0-%d）",
                                i, preset_indices[i], block->preset_count - 1);
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
            lv_RETURN_ERROR_VAL(lv_ERROR_RESOURCE_EXHAUSTED, lv_ERROR_RESOURCE_EXHAUSTED,
                                "创建多投影视图失败：block_id=%d 的视图ID空间已耗尽"
                                "（无法为preset_index=%d分配唯一ID）",
                                block_id, preset_indices[i]);
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
            lv_RETURN_ERROR_VAL(lv_ERROR_RESOURCE_EXHAUSTED, lv_ERROR_RESOURCE_EXHAUSTED,
                                "创建多投影视图失败：全局视图槽位已满"
                                "（最大=%d，当前=%d）",
                                HIGH_DIM_MAX_ACTIVE_VIEWS, g_multi_view_count);
        }

        view_ids[i] = vid;
    }

    /* DEBUG级别日志：提示UI层需要同步创建视图窗口 */
    LOG_DEBUG("high_dim", "已为block_id=%d创建%d个并排投影视图（view_ids[0]=%d,...）。", block_id, preset_count,
              view_ids[0]);

    return lv_OK;
}

/**
 * @brief 创建多投影并排视图（线程安全公开入口）
 *
 * 加锁保护全局视图数组后委托给内部实现。
 */
int high_dim_create_multi_projection_view(HighDimManager *manager, int block_id, const int *preset_indices,
                                          int preset_count, int *view_ids) {
    high_dim_views_lock();
    int rc = high_dim_create_multi_projection_view_locked(manager, block_id, preset_indices, preset_count, view_ids);
    high_dim_views_unlock();
    return rc;
}

static int high_dim_destroy_multi_projection_view_locked(HighDimManager *manager, int view_id) {
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
     * @note 调用者必须持有 g_multi_views_lock
     */
    if (!manager)
        return lv_ERROR_INVALID_PARAM;

    /* 验证 view_id 的基本有效性 */
    if (view_id <= 0) {
        lv_RETURN_ERROR_VAL(lv_ERROR_INVALID_PARAM, lv_ERROR_INVALID_PARAM,
                            "销毁视图失败：无效的视图ID=%d，ID必须为正值", view_id);
    }

    /* 在全局视图数组中找到该视图 */
    int view_index = high_dim_find_view_index(view_id);
    if (view_index < 0) {
        lv_RETURN_ERROR_VAL(lv_ERROR_NOT_FOUND, lv_ERROR_NOT_FOUND,
                            "销毁视图失败：未找到view_id=%d对应的活跃视图"
                            "（可能已被销毁或从未创建）",
                            view_id);
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

/**
 * @brief 销毁多投影视图（线程安全公开入口）
 *
 * 加锁保护全局视图数组后委托给内部实现。
 */
int high_dim_destroy_multi_projection_view(HighDimManager *manager, int view_id) {
    high_dim_views_lock();
    int rc = high_dim_destroy_multi_projection_view_locked(manager, view_id);
    high_dim_views_unlock();
    return rc;
}

static int high_dim_link_highlight_locked(HighDimManager *manager, const int *view_ids, int view_count, int element_id) {
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
     * @note 调用者必须持有 g_multi_views_lock
     */
    if (!manager || !view_ids || view_count <= 0) {
        return lv_ERROR_INVALID_PARAM;
    }

    /* 验证 element_id 有效性 */
    if (element_id < 0) {
        lv_RETURN_ERROR_VAL(lv_ERROR_INVALID_PARAM, lv_ERROR_INVALID_PARAM,
                            "联动高亮失败：无效的元素ID=%d", element_id);
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
        lv_RETURN_ERROR_VAL(lv_ERROR_NOT_FOUND, lv_ERROR_NOT_FOUND,
                            "联动高亮失败：所有%d个视图均未能记录高亮状态。跳过原因：%s", view_count,
                            skipped_info[0] ? skipped_info : "所有视图未注册或无效");
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

/**
 * @brief 多视图联动高亮元素（线程安全公开入口）
 *
 * 加锁保护全局视图数组后委托给内部实现。
 */
int high_dim_link_highlight(HighDimManager *manager, const int *view_ids, int view_count, int element_id) {
    high_dim_views_lock();
    int rc = high_dim_link_highlight_locked(manager, view_ids, view_count, element_id);
    high_dim_views_unlock();
    return rc;
}
/* ==================== 多视图管理操作处理器（查找表模式） ==================== */

/** 多视图管理操作处理器函数类型 */
typedef int (*MultiViewHandler)(HighDimManager *manager, int *view_ids, int *count);

/* ---- MULTIVIEW_OP_LIST ---- */
static int high_dim_mv_handler_list(HighDimManager *manager, int *view_ids, int *count) {
    (void)manager;
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
        /* 成功信息伪日志（info 级）：记录成功详情，非错误 */
    lv_set_error(lv_OK,
                     "列出视图：共%d个活跃视图，但输出数组容量仅%d，"
                     "实际写入%d个。请增大view_ids数组容量。",
                     written, max_count, max_count);
    } else {
        /* 成功信息伪日志（info 级）：记录成功详情，非错误 */
    lv_set_error(lv_OK, "列出视图：共%d个活跃视图，已全部写入view_ids数组。", written);
    }
    return lv_OK;
}

/* ---- MULTIVIEW_OP_COUNT ---- */
static int high_dim_mv_handler_count(HighDimManager *manager, int *view_ids, int *count) {
    (void)manager;
    (void)view_ids;
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

/* ---- MULTIVIEW_OP_CLEAR ---- */
static int high_dim_mv_handler_clear(HighDimManager *manager, int *view_ids, int *count) {
    (void)manager;
    (void)view_ids;
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
    /* 成功信息伪日志（info 级）：记录成功详情，非错误 */
    lv_set_error(lv_OK,
                 "多视图管理：已清除%d个活跃视图。"
                 "UI层需同步关闭所有视图窗口并释放渲染资源。",
                 cleared);
    return lv_OK;
}

/* ---- MULTIVIEW_OP_LIST_BY_BLOCK ---- */
static int high_dim_mv_handler_list_by_block(HighDimManager *manager, int *view_ids, int *count) {
    (void)manager;
    if (!view_ids || !count) {
        return lv_ERROR_INVALID_PARAM;
    }
    int block_filter = view_ids[0];
    int capacity = *count;
    int written = 0;
    for (int i = 0; i < g_multi_view_count; i++) {
        if (g_multi_views[i].is_active && g_multi_views[i].block_id == block_filter) {
            if (written < capacity) {
                view_ids[written + 1] = g_multi_views[i].view_id;
            }
            written++;
        }
    }
    *count = written;
    if (written > capacity) {
        /* 成功信息伪日志（info 级）：记录成功详情，非错误 */
    lv_set_error(lv_OK,
                     "按block_id=%d过滤列出视图：共%d个匹配，但输出数组容量仅%d，"
                     "实际写入%d个。请增大view_ids数组容量。",
                     block_filter, written, capacity, capacity);
    } else {
        /* 成功信息伪日志（info 级）：记录成功详情，非错误 */
    lv_set_error(lv_OK, "按block_id=%d过滤列出视图：共%d个匹配，已全部写入view_ids数组。", block_filter,
                     written);
    }
    return lv_OK;
}

/* ---- MULTIVIEW_OP_EXPORT_JSON ---- */
static int high_dim_mv_handler_export_json(HighDimManager *manager, int *view_ids, int *count) {
    (void)manager;
    if (!view_ids || !count) {
        return lv_ERROR_INVALID_PARAM;
    }
    int capacity = *count;
    if (capacity <= 0) {
        return lv_ERROR_INVALID_PARAM;
    }
    int written = high_dim_views_export_json((char *)view_ids, (size_t)capacity);
    if (written < 0) {
        lv_RETURN_ERROR_VAL(lv_ERROR_BUFFER_TOO_SMALL, lv_ERROR_BUFFER_TOO_SMALL,
                            "多视图导出失败：JSON序列化结果超过缓冲区容量%d字节。"
                            "请使用 high_dim_export_views_json() 并传入足够大的缓冲区。",
                            capacity);
    }
    *count = written;
    return lv_OK;
}

/* ---- MULTIVIEW_OP_QUERY_LAYOUT ---- */
static int high_dim_mv_handler_query_layout(HighDimManager *manager, int *view_ids, int *count) {
    (void)manager;
    if (!view_ids || !count) {
        return lv_ERROR_INVALID_PARAM;
    }
    int arr_len = *count;
    if (arr_len < 6) {
        lv_RETURN_ERROR_VAL(lv_ERROR_INVALID_PARAM, lv_ERROR_INVALID_PARAM,
                            "查询视图布局失败：view_ids数组长度=%d，至少需要6个元素。",
                            arr_len);
    }
    int target_vid = view_ids[0];
    int idx = high_dim_find_view_index(target_vid);
    if (idx < 0) {
        lv_RETURN_ERROR_VAL(lv_ERROR_NOT_FOUND, lv_ERROR_NOT_FOUND,
                            "查询视图布局失败：未找到view_id=%d对应的活跃视图。", target_vid);
    }
    HighDimMultiViewContext *v = &g_multi_views[idx];
    view_ids[1] = v->layout_x;
    view_ids[2] = v->layout_y;
    view_ids[3] = v->layout_width;
    view_ids[4] = v->layout_height;
    view_ids[5] = v->z_order;
    *count = 1;
    return lv_OK;
}

/* ---- MULTIVIEW_OP_CREATE_BATCH ---- */
static int high_dim_mv_handler_create_batch(HighDimManager *manager, int *view_ids, int *count) {
    if (!view_ids || !count) {
        return lv_ERROR_INVALID_PARAM;
    }
    int arr_len = *count;
    if (arr_len < 2) {
        return lv_ERROR_INVALID_PARAM;
    }
    int batch_block_id = view_ids[0];
    int batch_preset_count = arr_len - 1;
    if (batch_preset_count > HIGH_DIM_MAX_ACTIVE_VIEWS) {
        lv_RETURN_ERROR_VAL(lv_ERROR_INVALID_PARAM, lv_ERROR_INVALID_PARAM,
                            "批量创建视图失败：预设数量=%d超过最大视图数%d。",
                            batch_preset_count, HIGH_DIM_MAX_ACTIVE_VIEWS);
    }
    int input_presets[HIGH_DIM_MAX_ACTIVE_VIEWS];
    for (int i = 0; i < batch_preset_count; i++) {
        input_presets[i] = view_ids[i + 1];
    }
    int out_ids[HIGH_DIM_MAX_ACTIVE_VIEWS];
    int rc = high_dim_create_multi_projection_view_locked(manager, batch_block_id, input_presets,
                                                          batch_preset_count, out_ids);
    if (rc != lv_OK) {
        return rc;
    }
    for (int i = 0; i < batch_preset_count; i++) {
        view_ids[i] = out_ids[i];
    }
    *count = batch_preset_count;
    /* 成功信息伪日志（info 级）：记录成功详情，非错误 */
    lv_set_error(lv_OK, "批量创建多投影视图：block_id=%d 成功创建%d个视图。", batch_block_id, batch_preset_count);
    return lv_OK;
}

/* ---- MULTIVIEW_OP_CLONE ---- */
static int high_dim_mv_handler_clone(HighDimManager *manager, int *view_ids, int *count) {
    (void)manager;
    if (!view_ids || !count) {
        return lv_ERROR_INVALID_PARAM;
    }
    int src_vid = view_ids[0];
    int src_idx = high_dim_find_view_index(src_vid);
    if (src_idx < 0) {
        lv_RETURN_ERROR_VAL(lv_ERROR_NOT_FOUND, lv_ERROR_NOT_FOUND,
                            "克隆视图失败：未找到源视图view_id=%d。", src_vid);
    }
    HighDimMultiViewContext *src = &g_multi_views[src_idx];
    int base_vid = src->block_id * 1000 + src->preset_index;
    int new_vid = base_vid;
    int offset = 0;
    while (high_dim_find_view_index(new_vid) >= 0 && offset < 100) {
        offset++;
        new_vid = base_vid + offset * 10000;
    }
    if (offset >= 100) {
        lv_RETURN_ERROR_VAL(lv_ERROR_RESOURCE_EXHAUSTED, lv_ERROR_RESOURCE_EXHAUSTED,
                            "克隆视图失败：视图ID空间已耗尽。");
    }
    int slot = high_dim_allocate_view_slot(new_vid, src->block_id, src->preset_index);
    if (slot < 0) {
        lv_RETURN_ERROR_VAL(lv_ERROR_RESOURCE_EXHAUSTED, lv_ERROR_RESOURCE_EXHAUSTED,
                            "克隆视图失败：全局视图槽位已满。");
    }
    HighDimMultiViewContext *dst = &g_multi_views[slot];
    dst->z_order = src->z_order + 1;
    dst->layout_x = src->layout_x;
    dst->layout_y = src->layout_y;
    dst->layout_width = src->layout_width;
    dst->layout_height = src->layout_height;
    dst->camera_zoom = src->camera_zoom;
    dst->camera_angle = src->camera_angle;
    dst->highlighted_count = src->highlighted_count;
    memcpy(dst->highlighted_elements, src->highlighted_elements, sizeof(dst->highlighted_elements));
    view_ids[1] = new_vid;
    *count = new_vid;
    LOG_DEBUG("high_dim", "视图克隆成功：view_id=%d -> view_id=%d（继承布局/相机/高亮状态）。", src_vid,
              new_vid);
    return lv_OK;
}

/* ---- MULTIVIEW_OP_REORDER ---- */
static int high_dim_mv_handler_reorder(HighDimManager *manager, int *view_ids, int *count) {
    (void)manager;
    if (!view_ids || !count) {
        return lv_ERROR_INVALID_PARAM;
    }
    int n = *count;
    if (n <= 0 || n > HIGH_DIM_MAX_ACTIVE_VIEWS) {
        lv_RETURN_ERROR_VAL(lv_ERROR_INVALID_PARAM, lv_ERROR_INVALID_PARAM,
                            "视图重排失败：数组长度=%d超出有效范围（1-%d）。", n,
                            HIGH_DIM_MAX_ACTIVE_VIEWS);
    }
    for (int i = 0; i < n; i++) {
        if (high_dim_find_view_index(view_ids[i]) < 0) {
            lv_RETURN_ERROR_VAL(lv_ERROR_NOT_FOUND, lv_ERROR_NOT_FOUND,
                                "视图重排失败：数组第%d个view_id=%d不存在。", i, view_ids[i]);
        }
    }
    for (int i = 0; i < n; i++) {
        int idx = high_dim_find_view_index(view_ids[i]);
        g_multi_views[idx].z_order = i + 1;
    }
    /* 成功信息伪日志（info 级）：记录成功详情，非错误 */
    lv_set_error(lv_OK, "视图重排成功：%d个视图已按指定顺序重新排列。", n);
    return lv_OK;
}

/* ---- MULTIVIEW_OP_SNAPSHOT_SAVE ---- */
static int high_dim_mv_handler_snapshot_save(HighDimManager *manager, int *view_ids, int *count) {
    (void)manager;
    (void)view_ids;
    int saved = 0;
    for (int i = 0; i < g_multi_view_count && saved < HIGH_DIM_MAX_ACTIVE_VIEWS; i++) {
        if (g_multi_views[i].is_active) {
            g_multi_view_snapshot[saved] = g_multi_views[i];
            saved++;
        }
    }
    g_multi_view_snapshot_count = saved;
    if (count) {
        *count = saved;
    }
    /* 成功信息伪日志（info 级）：记录成功详情，非错误 */
    lv_set_error(lv_OK, "视图快照保存成功：已保存%d个活跃视图的状态。", saved);
    return lv_OK;
}

/* ---- MULTIVIEW_OP_SNAPSHOT_RESTORE ---- */
static int high_dim_mv_handler_snapshot_restore(HighDimManager *manager, int *view_ids, int *count) {
    (void)manager;
    (void)view_ids;
    int restored = 0;
    for (int s = 0; s < g_multi_view_snapshot_count; s++) {
        HighDimMultiViewContext *snap = &g_multi_view_snapshot[s];
        if (!snap->is_active) {
            continue;
        }
        int idx = high_dim_find_view_index(snap->view_id);
        if (idx >= 0) {
            HighDimMultiViewContext *v = &g_multi_views[idx];
            v->z_order = snap->z_order;
            v->layout_x = snap->layout_x;
            v->layout_y = snap->layout_y;
            v->layout_width = snap->layout_width;
            v->layout_height = snap->layout_height;
            v->camera_zoom = snap->camera_zoom;
            v->camera_angle = snap->camera_angle;
            v->highlighted_count = snap->highlighted_count;
            memcpy(v->highlighted_elements, snap->highlighted_elements, sizeof(v->highlighted_elements));
            restored++;
        } else {
            int new_vid = snap->view_id;
            if (high_dim_find_view_index(new_vid) >= 0) {
                int base = new_vid;
                int offset = 0;
                while (high_dim_find_view_index(new_vid) >= 0 && offset < 100) {
                    offset++;
                    new_vid = base + offset * 10000;
                }
                if (offset >= 100) {
                    continue;
                }
            }
            int slot = high_dim_allocate_view_slot(new_vid, snap->block_id, snap->preset_index);
            if (slot < 0) {
                continue;
            }
            HighDimMultiViewContext *v = &g_multi_views[slot];
            v->z_order = snap->z_order;
            v->layout_x = snap->layout_x;
            v->layout_y = snap->layout_y;
            v->layout_width = snap->layout_width;
            v->layout_height = snap->layout_height;
            v->camera_zoom = snap->camera_zoom;
            v->camera_angle = snap->camera_angle;
            v->highlighted_count = snap->highlighted_count;
            memcpy(v->highlighted_elements, snap->highlighted_elements, sizeof(v->highlighted_elements));
            restored++;
        }
    }
    if (count) {
        *count = restored;
    }
    /* 成功信息伪日志（info 级）：记录成功详情，非错误 */
    lv_set_error(lv_OK, "视图快照恢复成功：已恢复%d个视图的状态。", restored);
    return lv_OK;
}

/* ---- 默认（不支持的操作类型） ---- */
static int high_dim_mv_handler_unsupported(HighDimManager *manager, int *view_ids, int *count) {
    (void)manager;
    (void)view_ids;
    (void)count;
    /* 从调用处获取 operation 值的方式：通过栈回溯不可行，直接返回错误 */
    lv_RETURN_ERROR_VAL(lv_ERROR_UNSUPPORTED, lv_ERROR_UNSUPPORTED,
                        "多视图管理失败：不支持的操作类型"
                        "（有效值：0=LIST, 1=COUNT, 2=CLEAR, 3=LIST_BY_BLOCK, "
                        "4=EXPORT_JSON, 5=QUERY_LAYOUT, 6=CREATE_BATCH, 7=CLONE, "
                        "8=REORDER, 9=SNAPSHOT_SAVE, 10=SNAPSHOT_RESTORE）");
}

/** 多视图管理操作查找表：索引为 MULTIVIEW_OP_* 值，末项为 unsupported 兜底 */
static const MultiViewHandler kMultiViewHandlers[] = {
    [MULTIVIEW_OP_LIST]            = high_dim_mv_handler_list,
    [MULTIVIEW_OP_COUNT]           = high_dim_mv_handler_count,
    [MULTIVIEW_OP_CLEAR]           = high_dim_mv_handler_clear,
    [MULTIVIEW_OP_LIST_BY_BLOCK]   = high_dim_mv_handler_list_by_block,
    [MULTIVIEW_OP_EXPORT_JSON]     = high_dim_mv_handler_export_json,
    [MULTIVIEW_OP_QUERY_LAYOUT]    = high_dim_mv_handler_query_layout,
    [MULTIVIEW_OP_CREATE_BATCH]    = high_dim_mv_handler_create_batch,
    [MULTIVIEW_OP_CLONE]           = high_dim_mv_handler_clone,
    [MULTIVIEW_OP_REORDER]         = high_dim_mv_handler_reorder,
    [MULTIVIEW_OP_SNAPSHOT_SAVE]   = high_dim_mv_handler_snapshot_save,
    [MULTIVIEW_OP_SNAPSHOT_RESTORE] = high_dim_mv_handler_snapshot_restore,
};
static const int kMultiViewHandlerCount = sizeof(kMultiViewHandlers) / sizeof(kMultiViewHandlers[0]);

/* ==================== 多视图管理（统一接口） ==================== */

static int high_dim_manage_multi_views_locked(HighDimManager *manager, int operation, int *view_ids, int *count) {
    /**
     * @brief 统一的多投影视图管理接口
     *
     * 【实现概述】
     *   本函数将多投影视图的创建、销毁、查询、导出、布局、克隆、
     *   重排与快照操作统一为一个管理接口，使调用者可以通过 operation
     *   参数选择具体操作，简化API使用。
     *
     * 【支持的操作类型】
     *   MULTIVIEW_OP_LIST             (0) : 列出所有活跃视图的ID
     *   MULTIVIEW_OP_COUNT            (1) : 获取活跃视图的总数
     *   MULTIVIEW_OP_CLEAR            (2) : 清除所有视图（批量标记为inactive）
     *   MULTIVIEW_OP_LIST_BY_BLOCK    (3) : 按 block_id 过滤列出活跃视图
     *   MULTIVIEW_OP_EXPORT_JSON      (4) : 将全部视图状态导出为 JSON
     *   MULTIVIEW_OP_QUERY_LAYOUT     (5) : 查询指定视图的布局信息
     *   MULTIVIEW_OP_CREATE_BATCH     (6) : 批量创建视图
     *   MULTIVIEW_OP_CLONE            (7) : 克隆/复制一个视图到新视图
     *   MULTIVIEW_OP_REORDER          (8) : 按指定顺序重排视图
     *   MULTIVIEW_OP_SNAPSHOT_SAVE    (9) : 保存当前视图状态快照
     *   MULTIVIEW_OP_SNAPSHOT_RESTORE (10): 从快照恢复视图状态
     *
     * 【各操作参数约定】
     *   LIST (0):
     *     - view_ids 输出数组；*count 输入=容量，输出=实际写入数
     *   COUNT (1):
     *     - *count 输出=活跃视图总数
     *   CLEAR (2):
     *     - *count 输出=被清除的视图数
     *   LIST_BY_BLOCK (3):
     *     - view_ids[0] = 目标 block_id（输入），view_ids[1..] 输出匹配的视图ID
     *     - *count 输入=输出数组容量，输出=匹配的视图数
     *   EXPORT_JSON (4):
     *     - view_ids 需强制转换为 (char *) 作为 JSON 输出缓冲区
     *     - *count 输入=缓冲区字节容量，输出=实际写入字节数（不含终止符）
     *     - 更安全的做法：直接调用 high_dim_export_views_json()
     *   QUERY_LAYOUT (5):
     *     - view_ids[0] = 目标 view_id（输入）
     *     - view_ids[1..5] 输出 layout_x/layout_y/width/height/z_order（整数像素）
     *     - *count 输入=数组长度（需>=6），输出=1（找到）
     *   CREATE_BATCH (6):
     *     - view_ids[0] = block_id（输入），view_ids[1..] = preset_indices（输入）
     *     - *count 输入=数组长度（=preset数+1），输出=创建的视图数
     *     - view_ids[0..] 覆盖为创建出的 view_id 列表
     *   CLONE (7):
     *     - view_ids[0] = 源 view_id（输入），view_ids[1] 输出=新 view_id
     *     - *count 输出=新 view_id
     *   REORDER (8):
     *     - view_ids = 新的显示顺序（视图ID数组）；*count = 数组长度
     *     - 数组顺序即目标 z_order（1..n）
     *   SNAPSHOT_SAVE (9):
     *     - *count 输出=已保存的视图数（view_ids 可为 NULL）
     *   SNAPSHOT_RESTORE (10):
     *     - *count 输出=已恢复的视图数（view_ids 可为 NULL）
     *
     * 【并发安全】
     *   所有公开入口（create/destroy/link_highlight/manage_multi_views）
     *   在访问全局视图数组前持有 g_multi_views_lock 互斥锁，保证
     *   多线程环境下视图状态的一致性。
     *
     * @param manager 高维管理器指针
     * @param operation 操作类型（见 MULTIVIEW_OP_* 常量定义）
     * @param view_ids 视图ID数组（参数约定见上）
     * @param count 输入/输出参数（参数约定见上）
     * @return lv_OK 操作成功
     *         lv_ERROR_INVALID_PARAM 参数无效
     *         lv_ERROR_NOT_FOUND 未找到指定的视图/块
     *         lv_ERROR_RESOURCE_EXHAUSTED 视图槽位或ID空间耗尽
     *         lv_ERROR_BUFFER_TOO_SMALL 导出缓冲区容量不足
     *         lv_ERROR_UNSUPPORTED 不支持的操作类型
     * @note 调用者必须持有 g_multi_views_lock
     */
    if (operation >= 0 && operation < kMultiViewHandlerCount) {
        return kMultiViewHandlers[operation](manager, view_ids, count);
    }
    return high_dim_mv_handler_unsupported(manager, view_ids, count);
}

/**
 * @brief 统一的多投影视图管理接口（线程安全公开入口）
 *
 * 加锁保护全局视图数组后委托给内部实现。
 */
int high_dim_manage_multi_views(HighDimManager *manager, int operation, int *view_ids, int *count) {
    if (!manager) {
        return lv_ERROR_INVALID_PARAM;
    }
    high_dim_views_lock();
    int rc = high_dim_manage_multi_views_locked(manager, operation, view_ids, count);
    high_dim_views_unlock();
    return rc;
}