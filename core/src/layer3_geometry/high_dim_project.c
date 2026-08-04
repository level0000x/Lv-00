/*
 * @file high_dim_project.c
 * @brief High-dim module - coordinate projection and 3d projection
 * @details Split from high_dim.c
 */

#include "high_dim.h"
#include "high_dim_internal.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "lv/config.h"
#include "lv/lv_json.h"
#include "lv/lv_parse_utils.h"

#include "debug.h"
#include "error_codes.h"
#include "lv_internal.h"
#include "lv/lv_str_utils.h"
#include "lv_utils.h"
#include "stream.h"
#include "stream_context_util.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_xmacro.h"

/* ==================== 坐标投影 ==================== */

/**
 * @brief 高维坐标投影到二维
 *
 * 使用当前投影预设将高维符号坐标投影为二维投影坐标。
 *
 * 支持的映射类型：
 *   HIGH_DIM_MAP_TO_X/TO_Y：直接投影到对应坐标轴（线性标定）
 *   HIGH_DIM_MAP_FOLD/DISCARD：维度折叠（计入折叠信息）
 *   HIGH_DIM_MAP_LINEAR：加权线性组合（scale 为权重、offset 为偏置）
 *   HIGH_DIM_MAP_LOG：对数尺度映射（压缩大动态范围、保留符号）
 *   HIGH_DIM_MAP_EXP：指数尺度映射（放大差异，带溢出钳制）
 *   HIGH_DIM_MAP_PCA：主成分方向加权投影（单点退化形式）
 *   HIGH_DIM_MAP_T_SNE：t 分布核归一化近似（压缩极端值）
 *
 * @param manager         管理器指针
 * @param block_id        块 ID
 * @param high_dim_coords 高维坐标数组
 * @param coord_count     坐标数量
 * @param projected       输出参数，接收投影结果
 * @return lv_OK 成功，错误码表示失败原因
 */
typedef struct {
    HighDimProjectedCoord *projected;
    double scaled_value;
    double coord_value;
    const HighDimAxisMapping *mapping;
    char *folded_dims;
    size_t *folded_pos;
    int *folded_count;
} ProjectContext;

typedef void (*ProjectFn)(const ProjectContext *ctx);

static void project_to_x(const ProjectContext *ctx) {
    ctx->projected->x += ctx->scaled_value;
}
static void project_to_y(const ProjectContext *ctx) {
    ctx->projected->y += ctx->scaled_value;
}
static void project_log(const ProjectContext *ctx) {
    double v = ctx->coord_value;
    double logv = log(1.0 + fabs(v));
    ctx->projected->x += ctx->mapping->scale * (v < 0.0 ? -logv : logv) + ctx->mapping->offset;
}
static void project_exp(const ProjectContext *ctx) {
    double ev = exp(ctx->coord_value);
    if (ev > 1e12) ev = 1e12;
    if (ev < -1e12) ev = -1e12;
    ctx->projected->x += ctx->mapping->scale * ev + ctx->mapping->offset;
}
static void project_tsne(const ProjectContext *ctx) {
    double v = ctx->coord_value;
    ctx->projected->x += ctx->mapping->scale * v / (1.0 + fabs(v));
}
static void project_fold_discard(const ProjectContext *ctx) {
    if (*ctx->folded_count < 3) {
        char dim_info[32];
        high_dim_snprintf(dim_info, sizeof(dim_info), "%d:%.2f", ctx->mapping->axis_index, ctx->coord_value);
        lv_str_append_sep(ctx->folded_dims, 256, ctx->folded_pos, ", ", dim_info);
    }
    (*ctx->folded_count)++;
}

static const ProjectFn kProjectHandlers[] = {
    [HIGH_DIM_MAP_TO_X] = project_to_x,
    [HIGH_DIM_MAP_TO_Y] = project_to_y,
    [HIGH_DIM_MAP_LINEAR] = project_to_x,
    [HIGH_DIM_MAP_LOG] = project_log,
    [HIGH_DIM_MAP_EXP] = project_exp,
    [HIGH_DIM_MAP_PCA] = project_to_x,
    [HIGH_DIM_MAP_T_SNE] = project_tsne,
    [HIGH_DIM_MAP_FOLD] = project_fold_discard,
    [HIGH_DIM_MAP_DISCARD] = project_fold_discard,
};

