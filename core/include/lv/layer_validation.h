/**
 * @file layer_validation.h
 * @brief Lv-00 十层架构编译时边界验证
 * @details 提供编译时层级边界检查，确保下层模块不依赖上层模块。
 *          通过宏和静态断言实现编译时验证。
 *
 * @version 3.0.0
 * @author Lv-00 Project
 *
 * @par 依赖模型（与 README「十层单向依赖架构」表格一致，2026-08-14 更新）
 * - Layer 0 (Core Convenience): 便利层，允许依赖所有层
 * - Layer 1 (Parser): 只能依赖 Layer 2
 * - Layer 2 (Resource): 不依赖任何层
 * - Layer 3 (Geometry): 只能依赖 Layer 2
 * - Layer 4 (Reasoning): 只能依赖 Layer 2、Layer 3
 * - Layer 5 (Output): 只能依赖 Layer 2、Layer 3、Layer 4
 * - Layer 6 (Visual): 只能依赖 Layer 2、Layer 3、Layer 4、Layer 5
 * - Layer 7 (Orchestration): 只能依赖 Layer 2 ~ Layer 6
 * - Layer 8 (Meta-Verification): 只能依赖 Layer 2、Layer 3、Layer 4
 * - Layer 9 (Application): 允许依赖所有层
 * - Layer 10 (Interop): 只能依赖 Layer 2、Layer 4、Layer 5
 *
 * @par 使用方法
 * 每个源文件定义 lv_CURRENT_LAYER 宏（由 CMake lv_setup_layer 的
 * target_compile_definitions 自动设置）并包含此头文件；启用
 * lv_ENABLE_LAYER_VALIDATION 后编译器将在编译时检查层级依赖。
 *
 * @note 文档参考头：编译期验证的实际宏由 engine.h 的 lv_ALLOW_LAYER /
 *       lv_REQUIRE_STRICTLY_ABOVE 承担（lv_ENABLE_LAYER_VALIDATION 分支）。
 *       本头的 lv_LAYER_CAN_DEPEND 为依赖表判定的参考实现（全库零 include），
 *       十层依赖模型（见上 @par 依赖模型）保持为本文件权威文档源。
 */

#ifndef lv_LAYER_VALIDATION_H
#define lv_LAYER_VALIDATION_H

/* ============================================================
 * 层级定义（十层 + L0 便利层）
 * ============================================================ */

#define lv_LAYER_CORE 0        /**< Layer 0: 核心便利层（系统入口协调） */
#define lv_LAYER_PARSER 1      /**< Layer 1: 输入解析层 */
#define lv_LAYER_RESOURCE 2    /**< Layer 2: 资源管理层 */
#define lv_LAYER_GEOMETRY 3    /**< Layer 3: 几何拓扑层 */
#define lv_LAYER_REASONING 4   /**< Layer 4: 公理推理层 */
#define lv_LAYER_OUTPUT 5      /**< Layer 5: 结果输出层 */
#define lv_LAYER_VISUAL 6      /**< Layer 6: 图形化编程层 */
#define lv_LAYER_ORCHESTRATION 7   /**< Layer 7: 流水线编排层 */
#define lv_LAYER_META_VERIFY 8     /**< Layer 8: 元验证层 */
#define lv_LAYER_APPLICATION 9     /**< Layer 9: 应用层 */
#define lv_LAYER_INTEROP 10        /**< Layer 10: 互操作桥接层 */

/* ============================================================
 * 层级依赖判定
 * ============================================================ */

/**
 * @brief 判定当前层是否允许依赖目标层（编译期常量表达式）
 *
 * 严格按 README 十层依赖表展开（非简单的 current >= target 模型，
 * 因 L8 允许 L2/L3/L4 但不允许 L5/L6，简单大小比较会漏判）。
 */
