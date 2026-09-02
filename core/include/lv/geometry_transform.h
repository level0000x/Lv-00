#ifndef lv_GEOMETRY_TRANSFORM_H
#define lv_GEOMETRY_TRANSFORM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include <gmp.h>
#include <stdbool.h>
#include <stddef.h>

#include "lv/lv_utils.h"

/* -- 前向声明 -- */
struct ConstraintGraph;
typedef struct ConstraintGraph ConstraintGraph;

/* -- Transform type enum -- */
typedef enum {
    TRANSFORM_IDENTITY = 0,
    TRANSFORM_TRANSLATION,
    TRANSFORM_ROTATION,
    TRANSFORM_SCALE,
    TRANSFORM_SHEAR,
    TRANSFORM_REFLECTION,
    TRANSFORM_SCALING,
    TRANSFORM_AFFINE,
    TRANSFORM_PROJECTIVE,
    TRANSFORM_GLUING,
    TRANSFORM_COMPOSITE
} lvTransformType;

/* -- 2x3 affine matrix (rational) -- */
typedef struct {
    mpq_t a;
    mpq_t b;
    mpq_t tx;
    mpq_t c;
    mpq_t d;
    mpq_t ty;
} lvAffineMatrix;

/* -- Translation params -- */
typedef struct {
    mpq_t dx;
    mpq_t dy;
} lvTranslationParams;

/* -- Rotation params -- */
typedef struct {
    mpq_t cx;
    mpq_t cy;
    mpq_t cos_a;
    mpq_t sin_a;
    mpq_t cos_theta;
    mpq_t sin_theta;
    double angle;
    double angle_cos;
    double angle_sin;
    bool is_special_angle;
    int angle_numerator;
    int angle_denominator;
} lvRotationParams;

/* -- Scale params -- */
typedef struct {
    mpq_t sx;
    mpq_t sy;
} lvScaleParams;

/* -- Scaling params -- */
typedef struct {
    mpq_t sx;
    mpq_t sy;
    mpq_t cx;
    mpq_t cy;
    mpq_t scale;
} lvScalingParams;

/* -- Reflection params -- */
typedef struct {
    mpq_t ax;
    mpq_t ay;
    mpq_t bx;
    mpq_t by;
    mpq_t line_a;
    mpq_t line_b;
    mpq_t line_c;
} lvReflectionParams;

/* -- Transform params union -- */
typedef union {
    lvTranslationParams translation;
    lvRotationParams rotation;
    lvScaleParams scale;
    lvScalingParams scaling;
    lvReflectionParams reflection;
} lvTransformParamsUnion;

typedef struct {
    lvTransformParamsUnion params;
} lvTransformParams;

/* -- Main transform struct -- */
typedef struct lvTransform {
    lvTransformType type;
    lvAffineMatrix matrix;
    bool matrix_valid;
    lvTransformParams params;
    bool is_isometry;
    bool is_orientation_preserving;
    int ref_count;
} lvTransform;

/* -- Transform sequence -- */
typedef struct {
    lvDArray transforms_da;
    bool composite_valid;
} lvTransformSequence;

/* -- Transform group -- */
#define GROUP_MAX_GENERATORS 16
typedef struct {
    char *group_name;
    lvTransform **generators;
    int generator_count;
    int order;
    bool is_abelian;
} lvTransformGroup;

/* -- Transform matrix (output type, uses mpq_t) -- */
typedef lvAffineMatrix lvTransformMatrix;

/* ==== API ==== */
/**
 * @brief 创建恒等变换
 * @return 成功返回恒等变换指针，失败返回 NULL
 */
lvTransform *lv_transform_identity(void);
/**
 * @brief 创建平移变换
 * @param dx X 轴平移量（有理数）
 * @param dy Y 轴平移量（有理数）
 * @return 成功返回平移变换指针，失败返回 NULL
 */
