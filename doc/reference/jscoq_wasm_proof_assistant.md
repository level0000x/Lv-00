# jsCoq WASM 证明助手 核心借鉴设计

> **借鉴项目**：jsCoq（github.com/jscoq/jscoq）
> **核心借鉴点**：Coq → WASM 完整编译管道（js_of_ocaml）、Fleche 增量文档检查器、LSP 风格证明交互协议、addon 包管理生态
> **分类**：P2 高优先级 / Web 移植与证明交互协议
> **日期**：2026-05-24

---

## 1. 概述

jsCoq 是一个将完整 Coq 证明助手移植到浏览器中运行的开源项目。它通过 js_of_ocaml 编译器将 Coq 的 OCaml 实现编译为 JavaScript，然后进一步编译为 WebAssembly（WASM），最终在浏览器中提供原生的 Coq 证明环境。jsCoq 证明了**重型数学工具在浏览器中运行的可行性**——这一事实对 Lv-00 的 Web 移植战略具有决定性意义：如果 Coq（一个包含完整内核、策略引擎、标记引擎和标准库的证明助手）可以在浏览器中运行，那么 Lv-00 的 C 语言几何引擎完全可以通过 Emscripten → WASM 路径实现相同的浏览器部署。

jsCoq 对 Lv-00 的核心借鉴价值分为四个层面。第一，**js_of_ocaml → WASM 编译管道**为 Lv-00 的 C → Emscripten → WASM 移植路径提供了工程上经过验证的参考模式——包括内存管理、文件系统虚拟化、与 JavaScript 的互操作策略。第二，**Fleche 增量文档检查器**（jsCoq 2.0+ 的核心组件）实现了"每次编辑操作触发局部重新检查，而非全量重建"的增量验证架构——这与 Lv-00 约束图的增量求解需求精确对应。第三，**LSP 风格通信协议**定义了前端 GUI 与后端引擎之间交换证明状态、策略执行、搜索等原语的标准化接口——这为 Lv-00 的 `engine ↔ Web GUI` 通信协议设计提供了可直接参考的成熟方案。第四，**addon 包管理生态**结合 IndexedDB 本地持久化，证明了浏览器中也可以存在类似 opam/npm 的包管理系统——Lv-00 的公理包生态可以借鉴此模式实现浏览器内的包安装、缓存和版本管理。

最重要的是，jsCoq 验证了一个关键假设：**交互式证明的用户体验不必依赖本地安装的二进制文件**。用户只需打开浏览器即可进入完整的证明环境，代码和数据本地持久化，性能足以支持实时交互。

---

## 2. js_of_ocaml → WASM 编译管道

### 2.1 jsCoq 的编译链路

jsCoq 将 Coq（OCaml 实现）带到浏览器的编译链路如下：

```
Coq 源码（OCaml）
  │
  ├─ [ocamlc / ocamlopt]
  │   编译为 OCaml 字节码 / 原生码
  │
  ├─ [js_of_ocaml]
  │   将 OCaml 字节码编译为 JavaScript
  │   关键转换：
  │   - OCaml int32/int64 → JavaScript BigInt
  │   - OCaml 字符串 → JavaScript String
  │   - OCaml 数组 → JavaScript Array
  │   - OCaml 闭包 → JavaScript closures
  │   - OCaml GC → JavaScript 垃圾回收（依赖 JS 引擎）
  │
  ├─ [WASM backend of js_of_ocaml]
  │   将 js_of_ocaml 输出进一步编译为 WASM
  │   （可选：也可直接使用 asm.js 输出）
  │
  └─ [JavaScript 胶水代码]
       ├─ 文件系统虚拟化（MEMFS + IDBFS）
       ├─ Worker 线程管理
       ├─ DOM 事件桥接
       └─ Coq 标准库加载器
```

### 2.2 映射到 Lv-00 的 C → Emscripten → WASM 路径

| jsCoq 编译环节 | Lv-00 对应环节 | 说明 |
|:---|:---|:---|
| OCaml → js_of_ocaml → JS | C → Emscripten → WASM + JS 胶水 | 源码语言不同，编译目标类似 |
| OCaml GC → JS 引擎 GC | Lv-00 手动内存管理 → Emscripten 堆管理 | Lv-00 不依赖 GC，简化移植 |
| 文件系统虚拟化（MEMFS + IDBFS） | Emscripten 虚拟文件系统 | 公理包文件的虚拟化存储 |
| Worker 线程管理 | Emscripten pthread + Web Worker | CPU 密集型求解在 Worker 中运行 |
| Coq 标准库预加载 | Lv-00 公理包预加载 | 标准公理包随页面一同加载 |

### 2.3 Lv-00 的 Emscripten 编译配置

