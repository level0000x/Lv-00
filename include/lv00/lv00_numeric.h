/**
 * @file lv00_numeric.h
 * @brief 轻量数值计算基础设施 —— 借鉴 Eigen 纯头文件模式与 Geometry 模块
 *
 * @details 设计借鉴来源：
 *          - Eigen (gitlab.com/libeigen/eigen) — 纯头文件C++线性代数库
 *            · 纯头文件零依赖架构
 *            · Geometry 模块（Transform/Rotation/Quaternion/AngleAxis）
 *            · 固定大小矩阵栈分配优化
 *            · SIMD 向量化（SSE/AVX/NEON）
 *
 *          设计目标：
 *          - 纯 C 头文件，零依赖（仅依赖标准C库 math.h）
 *          - 3x3 和 4x4 矩阵栈分配（在栈上而非堆上，借鉴 Eigen Matrix4d）
 *          - 语义化几何变换 API（平移/旋转/缩放/仿射/射影）
 *          - SSE2 自动检测和加速
 *
 * @author Lv-00 Project
 * @version v3.3.0
 * @date 2026-05-24
 */

#ifndef LV00_NUMERIC_H
#define LV00_NUMERIC_H

#include <math.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── SIMD 检测 ── */
#if defined(__SSE2__) || defined(__x86_64__) || defined(_M_X64)
#define LV00_USE_SSE2 1
#include <emmintrin.h>
#endif

/* ========================================================================
 * 第一部分：小矩阵栈分配（借鉴 Eigen 固定大小矩阵）
 *
 * Eigen 中 Matrix3d / Matrix4d / Vector3d 等均为栈分配固定大小矩阵，
 * 避免动态内存分配。Lv-00 采用相同策略，所有结构体均可在栈上创建。
 * ======================================================================== */

/* ─── 数学常量 ─── */

/** 圆周率 pi，双精度 */
#define LV00_PI 3.14159265358979323846
/** 微小值阈值，用于浮点比较和奇异性检测 */
#define LV00_EPSILON 1e-15
/** 将角度从度转换为弧度 */
#define LV00_DEG2RAD(d) ((d) * LV00_PI / 180.0)
/** 将角度从弧度转换为度 */
#define LV00_RAD2DEG(r) ((r) * 180.0 / LV00_PI)

/* ─── 向量与矩阵类型定义 ─── */

/**
 * @brief 2D 向量（栈分配，16字节）
 *
 * 用于平面几何计算，参考 Eigen Vector2d。
 */
typedef struct {
    double x, y;
} Lv00Vec2;

/**
 * @brief 3D 向量（栈分配，24字节）
 *
 * 参考 Eigen Vector3d，用于三维空间中的方向、位置和法向量。
 */
typedef struct {
    double x, y, z;
} Lv00Vec3;

/**
 * @brief 4D / 齐次向量（栈分配，32字节）
 *
 * 参考 Eigen Vector4d，用于齐次坐标运算（w=1 为点，w=0 为方向）。
 */
typedef struct {
    double x, y, z, w;
} Lv00Vec4;

/**
 * @brief 3x3 矩阵（栈分配，72字节，行主序）
 *
 * 参考 Eigen Matrix3d，用于线性变换（旋转/缩放/剪切）。
 * 存储顺序：m[0..2]=第0行, m[3..5]=第1行, m[6..8]=第2行。
 */
typedef struct {
    double m[9];
} Lv00Mat3;

/**
 * @brief 4x4 齐次矩阵（栈分配，128字节，列主序）
 *
 * 参考 Eigen Matrix4d / Transform，用于仿射和投影变换。
 * 列主序与 OpenGL 兼容：m[0..3]=第0列, m[4..7]=第1列,
 * m[8..11]=第2列, m[12..15]=第3列。
 */
typedef struct {
    double m[16];
} Lv00Mat4;

/**
 * @brief 四元数（栈分配，32字节）
 *
 * 参考 Eigen Quaterniond，表示为 (w + xi + yj + zk)。
 */
typedef struct {
    double w, x, y, z;
} Lv00Quat;

/**
 * @brief 几何变换类型枚举
 *
 * 参考 Eigen 的 TransformTraits，区分不同类型的空间变换。
 */
typedef enum {
    LV00_TRANSFORM_IDENTITY = 0,    /**< 恒等变换 */
    LV00_TRANSFORM_TRANSLATION = 1, /**< 平移变换 */
    LV00_TRANSFORM_ROTATION = 2,    /**< 旋转变换 */
    LV00_TRANSFORM_SCALE = 3,       /**< 缩放变换 */
    LV00_TRANSFORM_AFFINE = 4,      /**< 仿射变换（线性+平移） */
    LV00_TRANSFORM_SIMILARITY = 5,  /**< 相似变换（缩放+旋转+平移） */
    LV00_TRANSFORM_ISOMETRY = 6,    /**< 等距变换（仅旋转+平移，保持距离） */
    LV00_TRANSFORM_PROJECTIVE = 7   /**< 投影变换 */
} Lv00TransformType;

/**
 * @brief 语义化几何变换
 *
 * 将变换类型与 4x4 齐次矩阵绑定，提供语义感知的变换操作。
 * 参考 Eigen 的 Transform 类设计。
 */
typedef struct Lv00GeomTransform {
    Lv00TransformType type; /**< 变换类型 */
    Lv00Mat4 matrix;        /**< 4x4 齐次变换矩阵 */
    bool is_identity;       /**< 是否为恒等变换 */
} Lv00GeomTransform;

