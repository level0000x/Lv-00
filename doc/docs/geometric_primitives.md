# Lv-00 几何层最小原语集

> **版本**: 1.0.0-draft  
> **日期**: 2026-05-29  
> **状态**: 设计阶段  
> **依赖**: Lv-00 v3.5.0 自举设计

---

## 概述

本文档定义 Lv-00 自举架构中几何层与 C 核交互的 **13 个最小原语**。这些原语构成几何层能够完整描述 C 核所有对外 API 接口的"汇编指令集"，是自举成功的关键基础。

**设计原则**:
- **最小完备性**: 13 个原语足以表达所有 C API 操作
- **正交性**: 每个原语职责单一，无功能重叠
- **可组合性**: 复杂操作通过原语组合实现
- **可验证性**: 每个原语有明确的数学语义

---

## 原语总览

| 编号 | 原语名称 | 对应 C API | 核心功能 |
|:---:|:---|:---|:---|
| 1 | `geo-create-node` | `graph_add_*` | 创建几何实体节点 |
| 2 | `geo-create-constraint` | `graph_add_*` | 创建约束关系 |
| 3 | `geo-solve` | `engine_solve` | 执行约束求解 |
| 4 | `geo-normalize` | `graph_normalize` | 约束图归一化 |
| 5 | `geo-rewrite` | `rewrite_with_rules` | 应用重写规则 |
| 6 | `geo-unify` | `unify_construction_with_proposition` | 合一检查 |
| 7 | `geo-pack` | `func_block_pack` | 打包函数块 |
| 8 | `geo-instantiate` | `func_block_instantiate` | 实例化函数块 |
| 9 | `geo-prove` | `proof_navigator_*` | 执行证明搜索 |
| 10 | `geo-export` | `proof_export_*` | 导出证明/构造 |
| 11 | `geo-serialize` | `graph_serialize_to_json` | 序列化图结构 |
| 12 | `geo-deserialize` | `graph_deserialize_from_json` | 反序列化图结构 |
| 13 | `geo-query` | `graph_get_*`, `type_infer_*` | 查询图状态 |

---

## 原语详细规范

### 原语 1: geo-create-node

**功能**: 在约束图中创建几何实体节点

**对应 C API**:
- `graph_add_point()`
- `graph_add_line_segment()`
- `graph_add_region()`
- `graph_add_port()`
- `graph_add_function_block()`

**几何层表示**:
```
(geo-create-node <node-type> <parameters>)
```

**参数**:
| 参数 | 类型 | 说明 |
|:---|:---|:---|
| `node-type` | Symbol | 节点类型: `point`, `line`, `region`, `port`, `function-block` |
| `parameters` | List | 类型特定的参数列表 |

**示例**:
```lisp
;; 创建点
(geo-create-node 'point '(0 0))

;; 创建线段
(geo-create-node 'line '(point-1 point-2))

;; 创建函数块
(geo-create-node 'function-block '(midpoint inputs outputs))
```

**数学语义**: 在几何本体论中创建新的几何实体实例

---

### 原语 2: geo-create-constraint

**功能**: 在几何实体之间建立约束关系

**对应 C API**:
- `graph_add_incidence()`
- `graph_add_betweenness()`
- `graph_add_intersection()`
- `graph_add_containment()`
- `graph_add_connection()`

**几何层表示**:
```
(geo-create-constraint <constraint-type> <participants>)
```

**参数**:
| 参数 | 类型 | 说明 |
|:---|:---|:---|
| `constraint-type` | Symbol | 约束类型: `incidence`, `betweenness`, `intersection`, `containment`, `connection` |
| `participants` | List | 参与约束的节点 ID 列表 |

**示例**:
```lisp
;; 关联约束：点在直线上
(geo-create-constraint 'incidence '(point-1 line-1))

;; 之间约束：点B在点A和点C之间
(geo-create-constraint 'betweenness '(point-A point-B point-C))

;; 连接约束：端口连接
(geo-create-constraint 'connection '(port-out port-in))
```

**数学语义**: 在几何实体之间建立几何关系，扩展约束图的边集

---

### 原语 3: geo-solve

**功能**: 执行约束求解，计算满足所有约束的坐标赋值

**对应 C API**:
- `engine_solve()`
- `solve_algebraic_system()`

