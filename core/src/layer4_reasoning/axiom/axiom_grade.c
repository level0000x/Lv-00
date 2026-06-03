/**
 * @file axiom_grade.c
 * @brief 公理分级系统实现 —— 难度过滤器、风格筛选与级进解锁
 *
 * @details 实现全局难度过滤器的设置与查询，提供公理分级元数据的
 *          创建/销毁/检查机制。支持按证明风格批量筛选公理，
 *          以及基于"通关解锁"的教育级进模式。
 *
 *          核心功能模块：
 *          - 全局难度过滤器：单例过滤器控制当前可见的公理等级
 *          - 公理分级元数据：为每个公理附加难度、风格和教学描述
 *          - 风格筛选：按证明风格（正向/反向/反证/归纳）批量选择公理
 *          - 级进解锁：模拟教育场景中难度逐步解锁的机制
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

#include "axiom_grade.h"

#include <string.h>

#include "lv00_utils.h"
#include "lv00_internal.h"

/* ============== 全局单例难度过滤器 ============== */
/* 线程局部存储：每个线程可有独立的难度过滤器 */
#ifdef LV00_THREAD_LOCAL
static LV00_THREAD_LOCAL Lv00AxiomGradeFilter g_grade_filter = {
    GRADE_BASIC,     /* min_grade */
    GRADE_INTERMEDIATE, /* max_grade 默认中级 */
    true,            /* filter_enabled 默认启用 */
    1                /* current_level 默认为1（已解锁中级） */
};
#else
static Lv00AxiomGradeFilter g_grade_filter = {
    GRADE_BASIC,
    GRADE_INTERMEDIATE,
    true,
    1
};
#endif

/* ============== 难度过滤器 API ============== */

/**
 * @brief 设置公理难度过滤器的上限
 *
 * 将全局过滤器的 max_grade 设置为指定等级，
 * 同时确保 min_grade <= max_grade，保持过滤器一致性。
 * 如果传入的 grade 低于当前 min_grade，则同步降低 min_grade。
 *
 * @param grade  允许的最高难度等级
 */
void lv00_axiom_set_difficulty(Lv00AxiomGrade grade) {
    g_grade_filter.max_grade = grade;
    /* 保持过滤器一致性：min 不能超过 max */
    if (g_grade_filter.min_grade > grade) {
        g_grade_filter.min_grade = grade;
    }
    g_grade_filter.filter_enabled = true;
}

/**
 * @brief 获取当前全局难度过滤器的配置
 *
 * @return 当前难度过滤器指针（只读），指向全局单例
 */
const Lv00AxiomGradeFilter *lv00_axiom_get_filter(void) {
    return &g_grade_filter;
}

/* ============== 公理分级元数据 API ============== */

/**
 * @brief 创建公理分级元数据
 *
 * 分配并初始化 Lv00AxiomGradeMeta，复制公理名称和描述。
 * 默认 prerequisite_count = 0，is_required = false。
 *
 * @param name        公理名称（不可为空）
 * @param grade       难度等级
 * @param style       推荐证明风格
 * @param description 教学描述（可为 NULL）
 * @return 新分配的公理分级元数据，失败返回 NULL
 */
Lv00AxiomGradeMeta *lv00_axiom_grade_meta_create(const char *name, Lv00AxiomGrade grade,
                                                  Lv00ProofStyle style, const char *description) {
    if (!name)
        return NULL;

    Lv00AxiomGradeMeta *meta = lv00_calloc(1, sizeof(Lv00AxiomGradeMeta));
    if (!meta)
        return NULL;

    /* 安全地复制公理名称（最多 127 字符 + 空终止符） */
    size_t name_len = strlen(name);
    if (name_len >= sizeof(meta->axiom_name)) {
        name_len = sizeof(meta->axiom_name) - 1;
    }
    memcpy(meta->axiom_name, name, name_len);
    meta->axiom_name[name_len] = '\0';

    meta->grade = grade;
    meta->style = style;
    meta->prerequisite_count = 0;
    meta->is_required = false;

    if (description) {
        meta->description = lv00_strdup(description);
    }

    return meta;
}

/**
 * @brief 销毁公理分级元数据
 *
 * 释放 description 字符串和结构体本身。
 *
 * @param meta  公理分级元数据指针（可为 NULL）
 */
