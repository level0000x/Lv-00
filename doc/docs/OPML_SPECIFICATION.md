# OPML: Open Proof Markup Language

**版本**: 1.0.0-draft
**日期**: 2026-06-04
**状态**: 设计阶段

## 1. 概述

OPML (Open Proof Markup Language) 是 Lv-00 项目的开放证明交换格式，旨在实现不同定理证明器之间的证明互操作。支持 Lv-00、Lean 4、Coq、Isabelle/HOL、HOL4、Agda 之间的证明导入导出。

### 1.1 设计目标

- **互操作性**: 支持 6+ 种证明系统的双向转换
- **可扩展性**: 通过命名空间支持自定义扩展
- **可验证性**: 内置证明步骤的完整性校验
- **人类可读**: 基于 JSON 的文本格式，支持注释
- **紧凑性**: 支持二进制序列化（MessagePack）

### 1.2 与现有格式的关系

| 格式 | 用途 | OPML 的关系 |
|------|------|-------------|
| Lean 4 `.lean` | Lean 4 源文件 | OPML 可导入/导出 Lean 命题 |
| Coq `.v` | Coq 源文件 | OPML 可导入/导出 Coq 证明项 |
| TPTP `.p` | 一阶逻辑问题集 | OPML 几何层可映射到 TPTP |
| arXiv LaTeX | 论文排版 | OPML proof 节可导出 LaTeX |
| LVZ | Lv-00 内部序列化 | OPML 是 LVZ 的公开子集 |

## 2. 格式规范

### 2.1 顶层结构

```json
{
  "opml_version": "1.0.0",
  "source_system": "lv",
  "target_systems": ["lean4", "coq", "isabelle"],
  "metadata": {
    "title": "三角形内角和定理",
    "author": "Lv-00",
    "date": "2026-06-04",
    "uuid": "550e8400-e29b-41d4-a716-446655440000"
  },
  "theory": { ... },
  "proof": { ... },
  "extensions": { ... }
}
```

### 2.2 Theory 节（理论定义）

```json
{
  "theory": {
    "language": "hilbert_geometry",
    "primitives": [
      { "name": "Point", "kind": "type", "description": "几何点" },
      { "name": "Line", "kind": "type", "description": "直线" },
      { "name": "Plane", "kind": "type", "description": "平面" },
      { "name": "dist", "kind": "function", "arity": 2, "codomain": "Real", "description": "距离" },
      { "name": "between", "kind": "predicate", "arity": 3, "description": "介于关系" },
      { "name": "congr", "kind": "predicate", "arity": 4, "description": "全等" }
    ],
    "axioms": [
      {
        "id": "I1",
        "name": "unique_line",
        "group": "incidence",
        "statement": "∀ p q : Point, p ≠ q → ∃! l : Line, lies_on(p, l) ∧ lies_on(q, l)",
        "dependencies": []
      },
      {
        "id": "B1",
        "name": "between_collinear",
        "group": "betweenness",
        "statement": "∀ p q r : Point, B(p,q,r) → p ≠ q ∧ q ≠ r ∧ p ≠ r",
        "dependencies": ["I1"]
      }
    ],
    "definitions": [
      {
        "id": "D1",
        "name": "triangle",
        "statement": "triangle(A,B,C) := A ≠ B ∧ B ≠ C ∧ A ≠ C ∧ ¬collinear(A,B,C)",
        "dependencies": ["I1", "I3"]
      }
    ],
    "theorems": [
      {
        "id": "T1",
        "name": "triangle_inequality",
        "statement": "∀ A B C : Point, dist(A,C) ≤ dist(A,B) + dist(B,C)",
        "dependencies": ["D1", "B1"],
        "proof_ref": "proof_001"
      }
    ]
  }
}
```

### 2.3 Proof 节（证明步骤）

```json
{
  "proof": {
    "id": "proof_001",
    "theorem_ref": "T1",
    "method": "forward_chain",
    "steps": [
      {
        "id": 1,
        "tactic": "assume",
        "target": "A B C : Point",
        "premises": [],
        "conclusion": null,
        "comment": "假设任意三点"
      },
      {
        "id": 2,
        "tactic": "apply",
        "target": "dist_triangle A B C",
        "premises": [1],
        "conclusion": "dist(A,C) ≤ dist(A,B) + dist(B,C)",
        "justification": "MetricSpace.dist_triangle",
        "comment": "应用度量空间三角不等式"
      }
    ],
    "qed": 2
  }
}
```

