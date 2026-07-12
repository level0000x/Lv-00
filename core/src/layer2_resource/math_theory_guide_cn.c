/**
 * @file math_theory_guide_cn.c
 * @brief 数学理论指南（中文版）
 *
 * 提供 Lv-00 系统中涉及的数学理论概念的中文说明。
 * 包括几何代数、射影几何、约束求解等核心理论的简明指南。
 *
 * @version 1.0.0
 */

#include <stddef.h>
#include <string.h>

/* ========================================================================
 * 理论主题条目
 * ======================================================================== */

typedef struct {
    const char *topic_id;     /**< 主题标识符 */
    const char *title_cn;     /**< 中文标题 */
    const char *summary_cn;   /**< 中文摘要 */
    const char *keywords_cn;  /**< 中文关键词 */
} MathTheoryEntry;

static const MathTheoryEntry g_theory_table[] = {
    {
        "ga_basics",
        "几何代数基础",
        "几何代数（Geometric Algebra）是一种统一描述几何变换的数学框架。"
        "核心概念包括外积（wedge product）、内积（inner product）和几何积（geometric product）。"
        "在 Lv-00 中，多重向量（multivector）是几何代数的基本数据类型。",
        "多重向量, 叶片, 外积, 内积, 几何积"
    },
    {
        "pga",
        "射影几何代数（PGA）",
        "射影几何代数是几何代数在射影空间中的应用。"
        "在 PGA 中，点、线、面等几何元素统一表示为多重向量的叶片（blade），"
        "相交和连接操作通过外积和对偶操作实现。Lv-00 的约束系统基于 PGA 框架。",
        "射影空间, 叶片, 对偶, 零向量, 无穷远元素"
    },
    {
        "constraint_solving",
        "约束求解理论",
        "约束求解是在给定约束条件下寻找满足条件的变量赋值。"
        "Lv-00 使用符号约束图表示几何约束，通过图重写和代数求解相结合的策略"
        "处理包含度量约束和射影约束的混合系统。",
        "约束图, 约束传播, Gr\u00f6bner 基, 吴消元法"
    },
    {
        "graph_rewrite",
        "图重写理论",
        "图重写是一种基于规则的变换系统，通过模式匹配和替换操作修改图结构。"
        "Lv-00 使用 Weisfeiler-Lehman 图核进行图规范化，"
        "然后应用几何特定的重写规则简化约束系统。",
        "模式匹配, 图规范化, Weisfeiler-Lehman, 归约规则"
    },
    {
        "symbolic_coord",
        "符号坐标系统",
        "Lv-00 的符号坐标系统支持精确的几何计算，避免浮点误差。"
        "坐标类型包括有理数、代数数、二次无理数和超越数。"
        "系统通过分层升级策略（有理数 -> 代数数 -> 二次无理数）管理计算精度。",
        "精确计算, 有理数, 代数数, 二次无理数, 精度升级"
    },
    {
        "proof_system",
        "证明系统理论",
        "Lv-00 的证明系统基于命题逻辑和几何推理的结合。"
        "证明过程包括约束推导、矛盾检测和证明验证三个阶段。"
        "支持前向推理（数据驱动）和后向推理（目标驱动）两种策略。",
        "命题证明, 前向推理, 后向推理, 矛盾检测, 证明验证"
    },
    {
        "unification",
        "合一检查",
        "合一（unification）是判断两个符号表达式是否可以通过变量替换变为相同的过程。"
        "在 Lv-00 中，合一检查用于图匹配和约束等价性判定。"
        "支持结构合一和语义合一两种模式。",
        "合一, 模式匹配, 变量替换, 发生检查"
    },
    {
        "normalization",
        "图归一化理论",
        "图归一化是将图变换为标准形式的过程，消除结构等价的不同表示。"
        "Lv-00 使用基于颜色细化的迭代算法进行图归一化，"
        "合并同构子图以减少求解器的搜索空间。",
        "颜色细化, 同构检测, 标准形式, 迭代算法"
    },
};

#define THEORY_TABLE_SIZE (sizeof(g_theory_table) / sizeof(g_theory_table[0]))

/* ========================================================================
 * 公共 API
 * ======================================================================== */

/**
 * @brief 获取指定主题的中文标题
 * @param topic_id 主题标识符
 * @return 中文标题，未找到返回 NULL
 */
const char *lv00_math_theory_get_title(const char *topic_id)
{
    if (topic_id == NULL) return NULL;

    for (size_t i = 0; i < THEORY_TABLE_SIZE; i++) {
        if (strcmp(g_theory_table[i].topic_id, topic_id) == 0) {
            return g_theory_table[i].title_cn;
        }
    }
    return NULL;
}

/**
 * @brief 获取指定主题的中文摘要
 * @param topic_id 主题标识符
 * @return 中文摘要，未找到返回 NULL
 */
const char *lv00_math_theory_get_summary(const char *topic_id)
{
    if (topic_id == NULL) return NULL;

    for (size_t i = 0; i < THEORY_TABLE_SIZE; i++) {
        if (strcmp(g_theory_table[i].topic_id, topic_id) == 0) {
            return g_theory_table[i].summary_cn;
        }
    }
    return NULL;
}

/**
 * @brief 获取指定主题的中文关键词
 * @param topic_id 主题标识符
 * @return 中文关键词，未找到返回 NULL
 */
const char *lv00_math_theory_get_keywords(const char *topic_id)
{
    if (topic_id == NULL) return NULL;

    for (size_t i = 0; i < THEORY_TABLE_SIZE; i++) {
        if (strcmp(g_theory_table[i].topic_id, topic_id) == 0) {
            return g_theory_table[i].keywords_cn;
        }
    }
    return NULL;
}

/**
 * @brief 获取理论主题的数量
 * @return 主题数量
 */
int lv00_math_theory_count(void)
{
    return (int)THEORY_TABLE_SIZE;
}

/**
 * @brief 按索引获取主题标识符
 * @param index 索引（0 起始）
 * @return 主题标识符，索引越界返回 NULL
 */
const char *lv00_math_theory_get_topic_id(int index)
{
    if (index < 0 || index >= (int)THEORY_TABLE_SIZE) return NULL;
    return g_theory_table[index].topic_id;
}
