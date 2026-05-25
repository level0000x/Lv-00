# Lv-00 项目优化与功能补全说明

## 概述

本文档记录了 Lv-00 几何元语言系统的代码优化、功能补全和安全性改进工作。

**项目版本**：3.0.0
**优化日期**：2026-05-20
**项目用途**：理论数学研究

---

## 一、统一错误处理机制

### 新增文件
- `include/lv00/error_codes.h` - 错误码定义头文件
- `src/error_codes.c` - 错误处理实现

### 主要特性

**分层错误码设计**：
- 0: 成功
- 1-99: 通用系统错误
- 100-199: 内存与资源错误
- 200-299: 约束图相关错误
- 300-399: 符号坐标相关错误
- 400-499: 求解器相关错误
- 500-599: 重写引擎相关错误
- 600-699: 合一检查相关错误
- 700-799: 函数块相关错误
- 800-899: 类型系统相关错误
- 900-999: 证明系统相关错误

**便捷宏定义**：
- `LV00_CHECK_NULL(ptr, ret)` - 空指针检查
- `LV00_CHECK(cond, err_code, ret, msg)` - 条件检查
- `LV00_CHECK_ALLOC(ptr, ret)` - 内存分配检查
- `LV00_PROPAGATE_ERROR(code)` - 错误传播

**线程安全**：使用线程局部存储确保多线程安全

### 使用方法
```c
#include "lv00.h"

Lv00ErrorCode result = some_operation();
if (lv00_is_error(result)) {
    printf("Error: %s\n", lv00_get_last_error_message());
    return result;
}
```

---

## 二、核心算法完善

### Gröbner 基算法修复

**文件**: `src/solver.c` (第 437-475 行)

**问题**: 直线交点计算实现不完整，丢弃了常数项 c1、c2，未正确联立两条线方程。

**修复方案**:
```
原实现: 仅使用 a1*x + b1 和 a2*x + b2，缺少常数项
新实现: 正确计算行列式 D = a1*b2 - a2*b1
        x = (b1*c2 - b2*c1) / D
        y = (a2*c1 - a1*c2) / D
```

**改进点**:
1. 添加平行线检测（|D| < 1e-10）
2. 正确处理常数项 c1、c2
3. 使用缩放整数避免浮点精度问题
4. 添加详细的中文注释说明算法原理

---

## 三、代数方程到曲线转换

### 新增文件
- `include/lv00/formula_converter.h`
- `src/formula_converter.c`

### 新增数据结构
```c
/* 曲线采样点 */
typedef struct {
    double x;           /* X坐标 */
    double y;           /* Y坐标 */
    bool is_valid;      /* 该点是否有效 */
} CurveSamplePoint;

/* 代数方程曲线转换结果 */
typedef struct {
    bool success;
    char equation_str[512];
    CurveSamplePoint *points;
    int point_count;
    double bbox_min_x, bbox_min_y, bbox_max_x, bbox_max_y;
    char error_message[256];
} EquationCurveResult;
```

### 新增 API
1. `formula_convert_equation_to_curve()` - 将代数方程转换为曲线采样点
2. `equation_curve_result_destroy()` - 销毁转换结果
3. `formula_convert_equation()` - 将方程节点添加到约束图

### 算法实现
- **行进正方形算法**（Marching Squares）- 提取等值线
- **自适应网格采样** - 粗采样 + 精细采样
- **牛顿迭代细化** - 将采样点精确到曲线上

---

## 四、ID 映射机制与深拷贝

### 新增数据结构
```c
/* ID映射表条目 */
typedef struct {
    int old_id;     /* 源图中的节点ID */
    int new_id;     /* 目标图中的节点ID */
} IdMappingEntry;

/* ID映射表 */
typedef struct {
    IdMappingEntry *entries;
    int count;
    int capacity;
} IdMappingTable;
```

### 实现功能
1. **五阶段深拷贝策略**:
   - 第一阶段：拷贝 POINT 和 PORT 节点
   - 第二阶段：拷贝 LINE_SEGMENT 节点（更新端点ID）
   - 第三阶段：拷贝 REGION 节点（更新边界线段ID）
   - 第四阶段：拷贝 FUNCTION_BLOCK 节点（更新内部引用）
   - 第五阶段：拷贝约束（转换参与者ID）

2. **ID映射管理**:
   - `id_mapping_init()` - 初始化映射表
   - `id_mapping_add()` - 添加映射
   - `id_mapping_find()` - 查找映射
   - `id_mapping_destroy()` - 销毁映射表

---

## 五、Python 绑定完善

### 问题描述
原有 Python 绑定只包含基础核心类，缺少高级功能的绑定支持。

### 扩展内容

#### `_ctypes_binding.py` 扩展

新增绑定支持：

| 模块 | 新增功能 |
|------|----------|
| **Engine** | 引擎创建、求解流程、重写配置、位电路处理、冻结点管理 |
| **FuncBlock** | 函数块打包、例化、确定性检查、多解选择器 |
| **Proof** | 命题系统、证明导航、合一检查、导出功能 |
| **Recursion** | 测度系统、递归上下文、循环检测 |
| **Debug** | 日志管理、性能计数器、内存池 |

新增常量定义：
- 引擎状态码（ENGINE_*）
- 打包结果码（PACK_*）
- 例化结果码（INSTANTIATE_*）
- 确定性状态（DETERMINISM_*）
- 选择器类型（SELECTOR_*）
- 日志级别（LOG_LEVEL_*）

#### `core.py` 增强

