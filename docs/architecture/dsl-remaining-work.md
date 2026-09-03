# Lv-00 DSL 线待执行登记（代码冻结，仅文档）

> 状态：**登记待执行（2026-09-03，批次 248）**——用户指示 DSL 代码暂不实施，仅书面登记，
> 供后续（用户或代理）按此清单执行。**本文档不构成当前代码改动。**
> 前置参考：`dsl-syntax-sugar-design.md`、`dsl-pfl-reference.md`、`dsl-syntax-baseline.md`、
> `TASK_CONTEXT.md`（批次 227 DSL 第一批、批次 228 S1/S7/S2/S5/S8）、R4 severity 现状。

---

## 一、已完成（供对齐基线）

| 项 | 说明 |
|:---|:---|
| R4 severity phase 1-3 | `LvDiagSeverity`（ERROR=0 默认）、sema 通道 `lv_sema_error_severity`、parser 填 ERROR+fix_hint |
| S1 坐标字面量 / S7 自动命名 / S2 管道 / S5 关键字参数等号 / S8 列表字面量 | 批次 228（落地 lv 家族） |

## 二、待执行（本文档登记，未实施）

### 2.1 诊断基础（原 R4 phase4 拆出）

1. **`LvDiagCode` 诊断码枚举**：按错误类别定义通用/具名码；`LvParseError` 增 `code` 字段；
   在 parser `expect`/`parser_error`/sema 记录点默认打码；`lv_diag_code_name` 命名；
   契约测试补 `result.errors[i].code` 断言。
2. **`#!suppress <code>` 指令**：lexer/parser 识别源内抑制某类诊断 → 记录 `LV_DIAG_INFO`/
   降级；需先有 1 的码体系。
3. **loader 汇总**：多源解析/语义诊断按文件聚合输出（含 suppress 生效说明）。

### 2.2 错误恢复（R5）

4. `@` / `#!` 恢复点增强（现 `synchronize` 仅分号/语句关键字基础）。

### 2.3 语法糖（第一批余项 → 后续批次）

5. S3 解构赋值 `let (P,Q)=…`（+1 LET 扩展/多结果）；
6. S4 字段访问 `A.x`（+`.`、FIELD_ACCESS）；
7. S6 默认参数（构造形参表）；
8. 第二批 S9-S15 / 第三批 S16-S24（中缀约束、have/show、路径、命名空间、模板宏等——
   逐批立项，见 dsl-syntax-sugar-design.md）。

### 2.4 其它

9. MPC 文本解析格式差异（复数 DSL 若启用需定文本规范，见批次 246 遗留）。

## 三、执行约定（重启该线时）

- 每项独立小批、登记（沿用 TASK_CONTEXT 批次制）；全量重建 + ctest 门禁；
- 与抽象层主线（P1-P7）冲突时先并入对应立项或排队；
- 本清单变更须更新本文档并登记。

> 冻结决定：DSL 代码当前不动（2026-09-03 用户指示）；待用户明示重启。
