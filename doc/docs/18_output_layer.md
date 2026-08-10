# 18. 输出层（Layer 5）：TikZ 导出、公式渲染与跨语言互操作

## 模块概述

输出层（Layer 5）是 Lv-00 十层架构中面向外部的表达层，负责将引擎内部状态——约束图、证明树、公式 AST——转换为可被人类阅读或被其他软件系统消费的产物。本层覆盖五个能力域：

1. **TikZ 导出**：将约束图导出为 LaTeX TikZ 代码（`tikz_export.h`），供 LaTeX 文档直接排版。
2. **公式渲染**：将公式 AST 渲染为 LaTeX / Python / DSL / MathML / ASCII / HTML(MathJax) 六种文本（`formula_renderer.h`）。
3. **跨语言互操作**：通过 stdio / WebSocket / pipe 三种接口与外部系统交换数据，支持 Coq、Lean、Isabelle/HOL、HOL Light、GeoGebra、GeoJSON、SVG 以及 OPML（JSON）证明导入导出（`interop.h`）。
4. **证明格式化**：将 `ProofNavigator` 证明导航器中的证明步骤格式化为 Coq/Lean 脚本与定理调用序列（`interop.h` 定理交换子模块）。
5. **DOT 图导出**：通过公共 DOT 写入器统一生成 Graphviz 有向图（`lv_dot_writer.h`），并经由 `ga_codegen` 的 DOT 目标为 GA 表达式树提供可视化。

跨层导出样板（XML 转义与文件写出）收敛在 `lv_export_common.h`，由 layer1 导出器与 layer5 导出器共用。

**覆盖头文件**：`core/include/lv/tikz_export.h`、`formula_renderer.h`、`interop.h`、`lv_export_common.h`、`lv_dot_writer.h`。

## 核心设计原则

1. **单一事实来源**：导出格式名（`interop_export_format_name` / `interop_parse_export_format`）、几何/约束类型名（`interop_geom_type_name` / `interop_constraint_type_name`）以及信任颜色映射（`interop_trust_color_to_svg` / `interop_trust_color_to_tikz`）全部集中到 interop 内部共享表，禁止各导出器自行复制；GeomType / ConstraintType 名称表由 `constraint_graph.h` 中的 X-macro 单一生成。
2. **定长缓冲安全输出**：所有写缓冲口的 API 遵循 snprintf 语义——空间不足时截断到 `size-1` 并保证 NUL 结尾（`lv_tikz_export`、`formula_render_to_buffer*`、`interop_server_process_command`）。
3. **统一转义**：SVG/HTML 导出共用 `lv_export_xml_escape`，实体语义与 `lv_str_escape_xml` 逐字节一致；DOT label 统一经 `lv_str_json_escape` 转义（JSON 字面量转义是 DOT 字符串转义子集），消除各调用点重复且未转义输出。
4. **插件化外部系统桥接**：外部证明系统（Coq/Lean/OPML 等）通过 `lvInteropPlugin` 注册表接入，导出/导入/验证三个回调（`export_proof` / `import_proof` / `validate`）构成统一协议；OPML 编解码器（`opml_codec.c`）即经由 `lv_register_opml_plugin` 注册为 `lv_EXT_JSON` 插件。
5. **流式与批式双通道**：导出既可一次性批式写文件（`interop_export_*`），也可经 `InteropServer` 的流式输出（`INTEROP_CMD_STREAM_*`）实时推送，与 layer6 流上下文（`interop_stream_ctx`）联动。

## 关键数据结构

```c
/* interop.h —— 导出格式与配置 */
typedef enum {
    INTEROP_EXPORT_COQ = 0,   /* Coq */
    INTEROP_EXPORT_LEAN,      /* Lean */
    INTEROP_EXPORT_HTML,      /* 独立 HTML */
    INTEROP_EXPORT_SVG,       /* SVG 矢量图 */
    INTEROP_EXPORT_PDF,       /* PDF 文档 */
    INTEROP_EXPORT_TIKZ,      /* LaTeX TikZ */
    INTEROP_EXPORT_GEOJSON,   /* GeoJSON */
    INTEROP_EXPORT_CANONICAL, /* 规范表示 */
    INTEROP_EXPORT_ISABELLE,  /* Isabelle/HOL */
    INTEROP_EXPORT_HOL_LIGHT  /* HOL Light */
} InteropExportFormat;

typedef struct {
    InteropExportFormat format;             /* 导出格式 */
    char output_path[INTEROP_MAX_PATH_LEN]; /* 输出路径 */
    bool include_proofs;                    /* 包含证明 */
    bool include_metadata;                  /* 包含元数据 */
    bool pretty_print;                      /* 美化输出 */
    int compression_level;                  /* 压缩级别 */
} InteropExportConfig;
```

