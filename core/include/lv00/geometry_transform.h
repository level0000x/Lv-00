/**
 * @file geometry_transform.h
 * @brief 几何变换推理系统 —— 旋转、轴对称、平移
 *
 * @details 提供三种基本几何变换的符号推理：
 *   1. 旋转（Rotation）：绕指定中心旋转指定角度
 *   2. 轴对称（Reflection）：关于指定直线的对称
 *   3. 平移（Translation）：沿指定向量平移
 *
 * 支持的功能：
 *   - 变换矩阵的符号计算（纯整数/有理数）
 *   - 变换下的约束保持性验证
 *   - 变换序列的复合
 *   - 不动点分析
 *   - 变换群结构分析
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#ifndef LV00_GEOMETRY_TRANSFORM_H
#define LV00_GEOMETRY_TRANSFORM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <gmp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "constraint_graph.h"
#include "symbolic_coord.h"

/* ============== 前向声明 ============== */

typedef struct Lv00Transform Lv00Transform;
typedef struct Lv00TransformSequence Lv00TransformSequence;
typedef struct Lv00TransformGroup Lv00TransformGroup;

/* ============== 变换类型 ============== */

/**
 * @brief 变换类型枚举
 */
typedef enum {
    TRANSFORM_IDENTITY,     /**< 恒等变换 */
    TRANSFORM_TRANSLATION,  /**< 平移 */
    TRANSFORM_ROTATION,     /**< 旋转 */
    TRANSFORM_REFLECTION,   /**< 轴对称 */
    TRANSFORM_SCALING,      /**< 缩放 */
    TRANSFORM_GLUING,       /**< 粘合（复合变换） */
    TRANSFORM_INVERSION     /**< 反演 */
} Lv00TransformType;

/**
 * @brief 变换参数
 */
typedef struct {
    union {
        /* 平移参数 */
        struct {
            mpq_t dx;       /**< x 方向位移 */
            mpq_t dy;       /**< y 方向位移 */
        } translation;

        /* 旋转参数 */
        struct {
            mpq_t cx;       /**< 旋转中心 x 坐标 */
            mpq_t cy;       /**< 旋转中心 y 坐标 */
            mpq_t cos_theta;/**< cos(θ)，有理数或根式 */
            mpq_t sin_theta;/**< sin(θ)，有理数或根式 */
            /* 特殊角度标记 */
            bool is_special_angle;
            int angle_numerator;    /**< 角度 = numerator * π / denominator */
            int angle_denominator;
        } rotation;

        /* 轴对称参数 */
        struct {
            mpq_t ax;       /**< 轴上一点 x 坐标 */
            mpq_t ay;       /**< 轴上一点 y 坐标 */
            mpq_t bx;       /**< 轴上另一点 x 坐标 */
            mpq_t by;       /**< 轴上另一点 y 坐标 */
            /* 或者用直线方程 ax + by + c = 0 表示 */
            mpq_t line_a;   /**< 直线方程系数 a */
            mpq_t line_b;   /**< 直线方程系数 b */
            mpq_t line_c;   /**< 直线方程系数 c */
        } reflection;

        /* 缩放参数 */
        struct {
            mpq_t cx;       /**< 缩放中心 x 坐标 */
            mpq_t cy;       /**< 缩放中心 y 坐标 */
            mpq_t scale;    /**< 缩放因子 */
        } scaling;

        /* 反演参数 */
        struct {
            mpq_t cx;       /**< 反演中心 x 坐标 */
            mpq_t cy;       /**< 反演中心 y 坐标 */
            mpq_t radius;   /**< 反演圆半径 */
        } inversion;
    } params;
} Lv00TransformParams;

/**
 * @brief 变换矩阵（3x3 齐次坐标）
 *
 * 使用有理数矩阵表示仿射变换：
 * [ a  b  tx ]
 * [ c  d  ty ]
 * [ 0  0  1  ]
 */
typedef struct {
    mpq_t a, b, tx;     /**< 第一行 */
    mpq_t c, d, ty;     /**< 第二行 */
} Lv00TransformMatrix;

/**
 * @brief 变换结构
 */
struct Lv00Transform {
    Lv00TransformType type;         /**< 变换类型 */
    Lv00TransformParams params;     /**< 变换参数 */
    Lv00TransformMatrix matrix;     /**< 变换矩阵（缓存） */
    bool matrix_valid;              /**< 矩阵是否有效 */
    bool is_isometry;               /**< 是否为等距变换 */
    bool is_orientation_preserving; /**< 是否保向 */
    int ref_count;                  /**< 引用计数 */
};

