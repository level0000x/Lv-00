# 批次 C 子方案：MPFR(REAL) 高精度复核/信任通道接线

> 状态：**C1-C3 已交付（批次 255-257），C4 可选待启（2026-09-03）**。
> 背景：浮点误差域（float_error/fptaylor）与信任颜色语义紧耦合，直接改内部风险高；
> 本方案把 REAL 复核作为**旁路验证通道**分阶段接入，避免破坏现有 double 区间/信任语义。

## 一、目标（C 阶段结束态）
对约束图/表达式做浮点误差验证时，可用 **REAL（mpfr 高精度）重算中心值**作为 ground-truth，
与 double 结果对比得出绝对/相对误差与**复核通过/降级**判定（供 TrustColor 决策），而非仅
依赖 double 区间一阶界。

## 二、接线切片（每片可独立 + 全量门禁）

| 片 | 内容 | 验收 |
|:--:|:---|:---|
| C1 | 新增独立复核 util（不动 float_error 内部）：`real` 中心值重算入口——输入表达式求值回调与 REAL 参考值，输出 abs/rel 误差 + 阈值判定 | 单测 + 全量 |
| C2 | float_error 导出入口增加可选 REAL 复核结果（新增 API，不改旧签名语义） | fptaylor 测试绿 |
| C3 | 信任决策接线：复核误差超阈 → 建议降级（决策钩子，不硬改现有 TrustColor 规则） | 相关域测试绿 |
| C4 | 端到端：示例/文档演示 REAL 复核路径 | ctest 全绿 |

## 三、约束/风险
- 不改 float_error/fptaylor/gappa 现有 double 区间语义（旧路径行为冻结）；
- REAL 复核为**新增旁路**：默认关闭（opt-in），避免信任语义漂移；
- gappa/float_error 仍自研分析逻辑，仅底层「高精度真值」换 REAL（与 dependency-policy C 定位一致）。

## 四、动工条件
- C1（独立 util + 单测）可在批准后立即做（不动引擎信任域）；
- C2-C4（进入 float_error 信任域 API）建议按 C1 稳定后再接。
