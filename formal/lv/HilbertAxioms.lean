/-  HilbertAxioms: 整合五组 Hilbert 几何公理
   (1) 关联公理 (Incidence)
   (2) 序公理 (Betweenness + Order)
   (3) 合同公理 (HCongruence)
   (4) 平行公理 (Parallel)
   (5) 连续性公理 (Continuity)

   由 MetricSpace + Parallel（含 Incidence）+ Order（含 Betweenness）+
     HCongruence + Continuity 构成一个 Hilbert 平面 -/

import lv.Basic
import lv.Incidence
import lv.Betweenness
import lv.Congruence
import lv.Parallel
import lv.Continuity
import lv.Order

namespace lv.HilbertAxioms

/-- Hilbert 平面：满足全部五组公理的几何结构
   - MetricSpace 提供距离度量
   - Parallel 内含 Incidence 关联公理
   - Order 内含 Betweenness 序公理
   - HCongruence 合同公理
   - Continuity 连续性公理 -/
class HilbertPlane (Point Line : Type) extends
  lv.Basic.MetricSpace Point,
  lv.Order.Order Point,
  lv.Congruence.HCongruence Point,
  lv.Parallel.Parallel Point Line,
  lv.Continuity.Continuity Point

end lv.HilbertAxioms
