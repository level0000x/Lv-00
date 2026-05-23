# Lv-00 API 使用指南

本文档提供 Lv-00 几何元语言库的 API 使用说明和最佳实践。

## 目录

1. [快速开始](#快速开始)
2. [核心概念](#核心概念)
3. [API 参考](#api-参考)
4. [示例代码](#示例代码)
5. [最佳实践](#最佳实践)

## 快速开始

### 包含头文件

```c
#include "lv00.h"
```

### 基本工作流程

```c
// 1. 创建约束图
ConstraintGraph *graph = graph_create();

// 2. 添加几何对象
SymbolicCoord *x = symbolic_coord_create_rational(0, 1);
SymbolicCoord *y = symbolic_coord_create_rational(1, 1);
SymbolicCoord *coords[] = {x, y};
graph_add_point(graph, coords, 2);

// 3. 添加约束
graph_add_line_segment(graph, 0, 1);
graph_add_incidence(graph, 0, 2);

// 4. 使用完成后销毁
graph_destroy(graph);
```

## 核心概念

### 1. 符号坐标 (SymbolicCoord)

Lv-00 使用符号坐标表示几何对象的位置，支持四种类型：

- **RATIONAL**: 有理数坐标 (a/b)
- **ALGEBRAIC**: 代数数（多项式根）
- **QUADRATIC**: 二次扩域元素 (a + b√d)
- **TRANSCENDENTAL**: 超越数（如 π, e）

```c
// 创建有理数坐标
SymbolicCoord *c1 = symbolic_coord_create_rational(3, 4);  // 3/4

// 创建二次坐标 (2 + 3√5)
Rational *a = rational_create(2, 1);
Rational *b = rational_create(3, 1);
SymbolicCoord *c2 = symbolic_coord_create_quadratic(a, b, 5);
```

### 2. 约束图 (ConstraintGraph)

约束图是 Lv-00 的核心数据结构，包含：
- **节点 (GeomNode)**: 点、线段、区域、端口、函数块
- **约束 (Constraint)**: 关联、之间、相交、包含、连接

### 3. 归一化 (Normalization)

归一化合并图中坐标相等的节点：

```c
NormalizationResult *result = graph_normalize(graph, false);
printf("合并了 %d 个节点\n", result->merged_count);
normalization_result_destroy(result);
```

### 4. 统一化 (Unification)

统一化验证构造是否满足命题：

```c
UnifyStatus status = unify_construction_with_proposition(construction, proposition);
if (status == UNIFY_STATUS_OK) {
    printf("构造满足命题！\n");
}
```

### 5. 函数块 (FuncBlock)

函数块是可复用的几何构造单元：

```c
// 打包函数块
FuncBlock *fb = NULL;
PackResult result = func_block_pack(
    graph, internal_ids, internal_count,
    input_port_ids, input_count,
    output_port_ids, output_count,
    cross_boundary_actions, action_count,
    &fb
);

// 实例化函数块
int arg_mappings[] = {arg_node_id};
int *new_node_ids = NULL;
int new_node_count = 0;
InstantiateResult inst_result = func_block_instantiate(
    fb, graph, arg_mappings, 1,
    &new_node_ids, &new_node_count
);
```

## API 参考

### 符号坐标 API

```c
// 创建
SymbolicCoord* symbolic_coord_create_rational(int64_t numer, uint64_t denom);
SymbolicCoord* symbolic_coord_create_algebraic(MP_Polynomial* poly, int root_index);
SymbolicCoord* symbolic_coord_create_quadratic(Rational* a, Rational* b, int64_t d);
SymbolicCoord* symbolic_coord_create_transcendental(const char* name);

// 销毁
void symbolic_coord_destroy(SymbolicCoord* coord);

// 序列化/反序列化
char* symbolic_coord_serialize(const SymbolicCoord* coord);
SymbolicCoord* symbolic_coord_deserialize(const char* str);

// 运算
SymbolicCoord* symbolic_coord_add(const SymbolicCoord* a, const SymbolicCoord* b);
SymbolicCoord* symbolic_coord_subtract(const SymbolicCoord* a, const SymbolicCoord* b);
SymbolicCoord* symbolic_coord_multiply(const SymbolicCoord* a, const SymbolicCoord* b);
SymbolicCoord* symbolic_coord_divide(const SymbolicCoord* a, const SymbolicCoord* b);
int symbolic_coord_compare(const SymbolicCoord* a, const SymbolicCoord* b);
```

### 约束图 API

```c
// 创建/销毁
ConstraintGraph* graph_create(void);
void graph_destroy(ConstraintGraph* graph);

// 添加节点
AddNodeResult graph_add_point(ConstraintGraph* graph, SymbolicCoord** coords, int coord_count);
AddNodeResult graph_add_line_segment(ConstraintGraph* graph, int p1_id, int p2_id);
AddNodeResult graph_add_region(ConstraintGraph* graph, GeomNode** boundary_segments, int segment_count);
AddNodeResult graph_add_port(ConstraintGraph* graph, PortType type, int connected_to, int parent_block_id);
AddNodeResult graph_add_function_block(ConstraintGraph* graph, int* internal_nodes, int internal_count,
                                       int* input_ports, int input_count,
                                       int* output_ports, int output_count);

// 添加约束
AddConstraintResult graph_add_incidence(ConstraintGraph* graph, int point_id, int line_or_region_id);
AddConstraintResult graph_add_betweenness(ConstraintGraph* graph, int p1_id, int p2_id, int p3_id);
AddConstraintResult graph_add_intersection(ConstraintGraph* graph, int line1_id, int line2_id, int result_point_id);
AddConstraintResult graph_add_containment(ConstraintGraph* graph, int inner_region_id, int outer_region_id);
AddConstraintResult graph_add_connection(ConstraintGraph* graph, int port1_id, int port2_id);

// 查询
GeomNode* graph_get_node(ConstraintGraph* graph, int node_id);
Constraint* graph_get_constraint(ConstraintGraph* graph, int constraint_id);

// 归一化
NormalizationResult* graph_normalize(ConstraintGraph* graph, bool dry_run);
void normalization_result_destroy(NormalizationResult* result);

// 跨边界约束检测
CrossBoundaryConstraint* find_cross_boundary_constraints(ConstraintGraph* graph,
                                                         int* internal_node_ids, int internal_count,
                                                         int* port_ids, int port_count,
                                                         int* out_count);
```

### 函数块 API

```c
// 创建/销毁
FuncBlock* func_block_create(int id);
void func_block_destroy(FuncBlock* fb);

// 设置属性
bool func_block_set_internal_nodes(FuncBlock* fb, int* node_ids, int count);
bool func_block_set_input_ports(FuncBlock* fb, int* port_ids, int count);
bool func_block_set_output_ports(FuncBlock* fb, int* port_ids, int count);
bool func_block_set_selector(FuncBlock* fb, SolutionSelector* selector);
bool func_block_add_port_dependency(FuncBlock* fb, PortDependency* dep);
bool func_block_set_preconditions(FuncBlock* fb, int* region_ids, int count);

// 打包
PackResult func_block_pack(ConstraintGraph* graph,
                           int* internal_node_ids, int internal_count,
                           int* input_port_ids, int input_count,
                           int* output_port_ids, int output_count,
                           CrossBoundaryAction* cross_boundary_actions,
                           int cross_boundary_count,
                           FuncBlock** out_func_block);

// 确定性检查
DeterminismCheckResult func_block_check_determinism_static(FuncBlock* fb, ConstraintGraph* graph, int step_limit);
DeterminismCheckResult func_block_check_determinism_dynamic(FuncBlock* fb, ConstraintGraph* graph,
                                                            SymbolicCoord** arg_values, int arg_count,
                                                            GeomNode*** out_solutions, int* out_solution_count);

// 实例化
InstantiateResult func_block_instantiate(FuncBlock* fb, ConstraintGraph* graph,
                                         int* arg_mappings, int arg_count,
                                         int** out_new_node_ids, int* out_new_node_count);

// 部分应用（柯里化）
bool func_block_partial_apply(FuncBlock* fb, ConstraintGraph* graph,
                              int* fixed_arg_mappings, int fixed_count,
                              FuncBlock** out_new_fb);

// 组合子
bool func_block_compose(FuncBlock* f, FuncBlock* g, ConstraintGraph* graph, FuncBlock** out_composed);
bool func_block_product(FuncBlock* f, FuncBlock* g, ConstraintGraph* graph, FuncBlock** out_product);

// 选择器
SolutionSelector* selector_create(SelectorType type);
SolutionSelector* selector_create_with_reference(SelectorType type, int reference_node_id);
SolutionSelector* selector_create_custom(SelectorFunction func, void* user_data);
void selector_destroy(SolutionSelector* selector);
bool selector_apply(SolutionSelector* selector, GeomNode** candidates, int count, int* out_selected_index);
```

### 统一化 API

```c
UnifyStatus unify_construction_with_proposition(ConstraintGraph* construction, ConstraintGraph* proposition);
```

## 示例代码

### 示例1: 创建点和线段

```c
#include "lv00.h"
#include <stdio.h>

int main() {
    ConstraintGraph *g = graph_create();
    
    // 创建两个点
    SymbolicCoord *x1 = symbolic_coord_create_rational(0, 1);
    SymbolicCoord *y1 = symbolic_coord_create_rational(0, 1);
    SymbolicCoord *coords1[] = {x1, y1};
    graph_add_point(g, coords1, 2);
    int p1 = g->next_node_id - 1;
    
    SymbolicCoord *x2 = symbolic_coord_create_rational(3, 1);
    SymbolicCoord *y2 = symbolic_coord_create_rational(4, 1);
    SymbolicCoord *coords2[] = {x2, y2};
    graph_add_point(g, coords2, 2);
    int p2 = g->next_node_id - 1;
    
    // 创建线段
    graph_add_line_segment(g, p1, p2);
    
    printf("创建了 %d 个节点\n", g->node_count);
    
    graph_destroy(g);
    return 0;
}
```

### 示例2: 函数块打包与实例化

```c
#include "lv00.h"
#include <stdio.h>

int main() {
    ConstraintGraph *g = graph_create();
    
    // 创建内部节点（两个点构成线段）
    SymbolicCoord *c1[] = {
        symbolic_coord_create_rational(0, 1),
        symbolic_coord_create_rational(0, 1)
    };
    graph_add_point(g, c1, 2);
    int p1 = g->next_node_id - 1;
    
    SymbolicCoord *c2[] = {
        symbolic_coord_create_rational(1, 1),
        symbolic_coord_create_rational(1, 1)
    };
    graph_add_point(g, c2, 2);
    int p2 = g->next_node_id - 1;
    
    graph_add_line_segment(g, p1, p2);
    int seg = g->next_node_id - 1;
    
    // 创建端口
    graph_add_port(g, PORT_INPUT, -1, -1);
    int in_port = g->next_node_id - 1;
    graph_add_port(g, PORT_OUTPUT, -1, -1);
    int out_port = g->next_node_id - 1;
    
    // 打包函数块
    int internal[] = {p1, p2, seg};
    int inputs[] = {in_port};
    int outputs[] = {out_port};
    
    FuncBlock *fb = NULL;
    PackResult result = func_block_pack(
        g, internal, 3, inputs, 1, outputs, 1,
        NULL, 0, &fb
    );
    
    if (result == PACK_OK) {
        printf("函数块创建成功，ID=%d\n", fb->id);
        
        // 创建实参并实例化
        SymbolicCoord *ac[] = {
            symbolic_coord_create_rational(5, 1),
            symbolic_coord_create_rational(5, 1)
        };
        graph_add_point(g, ac, 2);
        int arg = g->next_node_id - 1;
        
        int mappings[] = {arg};
        int *new_nodes = NULL;
        int new_count = 0;
        
        InstantiateResult inst = func_block_instantiate(
            fb, g, mappings, 1, &new_nodes, &new_count
        );
        
        if (inst == INSTANTIATE_OK) {
            printf("实例化成功，创建了 %d 个新节点\n", new_count);
            free(new_nodes);
        }
        
        func_block_destroy(fb);
    }
    
    graph_destroy(g);
    return 0;
}
```

### 示例3: 统一化验证

```c
#include "lv00.h"
#include <stdio.h>

int main() {
    // 创建构造图
    ConstraintGraph *construction = graph_create();
    
    SymbolicCoord *c1[] = {
        symbolic_coord_create_rational(0, 1),
        symbolic_coord_create_rational(0, 1)
    };
    graph_add_point(construction, c1, 2);
    
    SymbolicCoord *c2[] = {
        symbolic_coord_create_rational(1, 1),
        symbolic_coord_create_rational(0, 1)
    };
    graph_add_point(construction, c2, 2);
    
    graph_add_line_segment(construction, 0, 1);
    
    // 创建命题图（相同结构）
    ConstraintGraph *proposition = graph_create();
    
    SymbolicCoord *p1[] = {
        symbolic_coord_create_rational(0, 1),
        symbolic_coord_create_rational(0, 1)
    };
    graph_add_point(proposition, p1, 2);
    
    SymbolicCoord *p2[] = {
        symbolic_coord_create_rational(1, 1),
        symbolic_coord_create_rational(0, 1)
    };
    graph_add_point(proposition, p2, 2);
    
    graph_add_line_segment(proposition, 0, 1);
    
    // 统一化
    UnifyStatus status = unify_construction_with_proposition(construction, proposition);
    
    switch (status) {
        case UNIFY_STATUS_OK:
            printf("统一化成功！\n");
            break;
        case UNIFY_STATUS_CONSTRAINT_MISMATCH:
            printf("约束不匹配\n");
            break;
        default:
            printf("统一化失败: %d\n", status);
    }
    
    graph_destroy(construction);
    graph_destroy(proposition);
    return 0;
}
```

## 最佳实践

### 1. 内存管理

- 始终配对使用创建和销毁函数
- `symbolic_coord_create_*` 创建的坐标在添加到图后由图管理
- `func_block_pack` 创建的函数块需要手动销毁

```c
// 正确做法
FuncBlock *fb = func_block_create(1);
// ... 使用 fb ...
func_block_destroy(fb);

// 错误做法（内存泄漏）
FuncBlock *fb = func_block_create(1);
// ... 使用 fb ...
// 忘记调用 func_block_destroy(fb);
```

### 2. 错误处理

始终检查 API 返回值：

```c
PackResult result = func_block_pack(..., &fb);
if (result != PACK_OK) {
    fprintf(stderr, "打包失败: %s\n", pack_result_to_string(result));
    // 处理错误
}
```

### 3. 跨边界约束处理

打包函数块时，如果存在跨边界约束，必须提供处理方式：

```c
CrossBoundaryAction actions[] = {CROSS_BOUNDARY_PROMOTE};
PackResult result = func_block_pack(
    graph, internal_ids, internal_count,
    input_ids, input_count, output_ids, output_count,
    actions, 1, &fb
);
```

### 4. 归一化时机

- 在添加大量节点后执行归一化以合并重复节点
- 在统一化之前执行归一化以确保图的一致性
- 避免频繁归一化（性能开销）

### 5. 确定性检查

对于需要唯一解的函数块，在使用前进行确定性检查：

```c
DeterminismCheckResult det = func_block_check_determinism_static(fb, graph, 1000);
if (det != DETERMINISM_CHECK_UNIQUE) {
    // 设置选择器或处理多解情况
    SolutionSelector *sel = selector_create(SELECTOR_POSITIVE_ROOT);
    func_block_set_selector(fb, sel);
}
```

## 故障排除

### 问题: `graph_add_incidence` 返回错误

**原因**: 参数类型不匹配。`graph_add_incidence` 要求第二个参数是线段或区域。

**解决**: 确保传入正确的节点类型：

```c
// 正确
graph_add_incidence(graph, point_id, line_segment_id);

// 错误
graph_add_incidence(graph, point_id, another_point_id);  // 错误！
```

### 问题: `func_block_pack` 返回 `PACK_CROSS_BOUNDARY_CONFLICT`

**原因**: 内部节点与外部节点之间存在约束，但未提供处理方式。

**解决**: 提供 `CrossBoundaryAction` 数组：

```c
CrossBoundaryAction actions[] = {CROSS_BOUNDARY_PROMOTE};
PackResult result = func_block_pack(..., actions, 1, &fb);
```

### 问题: 内存泄漏

**原因**: 未正确销毁对象。

**解决**: 确保所有创建的对象都被销毁：

```c
// 使用 valgrind 或类似工具检测内存泄漏
valgrind --leak-check=full ./your_program
```

## 相关文档

- [模块文档](01_symbolic_coord.md) - 符号坐标系统
- [模块文档](02_constraint_graph.md) - 约束图系统
- [模块文档](03_normalization.md) - 归一化算法
- [模块文档](04_unify.md) - 统一化系统
- [模块文档](08_func_block.md) - 函数块系统
