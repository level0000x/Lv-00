# Lv-00 工程目录结构迁移计划

**版本**: v3.4-academic  
**状态**: 迁移规范

---

## 1. 当前目录结构

```
Lv-00/
├── core/
│   ├── include/lv/     # 公共头文件 (150+ 文件)
│   └── src/
│       ├── _deprecated/  # 已弃用代码
│       ├── axiom/        # 公理系统
│       ├── core/         # 核心引擎 (80+ 文件)
│       ├── func_block/   # 函数块
│       ├── interop/      # 互操作
│       ├── magic/        # 魔法模块
│       ├── parser/       # 解析器
│       ├── preset/       # 预设模块 (60+ 文件)
│       └── utils/        # 工具函数
├── doc/                  # 文档
├── test/                 # 测试
└── build/                # 构建产物
```

---

## 2. 目标十层目录结构

```
Lv-00/
├── core/
│   ├── include/lv/
│   │   ├── layer1_parser/      # 第1层：语法解析
│   │   ├── layer2_resource/    # 第2层：资源管理
│   │   ├── layer3_geometry/    # 第3层：几何内核
│   │   ├── layer4_reasoning/   # 第4层：推理引擎
│   │   └── layer5_output/      # 第5层：输出展示
│   └── src/
│       ├── layer1_parser/
│       ├── layer2_resource/
│       ├── layer3_geometry/
│       ├── layer4_reasoning/
│       └── layer5_output/
├── docs/                       # 规范文档
├── tests/                      # 测试套件
│   ├── unit/
│   ├── integration/
│   └── regression/
└── tools/                      # 开发工具
```

---

## 3. 模块映射表

### 3.1 第1层：语法解析 (Layer 1 - Parser)

| 当前位置 | 目标位置 | 说明 |
|----------|----------|------|
| `src/parser/formula_parser.c` | `layer1_parser/formula_parser.c` | 公式解析 |
| `src/parser/formula_converter.c` | `layer1_parser/formula_converter.c` | 格式转换 |
| `src/parser/formula_renderer.c` | `layer1_parser/formula_renderer.c` | 公式渲染 |
| `src/parser/lexer_shared.c` | `layer1_parser/lexer_shared.c` | 词法分析 |
| `src/parser/parser_safety.c` | `layer1_parser/parser_safety.c` | 解析安全 |

### 3.2 第2层：资源管理 (Layer 2 - Resource)

| 当前位置 | 目标位置 | 说明 |
|----------|----------|------|
| `src/core/memory_pool.c` | `layer2_resource/memory_pool.c` | 内存池 |
| `src/core/context.c` | `layer2_resource/context.c` | 上下文管理 |
| `src/core/error_codes.c` | `layer2_resource/error_codes.c` | 错误处理 |
| `src/core/debug.c` | `layer2_resource/debug.c` | 调试工具 |
| `src/utils/lv_utils.c` | `layer2_resource/utils.c` | 工具函数 |

### 3.3 第3层：几何内核 (Layer 3 - Geometry)

| 当前位置 | 目标位置 | 说明 |
|----------|----------|------|
| `src/core/constraint_graph.c` | `layer3_geometry/constraint_graph.c` | 约束图 |
| `src/core/normalization.c` | `layer3_geometry/normalization.c` | 规范化 |
| `src/core/symbolic_coord.c` | `layer3_geometry/symbolic_coord.c` | 符号坐标 |
| `src/core/type_system.c` | `layer3_geometry/type_system.c` | 类型系统 |
| `src/core/unify.c` | `layer3_geometry/unify.c` | 合一检查 |
| `src/core/euclidean_geometry.c` | `layer3_geometry/euclidean.c` | 欧氏几何 |

### 3.4 第4层：推理引擎 (Layer 4 - Reasoning)

| 当前位置 | 目标位置 | 说明 |
|----------|----------|------|
| `src/core/proof*.c` | `layer4_reasoning/proof*.c` | 证明系统 |
| `src/core/solver.c` | `layer4_reasoning/solver.c` | 求解器 |
| `src/core/rewrite.c` | `layer4_reasoning/rewrite.c` | 重写引擎 |
| `src/core/logic_check.c` | `layer4_reasoning/logic_check.c` | 逻辑检查 |
| `src/axiom/axiom_pkg.c` | `layer4_reasoning/axiom_pkg.c` | 公理包 |

### 3.5 第5层：输出展示 (Layer 5 - Output)

| 当前位置 | 目标位置 | 说明 |
|----------|----------|------|
| `src/core/tikz_export.c` | `layer5_output/tikz_export.c` | TikZ 导出 |
| `src/core/stream.c` | `layer5_output/stream.c` | 流式输出 |
| `src/core/formula_renderer.c` | `layer5_output/formula_renderer.c` | 公式渲染 |

