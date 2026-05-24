/**
 * @file preset_homological_algebra.h
 * @brief 同调代数预设函数块 - 头文件
 *
 * @details 提供同调代数相关的预设函数块，包括：
 *          - 链复形与同调群
 *          - 正合序列与蛇引理
 *          - 导出函子（Ext, Tor）
 *          - 谱序列
 *          - 同调维数
 *
 * @module HomologicalAlgebra
 * @category PRESET_CATEGORY_ALGEBRA
 * @version 1.0.0
 * @author Lv-00 Project
 */

#ifndef LV00_PRESET_HOMOLOGICAL_ALGEBRA_H
#define LV00_PRESET_HOMOLOGICAL_ALGEBRA_H

#include "func_block_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 预设名称常量定义 - 链复形与同调群
 * ============================================================ */

/** 链复形：构造链复形 (C_*, d) */
#define PRESET_CHAIN_COMPLEX "chain_complex"

/** 同调群：计算链复形的同调群 H_n(C) */
#define PRESET_HOMOLOGY_GROUP "homology_group"

/** 上链复形：构造上链复形 (C^*, d) */
#define PRESET_COCHAIN_COMPLEX "cochain_complex"

/** 上同调群：计算上链复形的上同调群 H^n(C) */
#define PRESET_COHOMOLOGY_GROUP "cohomology_group"

/** 边缘算子：计算链的边缘 ∂(c) */
#define PRESET_BOUNDARY_OPERATOR "boundary_operator"

/* ============================================================
 * 预设名称常量定义 - 正合序列
 * ============================================================ */

/** 正合序列判定：判定序列是否正合 */
#define PRESET_EXACT_SEQUENCE_CHECK "exact_sequence_check"

/** 短正合序列：构造短正合序列 0 → A → B → C → 0 */
#define PRESET_SHORT_EXACT_SEQUENCE "short_exact_sequence"

/** 长正合序列：由短正合序列诱导的长正合序列 */
#define PRESET_LONG_EXACT_SEQUENCE "long_exact_sequence"

/** 蛇引理：构造蛇引理的正合序列 */
#define PRESET_SNAKE_LEMMA "snake_lemma"

/** 五引理：验证五引理 */
#define PRESET_FIVE_LEMMA "five_lemma"

/* ============================================================
 * 预设名称常量定义 - 导出函子
 * ============================================================ */

/** Ext函子：计算 Ext^n_R(A, B) */
#define PRESET_EXT_FUNCTOR "ext_functor"

/** Tor函子：计算 Tor^R_n(A, B) */
#define PRESET_TOR_FUNCTOR "tor_functor"

/** 投射分解：构造模的投射分解 */
#define PRESET_PROJECTIVE_RESOLUTION "projective_resolution"

/** 内射分解：构造模的内射分解 */
#define PRESET_INJECTIVE_RESOLUTION "injective_resolution"

/** 自由分解：构造模的自由分解 */
#define PRESET_FREE_RESOLUTION "free_resolution"

/* ============================================================
 * 预设名称常量定义 - 谱序列
 * ============================================================ */

/** 谱序列：构造谱序列 {E_r^{p,q}} */
#define PRESET_SPECTRAL_SEQUENCE "spectral_sequence"

/** Serre谱序列：计算纤维化的Serre谱序列 */
#define PRESET_SERRE_SPECTRAL_SEQUENCE "serre_spectral_sequence"

/** Grothendieck谱序列：计算复合函子的Grothendieck谱序列 */
#define PRESET_GROTHENDIECK_SPECTRAL_SEQUENCE "grothendieck_spectral_sequence"

/** Leray谱序列：计算Leray谱序列 */
#define PRESET_LERAY_SPECTRAL_SEQUENCE "leray_spectral_sequence"

/** 谱序列收敛：判定谱序列是否收敛 */
#define PRESET_SPECTRAL_SEQUENCE_CONVERGENCE "spectral_sequence_convergence"

/* ============================================================
 * 预设名称常量定义 - 同调维数
 * ============================================================ */

/** 投射维数：计算模的投射维数 pd(M) */
#define PRESET_PROJECTIVE_DIMENSION "projective_dimension"

/** 内射维数：计算模的内射维数 id(M) */
#define PRESET_INJECTIVE_DIMENSION "injective_dimension"

/** 整体维数：计算环的整体维数 gl.dim(R) */
#define PRESET_GLOBAL_DIMENSION "global_dimension"

/** 同调维数：计算同调维数 */
#define PRESET_HOMOLOGICAL_DIMENSION "homological_dimension"

/** 深度：计算模的深度 depth(M) */
#define PRESET_DEPTH "depth"

/* ============================================================
 * 模块接口
 * ============================================================ */

/**
 * @brief 注册所有同调代数预设函数块
 *
 * @return true  所有预设注册成功
 * @return false 部分或全部预设注册失败
 */
bool preset_homological_algebra_register(void);

/**
 * @brief 获取同调代数预设函数块数量
 *
 * @return int 预设数量（固定为 25）
 */
int preset_homological_algebra_count(void);

/**
 * @brief 获取同调代数预设的类别
 *
 * @return PresetCategory 始终返回 PRESET_CATEGORY_ALGEBRA
 */
PresetCategory preset_homological_algebra_category(void);

/**
 * @brief 获取同调代数预设名称列表
 *
 * @param out_names 输出名称数组
 * @param out_count 输出名称数量
 * @return true 成功获取
 * @return false 失败
 */
bool preset_homological_algebra_get_names(char ***out_names, int *out_count);

#ifdef __cplusplus
}
#endif

#endif /* LV00_PRESET_HOMOLOGICAL_ALGEBRA_H */
