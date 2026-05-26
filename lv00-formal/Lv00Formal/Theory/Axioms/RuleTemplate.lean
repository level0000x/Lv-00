/-
Lv-00 自有理论核心：可执行规则模板
/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePrem/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph
/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace RuleTemplate

open/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace RuleTemplate

open Ontology
open Predicates
open Constraint

/-- 规则类型，对应 C/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace RuleTemplate

open Ontology
open Predicates
open Constraint

/-- 规则类型，对应 C 侧 `Lv00RuleType`。 -/
inductive RuleType where
  | inference/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace RuleTemplate

open Ontology
open Predicates
open Constraint

/-- 规则类型，对应 C 侧 `Lv00RuleType`。 -/
inductive RuleType where
  | inference
  | rewrite
  | axiom
  | definition
  | theorem
  |/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace RuleTemplate

open Ontology
open Predicates
open Constraint

/-- 规则类型，对应 C 侧 `Lv00RuleType`。 -/
inductive RuleType where
  | inference
  | rewrite
  | axiom
  | definition
  | theorem
  | lemma
  | tactic
  | constructor
/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace RuleTemplate

open Ontology
open Predicates
open Constraint

/-- 规则类型，对应 C 侧 `Lv00RuleType`。 -/
inductive RuleType where
  | inference
  | rewrite
  | axiom
  | definition
  | theorem
  | lemma
  | tactic
  | constructor
  deriving DecidableEq, Repr

/-- 规则状态，对应 C 侧/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace RuleTemplate

open Ontology
open Predicates
open Constraint

/-- 规则类型，对应 C 侧 `Lv00RuleType`。 -/
inductive RuleType where
  | inference
  | rewrite
  | axiom
  | definition
  | theorem
  | lemma
  | tactic
  | constructor
  deriving DecidableEq, Repr

/-- 规则状态，对应 C 侧 `Lv00RuleStatus`。 -/
inductive RuleStatus where
  | disabled
/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace RuleTemplate

open Ontology
open Predicates
open Constraint

/-- 规则类型，对应 C 侧 `Lv00RuleType`。 -/
inductive RuleType where
  | inference
  | rewrite
  | axiom
  | definition
  | theorem
  | lemma
  | tactic
  | constructor
  deriving DecidableEq, Repr

/-- 规则状态，对应 C 侧 `Lv00RuleStatus`。 -/
inductive RuleStatus where
  | disabled
  | enabled
  | deprecated
  | experimental
  deriving DecidableEq, Repr

/-- 规则优先级。/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace RuleTemplate

open Ontology
open Predicates
open Constraint

/-- 规则类型，对应 C 侧 `Lv00RuleType`。 -/
inductive RuleType where
  | inference
  | rewrite
  | axiom
  | definition
  | theorem
  | lemma
  | tactic
  | constructor
  deriving DecidableEq, Repr

/-- 规则状态，对应 C 侧 `Lv00RuleStatus`。 -/
inductive RuleStatus where
  | disabled
  | enabled
  | deprecated
  | experimental
  deriving DecidableEq, Repr

/-- 规则优先级。C 侧使用整数，这里抽象为/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace RuleTemplate

open Ontology
open Predicates
open Constraint

/-- 规则类型，对应 C 侧 `Lv00RuleType`。 -/
inductive RuleType where
  | inference
  | rewrite
  | axiom
  | definition
  | theorem
  | lemma
  | tactic
  | constructor
  deriving DecidableEq, Repr

/-- 规则状态，对应 C 侧 `Lv00RuleStatus`。 -/
inductive RuleStatus where
  | disabled
  | enabled
  | deprecated
  | experimental
  deriving DecidableEq, Repr

/-- 规则优先级。C 侧使用整数，这里抽象为五档。 -/
inductive RulePriority where
  | lowest
  | low
/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace RuleTemplate

open Ontology
open Predicates
open Constraint

/-- 规则类型，对应 C 侧 `Lv00RuleType`。 -/
inductive RuleType where
  | inference
  | rewrite
  | axiom
  | definition
  | theorem
  | lemma
  | tactic
  | constructor
  deriving DecidableEq, Repr

/-- 规则状态，对应 C 侧 `Lv00RuleStatus`。 -/
inductive RuleStatus where
  | disabled
  | enabled
  | deprecated
  | experimental
  deriving DecidableEq, Repr

/-- 规则优先级。C 侧使用整数，这里抽象为五档。 -/
inductive RulePriority where
  | lowest
  | low
  | normal
  | high
  | highest
  deriving DecidableEq, Repr/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace RuleTemplate

open Ontology
open Predicates
open Constraint

/-- 规则类型，对应 C 侧 `Lv00RuleType`。 -/
inductive RuleType where
  | inference
  | rewrite
  | axiom
  | definition
  | theorem
  | lemma
  | tactic
  | constructor
  deriving DecidableEq, Repr

/-- 规则状态，对应 C 侧 `Lv00RuleStatus`。 -/
inductive RuleStatus where
  | disabled
  | enabled
  | deprecated
  | experimental
  deriving DecidableEq, Repr

/-- 规则优先级。C 侧使用整数，这里抽象为五档。 -/
inductive RulePriority where
  | lowest
  | low
  | normal
  | high
  | highest
  deriving DecidableEq, Repr

/-- 规则条件类型，对应/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace RuleTemplate

open Ontology
open Predicates
open Constraint

/-- 规则类型，对应 C 侧 `Lv00RuleType`。 -/
inductive RuleType where
  | inference
  | rewrite
  | axiom
  | definition
  | theorem
  | lemma
  | tactic
  | constructor
  deriving DecidableEq, Repr

/-- 规则状态，对应 C 侧 `Lv00RuleStatus`。 -/
inductive RuleStatus where
  | disabled
  | enabled
  | deprecated
  | experimental
  deriving DecidableEq, Repr

/-- 规则优先级。C 侧使用整数，这里抽象为五档。 -/
inductive RulePriority where
  | lowest
  | low
  | normal
  | high
  | highest
  deriving DecidableEq, Repr

/-- 规则条件类型，对应 C 侧 `Lv00ConditionType`。/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace RuleTemplate

open Ontology
open Predicates
open Constraint

/-- 规则类型，对应 C 侧 `Lv00RuleType`。 -/
inductive RuleType where
  | inference
  | rewrite
  | axiom
  | definition
  | theorem
  | lemma
  | tactic
  | constructor
  deriving DecidableEq, Repr

/-- 规则状态，对应 C 侧 `Lv00RuleStatus`。 -/
inductive RuleStatus where
  | disabled
  | enabled
  | deprecated
  | experimental
  deriving DecidableEq, Repr

/-- 规则优先级。C 侧使用整数，这里抽象为五档。 -/
inductive RulePriority where
  | lowest
  | low
  | normal
  | high
  | highest
  deriving DecidableEq, Repr

/-- 规则条件类型，对应 C 侧 `Lv00ConditionType`。 -/
inductive ConditionType where
  | patternMatch
  | typeCheck
  | valueCompare
  | existsCheck
  | forallCheck
  | custom
  deriving DecidableEq, Repr

/-- 规则变量：名称、类型约束、可选绑定对象。 -/
structure RuleVariable where
  name : String
  typeName : String
  bound : Option LvObj := none
  deriving Repr

/-- 规则条件：目前保留模式、变量名和条件类型，后续可细化为可判定谓词。 -/
structure RuleCondition where
  type : ConditionType
  pattern : String
  variable : String
  deriving Repr

/-- 规则前提：一个谓词模式及其附加条件。 -/
structure RulePremise where
  predicate : PrimPred
  conditions : List RuleCondition := []
  optional : Bool := false
  deriving Repr

/-- 规则结论：可被约束图吸收的本原谓词，附带证明理由。 -/
structure RuleConclusion where
  predicate : PrimPred
  justification : String := ""
  deriving Repr

/-- 可执行规则模板。 -/
structure ExecutableRule where
  id : Nat
  baseKind : BaseAxiomKind
  name : String
  description : String
  type : RuleType
  status : RuleStatus
  variables : List RuleVariable
  premises : List RulePremise
  conclusions : List RuleConclusion
  priority : RulePriority := RulePriority.normal
  dependencies : List Nat := []
  packageName : String := ""
  deriving Repr

/-- 前提良构性。 -/
def WellFormedPremise (p : RulePremise) : Prop :=
  WellFormedPred p.predicate/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace RuleTemplate

open Ontology
open Predicates
open Constraint

/-- 规则类型，对应 C 侧 `Lv00RuleType`。 -/
inductive RuleType where
  | inference
  | rewrite
  | axiom
  | definition
  | theorem
  | lemma
  | tactic
  | constructor
  deriving DecidableEq, Repr

/-- 规则状态，对应 C 侧 `Lv00RuleStatus`。 -/
inductive RuleStatus where
  | disabled
  | enabled
  | deprecated
  | experimental
  deriving DecidableEq, Repr

/-- 规则优先级。C 侧使用整数，这里抽象为五档。 -/
inductive RulePriority where
  | lowest
  | low
  | normal
  | high
  | highest
  deriving DecidableEq, Repr

/-- 规则条件类型，对应 C 侧 `Lv00ConditionType`。 -/
inductive ConditionType where
  | patternMatch
  | typeCheck
  | valueCompare
  | existsCheck
  | forallCheck
  | custom
  deriving DecidableEq, Repr

/-- 规则变量：名称、类型约束、可选绑定对象。 -/
structure RuleVariable where
  name : String
  typeName : String
  bound : Option LvObj := none
  deriving Repr

/-- 规则条件：目前保留模式、变量名和条件类型，后续可细化为可判定谓词。 -/
structure RuleCondition where
  type : ConditionType
  pattern : String
  variable : String
  deriving Repr

/-- 规则前提：一个谓词模式及其附加条件。 -/
structure RulePremise where
  predicate : PrimPred
  conditions : List RuleCondition := []
  optional : Bool := false
  deriving Repr

