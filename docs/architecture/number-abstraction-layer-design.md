# Lv-00 数值抽象层设计（统一句柄 + 强制池化）

> 状态：**正式（2026-09-03 批准，批次 232）**——统一抽象层方向定稿，实施未启动
> 日期：2026-09-03（批准）
> 前置：`docs/architecture/dependency-policy.md`（外部库默认底层：GMP/MPFR/MPFI/MPC）
> 性质：设计文档（正式）——公共 API 不再泄漏 GMP/MPFR 类型；形态一（统一不透明句柄 lvNumber）
>        + 强制池化；ND-1..7 决策已定案（见 §7）

---

## 0. 目标与非目标

### 目标

1. **升级隔离**：`gmp.h`/`mpfr.h` 的 include 收敛到抽象层**唯一实现点**；未来换 GMP/加 MPFR 表示 = 只改实现层；
2. **公共 API 类型清零**：`geometry_transform.h`（60 处 mpq_t）、`nt_*`、`mpz_poly.h`、`expr_canonical.h`、
   `inequality_reasoning.h`、`coeff_pool.h`、`symbolic_coord.h`、`rational.h` 等公共头不再出现 GMP 类型；
3. **强制池化**：`lvNumber` 实例唯一来源为数值池，全库禁止「逐数系统分配」；
4. **序列化保真**：`to_string`/比较语义与 GMP 现行输出**逐字节等价**，graph hash / Lean 往返零漂移。

### 非目标

- 不改数值语义（mpq 精确语义保持现状等价）；
- 本设计**不引入** MPFR 表示（mpfr 接入由 dependency-policy 批次 A/B1 另行推进，抽象层为其预留表示位）；
- 不动约束图/证明系统/TrustColor 的语义层（抽象层在数值域之下）。

### 裁决原则（ND-7）

- 本工程所有后续未决取舍（含本文 §7 全部遗留项）一律按「**最小化后续工程债务**」裁决；
  如与直觉相悖，先按债务最小选项执行并登记，除非用户另行指示。

---

## 1. 现状证据（2026-09-03 只读审计）

| 证据 | 说明 |
|:---|:---|
| 26 个 `.c` + 10+ 公共头 include `<gmp.h>` | 分布：layer2（工具）、layer3（symbolic/geometry/algebraic）、layer4（solver/nt）、core 顶层桥 |
| 公共头裸 GMP 类型 | `geometry_transform.h` 60 处 `mpq_t`；`nt_polynomial.h`/`nt_number_theory.h`/`mpz_poly.h`/`expr_canonical.h`/`inequality_reasoning.h`/`coeff_pool.h`/`lv_numeric.h`/`lv_str_utils.h`/`groebner_engine.h` |
| 双轨有理数类型 | `rational.h` `lvRational{mpq_t value}` vs `symbolic_coord.h` `struct Rational{mpq_t value}`（rational.h 注释自称「同构」）|
| 热区裸调 | `solver_equation_extract.c` 342 次、`solver_symbolic.c` 268、`symbolic_coord_transform.c` 249、`geometry_transform.c` 238、`mpz_poly_resultant.c` 153、`algebraic.c` 150 |
| 既有句柄雏形 | `lv_number.h`：`void *impl; // mpq_t, algebraic, interval 等` |
| GMP 分配器未接线 | SECURITY.md：GMP 用系统默认分配器，不受 lv 内存封装管理 |
| 序列化依赖 | 约束图节点值/证明输出依赖 mpq 规范字符串（`"3/2"`、分母符号规约）——保真是硬约束 |

---

## 2. 抽象面设计（统一句柄 lvNumber）

### 2.1 类型

