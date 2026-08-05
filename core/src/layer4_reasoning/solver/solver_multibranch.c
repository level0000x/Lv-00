/**
 * @file solver_multibranch.c
 * @brief 多解分支处理
 *
 * @details 从 solver.c 拆分出的子模块（Lv-00 项目 v3.3.0+）。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "solver_common.h"

SymbolicCoord *poly_eval_symbolic(const mpz_poly_t *poly, const SymbolicCoord *value);
void symbolic_coord_destroy(SymbolicCoord *coord);
int coord_to_double(const SymbolicCoord *c, double *out);
SymbolicCoord *symbolic_coord_create_rational(int64_t num, uint64_t den);
char *symbolic_coord_serialize(const SymbolicCoord *coord);

/* 流式上下文（定义在 solver_engine.c，通过 solver_types.h 的 extern 引用） */

/* ================================================================== */
/*  PUBLIC API: solver_handle_multiple_solutions                       */
/* ================================================================== */

/**
 * @brief 处理二次方程的多解分支
 *
 * @details 当二次方程 a*x^2 + b*x + c = 0 有判别式 D = b^2 - 4ac 时：
 *          - D > 0：两个不同实根
 *          - D = 0：一个重根
 *          - D < 0：无实根
 *          算法流程：
 *          1. 扫描方程系统中所有 degree=2 的方程
 *          2. 对每个二次方程计算两个根
 *          3. 生成所有组合（笛卡尔积）= 2^k 个分支（上限 2^12 = 4096）
 *          4. 过滤导致其他方程矛盾的分支
 *          流式输出每个根和解分支的进度。
 *
 * @param result         GroebnerResult 指针（用于流式输出元数据）
 * @param system         方程系统指针
 * @param out_branches   输出：有效分支的解数组
 * @param out_branch_count 输出：有效分支数量
 * @return SOLVER_STATUS_UNIQUE 表示唯一解，SOLVER_STATUS_NO_SOLUTION 表示无解，
 *         SOLVER_STATUS_OK 表示多解，SOLVER_STATUS_TIMEOUT 表示内存不足
 */
