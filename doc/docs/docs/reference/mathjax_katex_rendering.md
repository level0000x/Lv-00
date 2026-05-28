# MathJax - 数学公式渲染引擎参考文档

> **项目名称**：MathJax / KaTeX
> **项目链接**：https://github.com/mathjax/MathJax
> **项目链接**：https://github.com/KaTeX/KaTeX
> **项目类型**：Web 数学公式渲染
> **语言/技术栈**：JavaScript/TypeScript、WebAssembly (KaTeX)、SVG/HTML/CSS
> **最后更新**：2024年持续活跃
> **文档版本**：v1.0
> **适用层级**：第5层（输出证明编译层）

---

## 1. 项目概述

### 1.1 项目背景与定位

MathJax 是最流行的 Web 数学公式渲染库，由 American Mathematical Society、Dijkstra 协会和项目贡献者联合开发。MathJax 的核心理念是**"Web 上的数学应该是美丽的"**——将 LaTeX/MathML 公式渲染为高质量的 Web 可视化。

KaTeX 是 Khan Academy 开发的轻量级替代品，专注于**渲染速度**，使用 WebAssembly 实现接近原生的性能。

### 1.2 核心功能对比

| 特性 | MathJax 3 | KaTeX |
|:---|:---|:---|
| 渲染速度 | 中等（纯JS） | 快速（WASM） |
| 输出格式 | SVG/HTML | HTML/CSS |
| 输入格式 | LaTeX/MathML/AsciiMath | LaTeX |
| 浏览器支持 | IE11+ | 所有现代浏览器 |
| 无障碍支持 | 优秀 | 良好 |
| 包大小 | ~250KB | ~100KB |
| 可扩展性 | 高 | 中等 |

### 1.3 架构设计

MathJax 3 的模块化架构：

```
┌─────────────────────────────────────────────────────┐
│ Input: LaTeX / MathML / AsciiMath                   │
├─────────────────────────────────────────────────────┤
│ [a11y] [assistive-mml] [checkmark] [color] ...      │
├─────────────────────────────────────────────────────┤
│ Core: MathDocument → MathList → MathItem            │
├─────────────────────────────────────────────────────┤
│ Output: SVG / CHTML / CommonHTML                    │
├─────────────────────────────────────────────────────┤
│ Render to DOM / Canvas / SVG                        │
└─────────────────────────────────────────────────────┘
```

---

## 2. 核心借鉴点

### 2.1 TeX 到渲染树的转换

MathJax 的核心是将 TeX/LaTeX 源码转换为渲染树：

```javascript
// MathJax v3 架构
import { MathDocument, MathItem, MathML } from '@mathjax/mathml'
import { HTMLHandler } from '@mathjax/html'
import { TeX } from '@mathjax/tex'

// 输入处理器
const TeXinput = new TeX({ 
  packages: ['base', 'ams', 'newcommand'] 
})

// 输出处理器  
const HTMLoutput = new HTMLHandler({
  fontFamily: 'KaTeX_Main',
  scale: 1.1
})

// 文档处理器
const MathDoc = new MathDocument(HtmlDocument, {
  InputJax: TeXinput,
  OutputJax: HTMLoutput
})

// 渲染流程
const mathItem = new MathItem('\\frac{-b \\pm \\sqrt{b^2-4ac}}{2a}', 
                               TeXinput)
mathItem.compile(MathDoc)
mathItem.typeset(HTMLoutput)
```

**Lv-00 借鉴价值**：Lv-00 可参考此架构，将**几何证明表达式**转换为**渲染树**，再渲染为图形。

### 2.2 WebAssembly 渲染加速

KaTeX 使用 WebAssembly 实现高性能渲染：

```javascript
// KaTeX WASM 渲染
import katex from 'katex'

// 渲染为 HTML 字符串
const html = katex.renderToString('\\int_0^\\infty f(x) dx', {
    throwOnError: false,
    displayMode: true,
    output: 'htmlAndMathml'
})

// 渲染到 DOM
katex.render('E = mc^2', document.getElementById('math'), {
    throwOnError: true
})
```

**Lv-00 借鉴价值**：几何对象的 SVG 渲染可使用 WASM 加速，实现实时预览。

