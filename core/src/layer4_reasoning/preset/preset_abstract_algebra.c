/**
 * @file preset_abstract_algebra.c
 * @brief 抽象代数预设函数块模块 - 实现
 *
 * @details 实现抽象代数领域的预设函数块。
 *          模块包含40个预设，分为五大类别：
 *            - 群论（9个）：二元运算、逆元、单位元、子群判定、正规子群判定、
 *              商群、直积、半直积、群同态
 *            - 环论（8个）：环加法、环乘法、环单位元判定、理想判定、
 *              主理想、商环、多项式环、环同态
 *            - 域论（8个）：域加法、域乘法、域逆元、扩域次数、
 *              代数元判定、分裂域、Galois群、有限域
 *            - 模论（8个）：模加法、标量乘法、子模判定、商模、
 *              自由模、张量积、正合列判定、模同态
 *            - 范畴论（7个）：对象映射、态射复合、函子、自然变换、
 *              极限、余极限、伴随函子
 *
 * @module AbstractAlgebra
 * @category PRESET_CATEGORY_ALGEBRA
 * @version 1.1.0
 */

#include "preset_abstract_algebra.h"
#include "preset_blocks.h"
#include "preset_common.h"
#include "lv_internal.h"
#include "lv_utils.h"

#include <string.h>

/* ---- 内部辅助函数 ---- */

static bool register_algebra_preset(
    const char *name, const char *description,
    const PresetType *input_types, int input_count, PresetType output_type,
    const char *math_def, const char *complexity,
    bool is_constructive, bool is_reversible)
{
    return preset_blocks_register_simple(
        name, description, PRESET_CATEGORY_ALGEBRA,
        input_types, input_count, output_type,
        math_def, complexity, is_constructive, is_reversible);
}

#define REGISTER_ALGEBRA(name, desc, inputs, in_count, output, math, comp, cons, rev) \
    do { \
        if (register_algebra_preset( \
                (name), (desc), (inputs), (in_count), (output), \
                (math), (comp), (cons), (rev))) { \
            success_count++; \
        } \
    } while (0)

/* ---- 公共同类型数组 ---- */
static const PresetType TYPES_GROUP_GROUP[]  = {PRESET_TYPE_GROUP, PRESET_TYPE_GROUP};
static const PresetType TYPES_RING_RING[]    = {PRESET_TYPE_RING, PRESET_TYPE_RING};
static const PresetType TYPES_FIELD_FIELD[]  = {PRESET_TYPE_FIELD, PRESET_TYPE_FIELD};
static const PresetType TYPES_MODULE_MODULE[] = {PRESET_TYPE_MODULE, PRESET_TYPE_MODULE};
static const PresetType TYPES_SCALAR_MODULE[] = {PRESET_TYPE_FIELD, PRESET_TYPE_MODULE};

/* ---- 公共 API ---- */

