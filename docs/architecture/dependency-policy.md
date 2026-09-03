# Lv-00 外部依赖策略（代码标准 v1.0）

> 状态：**正式**（2026-09-03 用户批准；代码标准之一）
> 日期：2026-09-03（批准）
> 性质：代码标准——外部依赖的默认底层、外包边界判据、外部材料三级分类与许可红线
> 标准入口：CONTRIBUTING.md「代码规范 · 外部依赖规范」
> 标准变更：需评审登记后方可修改

---

## 0. 一句话策略

**只外包调用栈底部、语义自足固定的数学库；中层一律自研但保持「外包就绪」的窄接口；外部材料按三级分类管理（依赖级 / 参考实现级 / 可抄实现级）。**

---

## 决策登记（2026-09-03 批准）

| ID | 决策 | 状态 |
|:---|:---|:---:|
| D-0903-1 | 草案评审通过并正式化：本文档升级为**代码标准**（入口 CONTRIBUTING「代码规范」） | ✅ |
| D-0903-2 | **GMP 家族四件套 = 默认底层依赖集**：GMP / MPFR / MPFI / MPC；非 WASM 构建全链查找，WASM 经 `lv_NO_GMP` / `lv_NO_MPFR` guard | ✅ |
| D-0903-3 | 「装了 ≠ 用了」仅约束**代码层**：`#include` 按真实消费者逐个接入（MPFR 首批） | ✅ |
| D-0903-4 | 参考实现级 / 可抄实现级三级分类与许可红线采纳（§3-§6） | ✅ |
| D-0903-5 | 实施拆批 A → B1 → C → D，每批按惯例登记 + 测试全绿 | A ✅（批次 233）/ B1-C-D ⏳ |
| D-0903-6 | 实施口径（批次 233）：非 WASM 构建 **GMP/MPFR/MPC REQUIRED（Windows 静态优先）**、**MPFI 可选 + `lv_HAS_MPFI`**（msys2 mingw64 仓库缺包，首个消费者立项时转 REQUIRED）；pkg-config `Requires: gmp mpfr mpc` 与 CPACK 同步；WASM 经 `lv_NO_MPFR` guard | ✅ 2026-09-03 |

---

## 1. 外包边界判据（四闸）

一个库是否值得引入为**正式依赖**，须同时通过四闸：

| # | 闸 | 判据 | 反例 |
|:--:|:---|:---|:---|
| 1 | 位置 | 位于调用栈底部（纯数值原语层） | SAT/SMT、ODE、CGAL 等中层库 |
| 2 | 语义 | 数学/格式语义自足且固定，不随本项目演进 | 任何与约束图/TrustColor/证明格式耦合的库 |
| 3 | 升级 | 升级外部库**零协同面**——不会被迫连带改我们的语义/格式/证明 | proof-Merkle 换库 = 改格式 + 改 Lean 证明 |
| 4 | 痛苦 | 自维护痛苦足够大，值得引入 | sha256 自研仅 ~百行且稳定 |

> **核心直觉**：升级外部库时，会不会被迫连带改我们的东西？底部数值层永远「不会」；再往上一层几乎总是「会」。

**推导出的纪律**：
- 只通过四闸的库才进入依赖级；
- 未通过但算法/设计有价值的 → 参考实现级（只读，不碰代码）；
- 未通过且许可允许的 → 可抄实现级（vendor/移植，保留版权头）；
- 强传染许可（GPL 系）→ 只读参考，绝不可抄；
- 自研新内核一律按「纯函数窄接口」实现，保持未来可替换性（外包就绪），**但不实际外包**。

---

## 2. 依赖级：GMP 家族 = 默认底层（四件套，D-0903-2）

家族采纳的共性与 GMP 完全同姿态：底部、语义固定、升级零协同面、MSYS2 同源同装、LGPL 已有先例。

