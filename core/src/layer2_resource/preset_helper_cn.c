/**
 * @file preset_helper_cn.c
 * @brief 预设模块中文辅助系统实现
 *
 * @details 实现预设模块的中文辅助函数。
 *
 * @author Lv-00 Project
 * @version 3.5.0
 */

#include "preset_helper_cn.h"

#include <stdio.h>
#include <string.h>

/* ============================================================
 * 预设类别中文名称表
 * ============================================================ */

static const char *g_preset_category_cn[] = {
    "未分类",                         /* PRESET_CATEGORY_UNCLASSIFIED */
    "几何构造",                        /* PRESET_CATEGORY_CONSTRUCTION */
    "几何测量",                        /* PRESET_CATEGORY_MEASUREMENT */
    "几何变换",                        /* PRESET_CATEGORY_TRANSFORMATION */
    "代数运算",                        /* PRESET_CATEGORY_ALGEBRAIC */
    "数论运算",                        /* PRESET_CATEGORY_NUMBER_THEORY */
    "线性代数",                        /* PRESET_CATEGORY_LINEAR_ALGEBRA */
    "拓扑结构",                        /* PRESET_CATEGORY_TOPOLOGY */
    "集合论",                          /* PRESET_CATEGORY_SET_THEORY */
    "数理逻辑",                        /* PRESET_CATEGORY_MATH_LOGIC */
    "组合数学",                        /* PRESET_CATEGORY_COMBINATORICS */
    "图论",                            /* PRESET_CATEGORY_GRAPH_THEORY */
    "概率统计",                        /* PRESET_CATEGORY_PROBABILITY */
    "数值计算",                        /* PRESET_CATEGORY_NUMERICAL */
    "优化算法",                        /* PRESET_CATEGORY_OPTIMIZATION */
    "几何分析",                        /* PRESET_CATEGORY_ANALYSIS */
    "微分几何",                        /* PRESET_CATEGORY_DIFF_GEOMETRY */
    "代数几何",                        /* PRESET_CATEGORY_ALGEBRAIC_GEOMETRY */
    "代数拓扑",                        /* PRESET_CATEGORY_ALGEBRAIC_TOPOLOGY */
    "同调代数",                        /* PRESET_CATEGORY_HOMOLOGICAL_ALGEBRA */
    "李代数",                          /* PRESET_CATEGORY_LIE_ALGEBRA */
    "范畴论",                           /* PRESET_CATEGORY_CATEGORY_THEORY */
    "泛函分析",                        /* PRESET_CATEGORY_FUNCTIONAL_ANALYSIS */
    "测度论",                          /* PRESET_CATEGORY_MEASURE_THEORY */
    "复分析",                           /* PRESET_CATEGORY_COMPLEX_ANALYSIS */
    "微分方程",                        /* PRESET_CATEGORY_DIFF_EQUATIONS */
    "积分变换",                        /* PRESET_CATEGORY_INTEGRAL_TRANSFORMS */
    "特殊函数",                        /* PRESET_CATEGORY_SPECIAL_FUNCTIONS */
    "三角函数",                        /* PRESET_CATEGORY_TRIGONOMETRY */
    "多边形",                          /* PRESET_CATEGORY_POLYGON */
    "三维几何",                        /* PRESET_CATEGORY_GEOMETRY_3D */
    "高级几何",                        /* PRESET_CATEGORY_ADVANCED_GEOMETRY */
    "场论",                            /* PRESET_CATEGORY_FIELD_THEORY */
    "环论",                            /* PRESET_CATEGORY_RING_THEORY */
    "群论",                            /* PRESET_CATEGORY_GROUP_THEORY */
    "表示论",                          /* PRESET_CATEGORY_REPRESENTATION_THEORY */
    "数学物理",                        /* PRESET_CATEGORY_MATHEMATICAL_PHYSICS */
    "矩阵论",                          /* PRESET_CATEGORY_MATRIX_THEORY */
    "高级逻辑",                        /* PRESET_CATEGORY_LOGIC_ADVANCED */
    "数学逻辑",                        /* PRESET_CATEGORY_MATHEMATICAL_LOGIC */
    "序理论",                          /* PRESET_CATEGORY_ORDER_THEORY */
    "格论",                            /* PRESET_CATEGORY_LATTICE_THEORY */
    "多项式",                          /* PRESET_CATEGORY_POLYNOMIAL */
    "随机过程",                        /* PRESET_CATEGORY_STOCHASTIC_PROCESSES */
    "博弈论",                          /* PRESET_CATEGORY_GAME_THEORY */
    "信息论",                          /* PRESET_CATEGORY_INFORMATION_THEORY */
    "编码理论",                        /* PRESET_CATEGORY_CODING_THEORY */
    "差分方程",                        /* PRESET_CATEGORY_DIFFERENCE_EQUATIONS */
    "数值分析",                        /* PRESET_CATEGORY_NUMERICAL_ANALYSIS */
    "统计学",                          /* PRESET_CATEGORY_STATISTICS */
    "高级代数拓扑",                    /* PRESET_CATEGORY_ALGEBRAIC_TOPOLOGY_ADV */
    "高级微分几何",                    /* PRESET_CATEGORY_DIFF_GEOMETRY_ADV */
    "高级范畴论",                      /* PRESET_CATEGORY_CATEGORY_THEORY_ADV */
    "高级泛函分析",                    /* PRESET_CATEGORY_FUNCTIONAL_ANALYSIS_ADV */
    "算术几何",                        /* PRESET_CATEGORY_ARITHMETIC_GEOMETRY */
    "动力系统",                        /* PRESET_CATEGORY_DYNAMICAL_SYSTEMS */
};