```c
/**
 * @brief Lv-00 WASM 编译配置
 *
 * 借鉴 jsCoq 的 js_of_ocaml → WASM 编译管道，
 * 使用 Emscripten 将 Lv-00 C 引擎编译为 WASM。
 *
 * 编译命令示例：
 *   emcc lv00_engine.c -o lv00.js \
 *     -s WASM=1 \
 *     -s EXPORTED_FUNCTIONS='["_lv00_init", "_lv00_solve", "_lv00_prove",
 *         "_lv00_get_proof_state", "_lv00_execute_tactic",
 *         "_lv00_load_package", "_lv00_free"]' \
 *     -s EXPORTED_RUNTIME_METHODS='["ccall", "cwrap", "FS", "IDBFS"]' \
 *     -s ALLOW_MEMORY_GROWTH=1 \
 *     -s INITIAL_MEMORY=64MB \
 *     -s MAXIMUM_MEMORY=512MB \
 *     -s MODULARIZE=1 \
 *     -s EXPORT_NAME='Lv00Engine' \
 *     -O3
 */

/**
 * @brief Lv-00 WASM 引擎初始化
 *
 * 借鉴 jsCoq 的加载流程：
 *  1. 下载 WASM 二进制
 *  2. 初始化虚拟文件系统（MEMFS）
 *  3. 挂载 IndexedDB 持久化层（IDBFS）
 *  4. 加载预置公理包
 *  5. 返回引擎句柄
 */
```

---

## 3. Fleche 增量文档检查器

### 3.1 Fleche 的核心设计

Fleche 是 jsCoq 2.0+ 引入的增量文档检查器，其核心设计原则是**"每个编辑操作仅触发局部重新检查"**——这对比传统 Coq 的全量重检（从头到尾运行整个文档）有数量级的性能提升。

Fleche 的工作流程：

```
编辑器文档（.v 文件）
  │
  ├─ 编辑操作（字符插入/删除）
  │   │
  │   ├─ [Fleche 增量解析]
  │   │   仅重新解析受影响的句子（sentence）
  │   │   使用句子锚点（sentence anchor）追踪变化范围
  │   │
  │   ├─ [Fleche 增量检查]
  │   │   仅重新检查受影响的句子范围
  │   │   复用未变化句子的检查缓存
  │   │   ├─ 前向依赖：变化句子之后的所有句子可能需重检
  │   │   └─ 后向无关：变化句子之前的句子缓存有效
  │   │
  │   └─ [检查结果更新]
  │       仅发送变化的诊断信息
  │
  └─ 增量结果 → LSP → 编辑器 UI
```

### 3.2 Fleche 增量检查映射到 Lv-00 约束图的增量求解

Fleche 的增量检查架构与 Lv-00 约束图的增量求解需求高度对应：

| Fleche 概念 | Lv-00 约束图映射 | 说明 |
|:---|:---|:---|
| 句子（Sentence） | 约束图节点（ConstraintNode） | 最小可重检单元 |
| 句子锚点（Anchor） | 节点 ID + 拓扑序 | 定位变化范围 |
| 增量解析范围 | 受影响的子图 | 从变化节点出发的拓扑可达子图 |
| 前向依赖重检 | 拓扑下游传播 | 受影响的节点和约束边 |
| 缓存复用 | 约束验证结果缓存 | 未受影响节点的结果保留 |
| 诊断信息差分 | 约束违反报告差分 | 仅发送新产生/消除的违反 |

### 3.3 增量约束求解器设计

```c
/**
 * @brief 增量约束求解器——借鉴 jsCoq Fleche 增量检查架构
 *
 * Lv-00 的约束图支持增量编辑（添加/删除/修改几何对象），
 * 每次编辑后需要重新检查约束。借鉴 Fleche，仅重新求解
 * 受影响的子图部分。
 *
 * 核心数据结构：
 *  - 节点拓扑序（Topological Order）：用于快速判断依赖方向
 *  - 约束验证缓存（Constraint Cache）：避免重复验证
 *  - 脏标记（Dirty Flags）：追踪需要重新验证的约束
 */

/**
 * @brief 约束图增量求解器
 */
typedef struct {
    ConstraintGraph *graph;          /**< 约束图 */
    int *topological_order;          /**< 节点拓扑排序 */
    int topo_count;                  /**< 拓扑序大小 */
    int *dirty_nodes;                /**< 脏节点列表（需重新验证） */
    int dirty_count;                 /**< 脏节点数量 */
    bool *constraint_cache_valid;    /**< 约束验证缓存有效性 */
    DecisionResult *constraint_cache;/**< 约束验证结果缓存 */
    int *affected_constraints;       /**< 增量求解影响的约束列表 */
    int affected_count;              /**< 影响的约束数量 */
} IncrementalConstraintSolver;

/**
 * @brief 标记编辑操作的影响范围
 *
 * 借鉴 Fleche 的增量检查：当节点被修改时，
 * 仅标记从该节点出发的拓扑下游节点为脏节点。
 *
 * @param solver  增量求解器
 * @param node_id 被编辑的节点
 * @return 被标记为脏的节点数量
 */
int ics_mark_dirty(IncrementalConstraintSolver *solver, int node_id);

/**
 * @brief 增量重新求解
 *
 * 仅对脏节点执行约束验证。
 * 未受影响的节点复用缓存的验证结果。
 *
 * @param solver  增量求解器
 * @param out_new_violations    输出：新产生的约束违反
 * @param out_resolved          输出：已解决的约束违反
 * @return 求解状态
 */
SolverStatus ics_solve_incremental(
    IncrementalConstraintSolver *solver,
    int **out_new_violations,
    int *out_new_count,
    int **out_resolved,
    int *out_resolved_count
);
```