/**
 * @brief 变换序列
 */
struct Lv00TransformSequence {
    Lv00Transform **transforms;     /**< 变换数组 */
    uint32_t count;                 /**< 变换数量 */
    uint32_t capacity;              /**< 数组容量 */
    Lv00TransformMatrix composite;  /**< 复合变换矩阵（缓存） */
    bool composite_valid;           /**< 复合矩阵是否有效 */
};

/**
 * @brief 变换群
 */
struct Lv00TransformGroup {
    Lv00Transform **generators;     /**< 生成元数组 */
    uint32_t generator_count;       /**< 生成元数量 */
    char *group_name;               /**< 群名称（如 "D4", "C3"） */
    uint32_t order;                 /**< 群阶（有限群）或 0（无限群） */
    bool is_abelian;                /**< 是否为阿贝尔群 */
};

/* ============== 变换创建/销毁 ============== */

/**
 * @brief 创建恒等变换
 * @return 新变换
 */
Lv00Transform *lv00_transform_identity(void);

/**
 * @brief 创建平移变换
 * @param dx x 方向位移
 * @param dy y 方向位移
 * @return 新变换
 */
Lv00Transform *lv00_transform_translation(const mpq_t dx, const mpq_t dy);

/**
 * @brief 创建旋转变换
 * @param cx 旋转中心 x 坐标
 * @param cy 旋转中心 y 坐标
 * @param angle_num 角度分子（角度 = num * π / denom）
 * @param angle_denom 角度分母
 * @return 新变换
 */
Lv00Transform *lv00_transform_rotation(const mpq_t cx, const mpq_t cy,
                                        int angle_num, int angle_denom);

/**
 * @brief 创建旋转变换（任意角度）
 * @param cx 旋转中心 x 坐标
 * @param cy 旋转中心 y 坐标
 * @param cos_theta cos(θ)
 * @param sin_theta sin(θ)
 * @return 新变换
 */
Lv00Transform *lv00_transform_rotation_arbitrary(const mpq_t cx, const mpq_t cy,
                                                  const mpq_t cos_theta,
                                                  const mpq_t sin_theta);

/**
 * @brief 创建轴对称变换
 * @param ax 轴上一点 x 坐标
 * @param ay 轴上一点 y 坐标
 * @param bx 轴上另一点 x 坐标
 * @param by 轴上另一点 y 坐标
 * @return 新变换
 */
Lv00Transform *lv00_transform_reflection(const mpq_t ax, const mpq_t ay,
                                          const mpq_t bx, const mpq_t by);

/**
 * @brief 创建轴对称变换（直线方程）
 * @param a 直线方程系数 a (ax + by + c = 0)
 * @param b 直线方程系数 b
 * @param c 直线方程系数 c
 * @return 新变换
 */
Lv00Transform *lv00_transform_reflection_line(const mpq_t a, const mpq_t b,
                                               const mpq_t c);

/**
 * @brief 创建缩放变换
 * @param cx 缩放中心 x 坐标
 * @param cy 缩放中心 y 坐标
 * @param scale 缩放因子
 * @return 新变换
 */
Lv00Transform *lv00_transform_scaling(const mpq_t cx, const mpq_t cy,
                                       const mpq_t scale);

/**
 * @brief 销毁变换
 * @param t 变换指针
 */
void lv00_transform_destroy(Lv00Transform *t);

/**
 * @brief 增加变换引用计数
 * @param t 变换
 */
void lv00_transform_ref(Lv00Transform *t);

/**
 * @brief 减少变换引用计数
 * @param t 变换
 */
void lv00_transform_unref(Lv00Transform *t);

/* ============== 变换应用 ============== */

/**
 * @brief 应用变换到点
 * @param t 变换
 * @param x 点 x 坐标（输入/输出）
 * @param y 点 y 坐标（输入/输出）
 * @return 是否成功
 */
bool lv00_transform_apply_point(const Lv00Transform *t, mpq_t x, mpq_t y);

/**
 * @brief 应用变换到符号坐标
 * @param t 变换
 * @param coord 符号坐标
 * @return 变换后的新坐标
 */
SymbolicCoord *lv00_transform_apply_coord(const Lv00Transform *t,
                                           const SymbolicCoord *coord);

/**
 * @brief 应用变换到约束图
 * @param t 变换
 * @param graph 约束图
 * @param node_ids 要变换的节点 ID 数组（NULL 表示全部）
 * @param node_count 节点数量
 * @return 是否成功
 */