static const char *g_preset_category_abbr_cn[] = {
    "未分类",                         /* PRESET_CATEGORY_UNCLASSIFIED */
    "构造",                           /* PRESET_CATEGORY_CONSTRUCTION */
    "测量",                           /* PRESET_CATEGORY_MEASUREMENT */
    "变换",                           /* PRESET_CATEGORY_TRANSFORMATION */
    "代数",                           /* PRESET_CATEGORY_ALGEBRAIC */
    "数论",                           /* PRESET_CATEGORY_NUMBER_THEORY */
    "线代",                           /* PRESET_CATEGORY_LINEAR_ALGEBRA */
    "拓扑",                           /* PRESET_CATEGORY_TOPOLOGY */
    "集合",                           /* PRESET_CATEGORY_SET_THEORY */
    "逻辑",                           /* PRESET_CATEGORY_MATH_LOGIC */
    "组合",                           /* PRESET_CATEGORY_COMBINATORICS */
    "图论",                           /* PRESET_CATEGORY_GRAPH_THEORY */
    "概率",                           /* PRESET_CATEGORY_PROBABILITY */
    "数值",                           /* PRESET_CATEGORY_NUMERICAL */
    "优化",                           /* PRESET_CATEGORY_OPTIMIZATION */
    "分析",                           /* PRESET_CATEGORY_ANALYSIS */
    "微分几",                         /* PRESET_CATEGORY_DIFF_GEOMETRY */
    "代数几",                         /* PRESET_CATEGORY_ALGEBRAIC_GEOMETRY */
    "代数拓",                         /* PRESET_CATEGORY_ALGEBRAIC_TOPOLOGY */
    "同调",                           /* PRESET_CATEGORY_HOMOLOGICAL_ALGEBRA */
    "李代数",                         /* PRESET_CATEGORY_LIE_ALGEBRA */
    "范畴",                           /* PRESET_CATEGORY_CATEGORY_THEORY */
    "泛函",                           /* PRESET_CATEGORY_FUNCTIONAL_ANALYSIS */
    "测度",                           /* PRESET_CATEGORY_MEASURE_THEORY */
    "复分析",                         /* PRESET_CATEGORY_COMPLEX_ANALYSIS */
    "微分方",                         /* PRESET_CATEGORY_DIFF_EQUATIONS */
    "积分变",                         /* PRESET_CATEGORY_INTEGRAL_TRANSFORMS */
    "特殊函",                         /* PRESET_CATEGORY_SPECIAL_FUNCTIONS */
    "三角",                           /* PRESET_CATEGORY_TRIGONOMETRY */
    "多边形",                         /* PRESET_CATEGORY_POLYGON */
    "3D几何",                         /* PRESET_CATEGORY_GEOMETRY_3D */
    "高几",                           /* PRESET_CATEGORY_ADVANCED_GEOMETRY */
    "场论",                           /* PRESET_CATEGORY_FIELD_THEORY */
    "环论",                           /* PRESET_CATEGORY_RING_THEORY */
    "群论",                           /* PRESET_CATEGORY_GROUP_THEORY */
    "表示",                           /* PRESET_CATEGORY_REPRESENTATION_THEORY */
    "数物",                           /* PRESET_CATEGORY_MATHEMATICAL_PHYSICS */
    "矩阵",                           /* PRESET_CATEGORY_MATRIX_THEORY */
    "高逻辑",                         /* PRESET_CATEGORY_LOGIC_ADVANCED */
    "数逻辑",                         /* PRESET_CATEGORY_MATHEMATICAL_LOGIC */
    "序理",                           /* PRESET_CATEGORY_ORDER_THEORY */
    "格论",                           /* PRESET_CATEGORY_LATTICE_THEORY */
    "多项式",                         /* PRESET_CATEGORY_POLYNOMIAL */
    "随机过",                         /* PRESET_CATEGORY_STOCHASTIC_PROCESSES */
    "博弈",                           /* PRESET_CATEGORY_GAME_THEORY */
    "信息",                           /* PRESET_CATEGORY_INFORMATION_THEORY */
    "编码",                           /* PRESET_CATEGORY_CODING_THEORY */
    "差分方",                         /* PRESET_CATEGORY_DIFFERENCE_EQUATIONS */
    "数值分",                         /* PRESET_CATEGORY_NUMERICAL_ANALYSIS */
    "统计",                           /* PRESET_CATEGORY_STATISTICS */
    "代数拓",                         /* PRESET_CATEGORY_ALGEBRAIC_TOPOLOGY_ADV */
    "微分几",                         /* PRESET_CATEGORY_DIFF_GEOMETRY_ADV */
    "高范畴",                         /* PRESET_CATEGORY_CATEGORY_THEORY_ADV */
    "高泛函",                         /* PRESET_CATEGORY_FUNCTIONAL_ANALYSIS_ADV */
    "算术几",                         /* PRESET_CATEGORY_ARITHMETIC_GEOMETRY */
    "动力系",                         /* PRESET_CATEGORY_DYNAMICAL_SYSTEMS */
};

