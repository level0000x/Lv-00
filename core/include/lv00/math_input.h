/* ========================================================================
 * 模块名称：数学输入系统 (math_input)
 * 功能概述：借鉴 MathLive 的交互式编辑器设计，提供所见即所得的数学
 *          公式输入系统。支持 LaTeX/纯文本/可视化三种输入模式，
 *          预置 20+ 几何专用宏，可自定义宏库和虚拟键盘布局，
 *          支持命令补全、undo/redo 和双向绑定。
 *
 * 主要 API：
 *   - math_input_init / destroy               — 初始化/销毁输入状态
 *   - math_input_set_mode / get_mode          — 输入模式切换
 *   - math_input_register_macro               — 注册自定义宏
 *   - math_input_create_default_geometry_macros — 创建几何宏库
 *   - math_input_autocomplete                 — 命令补全
 *   - math_input_render / parse               — 渲染/解析
 *   - math_input_export_latex / import_latex  — LaTeX 导入/导出
 *   - math_input_insert_text / delete / undo  — 文本编辑
 *
 * 使用示例：
 *   MathInputState *state = math_input_init();
 *   math_input_create_default_geometry_macros();
 *   math_input_insert_text(state, "\\point{A}");
 *   const char *rendered = math_input_render(state);
 *
 * @version v3.3.0
 * ======================================================================== */

/**
 * @file math_input.h
 * @brief 所见即所得数学公式输入系统 —— 借鉴 MathLive 的交互式编辑器设计
 */

#ifndef LV00_MATH_INPUT_H
#define LV00_MATH_INPUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 前向声明 MathExpr（来自 math_protocol.h） */
typedef struct MathExpr MathExpr;

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 第一部分：输入模式与键盘布局
 * ======================================================================== */

/**
 * @brief 数学输入模式枚举
 */
typedef enum {
    INPUT_MODE_LATEX,     /**< LaTeX 输入模式：用户输入 LaTeX 源码，实时渲染 */
    INPUT_MODE_PLAINTEXT, /**< 纯文本输入模式：类似 ASCIIMath 的文本表示 */
    INPUT_MODE_VISUAL     /**< 可视化输入模式：点选虚拟键盘/菜单构建表达式 */
} MathInputMode;

/**
 * @brief 虚拟键盘布局枚举
 */
typedef enum {
    KEYBOARD_STANDARD, /**< 标准数学键盘：数字、基本运算符、希腊字母 */
    KEYBOARD_GEOMETRY, /**< 几何键盘：\point, \line, \circle, \triangle 等宏按钮 */
    KEYBOARD_PROOF,    /**< 证明键盘：\implies, \forall, \exists, \vdash 等逻辑符号 */
    KEYBOARD_GREEK,    /**< 希腊字母键盘：\alpha, \beta, \gamma, ... */
    KEYBOARD_CUSTOM    /**< 用户自定义键盘布局 */
} MathKeyboardLayout;

/* ========================================================================
 * 第二部分：数学宏系统
 *
 * 借鉴 MathLive 的自定义宏机制：
 *   用户可定义 \point{A} → \text{点 }A 这样的快捷宏。
 *   系统预置 20+ 几何专用宏（带分类和键盘图标提示）。
 * ======================================================================== */

/**
 * @brief 宏的分类
 */
typedef enum {
    MACRO_CAT_GEOMETRY, /**< 几何：\point, \line, \circle, \segment, \angle, \triangle 等 */
    MACRO_CAT_ALGEBRA,  /**< 代数：\frac, \sqrt, \sum, \prod, \int 等 */
    MACRO_CAT_PROOF,    /**< 证明：\implies, \therefore, \forall, \exists 等 */
    MACRO_CAT_CUSTOM    /**< 用户自定义 */
} MathMacroCategory;

/**
 * @brief 数学宏 —— 一个可展开的 LaTeX 快捷命令
 *
 * 范例：
 *   \point{A}  展开为 \text{点 }A
 *   \segment{AB} 展开为 \overline{AB}
 *   \similar 展开为 \sim
 */
typedef struct MathMacro {
    char *macro_name;           /**< 宏名称（不含反斜杠，如 "point"） */
    char *expansion;            /**< 展开后的 LaTeX 模板（如 "\\text{点 }{#1}") */
    MathMacroCategory category; /**< 宏分类 */
    int arg_count;              /**< 参数数量（0 = 无参数） */
    char *keyboard_hint;        /**< 虚拟键盘上的显示提示（如 "P" 或图标名） */
    char *description;          /**< 宏的描述文本 */
    int id;                     /**< 宏在库中的唯一 ID */
} MathMacro;

