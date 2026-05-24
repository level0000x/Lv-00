/**
 * @file tikz_export.h
 * @brief TikZ 几何导出与渲染 —— 借鉴 jsTikZ / TikZJax 前端 WASM 渲染管道
 *
 * @details 设计借鉴来源：
 *          - jsTikZ (github.com/joaomilho/jsTikZ, 2013-)
 *            · 浏览器端 TikZ 渲染的早期探索
 *            · 基于 Canvas 的手动 TikZ 命令解释器
 *
 *          - TikZJax (github.com/kisonecat/tikzjax, 2019-)
 *            · TikZ 源码 -> WebAssembly LaTeX -> SVG 输出的完整管道
 *            · 增量编译优化：利用 .fmt 格式文件缓存加速 TiKZ 编译
 *            · 将 TeX Live 编译为 WebAssembly 在浏览器端运行
 *
 *          设计目标：
 *          - 提供 TikZ 抽象层，支持从 Lv-00 约束图自动生成 TikZ 几何图
 *          - 多渲染后端：LaTeX 原生 / WASM 浏览器 / dvisvgm 服务端
 *          - 增量编译支持，利用 .fmt 缓存大幅加速连续渲染
 *          - 完整的 TikZ 样式映射（trust_color -> 线型/颜色对应）
 *          - 支持导出 .tex 文件用于 LaTeX 文档嵌入
 *
 * @author Lv-00 Project
 * @version v3.3.0
 * @date 2026-05-24
 */

#ifndef LV00_TIKZ_EXPORT_H
#define LV00_TIKZ_EXPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 常量定义 ==================== */

/** 最大 TikZ 元素数量 */
#define LV00_TIKZ_MAX_ELEMENTS 4096

/** 最大样式数量 */
#define LV00_TIKZ_MAX_STYLES 128

/** 元素 ID 最大字符串长度 */
#define LV00_TIKZ_MAX_ID_LEN 64

/** LaTeX 前导区缓冲区大小 */
#define LV00_TIKZ_PREAMBLE_BUFFER_SIZE 4096

/** 完整 TikZ 文档缓冲区大小 */
#define LV00_TIKZ_DOC_BUFFER_SIZE 262144

/** SVG 输出缓冲区大小 */
#define LV00_TIKZ_SVG_BUFFER_SIZE 524288

/** 样式名称最大长度 */
#define LV00_TIKZ_STYLE_NAME_LEN 32

/** 颜色字符串最大长度 */
#define LV00_TIKZ_COLOR_LEN 16

/** 标签文本最大长度 */
#define LV00_TIKZ_LABEL_LEN 256

/** 缓存路径最大长度 */
#define LV00_TIKZ_PATH_MAX 512

/* ==================== 枚举定义 ==================== */

/**
 * @brief TikZ 元素类型 —— 涵盖几何绘制所需的所有图形原语
 *
 * 本枚举定义了 Lv-00 到 TikZ 的所有可映射几何元素类型。
 * 每个枚举值对应一个 TikZ 绘制命令或组合。
 */
