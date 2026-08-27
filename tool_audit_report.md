# Lv-00 工具/脚本/构建辅助标准现状审计报告

**审计范围**：`tool/`、`tools/`、`scripts/`、根目录散落脚本、`.github/workflows/`、`build*` 构建目录、CMake 配置。
**审计方法**：只读盘点（glob/read/grep），证据 file:line 级落位，未修改任何文件。
**结论先行**：本项目存在多组「一需求多脚本」，且 `tool/` 是**抓不牢的垃圾场**（一层目录塞了三类无关子目录），而 `tool/` 与 `tools/` 是命名撞车的**同级目录**。真正的平台分化（窗/Unix）在本项目基本不存在——所有 `.sh` 均属 mathlib/batteries 依赖，项目自身没有一套需要保留的 ps1⇄sh 孪生脚本。

---

## 0. 规模校正（纠正已知线索里的失实数字）

审计前先核实「声称 vs 实际」，避免把依赖项脚本算进「项目自有工具」：

| 线索 | 实际 | 校正结论 |
|---|---|---|
| 「12 个 .ps1」 | 项目自有 12 个 | ✅ 属实（见 §1.3） |
| 「21 个 .sh」 | 项目自有 **0 个** | ❌ 全部 21 个都在 `formal/.lake/packages/mathlib/scripts/` 与 `batteries/scripts/`，是第三方依赖自带脚本，与项目工具链无关 |
| 「tool/ 与 tools/ 两个目录并存」 | ✅ 属实 | `tool/` 8 类脚本，`tools/` 仅 1 个脚本 |

> 因此「Windows ps1 vs Unix sh 需保留分化」这一前提在本项目**不成立**：没有一对真正的平台孪生脚本。分化的必要性只体现在 CI 矩阵（见 §4）。

---

## 1. tool/ 与 tools/ 脚本清单表

### 1.1 `tool/` 目录（含子目录）

| 脚本/文件 | 语言 | 用途 | 调用方 |
|---|---|---|---|
| `tool/extract_preset_data.py` | py | 预设注册数据 → `preset_registry.yaml`（**不完整**：仅 47 行，`PRESET_TYPE_MAP` 字典中途截断，语法错误） | 无（孤儿） |
| `tool/fix_build.py` | py | ①扫 CMakeLists.txt 补建缺失 `.h/.c` 桩；②解 `# [QA] file missing` + 取消 `# core/src/*.c` 注释 | 手动 |
| `tool/fix_cmake.ps1` | ps1 | 仅解 `# [QA] file missing` + 取消 `# core/src/*.c` 注释 | 手动 |
| `tool/gen_stubs.ps1` | ps1 | 按**硬编码清单**（~150 个）批量生成 stub `.c` | 手动（TASK_CONTEXT.md:882,935 引用、提示删桩时同步处理） |
| `tool/report_generators/docx_helpers.js` | js | docx 共享辅助（字体/页边距/表/编号常量 + 构建函数） | generate_report.js、gen_report_v3.3.0.js、generate_opt_report.js、`doc/generate_version_doc.js` |
| `tool/report_generators/generate_report.js` | js | 生成「全域优化任务汇报」v3.4.0 .docx | `ui/package.json` `"report"` 脚本 |
| `tool/report_generators/gen_report_v3.3.0.js` | js | 生成 v3.3.0 .docx（含目录/封面信息表） | 手动 |
| `tool/report_generators/generate_opt_report.js` | js | 生成「优化汇报」.docx（复用同一 helpers） | 手动 |
| `tool/scripts/gen_report.py` | py | 生成「UI 系统全面优化任务汇报」.docx（python-docx、独立实现） | 手动 |
| `tool/scripts/cleanup_code.py` | py | C 代码清理分析器（未用符号/死代码/重复块/过时注释 → md/json） | 手动 |
| `tool/tools/fix_symbolic_coord.py` | py | 正则替换 `symbolic_coord.c` 的 malloc/free → lv_ 版本 | 手动 |

### 1.2 `tools/` 目录

| 脚本 | 语言 | 用途 | 调用方 |
|---|---|---|---|
| `tools/check_preset_sync.py` | py | 校验 `module/presets/*.lvz` 与 `preset_*.c` 同步；`--fix` 重生成；`--verify` 用 Python 复刻 C loader（module_lvz.c 词法/语法） | 手动（本地）。**未接入 CI**；`import convert_presets` |