int high_dim_project_coordinates(HighDimManager *manager, int block_id, const SymbolicCoord **high_dim_coords,
                                 int coord_count, HighDimProjectedCoord *projected) {
    /*
     * 【边界检查和错误处理】
     *   1. NULL 指针检查：manager、high_dim_coords、projected 必须非空
     *   2. coord_count 非负检查：虽然 >= 0 允许空数组，但 > preset->mapping_count
     *      时多余的坐标会因循环条件 (i < coord_count) 被跳过 —— 属于安全行为
     *   3. coord_count 不足时（< preset->mapping_count），循环仅处理现有坐标，
     *      缺失维度默认值为 0.0 —— 投影结果仍可用但精度下降
     *   4. high_dim_coords 为 NULL 时，coord_count 必须为 0；如果 coord_count > 0
     *      则本函数信任调用者保证的数组有效性
     *   5. 如果当前块没有有效的投影预设（preset == NULL），返回 INVALID_STATE
     *   6. projected->folded_info 缓冲区大小为 HIGH_DIM_FOLDED_INFO_MAX，
     *      确保 high_dim_snprintf 不会越界写入
     */
    if (!manager || !high_dim_coords || !projected || coord_count < 0) {
        return lv_ERROR_INVALID_PARAM;
    }

    HighDimAbstractBlock *block = high_dim_get_block(manager, block_id);
    if (!block) {
        return lv_ERROR_NOT_FOUND;
    }

    const HighDimProjectionPreset *preset = high_dim_get_current_preset(manager, block_id);
    if (!preset) {
        return lv_ERROR_INVALID_STATE;
    }

    /* 初始化投影结果 */
    projected->x = 0.0;
    projected->y = 0.0;
    projected->is_valid = true;
    projected->folded_info[0] = '\0';

    /* 收集折叠维度的信息 */
    char folded_dims[256] = "";
    size_t folded_pos = 0;
    int folded_count = 0;

    /* 应用维度映射 */
    for (int i = 0; i < preset->mapping_count && i < coord_count; i++) {
        const HighDimAxisMapping *mapping = &preset->mappings[i];

        /* 获取坐标数值近似值 */
        double coord_value = 0.0;
        if (high_dim_coords[i]) {
            coord_value = symbolic_coord_to_double(high_dim_coords[i]);
        }

        double scaled_value = coord_value * mapping->scale + mapping->offset;

        if ((unsigned)mapping->mapping_type < sizeof(kProjectHandlers)/sizeof(kProjectHandlers[0]) && kProjectHandlers[mapping->mapping_type]) {
            ProjectContext pctx = {projected, scaled_value, coord_value, mapping, folded_dims, &folded_pos, &folded_count};
            kProjectHandlers[mapping->mapping_type](&pctx);
        }
    }

    /* 应用2D变换 */
    double x = projected->x;
    double y = projected->y;
    projected->x = preset->transform.m[0][0] * x + preset->transform.m[0][1] * y;
    projected->y = preset->transform.m[1][0] * x + preset->transform.m[1][1] * y;

    /* 设置折叠维度信息 */
    if (folded_count > 0) {
        high_dim_snprintf(projected->folded_info, sizeof(projected->folded_info), "折叠维度(%d): %s", folded_count,
                          folded_dims);
    }

    return lv_OK;
}

/**
 * @brief 对投影坐标应用二维变换
 *
 * @param coord     投影坐标
 * @param transform 二维变换矩阵
 * @param result    输出参数，接收变换结果
 * @return lv_OK 成功，错误码表示失败原因
 */