typedef enum {
    TIKZ_POINT          = 0,   /**< 点：\fill 填充小圆 */
    TIKZ_LINE           = 1,   /**< 直线：\draw (A) -- (B) */
    TIKZ_CIRCLE         = 2,   /**< 圆：\draw circle */
    TIKZ_ARC            = 3,   /**< 圆弧：\draw arc */
    TIKZ_POLYGON        = 4,   /**< 多边形：\draw (A) -- (B) -- ... -- cycle */
    TIKZ_LABEL          = 5,   /**< 标签：\node at (A) {text} */
    TIKZ_FILL           = 6,   /**< 填充区域：\fill */
    TIKZ_ARROW          = 7,   /**< 箭头线：\draw[->] */
    TIKZ_ANGLE_ARC      = 8,   /**< 角度弧：\draw angle 标注 */
    TIKZ_MARK_RIGHTANGLE= 9,   /**< 直角标记：\draw 小正方形 */
    TIKZ_DASHED         = 10,  /**< 虚线：\draw[dashed] */
    TIKZ_DOTTED         = 11,  /**< 点线：\draw[dotted] */
    TIKZ_GRID           = 12,  /**< 网格：\draw[help lines] grid */
    TIKZ_AXES           = 13,  /**< 坐标轴：\draw[->] 带箭头 */
    TIKZ_NODE            = 14,  /**< 节点：\node 带形状 */
    TIKZ_PATH            = 15,  /**< 路径：\path 通用路径 */
    TIKZ_SCOPE           = 16,  /**< 作用域：\begin{scope}...\end{scope} */
    TIKZ_CLIP            = 17,  /**< 裁剪：\clip 裁剪区域 */
    TIKZ_SHADE           = 18,  /**< 渐变：\shade 阴影填充 */
    TIKZ_PATTERN         = 19,  /**< 图案填充：\fill[pattern=...] */
    TIKZ_PLOT            = 20,  /**< 函数图：\draw plot 函数曲线 */
    TIKZ_BEZIER          = 21,  /**< 贝塞尔曲线：\draw .. controls .. */
    TIKZ_ELLIPSE         = 22,  /**< 椭圆：\draw ellipse */
    TIKZ_RECTANGLE       = 23,  /**< 矩形：\draw rectangle */
    TIKZ_COORDINATE      = 24,  /**< 坐标点（不可见图元）：\coordinate */
    TIKZ_TANGENT         = 25,  /**< 切线标记 */
    TIKZ_PARALLEL_MARK   = 26,  /**< 平行线标记 */
    TIKZ_EQUAL_LENGTH    = 27,  /**< 等长标记（线段上的短划线） */
    TIKZ_CUSTOM          = 99   /**< 自定义元素（原始 TikZ 代码） */
} Lv00TikZElementType;

/**
 * @brief 渲染后端 —— 借鉴 TikZJax 多级渲染架构
 *
 * TikZJax 将 TeX Live 编译为 WebAssembly，在浏览器端完成
 * TikZ -> DVI -> SVG 的完整转换。Lv-00 支持三种渲染后端。
 */
typedef enum {
    RENDER_VIA_LATEX    = 0,  /**< 传统 LaTeX 渲染（需系统安装 TeX Live） */
    RENDER_VIA_WASM     = 1,  /**< WebAssembly 渲染（TikZJax 风格，浏览器端） */
    RENDER_VIA_DVISVGM  = 2   /**< dvisvgm 渲染（服务端，DVI 转 SVG） */
} Lv00TikZRenderBackend;

/**
 * @brief 箭头样式枚举
 */
typedef enum {
    ARROW_NONE        = 0,  /**< 无箭头 */
    ARROW_STANDARD    = 1,  /**< 标准箭头 -> */
    ARROW_REVERSE     = 2,  /**< 反向箭头 <- */
    ARROW_DOUBLE      = 3,  /**< 双向箭头 <-> */
    ARROW_STEALTH     = 4,  /**< stealth 风格箭头 */
    ARROW_LATEX       = 5,  /**< LaTeX 风格箭头 */
    ARROW_HARPOON     = 6,  /**< 鱼叉箭头 |-> */
} Lv00TikZArrowStyle;

/**
 * @brief 破折线样式枚举
 */
typedef enum {
    DASH_SOLID        = 0,  /**< 实线 */
    DASH_DASHED       = 1,  /**< 虚线 */
    DASH_DOTTED       = 2,  /**< 点线 */
    DASH_DASHDOT      = 3,  /**< 点划线 */
    DASH_DASHDOTDOT   = 4,  /**< 双点划线 */
    DASH_LOOSELY_DASHED = 5, /**< 稀疏虚线 */
    DASH_DENSELY_DASHED = 6, /**< 密集虚线 */
    DASH_LOOSELY_DOTTED  = 7, /**< 稀疏点线 */
    DASH_DENSELY_DOTTED  = 8, /**< 密集点线 */
    DASH_CUSTOM       = 99, /**< 自定义破折线模式 */
} Lv00TikZDashPattern;

/**
 * @brief TikZ 编译步骤
 */
typedef enum {
    TIKZ_COMPILE_STEP_LATEX      = 0,  /**< LaTeX 编译：.tex -> .dvi */
    TIKZ_COMPILE_STEP_DVI_TO_SVG = 1,  /**< DVI 转 SVG：.dvi -> .svg */
    TIKZ_COMPILE_STEP_DVI_TO_PDF = 2,  /**< DVI 转 PDF：.dvi -> .pdf */
    TIKZ_COMPILE_STEP_WASM       = 3,  /**< WASM 内联编译（单步完成） */
} Lv00TikZCompileStep;