### 2.3 核心借鉴点对照表

| MathJax/KaTeX 特性 | Lv-00 现有能力 | Lv-00 借鉴方案 |
|:---|:---|:---|
| LaTeX 解析 | `tikz_export.h` 基础 | 增强 LaTeX 解析，支持几何扩展 |
| 渲染树 | 无 | 新增 geo_render_tree 类型 |
| SVG/HTML 输出 | 基础 | 增强可视化输出 |
| 字体加载 | 无 | 集成数学字体系统 |
| 无障碍支持 | 无 | 新增 MathML 输出 |

---

## 3. Lv-00 映射方案

### 3.1 几何渲染树设计

```c
// geo_render_tree.h - 几何渲染树

#ifndef LV00_GEO_RENDER_TREE_H
#define LV00_GEO_RENDER_TREE_H

#include <lv00.h>

// ============ 渲染节点类型 ============

typedef enum {
    LV00_RNODE_TEXT,           // 文本节点
    LV00_RNODE_SYMBOL,         // 数学符号
    LV00_RNODE_FRACTION,       // 分式
    LV00_RNODE_SURD,          // 根号
    LV00_RNODE_SCRIPT,         // 上标/下标
    LV00_RNODE_OVERLINE,       // 上划线
    LV00_RNODE_UNDERLINE,      // 下划线
    LV00_RNODE_ACCENT,         // 重音符号
    LV00_RNODE_GEOM,           // 几何图形
    LV00_RNODE_GEOM_POINT,     // 几何点
    LV00_RNODE_GEOM_LINE,      // 几何线
    LV00_RNODE_GEOM_CIRCLE,    // 几何圆
    LV00_RNODE_GEOM_ANGLE,     // 角度标记
    LV00_RNODE_GEOM_ARROW,     // 箭头
    LV00_RNODE_GEOM_MARK,      // 标记（如 ∥、⊥）
    LV00_RNODE_GROUP,          // 分组
    LV00_RNODE_TABLE           // 表格/矩阵
} Lv00RenderNodeTag;

// 渲染节点
typedef struct Lv00RenderNode Lv00RenderNode;
struct Lv00RenderNode {
    Lv00RenderNodeTag tag;
    
    // 样式
    struct {
        float font_size;
        const char* font_family;
        float color[4];           // RGBA
        float background[4];
        int bold, italic, serif;
        float margin[4];           // 上右下左
        float padding[4];
    } style;
    
    // 布局属性
    struct {
        float width, height;
        float baseline;           // 基线偏移
        float italic_correction;
        float accent_underscore;
    } metrics;
    
    union {
        struct { char* text; } text;
        struct { char* sym; } symbol;
        struct { 
            Lv00RenderNode* num; 
            Lv00RenderNode* den; 
            float bar_thickness;
        } fraction;
        struct {
            Lv00RenderNode* base;
            Lv00RenderNode* index;
        } script;
        struct {
            Lv00RenderNode* base;
            char* accent_name;
        } accent;
        struct {
            Lv00VisualObject* obj;  // 关联的几何对象
            float position[2];      // 位置
            float scale;
        } geom;
        struct {
            Lv00RenderNode** children;
            size_t child_count;
            int row_align;         // 基线对齐
        } group;
        struct {
            Lv00RenderNode*** rows;
            size_t row_count;
            size_t* col_counts;
            float* col_widths;
            float* row_heights;
            float border_width;
        } table;
    } data;
    
    // 子节点（用于通用分组）
    Lv00RenderNode** children;
    size_t child_count;
};

// 渲染树
typedef struct Lv00RenderTree Lv00RenderTree;
struct Lv00RenderTree {
    Lv00RenderNode* root;
    float width, height;
    float default_font_size;
};

// ============ 几何符号扩展 ============

// 几何标记符号
typedef enum {
    LV00_GEO_MARK_NONE,
    LV00_GEO_MARK_PARALLEL,     // ∥
    LV00_GEO_MARK_PERPENDICULAR, // ⟂
    LV00_GEO_MARK_CONGRUENT,     // ≅
    LV00_GEO_MARK_SIMILAR,       // ∼
    LV00_GEO_MARK_EQUAL,         // =
    LV00_GEO_MARK_ARROW_RIGHT,   // →
    LV00_GEO_MARK_ARROW_LEFT,    // ←
    LV00_GEO_MARK_ARROW_BOTH,    // ↔
    LV00_GEO_MARK_TRIANGLE,      // △
    LV00_GEO_MARK_CIRCLE,        // ⊙
    LV00_GEO_MARK_SEGMENT,       // 线段标记
    LV00_GEO_MARK_ARC            // 圆弧
} Lv00GeoMark;

// ============ API 声明 ============

// 节点构造
Lv00RenderNode* lv00_rnode_text_create(const char* text);
Lv00RenderNode* lv00_rnode_symbol_create(const char* sym);
Lv00RenderNode* lv00_rnode_fraction_create(Lv00RenderNode* num, Lv00RenderNode* den);
Lv00RenderNode* lv00_rnode_script_create(Lv00RenderNode* base, 
                                         Lv00RenderNode* sup, 
                                         Lv00RenderNode* sub);
Lv00RenderNode* lv00_rnode_sqrt_create(Lv00RenderNode* base);
Lv00RenderNode* lv00_rnode_geom_create(Lv00VisualObject* obj);
Lv00RenderNode* lv00_rnode_geom_mark_create(Lv00Term* a, Lv00Term* b, Lv00GeoMark mark);
Lv00RenderNode* lv00_rnode_group_create(Lv00RenderNode** children, size_t count);

// 样式设置
void lv00_rnode_set_style(Lv00RenderNode* node, const char* font_family, float size);
void lv00_rnode_set_color(Lv00RenderNode* node, float r, float g, float b, float a);
void lv00_rnode_set_bold(Lv00RenderNode* node, int bold);

// 渲染树构建
Lv00RenderTree* lv00_rtree_create(void);
void lv00_rtree_set_root(Lv00RenderTree* tree, Lv00RenderNode* root);
void lv00_rtree_measure(Lv00RenderTree* tree);

// 渲染
typedef enum {
    LV00_RENDER_SVG,
    LV00_RENDER_HTML,
    LV00_RENDER_PNG,
    LV00_RENDER_TIKZ,
    LV00_RENDER_MATHML
} Lv00RenderFormat;

char* lv00_rtree_render_svg(Lv00RenderTree* tree, float scale);
char* lv00_rtree_render_html(Lv00RenderTree* tree);
char* lv00_rtree_render_mathml(Lv00RenderTree* tree);
void lv00_rtree_render_file(Lv00RenderTree* tree, const char* path, 
                            Lv00RenderFormat format);

// LaTeX 转换
Lv00RenderTree* lv00_latex_to_rtree(const char* latex);

// 清理
void lv00_rnode_destroy(Lv00RenderNode* node);
void lv00_rtree_destroy(Lv00RenderTree* tree);

#endif // LV00_GEO_RENDER_TREE_H
```

