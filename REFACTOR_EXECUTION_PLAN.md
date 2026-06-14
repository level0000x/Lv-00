# Lv-00 架构改革执行计划 v1.0

**目标**: 不破坏任何功能，逐步改善代码组织  
**总周期**: 4 周 (10 个阶段)  
**风险等级**: 低 (每个阶段都是向后兼容)

---

## 阶段概览

```
第 1 阶段  : 创建新的头文件目录结构 (不修改现有文件)
第 2 阶段  : 实现 CMake 层级验证脚本
第 3 阶段  : 标记内部头文件 (在注释中)
第 4 阶段  : Layer 4 源文件初步分类 (不移动)
第 5 阶段  : 创建头文件重定向层 (兼容性)
第 6 阶段  : 迁移 Layer 3 头文件
第 7 阶段  : 迁移 Layer 4 核心头文件
第 8 阶段  : 重组 Layer 4 源文件 (先创建子目录)
第 9 阶段  : 更新 CMakeLists.txt
第10 阶段  : 清理过时 include 路径，完成验收
```

每个阶段完成后都会**生成 PR 且包含测试验证**。

---

## 第 1 阶段：创建新头文件目录结构

**目标**: 在 `core/include/lv00/` 下建立分层子目录，但保持兼容性

**操作**:

```bash
# 创建子目录结构（仅创建，不移动文件）
mkdir -p core/include/lv00/layer2_shared/
mkdir -p core/include/lv00/layer3/
mkdir -p core/include/lv00/layer4/
mkdir -p core/include/lv00/layer4_backends/
mkdir -p core/include/lv00/layer4_presets/
mkdir -p core/include/lv00/layer5/
mkdir -p core/include/lv00/layer6/
mkdir -p core/include/lv00/internal/

# 在各目录下创建 README 说明文件
cat > core/include/lv00/layer3/README.md << 'EOF'
# Layer 3: Geometry & Topology

公开头文件，定义几何原语、约束图、拓扑等基础概念。

## 包含的模块
- constraint_graph.h
- symbolic_coord.h
- normalization.h
- ... 等

## 依赖关系
- 依赖: Layer 2 (资源管理)
- 被依赖: Layer 4 (推理层)

## 使用指南
公共 API，可在任何层级中 #include
EOF
```

**状态**: ✅ 完成后 = 目录存在但为空
**风险**: 无（仅创建目录）
**验收**: 列出所有新目录

---

## 第 2 阶段：实现 CMake 层级验证脚本

**目标**: 创建 `scripts/check_layer_boundaries.cmake`，能在构建时检查边界违规

**操作**:

创建文件 `scripts/check_layer_boundaries.cmake`:

```cmake
# 检查文件中的 #include 是否违反层级依赖
function(lv00_check_layer_boundary target layer_id forbidden_layers)
    get_target_property(sources ${target} SOURCES)
    
    foreach(src ${sources})
        if(NOT EXISTS "${src}")
            continue()
        endif()
        
        file(READ "${src}" file_content)
        
        # 查找所有 #include 语句
        string(REGEX MATCHALL "#include[[:space:]]*[<\"]([^>\"]+)[>\"]"
               includes "${file_content}")
        
        foreach(match ${includes})
            string(REGEX MATCH "[<\"]([^>\"]+)[>\"]" _ "${match}")
            set(inc_file "${CMAKE_MATCH_1}")
            
            # 检查是否在禁止层中
            foreach(forbidden_layer ${forbidden_layers})
                if(inc_file MATCHES "layer${forbidden_layer}/" OR
                   inc_file MATCHES "layer${forbidden_layer}_")
                    message(WARNING
                        "Layer ${layer_id} -> Layer ${forbidden_layer} include detected:\n"
                        "  File: ${src}\n"
                        "  Include: ${inc_file}"
                    )
                endif()
            endforeach()
        endforeach()
    endforeach()
endfunction()
```

在 `CMakeLists.txt` 中：

