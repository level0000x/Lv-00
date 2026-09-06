/**
 * @file geometry_transform_group.c
 * @brief 变换群与对称性识别（由 geometry_transform.c 拆分子模块）
 *
 * @details 变换群创建/预设（C2/C4/D2/Klein）、变换阶计算与
 *          约束图对称性自动识别。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/lv_platform.h"
#include "lv/lv_internal.h"

#include "lv/geometry_transform.h"

#include "lv/constraint_graph.h"
#include "lv/lv_numeric.h"
#include "lv/lv_strbuf.h"
#include "lv/lv_str_utils.h"
#include "lv/symbolic_coord.h"
#include "lv/lv_utils.h"
#include "lv/lv_xmacro.h" /* LV_DISPATCH_VOID */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * 对称性验证时从 double 重建符号坐标的缩放比例。
 * 非有理数坐标（代数数、二次根式等）经 lv_transform_apply_double 计算后，
 * 按该缩放比例重建为有理数坐标。缩放越大精度越高，但需保证
 * 坐标值 × 缩放不超过 int64 安全范围。
 */
#define SYMMETRY_COORD_SCALE 1000000LL

/** 变换群生成元最大数量 */
#define GROUP_MAX_GENERATORS 16

/* lv_transform_equal 定义于 geometry_transform_analysis.c（未列入公开头文件），此处前置声明 */
bool lv_transform_equal(const lvTransform *t1, const lvTransform *t2);

/* ============== 变换群实现 ============== */

lvTransformGroup *lv_transform_group_create(const char *name) {
    lvTransformGroup *group = (lvTransformGroup *) lv_calloc(1, sizeof(lvTransformGroup));
    if (!group) {
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_transform_group_create: calloc failed");
    }

    group->group_name = lv_strdup_safe(name);

    group->generators = (lvTransform **) lv_malloc(GROUP_MAX_GENERATORS * sizeof(lvTransform *));
    if (!group->generators) {
        lv_free((void **) &group);
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_transform_group_create: generators malloc failed");
    }

    return group;
}

void lv_transform_group_destroy(lvTransformGroup *group) {
    if (!group) {
        return;
    }

    for (uint32_t i = 0; i < group->generator_count; i++) {
        lv_transform_unref(group->generators[i]);
    }
    lv_free((void **) &group->generators);
    lv_free((void **) &group->group_name);
    lv_free((void **) &group);
}

bool lv_transform_group_add_generator(lvTransformGroup *group, lvTransform *generator) {
    if (!group || !generator || group->generator_count >= GROUP_MAX_GENERATORS) {
        return false;
    }

    lv_transform_ref(generator);
    group->generators[group->generator_count++] = generator;

    return true;
}

/* ---------- 预设群构造过程（查找表分发，替代 strcmp 分支链） ---------- */

/** @brief 预设群构造签名：向 group 填充生成器与群属性（zero/one 为预置有理数常量） */
typedef void (*PresetGroupBuilder)(lvTransformGroup *group, mpq_t zero, mpq_t one);

static void preset_group_c2(lvTransformGroup *group, mpq_t zero, mpq_t one) {
    (void)one;
    /* C2: 180度旋转 */
    lvTransform *rot = lv_transform_rotation(zero, zero, 180, 1);
    lv_transform_group_add_generator(group, rot);
    lv_transform_unref(rot);
    group->order = 2;
    group->is_abelian = true;
}

static void preset_group_c4(lvTransformGroup *group, mpq_t zero, mpq_t one) {
    (void)one;
    /* C4: 90度旋转 */
    lvTransform *rot = lv_transform_rotation(zero, zero, 90, 1);
    lv_transform_group_add_generator(group, rot);
    lv_transform_unref(rot);
    group->order = 4;
    group->is_abelian = true;
}