bool preset_abstract_algebra_register(void) {
    int success_count = 0;

    /* ============ 群论 (9) ============ */
    REGISTER_ALGEBRA("群二元运算", "群 (G, ·) 的乘法 a · b",
                     TYPES_GROUP_GROUP, 2, PRESET_TYPE_GROUP,
                     "a \\cdot b \\in G", "O(1)", true, false);
    REGISTER_ALGEBRA("群逆元", "群元素 a 的逆元 a⁻¹",
                     TYPES_GROUP_GROUP, 1, PRESET_TYPE_GROUP,
                     "a^{-1}, \\ a \\cdot a^{-1} = e", "O(1)", true, false);
    REGISTER_ALGEBRA("群单位元判定", "判定 e 是否为群 G 的单位元",
                     TYPES_GROUP_GROUP, 2, PRESET_TYPE_BOOLEAN,
                     "\\forall a \\in G: e \\cdot a = a \\cdot e = a", "O(|G|)", false, false);
    REGISTER_ALGEBRA("子群判定", "判定 H 是否为 G 的子群",
                     TYPES_GROUP_GROUP, 2, PRESET_TYPE_BOOLEAN,
                     "H \\leq G \\iff (\\forall a,b \\in H: ab^{-1} \\in H)", "O(|H|^2)", false, false);
    REGISTER_ALGEBRA("正规子群判定", "判定 N 是否为 G 的正规子群",
                     TYPES_GROUP_GROUP, 2, PRESET_TYPE_BOOLEAN,
                     "N \\trianglelefteq G \\iff (\\forall g \\in G, n \\in N: gng^{-1} \\in N)", "O(|G||N|)", false, false);
    REGISTER_ALGEBRA("商群构造", "由群 G 和正规子群 N 构造商群 G/N",
                     TYPES_GROUP_GROUP, 2, PRESET_TYPE_GROUP,
                     "G/N = \\{gN : g \\in G\\}", "O(|G|)", true, false);
    REGISTER_ALGEBRA("群直积", "两个群的直积 G × H",
                     TYPES_GROUP_GROUP, 2, PRESET_TYPE_GROUP,
                     "G \\times H = \\{(g,h): g \\in G, h \\in H\\}", "O(|G||H|)", true, false);
    REGISTER_ALGEBRA("群半直积", "两个群的半直积 G ⋊ H",
                     TYPES_GROUP_GROUP, 2, PRESET_TYPE_GROUP,
                     "G \\rtimes H", "O(|G||H|)", true, false);
    REGISTER_ALGEBRA("群同态核像", "群同态 φ: G → H 的核与像",
                     TYPES_GROUP_GROUP, 2, PRESET_TYPE_GROUP,
                     "\\ker\\phi = \\{g \\in G: \\phi(g) = e_H\\}", "O(|G|)", true, false);

    /* ============ 环论 (8) ============ */
    REGISTER_ALGEBRA("环加法群", "环 (R, +, ·) 的加法群结构",
                     TYPES_RING_RING, 1, PRESET_TYPE_GROUP,
                     "(R, +) \\text{ is an abelian group}", "O(1)", true, false);
    REGISTER_ALGEBRA("环乘法半群", "环 (R, +, ·) 的乘法半群结构",
                     TYPES_RING_RING, 1, PRESET_TYPE_GROUP,
                     "(R, \\cdot) \\text{ is a semigroup}", "O(1)", true, false);
    REGISTER_ALGEBRA("环单位元判定", "判定 1 是否为环 R 的乘法单位元",
                     TYPES_RING_RING, 2, PRESET_TYPE_BOOLEAN,
                     "\\forall r \\in R: 1 \\cdot r = r \\cdot 1 = r", "O(|R|)", false, false);
    REGISTER_ALGEBRA("理想判定", "判定 I 是否为环 R 的理想",
                     TYPES_RING_RING, 2, PRESET_TYPE_BOOLEAN,
                     "I \\trianglelefteq R \\iff (\\forall r \\in R, i \\in I: ri,ir \\in I)", "O(|R||I|)", false, false);
    REGISTER_ALGEBRA("主理想生成", "由元素 a ∈ R 生成的主理想 ⟨a⟩",
                     TYPES_RING_RING, 2, PRESET_TYPE_RING,
                     "\\langle a \\rangle = \\{ra : r \\in R\\}", "O(|R|)", true, false);
    REGISTER_ALGEBRA("商环构造", "由环 R 和理想 I 构造商环 R/I",
                     TYPES_RING_RING, 2, PRESET_TYPE_RING,
                     "R/I = \\{r + I : r \\in R\\}", "O(|R|)", true, false);
    REGISTER_ALGEBRA("多项式环", "构造环 R 上的多项式环 R[x]",
                     TYPES_RING_RING, 1, PRESET_TYPE_RING,
                     "R[x] = \\{\\sum a_i x^i : a_i \\in R\\}", "O(n)", true, false);
    REGISTER_ALGEBRA("环同态核像", "环同态 φ: R → S 的核与像",
                     TYPES_RING_RING, 2, PRESET_TYPE_RING,
                     "\\ker\\phi = \\{r \\in R: \\phi(r) = 0_S\\}", "O(|R|)", true, false);

    /* ============ 域论 (8) ============ */
    REGISTER_ALGEBRA("域加法群", "域 (F, +, ·) 的加法群",
                     TYPES_FIELD_FIELD, 1, PRESET_TYPE_GROUP,
                     "(F, +) \\text{ is an abelian group}", "O(1)", true, false);
    REGISTER_ALGEBRA("域乘法群", "域 F 的非零元乘法群 F×",
                     TYPES_FIELD_FIELD, 1, PRESET_TYPE_GROUP,
                     "F^{\\times} = F \\setminus \\{0\\}", "O(1)", true, false);
    REGISTER_ALGEBRA("域逆元计算", "域中非零元素 a 的乘法逆元",
                     TYPES_FIELD_FIELD, 1, PRESET_TYPE_FIELD,
                     "a^{-1}, \\ a \\cdot a^{-1} = 1 \\ (a \\neq 0)", "O(\\log p)", true, false);
    REGISTER_ALGEBRA("扩域次数", "计算域扩张 E/F 的次数 [E:F]",
                     TYPES_FIELD_FIELD, 2, PRESET_TYPE_NUMBER,
                     "[E:F] = \\dim_F E", "O(|E|)", true, false);
    REGISTER_ALGEBRA("代数元判定", "判定 α 在域 F 上是否代数元",
                     TYPES_FIELD_FIELD, 2, PRESET_TYPE_BOOLEAN,
                     "\\exists p(x) \\in F[x], p \\neq 0: p(\\alpha) = 0", "O(n^2)", false, false);
    REGISTER_ALGEBRA("分裂域构造", "构造多项式 f 在 F 上的分裂域",
                     TYPES_FIELD_FIELD, 2, PRESET_TYPE_FIELD,
                     "\\text{Splitting field of } f \\text{ over } F", "O(n!)", true, false);
    REGISTER_ALGEBRA("Galois 群计算", "计算 Galois 扩张 E/F 的 Galois 群",
                     TYPES_FIELD_FIELD, 2, PRESET_TYPE_GROUP,
                     "\\text{Gal}(E/F) = \\text{Aut}_F(E)", "O(n!)", true, false);
    REGISTER_ALGEBRA("有限域构造", "构造 q = pⁿ 元有限域 GF(q)",
                     TYPES_FIELD_FIELD, 2, PRESET_TYPE_FIELD,
                     "\\mathbb{F}_q, \\ q = p^n", "O(p^n)", true, false);

    /* ============ 模论 (8) ============ */
    REGISTER_ALGEBRA("模加法群", "R-模 M 的加法群结构",
                     TYPES_MODULE_MODULE, 1, PRESET_TYPE_GROUP,
                     "(M, +) \\text{ is an abelian group}", "O(1)", true, false);
    REGISTER_ALGEBRA("标量乘法", "环 R 的元素 r 与模元素 m 的标量乘法",
                     TYPES_SCALAR_MODULE, 2, PRESET_TYPE_MODULE,
                     "r \\cdot m \\in M, \\ r \\in R, m \\in M", "O(1)", true, false);
    REGISTER_ALGEBRA("子模判定", "判定 N 是否为 R-模 M 的子模",
                     TYPES_MODULE_MODULE, 2, PRESET_TYPE_BOOLEAN,
                     "N \\leq M \\iff (\\forall n_1,n_2 \\in N, r \\in R: n_1+rn_2 \\in N)", "O(|N|^2)", false, false);
    REGISTER_ALGEBRA("商模构造", "构造商模 M/N",
                     TYPES_MODULE_MODULE, 2, PRESET_TYPE_MODULE,
                     "M/N = \\{m + N : m \\in M\\}", "O(|M|)", true, false);
    REGISTER_ALGEBRA("自由模判定", "判定 R-模 M 是否为自由模",
                     TYPES_MODULE_MODULE, 1, PRESET_TYPE_BOOLEAN,
                     "M \\cong \\bigoplus R", "O(|M|)", false, false);
    REGISTER_ALGEBRA("张量积计算", "计算模 M 和 N 的张量积 M ⊗ N",
                     TYPES_MODULE_MODULE, 2, PRESET_TYPE_MODULE,
                     "M \\otimes_R N", "O(|M||N|)", true, false);
    REGISTER_ALGEBRA("正合列判定", "判定序列 M→N→P 是否正合",
                     TYPES_MODULE_MODULE, 3, PRESET_TYPE_BOOLEAN,
                     "\\text{im}(f) = \\ker(g)", "O(|N|^2)", false, false);
    REGISTER_ALGEBRA("模同态核像", "模同态 φ: M → N 的核与像",
                     TYPES_MODULE_MODULE, 2, PRESET_TYPE_MODULE,
                     "\\ker\\phi = \\{m \\in M: \\phi(m) = 0\\}", "O(|M|)", true, false);

    /* ============ 范畴论 (7) ============ */
    REGISTER_ALGEBRA("对象映射", "函子 F: C → D 的对象映射 F(X)",
                     NULL, 1, PRESET_TYPE_OBJECT,
                     "F: \\text{Ob}(C) \\to \\text{Ob}(D)", "O(1)", true, false);
    REGISTER_ALGEBRA("态射复合", "范畴中态射的复合 g ∘ f",
                     NULL, 2, PRESET_TYPE_MORPHISM,
                     "g \\circ f: X \\to Z", "O(1)", true, false);
    REGISTER_ALGEBRA("函子构造", "构造协变函子 F: C → D",
                     NULL, 2, PRESET_TYPE_FUNCTOR,
                     "F: C \\to D \\text{ (covariant)}", "O(|C|)", true, false);
    REGISTER_ALGEBRA("自然变换判定", "判定 η: F ⇒ G 是否为自然变换",
                     NULL, 2, PRESET_TYPE_BOOLEAN,
                     "\\forall f: X \\to Y: G(f) \\circ \\eta_X = \\eta_Y \\circ F(f)", "O(|C|^2)", false, false);
    REGISTER_ALGEBRA("极限计算", "计算函子 F: J → C 的极限",
                     NULL, 1, PRESET_TYPE_OBJECT,
                     "\\varprojlim F", "O(|J||C|)", true, false);
    REGISTER_ALGEBRA("余极限计算", "计算函子 F: J → C 的余极限",
                     NULL, 1, PRESET_TYPE_OBJECT,
                     "\\varinjlim F", "O(|J||C|)", true, false);
    REGISTER_ALGEBRA("伴随函子判定", "判定 F ⊣ G 是否伴随",
                     NULL, 2, PRESET_TYPE_BOOLEAN,
                     "\\text{Hom}_D(FX,Y) \\cong \\text{Hom}_C(X,GY)", "O(|C||D|)", false, false);

    return (success_count == ABSTRACT_ALGEBRA_PRESET_COUNT);
}