### 3.2 几何 LaTeX 扩展

```c
// geo_latex_ext.h - 几何 LaTeX 扩展

#ifndef LV00_GEO_LATEX_EXT_H
#define LV00_GEO_LATEX_EXT_H

#include <lv00.h>
#include <lv00/geo_render_tree.h>

// ============ 几何宏定义 ============

// 几何点标记
#define LV00_LATEX_POINT(p) "\\mathbf{" #p "}"
#define LV00_LATEX_POINT_NAMED(name, p) "\\stackrel{" #name "}{\\mathbf{" #p "}}"

// 几何线
#define LV00_LATEX_LINE_AB(a, b) "\\overline{" #a #b "}"
#define LV00_LATEX_LINE_L(l) "\\ell_{" #l "}"

// 几何标记
#define LV00_LATEX_PARALLEL(a, b) #a "\\parallel" #b
#define LV00_LATEX_PERP(a, b) #a "\\perp" #b
#define LV00_LATEX_CONGRUENT(a, b) #a "\\cong" #b
#define LV00_LATEX_SIMILAR(a, b) #a "\\sim" #b

// 角度标记
#define LV00_LATEX_ANGLE(a, b, c) "\\angle" #a #b #c
#define LV00_LATEX_ANGLE_MEASURE(a, b, c, d) "\\angle" #a #b #c "=" #d "^{\\circ}"

// ============ 解析器扩展 ============

typedef struct Lv00LatexParser Lv00LatexParser;
struct Lv00LatexParser {
    // 基解析器
    void* base_parser;
    
    // 几何宏表
    struct {
        const char* name;
        Lv00RenderNode* (*expand)(void* ctx, const char* args);
    }* geo_macros;
    size_t macro_count;
    
    // 几何命令处理
    int (*geom_point_handler)(Lv00LatexParser* p, const char* name, 
                              Lv00RenderNode** out);
    int (*geom_line_handler)(Lv00LatexParser* p, const char* name,
                             Lv00RenderNode** out);
    int (*geom_mark_handler)(Lv00LatexParser* p, const char* mark_type,
                             Lv00RenderNode** out);
};

// 解析器操作
Lv00LatexParser* lv00_latex_parser_create(void);
void lv00_latex_parser_register_geom(Lv00LatexParser* parser);

// 解析几何 LaTeX
Lv00RenderTree* lv00_geo_latex_parse(const char* latex, 
                                      Lv00LatexParser* parser);

// ============ 预定义几何符号 ============

// Unicode 数学符号
extern const char* LV00_GEO_SYMS[];
enum {
    LV00_SYM_PARALLEL = 0x2225,      // ∥
    LV00_SYM_PERP = 0x27C2,           // ⟂
    LV00_SYM_CONGRUENT = 0x2245,     // ≅
    LV00_SYM_SIMILAR = 0x223C,       // ∼
    LV00_SYM_ANGLE = 0x2220,         // ∠
    LV00_SYM_TRIANGLE = 0x25B3,      // △
    LV00_SYM_CIRCLE = 0x25CB,        // ○
    LV00_SYM_SQUARE = 0x25A1,        // □
    LV00_SYM_ARC = 0x23DA,           // ⏜
    LV00_SYM_ARROW_RIGHT = 0x2192,   // →
    LV00_SYM_ARROW_LEFT = 0x2190,   // ←
    LV00_SYM_ARROW_BOTH = 0x2194,   // ↔
};

// 清理
void lv00_latex_parser_destroy(Lv00LatexParser* parser);

#endif // LV00_GEO_LATEX_EXT_H
```

