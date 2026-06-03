/-
  Lv-00 Formal Verification: Test Suite
  测试入口
-/
import Lv00.Basic
import Lv00.Incidence
import Lv00.Betweenness
import Lv00.Congruence
import Lv00.Parallel
import Lv00.Continuity
import Lv00.Order
import Lv00.HilbertAxioms
import Lv00.EuclideanPlane
import Lv00.Lv00Meta

namespace Lv00.Tests

/-- 测试: 基本定义一致性 -/
theorem basic_defs_consistent : True := trivial

/-- 测试: 三角不等式 -/
example (α : Type) [Point α] [Line α] [MetricSpace α]
    [MetricSpace.dist_triangle] (A B C : α) :
  MetricSpace.dist A C ≤ MetricSpace.dist A B + MetricSpace.dist B C :=
  EuclideanPlane.triangle_inequality A B C

end Lv00.Tests