/* ========================================================================
 * 第二部分：Vec2 基础运算（借鉴 Eigen 运算符重载映射为命名函数）
 * ======================================================================== */

/** @brief Vec2 加法 */
static inline Lv00Vec2 lv00_vec2_add(Lv00Vec2 a, Lv00Vec2 b) {
    return (Lv00Vec2) {a.x + b.x, a.y + b.y};
}

/** @brief Vec2 减法 */
static inline Lv00Vec2 lv00_vec2_sub(Lv00Vec2 a, Lv00Vec2 b) {
    return (Lv00Vec2) {a.x - b.x, a.y - b.y};
}

/** @brief Vec2 标量乘法 */
static inline Lv00Vec2 lv00_vec2_scale(Lv00Vec2 v, double s) {
    return (Lv00Vec2) {v.x * s, v.y * s};
}

/** @brief Vec2 点积（内积） */
static inline double lv00_vec2_dot(Lv00Vec2 a, Lv00Vec2 b) {
    return a.x * b.x + a.y * b.y;
}

/** @brief Vec2 欧几里得范数（长度） */
static inline double lv00_vec2_norm(Lv00Vec2 v) {
    return sqrt(v.x * v.x + v.y * v.y);
}

/** @brief Vec2 归一化（返回单位向量，零向量不变） */
static inline Lv00Vec2 lv00_vec2_normalize(Lv00Vec2 v) {
    double n = lv00_vec2_norm(v);
    if (n < LV00_EPSILON)
        return v;
    return lv00_vec2_scale(v, 1.0 / n);
}

/** @brief Vec2 叉积的 Z 分量（标量叉积，a.x*b.y - a.y*b.x） */
static inline double lv00_vec2_cross(Lv00Vec2 a, Lv00Vec2 b) {
    return a.x * b.y - a.y * b.x;
}

/** @brief Vec2 逐分量乘法 */
static inline Lv00Vec2 lv00_vec2_mul_elem(Lv00Vec2 a, Lv00Vec2 b) {
    return (Lv00Vec2) {a.x * b.x, a.y * b.y};
}

/* ========================================================================
 * 第三部分：Vec3 基础运算
 * ======================================================================== */

/** @brief Vec3 加法 */
static inline Lv00Vec3 lv00_vec3_add(Lv00Vec3 a, Lv00Vec3 b) {
    return (Lv00Vec3) {a.x + b.x, a.y + b.y, a.z + b.z};
}

/** @brief Vec3 减法 */
static inline Lv00Vec3 lv00_vec3_sub(Lv00Vec3 a, Lv00Vec3 b) {
    return (Lv00Vec3) {a.x - b.x, a.y - b.y, a.z - b.z};
}

/** @brief Vec3 标量乘法 */
static inline Lv00Vec3 lv00_vec3_scale(Lv00Vec3 v, double s) {
    return (Lv00Vec3) {v.x * s, v.y * s, v.z * s};
}

