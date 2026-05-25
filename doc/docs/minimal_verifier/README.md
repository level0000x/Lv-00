# Lv-00 最小验证器 (Minimal Verifier)

受 **mm0 / Metamath** 验证器启发，为 Lv-00 约束图系统创建的极简验证原型。

## 设计哲学

### 极简即强壮 (Minimal is Robust)

mm0 的验证器不到 2000 行 C 代码，它之所以可信，恰恰因为它做的事极少：

- **不内建任何逻辑或推理规则**。所有"知识"（几何公理、约束语义、推理步骤）均以**声明形式**存在于 `ConstraintGraph` 和 `AxiomPackage` 数据结构中。
- 验证器只做**替换检查**（substitution check）：确认被引用的东西确实已被声明，确认参数数量对得上。
- 代码越少，bug 越少。本验证器约 400 行 C 代码，可在数分钟内通读并审计全部逻辑。

### 独立验证器 (Independent Verifier)

本验证器有两种编译模式：

1. **独立模式**：不依赖 Lv-00 主引擎的任何 `.c` 文件。所有类型定义内联于源文件中，仅需 C 标准库即可编译。这保证验证器不会被主引擎的改动意外破坏。

2. **链接模式**：直接 `#include "constraint_graph.h"` 和 `"axiom_pkg.h"`，链接 Lv-00 主引擎。删除源文件中的内联类型定义段即可。

## 验证范围（三遍扫描）

| 遍次 | 检查内容 | 错误类型 |
|------|----------|----------|
| 第一遍 | **节点声明-引用一致性** | 约束的每个 participant ID 必须在 `nodes[]` 中存在；函数块的 `internal_nodes`、`input_port_ids`、`output_port_ids` 同理 |
| 第二遍 | **约束类型参数数量** | `INCIDENCE` 必须 2 个参与者，`BETWEENNESS` 必须 3 个，等等 |
| 第三遍 | **公理包一致性** | 模板名非空无重复，`param_count` 与 `param_desc_count` 匹配，依赖引用节点存在性 |

### 显式不验证的内容（留给上层系统）

- 几何正确性（点是否真的在线段上）
- 约束冲突 / 冗余判断
- 类型推演与合一
- 信任颜色变换逻辑
- 公理包内容哈希比对

## 编译与运行

### 独立编译（推荐用于审计）

```bash
gcc -std=c99 -Wall -Wextra -pedantic minimal_verifier.c -o minimal_verifier
./minimal_verifier --self-test
```

### 与 Lv-00 链接

```bash
gcc -std=c99 -I../../include minimal_verifier.c \
    ../../src/core/constraint_graph.c \
    ../../src/axiom/axiom_pkg.c \
    -o minimal_verifier -lgmp -lm
```

然后从 Lv-00 主程序调用：

```c
#include "constraint_graph.h"
#include "axiom_pkg.h"

// ... 构建 ConstraintGraph 和 AxiomPackage ...

MvReport report;
minimal_verifier_validate(graph, pkg, &report);
minimal_verifier_report_print(&report);

if (!report.passed) {
    // 处理结构错误
}
```

## API

### `void minimal_verifier_validate(const ConstraintGraph *graph, const AxiomPackage *pkg, MvReport *rpt)`

核心入口。对约束图执行三遍验证扫描。

- `graph`: 待验证的约束图（可为 NULL，将报告错误）
- `pkg`: 关联的公理包（可为 NULL，跳过公理包检查）
- `rpt`: 输出验证报告（调用者分配，不可为 NULL）

### `void minimal_verifier_report_print(const MvReport *rpt)`

将验证报告打印到 stdout。

### `MvReport` 结构

```c
typedef struct {
    int err_c, warn_c;                          // 错误和警告数量
    char errs[128][256];                        // 错误消息数组
    char warns[128][256];                       // 警告消息数组
    bool passed;                                // 全部通过则为 true
} MvReport;
```

## 约束类型参数数量

| 类型 | 预期参与者 | 语义 |
|------|-----------|------|
| `INCIDENCE` (0) | 2 | point_id, line_or_region_id |
| `BETWEENNESS` (1) | 3 | p1_id, p2_id, p3_id |
| `INTERSECTION` (2) | 3 | line1_id, line2_id, result_point_id |
| `CONTAINMENT` (3) | 2 | inner_id, outer_id |
| `CONNECTION` (4) | 2 | src_port_id, dst_port_id |

## 自测

自测包含 6 个测试用例，覆盖：

1. NULL 图检测
2. 空图通过验证
3. 未声明节点引用检测
4. 约束参数数量错误检测
5. 正确图的通过验证
6. 公理包模板名重复检测

所有测试均手工构造数据结构，不依赖 Lv-00 主引擎。

## 与 mm0 的对应关系

| mm0 概念 | Lv-00 最小验证器对应 |
|----------|---------------------|
| 项/公式的声明-引用检查 | 节点 ID 的声明-引用检查 (Pass 1) |
| 公理/定理的参数数量检查 | 约束类型的 participant_count 检查 (Pass 2) |
| 外部引用的完整性 | 公理包模板/依赖引用检查 (Pass 3) |
| 替换检查（substitution） | 仅检查 ID 存在性与数量匹配，不做语义替换 |
| ~2000 行 C | ~400 行 C |

## 文件结构

```
docs/minimal_verifier/
  minimal_verifier.c    # 验证器源码（~400 行）
  README.md             # 本文件
```