### 3.4 增量求解性能对比

Fleche 的增量检查带来的性能提升可作为 Lv-00 预期性能的参考：

```
场景：1000 节点约束图，修改 1 个节点

传统全量求解：
  - 重新验证全部 1000 个节点
  - 假设每个节点 1ms → 1000ms

增量求解（Fleche 模式）：
  - 仅重新验证受影响的节点（假设 50 个）
  - 缓存复用 950 个节点的结果
  - 50ms（20 倍加速）

UI 交互体验：
  - 全量：1s 延迟，用户感知明显卡顿
  - 增量：50ms，感知为即时响应（<100ms 阈值）
```

---

## 4. LSP 风格证明交互协议

### 4.1 jsCoq 的通信原语

jsCoq 通过 LSP 风格的消息协议将证明引擎（后端）与 Web GUI（前端）解耦。核心通信原语：

| 原语 | 方向 | 语义 | Lv-00 映射 |
|:---|:---|:---|:---|
| `proof/goals` | 请求→响应 | 获取当前证明目标 | 获取约束求解状态 |
| `proof/tactic` | 请求→响应 | 执行一个策略 | 执行一个构造/证明步骤 |
| `proof/search` | 请求→响应 | 搜索适用的策略 | 搜索适用的公理/定理 |
| `proof/peek` | 请求→响应 | 窥视策略效果（不提交） | 预览构造步骤的几何效果 |
| `proof/undo` | 请求→响应 | 撤销最后一步 | 撤销最后的构造/求解步骤 |
| `proof/state` | 通知（←） | 证明状态变化通知 | 约束图变化通知 |
| `proof/diagnostics` | 通知（←） | 诊断信息（错误/警告） | 约束违反/精化失败通知 |
| `textDocument/didChange` | 通知（→） | 文档编辑通知 | 几何构造编辑通知 |
| `textDocument/didSave` | 通知（→） | 文档保存通知 | 公理包保存通知 |
| `workspace/configuration` | 请求→响应 | 获取配置 | 获取引擎/求解器配置 |

### 4.2 Lv-00 Web GUI 通信协议

```c
/**
 * @brief Lv-00 Web GUI 通信协议——借鉴 jsCoq LSP 风格协议
 *
 * 协议由 JSON 消息组成，通过 WebSocket 或 postMessage
 * 在前端（Web GUI）和后端（WASM 引擎）之间交换。
 *
 * 消息格式：
 * {
 *   "jsonrpc": "2.0",
 *   "id": <request_id>,
 *   "method": "<method_name>",
 *   "params": { ... }
 * }
 *
 * 响应格式：
 * {
 *   "jsonrpc": "2.0",
 *   "id": <request_id>,
 *   "result": { ... }
 * }
 */

/**
 * @brief 协议方法定义
 */
typedef enum {
    /* 证明状态查询 */
    LV00_METHOD_GET_GOALS,           /**< lv00/getGoals —— 获取当前目标 */
    LV00_METHOD_GET_CONTEXT,         /**< lv00/getContext —— 获取当前上下文 */
    LV00_METHOD_GET_CONSTRAINTS,     /**< lv00/getConstraints —— 获取活跃约束 */

    /* 证明操作 */
    LV00_METHOD_EXECUTE_TACTIC,      /**< lv00/executeTactic —— 执行策略 */
    LV00_METHOD_UNDO,                /**< lv00/undo —— 撤销操作 */
    LV00_METHOD_REDO,                /**< lv00/redo —— 重做操作 */
    LV00_METHOD_ABORT,               /**< lv00/abort —— 放弃当前证明 */

    /* 搜索与预览 */
    LV00_METHOD_SEARCH,              /**< lv00/search —— 搜索适用定理 */
    LV00_METHOD_PREVIEW,             /**< lv00/preview —— 预览构造效果 */
    LV00_METHOD_CHECK,               /**< lv00/check —— 检查表达式类型 */

    /* 构造操作 */
    LV00_METHOD_CREATE_OBJECT,       /**< lv00/createObject —— 创建几何对象 */
    LV00_METHOD_DELETE_OBJECT,       /**< lv00/deleteObject —— 删除几何对象 */
    LV00_METHOD_MODIFY_OBJECT,       /**< lv00/modifyObject —— 修改几何对象 */
    LV00_METHOD_ADD_CONSTRAINT,      /**< lv00/addConstraint —— 添加约束 */
    LV00_METHOD_REMOVE_CONSTRAINT,   /**< lv00/removeConstraint —— 移除约束 */

    /* 可视化 */
    LV00_METHOD_RENDER,              /**< lv00/render —— 请求重绘 */
    LV00_METHOD_HIGHLIGHT,           /**< lv00/highlight —— 高亮几何元素 */

    /* 包管理 */
    LV00_METHOD_LOAD_PACKAGE,        /**< lv00/loadPackage —— 加载公理包 */
    LV00_METHOD_LIST_PACKAGES,       /**< lv00/listPackages —— 列出公理包 */
    LV00_METHOD_INSTALL_PACKAGE,     /**< lv00/installPackage —— 安装公理包 */

    /* 通知（后端 → 前端） */
    LV00_NOTIFY_STATE_CHANGED,       /**< lv00/stateChanged —— 状态变化 */
    LV00_NOTIFY_DIAGNOSTICS,         /**< lv00/diagnostics —— 诊断信息 */
    LV00_NOTIFY_PROGRESS,            /**< lv00/progress —— 求解进度 */
    LV00_NOTIFY_ERROR,               /**< lv00/error —— 错误通知 */
} Lv00ProtocolMethod;
```