---

## 4. 迁移步骤

### 4.1 阶段1：准备 (Week 1)

1. **创建目标目录结构**
   ```bash
   mkdir -p core/src/{layer1_parser,layer2_resource,layer3_geometry,layer4_reasoning,layer5_output}
   mkdir -p core/include/lv/{layer1_parser,layer2_resource,layer3_geometry,layer4_reasoning,layer5_output}
   ```

2. **更新 CMakeLists.txt**
   - 添加新的源文件分组
   - 定义十层库目标

3. **创建迁移脚本**
   - 自动化文件移动
   - 更新 #include 路径

### 4.2 阶段2：底层迁移 (Week 2)

1. **迁移第1层（语法解析）**
   - 移动解析器相关文件
   - 更新头文件路径
   - 运行测试验证

2. **迁移第2层（资源管理）**
   - 移动资源管理文件
   - 更新依赖关系
   - 运行测试验证

### 4.3 阶段3：核心迁移 (Week 3)

1. **迁移第3层（几何内核）**
   - 移动几何核心文件
   - 更新约束图和规范化模块
   - 运行测试验证

2. **迁移第4层（推理引擎）**
   - 移动证明系统文件
   - 更新求解器和重写引擎
   - 运行测试验证

### 4.4 阶段4：输出迁移 (Week 4)

1. **迁移第5层（输出展示）**
   - 移动导出和渲染文件
   - 更新流式输出模块
   - 运行测试验证

2. **清理旧结构**
   - 删除空目录
   - 更新文档
   - 更新 CI/CD 配置

---

## 5. CMake 配置示例

```cmake
# 第1层：语法解析
add_library(lv_layer1_parser
    core/src/layer1_parser/formula_parser.c
    core/src/layer1_parser/formula_converter.c
    core/src/layer1_parser/formula_renderer.c
    core/src/layer1_parser/lexer_shared.c
    core/src/layer1_parser/parser_safety.c
)
target_include_directories(lv_layer1_parser PUBLIC
    core/include/lv/layer1_parser
)

# 第2层：资源管理
add_library(lv_layer2_resource
    core/src/layer2_resource/memory_pool.c
    core/src/layer2_resource/context.c
    core/src/layer2_resource/error_codes.c
    core/src/layer2_resource/debug.c
    core/src/layer2_resource/utils.c
)

# 第3层：几何内核
add_library(lv_layer3_geometry
    core/src/layer3_geometry/constraint_graph.c
    core/src/layer3_geometry/normalization.c
    core/src/layer3_geometry/symbolic_coord.c
    core/src/layer3_geometry/type_system.c
    core/src/layer3_geometry/unify.c
)
target_link_libraries(lv_layer3_geometry PUBLIC lv_layer2_resource)

# 第4层：推理引擎
add_library(lv_layer4_reasoning
    core/src/layer4_reasoning/proof_core.c
    core/src/layer4_reasoning/proof_scoped.c
    core/src/layer4_reasoning/solver.c
    core/src/layer4_reasoning/rewrite.c
    core/src/layer4_reasoning/logic_check.c
)
target_link_libraries(lv_layer4_reasoning PUBLIC lv_layer3_geometry)

# 第5层：输出展示
add_library(lv_layer5_output
    core/src/layer5_output/tikz_export.c
    core/src/layer5_output/stream.c
)
target_link_libraries(lv_layer5_output PUBLIC lv_layer4_reasoning)

# 主库
add_library(lv STATIC
    $<TARGET_OBJECTS:lv_layer1_parser>
    $<TARGET_OBJECTS:lv_layer2_resource>
    $<TARGET_OBJECTS:lv_layer3_geometry>
    $<TARGET_OBJECTS:lv_layer4_reasoning>
    $<TARGET_OBJECTS:lv_layer5_output>
)
```

---

## 6. 风险与缓解

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 头文件路径变更 | 编译错误 | 使用相对路径，逐步迁移 |
| 循环依赖 | 链接失败 | 明确层级依赖方向 |
| 测试覆盖不足 | 回归问题 | 迁移前增加测试用例 |
| CI/CD 配置过时 | 构建失败 | 同步更新 CI 配置 |

---

## 7. 验收标准

1. **编译通过**：所有目标平台编译无错误
2. **测试通过**：所有现有测试用例通过
3. **文档更新**：架构文档反映新目录结构
4. **CI/CD 正常**：持续集成流水线运行正常

---

**文档状态**: 已完成  
**下一步**: 执行迁移脚本，逐步迁移各层模块
