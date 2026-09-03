# Lv-00 域迁移路线图（公共头 GMP 零泄漏跟踪 + 整簇分期）

> 状态：**正式（2026-09-03，批次 241）**——数值抽象层（number-abstraction-layer-design.md）
> 域迁移的执行路线图；每簇独立立项、全量重建 + ctest 门禁。
> 目标：公共头不再出现 GMP 类型（mpq_t/mpz_t…），`gmp.h`/`mpfr.h` include 收敛到
> 抽象层唯一实现点，为外部库升级/MPFR 表示接入留隔离带。

---

## 0. 进度跟踪：公共头 GMP 泄漏清单（2026-09-03 基线）

| 公共头 | GMP 类型计数 | 归属簇 | 状态 |
|:---|:--:|:---|:--:|
| `geometry_transform.h` | 60 | S4 | ⏳ |
| `nt_number_theory.h` | 26 | S1 | ⏳ |
| `mpz_poly.h` | 14 | S1 | ⏳ |
| `coeff_pool.h` | 12 | S2 | ⏳ |
| `rational.h` | 8 | S2 | ⏳ |
| `nt_polynomial.h` | 5 | S1 | ⏳ |
| `symbolic_coord.h` | 3 | S3/S5 | ⏳ |
| `lv_numeric.h` | 3 | S2 | ⏳ |
| `expr_canonical.h` | 3 | S1 | ⏳ |
| `lv_str_utils.h` | 2 | S2 | ⏳ |
| `groebner_engine.h` | 2 | S5 | ⏳ |
| `inequality_reasoning.h` | 2 | S5 | ⏳ |

**已清零**：`lv_number.h`（0，批次 234/237）——模板域 `expr_canon.h`（0，批次 237）。

---

## 1. 分期（按依赖与边界，每簇一次立项）

| 簇 | 内容 | 关键点 | 映射（设计 §4） |
|:--:|:---|:---|:--:|
| **S1 系数数组族** | `nt_polynomial`/`mpz_poly`/`expr_canonical` + `nt_number_theory` 数组形态 | 多项式系数「整批数值」→ **0e 连续段原语同批落地（ND-5）**；含 test/ 直读排查 | 期 3（+0e） |
| **S2 工具面** | `rational.h`（lvRational opaque 化或访问器收敛）/`lv_numeric.h`/`lv_str_utils.h`/`coeff_pool.h` | 函数级 mpq 参数收敛；`coeff_pool` 并入池设施 | 期 2 |
| **S3 quadratic 整簇** | `quadratic.c` + `algebraic_number_quadratic` + coord 比较/序列化读方 | 已实测 quadratic 全走 Rational API（25 处 a/b 无 direct value）；依赖链构成整簇，不单文件切碎 | 期 4 |
| **S4 geometry_transform** | 60 处 mpq 内嵌字段 + API | 结构体内嵌数值 → 坐标句柄类型 | 期 4 |
| **S5 终端大域** | `symbolic_coord` 值域 / `constraint_graph` 节点值 / `solver` / `groebner_engine` / `inequality_reasoning` | 最大、依赖前述各簇 | 期 5 |

**每簇硬门禁**：
1. grep（core + test + module + examples）直读消费点清零/同步更新；
2. 全量重建 0 error/0 warning；
3. ctest 全绿 + 涉簇契约测试**零改动或按语义等价显式更新**（批次 238 教训：本地必须全量重建后再跑 ctest）。

---

## 2. 簇内推进节奏（以 S1/S3 为例）

- **S1**：先出「系数表示设计」（句柄数组 vs 段基址+偏移），与 0e 段原语一同评审 → 逐文件迁移（nt_number_theory 需从 mpz 语义切到 lvNumber int/rational 的表示决策，含大整数——lvNumber INTEGER 仅 int64 inline，**需先扩 mpz 表示位**，此为 S1 前置设计点）；
- **S3**：quadratic 字段改为不透明句柄 → algebraic_number_quadratic 同步 → coord 比较/序列化消费点收敛（可用别名过渡期桥接）。

---

## 3. 遗留与依赖

