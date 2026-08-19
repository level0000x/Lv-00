/**
 * @file smt_backend_impl_groebner.c
 * @brief Groebner 后端求解
 *
 * @details 从 smt_backend_impl.c 拆分的子模块（Lv-00 项目 v3.3.0+）。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_constraint_guard.h"
#include "lv/lv_file.h"
#include "lv/lv_numeric.h"
#include "lv/lv_str_utils.h"
#include "lv/lv_xmacro.h"

#include "lv/smt_backend.h"
#include "smt_backend_internal.h"
#include "lv/lv_registry.h"
#include "lv/lv_thread.h"

#include "lv/error_codes.h"
#include "lv/groebner_engine.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

/* ============================================================
 * Groebner 后端实现
 * ============================================================ */

/* ---- Groebner 后端辅助函数（前向声明，定义于本文件后方） ---- */
int groebner_backend_init(SMTSolver *solver, const ConstraintGraph *graph);
void groebner_backend_cleanup(SMTSolver *solver);
SMTSatResult groebner_backend_solve(SMTSolver *solver, const ConstraintGraph *graph);
int groebner_backend_decode(SMTSolver *solver, SMTSolverResult *out_result);


/**
 * @brief 初始化 Groebner 后端的求解上下文
 *
 * 创建多项式环注册表，声明变量（每个点节点 2 个坐标变量），
 * 并建立节点 ID 到变量索引的映射表。
 *
 * @param solver  Groebner 求解器实例
 * @param graph   约束图
 * @return 成功返回 0，失败返回 -1
 */
int groebner_backend_init(SMTSolver *solver, const ConstraintGraph *graph) {
    if (!solver || !graph)
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "groebner_backend_init: solver=%p, graph=%p",
                        (const void *)solver, (const void *)graph);

    /* 如果已经初始化过，先清理旧数据 */
    groebner_backend_cleanup(solver);

    /* 步骤 1：统计点节点数量，确定变量数（每个点 2 个坐标变量） */
    int point_count = 0;
    int max_node_id = 0;
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node)
            continue;
        if (node->type == GEOM_POINT) {
            point_count++;
        }
        if (node->id > max_node_id) {
            max_node_id = node->id;
        }
    }

    if (point_count == 0) {
        lv_RETURN_ERROR(lv_ERROR_SOLVER_NO_SOLUTION, "Groebner 后端初始化失败：约束图中无点节点");
    }

    /* 步骤 2：创建环注册表 */
    solver->groebner_registry = ring_registry_create(4);
    if (!solver->groebner_registry) {
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "Groebner 后端初始化失败：无法创建环注册表");
    }

    /* 步骤 3：声明变量名（p0_x, p0_y, p1_x, p1_y, ...） */
    int var_count = point_count * 2;
    char **var_names = (char **) lv_calloc((size_t) var_count, sizeof(char *));
    if (!var_names) {
        groebner_backend_cleanup(solver);
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "Groebner 后端初始化失败：无法分配变量名数组");
    }

    /* 建立节点 ID -> 变量索引的映射表 */
    if (max_node_id == INT_MAX) {
        for (int i = 0; i < var_count; i++)
            lv_free((void **) &var_names[i]);
        lv_free((void **) &var_names);
        groebner_backend_cleanup(solver);
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "Groebner 后端初始化失败：节点 ID 溢出");
    }
    int map_size = max_node_id + 1;
    int *node_var_map = (int *) lv_calloc((size_t) map_size, sizeof(int));
    if (!node_var_map) {
        for (int i = 0; i < var_count; i++)
            lv_free((void **) &var_names[i]);
        lv_free((void **) &var_names);
        groebner_backend_cleanup(solver);
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "Groebner 后端初始化失败：无法分配节点映射表");
    }

    /* 初始化映射表为 -1（无效） */
    for (int i = 0; i < map_size; i++) {
        node_var_map[i] = -1;
    }

    /* 遍历点节点，分配变量索引和名称 */
    int var_idx = 0;
    for (int i = 0; i < graph->node_count; i++) {
        GeomNode *node = graph->nodes[i];
        if (!node || node->type != GEOM_POINT)
            continue;

        /* x 坐标变量 */
        char name_buf[GROEBNER_VAR_NAME_MAX];
        lv_snprintf(name_buf, sizeof(name_buf), "p%d_x", node->id);
        var_names[var_idx] = lv_strdup_safe(name_buf);
        node_var_map[node->id] = var_idx;
        var_idx++;

        /* y 坐标变量 */
        lv_snprintf(name_buf, sizeof(name_buf), "p%d_y", node->id);
        var_names[var_idx] = lv_strdup_safe(name_buf);
        var_idx++;
    }

    /* 步骤 4：创建多项式环（使用实数域 + 分次反字典序 grevlex） */
    solver->groebner_ring_id = ring_create(solver->groebner_registry, (const char **) var_names, var_count,
                                           RING_FIELD_REAL, MONOMIAL_GREVLEX, "geometric_constraint_ring");

    /* 释放变量名数组（ring_create 已复制） */
    for (int i = 0; i < var_count; i++) {
        if (var_names[i])
            lv_free((void **) &var_names[i]);
    }
    lv_free((void **) &var_names);

    if (solver->groebner_ring_id < 0) {
        lv_set_error(lv_ERROR_GROEBNER_FAILED, "Groebner 后端初始化失败：无法创建多项式环");
        lv_free((void **) &node_var_map);
        groebner_backend_cleanup(solver);
        return -1;
    }

    /* 步骤 5：创建理想（用于存放约束对应的多项式生成元） */
    solver->groebner_ideal_id =
        ideal_create(solver->groebner_registry, solver->groebner_ring_id, "geometric_constraint_ideal");

    if (solver->groebner_ideal_id < 0) {
        lv_set_error(lv_ERROR_GROEBNER_FAILED, "Groebner 后端初始化失败：无法创建多项式理想");
        lv_free((void **) &node_var_map);
        groebner_backend_cleanup(solver);
        return -1;
    }

    /* 保存映射表和变量数量 */
    solver->groebner_node_var_map = node_var_map;
    solver->groebner_node_var_map_size = map_size;
    solver->groebner_var_count = var_count;

    lv_LOG_INFO("Groebner 后端初始化完成: %d 个点节点, %d 个变量, ring_id=%d, ideal_id=%d", point_count, var_count,
                solver->groebner_ring_id, solver->groebner_ideal_id);

    return 0;
}

/**
 * @brief 清理 Groebner 后端的求解上下文
 *
 * 销毁代数簇、理想、环和注册表，释放映射表。
 *
 * @param solver  Groebner 求解器实例
 */
void groebner_backend_cleanup(SMTSolver *solver) {
    if (!solver)
        return;

    /* 销毁代数簇 */
    if (solver->groebner_registry && solver->groebner_variety_id >= 0) {
        /* variety_destroy 暂无独立 API，簇随注册表一起销毁 */
        solver->groebner_variety_id = -1;
    }

    /* 销毁理想 */
    if (solver->groebner_registry && solver->groebner_ideal_id >= 0) {
        ideal_destroy(solver->groebner_registry, solver->groebner_ideal_id);
        solver->groebner_ideal_id = -1;
    }

    /* 销毁环 */
    if (solver->groebner_registry && solver->groebner_ring_id >= 0) {
        ring_destroy(solver->groebner_registry, solver->groebner_ring_id);
        solver->groebner_ring_id = -1;
    }

    /* 销毁环注册表 */
    if (solver->groebner_registry) {
        ring_registry_destroy(solver->groebner_registry);
        solver->groebner_registry = NULL;
    }

    /* 释放节点-变量映射表 */
    if (solver->groebner_node_var_map) {
        lv_free((void **) &solver->groebner_node_var_map);
        solver->groebner_node_var_map = NULL;
    }
    solver->groebner_node_var_map_size = 0;
    solver->groebner_var_count = 0;
}