```c
/* core/include/lv/lv_number.h —— 唯一数值抽象面（升级现有 lv_number.h） */
typedef struct lvNumber lvNumber;          /* 不透明：布局只存在于实现 .c */
typedef enum lvNumberKind {
    LV_NUM_INT = 1,        /* 整数值（小整数 inline 优化位） */
    LV_NUM_RATIONAL,       /* 精确有理数（mpq 语义） */
    LV_NUM_REAL_MPFR = 16, /* 预留：MPFR 表示（dependency-policy 批次引入后再激活） */
    LV_NUM_INTERVAL,       /* 预留 */
    LV_NUM_ALGEBRAIC       /* 预留 */
} lvNumberKind;
```

### 2.2 API 面（一套，kind 内部分派）

- **工厂**：`lv_number_int_from_i64` / `lv_number_rational_from_i64(i64,i64)` /
  `lv_number_parse(const char *text)`（十进制串走精确路径，对齐批次 D）/ `lv_number_clone`
- **算术**：`add / sub / mul / div / neg / inv / abs / pow_i64`
- **比较/判定**：`cmp / equal / is_zero / is_integer / get_kind`
- **IO 保真**：`to_string`（GMP 规范形逐字节复刻）/ `to_decimal_string`（显示用，允许格式差异）
- **hash**：供 graph 节点值哈希路径复用（同值同 hash 不变式）
- **生命周期（强制池化）**：`lv_number_destroy` = 归还池；`lv_number_frame_begin/end`（见 §3）

> 语义对齐规则：混合 kind 提升（int→rational）与 GMP 现行为一致；除法语义（0 分母错误码）
> 与现有 `lv_rational_div` 一致。所有函数 `NULL` 契约沿既有头注释规范。

### 2.3 与现有类型的过渡关系

| 现状 | 去向 |
|:---|:---|
| `rational.h` `lvRational` | 0 期并入抽象层（`lvNumber` 承担其角色；旧符号保留薄包装，见 §4） |
| `symbolic_coord.h` `struct Rational` | 随 symbolic 域迁移替换（保留 typedef 别名直至该域迁移完） |
| `mpz_poly.h` / `nt_polynomial.h` | 系数数组改存 `lvNumber`（池指针数组或帧内连续段，见 §3.3） |
| `geometry_transform.h` 60 处 mpq_t | struct 字段改 `lvNumber *`（池指针）+ API 参数改 `const lvNumber *` |
| `lv_numeric.h` 样板函数 | 保留（内部委托抽象层），`lv_mpq_set_d_checked` 等不再对公共头暴露 mpq_t |

---

## 3. 强制池化设计（核心约束，ND-2）

### 3.1 原则

1. **`lvNumber` 实例唯一来源 = 数值池**；全库禁止用 `lv_malloc`/`lv_calloc` 逐数构造 number（CI grep 门禁，见 §4 6 期）；
2. 既有「0 原生 malloc」规矩不变：池块本身经 `lv_malloc` 一次性申请；
3. Debug 构建开启数值池校验（越界/双归还/泄漏 WARN），与既有 leak detector 风格一致。

### 3.2 两级池模型

| 池 | 语义 | 载体 | 适用 |
|:--|:---|:---|:---|
| **帧池（短命运算）** | `lv_number_frame_begin()` … `end()` 之间分配的数随帧整体回收（bump + 回滚） | 扩展现有 `lv_arena`（mark/reset 先例） | solver 热区每轮运算、表达式求值临时量 |
| **常驻池（长命对象）** | destroy 归还 free-list；池块不缩（块复用） | 泛化 `coeff_pool.h`（现 mpz 池，块容量 8 mpz_t 先例） | graph 节点值、transform 坐标等跨作用域存活对象 |

- 帧池内临时量**不得**逸出帧（Debug 断言 + 文档契约）；需要长存的量显式 `lv_number_promote_to_resident`（拷贝进常驻池）——与 graph 节点写值路径对齐。

### 3.3 热数据结构接入方式