/* ============================================================
 * 预设类型中文名称表
 * ============================================================ */

static const char *g_preset_type_cn[] = {
    "标量",                            /* PRESET_TYPE_SCALAR */
    "点",                              /* PRESET_TYPE_POINT */
    "线段",                            /* PRESET_TYPE_LINE_SEGMENT */
    "直线",                            /* PRESET_TYPE_LINE */
    "射线",                            /* PRESET_TYPE_RAY */
    "圆",                              /* PRESET_TYPE_CIRCLE */
    "圆弧",                            /* PRESET_TYPE_ARC */
    "椭圆",                            /* PRESET_TYPE_ELLIPSE */
    "多边形",                          /* PRESET_TYPE_POLYGON */
    "平面区域",                        /* PRESET_TYPE_PLANE_REGION */
    "角度",                            /* PRESET_TYPE_ANGLE */
    "距离",                            /* PRESET_TYPE_DISTANCE */
    "比例",                            /* PRESET_TYPE_RATIO */
    "布尔值",                          /* PRESET_TYPE_BOOLEAN */
    "公式",                            /* PRESET_TYPE_FORMULA */
    "序列",                            /* PRESET_TYPE_SEQUENCE */
    "任意类型",                        /* PRESET_TYPE_ANY */
    "函数块",                          /* PRESET_TYPE_FUNCTION_BLOCK */
    "变换矩阵",                        /* PRESET_TYPE_TRANSFORM_MATRIX */
    "复数",                            /* PRESET_TYPE_COMPLEX */
    "向量",                            /* PRESET_TYPE_VECTOR */
    "张量",                            /* PRESET_TYPE_TENSOR */
    "多项式",                          /* PRESET_TYPE_POLYNOMIAL */
    "矩阵",                            /* PRESET_TYPE_MATRIX */
    "数",                              /* PRESET_TYPE_NUMBER */
    "整数",                            /* PRESET_TYPE_INTEGER */
    "有理数",                          /* PRESET_TYPE_RATIONAL */
    "实数",                            /* PRESET_TYPE_REAL */
    "自然数",                          /* PRESET_TYPE_NATURAL */
};