int high_dim_apply_transform(const HighDimProjectedCoord *coord, const HighDimTransform2D *transform,
                             HighDimProjectedCoord *result) {
    if (!coord || !transform || !result) {
        return lv_ERROR_INVALID_PARAM;
    }

    double x = coord->x;
    double y = coord->y;

    result->x = transform->m[0][0] * x + transform->m[0][1] * y;
    result->y = transform->m[1][0] * x + transform->m[1][1] * y;
    result->is_valid = coord->is_valid;
    lv_strlcpy(result->folded_info, coord->folded_info, sizeof(result->folded_info));

    return lv_OK;
}

/**
 * @brief 创建旋转变换矩阵
 *
 * @param angle_rad 旋转角度（弧度）
 * @param transform 输出参数，接收变换矩阵
 * @return lv_OK 成功，错误码表示失败原因
 */
int high_dim_create_rotation_transform(double angle_rad, HighDimTransform2D *transform) {
    if (!transform)
        return lv_ERROR_INVALID_PARAM;

    double cos_a = cos(angle_rad);
    double sin_a = sin(angle_rad);

    transform->m[0][0] = cos_a;
    transform->m[0][1] = -sin_a;
    transform->m[1][0] = sin_a;
    transform->m[1][1] = cos_a;

    return lv_OK;
}

/**
 * @brief 创建缩放变换矩阵
 *
 * @param scale_x   x 轴缩放因子
 * @param scale_y   y 轴缩放因子
 * @param transform 输出参数，接收变换矩阵
 * @return lv_OK 成功，错误码表示失败原因
 */
int high_dim_create_scale_transform(double scale_x, double scale_y, HighDimTransform2D *transform) {
    if (!transform)
        return lv_ERROR_INVALID_PARAM;

    transform->m[0][0] = scale_x;
    transform->m[0][1] = 0.0;
    transform->m[1][0] = 0.0;
    transform->m[1][1] = scale_y;

    return lv_OK;
}
/* ==================== 4D到3D投影 ==================== */

/**
 * @brief 应用 4x4 旋转矩阵（SO(4) 群元素）到 4D 向量
 *
 * 行主序矩阵乘法：out[r] = sum_c rot[r][c] * in[c]
 *
 * @param in  输入的 4D 向量
 * @param rot 4x4 旋转矩阵（行主序，即 rot[4][4]）
 * @param out 输出的 4D 向量
 * @return lv_OK 成功，lv_ERROR_INVALID_PARAM 参数无效
 */
static int high_dim_rotate_4d(const double in[4], const double rot[4][4], double out[4]) {
    if (!in || !rot || !out) {
        return lv_ERROR_INVALID_PARAM;
    }
    for (int r = 0; r < 4; r++) {
        out[r] = rot[r][0] * in[0] + rot[r][1] * in[1] + rot[r][2] * in[2] + rot[r][3] * in[3];
    }
    return lv_OK;
}

/* ==================== 4D/高维 -> 3D 投影 ==================== */

/**
 * @brief 3D 投影上下文：承载各投影实现共享的输入/输出数据
 *
 * 透视/正交/立体三种投影实现共用同一签名，通过本结构体交换数据。
 */
typedef struct {
    double px, py, pz, pw;  /**< 输入：按轴选择提取的 3 个坐标与深度轴值 */
    double camera_distance; /**< 输入：摄像机到原点的距离（透视投影使用） */
    double factor;          /**< 输出：投影缩放因子（正交恒为 1.0） */
    double depth;           /**< 输出：深度值（透视/立体为 factor，正交为 pw） */
    double *coord_3d;       /**< 输出：3D 坐标数组（长度 >= 3） */
} ProjectTo3dContext;

/** @brief 3D 投影实现签名：成功返回 lv_OK，参数非法返回 lv_ERROR_INVALID_PARAM */
typedef int (*ProjectTo3dFn)(ProjectTo3dContext *ctx);

