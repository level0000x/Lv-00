# 数值抽象层 & MPFR 外包 · 文档索引

> 状态：索引（2026-09-03，批次 261）。汇总「Lv-00 数值抽象层 + GMP/MPFR 复杂度外包」
> 相关文档与批次进展，便于定位。

## 一、权威设计

| 文档 | 内容 | 状态 |
|:---|:---|:--:|
| `dependency-policy.md` | 外部依赖策略（默认底层 GMP/MPFR/MPC(+MPFI 待装)、四闸判据、三级分类/许可红线） | 正式（批次 231） |
| `number-abstraction-layer-design.md` | lvNumber 不透明句柄 + 两级池 + 精确提升（ND-1..7）+ 0 期进展 | 正式（批次 232） |
| `domain-migration-roadmap.md` | 公共头 GMP 零泄漏跟踪 + 整簇分期 S1-S5 + S1 前置表示/段设计 | 正式（241/242/250） |
| `p1-s3-execution-plan.md` | P1(S3) 分层 + 读者面建图结论（全引擎 ~150 文件）+ 范围 A/B/C | 登记（249/250） |
| `batch-c-reverify-plan.md` | 批次 C：MPFR(REAL) 高精度复核接线（C1-C3 交付，C4 可选） | C1-C3 ✅（255-257） |
| `dsl-remaining-work.md` | DSL 线待执行登记（冻结，代码未动） | 冻结（248） |

## 二、实施状态（TASK_CONTEXT 批次 232-260）

- **依赖地基**（233/245/246/258）：GMP/MPFR/MPC REQUIRED + 静态优先限 WIN32（跨平台共享修复）
- **lvNumber 内核**（234/235/236/243）：两级池 + GMP allocator 接线 + Rational 别名 + 连续段原语
- **REAL(MPFR)**（251-253/255-257）：kind/存储/工厂/算术/精度上下文 + real_verify +
  fptaylor REAL 重算 + double 中心复核判定
- **MPFR 分配器接线**：回退（API 缺/线程安全，批次 260）
- 全量测试基线：303；主 CI + Python Bindings 全绿

## 三、待决 / 下一步候选

- 范围选择 A（MPFR/新代码隔离抽象）/ B（窄 retro）/ C（全引擎 zero-GMP）——见 p1 plan
- C4（复核信号入 fptaylor 信任域，可选）、P6 MPFI、DSL 解冻、你的实际优先级