int preset_abstract_algebra_count(void) {
    return ABSTRACT_ALGEBRA_PRESET_COUNT;
}

bool preset_abstract_algebra_get_names(char ***out_names, int *out_count) {
    if (!out_names || !out_count) return false;

    static const char *names[] = {
        /* 群论 */
        "群二元运算", "群逆元", "群单位元判定", "子群判定", "正规子群判定",
        "商群构造", "群直积", "群半直积", "群同态核像",
        /* 环论 */
        "环加法群", "环乘法半群", "环单位元判定", "理想判定",
        "主理想生成", "商环构造", "多项式环", "环同态核像",
        /* 域论 */
        "域加法群", "域乘法群", "域逆元计算", "扩域次数",
        "代数元判定", "分裂域构造", "Galois 群计算", "有限域构造",
        /* 模论 */
        "模加法群", "标量乘法", "子模判定", "商模构造",
        "自由模判定", "张量积计算", "正合列判定", "模同态核像",
        /* 范畴论 */
        "对象映射", "态射复合", "函子构造", "自然变换判定",
        "极限计算", "余极限计算", "伴随函子判定"
    };

    *out_count = ABSTRACT_ALGEBRA_PRESET_COUNT;
    *out_names = lv_malloc(sizeof(char *) * (*out_count));
    if (!*out_names) return false;

    for (int i = 0; i < *out_count; i++) {
        (*out_names)[i] = lv_strdup(names[i]);
        if (!(*out_names)[i]) {
            for (int j = 0; j < i; j++) lv_free((*out_names)[j]);
            lv_free(*out_names);
            *out_names = NULL;
            return false;
        }
    }

    return true;
}