/**
 * @brief 坐标维度模式
 */
typedef enum {
    COORD_2D  = 0,  /**< 二维坐标 (x, y) */
    COORD_3D  = 1,  /**< 三维坐标 (x, y, z) */
    COORD_POLAR = 2, /**< 极坐标 (theta:radius) */
} Lv00TikZCoordMode;

/* ==================== 结构体定义 ==================== */

/**
 * @brief TikZ 样式定义
 *
 * 封装 TikZ 绘图的所有视觉样式属性。
 * 包括线型（颜色/宽度/破折线/箭头）、填充、标签字体等。
 */
typedef struct Lv00TikZStyle {
    /* ── 线条样式 ── */
    double  line_width;                         /**< 线条宽度（pt） */
    char    line_color[LV00_TIKZ_COLOR_LEN];    /**< 线条颜色（TikZ 颜色名或 #RRGGBB） */
    char    fill_color[LV00_TIKZ_COLOR_LEN];    /**< 填充颜色 */
    Lv00TikZDashPattern dash_pattern;           /**< 破折线模式 */
    double  opacity;                            /**< 透明度 [0.0, 1.0] */

    /* ── 箭头 ── */
    Lv00TikZArrowStyle arrow_style;             /**< 箭头样式 */

    /* ── 标签 ── */
    int     label_font_size;                    /**< 标签字体大小（pt） */

    /* ── 补充 ── */
    char    style_name[LV00_TIKZ_STYLE_NAME_LEN]; /**< 样式名称（用于 \tikzset） */
    char    custom_options[128];                /**< 自定义 TikZ 选项字符串 */

    /* ── trust_color 自动映射 ── */
    /** 映射规则：
     *  GREEN  -> blue, very thick
     *  BLUE   -> solid, default
     *  YELLOW -> dashed
     *  ORANGE -> dotted
     *  RED    -> red, thick
     */
    int     trust_color_mapping;                /**< 信任颜色映射值 */
} Lv00TikZStyle;

/**
 * @brief 单个 TikZ 元素
 *
 * 代表一个可渲染的 TikZ 图形原语。
 * 可独立绘制或作为约束图中的节点对应项。
 */
typedef struct Lv00TikZElement {
    /* ── 标识 ── */
    char    element_id[LV00_TIKZ_MAX_ID_LEN];   /**< 元素唯一标识符 */
    Lv00TikZElementType tikz_type;              /**< TikZ 元素类型 */

    /* ── 坐标数据 ── */
    Lv00TikZCoordMode coord_mode;               /**< 坐标维度模式 */
    double *coords;                             /**< 坐标数组（每 2 或 3 个值为一组） */
    int     coord_count;                        /**< 坐标值总数 */
    int     point_count;                        /**< 点数 = coord_count / (2 或 3) */

    /* ── 点标签 ── */
    char  **point_labels;                       /**< 点标签数组（如 "A", "B", "C"） */
    int     point_label_count;                  /**< 标签数量 */

    /* ── 通用标签 ── */
    char    label_text[LV00_TIKZ_LABEL_LEN];    /**< 通用标签文本 */

    /* ── 样式引用 ── */
    int     style_ref;                          /**< 样式索引（-1 = 使用默认样式） */

    /* ── 父子关系 ── */
    int     parent_element_id;                  /**< 父元素索引（-1 = 顶层） */

    /* ── 约束图关联 ── */
    int     constraint_graph_node_id;           /**< 对应约束图节点 ID（-1 = 无关联） */

    /* ── 原始 TikZ 代码（自定义元素专用） ── */
    char   *raw_tikz_code;                      /**< 原始 TikZ 代码字符串（TIKZ_CUSTOM 使用） */
} Lv00TikZElement;

/**
 * @brief TikZ 渲染上下文
 *
 * 管理一组 TikZ 元素和样式，提供完整的 TikZ 图片描述。
 * 是渲染操作的主要入口。
 */