static int project_to_3d_perspective(ProjectTo3dContext *ctx) {
    /*
     * 透视投影模式
     *
     * 核心思想：将深度轴坐标 w 视为"与摄像机的距离"，
     * 通过透视除法实现远小近大的效果。
     * 退化处理：dim_count < 4 时 w=0，factor=1，直接取前三维。
     */
    if (ctx->camera_distance <= 0.0) {
        lv_set_error(lv_ERROR_INVALID_PARAM, "4D透视投影失败：camera_distance=%.2f无效，必须大于0",
                     ctx->camera_distance);
        return lv_ERROR_INVALID_PARAM;
    }
    double denominator = ctx->camera_distance - ctx->pw;
    if (fabs(denominator) < ctx->camera_distance * 1e-6 + 1e-12) {
        double min_denom = ctx->camera_distance * 1e-6 + 1e-12;
        denominator = (denominator >= 0) ? min_denom : -min_denom;
        lv_set_error(lv_OK,
                     "4D透视投影：w=%.4f接近摄像机距离d=%.4f，"
                     "已应用奇点保护（截断因子=%.0fx）",
                     ctx->pw, ctx->camera_distance, ctx->camera_distance / min_denom);
    }
    ctx->factor = ctx->camera_distance / denominator;
    if (ctx->factor > 1e12) {
        ctx->factor = 1e12;
    }
    if (ctx->factor < -1e12) {
        ctx->factor = -1e12;
    }
    ctx->depth = ctx->factor;
    ctx->coord_3d[0] = ctx->px * ctx->factor;
    ctx->coord_3d[1] = ctx->py * ctx->factor;
    ctx->coord_3d[2] = ctx->pz * ctx->factor;
    return lv_OK;
}

static int project_to_3d_orthographic(ProjectTo3dContext *ctx) {
    /*
     * 正交投影模式
     *
     * 简单丢弃深度轴及以上的所有维度，是"轴对齐切片"。
     * 没有透视效果，保距性好（保留 x,y,z 的真实比例）。
     */
    ctx->factor = 1.0;
    ctx->depth = ctx->pw;
    ctx->coord_3d[0] = ctx->px;
    ctx->coord_3d[1] = ctx->py;
    ctx->coord_3d[2] = ctx->pz;
    return lv_OK;
}

static int project_to_3d_stereographic(ProjectTo3dContext *ctx) {
    /*
     * 立体投影模式（stereographic）
     *
     * 将 4D 球面 S^3 的点 (x,y,z,w) 从南极点 (0,0,0,1) 投影到
     * 赤道超平面 w=0：
     *   x' = x / (1-w), y' = y / (1-w), z' = z / (1-w)
     * 保角映射，无透视收缩。当 w 接近极点 1 时分母趋于 0，
     * 使用最小阈值做奇点保护。
     */
    double denom = 1.0 - ctx->pw;
    if (fabs(denom) < 1e-12) {
        denom = (denom >= 0) ? 1e-12 : -1e-12;
        lv_set_error(lv_OK, "4D立体投影：w=%.4f接近极点，已应用奇点保护。", ctx->pw);
    }
    ctx->factor = 1.0 / denom;
    ctx->depth = ctx->factor;
    ctx->coord_3d[0] = ctx->px * ctx->factor;
    ctx->coord_3d[1] = ctx->py * ctx->factor;
    ctx->coord_3d[2] = ctx->pz * ctx->factor;
    return lv_OK;
}

/** @brief 投影模式 -> 投影实现函数 查找表（0=透视, 1=正交, 2=立体投影） */
static const ProjectTo3dFn kProjectTo3dHandlers[] = {
    [0] = project_to_3d_perspective,
    [1] = project_to_3d_orthographic,
    [2] = project_to_3d_stereographic,
};

