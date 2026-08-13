/**
 * @file graph_rank.c
 * @brief 约束图线性相关检测的公共 mpq 行阶梯消元核心
 *
 * @details 从 graph_conflict.c（find_linearly_dependent_constraints）与
 *          graph_memory.c（graph_detect_redundant_constraints）抽取的高斯消元
 *          公共骨架，两处原为同一算法语义的重复实现（约 85 行近乎逐行同构）：
 *
 *          - 输入：调用方构建好的 mpq_t 增广矩阵（行主序，num_linear 行、
 *            num_vars + 1 列，最后一列为右端项；调用方负责 mpq_init 与清理）
 *          - 消元：部分选主元（列内首个非零行）→ 主元行缩放为 1 →
 *            消去所有行当前列 → rank++
 *          - 主元映射：pivot_row[i] 始终对应当前第 i 行内容的原始行索引
 *            （调用方预填充后由本函数在行交换时同步交换，可为 NULL 表示不维护）
 *          - 返回矩阵秩；调用方据此扫描全零行 / 同构行做冗余判定
 *
 *          仅依赖 GMP mpq 精确算术（mpq_inv/mul 与 mpq_div 数学等价，
 *          跳过零因子行的消元结果不变），故与 graph_conflict.c 原实现
 *          （mpq_div + 不跳过零因子）在数学上逐位等价。
 *
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "graph_node_internal.h"

/* mpq_t 经由 lv/constraint_graph.h -> lv/symbolic_coord.h -> gmp.h 可见，
 * 此处显式引用以明确依赖 */
#include "lv/symbolic_coord.h"

/**
 * @brief mpq 增广矩阵行阶梯消元（部分选主元 + 主元映射 + 秩）
 *
 * @param matrix     行主序 num_linear x (num_vars+1) 的 mpq_t 数组（调用方已 mpq_init）
 * @param num_linear 行数
 * @param num_vars   变量列数（增广常数项位于列 num_vars）
 * @param pivot_row  长度 num_linear 的 int 数组（可为 NULL）；输入时调用方预填充
 *                   "行→原始约束索引"映射，本函数在行交换时同步交换；
 *                   输出时 pivot_row[i] 为第 i 行内容的原始行索引
 * @return 矩阵秩（主元行数），调用方用于冗余判定
 */
int cg_mpq_row_echelon(mpq_t *matrix, int num_linear, int num_vars, int *pivot_row) {
    int rank = 0;
    for (int col = 0; col < num_vars && rank < num_linear; col++) {
        /* 选主元：列内首个非零行 */
        int pivot = -1;
        for (int row = rank; row < num_linear; row++) {
            if (mpq_sgn(matrix[row * (num_vars + 1) + col]) != 0) {
                pivot = row;
                break;
            }
        }
        if (pivot < 0)
            continue; /* 该列全零 */

        /* 交换行 rank 与 pivot（同步交换主元映射） */
        if (pivot != rank) {
            for (int j = 0; j <= num_vars; j++) {
                mpq_swap(matrix[rank * (num_vars + 1) + j], matrix[pivot * (num_vars + 1) + j]);
            }
            if (pivot_row) {
                lv_SWAP(int, pivot_row[rank], pivot_row[pivot]);
            }
        }

        /* 缩放主元行，使主元为 1 */
        mpq_t inv_pivot;
        mpq_init(inv_pivot);
        mpq_inv(inv_pivot, matrix[rank * (num_vars + 1) + col]);
        for (int j = col; j <= num_vars; j++) {
            mpq_mul(matrix[rank * (num_vars + 1) + j], matrix[rank * (num_vars + 1) + j], inv_pivot);
        }
        mpq_clear(inv_pivot);

        /* 消去所有行当前列 */
        for (int row = 0; row < num_linear; row++) {
            if (row == rank)
                continue;
            if (mpq_sgn(matrix[row * (num_vars + 1) + col]) == 0)
                continue;
            mpq_t factor;
            mpq_init(factor);
            mpq_set(factor, matrix[row * (num_vars + 1) + col]);
            for (int j = col; j <= num_vars; j++) {
                mpq_t tmp;
                mpq_init(tmp);
                mpq_mul(tmp, factor, matrix[rank * (num_vars + 1) + j]);
                mpq_sub(matrix[row * (num_vars + 1) + j], matrix[row * (num_vars + 1) + j], tmp);
                mpq_clear(tmp);
            }
            mpq_clear(factor);
        }

        rank++;
    }
    return rank;
}
