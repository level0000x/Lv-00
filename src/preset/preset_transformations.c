/**
 * @file preset_transformations.c
 * @brief 几何变换预设函数块 - 实现
 *
 * @details 实现几何变换模块的所有预设函数块。
 *          采用统一的注册接口 preset_blocks_register_simple。
 *          共16个预设，涵盖平移、旋转、反射、位似、仿射变换等。
 *
 * @module Transformations
 * @category PRESET_CATEGORY_TRANSFORMATION
 * @version 1.0.0
 */

#include "preset_transformations.h"

#include <string.h>

#include "lv00_internal.h"
#include "lv00_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ============================================================
 * 预设数量定义
 * ============================================================ */

/** 几何变换模块预设函数块总数 */
#define TRANSFORMATIONS_PRESET_COUNT 16

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/**
 * @brief 注册单个几何变换预设
 *
 * 辅助函数，用于简化预设注册过程。
 * 所有几何变换预设都属于 PRESET_CATEGORY_TRANSFORMATION 类别。
 *
 * @param name 预设名称
 * @param description 中文描述
 * @param input_types 输入类型数组
 * @param input_count 输入数量
 * @param output_type 输出类型
 * @param math_def 数学定义（LaTeX格式）
 * @param complexity 时间复杂度
 * @param is_constructive 是否构造性
 * @param is_reversible 是否可逆
 * @return true 注册成功
 * @return false 注册失败
 */
static bool register_transform_preset(const char *name, const char *description, const PresetType *input_types,
                                      int input_count, PresetType output_type, const char *math_def,
                                      const char *complexity, bool is_constructive, bool is_reversible) {
    return preset_blocks_register_simple(name, description, PRESET_CATEGORY_TRANSFORMATION, input_types, input_count,
                                         output_type, math_def, complexity, is_constructive, is_reversible);
}

/* ============================================================
 * 模块注册实现
 * ============================================================ */

bool preset_transformations_register(void) {
    int success_count = 0;

    /* ============================================================
     * 平移变换 (1个)
     * ============================================================ */

    /* 平移变换：将点沿向量平移 */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_transform_preset(PRESET_TRANSLATION, "平移变换：将点沿向量平移", inputs, 3, PRESET_TYPE_POINT,
                                      "T_{\\vec{v}}(P) = P + \\vec{v}", "O(1)", true, true)) {
            success_count++;
        }
    }

    /* ============================================================
     * 旋转变换 (3个)
     * ============================================================ */

    /* 绕点旋转：将点绕中心旋转指定角度 */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_SCALAR};
        if (register_transform_preset(PRESET_ROTATION, "绕点旋转：将点绕中心旋转指定角度（弧度）", inputs, 3,
                                      PRESET_TYPE_POINT, "R_{O,\\theta}(P) = O + R_{\\theta}(P - O)", "O(1)", true,
                                      true)) {
            success_count++;
        }
    }

    /* 通过参考点构造旋转 */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_transform_preset(PRESET_ROTATION_BY_REFERENCE, "通过参考点构造旋转：使参考起点映射到参考终点",
                                      inputs, 4, PRESET_TYPE_POINT, "R_{O,A\\to B}(P)", "O(1)", true, true)) {
            success_count++;
        }
    }

    /* ============================================================
     * 反射变换 (3个)
     * ============================================================ */

    /* 关于直线的反射 */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_transform_preset(PRESET_REFLECTION_LINE, "关于直线的反射：求点关于直线的对称点", inputs, 3,
                                      PRESET_TYPE_POINT, "Ref_l(P)", "O(1)", true, true)) {
            success_count++;
        }
    }

    /* 关于点的中心反射 */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_transform_preset(PRESET_REFLECTION_POINT, "关于点的中心反射：等价于绕该点旋转180度", inputs, 2,
                                      PRESET_TYPE_POINT, "C_O(P) = 2O - P", "O(1)", true, true)) {
            success_count++;
        }
    }

    /* 滑移反射 */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_SCALAR};
        if (register_transform_preset(PRESET_GLIDE_REFLECTION, "滑移反射：反射后沿反射轴方向平移", inputs, 4,
                                      PRESET_TYPE_POINT, "G_{l,d}(P) = T_d \\circ Ref_l(P)", "O(1)", true, true)) {
            success_count++;
        }
    }

    /* ============================================================
     * 位似/缩放变换 (3个)
     * ============================================================ */

    /* 位似变换（中心缩放） */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_SCALAR};
        if (register_transform_preset(PRESET_HOMOTHETY, "位似变换：以中心点为基准按比例缩放", inputs, 3,
                                      PRESET_TYPE_POINT, "H_{O,k}(P) = O + k(P - O)", "O(1)", true, true)) {
            success_count++;
        }
    }

    /* 通过参考点构造位似 */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_transform_preset(PRESET_HOMOTHETY_BY_REFERENCE, "通过参考点构造位似：使参考起点映射到参考终点",
                                      inputs, 4, PRESET_TYPE_POINT, "H_{O,A\\to B}(P)", "O(1)", true, true)) {
            success_count++;
        }
    }

    /* 均匀缩放 */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_SCALAR};
        if (register_transform_preset(PRESET_SCALE, "均匀缩放：以原点为中心按比例缩放", inputs, 2, PRESET_TYPE_POINT,
                                      "S_k(P) = kP", "O(1)", true, true)) {
            success_count++;
        }
    }

    /* ============================================================
     * 仿射变换 (1个)
     * ============================================================ */

    /* 错切变换 */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_SCALAR};
        if (register_transform_preset(PRESET_SHEAR, "错切变换：沿指定方向的错切变换", inputs, 3, PRESET_TYPE_POINT,
                                      "Sh_{dir,k}(P)", "O(1)", true, true)) {
            success_count++;
        }
    }

    /* ============================================================
     * 变换组合 (3个)
     * ============================================================ */

    /* 变换复合 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_transform_preset(PRESET_TRANSFORM_COMPOSE, "变换复合：构造两个变换的复合变换 g∘f", inputs, 2,
                                      PRESET_TYPE_FUNCTION, "(g \\circ f)(P) = g(f(P))", "O(1)", true, false)) {
            success_count++;
        }
    }

    /* 变换逆 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_transform_preset(PRESET_TRANSFORM_INVERSE, "变换逆：构造变换的逆变换", inputs, 1,
                                      PRESET_TYPE_FUNCTION, "f^{-1}(f(P)) = P", "O(1)", true, false)) {
            success_count++;
        }
    }

    /* 恒等变换 */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT};
        if (register_transform_preset(PRESET_IDENTITY_TRANSFORM, "恒等变换：点保持不变", inputs, 1, PRESET_TYPE_POINT,
                                      "Id(P) = P", "O(1)", true, true)) {
            success_count++;
        }
    }

    /* ============================================================
     * 特殊变换 (2个)
     * ============================================================ */

    /* 反演变换（关于圆） */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_POINT};
        if (register_transform_preset(PRESET_INVERSION, "反演变换：关于圆的反演，点P满足|OP|·|OP'|=r²", inputs, 3,
                                      PRESET_TYPE_POINT, "|OP| \\cdot |OP'| = r^2", "O(1)", true, true)) {
            success_count++;
        }
    }

    /* 螺旋相似 */
    {
        PresetType inputs[] = {PRESET_TYPE_POINT, PRESET_TYPE_POINT, PRESET_TYPE_SCALAR, PRESET_TYPE_SCALAR};
        if (register_transform_preset(PRESET_SPIRAL_SIMILARITY, "螺旋相似：旋转与位似的复合变换", inputs, 4,
                                      PRESET_TYPE_POINT, "S_{O,\\theta,k}(P) = O + k \\cdot R_{\\theta}(P - O)", "O(1)",
                                      true, true)) {
            success_count++;
        }
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == TRANSFORMATIONS_PRESET_COUNT;
}