### 3.3 使用示例

```c
// 示例：渲染几何证明为 SVG

#include <lv00.h>
#include <lv00/geo_render_tree.h>
#include <lv00/geo_latex_ext.h>

void render_geometric_proof_svg(const char* output_path) {
    // 创建渲染器
    Lv00LatexParser* parser = lv00_latex_parser_create();
    lv00_latex_parser_register_geom(parser);
    
    // 几何证明 LaTeX（带几何标记）
    const char* proof_latex = 
        "\\text{Proof of Triangle Congruence (SAS)} \\\\["
        "\\text{Given: } \\triangle ABC \\text{ and } \\triangle DEF \\\\["
        "AB \\stackrel{\\parallel}{=} DE, \\quad "   // AB = DE
        "BC \\stackrel{\\parallel}{=} EF, \\quad "   // BC = EF \\\\"
        "\\angle ABC \\stackrel{\\perp}{=} \\angle DEF \\\\"  // ∠ABC = ∠DEF
        "\\text{Prove: } \\triangle ABC \\cong \\triangle DEF \\\\["
        "\\text{By SAS, } \\triangle ABC \\cong \\triangle DEF \\\\"
        "\\qed"
    ;
    
    // 解析为渲染树
    Lv00RenderTree* tree = lv00_geo_latex_parse(proof_latex, parser);
    
    // 测量尺寸
    lv00_rtree_measure(tree);
    
    // 渲染为 SVG
    char* svg = lv00_rtree_render_svg(tree, 1.5f);
    
    // 写入文件
    FILE* f = fopen(output_path, "w");
    fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\\n");
    fprintf(f, "%s", svg);
    fclose(f);
    
    // 清理
    free(svg);
    lv00_rtree_destroy(tree);
    lv00_latex_parser_destroy(parser);
}

// 示例：几何图形嵌入

void render_triangle_with_marks(void) {
    // 创建几何对象
    Lv00Point* A = lv00_point_create(0, 0);
    Lv00Point* B = lv00_point_create(4, 0);
    Lv00Point* C = lv00_point_create(2, 3);
    Lv00Triangle* ABC = lv00_triangle_create(A, B, C);
    
    // 创建标记（边长相等标记）
    Lv00RenderNode* AB_mark = lv00_rnode_geom_mark_create(
        lv00_term_segment(A, B),
        LV00_GEO_MARK_CONGRUENT
    );
    
    // 创建渲染树
    Lv00RenderTree* tree = lv00_rtree_create();
    
    // 根节点：三角形 + 标记
    Lv00RenderNode* triangle_geom = lv00_rnode_geom_create(ABC);
    Lv00RenderNode* marks = lv00_rnode_group_create(&AB_mark, 1);
    Lv00RenderNode* root = lv00_rnode_group_create(
        (Lv00RenderNode*[]){ triangle_geom, marks }, 2
    );
    
    lv00_rtree_set_root(tree, root);
    lv00_rtree_measure(tree);
    
    // 输出 SVG
    char* svg = lv00_rtree_render_svg(tree, 2.0f);
    printf("%s\\n", svg);
    
    free(svg);
    lv00_rtree_destroy(tree);
}
```