**几何层表示**:
```
(geo-solve <graph-id> <options>)
```

**参数**:
| 参数 | 类型 | 说明 |
|:---|:---|:---|
| `graph-id` | Integer | 约束图标识符 |
| `options` | Dict | 求解选项: `method`, `timeout`, `max-iterations` |

**返回值**:
| 字段 | 类型 | 说明 |
|:---|:---|:---|
| `status` | Symbol | `success`, `failure`, `timeout`, `incomplete` |
| `solution` | Dict | 变量到符号坐标的映射 |
| `statistics` | Dict | 求解统计信息 |

**示例**:
```lisp
(geo-solve graph-1 '((method . groebner)
                     (timeout . 5000)
                     (max-iterations . 1000)))
;; => ((status . success)
;;     (solution . ((point-1 . (0 0)) (point-2 . (1 1))))
;;     (statistics . ((steps . 42) (time-ms . 123))))
```

**数学语义**: 求解约束方程组，找到满足所有几何约束的坐标配置

---

### 原语 4: geo-normalize

**功能**: 执行约束图归一化，合并等价实体，消除冗余

**对应 C API**:
- `graph_normalize()`
- `merge_line_segments()`
- `merge_regions()`

**几何层表示**:
```
(geo-normalize <graph-id> <scope-aware>)
```

**参数**:
| 参数 | 类型 | 说明 |
|:---|:---|:---|
| `graph-id` | Integer | 约束图标识符 |
| `scope-aware` | Bool | 是否考虑作用域（函数块边界） |

**返回值**:
| 字段 | 类型 | 说明 |
|:---|:---|:---|
| `merged-count` | Integer | 合并的节点数量 |
| `id-mapping` | Dict | 旧 ID 到新 ID 的映射 |
| `log` | List | 归一化操作日志 |

**示例**:
```lisp
(geo-normalize graph-1 #t)
;; => ((merged-count . 3)
;;     (id-mapping . ((old-5 . new-3) (old-7 . new-3) (old-9 . new-4)))
;;     (log . ((merged line-segment-5 line-segment-7 into line-segment-3)
;;             (merged region-9 into region-4))))
```

**数学语义**: 在约束图上执行等价类合并，保持语义等价的前提下简化图结构

---

### 原语 5: geo-rewrite

**功能**: 应用重写规则，将约束图转换为等价但可能更简单的形式

**对应 C API**:
- `rewrite_with_rules()`
- `apply_rewrite()`

**几何层表示**:
```
(geo-rewrite <graph-id> <rules> <options>)
```

**参数**:
| 参数 | 类型 | 说明 |
|:---|:---|:---|
| `graph-id` | Integer | 约束图标识符 |
| `rules` | List | 重写规则名称列表 |
| `options` | Dict | 重写选项: `step-limit`, `normalize-between-steps` |

**返回值**:
| 字段 | 类型 | 说明 |
|:---|:---|:---|
| `status` | Symbol | `success`, `step-limit-reached`, `no-match` |
| `steps-applied` | Integer | 应用的重写步数 |
| `final-graph` | Integer | 重写后的图 ID |

**示例**:
```lisp
(geo-rewrite graph-1 '(midpoint-definition perpendicular-transitivity)
             '((step-limit . 100) (normalize-between-steps . #t)))
;; => ((status . success)
;;     (steps-applied . 5)
;;     (final-graph . graph-2))
```

**数学语义**: 在约束图上应用数学等价变换，保持语义不变的前提下简化或转换图结构

---

### 原语 6: geo-unify

**功能**: 执行合一检查，验证构造是否满足命题模式

**对应 C API**:
- `unify_construction_with_proposition()`
- `unify_construction_with_proposition_detailed()`

**几何层表示**:
```
(geo-unify <construction-graph> <proposition-graph> <options>)
```

**参数**:
| 参数 | 类型 | 说明 |
|:---|:---|:---|
| `construction-graph` | Integer | 构造图标识符 |
| `proposition-graph` | Integer | 命题图标识符 |
| `options` | Dict | 合一选项: `normalize-first`, `hash-filter` |

**返回值**:
| 字段 | 类型 | 说明 |
|:---|:---|:---|
| `status` | Symbol | `success`, `type-mismatch`, `constraint-mismatch`, `port-mismatch` |
| `bindings` | Dict | 变量绑定映射 |
| `failure-info` | Dict | 失败时的详细信息 |