static void preset_group_d2(lvTransformGroup *group, mpq_t zero, mpq_t one) {
    (void)zero;
    (void)one;
    /* Klein 四元群：两个正交反射 */
    mpq_t ax, ay, bx, by;
    mpq_init(ax);
    mpq_init(ay);
    mpq_init(bx);
    mpq_init(by);
    mpq_set_ui(ax, 0, 1);
    mpq_set_ui(ay, 0, 1);
    mpq_set_ui(bx, 1, 1);
    mpq_set_ui(by, 0, 1);
    lvTransform *r1 = lv_transform_reflection(ax, ay, bx, by);

    mpq_set_ui(bx, 0, 1);
    mpq_set_ui(by, 1, 1);
    lvTransform *r2 = lv_transform_reflection(ax, ay, bx, by);

    lv_transform_group_add_generator(group, r1);
    lv_transform_group_add_generator(group, r2);
    lv_transform_unref(r1);
    lv_transform_unref(r2);
    mpq_clear(ax);
    mpq_clear(ay);
    mpq_clear(bx);
    mpq_clear(by);

    group->order = 4;
    group->is_abelian = true;
}

/** @brief 预设群名 -> 构造过程 查找表（D2 与 Klein 共享同一构造过程） */
static const struct {
    const char *name;
    PresetGroupBuilder builder;
} kPresetGroupBuilders[] = {
    {"C2", preset_group_c2},
    {"C4", preset_group_c4},
    {"D2", preset_group_d2},
    {"Klein", preset_group_d2},
};

lvTransformGroup *lv_transform_group_create_preset(const char *type) {
    if (!type) {
        lv_RETURN_ERROR_NULL(lv_ERROR_NULL_POINTER, "lv_transform_group_create_preset: NULL type");
    }

    lvTransformGroup *group = lv_transform_group_create(type);
    if (!group) {
        lv_RETURN_ERROR_NULL(lv_ERROR_OUT_OF_MEMORY, "lv_transform_group_create_preset: group creation failed");
    }

    mpq_t zero, one, neg_one;
    mpq_init(zero);
    mpq_init(one);
    mpq_init(neg_one);
    mpq_set_ui(zero, 0, 1);
    mpq_set_ui(one, 1, 1);
    mpq_set_si(neg_one, -1, 1);

    /* 按预设群名查表分发构造（替代 strcmp 分支链） */
    for (size_t gi = 0; gi < lv_ARRAY_SIZE(kPresetGroupBuilders); gi++) {
        if (lv_str_eq(type, kPresetGroupBuilders[gi].name)) {
            kPresetGroupBuilders[gi].builder(group, zero, one);
            break;
        }
    }

    mpq_clear(zero);
    mpq_clear(one);
    mpq_clear(neg_one);

    return group;
}

/* ============== 变换阶计算与对称性识别 ============== */

/** 变换阶计算的安全上限，防止无限循环 */
#define TRANSFORM_ORDER_MAX_ITERATIONS 1000

/**
 * @brief 计算变换的阶 -- 满足 T^n = I 的最小正整数 n
 *
 * 通过反复复合变换并检查是否为单位矩阵来确定阶。
 * 对于有限阶变换（如旋转 90 度 -> 阶为 4），返回最小的 n。
 * 对于无限阶变换（如非平凡平移），返回 0。
 *
 * @param t 变换指针（非 NULL）
 * @return n > 0: 变换的阶（T^n = 恒等变换）
 * @return 0: 变换为无限阶（如非零平移、非等比缩放）
 * @return -1: 参数无效（t 为 NULL 或矩阵无效）
 */
int lv_transform_order(const lvTransform *t) {
    if (!t || !t->matrix_valid) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "lv_transform_order: NULL or invalid matrix");
    }

    /* 恒等变换的阶为 1 */
    if (t->type == TRANSFORM_IDENTITY) {
        return 1;
    }

    /* 获取当前变换矩阵的有理数副本用于比较 */
    lvTransform *current = lv_transform_identity();
    if (!current) {
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_transform_order: identity creation failed");
    }

    /* 生成恒等矩阵用于比较 */
    lvTransform *identity = lv_transform_identity();
    if (!identity) {
        lv_transform_destroy(current);
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "lv_transform_order: identity creation failed");
    }

    /* 迭代计算 T^n，检查何时等于恒等变换 */
    for (int n = 1; n <= TRANSFORM_ORDER_MAX_ITERATIONS; n++) {
        /* current = current * t (即 T^n) */
        lvTransform *next = lv_transform_compose(current, t);
        lv_transform_destroy(current);
        current = next;

        if (!current) {
            /* 复合失败（内存不足等） */
            lv_transform_destroy(identity);
            lv_RETURN_ERROR(lv_ERROR_INTERNAL, "lv_transform_order: compose failed");
        }

        /* 检查 T^n 是否为单位矩阵 */
        if (lv_transform_equal(current, identity)) {
            lv_transform_destroy(current);
            lv_transform_destroy(identity);
            return n;
        }

        /* 对于平移变换，T^n 的平移量是 n 倍，永不等于恒等（除非 dx=dy=0）。
         * 快速检测：如果第一次复合后平移量不为零，且类型是平移，则无限阶 */
        if (n == 1 && t->type == TRANSFORM_TRANSLATION) {
            if (mpq_cmp_ui(current->matrix.tx, 0, 1) != 0 || mpq_cmp_ui(current->matrix.ty, 0, 1) != 0) {
                lv_transform_destroy(current);
                lv_transform_destroy(identity);
                return 0; /* 无限阶 */
            }
        }
    }

    /* 超过最大迭代次数，认为无限阶 */
    lv_transform_destroy(current);
    lv_transform_destroy(identity);
    return 0;
}