SolverStatus solver_handle_multiple_solutions(const GroebnerResult *result, const EquationSystem *system,
                                              SymbolicCoord ***out_branches, int *out_branch_count) {
    lv_UNUSED(result);
    if (!out_branches || !out_branch_count)
        return SOLVER_STATUS_TIMEOUT;
    *out_branches = NULL;
    *out_branch_count = 0;

    /* No system means no branches to handle */
    if (!system || system->eqs.count == 0) {
        return SOLVER_STATUS_UNIQUE;
    }

    /* Step 1: Identify quadratic equations that have distinct real roots.
     * For each quadratic a*x^2 + b*x + c = 0:
     *   root = (-b ± sqrt(D)) / (2*a)  where D = b^2 - 4*a*c
     * We collect the two possible values for each quadratic variable. */
    struct BranchVariable {
        int var_node_id; /* variable node ID */
        int coord_index; /* 0=x, 1=y */
        int eq_index;    /* index in system->eqs */
        double root1;    /* first root ( -b + sqrt(D) ) / (2a) */
        double root2;    /* second root ( -b - sqrt(D) ) / (2a) */
        bool valid;      /* has two distinct real roots */
    };

    int max_branch_vars = system->eqs.count;
    struct BranchVariable *branch_vars = lv_calloc((size_t) max_branch_vars, sizeof(struct BranchVariable));
    if (!branch_vars)
        return SOLVER_STATUS_TIMEOUT;

    int branch_count = 0;
    int64_t scale_factor = lv_SOLVER_SCALE_FACTOR;

    for (int i = 0; i < system->eqs.count; i++) {
        PolyEquation *pe = ((PolyEquation *)lv_darray_get(&system->eqs, i));
        if (pe->poly.degree != 2)
            continue;

        /* Extract coefficients from GMP scaled integers */
        double a = mpz_get_d(pe->poly.coeffs[2]) / scale_factor;
        double b = mpz_get_d(pe->poly.coeffs[1]) / scale_factor;
        double c = mpz_get_d(pe->poly.coeffs[0]) / scale_factor;

        if (fabs(a) < lv_EPSILON_DOUBLE)
            continue;

        double D = b * b - 4.0 * a * c; /* discriminant */
        if (D <= 0)
            continue; /* no distinct real roots */

        double sqrt_D = sqrt(D);
        double root1 = (-b + sqrt_D) / (2.0 * a);
        double root2 = (-b - sqrt_D) / (2.0 * a);

        /* Store as distinct roots */
        branch_vars[branch_count].var_node_id = pe->var_node_id;
        branch_vars[branch_count].coord_index = pe->coord_index;
        branch_vars[branch_count].eq_index = i;
        branch_vars[branch_count].root1 = root1;
        branch_vars[branch_count].root2 = root2;
        branch_vars[branch_count].valid = true;
        branch_count++;

        /* Stream: found quadratic root pair */
        if (solver_stream_ctx) {
            StreamEvent ev;
            memset(&ev, 0, sizeof(ev));
            ev.type = STREAM_EVENT_SOLVE_VARIABLE_RESOLVED;
            ev.timestamp_ms = stream_timestamp_ms();
            ev.step_number = branch_count;
            ev.description = "发现二次方程根对";
            char detail[lv_SOLVER_DETAIL_BUF_SIZE];
            int _snw_bv;
            lv_SAFE_SNPRINTF(_snw_bv, detail, sizeof(detail),
                             "{\"var_id\":%d,\"coord_index\":%d,\"root1\":%.6f,\"root2\":%.6f}",
                             pe->var_node_id, pe->coord_index, root1, root2);
            lv_UNUSED(_snw_bv);
            ev.detail_json = detail;
            stream_emit(solver_stream_ctx, &ev);
        }
    }

    /* Step 2: Determine if there are branches to explore.
     * For k quadratic equations, there are 2^k possible branches.
     * We cap at 2^12 = 4096 to avoid combinatorial explosion. */
    if (branch_count == 0) {
        lv_free((void **) &branch_vars);
        return SOLVER_STATUS_UNIQUE; /* No quadratic equations with distinct roots */
    }

    if (branch_count > 12) {
        if (solver_stream_ctx) {
            stream_emit_simple(solver_stream_ctx, STREAM_EVENT_WARNING,
                               "多解分支过多 (k>12)，已截断为 2^12=4096 个分支", branch_count);
        }
        branch_count = 12;
    }

    int total_branches = 1 << branch_count; /* 2^k */
    int valid_branches = 0;                 /* branches that passed validation */

    /* Step 3: Allocate flat 2D array for all branch coordinates.
     * branch_coords[b * branch_count + v] = v-th coordinate of branch b */
    int total_coords = total_branches * branch_count;
    SymbolicCoord **branch_coords = lv_calloc((size_t) total_coords, sizeof(SymbolicCoord *));
    if (!branch_coords) {
        lv_free((void **) &branch_vars);
        return SOLVER_STATUS_OUT_OF_MEMORY;
    }

    for (int b = 0; b < total_branches; b++) {
        for (int v = 0; v < branch_count; v++) {
            /* Bit v of b determines which root: 0 = root1, 1 = root2 */
            double chosen = (b & (1u << v)) ? branch_vars[v].root2 : branch_vars[v].root1;

            /* Create a rational coordinate from the double value. */
            int64_t num_val = (int64_t) (chosen * lv_SOLVER_SCALE_FACTOR);
            SymbolicCoord *coord = symbolic_coord_create_rational(num_val, (uint64_t) lv_SOLVER_SCALE_FACTOR);
            if (!coord) {
                /* Cleanup: destroy all coords created so far */
                for (int i = 0; i < b * branch_count + v; i++) {
                    symbolic_coord_destroy(branch_coords[i]);
                }
                lv_free((void **) &branch_coords);
                lv_free((void **) &branch_vars);
                return SOLVER_STATUS_TIMEOUT;
            }
            branch_coords[b * branch_count + v] = coord;
        }
    }

    /* Step 4: Validate each branch against remaining equations.
     * A branch is valid if it doesn't contradict any of the other equations
     * in the system when the quadratic values are substituted. */
    bool *branch_valid = lv_calloc((size_t) total_branches, sizeof(bool));
    if (!branch_valid) {
        for (int i = 0; i < total_coords; i++) {
            symbolic_coord_destroy(branch_coords[i]);
        }
        lv_free((void **) &branch_coords);
        lv_free((void **) &branch_vars);
        return SOLVER_STATUS_TIMEOUT;
    }

    for (int b = 0; b < total_branches; b++) {
        bool valid = true;

        /* Check each non-quadratic equation in the system:
         * substitute the branch's values and verify the equation holds */
        for (int eq = 0; eq < system->eqs.count; eq++) {
            /* Skip the quadratic equations themselves */
            bool is_branch_eq = false;
            for (int v = 0; v < branch_count; v++) {
                if (branch_vars[v].eq_index == eq) {
                    is_branch_eq = true;
                    break;
                }
            }
            if (is_branch_eq)
                continue;

            /* For linear equations (degree=1): check if the branch value
             * satisfies the equation. For now we do a simple linear check:
             * a*x + c = 0 => x ≈ -c/a */
            PolyEquation *pe_eq = ((PolyEquation *)lv_darray_get(&system->eqs, eq));
            if (pe_eq->poly.degree == 1) {
                /* Check if this equation constrains the same variable */
                if (pe_eq->poly.degree < 0) {
                /* 跳过未初始化的方程槽位（poly.degree < 0 表示该槽位未被
                 * 实际约束填充或已无效化）。占位符通常出现在方程系统被清空
                 * 后重新填充的过程中，不影响求解正确性。 */
                if (debug_is_debug_mode()) {
                    static lv_THREAD_LOCAL int placeholder_skip_count = 0;
                    placeholder_skip_count++;
                    if (placeholder_skip_count == 1 || placeholder_skip_count % 100 == 0) {
                        debug_log(LOG_LEVEL_DEBUG, "solver",
                                  "跳过占位符方程 #%d（poly.degree < 0），"
                                  "当前累计跳过 %d 个占位符方程",
                                  eq, placeholder_skip_count);
                    }
                }
                continue;
            }

                /* Find the branch value for this variable */
                double branch_val = 0.0;
                bool found = false;
                /* 安全检查：确保分支数不超过 unsigned int 位数，避免位移未定义行为 */
                if (branch_count > (int) (sizeof(unsigned int) * 8)) {
                    valid = false;
                    break;
                }
                for (int v = 0; v < branch_count; v++) {
                    if (branch_vars[v].var_node_id == pe_eq->var_node_id &&
                        branch_vars[v].coord_index == pe_eq->coord_index) {
                        branch_val = (b & (1u << v)) ? branch_vars[v].root2 : branch_vars[v].root1;
                        found = true;
                        break;
                    }
                }
                if (!found)
                    continue; /* Different variable, skip */

                /* Verify: a*x + c ≈ 0
                 * 使用符号坐标多项式求值进行精确验证，避免 double 精度丢失。
                 * 先尝试符号求值，失败时回退到 double 近似。 */
                bool equation_satisfied = false;

                /* 尝试符号精确验证 */
                if (branch_coords && branch_count > 0) {
                    /* 找到该分支中对应变量的符号坐标 */
                    for (int v = 0; v < branch_count; v++) {
                        if (branch_vars[v].var_node_id == pe_eq->var_node_id &&
                            branch_vars[v].coord_index == pe_eq->coord_index) {
                            SymbolicCoord *branch_coord = branch_coords[b * branch_count + v];
                            if (branch_coord) {
                                /* 用符号坐标在多项式上求值 */
                                SymbolicCoord *poly_val = poly_eval_symbolic(&pe_eq->poly, branch_coord);
                                if (poly_val) {
                                    /* 检查求值结果是否为零 */
                                    double eval_d = 0.0;
                                    if (coord_to_double(poly_val, &eval_d)) {
                                        equation_satisfied = (fabs(eval_d) < 1e-6);
                                    }
                                    symbolic_coord_destroy(poly_val);
                                }
                            }
                            break;
                        }
                    }
                }

                /* 回退到 double 近似验证 */
                if (!equation_satisfied) {
                    double a_coeff = mpz_get_d(pe_eq->poly.coeffs[1]) / lv_SOLVER_SCALE_FACTOR;
                    double c_coeff = mpz_get_d(pe_eq->poly.coeffs[0]) / lv_SOLVER_SCALE_FACTOR;
                    double lhs = a_coeff * branch_val + c_coeff;
                    equation_satisfied = (fabs(lhs) < 1e-6);
                }

                if (!equation_satisfied) {
                    /* Branch contradicts this equation */
                    valid = false;
                    break;
                }
            }
            /* For degree 0: check constant = 0 (contradiction if non-zero) */
            PolyEquation *pe_eq0 = ((PolyEquation *)lv_darray_get(&system->eqs, eq));
            if (pe_eq0->poly.degree == 0) {
                double const_val = mpz_get_d(pe_eq0->poly.coeffs[0]) / lv_SOLVER_SCALE_FACTOR;
                if (fabs(const_val) > lv_EPSILON_DOUBLE) {
                    valid = false;
                    break;
                }
            }
        }

        branch_valid[b] = valid;
        if (valid)
            valid_branches++;

        /* Stream: progress */
        if (solver_stream_ctx && (b % 100 == 0 || b == total_branches - 1)) {
            StreamEvent ev;
            memset(&ev, 0, sizeof(ev));
            ev.type = STREAM_EVENT_PROGRESS;
            ev.timestamp_ms = stream_timestamp_ms();
            ev.progress = (double) (b + 1) / (double) total_branches;
            ev.description = "多解分支验证中";
            char detail[lv_SOLVER_DETAIL_BUF_SIZE];
            int _snw4;
            lv_SAFE_SNPRINTF(_snw4, detail, sizeof(detail), "{\"checked\":%d,\"total\":%d,\"valid\":%d}", b + 1,
                             total_branches, valid_branches);
            lv_UNUSED(_snw4);
            ev.detail_json = detail;
            stream_emit(solver_stream_ctx, &ev);
        }
    }

    /* Step 5: Build output array of valid branches.
     * Each valid branch is a newly-allocated SymbolicCoord* array of branch_count entries,
     * copied from the flat branch_coords. */
    SymbolicCoord **out_branch_arr = lv_calloc((size_t) (valid_branches * branch_count), sizeof(SymbolicCoord *));
    if (!out_branch_arr) {
        for (int i = 0; i < total_coords; i++) {
            symbolic_coord_destroy(branch_coords[i]);
        }
        lv_free((void **) &branch_coords);
        lv_free((void **) &branch_valid);
        lv_free((void **) &branch_vars);
        return SOLVER_STATUS_OUT_OF_MEMORY;
    }

    int out_idx = 0;
    for (int b = 0; b < total_branches; b++) {
        if (!branch_valid[b])
            continue;

        /* Copy this branch's coordinates to the output array */
        int dst_base = out_idx * branch_count;
        int src_base = b * branch_count;
        for (int v = 0; v < branch_count; v++) {
            out_branch_arr[dst_base + v] = branch_coords[src_base + v];
        }
        out_idx++;

        /* Stream: emit valid branch */
        if (solver_stream_ctx) {
            StreamEvent ev;
            memset(&ev, 0, sizeof(ev));
            ev.type = STREAM_EVENT_SOLVE_VARIABLE_RESOLVED;
            ev.timestamp_ms = stream_timestamp_ms();
            ev.step_number = out_idx;
            ev.description = "有效多解分支";
            char detail[lv_SOLVER_DETAIL_BUF_SIZE];
            int pos;
            lv_SAFE_SNPRINTF(pos, detail, sizeof(detail), "{\"branch\":%d,\"valid\":true,\"values\":[", out_idx);
            for (int v = 0; v < branch_count && pos < (int) sizeof(detail) - 30; v++) {
                char *coord_str = symbolic_coord_serialize(branch_coords[src_base + v]);
                if (coord_str) {
                    int _sn_tmp;
                    lv_SAFE_SNPRINTF(_sn_tmp, detail + pos, (size_t) (sizeof(detail) - pos - 5), "%s\"%s\"",
                                     (v > 0 ? "," : ""), coord_str);
                    pos += _sn_tmp;
                    lv_free((void **) &coord_str);
                }
            }
            {
                int _sn_tmp2;
                lv_SAFE_SNPRINTF(_sn_tmp2, detail + pos, (size_t) (sizeof(detail) - pos - 3), "]}");
                lv_UNUSED(_sn_tmp2);
                lv_UNUSED(pos);
            }
            ev.detail_json = detail;
            stream_emit(solver_stream_ctx, &ev);
        }
    }

    /* Destroy invalid branch coordinates (they're not moved to out_branch_arr) */
    for (int b = 0; b < total_branches; b++) {
        if (!branch_valid[b]) {
            for (int v = 0; v < branch_count; v++) {
                symbolic_coord_destroy(branch_coords[b * branch_count + v]);
            }
        }
    }

    /* Free temporary working arrays.
     * branch_coords entries for valid branches were MOVED to out_branch_arr;
     * don't double-free them. */
    lv_free((void **) &branch_coords);
    lv_free((void **) &branch_valid);
    lv_free((void **) &branch_vars);

    if (valid_branches == 0) {
        lv_free((void **) &out_branch_arr);
        *out_branches = NULL;
        *out_branch_count = 0;
        if (solver_stream_ctx) {
            stream_emit_simple(solver_stream_ctx, STREAM_EVENT_ERROR, "多解分支处理: 所有分支均无效", 0);
        }
        return SOLVER_STATUS_NO_SOLUTION;
    }

    *out_branches = out_branch_arr;
    *out_branch_count = valid_branches;

    if (valid_branches == 1) {
        if (solver_stream_ctx) {
            stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_DONE, "多解分支处理: 过滤后仅剩唯一有效解",
                               valid_branches);
        }
        return SOLVER_STATUS_UNIQUE;
    }

    if (solver_stream_ctx) {
        char msg[128];
        int _snw5;
        lv_SAFE_SNPRINTF(_snw5, msg, sizeof(msg), "多解分支处理: 生成 %d 个有效分支 (共 %d 个理论组合)", valid_branches,
                         total_branches);
        lv_UNUSED(_snw5);
        stream_emit_simple(solver_stream_ctx, STREAM_EVENT_SOLVE_DONE, msg, valid_branches);
    }

    return SOLVER_STATUS_OK;
}