---

## 4. 实现路线图

### 4.1 分阶段实现计划

| 阶段 | 名称 | 时间 | 核心任务 | 交付物 | 优先级 |
|:---:|:---|:---:|:---|:---|:---:|
| 1 | 渲染树基础 | 第1-2周 | 节点类型、度量计算 | `geo_render_tree.h` (约400行) | P0 |
| 2 | SVG 输出 | 第3-4周 | SVG 生成器 | `render_svg.c` (约300行) | P0 |
| 3 | LaTeX 扩展 | 第5-6周 | 几何宏、命令解析 | `geo_latex_ext.h` (约250行) | P1 |
| 4 | 几何符号 | 第7-8周 | 几何标记、箭头、角度 | symbol_renderer.c | P1 |
| 5 | WASM 加速 | 第9-10周 | 渲染器 WASM 化 | render_wasm/ | P2 |

### 4.2 依赖关系

```
阶段1 (geo_render_tree.h)
    │
    ├── 依赖：无
    │
    ▼
阶段2 (render_svg.c)
    │
    ├── 依赖：阶段1
    │
    ▼
阶段3 (geo_latex_ext.h)
    │
    ├── 依赖：阶段1 + 阶段2
    │   └── 需要：LaTeX parser（可用 tinytex 或 wasm 版）
    │
    ▼
阶段4 (symbol_renderer.c)
    │
    ├── 依赖：阶段2
    │
    ▼
阶段5 (render_wasm/)
    │
    ├── 依赖：阶段1-4
    │   └── 需要：WASM 工具链（Emscripten）
    │
    ▼
完成
```

---

## 5. 附录

### 5.1 参考资源

- MathJax 官网：https://www.mathjax.org/
- MathJax GitHub：https://github.com/mathjax/MathJax
- KaTeX 官网：https://katex.org/
- KaTeX GitHub：https://github.com/KaTeX/KaTeX
- MathJax TeX Commands：https://docs.mathjax.org/en/latest/input/tex/macros/index.html

### 5.2 术语表

| 术语 | 英文 | 定义 |
|:---|:---|:---|
| TeX | TeX | Donald Knuth 开发的排版系统 |
| LaTeX | LaTeX | 基于 TeX 的文档准备系统 |
| MathML | Mathematical Markup Language | W3C 的数学标记语言 |
| SVG | Scalable Vector Graphics | 可缩放矢量图形 |
| WASM | WebAssembly | Web 原生二进制格式 |
| 渲染树 | Render Tree | 将文档结构映射到渲染格式 |

### 5.3 许可证兼容性

- MathJax 使用 Apache-2.0 许可证
- KaTeX 使用 MIT 许可证
- Lv-00 可自由参考和借鉴

---

*文档生成日期：2026-05-28*
*参考版本：MathJax 3.2+, KaTeX 0.16+*