**示例**:
```lisp
(geo-unify construction-1 proposition-1 '((normalize-first . #t)))
;; => ((status . success)
;;     (bindings . ((var-A . point-3) (var-B . point-5))))
```

**数学语义**: 验证构造图是否是命题图的一个实例（通过变量替换）

---

### 原语 7: geo-pack

**功能**: 将约束子图打包为函数块，实现模块化封装

**对应 C API**:
- `func_block_pack()`
- `func_block_pack_ex()`

**几何层表示**:
```
(geo-pack <graph-id> <internal-nodes> <config>)
```

**参数**:
| 参数 | 类型 | 说明 |
|:---|:---|:---|
| `graph-id` | Integer | 约束图标识符 |
| `internal-nodes` | List | 内部节点 ID 列表 |
| `config` | Dict | 打包配置: `name`, `description`, `cross-boundary-action` |

**返回值**:
| 字段 | 类型 | 说明 |
|:---|:---|:---|
| `status` | Symbol | `success`, `cross-boundary-conflict`, `invalid-internal-nodes` |
| `block-id` | Integer | 新创建的函数块 ID |
| `port-mappings` | Dict | 输入/输出端口映射 |

**示例**:
```lisp
(geo-pack graph-1 '(point-3 point-4 point-5)
          '((name . "midpoint-construction")
            (description . "构造线段中点")
            (cross-boundary-action . promote)))
;; => ((status . success)
;;     (block-id . 42)
;;     (port-mappings . ((inputs . (point-1 point-2))
;;                       (outputs . (point-3)))))
```

**数学语义**: 将约束子图抽象为可复用的函数块，实现几何构造的模块化

---

### 原语 8: geo-instantiate

**功能**: 实例化函数块，将抽象函数块应用到具体参数

**对应 C API**:
- `func_block_instantiate()`
- `func_block_instantiate_capture_avoiding()`

**几何层表示**:
```
(geo-instantiate <block-id> <graph-id> <arg-mappings>)
```

**参数**:
| 参数 | 类型 | 说明 |
|:---|:---|:---|
| `block-id` | Integer | 函数块标识符 |
| `graph-id` | Integer | 目标约束图标识符 |
| `arg-mappings` | Dict | 形式参数到实际参数的映射 |

**返回值**:
| 字段 | 类型 | 说明 |
|:---|:---|:---|
| `status` | Symbol | `success`, `type-mismatch`, `arity-mismatch` |
| `new-node-ids` | List | 实例化产生的新节点 ID 列表 |
| `output-mappings` | Dict | 输出端口到新节点的映射 |

**示例**:
```lisp
(geo-instantiate 42 graph-2 '((input-0 . point-10) (input-1 . point-11)))
;; => ((status . success)
;;     (new-node-ids . (point-12 line-13))
;;     (output-mappings . ((output-0 . point-12))))
```

**数学语义**: 执行 β-归约，将函数块的形式参数替换为实际参数

---

### 原语 9: geo-prove

**功能**: 执行证明搜索，尝试构造目标命题的证明

**对应 C API**:
- `proof_navigator_create()`
- `proof_multi_strategy_execute()`
- `proof_search_with_strategy()`

**几何层表示**:
```
(geo-prove <goal-proposition> <construction-graph> <strategy> <options>)
```

**参数**:
| 参数 | 类型 | 说明 |
|:---|:---|:---|
| `goal-proposition` | Integer | 目标命题 ID |
| `construction-graph` | Integer | 构造图标识符 |
| `strategy` | Symbol | 证明策略: `auto`, `bfs`, `mcts`, `best-first`, `contradiction` |
| `options` | Dict | 证明选项: `max-steps`, `timeout`, `strategy-pipeline` |

**返回值**:
| 字段 | 类型 | 说明 |
|:---|:---|:---|
| `status` | Symbol | `proved`, `failed`, `timeout`, `incomplete` |
| `proof-tree` | Integer | 证明树标识符（成功时） |
| `trace` | List | 证明步骤追踪 |
| `statistics` | Dict | 证明统计信息 |