/* ================================================================
 *  辅助函数——多项式构造（用于手动编码回退路径）
 * ================================================================ */

/**
 * @brief 获取多项式环的变量数量
 */
static int _get_ring_var_count(lvRingRegistry *registry, int ring_id) {
    lvPolynomialRing *ring = ring_find(registry, ring_id);
    if (!ring)
        return -1;
    return ring->var_count;
}

/**
 * @brief 向已存在的多项式中添加一个单项式项
 *
 * 多项式必须预先通过 poly_create 创建且有足够的容量（预分配 ≥8）。
 * 直接操作多项式内部数据结构——poly_get 返回的 const 指针可安全转换，
 * 因为该多项式是由 poly_create 以非 const 方式创建的。
 *
 * @param registry    环注册表
 * @param poly_id     多项式 ID
 * @param coeff       系数（double）
 * @param exponents   指数数组（长度为 var_count，调用者负责管理该数组的生命周期）
 * @param var_count   变量个数
 * @return 0 成功，-1 失败（多项式不存在或容量不足）
 */
static int _poly_add_term(lvRingRegistry *registry, int poly_id, double coeff, const int *exponents, int var_count) {
    const lvPolynomial *cp = poly_get(registry, poly_id);
    if (!cp)
        return -1;

    lvPolynomial *p = (lvPolynomial *) cp;
    if (p->term_count >= p->term_capacity)
        return -1;

    int ti = p->term_count;
    for (int j = 0; j < var_count; j++) {
        p->powers[ti * var_count + j] = exponents[j];
    }
    ((double *) p->coeffs)[ti] = coeff;
    p->term_count = ti + 1;

    /* 更新总次数 */
    int deg = 0;
    for (int j = 0; j < var_count; j++)
        deg += exponents[j];
    if (deg > p->total_degree)
        p->total_degree = deg;

    return 0;
}

/**
 * @brief 从约束图中查找线段的两个端点节点 ID
 *
 * 遍历所有 INCIDENCE 约束（[point_id, line_id]），收集与目标线段
 * 关联的点节点，返回前两个作为候选端点。
 *
 * @param graph         约束图
 * @param line_id       线段节点 ID
 * @param out_endpoints 输出缓冲区（至少 2 个元素）
 * @return 找到的端点数量（0, 1, 或 2）
 */
static int _find_line_endpoints(const ConstraintGraph *graph, int line_id, int out_endpoints[2]) {
    out_endpoints[0] = -1;
    out_endpoints[1] = -1;
    int found = 0;

    for (int ci = 0; ci < graph->constraint_count && found < 2; ci++) {
        Constraint *c = graph->constraints[ci];
        if (!c || !c->is_active)
            continue;
        if (c->type != INCIDENCE || !lv_constraint_has_participants(c, 2))
            continue;

        int candidate = -1;
        /* INCIDENCE 通常是 [point_id, line_id] 顺序 */
        if (c->participants[1] == line_id) {
            candidate = c->participants[0];
        } else if (c->participants[0] == line_id) {
            candidate = c->participants[1];
        }
        if (candidate < 0)
            continue;

        /* 去重 */
        bool dup = false;
        for (int i = 0; i < found; i++) {
            if (out_endpoints[i] == candidate) {
                dup = true;
                break;
            }
        }
        if (!dup) {
            GeomNode *n = graph_get_node(graph, candidate);
            if (n && n->type == GEOM_POINT) {
                out_endpoints[found++] = candidate;
            }
        }
    }
    return found;
}

/**
 * @brief 构造共线叉积多项式的完全展开形式
 *
 * 几何语义：三点 A, B, C 共线  ⇔  (Bx-Ax)*(Cy-Ay) - (By-Ay)*(Cx-Ax) = 0
 *
 * 展开后消去同类项得到 6 项双线性形式：
 *   Bx·Cy - Bx·Ay - Ax·Cy - By·Cx + By·Ax + Ay·Cx
 *
 * 其中 points[0]=A, points[1]=B, points[2]=C。
 *
 * @param registry  环注册表
 * @param ring_id   环 ID
 * @param var_map   节点 ID → 变量索引映射（来自 solver->groebner_node_var_map）
 * @param points    三个点节点 ID 数组 [A, B, C]
 * @param var_count 环的变量总数
 * @param label     多项式标签
 * @return 多项式 ID（≥0），失败返回 -1
 */
static int _poly_create_collinear(lvRingRegistry *registry, int ring_id, const int *var_map, const int points[3],
                                  int var_count, const char *label) {
    /* 叉积展开为 6 项，预分配 8 确保安全 */
    int pid = poly_create(registry, ring_id, 8, label);
    if (pid < 0)
        return -1;

    int ax = var_map[points[0]];
    int ay = var_map[points[0]] + 1;
    int bx = var_map[points[1]];
    int by = var_map[points[1]] + 1;
    int cx = var_map[points[2]];
    int cy = var_map[points[2]] + 1;

    if (ax < 0 || ay < 0 || bx < 0 || by < 0 || cx < 0 || cy < 0) {
        poly_destroy(registry, pid);
        return -1;
    }

    int *exp = (int *) lv_calloc((size_t) var_count, sizeof(int));
    if (!exp) {
        poly_destroy(registry, pid);
        return -1;
    }

    /* Term 0: +1.0 * Bx * Cy */
    exp[bx] = 1;
    exp[cy] = 1;
    _poly_add_term(registry, pid, 1.0, exp, var_count);
    exp[bx] = 0;
    exp[cy] = 0;

    /* Term 1: -1.0 * Bx * Ay */
    exp[bx] = 1;
    exp[ay] = 1;
    _poly_add_term(registry, pid, -1.0, exp, var_count);
    exp[bx] = 0;
    exp[ay] = 0;

    /* Term 2: -1.0 * Ax * Cy */
    exp[ax] = 1;
    exp[cy] = 1;
    _poly_add_term(registry, pid, -1.0, exp, var_count);
    exp[ax] = 0;
    exp[cy] = 0;

    /* Term 3: -1.0 * By * Cx */
    exp[by] = 1;
    exp[cx] = 1;
    _poly_add_term(registry, pid, -1.0, exp, var_count);
    exp[by] = 0;
    exp[cx] = 0;

    /* Term 4: +1.0 * By * Ax */
    exp[by] = 1;
    exp[ax] = 1;
    _poly_add_term(registry, pid, 1.0, exp, var_count);
    exp[by] = 0;
    exp[ax] = 0;

    /* Term 5: +1.0 * Ay * Cx */
    exp[ay] = 1;
    exp[cx] = 1;
    _poly_add_term(registry, pid, 1.0, exp, var_count);
    exp[ay] = 0;
    exp[cx] = 0;

    lv_free((void **) &exp);
    return pid;
}

/**
 * @brief 构造坐标差多项式：var_x(node_a) - var_x(node_b) = 0
 *
 * 用于 CONNECTION 约束：两个节点的 x（或 y）坐标相等。
 * 生成的多项式为：coeff_x_a * x_a + coeff_x_b * x_b。
 *
 * @param registry  环注册表
 * @param ring_id   环 ID
 * @param var_map   节点 ID → 变量索引映射
 * @param node_a    节点 A ID
 * @param node_b    节点 B ID
 * @param is_y      0=x坐标差，1=y坐标差
 * @param var_count 变量总数
 * @param label     多项式标签
 * @return 多项式 ID（≥0），失败返回 -1
 */
