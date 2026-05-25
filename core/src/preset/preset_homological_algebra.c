/**
 * @file preset_homological_algebra.c
 * @brief 同调代数预设函数块模块 - 实现
 *
 * 实现理论数学研究中常用的同调代数预设函数块。
 * 涵盖链复形与同调群、正合序列、导出函子、谱序列、同调维数，
 * 共25个预设。
 *
 * @module HomologicalAlgebra
 * @category PRESET_CATEGORY_ALGEBRA
 * @version 1.0.0
 * @author Lv-00 Project
 */

#include "preset_homological_algebra.h"

#include <string.h>

#include "lv00_internal.h"
#include "lv00_utils.h"
#include "preset_blocks.h"
#include "preset_common.h"

/* ==================== 预设函数块数量 ==================== */

/** 同调代数模块预设函数块总数 */
#define HOMOLOGICAL_ALGEBRA_PRESET_COUNT 25

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 注册单个同调代数预设
 *
 * 统一调用 preset_blocks_register_simple，类别固定为
 * PRESET_CATEGORY_ALGEBRA。
 *
 * @param name 预设名称
 * @param description 中文描述
 * @param input_types 输入类型数组
 * @param input_count 输入数量
 * @param output_type 输出类型
 * @param math_def 数学定义（LaTeX 格式）
 * @param complexity 时间复杂度描述
 * @param is_constructive 是否构造性
 * @param is_reversible 是否可逆
 * @return true 注册成功
 * @return false 注册失败
 */
static bool register_ha_preset(const char *name, const char *description, const PresetType *input_types,
                               int input_count, PresetType output_type, const char *math_def, const char *complexity,
                               bool is_constructive, bool is_reversible) {
    return preset_blocks_register_simple(name, description, PRESET_CATEGORY_ALGEBRA, input_types, input_count,
                                         output_type, math_def, complexity, is_constructive, is_reversible);
}

/* ==================== 模块注册实现 ==================== */

