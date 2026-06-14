# Internal Implementation Headers

内部实现头文件，仅供库内部使用，不导出为公共 API。

## 文件说明

### 数据结构定义
- `lv00_internal.h` — Layer 2 内部数据结构
- `geometry_types.h` — Layer 3 内部几何类型
- `config.h` — 编译时配置（仅库内部）
- `context.h` — 执行上下文（标记为内部）

### 算法实现
- `func_block_internal.h` — 函数块的内部接口
- `expr_canon.h` — 表达式规范化（内部）
- `parser_safety.h` — 解析器安全检查
- `node_deep_copy.h` — 节点深拷贝（内部工具）

### 优化与监控
- `axiom_grade.h` — 公理等级评分
- `proof_priority.h` — 证明优先级
- `proof_trace.h` — 证明路径追踪
- `runtime_guard.h` — 运行时保护
- `circuit_breaker.h` — 中断机制
- `logic_check.h` — 逻辑检查

### 数学底层
- `rational.h` — 有理数表示
- `exact_arithmetic.h` — 精确算术
- `three_valued_logic.h` — 三值逻辑
- `quantifier.h` — 量词处理
- `modal_operators.h` — 模态算子
- `status_codes.h` — 状态码定义

## ⚠️ 使用注意

这些头文件不应在库外部代码中使用。如果你需要访问相关功能，应该：

1. 查看对应公开 API 头文件
2. 提交特性请求以导出所需接口
3. 不要直接依赖内部实现

## 维护指南

- 内部文件可自由重构，无需保证向后兼容
- 但应通过公开 API 支持等价功能
- 定期审查是否应导出为公开 API