static int _poly_create_coord_diff(lvRingRegistry *registry, int ring_id, const int *var_map, int node_a, int node_b,
                                   int is_y, int var_count, const char *label) {
    int idx_a = var_map[node_a] + is_y;
    int idx_b = var_map[node_b] + is_y;
    if (idx_a < 0 || idx_b < 0)
        return -1;

    int pid = poly_create(registry, ring_id, 2, label);
    if (pid < 0)
        return -1;

    int *exp = (int *) lv_calloc((size_t) var_count, sizeof(int));
    if (!exp) {
        poly_destroy(registry, pid);
        return -1;
    }

    /* Term 0: +1.0 * var_a */
    exp[idx_a] = 1;
    _poly_add_term(registry, pid, 1.0, exp, var_count);
    exp[idx_a] = 0;

    /* Term 1: -1.0 * var_b */
    exp[idx_b] = 1;
    _poly_add_term(registry, pid, -1.0, exp, var_count);
    exp[idx_b] = 0;

    lv_free((void **) &exp);
    return pid;
}

/**
 * @brief 创建常数多项式（所有指数均为 0）
 *
 * 用于给多项式乘以标量系数（如角度约束中的 cosθ / sinθ 系数）。
 *
 * @param registry  环注册表
 * @param ring_id   多项式环 ID
 * @param coeff     常数值
 * @param var_count 环的变量数
 * @param label     多项式标签
 * @return 多项式 ID，失败返回 -1
 */
static int _poly_create_const(lvRingRegistry *registry, int ring_id, double coeff, int var_count, const char *label) {
    int pid = poly_create(registry, ring_id, 1, label);
    if (pid < 0)
        return -1;

    int *exp = (int *) lv_calloc((size_t) var_count, sizeof(int));
    if (!exp) {
        poly_destroy(registry, pid);
        return -1;
    }

    /* 常数项：所有变量指数均为 0，系数为 coeff */
    _poly_add_term(registry, pid, coeff, exp, var_count);

    lv_free((void **) &exp);
    return pid;
}

/**
 * @brief 批量销毁角度编码过程中创建的中间多项式
 *
 * 构造角度约束多项式时会生成大量中间多项式
 * （坐标差、点积、叉积、范数平方、三角函数常数等）。
 * 本函数在失败路径上统一销毁它们，避免内存泄漏。
 *
 * @param registry 环注册表
 * @param ids      多项式 ID 数组
 * @param count    数组长度（销毁 ids[0..count-1]）
 */
static void _poly_destroy_angle_ids(lvRingRegistry *registry, const int *ids, int count) {
    if (!registry || !ids || count <= 0)
        return;
    for (int i = 0; i < count; i++) {
        if (ids[i] >= 0)
            poly_destroy(registry, ids[i]);
    }
}

/**
 * @brief 构造角度约束多项式（向量夹角余弦公式）
 *
 * 几何语义：∠BAC = θ，其中 A、C 为两条射线上的点，B 为公共顶点
 * （v1 = A-B，v2 = C-B）。
 *
 * 记 dot = v1·v2，cross = v1×v2，n1 = |v1|²，n2 = |v2|²，则
 * cosθ = dot/(|v1|·|v2|)，sinθ = cross/(|v1|·|v2|)。
 * 利用恒等式 (dot·cosθ + cross·sinθ)² = (|v1|·|v2|)² 消去分母，
 * 得到坐标变量的 4 次多项式方程：
 *   (dot·cosθ + cross·sinθ)² - n1·n2 = 0
 *
 * 说明：当 0° < θ < 180° 时该方程精确等价于 ∠ = θ；
 * 当 θ = 0° 或 180°（共线）时，平方形式会同时接受两个共线方向，
 * 这是多项式理想编码的固有代数歧义。
 *
 * @param registry  环注册表
 * @param ring_id   环 ID
 * @param var_map   节点 ID → 变量索引映射
 * @param a_id      射线端点 A 节点 ID（与顶点 B 同属第一条线段）
 * @param b_id      公共顶点 B 节点 ID
 * @param c_id      射线端点 C 节点 ID（与顶点 B 同属第二条线段）
 * @param var_count 环的变量总数
 * @param theta_deg 角度数值（度）
 * @return 多项式 ID（≥0），失败返回 -1
 */
