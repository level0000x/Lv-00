/**
 * @file lv_render_visitor.h
 * @brief 渲染访问器 —— 多后端渲染的统一抽象层
 *
 * 将场景图的遍历与后端输出代码分离。
 * 每个后端只需实现 lvRenderVisitor 中的回调函数，
 * 由 lv_render_scene() 统一完成递归遍历和类型分发。
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#ifndef lv_RENDER_VISITOR_H
#define lv_RENDER_VISITOR_H

#include <stdbool.h>
#include <stddef.h>

#include "lv/geo_visual.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 渲染访问器
 *
 * 后端实现这些回调函数，user_data 携带后端私有状态（FILE*, 缓冲区等）。
 * 所有回调返回 false 可中止遍历。
 */
typedef struct {
    /* ── 场景生命周期 ── */
    bool (*begin_scene)(void *user_data, const char *title, int width, int height);
    bool (*end_scene)(void *user_data);

    /* ── 组合对象 ── */
    bool (*begin_group)(void *user_data, const char *label);
    bool (*end_group)(void *user_data);

    /* ── 图元 ── */
    bool (*visit_point)(void *user_data, double x, double y, const lvVisualStyle *style);
    bool (*visit_segment)(void *user_data, double x1, double y1, double x2, double y2,
                          const lvVisualStyle *style);
    bool (*visit_line)(void *user_data, double x1, double y1, double x2, double y2,
                       const lvVisualStyle *style);
    bool (*visit_circle)(void *user_data, double cx, double cy, double r,
                         const lvVisualStyle *style);
    bool (*visit_polygon)(void *user_data, const double *points, int point_count,
                          const lvVisualStyle *style);

    void *user_data; /**< 后端私有数据 */
} lvRenderVisitor;

/**
 * @brief 使用渲染访问器遍历场景
 *
 * 对 scene 中的每个对象递归执行类型分发，
 * 调用 visitor 中对应的 visit_* 回调。
 * 组合对象（GROUP）会自动递归遍历子对象。
 *
 * @param visitor 渲染访问器
 * @param scene   可视化场景
 * @return true  全部渲染成功
 * @return false 遍历被某个回调中止
 */
bool lv_render_scene(const lvRenderVisitor *visitor, const lvVisualScene *scene);

/* ── 内置后端工厂函数 ── */

/**
 * @brief 创建 TikZ 后端访问器
 * @param output_path 输出 .tex 文件路径
 * @param visitor     [out] 填充回调的 visitor 结构体
 * @return true 成功
 */
bool lv_render_visitor_tikz_create(const char *output_path, lvRenderVisitor *visitor);

/**
 * @brief 销毁 TikZ 后端访问器（关闭文件，释放内存）
 * @param visitor 由 lv_render_visitor_tikz_create 填充的 visitor
 */
void lv_render_visitor_tikz_destroy(lvRenderVisitor *visitor);

#ifdef __cplusplus
}
#endif

#endif /* lv_RENDER_VISITOR_H */