typedef struct Lv00TikZContext {
    /* ── 元素与样式 ── */
    Lv00TikZElement elements[LV00_TIKZ_MAX_ELEMENTS]; /**< TikZ 元素数组 */
    int              element_count;                   /**< 已注册元素数量 */
    Lv00TikZStyle   styles[LV00_TIKZ_MAX_STYLES];    /**< 样式数组 */
    int              style_count;                     /**< 已注册样式数量 */

    /* ── LaTeX 前导区 ── */
    char  preamble_packages[LV00_TIKZ_PREAMBLE_BUFFER_SIZE]; /**< 前导区包列表 */
    /** 默认包括：tikz, calc, angles, quotes, patterns, intersections,
     *          arrows.meta, decorations.markings, backgrounds, fit */
    int   preamble_custom_flags;                           /**< 自定义前导区标志 */

    /* ── 图片属性 ── */
    double scale;                                   /**< 全局缩放（TikZ scale 选项） */
    double bounding_box_xmin;                       /**< 包围盒 X 最小值 */
    double bounding_box_ymin;                       /**< 包围盒 Y 最小值 */
    double bounding_box_xmax;                       /**< 包围盒 X 最大值 */
    double bounding_box_ymax;                       /**< 包围盒 Y 最大值 */

    /* ── 文档模式 ── */
    bool   standalone_mode;                         /**< 是否生成完整 LaTeX 文档 */
    /** standalone_mode = true：
     *  输出完整 \documentclass[tikz]{standalone} ... \end{document}
     *  standalone_mode = false：
     *  仅输出 \begin{tikzpicture}...\end{tikzpicture} 片段 */

    /* ── 调试选项 ── */
    bool   show_construction_lines;                /**< 显示辅助构造线 */
    bool   show_point_labels;                      /**< 显示点标签 */
    bool   show_coordinates;                       /**< 显示坐标文本 */
} Lv00TikZContext;

/**
 * @brief TikZ 渲染配置
 *
 * 控制渲染管道的后端选择和编译参数。
 * 借鉴 TikZJax 的多后端支持与增量编译优化。
 */
typedef struct Lv00TikZRenderConfig {
    /* ── 渲染后端 ── */
    Lv00TikZRenderBackend backend;            /**< 渲染后端选择 */

    /* ── 输出质量 ── */
    int    dpi;                               /**< 输出 DPI（用于位图渲染） */
    bool   antialias;                         /**< 是否启用抗锯齿 */

    /* ── 增量编译 ── */
    bool   incremental;                       /**< 启用增量编译模式 */
    /** 增量编译流程：
     *  1. 首次编译生成 .fmt 格式文件（预编译宏包集合）
     *  2. 后续编译加载 .fmt 直接跳入 tikzpicture 渲染
     *  3. 大幅缩短编译时间（典型加速比 5x-20x） */
    char   fmt_cache_path[LV00_TIKZ_PATH_MAX]; /**< .fmt 格式文件缓存路径 */

    /* ── LaTeX 配置 ── */
    char   latex_engine[32];                  /**< LaTeX 引擎：pdflatex / xelatex / lualatex */
    char   texlive_path[LV00_TIKZ_PATH_MAX];  /**< TeX Live 安装路径 */

    /* ── WASM 配置 ── */
    char   wasm_module_path[LV00_TIKZ_PATH_MAX];  /**< TikZJax WASM 模块路径 */

    /* ── 超时 ── */
    int    compile_timeout_ms;                /**< 编译超时（毫秒），0 = 无超时 */

    /* ── 缓存 ── */
    bool   use_element_cache;                 /**< 使用元素级渲染缓存 */
    int    cache_max_entries;                 /**< 缓存最大条目数 */
} Lv00TikZRenderConfig;

/* ==================== 生命周期 ==================== */

/**
 * @brief 初始化 TikZ 渲染上下文
 *
 * 分配并初始化 Lv00TikZContext，设置默认样式（实线黑色、1pt 线宽）、
 * 默认前导区包列表、standalone 模式开启。
 *
 * @return 新分配的 TikZ 上下文，失败返回 NULL
 */
Lv00TikZContext* tikz_init(void);

/**
 * @brief 销毁 TikZ 渲染上下文并释放所有关联资源
 *
 * 包括所有元素的坐标数组、标签数组、原始代码字符串等。
 * 传入 NULL 是安全的。
 *
 * @param[in,out] ctx TikZ 上下文（设为 NULL 是安全的）
 */