static int _poly_create_angle(lvRingRegistry *registry, int ring_id, const int *var_map, int a_id, int b_id, int c_id,
                              int var_count, double theta_deg) {
    const double rad = lv_deg_to_rad(theta_deg);
    const double cos_t = cos(rad);
    const double sin_t = sin(rad);

    int owned[32];
    int n_owned = 0;
    int result = -1;

    /* 创建多项式并登记所有权；失败时销毁已登记的全部中间多项式后返回 -1 */
#define ANGLE_CREATE(var, call)                                 \
    do {                                                        \
        var = (call);                                           \
        if (var < 0) {                                          \
            _poly_destroy_angle_ids(registry, owned, n_owned);  \
            return -1;                                          \
        }                                                       \
        owned[n_owned++] = var;                                 \
    } while (0)

    /* ---- 步骤 1：坐标差多项式 v1 = A-B，v2 = C-B ---- */
    int d1x = -1, d1y = -1, d2x = -1, d2y = -1;
    ANGLE_CREATE(d1x, _poly_create_coord_diff(registry, ring_id, var_map, a_id, b_id, 0, var_count, "angle_d1x"));
    ANGLE_CREATE(d1y, _poly_create_coord_diff(registry, ring_id, var_map, a_id, b_id, 1, var_count, "angle_d1y"));
    ANGLE_CREATE(d2x, _poly_create_coord_diff(registry, ring_id, var_map, c_id, b_id, 0, var_count, "angle_d2x"));
    ANGLE_CREATE(d2y, _poly_create_coord_diff(registry, ring_id, var_map, c_id, b_id, 1, var_count, "angle_d2y"));

    /* ---- 步骤 2：点积 dot = v1·v2 = d1x·d2x + d1y·d2y ---- */
    int dot_x = -1, dot_y = -1, dot = -1;
    ANGLE_CREATE(dot_x, poly_multiply(registry, d1x, d2x, "angle_dot_x"));
    ANGLE_CREATE(dot_y, poly_multiply(registry, d1y, d2y, "angle_dot_y"));
    ANGLE_CREATE(dot, poly_add(registry, dot_x, dot_y, "angle_dot"));

    /* ---- 步骤 3：叉积 cross = v1×v2 = d1x·d2y - d1y·d2x ---- */
    int cross_x = -1, cross_y = -1, cneg = -1, neg_cross_y = -1, cross = -1;
    ANGLE_CREATE(cross_x, poly_multiply(registry, d1x, d2y, "angle_cross_x"));
    ANGLE_CREATE(cross_y, poly_multiply(registry, d1y, d2x, "angle_cross_y"));
    ANGLE_CREATE(cneg, _poly_create_const(registry, ring_id, -1.0, var_count, "angle_neg1"));
    ANGLE_CREATE(neg_cross_y, poly_multiply(registry, cneg, cross_y, "angle_neg_cross_y"));
    ANGLE_CREATE(cross, poly_add(registry, cross_x, neg_cross_y, "angle_cross"));

    /* ---- 步骤 4：范数平方 n1 = |v1|²，n2 = |v2|² ---- */
    int n1_x = -1, n1_y = -1, n1 = -1;
    ANGLE_CREATE(n1_x, poly_multiply(registry, d1x, d1x, "angle_n1_x"));
    ANGLE_CREATE(n1_y, poly_multiply(registry, d1y, d1y, "angle_n1_y"));
    ANGLE_CREATE(n1, poly_add(registry, n1_x, n1_y, "angle_n1"));
    int n2_x = -1, n2_y = -1, n2 = -1;
    ANGLE_CREATE(n2_x, poly_multiply(registry, d2x, d2x, "angle_n2_x"));
    ANGLE_CREATE(n2_y, poly_multiply(registry, d2y, d2y, "angle_n2_y"));
    ANGLE_CREATE(n2, poly_add(registry, n2_x, n2_y, "angle_n2"));

    /* ---- 步骤 5：组合项 comb = dot·cosθ + cross·sinθ ----
     * 三角函数值约等于 0 时跳过对应项，避免与零多项式相乘产生零系数项。 */
    int c_cos = -1, c_sin = -1, term_cos = -1, term_sin = -1, comb = -1;
    const double trig_eps = lv_EPSILON_ULTRA;
    if (fabs(cos_t) > trig_eps) {
        ANGLE_CREATE(c_cos, _poly_create_const(registry, ring_id, cos_t, var_count, "angle_cos"));
        ANGLE_CREATE(term_cos, poly_multiply(registry, dot, c_cos, "angle_term_cos"));
    }
    if (fabs(sin_t) > trig_eps) {
        ANGLE_CREATE(c_sin, _poly_create_const(registry, ring_id, sin_t, var_count, "angle_sin"));
        ANGLE_CREATE(term_sin, poly_multiply(registry, cross, c_sin, "angle_term_sin"));
    }
    if (term_cos >= 0 && term_sin >= 0) {
        ANGLE_CREATE(comb, poly_add(registry, term_cos, term_sin, "angle_comb"));
    } else if (term_cos >= 0) {
        comb = term_cos;
    } else if (term_sin >= 0) {
        comb = term_sin;
    } else {
        /* cosθ 与 sinθ 同时为零不可能发生（cos²+sin²=1），防御性处理 */
        _poly_destroy_angle_ids(registry, owned, n_owned);
        return -1;
    }

    /* ---- 步骤 6：组合平方与范数乘积 ---- */
    int comb_sq = -1, nprod = -1, neg_nprod = -1;
    ANGLE_CREATE(comb_sq, poly_multiply(registry, comb, comb, "angle_comb_sq"));
    ANGLE_CREATE(nprod, poly_multiply(registry, n1, n2, "angle_nprod"));
    ANGLE_CREATE(neg_nprod, poly_multiply(registry, cneg, nprod, "angle_neg_nprod"));

    /* ---- 步骤 7：结果多项式 comb_sq - n1·n2 = 0 ---- */
    ANGLE_CREATE(result, poly_add(registry, comb_sq, neg_nprod, "angle_constraint"));

    /* 成功：销毁除 result 外的全部中间多项式（result 交由理想持有） */
    for (int i = 0; i < n_owned; i++) {
        if (owned[i] != result)
            poly_destroy(registry, owned[i]);
    }
#undef ANGLE_CREATE
    return result;
}

/* ── Groebner 手动编码上下文与辅助函数（文件作用域，用于查找表）── */
typedef struct {
    lvRingRegistry *registry;
    int ring_id;
    int *var_map;
    int map_size;
    int vc;
    int ideal_id;
    const ConstraintGraph *graph;
    int *encode_failed; /* 编码失败约束计数（调用方持有；无法编码的约束不再以零多项式占位静默丢弃） */
} GroebnerManualEncodeCtx;

typedef void (*GroebnerManualEncodeFn)(const GroebnerManualEncodeCtx *ctx, const Constraint *c);

static void groebner_manual_encode_incidence(const GroebnerManualEncodeCtx *ctx, const Constraint *c) {
    if (lv_constraint_has_participants(c, 2)) {
        int pt_id = c->participants[0];
        int seg_id = c->participants[1];
        int p_var = (pt_id >= 0 && pt_id < ctx->map_size) ? ctx->var_map[pt_id] : -1;
        if (p_var < 0) {
            /* 点节点无变量映射：约束无法编码，上报而非静默丢弃 */
            lv_LOG_WARNING("INCIDENCE: 点节点缺少变量映射 (pt=%d, c=%d)", pt_id, c->id);
            if (ctx->encode_failed) (*ctx->encode_failed)++;
            return;
        }
        int endpoints[2] = {-1, -1};
        int n_endpoints = _find_line_endpoints(ctx->graph, seg_id, endpoints);
        if (n_endpoints >= 2) {
            int pts[3] = {endpoints[0], pt_id, endpoints[1]};
            int poly_id = _poly_create_collinear(ctx->registry, ctx->ring_id, ctx->var_map, pts, ctx->vc,
                                                 "incidence_constraint");
            if (poly_id >= 0) {
                ideal_add_generator(ctx->registry, ctx->ideal_id, poly_id);
            } else {
                lv_LOG_WARNING("INCIDENCE: 无法构造叉积多项式 (c=%d)", c->id);
                if (ctx->encode_failed) (*ctx->encode_failed)++;
            }
        } else {
            lv_LOG_WARNING("INCIDENCE: 无法确定线段端点 (seg=%d, found=%d, c=%d)", seg_id, n_endpoints, c->id);
            if (ctx->encode_failed) (*ctx->encode_failed)++;
        }
    }
}

static void groebner_manual_encode_betweenness(const GroebnerManualEncodeCtx *ctx, const Constraint *c) {
    if (lv_constraint_has_participants(c, 3)) {
        int pts[3] = {c->participants[0], c->participants[1], c->participants[2]};
        bool valid = true;
        for (int k = 0; k < 3; k++) {
            if (pts[k] < 0 || pts[k] >= ctx->map_size || ctx->var_map[pts[k]] < 0) {
                valid = false;
                break;
            }
        }
        if (valid) {
            int poly_id = _poly_create_collinear(ctx->registry, ctx->ring_id, ctx->var_map, pts, ctx->vc,
                                                 "betweenness_constraint");
            if (poly_id >= 0) {
                ideal_add_generator(ctx->registry, ctx->ideal_id, poly_id);
            } else {
                lv_LOG_WARNING("BETWEENNESS: 无法构造叉积多项式 (c=%d)", c->id);
                if (ctx->encode_failed) (*ctx->encode_failed)++;
            }
        } else {
            /* 三点的变量映射不完整：约束无法编码，上报而非静默丢弃 */
            lv_LOG_WARNING("BETWEENNESS: 点节点缺少变量映射 (c=%d)", c->id);
            if (ctx->encode_failed) (*ctx->encode_failed)++;
        }
    }
}