lvTransform *lv_transform_translation(const mpq_t dx, const mpq_t dy);
/**
 * @brief 创建旋转变换（有理数角度）
 * @param cx 旋转中心 X 坐标（有理数）
 * @param cy 旋转中心 Y 坐标（有理数）
 * @param angle_num 旋转角度分子
 * @param angle_denom 旋转角度分母
 * @return 成功返回旋转变换指针，失败返回 NULL
 */
lvTransform *lv_transform_rotation(const mpq_t cx, const mpq_t cy, int angle_num, int angle_denom);
/**
 * @brief 创建旋转变换（任意有理余弦/正弦值）
 * @param cx 旋转中心 X 坐标（有理数）
 * @param cy 旋转中心 Y 坐标（有理数）
 * @param cos_a 旋转角余弦值（有理数）
 * @param sin_a 旋转角正弦值（有理数）
 * @return 成功返回旋转变换指针，失败返回 NULL
 */
lvTransform *lv_transform_rotation_arbitrary(const mpq_t cx, const mpq_t cy, const mpq_t cos_a, const mpq_t sin_a);
/**
 * @brief 创建旋转变换（浮点角度）
 * @param cx 旋转中心 X 坐标
 * @param cy 旋转中心 Y 坐标
 * @param angle_rad 旋转角度（弧度）
 * @return 成功返回旋转变换指针，失败返回 NULL
 */
lvTransform *lv_transform_rotation_double(double cx, double cy, double angle_rad);
/**
 * @brief 创建缩放变换（沿坐标轴）
 * @param sx X 轴缩放因子（有理数）
 * @param sy Y 轴缩放因子（有理数）
 * @return 成功返回缩放变换指针，失败返回 NULL
 */
lvTransform *lv_transform_scale(const mpq_t sx, const mpq_t sy);
/**
 * @brief 创建反射变换（通过线上两点定义反射轴）
 * @param ax 反射轴上点 A 的 X 坐标（有理数）
 * @param ay 反射轴上点 A 的 Y 坐标（有理数）
 * @param bx 反射轴上点 B 的 X 坐标（有理数）
 * @param by 反射轴上点 B 的 Y 坐标（有理数）
 * @return 成功返回反射变换指针，失败返回 NULL
 */
lvTransform *lv_transform_reflection(const mpq_t ax, const mpq_t ay, const mpq_t bx, const mpq_t by);
/**
 * @brief 创建反射变换（通过直线方程定义反射轴）
 * @param a 直线方程系数 a（有理数）
 * @param b 直线方程系数 b（有理数）
 * @param c 直线方程系数 c（有理数）
 * @return 成功返回反射变换指针，失败返回 NULL
 */
lvTransform *lv_transform_reflection_line(const mpq_t a, const mpq_t b, const mpq_t c);
/**
 * @brief 释放变换内存
 * @param t 变换指针
 */
lv_PUBLIC_API void lv_transform_destroy(lvTransform *t);
/**
 * @brief 增加变换引用计数
 * @param t 变换指针
 */
lv_PUBLIC_API void lv_transform_ref(lvTransform *t);
/**
 * @brief 减少变换引用计数（计数归零时释放）
 * @param t 变换指针
 */
lv_PUBLIC_API void lv_transform_unref(lvTransform *t);
/**
 * @brief 使用变换平移点（原地修改有理数坐标）
 * @param t 变换指针
 * @param x 点的 X 坐标（输入/输出，有理数）
 * @param y 点的 Y 坐标（输入/输出，有理数）
 * @return 成功返回 true，失败返回 false
 */
lv_PUBLIC_API bool lv_transform_apply_point(const lvTransform *t, mpq_t x, mpq_t y);
/**
 * @brief 使用变换平移点（有理数坐标，输出到目标坐标）
 * @param t 变换指针
 * @param src_x 源点 X 坐标（有理数）
 * @param src_y 源点 Y 坐标（有理数）
 * @param dst_x 目标 X 坐标（输出，有理数）
 * @param dst_y 目标 Y 坐标（输出，有理数）
 */