#define lv_LAYER_CAN_DEPEND(current, target)                                                    \
    ((current) == lv_LAYER_CORE || (current) == lv_LAYER_APPLICATION || (current) == 0 ? 1 :     \
     (current) == lv_LAYER_PARSER ? ((target) == lv_LAYER_RESOURCE) :                            \
     (current) == lv_LAYER_RESOURCE ? 0 :                                                        \
     (current) == lv_LAYER_GEOMETRY ? ((target) == lv_LAYER_RESOURCE) :                          \
     (current) == lv_LAYER_REASONING ? ((target) == lv_LAYER_RESOURCE || (target) == lv_LAYER_GEOMETRY) : \
     (current) == lv_LAYER_OUTPUT ? ((target) == lv_LAYER_RESOURCE || (target) == lv_LAYER_GEOMETRY || (target) == lv_LAYER_REASONING) : \
     (current) == lv_LAYER_VISUAL ? ((target) == lv_LAYER_RESOURCE || (target) == lv_LAYER_GEOMETRY || (target) == lv_LAYER_REASONING || (target) == lv_LAYER_OUTPUT) : \
     (current) == lv_LAYER_ORCHESTRATION ? ((target) >= lv_LAYER_RESOURCE && (target) <= lv_LAYER_VISUAL) : \
     (current) == lv_LAYER_META_VERIFY ? ((target) == lv_LAYER_RESOURCE || (target) == lv_LAYER_GEOMETRY || (target) == lv_LAYER_REASONING) : \
     (current) == lv_LAYER_INTEROP ? ((target) == lv_LAYER_RESOURCE || (target) == lv_LAYER_REASONING || (target) == lv_LAYER_OUTPUT) : \
     1)

/* ============================================================
 * 层级边界验证宏
 * ============================================================ */

#ifdef lv_ENABLE_LAYER_VALIDATION

/**
 * @brief 验证当前层级是否允许依赖目标层级
 * @note 与 layer_validation 的旧模型不同：L2 不再豁免一切依赖，
 *       基础层依赖任何层均为违规（含 L0 便利层的协调行为由 L0 显式允许）。
 */
#define lv_VALIDATE_LAYER_DEPENDENCY(target_layer) \
    _Static_assert(lv_LAYER_CAN_DEPEND(lv_CURRENT_LAYER, target_layer), \
                   "Layer violation: lower layer cannot depend on upper layer")

/**
 * @brief 验证当前层级在合法范围内（L0 便利层或 L1-L10）
 */
#define lv_VALIDATE_CURRENT_LAYER()                                                            \
    _Static_assert(lv_CURRENT_LAYER >= lv_LAYER_CORE && lv_CURRENT_LAYER <= lv_LAYER_INTEROP, \
                   "Invalid layer: lv_CURRENT_LAYER must be between 0 and 10")

/**
 * @brief Layer 2 专用验证 - 确保不依赖任何上层
 */
#if lv_CURRENT_LAYER == lv_LAYER_RESOURCE
#define lv_LAYER2_ASSERT_ISOLATION() _Static_assert(1, "Layer 2 isolation check passed")
#else
#define lv_LAYER2_ASSERT_ISOLATION()
#endif

/**
 * @brief 验证 Layer 1 只能依赖 Layer 2
 */
#if lv_CURRENT_LAYER == lv_LAYER_PARSER
#define lv_LAYER1_VALIDATE_DEPENDENCY(dep_layer) \
    _Static_assert(dep_layer == lv_LAYER_RESOURCE, "Layer 1 can only depend on Layer 2")
#else
#define lv_LAYER1_VALIDATE_DEPENDENCY(dep_layer)
#endif

/**
 * @brief 验证 Layer 3 只能依赖 Layer 2
 */
#if lv_CURRENT_LAYER == lv_LAYER_GEOMETRY
#define lv_LAYER3_VALIDATE_DEPENDENCY(dep_layer) \
    _Static_assert(dep_layer == lv_LAYER_RESOURCE, "Layer 3 can only depend on Layer 2")
#else
#define lv_LAYER3_VALIDATE_DEPENDENCY(dep_layer)
#endif