### 4.3 协议消息的 C 序列化/反序列化

```c
/**
 * @brief 协议消息结构
 */
typedef struct {
    char *jsonrpc;                 /**< "2.0" */
    int64_t id;                    /**< 请求 ID（通知时可为 0） */
    Lv00ProtocolMethod method;     /**< 方法 */
    cJSON *params;                 /**< 参数 JSON 对象 */
} Lv00Message;

/**
 * @brief 协议响应结构
 */
typedef struct {
    char *jsonrpc;                 /**< "2.0" */
    int64_t id;                    /**< 对应请求的 ID */
    cJSON *result;                 /**< 结果 JSON 对象（成功时） */
    cJSON *error;                  /**< 错误 JSON 对象（失败时） */
} Lv00Response;

/**
 * @brief 解析收到的协议消息
 */
Lv00Message *lv00_protocol_parse(const char *json_str);

/**
 * @brief 序列化协议响应
 */
char *lv00_protocol_serialize_response(const Lv00Response *resp);

/**
 * @brief 处理协议消息的调度器
 *
 * 借鉴 jsCoq 的消息分发机制：根据 method 字段
 * 路由到对应的处理函数。
 */
Lv00Response *lv00_protocol_dispatch(
    const Lv00Message *msg,
    ProofSession *session
);
```

### 4.4 代码示例：LSP 风格证明状态查询与策略执行

```c
/**
 * @brief 示例：Lv-00 Web 前端的 LSP 风格证明交互
 *
 * 本示例展示前端 JavaScript 如何通过 LSP 风格协议
 * 与 WASM 中的 Lv-00 引擎交互：获取证明状态、执行策略。
 */

// --- JavaScript 前端代码 ---

/**
 * Lv-00 Web 前端客户端
 * 借鉴 jsCoq 的 LSP 风格通信
 */
class Lv00WebClient {
    constructor(wasmInstance) {
        this.engine = wasmInstance;
        this.requestId = 0;
        this.pendingRequests = new Map();
    }

    /**
     * 发送请求并等待响应
     */
    async sendRequest(method, params = {}) {
        const id = ++this.requestId;
        const msg = JSON.stringify({
            jsonrpc: "2.0",
            id: id,
            method: method,
            params: params
        });

        return new Promise((resolve, reject) => {
            this.pendingRequests.set(id, { resolve, reject });

            // 调用 WASM 函数处理消息
            const responseJson = this.engine.ccall(
                'lv00_protocol_handle_message',
                'string',
                ['string'],
                [msg]
            );

            const response = JSON.parse(responseJson);
            this.pendingRequests.delete(id);

            if (response.error) {
                reject(new Error(response.error.message));
            } else {
                resolve(response.result);
            }
        });
    }

    /**
     * 获取当前证明状态（对应 jsCoq 的 proof/goals）
     */
    async getProofState() {
        const result = await this.sendRequest('lv00/getGoals');
        return {
            goals: result.goals,              // 待证明的目标列表
            context: result.context,           // 当前上下文
            constraints: result.constraints,   // 活跃约束
            proofTree: result.proofTree        // 证明树结构
        };
    }

    /**
     * 执行构造策略（对应 jsCoq 的 proof/tactic）
     */
    async executeTactic(tacticName, tacticParams = {}) {
        const result = await this.sendRequest('lv00/executeTactic', {
            tactic: tacticName,
            params: tacticParams
        });

        return {
            success: result.success,
            newGoals: result.newGoals || [],
            solvedGoals: result.solvedGoals || [],
            diagnostics: result.diagnostics || []
        };
    }

    /**
     * 搜索适用定理/公理（对应 jsCoq 的 proof/search）
     */
    async searchApplicable(pattern) {
        const result = await this.sendRequest('lv00/search', {
            pattern: pattern,
            limit: 10
        });

        return result.results.map(r => ({
            name: r.name,
            statement: r.statement,
            matchScore: r.score
        }));
    }

    /**
     * 创建几何对象
     */
    async createGeometricObject(type, coords, constraints = []) {
        const result = await this.sendRequest('lv00/createObject', {
            type: type,           // "Point", "LineSegment", "Circle", etc.
            coordinates: coords,
            constraints: constraints
        });

        return {
            objectId: result.objectId,
            typeRegion: result.typeRegion,
            warnings: result.warnings || []
        };
    }

    /**
     * 监听引擎状态变化通知（对应 jsCoq 的 notification 机制）
     */
    onStateChanged(callback) {
        // 通过 Web Worker 的消息通道接收通知
        this.engine.addNotificationListener('lv00/stateChanged', callback);
    }

    onDiagnostics(callback) {
        this.engine.addNotificationListener('lv00/diagnostics', callback);
    }
}

// --- 使用示例 ---

async function exampleLv00Session() {
    // 初始化 WASM 引擎
    const wasmModule = await Lv00Engine();
    const client = new Lv00WebClient(wasmModule);

    // 监听诊断通知
    client.onDiagnostics((diag) => {
        console.log('约束违反:', diag.violations);
        // 在 UI 中高亮显示违反约束的几何元素
        highlightViolations(diag.violations);
    });

    // 创建三角形
    const A = await client.createGeometricObject('Point', { x: 0, y: 0 });
    const B = await client.createGeometricObject('Point', { x: 4, y: 0 });
    const C = await client.createGeometricObject('Point', { x: 0, y: 3 });

    // 查看证明状态
    const state = await client.getProofState();
    console.log('当前目标:', state.goals);

    // 搜索关于中线的定理
    const theorems = await client.searchApplicable('midline');
    console.log('相关定理:', theorems);

    // 执行"构造中点"策略
    const result = await client.executeTactic('construct_midpoint', {
        segment: [B.objectId, C.objectId]
    });
    console.log('策略执行结果:', result);
}
```