```cmake
if(ENABLE_LAYER_VALIDATION)
    include(${CMAKE_SOURCE_DIR}/scripts/check_layer_boundaries.cmake)
    
    # 定义允许的依赖关系
    lv00_check_layer_boundary(lv00_layer1_parser 1 "3;4;5;6;7;8;9;10")
    lv00_check_layer_boundary(lv00_layer3_geometry 3 "4;5;6;7;8;9;10")
    lv00_check_layer_boundary(lv00_layer4_reasoning 4 "5;6;7;8;9;10")
    # ... 等等
endif()
```

**状态**: ✅ 完成后 = 可以检测边界违规
**风险**: 低 (仅提示，不中止编译)
**验收**: 构建时能看到警告信息

---

## 第 3 阶段：标记内部头文件

**目标**: 在所有"内部实现"头文件的注释中标记，便于识别

**操作**:

在以下头文件顶部添加注释:

```c
/**
 * @file lv00_internal.h
 * @brief [内部实现头文件] Layer 2 内部数据结构定义
 * 
 * ⚠️ 注意：本头文件为内部实现，不应直接使用。
 * 请通过公开的 API 头文件（如 lv00.h）访问相关功能。
 * 
 * @visibility internal
 * @layer 2
 */
```

需要标记的文件 (~20个):
- `lv00_internal.h`
- `func_block_internal.h`
- `expr_canon.h`
- `axiom_grade.h`
- `proof_priority.h`
- `proof_trace.h`
- `runtime_guard.h`
- `logic_check.h`
- `circuit_breaker.h`
- `modal_operators.h`
- `quantifier.h`
- `rational.h`
- `proof_score.h`
- `exact_arithmetic.h`
- `parser_safety.h`
- `node_deep_copy.h`
- `geometry_types.h`
- `config.h`
- `context.h`
- `status_codes.h`

**状态**: ✅ 完成后 = 所有内部文件都有标记
**风险**: 无（仅添加注释）
**验收**: grep 检查标记存在

---

## 第 4 阶段：Layer 4 源文件初步分类

**目标**: 在不移动文件的情况下，创建分类文档，标记每个源文件应该去哪

**操作**:

创建 `core/src/layer4_reasoning/FILE_CLASSIFICATION.md`:

```markdown
# Layer 4 源文件分类计划

## 核心引擎 (core/) - 应迁移至 core/ 子目录
- engine.c
- engine_scheduler.c
- solver.c
- solver_core.c

## 证明系统 (proof/) - 应迁移至 proof/ 子目录
- proof.c
- proof_optimize.c
- proof_multi_strategy.c
- proof_trace.c
- proof_priority.c
- logic_check.c
- circuit_breaker.c

## 重写与合一 (rewrite_unify/) - 应迁移至 rewrite_unify/ 子目录
- rewrite.c
- rewrite_strategy.c
- unify.c
- normalization.c
- recursion.c
- expr_canonical.c
- expr_canon.c
- exact_arithmetic.c

## 类型与逻辑 (type_logic/) - 应迁移至 type_logic/ 子目录
- type_system.c
- prop_verifier.c
- three_valued_logic.c
- modal_operators.c
- quantifier.c

## 公理系统 (axiom/) - 已存在子目录
- axiom_pkg.c
- axiom_grade.c
✓ 保持不动

## 函数块 (func_block/) - 已存在子目录
- func_block.c
- func_block_compose.c
- func_block_determinism.c
- ... (10 files)
✓ 保持不动

## 后端：Groebner (backend_groebner/) - 新建子目录
- groebner_engine.c
- groebner_parallel.c

## 后端：SMT (backend_smt/) - 新建子目录
- smt_backend_impl.c
- smt_theory_combiner.c
- smt_bitvector.c
- smt_trigger_engine.c

## 后端：SAT/BDD (backend_sat/) - 新建子目录
- sat_encoding.c
- bdd_encoding.c
- approx_counter.c

## 后端：ATP (backend_atp/) - 新建子目录
- atp_backend.c

## 数值约束 (numerical/) - 新建子目录
- probabilistic_constraint.c
- inequality_reasoning.c
- rational.c

## 符号代数 (algebra_symbolic/) - 新建子目录
- nt_number_theory.c
- nt_polynomial.c
- sym_expr.c

## 预设模块 (preset/) - 已存在子目录，待重组
(55 files, 见下面的预设分类)

## 系统支持 (system/) - 新建子目录
- module.c
- mini_kernel.c
- gc_language.c
- ecosystem.c
- math_protocol.c
- stream.c
- stream_context_util.c
- relation_model.c
- algebra_mode.c

## 内部工具 (internal/) - 新建子目录
- conflict_detector.c
- proof_contradiction.c
- adaptive_pruning.c

## 预设分类方案

### geometry/ (15 files)
- preset_basic_geometry.c
- preset_advanced_geometry.c
- preset_geometry_3d.c
- preset_transformations.c
- preset_measurements.c
- preset_polygons.c
- preset_euclidean_geometry.c (如果存在)
- ... (8 more)

### algebra/ (12 files)
- preset_algebraic.c
- preset_linear_algebra.c
- preset_matrix.c
- preset_polynomial.c
- preset_group_theory.c
- preset_ring_theory.c
- preset_field_theory.c
- preset_number_theory.c
- ... (4 more)

### analysis/ (10 files)
- preset_analysis.c
- preset_differential_equations.c
- preset_differential_geometry.c
- preset_differential_geometry_adv.c
- preset_calculus.c
- preset_integral_transforms.c
- preset_special_functions.c
- ... (3 more)

### logic/ (8 files)
- preset_math_logic.c
- preset_mathematical_logic.c
- preset_logic_advanced.c
- preset_set_theory.c
- preset_category_theory.c
- preset_category_theory_adv.c
- ... (2 more)

### advanced/ (10 files)
- preset_homological_algebra.c
- preset_algebraic_geometry.c
- preset_arithmetic_geometry.c
- preset_algebraic_topology.c
- preset_algebraic_topology_adv.c
- preset_representation_theory.c
- preset_dynamical_systems.c
- preset_functional_analysis_adv.c
- preset_lattice_theory.c
- preset_order_theory.c

## 分类统计
总计: 73 文件
- 核心引擎: 4
- 证明系统: 7
- 重写合一: 8
- 类型逻辑: 5
- 公理系统: 3 (已分组)
- 函数块: 10 (已分组)
- Groebner: 2
- SMT: 4
- SAT/BDD: 3
- ATP: 1
- 数值: 3
- 符号代数: 3
- 预设: 55 (待重组)
- 系统: 8
- 内部: 3
总计 ✓
```

**状态**: ✅ 完成后 = 分类文档完成
**风险**: 无（仅文档）
**验收**: 文档清晰，分类完整

---

## 第 5 阶段：创建头文件重定向层

**目标**: 在 `core/include/lv00/` 创建兼容性头文件，引入新位置的文件

**操作**:

创建 `core/include/lv00/LAYER_MIGRATION.md` 说明：

```markdown
# 头文件迁移兼容性方案

为了保证向后兼容，采用"重定向"策略：

## 方案
1. 新头文件放在 `layer3/`, `layer4/` 等子目录
2. 在 `core/include/lv00/` 下保留兼容性头文件
3. 兼容性头文件只做 #include 重定向

## 示例

### 旧方式（v5.0）
```c
#include <lv00/constraint_graph.h>  // 直接在 lv00/
```

### 新方式（v5.1，推荐）
```c
#include <lv00/layer3/constraint_graph.h>  // 新位置
```

### 兼容性（v5.1，支持旧代码）
```c
#include <lv00/constraint_graph.h>  // 仍然可用！
```

## 实现
`core/include/lv00/constraint_graph.h` 内容：
```c
#ifndef LV00_CONSTRAINT_GRAPH_H
#define LV00_CONSTRAINT_GRAPH_H