void lv00_axiom_grade_meta_destroy(Lv00AxiomGradeMeta *meta) {
    if (!meta)
        return;
    lv00_free((void **) &meta->description);
    lv00_free((void **) &meta);
}

/**
 * @brief 检查给定公理是否通过当前难度筛选
 *
 * 筛选规则：
 * 1. 如果过滤器未启用，所有公理都通过。
 * 2. 如果公理为必修（is_required），始终通过。
 * 3. 否则检查 grade 是否在 [min_grade, max_grade] 范围内。
 *
 * @param meta  公理分级元数据（可为 NULL，NULL 视为不通过）
 * @return true  通过筛选
 */
bool lv00_axiom_grade_check(const Lv00AxiomGradeMeta *meta) {
    if (!meta)
        return false;

    /* 过滤器关闭时，所有公理都可用 */
    if (!g_grade_filter.filter_enabled)
        return true;

    /* 必修公理不受难度限制 */
    if (meta->is_required)
        return true;

    /* 检查难度等级是否在允许范围内 */
    return (meta->grade >= g_grade_filter.min_grade &&
            meta->grade <= g_grade_filter.max_grade);
}

/* ============== 字符串转换 ============== */

/**
 * @brief 将难度等级转换为中文字符串
 *
 * @param grade  难度等级
 * @return 中文描述字符串（静态内存）
 */
const char *lv00_axiom_grade_to_string(Lv00AxiomGrade grade) {
    switch (grade) {
        case GRADE_BASIC:       return "基础级";
        case GRADE_INTERMEDIATE: return "中级";
        case GRADE_ADVANCED:     return "高级";
        case GRADE_EXPERT:       return "专家级";
        default:                 return "未知等级";
    }
}

/**
 * @brief 将证明风格转换为中文字符串
 *
 * @param style  证明风格
 * @return 中文描述字符串（静态内存）
 */
const char *lv00_proof_style_to_string(Lv00ProofStyle style) {
    switch (style) {
        case STYLE_FORWARD:       return "正向推理";
        case STYLE_BACKWARD:      return "反向推理";
        case STYLE_CONTRADICTION: return "反证法（归谬法）";
        case STYLE_INDUCTION:     return "归纳法";
        default:                  return "未知风格";
    }
}

/* ============== 级进解锁 ============== */

/**
 * @brief 递进解锁下一个难度等级
 *
 * 将 current_level 递增，同时更新 max_grade。
 * GRADE_BASIC->GRADE_INTERMEDIATE->GRADE_ADVANCED->GRADE_EXPERT。
 * 到达 GRADE_EXPERT 后不再变化。
 *
 * @return 解锁后的新难度等级
 */
Lv00AxiomGrade lv00_axiom_unlock_next_grade(void) {
    if (g_grade_filter.current_level < (int) GRADE_EXPERT) {
        g_grade_filter.current_level++;
    }
    /* 将 max_grade 同步为当前解锁的最高等级 */
    g_grade_filter.max_grade = (Lv00AxiomGrade) g_grade_filter.current_level;
    return g_grade_filter.max_grade;
}

/* ============== 风格筛选 ============== */

/**
 * @brief 按证明风格筛选公理
 *
 * 遍历公理分级元数据数组，将匹配指定证明风格的公理索引写入输出数组。
 * 如果输出数组容量不足，只写入前 max_out 个匹配项。
 *
 * @param metas         公理元数据数组
 * @param meta_count    元数据数量
 * @param style         目标证明风格
 * @param out_indices   输出：匹配公理的索引数组
 * @param max_out       输出数组最大容量
 * @return 实际匹配的公理数量（可能大于 max_out）
 */
int lv00_axiom_filter_by_style(const Lv00AxiomGradeMeta *metas, int meta_count,
                                Lv00ProofStyle style, int *out_indices, int max_out) {
    if (!metas || meta_count <= 0 || !out_indices || max_out <= 0)
        return 0;

    int matched = 0;
    for (int i = 0; i < meta_count; i++) {
        if (metas[i].style == style) {
            if (matched < max_out) {
                out_indices[matched] = i;
            }
            matched++;
        }
    }
    return matched;
}