**示例**:
```lisp
(geo-prove proposition-5 construction-1 'auto
           '((max-steps . 1000) (timeout . 30000)))
;; => ((status . proved)
;;     (proof-tree . tree-7)
;;     (trace . ((step-1 . (apply-axiom "midpoint-definition"))
;;               (step-2 . (rewrite "perpendicular-transitivity"))
;;               (step-3 . (unify target))))
;;     (statistics . ((steps . 3) (time-ms . 456) (backtracks . 0))))
```

**数学语义**: 在证明搜索空间中探索，尝试找到从公理和构造到目标命题的推理链

---

### 原语 10: geo-export

**功能**: 导出证明或构造为外部格式

**对应 C API**:
- `proof_export_html()`
- `proof_export_latex()`
- `proof_export_coq()`
- `graph_export_dot()`

**几何层表示**:
```
(geo-export <source-id> <format> <filepath> <options>)
```

**参数**:
| 参数 | 类型 | 说明 |
|:---|:---|:---|
| `source-id` | Integer | 证明树或约束图标识符 |
| `format` | Symbol | 导出格式: `html`, `latex`, `coq`, `lean`, `dot`, `json` |
| `filepath` | String | 输出文件路径 |
| `options` | Dict | 导出选项: 格式特定的选项 |

**返回值**:
| 字段 | 类型 | 说明 |
|:---|:---|:---|
| `status` | Symbol | `success`, `io-error`, `unsupported-format` |
| `bytes-written` | Integer | 写入的字节数 |

**示例**:
```lisp
(geo-export tree-7 'coq "./proofs/midpoint_theorem.v"
          '((include-comments . #t) (tactic-style . "ssreflect")))
;; => ((status . success) (bytes-written . 2048))
```

**数学语义**: 将内部表示转换为人类可读或可机器验证的外部格式

---

### 原语 11: geo-serialize

**功能**: 将约束图序列化为持久化格式

**对应 C API**:
- `graph_serialize_to_json()`
- `graph_node_serialize_to_json()`
- `graph_constraint_serialize_to_json()`

**几何层表示**:
```
(geo-serialize <graph-id> <format>)
```

**参数**:
| 参数 | 类型 | 说明 |
|:---|:---|:---|
| `graph-id` | Integer | 约束图标识符 |
| `format` | Symbol | 序列化格式: `json`, `binary` |

**返回值**:
| 字段 | 类型 | 说明 |
|:---|:---|:---|
| `status` | Symbol | `success`, `error` |
| `data` | String/Bytes | 序列化后的数据 |
| `error-message` | String | 错误信息（失败时） |

**示例**:
```lisp
(geo-serialize graph-1 'json)
;; => ((status . success)
;;     (data . "{\"nodes\":[{\"id\":1,\"type\":\"point\",...}],...}"))
```

**数学语义**: 将约束图编码为可传输、可存储的字节序列

---

### 原语 12: geo-deserialize

**功能**: 从持久化格式反序列化约束图

**对应 C API**:
- `graph_deserialize_from_json()`

**几何层表示**:
```
(geo-deserialize <data> <format>)
```

**参数**:
| 参数 | 类型 | 说明 |
|:---|:---|:---|
| `data` | String/Bytes | 序列化数据 |
| `format` | Symbol | 数据格式: `json`, `binary` |

**返回值**:
| 字段 | 类型 | 说明 |
|:---|:---|:---|
| `status` | Symbol | `success`, `parse-error`, `version-mismatch` |
| `graph-id` | Integer | 反序列化后的图标识符 |
| `error-message` | String | 错误信息（失败时） |

**示例**:
```lisp
(geo-deserialize "{\"nodes\":...}" 'json)
;; => ((status . success) (graph-id . 5))
```

**数学语义**: 将字节序列解码为内存中的约束图结构

---

### 原语 13: geo-query

**功能**: 查询约束图的状态和属性

**对应 C API**:
- `graph_get_node()`
- `graph_get_constraint()`
- `type_infer_node()`
- `graph_get_node_count()`
- `graph_get_constraint_count()`

**几何层表示**:
```
(geo-query <graph-id> <query-type> <parameters>)
```

**参数**:
| 参数 | 类型 | 说明 |
|:---|:---|:---|
| `graph-id` | Integer | 约束图标识符 |
| `query-type` | Symbol | 查询类型: `node`, `constraint`, `type`, `count`, `stats` |
| `parameters` | Dict | 查询特定的参数 |