```c
/* interop.h —— 外部证明系统插件与定理交换上下文 */
typedef enum { lv_EXT_COQ, lv_EXT_LEAN4, lv_EXT_JSON, lv_EXT_COUNT } lvExternalSystem;

struct lvInteropPlugin {
    char name[64];
    char version[32];
    lvExternalSystem system;
    int (*export_proof)(void *, char *, int);   /* 导出证明 */
    int (*import_proof)(const char *, void **); /* 导入证明 */
    int (*validate)(const char *);              /* 验证 */
};

typedef struct {
    char trust_base_name[64];    /* 信任基名称 */
    char trust_base_version[32]; /* 信任基版本 */
    char *exported_calls;        /* 导出的调用序列 */
    size_t calls_len;
    size_t calls_capacity;
} InteropTheoremContext;
```

```c
/* interop.h —— 互操作服务器（兼容别名 lvInteropManager） */
typedef struct {
    InteropInterfaceType type; /* STDIO / WEBSOCKET / PIPE */
    int port;
    bool running;
    void *internal_data;
    void *persistent_engine; /* lvEngine*，首次命令时惰性创建 */
    int engine_in_use;
    bool stream_enabled;
    int stream_callback_id;
    uint64_t stream_filter_mask;
    long stream_events_sent;
} InteropServer;
```

```c
/* lv_dot_writer.h —— 输出目标为 lvStrBuf（SSO 小字符串优化缓冲） */
typedef struct lvStrBuf {
    char *data;
    char stack[lv_STRBUF_SSO_SIZE]; /* SSO 栈缓冲，256 字节 */
    size_t len;
    size_t cap;
} lvStrBuf;
```

## 主要接口

| 模块 | 函数签名 | 说明 |
|------|----------|------|
| TikZ 导出 | `int lv_tikz_export(void *graph, char *out, size_t buf_size)` | 约束图 → TikZ 字符串（snprintf 语义） |
| TikZ 导出 | `int lv_tikz_export_file(void *graph, const char *filename)` | 约束图 → TikZ 文件 |
| TikZ 导出 | `static inline int tikz_byte(float c)` | [0,1] 浮点颜色 → [0,255] 字节 |
| 公式渲染 | `char *formula_render_ex(const FormulaNode *node, OutputFormat format, const RenderOptions *options)` | 通用渲染（可空选项用默认值） |
| 公式渲染 | `int formula_render_to_buffer_ex(const FormulaNode *node, OutputFormat format, const RenderOptions *options, char *buffer, size_t size)` | 渲染到定长缓冲 |
| 公式渲染 | `char *formula_render_latex(const FormulaNode *node)` 等格式特定入口 | LaTeX / Python / DSL 快捷渲染 |
| 公式渲染 | `char *formula_render_point_latex(const char *name, const FormulaNode **coords, int coord_count)` | 几何点 LaTeX（如 `A = \left(1, 2\right)`） |
| 互操作导出 | `int interop_export_coq(const ProofNavigator *proof, const InteropExportConfig *config)` | 证明 → Coq |
| 互操作导出 | `int interop_export_lean(const ProofNavigator *proof, const InteropExportConfig *config)` | 证明 → Lean |
| 互操作导出 | `int interop_export_html(const lvEngine *engine, const InteropExportConfig *config)` | 独立 HTML 演示包 |
| 互操作导出 | `int interop_export_svg(const ConstraintGraph *graph, const InteropExportConfig *config)` | 约束图 → SVG |
| 互操作导出 | `int interop_export_canonical(const ConstraintGraph *graph, const char *output_path)` | 约束图 → 规范表示 |
| 互操作导出 | `int interop_export_geojson(const ConstraintGraph *graph, const InteropExportConfig *config)` | 约束图 → GeoJSON |
| 互操作导入 | `int interop_import_geogebra / interop_import_geojson / interop_import_svg(lvEngine *engine, const InteropImportConfig *config)` | GeoGebra / GeoJSON / SVG 导入 |
| 服务器 | `InteropServer *interop_server_create(InteropInterfaceType type)`；`int interop_server_start(InteropServer *server, int port)` | 服务器创建与启动 |
| 服务器 | `int interop_server_process_command(InteropServer *server, const char *input, char *output, size_t output_size)` | stdio 命令处理 |
| 服务器 | `int interop_parse_command(const char *input, InteropCommand *cmd)`；`int interop_execute_command(lvEngine *engine, const InteropCommand *cmd, InteropResponse *resp)` | 命令解析与执行 |
| 定理交换 | `int interop_theorem_add_call(InteropTheoremContext *ctx, const char *theorem_name, const char **params, int param_count)` | 追加定理调用 |
| 定理交换 | `int interop_theorem_export_calls(const InteropTheoremContext *ctx, InteropExportFormat format, char *output, size_t output_size)` | 导出调用序列（coq/lean） |
| 定理交换 | `int interop_import_external_theorem(lvEngine *engine, const char *trust_base_name, const char *content_hash, const char *description, int *block_id)` | 导入外部定理为可信基块 |
| 插件注册 | `int lv_interop_register_plugin(lvInteropManager *mgr, const lvPlugin *plugin)`；`int lv_interop_reset_plugins(void)` | 注册/重置外部系统插件 |
| DOT | `void lv_dot_begin(lvStrBuf *sb, const char *graph_name, const char *rankdir, const char *node_defaults, const char *edge_defaults)` | digraph 头 |
| DOT | `void lv_dot_node(lvStrBuf *sb, const char *id, const char *label, const char *extra_attrs)`；`lv_dot_edge(...)` | 节点/边语句（label 经 JSON 转义） |
| DOT | `void lv_dot_end(lvStrBuf *sb)`；`bool lv_dot_write_file(const char *path, const char *content, size_t len)` | 收尾与落盘 |
| 公共工具 | `void lv_export_xml_escape(const char *src, char *dst, size_t dst_size)` | XML 实体转义 |
| 公共工具 | `int lv_export_write_file(const char *path, const void *data, size_t len)` | 一次性写文件（失败返回 -1） |
| 工具查询 | `const char *interop_export_format_name(InteropExportFormat format)`；`InteropExportFormat interop_parse_export_format(const char *str)` | 格式名双向解析 |
| 工具查询 | `const char *interop_trust_color_to_svg(TrustColor trust)`；`const char *interop_trust_color_to_tikz(TrustColor trust)` | 信任颜色 → SVG/TikZ 颜色 |
| 工具查询 | `char **interop_get_command_completions(lvEngine *engine, const char *prefix, int *out_count)` | 命令补全（`interop_free_completions` 释放） |