int high_dim_project_to_3d_full(const double *coord_nd, int dim_count, double camera_distance, int projection_mode,
                                const int *axis_keep, const double *rotation_4d, int fold_strategy,
                                double *coord_3d, double *depth_out, double *proj_matrix, int *clip_result) {
    /**
     * @brief 将4D及以上坐标投影到3D空间（完整版）
     *
     * 【实现概述】
     *   本函数实现从高维空间（4D及以上）到三维空间的完整投影变换，
     *   在基础投影之外支持旋转、轴选择、5D+ 折叠、投影矩阵输出、
     *   深度缓冲区与视锥体裁剪。
     *
     * 【支持的三类投影模式】
     *   - 透视投影（0）：w 坐标作为“深度”维度，factor = d/(d-w)，
     *     远点收缩、近点放大，产生类似 3D 透视的效果
     *   - 正交投影（1）：轴选择后直接取 3 个坐标轴，保距性好
     *   - 立体投影（2，stereographic）：将 4D 球面 S^3 的点
     *     (x,y,z,w) 投影到 R^3：x'=x/(1-w), y'=y/(1-w), z'=z/(1-w)，
     *     保角、无透视收缩，适合可视化球面上的流形结构
     *
     * 【已实现的完整功能】
     *   1. 旋转投影 —— 通过 rotation_4d（4x4 行主序 SO(4) 矩阵）在投影前
     *      旋转前 4 维坐标，可把任意方向对齐到观测平面
     *   2. 立体投影 —— projection_mode=2，S^3 到 R^3 的球极投影，含极点保护
     *   3. 正交轴选择 —— axis_keep[3] 指定保留的 3 个轴索引
     *      （未指定时默认保留第 0/1/2 轴，第 4 维作为深度 w）
     *   4. 5D及以上折叠 —— fold_strategy=1 时，第 5 维及以上的维度按
     *      指数衰减权重 w_k = 2^(-(k-4)) 加权折叠叠加到三个输出轴
     *   5. 投影矩阵输出 —— 输出 4x3 行主序投影矩阵（正交：轴选择矩阵；
     *      透视/立体：轴选择 × 缩放因子的线性近似），用于逆投影与误差分析
     *   6. 深度缓冲区 —— depth_out 输出透视深度（透视因子 / 深度轴值）
     *   7. 视锥体裁剪 —— clip_result 输出该点是否落在视锥体内
     *      （近平面 0.1、远平面 100.0，NDC 范围 [-1,1]）
     *
     * @param coord_nd     输入的高维坐标数组（长度 >= dim_count）
     * @param dim_count    输入坐标的维度数（>= 1）
     * @param camera_distance 摄像机到原点的距离（透视投影使用，必须 > 0）
     * @param projection_mode 投影模式：0=透视, 1=正交, 2=立体投影
     * @param axis_keep    保留的 3 个轴索引数组（NULL 时默认 {0,1,2}）
     * @param rotation_4d  4x4 行主序旋转矩阵（NULL 表示不旋转）
     * @param fold_strategy 5D+ 折叠策略：0=丢弃, 1=指数衰减加权折叠
     * @param coord_3d     输出的3D坐标数组（长度 >= 3）
     * @param depth_out    输出深度值（可为 NULL）
     * @param proj_matrix  输出 4x3 行主序投影矩阵（可为 NULL）
     * @param clip_result  输出视锥体裁剪结果（1=在视锥体内，可为 NULL）
     * @return lv_OK 投影成功
     *         lv_ERROR_INVALID_PARAM 参数无效
     */
    if (!coord_nd || !coord_3d || dim_count < 1) {
        return lv_ERROR_INVALID_PARAM;
    }

    /* 初始化输出为0 */
    coord_3d[0] = 0.0;
    coord_3d[1] = 0.0;
    coord_3d[2] = 0.0;

    /* 默认轴选择：前 3 维 */
    int keep[3] = { 0, 1, 2 };
    if (axis_keep) {
        for (int i = 0; i < 3; i++) {
            if (axis_keep[i] < 0 || axis_keep[i] >= dim_count) {
                lv_set_error(lv_ERROR_INVALID_PARAM, "3D投影失败：axis_keep[%d]=%d超出维度范围[0,%d)",
                             i, axis_keep[i], dim_count);
                return lv_ERROR_INVALID_PARAM;
            }
            keep[i] = axis_keep[i];
        }
    }

    /* 提取前4维并应用旋转（旋转仅作用于前4维） */
    double p4[4] = { 0.0, 0.0, 0.0, 0.0 };
    for (int d = 0; d < 4 && d < dim_count; d++) {
        p4[d] = coord_nd[d];
    }
    if (rotation_4d) {
        const double(*rm)[4] = (const double(*)[4])rotation_4d;
        double r4[4];
        if (high_dim_rotate_4d(p4, rm, r4) != lv_OK) {
            return lv_ERROR_INVALID_PARAM;
        }
        for (int d = 0; d < 4; d++) {
            p4[d] = r4[d];
        }
    }

    /* 按轴选择提取输出坐标；深度 w 取前4维中未被选中的轴 */
    double px, py, pz;
    px = (keep[0] < 4) ? p4[keep[0]] : coord_nd[keep[0]];
    py = (keep[1] < 4) ? p4[keep[1]] : coord_nd[keep[1]];
    pz = (keep[2] < 4) ? p4[keep[2]] : coord_nd[keep[2]];

    bool used_axis[4] = { false, false, false, false };
    for (int i = 0; i < 3; i++) {
        if (keep[i] >= 0 && keep[i] < 4) {
            used_axis[keep[i]] = true;
        }
    }
    int w_axis = 3;
    for (int a = 0; a < 4; a++) {
        if (!used_axis[a]) {
            w_axis = a;
            break;
        }
    }
    double pw = (dim_count > w_axis) ? p4[w_axis] : 0.0;

    /* 按投影模式分发到对应投影实现函数（查找表替代 switch 分支链） */
    ProjectTo3dContext ctx = {
        .px = px,
        .py = py,
        .pz = pz,
        .pw = pw,
        .camera_distance = camera_distance,
        .factor = 1.0,
        .depth = 1.0,
        .coord_3d = coord_3d,
    };
    if ((unsigned) projection_mode >= lv_ARRAY_SIZE(kProjectTo3dHandlers) || !kProjectTo3dHandlers[projection_mode]) {
        lv_set_error(lv_ERROR_INVALID_PARAM,
                     "3D投影失败：不支持的projection_mode=%d（有效值：0=透视, 1=正交, 2=立体投影）",
                     projection_mode);
        return lv_ERROR_INVALID_PARAM;
    }
    int proj_rc = kProjectTo3dHandlers[projection_mode](&ctx);
    if (proj_rc != lv_OK)
        return proj_rc;
    double factor = ctx.factor;
    double depth = ctx.depth;

    /* 5D及以上维度的加权折叠 */
    if (fold_strategy == 1 && dim_count > 4) {
        double extra = 0.0;
        for (int k = 4; k < dim_count; k++) {
            double weight = pow(0.5, (double)(k - 4));
            extra += coord_nd[k] * weight;
        }
        double fold_amount = extra * 0.25;
        coord_3d[0] += fold_amount;
        coord_3d[1] += fold_amount;
        coord_3d[2] += fold_amount;
        if (extra != 0.0) {
            lv_set_error(lv_OK,
                         "3D投影：已将第5维及以上的%d个维度加权折叠到3D坐标"
                         "（权重按指数衰减 w_k=2^(-(k-4))）",
                         dim_count - 4);
        }
    }

    /* 投影矩阵输出：4x3 行主序，P[r][c]=第r个输出轴对第c个输入轴(0..3)的系数 */
    if (proj_matrix) {
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 4; c++) {
                proj_matrix[r * 4 + c] = 0.0;
            }
        }
        if (projection_mode == 1) {
            /* 正交：轴选择矩阵（单位基） */
            for (int r = 0; r < 3; r++) {
                if (keep[r] < 4) {
                    proj_matrix[r * 4 + keep[r]] = 1.0;
                }
            }
        } else {
            /* 透视/立体：轴选择 × 缩放因子的线性近似（忽略 w 的非线性项） */
            for (int r = 0; r < 3; r++) {
                if (keep[r] < 4) {
                    proj_matrix[r * 4 + keep[r]] = factor;
                }
            }
        }
    }

    /* 深度缓冲区输出 */
    if (depth_out) {
        *depth_out = depth;
    }

    /* 视锥体裁剪：NDC 范围 [-1,1]，深度范围 [0.1, 100.0] */
    if (clip_result) {
        int in_frustum = 1;
        if (fabs(coord_3d[0]) > 1.0 || fabs(coord_3d[1]) > 1.0 || fabs(coord_3d[2]) > 1.0) {
            in_frustum = 0;
        }
        if (depth < 0.1 || depth > 100.0) {
            in_frustum = 0;
        }
        *clip_result = in_frustum;
    }

    return lv_OK;
}

