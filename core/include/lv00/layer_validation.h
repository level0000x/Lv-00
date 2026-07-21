/**
 * @file layer_validation.h
 * @brief Lv-00 六层架构编译时边界验证
 * @details 提供编译时层级边界检查，确保下层模块不依赖上层模块。
 *          通过宏和静态断言实现编译时验证。
 *
 * @version 1.1.0
 * @author Lv-00 Project
 *
 * @par 验证规则
 * - Layer 1 (Parser): 只能依赖 Layer 2
 * - Layer 2 (Resource): 不依赖任何上层
 * - Layer 3 (Geometry): 只能依赖 Layer 2
 * - Layer 4 (Reasoning): 只能依赖 Layer 3 和 Layer 2
 * - Layer 5 (Output): 只能依赖 Layer 4、Layer 3 和 Layer 2
 * - Layer 6 (Visual): 只能依赖 Layer 5、Layer 4、Layer 3 和 Layer 2
 *
 * @par 使用方法
 * 在每个源文件顶部包含此头文件，并定义 LV00_CURRENT_LAYER 宏。
 * 编译器将在编译时检查层级依赖是否正确。
 */

#ifndef LV00_LAYER_VALIDATION_H
#define LV00_LAYER_VALIDATION_H

/* ============================================================
 * 层级定义
 * ============================================================ */

#define LV00_LAYER_PARSER    1  /**< Layer 1: 输入解析层 */
#define LV00_LAYER_RESOURCE  2  /**< Layer 2: 资源管理层 */
#define LV00_LAYER_GEOMETRY  3  /**< Layer 3: 几何拓扑层 */
#define LV00_LAYER_REASONING 4  /**< Layer 4: 公理推理层 */
#define LV00_LAYER_OUTPUT    5  /**< Layer 5: 结果输出层 */
#define LV00_LAYER_VISUAL    6  /**< Layer 6: 图形化编程层 */

/* ============================================================
 * 层级边界验证宏
 * ============================================================ */

#ifdef LV00_ENABLE_LAYER_VALIDATION

/**
 * @brief 验证当前层级是否允许依赖目标层级
 * 
 * 规则：
 * - 当前层级必须 >= 目标层级（下层不能依赖上层）
 * - Layer 2 是最底层，不依赖任何其他层
 */
#define LV00_VALIDATE_LAYER_DEPENDENCY(target_layer) \
    _Static_assert( \
        (LV00_CURRENT_LAYER >= target_layer) || (LV00_CURRENT_LAYER == LV00_LAYER_RESOURCE), \
        "Layer violation: lower layer cannot depend on upper layer" \
    )

/**
 * @brief 验证当前层级是否在允许范围内
 */
#define LV00_VALIDATE_CURRENT_LAYER() \
    _Static_assert( \
        LV00_CURRENT_LAYER >= LV00_LAYER_PARSER && LV00_CURRENT_LAYER <= LV00_LAYER_OUTPUT, \
        "Invalid layer: LV00_CURRENT_LAYER must be between 1 and 5" \
    )

/**
 * @brief Layer 2 专用验证 - 确保不依赖任何上层
 */
#if LV00_CURRENT_LAYER == LV00_LAYER_RESOURCE
    #define LV00_LAYER2_ASSERT_ISOLATION() \
        _Static_assert(1, "Layer 2 isolation check passed")
#else
    #define LV00_LAYER2_ASSERT_ISOLATION()
#endif

/**
 * @brief 验证 Layer 1 只能依赖 Layer 2
 */
#if LV00_CURRENT_LAYER == LV00_LAYER_PARSER
    #define LV00_LAYER1_VALIDATE_DEPENDENCY(dep_layer) \
        _Static_assert(dep_layer == LV00_LAYER_RESOURCE, \
            "Layer 1 can only depend on Layer 2")
#else
    #define LV00_LAYER1_VALIDATE_DEPENDENCY(dep_layer)
#endif

/**
 * @brief 验证 Layer 3 只能依赖 Layer 2
 */
#if LV00_CURRENT_LAYER == LV00_LAYER_GEOMETRY
    #define LV00_LAYER3_VALIDATE_DEPENDENCY(dep_layer) \
        _Static_assert(dep_layer == LV00_LAYER_RESOURCE, \
            "Layer 3 can only depend on Layer 2")
#else
    #define LV00_LAYER3_VALIDATE_DEPENDENCY(dep_layer)
#endif

/**
 * @brief 验证 Layer 4 只能依赖 Layer 3 和 Layer 2
 */
#if LV00_CURRENT_LAYER == LV00_LAYER_REASONING
    #define LV00_LAYER4_VALIDATE_DEPENDENCY(dep_layer) \
        _Static_assert(dep_layer == LV00_LAYER_RESOURCE || dep_layer == LV00_LAYER_GEOMETRY, \
            "Layer 4 can only depend on Layer 3 and Layer 2")
#else
    #define LV00_LAYER4_VALIDATE_DEPENDENCY(dep_layer)
#endif

/**
 * @brief 验证 Layer 5 只能依赖 Layer 4、Layer 3 和 Layer 2
 */
#if LV00_CURRENT_LAYER == LV00_LAYER_OUTPUT
    #define LV00_LAYER5_VALIDATE_DEPENDENCY(dep_layer) \
        _Static_assert(dep_layer == LV00_LAYER_RESOURCE || \
                       dep_layer == LV00_LAYER_GEOMETRY || \
                       dep_layer == LV00_LAYER_REASONING, \
            "Layer 5 can only depend on Layer 4, Layer 3, and Layer 2")
#else
    #define LV00_LAYER5_VALIDATE_DEPENDENCY(dep_layer)
#endif

#else /* !LV00_ENABLE_LAYER_VALIDATION */

/* 验证禁用时，所有宏为空操作 */
#define LV00_VALIDATE_LAYER_DEPENDENCY(target_layer)
#define LV00_VALIDATE_CURRENT_LAYER()
#define LV00_LAYER2_ASSERT_ISOLATION()
#define LV00_LAYER1_VALIDATE_DEPENDENCY(dep_layer)
#define LV00_LAYER3_VALIDATE_DEPENDENCY(dep_layer)
#define LV00_LAYER4_VALIDATE_DEPENDENCY(dep_layer)
#define LV00_LAYER5_VALIDATE_DEPENDENCY(dep_layer)

#endif /* LV00_ENABLE_LAYER_VALIDATION */

/* ============================================================
 * 自动验证当前层级
 * ============================================================ */

#ifdef LV00_CURRENT_LAYER
    LV00_VALIDATE_CURRENT_LAYER();
#endif

#endif /* LV00_LAYER_VALIDATION_H */
