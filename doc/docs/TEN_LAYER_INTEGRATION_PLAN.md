# Lv-00 十层架构整合方案

> **版本**: 1.0.0  
> **日期**: 2026-06-05  
> **状态**: 设计方案  

---

## 1. 任务重述

### 1.1 目标
将 Lv-00 项目中所有未纳入前5层框架的代码（UI、Python绑定、LLM辅助、监控、形式化验证等）整合进完整的**十层单向依赖架构**中。

### 1.2 约束
- **前5层固定不可变**：第1-5层及Shared层的定义和目录结构保持现状
- **后5层可调整**：第6-10层的具体划分可根据模块特性灵活设计
- **单向依赖**：严格保持下层不依赖上层的原则
- **最小改动**：优先通过目录重组和文档规范实现整合，避免大规模代码重构

### 1.3 成功标准
- 所有代码模块都有明确的层级归属
- 层间依赖关系清晰、无循环依赖
- 架构文档完整描述十层定义和依赖规则

---

## 2. 现有前5层架构（固定）

| 层级 | 名称 | 目录 | 职责 |
|------|------|------|------|
| Shared | 公共基础层 | `core/src/shared/` | 错误码、基础类型、内存管理、日志、诊断 |
| 第1层 | 词法语法解析层 | `core/src/layer1_parser/` | BNF文法、词法分析、AST/Typed IR生成 |
| 第2层 | 基础几何公理层 | `core/src/layer2_axiom/` | 几何实体、本体、度量关系、公理库 |
| 第3层 | 约束拓扑规约层 | `core/src/layer3_constraint/` | 约束图、拓扑归一化、相容检测 |
| 第4层 | 多策略自动推理层 | `core/src/layer4_reasoning/` | 正向/反向/反证推理、代数消元、SMT/ATP调度 |
| 第5层 | 输出证明编译层 | `core/src/layer5_output/` | 证明格式化、跨语言导出、可视化转换 |

---

## 3. 后5层架构设计

### 3.1 设计原则

1. **用户面向性**：第6层起面向最终用户和开发者，离核心推理越来越远
2. **语言无关性**：第7层处理多语言绑定，是核心与外部世界的桥梁
3. **工具独立性**：第8-10层是独立工具，不直接影响核心推理正确性
4. **依赖单向性**：第6层可依赖第1-5层，第7层可依赖第1-5层，第8层可依赖第7层，以此类推

### 3.2 后5层定义

| 层级 | 名称 | 核心职责 | 依赖关系 |
|------|------|----------|----------|
| **第6层** | **交互与可视化层** | 几何画布、交互编辑、多视图同步、Web前端、Penrose渲染 | 可依赖第1-5层（只读） |
| **第7层** | **多语言绑定与桥接层** | Python/JS/WASM绑定、流式桥接、API封装、异步迭代器 | 可依赖第1-5层（绑定） |
| **第8层** | **智能辅助与LLM层** | AI编程辅助、代码生成、知识库、提示词引擎 | 可依赖第7层 |
| **第9层** | **运行时监控与运维层** | 并发监控、Web仪表盘、性能采集、日志分析 | 可依赖第7层 |
| **第10层** | **形式化验证与生态层** | Lean/Coq形式化、文档生成、示例库、插件系统 | 独立于运行时 |

---

## 4. 未纳入模块的层级归属

### 4.1 第6层：交互与可视化层

| 当前位置 | 模块 | 功能 | 归属理由 |
|----------|------|------|----------|
| `module/python/lv/interactive_geo.py` | 交互几何 | 画布状态管理、交互模式、拖拽约束维护 | 面向用户的交互功能 |
| `module/python/lv/high_dim.py` | 高维可视化 | 高维结构投影、多投影视图管理 | 可视化功能 |
| `web/gui/src/` | React前端 | Web GUI完整实现 | 用户交互界面 |
| `web/gui/src/components/canvas/` | 几何画布 | Canvas渲染、工具栏 | 核心可视化组件 |
| `web/gui/src/components/panels/` | 各种面板 | 约束图、证明、公式、调试面板 | 交互式信息展示 |
| `web/gui/src/stores/` | 状态管理 | 几何状态、UI状态 | 交互层状态管理 |
| `web/gui/src/utils/penroseRenderer.ts` | Penrose渲染 | 几何图形渲染 | 可视化渲染工具 |

### 4.2 第7层：多语言绑定与桥接层

