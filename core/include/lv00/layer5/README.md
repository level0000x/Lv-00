# Layer 5: Output & Export

证明输出、可视化导出、互操作接口的汇集层。

## 模块说明

### 核心职责

1. **证明导出** (`proof_widget.h`, `proof_export_enhanced.h`)
   - 格式化证明步骤
   - 生成人可读的证明文本

2. **几何导出** (`geo_visual.h`)
   - TikZ 代码生成
   - SVG/PNG 光栅化
   - 其他图形格式

3. **互操作** (`interop.h`)
   - Lean/Coq 验证器接口
   - 其他验证系统的桥接

## 依赖关系

```
        Layer 5 (Output)
             ↓ 依赖
        Layer 4 (Reasoning)
        Layer 3 (Geometry)
        Layer 2 (Resource)
```

## 使用指南

Layer 5 主要被 Layer 6+ 使用，作为数据导出的统一出口。

## 文件清单

```
core/include/lv00/layer5/
├── proof_widget.h
├── tikz_export.h
├── geo_visual.h
└── ...
```