**返回值**: 根据查询类型返回相应的结果

**示例**:
```lisp
;; 查询节点信息
(geo-query graph-1 'node '((node-id . 5)))
;; => ((id . 5) (type . point) (coordinates . (0 0)) (constraints . (3 7)))

;; 查询类型推断
(geo-query graph-1 'type '((node-id . 5)))
;; => ((inferred-type . Point) (confidence . certain))

;; 查询图统计
(geo-query graph-1 'stats)
;; => ((node-count . 42) (constraint-count . 38) (freedom-degree . 4))
```

**数学语义**: 从约束图中提取信息，支持元编程和调试

---

## 原语组合示例

### 示例 1: 构造并验证中点

```lisp
;; 创建点和线段
(let ((A (geo-create-node 'point '(0 0)))
      (B (geo-create-node 'point '(2 0)))
      (AB (geo-create-node 'line (list A B))))
  
  ;; 添加中点约束
  (let ((M (geo-create-node 'point '())))
    (geo-create-constraint 'betweenness (list A M B))
    (geo-create-constraint 'incidence (list M AB))
    
    ;; 求解
    (geo-solve graph-1 '())
    
    ;; 验证 M 是中点
    (let ((prop (create-midpoint-proposition AB M)))
      (geo-unify graph-1 prop '()))))
```

### 示例 2: 打包并复用构造

```lisp
;; 打包中点构造为函数块
(let ((midpoint-block (geo-pack graph-1 '(point-3)
                                '((name . "midpoint")))))
  
  ;; 在新图中实例化
  (let ((C (geo-create-node 'point '(10 10)))
        (D (geo-create-node 'point '(20 10))))
    (geo-instantiate midpoint-block graph-2
                     `((input-0 . ,C) (input-1 . ,D)))))
```

### 示例 3: 证明定理

```lisp
;; 构造等腰三角形
(let ((ABC (construct-isosceles-triangle)))
  
  ;; 证明底角相等
  (let ((goal (create-equal-angles-proposition 
               (base-angle-1 ABC) (base-angle-2 ABC))))
    
    (geo-prove goal ABC 'auto
               '((max-steps . 1000)))))
```

---

## 与 C API 的映射关系

| 原语 | 主要 C API | 辅助 C API |
|:---|:---|:---|
| geo-create-node | `graph_add_point`, `graph_add_line_segment`, `graph_add_region` | `graph_add_port`, `graph_add_function_block` |
| geo-create-constraint | `graph_add_incidence`, `graph_add_betweenness` | `graph_add_intersection`, `graph_add_containment`, `graph_add_connection` |
| geo-solve | `engine_solve` | `solve_algebraic_system`, `solver_incremental_solve` |
| geo-normalize | `graph_normalize` | `merge_line_segments`, `merge_regions` |
| geo-rewrite | `rewrite_with_rules` | `apply_rewrite`, `rewrite_strategy_apply` |
| geo-unify | `unify_construction_with_proposition` | `unify_construction_with_proposition_detailed` |
| geo-pack | `func_block_pack` | `func_block_pack_ex`, `func_block_detect_cross_boundary` |
| geo-instantiate | `func_block_instantiate` | `func_block_instantiate_capture_avoiding`, `func_block_partial_apply` |
| geo-prove | `proof_navigator_create`, `proof_multi_strategy_execute` | `proof_search_with_strategy`, `lv00_proof_engine_prove` |
| geo-export | `proof_export_coq`, `proof_export_latex` | `proof_export_html`, `graph_export_dot` |
| geo-serialize | `graph_serialize_to_json` | `graph_node_serialize_to_json` |
| geo-deserialize | `graph_deserialize_from_json` | - |
| geo-query | `graph_get_node`, `graph_get_constraint` | `type_infer_node`, `graph_get_node_count` |

---

## 版本历史

| 版本 | 日期 | 变更 |
|:---|:---|:---|
| 1.0.0-draft | 2026-05-29 | 初始版本，基于自举设计文档定义 |

---

## 参考文档

- [自举架构设计](self_bootstrapping_design.md)
- [API 参考](API_REFERENCE.md)
- [函数块系统](07_func_block.md)
- [类型系统](08_type_system.md)