| 成员 | 职责 | 依赖 | 采纳状态 | 代码接入状态 |
|:---|:---|:---|:---|:---|
| GMP | 整数/有理数（mpz/mpq） | — | ✅ 已接入 | ✅ 已使用 |
| **MPFR** | 任意精度浮点 + 正确舍入 + 超越函数 | GMP | ✅ 默认底层成员 | 🔧 首批代码接入对象（批次 A/B1） |
| MPFI | 任意精度区间算术 | MPFR | ✅ 默认底层成员 | ⛔ 代码未接入（零冲突，已核） |
| MPC | 任意精度复数 | GMP+MPFR | ✅ 默认底层成员 | ⛔ 代码未接入（零冲突，已核） |

> 已核实（2026-09-03，grep core/）：`mpc_*` / `mpfi_*` / `<complex.h>` / `mpf_t` 全部**零引用**——MPFI/MPC 在代码侧为绿字段，无撞车。
> 潜在消费者：MPFI ← 任意精度区间（B2 宽改 / 高精度复核角色②）；MPC ← `preset_complex_analysis.h` 复杂分析预设（未来）。

### 2.1 默认底层规则（D-0903-2 / D-0903-3）

- **默认底层依赖集**：GMP/MPFR/MPFI/MPC 四件套为 Lv-00 标准底层；非 WASM 构建按 GMP 同策略全链查找（静态优先 `lib*.a`）；WASM 构建经 `lv_NO_GMP` / `lv_NO_MPFR` guard 整体跳过；
- **「装了 ≠ 用了」**：构建/CI/导出层全链就位（find_library、pkg-config Requires、CPACK、CI 三平台安装步骤）；**代码层** `#include` 按真实消费者逐个开——MPFR 为首个接入对象（批次 A/B1）；MPFI/MPC 等到对应立项或出现真实需求再 include，**不引死依赖**。

### 2.2 落地批次形状（待立项拆分）

| 批 | 内容 | 说明 |
|:--:|:---|:---|
| A | 依赖地基 | CMake 镜像 GMP 段接入 MPFR（静态 libmpfr.a 优先）、pkg-config/CPACK/导出同步、WASM guard、CI 各平台依赖；MPFI/MPC 仅入链不入码 |
| B1 | 试点：区间算术算子级替换 | `interval_arith.c`（~1000 行，double+nextafter 手搓）端点计算改走 MPFR 定向舍入（lo=RNDD / hi=RNDU）；`lvInterval` 结构**不动**、消费方零改动；sin/cos/exp/log 区间正确化 |
| C | 浮点误差验证族算子级替换 | `float_error.c`/`fptaylor_eval.c`/`gappa_*` 的底层运算改用 MPFR 建模（角色①：IEEE 语义精确模拟；角色②：高精度真值复核 → TrustColor 降级通道）——**分析逻辑自研不外包**，仅换底部算子 |
| D | 十进制/字面量路径精确化 | DECIMAL 字面量文本 → GMP `mpq_set_str` 一步精确（base 10 支持小数串）；删除 S1 手搓 double→分数循环；**落点 = loader（core 顶层），不进 layer1**（layer1 零 GMP 约束已核） |

> 横切：S1 在途（未提交，`lv_loader_engine.c`/`lv_parser.c`/`test_lv_parser.c`）与 D 同路径——**D 排在 S1 收尾之后**，避免同一函数上两套改动打架。
> 分层约束：layer1_parser 零 GMP（已核 grep）；MPFR 接入点 = layer3（interval/symbolic）+ core 顶层 loader + layer4 数值后端，不侵入 layer1/2。

---

## 3. 参考实现级（只读：读思想/算法/测试向量，代码一行不进）

> 用途：我们的自研实现对照其算法与测试向量做正确性/完整性审查；不链接、不 vendor。

| 库 | 领域 | 参考价值 | 许可（只读无碍，记录在案） |
|:---|:---|:---|:---|
| Arb / FLINT | 区间/球算术 | interval sin/cos 单调域处理、端点外扩技巧、误差界测试向量 | LGPL 家族 |
| Z3 / CVC5 | SMT | 证明语义、多理论组合设计 | MIT / BSD-3 |
| FPTaylor / Gappa | 浮点误差分析 | 误差分析框架思想（float_error.h 注释已声明借鉴） | 研究工具（实施前核实） |
| CGAL / Core2 | 实代数/精确几何 | 实代数组织、精确谓词分层 | LGPL/GPL 混合 / LGPL |
| Singular | Gröbner/计算机代数 | 多项式引擎调度组织 | **GPL-2 ⚠️ 只读** |
| SUNDIALS | ODE | 自适应步进设计 | BSD-3 |
| CUDD | BDD | BDD 结构与内存管理 | 宽松自定义 |