---

## 5. Addon 包管理生态

### 5.1 jsCoq 的包管理架构

jsCoq 通过 addon 机制支持按需加载 Coq 包（coq-hott、mathcomp、CoqHammer 等），其架构如下：

```
用户浏览器
  │
  ├─ [IndexedDB 本地持久化]
  │   ├─ 已安装的 addon 缓存
  │   ├─ 用户文档缓存
  │   └─ 证明状态快照
  │
  ├─ [jsCoq Addon Manager]
  │   ├─ addon 元数据注册表（manifest）
  │   ├─ 依赖解析（类似 opam）
  │   ├─ 版本管理（语义化版本）
  │   └─ 按需下载（lazy loading）
  │
  └─ [CDN / 远程仓库]
      ├─ addon 包文件（.tar.gz / .zip）
      ├─ 元数据文件（manifest.json）
      └─ 签名文件（完整性验证）
```

### 5.2 映射到 Lv-00 公理包生态系统

| jsCoq 概念 | Lv-00 公理包映射 | 说明 |
|:---|:---|:---|
| addon 包 | 公理包 `.lvp` 文件 | 包含公理、定理、定义 |
| manifest（元数据注册表） | 公理包索引 `packages.json` | 远程仓库的包列表 |
| 依赖解析 | `lv00_package_resolve()` | 拓扑排序依赖图 |
| IndexedDB 缓存 | IDBFS 公理包缓存 | 已安装包的本地持久化 |
| 按需下载（lazy loading） | 公理包延迟加载 | 仅在 import 时下载 |
| 版本管理 | 语义化版本（semver） | 公理包版本约束 |
| 完整性验证 | SHA-256 校验 | 包完整性校验 |

### 5.3 公理包管理器实现

```c
/**
 * @brief 公理包管理器——借鉴 jsCoq Addon Manager
 *
 * 提供浏览器内的公理包安装、缓存、版本管理和按需加载。
 */
typedef struct {
    char *cache_dir;               /**< 缓存目录（IDBFS 虚拟路径） */
    char *registry_url;            /**< 远程公理包注册表地址 */
    PackageIndex *index;           /**< 本地包索引 */
    PackageIndex *remote_index;    /**< 远程包索引（延迟加载） */
} PackageManager;

/**
 * @brief 公理包元数据
 */
typedef struct {
    char *name;                    /**< 包名 */
    char *version;                 /**< 版本号（semver） */
    char *description;             /**< 包描述 */
    char **authors;                /**< 作者列表 */
    int author_count;              /**< 作者数量 */
    char **depends;                /**< 依赖包（name@version） */
    int depend_count;              /**< 依赖数量 */
    char *sha256;                  /**< SHA-256 校验和 */
    int64_t size_bytes;            /**< 包大小 */
    char *download_url;            /**< 下载地址 */
    bool is_installed;             /**< 是否已安装 */
    bool is_cached;                /**< 是否在本地缓存中 */
} PackageMeta;

/**
 * @brief 安装公理包
 *
 * 借鉴 jsCoq 的 addon 安装流程：
 *  1. 解析依赖
 *  2. 检查本地缓存
 *  3. 下载缺失的包
 *  4. 验证完整性
 *  5. 解压到本地缓存
 *  6. 更新包索引
 */
typedef enum {
    INSTALL_OK,
    INSTALL_ALREADY_INSTALLED,
    INSTALL_DEPENDENCY_MISSING,
    INSTALL_DOWNLOAD_FAILED,
    INSTALL_CHECKSUM_MISMATCH,
    INSTALL_UNPACK_FAILED,
    INSTALL_CACHE_ERROR
} PackageInstallResult;

PackageInstallResult pm_install_package(
    PackageManager *pm,
    const char *package_name,
    const char *version_constraint
);

/**
 * @brief 列出可用包
 */
PackageMeta **pm_list_available(PackageManager *pm, int *out_count);

/**
 * @brief 搜索包
 */
PackageMeta **pm_search(PackageManager *pm, const char *query, int *out_count);

/**
 * @brief 同步远程包索引
 */
bool pm_sync_index(PackageManager *pm);
```