lv_PUBLIC_API void lv_transform_apply_mpq(const lvTransform *t, const mpq_t src_x, const mpq_t src_y, mpq_t dst_x, mpq_t dst_y);
/**
 * @brief 使用变换平移点（浮点坐标）
 * @param t 变换指针
 * @param src_x 源点 X 坐标
 * @param src_y 源点 Y 坐标
 * @param dst_x 目标 X 坐标（输出）
 * @param dst_y 目标 Y 坐标（输出）
 */
lv_PUBLIC_API void lv_transform_apply_double(const lvTransform *t, double src_x, double src_y, double *dst_x, double *dst_y);
/**
 * @brief 组合两个变换（先应用 a 再应用 b）
 * @param a 第一个变换指针
 * @param b 第二个变换指针
 * @return 成功返回组合变换指针，失败返回 NULL
 */
lvTransform *lv_transform_compose(const lvTransform *a, const lvTransform *b);
/**
 * @brief 获取变换的仿射矩阵
 * @param t 变换指针
 * @param matrix 输出矩阵指针
 * @return 成功返回 true，失败返回 false
 */
lv_PUBLIC_API bool lv_transform_get_matrix(lvTransform *t, lvTransformMatrix *matrix);
/**
 * @brief 获取变换类型的名称字符串
 * @param type 变换类型枚举
 * @return 返回类型名称字符串
 */
lv_PUBLIC_API const char *lv_transform_type_name(lvTransformType type);
/**
 * @brief 判断变换是否为等距变换
 * @param t 变换指针
 * @return 是等距变换返回 true，否则返回 false
 */
lv_PUBLIC_API bool lv_transform_is_isometry(const lvTransform *t);
/**
 * @brief 判断变换是否保持定向
 * @param t 变换指针
 * @return 保持定向返回 true，否则返回 false
 */
lv_PUBLIC_API bool lv_transform_is_orientation_preserving(const lvTransform *t);
/**
 * @brief 计算逆变换
 * @param t 变换指针
 * @return 成功返回逆变换指针，失败返回 NULL
 */
lvTransform *lv_transform_inverse(const lvTransform *t);
/**
 * @brief 计算点关于直线的反射
 * @param px 被反射点 P 的 X 坐标（有理数）
 * @param py 被反射点 P 的 Y 坐标（有理数）
 * @param ax 直线上点 A 的 X 坐标（有理数）
 * @param ay 直线上点 A 的 Y 坐标（有理数）
 * @param bx 直线上点 B 的 X 坐标（有理数）
 * @param by 直线上点 B 的 Y 坐标（有理数）
 * @param rx 反射结果 X 坐标（输出，有理数）
 * @param ry 反射结果 Y 坐标（输出，有理数）
 * @return 成功返回 true，失败返回 false
 *
 * @note 修复（C-㊺续9 测试暴露）：原声明参数顺序 (ax,ay,bx,by,px,py) 与
 *       实现及内部调用（geometry_transform_analysis.c，点在前）不一致
 *       （M4 声称与实现脱节）——统一为点在前 (px,py,ax,ay,bx,by)。
 */
bool lv_reflect_point(const mpq_t px, const mpq_t py, const mpq_t ax, const mpq_t ay, const mpq_t bx, const mpq_t by,
                      mpq_t rx, mpq_t ry);

/* -- Sequence API -- */
/**
 * @brief 创建变换序列
 * @return 成功返回序列指针，失败返回 NULL
 */
lvTransformSequence *lv_transform_sequence_create(void);
/**
 * @brief 销毁变换序列
 * @param seq 变换序列指针
 */
lv_PUBLIC_API void lv_transform_sequence_destroy(lvTransformSequence *seq);
/**
 * @brief 向变换序列添加一个变换
 * @param seq 变换序列指针
 * @param t 要添加的变换指针
 * @return 成功返回 true，失败返回 false
 */