bool preset_homological_algebra_register(void) {
    int success_count = 0;

    /* ============================================================
     * 第一部分：链复形与同调群（5个预设）
     *
     * 链复形是同调代数的基本对象，由一系列模和边缘算子组成。
     * 同调群是链复形的核心不变量，测量"闭链模去边缘链"。
     * ============================================================ */

    /* -------------------- chain_complex：链复形 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MODULE, PRESET_TYPE_FUNCTION};
        if (register_ha_preset(PRESET_CHAIN_COMPLEX, "链复形：构造链复形 (C_*, d)，满足 d² = 0", inputs, 2,
                               PRESET_TYPE_SEQUENCE,
                               "(C_\\bullet, d): \\cdots \\xrightarrow{d} C_{n+1} "
                               "\\xrightarrow{d} C_n \\xrightarrow{d} C_{n-1} \\xrightarrow{d} \\cdots, "
                               "\\quad d \\circ d = 0",
                               "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- homology_group：同调群 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE, PRESET_TYPE_INTEGER};
        if (register_ha_preset(PRESET_HOMOLOGY_GROUP, "同调群：计算链复形的第n个同调群 H_n(C)", inputs, 2,
                               PRESET_TYPE_MODULE,
                               "H_n(C) = \\ker(d_n) / \\mathrm{im}(d_{n+1}) = "
                               "Z_n / B_n, \\quad Z_n = \\ker(d_n), B_n = \\mathrm{im}(d_{n+1})",
                               "O(n^3)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- cochain_complex：上链复形 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MODULE, PRESET_TYPE_FUNCTION};
        if (register_ha_preset(PRESET_COCHAIN_COMPLEX, "上链复形：构造上链复形 (C^*, d)，满足 d² = 0", inputs, 2,
                               PRESET_TYPE_SEQUENCE,
                               "(C^\\bullet, d): \\cdots \\xrightarrow{d} C^{n-1} "
                               "\\xrightarrow{d} C^n \\xrightarrow{d} C^{n+1} \\xrightarrow{d} \\cdots, "
                               "\\quad d \\circ d = 0",
                               "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- cohomology_group：上同调群 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE, PRESET_TYPE_INTEGER};
        if (register_ha_preset(PRESET_COHOMOLOGY_GROUP, "上同调群：计算上链复形的第n个上同调群 H^n(C)", inputs, 2,
                               PRESET_TYPE_MODULE,
                               "H^n(C) = \\ker(d^n) / \\mathrm{im}(d^{n-1}) = "
                               "Z^n / B^n, \\quad Z^n = \\ker(d^n), B^n = \\mathrm{im}(d^{n-1})",
                               "O(n^3)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- boundary_operator：边缘算子 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE, PRESET_TYPE_INTEGER};
        if (register_ha_preset(PRESET_BOUNDARY_OPERATOR, "边缘算子：计算链的边缘 ∂(c)，满足 ∂² = 0", inputs, 2,
                               PRESET_TYPE_MODULE,
                               "\\partial_n: C_n \\to C_{n-1}, \\quad "
                               "\\partial_n\\left(\\sum_i a_i \\sigma_i\\right) = "
                               "\\sum_i a_i \\sum_{j=0}^{n} (-1)^j \\sigma_i|_{\\hat{v}_j}",
                               "O(n)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第二部分：正合序列（5个预设）
     *
     * 正合序列是同调代数的核心工具，描述模之间的精确关系。
     * 蛇引理和五引理是处理正合序列的基本技术。
     * ============================================================ */

    /* -------------------- exact_sequence_check：正合序列判定 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE};
        if (register_ha_preset(PRESET_EXACT_SEQUENCE_CHECK, "正合序列判定：判定序列在每个位置是否正合", inputs, 1,
                               PRESET_TYPE_BOOLEAN,
                               "\\cdots \\to M_{n+1} \\xrightarrow{f_{n+1}} M_n "
                               "\\xrightarrow{f_n} M_{n-1} \\to \\cdots \\text{ 正合} "
                               "\\Leftrightarrow \\ker(f_n) = \\mathrm{im}(f_{n+1}), \\forall n",
                               "O(n^2)", false, false)) {
            success_count++;
        }
    }

    /* -------------------- short_exact_sequence：短正合序列 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MODULE, PRESET_TYPE_MODULE, PRESET_TYPE_MODULE};
        if (register_ha_preset(PRESET_SHORT_EXACT_SEQUENCE, "短正合序列：构造短正合序列 0 → A → B → C → 0", inputs, 3,
                               PRESET_TYPE_SEQUENCE,
                               "0 \\to A \\xrightarrow{i} B \\xrightarrow{p} C \\to 0 \\text{ 正合} "
                               "\\Leftrightarrow i \\text{ 单}, p \\text{ 满}, \\ker(p) = \\mathrm{im}(i)",
                               "O(1)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- long_exact_sequence：长正合序列 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE};
        if (register_ha_preset(PRESET_LONG_EXACT_SEQUENCE, "长正合序列：由短正合序列诱导的同调长正合序列", inputs, 1,
                               PRESET_TYPE_SEQUENCE,
                               "0 \\to A_\\bullet \\to B_\\bullet \\to C_\\bullet \\to 0 "
                               "\\Rightarrow \\cdots \\to H_n(A) \\to H_n(B) \\to H_n(C) "
                               "\\xrightarrow{\\partial} H_{n-1}(A) \\to \\cdots",
                               "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- snake_lemma：蛇引理 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE, PRESET_TYPE_SEQUENCE, PRESET_TYPE_FUNCTION};
        if (register_ha_preset(PRESET_SNAKE_LEMMA, "蛇引理：构造蛇引理的正合序列，包含连接同态", inputs, 3,
                               PRESET_TYPE_SEQUENCE,
                               "\\ker(f') \\to \\ker(f) \\to \\ker(f'') "
                               "\\xrightarrow{\\partial} \\mathrm{coker}(f') \\to \\mathrm{coker}(f) "
                               "\\to \\mathrm{coker}(f'')",
                               "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- five_lemma：五引理 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE, PRESET_TYPE_SEQUENCE};
        if (register_ha_preset(PRESET_FIVE_LEMMA, "五引理：验证五引理（中间同态是同构）", inputs, 2,
                               PRESET_TYPE_BOOLEAN,
                               "A_1 \\xrightarrow{\\cong} B_1 \\to A_2 \\xrightarrow{f} B_2 \\to "
                               "A_3 \\xrightarrow{\\cong} B_3 \\Rightarrow f \\text{ 是同构}",
                               "O(n)", false, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第三部分：导出函子（5个预设）
     *
     * 导出函子是同调代数的核心概念，用于"修复"
     * 非正合函子。Ext和Tor是最重要的导出函子。
     * ============================================================ */

    /* -------------------- ext_functor：Ext函子 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MODULE, PRESET_TYPE_MODULE, PRESET_TYPE_INTEGER};
        if (register_ha_preset(PRESET_EXT_FUNCTOR, "Ext函子：计算 Ext^n_R(A, B)（扩张群）", inputs, 3,
                               PRESET_TYPE_MODULE,
                               "\\mathrm{Ext}^n_R(A, B) = H^n(\\mathrm{Hom}_R(P_\\bullet, B)), "
                               "\\quad P_\\bullet \\to A \\text{ 是投射分解}; "
                               "\\mathrm{Ext}^1_R(A, B) \\cong \\text{扩张类}",
                               "O(n!)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- tor_functor：Tor函子 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MODULE, PRESET_TYPE_MODULE, PRESET_TYPE_INTEGER};
        if (register_ha_preset(PRESET_TOR_FUNCTOR, "Tor函子：计算 Tor^R_n(A, B)（挠积）", inputs, 3, PRESET_TYPE_MODULE,
                               "\\mathrm{Tor}^R_n(A, B) = H_n(P_\\bullet \\otimes_R B), "
                               "\\quad P_\\bullet \\to A \\text{ 是投射分解}; "
                               "\\mathrm{Tor}^R_1(A, B) \\text{ 测量非平坦性}",
                               "O(n!)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- projective_resolution：投射分解 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MODULE};
        if (register_ha_preset(PRESET_PROJECTIVE_RESOLUTION, "投射分解：构造模的投射分解 P_• → M → 0", inputs, 1,
                               PRESET_TYPE_SEQUENCE,
                               "\\cdots \\to P_n \\to P_{n-1} \\to \\cdots \\to P_0 \\to M \\to 0, "
                               "\\quad P_i \\text{ 投射}, \\quad H_i(P_\\bullet) = "
                               "\\begin{cases} M & i=0 \\\\ 0 & i>0 \\end{cases}",
                               "O(n!)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- injective_resolution：内射分解 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MODULE};
        if (register_ha_preset(PRESET_INJECTIVE_RESOLUTION, "内射分解：构造模的内射分解 0 → M → I^•", inputs, 1,
                               PRESET_TYPE_SEQUENCE,
                               "0 \\to M \\to I^0 \\to I^1 \\to \\cdots \\to I^n \\to \\cdots, "
                               "\\quad I^i \\text{ 内射}, \\quad H^i(I^\\bullet) = "
                               "\\begin{cases} M & i=0 \\\\ 0 & i>0 \\end{cases}",
                               "O(n!)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- free_resolution：自由分解 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MODULE};
        if (register_ha_preset(PRESET_FREE_RESOLUTION, "自由分解：构造模的自由分解 F_• → M → 0", inputs, 1,
                               PRESET_TYPE_SEQUENCE,
                               "\\cdots \\to F_n \\to F_{n-1} \\to \\cdots \\to F_0 \\to M \\to 0, "
                               "\\quad F_i \\text{ 自由R模}",
                               "O(n!)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第四部分：谱序列（5个预设）
     *
     * 谱序列是计算同调群的强大工具，通过逐次逼近的方式
     * 计算极限项。Serre谱序列和Grothendieck谱序列
     * 是最重要的应用。
     * ============================================================ */

    /* -------------------- spectral_sequence：谱序列 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE, PRESET_TYPE_FUNCTION};
        if (register_ha_preset(PRESET_SPECTRAL_SEQUENCE, "谱序列：构造谱序列 {E_r^{p,q}}，满足 d_r² = 0", inputs, 2,
                               PRESET_TYPE_SEQUENCE,
                               "E_1^{p,q} \\Rightarrow E_2^{p,q} \\Rightarrow \\cdots \\Rightarrow E_\\infty^{p,q}, "
                               "\\quad d_r: E_r^{p,q} \\to E_r^{p+r, q-r+1}, "
                               "\\quad E_{r+1} = H(E_r, d_r)",
                               "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- serre_spectral_sequence：Serre谱序列 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_INTEGER};
        if (register_ha_preset(PRESET_SERRE_SPECTRAL_SEQUENCE, "Serre谱序列：计算纤维化 F → E → B 的Serre谱序列",
                               inputs, 2, PRESET_TYPE_SEQUENCE,
                               "E_2^{p,q} = H^p(B; H^q(F)) \\Rightarrow H^{p+q}(E), "
                               "\\quad F \\to E \\xrightarrow{\\pi} B \\text{ 是纤维化}",
                               "O(n!)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- grothendieck_spectral_sequence：Grothendieck谱序列 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION, PRESET_TYPE_MODULE};
        if (register_ha_preset(PRESET_GROTHENDIECK_SPECTRAL_SEQUENCE,
                               "Grothendieck谱序列：计算复合函子的Grothendieck谱序列", inputs, 3, PRESET_TYPE_SEQUENCE,
                               "E_2^{p,q} = (R^p F)(R^q G)(A) \\Rightarrow R^{p+q}(F \\circ G)(A), "
                               "\\quad G \\text{ 将内射对象映为F-零调对象}",
                               "O(n!)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- leray_spectral_sequence：Leray谱序列 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_FUNCTION};
        if (register_ha_preset(PRESET_LERAY_SPECTRAL_SEQUENCE, "Leray谱序列：计算连续映射的Leray谱序列", inputs, 2,
                               PRESET_TYPE_SEQUENCE,
                               "E_2^{p,q} = H^p(X, R^q f_* \\mathcal{F}) \\Rightarrow H^{p+q}(Y, \\mathcal{F}), "
                               "\\quad f: X \\to Y \\text{ 连续映射}",
                               "O(n!)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- spectral_sequence_convergence：谱序列收敛 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE, PRESET_TYPE_SEQUENCE};
        if (register_ha_preset(PRESET_SPECTRAL_SEQUENCE_CONVERGENCE, "谱序列收敛：判定谱序列是否收敛到目标", inputs, 2,
                               PRESET_TYPE_BOOLEAN,
                               "E_r^{p,q} \\Rightarrow E_\\infty^{p,q} = F^p H^{p+q} / F^{p+1} H^{p+q}, "
                               "\\quad F^\\bullet H^n \\text{ 是 } H^n \\text{ 的滤过}",
                               "O(n!)", false, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第五部分：同调维数（5个预设）
     *
     * 同调维数是衡量环和模复杂程度的重要不变量，
     * 包括投射维数、内射维数和整体维数。
     * ============================================================ */

    /* -------------------- projective_dimension：投射维数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MODULE};
        if (register_ha_preset(PRESET_PROJECTIVE_DIMENSION, "投射维数：计算模的投射维数 pd(M)", inputs, 1,
                               PRESET_TYPE_INTEGER,
                               "\\mathrm{pd}(M) = \\min\\{n : \\exists \\text{ 投射分解 } "
                               "0 \\to P_n \\to \\cdots \\to P_0 \\to M \\to 0\\} "
                               "= \\sup\\{n : \\mathrm{Ext}^n(M, -) \\neq 0\\}",
                               "O(n!)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- injective_dimension：内射维数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MODULE};
        if (register_ha_preset(PRESET_INJECTIVE_DIMENSION, "内射维数：计算模的内射维数 id(M)", inputs, 1,
                               PRESET_TYPE_INTEGER,
                               "\\mathrm{id}(M) = \\min\\{n : \\exists \\text{ 内射分解 } "
                               "0 \\to M \\to I^0 \\to \\cdots \\to I^n \\to 0\\} "
                               "= \\sup\\{n : \\mathrm{Ext}^n(-, M) \\neq 0\\}",
                               "O(n!)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- global_dimension：整体维数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_RING};
        if (register_ha_preset(PRESET_GLOBAL_DIMENSION, "整体维数：计算环的整体维数 gl.dim(R)", inputs, 1,
                               PRESET_TYPE_INTEGER,
                               "\\mathrm{gl.dim}(R) = \\sup\\{\\mathrm{pd}(M) : M \\in R\\text{-Mod}\\} "
                               "= \\sup\\{\\mathrm{id}(M) : M \\in R\\text{-Mod}\\} "
                               "= \\sup\\{n : \\mathrm{Ext}^n_R \\neq 0\\}",
                               "O(n!)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- homological_dimension：同调维数 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MODULE, PRESET_TYPE_RING};
        if (register_ha_preset(PRESET_HOMOLOGICAL_DIMENSION, "同调维数：计算模在环上的同调维数", inputs, 2,
                               PRESET_TYPE_INTEGER,
                               "\\dim_R(M) = \\mathrm{pd}_R(M), \\quad "
                               "\\text{性质：}\\dim_R(M) \\le \\mathrm{gl.dim}(R)",
                               "O(n!)", true, false)) {
            success_count++;
        }
    }

    /* -------------------- depth：深度 -------------------- */
    {
        PresetType inputs[] = {PRESET_TYPE_MODULE, PRESET_TYPE_IDEAL};
        if (register_ha_preset(PRESET_DEPTH, "深度：计算模在理想上的深度 depth_I(M)", inputs, 2, PRESET_TYPE_INTEGER,
                               "\\mathrm{depth}_I(M) = \\min\\{n : \\mathrm{Ext}^n_R(R/I, M) \\neq 0\\} "
                               "= \\text{M-正则序列的最大长度}, "
                               "\\quad I \\subseteq \\sqrt{\\mathrm{Ann}(M)}",
                               "O(n!)", true, false)) {
            success_count++;
        }
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == HOMOLOGICAL_ALGEBRA_PRESET_COUNT;
}

/**
 * @brief 获取同调代数预设函数块数量
 *
 * @return int 同调代数模块预设函数块总数（25）
 */
int preset_homological_algebra_count(void) {
    return HOMOLOGICAL_ALGEBRA_PRESET_COUNT;
}

/**
 * @brief 获取同调代数模块的预设类别
 *
 * 所有同调代数预设均属于代数学类别。
 *
 * @return PresetCategory 始终返回 PRESET_CATEGORY_ALGEBRA
 */
PresetCategory preset_homological_algebra_category(void) {
    return PRESET_CATEGORY_ALGEBRA;
}

/**
 * @brief 获取同调代数模块的所有预设名称列表
 *
 * @param out_names 输出名称数组
 * @param out_count 输出名称数量
 * @return true 成功
 * @return false 失败
 */
bool preset_homological_algebra_get_names(char ***out_names, int *out_count) {
    if (!out_names || !out_count)
        return false;

    /* 分配名称数组 */
    char **names = (char **) lv00_malloc(HOMOLOGICAL_ALGEBRA_PRESET_COUNT * sizeof(char *));
    if (!names)
        return false;

    /* 填充预设名称列表 */
    const char *preset_names[] = {
        /* 链复形与同调群 */
        PRESET_CHAIN_COMPLEX,
        PRESET_HOMOLOGY_GROUP,
        PRESET_COCHAIN_COMPLEX,
        PRESET_COHOMOLOGY_GROUP,
        PRESET_BOUNDARY_OPERATOR,
        /* 正合序列 */
        PRESET_EXACT_SEQUENCE_CHECK,
        PRESET_SHORT_EXACT_SEQUENCE,
        PRESET_LONG_EXACT_SEQUENCE,
        PRESET_SNAKE_LEMMA,
        PRESET_FIVE_LEMMA,
        /* 导出函子 */
        PRESET_EXT_FUNCTOR,
        PRESET_TOR_FUNCTOR,
        PRESET_PROJECTIVE_RESOLUTION,
        PRESET_INJECTIVE_RESOLUTION,
        PRESET_FREE_RESOLUTION,
        /* 谱序列 */
        PRESET_SPECTRAL_SEQUENCE,
        PRESET_SERRE_SPECTRAL_SEQUENCE,
        PRESET_GROTHENDIECK_SPECTRAL_SEQUENCE,
        PRESET_LERAY_SPECTRAL_SEQUENCE,
        PRESET_SPECTRAL_SEQUENCE_CONVERGENCE,
        /* 同调维数 */
        PRESET_PROJECTIVE_DIMENSION,
        PRESET_INJECTIVE_DIMENSION,
        PRESET_GLOBAL_DIMENSION,
        PRESET_HOMOLOGICAL_DIMENSION,
        PRESET_DEPTH,
    };

    int count = (int) (sizeof(preset_names) / sizeof(preset_names[0]));

    for (int i = 0; i < count; i++) {
        names[i] = lv00_strdup(preset_names[i]);
        if (names[i] == NULL) {
            /* 释放已分配的内存 */
            for (int j = 0; j < i; j++) {
                void *tmp = names[j];
                lv00_free(&tmp);
            }
            {
                void *tmp = names;
                lv00_free(&tmp);
            }
            return false;
        }
    }

    *out_names = names;
    *out_count = count;
    return true;
}