// 向后兼容：将新位置的文件引入
#include "layer3/constraint_graph.h"

#endif // LV00_CONSTRAINT_GRAPH_H
```

这样既支持新代码使用 `#include <lv00/layer3/constraint_graph.h>`，
也支持旧代码继续使用 `#include <lv00/constraint_graph.h>`。
```

现在创建一些关键的重定向头文件（不创建所有，先做示范）：

**示范文件** (第 5 阶段只创建这些)：
- 不实际移动文件，只创建重定向
- 验证重定向机制有效

**状态**: ✅ 完成后 = 可以测试新旧 include 路径
**风险**: 低
**验收**: 编译不报错，两种 include 方式都工作

---

## 第 6 阶段：迁移 Layer 3 头文件

**目标**: 真正移动 Layer 3 的头文件到 `layer3/` 子目录

**操作**:

1. 在 `core/include/lv00/layer3/` 中创建所有 Layer 3 头文件
2. 更新头文件中的相对 include 路径
3. 在 `core/include/lv00/` 创建重定向头文件（用第 5 阶段方案）
4. 更新 CMakeLists.txt include path

**头文件列表** (~18个):
```
constraint_graph.h
symbolic_coord.h
euclidean_geometry.h
normalization.h
propagation.h
equiv_class.h
high_dim.h
geometry_compress.h
geometry_transform.h
sparse_linear_algebra.h
float_error.h
geo_event_detect.h
geo_spec.h
geometry_csg.h
geom_evol.h
mpz_poly.h
algebraic_number.h
geo_utils.h
... (18 total)
```

**状态**: ✅ 完成后 = Layer 3 头文件在 layer3/ 目录
**风险**: 低（有重定向层保护）
**验收**: 测试编译通过，两种 include 路径都工作

---

## 第 7 阶段：迁移 Layer 4 核心头文件

**目标**: 移动 Layer 4 的核心头文件到各子目录

**子目录分配**:

```
core/include/lv00/layer4/
├── core/
│   ├── engine.h
│   ├── solver.h
│   ├── engine_scheduler.h
│   └── solver_core.h
├── proof/
│   ├── proof.h
│   ├── proof_priority.h
│   ├── proof_trace.h
│   └── logic_check.h
├── backends/
│   ├── groebner_engine.h
│   ├── smt_backend.h
│   ├── sat_encoding.h
│   ├── bdd_encoding.h
│   ├── atp_backend.h
│   └── ...
└── [others]/
```

**操作**:
1. 在 `core/include/lv00/layer4/` 创建子目录
2. 复制头文件到对应子目录
3. 更新头文件内的相对 include
4. 创建重定向头文件
5. 更新 CMakeLists.txt

**状态**: ✅ 完成后 = Layer 4 头文件重组完成
**风险**: 中等（需更新许多 include 路径）
**验收**: 构建通过，所有测试通过

---

## 第 8 阶段：重组 Layer 4 源文件

**目标**: 在 `core/src/layer4_reasoning/` 中创建子目录，移动源文件

**操作**:

```bash
cd core/src/layer4_reasoning/

# 创建子目录
mkdir -p core/
mkdir -p proof/
mkdir -p backend_groebner/
mkdir -p backend_smt/
mkdir -p backend_sat/
mkdir -p backend_atp/
mkdir -p numerical/
mkdir -p algebra_symbolic/
mkdir -p system/
mkdir -p internal/

# 移动源文件到对应目录
# 注意：不是这个命令行完成，而是在代码中实现

