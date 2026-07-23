/**
 * @file preset_functional_analysis.c
 * @brief 泛函分析预设函数块 - 实现
 *
 * @details 实现泛函分析模块的所有预设函数块。
 *          采用统一的注册接口 preset_blocks_register_simple。
 *          共22个预设，涵盖赋范空间、内积空间、线性算子理论、
 *          三大基本定理、一致有界原理、弱收敛、对偶与伴随、
 *          投影与正交化以及不动点理论。
 *
 * @module FunctionalAnalysis
 * @category PRESET_CATEGORY_ANALYSIS
 * @version 1.0.0
 */

#include "preset_functional_analysis.h"

#include <string.h>

#include "lv_internal.h"
#include "lv_utils.h"
#include "preset_blocks.h"

/* ============================================================
 * 预设数量定义
 * ============================================================ */

/** 泛函分析模块预设函数块总数 */

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/**
 * @brief 注册单个泛函分析预设
 */
static bool register_fa_preset(const char *name, const char *description, const PresetType *input_types,
                               int input_count, PresetType output_type, const char *math_def, const char *complexity,
                               bool is_constructive, bool is_reversible) {
    return preset_blocks_register_simple(name, description, PRESET_CATEGORY_ANALYSIS, input_types, input_count,
                                         output_type, math_def, complexity, is_constructive, is_reversible);
}

/* ============================================================
 * 模块注册实现
 * ============================================================ */