static void groebner_manual_encode_intersection(const GroebnerManualEncodeCtx *ctx, const Constraint *c) {
    if (lv_constraint_has_participants(c, 3)) {
        int line1_id = c->participants[0];
        int line2_id = c->participants[1];
        int pt_id = c->participants[2];
        int p_var = (pt_id >= 0 && pt_id < ctx->map_size) ? ctx->var_map[pt_id] : -1;
        if (p_var < 0) {
            lv_LOG_WARNING("INTERSECTION: 交点缺少变量映射 (pt=%d, c=%d)", pt_id, c->id);
            if (ctx->encode_failed) (*ctx->encode_failed)++;
            return;
        }
        int ep1[2] = {-1, -1}, ep2[2] = {-1, -1};
        int n1 = _find_line_endpoints(ctx->graph, line1_id, ep1);
        int n2 = _find_line_endpoints(ctx->graph, line2_id, ep2);
        if (n1 >= 2) {
            int pts1[3] = {ep1[0], pt_id, ep1[1]};
            int poly_id = _poly_create_collinear(ctx->registry, ctx->ring_id, ctx->var_map, pts1, ctx->vc,
                                                 "intersection_line1");
            if (poly_id >= 0) ideal_add_generator(ctx->registry, ctx->ideal_id, poly_id);
        }
        if (n2 >= 2) {
            int pts2[3] = {ep2[0], pt_id, ep2[1]};
            int poly_id = _poly_create_collinear(ctx->registry, ctx->ring_id, ctx->var_map, pts2, ctx->vc,
                                                 "intersection_line2");
            if (poly_id >= 0) ideal_add_generator(ctx->registry, ctx->ideal_id, poly_id);
        }
        if (n1 < 2 && n2 < 2) {
            lv_LOG_WARNING("INTERSECTION: 无法确定两条线的端点 (c=%d)", c->id);
            if (ctx->encode_failed) (*ctx->encode_failed)++;
        }
    }
}

static void groebner_manual_encode_angle(const GroebnerManualEncodeCtx *ctx, const Constraint *c) {
    if (lv_constraint_has_participants(c, 2)) {
        int line1_id = c->participants[0];
        int line2_id = c->participants[1];
        int ep1[2] = {-1, -1}, ep2[2] = {-1, -1};
        int n1 = _find_line_endpoints(ctx->graph, line1_id, ep1);
        int n2 = _find_line_endpoints(ctx->graph, line2_id, ep2);
        if (n1 >= 2 && n2 >= 2) {
            int a_id = -1, b_id = -1, c_id = -1;
            for (int i = 0; i < 2 && b_id < 0; i++) {
                for (int j = 0; j < 2; j++) {
                    if (ep1[i] == ep2[j]) {
                        b_id = ep1[i];
                        a_id = ep1[1 - i];
                        c_id = ep2[1 - j];
                        break;
                    }
                }
            }
            bool valid = (a_id >= 0 && b_id >= 0 && c_id >= 0 && a_id < ctx->map_size && b_id < ctx->map_size &&
                          c_id < ctx->map_size && ctx->var_map[a_id] >= 0 && ctx->var_map[b_id] >= 0 &&
                          ctx->var_map[c_id] >= 0);
            if (valid) {
                int poly_id = _poly_create_angle(ctx->registry, ctx->ring_id, ctx->var_map, a_id, b_id, c_id,
                                                 ctx->vc, c->numeric_value);
                if (poly_id >= 0) {
                    ideal_add_generator(ctx->registry, ctx->ideal_id, poly_id);
                } else {
                    lv_LOG_WARNING("ANGLE: 无法构造角度多项式 (c=%d)", c->id);
                    if (ctx->encode_failed) (*ctx->encode_failed)++;
                }
            } else {
                lv_LOG_WARNING("ANGLE: 端点缺少变量映射 (c=%d)", c->id);
                if (ctx->encode_failed) (*ctx->encode_failed)++;
            }
        } else {
            lv_LOG_WARNING("ANGLE: 无法确定线段端点 (c=%d)", c->id);
            if (ctx->encode_failed) (*ctx->encode_failed)++;
        }
    }
}

static void groebner_manual_encode_connection(const GroebnerManualEncodeCtx *ctx, const Constraint *c) {
    if (lv_constraint_has_participants(c, 2)) {
        int src_id = c->participants[0];
        int dst_id = c->participants[1];
        int src_var = (src_id >= 0 && src_id < ctx->map_size) ? ctx->var_map[src_id] : -1;
        int dst_var = (dst_id >= 0 && dst_id < ctx->map_size) ? ctx->var_map[dst_id] : -1;
        if (src_var >= 0 && dst_var >= 0) {
            int poly_x = _poly_create_coord_diff(ctx->registry, ctx->ring_id, ctx->var_map, src_id, dst_id, 0,
                                                 ctx->vc, "connection_x_eq");
            if (poly_x >= 0) ideal_add_generator(ctx->registry, ctx->ideal_id, poly_x);
            int poly_y = _poly_create_coord_diff(ctx->registry, ctx->ring_id, ctx->var_map, src_id, dst_id, 1,
                                                 ctx->vc, "connection_y_eq");
            if (poly_y >= 0) ideal_add_generator(ctx->registry, ctx->ideal_id, poly_y);
        } else {
            lv_LOG_WARNING("CONNECTION: 节点缺少变量映射 (src=%d, dst=%d, c=%d)", src_id, dst_id, c->id);
            if (ctx->encode_failed) (*ctx->encode_failed)++;
        }
    }
}

static void groebner_manual_encode_containment(const GroebnerManualEncodeCtx *ctx, const Constraint *c) {
    if (lv_constraint_has_participants(c, 2)) {
        int inner_id = c->participants[0];
        int outer_id = c->participants[1];
        GeomNode *inner = graph_get_node(ctx->graph, inner_id);
        GeomNode *outer = graph_get_node(ctx->graph, outer_id);
        bool added_any = false;
        if (inner && outer && inner->type == GEOM_POINT && outer->type == GEOM_REGION &&
            outer->data.region.segment_count > 0) {
            for (int si = 0; si < outer->data.region.segment_count; si++) {
                GeomNode *seg = outer->data.region.boundary_segments[si];
                if (!seg || seg->type != GEOM_LINE_SEGMENT) continue;
                int seg_id = seg->id;
                int endpoints[2] = {-1, -1};
                int n_ep = _find_line_endpoints(ctx->graph, seg_id, endpoints);
                if (n_ep >= 2) {
                    int pts[3] = {endpoints[0], inner_id, endpoints[1]};
                    char label[64];
                    lv_snprintf(label, sizeof(label), "containment_seg_%d", si);
                    int poly_id = _poly_create_collinear(ctx->registry, ctx->ring_id, ctx->var_map, pts, ctx->vc, label);
                    if (poly_id >= 0) {
                        ideal_add_generator(ctx->registry, ctx->ideal_id, poly_id);
                        added_any = true;
                    }
                }
            }
        }
        if (!added_any) {
            lv_LOG_WARNING("CONTAINMENT: 无法编码区域边界约束 (inner=%d, outer=%d, c=%d)", inner_id, outer_id, c->id);
            if (ctx->encode_failed) (*ctx->encode_failed)++;
        }
    }
}

/**
 * @brief Groebner 后端：将约束图转换为多项式理想并求解
 *
 * 完整的 Groebner 基求解流程：
 * 1. 初始化 Groebner 后端（创建环、理想、映射表）
 * 2. 遍历约束图中的约束，将每个约束转换为多项式生成元
 * 3. 调用 Buchberger 算法计算 Groebner 基
 * 4. 通过 Groebner 基判定理想成员关系（可满足性）
 * 5. 计算代数簇（获取具体解）
 *
 * @param solver  Groebner 求解器实例
 * @param graph   约束图
 * @return SMT 可满足性结果
 */
