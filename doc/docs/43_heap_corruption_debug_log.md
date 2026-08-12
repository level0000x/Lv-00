# 43. 堆损坏排查记录（0xc0000374 事件）

> 本文档记录 2026-08 期间 `layer5_core_test` 堆损坏崩溃（退出码 `0xc0000374`）的完整排查过程、已确认事实、最终根因与加固结果。**事件已闭环**：根因确认并修复，全量回归通过。

---

## 1. 事件摘要

| 项目 | 内容 |
| --- | --- |
| 现象 | 运行 `layer5_core_test` 崩溃，退出码 `0xc0000374`（`STATUS_HEAP_CORRUPTION`） |
| 触发路径（初次） | `test_proof_compiler_all_formats` → `lv_proof_compiler_to_json` → 第一次 `lv_strbuf_printf(&sb, "{\n")` |
| 崩溃点（初次） | `lv_strbuf_vprintf` 入口的 `_heapchk()` 检测到堆已损坏（fast-fail），**并非**堆操作本身越界 |
| 触发路径（加固后残留） | `test_proof_object_with_premises` 结束时 `lv_proof_object_destroy` 内的 `proposition_unref`（间歇性，~10-30% 概率） |
| **最终根因** | 测试把 `lv_strdup("conclusion")` 字符串塞进 `lvProofStepRecord::conclusion`（类型为 `Proposition*`）；destroy 时被当作 Proposition 读取越界字段并触发 `proposition_destroy(非法指针)` |
| 最终结果 | 根因修复 + 全量加固后：`test_layer5_core` 连续 30 次直跑通过，全量 ctest **170/170 通过** |
| 关联事件 | 排查前完成了 `magic` 模块彻底删除；经 git diff 确认与崩溃无因果关系（堆布局变化可能改变崩溃暴露时机） |

## 2. 已确认的事实（排除项）

以下假设在排查过程中被**证伪**，后续排障不必重复验证：

1. **不是 darray 语义问题**。`lv_darray_get` 返回**槽地址**（`data + index*elem_size`），不是元素副本。因此
   `plugin_system_autoload.c` 中 `lv_free((void **) lv_darray_get(&system->search_paths, i))` 语义正确
   （先取槽地址、再释放槽内字符串指针）。swap-remove 逻辑（L65-82）亦无缺陷。
2. **不是并行/时序竞态**。串行 `ctest -j1`、甚至直接循环运行 exe 8 次中 6 次崩溃，属于稳定复现的真实 bug。
3. **不是 magic 删除引起**。`test_layer5_core.c` 中 proof/plugin 测试代码经 `git diff` 确认零改动。
4. **不是 strbuf 扩容逻辑本身**（`lv_strbuf_grow` 的倍增/截断路径已逐行审查，含 `SIZE_MAX/2` 防回绕检查）。

## 3. 排查过程时间线

1. `ctest --test-dir build3` 并行全量跑 → 出现 `0xc0000374`，初判"环境/时序问题"。
2. 串行隔离运行 `ctest -R "^layer5_core_test$" -V` → 同样崩溃 → 排除环境假设。
3. 直接运行 `build3\test_layer5_core.exe`（注意：实际路径在 `build3/` 根，不在 `build3/test/`）→ 循环 8 次 6 次崩溃。
4. 插桩定位（五处临时 DBG/MARK 打印，见第 6 节）：
   - `test_layer5_core.c`：MARK:enter / obj_ready / json_returned / json_done …
   - `proof_compiler.c`：DBG:to_json enter / hdr / steps_begin / step[i] …
   - `lv_strbuf.c`：DBG:vprintf / grow / `_heapchk()` 检查
   - `allocator.c`、`lv_utils.c`：debug_alloc / lv_malloc 入口打印
5. 观察结果：`DBG:to_json enter` 打印后，`DBG:to_json hdr` 与 `DBG:vprintf` 均未出现
   → 崩溃发生在 to_json 第一次 strbuf_printf 的 `_heapchk()` 内部 → 堆在进入 to_json 前已损坏。

### 3.1 插桩过程中的工具性坑（已解决）

