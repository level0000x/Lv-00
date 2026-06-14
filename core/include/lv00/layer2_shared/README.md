# Layer 2: Shared Resources

资源管理与共享工具层，为所有上层提供基础支持。

## 模块说明

### 核心职责
- 内存分配与管理 (`memory_pool.h`)
- 错误码定义与处理 (`error_codes.h`)
- 执行上下文 (`context.h`)
- 调试工具 (`debug.h`)
- 运行时监控 (`runtime_monitor.h`)

## 公开 API

所有 Layer 2 的头文件都是**公共接口**，上层可自由使用。

### 推荐 include 方式
```c
// 旧方式（仍支持）
#include <lv00/error_codes.h>
#include <lv00/context.h>

// 新方式（推荐）
#include <lv00/layer2_shared/error_codes.h>
#include <lv00/layer2_shared/context.h>
```

## 依赖关系
- **被依赖**: Layer 1, Layer 3, Layer 4, ..., Layer 10（所有层）
- **依赖**: GMP 库、C 标准库

## 注意事项

⚠️ **内部实现头文件** (不应直接使用):
- `lv00_internal.h` — 内部数据结构定义
- `config.h` — 编译时配置
- `lv00_utils.c` 中的私有函数

这些头文件应该通过公开 API 间接使用。

## 文件清单

```
core/include/lv00/layer2_shared/
├── context.h              # 执行上下文
├── memory_pool.h          # 内存池管理
├── error_codes.h          # 错误码（通常在 lv00/ 下）
└── debug.h                # 调试工具

core/include/lv00/
├── lv00_utils.h           # 共享工具函数
├── lv00_internal.h        # 内部实现（标记）
├── config.h               # 编译配置（标记）
└── ...
```

## 维护指南

- Layer 2 应保持**轻量**和**稳定**
- 不引入高层的依赖（避免循环依赖）
- 所有功能应有充分的文档和测试
- 变更需通知所有依赖层
