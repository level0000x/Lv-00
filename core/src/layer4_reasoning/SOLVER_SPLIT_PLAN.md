# solver.c 拆分方案

## 现状（v1.1.0 — 全部完成）

- **文件**: `core/src/layer4_reasoning/solver.c`（聚合入口，仅 forward declarations）
- **原始行数**: ~8818 行 → **200 行**
- **函数数**: 87 个（已全部分布到 18 个子模块）
- **子模块目录**: `core/src/layer4_reasoning/solver/`
- **状态**: 全部 21 个模块已提取完成（另包含 solver_core.c CDCL SAT 求解器）

## 拆分策略

按功能子系统拆分为 19 个子文件，每个 80-800 行。solver.c 保留为聚合入口（仅 `#include` 和公共 API 包装）。

## 拆分清单

| # | 新文件 | 行数 | 原行范围 | 函数数 | 状态 |
|---|--------|------|----------|--------|------|
| 1 | `solver_dirty_set.h/.c` | ~60 | L6375-L6453 | 5 | ✅ 已完成 |
| 2 | `solver_snapshot.h/.c` | ~100 | L108-L222 | 3 | ✅ 已完成 |
| 3 | `mv_polynomial.h/.c` | ~230 | L4703-L4950 | 15 | ✅ 已完成 |
| 4 | `equation_system` | ~200 | — | 9 | ✅ solver_eq_system.c |
| 5 | `solver_coord_extract` | ~300 | — | 6 | ✅ solver_coord_extract.c |
| 6 | `solver_geom_util` | ~80 | — | 3 | ✅ solver_coord_extract.c (内含) |
| 7 | `solver_equation_extract` | ~1600 | — | 2 | ✅ solver_equation_extract.c（C-⑬ 从 solver_coord_extract.c 拆分落地） |
| 8 | `solver_univariate` | ~700 | — | 7 | ✅ solver_symbolic.c + solver_linear.c |
| 9 | `solver_symbolic_util` | ~250 | — | 3 | ✅ solver_symbolic.c (内含) |
| 10 | `solver_substitution` | ~200 | — | 2 | ✅ solver_symbolic.c（内含） |
| 11 | `solver_stats` | ~100 | — | 3 | ✅ solver_stats.c |
| 12 | `solver_conflict` | ~200 | — | 3 | ✅ solver_conflict.c |
| 13 | `solver_result` | ~80 | — | 3 | ✅ solver_result.c |
| 14 | `solver_engine` | ~800 | — | 2 | ✅ solver_engine.c |
| 15 | `solver_feedback` | ~150 | — | 3 | ✅ solver_feedback.c |
| 16 | `solver_eliminate` | ~600 | — | 3 | ✅ solver_eliminate.c |
| 17 | `solver_groebner` | ~800 | — | 5 | ✅ solver_groebner.c |
| 18 | `solver_geom_templates` | ~700 | — | 5 | ✅ solver_geom_templates.c |
| 19 | `solver_order` | ~280 | — | 2 | ✅ solver_order.c |
| 20 | `solver_incremental` | ~350 | — | 2 | ✅ solver_incremental.c |
| 21 | `solver_multibranch` | ~400 | — | 1 | ✅ solver_multibranch.c |

## 已识别的重复代码（拆分后需消除）

| 重复模式 | 涉及位置 | 建议 |
|----------|----------|------|
| 银行家舍入 | `coord_to_mpz_scaled` + `double_to_mpz_scaled` | 提取为 `mpz_banker_round()` |
| INCIDENCE 约束提取 | `extract_equations_from_constraints` + `solver_extract_equations_full` | 提取为 `extract_incidence_equation()` |
| INTERSECTION 约束提取 | 同上两处 | 提取为 `extract_intersection_equation()` |
| CONNECTION 距离约束 | 三处 | 提取为 `extract_distance_equation()` |
| 流式事件输出 | 8+ 处 | 封装为 `emit_solver_event()` 宏 |

## 拆分注意事项

1. 原 `static` 函数提取后需去掉 `static`，改为 `lv_PUBLIC_API`（如需要）或模块内部可见
2. 结构体定义移至对应 .h 文件
3. 每个新模块需包含 `lv/lv.h` 获取基础 API
4. solver.c 保留 `#include "solver_xxx.h"` 聚合所有子模块
5. CMakeLists.txt 需同步更新