### 5.4 IndexedDB 持久化存储

```c
/**
 * @brief IndexedDB 持久化管理——借鉴 jsCoq 的 IndexedDB 缓存
 *
 * Emscripten 提供 IDBFS（IndexedDB-backed File System），
 * 使得 WASM 中的虚拟文件系统可以持久化到浏览器 IndexedDB。
 *
 * 持久化内容：
 *  - 已安装的公理包文件
 *  - 用户创建的几何构造文档
 *  - 证明状态快照
 *  - 用户偏好设置
 */

/**
 * @brief 初始化持久化文件系统
 *
 * 借鉴 jsCoq 的 IDBFS 使用模式：
 *  1. 挂载 IDBFS 到虚拟路径 /persist
 *  2. 从 IndexedDB 同步数据到 MEMFS
 *  3. 定期将 MEMFS 变更同步回 IndexedDB
 */
bool lv00_fs_init_persistent(void);

/**
 * @brief 同步内存变更到持久化存储
 *
 * 借鉴 jsCoq 的保存策略：定期自动保存 + 手动保存。
 * 对应用户的 Ctrl+S 或定时自动保存。
 */
bool lv00_fs_sync_to_persistent(void);

/**
 * @brief 创建用户文档的快照
 */
typedef struct {
    char *document_id;
    char *document_name;
    char *document_json;          /**< JSON 序列化的构造图 */
    int64_t timestamp;
    char *thumbnail_svg;          /**< SVG 缩略图 */
} DocumentSnapshot;

bool lv00_fs_save_snapshot(const DocumentSnapshot *snap);
DocumentSnapshot *lv00_fs_load_snapshot(const char *document_id);
DocumentSnapshot **lv00_fs_list_snapshots(int *out_count);
```

---

## 6. 对照表：jsCoq 通信原语 → Lv-00 Web GUI API

| jsCoq 组件 | jsCoq 功能 | Lv-00 Web GUI 对应 | 实现文件 |
|:---|:---|:---|:---|
| `CoqManager` | 证明会话管理 | `Lv00Session` | `web/session.js` |
| `CoqWorker` | WASM Worker 线程 | `Lv00Worker` | `web/worker.js` |
| `CoqLSP` | LSP 协议实现 | `Lv00Protocol` | `web/protocol.js` |
| `CoqPanel` | 证明面板 UI | `ProofPanel` | `web/panels/proof.js` |
| `GoalPanel` | 目标展示 | `ConstraintPanel` | `web/panels/constraint.js` |
| `AddonManager` | 包管理 | `PackageManager` | `web/packages.js` |
| `FileSystem` | 文件系统 | `Lv00FS` | `web/filesystem.js` |
| `ContextPanel` | 上下文展示 | `GeometryContext` | `web/panels/context.js` |
| `SearchView` | 搜索界面 | `TheoremSearch` | `web/panels/search.js` |
| `Editor` | 编辑器集成 | `GeometryCanvas` | `web/canvas.js` |

---

## 7. 架构总览：Lv-00 Web 部署

### 7.1 总体架构

```
浏览器
│
├─ [Web GUI 层]
│   ├─ GeometryCanvas      几何画布（交互式几何构造）
│   ├─ ProofPanel          证明面板（证明状态展示）
│   ├─ ConstraintPanel     约束面板（约束管理）
│   ├─ TheoremSearch       定理搜索
│   └─ PackageManager UI   包管理界面
│       │
│       │ Lv00Protocol（JSON-RPC over postMessage）
│       │
├─ [Web Worker 线程]
│   │  ↑ 借鉴 jsCoq 的 CoqWorker 模式
│   │  ↑ CPU 密集型任务在 Worker 中运行，不阻塞 UI
│   │
│   └─ [Lv-00 WASM 引擎]
│       ├─ type_system    类型系统引擎
│       ├─ solver         约束求解器
│       ├─ proof          证明导航器
│       ├─ PackageManager 公理包管理器
│       └─ IncrementalConstraintSolver 增量求解器
│
├─ [IndexedDB 持久化]
│   ├─ 公理包缓存
│   ├─ 用户文档
│   └─ 证明状态快照
│
└─ [CDN / 远程服务]
    ├─ 公理包注册表
    └─ 公理包下载
```

### 7.2 Worker 线程通信

```c
/**
 * @brief Worker 线程通信——借鉴 jsCoq 的 CoqWorker 模式
 *
 * Lv-00 的 WASM 引擎在 Web Worker 中运行，
 * 通过 postMessage 与主线程（UI 线程）通信。
 *
 * 主线程（UI）           Worker 线程（WASM 引擎）
 *     │                        │
 *     │── postMessage ────────→│  lv00/getGoals
 *     │                        │  ↓
 *     │                        │  proof_get_state()
 *     │                        │  ↓
 *     │←── postMessage ────────│  返回目标列表
 *     │                        │
 *     │── postMessage ────────→│  lv00/executeTactic
 *     │                        │  ↓
 *     │   (UI 显示进度条)       │  约束求解（可能耗时）
 *     │←── progress notify ────│  进度 50%
 *     │←── progress notify ────│  进度 90%
 *     │←── result ────────────│  策略执行结果
 *     │                        │
 *     │   (UI 更新画布)         │
 *     │                        │
 */

/**
 * @brief 创建 WASM Worker
 */
Lv00Worker *lv00_worker_create(const char *wasm_url);

/**
 * @brief 向 Worker 发送请求
 */
void lv00_worker_post_request(Lv00Worker *w, const Lv00Message *msg);

/**
 * @brief 注册响应回调
 */
void lv00_worker_on_response(Lv00Worker *w,
    void (*callback)(const Lv00Response *resp, void *user_data),
    void *user_data);

/**
 * @brief 注册通知回调
 */
void lv00_worker_on_notification(Lv00Worker *w,
    void (*callback)(const Lv00Message *notif, void *user_data),
    void *user_data);
```