/**
 * @brief 由精确有理数创建有理型符号坐标
 *
 * 几何变换模块需要保留变换后坐标的精确性（用于退化线段 / 重复约束检测），
 * 因此对 RATIONAL 类型坐标走精确 mpq 路径，避免 double 舍入误差。
 * 该函数将 mpq 值封装为 SymbolicCoord（RATIONAL 类型），供验证流程使用。
 *
 * @param value 有理数值（mpq）
 * @param trust 继承原坐标的信任颜色
 * @return 新建的符号坐标，失败返回 NULL
 */
static SymbolicCoord *symbolic_coord_create_from_mpq(const mpq_t value, TrustColor trust) {
    SymbolicCoord *coord = lv_calloc(1, sizeof(SymbolicCoord));
    if (!coord) {
        return NULL;
    }

    coord->type = RATIONAL;
    coord->trust = trust;
    coord->cache_valid = false;
    coord->cached_value = 0.0;

    Rational *rat = lv_rational_from_mpq(value);
    if (!rat) {
        lv_free((void **) &coord);
        return NULL;
    }
    coord->data.rational = rat;

    return coord;
}

/**
 * @brief 验证候选变换是否保持约束图的所有约束关系
 *
 * 对称变换必须是约束图的自同构：对图中每个几何节点的符号坐标应用变换后，
 * 所有约束仍应成立。实现步骤：
 *   1. 用 graph_copy 深拷贝原图，在副本上操作，避免修改调用者的数据；
 *   2. 以 (x, y) 点对为单位遍历每个节点的符号坐标：
 *        - RATIONAL 坐标走精确有理数路径（mpq 矩阵运算，无浮点误差）；
 *        - 其他类型坐标经 lv_transform_apply_double 计算后按固定缩放重建；
 *   3. 调用 graph_check_compatibility 校验变换后图与原图具有相同的
 *      相容性状态——若出现退化线段（INCONSISTENT）或新增重复约束
 *      （OVER_CONSTRAINED）等差异，说明变换破坏了约束关系。
 *
 * @param graph 约束图（const，不会被修改）
 * @param t     候选变换（非 NULL，矩阵须有效）
 * @return true 表示变换保持约束图（对称变换），false 表示变换破坏了约束
 */
