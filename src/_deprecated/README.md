# _deprecated 目录说明

本目录存放项目演进过程中废弃的源文件和头文件。

## 当前内容

| 文件 | 废弃日期 | 原因 | 替代模块 |
|------|----------|------|----------|
| preset_basic_geometry_v2.c.deprecated | 2026-05 | v2 实现已迁移到 preset_basic_geometry.c | src/preset/preset_basic_geometry.c |
| symbolic_coord_fixed.c.deprecated | 2026-05 | 修复后的实现已合并到主文件 | src/core/symbolic_coord.c |
| symbolic_coord_new.c.deprecated | 2026-05 | 新实现已合并到主文件 | src/core/symbolic_coord.c |
| preset_basic_math.h | 2026-05 | 头文件拆分到独立预设模块 | include/lv00/preset_*.h |
| preset_blocks.h | 2026-05 | 注册逻辑迁移到 preset_blocks.c | src/func_block/preset_blocks.c |
| preset_calculus.h | 2026-05 | calculus 预设独立为模块 | include/lv00/preset_calculus.h |
| preset_common.h | 2026-05 | 公共定义迁移到 preset_common.c | src/func_block/preset_common.c |
| preset_core.h | 2026-05 | 核心定义已整合到主头文件 | include/lv00/lv00.h |

## 使用说明

- 这些文件**不参与构建**（已在 CMakeLists.txt 中排除）。
- 仅供历史参考，如需恢复旧实现可直接查阅。
- 带有 .deprecated 后缀的文件是标识为废弃的源文件。
- 不带后缀的 .h 文件是未被 .deprecated 标记但已废弃的头文件（将在后续版本清理）。
