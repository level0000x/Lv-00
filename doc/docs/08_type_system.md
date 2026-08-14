# 08. 类型系统（Type System）

## 模块概述

类型系统为 Lv-00 提供类似 Martin-Lof 类型论的类型理论实现，覆盖**宇宙层级、类型等价检查、类型推断与类型检查器**四大能力。它以 `type_system.h` 为核心上下文，管理类型区域、类型变量、重写规则与推断规则表；并以 λ-演算子体系（`lambda_term.h` 的 De Bruijn λ-项、`lambda_church.h` 的 Church 编码、`lambda_to_graph.h` 的图上 β-归约）提供可求值、可编译的项层支撑。类型区域通过外部映射表附加到几何节点，不侵入 `GeomNode` 结构。

**覆盖头文件**：
- `type_system.h` —— 宇宙层级、类型种类、等价检查、推断规则表、路径探索器
- `lambda_term.h` —— LvLambdaTerm 数据结构与 β-归约求值器
- `lambda_church.h` —— Church 数字/布尔/列表及其运算的 λ-项构造器
- `lambda_to_graph.h` —— λ-项编译为约束图与图上 β-归约

## 核心设计原则

1. **宇宙层级与累积性**：`UniverseLevel` 整数层级（0=基本几何体，1=类型区域，2+=高阶类型），`UNIVERSE_LEVEL_ANY` 关闭层级检查进入非良基模式；`cumulative` 开关控制低层类型是否自动属于高层。
2. **良基/非良基双模式**：`well_founded` 开关切换；非良基模式下支持循环类型检测（`type_detect_cycle`）与相容性检查。
3. **联合体风格平铺字段**：`TypeRegion` 不分嵌套子结构体，所有种类共享同一布局，按 `kind` 字段安全访问子集字段，减少间接寻址与分配次数。
4. **规则表驱动推断**：`TypeInferenceRule` 按优先级（数值小优先）排序，`type_infer_by_rules` 遍历规则表取首条匹配，支持注册/清除自定义规则。
5. **等价检查两级**：`type_check_equivalence` 支持直接结构比较与基于重写引擎的规范化比较；无法归一化时返回 `TYPE_EQUIV_NEEDS_INTERACTION` 交给路径探索器。
6. **外部节点-类型映射**：`NodeTypeMapping` 数组将类型附加到节点，不改动约束图节点本体，支持附加/查询/分离。
7. **λ-演算几何编码互操作**：De Bruijn 索引编码约定在 λ-项与图上端口之间保持一致，α-等价自然满足。

## 关键数据结构

