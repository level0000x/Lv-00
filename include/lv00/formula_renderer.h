/**
 * @file formula_renderer.h
 * @brief 公式渲染器 —— 将 AST 渲染为 LaTeX / Python / DSL 字符串
 *
 * @details 提供三种输出格式的渲染功能：
 *          - LaTeX: 用于数学排版和显示
 *          - Python: 用于数值计算和代码生成
 *          - DSL: 用于 Lv-00 几何元语言系统
 *
 * @author Lv-00 Project
 * @version 3.2.0
 */

#ifndef LV00_FORMULA_RENDERER_H
#define LV00_FORMULA_RENDERER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "formula_parser.h"

/* ============================================================
 * 渲染输出格式
 * ============================================================ */

typedef enum {
    OUTPUT_LATEX,  /* LaTeX 格式 */
    OUTPUT_PYTHON, /* Python 代码格式 */
    OUTPUT_DSL     /* Lv-00 DSL 格式 */
} OutputFormat;

/* ============================================================
 * 渲染选项
 * ============================================================ */

typedef struct {
    bool implicit_multiplication; /* LaTeX: 隐式乘法 (ab 而非 a \cdot b) */
    bool display_mode;            /* LaTeX: 显示模式 (独立行) */
    bool fraction_mode;           /* Python: 分数模式 (Fraction) 或浮点数 */
    bool simplify_output;         /* 是否简化输出 */
    int precision;                /* 浮点数精度 */
} RenderOptions;

/* 默认渲染选项 */
#define RENDER_OPTIONS_DEFAULT {false, true, true, true, 6}

/* ============================================================
 * 核心 API
 * ============================================================ */

/**
 * @brief 将 AST 渲染为字符串（使用默认选项）
 * @param[in] node   AST 节点
 * @param[in] format 输出格式
 * @return 渲染后的字符串，调用者负责释放；失败返回 NULL
 */
char *formula_render(const FormulaNode *node, OutputFormat format);

/**
 * @brief 将 AST 渲染为字符串（带自定义选项）
 * @param[in] node    AST 节点
 * @param[in] format  输出格式
 * @param[in] options 渲染选项，为 NULL 时使用默认选项
 * @return 渲染后的字符串，调用者负责释放；失败返回 NULL
 */
char *formula_render_ex(const FormulaNode *node, OutputFormat format, const RenderOptions *options);

/**
 * @brief 将 AST 渲染到已有缓冲区（使用默认选项）
 * @param[in]  node   AST 节点
 * @param[in]  format 输出格式
 * @param[out] buffer 目标缓冲区
 * @param[in]  size   缓冲区大小（字节数）
 * @return 实际写入的字符数（不含终止符），失败返回负数
 */
int formula_render_to_buffer(const FormulaNode *node, OutputFormat format, char *buffer, size_t size);

/**
 * @brief 将 AST 渲染到已有缓冲区（带自定义选项）
 * @param[in]  node    AST 节点
 * @param[in]  format  输出格式
 * @param[in]  options 渲染选项，为 NULL 时使用默认选项
 * @param[out] buffer  目标缓冲区
 * @param[in]  size    缓冲区大小（字节数）
 * @return 实际写入的字符数（不含终止符），失败返回负数
 */
int formula_render_to_buffer_ex(const FormulaNode *node, OutputFormat format, const RenderOptions *options,
                                char *buffer, size_t size);

/**
 * @brief 获取最后一次渲染错误信息
 * @return 错误信息字符串（线程局部存储，无需释放）
 */
const char *formula_render_get_last_error(void);

/* ============================================================
 * 格式特定 API
 * ============================================================ */

/**
 * @brief 渲染为 LaTeX 格式
 * @param[in] node AST 节点
 * @return LaTeX 字符串，调用者负责释放
 */
char *formula_render_latex(const FormulaNode *node);

/**
 * @brief 渲染为 Python 格式
 * @param[in] node AST 节点
 * @return Python 代码字符串，调用者负责释放
 */
char *formula_render_python(const FormulaNode *node);

/**
 * @brief 渲染为 DSL 格式
 * @param[in] node AST 节点
 * @return DSL 字符串，调用者负责释放
 */
char *formula_render_dsl(const FormulaNode *node);

/* ============================================================
 * 特殊渲染函数
 * ============================================================ */

/**
 * @brief 渲染几何点为 LaTeX
 * @param[in] name       点名
 * @param[in] coords     坐标数组
 * @param[in] coord_count 坐标数量
 * @return LaTeX 字符串，如 "A = \\left(1, 2\\right)"，调用者负责释放
 */
char *formula_render_point_latex(const char *name, const FormulaNode **coords, int coord_count);

/**
 * @brief 渲染几何线段为 LaTeX
 * @param[in] name 线段名
 * @return LaTeX 字符串，如 "\\overline{AB}"，调用者负责释放
 */
char *formula_render_segment_latex(const char *name);

/**
 * @brief 渲染几何圆为 LaTeX
 * @param[in] name   圆名
 * @param[in] center 圆心点名
 * @param[in] radius 半径表达式
 * @return LaTeX 字符串，调用者负责释放
 */
char *formula_render_circle_latex(const char *name, const char *center, const FormulaNode *radius);

/**
 * @brief 渲染分数为 LaTeX
 * @param[in] numerator   分子
 * @param[in] denominator 分母
 * @return LaTeX 字符串，如 "\\frac{3}{4}"，调用者负责释放
 */
char *formula_render_fraction_latex(int64_t numerator, uint64_t denominator);

/**
 * @brief 查找希腊字母对应的 LaTeX 命令
 * @param[in] name 变量名（如 "theta", "pi"）
 * @return LaTeX 命令字符串（如 "\\theta", "\\pi"），未找到则返回原始名称
 */
const char *formula_latex_greek_name(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* LV00_FORMULA_RENDERER_H */
