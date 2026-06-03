# Lv-00 UI编程辅助系统

专为Lv-00几何元语言可视化界面优化的智能编程助手。

## 🎯 简介

这个系统为Lv-00项目的UI编程提供全面的辅助支持，包括：

- 📚 **领域知识库** - Lv-00的API、概念、设计规范
- 📝 **代码模板** - WebAssembly绑定、Canvas渲染器、UI组件模板
- 💡 **智能提示** - 根据上下文生成优化提示词
- 🔍 **API参考** - 快速查询可用函数和用法

## 🚀 快速开始

### 运行助手

```bash
cd llm_coding_assistant
python main.py
```

### 常用命令

```
# 查看帮助
help

# 查询API
api graph          # 查看图操作API
api proof          # 查看证明系统API
api coord          # 查看坐标系统API

# 获取代码模板
template wasm_string    # WebAssembly字符串绑定模板
template canvas         # Canvas渲染器模板

# 获取代码片段
snippet c_memory       # C语言内存管理
snippet js_promise     # JavaScript异步调用

# 生成任务指南
task 添加新的约束类型UI
task 实现证明导航器

# 解释概念
explain normalization   # 图归一化
explain unification      # 合一检查
explain proof           # 证明系统
```

## 📁 项目结构

```
llm_coding_assistant/
├── main.py              # 主程序入口
├── lv00_knowledge.py    # 领域知识库和提示词引擎
├── templates.py         # 代码模板库
└── README.md            # 本文档
```

## 🔧 Lv-00 UI架构

Lv-00的UI涉及多层技术栈：

```
┌─────────────────────────────────────────┐
│         HTML/CSS UI界面                 │
│  (深色主题、模块面板、Canvas画布)       │
├─────────────────────────────────────────┤
│      JavaScript交互逻辑                  │
│  (事件处理、Canvas渲染、API封装)         │
├─────────────────────────────────────────┤
│      WebAssembly绑定层                   │
│  (Emscripten编译的C代码桥接)             │
├─────────────────────────────────────────┤
│      C语言内核                           │
│  (符号坐标、约束图、归一化、求解器...)    │
└─────────────────────────────────────────┘
```

## 📖 核心概念

### 1. 符号坐标系统

Lv-00支持多种坐标类型：

- **有理数** (`RATIONAL`) - 使用GMP的mpq_t
- **代数数** (`ALGEBRAIC`) - 极小多项式+隔离区间
- **二次根式** (`QUADRATIC`) - a + b√n形式
- **超越常数** (`TRANSCENDENTAL`) - π, e

### 2. 约束图

核心数据结构，包含：

- **节点类型**: 点、线段、区域、端口、函数块
- **约束类型**: 关联、之间、相交、包含、连接

### 3. 归一化

保证幂等性的核心机制：

1. 合并坐标相同的点
2. 合并端点相同的线段
3. 稳定化拓扑排序

### 4. 合一检查

证明系统的核心：

1. 归一化构造图和命题图
2. 三层匹配（端口、约束、坐标）
3. 成功则命题得证

### 5. 函数块

封装内部约束子图为可复用单元：

- **打包** (Pack): 创建函数块
- **例化** (Instantiate): 创建实例
- **组合** (Compose): 组合多个函数块

## 🎨 UI编程模式

### Canvas渲染器模式

```javascript
class GeometryRenderer {
    constructor(canvas) {
        this.canvas = canvas;
        this.ctx = canvas.getContext('2d');
        this.zoom = 1.0;
        this.offset = { x: 0, y: 0 };
    }
    
    worldToScreen(wx, wy) {
        return {
            x: (wx * this.zoom) + this.offset.x,
            y: (wy * this.zoom) + this.offset.y
        };
    }
    
    render() {
        // 绘制几何对象
    }
}
```

### WebAssembly绑定模式

```c
#include <emscripten.h>

EMSCRIPTEN_KEEPALIVE
int web_function_name(void* graph, int param) {
    if (!graph) return -1;
    
    ConstraintGraph* g = (ConstraintGraph*)graph;
    // 调用C API
    ResultType result = c_api_function(g, param);
    
    return (result == EXPECTED) ? 0 : -1;
}
```

## 📚 API速查

### 图操作

| 函数 | 说明 |
|------|------|
| `graph_create()` | 创建约束图 |
| `graph_destroy(g)` | 销毁约束图 |
| `graph_add_point(g, coords, count)` | 添加点 |
| `graph_add_line_segment(g, p1, p2)` | 添加线段 |
| `graph_normalize(g, interactive)` | 归一化 |

### 约束

| 函数 | 说明 |
|------|------|
| `graph_add_incidence(g, point, line)` | 关联约束 |
| `graph_add_betweenness(g, p1, p2, p3)` | 之间约束 |
| `graph_add_intersection(g, l1, l2)` | 相交约束 |

### 函数块

| 函数 | 说明 |
|------|------|
| `func_block_pack(...)` | 打包函数块 |
| `func_block_instantiate(...)` | 例化函数块 |
| `func_block_compose(...)` | 组合函数块 |

### 证明

| 函数 | 说明 |
|------|------|
| `proof_create_proposition(...)` | 创建命题 |
| `proof_unify(...)` | 合一检查 |
| `proof_step_forward(...)` | 前进一步 |
| `proof_step_backward(...)` | 后退一步 |

## 🎯 常见任务

### 1. 添加新的WebAssembly绑定

```bash
# 查看绑定模板
template wasm_string

# 查看相关API
api graph
```

### 2. 实现新的Canvas渲染

```bash
# 获取渲染器模板
template canvas

# 获取交互处理器模板
template interaction
```

### 3. 添加新的UI面板

```bash
# 获取面板模板
template panel

# 查看示例面板
cat ../Lv-00/web/index.html
```

## 🔍 调试技巧

### 启用调试日志

```javascript
// 在浏览器控制台
window.app.debug = true;
window.app.updateStatus('DEBUG MODE');
```

### 查看性能计数器

```javascript
// 访问调试面板
document.querySelector('.module-tab[data-module="debug"]').click();
```

### 检查内存使用

```javascript
Module.ccall('web_memory_usage', 'number', [], []);
```

## 📦 依赖

- Python 3.8+
- 无其他外部依赖（纯本地知识库）

## 🤝 贡献

欢迎提交Issue和Pull Request！

## 📄 许可证

MIT License - 同Lv-00主项目

---

Made with ❤️ for Lv-00 Symbolic Geometry Engine
