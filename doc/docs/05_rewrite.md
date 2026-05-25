# 图重写引擎 (Graph Rewrite Engine)

## 模块概述

图重写引擎是 Lv-00 的核心计算机制，负责将几何约束图通过重写规则逐步化简。引擎支持带变量的模式匹配、条件重写、循环检测和步数熔断，确保重写过程可控且可终止。

## 核心设计原则

1. **局部等价容忍**：匹配时基于符号坐标判等，无需全局规范化
2. **约简测度驱动**：有测度的规则优先应用，确保终止性
3. **事务性回滚**：替换产生冲突时自动回滚
4. **循环检测**：基于 WL 图核的哈希检测重写循环

## 数据类型定义

### 重写规则结构

```c
typedef struct RewriteRule {
    int id;                          // 规则唯一 ID
    char *name;                      // 规则名称
    
    // 匹配模式
    ConstraintGraph *pattern;        // 约束子图模板
    int *pattern_variable_nodes;     // 变量节点 ID（负数）
    int pattern_var_count;
    
    // 替换模式
    ConstraintGraph *replacement;    // 替换子图
    int *replacement_mappings;       // 替换节点到模式变量的映射
    
    // 控制参数
    int reduction_measure;           // 约简测度（正值=约简，0=中性，负值=扩展）
    
    // 前置条件
    RewriteCondition *condition;     // 可选的附加条件
    
    // 元数据
    char *axiom_package;             // 所属公理包
    int priority;                    // 优先级（相同测度时的排序）
} RewriteRule;
```

### 重写条件

```c
typedef struct RewriteCondition {
    ConditionType type;
    union {
        struct {
            int node1_id;            // 变量节点 ID
            int node2_id;
            ComparisonOp op;         // >, <, =, >=, <=, !=
        } algebraic;
        
        struct {
            int point_id;            // 变量节点 ID
            int region_id;
        } containment;
        
        struct {
            char *custom_predicate;  // 自定义谓词名称
            int *arg_nodes;          // 参数节点 ID
            int arg_count;
        } custom;
    } data;
} RewriteCondition;

typedef enum {
    COND_ALGEBRAIC,      // 代数比较
    COND_CONTAINMENT,    // 几何包含
    COND_CUSTOM          // 自定义条件
} ConditionType;
```

### 匹配结果

```c
typedef struct MatchResult {
    bool success;
    RewriteRule *rule;
    
    // 变量绑定
    struct {
        int pattern_var_id;     // 模式中的变量节点 ID（负数）
        int actual_node_id;     // 实际匹配的节点 ID
    } *bindings;
    int binding_count;
    
    // 匹配位置信息
    int *matched_node_ids;      // 所有匹配到的节点 ID
    int matched_count;
    int *matched_constraint_ids;
    int matched_constraint_count;
} MatchResult;
```

### 重写结果

```c
typedef struct RewriteResult {
    bool success;
    RewriteRule *applied_rule;
    
    // 替换信息
    struct {
        int old_node_id;
        int new_node_id;
    } *node_replacements;
    int replacement_count;
    
    // 新创建的节点
    int *created_node_ids;
    int created_count;
    
    // 删除的节点
    int *deleted_node_ids;
    int deleted_count;
    
    // 步数信息
    int step_number;
} RewriteResult;
```

## 1. 重写规则定义

### 规则组成

每条规则包含：

1. **匹配模式（pattern）**：一个约束子图模板
   - "变量节点"的 ID 用负数表示，象征可绑定到任意实际节点
   - 模式约束仅涉及这些负 ID 节点和可选的已有具体节点

2. **替换模式（replacement）**：一个约束子图
   - 节点 ID 引用模式中的负 ID 变量
   - 可包含新节点（用特殊标记），在规则应用时创建

3. **约简测度（reduction_measure）**：整数
   - 正值：规则应用后某个度量（节点数、约束数、代数次数）的减少量
   - 0：无测度（中性规则）
   - 负值：扩展规则（仅用户显式触发）