bool lv00_transform_apply_graph(const Lv00Transform *t, ConstraintGraph *graph,
                                 const int *node_ids, uint32_t node_count);

/**
 * @brief 获取变换矩阵
 * @param t 变换
 * @param matrix 输出矩阵
 * @return 是否成功
 */
bool lv00_transform_get_matrix(Lv00Transform *t, Lv00TransformMatrix *matrix);

/* ============== 变换复合 ============== */

/**
 * @brief 创建变换序列
 * @return 新序列
 */
Lv00TransformSequence *lv00_transform_sequence_create(void);

/**
 * @brief 销毁变换序列
 * @param seq 序列指针
 */
void lv00_transform_sequence_destroy(Lv00TransformSequence *seq);

/**
 * @brief 添加变换到序列
 * @param seq 序列
 * @param t 变换
 * @return 是否成功
 */
bool lv00_transform_sequence_add(Lv00TransformSequence *seq, Lv00Transform *t);

/**
 * @brief 计算复合变换
 * @param seq 序列
 * @return 复合变换（新对象）
 */
Lv00Transform *lv00_transform_sequence_composite(const Lv00TransformSequence *seq);

/**
 * @brief 应用变换序列
 * @param seq 序列
 * @param x 点 x 坐标（输入/输出）
 * @param y 点 y 坐标（输入/输出）
 * @return 是否成功
 */
bool lv00_transform_sequence_apply(const Lv00TransformSequence *seq,
                                    mpq_t x, mpq_t y);

/* ============== 变换性质分析 ============== */

/**
 * @brief 检查变换是否为等距变换
 * @param t 变换
 * @return 是否为等距变换
 */
bool lv00_transform_is_isometry(const Lv00Transform *t);

/**
 * @brief 检查变换是否保向
 * @param t 变换
 * @return 是否保向
 */
bool lv00_transform_is_orientation_preserving(const Lv00Transform *t);

/**
 * @brief 查找变换的不动点
 * @param t 变换
 * @param out_x 输出不动点 x 坐标
 * @param out_y 输出不动点 y 坐标
 * @return 是否存在不动点
 */
bool lv00_transform_find_fixed_point(const Lv00Transform *t,
                                      mpq_t out_x, mpq_t out_y);

/**
 * @brief 计算变换的阶
 *
 * 对于有限阶变换，返回使 T^n = I 的最小正整数 n。
 * 对于无限阶变换，返回 0。
 *
 * @param t 变换
 * @return 变换阶数
 */
uint32_t lv00_transform_order(const Lv00Transform *t);

/**
 * @brief 计算变换的逆
 * @param t 变换
 * @return 逆变换（新对象）
 */
Lv00Transform *lv00_transform_inverse(const Lv00Transform *t);

/**
 * @brief 复合两个变换
 * @param t1 第一个变换（先应用）
 * @param t2 第二个变换（后应用）
 * @return t2 ∘ t1（新对象）
 */
Lv00Transform *lv00_transform_compose(const Lv00Transform *t1,
                                       const Lv00Transform *t2);

/**
 * @brief 检查两个变换是否相等
 * @param t1 第一个变换
 * @param t2 第二个变换
 * @return 是否相等
 */
bool lv00_transform_equal(const Lv00Transform *t1, const Lv00Transform *t2);

/* ============== 变换群 ============== */

/**
 * @brief 创建变换群
 * @param name 群名称
 * @return 新群
 */
Lv00TransformGroup *lv00_transform_group_create(const char *name);

/**
 * @brief 销毁变换群
 * @param group 群指针
 */
void lv00_transform_group_destroy(Lv00TransformGroup *group);

/**
 * @brief 添加生成元
 * @param group 群
 * @param generator 生成元
 * @return 是否成功
 */
bool lv00_transform_group_add_generator(Lv00TransformGroup *group,
                                         Lv00Transform *generator);

/**
 * @brief 生成群的所有元素（有限群）
 * @param group 群
 * @param out_elements 输出元素数组
 * @param max_count 最大元素数量
 * @return 实际元素数量
 */
uint32_t lv00_transform_group_generate_elements(const Lv00TransformGroup *group,
                                                 Lv00Transform ***out_elements,
                                                 uint32_t max_count);

/**
 * @brief 检查变换是否属于群
 * @param group 群
 * @param t 变换
 * @return 是否属于群
 */
bool lv00_transform_group_contains(const Lv00TransformGroup *group,
                                    const Lv00Transform *t);

/**
 * @brief 创建常见变换群
 * @param type 群类型名称（如 "D4", "C3", "Klein"）
 * @return 新群
 */