lv_PUBLIC_API bool lv_transform_sequence_add(lvTransformSequence *seq, lvTransform *t);
/**
 * @brief 将序列中所有变换合成为一个变换
 * @param seq 变换序列指针
 * @return 成功返回合成变换指针，失败返回 NULL
 */
lvTransform *lv_transform_sequence_compose_all(const lvTransformSequence *seq);

/* -- Group API -- */
/**
 * @brief 创建变换群
 * @param name 群名称
 * @return 成功返回变换群指针，失败返回 NULL
 */
lvTransformGroup *lv_transform_group_create(const char *name);
/**
 * @brief 销毁变换群
 * @param group 变换群指针
 */
lv_PUBLIC_API void lv_transform_group_destroy(lvTransformGroup *group);
/**
 * @brief 向变换群添加生成元
 * @param group 变换群指针
 * @param generator 生成元变换指针
 * @return 成功返回 true，失败返回 false
 */
lv_PUBLIC_API bool lv_transform_group_add_generator(lvTransformGroup *group, lvTransform *generator);
/**
 * @brief 创建预设变换群
 * @param type 预设类型字符串
 * @return 成功返回变换群指针，失败返回 NULL
 */
lvTransformGroup *lv_transform_group_create_preset(const char *type);

/* -- Double convenience API -- */
/**
 * @brief 创建恒等 4x4 矩阵（双精度）
 * @param out 输出的 4x4 矩阵（16 元素数组）
 */
lv_PUBLIC_API void lv_transform_identity_double(double out[16]);
/**
 * @brief 创建平移 4x4 矩阵（双精度）
 * @param out 输出的 4x4 矩阵（16 元素数组）
 * @param x X 轴平移量
 * @param y Y 轴平移量
 * @param z Z 轴平移量
 */
lv_PUBLIC_API void lv_transform_translate_double(double out[16], double x, double y, double z);
/**
 * @brief 创建旋转 4x4 矩阵（双精度）
 * @param out 输出的 4x4 矩阵（16 元素数组）
 * @param angle_rad 旋转角度（弧度）
 * @param x 旋转轴 X 分量
 * @param y 旋转轴 Y 分量
 * @param z 旋转轴 Z 分量
 */
lv_PUBLIC_API void lv_transform_rotate_double(double out[16], double angle_rad, double x, double y, double z);
/**
 * @brief 创建缩放 4x4 矩阵（双精度）
 * @param out 输出的 4x4 矩阵（16 元素数组）
 * @param sx X 轴缩放因子
 * @param sy Y 轴缩放因子
 * @param sz Z 轴缩放因子
 */
lv_PUBLIC_API void lv_transform_scale_double(double out[16], double sx, double sy, double sz);
/**
 * @brief 应用 4x4 变换到顶点数组（双精度）
 * @param t 4x4 变换矩阵（16 元素数组）
 * @param in 输入顶点数组（每 3 个元素为一个顶点）
 * @param out 输出顶点数组
 * @param count 顶点数量
 */
lv_PUBLIC_API void lv_transform_apply_double4x4(const double t[16], const double *in, double *out, size_t count);

/* -- 变换阶与对称性分析 -- */

/**
 * @brief 计算变换的阶
 *
 * 返回满足 T^n = I（恒等变换）的最小正整数 n。
 * 无限阶变换返回 0，参数无效返回 -1。
 *
 * @param t 变换指针
 * @return 阶数（1=恒等，0=无限阶，-1=错误）
 */
lv_PUBLIC_API int lv_transform_order(const lvTransform *t);

/**
 * @brief 分析约束图的对称变换
 *
 * 检测约束图中的几何对称性，包括轴反射、中心对称、旋转对称等。
 *
 * @param graph           约束图（const，不会修改）
 * @param out_transforms  输出数组（调用者负责逐个 destroy 并 free）
 * @param max_count       数组最大容量
 * @return 找到的对称变换数量
 */
lv_PUBLIC_API int lv_transform_identify_symmetries(const ConstraintGraph *graph, lvTransform **out_transforms, int max_count);

#ifdef __cplusplus
}
#endif
#endif