4. **前置条件（condition）**：可选的附加条件表达式
   - 支持代数比较（如两线段长度比 > 1）
   - 支持几何包含（某点在某区域内）
   - 在模式匹配成功后、执行替换前评估

### 规则示例

**线段合并规则**：
```
模式：A --L1--> B --L2--> C（三点共线，B 在 A、C 之间）
替换：A --L--> C（合并为一条线段）
测度：-1（节点数减少 1）
条件：无
```

**直角三角形勾股定理规则**：
```
模式：直角三角形 ABC（∠C = 90°）
替换：添加约束 AC² + BC² = AB²
测度：0（添加约束，不减少节点）
条件：angle(AC, BC) = 90°
```

## 2. 匹配算法

### VF2 子图同构改进

```c
MatchResult *find_matches(
    ConstraintGraph *graph,
    RewriteRule *rule
);
```

**改进点**：
- 对于 POINT 节点，匹配条件不要求节点 ID 相同
- 而是调用 `coord_equal()` 进行符号坐标判等
- 这使得"坐标相同但 ID 不同"的两个点被视为可匹配
- 即局部等价容忍，避免了在重写前需要调用全局规范化遍的依赖

### 匹配流程

```c
bool vf2_match_recursive(
    ConstraintGraph *graph,
    ConstraintGraph *pattern,
    MatchState *state,
    int pattern_node_idx
);
```

1. **初始化**：从模式的第一个节点开始
2. **候选选择**：在图中寻找可能匹配的节点
   - 类型相同
   - 符号坐标判等（POINT 节点）
3. **可行性检查**：
   - 检查已匹配节点的邻接关系是否与模式一致
   - 检查约束类型和参与者是否匹配
4. **递归扩展**：匹配下一个模式节点
5. **回溯**：若无法继续则回溯

### 最佳匹配选择

```c
MatchResult select_best_match(MatchResult *matches, int count);
```

若有多个不重叠的匹配位置，选择"最佳匹配"：
- 匹配子图节点数最多
- 或按特定启发式排序

## 3. 替换操作

### 替换执行

```c
RewriteResult execute_replacement(
    ConstraintGraph *graph,
    RewriteRule *rule,
    MatchResult *match
);
```

**替换流程**：

1. **实例化替换模式**：
   ```c
   ConstraintGraph *instantiate_replacement(
       RewriteRule *rule,
       MatchResult *match
   );
   ```
   - 根据模式匹配产生的绑定映射（负 ID → 实际节点 ID）
   - 复制替换模式并实例化到目标图中

2. **重连边界**：
   ```c
   void reconnect_boundaries(
       ConstraintGraph *graph,
       ConstraintGraph *replacement,
       MatchResult *match
   );
   ```
   - 替换模式中引用外部节点的连接
   - 在替换后重新定向到正确的实际节点

3. **分离旧子图**：
   ```c
   void detach_matched_subgraph(
       ConstraintGraph *graph,
       MatchResult *match
   );
   ```
   - 旧子图（被匹配的部分）被分离
   - 从约束图中移除
   - 若不再被其他部分引用则完全删除

### 事务性回滚

```c
GraphSnapshot *create_snapshot(ConstraintGraph *graph);
void restore_snapshot(ConstraintGraph *graph, GraphSnapshot *snapshot);
void destroy_snapshot(GraphSnapshot *snapshot);
```

**回滚触发条件**：
- 替换后立即验证约束图是否产生新的冲突
- 若产生冲突，回滚整个替换操作
- 恢复图状态，并记录该规则在此上下文的失败

## 4. 引擎控制循环

### 主控制循环

```c
RewriteResult rewrite_engine_run(
    ConstraintGraph *graph,
    RewriteConfig *config
);
```

**控制流程**：

1. **唤醒时机**：
   - 新约束添加后
   - 用户触发重写时

2. **规则排序**：
   ```c
   RewriteRule **sort_rules_by_measure(RewriteRule **rules, int count);
   ```
   - 按约简测度降序遍历所有可用规则
   - 相同测度则按规则注册顺序

3. **规则匹配**：
   - 每条规则尝试与全图匹配
   - 若有多个不重叠的匹配位置，选择"最佳匹配"