/* ========================================================================
 * 第三部分：宏库 —— 20+ 几何专用宏预置
 * ======================================================================== */

/**
 * @brief 数学宏库
 *
 * 维护所有已注册的宏。预置 20+ 几何宏：
 *   \point \line \circle \segment \angle \triangle \quadrilateral
 *   \parallel \perp \cong \similar \intersect \tangent \bisector
 *   \midpoint \distance \area \perimeter \circumcircle \incircle
 *   \centroid \orthocenter \collinear \concurrent
 */
typedef struct MathMacroLibrary {
    MathMacro *macros;  /**< 宏数组 */
    int macro_count;    /**< 当前宏数量 */
    int macro_capacity; /**< 宏数组容量 */
    char *library_name; /**< 库名称（便于调试和导出） */
} MathMacroLibrary;

/* ========================================================================
 * 第四部分：命令补全系统
 *
 * 当用户输入前缀（如 "\ang"）时，系统从宏库中匹配候选列表。
 * 借鉴 MathLive 的 autocomplete 机制。
 * ======================================================================== */

/**
 * @brief 命令补全候选
 */
typedef struct MathCompletionCandidate {
    char *name;                 /**< 补全字符串（如 "angle"） */
    char *display;              /**< 显示文本（如 "\\angle{#1}"） */
    MathMacroCategory category; /**< 分类 */
    int score;                  /**< 匹配分数（越高越相关） */
} MathCompletionCandidate;

/**
 * @brief 命令补全上下文
 *
 * 维护前缀、候选列表和触发字符。
 */
typedef struct MathAutocomplete {
    char *prefix;                        /**< 用户已输入的前缀（含反斜杠，如 "\\ang"） */
    MathCompletionCandidate *candidates; /**< 匹配的候选列表 */
    int candidate_count;                 /**< 候选数量 */
    int candidate_capacity;              /**< 候选列表容量 */
    int max_candidates;                  /**< 最大候选数（默认 10） */
    char trigger_char;                   /**< 触发字符（默认 '\\'） */
    int selected_index;                  /**< 当前选中的候选索引（-1 = 无选中） */
} MathAutocomplete;

/* ========================================================================
 * 第五部分：输入状态
 *
 * 维护编辑器的完整状态：光标位置、选中区域、模式、活跃宏、
 * 历史记录等。支持 undo/redo。
 * ======================================================================== */

/** @brief 历史记录最大深度 */
#define MATH_INPUT_HISTORY_MAX 64

/**
 * @brief 数学输入状态
 *
 * 封装编辑器的所有运行时状态，支持双向绑定（编辑器↔渲染视图）。
 */
typedef struct MathInputState {
    /* 当前文本 */
    char *input_text;  /**< 当前输入缓冲区的完整文本 */
    int text_length;   /**< 文本长度 */
    int text_capacity; /**< 文本缓冲区容量 */

    /* 光标与选区 */
    int cursor_position; /**< 光标位置（字符偏移） */
    int selection_start; /**< 选区起始（= cursor_position 表示无选区） */
    int selection_end;   /**< 选区结束 */

    /* 模式 */
    MathInputMode current_mode;         /**< 当前输入模式 */
    MathKeyboardLayout active_keyboard; /**< 活跃键盘布局 */

    /* 宏 */
    MathMacroLibrary *macro_library; /**< 关联的宏库 */
    int *active_macro_ids;           /**< 当前活跃的宏 ID 数组 */
    int active_macro_count;          /**< 活跃宏数量 */

    /* 历史记录 */
    char *history[MATH_INPUT_HISTORY_MAX]; /**< 文本历史（undo 栈） */
    int history_index;                     /**< 当前历史位置 */
    int history_count;                     /**< 历史条目总数 */

    /* 双向绑定 */
    bool dirty;            /**< 是否有未同步的更改 */
    MathExpr *parsed_expr; /**< 缓存的解析结果（MathExpr 树） */
    char *rendered_output; /**< 渲染输出字符串 */

    /* 错误信息 */
    bool has_error;          /**< 是否存在解析/渲染错误 */
    char error_message[256]; /**< 错误描述 */
} MathInputState;

/* ========================================================================
 * 第六部分：核心 API —— 初始化与配置
 * ======================================================================== */

/**
 * @brief 初始化数学输入状态
 *
 * 创建并初始化 MathInputState，默认使用 LaTeX 输入模式
 * 和标准键盘布局。
 *
 * @return 新分配的 MathInputState，失败返回 NULL
 */
MathInputState *math_input_init(void);

