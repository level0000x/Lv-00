# P1 执行方案：S3 quadratic 整簇 + 工具面 opaque（分层方案）

> 状态：**登记（2026-09-03，批次 249）**——P1 立项通过；本方案为代码动工前的分层
> 与风险前置。参考 `domain-migration-roadmap.md` S3/S2 与 `number-abstraction-layer-design.md`。

## 一、目标（P1 结束态）
`Quadratic`（symbolic_coord.h）字段 `a/b` 与 `Rational`（= lvRational）不再经**公共
头暴露 GMP 布局**：lvRational opaque 化（句柄 + 值访问器），quadratic/代数数/坐标域
全部经访问器/API 使用；lv_number 池句柄承载存储（ND-4/ND-5）。

## 二、风险前置（本批调查结论，批次 249）
- 粗扫 `->value` 命中 89+ 文件、`->a->/->b->` 命中 22+ 文件，**含大量非 Rational/Quadratic
  的通用字段**（expr->a、cfg.value 等）——不能据此直接改；
- 真读者面须逐对象（SymbolicCoord data.rational / data.quadratic / Algebraic /
  lvRational* 形参）**精确建图**后方可迁移；
- Quadratic ↔ algebraic_number_quadratic ↔ coord 比较/序列化/生命周期为紧耦合链，
  单文件切碎会产生半态。

## 三、分层切片（每片独立批 + 全量重建/ctest 门禁）

| 片 | 内容 | 验收 |
|:--:|:---|:---|
| P1a | **读者面精确建图**：grep 精确到 SymbolicCoord.data.* / Quadratic* / lvRational* 类型使用点；产出受影响文件+行清单 | 清单入库 |
| P1b | **lvRational opaque 化**：layout 收进实现 + 新增值访问器/构造（含既有 `value` 直读点经访问器迁移，按 P1a 清单逐文件） | rational.h 无 mpq_t/mpz_t token |
| P1c | quadratic.c 内部迁移到访问器/lvNumber | quadratic 测试绿 |
| P1d | algebraic_number_quadratic / coord(compare/lifecycle/serialize) 消费点收敛 | 代数数/坐标测试绿 |
| P1e | S4 geometry_transform（60 mpq 字段 → 坐标句柄类型）承接 | geometry 测试绿 |
| P1f | 收尾：gmp.h include 白名单、`.value` 清零 grep、登记 | 门禁全绿 |

## 四、动工条件
- P1a（建图）可在本方案批准后立即做（只读产出）；
- P1b 起为**代码重构**，涉及核心几何/代数语义——建议由用户或明示授权开启；
  无人值守下不擅自对核心代数域做破坏性批次（避免大 diff 难复核）。

## 五、与本项目其余线关系
- P2（S1 系数段）可与 P1b 工具面共享访问器成果；
- P5（MPFR 表示）依赖 P1b/c 后抽象层存储稳定；
- DSL 线冻结（见 dsl-remaining-work.md），互不影响。

## 六、P1a 建图结论（批次 250，范围重估）
- include `rational.h` / `symbolic_coord.h` 的 .c **遍布全 core ~150 文件**（geometry_compress、
  constraint_graph、euclidean_geometry、geometry_transform、solver…），Rational 型 API 已渗入
  引擎几乎所有域；
- 故「Rational 公共 API 全量 opaque/公共头零 GMP」= **全引擎级重构**（数十批、超大 diff、
  长时间无人值守期），超出 S3/P1 合理范围，属**战略级**再架构。

## 七、范围修订建议（待用户知情拍板）
- 方案 A（推荐，聚焦原诉求）：**lv_number/MPFR 作为隔离抽象，只服务新增/外部复杂度外包
  （P5 MPFR 表示、批次 C 复核通道）**；不对既有 Rational 公共 API 做全量 retro（高风险/
  低收益）；
- 方案 B：窄 retro——仅 expr_canon 已完成、quadratic 若确有 API 纯净则做（不 opaque、
  不追公共头零 GMP）；
- 方案 C：仍坚持全量 zero-GMP（接受数十批全引擎重构，分期长跑）。
> 结论：建议 A（或 B）；C 需明确收益/成本认账后启动。