Lv00TransformGroup *lv00_transform_group_create_preset(const char *type);

/* ============== 约束保持性验证 ============== */

/**
 * @brief 约束保持性检查结果
 */
typedef enum {
    CONSTRAINT_PRESERVED,       /**< 约束完全保持 */
    CONSTRAINT_TRANSFORMED,     /**< 约束被变换为另一约束 */
    CONSTRAINT_BROKEN,          /**< 约束被破坏 */
    CONSTRAINT_CHECK_FAILED     /**< 检查失败 */
} Lv00ConstraintPreservation;

/**
 * @brief 检查约束在变换下的保持性
 * @param t 变换
 * @param constraint 约束
 * @param graph 约束图
 * @return 保持性结果
 */
Lv00ConstraintPreservation lv00_transform_check_constraint(
    const Lv00Transform *t,
    const Constraint *constraint,
    const ConstraintGraph *graph);

/**
 * @brief 检查所有约束在变换下的保持性
 * @param t 变换
 * @param graph 约束图
 * @param out_broken 输出被破坏的约束 ID 数组
 * @param max_broken 最大输出数量
 * @return 被破坏的约束数量
 */
uint32_t lv00_transform_check_all_constraints(
    const Lv00Transform *t,
    const ConstraintGraph *graph,
    int *out_broken,
    uint32_t max_broken);

/* ============== 特殊变换识别 ============== */

/**
 * @brief 识别图形的对称变换
 *
 * 分析约束图所描述的几何图形，识别其所有对称变换。
 *
 * @param graph 约束图
 * @param out_transforms 输出变换数组
 * @param max_count 最大输出数量
 * @return 实际变换数量
 */
uint32_t lv00_transform_identify_symmetries(const ConstraintGraph *graph,
                                             Lv00Transform ***out_transforms,
                                             uint32_t max_count);

/**
 * @brief 检查两点是否关于直线对称
 * @param px 第一点 x 坐标
 * @param py 第一点 y 坐标
 * @param qx 第二点 x 坐标
 * @param qy 第二点 y 坐标
 * @param ax 轴上一点 x 坐标
 * @param ay 轴上一点 y 坐标
 * @param bx 轴上另一点 x 坐标
 * @param by 轴上另一点 y 坐标
 * @return 是否对称
 */
bool lv00_points_are_symmetric(const mpq_t px, const mpq_t py,
                                const mpq_t qx, const mpq_t qy,
                                const mpq_t ax, const mpq_t ay,
                                const mpq_t bx, const mpq_t by);

/**
 * @brief 计算点关于直线的对称点
 * @param px 原点 x 坐标
 * @param py 原点 y 坐标
 * @param ax 轴上一点 x 坐标
 * @param ay 轴上一点 y 坐标
 * @param bx 轴上另一点 x 坐标
 * @param by 轴上另一点 y 坐标
 * @param out_x 输出对称点 x 坐标
 * @param out_y 输出对称点 y 坐标
 * @return 是否成功
 */
bool lv00_reflect_point(const mpq_t px, const mpq_t py,
                         const mpq_t ax, const mpq_t ay,
                         const mpq_t bx, const mpq_t by,
                         mpq_t out_x, mpq_t out_y);

/**
 * @brief 计算点绕中心旋转后的位置
 * @param px 原点 x 坐标
 * @param py 原点 y 坐标
 * @param cx 旋转中心 x 坐标
 * @param cy 旋转中心 y 坐标
 * @param angle_num 角度分子
 * @param angle_denom 角度分母
 * @param out_x 输出 x 坐标
 * @param out_y 输出 y 坐标
 * @return 是否成功
 */
bool lv00_rotate_point(const mpq_t px, const mpq_t py,
                        const mpq_t cx, const mpq_t cy,
                        int angle_num, int angle_denom,
                        mpq_t out_x, mpq_t out_y);

/* ============== 变换序列化 ============== */

/**
 * @brief 变换序列化为字符串
 * @param t 变换
 * @return 字符串（调用者负责释放）
 */
char *lv00_transform_to_string(const Lv00Transform *t);

/**
 * @brief 变换序列化为 JSON
 * @param t 变换
 * @return JSON 字符串（调用者负责释放）
 */
char *lv00_transform_to_json(const Lv00Transform *t);

/**
 * @brief 从 JSON 解析变换
 * @param json JSON 字符串
 * @return 新变换
 */
Lv00Transform *lv00_transform_from_json(const char *json);

#ifdef __cplusplus
}
#endif

#endif /* LV00_GEOMETRY_TRANSFORM_H */