| 当前位置 | 模块 | 功能 | 归属理由 |
|----------|------|------|----------|
| `module/python/lv/_ctypes_binding.py` | C库绑定 | ctypes FFI绑定、结构体定义 | 基础语言绑定 |
| `module/python/lv/core.py` | Python核心封装 | C核心类的Python OO封装 | 语言绑定封装 |
| `module/python/lv/engine.py` | 引擎接口 | 引擎高级Python API | 推理层绑定 |
| `module/python/lv/formula.py` | 公式模块 | 公式解析、渲染、转换 | 解析层绑定扩展 |
| `module/python/lv/dsl*.py` | DSL模块 | PyEuclid风格链式语言 | 语言绑定层DSL |
| `module/python/lv/groebner_engine.py` | Groebner接口 | 多项式环、Gröbner基计算 | 推理层绑定 |
| `module/python/lv/type_system.py` | 类型系统接口 | 类型创建、依赖类型检查 | 推理层绑定 |
| `module/python/lv/sparse_la.py` | 稀疏线性代数 | 稀疏矩阵、约束传播 | 约束/推理层绑定 |
| `module/python/lv/proof_extras.py` | 扩展证明 | 证明树可视化、多策略引擎 | 证明层绑定 |
| `module/python/lv/stream_bridge.py` | 流式桥接 | C引擎与Web前端桥接 | 跨运行时绑定 |
| `module/python/lv/ws_server.py` | WebSocket服务器 | 实时事件推送 | 流式通信绑定 |
| `module/python/lv/async_stream.py` | 异步流式 | 异步迭代器接口 | 异步I/O绑定 |
| `module/python/lv/py_euclid_style.py` | PyEuclid API | 高层Pythonic API | 语言绑定高级封装 |
| `module/python/lv/preset*.py` | 预设函数块 | 常用几何构造预设 | 语言绑定预设库 |
| `module/python/lv/constraints.py` | 约束封装 | 约束类型Python接口 | 约束层绑定 |
| `module/python/lv/normalization.py` | 归一化接口 | 归一化结果处理 | 约束层绑定 |
| `module/python/lv/func_block.py` | 函数块接口 | 函数块系统Python接口 | 推理层绑定 |
| `module/python/lv/math_presets.py` | 数学预设 | 数学预设函数和常量 | 语言绑定预设 |
| `web/gui/src/engine/` | 后端抽象 | WASM/JS后端切换 | JS/TS语言绑定 |
| `web/gui/src/services/stream*.ts` | 流式客户端 | 引擎流式事件消费 | 前端流式绑定 |

### 4.3 第8层：智能辅助与LLM层

| 当前位置 | 模块 | 功能 | 归属理由 |
|----------|------|------|----------|
| `module/llm_coding_assistant/` | LLM辅助完整系统 | 编程辅助、代码生成、AI对话 | AI辅助工具 |
| `module/llm_coding_assistant/main.py` | 主程序 | 交互式命令行入口 | LLM辅助入口 |
| `module/llm_coding_assistant/lv_knowledge.py` | 领域知识库 | C API文档、代码模式、提示词引擎 | AI知识库 |
| `module/llm_coding_assistant/api_server.py` | API服务器 | FastAPI RESTful/WebSocket接口 | AI服务暴露 |
| `module/llm_coding_assistant/core/` | AI核心 | AI引擎、代码分析器 | AI核心组件 |
| `module/llm_coding_assistant/templates.py` | 代码模板 | WASM绑定、Canvas渲染模板 | AI模板库 |
| `web/gui/src/services/apiClient.ts` | AI API客户端 | 多提供者AI客户端 | 前端AI通信 |

### 4.4 第9层：运行时监控与运维层

| 当前位置 | 模块 | 功能 | 归属理由 |
|----------|------|------|----------|
| `module/concurrent_monitor/` | 并发监控完整系统 | 进程监控、Web仪表盘 | 运维监控工具 |
| `module/concurrent_monitor/core/` | 监控核心 | 引擎、事件、模型、配置 | 监控核心组件 |
| `module/concurrent_monitor/web/` | Web仪表盘 | Flask仪表盘、路由、模板 | 运维可视化 |
| `module/concurrent_monitor/cli/` | CLI工具 | 命令行监控入口 | 运维CLI |
| `module/concurrent_monitor/tests/` | 监控测试 | 单元测试、集成测试 | 运维层测试 |

### 4.5 第10层：形式化验证与生态层

| 当前位置 | 模块 | 功能 | 归属理由 |
|----------|------|------|----------|
| `formal/` | Lean形式化 | 基础几何定义、公理 | 形式化验证 |
| `lv-formal/` | Lean核心形式化 | 核心类型、等价性验证 | 形式化验证 |
| `examples/` | 示例库 | 几何示例、模板、插件示例 | 生态系统 |
| `doc/generate_version_doc.js` | 文档生成器 | 版本文档自动生成 | 生态工具 |
| `doc/docs/` | 技术文档 | 架构手册、API参考、教程 | 生态文档 |
| `tool/scripts/` | 工具脚本 | 汇报文档生成等 | 生态工具 |
| `module/axiom_packages/` | 公理包库 | 领域公理包 | 生态扩展 |

