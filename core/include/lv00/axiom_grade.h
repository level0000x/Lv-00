/**
 * @file axiom_grade.h
 * @brief 公理分级系统 —— 按教育难度分级、证明风格分类与公理过滤器
 *
 * 提供公理按难度等级（基础/中级/高级/专家）进行分类的能力，
 * 以及按证明风格（正向/反向/反证/归纳）进行标记和筛选。
 * 支持在教学场景中按用户水平自适应切换公理集合。
 */

#ifndef LV00_AXIOM_GRADE_H
#define LV00_AXIOM_GRADE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

/* ============== 公理难度等级 ============== */

/**
 * @brief 公理难度等级 —— 用于教育场景下的自适应难度筛选
 *
 * 四个等级对应从初学到专家的递进难度：
 * - GRADE_BASIC:       基础公理，如欧几里得几何基本公设、自然数归纳法基例
 * - GRADE_INTERMEDIATE: 中级公理，如群论基本性质、线代秩-零度定理
 * - GRADE_ADVANCED:     高级公理，如Zorn引理、代数闭域存在性
 * - GRADE_EXPERT:       专家级公理，如大基数公理、V=L、选择公理的精细等价形式
 */
typedef enum {
    GRADE_BASIC       = 0,  /**< 基础级 —— 适用于初学者 */
    GRADE_INTERMEDIATE = 1,  /**< 中级 —— 适用于有一定基础的学习者 */
    GRADE_ADVANCED     = 2,  /**< 高级 —— 适用于进阶学习者 */
    GRADE_EXPERT       = 3   /**< 专家级 —— 适用于研究级用户 */
} Lv00AxiomGrade;

/* ============== 证明风格 ============== */

/**
 * @brief 证明风格 —— 描述公理/定理最自然的证明方法
 *
 * 四种基本证明风格：
 * - STYLE_FORWARD:      正向推理（从前提到结论的直接推导）
 * - STYLE_BACKWARD:     反向推理（从结论出发倒推前提）
 * - STYLE_CONTRADICTION: 反证法（假设结论为假，导出矛盾）
 * - STYLE_INDUCTION:     归纳法（基于良基关系的归纳证明）
 */
typedef enum {
    STYLE_FORWARD       = 0,  /**< 正向推理法 */
    STYLE_BACKWARD      = 1,  /**< 反向推理法 */
    STYLE_CONTRADICTION = 2,  /**< 反证法（归谬法） */
    STYLE_INDUCTION     = 3   /**< 归纳法（数学归纳/良基归纳） */
} Lv00ProofStyle;

/* ============== 带分级的公理元数据 ============== */

/**
 * @brief 公理分级元数据 —— 附加在每个公理上的难度和风格信息
 *
 * 该结构体可通过在 AxiomPackage 中添加 axiom_grades 数组来与现有公理
 * 关联。每个公理通过其索引或名称映射到对应的 Lv00AxiomGradeMeta。
 */
typedef struct {
    char   axiom_name[128];    /**< 公理名称（与 ConstraintTemplate 的 name 对应） */
    Lv00AxiomGrade grade;      /**< 难度等级 */
    Lv00ProofStyle style;      /**< 推荐证明风格 */
    int    prerequisite_count; /**< 前置公理数量（用于拓扑排序教学顺序） */
    char  *description;        /**< 教学性描述文本（可为 NULL） */
    bool   is_required;        /**< 是否为必修公理（不受难度筛选影响） */
} Lv00AxiomGradeMeta;

/* ============== 难度过滤器 ============== */

/**
 * @brief 公理难度过滤器 —— 控制当前可见的公理等级范围
 *
 * 通过设置 min_grade 和 max_grade 来限定可用公理的范围。
 * 例如设置 max_grade = GRADE_INTERMEDIATE 则只允许基础和中级公理。
 */
typedef struct {
    Lv00AxiomGrade min_grade;        /**< 最低允许难度（含） */
    Lv00AxiomGrade max_grade;        /**< 最高允许难度（含） */
    bool           filter_enabled;   /**< 是否启用过滤 */
    int            current_level;    /**< 当前难度级别（0-based，递增解锁） */
} Lv00AxiomGradeFilter;

/* ============== API ============== */

/**
 * @brief 设置公理难度过滤器的上限
 *
 * 只允许 grade <= max_grade 的公理通过筛选。
 * 必修公理（is_required == true）不受此限制影响。
 *
 * @param grade  允许的最高难度等级
 */
void lv00_axiom_set_difficulty(Lv00AxiomGrade grade);

/**
 * @brief 获取当前全局难度过滤器的配置
 *
 * @return 当前难度过滤器指针（只读），可能为 NULL
 */
const Lv00AxiomGradeFilter *lv00_axiom_get_filter(void);

/**
 * @brief 创建公理分级元数据
 *
 * @param name        公理名称
 * @param grade       难度等级
 * @param style       推荐证明风格
 * @param description 教学描述（可为 NULL，内部会复制）
 * @return 新分配的公理分级元数据，失败返回 NULL
 */
Lv00AxiomGradeMeta *lv00_axiom_grade_meta_create(const char *name, Lv00AxiomGrade grade,
                                                  Lv00ProofStyle style, const char *description);

/**
 * @brief 销毁公理分级元数据
 *
 * @param meta  公理分级元数据指针（可为 NULL）
 */
void lv00_axiom_grade_meta_destroy(Lv00AxiomGradeMeta *meta);

/**
 * @brief 检查给定公理是否通过当前难度筛选
 *
 * @param meta  公理分级元数据
 * @return true  通过筛选（允许使用该公理）
 * @return false 未通过筛选（该公理被屏蔽）
 */
bool lv00_axiom_grade_check(const Lv00AxiomGradeMeta *meta);

/**
 * @brief 将难度等级转换为中文字符串
 *
 * @param grade  难度等级
 * @return 中文描述字符串（静态内存，不可释放）
 */
const char *lv00_axiom_grade_to_string(Lv00AxiomGrade grade);

/**
 * @brief 将证明风格转换为中文字符串
 *
 * @param style  证明风格
 * @return 中文描述字符串（静态内存，不可释放）
 */
const char *lv00_proof_style_to_string(Lv00ProofStyle style);

/**
 * @brief 递进解锁下一个难度等级
 *
 * 将当前级别 +1，相当于"通关当前难度后解锁下一级"。
 * 如果已到达最高级（GRADE_EXPERT），则不变化。
 *
 * @return 解锁后的新难度等级
 */
Lv00AxiomGrade lv00_axiom_unlock_next_grade(void);

/**
 * @brief 按证明风格筛选公理
 *
 * 遍历公理分级元数据数组，筛选出匹配指定证明风格的公理，
 * 将匹配的索引写入 out_indices。
 *
 * @param metas         公理元数据数组
 * @param meta_count    元数据数量
 * @param style         目标证明风格
 * @param out_indices   输出：匹配公理的索引数组（调用者分配，至少meta_count个元素）
 * @param max_out       输出数组最大容量
 * @return 实际匹配的公理数量
 */
int lv00_axiom_filter_by_style(const Lv00AxiomGradeMeta *metas, int meta_count,
                                Lv00ProofStyle style, int *out_indices, int max_out);

#ifdef __cplusplus
}
#endif

#endif /* LV00_AXIOM_GRADE_H */
