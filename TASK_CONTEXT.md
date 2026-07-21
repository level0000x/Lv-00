# Lv-00 任务上下文 — v1.1.0 → v1.2.0

**版本**: v1.2.0-dev | **日期**: 2026-07-21 | **阶段**: 内核完备化

---

## 一、已完成

| 任务 | 状态 |
|:---|:--:|
| v1.0→v1.1 编译器形式化验证 (R1-R6) | ✅ |
| GMP 精确计算统一 (mpq_t, 零 double/float) | ✅ |
| formal/ 零 sorry (81 .lean, 编译器 pipeline) | ✅ |
| Hilbert 公理框架 (10 文件, 含 EuclideanPlane) | ✅ |
| 版本统一 (全部 1.1.0) | ✅ |
| Phase 14: lv00_impl_upper.c ~168 桩函数 → 完整实现 | ✅ |
| Phase 15: 分散 8 文件 ~35 桩函数 → 完整实现 | ✅ |
| 交叉审计: 3 孤儿函数实现 + 编译验证 | ✅ |
| 116/116 tests passed, 0 GCC errors | ✅ |

## 二、设计文档对照差距（排除 UI）

设计文档 15 项核心特性审计 → **12 完全实现、2 部分实现、1 未实现**。
UI 系统（画布、导航器可视化、对话框等）排入后续迭代。

| # | 特性 | 状态 | 计划 |
|---|------|------|------|
| 关键对计算引擎 | 零代码 | **v1.2.0 实现** |
| 交互式类型等价探索器引擎 | 仅信号标记 | **v1.2.0 实现** |
| A/B 双轨代数数 (SymEngine/FLINT) | GMP only | 保留 B 轨接口，暂不实现 |
| 微自举 A (线段长度判等器) | 未启动 | v1.3.0 |
| 微自举 B (公式化简器) | 未启动 | v1.4.0 |

## 三、v1.2.0 任务：内核完备化

### Task A: 关键对计算引擎 (Critical Pair Engine)

**设计文档参考**: §3.6 图重写引擎、§十 关键对可视化器（核心计算部分）

**目标**: 实现关键对（critical pair）的自动计算——找到两条重写规则的叠加应用可能产生歧义的项，并对每个关键对生成两条归约路径的结果图进行比较。这是汇合性验证的核心基础设施。

