# Lv-00 Layer 6：图形化编程层设计规范

> **版本**: 1.0.0-draft  
> **日期**: 2026-05-29  
> **状态**: 设计阶段  
> **依赖**: Lv-00 v3.5.0 五层架构 + 函数块系统 + WFC范式 + 自举设计

---

## 目录

1. [设计理念：继承与扩展](#一设计理念继承与扩展)
2. [Layer 6 核心架构](#二layer-6-核心架构)
3. [多范式可视化编辑器](#三subsystem-1多范式可视化编辑器)
4. [函数块运行时扩展](#四subsystem-2函数块运行时扩展)
5. [类型系统扩展](#五subsystem-3类型系统扩展)
6. [表示转换层](#六subsystem-4表示转换层)
7. [实现路线图](#七实现路线图)
8. [与原有设计的协调](#八与原有设计的协调)
9. [成功标准](#九成功标准)

---

## 一、设计理念：继承与扩展

### 1.1 核心洞见

Layer 6 的设计基于以下关键洞见：

| 原有设计 | Layer 6 继承与扩展 |
|---------|-------------------|
| **函数块系统** | 函数块作为图形化编程的**核心抽象单元**——可视化节点 |
| **WFC范式** | 图形化编辑器使用**约束传播**实现实时布局与类型检查 |
| **自举设计** | Layer 6 的元表示层**复用自举的几何编码方案** |
| **类型系统** | 扩展宇宙层级支持**通用编程类型**（List、Map、IO等） |
| **五层架构** | Layer 6 作为**新顶层**，单向依赖 Layer 5，保持架构纯净 |

### 1.2 双重身份的实现

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Layer 6: 图形化编程层                              │
│                                                                     │
│  ┌─────────────────────┐    ┌─────────────────────┐               │
│  │   身份A：几何元语言   │    │   身份B：通用编程    │               │
│  │   (Domain-Specific) │    │   (General-Purpose) │               │
│  ├─────────────────────┤    ├─────────────────────┤               │
│  │ • 几何画布视图       │    │ • 节点图视图         │               │
│  │ • 几何构造函数块     │    │ • 通用控制流块       │               │
│  │ • 证明目标节点       │    │ • IO/系统交互块      │               │
│  │ • 约束可视化         │    │ • 数据结构块         │               │
│  │ • 符号坐标精确计算   │    │ • 文本代码视图       │               │
│  └─────────────────────┘    └─────────────────────┘               │
│           ↓                          ↓                             │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │              统一函数块运行时 (FuncBlock Runtime)             │   │
│  │  • 所有视图共享同一套函数块抽象                               │   │
│  │  • 几何块与通用块可自由组合                                   │   │
│  │  • 类型系统统一检查                                           │   │
│  └─────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
                              ↓ 单向依赖
┌─────────────────────────────────────────────────────────────────────┐
│  Layer 1-5: 现有Lv-00核心（冻结接口）                                │
│  • 函数块系统 (Layer 4)                                             │
│  • 约束传播引擎 (Layer 3)                                           │
│  • 类型系统 (Layer 4)                                               │
│  • 证明系统 (Layer 4-5)                                             │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 二、Layer 6 核心架构

### 2.1 四大子系统

```
Layer 6 内部架构：

┌─────────────────────────────────────────────────────────────────────┐
│  Subsystem 1: 多范式可视化编辑器 (VisualEditorCore)                      │
│  ├─ 几何画布引擎                                                     │
│  ├─ 节点图引擎                                                       │
│  ├─ 积木块引擎                                                       │
│  └─ 文本代码编辑器                                                   │
│  └─ 视图同步器（四种视图实时同步）                                   │
├─────────────────────────────────────────────────────────────────────┤
│  Subsystem 2: 函数块运行时扩展 (FuncBlockRuntime)                        │
│  ├─ 通用控制流块（If/While/For/Match）                              │
│  ├─ IO交互块（File/Network/UI）                                     │
│  ├─ 数据结构块（List/Map/Set/Record）                               │
│  ├─ 函数块调度器（执行图遍历）                                       │
│  └─ 增量执行引擎（热路径缓存）                                       │
├─────────────────────────────────────────────────────────────────────┤
│  Subsystem 3: 类型系统扩展 (ExtendedTypeSystem)                           │
│  ├─ 通用类型区域（List<T>、Map<K,V>等）                             │
│  ├─ IO类型系统（FileHandle、Stream等）                              │
│  ├─ 效果类型（Effect追踪IO副作用）                                   │
│  ├─ 类型推断增强（从节点图推断类型）                                 │
│  └─ 类型可视化渲染                                                  │
├─────────────────────────────────────────────────────────────────────┤
│  Subsystem 4: 表示转换层 (RepresentationConverter)                   │
│  ├─ 函数块 ↔ 节点图节点                                              │
│  ├─ 函数块 ↔ 几何画布实体                                            │
│  ├─ 函数块 ↔ 积木块                                                  │
│  ├─ 函数块 ↔ Lv-00文本代码                                           │
│  └─ 双向同步协议                                                    │
└─────────────────────────────────────────────────────────────────────┘
```

### 2.2 与五层架构的依赖关系

```
依赖规则（继承五层架构原则）：

Layer 6 允许依赖：
  ├─ Layer 5: 输出证明编译层（只读结果）
  ├─ Layer 4: 多策略自动推理层（函数块系统、类型系统、证明系统）
  ├─ Layer 3: 约束拓扑规约层（约束图、归一化）
  ├─ Layer 2: 基础几何公理层（几何实体定义）
  ├─ Layer 1: 词法语法解析层（Lv-00文本解析）
  └─ Shared: 公共基础层

Layer 6 禁止依赖：
  ├─ 禁止反向调用上层（Layer 6 不被下层调用）
  ├─ 禁止修改推理上下文（只读 Layer 4-5 结果）
  └─ 禁止绕过类型系统直接操作约束图

新增规则：
  ├─ R9: 图形化编辑器不得绕过函数块系统直接创建几何实体
  ├─ R10: 通用编程块不得破坏几何块的确定性保证
  └─ R11: 表示转换必须保持语义等价（双向可逆）
```

---

## 三、Subsystem 1：多范式可视化编辑器

### 3.1 四种视图的统一抽象

**核心设计原则**：四种视图是**同一程序的不同可视化表示**，底层共享同一套函数块图。

```c
/* ========== visual_editor.h ========== */

typedef enum {
    VIEW_GEOMETRY_CANVAS,    /* 几何画布：点、线、圆等几何实体 */
    VIEW_NODE_GRAPH,         /* 节点图：函数块作为节点，端口连接 */
    VIEW_BLOCK_CANVAS,       /* 积木块：拖拽式编程 */
    VIEW_TEXT_CODE           /* 文本代码：Lv-00 DSL 源码 */
} ViewType;

typedef struct VisualEditor {
    /* 核心程序表示 */
    FuncBlockGraph *program_graph;    /* 函数块图（四种视图共享） */
    
    /* 四种视图实例 */
    GeometryCanvasView *geo_view;
    NodeGraphView *node_view;
    BlockCanvasView *block_view;
    TextCodeView *text_view;
    
    /* 视图同步器 */
    ViewSynchronizer *sync;
    
    /* 当前活跃视图 */
    ViewType active_view;
    
    /* 编辑历史 */
    EditHistory *history;
    
    /* 类型检查器（实时） */
    TypeChecker *type_checker;
} VisualEditor;
```

### 3.2 几何画布视图

**继承现有几何能力**：几何画布直接使用 Layer 2-3 的几何实体和约束图。

```c
typedef struct GeometryCanvasView {
    /* 几何实体渲染 */
    CanvasRenderer *renderer;
    
    /* 几何实体映射 */
    HashTable *entity_to_block;    /* GeomNode ID → FuncBlock ID */
    HashTable *block_to_entity;    /* FuncBlock ID → GeomNode ID */
    
    /* 交互工具 */
    struct {
        Tool *point_tool;
        Tool *line_tool;
        Tool *circle_tool;
        Tool *constraint_tool;
        Tool *measure_tool;
    } tools;
    
    /* 约束可视化 */
    ConstraintVisualizer *constraint_vis;
    
    /* 证明目标显示 */
    ProofGoalOverlay *proof_overlay;
} GeometryCanvasView;
```

**几何实体 ↔ 函数块映射**：

| 几何实体 | 函数块类型 | 端口 |
|---------|-----------|------|
| `Point` | `PointBlock` | 输出：坐标 |
| `Line` | `LineBlock` | 输入：两点；输出：直线对象 |
| `Circle` | `CircleBlock` | 输入：中心+半径/三点；输出：圆对象 |
| `Constraint` | `ConstraintBlock` | 输入：实体；输出：约束对象 |
| `Measure` | `MeasureBlock` | 输入：实体；输出：Scalar |

### 3.3 节点图视图

**核心抽象**：函数块作为节点，端口连接作为边。

```c
typedef struct NodeGraphView {
    /* 节点渲染 */
    NodeRenderer *node_renderer;
    
    /* 节点布局（使用 WFC 约束传播） */
    NodeLayoutEngine *layout;
    
    /* 节点交互 */
    struct {
        NodeInteraction *select;
        NodeInteraction *connect;
        NodeInteraction *drag;
        NodeInteraction *fold_unfold;
    } interactions;
    
    /* 端口可视化 */
    PortVisualizer *port_vis;
    
    /* 数据流动画 */
    DataFlowAnimator *flow_anim;
} NodeGraphView;
```

**节点布局的 WFC 应用**：

借鉴 `11_wfc_paradigm.md` 的约束传播引擎：

```
节点布局问题 = WFC 约束满足问题：

节点位置状态空间：
  ├─ 每个节点有候选位置集合（网格位置）
  ├─ 约束：节点不重叠
  ├─ 约束：连接线不交叉（可选）
  ├─ 约束：相关节点靠近
  └─ 熵最小化选择 → 自动布局
```

### 3.4 积木块视图

**设计理念**：类似 Scratch/Blockly，但基于 Lv-00 函数块抽象。

```c
typedef struct BlockCanvasView {
    /* 积木块库 */
    BlockLibrary *library;
    
    /* 积木块渲染 */
    BlockRenderer *block_renderer;
    
    /* 拖拽系统 */
    DragSystem *drag;
    
    /* 拼接检测 */
    SnapDetector *snap;
    
    /* 积木块分类 */
    struct {
        BlockCategory *geometry;     /* 几何构造块 */
        BlockCategory *measure;      /* 度量计算块 */
        BlockCategory *logic;        /* 逻辑证明块 */
        BlockCategory *control;      /* 控制流块（新增） */
        BlockCategory *data;         /* 数据结构块（新增） */
        BlockCategory *io;           /* IO交互块（新增） */
    } categories;
} BlockCanvasView;
```

### 3.5 文本代码视图

**双向同步**：文本代码 ↔ 函数块图实时同步。

```c
typedef struct TextCodeView {
    /* 文本编辑器 */
    TextEditor *editor;
    
    /* Lv-00 解析器（Layer 1） */
    Lv00Parser *parser;
    
    /* 代码生成器（函数块图 → Lv-00 DSL） */
    CodeGenerator *generator;
    
    /* 语法高亮 */
    SyntaxHighlighter *highlighter;
    
    /* 错误标记 */
    ErrorMarker *error_marker;
} TextCodeView;
```

---

## 四、Subsystem 2：函数块运行时扩展

### 4.1 通用控制流块

**设计原则**：控制流块保持函数块的封装性和确定性追踪。

```c
/* ========== control_flow_blocks.h ========== */

/* 条件块 */
typedef struct IfBlock {
    FuncBlock base;
    
    /* 输入端口 */
    int condition_port;       /* Bool 类型输入 */
    
    /* 输出端口 */
    int then_output;          /* 条件为真时的输出 */
    int else_output;          /* 条件为假时的输出 */
    
    /* 内部分支 */
    struct {
        FuncBlock *then_branch;
        FuncBlock *else_branch;  /* 可选 */
    } branches;
    
    /* 确定性：条件块本身是确定的（给定输入，输出唯一） */
} IfBlock;

/* 循环块 */
typedef struct WhileBlock {
    FuncBlock base;
    
    /* 输入端口 */
    int init_port;            /* 初始状态 */
    int condition_port;       /* 循环条件 */
    
    /* 输出端口 */
    int output_port;          /* 最终状态 */
    
    /* 内部循环体 */
    FuncBlock *body;
    
    /* 循环不变式（证明支持） */
    Proposition *invariant;
    
    /* 确定性：需要证明循环终止 */
    DeterminismState determinism;  /* VERIFIED 需要终止性证明 */
} WhileBlock;

/* 匹配块（模式匹配） */
typedef struct MatchBlock {
    FuncBlock base;
    
    /* 输入端口 */
    int input_port;           /* 待匹配值 */
    
    /* 输出端口 */
    int output_port;          /* 匹配结果 */
    
    /* 模式分支 */
    struct {
        Pattern *pattern;
        FuncBlock *handler;
        int output_port;
    } *cases;
    int case_count;
    
    /* 默认分支 */
    FuncBlock *default_handler;
} MatchBlock;
```

### 4.2 IO交互块

**设计原则**：IO块使用效果类型追踪副作用，与纯几何块分离。

```c
/* ========== io_blocks.h ========== */

/* 文件块 */
typedef struct FileBlock {
    FuncBlock base;
    
    /* 效果标记 */
    EffectType effect;        /* FILE_READ / FILE_WRITE */
    
    /* 输入端口 */
    int path_port;            /* String: 文件路径 */
    int data_port;            /* 写入时的数据 */
    
    /* 输出端口 */
    int result_port;          /* FileHandle 或 String */
    int status_port;          /* IOStatus: 成功/失败 */
} FileBlock;

/* 网络块 */
typedef struct NetworkBlock {
    FuncBlock base;
    
    /* 效果标记 */
    EffectType effect;        /* NETWORK_REQUEST */
    
    /* 输入端口 */
    int url_port;
    int request_port;
    
    /* 输出端口 */
    int response_port;
    int status_port;
} NetworkBlock;

/* UI事件块 */
typedef struct UIEventBlock {
    FuncBlock base;
    
    /* 效果标记 */
    EffectType effect;        /* UI_RENDER / UI_INPUT */
    
    /* 输入端口 */
    int event_port;           /* UIEvent */
    
    /* 输出端口 */
    int action_port;          /* 用户响应 */
} UIEventBlock;
```

### 4.3 数据结构块

```c
/* ========== data_structure_blocks.h ========== */

/* 列表块 */
typedef struct ListBlock {
    FuncBlock base;
    
    /* 类型参数（多态） */
    TypeVariable *elem_type;
    
    /* 操作类型 */
    enum {
        LIST_CREATE,
        LIST_APPEND,
        LIST_GET,
        LIST_MAP,
        LIST_FILTER,
        LIST_REDUCE
    } operation;
    
    /* 输入/输出端口根据操作类型动态配置 */
} ListBlock;

/* 映射块 */
typedef struct MapBlock {
    FuncBlock base;
    
    /* 类型参数 */
    TypeVariable *key_type;
    TypeVariable *value_type;
    
    /* 操作类型 */
    enum {
        MAP_CREATE,
        MAP_INSERT,
        MAP_GET,
        MAP_REMOVE,
        MAP_KEYS,
        MAP_VALUES
    } operation;
} MapBlock;

/* 记录块（结构体） */
typedef struct RecordBlock {
    FuncBlock base;
    
    /* 字段定义 */
    struct {
        char *field_name;
        TypeRegion *field_type;
        int field_port;
    } *fields;
    int field_count;
} RecordBlock;
```

### 4.4 函数块调度器

**执行模型**：函数块图的拓扑排序执行。

```c
/* ========== block_scheduler.h ========== */

typedef struct BlockScheduler {
    /* 待执行图 */
    FuncBlockGraph *graph;
    
    /* 执行队列 */
    int *execution_queue;
    int queue_count;
    
    /* 执行状态 */
    HashTable *port_values;    /* 端口ID → 当前值 */
    
    /* 增量执行 */
    struct {
        int *dirty_blocks;     /* 需要重新执行的块 */
        int dirty_count;
        HashTable *cached_results;  /* 热路径缓存 */
    } incremental;
    
    /* 效果追踪 */
    EffectLog *effect_log;     /* IO副作用记录 */
    
    /* 执行策略 */
    enum {
        SCHED_FULL,            /* 全量执行 */
        SCHED_INCREMENTAL,     /* 增量执行 */
        SCHED_LAZY             /* 惰性执行（按需求解） */
    } strategy;
} BlockScheduler;

/* 执行流程 */
ExecutionResult block_scheduler_run(BlockScheduler *sched);
```

---

## 五、Subsystem 3：类型系统扩展

### 5.1 通用类型区域

**继承宇宙层级**：通用类型位于第 1 层，与几何类型同级。

```c
/* ========== extended_types.h ========== */

/* 列表类型 */
typedef struct ListTypeRegion {
    TypeRegion base;
    
    /* 元素类型参数 */
    TypeRegion *elem_type;
    
    /* 宇宙层级：max(elem_type.level, 1) */
} ListTypeRegion;

/* 映射类型 */
typedef struct MapTypeRegion {
    TypeRegion base;
    
    /* 键值类型参数 */
    TypeRegion *key_type;
    TypeRegion *value_type;
} MapTypeRegion;

/* 函数类型（依赖类型） */
typedef struct FunctionTypeRegion {
    TypeRegion base;
    
    /* Π(x:A).B(x) */
    DependentType dependent;
} FunctionTypeRegion;

/* 效果类型 */
typedef struct EffectTypeRegion {
    TypeRegion base;
    
    /* 效果集合 */
    EffectType *effects;
    int effect_count;
    
    /* 结果类型 */
    TypeRegion *result_type;
} EffectTypeRegion;
```

### 5.2 效果系统

**设计理念**：借鉴 Haskell/Scala 的效果追踪，IO块标记效果，纯几何块无效果。

```c
typedef enum {
    EFFECT_PURE,              /* 无副作用（几何块默认） */
    EFFECT_FILE_READ,
    EFFECT_FILE_WRITE,
    EFFECT_NETWORK,
    EFFECT_UI_RENDER,
    EFFECT_UI_INPUT,
    EFFECT_RANDOM,            /* 非确定性 */
    EFFECT_TIME               /* 时间依赖 */
} EffectType;

typedef struct EffectAnnotation {
    EffectType *effects;
    int effect_count;
    
    /* 效果组合 */
    EffectAnnotation *compose_with(EffectAnnotation *other);
} EffectAnnotation;
```

**效果检查规则**：

```
效果检查规则：

1. 纯块 + 纯块 = 纯块
2. 纯块 + IO块 = IO块（效果传播）
3. IO块 + IO块 = IO块（效果合并）
4. 几何证明块必须为纯块（效果检查强制）
5. 效果块不能作为证明的依据（信任颜色降级）
```

### 5.3 类型可视化渲染

```c
/* ========== type_visualization.h ========== */

/* 类型颜色编码 */
typedef struct TypeColorScheme {
    /* 几何类型 */
    uint32_t point_color;      /* 红色 */
    uint32_t line_color;       /* 蓝色 */
    uint32_t circle_color;     /* 绿色 */
    uint32_t region_color;     /* 紫色 */
    
    /* 通用类型 */
    uint32_t list_color;       /* 橙色 */
    uint32_t map_color;        /* 黄色 */
    uint32_t function_color;   /* 青色 */
    
    /* 效果类型 */
    uint32_t pure_color;       /* 白色 */
    uint32_t io_color;         /* 灰色（带警告标记） */
} TypeColorScheme;

/* 类型标签渲染 */
void render_type_label(TypeRegion *type, Canvas *canvas, Point position);
```

---

## 六、Subsystem 4：表示转换层

### 6.1 统一转换协议

**核心原则**：所有转换必须**双向可逆**，保持语义等价。

```c
/* ========== representation_converter.h ========== */

typedef struct RepresentationConverter {
    /* 函数块图（核心表示） */
    FuncBlockGraph *core_graph;
    
    /* 转换器注册表 */
    struct {
        Converter *to_geometry;    /* 函数块 → 几何实体 */
        Converter *to_node;        /* 函数块 → 节点图节点 */
        Converter *to_block;       /* 函数块 → 积木块 */
        Converter *to_text;        /* 函数块 → Lv-00 DSL */
    } converters;
    
    /* 双向同步 */
    struct {
        Converter *from_geometry;
        Converter *from_node;
        Converter *from_block;
        Converter *from_text;
    } reverse_converters;
    
    /* 同步冲突检测 */
    ConflictDetector *conflict_detector;
} RepresentationConverter;
```

### 6.2 函数块 ↔ 几何实体转换

**复用自举设计的元表示**：

```c
/*
 * 借鉴 self_bootstrapping_design.md 的几何编码方案：
 * 
 * C 结构体 → 几何隐喻
 * 
 * 函数块 → 几何实体映射：
 * 
 * FuncBlock        → REGION（区域）
 * 输入端口         → PORT（端口）
 * 输出端口         → PORT（端口）
 * 内部节点         → 内部 POINT/LINE 等
 * 端口连接         → CONNECTION 约束
 */

ConverterResult convert_block_to_geometry(FuncBlock *block);
ConverterResult convert_geometry_to_block(GeomNode *entity);
```

### 6.3 函数块 ↔ Lv-00 文本转换

**双向生成**：

```c
/* 函数块图 → Lv-00 DSL */
char *generate_lv00_code(FuncBlockGraph *graph);

/* Lv-00 DSL → 函数块图 */
FuncBlockGraph *parse_lv00_code(char *code);

/* 示例转换 */

/* 函数块图表示：
 * 
 *   [Point A]──→[midpoint]──→[Point M]
 *   [Point B]──→
 */

/* Lv-00 DSL 表示：
 * 
 *   Point A, B;
 *   Let M : Point = midpoint(A, B);
 */
```

---

## 七、实现路线图

### 7.1 阶段划分

```
Phase 1: 核心架构搭建（4周）
  ├─ Week 1: Layer 6 目录结构 + CMake 配置
  ├─ Week 2: 函数块运行时扩展（控制流块）
  ├─ Week 3: 类型系统扩展（通用类型 + 效果系统）
  └─ Week 4: 表示转换层基础框架

Phase 2: 可视化编辑器（6周）
  ├─ Week 5-6: 节点图视图（布局引擎 + 交互）
  ├─ Week 7-8: 几何画布视图（复用现有渲染）
  ├─ Week 9: 积木块视图
  └─ Week 10: 文本代码视图 + 双向同步

Phase 3: 通用编程能力（4周）
  ├─ Week 11: IO交互块 + 效果追踪
  ├─ Week 12: 数据结构块
  ├─ Week 13: 函数块调度器 + 增量执行
  └─ Week 14: 与 Layer 1-5 集成测试

Phase 4: 验证与优化（2周）
  ├─ Week 15: 类型系统一致性验证
  ├─ Week 16: 性能优化 + 文档完善
```

### 7.2 文件规划

```
core/
├── include/lv00/
│   ├── visual_editor.h          # 可视化编辑器公共接口
│   ├── control_flow_blocks.h    # 控制流块
│   ├── io_blocks.h              # IO交互块
│   ├── data_structure_blocks.h  # 数据结构块
│   ├── extended_types.h         # 扩展类型系统
│   ├── effect_system.h          # 效果系统
│   ├── block_scheduler.h        # 函数块调度器
│   └── representation_converter.h # 表示转换层
│
├── src/layer6_visual/
│   ├── visual_editor.c          # 可视化编辑器核心
│   ├── geometry_canvas.c        # 几何画布视图
│   ├── node_graph.c             # 节点图视图
│   ├── block_canvas.c           # 积木块视图
│   ├── text_code.c              # 文本代码视图
│   ├── view_synchronizer.c      # 视图同步器
│   │
│   ├── control_flow/
│   │   ├── if_block.c           # 条件块
│   │   ├── while_block.c        # 循环块
│   │   ├── match_block.c        # 匹配块
│   │
│   ├── io/
│   │   ├── file_block.c         # 文件块
│   │   ├── network_block.c      # 网络块
│   │   ├── ui_block.c           # UI块
│   │
│   ├── data/
│   │   ├── list_block.c         # 列表块
│   │   ├── map_block.c          # 映射块
│   │   ├── record_block.c       # 记录块
│   │
│   ├── runtime/
│   │   ├── block_scheduler.c    # 调度器
│   │   ├── incremental_exec.c   # 增量执行
│   │   ├── effect_tracker.c     # 效果追踪
│   │
│   ├── types/
│   │   ├── extended_types.c     # 扩展类型
│   │   ├── effect_types.c       # 效果类型
│   │   ├── type_inference.c     # 类型推断增强
│   │
│   └── converter/
│       ├── block_to_geometry.c  # 函数块→几何
│       ├── block_to_node.c      # 函数块→节点图
│       ├── block_to_text.c      # 函数块→文本
│       ├── sync_protocol.c      # 同步协议
│
└── test/c/
    ├── test_layer6_visual.c     # 可视化编辑器测试
    ├── test_control_flow.c      # 控制流测试
    ├── test_io_blocks.c         # IO块测试
    ├── test_extended_types.c    # 扩展类型测试
    └── test_converter.c         # 转换器测试
```

---

## 八、与原有设计的协调

### 8.1 与函数块系统的协调

| 原有函数块系统 | Layer 6 扩展 |
|--------------|-------------|
| 40个几何预设块 | 保持不变，直接复用 |
| 打包/实例化机制 | 扩展支持控制流块打包 |
| 确定性状态机 | 控制流块需要终止性证明 |
| 组合子 | 扩展支持通用块组合 |
| 视图折叠/展开 | 直接用于节点图视图 |

### 8.2 与WFC范式的协调

| WFC范式模块 | Layer 6 应用 |
|------------|-------------|
| Module A: 约束传播引擎 | 用于节点图自动布局 |
| Module B: 等价类合并 | 用于类型等价检查 |
| Module C: 元证明 | 用于控制流终止性证明 |

### 8.3 与自举设计的协调

| 自举设计阶段 | Layer 6 复用 |
|-------------|-------------|
| 阶段一：元表示层 | 函数块→几何实体转换 |
| 阶段二：操作层 | 函数块运行时扩展 |
| 阶段三：编译器层 | 表示转换层 |

---

## 九、成功标准

| 标准 | 度量方法 | 目标 |
|------|---------|------|
| 视图同步正确性 | 四种视图编辑后语义一致 | 100% |
| 类型系统一致性 | 扩展类型与原有类型兼容 | 100% |
| 函数块扩展正确性 | 控制流/IO/数据块单元测试 | 100% |
| 几何能力保持 | 现有几何测试全部通过 | 0 回归 |
| 性能（节点图100节点） | 布局渲染时间 | < 200ms |
| 双向转换正确性 | 转换往返语义等价 | 100% |

---

## 参考文档

- [函数块系统](07_func_block.md)
- [WFC范式注入](11_wfc_paradigm.md)
- [自举架构设计](self_bootstrapping_design.md)
- [类型系统](08_type_system.md)
- [架构手册](ARCHITECTURE_MANUAL.md)
- [语言规范](LV00_LANGUAGE_SPEC.md)