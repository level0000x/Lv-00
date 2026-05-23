/**
 * @file preset_category_theory_adv.h
 * @brief 范畴论进阶预设函数块 —— Yoneda引理、Kan扩张、单子、预层、可表函子等高级概念
 *
 * 为理论数学研究提供范畴论进阶运算的预设函数块定义。
 * 本模块是 preset_category_theory.h 的补充，覆盖更深入的范畴论主题。
 *
 * @module CategoryTheoryAdvanced
 * @category PRESET_CATEGORY_CATEGORY_THEORY
 * @version 10.0.0
 */

#ifndef LV00_PRESET_CATEGORY_THEORY_ADV_H
#define LV00_PRESET_CATEGORY_THEORY_ADV_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/* ==================== 预设函数块名称常量（v10.0 新增，共20个） ==================== */

/* ── 可表函子与米田引理 ── */
#define PRESET_CAT_ADV_HOM_FUNCTOR           "cat_adv_hom_functor"           /**< Hom函子构造 */
#define PRESET_CAT_ADV_REPRESENTABLE_FUNCTOR  "cat_adv_representable_functor" /**< 可表函子判定 */
#define PRESET_CAT_ADV_YONEDA_EMBEDDING       "cat_adv_yoneda_embedding"      /**< Yoneda嵌入 */
#define PRESET_CAT_ADV_YONEDA_LEMMA           "cat_adv_yoneda_lemma"          /**< Yoneda引理应用 */

/* ── 极限与余极限（进阶） ── */
#define PRESET_CAT_ADV_LIMIT                 "cat_adv_limit"                 /**< 一般极限构造 */
#define PRESET_CAT_ADV_COLIMIT               "cat_adv_colimit"               /**< 一般余极限构造 */
#define PRESET_CAT_ADV_FILTERED_COLIMIT      "cat_adv_filtered_colimit"      /**< 滤过余极限 */
#define PRESET_CAT_ADV_KAN_EXTENSION          "cat_adv_kan_extension"         /**< Kan扩张 */
#define PRESET_CAT_ADV_KAN_LIFT              "cat_adv_kan_lift"              /**< Kan提升 */

/* ── 单子与余单子 ── */
#define PRESET_CAT_ADV_MONAD                 "cat_adv_monad"                 /**< 单子（Monad） */
#define PRESET_CAT_ADV_COMONAD               "cat_adv_comonad"               /**< 余单子（Comonad） */
#define PRESET_CAT_ADV_KLEISLI_CATEGORY       "cat_adv_kleisli_category"       /**< Kleisli范畴 */
#define PRESET_CAT_ADV_EILENBERG_MOORE        "cat_adv_eilenberg_moore"        /**< Eilenberg-Moore代数 */

/* ── 预层与层 ── */
#define PRESET_CAT_ADV_PRESHEAF              "cat_adv_presheaf"              /**< 预层构造 */
#define PRESET_CAT_ADV_SHEAFIFICATION        "cat_adv_sheafification"        /**< 层化 */
#define PRESET_CAT_ADV_GROTHENDIECK_TOPOLOGY  "cat_adv_grothendieck_topology"  /**< Grothendieck拓扑 */

/* ── 伴随与泛性质 ── */
#define PRESET_CAT_ADV_ADJOINT_UNIT_COUNIT   "cat_adv_adjoint_unit_counit"   /**< 伴随单位/余单位 */
#define PRESET_CAT_ADV_UNIVERSAL_PROPERTY     "cat_adv_universal_property"    /**< 泛性质验证 */
#define PRESET_CAT_ADV_FREE_FUNCTOR          "cat_adv_free_functor"          /**< 自由函子 */
#define PRESET_CAT_ADV_FORGETFUL_FUNCTOR      "cat_adv_forgetful_functor"      /**< 遗忘函子 */

/* ==================== 模块注册函数 ==================== */

/**
 * @brief 注册进阶范畴论预设函数块
 *
 * 一次性注册本模块内定义的全部20个进阶范畴论预设。
 * 幂等操作：重复调用不会产生副作用。
 *
 * @return true 全部注册成功，false 部分或全部注册失败
 */
bool preset_category_theory_adv_register(void);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_CATEGORY_THEORY_ADV_H */