### 1.3 同一预设链上的根目录生成器（构成「链」而非孤立）

| 脚本 | 语言 | 用途 | 调用方 |
|---|---|---|---|
| `convert_presets.py`（根） | py | 从 56 个 `preset_*.c` 提取注册宏 → 生成 56 个 `module/presets/*.lvz`。**判定为 .lvz 唯一事实源**（TASK_CONTEXT.md:1891,1963） | `tools/check_preset_sync.py` import；手动 |
| `tools/check_preset_sync.py` | py | 见 §1.2 | —— |

### 1.4 项目自有的 12 个 `.ps1`（对照「12 个 ps1」线索）

| 目录 | 脚本 | 用途 |
|---|---|---|
| 根 | `do_switch5.ps1`、`refactor_s4.ps1`、`refactor_step1.ps1` | 一次性重构辅助 |
| 根 | `_c11_migrate.ps1`、`_c11_migrate_test.ps1` | C11 迁移（含测试版） |
| `tool/` | `fix_cmake.ps1`、`gen_stubs.ps1` | 见 §1.1 |
| `core/src/layer4_reasoning/preset/` | `_migrate.ps1`、`_migrate_adv.ps1` | 预设迁移（adv 为变更版） |
| `formal/lvFormal/` | `fix_codegen.ps1`、`fix_cv00.ps1` | 修复 codegen / cv00 |

---

## 2. 重复脚本/目录清单（一需求多脚本）

### ⭐ 覆盖重复（同语义，多个实现）

| # | 需求 | 重复实现 | 说明 | 应统一 |
|---|---|---|---|---|
| R1 | **修复 CMakeLists.txt**（解 QA 注释 + 取消 `# core/src`） | `tool/fix_build.py`(步骤2) 与 `tool/fix_cmake.ps1`(整文件) | 两文件正则逻辑逐字一致；ps1 只是 py 的子集 | ✅ 收敛到一处 |
| R2 | **补建缺失 .c/.h 桩** | `tool/gen_stubs.ps1`(硬编码清单) 与 `tool/fix_build.py`(步骤1 扫 CMakeLists) | 同一目标、两种输入源；ps1 清单需人工维护易腐，py 扫描法更稳 | ✅ 收敛到扫描法 |
| R3 | **预设提取/生成** | `convert_presets.py`(.lvz) + `tools/check_preset_sync.py`(同步检查/--fix 复用 convert_presets) + `tool/extract_preset_data.py`(.yaml,孤儿) | 3 个工具解析同一批 `preset_*.c` 注册宏；`convert_presets.py` 与 `extract_preset_data.py` **各自维护一份 `PRESET_TYPE_MAP`**（D3 常量重复） | ✅ 唯一生成器 `convert_presets.py`，删 `extract_preset_data.py`；check 只做「读」不做「生成」 |
| R4 | **生成任务汇报 .docx** | `tool/report_generators/` 3 个 JS 生成器 + `doc/generate_version_doc.js`(第4个消费者) 共用 `docx_helpers.js`；另有 `tool/scripts/gen_report.py` 用 python-docx **独立重写** | 同一需求同时存在 JS docx 栈与 Python docx 栈 | ✅ 留 JS 栈（共享 helpers），删 `gen_report.py` |

### 结构重复（A3：目录/入口）