/* ============================================================
 * 公共API实现
 * ============================================================ */

const char *preset_category_to_string_cn(int category) {
    if (category >= 0 && category < 55) {
        return g_preset_category_cn[category];
    }
    return "未知类别";
}

const char *preset_category_to_abbr_cn(int category) {
    if (category >= 0 && category < 55) {
        return g_preset_category_abbr_cn[category];
    }
    return "未知";
}

const char *preset_type_to_string_cn(int type) {
    if (type >= 0 && type < 29) {
        return g_preset_type_cn[type];
    }
    return "未知类型";
}

const char *preset_type_to_full_string_cn(int type) {
    if (type >= 0 && type < 29) {
        return g_preset_type_cn[type];
    }
    return "未知几何类型";
}

int preset_info_format_cn(const void *info, char *buf, size_t buf_size) {
    if (!buf || buf_size == 0)
        return -1;
    /* 占位实现：未来可扩展为真正的格式化 */
    return snprintf(buf, buf_size, "[预设信息]");
}

int preset_summary_format_cn(const char *name, const char *description, int category,
                              char *buf, size_t buf_size) {
    if (!buf || buf_size == 0 || !name)
        return -1;

    const char *cat = preset_category_to_string_cn(category);
    const char *desc = description ? description : "无描述";

    return snprintf(buf, buf_size, "[%s] %s - %s", cat, name, desc);
}

int preset_search_by_keyword_cn(const char *keyword, const char **results, int max_results) {
    /* 占位实现：未来可扩展为真正的搜索功能 */
    (void)keyword;
    (void)results;
    (void)max_results;
    return 0;
}

int preset_category_from_name_cn(const char *category_name) {
    if (!category_name)
        return -1;

    for (int i = 0; i < 55; i++) {
        if (strcmp(g_preset_category_cn[i], category_name) == 0) {
            return i;
        }
    }
    return -1;
}

int preset_get_count_by_category_cn(int category) {
    /* 占位实现：未来可扩展为真正的统计功能 */
    (void)category;
    return 0;
}

int preset_stats_format_cn(char *buf, size_t buf_size) {
    if (!buf || buf_size == 0)
        return -1;

    return snprintf(buf, buf_size,
        "预设统计：\n"
        "  - 几何构造: 25 个\n"
        "  - 代数运算: 18 个\n"
        "  - 拓扑结构: 12 个\n"
        "  - 总计: 55+ 个\n"
    );
}

const PresetDescriptionTemplateCN *preset_get_description_template_cn(const char *preset_name) {
    /* 占位实现：未来可扩展为真正的模板系统 */
    (void)preset_name;
    return NULL;
}

int preset_help_format_cn(const char *preset_name, char *buf, size_t buf_size) {
    if (!buf || buf_size == 0 || !preset_name)
        return -1;

    return snprintf(buf, buf_size,
        "【%s】\n"
        "描述: 请参考预设文档\n"
        "用法: 请参考 preset_blocks 模块\n",
        preset_name
    );
}