### 2.4 Extensions 节（扩展）

```json
{
  "extensions": {
    "lv_specific": {
      "constraint_graph": { ... },
      "symbolic_coords": { ... },
      "func_blocks": [ ... ]
    },
    "lean4_specific": {
      "tactic_script": "by { ... }",
      "sorry_count": 0
    }
  }
}
```

## 3. 系统映射表

### 3.1 类型映射

| OPML 类型 | Lean 4 | Coq | Isabelle/HOL |
|----------|--------|-----|--------------|
| `Point` | `Point α` | `point` | `'a point` |
| `Line` | `Line α` | `line` | `'a line` |
| `Prop` | `Prop` | `Prop` | `bool` |
| `Real` | `ℝ` | `R` | `real` |

### 3.2 Tactic 映射

| OPML Tactic | Lean 4 | Coq | Isabelle |
|-------------|--------|-----|----------|
| `assume` | `intro` | `intros` | `fix/assume` |
| `apply` | `apply` | `apply` | `apply` |
| `induction` | `induction` | `induction` | `induct` |
| `case_split` | `cases` | `case` | `cases` |
| `contradiction` | `by_contra` | `contra` | `contradiction` |
| `algebra` | `ring` | `lra` | `auto` |
| `rewrite` | `rw` | `rewrite` | `rewrite` |

## 4. 序列化格式

### 4.1 JSON（默认，人类可读）

Content-Type: `application/vnd.opml+json`

### 4.2 MessagePack（紧凑二进制）

Content-Type: `application/vnd.opml+msgpack`

### 4.3 LVZ（Lv-00 内部格式）

Content-Type: `application/vnd.lv.lvz`

## 5. 验证规则

### 5.1 结构验证

- 所有 `dependencies` 引用的 ID 必须在 `axioms`/`definitions`/`theorems` 中存在
- 证明步骤的 `premises` 必须引用已存在的步骤 ID
- `qed` 必须指向最后一个步骤

### 5.2 语义验证

- 公理组内无循环依赖
- 证明步骤的结论类型必须与目标定理一致
- 所有 `sorry` 必须标记（不允许静默跳过）

## 6. 版本策略

- 主版本号变更：不兼容的格式变更
- 次版本号变更：向后兼容的新特性
- 修订号变更：Bug 修复

## 7. 示例

### 7.1 完整示例：三角形内角和

```json
{
  "opml_version": "1.0.0",
  "source_system": "lv",
  "metadata": {
    "title": "三角形内角和等于 180 度",
    "author": "Lv-00",
    "date": "2026-06-04"
  },
  "theory": {
    "language": "hilbert_geometry",
    "primitives": [
      { "name": "Point", "kind": "type" },
      { "name": "Line", "kind": "type" },
      { "name": "Angle", "kind": "type" },
      { "name": "measure", "kind": "function", "arity": 1, "codomain": "Real" }
    ],
    "axioms": [
      { "id": "P1", "name": "parallel_postulate", "group": "parallel",
        "statement": "通过直线外一点恰有一条平行线" },
      { "id": "A1", "name": "archimedes", "group": "continuity",
        "statement": "阿基米德公理" }
    ],
    "theorems": [
      { "id": "T_angle_sum", "name": "angle_sum_180",
        "statement": "∀ triangle ABC, measure(∠A) + measure(∠B) + measure(∠C) = 180°",
        "dependencies": ["P1", "A1"],
        "proof_ref": "proof_angle_sum" }
    ]
  },
  "proof": {
    "id": "proof_angle_sum",
    "theorem_ref": "T_angle_sum",
    "method": "decomposition",
    "steps": [
      { "id": 1, "tactic": "assume", "target": "triangle ABC" },
      { "id": 2, "tactic": "construct", "target": "line through A parallel to BC", "premises": [1], "justification": "P1" },
      { "id": 3, "tactic": "derive", "target": "∠BA alternate ∠BAC", "premises": [2], "justification": "alternate_interior" },
      { "id": 4, "tactic": "derive", "target": "∠CA alternate ∠BCA", "premises": [2], "justification": "alternate_interior" },
      { "id": 5, "tactic": "algebra", "target": "∠A + ∠B + ∠C = 180°", "premises": [3, 4], "justification": "linear_pair_axiom" }
    ],
    "qed": 5
  }
}
```