| # | 观察 | 判定 |
|---|---|---|
| S1 | **`tool/` 与 `tools/` 同级并存**：`tool/`（8 类脚本）vs `tools/`（仅 check_preset_sync.py） | 二者不是「并行职责」，tools/ 仅 1 个脚本却占用一个顶层目录 → **目录归一** |
| S2 | **`tool/` 内部再分 `scripts/`、`tools/`、`report_generators/`** 三个子目录 | 「脚本」与「工具」概念重叠，`tool/tools/` 与顶层 `tools/` 撞名 → 层级混乱 |
| S3 | **9 个并行 build 目录**：`build`(VS2022)、`build3`(Ninja·static·Debug)、`build_gov`(MinGW·static·Debug)、`build_symcheck`(Ninja·shared)、`build_verify`(Ninja·shared·Release)、`build4/build_check/build_final/build_san`(无 cache) | build3 与 build_gov 同为 static-Debug 只差生成器；build_symcheck 与 build_verify 同为 shared-Ninja 只差配置；无 `CMakePresets.json` 集中 configure 选项；`_ctypes_binding.py` 搜索路径列举 build3/build4/build/build/Release 多个输出目录 | ✅ 用 CMakePresets 归一，删冗余 build 目录 |
| S4 | 根目录**成堆一次性脚本**：`check_csg.py`、`fix_csg.py`、`fix_csg2.py`、`split_csg.py`、`split_dsl_lexer.py`、`split_eg.py`、`split_export_pdf.py`、`split_ie.py`、`split_pee.py`、`split_pv.py`、`edit_memory.py`、`scan_size.py`、`test_regex.py`、`test_regex2.py`、`_scan_zero_cov.py` | 多为一次性重构辅助；成对出现（`fix_csg`/`fix_csg2`、`split_*` 系列、`test_regex`/`test_regex2`） | 归档到一次性工具目录（如 `archive/oneoff/`）或删除，避免污染根目录 |

---

## 3. 构建脚本入口分析

**结论：构建没有独立的 wrapper 脚本**，入口只有两个——`CMakeLists.txt` + CI workflow。

- 全库 grep `cmake|ninja|build3|build_symcheck|build_verify|...`：命中 11 处，仅 `tool/fix_build.py`、`tool/fix_cmake.ps1`（两者都是在**修 CMakeLists.txt**，不是 configure/build 包装器）与 `_ctypes_binding.py`/`setup.py`（运行时找库路径）。无任何 `.ps1/.sh/.py` 是 configure/test 包装器。
- `build3`（Ninja/static/Debug）、`build_symcheck`（Ninja/shared）是**CMake 构建目录**，不是脚本；它们由 CI 直接 `cmake -B build ...` 驱动。
- 配置入口：只有 `CMakeSettings.json`（VS IDE 用 Ninja 单配置，指向 `out/build/`，但实际构建目录是全根 `build*`，二者路径不一致）；**没有** `CMakePresets.json`/`CMakeUserPresets.json` 做集中配置。
- **等价入口问题**：不存在「同功能多 wrapper」；真实问题是 **S3（9 个并行配置 + 无 preset 归一眼）**——`build`(VS) / `build3`(Ninja) / `build_gov`(MinGW) 三套独立调试配置在功能上高度重叠。

**建议**：用 `CMakePresets.json` 声明 `vs-debug`/`ninja-static-debug`/`ninja-shared`/`ninja-shared-release` 等命名 preset，作为唯一 configure/test 入口；删除空置/重复的 build 目录；`_ctypes_binding.py` 的库搜索路径收敛到 preset 输出目录。

---

## 4. CI 与本地脚本：重复逻辑 / 缺口

| 检查 | 是否 CI 执行 | 是否本地脚本 | 问题 |
|---|---|---|---|
| .lvz 同步校验（check_preset_sync） | ❌ **CI 未接入** | ✅ `tools/check_preset_sync.py` | 唯一会执行「生成-校验」的本地脚本被 CI 漏掉 → **CI/本地缺口**；preset 数据源正确性全靠人跑 |
| preset 生成（convert_presets.py） | ❌ CI 未跑 | ✅ 手动 | 同上 |
| 构建+CTest | ✅ `ci.yml` | 无对应本地脚本 | CI 以内联 bash 写，逻辑留在 YAML，本地无法复用（见「共享」建议） |
| Python lint/type/flake8+mypy | ✅ `python.yml` | 无本地脚本 | 同上 |
| 构建错误/测试失败报告（grep error / awk 失败名单） | ✅ `ci.yml`/`python.yml` 内联 | 无 | 每处 workflow 各自手写一套 grep/awk，**跨 workflow 重复且易偏** |

**关键结论**：
- **同一检查在 CI 和本地各写一遍**的情形在本项目**恰相反**——CI 写的构建/测试/报告逻辑（多处长 grep+awk 重复）没有抽成可共享脚本；**本地独有的** preset 同步校验反而没进 CI。即「CI 内重复、本地与 CI 割裂」。
- **建议**（CI/本地共享）：把「构建+测试+失败报告」做成一个仓库级脚本（如 `scripts/ci/build_and_test.sh`），CI 与本地开发者都调用它；把 preset 同步校验（check_preset_sync）挂进 CI 作为回归门。

