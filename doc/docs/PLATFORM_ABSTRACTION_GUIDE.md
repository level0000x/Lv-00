# Lv-00 平台抽象层使用指南

版本: v3.4.1
日期: 2026-05-25

## 一、概述

Lv-00 使用统一的平台抽象层来隔离 Windows 和 POSIX 平台的差异，主要涉及：
- 互斥锁（Mutex）
- 条件变量（Condition Variable）
- 线程局部存储（Thread Local Storage）

## 二、已定义的统一宏

### 2.1 互斥锁操作

| 宏 | 说明 |
|---|---|
| `LV00_MUTEX_INIT_PTR(m)` | 初始化互斥锁 |
| `LV00_MUTEX_DESTROY_PTR(m)` | 销毁互斥锁 |
| `LV00_MUTEX_LOCK_PTR(m)` | 加锁 |
| `LV00_MUTEX_UNLOCK_PTR(m)` | 解锁 |

### 2.2 条件变量操作

| 宏 | 说明 |
|---|---|
| `LV00_CONDVAR_INIT_PTR(cv)` | 初始化条件变量 |
| `LV00_CONDVAR_DESTROY_PTR(cv)` | 销毁条件变量 |
| `LV00_CONDVAR_SIGNAL_PTR(cv)` | 唤醒单个等待线程 |
| `LV00_CONDVAR_BROADCAST_PTR(cv)` | 唤醒所有等待线程 |
| `LV00_CONDVAR_WAIT_PTR(cv, m)` | 等待条件变量（需要已持有互斥锁 m）|

### 2.3 线程局部存储

```c
/* 使用示例 */
LV00_THREAD_LOCAL int g_counter = 0;
```

## 三、迁移指南

### 3.1 各模块当前实现

| 模块 | 当前宏前缀 | 目标宏前缀 |
|---|---|---|
| memory_pool.c | `LV00_MUTEX_` | `LV00_MUTEX_PTR` |
| runtime_monitor.c | `MUTEX_` | `LV00_MUTEX_PTR` |
| stream.c | `lv00_mutex_*` 函数 | `LV00_MUTEX_PTR` 宏 |

### 3.2 迁移步骤

1. 在源文件顶部添加：
```c
#include "lv00_internal.h"
```

2. 替换旧的互斥锁函数调用：
```c
// 旧代码
mutex_lock(&pool->mutex);
mutex_unlock(&pool->mutex);

// 新代码
LV00_MUTEX_LOCK_PTR(&pool->mutex);
LV00_MUTEX_UNLOCK_PTR(&pool->mutex);
```

3. 替换初始化和销毁：
```c
// 旧代码
lv00_mutex_create(&mutex);
lv00_mutex_destroy(&mutex);

// 新代码
LV00_MUTEX_INIT_PTR(&mutex);
LV00_MUTEX_DESTROY_PTR(&mutex);
```

### 3.3 注意事项

1. **结构体定义**：Windows 使用 `CRITICAL_SECTION`，POSIX 使用 `pthread_mutex_t`。统一使用 `lv00_mutex_t` 类型别名。

2. **初始化顺序**：必须先初始化互斥锁，再使用。销毁顺序相反。

3. **死锁预防**：始终确保加锁后有对应的解锁调用。

## 四、FNV-1a 哈希函数

已统一到 `lv00_internal.h`：

```c
/* 字节数组哈希 */
uint32_t lv00_fnv1a_32(const void *data, size_t len);

/* 字符串哈希 */
uint32_t lv00_fnv1a_str(const char *str);
```

使用示例：
```c
uint32_t hash = lv00_fnv1a_str("example");
uint32_t hash2 = lv00_fnv1a_32(buffer, buffer_len);
```
