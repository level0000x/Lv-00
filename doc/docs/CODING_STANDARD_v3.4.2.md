# Lv-00 代码规范 v3.4.2

> 本文档定义 Lv-00 项目的代码风格规范，所有代码修改应遵循此规范。

## 1. 命名规范

### 1.1 文件命名
- C源文件：小写下划线，如 `constraint_graph.c`
- 头文件：小写下划线，如 `func_block.h`
- 内部头文件：以 `_internal.h` 结尾

### 1.2 类型命名
- 结构体：`PascalCase`，如 `FuncBlock`, `ConstraintGraph`
- 枚举：`PascalCase`，如 `DeterminismState`
- 枚举值：`UPPER_SNAKE_CASE`，如 `DETERMINISM_VERIFIED`
- typedef：`PascalCase`，如 `PackResult`

### 1.3 函数命名
- 公共API：`模块名_动词_名词`，如 `func_block_create`
- 内部函数：`static` + 下划线前缀，如 `_internal_helper`
- 回调类型：`_cb` 或 `_callback` 后缀

### 1.4 变量命名
- 局部变量：`snake_case`，如 `node_count`
- 全局变量：`g_` 前缀，如 `g_preset_library`
- 静态变量：`s_` 前缀
- 指针变量：明确标注，如 `node_ptr`
- 布尔变量：`is_` / `has_` / `can_` 前缀

### 1.5 宏命名
- 常量宏：`UPPER_SNAKE_CASE`，如 `LV00_MAX_NODES`
- 函数式宏：`UPPER_SNAKE_CASE`，如 `SAFE_FREE(ptr)`
- 头文件保护：`LV00_文件名_H`

## 2. 代码格式

### 2.1 缩进
- 使用4个空格缩进
- 不使用Tab字符
- 续行缩进4个空格

### 2.2 括号
```c
/* 函数定义 */
ReturnType function_name(Type param1,
                         Type param2) {
    /* 代码 */
}

/* 控制结构 */
if (condition) {
    /* 代码 */
} else if (other_condition) {
    /* 代码 */
} else {
    /* 代码 */
}

/* switch */
switch (value) {
    case CASE_A:
        /* 代码 */
        break;
    case CASE_B:
        /* 代码 */
        /* fallthrough */
    case CASE_C:
        /* 代码 */
        break;
    default:
        /* 代码 */
        break;
}
```

### 2.3 空行
- 函数之间：2个空行
- 逻辑段落之间：1个空行
- 变量声明和代码之间：1个空行

## 3. 注释规范

### 3.1 文件头注释
```c
/**
 * @file filename.c
 * @brief 简短描述
 * @details 详细描述（可选）
 *
 * @version x.x.x
 * @date YYYY-MM-DD
 *
 * @note 重要说明
 * @warning 警告信息
 */
```

### 3.2 函数注释
```c
/**
 * @brief 函数简短描述
 * @details 详细描述（可选）
 *
 * @param[in]  param1  输入参数描述
 * @param[out] param2  输出参数描述
 * @param[in,out] param3  输入输出参数描述
 * @return 返回值描述
 * @retval VALUE_A 特定返回值A的含义
 * @retval VALUE_B 特定返回值B的含义
 *
 * @note 使用说明
 * @warning 警告信息
 * @code
 *   // 使用示例
 * @endcode
 */
```

### 3.3 行内注释
```c
/* 简短说明 */
int variable; /* 变量说明 */

/*
 * 多行注释说明
 * 第二行内容
 */
```

### 3.4 版本标记
```c
/* v3.4.2: 修复整数溢出问题 */
/* v3.4.2 新增：添加版本字段 */
```

## 4. 安全规范

### 4.1 内存操作
- 所有内存分配必须检查返回值
- 使用 `lv00_malloc` / `lv00_free` 统一接口
- 释放后置空指针：`lv00_free((void**)&ptr)`
- 避免裸 `malloc` / `free`

### 4.2 边界检查
```c
/* 数组访问前检查索引 */
if (index < 0 || index >= array_count) {
    LV00_LOG_ERROR("索引越界: %d (范围: 0-%d)", index, array_count - 1);
    return ERROR_INVALID_INDEX;
}

/* 字符串操作使用安全版本 */
strncpy(dst, src, dst_size - 1);
dst[dst_size - 1] = '\0';
```

### 4.3 整数运算
```c
/* 乘法溢出检查 */
if (a > INT_MAX / b) {
    /* 溢出处理 */
}

/* 加法溢出检查 */
if (a > INT_MAX - b) {
    /* 溢出处理 */
}
```

## 5. 错误处理

### 5.1 错误码使用
- 使用统一的错误码枚举
- 错误码命名：`LV00_ERROR_描述`
- 成功返回：`LV00_ERROR_NONE` 或 `true`

### 5.2 错误处理模式
```c
/* 提前返回模式 */
if (!ptr) {
    LV00_LOG_ERROR("空指针: %s", "ptr");
    return NULL;
}

/* 单一出口模式 */
ErrorCode result = LV00_ERROR_NONE;
if (condition1) {
    result = do_something();
    if (result != LV00_ERROR_NONE) goto cleanup;
}
/* ... */
cleanup:
    free_resources();
    return result;
```

## 6. 模块化设计

### 6.1 文件组织
```
src/
├── core/          # 核心引擎
├── func_block/    # 函数块系统
├── preset/        # 预设模块
├── parser/        # 解析器
└── utils/         # 工具函数
```

### 6.2 头文件组织
- 公共头文件：`include/lv00/*.h`
- 内部头文件：`src/module/internal.h`
- 避免循环依赖

### 6.3 API设计原则
- 单一职责：每个函数只做一件事
- 明确所有权：文档说明内存所有权
- 向后兼容：API修改保持兼容性

## 7. 版本控制

### 7.1 版本号格式
`主版本.次版本.补丁版本`
- 主版本：不兼容的API修改
- 次版本：向后兼容的功能添加
- 补丁版本：向后兼容的问题修复

### 7.2 版本标记
```c
#define LV00_VERSION_MAJOR 3
#define LV00_VERSION_MINOR 4
#define LV00_VERSION_PATCH 2
```

## 8. 测试规范

### 8.1 单元测试
- 测试文件：`tests/test_*.c`
- 测试函数：`test_模块名_功能()`
- 使用断言验证结果

### 8.2 测试覆盖
- 正常路径
- 错误路径
- 边界条件
- 并发场景（如适用）

## 9. 文档规范

### 9.1 API文档
- 所有公共API必须有文档注释
- 使用Doxygen格式
- 包含使用示例

### 9.2 变更日志
- 记录所有版本变更
- 分类：新增、修复、改进、废弃
- 关联Issue编号

## 10. 代码审查清单

- [ ] 命名符合规范
- [ ] 注释完整清晰
- [ ] 错误处理完善
- [ ] 内存安全（无泄漏、无越界）
- [ ] 整数运算安全
- [ ] 线程安全（如适用）
- [ ] 向后兼容
- [ ] 测试覆盖
- [ ] 文档更新