```c
/* 宇宙层级：0=基本几何体，1=类型区域，2+=高阶；ANY 关闭层级检查 */
typedef enum {
    UNIVERSE_LEVEL_0 = 0, UNIVERSE_LEVEL_1 = 1,
    UNIVERSE_LEVEL_2 = 2, UNIVERSE_LEVEL_3 = 3,
    UNIVERSE_LEVEL_ANY = -1
} UniverseLevel;

/* 类型种类：由 LV_TYPE_KIND_X X-macro 生成枚举与字符串映射 */
typedef enum {
    TYPE_KIND_POINT, TYPE_KIND_LINE_SEGMENT, TYPE_KIND_REGION,
    TYPE_KIND_FUNCTION, TYPE_KIND_PRODUCT, TYPE_KIND_SUM,
    TYPE_KIND_VARIABLE, TYPE_KIND_DEPENDENT, TYPE_KIND_BOTTOM,
    TYPE_KIND_PREDICATE_SUBTYPE
} TypeKind;

/* 等价/检查结果 */
typedef enum { TYPE_EQUIV_OK, TYPE_EQUIV_NOT_EQUIV, TYPE_EQUIV_UNKNOWN,
               TYPE_EQUIV_ERROR, TYPE_EQUIV_NEEDS_INTERACTION } TypeEquivResult;
typedef enum { TYPE_CHECK_OK, TYPE_CHECK_MISMATCH, TYPE_CHECK_INCOMPATIBLE,
               TYPE_CHECK_LEVEL_ERROR, TYPE_CHECK_CYCLE,
               TYPE_CHECK_INFERRED, TYPE_CHECK_ERROR } TypeCheckResult;

/* 类型区域：联合体风格平铺字段，按 kind 决定活跃字段子集 */
struct TypeRegion {
    int id;              TypeKind kind;       UniverseLevel level;
    int *contained_node_ids; int contained_count;       /* REGION */
    TypeRegion *input_type;  TypeRegion *output_type;   /* FUNCTION */
    TypeRegion *left_type;   TypeRegion *right_type;    /* PRODUCT */
    TypeRegion *first_type;  TypeRegion *second_type;   /* SUM */
    int variable_id;     char *variable_name;           /* VARIABLE */
    int param_node_id;   TypeRegion *body_type;         /* DEPENDENT */
    TypeRegion *base_type; char *predicate_name;        /* PREDICATE_SUBTYPE */
    char *predicate_expr; int predicate_constraint_id;
    char *alias_name;    TypeRegion *aliased_type;      /* 类型别名 */
    int *constraint_ids; int constraint_count;          /* 约束条件 */
};

/* 类型变量：多态变量经 bound_type 实例化 */
struct TypeVariable {
    int id; char *name; TypeRegion *bound_type; bool is_polymorphic;
};

/* 类型推断规则：按 priority 排序，数值越小越优先 */
typedef struct {
    int source_node_type;    /* 源节点几何类型，如 GEOM_POINT */
    int target_type_kind;    /* 推断出的类型种类，如 TYPE_KIND_POINT */
    int priority;            /* 优先级（数值越小优先级越高） */
    const char *description; /* 人类可读规则描述 */
} TypeInferenceRule;

/* 节点-类型映射：外部映射表条目，不修改 GeomNode */
typedef struct { int node_id; TypeRegion *type; } NodeTypeMapping;

/* 重写路径：记录类型重写历史，支持回放 */
typedef struct {
    TypeRewriteStep *steps; /* 含 step_number / rule_name / before / after */
    int step_count; int capacity;
} TypeRewritePath;

/* 类型系统上下文：多组动态数组，count/capacity 必须同步扩容 */
struct TypeSystem {
    TypeRegion **type_regions; int type_region_count; int type_region_capacity;
    TypeVariable **type_vars; int type_var_count; int type_var_capacity;
    RewriteRule **rewrite_rules; int rewrite_rule_count;
    bool well_founded; bool cumulative; int max_universe_level;
    NodeTypeMapping *node_type_mappings; int node_type_mapping_count; int node_type_mapping_capacity;
    TypeRewritePath *rewrite_path;
    TypeInferenceRule *inference_rules; int inference_rule_count; int inference_rule_capacity;
};

/* λ-项：tagged union，Var 使用 De Bruijn 相对索引 */
typedef struct LvLambdaTerm {
    LvLambdaTermType type;  /* LV_LAMBDA_VAR / LV_LAMBDA_ABS / LV_LAMBDA_APP */
    union {
        struct { int index; } var;                 /* De Bruijn 索引，0=最近 binder */
        struct { int binder; struct LvLambdaTerm *body; } abs;
        struct { struct LvLambdaTerm *left; struct LvLambdaTerm *right; } app;
    } data;
} LvLambdaTerm;
```

## 主要接口（表格）

| 分组 | 接口 | 说明 |
| --- | --- | --- |
| 系统管理 | `type_system_create/destroy`、`type_system_set_well_founded`、`type_system_set_cumulative` | 类型系统生命周期与模式开关 |
| 类型构造器 | `type_create_point/line_segment/region/function/product/sum/variable/dependent/bottom` | 工厂函数；`type_region_destroy` 释放 |
| 谓词子类型 | `type_create_predicate_subtype`、`type_check_predicate_subtype_value`、`type_predicate_subtype_get_base` | PVS 风格 `{x:base \| P}` 的构造与值验证 |
| 宇宙层级 | `type_get_level`、`type_check_level_validity`、`type_check_cumulative` | 类型区域（TypeRegion）层级查询与累积性校验。几何节点（GeomNode）的包含约束宇宙层级校验已下沉 L3（`graph_add_containment` 内建，见 constraint_graph 模块），不再经 type_system 暴露 |
| 等价/检查 | `type_check_equivalence`、`type_check_port_compatibility` | 类型等价与端口兼容性（返回 `TypeCheckResult`） |
| 类型推断 | `type_infer_node`、`type_infer_port`、`type_infer_by_rules`、`type_system_register_inference_rule`、`type_system_get_inference_rules`、`type_system_clear_inference_rules` | 节点/端口推断与规则表管理 |
| 变量实例化 | `type_instantiate_variable`、`type_substitute_variable` | 多态类型变量实例化与项内替换 |
| 非良基 | `type_detect_cycle`、`type_check_non_well_founded_compatibility` | 循环类型检测与非良基相容性 |
| 规范化/深拷贝 | `type_normalize`、`type_region_deep_copy/deep_free`、`type_region_foreach_child` | 规范化、快照深拷贝与子节点遍历 |
| 节点附加 | `type_attach_to_node`、`type_get_node_type`、`type_detach_node_type` | 外部映射表的附加/查询/分离 |
| 依赖类型 | `type_check_dependent` | 对 Π(x:A).B(x) 以 `SymbolicCoord` 输入值做兼容性验证 |
| 路径探索器 | `path_explorer_create/destroy/get_applicable_rules/preview_rule/apply_rule/undo/check_goal/save_path/get_step_count/get_steps/get_current` | 交互式类型重写路径搜索 |
| 重写路径 | `type_rewrite_path_create/destroy/record/replay`、`type_system_get_rewrite_path` | 重写历史记录与回放 |
| λ-项 | `lv_lambda_create_var/abs/app`、`lv_lambda_destroy/copy/to_string`、`lv_lambda_eval/eval_full/eval_steps/set_max_steps` | λ-项构造与规范序 β-归约（默认 10000 步上限） |
| Church 编码 | `lv_church_n/succ/pred/add/sub/mul/pow/div`、`lv_church_true/false/if/not/and/or/xor/iszero/leq/eq/gt`、`lv_church_nil/cons/isnil/head/tail/map/filter/foldr/foldl/length/append/pair/first/second/y_combinator/factorial/fib` | Church 数字/布尔/列表/对及其运算 |
| 图编译 | `lambda_to_graph`、`graph_to_lambda`、`beta_reduce`、`beta_reduce_n`、`beta_reduce_fully` | λ-项与约束图双向转换及图上 β-归约 |