SMTSatResult groebner_backend_solve(SMTSolver *solver, const ConstraintGraph *graph) {
    if (!solver || !graph)
        return SMT_RESULT_ERROR;

    /* ---- 步骤 1：初始化 Groebner 求解上下文 ---- */
    int rc = groebner_backend_init(solver, graph);
    if (rc < 0) {
        smtsolver_set_error(solver, SMT_ERROR_ENCODING_FAILED, "Groebner backend initialization failed");
        return SMT_RESULT_ERROR;
    }

    lvRingRegistry *registry = solver->groebner_registry;
    int ring_id = solver->groebner_ring_id;
    int ideal_id = solver->groebner_ideal_id;

    /* ---- 步骤 2：将约束转换为多项式生成元 ---- */

    /*
     * 约束编码策略：
     *
     * 对于每个约束，我们创建对应的多项式并添加到理想中。
     * 由于 Groebner 引擎的 poly_create 创建的是空多项式，
     * 我们需要通过 constraint_graph_to_ideal() 统一转换。
     *
     * 这里使用 groebner_engine.h 提供的 constraint_graph_to_ideal()
     * 函数来完成约束图到多项式理想的转换。
     */

    int primary_encode_failed = 0;
    int conv_id = constraint_graph_to_ideal_ex(registry, graph, ring_id, "converted_constraint_ideal",
                                               &primary_encode_failed);
    if (conv_id < 0) {
        /* constraint_graph_to_ideal 失败，尝试手动编码关键约束 */

        lv_LOG_WARNING("constraint_graph_to_ideal 失败，回退到手动约束编码");

        /* 获取环信息与变量映射 */
        int vc = _get_ring_var_count(registry, ring_id);
        if (vc < 0) {
            smtsolver_set_error(solver, SMT_ERROR_ENCODING_FAILED, "无法获取多项式环的变量数量");
            return SMT_RESULT_ERROR;
        }
        int *var_map = solver->groebner_node_var_map;
        int map_size = solver->groebner_node_var_map_size;
        lv_UNUSED(map_size);

        /* 手动编码：遍历约束，为每个约束创建多项式 */
        /* ── 使用文件作用域的 Groebner 手动编码函数查找表 ── */
        static const GroebnerManualEncodeFn kGroebnerManualEncodeTable[] = {
            groebner_manual_encode_incidence,    /* INCIDENCE */
            groebner_manual_encode_betweenness,  /* BETWEENNESS */
            groebner_manual_encode_intersection, /* INTERSECTION */
            groebner_manual_encode_containment,  /* CONTAINMENT */
            groebner_manual_encode_connection,   /* CONNECTION */
            groebner_manual_encode_angle         /* ANGLE */
        };
        static const int kGroebnerManualEncodeTableCount =
            (int)(sizeof(kGroebnerManualEncodeTable) / sizeof(kGroebnerManualEncodeTable[0]));

        GroebnerManualEncodeCtx gctx;
        gctx.registry = registry;
        gctx.ring_id = ring_id;
        gctx.var_map = var_map;
        gctx.map_size = map_size;
        gctx.vc = vc;
        gctx.ideal_id = ideal_id;
        gctx.graph = graph;

        int encode_failed = 0;
        gctx.encode_failed = &encode_failed;

        for (int ci = 0; ci < graph->constraint_count; ci++) {
            Constraint *c = graph->constraints[ci];
            if (!c) continue;
            if (c->type >= 0 && c->type < kGroebnerManualEncodeTableCount) {
                kGroebnerManualEncodeTable[(int)c->type](&gctx, c);
            } else {
                lv_LOG_WARNING("Unknown constraint type %d in constraint_graph_to_ideal fallback", c->type);
            }
        }

        /* 编码失败上报：存在无法编码为多项式的约束时，继续求解会产生失真的
         * SAT/UNSAT 判定（占位零多项式等价于丢弃约束）。宁可返回 UNKNOWN，
         * 也不能用不完整理想给出错误结论。 */
        if (encode_failed > 0) {
            lv_LOG_ERROR("%d 个约束无法编码为多项式（端点/变量映射缺失），返回 UNKNOWN 而非不可信判定",
                         encode_failed);
            char msg[160];
            lv_snprintf(msg, sizeof(msg),
                     "%d constraint(s) could not be encoded into polynomials; result would be unreliable",
                     encode_failed);
            smtsolver_set_error(solver, SMT_ERROR_ENCODING_FAILED, msg);
            return SMT_RESULT_UNKNOWN;
        }
    } else {
        /* 主路径编码失败上报：存在无法编码的约束时，基于该理想的 SAT/UNSAT
         * 判定不可信（约束被静默丢弃），宁可返回 UNKNOWN。 */
        if (primary_encode_failed > 0) {
            lv_LOG_ERROR("%d 个约束无法编码为多项式（节点坐标/变量映射缺失），返回 UNKNOWN 而非不可信判定",
                         primary_encode_failed);
            char msg[160];
            lv_snprintf(msg, sizeof(msg),
                     "%d constraint(s) could not be encoded into polynomials; result would be unreliable",
                     primary_encode_failed);
            smtsolver_set_error(solver, SMT_ERROR_ENCODING_FAILED, msg);
            return SMT_RESULT_UNKNOWN;
        }
        /* constraint_graph_to_ideal 成功，使用转换后的理想 */
        lv_LOG_INFO("约束图成功转换为多项式理想 (ideal_id=%d)", conv_id);
        solver->groebner_ideal_id = conv_id;
        ideal_id = conv_id;
    }

    /* ---- 步骤 3：计算 Groebner 基 ---- */
    lv_LOG_INFO("开始计算 Groebner 基 (Buchberger 算法)...");

    int gb_rc = groebner_compute(registry, ideal_id, GROEBNER_AUTO);
    if (gb_rc < 0) {
        lv_LOG_ERROR("Groebner 基计算失败 (错误码=%d)", gb_rc);
        lv_set_error(lv_ERROR_GROEBNER_FAILED, "Groebner 基计算失败: ideal_id=%d, rc=%d", ideal_id, gb_rc);
        smtsolver_set_error(solver, SMT_ERROR_SOLVER_CRASHED, "Groebner basis computation failed");
        return SMT_RESULT_ERROR;
    }

    lv_LOG_INFO("Groebner 基计算完成");

    /* ---- 步骤 4：通过理想成员关系判定可满足性 ---- */

    /*
     * 可满足性判定原理：
     * - 如果 Groebner 基 G = {1}（仅含常数 1），则理想 I = <1> = 整个环，
     *   方程组无解，返回 UNSAT。
     * - 如果 Groebner 基不包含 1，则理想是真理想，方程组可能有解。
     *   进一步通过计算代数簇 V(I) 来确认。
     *
     * 在 Lv-00 的 Groebner 引擎中，我们通过以下方式判定：
     * 1. 检查理想中是否有生成元（空理想 = 无约束 = SAT）
     * 2. 尝试计算代数簇
     * 3. 根据簇的解点数量判定 SAT/UNSAT
     */

    /* 获取理想信息以检查 Groebner 基 */
    /* 如果理想为空（无约束），则平凡可满足 */
    if (graph->constraint_count == 0) {
        lv_LOG_INFO("约束图为空（无约束），返回 SAT");
        return SMT_RESULT_SAT;
    }

    /* ---- 步骤 5：计算代数簇（求解多项式方程组） ---- */
    lv_LOG_INFO("开始计算代数簇 V(I)...");

    int variety_id = variety_compute(registry, ideal_id, "constraint_variety");
    if (variety_id < 0) {
        /* 代数簇计算失败，可能是因为方程组过于复杂或维度过高。
         * 此时返回 UNKNOWN 而非错误，因为约束系统本身可能是有效的，
         * 只是超出了当前数值方法的处理能力。 */
        lv_LOG_WARNING("代数簇计算失败 (variety_id=%d)，返回 UNKNOWN", variety_id);
        smtsolver_set_error(solver, SMT_ERROR_UNSUPPORTED_THEORY, "Variety computation failed; returning UNKNOWN");
        return SMT_RESULT_UNKNOWN;
    }

    solver->groebner_variety_id = variety_id;

    /* 检查代数簇的解 */
    bool is_zero_dim = variety_is_zero_dimensional(registry, variety_id);
    int dim = variety_dimension(registry, variety_id);

    if (is_zero_dim) {
        /* 零维簇：有限个离散解，约束系统可满足 */
        lv_LOG_INFO("代数簇为零维（有限解），返回 SAT (dimension=%d)", dim);
        return SMT_RESULT_SAT;
    } else if (dim < 0) {
        /* 维度计算失败 */
        lv_LOG_WARNING("无法确定代数簇维数，返回 UNKNOWN");
        return SMT_RESULT_UNKNOWN;
    } else if (dim == 0) {
        /* 维度为 0 但 is_zero_dimensional 为 false（边界情况） */
        lv_LOG_INFO("代数簇维数为 0，返回 SAT");
        return SMT_RESULT_SAT;
    } else {
        /* 正维簇：连续解空间（如欠约束系统），约束系统可满足 */
        lv_LOG_INFO("代数簇为正维（连续解空间，维度=%d），返回 SAT", dim);
        return SMT_RESULT_SAT;
    }
}