---

## 8. 实现路线图

### 8.1 第一阶段：Emscripten 基础移植（P2-1）

- [ ] 配置 Emscripten 编译环境
- [ ] 为 Lv-00 核心库编写 `Makefile.em` / `CMakeLists.txt` Emscripten 构建配置
- [ ] 标记需要导出的 C 函数（`EMSCRIPTEN_KEEPALIVE`）
- [ ] 处理文件 I/O → Emscripten 虚拟文件系统适配
- [ ] 处理内存分配——启用 `ALLOW_MEMORY_GROWTH`
- [ ] 编写 WASM 基本功能验证测试
  - [ ] 创建类型系统 → 验证
  - [ ] 创建约束图 → 验证
  - [ ] 运行简单求解 → 验证
- [ ] 性能基准测试：WASM vs 原生 C 的性能对比

### 8.2 第二阶段：增量求解器（P2-2）

- [ ] 为约束图实现拓扑排序算法
- [ ] 实现 `ics_mark_dirty()` 脏标记传播
- [ ] 实现约束验证缓存（`constraint_cache`）
- [ ] 实现 `ics_solve_incremental()` 增量求解核心
- [ ] 实现增量求解与全量求解结果的等价性验证
- [ ] 编写增量求解的基准测试
  - [ ] 100/500/1000 节点约束图的增量编辑性能
  - [ ] 与全量求解的性能对比

### 8.3 第三阶段：LSP 风格通信协议（P2-3）

- [ ] 设计 JSON-RPC 2.0 消息格式
- [ ] 实现 `Lv00ProtocolMethod` 全部方法枚举
- [ ] 实现 `lv00_protocol_parse()` / `lv00_protocol_serialize_response()`
- [ ] 实现 `lv00_protocol_dispatch()` 消息路由
- [ ] 实现各方法对应的处理函数
- [ ] 实现 Web Worker 通信基础设施
  - [ ] Worker 创建与管理
  - [ ] postMessage 通道
  - [ ] 进度通知机制
- [ ] 编写协议集成测试

### 8.4 第四阶段：包管理与持久化（P2-4）

- [ ] 设计公理包远程注册表格式（`packages.json`）
- [ ] 实现 `PackageManager` 核心逻辑
- [ ] 实现 `pm_sync_index()` 远程索引同步
- [ ] 实现 `pm_install_package()` 包安装
- [ ] 实现 IDBFS 持久化文件系统
- [ ] 实现文档快照的保存与恢复
- [ ] 实现自动保存和恢复提醒
- [ ] 编写包管理和持久化的集成测试

### 8.5 第五阶段：Web GUI 集成（P2-5）

- [ ] 实现 `Lv00WebClient` JavaScript 客户端 SDK
- [ ] 实现 `GeometryCanvas` 几何画布组件
- [ ] 实现 `ProofPanel` 证明面板组件
- [ ] 实现 `ConstraintPanel` 约束面板组件
- [ ] 实现 `TheoremSearch` 定理搜索组件
- [ ] 实现 Web GUI 端到端测试

---

## 9. 设计决策与权衡

### 9.1 WASM 移植 vs 原生 JS 重写

Lv-00 选择 C → Emscripten → WASM 的移植路径（而非 JavaScript 原生重写），基于以下考量：

- **代码复用**：Lv-00 的 C 引擎已在原生环境下开发和测试，WASM 移植保持单一代码库
- **性能保障**：数值计算密集型操作（坐标消解、Groebner 基、矩阵运算）在 WASM 中的性能接近原生（通常 80-95%），远优于纯 JavaScript
- **维护成本**：单一 C 代码库 vs C + JS 双代码库，长期维护成本显著更低
- **jsCoq 先例**：jsCoq 已经证明了"编译器移植重型数学工具到浏览器"路线的可行性

成本：
- Emscripten 工具链的构建配置和维护
- WASM 与 JavaScript 之间的序列化/反序列化开销
- 文件 I/O 需要适配虚拟文件系统

### 9.2 增量 vs 全量求解的触发策略

增量求解仅在特定条件下有效：

- **适用增量**：单节点/单约束编辑、局部修改、少量几何对象增删
- **回退全量**：批量导入、公理包切换、几何空间变换（如从欧几里得到双曲几何）
- **混合策略**：系统自动判断编辑影响的节点比例，超过阈值（如 30%）则触发全量