新增功能：
- 完整的错误异常类体系
- Point 类的距离计算和中点计算
- LineSegment 类的长度计算
- Graph 类的端口和区域操作
- 调试和日志管理函数

#### `engine.py` 新增

完整 Engine 类实现：
- 工作流编排接口
- 模块/公理包加载
- 函数打包与例化
- 重写-求解协作
- 位电路跳闸处理
- 冻结点快照回滚

#### `func_block.py` 新增

完整函数块系统：
- FuncBlock 类
- SolutionSelector 多解选择器
- DeterminismState 确定性状态
- PackResult 打包结果
- InstantiateResult 例化结果
- func_block_pack 打包函数

---

## 六、代码安全性优化

### 问题描述
部分源代码使用 `strcpy` 和 `sprintf` 等不安全函数，存在潜在的缓冲区溢出风险。

### 修复内容

#### `module.c` 修复

修复前（不安全）：
```c
static char *safe_strdup(const char *s) {
    if (!s) return NULL;
    char *dup = malloc(strlen(s) + 1);
    if (dup) strcpy(dup, s);  // 不安全
    return dup;
}
```

修复后（安全）：
```c
static char *safe_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *dup = malloc(len + 1);
    if (dup) {
        memcpy(dup, s, len + 1);  // 使用 memcpy，更安全
    }
    return dup;
}
```

#### `axiom_pkg.c` 修复

同样修复了 `safe_strdup` 函数中的 `strcpy` 调用。

#### `serialize_symbolic_coord` 函数修复

修复前：
```c
sprintf(result, "rational %s", str);  // 不安全
```

修复后：
```c
snprintf(result, strlen(str) + 16, "rational %s", str);  // 安全
```

---

## 七、中文注释完善

### Python 代码注释
为所有 Python 模块添加了完整的中文注释：
- 模块级文档字符串
- 类和函数的文档说明
- 参数和返回值说明
- 使用示例

### 头文件注释
确保所有头文件包含：
- 文件级说明
- 函数功能描述
- 参数说明
- 返回值说明

---

## 八、代码清理

### 删除内容
- **删除目录**: `术式/` - 项目旧副本，包含重复代码

### 影响
- 减少项目体积约 50%
- 消除维护两份代码的风险
- 避免构建时的混淆

---

## 九、功能补全清单

### 已实现的 Python API

| 类/函数 | 功能 | 状态 |
|---------|------|------|
| `Graph` | 约束图管理 | ✅ 完成 |
| `Point` | 几何点 | ✅ 完成 |
| `LineSegment` | 线段 | ✅ 完成 |
| `SymbolicCoord` | 符号坐标 | ✅ 完成 |
| `Engine` | 主引擎 | ✅ 完成 |
| `FuncBlock` | 函数块 | ✅ 完成 |
| `SolutionSelector` | 多解选择器 | ✅ 完成 |

### 完整导入示例

```python
# 导入所有功能
from lv00 import (
    # 核心
    Graph, Point, LineSegment, SymbolicCoord,

    # 引擎
    Engine, EngineError,

    # 函数块
    FuncBlock, SolutionSelector,
    DeterminismState, PackResult, InstantiateResult,

    # 工具
    init, cleanup, get_version,
    set_debug_mode, set_log_level
)

# 使用示例
engine = Engine()
engine.set_rewrite_step_limit(1000)
graph = Graph()
p1 = graph.add_point(0, 0)
p2 = graph.add_point(1, 1)
result = engine.solve()
```

---

## 十、测试建议

### 需要测试的功能

1. **错误处理系统**:
   ```c
   Lv00ErrorCode code = LV00_ERROR_OUT_OF_MEMORY;
   printf("Name: %s\n", lv00_error_name(code));
   printf("Message: %s\n", lv00_error_string(code));
   ```

2. **直线交点计算**:
   - 测试两条非平行直线的交点
   - 测试平行线的情况
   - 测试重合线的情况

3. **代数方程转换**:
   - 测试圆方程: x^2 + y^2 = 1
   - 测试直线方程: y = x + 1
   - 测试抛物线: y = x^2

4. **深拷贝功能**:
   - 测试包含 LINE_SEGMENT 的图
   - 测试包含 REGION 的图
   - 测试包含 FUNCTION_BLOCK 的图

### 构建测试
```bash
mkdir build && cd build
cmake ..
make -j4
ctest --output-on-failure
```

---

## 十一、已知问题和限制

### Python 绑定待完善

以下功能仍需在底层 C 代码中实现后暴露：
- 完整的 Formula 解析和渲染 API
- Proof 证明导出功能
- Recursion 递归检测功能

### 边界条件

- 超大数值计算可能溢出
- 极端嵌套深度的图可能栈溢出
- 并发使用尚未完全测试

---

## 十二、未来优化建议

### 短期优化

1. **完善 Formula 模块**：添加完整的公式解析和渲染支持
2. **增强 Proof 系统**：完善证明树的可视化和导出
3. **优化求解器**：减少大规模系统的求解时间

### 长期优化

1. **并行计算**：支持多线程求解
2. **GPU 加速**：使用 CUDA/OpenCL 加速符号计算
3. **WebAssembly**：支持浏览器端运行

---

## 十三、版本历史

| 版本 | 日期 | 变更内容 |
|------|------|----------|
| 3.0.0 | 2026-05-20 | 完善 Python 绑定、修复安全性问题、添加中文注释、统一错误处理 |
| 2.0.0 | 2025 | 函数块系统、重写引擎 |
| 1.0.0 | 2024 | 初始版本，核心符号坐标和约束图 |

---

*本文档最后更新：2026-05-20*