/** @brief Vec3 点积（内积） */
static inline double lv00_vec3_dot(Lv00Vec3 a, Lv00Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

/** @brief Vec3 欧几里得范数（长度） */
static inline double lv00_vec3_norm(Lv00Vec3 v) {
    return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

/** @brief Vec3 归一化 */
static inline Lv00Vec3 lv00_vec3_normalize(Lv00Vec3 v) {
    double n = lv00_vec3_norm(v);
    if (n < LV00_EPSILON)
        return v;
    return lv00_vec3_scale(v, 1.0 / n);
}

/** @brief Vec3 叉积（外积） */
static inline Lv00Vec3 lv00_vec3_cross(Lv00Vec3 a, Lv00Vec3 b) {
    return (Lv00Vec3) {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

/** @brief Vec3 逐分量乘法 */
static inline Lv00Vec3 lv00_vec3_mul_elem(Lv00Vec3 a, Lv00Vec3 b) {
    return (Lv00Vec3) {a.x * b.x, a.y * b.y, a.z * b.z};
}

/* ========================================================================
 * 第四部分：Vec4 基础运算
 * ======================================================================== */

/** @brief Vec4 加法 */
static inline Lv00Vec4 lv00_vec4_add(Lv00Vec4 a, Lv00Vec4 b) {
    return (Lv00Vec4) {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
}

/** @brief Vec4 标量乘法 */
static inline Lv00Vec4 lv00_vec4_scale(Lv00Vec4 v, double s) {
    return (Lv00Vec4) {v.x * s, v.y * s, v.z * s, v.w * s};
}

/** @brief Vec4 点积 */
static inline double lv00_vec4_dot(Lv00Vec4 a, Lv00Vec4 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

/* ========================================================================
 * 第五部分：Mat3 操作（借鉴 Eigen Matrix3d）
 *
 * 提供 3x3 矩阵的构造、乘法、求逆等运算。
 * 存储为行主序：m[row*3 + col]。
 * ======================================================================== */

/** @brief 构造 3x3 单位矩阵 */
static inline Lv00Mat3 lv00_mat3_identity(void) {
    Lv00Mat3 r;
    memset(r.m, 0, sizeof(r.m));
    r.m[0] = 1.0;
    r.m[4] = 1.0;
    r.m[8] = 1.0;
    return r;
}

/** @brief Mat3 转置 */
static inline Lv00Mat3 lv00_mat3_transpose(Lv00Mat3 a) {
    return (Lv00Mat3) {{a.m[0], a.m[3], a.m[6], a.m[1], a.m[4], a.m[7], a.m[2], a.m[5], a.m[8]}};
}

/** @brief Mat3 乘法：C = A * B */
static inline Lv00Mat3 lv00_mat3_mul(Lv00Mat3 a, Lv00Mat3 b) {
    Lv00Mat3 r;
    r.m[0] = a.m[0] * b.m[0] + a.m[1] * b.m[3] + a.m[2] * b.m[6];
    r.m[1] = a.m[0] * b.m[1] + a.m[1] * b.m[4] + a.m[2] * b.m[7];
    r.m[2] = a.m[0] * b.m[2] + a.m[1] * b.m[5] + a.m[2] * b.m[8];
    r.m[3] = a.m[3] * b.m[0] + a.m[4] * b.m[3] + a.m[5] * b.m[6];
    r.m[4] = a.m[3] * b.m[1] + a.m[4] * b.m[4] + a.m[5] * b.m[7];
    r.m[5] = a.m[3] * b.m[2] + a.m[4] * b.m[5] + a.m[5] * b.m[8];
    r.m[6] = a.m[6] * b.m[0] + a.m[7] * b.m[3] + a.m[8] * b.m[6];
    r.m[7] = a.m[6] * b.m[1] + a.m[7] * b.m[4] + a.m[8] * b.m[7];
    r.m[8] = a.m[6] * b.m[2] + a.m[7] * b.m[5] + a.m[8] * b.m[8];
    return r;
}

/** @brief Mat3 乘 Vec3：out = M * v（列向量右乘） */
static inline Lv00Vec3 lv00_mat3_mul_vec(Lv00Mat3 m, Lv00Vec3 v) {
    return (Lv00Vec3) {m.m[0] * v.x + m.m[1] * v.y + m.m[2] * v.z, m.m[3] * v.x + m.m[4] * v.y + m.m[5] * v.z,
                       m.m[6] * v.x + m.m[7] * v.y + m.m[8] * v.z};
}

/** @brief Mat3 行列式 */
static inline double lv00_mat3_det(Lv00Mat3 m) {
    return m.m[0] * (m.m[4] * m.m[8] - m.m[5] * m.m[7]) - m.m[1] * (m.m[3] * m.m[8] - m.m[5] * m.m[6]) +
           m.m[2] * (m.m[3] * m.m[7] - m.m[4] * m.m[6]);
}

/**
 * @brief Mat3 逆矩阵（伴随矩阵法）
 *
 * @param[in] m  输入 3x3 矩阵
 * @return 逆矩阵；如果行列式接近零则返回零矩阵
 */
static inline Lv00Mat3 lv00_mat3_inverse(Lv00Mat3 m) {
    double det = lv00_mat3_det(m);
    Lv00Mat3 r;
    if (fabs(det) < LV00_EPSILON) {
        memset(r.m, 0, sizeof(r.m));
        return r;
    }
    double inv_det = 1.0 / det;
    r.m[0] = (m.m[4] * m.m[8] - m.m[5] * m.m[7]) * inv_det;
    r.m[1] = (m.m[2] * m.m[7] - m.m[1] * m.m[8]) * inv_det;
    r.m[2] = (m.m[1] * m.m[5] - m.m[2] * m.m[4]) * inv_det;
    r.m[3] = (m.m[5] * m.m[6] - m.m[3] * m.m[8]) * inv_det;
    r.m[4] = (m.m[0] * m.m[8] - m.m[2] * m.m[6]) * inv_det;
    r.m[5] = (m.m[2] * m.m[3] - m.m[0] * m.m[5]) * inv_det;
    r.m[6] = (m.m[3] * m.m[7] - m.m[4] * m.m[6]) * inv_det;
    r.m[7] = (m.m[1] * m.m[6] - m.m[0] * m.m[7]) * inv_det;
    r.m[8] = (m.m[0] * m.m[4] - m.m[1] * m.m[3]) * inv_det;
    return r;
}

/** @brief 构造绕 X 轴旋转的 3x3 矩阵（弧度制） */
static inline Lv00Mat3 lv00_mat3_rotation_x(double angle) {
    double c = cos(angle), s = sin(angle);
    Lv00Mat3 r;
    r.m[0] = 1.0;
    r.m[1] = 0.0;
    r.m[2] = 0.0;
    r.m[3] = 0.0;
    r.m[4] = c;
    r.m[5] = -s;
    r.m[6] = 0.0;
    r.m[7] = s;
    r.m[8] = c;
    return r;
}

/** @brief 构造绕 Y 轴旋转的 3x3 矩阵（弧度制） */
static inline Lv00Mat3 lv00_mat3_rotation_y(double angle) {
    double c = cos(angle), s = sin(angle);
    Lv00Mat3 r;
    r.m[0] = c;
    r.m[1] = 0.0;
    r.m[2] = s;
    r.m[3] = 0.0;
    r.m[4] = 1.0;
    r.m[5] = 0.0;
    r.m[6] = -s;
    r.m[7] = 0.0;
    r.m[8] = c;
    return r;
}

/** @brief 构造绕 Z 轴旋转的 3x3 矩阵（弧度制） */
static inline Lv00Mat3 lv00_mat3_rotation_z(double angle) {
    double c = cos(angle), s = sin(angle);
    Lv00Mat3 r;
    r.m[0] = c;
    r.m[1] = -s;
    r.m[2] = 0.0;
    r.m[3] = s;
    r.m[4] = c;
    r.m[5] = 0.0;
    r.m[6] = 0.0;
    r.m[7] = 0.0;
    r.m[8] = 1.0;
    return r;
}

/* ========================================================================
 * 第六部分：Mat4 操作（借鉴 Eigen Matrix4d / Transform）
 *
 * 4x4 齐次矩阵，列主序存储（兼容 OpenGL）。
 * 索引：m[col*4 + row]。
 * ======================================================================== */

/** @brief 构造 4x4 单位矩阵 */
static inline Lv00Mat4 lv00_mat4_identity(void) {
    Lv00Mat4 r;
    memset(r.m, 0, sizeof(r.m));
    r.m[0] = 1.0;
    r.m[5] = 1.0;
    r.m[10] = 1.0;
    r.m[15] = 1.0;
    return r;
}

/** @brief Mat4 乘法：C = A * B（列主序） */
static inline Lv00Mat4 lv00_mat4_mul(Lv00Mat4 a, Lv00Mat4 b) {
    Lv00Mat4 r;
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            int idx = col * 4 + row;
            r.m[idx] = a.m[0 * 4 + row] * b.m[col * 4 + 0] + a.m[1 * 4 + row] * b.m[col * 4 + 1] +
                       a.m[2 * 4 + row] * b.m[col * 4 + 2] + a.m[3 * 4 + row] * b.m[col * 4 + 3];
        }
    }
    return r;
}

/** @brief Mat4 乘 Vec4：out = M * v（列主序） */
static inline Lv00Vec4 lv00_mat4_mul_vec(Lv00Mat4 m, Lv00Vec4 v) {
    return (Lv00Vec4) {m.m[0] * v.x + m.m[4] * v.y + m.m[8] * v.z + m.m[12] * v.w,
                       m.m[1] * v.x + m.m[5] * v.y + m.m[9] * v.z + m.m[13] * v.w,
                       m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z + m.m[14] * v.w,
                       m.m[3] * v.x + m.m[7] * v.y + m.m[11] * v.z + m.m[15] * v.w};
}

/**
 * @brief Mat4 逆矩阵（Gauss-Jordan 消元法）
 *
 * 使用增广矩阵 [M | I] 通过行变换得到 [I | M^{-1}]。
 * 对纯旋转/平移矩阵有更好的数值稳定性。
 *
 * @param[in] m  输入 4x4 矩阵
 * @return 逆矩阵；如果奇异则返回零矩阵
 */
static inline Lv00Mat4 lv00_mat4_inverse(Lv00Mat4 m) {
    Lv00Mat4 inv = lv00_mat4_identity();
    double a[4][4];
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            a[i][j] = m.m[j * 4 + i]; /* 转置为行主序便于消元 */

    for (int i = 0; i < 4; ++i) {
        /* 选主元 */
        double pivot = a[i][i];
        if (fabs(pivot) < LV00_EPSILON) {
            int swap_row = -1;
            for (int k = i + 1; k < 4; ++k) {
                if (fabs(a[k][i]) > LV00_EPSILON) {
                    swap_row = k;
                    break;
                }
            }
            if (swap_row < 0) {
                Lv00Mat4 zero;
                memset(zero.m, 0, sizeof(zero.m));
                return zero;
            }
            for (int j = 0; j < 4; ++j) {
                double tmp = a[i][j];
                a[i][j] = a[swap_row][j];
                a[swap_row][j] = tmp;
            }
            for (int j = 0; j < 16; ++j) {
                /* inv 的索引为 j*4 + row，即列主序储存 */
                int ri = j * 4 + i, rs = j * 4 + swap_row;
                double tmp = inv.m[ri];
                inv.m[ri] = inv.m[rs];
                inv.m[rs] = tmp;
            }
            pivot = a[i][i];
        }
        double inv_p = 1.0 / pivot;
        for (int j = 0; j < 4; ++j)
            a[i][j] *= inv_p;
        for (int j = 0; j < 16; ++j) {
            int ri = j * 4 + i;
            inv.m[ri] *= inv_p;
        }

        for (int k = 0; k < 4; ++k) {
            if (k == i)
                continue;
            double factor = a[k][i];
            for (int j = 0; j < 4; ++j)
                a[k][j] -= factor * a[i][j];
            for (int j = 0; j < 16; ++j) {
                int rk = j * 4 + k, ri = j * 4 + i;
                inv.m[rk] -= factor * inv.m[ri];
            }
        }
    }
    return inv;
}

/** @brief 构造平移矩阵（列主序） */
static inline Lv00Mat4 lv00_mat4_translation(double x, double y, double z) {
    Lv00Mat4 r = lv00_mat4_identity();
    r.m[12] = x;
    r.m[13] = y;
    r.m[14] = z;
    return r;
}

/** @brief 构造缩放矩阵 */
static inline Lv00Mat4 lv00_mat4_scale(double sx, double sy, double sz) {
    Lv00Mat4 r;
    memset(r.m, 0, sizeof(r.m));
    r.m[0] = sx;
    r.m[5] = sy;
    r.m[10] = sz;
    r.m[15] = 1.0;
    return r;
}

/** @brief 构造绕 X 轴旋转的 4x4 矩阵（弧度制） */
static inline Lv00Mat4 lv00_mat4_rotation_x(double angle) {
    double c = cos(angle), s = sin(angle);
    Lv00Mat4 r;
    r.m[0] = 1.0;
    r.m[1] = 0.0;
    r.m[2] = 0.0;
    r.m[3] = 0.0;
    r.m[4] = 0.0;
    r.m[5] = c;
    r.m[6] = s;
    r.m[7] = 0.0;
    r.m[8] = 0.0;
    r.m[9] = -s;
    r.m[10] = c;
    r.m[11] = 0.0;
    r.m[12] = 0.0;
    r.m[13] = 0.0;
    r.m[14] = 0.0;
    r.m[15] = 1.0;
    return r;
}

/** @brief 构造绕 Y 轴旋转的 4x4 矩阵（弧度制） */
static inline Lv00Mat4 lv00_mat4_rotation_y(double angle) {
    double c = cos(angle), s = sin(angle);
    Lv00Mat4 r;
    r.m[0] = c;
    r.m[1] = 0.0;
    r.m[2] = -s;
    r.m[3] = 0.0;
    r.m[4] = 0.0;
    r.m[5] = 1.0;
    r.m[6] = 0.0;
    r.m[7] = 0.0;
    r.m[8] = s;
    r.m[9] = 0.0;
    r.m[10] = c;
    r.m[11] = 0.0;
    r.m[12] = 0.0;
    r.m[13] = 0.0;
    r.m[14] = 0.0;
    r.m[15] = 1.0;
    return r;
}

/** @brief 构造绕 Z 轴旋转的 4x4 矩阵（弧度制） */
static inline Lv00Mat4 lv00_mat4_rotation_z(double angle) {
    double c = cos(angle), s = sin(angle);
    Lv00Mat4 r;
    r.m[0] = c;
    r.m[1] = s;
    r.m[2] = 0.0;
    r.m[3] = 0.0;
    r.m[4] = -s;
    r.m[5] = c;
    r.m[6] = 0.0;
    r.m[7] = 0.0;
    r.m[8] = 0.0;
    r.m[9] = 0.0;
    r.m[10] = 1.0;
    r.m[11] = 0.0;
    r.m[12] = 0.0;
    r.m[13] = 0.0;
    r.m[14] = 0.0;
    r.m[15] = 1.0;
    return r;
}

/**
 * @brief 构造正交投影矩阵（列主序，兼容 OpenGL）
 *
 * @param[in] left   左裁剪面
 * @param[in] right  右裁剪面
 * @param[in] bottom 下裁剪面
 * @param[in] top    上裁剪面
 * @param[in] near   近裁剪面
 * @param[in] far    远裁剪面
 * @return 正交投影矩阵
 */
static inline Lv00Mat4 lv00_mat4_ortho(double left, double right, double bottom, double top, double near, double far) {
    Lv00Mat4 r;
    memset(r.m, 0, sizeof(r.m));
    double rl = right - left, tb = top - bottom, fn = far - near;
    r.m[0] = 2.0 / rl;
    r.m[5] = 2.0 / tb;
    r.m[10] = -2.0 / fn;
    r.m[12] = -(right + left) / rl;
    r.m[13] = -(top + bottom) / tb;
    r.m[14] = -(far + near) / fn;
    r.m[15] = 1.0;
    return r;
}

/**
 * @brief 构造透视投影矩阵（列主序，兼容 OpenGL）
 *
 * @param[in] fov_y   垂直视场角（弧度制）
 * @param[in] aspect  宽高比（width / height）
 * @param[in] near    近裁剪面距离
 * @param[in] far     远裁剪面距离
 * @return 透视投影矩阵
 */
static inline Lv00Mat4 lv00_mat4_perspective(double fov_y, double aspect, double near, double far) {
    Lv00Mat4 r;
    memset(r.m, 0, sizeof(r.m));
    double f = 1.0 / tan(fov_y * 0.5);
    double fn = far - near;
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = -(far + near) / fn;
    r.m[11] = -1.0;
    r.m[14] = -2.0 * far * near / fn;
    return r;
}

/**
 * @brief 构造 Look-At 视图矩阵（列主序）
 *
 * @param[in] eye    相机位置
 * @param[in] center 目标点
 * @param[in] up     上方向向量
 * @return 视图矩阵（世界空间 -> 相机空间）
 */
static inline Lv00Mat4 lv00_mat4_look_at(Lv00Vec3 eye, Lv00Vec3 center, Lv00Vec3 up) {
    Lv00Vec3 f = lv00_vec3_normalize(lv00_vec3_sub(center, eye));
    Lv00Vec3 s = lv00_vec3_normalize(lv00_vec3_cross(f, up));
    Lv00Vec3 u = lv00_vec3_cross(s, f);

    Lv00Mat4 r;
    r.m[0] = s.x;
    r.m[4] = s.y;
    r.m[8] = s.z;
    r.m[12] = -lv00_vec3_dot(s, eye);
    r.m[1] = u.x;
    r.m[5] = u.y;
    r.m[9] = u.z;
    r.m[13] = -lv00_vec3_dot(u, eye);
    r.m[2] = -f.x;
    r.m[6] = -f.y;
    r.m[10] = -f.z;
    r.m[14] = lv00_vec3_dot(f, eye);
    r.m[3] = 0.0;
    r.m[7] = 0.0;
    r.m[11] = 0.0;
    r.m[15] = 1.0;
    return r;
}

/* ========================================================================
 * 第七部分：四元数操作（借鉴 Eigen Quaterniond）
 *
 * 四元数表示为 q = w + xi + yj + zk，用于表示 3D 旋转。
 * 所有角度均为弧度制。
 * ======================================================================== */

/** @brief 构造单位四元数（表示无旋转） */
static inline Lv00Quat lv00_quat_identity(void) {
    return (Lv00Quat) {1.0, 0.0, 0.0, 0.0};
}

/**
 * @brief 从轴-角表示构造四元数
 *
 * @param[in] axis  旋转轴（不需要归一化，内部会归一化）
 * @param[in] angle 旋转角（弧度制）
 * @return 表示绕 axis 旋转 angle 弧度的四元数
 */
static inline Lv00Quat lv00_quat_from_axis_angle(Lv00Vec3 axis, double angle) {
    Lv00Vec3 n = lv00_vec3_normalize(axis);
    double half = angle * 0.5;
    double s = sin(half);
    return (Lv00Quat) {cos(half), n.x * s, n.y * s, n.z * s};
}

/**
 * @brief 从欧拉角构造四元数（roll-pitch-yaw，内旋 ZYX 顺序）
 *
 * @param[in] roll  绕 Z 轴旋转角（弧度）
 * @param[in] pitch 绕 Y 轴旋转角（弧度）
 * @param[in] yaw   绕 X 轴旋转角（弧度）
 * @return 组合旋转四元数
 */
static inline Lv00Quat lv00_quat_from_euler(double roll, double pitch, double yaw) {
    double cr = cos(roll * 0.5), sr = sin(roll * 0.5);
    double cp = cos(pitch * 0.5), sp = sin(pitch * 0.5);
    double cy = cos(yaw * 0.5), sy = sin(yaw * 0.5);
    return (Lv00Quat) {cr * cp * cy + sr * sp * sy, sr * cp * cy - cr * sp * sy, cr * sp * cy + sr * cp * sy,
                       cr * cp * sy - sr * sp * cy};
}

/** @brief 四元数乘法（组合旋转）：q = a * b */
static inline Lv00Quat lv00_quat_mul(Lv00Quat a, Lv00Quat b) {
    return (Lv00Quat) {a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z, a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
                       a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x, a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w};
}

/**
 * @brief 使用四元数旋转 3D 向量
 *
 * @param[in] q  单位四元数（旋转）
 * @param[in] v  待旋转的 3D 向量
 * @return 旋转后的向量
 */
static inline Lv00Vec3 lv00_quat_rotate_vec(Lv00Quat q, Lv00Vec3 v) {
    Lv00Vec3 qv = {q.x, q.y, q.z};
    Lv00Vec3 t = lv00_vec3_scale(lv00_vec3_cross(qv, v), 2.0);
    return (Lv00Vec3) {v.x + q.w * t.x + (qv.y * t.z - qv.z * t.y), v.y + q.w * t.y + (qv.z * t.x - qv.x * t.z),
                       v.z + q.w * t.z + (qv.x * t.y - qv.y * t.x)};
}

/**
 * @brief 四元数转 4x4 旋转矩阵（列主序）
 *
 * @param[in] q  单位四元数
 * @return 等价的 4x4 旋转矩阵
 */
static inline Lv00Mat4 lv00_quat_to_mat4(Lv00Quat q) {
    double xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    double xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    double wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;

    Lv00Mat4 r;
    r.m[0] = 1.0 - 2.0 * (yy + zz);
    r.m[4] = 2.0 * (xy - wz);
    r.m[8] = 2.0 * (xz + wy);
    r.m[12] = 0.0;
    r.m[1] = 2.0 * (xy + wz);
    r.m[5] = 1.0 - 2.0 * (xx + zz);
    r.m[9] = 2.0 * (yz - wx);
    r.m[13] = 0.0;
    r.m[2] = 2.0 * (xz - wy);
    r.m[6] = 2.0 * (yz + wx);
    r.m[10] = 1.0 - 2.0 * (xx + yy);
    r.m[14] = 0.0;
    r.m[3] = 0.0;
    r.m[7] = 0.0;
    r.m[11] = 0.0;
    r.m[15] = 1.0;
    return r;
}

/**
 * @brief 四元数球面线性插值（SLERP）
 *
 * @param[in] a  起始四元数
 * @param[in] b  终止四元数
 * @param[in] t  插值参数 [0, 1]
 * @return 插值结果四元数
 */
static inline Lv00Quat lv00_quat_slerp(Lv00Quat a, Lv00Quat b, double t) {
    double cos_omega = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
    Lv00Quat b2 = b;
    if (cos_omega < 0.0) {
        cos_omega = -cos_omega;
        b2.w = -b2.w;
        b2.x = -b2.x;
        b2.y = -b2.y;
        b2.z = -b2.z;
    }
    double k0, k1;
    if (cos_omega > 0.9999) {
        k0 = 1.0 - t;
        k1 = t;
    } else {
        double sin_omega = sqrt(1.0 - cos_omega * cos_omega);
        double omega = atan2(sin_omega, cos_omega);
        double inv_sin = 1.0 / sin_omega;
        k0 = sin((1.0 - t) * omega) * inv_sin;
        k1 = sin(t * omega) * inv_sin;
    }
    return (Lv00Quat) {k0 * a.w + k1 * b2.w, k0 * a.x + k1 * b2.x, k0 * a.y + k1 * b2.y, k0 * a.z + k1 * b2.z};
}

/* ========================================================================
 * 第八部分：语义化几何变换（借鉴 Eigen Geometry 模块）
 *
 * 将变换矩阵与变换类型绑定，提供类型感知的组合/求逆/点变换 API。
 * ======================================================================== */

/** @brief 构造恒等变换 */
static inline Lv00GeomTransform lv00_transform_identity(void) {
    Lv00GeomTransform t;
    t.type = LV00_TRANSFORM_IDENTITY;
    t.matrix = lv00_mat4_identity();
    t.is_identity = true;
    return t;
}

/** @brief 构造平移变换 */
static inline Lv00GeomTransform lv00_transform_translation(double x, double y, double z) {
    Lv00GeomTransform t;
    t.type = LV00_TRANSFORM_TRANSLATION;
    t.matrix = lv00_mat4_translation(x, y, z);
    t.is_identity = false;
    return t;
}

/** @brief 构造旋转变换（轴-角） */
static inline Lv00GeomTransform lv00_transform_rotation(Lv00Vec3 axis, double angle) {
    Lv00GeomTransform t;
    t.type = LV00_TRANSFORM_ROTATION;
    t.matrix = lv00_quat_to_mat4(lv00_quat_from_axis_angle(axis, angle));
    t.is_identity = false;
    return t;
}

/** @brief 构造缩放变换 */
static inline Lv00GeomTransform lv00_transform_scale(double sx, double sy, double sz) {
    Lv00GeomTransform t;
    t.type = LV00_TRANSFORM_SCALE;
    t.matrix = lv00_mat4_scale(sx, sy, sz);
    t.is_identity = false;
    return t;
}

/**
 * @brief 组合两个变换（右乘：先应用 a 再应用 b，即 T_out = T_b * T_a）
 *
 * @param[in] a  第一个变换（先应用）
 * @param[in] b  第二个变换（后应用）
 * @return 组合变换
 */
static inline Lv00GeomTransform lv00_transform_compose(Lv00GeomTransform a, Lv00GeomTransform b) {
    Lv00GeomTransform t;
    t.type = LV00_TRANSFORM_AFFINE;
    t.matrix = lv00_mat4_mul(b.matrix, a.matrix);
    t.is_identity = false;
    return t;
}

/** @brief 将变换应用于 3D 点（齐次坐标 w=1） */
static inline Lv00Vec3 lv00_transform_apply_point(Lv00GeomTransform t, Lv00Vec3 p) {
    Lv00Vec4 v = {p.x, p.y, p.z, 1.0};
    Lv00Vec4 r = lv00_mat4_mul_vec(t.matrix, v);
    double inv_w = (fabs(r.w) > LV00_EPSILON) ? (1.0 / r.w) : 1.0;
    return (Lv00Vec3) {r.x * inv_w, r.y * inv_w, r.z * inv_w};
}

/** @brief 计算变换的逆 */
static inline Lv00GeomTransform lv00_transform_inverse(Lv00GeomTransform t) {
    Lv00GeomTransform inv = t;
    inv.matrix = lv00_mat4_inverse(t.matrix);
    return inv;
}

/* ========================================================================
 * 第九部分：SIMD 加速（借鉴 Eigen SSE 向量化）
 *
 * 在支持 SSE2 的平台上提供加速版本的核心运算。
 * 当 LV00_USE_SSE2 未定义时，回退到标量实现。
 * ======================================================================== */

#ifdef LV00_USE_SSE2

/** @brief SSE2 加速的 4x4 矩阵乘法（列主序） */
static inline void lv00_mat4_mul_sse2(const double a[16], const double b[16], double out[16]) {
    __m128d b0 = _mm_loadu_pd(b + 0);
    __m128d b1 = _mm_loadu_pd(b + 2);
    __m128d b2 = _mm_loadu_pd(b + 4);
    __m128d b3 = _mm_loadu_pd(b + 6);
    __m128d b4 = _mm_loadu_pd(b + 8);
    __m128d b5 = _mm_loadu_pd(b + 10);
    __m128d b6 = _mm_loadu_pd(b + 12);
    __m128d b7 = _mm_loadu_pd(b + 14);

    for (int i = 0; i < 4; ++i) {
        __m128d a00 = _mm_set1_pd(a[i]);
        __m128d a01 = _mm_mul_pd(a00, b0);
        __m128d a02 = _mm_add_pd(a01, _mm_mul_pd(_mm_set1_pd(a[4 + i]), b2));
        __m128d a03 = _mm_add_pd(a02, _mm_mul_pd(_mm_set1_pd(a[8 + i]), b4));
        __m128d a04 = _mm_add_pd(a03, _mm_mul_pd(_mm_set1_pd(a[12 + i]), b6));
        _mm_storeu_pd(out + i, a04);

        __m128d a10 = _mm_mul_pd(a00, b1);
        __m128d a11 = _mm_add_pd(a10, _mm_mul_pd(_mm_set1_pd(a[4 + i]), b3));
        __m128d a12 = _mm_add_pd(a11, _mm_mul_pd(_mm_set1_pd(a[8 + i]), b5));
        __m128d a13 = _mm_add_pd(a12, _mm_mul_pd(_mm_set1_pd(a[12 + i]), b7));
        _mm_storeu_pd(out + 8 + i, a13);
    }
}

/** @brief SSE2 加速的 Vec4 加法 */
static inline void lv00_vec4_add_sse2(const double a[4], const double b[4], double out[4]) {
    __m128d va = _mm_loadu_pd(a);
    __m128d vb = _mm_loadu_pd(b);
    _mm_storeu_pd(out, _mm_add_pd(va, vb));
    va = _mm_loadu_pd(a + 2);
    vb = _mm_loadu_pd(b + 2);
    _mm_storeu_pd(out + 2, _mm_add_pd(va, vb));
}

#else
/* ── 无 SSE2 时的回退声明（使用标量实现）── */

static inline void lv00_mat4_mul_sse2(const double a[16], const double b[16], double out[16]) {
    Lv00Mat4 A, B, R;
    memcpy(A.m, a, sizeof(A.m));
    memcpy(B.m, b, sizeof(B.m));
    R = lv00_mat4_mul(A, B);
    memcpy(out, R.m, sizeof(R.m));
}

static inline void lv00_vec4_add_sse2(const double a[4], const double b[4], double out[4]) {
    out[0] = a[0] + b[0];
    out[1] = a[1] + b[1];
    out[2] = a[2] + b[2];
    out[3] = a[3] + b[3];
}

#endif /* LV00_USE_SSE2 */

/* ========================================================================
 * 第十部分：点积 / 外积 / 距离等几何运算
 *
 * 提供常用的几何计算工具函数，便于约束图和证明系统调用。
 * ======================================================================== */

/** @brief 2D 点间欧几里得距离 */
static inline double lv00_point_distance_2d(Lv00Vec2 a, Lv00Vec2 b) {
    double dx = a.x - b.x, dy = a.y - b.y;
    return sqrt(dx * dx + dy * dy);
}

/** @brief 3D 点间欧几里得距离 */
static inline double lv00_point_distance_3d(Lv00Vec3 a, Lv00Vec3 b) {
    Lv00Vec3 d = lv00_vec3_sub(a, b);
    return lv00_vec3_norm(d);
}

/**
 * @brief 2D 点到线段的最短距离
 *
 * @param[in] pt      待测点
 * @param[in] line_a  线段端点 A
 * @param[in] line_b  线段端点 B
 * @return 点到线段的最短距离
 */
static inline double lv00_point_to_line_distance_2d(Lv00Vec2 pt, Lv00Vec2 line_a, Lv00Vec2 line_b) {
    Lv00Vec2 ab = lv00_vec2_sub(line_b, line_a);
    Lv00Vec2 ap = lv00_vec2_sub(pt, line_a);
    double ab_len2 = lv00_vec2_dot(ab, ab);
    if (ab_len2 < LV00_EPSILON) {
        return lv00_point_distance_2d(pt, line_a);
    }
    double t = lv00_vec2_dot(ap, ab) / ab_len2;
    if (t < 0.0)
        t = 0.0;
    if (t > 1.0)
        t = 1.0;
    Lv00Vec2 proj = lv00_vec2_add(line_a, lv00_vec2_scale(ab, t));
    return lv00_point_distance_2d(pt, proj);
}

/**
 * @brief 计算三角形法向量（右手定则，未归一化）
 *
 * @param[in] a  顶点 A
 * @param[in] b  顶点 B
 * @param[in] c  顶点 C
 * @return 法向量（方向为 (B-A) x (C-A)）
 */
static inline Lv00Vec3 lv00_triangle_normal(Lv00Vec3 a, Lv00Vec3 b, Lv00Vec3 c) {
    Lv00Vec3 ab = lv00_vec3_sub(b, a);
    Lv00Vec3 ac = lv00_vec3_sub(c, a);
    return lv00_vec3_cross(ab, ac);
}

/**
 * @brief 三角形面积（利用叉积大小的 1/2）
 *
 * @param[in] a  顶点 A
 * @param[in] b  顶点 B
 * @param[in] c  顶点 C
 * @return 三角形面积
 */
static inline double lv00_triangle_area_3d(Lv00Vec3 a, Lv00Vec3 b, Lv00Vec3 c) {
    return 0.5 * lv00_vec3_norm(lv00_triangle_normal(a, b, c));
}

/**
 * @brief 浮点数近似相等比较
 *
 * @param[in] a       第一个值
 * @param[in] b       第二个值
 * @param[in] epsilon 容差阈值
 * @return 在 epsilon 范围内相等返回 true
 */
static inline bool lv00_nearly_equal(double a, double b, double epsilon) {
    return fabs(a - b) < epsilon;
}

/* ═══════════════════════════════════════════════════════════════
 * 使用示例（参考）
 * ═══════════════════════════════════════════════════════════════
 *
 * @code
 * // 构造一个绕 Y 轴旋转 45 度、再平移 (1,0,0) 的变换
 * Lv00GeomTransform rot = lv00_transform_rotation(
 *     (Lv00Vec3){0, 1, 0}, LV00_DEG2RAD(45));
 * Lv00GeomTransform trans = lv00_transform_translation(1, 0, 0);
 * Lv00GeomTransform combined = lv00_transform_compose(trans, rot);
 *
 * // 将变换应用于点
 * Lv00Vec3 pt = {0, 0, 0};
 * Lv00Vec3 result = lv00_transform_apply_point(combined, pt);
 *
 * // 使用四元数 SLERP 插值
 * Lv00Quat q1 = lv00_quat_identity();
 * Lv00Quat q2 = lv00_quat_from_axis_angle((Lv00Vec3){0, 1, 0}, LV00_PI / 2);
 * Lv00Quat qmid = lv00_quat_slerp(q1, q2, 0.5);
 * @endcode
 */

#ifdef __cplusplus
}
#endif

#endif /* LV00_NUMERIC_H */
