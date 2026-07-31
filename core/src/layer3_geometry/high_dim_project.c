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
 * @param manager         管理器指针
 * @param block_id        块 ID
 * @param high_dim_coords 高维坐标数组
 * @param coord_count     坐标数量
 * @param projected       输出参数，接收投影结果
 * @return lv_OK 成功，错误码表示失败原因
 */
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

        switch (mapping->mapping_type) {
            case HIGH_DIM_MAP_TO_X:
                projected->x += scaled_value;
                break;
            case HIGH_DIM_MAP_TO_Y:
                projected->y += scaled_value;
                break;
            case HIGH_DIM_MAP_FOLD:
            case HIGH_DIM_MAP_DISCARD:
                if (folded_count < 3) {
                    char dim_info[32];
                    high_dim_snprintf(dim_info, sizeof(dim_info), "%d:%.2f", mapping->axis_index, coord_value);
                    lv_str_append_sep(folded_dims, sizeof(folded_dims), &folded_pos, ", ", dim_info);
                }
                folded_count++;
                break;
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

int high_dim_project_to_3d(const double *coord_4d, int dim_count, double camera_distance, int projection_mode,
                           double *coord_3d) {
    /**
     * @brief 将4D及以上坐标投影到3D空间
     *
     * 【实现概述】
     *   本函数实现从高维空间（4D及以上）到三维空间的投影变换。
     *   支持两种投影模式：
     *     - 透视投影（perspective）：模拟4D摄像机，w坐标作为"深度"维度，
     *       远点收缩、近点放大，产生类似3D透视的效果
     *     - 正交投影（orthographic）：直接丢弃第4维及以上的维度，
     *       仅保留前3维坐标（x, y, z）
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
     *   此公式的几何直观：
     *   - 将4D空间视为3D空间沿w轴的"堆叠"
     *   - 摄像机位于w轴正半轴，距离原点d处
     *   - 每个3D切片(w=const)被缩放后投影到w=0的超平面上
     *
     * 【已实现功能】
     *   1. 透视投影（PROJECTION_PERSPECTIVE）:
     *      - 4D点 (x,y,z,w) -> 3D点 (x',y',z')
     *      - 自适应降维：dim_count < 4 时自动退化为正交投影
     *      - 奇点保护：|d - w| < epsilon 时使用截断因子
     *   2. 正交投影（PROJECTION_ORTHOGRAPHIC）:
     *      - 直接取前3维坐标，丢弃第4维及以上
     *      - dim_count < 3 时用0填充缺失维度
     *   3. 参数验证：
     *      - NULL指针检查
     *      - dim_count合法性（>= 1）
     *      - camera_distance > 0 检查
     *      - projection_mode 枚举值验证
     *
     * 【仍为桩函数的部分（需要外部依赖或后续版本实现）】
     *   1. 旋转投影 —— 当前投影假设坐标轴已对齐，不支持4D旋转后的投影
     *      （需要4D旋转矩阵，即 SO(4) 群元素）
     *   2. 立体投影（stereographic）—— 4D球面 S^3 到 3D空间 R^3 的投影
     *   3. 正交投影的轴选择 —— 当前固定取前3维，不支持用户指定保留哪3个维度
     *   4. 5D及以上 —— dim_count > 4 时，第5维及以上的折叠策略不完善
     *      （应支持级联投影或加权折叠）
     *   5. 投影矩阵输出 —— 当前仅输出投影后的3D坐标，不输出4x3投影矩阵
     *      （用于后续的逆投影和误差分析）
     *   6. 深度缓冲区 —— 不维护投影深度信息用于遮挡剔除
     *   7. 视锥体裁剪 —— 不检查投影后的点是否在视锥体范围内
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
    if (!coord_4d || !coord_3d || dim_count < 1) {
        return lv_ERROR_INVALID_PARAM;
    }

    /* 初始化输出为0 */
    coord_3d[0] = 0.0;
    coord_3d[1] = 0.0;
    coord_3d[2] = 0.0;

    if (projection_mode == 0) {
        /*
         * 透视投影模式
         *
         * 核心思想：将第4维坐标w视为"与摄像机的距离"，
         * 通过透视除法实现远小近大的效果。
         *
         * 退化处理：dim_count < 4 时，w=0（原点平面），直接取前三维。
         */
        if (dim_count >= 4) {
            if (camera_distance <= 0.0) {
                /* 摄像机距离必须为正，否则投影公式中的分母 d-w 可能为0 */
                lv_set_error(lv_ERROR_INVALID_PARAM, "4D透视投影失败：camera_distance=%.2f无效，必须大于0",
                             camera_distance);
                return lv_ERROR_INVALID_PARAM;
            }

            double w = coord_4d[3]; /* 第4维坐标 */
            double denominator = camera_distance - w;

            /*
             * 奇点保护：当 w 接近 camera_distance 时，投影点趋于无穷。
             * 使用最小阈值避免除零和数值溢出。
             */
            if (fabs(denominator) < camera_distance * 1e-6 + 1e-12) {
                double min_denom = camera_distance * 1e-6 + 1e-12;
                denominator = (denominator >= 0) ? min_denom : -min_denom;
                lv_set_error(lv_OK,
                             "4D透视投影：w=%.4f接近摄像机距离d=%.4f，"
                             "已应用奇点保护（截断因子=%.0fx）",
                             w, camera_distance, camera_distance / min_denom);
            }

            double factor = camera_distance / denominator;

            /* 防止 factor 过大导致坐标溢出 */
            if (factor > 1e12)
                factor = 1e12;
            if (factor < -1e12)
                factor = -1e12;

            coord_3d[0] = coord_4d[0] * factor;
            coord_3d[1] = coord_4d[1] * factor;
            coord_3d[2] = coord_4d[2] * factor;
        } else {
            /*
             * 维度不足4时的退化处理：
             * 直接复制前dim_count个维度到3D坐标，
             * 缺失维度用0填充。
             */
            for (int d = 0; d < 3 && d < dim_count; d++) {
                coord_3d[d] = coord_4d[d];
            }
        }
    } else if (projection_mode == 1) {
        /*
         * 正交投影模式
         *
         * 简单丢弃第4维及以上的所有维度。
         * 这是一种"轴对齐切片"——将高维点沿w轴方向垂直投影到3D超平面上。
         * 没有透视效果，保距性好（保留x,y,z的真实比例）。
         *
         * 缺失维度（dim_count < 3）：用0填充。
         * 多余维度（dim_count > 4）：第4维及以上的信息完全丢失。
         *   后续版本应支持维度加权折叠（如 PCA 降维）以减少信息损失。
         */
        for (int d = 0; d < 3; d++) {
            coord_3d[d] = (d < dim_count) ? coord_4d[d] : 0.0;
        }
    } else {
        lv_set_error(lv_ERROR_INVALID_PARAM, "4D投影失败：不支持的projection_mode=%d（有效值：0=透视, 1=正交）",
                     projection_mode);
        return lv_ERROR_INVALID_PARAM;
    }

    return lv_OK;
}