/-- 规则结论：可被约束图吸收的本原谓词，附带证明理由。 -/
structure RuleConclusion where
  predicate : PrimPred
  justification : String := ""
  deriving Repr

/-- 可执行规则模板。 -/
structure ExecutableRule where
  id : Nat
  baseKind : BaseAxiomKind
  name : String
  description : String
  type : RuleType
  status : RuleStatus
  variables : List RuleVariable
  premises : List RulePremise
  conclusions : List RuleConclusion
  priority : RulePriority := RulePriority.normal
  dependencies : List Nat := []
  packageName : String := ""
  deriving Repr

/-- 前提良构性。 -/
def WellFormedPremise (p : RulePremise) : Prop :=
  WellFormedPred p.predicate

/-- 结论良构性。 -/
def WellFormedConclusion (c : RuleConclusion) : Prop :=
  WellFormedPred c.predicate

//-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace RuleTemplate

open Ontology
open Predicates
open Constraint

/-- 规则类型，对应 C 侧 `Lv00RuleType`。 -/
inductive RuleType where
  | inference
  | rewrite
  | axiom
  | definition
  | theorem
  | lemma
  | tactic
  | constructor
  deriving DecidableEq, Repr

/-- 规则状态，对应 C 侧 `Lv00RuleStatus`。 -/
inductive RuleStatus where
  | disabled
  | enabled
  | deprecated
  | experimental
  deriving DecidableEq, Repr

/-- 规则优先级。C 侧使用整数，这里抽象为五档。 -/
inductive RulePriority where
  | lowest
  | low
  | normal
  | high
  | highest
  deriving DecidableEq, Repr

/-- 规则条件类型，对应 C 侧 `Lv00ConditionType`。 -/
inductive ConditionType where
  | patternMatch
  | typeCheck
  | valueCompare
  | existsCheck
  | forallCheck
  | custom
  deriving DecidableEq, Repr

/-- 规则变量：名称、类型约束、可选绑定对象。 -/
structure RuleVariable where
  name : String
  typeName : String
  bound : Option LvObj := none
  deriving Repr

/-- 规则条件：目前保留模式、变量名和条件类型，后续可细化为可判定谓词。 -/
structure RuleCondition where
  type : ConditionType
  pattern : String
  variable : String
  deriving Repr

/-- 规则前提：一个谓词模式及其附加条件。 -/
structure RulePremise where
  predicate : PrimPred
  conditions : List RuleCondition := []
  optional : Bool := false
  deriving Repr

/-- 规则结论：可被约束图吸收的本原谓词，附带证明理由。 -/
structure RuleConclusion where
  predicate : PrimPred
  justification : String := ""
  deriving Repr

/-- 可执行规则模板。 -/
structure ExecutableRule where
  id : Nat
  baseKind : BaseAxiomKind
  name : String
  description : String
  type : RuleType
  status : RuleStatus
  variables : List RuleVariable
  premises : List RulePremise
  conclusions : List RuleConclusion
  priority : RulePriority := RulePriority.normal
  dependencies : List Nat := []
  packageName : String := ""
  deriving Repr

/-- 前提良构性。 -/
def WellFormedPremise (p : RulePremise) : Prop :=
  WellFormedPred p.predicate

/-- 结论良构性。 -/
def WellFormedConclusion (c : RuleConclusion) : Prop :=
  WellFormedPred c.predicate