---

## 5. 目标目录结构

```
Lv-00/
├── core/
│   ├── include/lv/
│   │   ├── shared/              # Shared 公共基础层
│   │   ├── layer1_parser/       # 第1层：词法语法解析
│   │   ├── layer2_axiom/        # 第2层：基础几何公理
│   │   ├── layer3_constraint/   # 第3层：约束拓扑规约
│   │   ├── layer4_reasoning/    # 第4层：多策略自动推理
│   │   └── layer5_output/       # 第5层：输出证明编译
│   └── src/
│       ├── shared/
│       ├── layer1_parser/
│       ├── layer2_axiom/
│       ├── layer3_constraint/
│       ├── layer4_reasoning/
│       └── layer5_output/
│
├── layer6_interactive/          # 第6层：交互与可视化
│   ├── web_gui/                 # React前端
│   ├── python_interactive/      # Python交互几何
│   └── penrose_renderer/        # Penrose渲染器
│
├── layer7_binding/              # 第7层：多语言绑定与桥接
│   ├── python/                  # Python绑定包
│   ├── js_ts/                   # JavaScript/TypeScript绑定
│   ├── wasm/                    # WASM绑定
│   └── stream_bridge/           # 流式桥接
│
├── layer8_ai_assistant/         # 第8层：智能辅助与LLM
│   ├── core/                    # AI引擎、代码分析
│   ├── api_server/              # FastAPI服务
│   ├── knowledge_base/          # 领域知识库
│   └── templates/               # 代码模板
│
├── layer9_monitoring/           # 第9层：运行时监控与运维
│   ├── core/                    # 监控引擎
│   ├── web_dashboard/           # Web仪表盘
│   ├── cli/                     # 命令行工具
│   └── tests/                   # 监控测试
│
├── layer10_ecosystem/           # 第10层：形式化验证与生态
│   ├── formal_verification/     # Lean形式化
│   ├── examples/                # 示例库
│   ├── axiom_packages/          # 公理包库
│   ├── docs/                    # 技术文档
│   └── tools/                   # 生态工具
│
├── tests/                       # 核心测试套件
├── cmake/                       # CMake配置
└── .github/                     # CI/CD配置
```

---

## 6. 层间依赖规则

### 6.1 依赖矩阵

| 层级 | 可依赖的层级 | 禁止依赖的层级 |
|------|-------------|---------------|
| Shared | 无 | 所有业务层 |
| 第1层 | Shared | 第2-10层 |
| 第2层 | Shared, 第1层(只读类型) | 第3-10层 |
| 第3层 | Shared, 第1-2层 | 第4-10层 |
| 第4层 | Shared, 第1-3层 | 第5-10层 |
| 第5层 | Shared, 第1-4层(只读) | 第6-10层 |
| 第6层 | Shared, 第1-5层(只读) | 第7-10层 |
| 第7层 | Shared, 第1-5层(绑定) | 第8-10层 |
| 第8层 | 第7层(通过绑定调用) | 第1-6层, 第9-10层 |
| 第9层 | 第7层(监控绑定进程) | 第1-8层, 第10层 |
| 第10层 | 无(独立运行) | 所有运行时层 |

### 6.2 关键规则

1. **第6层（交互层）只读原则**：只能读取第1-5层的状态，禁止修改核心推理状态
2. **第7层（绑定层）封装原则**：所有跨语言调用必须通过绑定层，禁止上层直接调用C API
3. **第8层（AI层）间接原则**：AI辅助必须通过第7层调用核心，禁止直接访问C库
4. **第9层（监控层）外部原则**：监控系统作为外部工具运行，不嵌入核心进程
5. **第10层（生态层）独立原则**：形式化验证和文档工具完全独立，与运行时无依赖

---

## 7. 迁移实施计划

### 7.1 阶段1：目录结构创建（Week 1）

```bash
# 创建第6-10层目录
mkdir -p layer6_interactive/{web_gui,python_interactive,penrose_renderer}
mkdir -p layer7_binding/{python,js_ts,wasm,stream_bridge}
mkdir -p layer8_ai_assistant/{core,api_server,knowledge_base,templates}
mkdir -p layer9_monitoring/{core,web_dashboard,cli,tests}
mkdir -p layer10_ecosystem/{formal_verification,examples,axiom_packages,docs,tools}
```

### 7.2 阶段2：第7层迁移（Week 2）

**目标**：将 `module/python/lv/` 中的绑定代码迁移到 `layer7_binding/python/`

