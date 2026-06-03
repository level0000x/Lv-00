# LV-00 插件示例

本目录包含 LV-00 插件系统的示例代码，演示如何创建、编译和使用插件。

## 文件说明

- `sample_plugin.c` - 示例插件源代码
- `CMakeLists.txt` - CMake 构建配置
- `sample_plugin.conf` - 插件配置文件
- `README.md` - 本文件

## 插件功能

示例插件演示了以下功能：

1. **生命周期管理**
   - 插件加载/卸载
   - 插件激活/停用
   - 配置管理

2. **事件处理**
   - 接收和处理系统事件
   - 广播事件

3. **接口注册**
   - 注册自定义接口
   - 导出函数

4. **自定义功能**
   - 几何运算（海伦公式）
   - 三角形验证

## 编译步骤

### 1. 创建构建目录

```bash
mkdir build
cd build
```

### 2. 配置 CMake

```bash
cmake ..
```

### 3. 编译

```bash
cmake --build .
```

### 4. 安装（可选）

```bash
cmake --install .
```

## 使用插件

### 1. 加载插件

```c
#include "lv00/plugin_system.h"

Lv00PluginSystem* system = lv00_plugin_system_create(ctx);
lv00_plugin_system_init(system);

// 添加搜索路径
lv00_plugin_system_add_search_path(system, "/path/to/plugins");

// 加载插件
Lv00Plugin* plugin = lv00_plugin_load(system, "sample_plugin.dll");
```

### 2. 激活插件

```c
lv00_plugin_activate(plugin);
```

### 3. 使用插件功能

```c
// 查询接口
Lv00PluginInterface* interface = lv00_plugin_query_interface(
    system, "sample_interface", 1);

// 调用函数
typedef double (*compute_area_func)(double, double, double);
compute_area_func compute_area = (compute_area_func)sample_get_function("compute_area");

double area = compute_area(3.0, 4.0, 5.0);
```

### 4. 卸载插件

```c
lv00_plugin_unload(system, plugin);
lv00_plugin_system_destroy(system);
```

## 插件开发指南

### 必需函数

插件必须实现以下生命周期回调函数：

```c
int lv00_plugin_on_load(Lv00PluginContext* ctx);
int lv00_plugin_on_unload(Lv00PluginContext* ctx);
int lv00_plugin_on_activate(Lv00PluginContext* ctx);
int lv00_plugin_on_deactivate(Lv00PluginContext* ctx);
int lv00_plugin_on_configure(Lv00PluginContext* ctx, const Lv00PluginConfig* config);
int lv00_plugin_on_event(Lv00PluginContext* ctx, const Lv00PluginEvent* event);
```

### 入口宏

使用提供的宏定义插件入口：

```c
LV00_PLUGIN_DECLARE("your_plugin_name");
LV00_PLUGIN_ENTRY();
```

### 最佳实践

1. **错误处理**
   - 始终检查返回值
   - 使用日志记录错误信息
   - 在失败时正确清理资源

2. **配置管理**
   - 提供合理的默认值
   - 验证配置参数
   - 支持运行时配置更新

3. **资源管理**
   - 在 on_unload 中释放所有资源
   - 避免内存泄漏
   - 使用 RAII 模式

4. **线程安全**
   - 使用互斥锁保护共享数据
   - 避免死锁
   - 注意回调的线程上下文

## 扩展示例

### 添加自定义几何运算

```c
static double custom_operation(Lv00GeometryEntity* entity) {
    // 实现自定义运算
    return result;
}
```

### 注册自定义证明规则

```c
static int custom_proof_rule(Lv00Proof* proof, Lv00Constraint* constraint) {
    // 实现证明规则
    return 0;
}
```

### 导出可视化组件

```c
static void custom_visual_render(Lv00VisualScene* scene, Lv00VisualObject* obj) {
    // 实现自定义渲染
}
```

## 调试技巧

1. **启用调试日志**
   ```c
   lv00_plugin_config_set(config, "log_level", "debug", 0);
   ```

2. **使用调试工具**
   - 使用 debug_trace 系统记录插件行为
   - 创建运行快照进行问题分析

3. **单元测试**
   ```c
   void test_plugin_function() {
       // 测试插件功能
       assert(custom_operation(test_entity) == expected_result);
   }
   ```

## 参考文档

- `core/include/lv00/plugin_system.h` - 插件系统 API
- `doc/docs/API_USAGE_GUIDE.md` - API 使用指南
- `doc/docs/ARCHITECTURE_v3.3.md` - 系统架构文档