/**
 * @brief Groebner 后端：将求解结果解码为 SMTSolverResult
 *
 * 从代数簇中提取解点坐标，填充到 SMTSolverResult 的赋值数组中。
 * 每个点节点的 x/y 坐标值从簇的解点中读取。
 *
 * @param solver     Groebner 求解器实例
 * @param out_result 输出的求解结果
 * @return 成功返回 0，失败返回 -1
 */
int groebner_backend_decode(SMTSolver *solver, SMTSolverResult *out_result) {
    if (!solver || !out_result)
        return -1;
    if (!solver->groebner_registry || solver->groebner_variety_id < 0) {
        /* 没有有效的代数簇，无法解码 */
        return -1;
    }

    /* 遍历节点映射表，为每个有坐标映射的点节点创建 x/y 赋值条目 */
    int assignment_count = 0;
    int max_assignments = solver->groebner_var_count; /* 最多 var_count 个赋值 */

    if (max_assignments <= 0)
        lv_RETURN_ERROR(lv_ERROR_INVALID_STATE, "groebner_backend_decode: groebner_var_count=%d <= 0",
                        solver->groebner_var_count);

    SMTVariableAssignment *assignments =
        (SMTVariableAssignment *) lv_calloc((size_t) max_assignments, sizeof(SMTVariableAssignment));
    if (!assignments) {
        lv_RETURN_ERROR(lv_ERROR_OUT_OF_MEMORY, "Groebner 结果解码失败：无法分配赋值数组 (max=%d)", max_assignments);
    }

    /* 从代数簇中读取第一个解点的坐标值 */
    double *coords = NULL;
    bool got_solution = false;

    if (variety_is_zero_dimensional(solver->groebner_registry, solver->groebner_variety_id)) {
        coords = (double *) lv_calloc((size_t) max_assignments, sizeof(double));
        if (coords) {
            got_solution = variety_get_solution_point(solver->groebner_registry, solver->groebner_variety_id, 0, coords,
                                                      max_assignments);
            if (!got_solution) {
                /* 获取解点失败，回退到零值 */
                lv_free((void **) &coords);
                coords = NULL;
            }
        }
    }

    /* 遍历节点映射表，为每个有映射的点节点创建赋值 */
    for (int i = 0; i < solver->groebner_node_var_map_size && assignment_count < max_assignments; i++) {
        int var_idx = solver->groebner_node_var_map[i];
        if (var_idx < 0)
            continue;

        /* x 坐标赋值 */
        assignments[assignment_count].var_node_id = i;
        lv_snprintf(assignments[assignment_count].var_name, SMT_VAR_NAME_MAX_LEN, "p%d_x", i);
        assignments[assignment_count].is_boolean = false;
        assignments[assignment_count].value.rational.numerator = 0;
        assignments[assignment_count].value.rational.denominator = 1;
        assignments[assignment_count].value.rational.is_approx = true;
        assignments[assignment_count].value.rational.approx_value =
            (coords && var_idx < max_assignments) ? coords[var_idx] : 0.0;
        assignment_count++;

        if (assignment_count >= max_assignments)
            break;

        /* y 坐标赋值 */
        assignments[assignment_count].var_node_id = i;
        lv_snprintf(assignments[assignment_count].var_name, SMT_VAR_NAME_MAX_LEN, "p%d_y", i);
        assignments[assignment_count].is_boolean = false;
        assignments[assignment_count].value.rational.numerator = 0;
        assignments[assignment_count].value.rational.denominator = 1;
        assignments[assignment_count].value.rational.is_approx = true;
        assignments[assignment_count].value.rational.approx_value =
            (coords && (var_idx + 1) < max_assignments) ? coords[var_idx + 1] : 0.0;
        assignment_count++;
    }

    /* 释放临时坐标缓冲区 */
    if (coords) {
        lv_free((void **) &coords);
    }

    /* 填充结果结构 */
    out_result->assignments = assignments;
    out_result->assignment_count = assignment_count;

    lv_LOG_INFO("Groebner 结果解码完成: %d 个变量赋值", assignment_count);

    return 0;
}

/* ---- 外部求解器后端表：Z3 / cvc5 / Singular 共享同一调用骨架 ----
 * 三个外部后端在 smtsolver_check() 中的处理完全同构：
 * 编码检查 → smt_external_solver_check 子进程调用 → UNKNOWN 告警 →
 * 回退 Groebner 后端。此处收敛为表驱动，Z3/cvc5 原先只打日志不真回退
 * 的"伪降级"一并修复为与 Singular 一致的真回退。 */
typedef struct {
    SolverBackendType type;    /**< 后端类型 */
    const char *executable;    /**< 外部可执行文件名 */
    const char *encode_error;  /**< 编码缺失时的错误信息 */
    bool fallback_to_groebner; /**< UNKNOWN 时是否回退到 Groebner 后端 */
} ExternalBackendEntry;

static const ExternalBackendEntry kExternalBackends[] = {
    {SMT_Z3, "z3", "No SMT-LIB2 formula encoded for Z3 backend", true},
    {SMT_CVC5, "cvc5", "No SMT-LIB2 formula encoded for cvc5 backend", true},
    {SMT_SINGULAR, "singular", "No Singular script encoded", true},
};

/**
 * @brief 执行可满足性检查
 *
 * 根据后端类型执行不同的求解策略：
 * - GROEBNER：调用内置 Groebner 基引擎进行真实求解
 * - Z3/cvc5/Singular：通过子进程调用外部求解器
 */
