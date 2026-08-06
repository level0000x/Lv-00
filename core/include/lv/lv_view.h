#ifndef lv_VIEW_H
#define lv_VIEW_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 视图类型 */
typedef enum {
    lv_VIEW_BLOCK_CANVAS,
    lv_VIEW_GEOMETRY_CANVAS,
    lv_VIEW_NODE_GRAPH,
    lv_VIEW_TEXT_CODE,
    lv_VIEW_COUNT
} lvViewType;

/** @brief 视图基类 */
typedef struct lvView {
    lvViewType type;
    bool dirty;
    void (*destroy)(struct lvView *view);
    bool (*render_svg)(struct lvView *view, char *buf, size_t size);
} lvView;

/** @brief 统一导出格式枚举（覆盖 interop / tikz / proof 三套导出系统） */
typedef enum {
    lv_EXPORT_COQ,             /**< Coq 证明脚本 */
    lv_EXPORT_LEAN,            /**< Lean 4 证明脚本 */
    lv_EXPORT_HTML,            /**< 独立 HTML 页面 */
    lv_EXPORT_SVG,             /**< SVG 矢量图 */
    lv_EXPORT_PDF,             /**< PDF 文档 */
    lv_EXPORT_TIKZ,            /**< LaTeX TikZ 完整文档 */
    lv_EXPORT_GEOJSON,         /**< GeoJSON 地理数据 */
    lv_EXPORT_CANONICAL,       /**< 规范表示 */
    lv_EXPORT_ISABELLE,        /**< Isabelle/HOL 证明脚本 */
    lv_EXPORT_HOL_LIGHT,       /**< HOL Light 证明脚本 */
    lv_EXPORT_LATEX,           /**< LaTeX 证明文档（proof.h） */
    lv_EXPORT_NATURAL_LANGUAGE,/**< 自然语言描述（proof.h） */
    lv_EXPORT_COUNT            /**< 导出格式总数 */
} lvExportFormat;

#ifdef __cplusplus
}
#endif

#endif
