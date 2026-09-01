# 内存所有权契约（K10/F39）

> 单源文档：Lv-00 公共 API 的内存所有权三态契约。
> 立项：F39（K10 所有权三态契约落地，用户 2026-08-31 确认 P0）。
> 状态：契约定义 + 首批修复落地；[copy]/[take]/[borrow] 头注释全覆盖与
> 静态检查脚本为后续批次。

## 1. 三态契约

所有返回指针或接收指针的公共 API，其所有权语义必须且只能属于以下三态之一：

| 标注 | 语义 | 调用者义务 |
| --- | --- | --- |
| `[copy]` | 返回**新分配**的对象，与源对象完全独立 | 调用者负责销毁（与分配器配对释放） |
| `[take]` | 接收/返回对象的所有权转移 | 接收方负责销毁；交出方不再触碰 |
| `[borrow]` | 借用指针，不转移所有权 | 调用者不得释放；借用期不得销毁源对象 |

## 2. 分配/释放配对规则

| 分配方式 | 必须配对释放 | 反例（UB） |
| --- | --- | --- |
| `lv_malloc` / `lv_calloc` / `lv_realloc` / `lv_strdup` / `lv_strbuf_to_string` | `lv_free`（经 `lv_free((void**)&p)`） | `free(p)` —— 混合分配器 |
| 标准 `malloc` / `strdup` | `free` | `lv_free(p)` |

> 判断分配方式：头注释或实现。无法确定时以实现为准（读 .c 的分配调用）。
> `module_compute_content_hash` 曾注释「调用者负责 free()」而实际 `lv_calloc`
> 分配——已修复（批次 183，见 module.h），这是混合分配器 UB 的典型样例。

## 3. 首批标注与修复（批次 183）

| API | 实际语义 | 修复 |
| --- | --- | --- |
| `func_block_register` | `[copy]`：注册表深拷贝 fb + name/description 副本；调用者仍持有原 fb | 头注释矛盾（一处说深拷贝、一处说注册表接管）已统一为 copy 语义；原「注册后由注册表接管管理」误导删除 |
| `module_compute_content_hash` | `[take]`：返回 lv_calloc 缓冲，调用者 `lv_free` | 注释「调用者负责 free()」→「lv_free」（混合分配器 UB 修复） |

## 4. 已确认健康面（K10 复核）

- `func_block_registry_lookup`：`[copy]`（func_block_copy 深拷贝，调用者释放）——注释已正确
- Python `_PtrOwner`：运行期强制版（_ptr_owner.py，11 文件复用）——与 C 侧契约对应
- 回调所有权规则：C API 回调语义（K29 复核一致）
- `graph_get_node` 借用语义：`[borrow]`（不转移，仅 design doc 记录——待头注释补齐）

## 5. 后续批次

- [copy]/[take]/[borrow] 头注释全覆盖（~40+ 处「调用者负责 free」标注逐点核对）
- 静态检查脚本（grep 分配器/释放器配对，接入 CI）
- memory-ownership 与 API_QUICKSTART 的 _create/_get 表、TEN_LAYER_OPTIMIZED_PLAN
  的 _create/_alloc 后缀表对齐