| 坑 | 原因 | 处理 |
| --- | --- | --- |
| `_heapchk()` 隐式声明编译失败 | MSVC/GCC 需要 `<malloc.h>` | `lv_strbuf.c` 增加 `#include <malloc.h>` |
| stderr 管道块缓冲导致插桩输出丢失 | 崩溃时未刷新的缓冲被丢弃 | 所有关键打印后加 `fflush(stderr)` |
| 旧插桩文本反复出现 | `.ninja_log` 损坏触发全量重建，且重建一度卡在 `simd_ops.c` | 删除损坏的 `.ninja_log`，允许 555 文件全量重建 |
| `Select-Object -Last 3` 过滤掉真实编译错误 | PowerShell 管道截断 | 改用 `Select-String -Pattern 'error|FAILED'` 捕获 |

## 4. 根因分析

### 4.1 第一类根因（已修复）：premise_step_ids 指针被覆盖

插桩日志中，构造 `s2` 时出现两次 `debug_alloc size=32`（无对应 `lv_malloc` 打印），
对应 `lv_proof_step_record_create` 内部的 `lv_darray_reserve(&arr, 8)`（8 × sizeof(int) = 32 字节）预分配。

**高风险模式**（测试代码 `test_layer5_core.c` L368-L370）：

```c
s2->premise_step_ids = (int *) lv_malloc(sizeof(int)); /* 覆盖了 create 内部 darray 分配的 32 字节指针 */
s2->premise_step_ids[0] = 0;
s2->premise_count = 1;
```

影响：
- `lv_proof_step_record_create` 已通过 darray 分配 32 字节并写入 `premise_capacity = 8`；
  测试又用 `lv_malloc(sizeof(int))`（4 字节）**覆盖同一指针** → 原 32 字节泄漏，且
  `premise_capacity`（=8）与实际分配（=4 字节）不一致，构成**潜在越界写**。
- 相同模式也存在于 `test_proof_object_invalid_chain`（L320-322）。

### 4.2 第二类根因（最终确认，最可疑）：conclusion 字符串被当作 Proposition 释放

**现象**：修复 4.1 后，`test_layer5_core` 仍以 ~10-30% 概率间歇性崩溃，
崩溃检测点从 `test_proof_compiler_all_formats` 前移到 `test_proof_object_with_premises`。
`_heapchk()` 在所有 allocator 检查点**全部通过**、无任何魔数检测日志 → 排除越界写/双 free 走
debug fallback 路径；推断为"读取垃圾值后对非法指针执行 free"型损坏。

**定位手段**：allocator 三个入口加 `_heapchk` 检查点 + `debug_free`/`debug_realloc` fallback
路径打印头魔数（定位完成后已全部移除）。结果：崩溃前所有检查点通过、无 fallback 触发，
确认损坏发生在两次检查点之间、且由后续堆操作间接暴露。

**根因**（test_layer5_core.c L301）：

```c
s3->conclusion = lv_strdup("conclusion"); /* 类型错误：conclusion 是 Proposition*，不是字符串 */
```

- `lvProofStepRecord::conclusion` 类型为 `Proposition*`（见 `proof_compiler.h`），
  destroy 时经 `destroy_proposition_field` → `proposition_unref` 处理。
- `proposition_unref` 读取 `ref_count`（`Proposition` 内偏移 20），而 strdup 只分配 11 字节，
  **读指针越界到相邻堆块**，取到的垃圾值取决于堆布局 → 这就是"间歇性"的来源。
- 当垃圾 `ref_count == 0` 时，`proposition_unref` 调用 `proposition_destroy(字符串指针)`，
  把字符串字节当结构体字段指针逐个 `lv_free` → 对非法地址 free → **堆损坏**。

**修复**：改为复用真实命题并平衡引用计数：

```c
s3->conclusion = goal;
proposition_ref(goal);
s3->conclusion_id = goal->id;
```

> 注意：`lvProofTreeNode::conclusion`（`proof_trace.h`）是 `char*` 字符串字段，
> `test_proof_trace.c` 中 `lv_strdup` 用法**正确**，勿误改。

## 5. 加固方案（"全面加固"策略，已全部完成）

用户决定不再逐个二分定位，而是对所有可疑分配/释放/边界路径系统性加固：

- [x] `lv_proof_step_record`：新增 `lv_proof_step_record_set_premises()` API（`proof_compiler.h` 辅助函数区），
      内部统一分配/释放并同步 `premise_capacity`；`create` 不再预分配 darray（初始全为 NULL/0）。
      `destroy` 走原有 `lv_FIELD_PLAIN` 释放路径，与 API 分配完全兼容。