```c
#define INCREMENTAL_SOLVE_THRESHOLD 0.30  /**< 超过 30% 节点受影响时回退全量求解 */

SolverStatus ics_solve_adaptive(
    IncrementalConstraintSolver *solver,
    int total_nodes
) {
    double affected_ratio = (double)solver->dirty_count / total_nodes;
    if (affected_ratio > INCREMENTAL_SOLVE_THRESHOLD) {
        // 回退全量求解
        return solver_solve_full(solver->graph);
    } else {
        // 增量求解
        return ics_solve_incremental(solver, NULL, NULL, NULL, NULL);
    }
}
```

### 9.3 Web Worker 线程模型

借鉴 jsCoq 的单 Worker 模型而非多 Worker 模型：

- **单 Worker**（jsCoq 模式）：简单、无并发问题、内存共享单一 WASM 实例。适合证明助手（交互式而非批处理）。
- **多 Worker**：复杂、需处理协调、WASM 实例间无法直接共享内存。仅在"并行搜索策略"（类似 Isabelle Sledgehammer）场景下有价值。

Lv-00 初期采用单 Worker 模型，保留将来引入"策略搜索 Worker 池"的扩展空间。

### 9.4 公理包的远程托管策略

借鉴 jsCoq 的 CDN 托管模型：

- **核心公理包**（`euclidean_geometry_core.lvp`）：随 WASM 一同部署，无需额外下载
- **社区公理包**：托管在 CDN 上，通过公理包注册表索引，按需下载
- **私有公理包**：用户可通过 URL 或本地文件导入

公理包注册表格式（借鉴 npm 的 registry 模型）：

```json
{
  "name": "lv00-packages",
  "updated": "2026-05-24T00:00:00Z",
  "packages": {
    "euclidean_geometry_advanced": {
      "versions": {
        "1.0.0": {
          "name": "euclidean_geometry_advanced",
          "version": "1.0.0",
          "description": "欧几里得几何高级定理包",
          "depends": { "euclidean_geometry_core": ">=1.0.0" },
          "sha256": "abc123...",
          "size": 245760,
          "url": "https://cdn.lv00.org/packages/ega-1.0.0.lvp"
        }
      }
    }
  }
}
```

---

## 10. 特别强调：jsCoq 的里程碑意义

jsCoq 对 Lv-00 最重要的启示不是某个具体的技术细节，而是**验证了"重型数学工具在浏览器中运行的可行性"这一关键假设**。具体而言：

1. **Coq 的复杂性远高于 Lv-00**：Coq 包含完整的依赖类型检查器、策略引擎（Ltac/Ltac2）、标记引擎（Notation）、宇宙层级检查、模块系统——这些组件的计算复杂性不低于 Lv-00 的约束求解 + 类型检查。如果 Coq 可以在浏览器中运行，Lv-00 没有理由不行。

2. **用户体验已得到验证**：jsCoq 已经在教学场景（如 Coq 交互式教程、Software Foundations 在线版）中证明了浏览器内证明助手的可用性。用户无需安装，打开网页即可开始证明。

3. **增量交互性能达标**：Fleche 增量检查器证明了"逐字符编辑 → 即时反馈"的交互模式在证明助手中是可行的——反应时间通常在 100ms 以内。

4. **包管理生态的浏览器可行性**：jsCoq 的 addon 系统证明了浏览器环境也可以支持与传统包管理（opam）类似的包安装、版本管理和按需加载机制。

5. **Lv-00 天然优势**：相比 Coq，Lv-00 的 C 语言实现通过 Emscripten 编译为 WASM 的路径更直接（无需 js_of_ocaml 中间层），且 Lv-00 专注于几何领域（而非通用数学），计算复杂度更可控。

---

## 11. 参考资源

- jsCoq 项目主页：https://github.com/jscoq/jscoq
- jsCoq 在线试用：https://coq.vercel.app/
- js_of_ocaml 项目：https://github.com/ocsigen/js_of_ocaml
- js_of_ocaml WASM 后端文档：https://ocsigen.org/js_of_ocaml/latest/manual/wasm
- Fleche 增量文档检查器：https://github.com/jscoq/jscoq/tree/main/fleche
- 《jsCoq: Towards Hybrid Theorem Proving Interfaces》（2020）—— jsCoq 设计论文
- Emscripten 官方文档：https://emscripten.org/docs/
- Emscripten IDBFS 文档：https://emscripten.org/docs/api_reference/Filesystem-API.html
- Language Server Protocol（LSP）规范：https://microsoft.github.io/language-server-protocol/
- Web Worker API：https://developer.mozilla.org/en-US/docs/Web/API/Web_Workers_API
- IndexedDB API：https://developer.mozilla.org/en-US/docs/Web/API/IndexedDB_API
- 《WASM in the Browser: Lessons from jsCoq》（Shachar Itzhaky, 2023）
- Lv-00 相关文档：
  - `solver.h` —— 约束求解器引擎
  - `proof.h` —— 证明导航器与证明状态
  - `type_system.h` —— 类型系统引擎
  - `mizar_declarative_proof_style.md` —— Mizar 声明式证明输出参考
  - `fstar_refinement_smt.md` —— F* 精化类型与 SMT 双引擎参考