/**
 * @brief 销毁数学输入状态
 *
 * 释放所有资源，包括输入缓冲区、历史记录、宏库引用和
 * 缓存的解析/渲染结果。
 *
 * @param state 输入状态（可为 NULL）
 */
void math_input_destroy(MathInputState *state);

/**
 * @brief 设置输入模式
 *
 * 在 LaTeX / 纯文本 / 可视化三种模式间切换。
 * 切换模式时会重新解析当前输入文本。
 *
 * @param state 输入状态
 * @param mode  目标输入模式
 */
void math_input_set_mode(MathInputState *state, MathInputMode mode);

/**
 * @brief 获取当前输入模式
 *
 * @param state 输入状态
 * @return 当前输入模式枚举值
 */
MathInputMode math_input_get_mode(const MathInputState *state);

/* ========================================================================
 * 第七部分：核心 API —— 宏管理
 * ======================================================================== */

/**
 * @brief 向输入状态注册一个自定义宏
 *
 * 注册后，用户输入 \macro_name 时将被自动展开为 expansion。
 * 几何模块调用此函数注册 \point, \line 等宏。
 *
 * @param state       输入状态
 * @param name        宏名称（如 "point"）
 * @param expansion   展开模板（如 "\\text{点 }{#1}"，{#1} 为参数占位符）
 * @param category    宏分类
 * @param arg_count   参数数量
 * @param keyboard_hint 键盘显示提示（如 "Pt"）
 * @return 注册的宏 ID（>= 0），失败返回 -1
 */
int math_input_register_macro(MathInputState *state, const char *name, const char *expansion,
                              MathMacroCategory category, int arg_count, const char *keyboard_hint);

/**
 * @brief 获取所有已注册的宏
 *
 * @param state    输入状态
 * @param out_count 输出：宏的数量
 * @return 宏数组指针（借引用，不释放），state 为 NULL 返回 NULL
 */
const MathMacro *math_input_get_macros(const MathInputState *state, int *out_count);

/**
 * @brief 创建预定义的几何宏库（20+ 预设）
 *
 * 创建并填充以下预设宏：
 *   \point, \line, \circle, \segment, \angle, \triangle,
 *   \parallel, \perp, \cong, \similar, \intersect, \tangent,
 *   \bisector, \midpoint, \distance, \area, \perimeter,
 *   \circumcircle, \incircle, \centroid, \orthocenter,
 *   \collinear, \concurrent
 *
 * @return 新分配的 MathMacroLibrary（填充了所有几何宏），失败返回 NULL
 */
MathMacroLibrary *math_input_create_default_geometry_macros(void);

/**
 * @brief 销毁宏库
 *
 * @param library 宏库（可为 NULL）
 */
void math_input_destroy_macro_library(MathMacroLibrary *library);

/* ========================================================================
 * 第八部分：核心 API —— 命令补全
 * ======================================================================== */

/**
 * @brief 根据输入前缀触发命令补全
 *
 * 从已注册的宏库中查找匹配 prefix 的所有候选宏。
 * 候选按匹配分数（score）降序排列。
 *
 * @param state     输入状态（提供宏库和当前文本）
 * @param prefix    前缀字符串（如 "\\ang" 或 "\\po"）
 * @param out_auto_complete 输出：补全上下文（调用者负责释放）
 * @return true  找到至少一个候选
 *         false 无匹配候选或参数无效
 */
bool math_input_autocomplete(MathInputState *state, const char *prefix, MathAutocomplete **out_auto_complete);

/**
 * @brief 销毁命令补全上下文
 *
 * @param ac 补全上下文（可为 NULL）
 */
void math_input_autocomplete_destroy(MathAutocomplete *ac);

/**
 * @brief 在补全候选列表中向下选择
 *
 * @param ac 补全上下文
 * @return 新选中的索引（0-based），无候选返回 -1
 */
int math_input_autocomplete_next(MathAutocomplete *ac);

/**
 * @brief 在补全候选列表中向上选择
 *
 * @param ac 补全上下文
 * @return 新选中的索引（0-based），无候选返回 -1
 */
int math_input_autocomplete_prev(MathAutocomplete *ac);

/* ========================================================================
 * 第九部分：核心 API —— 渲染与解析
 * ======================================================================== */

/**
 * @brief 将当前输入文本渲染为可显示的字符串
 *
 * LaTeX 模式下：将输入解析为内部表示后生成渲染输出（HTML/MathML）
 * 纯文本模式下：直接返回输入文本
 *
 * @param state 输入状态
 * @return 渲染后的字符串（借引用，随下次调用失效），失败返回 NULL
 */