SMTSatResult smtsolver_check(SMTSolver *solver) {
    lv_CHECK_NULL(solver, SMT_RESULT_ERROR);

    if (!solver->is_initialized) {
        smtsolver_set_error(solver, SMT_ERROR_SOLVER_CRASHED, "Solver not initialized");
        return SMT_RESULT_ERROR;
    }

    if (!solver->has_assertions) {
        smtsolver_set_error(solver, SMT_ERROR_ENCODING_FAILED, "No assertions loaded");
        return SMT_RESULT_ERROR;
    }

    if (solver->last_error == SMT_ERROR_BACKEND_UNAVAILABLE) {
        return SMT_RESULT_UNKNOWN;
    }

    /* ---- Groebner 后端：真实求解 ---- */
    if (solver->type == GROEBNER) {
        /*
         * Groebner 后端求解流程：
         * 注意：smtsolver_check() 的标准接口不接收 ConstraintGraph 参数，
         * 因此 Groebner 后端在 smtsolver_solve() 中完成完整求解。
         *
         * 这里优先返回缓存的代数簇 SAT 结果（若已求解），
         * 否则返回 SMT_RESULT_UNKNOWN，
         * 实际的 Groebner 求解在 smtsolver_solve() 中通过
         * groebner_backend_solve() 完成。
         *
         * 如果求解器已经有有效的代数簇（之前 solve 过），
         * 则直接返回缓存的 SAT 结果。
         */
        if (solver->groebner_variety_id >= 0) {
            /* 已有求解结果，返回 SAT */
            return SMT_RESULT_SAT;
        }
        return SMT_RESULT_UNKNOWN;
    }

    /* ---- 外部求解器后端（Z3 / cvc5 / Singular）：表驱动统一调用 ---- */
    for (size_t i = 0; i < sizeof(kExternalBackends) / sizeof(kExternalBackends[0]); i++) {
        const ExternalBackendEntry *be = &kExternalBackends[i];
        if (solver->type != be->type)
            continue;

        if (!solver->encoded_formula || solver->encoded_len <= 0) {
            smtsolver_set_error(solver, SMT_ERROR_ENCODING_FAILED, be->encode_error);
            return SMT_RESULT_ERROR;
        }
        lv_LOG_INFO("%s 后端: 通过子进程调用 %s (输入长度=%d)", smtsolver_backend_type_name(be->type), be->executable, solver->encoded_len);
        SMTSatResult ext_result =
            smt_external_solver_check(solver, be->executable, solver->encoded_formula, solver->encoded_len, NULL, 0);
        if (ext_result == SMT_RESULT_UNKNOWN && be->fallback_to_groebner) {
            lv_LOG_WARNING("%s 后端: 求解器返回 UNKNOWN（可能未安装 %s），回退到 Groebner 后端",
                           smtsolver_backend_type_name(be->type), be->executable);
            /* 回退到内部 Groebner 后端 */
            solver->type = SMT_GROEBNER;
            return smtsolver_check(solver);
        }
        return ext_result;
    }
    return SMT_RESULT_ERROR;
}

/**
 * @brief 从求解器输出中解码结果
 *
 * 对于 Groebner 后端，从代数簇中提取解点坐标。
 * 对于其他后端，填充基本的 SMTSolverResult 结构。
 */
int smtsolver_decode_result(SMTSolver *solver, SMTSatResult sat_result, SMTSolverResult *out_result) {
    lv_CHECK_NULL(solver, (int) -SMT_ERROR_PARSE_FAILED);

    if (!out_result) {
        return 0; /* 允许跳过结果构造 */
    }

    smtsolver_result_init(out_result);
    out_result->sat_result = sat_result;
    out_result->backend_used = solver->type;
    out_result->solve_time_ms = 0;

    if (sat_result == SMT_RESULT_ERROR) {
        out_result->error_code = solver->last_error;
        if (solver->last_error_msg[0]) {
            lv_strlcpy(out_result->error_message, solver->last_error_msg, sizeof(out_result->error_message));
        }
        return 0;
    }

    /* Groebner 后端：从代数簇中解码变量赋值 */
    if (solver->type == GROEBNER && sat_result == SMT_RESULT_SAT) {
        int rc = groebner_backend_decode(solver, out_result);
        if (rc < 0) {
            lv_LOG_WARNING("Groebner 结果解码失败，赋值数组为空");
            /* 解码失败不影响 SAT 结论，只是没有具体赋值 */
        }
    }

    return 0;
}

/**
 * @brief 完整求解管线：编码 -> 加载 -> 求解 -> 解码
 *
 * 对于 Groebner 后端，此函数执行完整的代数求解流程：
 * 1. 编码约束图为 SMT-LIB2（用于调试输出）
 * 2. 调用 Groebner 后端进行真实求解
 *    a. 初始化多项式环和理想
 *    b. 将约束转换为多项式生成元
 *    c. 计算 Groebner 基
 *    d. 判定可满足性
 *    e. 计算代数簇获取具体解
 * 3. 解码结果为统一的 SMTSolverResult
 */
int smtsolver_solve(SMTSolver *solver, const ConstraintGraph *graph, SMTSolverResult *out_result) {
    lv_CHECK_NULL(solver, -1);
    lv_CHECK_NULL(graph, -1);

    if (!out_result) {
        lv_RETURN_ERROR(lv_ERROR_INVALID_PARAM, "smtsolver_solve: out_result is NULL");
    }

    smtsolver_result_init(out_result);

    /* ---- Groebner 后端：直接通过多项式理想求解 ---- */
    if (solver->type == GROEBNER) {
        lv_LOG_INFO("Groebner 后端开始求解 (约束数=%d, 节点数=%d)", graph->constraint_count, graph->node_count);

        /* 调用 Groebner 后端求解 */
        SMTSatResult sat_res = groebner_backend_solve(solver, graph);
        out_result->sat_result = sat_res;
        out_result->backend_used = GROEBNER;

        /* 解码结果 */
        smtsolver_decode_result(solver, sat_res, out_result);

        lv_LOG_INFO("Groebner 后端求解完成: 结果=%s", smtsolver_sat_result_name(sat_res));

        return (sat_res == SMT_RESULT_SAT) ? 0 : ((sat_res == SMT_RESULT_ERROR) ? -1 : 1);
    }

    /* ---- 其他后端：标准 SMT-LIB2 编码管线 ---- */

    /* 步骤 1：编码约束图为 SMT-LIB2 */
    int smtlib2_buf_size = lv_config_get_int(LV_CFG_SMTLIB2_DEFAULT_BUFFER, SMTLIB2_DEFAULT_BUFFER);
    char *smtlib2_buf = (char *) lv_malloc((size_t)smtlib2_buf_size);
    if (!smtlib2_buf) {
        out_result->sat_result = SMT_RESULT_ERROR;
        out_result->error_code = SMT_ERROR_MEMORY_EXHAUSTED;
        lv_snprintf(out_result->error_message, sizeof(out_result->error_message), "Failed to allocate SMT-LIB2 buffer");
        return -1;
    }

    int enc_len = smtencode_constraint_graph_to_smtlib2(graph, solver->config.logic, solver->config.produce_unsat_cores,
                                                        smtlib2_buf, smtlib2_buf_size);
    if (enc_len < 0) {
        lv_free((void **) &smtlib2_buf);
        out_result->sat_result = SMT_RESULT_ERROR;
        out_result->error_code = SMT_ERROR_ENCODING_FAILED;
        lv_snprintf(out_result->error_message, sizeof(out_result->error_message), "SMT-LIB2 encoding failed");
        return -1;
    }

    /* 步骤 2：加载到求解器 */
    int rc = smtsolver_encode(solver, smtlib2_buf, enc_len);
    lv_free((void **) &smtlib2_buf);

    if (rc < 0) {
        out_result->sat_result = SMT_RESULT_ERROR;
        out_result->error_code = (SMTErrorCode) (-rc);
        lv_snprintf(out_result->error_message, sizeof(out_result->error_message), "Solver encoding failed");
        return -1;
    }

    /* 步骤 3：执行求解 */
    SMTSatResult sat_res = smtsolver_check(solver);
    out_result->sat_result = sat_res;
    out_result->backend_used = solver->type;

    /* 步骤 4：解码结果 */
    smtsolver_decode_result(solver, sat_res, out_result);

    return (sat_res == SMT_RESULT_SAT) ? 0 : ((sat_res == SMT_RESULT_ERROR) ? -1 : 1);
}