- [x] 测试代码：`test_layer5_core.c` 全部 5 处直接覆盖/`lv_realloc` 写入改为调用
      `lv_proof_step_record_set_premises()`（含 `test_proof_step_record_premises` 的容量断言）。
- [x] `lvProofStepRecord::premise_step_ids` 字段注释标注"须经 set_premises 管理"。
- [x] `lv_strbuf_vprintf`：grow 失败（OOM/容量溢出保护）时按剩余容量截断 `written`，
      与 `append_raw` 语义对齐，修复"`len +=` 未截断长度导致 `len > cap`、下次写入越界"的潜在越界。
- [x] 修复测试类型错误：`s3->conclusion` 改为真实 `Proposition`（复用 `goal` + `proposition_ref` 平衡计数），
      消除"字符串当 Proposition 释放"的 UB 与堆损坏（4.2 节）。
- [x] 调试分配器：`debug_realloc`/`debug_free` fallback 路径经插桩二次核对（确认未触发后还原），
      不改变"外部指针 raw free"语义。
- [x] 回归验证：`test_layer5_core` 连续 30 次直跑通过（修复前 ~10-30% 崩溃率）；
      全量 `ctest --test-dir build3 -j1` **170/170 通过**。

## 6. 插桩清理状态（已全部移除）

| 文件 | 原残留内容 | 状态 |
| --- | --- | --- |
| `core/src/layer2_resource/lv_strbuf.c` | DBG:vprintf / DBG:grow / `_heapchk()`、`<malloc.h>`、`fflush(stderr)` | ✅ 已移除 |
| `core/src/layer5_output/proof_compiler.c` | `DBG:to_json enter/hdr/steps_begin/step[i]/steps_done/tail/esc_done` | ✅ 已移除 |
| `core/src/layer2_resource/allocator.c` | `debug_alloc` 入口 DBG、`dbg_heap_chk`、`RAW-FREE` 日志 | ✅ 已移除 |
| `core/src/layer2_resource/lv_utils.c` | `lv_malloc` 入口 DBG 打印 | ✅ 已移除 |
| `test/c/test_layer5_core.c` | `MARK:enter/obj_ready/json_returned/json_done/…` | ✅ 已移除 |

## 7. 给后续排障者的提示

1. **崩溃退出码速查**：`3221226356 = 0xC0000374 = STATUS_HEAP_CORRUPTION`（Windows 堆损坏 fast-fail），
   不是普通段错误（`0xC0000005`）。
2. **`_heapchk()` 是"体检"不是"病因"**：它在 vprintf 入口检测到损坏，说明损坏早已发生；
   应继续向**更早**的构造路径排查，而不是盯着 to_json。
3. **优先怀疑"指针被覆盖"**：darray 预分配 + 外部直接覆盖指针 = 泄漏 + 容量不匹配，
   是堆损坏的高概率来源；先搜 `premise_step_ids`、`_ids` 等字段的直接赋值。
4. **stderr 管道缓冲会吞掉崩溃前最后一行日志**：插桩输出务必 `fflush(stderr)`。
5. **ninja `.ninja_log` 损坏**会触发全量重建，属正常现象；不要误判为环境抖动。
6. **间歇性崩溃 = "读垃圾值后非法操作"型损坏**：如果 `_heapchk` 检查点全部通过、魔数检测无日志，
   大概率不是越界写，而是**把非法指针当对象用**（本事件的 `proposition_destroy(字符串)`）。
   排查手法：在 allocator 入口加检查点缩小范围 + 在 fallback 路径打印头魔数，快速区分
   double-free / 越界写 / 非法对象释放。
7. **警惕测试代码里的"类型假装"**：`Proposition*` 字段塞 `lv_strdup` 字符串、`int*` 字段直接
   `lv_malloc` 覆盖——这类"看起来能跑"的写法在特定堆布局下会爆炸。结构体字段类型与
   赋值必须严格一致；需要字符串结论时用真正的字符串字段（如 `lvProofTreeNode::conclusion`）。
8. **引用计数对象共享**：同一 `Proposition` 同时挂到 `obj->goal` 和 `step->conclusion` 时，
   必须用 `proposition_ref` 平衡计数，否则 destroy 顺序不同会导致提前释放（use-after-free）或泄漏。
