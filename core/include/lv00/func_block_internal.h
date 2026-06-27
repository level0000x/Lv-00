#ifndef LV00_FUNC_BLOCK_INTERNAL_H
#define LV00_FUNC_BLOCK_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

#include "lv00/lv00.h"
#include "lv00/stream.h"
#include "lv00/func_block.h"

/* ============== 确定性统计结构 ============== */

typedef struct {
    int steps;
    int linear_count;
    int quadratic_count;
    int connection_count;
    int total_dof;
    int free_dof;
} DeterminismStaticStats;

/* ============== 流式上下文全局变量（跨子模块共享） ============== */

extern LV00_THREAD_LOCAL StreamContext *func_block_stream_ctx;

/* ============== 内部共享函数 ============== */

bool collect_all_block_ids(const FuncBlock *fb, int **out_ids, int *out_count);

int *determinism_collect_constraint_stats(
    const FuncBlock *fb,
    const ConstraintGraph *graph,
    int step_limit,
    DeterminismStaticStats *stats);

int determinism_evaluate_linear_dof(int free_dof);

void determinism_cleanup_groebner(void *gresult);

/* ============== 兼容宏（旧命名 → 新命名） ============== */

/* DeterminismState 旧命名兼容 */
#ifndef DETERMINISM_VERIFIED
#define DETERMINISM_VERIFIED DETERMINISM_STATE_VERIFIED
#endif
#ifndef DETERMINISM_NON_DETERMINISTIC
#define DETERMINISM_NON_DETERMINISTIC DETERMINISM_STATE_NON_DETERMINISTIC
#endif
#ifndef DETERMINISM_PARTIALLY_VERIFIED
#define DETERMINISM_PARTIALLY_VERIFIED DETERMINISM_STATE_PARTIALLY_VERIFIED
#endif
#ifndef DETERMINISM_UNVERIFIED
#define DETERMINISM_UNVERIFIED DETERMINISM_STATE_UNVERIFIED
#endif

/* DeterminismCheckResult 旧命名兼容 */
#ifndef DETERMINISM_CHECK_UNIQUE
#define DETERMINISM_CHECK_UNIQUE DETERMINISM_CHECK_RESULT_UNIQUE
#endif
#ifndef DETERMINISM_CHECK_MULTIPLE
#define DETERMINISM_CHECK_MULTIPLE DETERMINISM_CHECK_RESULT_MULTIPLE
#endif
#ifndef DETERMINISM_CHECK_NO_SOLUTION
#define DETERMINISM_CHECK_NO_SOLUTION DETERMINISM_CHECK_RESULT_NO_SOLUTION
#endif
#ifndef DETERMINISM_CHECK_TIMEOUT
#define DETERMINISM_CHECK_TIMEOUT DETERMINISM_CHECK_RESULT_TIMEOUT
#endif
#ifndef DETERMINISM_CHECK_OUT_OF_RANGE
#define DETERMINISM_CHECK_OUT_OF_RANGE DETERMINISM_CHECK_RESULT_OUT_OF_RANGE
#endif

/* SolverStatus 旧命名兼容 */
#ifndef SOLVER_UNIQUE
#define SOLVER_UNIQUE SOLVER_STATUS_UNIQUE
#endif
#ifndef SOLVER_MULTIPLE
#define SOLVER_MULTIPLE SOLVER_STATUS_MULTIPLE
#endif
#ifndef SOLVER_NO_SOLUTION
#define SOLVER_NO_SOLUTION SOLVER_STATUS_NO_SOLUTION
#endif
#ifndef SOLVER_OVERCONSTRAINED
#define SOLVER_OVERCONSTRAINED SOLVER_STATUS_OVERCONSTRAINED
#endif
#ifndef SOLVER_OUT_OF_SCOPE
#define SOLVER_OUT_OF_SCOPE SOLVER_STATUS_OUT_OF_SCOPE
#endif
#ifndef SOLVER_TIMEOUT
#define SOLVER_TIMEOUT SOLVER_STATUS_TIMEOUT
#endif

/* GeomNode NODE_TYPE 旧命名兼容（使用 GEOM_* 前缀） */
#ifndef NODE_TYPE_POINT
#define NODE_TYPE_POINT GEOM_POINT
#endif
#ifndef NODE_TYPE_LINE
#define NODE_TYPE_LINE GEOM_LINE_SEGMENT
#endif
#ifndef NODE_TYPE_REGION
#define NODE_TYPE_REGION GEOM_REGION
#endif
/* 以下类型未在 constraint_graph.h 的 GeomType 枚举中定义，
 * 使用负值确保 switch 语句不会匹配它们（由 default 处理） */
#ifndef NODE_TYPE_CIRCLE
#define NODE_TYPE_CIRCLE ((GeomType)-1)
#endif
#ifndef NODE_TYPE_ARC
#define NODE_TYPE_ARC ((GeomType)-2)
#endif
#ifndef NODE_TYPE_POLYGON
#define NODE_TYPE_POLYGON ((GeomType)-3)
#endif
#ifndef NODE_TYPE_SCALAR
#define NODE_TYPE_SCALAR ((GeomType)-4)
#endif
#ifndef NODE_TYPE_VECTOR
#define NODE_TYPE_VECTOR ((GeomType)-5)
#endif

/* FB_VIEW_STATE_* 旧命名兼容 */
#ifndef FB_VIEW_COLLAPSED
#define FB_VIEW_COLLAPSED FB_VIEW_STATE_COLLAPSED
#endif
#ifndef FB_VIEW_PINNED
#define FB_VIEW_PINNED FB_VIEW_STATE_PINNED
#endif
#ifndef FB_VIEW_EXPANDED
#define FB_VIEW_EXPANDED FB_VIEW_STATE_EXPANDED
#endif

/* 配置默认值 */
#ifndef LV00_DEFAULT_DETERMINISM_STEP_LIMIT
#define LV00_DEFAULT_DETERMINISM_STEP_LIMIT 10000
#endif

#ifdef __cplusplus
}
#endif

#endif /* LV00_FUNC_BLOCK_INTERNAL_H */