/-- 可执行规则良构性。 -/
def WellFormedExecutableRule (r : Execut/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace RuleTemplate

open Ontology
open Predicates
open Constraint

/-- 规则类型，对应 C 侧 `Lv00RuleType`。 -/
inductive RuleType where
  | inference
  | rewrite
  | axiom
  | definition
  | theorem
  | lemma
  | tactic
  | constructor
  deriving DecidableEq, Repr

/-- 规则状态，对应 C 侧 `Lv00RuleStatus`。 -/
inductive RuleStatus where
  | disabled
  | enabled
  | deprecated
  | experimental
  deriving DecidableEq, Repr

/-- 规则优先级。C 侧使用整数，这里抽象为五档。 -/
inductive RulePriority where
  | lowest
  | low
  | normal
  | high
  | highest
  deriving DecidableEq, Repr

/-- 规则条件类型，对应 C 侧 `Lv00ConditionType`。 -/
inductive ConditionType where
  | patternMatch
  | typeCheck
  | valueCompare
  | existsCheck
  | forallCheck
  | custom
  deriving DecidableEq, Repr

/-- 规则变量：名称、类型约束、可选绑定对象。 -/
structure RuleVariable where
  name : String
  typeName : String
  bound : Option LvObj := none
  deriving Repr

/-- 规则条件：目前保留模式、变量名和条件类型，后续可细化为可判定谓词。 -/
structure RuleCondition where
  type : ConditionType
  pattern : String
  variable : String
  deriving Repr

/-- 规则前提：一个谓词模式及其附加条件。 -/
structure RulePremise where
  predicate : PrimPred
  conditions : List RuleCondition := []
  optional : Bool := false
  deriving Repr

/-- 规则结论：可被约束图吸收的本原谓词，附带证明理由。 -/
structure RuleConclusion where
  predicate : PrimPred
  justification : String := ""
  deriving Repr

/-- 可执行规则模板。 -/
structure ExecutableRule where
  id : Nat
  baseKind : BaseAxiomKind
  name : String
  description : String
  type : RuleType
  status : RuleStatus
  variables : List RuleVariable
  premises : List RulePremise
  conclusions : List RuleConclusion
  priority : RulePriority := RulePriority.normal
  dependencies : List Nat := []
  packageName : String := ""
  deriving Repr

/-- 前提良构性。 -/
def WellFormedPremise (p : RulePremise) : Prop :=
  WellFormedPred p.predicate

/-- 结论良构性。 -/
def WellFormedConclusion (c : RuleConclusion) : Prop :=
  WellFormedPred c.predicate

/-- 可执行规则良构性。 -/
def WellFormedExecutableRule (r : ExecutableRule) : Prop :=
  r/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace RuleTemplate

open Ontology
open Predicates
open Constraint

/-- 规则类型，对应 C 侧 `Lv00RuleType`。 -/
inductive RuleType where
  | inference
  | rewrite
  | axiom
  | definition
  | theorem
  | lemma
  | tactic
  | constructor
  deriving DecidableEq, Repr

/-- 规则状态，对应 C 侧 `Lv00RuleStatus`。 -/
inductive RuleStatus where
  | disabled
  | enabled
  | deprecated
  | experimental
  deriving DecidableEq, Repr

/-- 规则优先级。C 侧使用整数，这里抽象为五档。 -/
inductive RulePriority where
  | lowest
  | low
  | normal
  | high
  | highest
  deriving DecidableEq, Repr

/-- 规则条件类型，对应 C 侧 `Lv00ConditionType`。 -/
inductive ConditionType where
  | patternMatch
  | typeCheck
  | valueCompare
  | existsCheck
  | forallCheck
  | custom
  deriving DecidableEq, Repr

/-- 规则变量：名称、类型约束、可选绑定对象。 -/
structure RuleVariable where
  name : String
  typeName : String
  bound : Option LvObj := none
  deriving Repr

/-- 规则条件：目前保留模式、变量名和条件类型，后续可细化为可判定谓词。 -/
structure RuleCondition where
  type : ConditionType
  pattern : String
  variable : String
  deriving Repr

/-- 规则前提：一个谓词模式及其附加条件。 -/
structure RulePremise where
  predicate : PrimPred
  conditions : List RuleCondition := []
  optional : Bool := false
  deriving Repr

/-- 规则结论：可被约束图吸收的本原谓词，附带证明理由。 -/
structure RuleConclusion where
  predicate : PrimPred
  justification : String := ""
  deriving Repr

/-- 可执行规则模板。 -/
structure ExecutableRule where
  id : Nat
  baseKind : BaseAxiomKind
  name : String
  description : String
  type : RuleType
  status : RuleStatus
  variables : List RuleVariable
  premises : List RulePremise
  conclusions : List RuleConclusion
  priority : RulePriority := RulePriority.normal
  dependencies : List Nat := []
  packageName : String := ""
  deriving Repr

/-- 前提良构性。 -/
def WellFormedPremise (p : RulePremise) : Prop :=
  WellFormedPred p.predicate

/-- 结论良构性。 -/
def WellFormedConclusion (c : RuleConclusion) : Prop :=
  WellFormedPred c.predicate

/-- 可执行规则良构性。 -/
def WellFormedExecutableRule (r : ExecutableRule) : Prop :=
  r.baseKind ∈ canonicalKinds ∧
  (/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace RuleTemplate

open Ontology
open Predicates
open Constraint

/-- 规则类型，对应 C 侧 `Lv00RuleType`。 -/
inductive RuleType where
  | inference
  | rewrite
  | axiom
  | definition
  | theorem
  | lemma
  | tactic
  | constructor
  deriving DecidableEq, Repr

/-- 规则状态，对应 C 侧 `Lv00RuleStatus`。 -/
inductive RuleStatus where
  | disabled
  | enabled
  | deprecated
  | experimental
  deriving DecidableEq, Repr

/-- 规则优先级。C 侧使用整数，这里抽象为五档。 -/
inductive RulePriority where
  | lowest
  | low
  | normal
  | high
  | highest
  deriving DecidableEq, Repr

/-- 规则条件类型，对应 C 侧 `Lv00ConditionType`。 -/
inductive ConditionType where
  | patternMatch
  | typeCheck
  | valueCompare
  | existsCheck
  | forallCheck
  | custom
  deriving DecidableEq, Repr

/-- 规则变量：名称、类型约束、可选绑定对象。 -/
structure RuleVariable where
  name : String
  typeName : String
  bound : Option LvObj := none
  deriving Repr

/-- 规则条件：目前保留模式、变量名和条件类型，后续可细化为可判定谓词。 -/
structure RuleCondition where
  type : ConditionType
  pattern : String
  variable : String
  deriving Repr

/-- 规则前提：一个谓词模式及其附加条件。 -/
structure RulePremise where
  predicate : PrimPred
  conditions : List RuleCondition := []
  optional : Bool := false
  deriving Repr

/-- 规则结论：可被约束图吸收的本原谓词，附带证明理由。 -/
structure RuleConclusion where
  predicate : PrimPred
  justification : String := ""
  deriving Repr

/-- 可执行规则模板。 -/
structure ExecutableRule where
  id : Nat
  baseKind : BaseAxiomKind
  name : String
  description : String
  type : RuleType
  status : RuleStatus
  variables : List RuleVariable
  premises : List RulePremise
  conclusions : List RuleConclusion
  priority : RulePriority := RulePriority.normal
  dependencies : List Nat := []
  packageName : String := ""
  deriving Repr

/-- 前提良构性。 -/
def WellFormedPremise (p : RulePremise) : Prop :=
  WellFormedPred p.predicate

/-- 结论良构性。 -/
def WellFormedConclusion (c : RuleConclusion) : Prop :=
  WellFormedPred c.predicate

/-- 可执行规则良构性。 -/
def WellFormedExecutableRule (r : ExecutableRule) : Prop :=
  r.baseKind ∈ canonicalKinds ∧
  (∀ p ∈ r.premises, WellFormedPremise p) ∧
  (/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace RuleTemplate

open Ontology
open Predicates
open Constraint

/-- 规则类型，对应 C 侧 `Lv00RuleType`。 -/
inductive RuleType where
  | inference
  | rewrite
  | axiom
  | definition
  | theorem
  | lemma
  | tactic
  | constructor
  deriving DecidableEq, Repr

/-- 规则状态，对应 C 侧 `Lv00RuleStatus`。 -/
inductive RuleStatus where
  | disabled
  | enabled
  | deprecated
  | experimental
  deriving DecidableEq, Repr

/-- 规则优先级。C 侧使用整数，这里抽象为五档。 -/
inductive RulePriority where
  | lowest
  | low
  | normal
  | high
  | highest
  deriving DecidableEq, Repr

/-- 规则条件类型，对应 C 侧 `Lv00ConditionType`。 -/
inductive ConditionType where
  | patternMatch
  | typeCheck
  | valueCompare
  | existsCheck
  | forallCheck
  | custom
  deriving DecidableEq, Repr

/-- 规则变量：名称、类型约束、可选绑定对象。 -/
structure RuleVariable where
  name : String
  typeName : String
  bound : Option LvObj := none
  deriving Repr

/-- 规则条件：目前保留模式、变量名和条件类型，后续可细化为可判定谓词。 -/
structure RuleCondition where
  type : ConditionType
  pattern : String
  variable : String
  deriving Repr

/-- 规则前提：一个谓词模式及其附加条件。 -/
structure RulePremise where
  predicate : PrimPred
  conditions : List RuleCondition := []
  optional : Bool := false
  deriving Repr

/-- 规则结论：可被约束图吸收的本原谓词，附带证明理由。 -/
structure RuleConclusion where
  predicate : PrimPred
  justification : String := ""
  deriving Repr

/-- 可执行规则模板。 -/
structure ExecutableRule where
  id : Nat
  baseKind : BaseAxiomKind
  name : String
  description : String
  type : RuleType
  status : RuleStatus
  variables : List RuleVariable
  premises : List RulePremise
  conclusions : List RuleConclusion
  priority : RulePriority := RulePriority.normal
  dependencies : List Nat := []
  packageName : String := ""
  deriving Repr

/-- 前提良构性。 -/
def WellFormedPremise (p : RulePremise) : Prop :=
  WellFormedPred p.predicate

/-- 结论良构性。 -/
def WellFormedConclusion (c : RuleConclusion) : Prop :=
  WellFormedPred c.predicate

/-- 可执行规则良构性。 -/
def WellFormedExecutableRule (r : ExecutableRule) : Prop :=
  r.baseKind ∈ canonicalKinds ∧
  (∀ p ∈ r.premises, WellFormedPremise p) ∧
  (∀ c ∈ r.conclusions, WellFormedConclusion c)

/-- 规则匹/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace RuleTemplate

open Ontology
open Predicates
open Constraint

/-- 规则类型，对应 C 侧 `Lv00RuleType`。 -/
inductive RuleType where
  | inference
  | rewrite
  | axiom
  | definition
  | theorem
  | lemma
  | tactic
  | constructor
  deriving DecidableEq, Repr

/-- 规则状态，对应 C 侧 `Lv00RuleStatus`。 -/
inductive RuleStatus where
  | disabled
  | enabled
  | deprecated
  | experimental
  deriving DecidableEq, Repr

/-- 规则优先级。C 侧使用整数，这里抽象为五档。 -/
inductive RulePriority where
  | lowest
  | low
  | normal
  | high
  | highest
  deriving DecidableEq, Repr

/-- 规则条件类型，对应 C 侧 `Lv00ConditionType`。 -/
inductive ConditionType where
  | patternMatch
  | typeCheck
  | valueCompare
  | existsCheck
  | forallCheck
  | custom
  deriving DecidableEq, Repr

/-- 规则变量：名称、类型约束、可选绑定对象。 -/
structure RuleVariable where
  name : String
  typeName : String
  bound : Option LvObj := none
  deriving Repr

/-- 规则条件：目前保留模式、变量名和条件类型，后续可细化为可判定谓词。 -/
structure RuleCondition where
  type : ConditionType
  pattern : String
  variable : String
  deriving Repr

/-- 规则前提：一个谓词模式及其附加条件。 -/
structure RulePremise where
  predicate : PrimPred
  conditions : List RuleCondition := []
  optional : Bool := false
  deriving Repr

/-- 规则结论：可被约束图吸收的本原谓词，附带证明理由。 -/
structure RuleConclusion where
  predicate : PrimPred
  justification : String := ""
  deriving Repr

/-- 可执行规则模板。 -/
structure ExecutableRule where
  id : Nat
  baseKind : BaseAxiomKind
  name : String
  description : String
  type : RuleType
  status : RuleStatus
  variables : List RuleVariable
  premises : List RulePremise
  conclusions : List RuleConclusion
  priority : RulePriority := RulePriority.normal
  dependencies : List Nat := []
  packageName : String := ""
  deriving Repr

/-- 前提良构性。 -/
def WellFormedPremise (p : RulePremise) : Prop :=
  WellFormedPred p.predicate

/-- 结论良构性。 -/
def WellFormedConclusion (c : RuleConclusion) : Prop :=
  WellFormedPred c.predicate

/-- 可执行规则良构性。 -/
def WellFormedExecutableRule (r : ExecutableRule) : Prop :=
  r.baseKind ∈ canonicalKinds ∧
  (∀ p ∈ r.premises, WellFormedPremise p) ∧
  (∀ c ∈ r.conclusions, WellFormedConclusion c)

/-- 规则匹配结果，对应 C 侧 `Lv00RuleMatch`。 -/
structure RuleMatch/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace RuleTemplate

open Ontology
open Predicates
open Constraint

/-- 规则类型，对应 C 侧 `Lv00RuleType`。 -/
inductive RuleType where
  | inference
  | rewrite
  | axiom
  | definition
  | theorem
  | lemma
  | tactic
  | constructor
  deriving DecidableEq, Repr

/-- 规则状态，对应 C 侧 `Lv00RuleStatus`。 -/
inductive RuleStatus where
  | disabled
  | enabled
  | deprecated
  | experimental
  deriving DecidableEq, Repr

/-- 规则优先级。C 侧使用整数，这里抽象为五档。 -/
inductive RulePriority where
  | lowest
  | low
  | normal
  | high
  | highest
  deriving DecidableEq, Repr

/-- 规则条件类型，对应 C 侧 `Lv00ConditionType`。 -/
inductive ConditionType where
  | patternMatch
  | typeCheck
  | valueCompare
  | existsCheck
  | forallCheck
  | custom
  deriving DecidableEq, Repr

/-- 规则变量：名称、类型约束、可选绑定对象。 -/
structure RuleVariable where
  name : String
  typeName : String
  bound : Option LvObj := none
  deriving Repr

/-- 规则条件：目前保留模式、变量名和条件类型，后续可细化为可判定谓词。 -/
structure RuleCondition where
  type : ConditionType
  pattern : String
  variable : String
  deriving Repr

/-- 规则前提：一个谓词模式及其附加条件。 -/
structure RulePremise where
  predicate : PrimPred
  conditions : List RuleCondition := []
  optional : Bool := false
  deriving Repr

/-- 规则结论：可被约束图吸收的本原谓词，附带证明理由。 -/
structure RuleConclusion where
  predicate : PrimPred
  justification : String := ""
  deriving Repr

/-- 可执行规则模板。 -/
structure ExecutableRule where
  id : Nat
  baseKind : BaseAxiomKind
  name : String
  description : String
  type : RuleType
  status : RuleStatus
  variables : List RuleVariable
  premises : List RulePremise
  conclusions : List RuleConclusion
  priority : RulePriority := RulePriority.normal
  dependencies : List Nat := []
  packageName : String := ""
  deriving Repr

/-- 前提良构性。 -/
def WellFormedPremise (p : RulePremise) : Prop :=
  WellFormedPred p.predicate

/-- 结论良构性。 -/
def WellFormedConclusion (c : RuleConclusion) : Prop :=
  WellFormedPred c.predicate

/-- 可执行规则良构性。 -/
def WellFormedExecutableRule (r : ExecutableRule) : Prop :=
  r.baseKind ∈ canonicalKinds ∧
  (∀ p ∈ r.premises, WellFormedPremise p) ∧
  (∀ c ∈ r.conclusions, WellFormedConclusion c)

/-- 规则匹配结果，对应 C 侧 `Lv00RuleMatch`。 -/
structure RuleMatch where
  rule : ExecutableRule
/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace RuleTemplate

open Ontology
open Predicates
open Constraint

/-- 规则类型，对应 C 侧 `Lv00RuleType`。 -/
inductive RuleType where
  | inference
  | rewrite
  | axiom
  | definition
  | theorem
  | lemma
  | tactic
  | constructor
  deriving DecidableEq, Repr

/-- 规则状态，对应 C 侧 `Lv00RuleStatus`。 -/
inductive RuleStatus where
  | disabled
  | enabled
  | deprecated
  | experimental
  deriving DecidableEq, Repr

/-- 规则优先级。C 侧使用整数，这里抽象为五档。 -/
inductive RulePriority where
  | lowest
  | low
  | normal
  | high
  | highest
  deriving DecidableEq, Repr

/-- 规则条件类型，对应 C 侧 `Lv00ConditionType`。 -/
inductive ConditionType where
  | patternMatch
  | typeCheck
  | valueCompare
  | existsCheck
  | forallCheck
  | custom
  deriving DecidableEq, Repr

/-- 规则变量：名称、类型约束、可选绑定对象。 -/
structure RuleVariable where
  name : String
  typeName : String
  bound : Option LvObj := none
  deriving Repr

/-- 规则条件：目前保留模式、变量名和条件类型，后续可细化为可判定谓词。 -/
structure RuleCondition where
  type : ConditionType
  pattern : String
  variable : String
  deriving Repr

/-- 规则前提：一个谓词模式及其附加条件。 -/
structure RulePremise where
  predicate : PrimPred
  conditions : List RuleCondition := []
  optional : Bool := false
  deriving Repr

/-- 规则结论：可被约束图吸收的本原谓词，附带证明理由。 -/
structure RuleConclusion where
  predicate : PrimPred
  justification : String := ""
  deriving Repr

/-- 可执行规则模板。 -/
structure ExecutableRule where
  id : Nat
  baseKind : BaseAxiomKind
  name : String
  description : String
  type : RuleType
  status : RuleStatus
  variables : List RuleVariable
  premises : List RulePremise
  conclusions : List RuleConclusion
  priority : RulePriority := RulePriority.normal
  dependencies : List Nat := []
  packageName : String := ""
  deriving Repr

/-- 前提良构性。 -/
def WellFormedPremise (p : RulePremise) : Prop :=
  WellFormedPred p.predicate

/-- 结论良构性。 -/
def WellFormedConclusion (c : RuleConclusion) : Prop :=
  WellFormedPred c.predicate

/-- 可执行规则良构性。 -/
def WellFormedExecutableRule (r : ExecutableRule) : Prop :=
  r.baseKind ∈ canonicalKinds ∧
  (∀ p ∈ r.premises, WellFormedPremise p) ∧
  (∀ c ∈ r.conclusions, WellFormedConclusion c)

/-- 规则匹配结果，对应 C 侧 `Lv00RuleMatch`。 -/
structure RuleMatch where
  rule : ExecutableRule
  bindings : List RuleVariable
  confidence : Nat
  matchedPremises : Nat
  complete : Bool
  deriving Repr

//-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace RuleTemplate

open Ontology
open Predicates
open Constraint

/-- 规则类型，对应 C 侧 `Lv00RuleType`。 -/
inductive RuleType where
  | inference
  | rewrite
  | axiom
  | definition
  | theorem
  | lemma
  | tactic
  | constructor
  deriving DecidableEq, Repr

/-- 规则状态，对应 C 侧 `Lv00RuleStatus`。 -/
inductive RuleStatus where
  | disabled
  | enabled
  | deprecated
  | experimental
  deriving DecidableEq, Repr

/-- 规则优先级。C 侧使用整数，这里抽象为五档。 -/
inductive RulePriority where
  | lowest
  | low
  | normal
  | high
  | highest
  deriving DecidableEq, Repr

/-- 规则条件类型，对应 C 侧 `Lv00ConditionType`。 -/
inductive ConditionType where
  | patternMatch
  | typeCheck
  | valueCompare
  | existsCheck
  | forallCheck
  | custom
  deriving DecidableEq, Repr

/-- 规则变量：名称、类型约束、可选绑定对象。 -/
structure RuleVariable where
  name : String
  typeName : String
  bound : Option LvObj := none
  deriving Repr

/-- 规则条件：目前保留模式、变量名和条件类型，后续可细化为可判定谓词。 -/
structure RuleCondition where
  type : ConditionType
  pattern : String
  variable : String
  deriving Repr

/-- 规则前提：一个谓词模式及其附加条件。 -/
structure RulePremise where
  predicate : PrimPred
  conditions : List RuleCondition := []
  optional : Bool := false
  deriving Repr

/-- 规则结论：可被约束图吸收的本原谓词，附带证明理由。 -/
structure RuleConclusion where
  predicate : PrimPred
  justification : String := ""
  deriving Repr

/-- 可执行规则模板。 -/
structure ExecutableRule where
  id : Nat
  baseKind : BaseAxiomKind
  name : String
  description : String
  type : RuleType
  status : RuleStatus
  variables : List RuleVariable
  premises : List RulePremise
  conclusions : List RuleConclusion
  priority : RulePriority := RulePriority.normal
  dependencies : List Nat := []
  packageName : String := ""
  deriving Repr

/-- 前提良构性。 -/
def WellFormedPremise (p : RulePremise) : Prop :=
  WellFormedPred p.predicate

/-- 结论良构性。 -/
def WellFormedConclusion (c : RuleConclusion) : Prop :=
  WellFormedPred c.predicate

/-- 可执行规则良构性。 -/
def WellFormedExecutableRule (r : ExecutableRule) : Prop :=
  r.baseKind ∈ canonicalKinds ∧
  (∀ p ∈ r.premises, WellFormedPremise p) ∧
  (∀ c ∈ r.conclusions, WellFormedConclusion c)

/-- 规则匹配结果，对应 C 侧 `Lv00RuleMatch`。 -/
structure RuleMatch where
  rule : ExecutableRule
  bindings : List RuleVariable
  confidence : Nat
  matchedPremises : Nat
  complete : Bool
  deriving Repr

/-- 规则匹配良构性：匹配对象对应的规则本身良构。 -/
def WellFormedMatch (m : RuleMatch) : Prop :=
  WellFormedExecutableRule m.rule ∧ m.complete = true

/-- 规则适用性：图良构、规则良构、匹配完整。 -/
def Applicable (g : ConstraintGraph) (m : RuleMatch) : Prop :=
  WellFormedGraph g ∧ WellFormedMatch m

/-- 规则应用结果：产生若干结论谓词。 -/
structure RuleApplication where
  sourceGraph : ConstraintGraph
  match : RuleMatch
  produced : List RuleConclusion
  deriving Repr

/-- 应/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace RuleTemplate

open Ontology
open Predicates
open Constraint

/-- 规则类型，对应 C 侧 `Lv00RuleType`。 -/
inductive RuleType where
  | inference
  | rewrite
  | axiom
  | definition
  | theorem
  | lemma
  | tactic
  | constructor
  deriving DecidableEq, Repr

/-- 规则状态，对应 C 侧 `Lv00RuleStatus`。 -/
inductive RuleStatus where
  | disabled
  | enabled
  | deprecated
  | experimental
  deriving DecidableEq, Repr

/-- 规则优先级。C 侧使用整数，这里抽象为五档。 -/
inductive RulePriority where
  | lowest
  | low
  | normal
  | high
  | highest
  deriving DecidableEq, Repr

/-- 规则条件类型，对应 C 侧 `Lv00ConditionType`。 -/
inductive ConditionType where
  | patternMatch
  | typeCheck
  | valueCompare
  | existsCheck
  | forallCheck
  | custom
  deriving DecidableEq, Repr

/-- 规则变量：名称、类型约束、可选绑定对象。 -/
structure RuleVariable where
  name : String
  typeName : String
  bound : Option LvObj := none
  deriving Repr

/-- 规则条件：目前保留模式、变量名和条件类型，后续可细化为可判定谓词。 -/
structure RuleCondition where
  type : ConditionType
  pattern : String
  variable : String
  deriving Repr

/-- 规则前提：一个谓词模式及其附加条件。 -/
structure RulePremise where
  predicate : PrimPred
  conditions : List RuleCondition := []
  optional : Bool := false
  deriving Repr

/-- 规则结论：可被约束图吸收的本原谓词，附带证明理由。 -/
structure RuleConclusion where
  predicate : PrimPred
  justification : String := ""
  deriving Repr

/-- 可执行规则模板。 -/
structure ExecutableRule where
  id : Nat
  baseKind : BaseAxiomKind
  name : String
  description : String
  type : RuleType
  status : RuleStatus
  variables : List RuleVariable
  premises : List RulePremise
  conclusions : List RuleConclusion
  priority : RulePriority := RulePriority.normal
  dependencies : List Nat := []
  packageName : String := ""
  deriving Repr

/-- 前提良构性。 -/
def WellFormedPremise (p : RulePremise) : Prop :=
  WellFormedPred p.predicate

/-- 结论良构性。 -/
def WellFormedConclusion (c : RuleConclusion) : Prop :=
  WellFormedPred c.predicate

/-- 可执行规则良构性。 -/
def WellFormedExecutableRule (r : ExecutableRule) : Prop :=
  r.baseKind ∈ canonicalKinds ∧
  (∀ p ∈ r.premises, WellFormedPremise p) ∧
  (∀ c ∈ r.conclusions, WellFormedConclusion c)

/-- 规则匹配结果，对应 C 侧 `Lv00RuleMatch`。 -/
structure RuleMatch where
  rule : ExecutableRule
  bindings : List RuleVariable
  confidence : Nat
  matchedPremises : Nat
  complete : Bool
  deriving Repr

/-- 规则匹配良构性：匹配对象对应的规则本身良构。 -/
def WellFormedMatch (m : RuleMatch) : Prop :=
  WellFormedExecutableRule m.rule ∧ m.complete = true

/-- 规则适用性：图良构、规则良构、匹配完整。 -/
def Applicable (g : ConstraintGraph) (m : RuleMatch) : Prop :=
  WellFormedGraph g ∧ WellFormedMatch m

/-- 规则应用结果：产生若干结论谓词。 -/
structure RuleApplication where
  sourceGraph : ConstraintGraph
  match : RuleMatch
  produced : List RuleConclusion
  deriving Repr

/-- 应用结果良构性。 -/
def WellFormedApplication (a : RuleApplication) : Prop :=
  Applicable a.sourceGraph a.match ∧
  a.produced = a.match.rule.conclusions

/-- 结论可被图吸收：结论谓词良构即可进入约束图。 -/
def ConclusionAbsorbable (c : RuleConclusion) : Prop :=
  WellFormedConclusion c

/-- 规则良构时，它的全部结论都可被图吸收。 -/
theorem executable_rule_conclusions_absorbable
    {r : ExecutableRule} (h : WellFormedExecutableRule r) :
    ∀ c ∈ r.conclusions, ConclusionAbsorbable/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace RuleTemplate

open Ontology
open Predicates
open Constraint

/-- 规则类型，对应 C 侧 `Lv00RuleType`。 -/
inductive RuleType where
  | inference
  | rewrite
  | axiom
  | definition
  | theorem
  | lemma
  | tactic
  | constructor
  deriving DecidableEq, Repr

/-- 规则状态，对应 C 侧 `Lv00RuleStatus`。 -/
inductive RuleStatus where
  | disabled
  | enabled
  | deprecated
  | experimental
  deriving DecidableEq, Repr

/-- 规则优先级。C 侧使用整数，这里抽象为五档。 -/
inductive RulePriority where
  | lowest
  | low
  | normal
  | high
  | highest
  deriving DecidableEq, Repr

/-- 规则条件类型，对应 C 侧 `Lv00ConditionType`。 -/
inductive ConditionType where
  | patternMatch
  | typeCheck
  | valueCompare
  | existsCheck
  | forallCheck
  | custom
  deriving DecidableEq, Repr

/-- 规则变量：名称、类型约束、可选绑定对象。 -/
structure RuleVariable where
  name : String
  typeName : String
  bound : Option LvObj := none
  deriving Repr

/-- 规则条件：目前保留模式、变量名和条件类型，后续可细化为可判定谓词。 -/
structure RuleCondition where
  type : ConditionType
  pattern : String
  variable : String
  deriving Repr

/-- 规则前提：一个谓词模式及其附加条件。 -/
structure RulePremise where
  predicate : PrimPred
  conditions : List RuleCondition := []
  optional : Bool := false
  deriving Repr

/-- 规则结论：可被约束图吸收的本原谓词，附带证明理由。 -/
structure RuleConclusion where
  predicate : PrimPred
  justification : String := ""
  deriving Repr

/-- 可执行规则模板。 -/
structure ExecutableRule where
  id : Nat
  baseKind : BaseAxiomKind
  name : String
  description : String
  type : RuleType
  status : RuleStatus
  variables : List RuleVariable
  premises : List RulePremise
  conclusions : List RuleConclusion
  priority : RulePriority := RulePriority.normal
  dependencies : List Nat := []
  packageName : String := ""
  deriving Repr

/-- 前提良构性。 -/
def WellFormedPremise (p : RulePremise) : Prop :=
  WellFormedPred p.predicate

/-- 结论良构性。 -/
def WellFormedConclusion (c : RuleConclusion) : Prop :=
  WellFormedPred c.predicate

/-- 可执行规则良构性。 -/
def WellFormedExecutableRule (r : ExecutableRule) : Prop :=
  r.baseKind ∈ canonicalKinds ∧
  (∀ p ∈ r.premises, WellFormedPremise p) ∧
  (∀ c ∈ r.conclusions, WellFormedConclusion c)

/-- 规则匹配结果，对应 C 侧 `Lv00RuleMatch`。 -/
structure RuleMatch where
  rule : ExecutableRule
  bindings : List RuleVariable
  confidence : Nat
  matchedPremises : Nat
  complete : Bool
  deriving Repr

/-- 规则匹配良构性：匹配对象对应的规则本身良构。 -/
def WellFormedMatch (m : RuleMatch) : Prop :=
  WellFormedExecutableRule m.rule ∧ m.complete = true

/-- 规则适用性：图良构、规则良构、匹配完整。 -/
def Applicable (g : ConstraintGraph) (m : RuleMatch) : Prop :=
  WellFormedGraph g ∧ WellFormedMatch m

/-- 规则应用结果：产生若干结论谓词。 -/
structure RuleApplication where
  sourceGraph : ConstraintGraph
  match : RuleMatch
  produced : List RuleConclusion
  deriving Repr

/-- 应用结果良构性。 -/
def WellFormedApplication (a : RuleApplication) : Prop :=
  Applicable a.sourceGraph a.match ∧
  a.produced = a.match.rule.conclusions

/-- 结论可被图吸收：结论谓词良构即可进入约束图。 -/
def ConclusionAbsorbable (c : RuleConclusion) : Prop :=
  WellFormedConclusion c

/-- 规则良构时，它的全部结论都可被图吸收。 -/
theorem executable_rule_conclusions_absorbable
    {r : ExecutableRule} (h : WellFormedExecutableRule r) :
    ∀ c ∈ r.conclusions, ConclusionAbsorbable c := by
  intro c hc
  exact h.2.2 c hc

/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace RuleTemplate

open Ontology
open Predicates
open Constraint

/-- 规则类型，对应 C 侧 `Lv00RuleType`。 -/
inductive RuleType where
  | inference
  | rewrite
  | axiom
  | definition
  | theorem
  | lemma
  | tactic
  | constructor
  deriving DecidableEq, Repr

/-- 规则状态，对应 C 侧 `Lv00RuleStatus`。 -/
inductive RuleStatus where
  | disabled
  | enabled
  | deprecated
  | experimental
  deriving DecidableEq, Repr

/-- 规则优先级。C 侧使用整数，这里抽象为五档。 -/
inductive RulePriority where
  | lowest
  | low
  | normal
  | high
  | highest
  deriving DecidableEq, Repr

/-- 规则条件类型，对应 C 侧 `Lv00ConditionType`。 -/
inductive ConditionType where
  | patternMatch
  | typeCheck
  | valueCompare
  | existsCheck
  | forallCheck
  | custom
  deriving DecidableEq, Repr

/-- 规则变量：名称、类型约束、可选绑定对象。 -/
structure RuleVariable where
  name : String
  typeName : String
  bound : Option LvObj := none
  deriving Repr

/-- 规则条件：目前保留模式、变量名和条件类型，后续可细化为可判定谓词。 -/
structure RuleCondition where
  type : ConditionType
  pattern : String
  variable : String
  deriving Repr

/-- 规则前提：一个谓词模式及其附加条件。 -/
structure RulePremise where
  predicate : PrimPred
  conditions : List RuleCondition := []
  optional : Bool := false
  deriving Repr

/-- 规则结论：可被约束图吸收的本原谓词，附带证明理由。 -/
structure RuleConclusion where
  predicate : PrimPred
  justification : String := ""
  deriving Repr

/-- 可执行规则模板。 -/
structure ExecutableRule where
  id : Nat
  baseKind : BaseAxiomKind
  name : String
  description : String
  type : RuleType
  status : RuleStatus
  variables : List RuleVariable
  premises : List RulePremise
  conclusions : List RuleConclusion
  priority : RulePriority := RulePriority.normal
  dependencies : List Nat := []
  packageName : String := ""
  deriving Repr

/-- 前提良构性。 -/
def WellFormedPremise (p : RulePremise) : Prop :=
  WellFormedPred p.predicate

/-- 结论良构性。 -/
def WellFormedConclusion (c : RuleConclusion) : Prop :=
  WellFormedPred c.predicate

/-- 可执行规则良构性。 -/
def WellFormedExecutableRule (r : ExecutableRule) : Prop :=
  r.baseKind ∈ canonicalKinds ∧
  (∀ p ∈ r.premises, WellFormedPremise p) ∧
  (∀ c ∈ r.conclusions, WellFormedConclusion c)

/-- 规则匹配结果，对应 C 侧 `Lv00RuleMatch`。 -/
structure RuleMatch where
  rule : ExecutableRule
  bindings : List RuleVariable
  confidence : Nat
  matchedPremises : Nat
  complete : Bool
  deriving Repr

/-- 规则匹配良构性：匹配对象对应的规则本身良构。 -/
def WellFormedMatch (m : RuleMatch) : Prop :=
  WellFormedExecutableRule m.rule ∧ m.complete = true

/-- 规则适用性：图良构、规则良构、匹配完整。 -/
def Applicable (g : ConstraintGraph) (m : RuleMatch) : Prop :=
  WellFormedGraph g ∧ WellFormedMatch m

/-- 规则应用结果：产生若干结论谓词。 -/
structure RuleApplication where
  sourceGraph : ConstraintGraph
  match : RuleMatch
  produced : List RuleConclusion
  deriving Repr

/-- 应用结果良构性。 -/
def WellFormedApplication (a : RuleApplication) : Prop :=
  Applicable a.sourceGraph a.match ∧
  a.produced = a.match.rule.conclusions

/-- 结论可被图吸收：结论谓词良构即可进入约束图。 -/
def ConclusionAbsorbable (c : RuleConclusion) : Prop :=
  WellFormedConclusion c

/-- 规则良构时，它的全部结论都可被图吸收。 -/
theorem executable_rule_conclusions_absorbable
    {r : ExecutableRule} (h : WellFormedExecutableRule r) :
    ∀ c ∈ r.conclusions, ConclusionAbsorbable c := by
  intro c hc
  exact h.2.2 c hc

/-- 若规则匹配完整且规则良/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace RuleTemplate

open Ontology
open Predicates
open Constraint

/-- 规则类型，对应 C 侧 `Lv00RuleType`。 -/
inductive RuleType where
  | inference
  | rewrite
  | axiom
  | definition
  | theorem
  | lemma
  | tactic
  | constructor
  deriving DecidableEq, Repr

/-- 规则状态，对应 C 侧 `Lv00RuleStatus`。 -/
inductive RuleStatus where
  | disabled
  | enabled
  | deprecated
  | experimental
  deriving DecidableEq, Repr

/-- 规则优先级。C 侧使用整数，这里抽象为五档。 -/
inductive RulePriority where
  | lowest
  | low
  | normal
  | high
  | highest
  deriving DecidableEq, Repr

/-- 规则条件类型，对应 C 侧 `Lv00ConditionType`。 -/
inductive ConditionType where
  | patternMatch
  | typeCheck
  | valueCompare
  | existsCheck
  | forallCheck
  | custom
  deriving DecidableEq, Repr

/-- 规则变量：名称、类型约束、可选绑定对象。 -/
structure RuleVariable where
  name : String
  typeName : String
  bound : Option LvObj := none
  deriving Repr

/-- 规则条件：目前保留模式、变量名和条件类型，后续可细化为可判定谓词。 -/
structure RuleCondition where
  type : ConditionType
  pattern : String
  variable : String
  deriving Repr

/-- 规则前提：一个谓词模式及其附加条件。 -/
structure RulePremise where
  predicate : PrimPred
  conditions : List RuleCondition := []
  optional : Bool := false
  deriving Repr

/-- 规则结论：可被约束图吸收的本原谓词，附带证明理由。 -/
structure RuleConclusion where
  predicate : PrimPred
  justification : String := ""
  deriving Repr

/-- 可执行规则模板。 -/
structure ExecutableRule where
  id : Nat
  baseKind : BaseAxiomKind
  name : String
  description : String
  type : RuleType
  status : RuleStatus
  variables : List RuleVariable
  premises : List RulePremise
  conclusions : List RuleConclusion
  priority : RulePriority := RulePriority.normal
  dependencies : List Nat := []
  packageName : String := ""
  deriving Repr

/-- 前提良构性。 -/
def WellFormedPremise (p : RulePremise) : Prop :=
  WellFormedPred p.predicate

/-- 结论良构性。 -/
def WellFormedConclusion (c : RuleConclusion) : Prop :=
  WellFormedPred c.predicate

/-- 可执行规则良构性。 -/
def WellFormedExecutableRule (r : ExecutableRule) : Prop :=
  r.baseKind ∈ canonicalKinds ∧
  (∀ p ∈ r.premises, WellFormedPremise p) ∧
  (∀ c ∈ r.conclusions, WellFormedConclusion c)

/-- 规则匹配结果，对应 C 侧 `Lv00RuleMatch`。 -/
structure RuleMatch where
  rule : ExecutableRule
  bindings : List RuleVariable
  confidence : Nat
  matchedPremises : Nat
  complete : Bool
  deriving Repr

/-- 规则匹配良构性：匹配对象对应的规则本身良构。 -/
def WellFormedMatch (m : RuleMatch) : Prop :=
  WellFormedExecutableRule m.rule ∧ m.complete = true

/-- 规则适用性：图良构、规则良构、匹配完整。 -/
def Applicable (g : ConstraintGraph) (m : RuleMatch) : Prop :=
  WellFormedGraph g ∧ WellFormedMatch m

/-- 规则应用结果：产生若干结论谓词。 -/
structure RuleApplication where
  sourceGraph : ConstraintGraph
  match : RuleMatch
  produced : List RuleConclusion
  deriving Repr

/-- 应用结果良构性。 -/
def WellFormedApplication (a : RuleApplication) : Prop :=
  Applicable a.sourceGraph a.match ∧
  a.produced = a.match.rule.conclusions

/-- 结论可被图吸收：结论谓词良构即可进入约束图。 -/
def ConclusionAbsorbable (c : RuleConclusion) : Prop :=
  WellFormedConclusion c

/-- 规则良构时，它的全部结论都可被图吸收。 -/
theorem executable_rule_conclusions_absorbable
    {r : ExecutableRule} (h : WellFormedExecutableRule r) :
    ∀ c ∈ r.conclusions, ConclusionAbsorbable c := by
  intro c hc
  exact h.2.2 c hc

/-- 若规则匹配完整且规则良构，则匹配规则的全部结论可被吸收。 -/
theorem match_conclusions_absorbable
    {m : RuleMatch} (h : WellFormedMatch m) :
    ∀ c ∈ m/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace RuleTemplate

open Ontology
open Predicates
open Constraint

/-- 规则类型，对应 C 侧 `Lv00RuleType`。 -/
inductive RuleType where
  | inference
  | rewrite
  | axiom
  | definition
  | theorem
  | lemma
  | tactic
  | constructor
  deriving DecidableEq, Repr

/-- 规则状态，对应 C 侧 `Lv00RuleStatus`。 -/
inductive RuleStatus where
  | disabled
  | enabled
  | deprecated
  | experimental
  deriving DecidableEq, Repr

/-- 规则优先级。C 侧使用整数，这里抽象为五档。 -/
inductive RulePriority where
  | lowest
  | low
  | normal
  | high
  | highest
  deriving DecidableEq, Repr

/-- 规则条件类型，对应 C 侧 `Lv00ConditionType`。 -/
inductive ConditionType where
  | patternMatch
  | typeCheck
  | valueCompare
  | existsCheck
  | forallCheck
  | custom
  deriving DecidableEq, Repr

/-- 规则变量：名称、类型约束、可选绑定对象。 -/
structure RuleVariable where
  name : String
  typeName : String
  bound : Option LvObj := none
  deriving Repr

/-- 规则条件：目前保留模式、变量名和条件类型，后续可细化为可判定谓词。 -/
structure RuleCondition where
  type : ConditionType
  pattern : String
  variable : String
  deriving Repr

/-- 规则前提：一个谓词模式及其附加条件。 -/
structure RulePremise where
  predicate : PrimPred
  conditions : List RuleCondition := []
  optional : Bool := false
  deriving Repr

/-- 规则结论：可被约束图吸收的本原谓词，附带证明理由。 -/
structure RuleConclusion where
  predicate : PrimPred
  justification : String := ""
  deriving Repr

/-- 可执行规则模板。 -/
structure ExecutableRule where
  id : Nat
  baseKind : BaseAxiomKind
  name : String
  description : String
  type : RuleType
  status : RuleStatus
  variables : List RuleVariable
  premises : List RulePremise
  conclusions : List RuleConclusion
  priority : RulePriority := RulePriority.normal
  dependencies : List Nat := []
  packageName : String := ""
  deriving Repr

/-- 前提良构性。 -/
def WellFormedPremise (p : RulePremise) : Prop :=
  WellFormedPred p.predicate

/-- 结论良构性。 -/
def WellFormedConclusion (c : RuleConclusion) : Prop :=
  WellFormedPred c.predicate

/-- 可执行规则良构性。 -/
def WellFormedExecutableRule (r : ExecutableRule) : Prop :=
  r.baseKind ∈ canonicalKinds ∧
  (∀ p ∈ r.premises, WellFormedPremise p) ∧
  (∀ c ∈ r.conclusions, WellFormedConclusion c)

/-- 规则匹配结果，对应 C 侧 `Lv00RuleMatch`。 -/
structure RuleMatch where
  rule : ExecutableRule
  bindings : List RuleVariable
  confidence : Nat
  matchedPremises : Nat
  complete : Bool
  deriving Repr

/-- 规则匹配良构性：匹配对象对应的规则本身良构。 -/
def WellFormedMatch (m : RuleMatch) : Prop :=
  WellFormedExecutableRule m.rule ∧ m.complete = true

/-- 规则适用性：图良构、规则良构、匹配完整。 -/
def Applicable (g : ConstraintGraph) (m : RuleMatch) : Prop :=
  WellFormedGraph g ∧ WellFormedMatch m

/-- 规则应用结果：产生若干结论谓词。 -/
structure RuleApplication where
  sourceGraph : ConstraintGraph
  match : RuleMatch
  produced : List RuleConclusion
  deriving Repr

/-- 应用结果良构性。 -/
def WellFormedApplication (a : RuleApplication) : Prop :=
  Applicable a.sourceGraph a.match ∧
  a.produced = a.match.rule.conclusions

/-- 结论可被图吸收：结论谓词良构即可进入约束图。 -/
def ConclusionAbsorbable (c : RuleConclusion) : Prop :=
  WellFormedConclusion c

/-- 规则良构时，它的全部结论都可被图吸收。 -/
theorem executable_rule_conclusions_absorbable
    {r : ExecutableRule} (h : WellFormedExecutableRule r) :
    ∀ c ∈ r.conclusions, ConclusionAbsorbable c := by
  intro c hc
  exact h.2.2 c hc

/-- 若规则匹配完整且规则良构，则匹配规则的全部结论可被吸收。 -/
theorem match_conclusions_absorbable
    {m : RuleMatch} (h : WellFormedMatch m) :
    ∀ c ∈ m.rule.conclusions, ConclusionAbsorbable c := by
  exact executable_rule_conclusions_absorbable h.1

/-- 若规则应用良构，则应用产生的所有结论/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace RuleTemplate

open Ontology
open Predicates
open Constraint

/-- 规则类型，对应 C 侧 `Lv00RuleType`。 -/
inductive RuleType where
  | inference
  | rewrite
  | axiom
  | definition
  | theorem
  | lemma
  | tactic
  | constructor
  deriving DecidableEq, Repr

/-- 规则状态，对应 C 侧 `Lv00RuleStatus`。 -/
inductive RuleStatus where
  | disabled
  | enabled
  | deprecated
  | experimental
  deriving DecidableEq, Repr

/-- 规则优先级。C 侧使用整数，这里抽象为五档。 -/
inductive RulePriority where
  | lowest
  | low
  | normal
  | high
  | highest
  deriving DecidableEq, Repr

/-- 规则条件类型，对应 C 侧 `Lv00ConditionType`。 -/
inductive ConditionType where
  | patternMatch
  | typeCheck
  | valueCompare
  | existsCheck
  | forallCheck
  | custom
  deriving DecidableEq, Repr

/-- 规则变量：名称、类型约束、可选绑定对象。 -/
structure RuleVariable where
  name : String
  typeName : String
  bound : Option LvObj := none
  deriving Repr

/-- 规则条件：目前保留模式、变量名和条件类型，后续可细化为可判定谓词。 -/
structure RuleCondition where
  type : ConditionType
  pattern : String
  variable : String
  deriving Repr

/-- 规则前提：一个谓词模式及其附加条件。 -/
structure RulePremise where
  predicate : PrimPred
  conditions : List RuleCondition := []
  optional : Bool := false
  deriving Repr

/-- 规则结论：可被约束图吸收的本原谓词，附带证明理由。 -/
structure RuleConclusion where
  predicate : PrimPred
  justification : String := ""
  deriving Repr

/-- 可执行规则模板。 -/
structure ExecutableRule where
  id : Nat
  baseKind : BaseAxiomKind
  name : String
  description : String
  type : RuleType
  status : RuleStatus
  variables : List RuleVariable
  premises : List RulePremise
  conclusions : List RuleConclusion
  priority : RulePriority := RulePriority.normal
  dependencies : List Nat := []
  packageName : String := ""
  deriving Repr

/-- 前提良构性。 -/
def WellFormedPremise (p : RulePremise) : Prop :=
  WellFormedPred p.predicate

/-- 结论良构性。 -/
def WellFormedConclusion (c : RuleConclusion) : Prop :=
  WellFormedPred c.predicate

/-- 可执行规则良构性。 -/
def WellFormedExecutableRule (r : ExecutableRule) : Prop :=
  r.baseKind ∈ canonicalKinds ∧
  (∀ p ∈ r.premises, WellFormedPremise p) ∧
  (∀ c ∈ r.conclusions, WellFormedConclusion c)

/-- 规则匹配结果，对应 C 侧 `Lv00RuleMatch`。 -/
structure RuleMatch where
  rule : ExecutableRule
  bindings : List RuleVariable
  confidence : Nat
  matchedPremises : Nat
  complete : Bool
  deriving Repr

/-- 规则匹配良构性：匹配对象对应的规则本身良构。 -/
def WellFormedMatch (m : RuleMatch) : Prop :=
  WellFormedExecutableRule m.rule ∧ m.complete = true

/-- 规则适用性：图良构、规则良构、匹配完整。 -/
def Applicable (g : ConstraintGraph) (m : RuleMatch) : Prop :=
  WellFormedGraph g ∧ WellFormedMatch m

/-- 规则应用结果：产生若干结论谓词。 -/
structure RuleApplication where
  sourceGraph : ConstraintGraph
  match : RuleMatch
  produced : List RuleConclusion
  deriving Repr

/-- 应用结果良构性。 -/
def WellFormedApplication (a : RuleApplication) : Prop :=
  Applicable a.sourceGraph a.match ∧
  a.produced = a.match.rule.conclusions

/-- 结论可被图吸收：结论谓词良构即可进入约束图。 -/
def ConclusionAbsorbable (c : RuleConclusion) : Prop :=
  WellFormedConclusion c

/-- 规则良构时，它的全部结论都可被图吸收。 -/
theorem executable_rule_conclusions_absorbable
    {r : ExecutableRule} (h : WellFormedExecutableRule r) :
    ∀ c ∈ r.conclusions, ConclusionAbsorbable c := by
  intro c hc
  exact h.2.2 c hc

/-- 若规则匹配完整且规则良构，则匹配规则的全部结论可被吸收。 -/
theorem match_conclusions_absorbable
    {m : RuleMatch} (h : WellFormedMatch m) :
    ∀ c ∈ m.rule.conclusions, ConclusionAbsorbable c := by
  exact executable_rule_conclusions_absorbable h.1

/-- 若规则应用良构，则应用产生的所有结论都可被约束图吸收。 -/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace RuleTemplate

open Ontology
open Predicates
open Constraint

/-- 规则类型，对应 C 侧 `Lv00RuleType`。 -/
inductive RuleType where
  | inference
  | rewrite
  | axiom
  | definition
  | theorem
  | lemma
  | tactic
  | constructor
  deriving DecidableEq, Repr

/-- 规则状态，对应 C 侧 `Lv00RuleStatus`。 -/
inductive RuleStatus where
  | disabled
  | enabled
  | deprecated
  | experimental
  deriving DecidableEq, Repr

/-- 规则优先级。C 侧使用整数，这里抽象为五档。 -/
inductive RulePriority where
  | lowest
  | low
  | normal
  | high
  | highest
  deriving DecidableEq, Repr

/-- 规则条件类型，对应 C 侧 `Lv00ConditionType`。 -/
inductive ConditionType where
  | patternMatch
  | typeCheck
  | valueCompare
  | existsCheck
  | forallCheck
  | custom
  deriving DecidableEq, Repr

/-- 规则变量：名称、类型约束、可选绑定对象。 -/
structure RuleVariable where
  name : String
  typeName : String
  bound : Option LvObj := none
  deriving Repr

/-- 规则条件：目前保留模式、变量名和条件类型，后续可细化为可判定谓词。 -/
structure RuleCondition where
  type : ConditionType
  pattern : String
  variable : String
  deriving Repr

/-- 规则前提：一个谓词模式及其附加条件。 -/
structure RulePremise where
  predicate : PrimPred
  conditions : List RuleCondition := []
  optional : Bool := false
  deriving Repr

/-- 规则结论：可被约束图吸收的本原谓词，附带证明理由。 -/
structure RuleConclusion where
  predicate : PrimPred
  justification : String := ""
  deriving Repr

/-- 可执行规则模板。 -/
structure ExecutableRule where
  id : Nat
  baseKind : BaseAxiomKind
  name : String
  description : String
  type : RuleType
  status : RuleStatus
  variables : List RuleVariable
  premises : List RulePremise
  conclusions : List RuleConclusion
  priority : RulePriority := RulePriority.normal
  dependencies : List Nat := []
  packageName : String := ""
  deriving Repr

/-- 前提良构性。 -/
def WellFormedPremise (p : RulePremise) : Prop :=
  WellFormedPred p.predicate

/-- 结论良构性。 -/
def WellFormedConclusion (c : RuleConclusion) : Prop :=
  WellFormedPred c.predicate

/-- 可执行规则良构性。 -/
def WellFormedExecutableRule (r : ExecutableRule) : Prop :=
  r.baseKind ∈ canonicalKinds ∧
  (∀ p ∈ r.premises, WellFormedPremise p) ∧
  (∀ c ∈ r.conclusions, WellFormedConclusion c)

/-- 规则匹配结果，对应 C 侧 `Lv00RuleMatch`。 -/
structure RuleMatch where
  rule : ExecutableRule
  bindings : List RuleVariable
  confidence : Nat
  matchedPremises : Nat
  complete : Bool
  deriving Repr

/-- 规则匹配良构性：匹配对象对应的规则本身良构。 -/
def WellFormedMatch (m : RuleMatch) : Prop :=
  WellFormedExecutableRule m.rule ∧ m.complete = true

/-- 规则适用性：图良构、规则良构、匹配完整。 -/
def Applicable (g : ConstraintGraph) (m : RuleMatch) : Prop :=
  WellFormedGraph g ∧ WellFormedMatch m

/-- 规则应用结果：产生若干结论谓词。 -/
structure RuleApplication where
  sourceGraph : ConstraintGraph
  match : RuleMatch
  produced : List RuleConclusion
  deriving Repr

/-- 应用结果良构性。 -/
def WellFormedApplication (a : RuleApplication) : Prop :=
  Applicable a.sourceGraph a.match ∧
  a.produced = a.match.rule.conclusions

/-- 结论可被图吸收：结论谓词良构即可进入约束图。 -/
def ConclusionAbsorbable (c : RuleConclusion) : Prop :=
  WellFormedConclusion c

/-- 规则良构时，它的全部结论都可被图吸收。 -/
theorem executable_rule_conclusions_absorbable
    {r : ExecutableRule} (h : WellFormedExecutableRule r) :
    ∀ c ∈ r.conclusions, ConclusionAbsorbable c := by
  intro c hc
  exact h.2.2 c hc

/-- 若规则匹配完整且规则良构，则匹配规则的全部结论可被吸收。 -/
theorem match_conclusions_absorbable
    {m : RuleMatch} (h : WellFormedMatch m) :
    ∀ c ∈ m.rule.conclusions, ConclusionAbsorbable c := by
  exact executable_rule_conclusions_absorbable h.1

/-- 若规则应用良构，则应用产生的所有结论都可被约束图吸收。 -/
theorem application_outputs_absorbable
/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace RuleTemplate

open Ontology
open Predicates
open Constraint

/-- 规则类型，对应 C 侧 `Lv00RuleType`。 -/
inductive RuleType where
  | inference
  | rewrite
  | axiom
  | definition
  | theorem
  | lemma
  | tactic
  | constructor
  deriving DecidableEq, Repr

/-- 规则状态，对应 C 侧 `Lv00RuleStatus`。 -/
inductive RuleStatus where
  | disabled
  | enabled
  | deprecated
  | experimental
  deriving DecidableEq, Repr

/-- 规则优先级。C 侧使用整数，这里抽象为五档。 -/
inductive RulePriority where
  | lowest
  | low
  | normal
  | high
  | highest
  deriving DecidableEq, Repr

/-- 规则条件类型，对应 C 侧 `Lv00ConditionType`。 -/
inductive ConditionType where
  | patternMatch
  | typeCheck
  | valueCompare
  | existsCheck
  | forallCheck
  | custom
  deriving DecidableEq, Repr

/-- 规则变量：名称、类型约束、可选绑定对象。 -/
structure RuleVariable where
  name : String
  typeName : String
  bound : Option LvObj := none
  deriving Repr

/-- 规则条件：目前保留模式、变量名和条件类型，后续可细化为可判定谓词。 -/
structure RuleCondition where
  type : ConditionType
  pattern : String
  variable : String
  deriving Repr

/-- 规则前提：一个谓词模式及其附加条件。 -/
structure RulePremise where
  predicate : PrimPred
  conditions : List RuleCondition := []
  optional : Bool := false
  deriving Repr

/-- 规则结论：可被约束图吸收的本原谓词，附带证明理由。 -/
structure RuleConclusion where
  predicate : PrimPred
  justification : String := ""
  deriving Repr

/-- 可执行规则模板。 -/
structure ExecutableRule where
  id : Nat
  baseKind : BaseAxiomKind
  name : String
  description : String
  type : RuleType
  status : RuleStatus
  variables : List RuleVariable
  premises : List RulePremise
  conclusions : List RuleConclusion
  priority : RulePriority := RulePriority.normal
  dependencies : List Nat := []
  packageName : String := ""
  deriving Repr

/-- 前提良构性。 -/
def WellFormedPremise (p : RulePremise) : Prop :=
  WellFormedPred p.predicate

/-- 结论良构性。 -/
def WellFormedConclusion (c : RuleConclusion) : Prop :=
  WellFormedPred c.predicate

/-- 可执行规则良构性。 -/
def WellFormedExecutableRule (r : ExecutableRule) : Prop :=
  r.baseKind ∈ canonicalKinds ∧
  (∀ p ∈ r.premises, WellFormedPremise p) ∧
  (∀ c ∈ r.conclusions, WellFormedConclusion c)

/-- 规则匹配结果，对应 C 侧 `Lv00RuleMatch`。 -/
structure RuleMatch where
  rule : ExecutableRule
  bindings : List RuleVariable
  confidence : Nat
  matchedPremises : Nat
  complete : Bool
  deriving Repr

/-- 规则匹配良构性：匹配对象对应的规则本身良构。 -/
def WellFormedMatch (m : RuleMatch) : Prop :=
  WellFormedExecutableRule m.rule ∧ m.complete = true

/-- 规则适用性：图良构、规则良构、匹配完整。 -/
def Applicable (g : ConstraintGraph) (m : RuleMatch) : Prop :=
  WellFormedGraph g ∧ WellFormedMatch m

/-- 规则应用结果：产生若干结论谓词。 -/
structure RuleApplication where
  sourceGraph : ConstraintGraph
  match : RuleMatch
  produced : List RuleConclusion
  deriving Repr

/-- 应用结果良构性。 -/
def WellFormedApplication (a : RuleApplication) : Prop :=
  Applicable a.sourceGraph a.match ∧
  a.produced = a.match.rule.conclusions

/-- 结论可被图吸收：结论谓词良构即可进入约束图。 -/
def ConclusionAbsorbable (c : RuleConclusion) : Prop :=
  WellFormedConclusion c

/-- 规则良构时，它的全部结论都可被图吸收。 -/
theorem executable_rule_conclusions_absorbable
    {r : ExecutableRule} (h : WellFormedExecutableRule r) :
    ∀ c ∈ r.conclusions, ConclusionAbsorbable c := by
  intro c hc
  exact h.2.2 c hc

/-- 若规则匹配完整且规则良构，则匹配规则的全部结论可被吸收。 -/
theorem match_conclusions_absorbable
    {m : RuleMatch} (h : WellFormedMatch m) :
    ∀ c ∈ m.rule.conclusions, ConclusionAbsorbable c := by
  exact executable_rule_conclusions_absorbable h.1

/-- 若规则应用良构，则应用产生的所有结论都可被约束图吸收。 -/
theorem application_outputs_absorbable
    {a : RuleApplication} (h :/-
Lv-00 自有理论核心：可执行规则模板

本文件对照 C 侧 `axiom_rule_engine.h` 与 `rule_registry.h` 建立 Lean 级规则模板：
- Lv00RuleVariable  -> RuleVariable
- Lv00RuleCondition -> RuleCondition
- Lv00RulePremise   -> RulePremise
- Lv00RuleConclusion -> RuleConclusion
- Lv00RuleMatch     -> RuleMatch
- 规则注册表 apply_all -> RuleApplication / applyRule 接口

目标不是复刻 C 数据结构细节，而是抽象出可证明的核心性质：
若规则模板、匹配和输入约束图良构，则规则应用产生的结论仍然是良构谓词，
因此可以被 Lv-00 约束图吸收。
-/

import Lv00Formal.Theory.Axioms.Primitive
import Lv00Formal.Theory.Constraint.Graph

namespace Lv00Formal
namespace Theory
namespace Axioms
namespace RuleTemplate

open Ontology
open Predicates
open Constraint

/-- 规则类型，对应 C 侧 `Lv00RuleType`。 -/
inductive RuleType where
  | inference
  | rewrite
  | axiom
  | definition
  | theorem
  | lemma
  | tactic
  | constructor
  deriving DecidableEq, Repr

/-- 规则状态，对应 C 侧 `Lv00RuleStatus`。 -/
inductive RuleStatus where
  | disabled
  | enabled
  | deprecated
  | experimental
  deriving DecidableEq, Repr

/-- 规则优先级。C 侧使用整数，这里抽象为五档。 -/
inductive RulePriority where
  | lowest
  | low
  | normal
  | high
  | highest
  deriving DecidableEq, Repr

/-- 规则条件类型，对应 C 侧 `Lv00ConditionType`。 -/
inductive ConditionType where
  | patternMatch
  | typeCheck
  | valueCompare
  | existsCheck
  | forallCheck
  | custom
  deriving DecidableEq, Repr

/-- 规则变量：名称、类型约束、可选绑定对象。 -/
structure RuleVariable where
  name : String
  typeName : String
  bound : Option LvObj := none
  deriving Repr

/-- 规则条件：目前保留模式、变量名和条件类型，后续可细化为可判定谓词。 -/
structure RuleCondition where
  type : ConditionType
  pattern : String
  variable : String
  deriving Repr

/-- 规则前提：一个谓词模式及其附加条件。 -/
structure RulePremise where
  predicate : PrimPred
  conditions : List RuleCondition := []
  optional : Bool := false
  deriving Repr

/-- 规则结论：可被约束图吸收的本原谓词，附带证明理由。 -/
structure RuleConclusion where
  predicate : PrimPred
  justification : String := ""
  deriving Repr

/-- 可执行规则模板。 -/
structure ExecutableRule where
  id : Nat
  baseKind : BaseAxiomKind
  name : String
  description : String
  type : RuleType
  status : RuleStatus
  variables : List RuleVariable
  premises : List RulePremise
  conclusions : List RuleConclusion
  priority : RulePriority := RulePriority.normal
  dependencies : List Nat := []
  packageName : String := ""
  deriving Repr

/-- 前提良构性。 -/
def WellFormedPremise (p : RulePremise) : Prop :=
  WellFormedPred p.predicate

/-- 结论良构性。 -/
def WellFormedConclusion (c : RuleConclusion) : Prop :=
  WellFormedPred c.predicate

/-- 可执行规则良构性。 -/
def WellFormedExecutableRule (r : ExecutableRule) : Prop :=
  r.baseKind ∈ canonicalKinds ∧
  (∀ p ∈ r.premises, WellFormedPremise p) ∧
  (∀ c ∈ r.conclusions, WellFormedConclusion c)

/-- 规则匹配结果，对应 C 侧 `Lv00RuleMatch`。 -/
structure RuleMatch where
  rule : ExecutableRule
  bindings : List RuleVariable
  confidence : Nat
  matchedPremises : Nat
  complete : Bool
  deriving Repr

/-- 规则匹配良构性：匹配对象对应的规则本身良构。 -/
def WellFormedMatch (m : RuleMatch) : Prop :=
  WellFormedExecutableRule m.rule ∧ m.complete = true

/-- 规则适用性：图良构、规则良构、匹配完整。 -/
def Applicable (g : ConstraintGraph) (m : RuleMatch) : Prop :=
  WellFormedGraph g ∧ WellFormedMatch m

/-- 规则应用结果：产生若干结论谓词。 -/
structure RuleApplication where
  sourceGraph : ConstraintGraph
  match : RuleMatch
  produced : List RuleConclusion
  deriving Repr

/-- 应用结果良构性。 -/
def WellFormedApplication (a : RuleApplication) : Prop :=
  Applicable a.sourceGraph a.match ∧
  a.produced = a.match.rule.conclusions

/-- 结论可被图吸收：结论谓词良构即可进入约束图。 -/
def ConclusionAbsorbable (c : RuleConclusion) : Prop :=
  WellFormedConclusion c

/-- 规则良构时，它的全部结论都可被图吸收。 -/
theorem executable_rule_conclusions_absorbable
    {r : ExecutableRule} (h : WellFormedExecutableRule r) :
    ∀ c ∈ r.conclusions, ConclusionAbsorbable c := by
  intro c hc
  exact h.2.2 c hc

/-- 若规则匹配完整且规则良构，则匹配规则的全部结论可被吸收。 -/
theorem match_conclusions_absorbable
    {m : RuleMatch} (h : WellFormedMatch m) :
    ∀ c ∈ m.rule.conclusions, ConclusionAbsorbable c := by
  exact executable_rule_conclusions_absorbable h.1

/-- 若规则应用良构，则应用产生的所有结论都可被约束图吸收。 -/
theorem application_outputs_absorbable
    {a : RuleApplication} (h : WellFormedApplication a) :