## 工作流程

1. **TikZ 导出流水线**：`lv_tikz_export` 遍历约束图节点/边，将坐标与几何元素映射为 TikZ 绘图原语，颜色经 `tikz_byte` 从浮点转换；`lv_tikz_export_file` 负责落盘。公式标注复用 `formula_render_latex` 生成的排版串。
2. **SVG/HTML 导出流水线**：`interop_export_svg` 经 `interop_trust_color_to_svg` 获取信任颜色、`interop_geom_type_name` 获取类型标签，文本统一经 `lv_export_xml_escape` 转义后由 `lv_export_write_file` 写出；`interop_export_html` 进一步把 `formula_render` 的 MathJax 片段嵌入独立 HTML 包。
3. **证明格式化与导出**：`interop_export_coq` / `interop_export_lean` 遍历 `ProofNavigator` 步骤，按 trust base 元数据（`InteropTheoremContext`）逐步发射定理调用；`interop_theorem_export_calls` 将累计调用序列格式化为目标语言脚本。
4. **OPML 交换**：`lv_register_opml_plugin` 将 `opml_codec.c` 的导出/导入/校验三回调注册为 `lv_EXT_JSON` 插件，`lv_interop_register_plugin` 挂入管理器后，即可经 `lvInteropPlugin.export_proof` / `import_proof` 完成 OPML JSON 与内部证明树互转。
5. **DOT 图生成**：调用方构造 `lvStrBuf`，按 `lv_dot_begin → lv_dot_node/lv_dot_edge → lv_dot_end` 顺序组装（label 自动 JSON 转义），最后 `lv_dot_write_file` 落盘；证明树 DOT（`proof.dot`）、GA 表达式树 DOT（`ga_render_dot`）均走此公共写入器。

## 模块关系

| 关联模块 | 编号文档 | 关系说明 |
|----------|----------|----------|
| 证明导出与追踪组件 | [22_proof_export_trace_widget.md](22_proof_export_trace_widget.md) | 输出层消费其 `ProofNavigator` / 证明追踪结果，`interop_export_coq/lean` 与定理交换以此为输入源 |
| 流处理与互操作 | [31_stream_interop.md](31_stream_interop.md) | `InteropServer` 复用 stream 事件体系，`interop_stream_callback` 与 `interop_stream_ctx` 把导出进度实时推向前端 |
| Gappa 数值验证 | [33_gappa_verification.md](33_gappa_verification.md) | 数值验证结论进入导出元数据，`include_proofs` / `include_metadata` 控制验证证据随证明一起输出 |
| 合一子系统 | [06_unify.md](06_unify.md) | 证明格式化过程中合一步骤的呈现依赖合一子系统给出的替换与回溯信息 |
| 推理层 | [17_reasoning_layer.md](17_reasoning_layer.md) | 证明导出输入的 `ProofNavigator` 由推理层证明引擎（proof.h）生成 |
| Layer 6 图形化编程 | [35_layer6_visual_programming.md](35_layer6_visual_programming.md) | SVG/TikZ/DOT 产物作为 Layer 6 可视化的下游输入，支撑图形化构造回放 |
| 几何层 | [16_geometry_layer.md](16_geometry_layer.md) | 导出对象（点/线/圆/约束）的类型与信任颜色源自几何层 `constraint_graph.h` / `symbolic_coord.h` |

## 版本历史

| 版本 | 日期 | 变更 |
|------|------|------|
| 1.0.0 | 2026-08-10 | 首版：整合 Layer 5 五个输出能力域，收录 `lv_export_common.h` 跨层样板与 `lv_dot_writer.h` 公共写入器 |