bool preset_functional_analysis_register(void) {
    int success_count = 0;

    /* ============================================================
     * 第一部分：赋范空间 (2个)
     * ============================================================ */

    /* 范数判定 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SPACE};
        if (register_fa_preset(PRESET_FA_NORM_CHECK, "范数判定：验证函数 ||·|| 是否满足正定性、齐次性、三角不等式",
                               inputs, 2, PRESET_TYPE_BOOLEAN,
                               "\\|x\\|=0\\Leftrightarrow x=0,"
                               "\\ \\|\\alpha x\\|=|\\alpha|\\|x\\|,"
                               "\\ \\|x+y\\|\\leq\\|x\\|+\\|y\\|",
                               "O(\\infty)", false, false)) {
            success_count++;
        }
    }

    /* Banach空间判定 */
    {
        PresetType inputs[] = {PRESET_TYPE_SPACE};
        if (register_fa_preset(PRESET_FA_BANACH_SPACE_CHECK, "Banach空间判定：验证赋范空间是否完备（Cauchy列收敛）",
                               inputs, 1, PRESET_TYPE_BOOLEAN,
                               "\\forall\\{x_n\\}: \\|x_m-x_n\\|\\to0\\Rightarrow\\exists x: \\|x_n-x\\|\\to0",
                               "O(\\infty)", false, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第二部分：内积空间 (2个)
     * ============================================================ */

    /* 内积判定 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SPACE};
        if (register_fa_preset(PRESET_FA_INNER_PRODUCT_CHECK, "内积判定：验证二元函数是否满足内积公理", inputs, 2,
                               PRESET_TYPE_BOOLEAN,
                               "\\langle x,x\\rangle\\geq0,\\ \\langle x,y\\rangle="
                               "\\overline{\\langle y,x\\rangle},"
                               "\\ \\langle\\alpha x+\\beta y,z\\rangle="
                               "\\alpha\\langle x,z\\rangle+\\beta\\langle y,z\\rangle",
                               "O(\\infty)", false, false)) {
            success_count++;
        }
    }

    /* Hilbert空间判定 */
    {
        PresetType inputs[] = {PRESET_TYPE_SPACE};
        if (register_fa_preset(PRESET_FA_HILBERT_SPACE_CHECK, "Hilbert空间判定：验证内积空间是否关于诱导范数完备",
                               inputs, 1, PRESET_TYPE_BOOLEAN,
                               "\\|x\\|=\\sqrt{\\langle x,x\\rangle},"
                               "\\ \\forall\\{x_n\\}\\text{ Cauchy}\\Rightarrow\\exists x: \\|x_n-x\\|\\to0",
                               "O(\\infty)", false, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第三部分：线性算子 (3个)
     * ============================================================ */

    /* 有界性判定 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SPACE, PRESET_TYPE_SPACE};
        if (register_fa_preset(PRESET_FA_BOUNDED_OPERATOR, "有界性判定：T:X→Y 线性，∃M>0: ||Tx||≤M||x||", inputs, 3,
                               PRESET_TYPE_BOOLEAN, "\\|T\\| = \\sup_{\\|x\\|\\leq1}\\|Tx\\| < \\infty", "O(\\infty)",
                               false, false)) {
            success_count++;
        }
    }

    /* 紧算子判定 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SPACE};
        if (register_fa_preset(
                PRESET_FA_COMPACT_OPERATOR, "紧算子判定：有界集在T下的像的闭包为紧集", inputs, 2, PRESET_TYPE_BOOLEAN,
                "\\forall B\\subset X\\text{ 有界}: \\overline{T(B)}\\text{ 为紧集}", "O(\\infty)", false, false)) {
            success_count++;
        }
    }

    /* 谱分析 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_fa_preset(PRESET_FA_SPECTRAL_ANALYSIS, "谱分析：分析有界线性算子T的谱集组成", inputs, 1,
                               PRESET_TYPE_LIST,
                               "\\sigma(T) = \\{\\lambda: (T-\\lambda I)^{-1}\\text{ 不存在或无界}\\}"
                               "= \\sigma_p\\cup\\sigma_c\\cup\\sigma_r",
                               "O(\\infty)", true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第四部分：三大基本定理 (3个)
     * ============================================================ */

    /* Hahn-Banach定理 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SPACE, PRESET_TYPE_SPACE};
        if (register_fa_preset(PRESET_FA_HAHN_BANACH, "Hahn-Banach定理：将子空间上的有界线性泛函保范延拓到全空间",
                               inputs, 3, PRESET_TYPE_FUNCTION, "\\exists F\\in X^*: F|_M = f,\\ \\|F\\| = \\|f\\|",
                               "O(\\infty)", true, false)) {
            success_count++;
        }
    }

    /* 开映射定理 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SET};
        if (register_fa_preset(PRESET_FA_OPEN_MAPPING, "开映射定理：Banach空间之间的满射有界线性算子将开集映射为开集",
                               inputs, 2, PRESET_TYPE_SET,
                               "T\\text{ 满射}\\Rightarrow T(U)\\text{ 为开集},\\forall U\\text{ 开集}", "O(\\infty)",
                               true, false)) {
            success_count++;
        }
    }

    /* 闭图像定理 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SPACE, PRESET_TYPE_SPACE};
        if (register_fa_preset(PRESET_FA_CLOSED_GRAPH, "闭图像定理：图像为闭集的线性算子必有界", inputs, 3,
                               PRESET_TYPE_BOOLEAN, "\\Gamma(T)\\text{ 闭}\\Rightarrow T\\text{ 有界}", "O(\\infty)",
                               false, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第五部分：一致有界原理 (1个)
     * ============================================================ */

    /* 一致有界原理 */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST, PRESET_TYPE_SPACE};
        if (register_fa_preset(PRESET_FA_UNIFORM_BOUNDEDNESS, "一致有界原理：逐点有界的算子族一致有界", inputs, 2,
                               PRESET_TYPE_BOOLEAN,
                               "\\sup_\\alpha\\|T_\\alpha x\\|<\\infty\\ (\\forall x)"
                               "\\Rightarrow\\sup_\\alpha\\|T_\\alpha\\|<\\infty",
                               "O(\\infty)", false, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第六部分：弱收敛 (2个)
     * ============================================================ */

    /* 弱收敛 */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE, PRESET_TYPE_VECTOR, PRESET_TYPE_SPACE};
        if (register_fa_preset(PRESET_FA_WEAK_CONVERGENCE, "弱收敛：x_n ⇀ x ⇔ ∀f∈X*: f(x_n)→f(x)", inputs, 3,
                               PRESET_TYPE_BOOLEAN,
                               "x_n\\rightharpoonup x \\Leftrightarrow "
                               "\\forall f\\in X^*: \\lim f(x_n)=f(x)",
                               "O(\\infty)", false, false)) {
            success_count++;
        }
    }

    /* 弱*收敛 */
    {
        PresetType inputs[] = {PRESET_TYPE_SEQUENCE, PRESET_TYPE_FUNCTION, PRESET_TYPE_SPACE};
        if (register_fa_preset(PRESET_FA_WEAK_STAR_CONVERGENCE, "弱*收敛：f_n ⇀* f ⇔ ∀x∈X: f_n(x)→f(x)", inputs, 3,
                               PRESET_TYPE_BOOLEAN,
                               "f_n\\rightharpoonup^* f \\Leftrightarrow "
                               "\\forall x\\in X: \\lim f_n(x)=f(x)",
                               "O(\\infty)", false, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第七部分：对偶与伴随 (3个)
     * ============================================================ */

    /* 对偶空间 */
    {
        PresetType inputs[] = {PRESET_TYPE_SPACE};
        if (register_fa_preset(PRESET_FA_DUAL_SPACE, "对偶空间：X* = L(X,F)，所有有界线性泛函构成的对偶空间", inputs, 1,
                               PRESET_TYPE_SPACE, "X^* = \\{f:X\\to\\mathbb{F}: f\\text{ 线性有界}\\}", "O(1)", true,
                               false)) {
            success_count++;
        }
    }

    /* 伴随算子 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_fa_preset(PRESET_FA_ADJOINT_OPERATOR, "伴随算子：⟨Tx,y⟩ = ⟨x,T*y⟩，T*为T的Hilbert伴随", inputs, 1,
                               PRESET_TYPE_FUNCTION, "\\langle Tx, y\\rangle = \\langle x, T^*y\\rangle", "O(\\infty)",
                               true, false)) {
            success_count++;
        }
    }

    /* 自伴算子 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION};
        if (register_fa_preset(PRESET_FA_SELF_ADJOINT, "自伴算子判定：T = T*，即 ⟨Tx,y⟩ = ⟨x,Ty⟩", inputs, 1,
                               PRESET_TYPE_BOOLEAN,
                               "T = T^* \\Leftrightarrow \\langle Tx, y\\rangle = \\langle x, Ty\\rangle", "O(\\infty)",
                               false, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第八部分：投影与正交化 (3个)
     * ============================================================ */

    /* 正交投影 */
    {
        PresetType inputs[] = {PRESET_TYPE_VECTOR, PRESET_TYPE_SPACE};
        if (register_fa_preset(PRESET_FA_ORTHOGONAL_PROJECTION, "正交投影：计算 x 在闭子空间 M 上的正交投影 P_M x",
                               inputs, 2, PRESET_TYPE_VECTOR, "x - P_M x \\perp M,\\quad P_M x \\in M", "O(n)", true,
                               false)) {
            success_count++;
        }
    }

    /* Gram-Schmidt正交化 */
    {
        PresetType inputs[] = {PRESET_TYPE_LIST};
        if (register_fa_preset(PRESET_FA_GRAM_SCHMIDT, "Gram-Schmidt正交化：将线性无关向量组正交规范化", inputs, 1,
                               PRESET_TYPE_LIST,
                               "e_k = v_k - \\sum_{j=1}^{k-1}"
                               "\\frac{\\langle v_k,e_j\\rangle}{\\langle e_j,e_j\\rangle}e_j",
                               "O(n^2)", true, false)) {
            success_count++;
        }
    }

    /* Riesz表示定理 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SPACE};
        if (register_fa_preset(PRESET_FA_RIESZ_REPRESENTATION,
                               "Riesz表示定理：∃!y_f∈H: f(x) = ⟨x,y_f⟩, ||f|| = ||y_f||", inputs, 2, PRESET_TYPE_VECTOR,
                               "\\forall f\\in H^*,\\ \\exists! y_f\\in H: f(x)=\\langle x,y_f\\rangle", "O(\\infty)",
                               true, false)) {
            success_count++;
        }
    }

    /* ============================================================
     * 第九部分：不动点理论 (2个)
     * ============================================================ */

    /* Banach不动点定理 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SPACE};
        if (register_fa_preset(PRESET_FA_FIXED_POINT, "Banach不动点定理：完备度量空间上的压缩映射存在唯一不动点",
                               inputs, 2, PRESET_TYPE_VECTOR,
                               "\\exists! x^*: T(x^*) = x^*,"
                               "\\quad d(Tx,Ty)\\leq k\\,d(x,y),\\ k\\in(0,1)",
                               "O(k^n)", true, false)) {
            success_count++;
        }
    }

    /* 压缩映射判定 */
    {
        PresetType inputs[] = {PRESET_TYPE_FUNCTION, PRESET_TYPE_SPACE};
        if (register_fa_preset(PRESET_FA_CONTRACTION_MAPPING, "压缩映射判定：∃k∈(0,1): d(Tx,Ty) ≤ k·d(x,y)", inputs, 2,
                               PRESET_TYPE_BOOLEAN, "\\exists k\\in(0,1): d(Tx,Ty)\\leq k\\cdot d(x,y),\\ \\forall x,y",
                               "O(\\infty)", false, false)) {
            success_count++;
        }
    }

    /* 返回是否所有预设都注册成功 */
    return success_count == FUNCTIONAL_ANALYSIS_PRESET_COUNT;
}

/* ============================================================
 * 模块信息接口
 * ============================================================ */

int preset_functional_analysis_count(void) {
    return FUNCTIONAL_ANALYSIS_PRESET_COUNT;
}

PresetCategory preset_functional_analysis_category(void) {
    return PRESET_CATEGORY_ANALYSIS;
}

bool preset_functional_analysis_get_names(char ***out_names, int *out_count) {
    if (!out_names || !out_count)
        return false;

    char **names = (char **) lv_malloc(FUNCTIONAL_ANALYSIS_PRESET_COUNT * sizeof(char *));
    if (!names)
        return false;

    const char *preset_names[] = {
        /* 赋范空间 */
        PRESET_FA_NORM_CHECK,
        PRESET_FA_BANACH_SPACE_CHECK,
        /* 内积空间 */
        PRESET_FA_INNER_PRODUCT_CHECK,
        PRESET_FA_HILBERT_SPACE_CHECK,
        /* 线性算子 */
        PRESET_FA_BOUNDED_OPERATOR,
        PRESET_FA_COMPACT_OPERATOR,
        PRESET_FA_SPECTRAL_ANALYSIS,
        /* 三大基本定理 */
        PRESET_FA_HAHN_BANACH,
        PRESET_FA_OPEN_MAPPING,
        PRESET_FA_CLOSED_GRAPH,
        /* 一致有界原理 */
        PRESET_FA_UNIFORM_BOUNDEDNESS,
        /* 弱收敛 */
        PRESET_FA_WEAK_CONVERGENCE,
        PRESET_FA_WEAK_STAR_CONVERGENCE,
        /* 对偶与伴随 */
        PRESET_FA_DUAL_SPACE,
        PRESET_FA_ADJOINT_OPERATOR,
        PRESET_FA_SELF_ADJOINT,
        /* 投影与正交化 */
        PRESET_FA_ORTHOGONAL_PROJECTION,
        PRESET_FA_GRAM_SCHMIDT,
        PRESET_FA_RIESZ_REPRESENTATION,
        /* 不动点理论 */
        PRESET_FA_FIXED_POINT,
        PRESET_FA_CONTRACTION_MAPPING,
    };

    int count = (int) (sizeof(preset_names) / sizeof(preset_names[0]));

    for (int i = 0; i < count; i++) {
        names[i] = lv_strdup(preset_names[i]);
        if (names[i] == NULL) {
            for (int j = 0; j < i; j++) {
                {
                    void *tmp = names[j];
                    lv_free(&tmp);
                }
            }
            {
                void *tmp = names;
                lv_free(&tmp);
            }
            return false;
        }
    }

    *out_names = names;
    *out_count = count;
    return true;
}