const char *math_input_render(MathInputState *state);

/**
 * @brief 将当前输入解析为 MathExpr 树
 *
 * 解析输入文本（LaTeX 或纯文本），构建 MathExpr AST。
 * 解析结果缓存于 state->parsed_expr 中。
 *
 * @param state 输入状态
 * @return MathExpr 树（所有权属于 state，调用者不可释放），解析失败返回 NULL
 */
const MathExpr *math_input_parse(MathInputState *state);

/* ========================================================================
 * 第十部分：核心 API —— LaTeX 导入/导出
 * ======================================================================== */

/**
 * @brief 将当前输入导出为 LaTeX 字符串
 *
 * 无论当前输入模式如何，都将编辑器内容导出为规范的 LaTeX 字符串。
 * 展开所有已注册的宏。
 *
 * @param state 输入状态
 * @return 新分配的 LaTeX 字符串（调用者负责 free），失败返回 NULL
 */
char *math_input_export_latex(const MathInputState *state);

/**
 * @brief 从 LaTeX 字符串导入到编辑器
 *
 * 将外部 LaTeX 字符串加载到编辑器，替换当前内容。
 * 自动展开已知的宏。
 *
 * @param state        输入状态
 * @param latex_string 要导入的 LaTeX 字符串
 * @return true 导入成功，false 解析失败
 */
bool math_input_import_latex(MathInputState *state, const char *latex_string);

/* ========================================================================
 * 第十一部分：核心 API —— 键盘布局
 * ======================================================================== */

/**
 * @brief 创建预定义的键盘布局
 *
 * 根据布局类型创建对应的虚拟键盘按钮矩阵。
 * 返回的布局由调用者负责释放（通过 math_input_destroy_keyboard_layout()）。
 *
 * @param layout 键盘布局类型
 * @return 布局描述字符串数组（NULL 终止），失败返回 NULL。
 *         每个字符串为一个按钮的 LaTeX 命令。
 *         例如 KEYBOARD_GEOMETRY 返回：
 *           {"\\point{}", "\\line{}", "\\circle{}", "\\segment{}",
 *            "\\angle{}", "\\triangle{}", "\\parallel", "\\perp",
 *            "\\cong", "\\similar", "\\midpoint{}", "\\distance{}",
 *            "\\area{}", "\\perimeter{}", "\\circumcircle{}",
 *            "\\incircle{}", "\\centroid{}", "\\orthocenter{}",
 *            "\\collinear{}", "\\concurrent{}", NULL}
 */
const char **math_input_create_keyboard_layout(MathKeyboardLayout layout);

/**
 * @brief 销毁键盘布局
 *
 * @param layout 键盘布局数组（可为 NULL）
 */
void math_input_destroy_keyboard_layout(const char **layout);

/**
 * @brief 设置当前活跃的键盘布局
 *
 * 切换虚拟键盘的按钮布局。
 *
 * @param state  输入状态
 * @param layout 目标键盘布局
 */
void math_input_set_keyboard_layout(MathInputState *state, MathKeyboardLayout layout);

/* ========================================================================
 * 第十二部分：核心 API —— 文本编辑与历史
 * ======================================================================== */

/**
 * @brief 在光标位置插入文本
 *
 * 模拟用户输入：在光标位置插入字符，光标前进。
 * 自动触发布局重渲染和命令补全状态更新。
 *
 * @param state 输入状态
 * @param text  要插入的文本
 * @return true 插入成功，false 缓冲区已满
 */
bool math_input_insert_text(MathInputState *state, const char *text);

/**
 * @brief 删除光标前/后的文本
 *
 * @param state    输入状态
 * @param backward true = Backspace（删除光标前），false = Delete（删除光标后）
 * @return true 删除成功，false 无内容可删除
 */
bool math_input_delete(MathInputState *state, bool backward);

/**
 * @brief 撤销上一次编辑（Undo）
 *
 * 恢复到上一次历史快照。
 *
 * @param state 输入状态
 * @return true 撤销成功，false 无更多历史记录
 */
bool math_input_undo(MathInputState *state);

/**
 * @brief 重做已撤销的编辑（Redo）
 *
 * @param state 输入状态
 * @return true 重做成功，false 无更多重做记录
 */
bool math_input_redo(MathInputState *state);

/**
 * @brief 清空编辑器内容
 *
 * 重置所有文本、选区、历史记录和缓存。
 *
 * @param state 输入状态
 */
void math_input_clear(MathInputState *state);

#ifdef __cplusplus
}
#endif

#endif /* LV00_MATH_INPUT_H */