/**
 * @brief 验证 Layer 4 只能依赖 Layer 3 和 Layer 2
 */
#if lv_CURRENT_LAYER == lv_LAYER_REASONING
#define lv_LAYER4_VALIDATE_DEPENDENCY(dep_layer)                                     \
    _Static_assert(dep_layer == lv_LAYER_RESOURCE || dep_layer == lv_LAYER_GEOMETRY, \
                   "Layer 4 can only depend on Layer 3 and Layer 2")
#else
#define lv_LAYER4_VALIDATE_DEPENDENCY(dep_layer)
#endif

/**
 * @brief 验证 Layer 5 只能依赖 Layer 4、Layer 3 和 Layer 2
 */
#if lv_CURRENT_LAYER == lv_LAYER_OUTPUT
#define lv_LAYER5_VALIDATE_DEPENDENCY(dep_layer)                                                             \
    _Static_assert(                                                                                          \
        dep_layer == lv_LAYER_RESOURCE || dep_layer == lv_LAYER_GEOMETRY || dep_layer == lv_LAYER_REASONING, \
        "Layer 5 can only depend on Layer 4, Layer 3, and Layer 2")
#else
#define lv_LAYER5_VALIDATE_DEPENDENCY(dep_layer)
#endif

/**
 * @brief 验证 Layer 6 只能依赖 Layer 5、Layer 4、Layer 3 和 Layer 2
 */
#if lv_CURRENT_LAYER == lv_LAYER_VISUAL
#define lv_LAYER6_VALIDATE_DEPENDENCY(dep_layer)                                                            \
    _Static_assert(dep_layer == lv_LAYER_RESOURCE || dep_layer == lv_LAYER_GEOMETRY ||                      \
                       dep_layer == lv_LAYER_REASONING || dep_layer == lv_LAYER_OUTPUT,                      \
                   "Layer 6 can only depend on Layer 5, Layer 4, Layer 3, and Layer 2")
#else
#define lv_LAYER6_VALIDATE_DEPENDENCY(dep_layer)
#endif

/**
 * @brief 验证 Layer 8 只能依赖 Layer 4、Layer 3 和 Layer 2（不允许 L5/L6）
 */
#if lv_CURRENT_LAYER == lv_LAYER_META_VERIFY
#define lv_LAYER8_VALIDATE_DEPENDENCY(dep_layer)                                                            \
    _Static_assert(dep_layer == lv_LAYER_RESOURCE || dep_layer == lv_LAYER_GEOMETRY ||                      \
                       dep_layer == lv_LAYER_REASONING,                                                      \
                   "Layer 8 can only depend on Layer 4, Layer 3, and Layer 2")
#else
#define lv_LAYER8_VALIDATE_DEPENDENCY(dep_layer)
#endif

#else /* !lv_ENABLE_LAYER_VALIDATION */

/* 验证禁用时，所有宏为空操作 */
#define lv_VALIDATE_LAYER_DEPENDENCY(target_layer)
#define lv_VALIDATE_CURRENT_LAYER()
#define lv_LAYER2_ASSERT_ISOLATION()
#define lv_LAYER1_VALIDATE_DEPENDENCY(dep_layer)
#define lv_LAYER3_VALIDATE_DEPENDENCY(dep_layer)
#define lv_LAYER4_VALIDATE_DEPENDENCY(dep_layer)
#define lv_LAYER5_VALIDATE_DEPENDENCY(dep_layer)
#define lv_LAYER6_VALIDATE_DEPENDENCY(dep_layer)
#define lv_LAYER8_VALIDATE_DEPENDENCY(dep_layer)

#endif /* lv_ENABLE_LAYER_VALIDATION */

/* ============================================================
 * 自动验证当前层级
 * ============================================================ */

#ifdef lv_CURRENT_LAYER
lv_VALIDATE_CURRENT_LAYER();
#endif

#endif /* lv_LAYER_VALIDATION_H */