void tikz_destroy(Lv00TikZContext *ctx);

/* ==================== 元素添加 ==================== */

/**
 * @brief 添加点到 TikZ 上下文
 *
 * @param[in,out] ctx    TikZ 上下文
 * @param[in]     x      点的 X 坐标
 * @param[in]     y      点的 Y 坐标
 * @param[in]     label  点标签（如 "A"，可为 NULL）
 * @param[in]     style_ref 样式索引（-1 = 默认样式）
 * @return 新元素的索引，失败返回 -1
 */
int tikz_add_point(Lv00TikZContext *ctx, double x, double y,
                    const char *label, int style_ref);

/**
 * @brief 添加直线到 TikZ 上下文
 *
 * @param[in,out] ctx        TikZ 上下文
 * @param[in]     x1, y1     起点坐标
 * @param[in]     x2, y2     终点坐标
 * @param[in]     style_ref  样式索引（-1 = 默认样式）
 * @return 新元素的索引，失败返回 -1
 */
int tikz_add_line(Lv00TikZContext *ctx,
                   double x1, double y1, double x2, double y2,
                   int style_ref);

/**
 * @brief 添加圆到 TikZ 上下文
 *
 * @param[in,out] ctx        TikZ 上下文
 * @param[in]     cx, cy     圆心坐标
 * @param[in]     radius     半径
 * @param[in]     style_ref  样式索引（-1 = 默认样式）
 * @return 新元素的索引，失败返回 -1
 */
int tikz_add_circle(Lv00TikZContext *ctx,
                     double cx, double cy, double radius,
                     int style_ref);

/**
 * @brief 添加圆弧到 TikZ 上下文
 *
 * @param[in,out] ctx           TikZ 上下文
 * @param[in]     cx, cy        圆心坐标
 * @param[in]     start_angle   起始角度（度）
 * @param[in]     end_angle     结束角度（度）
 * @param[in]     radius        半径
 * @param[in]     style_ref     样式索引（-1 = 默认样式）
 * @return 新元素的索引，失败返回 -1
 */
int tikz_add_arc(Lv00TikZContext *ctx,
                  double cx, double cy,
                  double start_angle, double end_angle, double radius,
                  int style_ref);

/**
 * @brief 添加多边形到 TikZ 上下文
 *
 * @param[in,out] ctx           TikZ 上下文
 * @param[in]     xs, ys        顶点 X/Y 坐标数组
 * @param[in]     vertex_count  顶点数量
 * @param[in]     labels        顶点标签数组（可为 NULL）
 * @param[in]     closed        是否闭合（true = cycle，false = 开放折线）
 * @param[in]     style_ref     样式索引（-1 = 默认样式）
 * @return 新元素的索引，失败返回 -1
 */
int tikz_add_polygon(Lv00TikZContext *ctx,
                      const double *xs, const double *ys,
                      int vertex_count, const char **labels,
                      bool closed, int style_ref);

/**
 * @brief 添加文本标签到 TikZ 上下文
 *
 * @param[in,out] ctx     TikZ 上下文
 * @param[in]     x, y    标签位置
 * @param[in]     text    标签文本
 * @param[in]     position 相对位置（"above"/"below"/"left"/"right"/"above left" 等）
 * @param[in]     font_size 字体大小（0 = 默认）
 * @return 新元素的索引，失败返回 -1
 */
int tikz_add_label(Lv00TikZContext *ctx, double x, double y,
                    const char *text, const char *position, int font_size);

/**
 * @brief 添加角度标记到 TikZ 上下文
 *
 * 使用 TikZ angles 库在三点处标记角度。
 *
 * @param[in,out] ctx     TikZ 上下文
 * @param[in]     ax, ay  角顶点坐标
 * @param[in]     bx, by  第一条边上的点
 * @param[in]     cx, cy  第二条边上的点
 * @param[in]     label   角度标签（可为 NULL）
 * @param[in]     style_ref 样式索引（-1 = 默认）
 * @return 新元素的索引，失败返回 -1
 */
int tikz_add_angle_mark(Lv00TikZContext *ctx,
                         double ax, double ay,
                         double bx, double by,
                         double cx, double cy,
                         const char *label, int style_ref);