# 更新 CMakeLists.txt 中的源文件列表
```

在 `CMakeLists.txt` 中更新 `LV00_LAYER4_SOURCES`:

```cmake
set(LV00_LAYER4_SOURCES
    # Core
    core/src/layer4_reasoning/core/engine.c
    core/src/layer4_reasoning/core/engine_scheduler.c
    core/src/layer4_reasoning/core/solver.c
    core/src/layer4_reasoning/core/solver_core.c
    
    # Proof
    core/src/layer4_reasoning/proof/proof.c
    core/src/layer4_reasoning/proof/proof_optimize.c
    core/src/layer4_reasoning/proof/proof_multi_strategy.c
    core/src/layer4_reasoning/proof/proof_trace.c
    core/src/layer4_reasoning/proof/proof_priority.c
    
    # Backends...
    # ...
)
```

**状态**: ✅ 完成后 = 源文件按功能分组
**风险**: 低（CMakeLists.txt 可完全向后兼容）
**验收**: 编译通过，构建时间无显著变化

---

## 第 9 阶段：更新 CMakeLists.txt 和头文件 include 路径

**目标**: 更新所有源文件中的 #include 路径，指向新位置

**操作**:

1. 生成脚本，自动更新 include：
   ```bash
   # 脚本逻辑：
   # 旧: #include "constraint_graph.h" → 新: #include <lv00/layer3/constraint_graph.h>
   # 旧: #include <lv00/solver.h> → 新: #include <lv00/layer4/core/solver.h>
   ```

2. 手工验证关键文件的 include 更新

3. 测试编译

**状态**: ✅ 完成后 = 所有 include 指向正确位置
**风险**: 中等（需仔细验证）
**验收**: 编译通过，无警告

---

## 第 10 阶段：清理过时 include 路径，完成验收

**目标**: 移除旧的 include 路径（可选），最终验证

**操作**:

1. 确认所有新 include 路径都工作
2. 运行完整测试套件
3. 更新文档（README, ARCHITECTURE.md 等）
4. 创建"架构改革完成"的标签

**最终验收清单**:
- [ ] 编译通过（Release 和 Debug）
- [ ] 所有测试通过
- [ ] CMake 依赖检查不报错
- [ ] 文档已更新
- [ ] 头文件目录结构清晰
- [ ] 内部 API 标记完成

**状态**: ✅ 完成后 = 架构改革完成
**风险**: 无
**验收**: 所有检查项 ✓

---

## 每个阶段的风险缓解

### 向后兼容策略
- **保留旧 include 路径** — 用重定向头文件
- **保留原始文件位置** — 先复制再删除
- **增量式迁移** — 先做文档和脚本，后做真实迁移

### 测试策略
- **每个阶段完成后编译** — 确认无破坏
- **运行现有测试** — 功能不变
- **新增层级检查测试** — 验证边界遵守

### 回滚计划
- **所有改动都在分支上** — 可随时回滚
- **保留 git 历史** — 可以 revert

---

## 时间预估

| 阶段 | 任务 | 工作量 | 时间 |
|------|------|--------|------|
| 1 | 创建目录 | 0.5h | 第1天 |
| 2 | CMake 验证脚本 | 1h | 第1天 |
| 3 | 标记内部头文件 | 1h | 第2天 |
| 4 | 源文件分类文档 | 1h | 第2天 |
| 5 | 重定向头文件 | 2h | 第3天 |
| 6 | 迁移 L3 头文件 | 2h | 第4-5天 |
| 7 | 迁移 L4 头文件 | 3h | 第5-6天 |
| 8 | 重组 L4 源文件 | 2h | 第6-7天 |
| 9 | 更新 include 路径 | 3h | 第7-8天 |
| 10 | 验收清理 | 2h | 第8-9天 |
| **总计** | | **17.5h** | **~2 周** |

---

## 后续维护

改架构完成后，建议：

1. **编写架构规范** (`doc/ARCHITECTURE_ADDING_NEW_FEATURE.md`)
2. **CI 集成检查** — 每次提交检查层级边界
3. **定期审查** — 每个季度检查一次架构清晰度
4. **记录决策** — 为什么某个功能放在某一层

---

**下一步**: 我现在开始执行第 1-3 阶段！