/**
 * @brief 获取变换模块的预设数量
 *
 * @return 预设函数块数量
 */
int preset_transformations_count(void) {
    return TRANSFORMATIONS_PRESET_COUNT;
}

/**
 * @brief 获取变换模块的类别
 *
 * @return 预设类别枚举值
 */
PresetCategory preset_transformations_category(void) {
    return PRESET_CATEGORY_TRANSFORMATION;
}

/**
 * @brief 获取变换模块所有预设名称
 *
 * @param[out] out_names 输出名称数组指针
 * @param[out] out_count 输出名称数量
 * @return true 成功获取
 * @return false 参数无效或内存分配失败
 */
bool preset_transformations_get_names(char ***out_names, int *out_count) {
    if (!out_names || !out_count)
        return false;

    *out_count = TRANSFORMATIONS_PRESET_COUNT;
    *out_names = (char **) lv00_malloc((size_t) TRANSFORMATIONS_PRESET_COUNT * sizeof(char *));
    if (!*out_names)
        return false;

    int idx = 0;
    (*out_names)[idx++] = lv00_strdup(PRESET_TRANSLATION);
    (*out_names)[idx++] = lv00_strdup(PRESET_ROTATION);
    (*out_names)[idx++] = lv00_strdup(PRESET_ROTATION_BY_REFERENCE);
    (*out_names)[idx++] = lv00_strdup(PRESET_REFLECTION_LINE);
    (*out_names)[idx++] = lv00_strdup(PRESET_REFLECTION_POINT);
    (*out_names)[idx++] = lv00_strdup(PRESET_GLIDE_REFLECTION);
    (*out_names)[idx++] = lv00_strdup(PRESET_HOMOTHETY);
    (*out_names)[idx++] = lv00_strdup(PRESET_HOMOTHETY_BY_REFERENCE);
    (*out_names)[idx++] = lv00_strdup(PRESET_SCALE);
    (*out_names)[idx++] = lv00_strdup(PRESET_SHEAR);
    (*out_names)[idx++] = lv00_strdup(PRESET_TRANSFORM_COMPOSE);
    (*out_names)[idx++] = lv00_strdup(PRESET_TRANSFORM_INVERSE);
    (*out_names)[idx++] = lv00_strdup(PRESET_IDENTITY_TRANSFORM);
    (*out_names)[idx++] = lv00_strdup(PRESET_INVERSION);
    (*out_names)[idx++] = lv00_strdup(PRESET_SPIRAL_SIMILARITY);

    /* 检查是否有分配失败 */
    for (int i = 0; i < idx; i++) {
        if (!(*out_names)[i]) {
            /* 回滚已分配的内存 */
            for (int j = 0; j < i; j++) {
                lv00_free((void **) &(*out_names)[j]);
            }
            lv00_free((void **) out_names);
            return false;
        }
    }

    return true;
}