## 4. 可抄实现级（许可允许：vendor/移植，必须保留版权头）

> 这些库未通过四闸（协同面或痛苦不足），但其实现可作代码来源。抄入时保留原版权头与许可声明，登记出处。

| 库 | 领域 | 许可 | 备注 |
|:---|:---|:---|:---|
| yyjson / cJSON | JSON 解析 | MIT | 若未来 JSON 性能/维护痛点升级再 vendor |
| CaDiCaL / Minisat | CDCL SAT | MIT | CDCL 细节参考/移植候选 |
| Clipper2 | 多边形布尔 | BSL-1.0 | CSG 多边形路径候选 |
| sha256 单文件（公共域） | 哈希 | 公共域/CC0 | 现自研够用（痛苦不足），仅记录 |
| OpenBLAS | BLAS 内核 | BSD-3 | 数值后端候选内核参考 |

## 5. 许可矩阵与红线

| 许可 | 可链接 | 可抄代码进 MIT 库 | 处置 |
|:---|:---:|:---:|:---|
| LGPL（GMP/MPFR/MPFI/MPC/Arb） | ✅（已有静态链 GMP 先例） | ❌ | 依赖级只链接；参考只读 |
| GPL（Singular/GSL 等） | ⚠️ 需决策（可选后端模式） | ❌ | 默认参考只读，不引入 |
| MIT / BSD / BSL / 公共域 | ✅ | ✅ 保留版权头 | 依赖或可抄实现级 |
| 强传染 + 无必要（GSL） | — | — | 不采用也不参考 |

**规则**：LGPL/GPL 代码**一行都不抄**进 MIT 代码库（抄 = 派生污染）；MIT/BSD/公共域可抄，**抄必带出处**。

## 6. 不采用清单（及理由）

| 项 | 理由 |
|:---|:---|
| GSL（GPL-3+ 强传染） | 许可不可接受，数值需求可由 MPFR/家族 + 自研覆盖 |
| CGAL/Core2 整链引入 | C++ 桥层成本 + 协同面；保留为参考 |
| 中层算法库（SAT/SMT/ODE/CA…） | 协同升级判据失败（§1）——非技术问题，是演进耦合问题 |

## 7. 实施期决策点（方向已批，实施时细化）

1. B 试点选 B1（窄改，`lvInterval` 结构不动）——实施时确认；B2/MPFI（任意精度区间）时机后续单独立项；
2. C 先做角色②（高精度复核 → TrustColor 通道）还是角色①（IEEE 语义建模）——实施时确认；
3. D 与 S1 收尾合并批次的编排——实施时确认；
4. 批次节奏：A → B1 → C → D（D-0903-5），每批按惯例登记 + 测试全绿。

---

## 附：审计证据索引（2026-09-03，只读）

- GMP include 地图：layer1 零 GMP；layer2（`lv_str_utils.c`）；layer3（symbolic/constraint/algebraic）；layer4（mv_polynomial/nt_*）；core 顶层（impl_*/loader）
- 手搓复杂度体量：`interval_arith.c` ~1000 行（double）；`float_error.c` 850 + `fptaylor_eval.c` 304 + `gappa_dsl.c` 847 + `gappa_propagate.c` 905（均 double 基底）；`algebraic_number_*` ~1300 行
- 环境：MSYS2 mingw64 已装 `mpfr.h`/`gmp.h`（同目录）；构建 = MinGW gcc + Ninja；GMP 静态库优先策略已有（CMakeLists ~L320-380）
- 现有「可选后端」先例：`lv_ENABLE_SINGULAR`/`lv_ENABLE_CUDA`/`lv_ENABLE_HIP` + `LV_HAS_*` 宏（CMakeLists ~L41-77）
- 家族零引用核验：core/ 无 `mpc_*`/`mpfi_*`/`<complex.h>`/`mpf_t` 命中
