# solver.c 拆分方案

## 现状

- **文件**: `core/src/layer4_reasoning/solver.c`
- **行数**: ~8818 行
- **函数数**: 87 个
- **问题**: 严重违反单一职责原则，难以维护、审查、测试

## 拆分策略

按功能子系统拆分为 19 个子文件，每个 80-800 行。solver.c 保留为聚合入口（仅 `#include` 和公共 API 包装）。

## 拆分清单

| # | 新文件 | 行数 | 原行范围 | 函数数 | 状态 |
|---|--------|------|----------|--------|------|
| 1 | `solver_dirty_set.h/.c` | ~60 | L6375-L6453 | 5 | ✅ 已完成 |
| 2 | `solver_snapshot.h/.c` | ~100 | L108-L222 | 3 | ✅ 已完成 |
| 3 | `mv_polynomial.h/.c` | ~230 | L4703-L4950 | 15 | ✅ 已完成 |
| 4 | `equation_system.h/.c` | ~200 | L256-L338, L6952-L6996 | 9 | 待提取 |
| 5 | `solver_coord_convert.h/.c` | ~300 | L355-L624 | 6 | 待提取 |
| 6 | `solver_geom_util.h/.c` | ~80 | L650-L700 | 3 | 待提取 |
| 7 | `solver_equation_extract.h/.c` | ~1600 | L754-L1635, L7369-L8185 | 2 | 待提取 |
| 8 | `solver_univariate.h/.c` | ~700 | L1700-L2858 | 7 | 待提取 |
| 9 | `solver_symbolic_util.h/.c` | ~250 | L2357-L2620 | 3 | 待提取 |
| 10 | `solver_substitution.h/.c` | ~200 | L2623-L2840 | 2 | 待提取 |
| 11 | `solver_stats.h/.c` | ~100 | L1636-L1699, L3112-L3170 | 3 | 待提取 |
| 12 | `solver_conflict.h/.c` | ~200 | L3019-L3110, L4555-L4700 | 3 | 待提取 |
| 13 | `solver_result.h/.c` | ~80 | L3171-L3201, L3358-L3376, L7338-L7368 | 3 | 待提取 |
| 14 | `solver_engine.h/.c` | ~800 | L3202-L3357, L3377-L3807 | 2 | 待提取 |
| 15 | `solver_feedback.h/.c` | ~150 | L3808-L3976 | 3 | 待提取 |
| 16 | `solver_eliminate.h/.c` | ~600 | L3977-L4388, L4149-L4554 | 3 | 待提取 |
| 17 | `solver_groebner.h/.c` | ~800 | L4961-L5696, L5592-L5696, L8186-L8417 | 5 | 待提取 |
| 18 | `solver_geom_templates.h/.c` | ~700 | L5697-L6390 | 5 | 待提取 |
| 19 | `solver_order.h/.c` | ~280 | L6520-L6951 | 2 | 待提取 |
| 20 | `solver_incremental.h/.c` | ~350 | L7022-L7337 | 2 | 待提取 |
| 21 | `solver_multibranch.h/.c` | ~400 | L8418-L8818 | 1 | 待提取 |

## 已识别的重复代码（拆分后需消除）

| 重复模式 | 涉及位置 | 建议 |
|----------|----------|------|
| 银行家舍入 | `coord_to_mpz_scaled` + `double_to_mpz_scaled` | 提取为 `mpz_banker_round()` |
| INCIDENCE 约束提取 | `extract_equations_from_constraints` + `solver_extract_equations_full` | 提取为 `extract_incidence_equation()` |
| INTERSECTION 约束提取 | 同上两处 | 提取为 `extract_intersection_equation()` |
| CONNECTION 距离约束 | 三处 | 提取为 `extract_distance_equation()` |
| 流式事件输出 | 8+ 处 | 封装为 `emit_solver_event()` 宏 |

## 拆分注意事项

1. 原 `static` 函数提取后需去掉 `static`，改为 `LV00_PUBLIC_API`（如需要）或模块内部可见
2. 结构体定义移至对应 .h 文件
3. 每个新模块需包含 `lv00/lv00.h` 获取基础 API
4. solver.c 保留 `#include "solver_xxx.h"` 聚合所有子模块
5. CMakeLists.txt 需同步更新