## 工作流程

1. **系统创建与配置**：`type_system_create()` 后按需 `type_system_set_well_founded` / `type_system_set_cumulative`。几何节点的包含约束宇宙层级校验由 L3 约束图内建（`graph_add_containment`），不依赖 type_system。
2. **类型构造**：经 `type_create_*()` 工厂构造 `TypeRegion`；函数类型连接 `input_type`/`output_type`，依赖类型绑定 `param_node_id` 与 `body_type`，谓词子类型记录 `base_type` 与谓词约束。
3. **附加到节点**：`type_attach_to_node()` 将类型区域登记进外部映射表，`type_get_node_type()` 供后续查询。
4. **类型推断**：`type_infer_node()`/`type_infer_port()` 基于约束图推断；规则表路径 `type_infer_by_rules()` 依优先级取首条匹配规则创建类型并附加。
5. **等价检查**：`type_check_equivalence(type1, type2, use_rewrite)` 直接比较或经重写引擎规范化比较；`TYPE_EQUIV_NEEDS_INTERACTION` 时转路径探索器交互证明。
6. **类型检查**：`type_check_port_compatibility()` 判定端口连接合法性；`type_check_dependent()` 将输入值代入验证依赖类型；`type_check_predicate_subtype_value()` 验证节点值满足谓词。
7. **实例化与替换**：多态场景 `type_instantiate_variable()` 绑定变量、`type_substitute_variable()` 在类型内部替换变量并输出新类型。
8. **非良基处理**：`type_detect_cycle()` 检测循环、`type_check_non_well_founded_compatibility()` 判定相容，保证公理化扩展安全。
9. **路径探索**：`path_explorer_create(ts, current, target)` 起探索，经 get/preview/apply/undo 循环直到 `EXPLORER_GOAL_REACHED`，`path_explorer_save_path` 导出为重写路径。
10. **λ-演算集成**：Church 编码经 `lv_lambda_eval` 规范化求值；`lambda_to_graph()` 将 λ-项编译为函数块，`beta_reduce_fully()` 在图上迭代归约至不动点，`graph_to_lambda()` 反编译验证正确性。

## 模块关系（表格）

| 相关模块 | 文档 | 关系说明 |
| --- | --- | --- |
| 符号坐标 | 01_symbolic_coord.md | `type_check_dependent` 接收 `SymbolicCoord**` 输入值；类型规范化涉及符号表达式比较 |
| 约束图 | 02_constraint_graph.md | 类型推断以 `ConstraintGraph` 为输入；节点-类型映射只登记 ID 不改节点本体 |
| 图规范化 | 03_normalization.md | 等价检查的重写路径依赖规范化语义，两者共用 `RewriteRule` 体系 |
| 求解器 | 04_solver.md | 求解器产出的符号坐标可作为依赖类型检查与谓词子类型验证的输入值 |
| 重写引擎 | 05_rewrite.md | `TypeSystem.rewrite_rules` 直接复用重写规则实现规范化等价检查 |
| 合一系统 | 06_unify.md | 端口类型兼容性与多态实例化最终收敛到合一判定；谓词子类型经约束验证 |
| 函数块 | 07_func_block.md | 函数类型与函数块端口连接对应；`lambda_to_graph` 以函数块实现 λ 抽象 |

## 版本历史

| 版本 | 变更说明 |
| --- | --- |
| v0.1 | 初稿：整合 type_system.h / lambda_term.h / lambda_church.h / lambda_to_graph.h 的真实接口，按"宇宙层级 → 等价检查 → 推断 → 检查 → λ 集成"组织内容 |