- **S1 前置**：lvNumber 尚无任意精度整数（INTEGER=int64）表示 → mpz 表示位（RATIONAL 的分子分母已可承载 mpz？lvRational mpq 已任意精度；S1 可把「整数系数」表示为 RATIONAL(k,1) 避免新增 mpz 表示——设计取舍待 S1 立项确认）；
- 0e 连续段随 S1 同批（**池连续段原语已落地：批次 243**，含 rational_set/segment_get/destroy + 契约测试）；
- 顺序建议 S2 → S1 → S3 → S4 → S5（工具面先清可减少后续簇的 mpq 互通样板）或按用户意愿插队。

> **S2 路线修正（批次 244）**：GMP 头内 `__mpz_struct`/`__mpq_struct` 为**匿名结构体 + typedef**
> （无 struct tag），「前向声明 `struct __mpz_struct;` + 指针参数」技巧**不成立**（会引入不同
> 的 incomplete tag）。故 S2 公共头去 GMP 不能靠签名微整形，须**真·句柄/工厂 API 化**
> （如 lvRational opaque 化 + 值访问器）——该子项与 S3 域迁移耦合，合并到 S3 立项时一并做；
> 路线图 S2 独立启动的价值下调（避免为 token 数做伪收敛）。

---

## 4. S1 前置表示设计（批次 242，草案定稿待立项实施）

### 4.1 系数表示取舍

| 方案 | 说明 | 判定 |
|:---|:---|:--:|
| A. 整数系数 → `RATIONAL(k,1)` | 复用 mpq 任意精度；无新表示位；显示/比较走既有 rational 路径 | ✅ 推荐（无新表示、零特例） |
| B. 新增 `mpz` 大整数表示位 | 内存/位操作更省，但新增 kind + ops + 字符串/比较/哈希全套特例 | ❌ 债务不划算（ND-7） |

结论：S1 系数一律 lvNumber `RATIONAL` kind；整数即分母 1。此取舍也同时消除 expr_canonical.h 的 mpq_t 值域与 `nt_number_theory` 的 mpz 语义切换问题（大整数走 mpq 分子）。

### 4.2 系数数组存储形态（ND-5 段草案）

- **段 = 一块连续 lvNumber 节点**（新块专用，避免 free-list 拆散）：池增加「预留连续段」路径——按需 `lv_malloc` 一块 ≥ n 节点的 Block，节点不进 free-list，直接按序授予调用方；段首地址 + n 即段句柄。
- 段内节点**元素级填充**（从 GMP 语义转换）需实现层写入口：公共面（lv_number.h 文本级零 GMP）提供
  `lv_number_segment_get(seg, i)`（返回可销毁句柄）与 `lv_number_from_lvRational`（现成）
  组合即可覆盖「先取句柄 → from_lvRational 语义置值」？——实际置值需写已有节点而非新造，
  故再补 `lv_number_rational_set(lvNumber*, const struct lvRational*)`（置值版，gmp-free 签名，
  内部 mpq_set）。两 API 供 S1 内核把 mpz/mpq 系数逐元素灌入段。
- **回收**：段析构 = 逐元素 `lv_number_destroy`（还 free-list）；或段级批量归还（块重建进
  free-list）——逐元素先落地，段级批量留作优化项（不引双轨）。
- 帧池语义：段为**常驻**对象（多项式长存）；临时系数块（如 result 构建）可走帧。

### 4.3 S1 落地顺序（立项后）

1. 池「预留连续段」原语（含测试：连续地址断言、超块扩容）；
2. `lv_number_rational_set` 置值 API + 契约测试；
3. `expr_canonical.h`（lvExpr 值域 mpq_t）→ lvNumber（此域消费面需全仓 grep：类型表/符号表/引擎转换点）；
4. `nt_polynomial`/`mpz_poly` 系数数组 → 段；
5. `nt_number_theory` mpz 语义 → RATIONAL(k,1)（大整数模逆等语义对拍是重点风险，独立小批验证）。

> 参考：number-abstraction-layer-design.md §4 期 2-6；批次 236-240 登记。