static bool transform_preserves_graph(const ConstraintGraph *graph, const lvTransform *t) {
    if (!graph || !t || !t->matrix_valid) {
        return false;
    }

    /* 评估原图的相容性状态，作为变换后状态对比的基准 */
    lvConstraintCompatibilityResult orig_compat;
    if (!graph_check_compatibility(graph, &orig_compat)) {
        return false;
    }

    /* 深拷贝原图，在副本上应用变换 */
    ConstraintGraph *temp = graph_copy(graph);
    if (!temp) {
        return false;
    }

    bool preserved = true;

    /* 逐节点应用变换：符号坐标以 (x, y) 点对连续存储（点为 1 对、线段为 2 对） */
    for (int i = 0; i < temp->node_count && preserved; i++) {
        GeomNode *node = temp->nodes[i];
        if (!node || !node->symbolic_coords || node->coord_count < 2) {
            continue;
        }
        for (int k = 0; k + 1 < node->coord_count; k += 2) {
            SymbolicCoord *sx = node->symbolic_coords[k];
            SymbolicCoord *sy = node->symbolic_coords[k + 1];
            if (!sx || !sy) {
                continue;
            }

            SymbolicCoord *nsx = NULL;
            SymbolicCoord *nsy = NULL;

            /* 有理数坐标：精确 mpq 路径，避免浮点舍入误差 */
            if (sx->type == RATIONAL && sy->type == RATIONAL && sx->data.rational && sy->data.rational) {
                mpq_t mx, my;
                mpq_init(mx);
                mpq_init(my);
                mpq_set(mx, sx->data.rational->value);
                mpq_set(my, sy->data.rational->value);
                if (lv_transform_apply_point(t, mx, my)) {
                    nsx = symbolic_coord_create_from_mpq(mx, sx->trust);
                    nsy = symbolic_coord_create_from_mpq(my, sy->trust);
                }
                mpq_clear(mx);
                mpq_clear(my);
            } else {
                /* 非有理坐标：double 路径，按固定缩放重建为有理坐标 */
                double dx = 0.0, dy = 0.0;
                lv_transform_apply_double(t, symbolic_coord_to_double(sx), symbolic_coord_to_double(sy), &dx, &dy);
                nsx = symbolic_coord_from_double_rounded(dx, SYMMETRY_COORD_SCALE);
                nsy = symbolic_coord_from_double_rounded(dy, SYMMETRY_COORD_SCALE);
            }

            if (!nsx || !nsy) {
                /* 坐标重建失败：释放已分配部分并判定变换未保持约束图 */
                symbolic_coord_destroy(nsx);
                symbolic_coord_destroy(nsy);
                preserved = false;
                break;
            }

            /* 用变换后的坐标替换原坐标（原坐标已由 graph_copy 深拷贝，可安全释放） */
            symbolic_coord_destroy(sx);
            symbolic_coord_destroy(sy);
            node->symbolic_coords[k] = nsx;
            node->symbolic_coords[k + 1] = nsy;
        }
    }

    if (preserved) {
        /* 校验所有约束在变换后是否仍然成立：变换后图的相容性状态必须与原图一致 */
        lvConstraintCompatibilityResult compat;
        if (!graph_check_compatibility(temp, &compat)) {
            preserved = false;
        } else {
            preserved = (compat.status == orig_compat.status);
        }
    }

    graph_destroy(temp);
    return preserved;
}

/**
 * @brief 分析约束图的所有对称变换
 *
 * 遍历约束图中的几何对象，识别保持约束关系的对称变换。
 * 检测的对称类型包括：
 *   - 关于坐标轴的反射
 *   - 关于原点的中心对称（180 度旋转）
 *   - 常见角度的旋转对称（60/90/120/180 度）
 *   - 简单平移对称
 *
 * 找到的对称变换以 lvTransform 指针数组的形式通过
 * out_transforms 输出。调用者负责销毁每个变换。
 *
 * @param graph           约束图指针（非 NULL）
 * @param out_transforms  输出：对称变换数组（由本函数分配，调用者负责 free）
 * @param max_count       输出数组的最大容量
 * @return 找到的对称变换数量（0 表示无对称或参数无效）
 */