int high_dim_project_to_3d(const double *coord_4d, int dim_count, double camera_distance, int projection_mode,
                           double *coord_3d) {
    /**
     * @brief 将4D及以上坐标投影到3D空间
     *
     * 【实现概述】
     *   本函数为 high_dim_project_to_3d_full() 的便捷包装，使用默认参数：
     *     - axis_keep     = NULL（默认保留前 3 维）
     *     - rotation_4d   = NULL（不旋转）
     *     - fold_strategy = 1（5D+ 维度指数衰减加权折叠）
     *     - depth/proj_matrix/clip 输出均关闭
     *
     * 【支持的投影模式】
     *   - 透视投影（0）：模拟4D摄像机，w坐标作为"深度"维度，
     *     远点收缩、近点放大，产生类似3D透视的效果
     *   - 正交投影（1）：直接取前3维坐标（含5D+加权折叠），
     *     仅保留前3维坐标（x, y, z）
     *
     * 【数学原理 - 4D透视投影】
     *   给定4D点 P = (x, y, z, w)，摄像机位置在 (0, 0, 0, d) 处，
     *   其中 d = camera_distance：
     *
     *   投影公式（第4维充当"深度"）：
     *     factor = d / (d - w)                    （透视缩放因子）
     *     x' = x * factor                         （投影到3D的x）
     *     y' = y * factor                         （投影到3D的y）
     *     z' = z * factor                         （投影到3D的z）
     *
     *   当 w → d 时，factor → ∞（点在摄像机平面上，产生极点奇异性）
     *   当 w → -∞ 时，factor → 0（远点收缩到原点）
     *   当 w = 0 时，factor = 1（4D原点平面上的点不变）
     *
     * 【已实现功能】
     *   1. 透视投影（PROJECTION_PERSPECTIVE）:
     *      - 4D点 (x,y,z,w) -> 3D点 (x',y',z')
     *      - 自适应降维：dim_count < 4 时自动退化为正交投影
     *      - 奇点保护：|d - w| < epsilon 时使用截断因子
     *   2. 正交投影（PROJECTION_ORTHOGRAPHIC）:
     *      - 直接取前3维坐标，丢弃第4维及以上
     *      - dim_count < 3 时用0填充缺失维度
     *   3. 旋转/轴选择/立体投影/5D+折叠/投影矩阵/深度缓冲/视锥体裁剪
     *      等完整能力请使用 high_dim_project_to_3d_full()
     *   4. 参数验证：
     *      - NULL指针检查
     *      - dim_count合法性（>= 1）
     *      - camera_distance > 0 检查
     *      - projection_mode 枚举值验证
     *
     * 【参数说明】
     * @param coord_4d    输入的高维坐标数组（长度 >= dim_count）
     *                    coord_4d[0]=x, coord_4d[1]=y, coord_4d[2]=z, coord_4d[3]=w
     * @param dim_count   输入坐标的维度数（>= 1）
     *                    dim_count=4 为完整的4D投影，<4 退化处理
     * @param camera_distance 摄像机到原点的距离（透视投影使用，必须 > 0）
     *                    典型值：3.0-10.0，值越大透视效果越弱（接近正交）
     * @param projection_mode 投影模式：
     *                    0 = PROJECTION_PERSPECTIVE（透视投影）
     *                    1 = PROJECTION_ORTHOGRAPHIC（正交投影）
     * @param coord_3d    输出的3D坐标数组（长度 >= 3），调用者分配内存
     *                    coord_3d[0]=x', coord_3d[1]=y', coord_3d[2]=z'
     *
     * @return lv_OK 投影成功
     *         lv_ERROR_INVALID_PARAM 参数无效（NULL指针、非法维度、非法距离）
     */
    return high_dim_project_to_3d_full(coord_4d, dim_count, camera_distance, projection_mode, NULL, NULL, 1,
                                       coord_3d, NULL, NULL, NULL);
}
