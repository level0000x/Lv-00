# G6 插件安全组设计要点（供确认后实现）

> 蓝图来源：TEN_LAYER_OPTIMIZED_PLAN §4.1.1（插件系统）/ §16.1（签名/沙箱/权限/审计）/ §16.2.1（DSL 注入检测）。
> 库内现状：`plugin_system.h` 已有 52 个插件生命周期 API（load/activate/config/interface/
> get_info_json）——G6 仅补**安全层**（签名验证、权限模型、审计、沙箱、DSL 注入检测），
> 不重复插件加载。12 个 API 分 6 组。

---

## ① 插件描述符查询（1 个：lv_plugin_get_descriptor ⚠️）

**蓝图**：`const lvPluginDescriptor* lv_plugin_get_descriptor(void)`——插件入口宏
`lv_PLUGIN_ENTRY()` 要求插件导出它。

**现状**：库内 `plugin_system.h` 的 `lv_plugin_get_info_json(plugin)` 已提供插件信息 JSON
（名称/版本/状态）。蓝图 lvPluginDescriptor 是「插件自身导出的描述符」——库内插件模型
用 JSON 而非描述符结构体。

**设计**：不引入第二套描述符结构。`lv_plugin_get_descriptor` 声明为**插件侧可选导出**
的兼容入口——库内提供弱引用桥：若插件导出该符号则返回；否则返回 NULL（文档注明
「库内插件应使用 lv_plugin_get_info_json 查询，本符号为蓝图兼容」）。

---

## ② 签名验证（3 个：verify_signature / add_trusted_key / set_enforcement ❌）

**蓝图**：`lvSignatureResult lv_plugin_verify_signature(plugin_path, manifest_path)`；
`bool lv_plugin_add_trusted_key(public_key_pem, key_id)`；`void lv_plugin_set_enforcement(bool)`。

**库内设施**：`lv_hash.h` 有 SHA-256（lv_hash_init/update/digest）+ FNV-1a。
**无** RSA/Ed25519 公钥库。

**设计（务实基础版）**：
- 签名 = 插件文件（或 manifest）的 **SHA-256 哈希** + 签名文件（`.sig` 存期望哈希 hex）。
- `lv_plugin_verify_signature(path, manifest)`：计算插件文件 SHA-256，与签名文件比对
  → lv_SIG_OK / NO_SIGNATURE / HASH_MISMATCH / INVALID_FORMAT / INTERNAL_ERROR
  （无真非对称签名——文档注明「对称哈希校验，非密码学强签名」；若未来引入
  libsodium/openssl 可替换校验器）。
- `lv_plugin_add_trusted_key(pem, key_id)`：信任表存 key_id→哈希摘要（校验白名单）；
  pem 未解析（仅存 key_id + 摘要字符串），文档注明。
- `lv_plugin_set_enforcement(bool)`：进程级开关；enforce=true 时
  `lv_plugin_load` 路径自动校验（接线 plugin_system 的加载前钩子——若不便接线则文档
  注明「调用方在 load 前显式 verify」）。

---

## ③ 沙箱（3 个：lv_sandbox_readonly / apply / check ❌）

**蓝图**：`lvSandboxConfig`（CPU 时限/RSS/路径白名单/网络/fork/fd/线程）+ readonly 默认
+ apply（真沙箱：seccomp/资源限制）/ check（校验配置）。

**设计（诚实范围）**：**本库是嵌入式 C 库，不做进程隔离**（无子进程执行模型）。
- `lv_sandbox_readonly()`：返回蓝图默认配置（纯数据构造，可完整实现）。
- `lv_sandbox_apply(config)`：真沙箱需 OS 级（setrlimit/prlimit/seccomp）——
  Windows 主开发环境无 seccomp。实现为**校验 + 记录**：配置合法（时限/字节数>0 等）
  则记录为「已应用的沙箱配置」（进程级，供 check/审计引用），返回 true；不执行 OS 限制。
  文档明确「配置记录模式，非强制隔离」。
- `lv_sandbox_check(config, violation, len)`：校验配置字段合法性（负值/矛盾路径等），
  违规写入 violation 描述返回 false。

---

## ④ 权限模型（4 个：lv_REQUIRE_PERMISSION 宏 / lv_plugin_get_permission / lv_perm_level_str / lv_audit_log）

**蓝图**：`lvPermissionLevel {READONLY=0, CONSTRUCTION=1, FULL=2}`；宏
lv_REQUIRE_PERMISSION(plugin, level, retval) 检查并审计拒绝；lv_perm_level_str 名称；
lv_audit_log(plugin, type, fmt, ...)。

**库内现状**：plugin_system.h 的 lvPlugin 不透明，无权限字段；lv_log 有统一日志。

**设计**：
- 权限表：进程级 plugin_name→lvPermissionLevel（默认 READONLY），
  `lv_plugin_set_permission(name, level)` 新增设置 API（G6 扩展，文档注明）。
- `lv_plugin_get_permission(lvPlugin*)`：从插件名查表（插件名经 lv_plugin_get_info_json
  或内部名获取——lvPlugin 不透明，若无法取名字符串则文档注明用 plugin 指针哈希
  登记）。设计选：**plugin 名→权限**用 `lv_plugin_find` 反查或登记名。
- `lv_REQUIRE_PERMISSION` 宏 + `lv_perm_level_str`：完整实现。
- `lv_audit_log(plugin, type, fmt, ...)`：格式化后经 lv_log（tag="audit"）输出
  `[audit] plugin=... event=<type> msg=...`；lvAuditEventType 枚举照蓝图。

---

## ⑤ DSL 注入检测（1 个：lv_dsl_security_check ❌）

**蓝图**：`lvErrorCode lv_dsl_security_check(input, len, error, err_len)`——预定义注入
模式（`;rm`/`|sh`/`../`/`%n`/`#include`/`#define` 等）组合检查。

**设计**：完整实现——扫描 input 中的注入模式表，命中写 error 并返回
lv_ERROR_INVALID_PARAM（或蓝图语义的拒绝码）；表可配置（原则 8：配置走配置不硬编码
——注入模式表做成 API 可扩展 `lv_dsl_add_injection_pattern`，默认内置 6 条）。

---

## ⑥ 依赖与层

- 新头：`core/include/lv/plugin_security.h`（声明全部 12 API + 枚举/结构/宏）。
- 实现：`core/src/layer8_meta_verify/plugin_security.c`（安全层归属元验证层，
  依赖 L2 的 lv_hash/lv_log/lv_utils + L4 plugin_system 的 lvPlugin 类型）。
  若层依赖不允许，则归 L4（plugin 所在层）——实现时按 layer_dep_matrix 校验调整。
- 权限/沙箱配置：进程级静态（线程安全用 lv_LAZY_LOCK_DEFINE），不落盘
  （库内无持久配置需求；如需持久化走 lv_config）。

---

## 红线合规自检

- 原则 8（可配置走配置）：注入模式表可扩展、沙箱配置入参、权限级别可设 → 不硬编码行为。
- 原则 9（最优优先）：签名用库内 SHA-256（无外部依赖）；沙箱诚实标注范围
  （不做假 seccomp）；权限/审计接线现有 lv_log。
- 测试：每 API 契约用例（verify 正/负路径、sandbox config 校验、权限检查宏触发审计、
  DSL 注入模式命中/放行）+ ctest 全绿 + 三层测试（等价/边界/性质）。

---

**请确认**：以上 6 组设计（尤其 ②签名=哈希校验非密码学强签名、③沙箱=配置记录非真隔离
的诚实范围）是否可接受？确认后按此实现 12 个 API。