- `geometry_transform`/graph 节点值：存常驻池指针 `lvNumber *`；transform 构造/应用在各自生命周期入口开帧，运算临时量走帧池；
- `nt_polynomial`/`mpz_poly` 等**批量数值**（系数数组）唯一形态 = **池内连续段 + 段级回收/晋升**（ND-5）；
  池 API 在 0 期提供 bulk 分配与段原语；**禁止逐系数独立分配**（延续 coeff_pool 块式先例，
  不做「每系数一池指针」的中间态）。

### 3.4 GMP / MPFR 分配器接线（补齐历史盲区）

- 抽象层实现初始化：`mp_set_memory_functions(lv_malloc_fn, lv_realloc_fn, lv_free_fn)` 统一 GMP 内部内存走 lv 分配器；
- MPFR 引入时同款接线 `mpfr_set_memory_functions`（批次 A 事项，本设计只登记接口位）；
- 效果：SECURITY.md「GMP 不受管」盲区关闭，内存审计全链统一。
- **0b 裁决（2026-09-03）**：全局接线拆为**独立小步**——须先迁移全部
  `mpz_get_str`/`mpq_get_str` 释放点为 allocator 感知（现多处用 `lv_free_external`
  = 系统 free，wire 后释放 lv 分配器内存会不匹配），再 `mp_set_memory_functions`；
  在接线完成前，lvNumber 池节点内 mpq 的 limb 存储走 GMP 默认分配器
  （同 coeff_pool/mpz 现状，无行为变化）。

---

## 4. 迁移分期（ND-3：抽象层先行 + 分批迁移）

| 期 | 内容 | 验收 |
|:--:|:---|:---|
| 0 | **抽象层落地**：`lv_number.h`/`lv_number.c`（唯一 include gmp.h 的实现点）+ 两级池（含 **bulk/连续段原语**，ND-5）+ allocator 接线 + **双轨内部合一**（lvRational/Rational 存储并入 lvNumber，公共符号薄包装保留，ND-4）+ **序列化对拍测试基线**（to_string/compare 与 GMP 逐字节一致；graph hash 同值不变式） | ctest 全绿 + 对拍 0 漂移 |
| 1 | **双轨过渡**：旧公共函数保留为薄包装（对外签名不变、内部走抽象层） | 现有 296/296 测试零改动通过 + Python 绑定冒烟 |
| 2 | 工具/IO 域：`rational.h` 双轨合一、`lv_numeric.h`/`lv_str_utils.h` 内部委托 | 该域测试绿 |
| 3 | 独立数值域：`nt_*`、`mpz_poly.h` 系数改**池内连续段**（ND-5） | 数论/多项式测试绿 |
| 4 | 几何/符号域：`geometry_transform`（60 字段）、`symbolic_coord` 坐标值 | 几何测试绿（含保真断言） |
| 5 | **solver 热区**（342/268 处裸调）+ graph 节点值存储 | 全量 ctest 绿 + 性能不劣化（帧池生效） |
| 6 | **收尾**：删除薄包装、`gmp.h` include 白名单 grep 清零、CI 加「数值依赖 include 白名单」+「ABI/符号对拍」（ND-6）门禁（复用 headers-sync/symbol-sync 模式）、决策登记 | CI 全绿 + 门禁生效 |

> **0 期实施进展（批次 234，2026-09-03）**：
> - **0b**：`lv_number.h` 不透明化（句柄即池节点，公共面无 GMP 类型）+ 两级池
>   （常驻 free-list + 帧池 TLS 栈）+ 跨类型算术**精确提升**（int×rational → rational；
>   任一 float → float；int÷int 截断契约保留）+ hash 统一 double-bits（eq→同哈希不变式）；
> - **0c 基线**：新增 `test_lv_number_pool_ext`（帧/常驻生命周期、精确提升、GMP 规范形
>   保真 "3/2"/"7"、跨帧 hash 同值）；既有 `lv_number_ext/ops_ext` 契约测试零改动通过；
> - 全局 GMP allocator 接线按上述 §3.4 裁决拆为独立小步（先迁移 get_str 释放点）。
> - 待续：双轨合一（ND-4）、批量连续段（ND-5 应用于 nt_*）、域迁移（期 1-6）。