int lv_transform_identify_symmetries(const ConstraintGraph *graph, lvTransform **out_transforms, int max_count) {
    if (!graph || !out_transforms || max_count <= 0) {
        return 0;
    }

    int found = 0;
    mpq_t zero, one;
    mpq_init(zero);
    mpq_init(one);
    mpq_set_ui(zero, 0, 1);
    mpq_set_ui(one, 1, 1);

    /* ---- 检测 1: 关于 x 轴的反射 ----
     * 反射直线: y = 0 (即 a=0, b=1, c=0 -> line from (0,0) to (1,0)) */
    if (found < max_count) {
        mpq_t ax, ay, bx, by;
        mpq_init(ax);
        mpq_init(ay);
        mpq_init(bx);
        mpq_init(by);
        mpq_set_ui(ax, 0, 1);
        mpq_set_ui(ay, 0, 1);
        mpq_set_ui(bx, 1, 1);
        mpq_set_ui(by, 0, 1);

        lvTransform *ref_x = lv_transform_reflection(ax, ay, bx, by);
        if (ref_x) {
            /* 验证 x 轴反射后所有约束是否仍然成立，成立才作为对称变换加入输出 */
            if (transform_preserves_graph(graph, ref_x)) {
                out_transforms[found++] = ref_x;
            } else {
                lv_transform_destroy(ref_x);
            }
        }

        mpq_clear(ax);
        mpq_clear(ay);
        mpq_clear(bx);
        mpq_clear(by);
    }

    /* ---- 检测 2: 关于 y 轴的反射 ----
     * 反射直线: x = 0 (即 a=1, b=0, c=0 -> line from (0,0) to (0,1)) */
    if (found < max_count) {
        mpq_t ax, ay, bx, by;
        mpq_init(ax);
        mpq_init(ay);
        mpq_init(bx);
        mpq_init(by);
        mpq_set_ui(ax, 0, 1);
        mpq_set_ui(ay, 0, 1);
        mpq_set_ui(bx, 0, 1);
        mpq_set_ui(by, 1, 1);

        lvTransform *ref_y = lv_transform_reflection(ax, ay, bx, by);
        if (ref_y) {
            /* 验证 y 轴反射后所有约束是否仍然成立，成立才作为对称变换加入输出 */
            if (transform_preserves_graph(graph, ref_y)) {
                out_transforms[found++] = ref_y;
            } else {
                lv_transform_destroy(ref_y);
            }
        }

        mpq_clear(ax);
        mpq_clear(ay);
        mpq_clear(bx);
        mpq_clear(by);
    }

    /* ---- 检测 3: 关于原点的 180 度旋转（中心对称） ---- */
    if (found < max_count) {
        lvTransform *rot180 = lv_transform_rotation(zero, zero, 180, 1);
        if (rot180) {
            /* 验证 180 度旋转后所有约束是否仍然成立，成立才作为对称变换加入输出 */
            if (transform_preserves_graph(graph, rot180)) {
                out_transforms[found++] = rot180;
            } else {
                lv_transform_destroy(rot180);
            }
        }
    }

    /* ---- 检测 4: 90 度旋转对称 ---- */
    if (found < max_count) {
        lvTransform *rot90 = lv_transform_rotation(zero, zero, 90, 1);
        if (rot90) {
            /* 验证 90 度旋转后所有约束是否仍然成立，成立才作为对称变换加入输出 */
            if (transform_preserves_graph(graph, rot90)) {
                out_transforms[found++] = rot90;
            } else {
                lv_transform_destroy(rot90);
            }
        }
    }

    /* ---- 检测 5: 120 度旋转对称（正三角形等） ---- */
    if (found < max_count) {
        lvTransform *rot120 = lv_transform_rotation(zero, zero, 120, 1);
        if (rot120) {
            /* 验证 120 度旋转后所有约束是否仍然成立，成立才作为对称变换加入输出 */
            if (transform_preserves_graph(graph, rot120)) {
                out_transforms[found++] = rot120;
            } else {
                lv_transform_destroy(rot120);
            }
        }
    }

    /* ---- 检测 6: 关于 y=x 的反射 ----
     * 反射直线: y=x (即 from (0,0) to (1,1)) */
    if (found < max_count) {
        mpq_t ax, ay, bx, by;
        mpq_init(ax);
        mpq_init(ay);
        mpq_init(bx);
        mpq_init(by);
        mpq_set_ui(ax, 0, 1);
        mpq_set_ui(ay, 0, 1);
        mpq_set_ui(bx, 1, 1);
        mpq_set_ui(by, 1, 1);

        lvTransform *ref_yx = lv_transform_reflection(ax, ay, bx, by);
        if (ref_yx) {
            /* 验证 y=x 反射后所有约束是否仍然成立，成立才作为对称变换加入输出 */
            if (transform_preserves_graph(graph, ref_yx)) {
                out_transforms[found++] = ref_yx;
            } else {
                lv_transform_destroy(ref_yx);
            }
        }

        mpq_clear(ax);
        mpq_clear(ay);
        mpq_clear(bx);
        mpq_clear(by);
    }

    mpq_clear(zero);
    mpq_clear(one);

    return found;
}