/**
 * @brief 添加直角标记到 TikZ 上下文
 *
 * 在三点处（角顶点 + 两边上各一点）绘制直角小正方形。
 *
 * @param[in,out] ctx      TikZ 上下文
 * @param[in]     vertex_x, vertex_y 直角顶点坐标
 * @param[in]     leg1_x, leg1_y     第一条直角边上的点
 * @param[in]     leg2_x, leg2_y     第二条直角边上的点
 * @param[in]     size              直角标记尺寸
 * @param[in]     style_ref         样式索引（-1 = 默认）
 * @return 新元素的索引，失败返回 -1
 */
int tikz_add_right_angle(Lv00TikZContext *ctx,
                          double vertex_x, double vertex_y,
                          double leg1_x, double leg1_y,
                          double leg2_x, double leg2_y,
                          double size, int style_ref);

/* ==================== 样式管理 ==================== */

/**
 * @brief 注册自定义样式
 *
 * 样式注册后可通过索引引用。索引 0 始终为默认样式（实线、黑色、1pt）。
 *
 * @param[in,out] ctx   TikZ 上下文
 * @param[in]     style 样式定义
 * @return 样式索引，失败返回 -1
 */
int tikz_set_style(Lv00TikZContext *ctx, const Lv00TikZStyle *style);

/**
 * @brief 设置 Lv-00 证明几何的默认样式映射
 *
 * 根据 trust_color 自动设置样式：
 * - GREEN  -> 蓝色实线加粗（已证明的核心构造）
 * - BLUE   -> 黑色实线标准粗细（已验证但未绝对证明）
 * - YELLOW -> 虚线（基于假设）
 * - ORANGE -> 点线（推测性）
 * - RED    -> 红色加粗（矛盾/错误）
 *
 * 此函数会注册 5 个样式到上下文中。
 *
 * @param[in,out] ctx TikZ 上下文
 * @return 成功返回 0，失败返回 -1
 */
int tikz_set_default_geometry_style(Lv00TikZContext *ctx);

/* ==================== 渲染 ==================== */

/**
 * @brief 渲染 TikZ 上下文为完整 LaTeX 文档字符串
 *
 * 生成从 \documentclass 到 \end{document} 的完整 LaTeX 文档
 * (standalone_mode = true) 或 tikzpicture 片段 (standalone_mode = false)。
 *
 * @param[in]  ctx    TikZ 上下文
 * @param[out] output 输出：LaTeX 代码字符串（调用者负责 free）
 * @return 生成的代码字符数，失败返回 -1
 */
int tikz_render(const Lv00TikZContext *ctx, char **output);

/**
 * @brief 渲染 TikZ 上下文为 SVG 字符串
 *
 * 使用配置的后端完成 TikZ -> SVG 转换。
 * RENDER_VIA_LATEX: 调用 latex + dvisvgm
 * RENDER_VIA_WASM:  使用 TikZJax WebAssembly 模块
 * RENDER_VIA_DVISVGM: 调用 dvisvgm 处理已有 DVI
 *
 * @param[in]  ctx    TikZ 上下文
 * @param[in]  config 渲染配置
 * @param[out] output 输出：SVG 字符串（调用者负责 free）
 * @return SVG 字符串的字符数，失败返回 -1
 */
int tikz_render_svg(const Lv00TikZContext *ctx,
                     const Lv00TikZRenderConfig *config,
                     char **output);

/**
 * @brief 完整 LaTeX 编译流程：.tex -> .dvi -> .svg / .pdf
 *
 * 执行完整的 TikZ 编译管道：
 * 1. 调用 tikz_render() 生成 .tex 内容
 * 2. 写入临时 .tex 文件
 * 3. 调用 LaTeX 编译器生成 .dvi
 * 4. 调用 dvisvgm 或 dvipdf 生成 .svg 或 .pdf
 * 5. 清理临时文件
 *
 * @param[in] ctx         TikZ 上下文
 * @param[in] config      渲染配置
 * @param[in] output_path 输出文件路径（.svg 或 .pdf 后缀决定格式）
 * @return 成功返回 0，编译失败返回负的错误码
 *
 * @note WASM 后端不经过文件系统，直接返回 tikz_render_svg()
 */