4. **条件评估**：
   - 若规则有前置条件，在匹配成功后评估
   - 条件不满足则跳过此匹配

5. **执行替换**：
   - 执行替换操作
   - 立即验证约束图是否产生新的冲突
   - 若产生冲突，回滚整个替换操作

6. **步数计数**：
   - 每次成功应用规则，全局步数计数器 +1
   - 达到步数上限时暂停并提示用户

### 协作协议

```
重写优先（新约束加入后先尝试所有可用规则）
    ↓
求解跟进（重写无法推进时调用符号求解器）
    ↓
冲突暴露（求解器检测到过约束冲突时高亮并暂停，请求用户介入）
```

## 5. 步数熔断

### 熔断配置

```c
typedef struct RewriteConfig {
    int max_steps;           // 全局步数上限（默认 1000）
    int step_counter;        // 当前步数计数器
    bool pause_on_limit;     // 达到上限时是否暂停
} RewriteConfig;
```

### 熔断响应

```c
typedef struct StepLimitReached {
    int current_step;
    int max_steps;
    MatchState *saved_state;     // 保存的当前匹配状态
    RewriteRule **pending_rules;  // 待应用的规则队列
} StepLimitReached;
```

**用户选项**：
- 继续（重置上限或增加上限）
- 终止当前重写批次

## 6. 图哈希循环检测

### WL 图核哈希

```c
uint64_t compute_graph_hash(ConstraintGraph *graph);
```

**算法**：
1. **初始标签**：基于节点类型和约束拓扑（忽略坐标值，基于邻居关系）
2. **WL 迭代**：执行 2 轮 WL 迭代，聚合邻居标签
3. **最终哈希**：所有节点标签的异或组合

**复杂度**：O(E)，E 为边数

### 循环检测

```c
typedef struct CycleDetector {
    uint64_t *hash_ring_buffer;   // 环形缓冲区
    int buffer_size;              // 大小（默认 16）
    int write_index;
} CycleDetector;
```

**检测流程**：
1. 每次重写步骤后计算图哈希
2. 维护最近 16 步图哈希的环形缓冲区
3. 若新哈希与缓冲区中任一值相同
4. 暂停并报告"检测到可能的重写循环"

**误报处理**：
- 即使哈希碰撞导致误报，仅触发暂停而非崩溃
- 用户可选择忽略继续

## 7. 规则管理

### 规则加载

```c
RewriteRule *load_rule_from_lvz(const char *lvz_path, const char *rule_name);
RewriteRule **load_rule_package(const char *package_path, int *count);
```

**热加载**：
- 规则可从 .lvz 规则包在运行时热加载
- 也可在运行时卸载

### 规则卸载

```c
void unload_rule(RewriteRule *rule);
```

**卸载规则不影响**：
- 已应用该规则的历史步骤
- 历史状态不可改写

### 优先级调整

```c
void set_rule_priority(RewriteRule *rule, int priority);
void reorder_rules(RewriteRule **rules, int *new_order, int count);
```

用户可在规则优先级列表中手动调整顺序。

## 8. 核心安全规则集

### 汇合性担保

核心公理包提供预验证汇合性的安全规则集：
- 该汇合性证明由外部数学审查担保
- 通过 Knuth-Bendix 完备化等方法在系统外手工完成
- 随公理包文档发布，构成系统的第二可信基

### 类型等价检查规则

对于类型等价检查最关键的规则子集（区域等价化简规则）：
- 核心公理包的安全规则集已经过外部汇合性验证
- 只要类型检查仅使用这些规则，范式唯一性被保证

## 实现文件

- **头文件**：`include/lv00/rewrite.h`
- **源文件**：`src/rewrite.c`

## 测试要点

1. 基本模式匹配（点、线段、区域）
2. 带变量模式的匹配
3. 符号坐标判等的局部等价容忍
4. 替换操作的正确性
5. 事务性回滚
6. 步数熔断的触发
7. 图哈希循环检测
8. 规则热加载和卸载
9. 大图的性能测试