---

## 5. 结论：应统一 vs 应保留

### 5.1 「一需求多脚本」——应统一（收敛到唯一权威位置）

1. **CMakeLists 修复**：`fix_cmake.ps1` 并入 `fix_build.py`（或反之），去掉重复正则逻辑。（R1）
2. **桩生成**：`gen_stubs.ps1` 硬编码清单改为**扫描 CMakeLists 引用**（同 `fix_build.py` 步骤1）并合并。（R2）
3. **预设生成链**：唯一保留 `convert_presets.py`；删除不完整孤儿 `extract_preset_data.py`；`check_preset_sync.py` 只做「只读校验」，`--fix` 改为调用 convert_presets 而非复刻生成。`PRESET_TYPE_MAP` 常量单源化。（R3）
4. **docx 汇报生成**：统一到 JS `docx_helpers.js` + 生成器；删除 `gen_report.py`（python-docx 独立栈）。（R4）
5. **目录归一**：`tool/` 与 `tools/` 合并为一个 `tools/`；`tool/scripts/`、`tool/tools/`、`tool/report_generators/` 按其职责归入 tools 下第 2 层（如 `tools/build/`、`tools/report/`）；根目录一次性 fix/split/refactor 脚本归档到 `tools/oneoff/` 或删除。（S1/S2/S4）

### 5.2 平台必要分化——应保留

- 本项目**没有**须保留的 Windows-ps1 / Unix-sh 孪生对（全部 `.sh` 属第三方依赖，项目自身 0 个）。所以「分化」不落在脚本层。
- 真正该保留的分化在 **CI 矩阵**：`ci.yml` 的 Ubuntu 完整构建 + `windows-build`(MSYS2 GCC 语法检查/Makefiles) + `python.yml` 的 3 平台矩阵。这是合理的平台分化，应保留（但可把每平台内联命令抽成可共享脚本，见 §4）。
- `.ps1`（Windows 辅助）在「项目自有 Windows 开发环境」作为一次性修复工具可保留，但应**归入 `tools/` 并压缩成单入口**，而不是散落根目录并互相重复。

---

## 6. 统一化建议（落地路径）

1. **目录归一**（先行，低风险）：
   - 顶层：`tool/` + `tools/` → `tools/`（单目录）。
   - `tools/` 下按职责分 3 个子目录：`preset/`（convert_presets、check_preset_sync）、`build/`（fix_build、gen_stubs→合并、修 CMake）、`report/`（docx_helpers + 生成器 js）。
   - `tool/scripts/gen_report.py`、`tool/tools/fix_symbolic_coord.py`、根目录一次性脚本 → `tools/oneoff/` 或删除。
2. **每功能单脚本**：
   - 合并 `fix_cmake.ps1` → `fix_build.py`。
   - 合并 `gen_stubs.ps1` → `fix_build.py`（扫描法）。
   - 保留 `convert_presets.py` 为唯一生成器；删 `extract_preset_data.py`。
   - docx 汇报只留 JS 栈 + helpers；删 `gen_report.py`。
   - `PRESET_TYPE_MAP`、`CATEGORY_MAP` 单源化（放入 convert_presets，其它 import）。
3. **CI/本地共享**：
   - 抽 `scripts/ci/build_and_test.sh`（构建+CTest+失败报告），CI 各 workflow 与本地共用。
   - 把 `tools/preset/check_preset_sync.py`（只读校验）挂入 CI 作为回归门，堵住「preset 生成-校验不连续」缺口。
4. **构建归一**：加 `CMakePresets.json` 声明 vs-debug / ninja-static-debug / ninja-shared / ninja-shared-release；删 `build_gov`、`build4`、`build_check`、`build_final`、`build_san`、`build_verify` 冗余目录；`_ctypes_binding.py` 搜索路径收敛到 preset 输出。

> 本报告为**审计输入**，未修改任何文件。实施前每个「收敛」应走一遍 code-abstraction-governance 判定内核（同构性 / 粒度门槛 / 行为等价验证 / 负面清单），并逐项跑构建+测试回归再落地。