**现有基础设施**:
- `RewriteRule` + `RewritePattern` + `RewriteReplacement` — [rewrite.h](file:///c:/Users/xingg/Desktop/知识体系化Wiki/Lv-00/core/include/lv00/rewrite.h)
- VF2 子图同构匹配 — `VF2State` in rewrite.h
- WL 图核哈希 — `rewrite_compute_wl_hash()` in rewrite_match.c
- 图规范化遍 — `graph_normalize()` in normalization.h
- 合一算法 — unify.c

**需实现**:

1. **`critical_pair.h`** — 新头文件
   ```c
   // 关键对结构
   typedef struct {
       RewriteRule *rule1;       // 第一条规则
       RewriteRule *rule2;       // 第二条规则
       ConstraintGraph *overlap; // 两条规则的重叠项（统一子）
       ConstraintGraph *reduced1; // 沿 rule1 归约一步的结果
       ConstraintGraph *reduced2; // 沿 rule2 归约一步的结果
       bool is_confluent;        // 该关键对是否汇合
   } CriticalPair;

   // 关键对集合
   typedef struct {
       CriticalPair *pairs;
       int pair_count;
       int capacity;
   } CriticalPairSet;
   ```

2. **`critical_pair_compute_all(RewriteRule **rules, int rule_count)`**
   - 对规则集两两计算所有关键对
   - 对每条规则也计算自叠加（一条规则的两个重叠应用）
   - 使用 VF2 匹配找到所有可能的叠加位置（模式图重叠）

3. **`critical_pair_compare(CriticalPair *cp)`**
   - 对每个关键对，分别沿两条规则归约一步
   - 比较两个归约结果通过图规范化遍 + 合一检查
   - 若合一成功 → 该关键对汇合
   - 若合一失败 → 记录不匹配的节点/约束列表（供审查）

4. **`critical_pair_export_canonical(CriticalPair *cp)`**
   - 导出关键对的两个归约结果为标准化邻接表文本文件
   - 格式：节点类型 + 约束类型 + 符号坐标规范表达式
   - 可供外部工具（nauty/Traces）独立验证
   - 不依赖 Lv-00 的任何实现

5. **`critical_pair_set_destroy(CriticalPairSet *set)`** — 资源释放

**文件**:
- `core/include/lv00/critical_pair.h` — 新建
- `core/src/layer4_reasoning/rewrite/critical_pair.c` — 新建
- 注册到 CMakeLists.txt

**验收**:
- 对欧氏几何公理包的安全规则集计算所有关键对
- 对已知汇合规则集，所有关键对应标记为 `is_confluent = true`
- 对故意构造的非汇合规则对，应正确检测并列出不匹配部分
- 导出文件格式可被标准图同构工具解析

---

### Task B: 交互式类型等价探索器引擎

**设计文档参考**: §3.6 "用户交互式路径探索"、§三 "类型等价检查"

**目标**: 实现交互式类型等价证明的核心引擎——当自动重写无法将两个类型区域归一到同一范式时，不立即判定"不等价"，而是提供路径管理、回溯、替代规则尝试的基础设施。

**现有基础设施**:
- `TYPE_EQUIV_NEEDS_INTERACTION` 枚举值 — [type_system.h:179](file:///c:/Users/xingg/Desktop/知识体系化Wiki/Lv-00/core/include/lv00/type_system.h#L179)
- `TypeRewritePath` + `TypeRewriteStep` — [type_system.h:219-268](file:///c:/Users/xingg/Desktop/知识体系化Wiki/Lv-00/core/include/lv00/type_system.h#L219-L268)
- `TypeSystem.rewrite_rules[]` — [type_system.h:301](file:///c:/Users/xingg/Desktop/知识体系化Wiki/Lv-00/core/include/lv00/type_system.h#L301)
- `type_check_equivalence()` — type_system.c

**需实现**:

1. **`type_equiv_explorer.h`** — 新头文件
   ```c
   // 探索会话状态
   typedef struct {
       TypeRegion *left;          // 左侧类型区域
       TypeRegion *right;         // 右侧类型区域
       TypeRewritePath *path;     // 当前重写路径
       TypeRewritePath **branches; // 已探索的分支路径
       int branch_count;
       int current_branch;
       bool proved_equivalent;    // 是否已证明等价
       int max_depth;             // 最大探索深度
       bool exhausted;            // 是否已穷尽所有路径
   } TypeEquivSession;

   // 可用操作
   typedef enum {
       TYPEEXPLORE_APPLY_RULE,   // 对左侧或右侧应用一条重写规则
       TYPEEXPLORE_BACKTRACK,    // 回退到上一个分支点
       TYPEEXPLORE_BRANCH,       // 保存当前状态并创建新分支
       TYPEEXPLORE_TRY_UNIFY,    // 尝试合一当前左右两侧
   } TypeExploreAction;
   ```

2. **`type_equiv_explore_create(TypeRegion *left, TypeRegion *right, TypeSystem *ts)`**
   - 创建探索会话
   - 初始化根路径（左右两侧原始类型作为起点）

3. **`type_equiv_explore_try_rule(TypeEquivSession *s, RewriteRule *rule, bool apply_to_left)`**
   - 对左侧或右侧应用指定规则
   - 若规则有多种匹配方式 → 自动创建分支
   - 每步记录到 `TypeRewritePath`

4. **`type_equiv_explore_backtrack(TypeEquivSession *s)`**
   - 撤销最近一步操作
   - 恢复到上一个分支点的状态
   - 若当前分支已穷尽 → 切换到下一个未探索分支

5. **`type_equiv_explore_check(TypeEquivSession *s)`**
   - 对当前路径的左右两侧执行合一检查
   - 成功 → 标记 `proved_equivalent = true`
   - 失败 → 返回不匹配详情

6. **`type_equiv_explore_search(TypeEquivSession *s, int max_steps)`**
   - 自动搜索模式：BFS 遍历规则应用组合
   - 在 max_steps 步内尝试找到合一路径
   - 搜索策略对比：
     - 规则按约简测度（reduction_measure）优先级排序
     - 优先应用能使两侧更接近的规则（基于 WL 哈希距离启发式）

7. **`type_equiv_explore_get_path(TypeEquivSession *s)`**
   - 返回当前会话的证明路径（可序列化的步骤列表）

8. **`type_equiv_explore_destroy(TypeEquivSession *s)`** — 资源释放

**文件**:
- `core/include/lv00/type_equiv_explorer.h` — 新建
- `core/src/layer4_reasoning/type_logic/type_equiv_explorer.c` — 新建
- 注册到 CMakeLists.txt

**验收**:
- 对已知等价的一对类型区域（如 `A×(B×C)` 和 `(A×B)×C`），在结合律重写规则存在下，自动搜索找到合一路径
- 对已知不等价的类型对，在步数上限内报告"未能证明等价"
- 回溯 + 分支管理：创建 3 个分支各走一步后回退，验证状态恢复正确
- 与现有 `type_check_equivalence()` 集成：当返回 `TYPE_EQUIV_NEEDS_INTERACTION` 时自动进入探索模式

---

### Task C: 集成与收尾

**目标**: 将新模块接入构建系统和现有调用链。

**需实现**:
1. CMakeLists.txt 更新 — 添加新 .c 文件到对应 library target
2. `type_check_equivalence()` 集成 — 当返回 `TYPE_EQUIV_NEEDS_INTERACTION` 时调用 `type_equiv_explore_search()`
3. 单元测试 — `test_critical_pair.c` 和 `test_type_equiv_explorer.c`

**文件**:
- `core/CMakeLists.txt` — 修改
- `core/src/layer4_reasoning/type_logic/type_system.c` — 修改
- `test/c/test_critical_pair.c` — 新建
- `test/c/test_type_equiv_explorer.c` — 新建

---

## 四、远期路线图（v1.3.0+）

| 版本 | 内容 | 预估 |
|:---|:---|:---|
| v1.3.0 | 微自举 A: 线段长度判等器 (< 100 nodes) | 几何可表达性验证 |
| v1.4.0 | 微自举 B: 公式化简器 (< 500 nodes) | 几何证明能力验证 |
| v1.5.0 | λ-演算几何原型 (β-归约, Y 组合子) | 自举可行性验证 |
| v2.0.0 | 命题逻辑验证器自举 | 首发自举 |
| — | UI 系统 (画布、导航器、对话框等) | 独立迭代 |

## 五、当前指标

| 指标 | 值 |
|:---|:---|
| 测试总数 | 116 |
| 通过 | 116 (100%) |
| 构建状态 | 编译通过 |
| GCC errors | 0 |
| 孤儿函数 | 0 |
| 桩函数 | 0 |

## 六、下一步提示词

```
按 TASK_CONTEXT.md v1.2.0 计划开始实现 Task A（关键对计算引擎）：
1. 创建 core/include/lv00/critical_pair.h
2. 创建 core/src/layer4_reasoning/rewrite/critical_pair.c
3. 更新 CMakeLists.txt
4. 编译 + 测试验证
```