> 每期独立提交、独立登记；任何一期不绿不进入下一期。函数**符号名全程保留** → ctypes/Python
> 绑定、测试、Lean 桥接无损；ABI 变更集中在公共头签名，版本策略见 §7（ND-6）。

---

## 5. 兼容与保真（硬约束）

1. `to_string` 必须复刻 GMP 规范形：`"3/2"` 非 `"6/4"`、分母恒为正、整数无分母；
2. `cmp`/`equal` 语义与 mpq 一致（cross-multiply 比较结果逐位一致）；
3. graph 节点值 hash：抽象层 hash 与现哈希路径同值同 hash（0 期钉测试）；
4. 现有 296 测试 = 保真回归基线；任何语义漂移按 M4 缺陷处理而非「接受新值」。

---

## 6. 风险与对策

| 风险 | 等级 | 对策 |
|:---|:--:|:---|
| 序列化保真漂移 → graph hash/Lean 往返大面积红 | P0 | 0 期对拍基线先行；按 M4 纪律修复 |
| 热区句柄分配开销 → solver/几何性能劣化 | P1 | 帧池强制生效后基准对拍（既有 benchmark 目标）；不过再上连续段形态 |
| 双轨清理不净 → 两套类型并存反弹 | P1 | 每期 grep 门禁推进度；6 期清零白名单 |
| 迁移中途公共 ABI 变更波及消费方 | P2 | 函数名保留 + 分期提交 + Python ctypes 冒烟每期跑 |
| GMP allocator 接线引入不稳定 | P2 | 0 期独立验证（allocator 往返/泄漏测试）+ 可回退开关登记 |

---

## 7. 决策登记

| ID | 决策 | 状态 |
|:---|:---|:---:|
| ND-1 | **形态一：统一不透明句柄 `lvNumber`**（kind 内部分派；升级现有 lv_number.h） | ✅ 用户批准 2026-09-03 |
| ND-2 | **强制池化**：lvNumber 唯一来源数值池（帧池 + 常驻池两级），全库禁止逐数系统分配 | ✅ 用户批准 2026-09-03 |
| ND-3 | 迁移节奏：抽象层先行 + 分批迁移（§4 六期），每期测试全绿 | ✅ 用户选择 2026-09-03 |
| ND-4 | **0 期内部合一**：`lvRational`/`Rational` 存储并入 lvNumber，公共符号保留薄包装至各域迁移完——避免双轨并行期债务 | ✅ 2026-09-03 |
| ND-5 | **批量数值唯一形态 = 池内连续段 + 段级回收/晋升**；0 期池 API 提供 bulk/段原语；禁止逐系数独立分配（不做指针数组中间态） | ✅ 2026-09-03 |
| ND-6 | **不建双 ABI 兼容层**（兼容层 = 永久债务）；ABI 变更随下次主版本统一 bump（2.0.0）；CI 加 ABI/符号对拍（扩展 symbol_sync_check）；Python 绑定回归 = pytest 全绿 + 符号对拍 | ✅ 2026-09-03 |
| ND-7 | **元决策**：本工程所有后续未决取舍按「最小化后续工程债务」裁决，除非用户另行指示（§0 裁决原则） | ✅ 2026-09-03 |

---

## 8. 关联与审计证据索引

- 关联：`dependency-policy.md`（外部库默认底层 + 批次 A/B1）；`memory-ownership.md`（[copy]/[take]/[borrow] 标注）；`coeff_pool.h`/`lv_arena.h`/`lv_mempool.h`（池基建先例）；SECURITY.md（GMP 分配器盲区）
- 审计证据（2026-09-03，只读）：26 `.c` include gmp.h；公共头裸 mpq_t 清单；热区调用计数；双轨类型位置