**步骤**：
1. 移动基础绑定文件（`_ctypes_binding.py`, `core.py`, `engine.py`）
2. 移动DSL和公式模块（`dsl*.py`, `formula.py`）
3. 移动推理绑定（`groebner_engine.py`, `type_system.py`, `sparse_la.py`, `proof_extras.py`）
4. 移动流式通信（`stream_bridge.py`, `ws_server.py`, `async_stream.py`）
5. 移动预设和工具（`preset*.py`, `constraints.py`, `normalization.py`, `func_block.py`）

**例外**：
- `interactive_geo.py` → `layer6_interactive/python_interactive/`
- `high_dim.py` → `layer6_interactive/python_interactive/`

### 7.3 阶段3：第6层迁移（Week 3）

**目标**：整合所有交互和可视化代码

**步骤**：
1. 移动 `web/gui/src/` → `layer6_interactive/web_gui/`
2. 移动 `module/python/lv/interactive_geo.py` → `layer6_interactive/python_interactive/`
3. 移动 `module/python/lv/high_dim.py` → `layer6_interactive/python_interactive/`
4. 整合Penrose渲染器到 `layer6_interactive/penrose_renderer/`

### 7.4 阶段4：第8-9层迁移（Week 4）

**目标**：迁移LLM辅助和监控系统

**步骤**：
1. 移动 `module/llm_coding_assistant/` → `layer8_ai_assistant/`
2. 移动 `module/concurrent_monitor/` → `layer9_monitoring/`
3. 更新内部引用路径

### 7.5 阶段5：第10层迁移（Week 5）

**目标**：整合形式化验证和生态工具

**步骤**：
1. 移动 `formal/` → `layer10_ecosystem/formal_verification/`
2. 移动 `lv-formal/` → `layer10_ecosystem/formal_verification/`
3. 移动 `examples/` → `layer10_ecosystem/examples/`
4. 移动 `module/axiom_packages/` → `layer10_ecosystem/axiom_packages/`
5. 移动 `doc/` → `layer10_ecosystem/docs/`
6. 移动 `tool/` → `layer10_ecosystem/tools/`

### 7.6 阶段6：验证与文档（Week 6）

1. **编译验证**：确保所有模块在新位置编译通过
2. **依赖检查**：运行静态分析，确认无跨层违规依赖
3. **测试验证**：运行所有现有测试用例
4. **文档更新**：更新 `ARCHITECTURE_MANUAL.md` 描述完整十层架构

---

## 8. 风险与缓解

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 头文件/导入路径变更 | 编译错误 | 使用相对路径，逐步迁移；提供兼容性shim |
| Python包路径变更 | 导入失败 | 更新 `__init__.py` 和 `setup.py` 路径 |
| Web前端构建路径变更 | 构建失败 | 更新 `vite.config.ts` / `webpack.config.js` |
| 循环依赖 | 链接/运行失败 | 严格遵循单向依赖矩阵 |
| 测试覆盖不足 | 回归问题 | 迁移前增加测试用例；迁移后全量测试 |
| CI/CD配置过时 | 构建失败 | 同步更新 GitHub Actions 配置 |

---

## 9. 验收标准

1. **编译通过**：所有目标平台编译无错误
2. **测试通过**：所有现有测试用例通过
3. **依赖合规**：静态检查确认无跨层违规依赖
4. **文档完整**：架构文档反映完整十层结构
5. **CI/CD正常**：持续集成流水线运行正常

---

## 10. 附录：模块归属速查表

| 模块路径 | 当前位置 | 目标层级 | 目标位置 |
|----------|----------|----------|----------|
| Python绑定包 | `module/python/lv/` | 第7层 | `layer7_binding/python/` |
| 交互几何 | `module/python/lv/interactive_geo.py` | 第6层 | `layer6_interactive/python_interactive/` |
| 高维可视化 | `module/python/lv/high_dim.py` | 第6层 | `layer6_interactive/python_interactive/` |
| Web前端 | `web/gui/src/` | 第6层 | `layer6_interactive/web_gui/` |
| LLM辅助 | `module/llm_coding_assistant/` | 第8层 | `layer8_ai_assistant/` |
| 并发监控 | `module/concurrent_monitor/` | 第9层 | `layer9_monitoring/` |
| Lean形式化 | `formal/`, `lv-formal/` | 第10层 | `layer10_ecosystem/formal_verification/` |
| 示例库 | `examples/` | 第10层 | `layer10_ecosystem/examples/` |
| 公理包 | `module/axiom_packages/` | 第10层 | `layer10_ecosystem/axiom_packages/` |
| 技术文档 | `doc/` | 第10层 | `layer10_ecosystem/docs/` |
| 工具脚本 | `tool/` | 第10层 | `layer10_ecosystem/tools/` |

---

**文档状态**: 已完成  
**下一步**: 按阶段执行迁移，从第7层Python绑定开始