int tikz_compile(const Lv00TikZContext *ctx,
                  const Lv00TikZRenderConfig *config,
                  const char *output_path);

/* ==================== 从约束图生成 ==================== */

/**
 * @brief 从约束图自动生成 TikZ 几何图 —— 核心映射接口
 *
 * 遍历约束图中的节点和约束，自动生成对应的 TikZ 元素：
 * - GEOM_POINT -> TIKZ_POINT + TIKZ_LABEL
 * - GEOM_LINE_SEGMENT -> TIKZ_LINE
 * - GEOM_REGION -> TIKZ_POLYGON (closed) / TIKZ_CIRCLE
 * - 约束（INCIDENCE/BETWEENNESS/INTERSECTION）-> 对应虚线辅助线
 *
 * trust_color 自动映射到样式（见 tikz_set_default_geometry_style()）。
 *
 * @param[in]  ctx        TikZ 上下文
 * @param[in]  graph      约束图句柄（void* 以避免循环依赖，内部转换为 ConstraintGraph*）
 * @return 成功返回 0，失败返回 -1
 */
int tikz_from_constraint_graph(Lv00TikZContext *ctx, const void *graph);

/* ==================== 文件导出 ==================== */

/**
 * @brief 导出 TikZ 上下文为 .tex 文件
 *
 * 调用 tikz_render() 生成内容后写入文件。
 *
 * @param[in] ctx       TikZ 上下文
 * @param[in] filepath  输出 .tex 文件路径
 * @return 成功返回 0，文件写入失败返回 -1
 */
int tikz_export_file(const Lv00TikZContext *ctx, const char *filepath);

/* ==================== 格式缓存 ==================== */

/**
 * @brief 预编译 LaTeX 格式文件加速后续编译 —— 借鉴 TikZJax 增量编译
 *
 * 生成一个 .fmt 格式文件，包含所有前导区包的预编译结果。
 * 后续编译通过 &fmt 加载 .fmt 文件，跳过前导区处理直接渲染 tikzpicture。
 *
 * 使用流程：
 * 1. tikz_cache_fmt(ctx, config)  —— 生成 .fmt 文件（一次性操作）
 * 2. tikz_render_svg(ctx, config)  —— config.incremental = true 利用缓存加速
 *
 * @param[in]  ctx    TikZ 上下文
 * @param[in]  config 渲染配置（fmt_cache_path 决定输出路径）
 * @return 成功返回 0，编译失败返回 -1
 */
int tikz_cache_fmt(const Lv00TikZContext *ctx,
                    const Lv00TikZRenderConfig *config);

/* ==================== 辅助函数 ==================== */

/**
 * @brief 创建默认渲染配置
 *
 * 默认：RENDER_VIA_LATEX, 300 dpi, 抗锯齿开启,
 * incremental = false, pdflatex, 30s 超时。
 *
 * @return 默认渲染配置
 */
Lv00TikZRenderConfig tikz_render_config_default(void);

/**
 * @brief 估算 TikZ 图片的包围盒
 *
 * 遍历所有已添加的元素，计算最小外包矩形。
 * 结果写入 ctx->bounding_box_* 字段。
 *
 * @param[in,out] ctx TikZ 上下文
 */
void tikz_compute_bounding_box(Lv00TikZContext *ctx);

/**
 * @brief 获取元素类型名称（用于调试）
 *
 * @param[in] type TikZ 元素类型
 * @return 类型名称字符串（如 "TIKZ_LINE"）
 */
const char* tikz_element_type_name(Lv00TikZElementType type);

/**
 * @brief 获取渲染后端名称
 *
 * @param[in] backend 渲染后端
 * @return 后端名称字符串（如 "RENDER_VIA_LATEX"）
 */
const char* tikz_render_backend_name(Lv00TikZRenderBackend backend);

/**
 * @brief 清除上下文中的所有元素
 *
 * 保留样式和配置，仅清除元素列表。用于重用上下文生成多个图片。
 *
 * @param[in,out] ctx TikZ 上下文
 */
void tikz_clear_elements(Lv00TikZContext *ctx);

#ifdef __cplusplus
}
#endif

#endif /* LV00_TIKZ_EXPORT_H */
