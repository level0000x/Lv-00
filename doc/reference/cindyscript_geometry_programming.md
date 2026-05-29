# CindyScript 几何编程语言核心借鉴设计

> **借鉴项目**：CindyJS（github.com/CindyJS/CindyJS）
> **核心借鉴点**：CindyScript 几何编程语言、基于定理证明的几何判定（随机化定理验证）、编译到 WebGL 的渲染管道、Cinderella 物理仿真引擎
> **分类**：P3 中优先级 / 交互几何编程语言与 Web 渲染
> **日期**：2026-05-24

---

## 1. 概述

CindyJS 是 Cinderella 交互几何系统的 JavaScript 移植版，由 Jürgen Richter-Gebert 和 Ulrich Kortenkamp 领导的团队开发。Cinderella 项目始于 1998 年，最初是 Java Applet 形式的交互几何软件，CindyJS 则将整个系统编译到 Web 平台，依赖 WebGL 和现代浏览器实现高性能几何渲染。其核心设计哲学是"数学是探索的，而非静态的"——用户通过与几何图形的实时交互来发现数学性质，而非被动阅读定理。

CindyScript 是 CindyJS/Cinderella 内置的几何专用编程语言。它与通用编程语言（如 Python + Shapely 或 JavaScript + GeoGebra API）的根本区别在于：**变量本身就是几何元素，运算产生新的几何构造**。例如，`A = [0,0]; B = [3,0]; C = circle(A,2) ∩ circle(B,2)` 中，`A` 和 `B` 是点，`circle` 返回的是圆对象，`∩` 是几何相交运算。这种"一切都是几何元素"的设计消除了通用语言中对象构造与几何语义之间的阻抗不匹配。对 Lv-00 而言，CindyScript 展示了一种将几何 DSL 从"通用语言上的薄层包装"提升为"一等几何语言"的路径。

CindyJS 的另一项核心创新是**随机化定理验证（Randomized Theorem Checking, RTC）**。在交互式几何探索中，用户拖拽一个点时，系统在后台对数千个随机实例运行几何构造，检查不变量是否始终保持。虽然 RTC 不是形式化证明（它使用数值浮点运算），但其即时反馈（毫秒级）弥补了完全形式化证明的高延迟缺陷。对 Lv-00 而言，RTC 提供了一种"快速探索→迭代验证→最终符号证明"的渐进式证明工作流：用户先用数值随机验证快速筛选猜想，再切换到符号证明引擎确认。

物理仿真引擎方面，Cinderella 引入了"现实引擎"（Realitätsmechanik）概念：几何点按物理约束（如质量、弹簧、引力）运动，同时保持在几何约束（如点在圆上）上。这保证了几何图形的连续变形——拖动一个点时，整个构造平滑过渡，避免了符号求解中常见的分支切换问题。Lv-00 可以借鉴这种"物理约束 + 几何约束"的混合模型，为交互式几何探索提供平滑的视觉反馈，同时在符号引擎中精确求解。

---

## 2. 核心借鉴 → Lv-00 映射

### 2.1 CindyScript：几何专用编程语言的设计理念

CindyScript 的设计核心是"变量是几何元素，运算产生新几何构造"。这与通用语言加几何库（如 Python + SymPy 的 `Point(0,0)` + `Circle(Point(0,0), 2)`) 的根本差异体现在三个层面：

**层面一：类型即几何范畴。** CindyScript 的变量类型天然是几何概念（点、线、圆、圆锥曲线），而非通用语言的通用类型。`A = [0,0]` 不是创建一个"列表"，而是声明一个"具有坐标 (0,0) 的几何点"。

**层面二：运算即几何构造。** `join(A, B)` 创建过 A、B 的直线，`meet(l1, l2)` 求两条直线的交点。这些运算是几何语义的直接体现，而非通用语言的函数调用包装。

**层面三：构造图即程序。** 几何点的依赖关系（A → B → C，其中 C 依赖 A 和 B）天然形成了一棵构造树。CindyScript 的求值顺序自动按依赖拓扑排序，而非源代码的书写顺序。这与 Lv-00 的约束图拓扑排序完全对应。

**Lv-00 借鉴**：将 CindyScript 的"一切都是几何元素"哲学融入 Lv-00 DSL 的设计。Lv-00 的 DSL 不应是 C 语言宏的集合，而应是一种声明型的几何构造语言，其中变量类型由构造操作自动推断。

### 2.2 随机化定理验证（Randomized Theorem Checking）

RTC 是 CindyJS 在交互式探索中的核心机制。其工作流程为：

```
用户拖动点 P
  ↓
[事件循环]
  ├─ 重新计算所有受 P 影响的构造（构造树重求值）
  ├─ 生成 N 个随机实例（N ≈ 1000，对每个自由变量随机取值）
  ├─ 对每个实例评估目标不变量（如"三点共线"的面积为 0）
  ├─ 统计：满足比例 > 0.999 → "极有可能成立"
  ├─ 统计：0 < 比例 < 0.999 → "可能不成立"
  └─ 统计：比例 = 0 → "不成立"
  ↓
[视觉反馈]
  ├─ 绿色高亮：定理似乎成立
  ├─ 黄色警告：不确定
  └─ 红色提示：可能不成立
```

**数学基础**：RTC 基于 Schwartz-Zippel 引理——如果两个多项式不恒等，则随机取值不等的概率极高。对于几何命题，所有约束都可编码为多项式等式/不等式，因此 RTC 以高概率识别"不恒等"的约束。

**Lv-00 借鉴**：在 Lv-00 的探索模式中引入 RTC 作为第一道验证门：

| 验证阶段 | 方法 | 耗时 | 可信度 | 使用场景 |
|:---|:---|:---|:---|:---|
| 探索验证 | 随机化数值检查（RTC） | < 50ms | 概率性 | 用户拖拽交互 |
| 快速验证 | 代数求解（Groebner 基） | < 500ms | 确定性 | 猜想确认 |
| 完整证明 | 符号证明引擎 | 数秒~数分钟 | 完全严格 | 定理最终化 |

### 2.3 物理仿真引擎："现实引擎"（Realitätsmechanik）

Cinderella 的"现实引擎"解决了交互几何中的一个核心难题：**当用户拖动点 P 时，如何确定整个几何构造的连续变形？**

纯符号方法的问题：如果 P 从位置 (x1, y1) 拖到 (x2, y2)，符号求解器需要求解所有受约束点的新位置。当构造复杂时，可能出现多解（如两个圆的交点有两个），纯符号方法需要选择"哪个解"——选择错误会导致"跳变"，破坏连续性的感知。

Cinderella 的解决方案：将每个几何点建模为具有质量、速度和受力的物理粒子。约束（如"点 C 在圆上"）变为物理力（如弹簧力将点拉向圆周）。每次迭代：
1. 所有点按当前速度运动一小步
2. 对所有约束评估力的贡献
3. 更新速度
4. 重复直到平衡（约束满足）

这种"物理趋近"方法保证了几何变形的连续性——即使存在多个数学解，物理仿真始终收敛到"最接近当前配置"的解。

**Lv-00 借鉴**：

| Cinderella 现实引擎 | Lv-00 映射 | 说明 |
|:---|:---|:---|
| 几何点建模为物理粒子 | `GeoNode` + `physics_state` 字段 | 每个节点附加速度、质量等物理属性 |
| 约束建模为弹簧力 | `ConstraintEdge` + `force_response` 回调 | 每条约束边提供力计算公式 |
| 迭代平衡 | `physics_relax()` 函数 | 物理仿真循环，逐步趋近约束 |
| 连续性保证 | 始终从当前状态启动 | 避免符号求解器的分支切换 |

### 2.4 CindyScript 构造 → Lv-00 几何 DSL 映射

| CindyScript 构造 | 语义 | Lv-00 DSL 映射 | 说明 |
|:---|:---|:---|:---|
| `A = [0,0]` | 创建坐标为 (0,0) 的点 | `let A = point(0, 0)` | 自由点声明 |
| `B = [3,0]` | 创建坐标为 (3,0) 的点 | `let B = point(3, 0)` | 自由点声明 |
| `l = join(A, B)` | 过 A、B 的直线 | `let l = line(A, B)` | 直线构造 |
| `c = circle(A, 2)` | 以 A 为心半径 2 的圆 | `let c = circle(A, 2)` | 圆构造 |
| `C = meet(c1, c2)` | 两圆的交点 | `let C = intersect(c1, c2, +)` | 交点构造（`+` 选上交点） |
| `m = midpoint(A, B)` | A、B 的中点 | `let m = midpoint(A, B)` | 中点构造 |
| `para = parallel(l, A)` | 过 A 平行于 l 的直线 | `let para = parallel(l, A)` | 平行线构造 |
| `perp = perpendicular(l, A)` | 过 A 垂直于 l 的直线 | `let perp = perpendicular(l, A)` | 垂线构造 |
| `ang = angle(A, B, C)` | 角 ABC 的度数 | `let ang = angle(A, B, C)` | 角度量算 |
| `dist = |A, B|` | A、B 之间的距离 | `let dist = distance(A, B)` | 距离量算 |
| `if(oncircle(P, c), ...)` | 检查 P 是否在圆 c 上 | `assert on_circle(P, c)` | 断言/条件检查 |
| `draw(P, color->[1,0,0])` | 绘制红色点 P | `render(P, color=red)` | 渲染指令 |

### 2.5 代码示例：借鉴 CindyScript 风格的 Lv-00 交互式几何编程接口

```c
/**
 * @brief Lv-00 交互式几何 DSL —— 借鉴 CindyScript 的"一切都是几何元素"设计
 *
 * 以下 DSL 代码演示 Lv-00 用户如何以声明型语法描述几何构造：
 * 构造三角形的外接圆并验证外心位置。
 */

/*
 * ── Lv-00 几何 DSL 示例：三角形的外心构造 ──
 *
 * // 1. 声明自由点
 * let A = point(0, 0);
 * let B = point(4, 0);
 * let C = point(1, 3);
 *
 * // 2. 构造三角形（三条边）
 * let tri = triangle(A, B, C);
 *
 * // 3. 构造两边的中垂线
 * let ab_perp = perpendicular_bisector(A, B);
 * let ac_perp = perpendicular_bisector(A, C);
 *
 * // 4. 求中垂线交点——外心 O
 * let O = intersect(ab_perp, ac_perp);
 *
 * // 5. 验证：O 到三顶点距离相等
 * assert equal(distance(O, A), distance(O, B));
 * assert equal(distance(O, A), distance(O, C));
 *
 * // 6. 渲染整个构造
 * render(tri, style="edges:black, fill:lightblue, alpha:0.3");
 * render(O, style="color:red, size:8, label:'O'");
 */

/**
 * @brief CindyScript 风格的 Lv-00 DSL 解析器核心结构
 *
 * 每个 CindyScript 构造语句映射为 Lv-00 的一个 `GeoConstruct`，
 * 在约束图中创建 `GeoNode` 节点和 `ConstraintEdge` 依赖边。
 */
typedef enum {
    GEO_CONSTRUCT_POINT,             /**< point(x, y) — 创建自由点 */
    GEO_CONSTRUCT_LINE,              /**< line(A, B) — 过两点的直线 */
    GEO_CONSTRUCT_CIRCLE,            /**< circle(center, radius) — 圆 */
    GEO_CONSTRUCT_INTERSECT,         /**< intersect(a, b, branch) — 交点 */
    GEO_CONSTRUCT_MIDPOINT,          /**< midpoint(A, B) — 中点 */
    GEO_CONSTRUCT_PARALLEL,          /**< parallel(line, point) — 平行线 */
    GEO_CONSTRUCT_PERPENDICULAR,     /**< perpendicular(line, point) — 垂线 */
    GEO_CONSTRUCT_PERPENDICULAR_BISECTOR, /**< perpendicular_bisector(A, B) — 中垂线 */
    GEO_CONSTRUCT_ANGLE_BISECTOR,    /**< angle_bisector(A, B, C) — 角平分线 */
    GEO_CONSTRUCT_REFLECTION,        /**< reflect(point, line) — 反射点 */
    GEO_CONSTRUCT_CIRCUMCIRCLE,      /**< circumcircle(A, B, C) — 外接圆 */
    GEO_CONSTRUCT_INCIRCLE,          /**< incircle(A, B, C) — 内切圆 */
    GEO_CONSTRUCT_LOCUS,             /**< locus(tracer, mover) — 轨迹 */
} GeoConstructKind;

/**
 * @brief 单个几何构造步骤
 *
 * 对应 CindyScript 中的一条构造语句。
 */
typedef struct {
    GeoConstructKind kind;           /**< 构造类型 */
    int *source_ids;                 /**< 源节点 ID 列表 */
    int source_count;                /**< 源节点数量 */
    int output_id;                   /**< 输出节点 ID（分配到约束图）*/
    double *numeric_params;          /**< 数值参数（如坐标、半径）*/
    int param_count;                 /**< 参数数量 */
    char *label;                     /**< 用户标签 */
} GeoConstruct;

/**
 * @brief 解析 CindyScript 风格的 Lv-00 DSL 并构建约束图
 *
 * 工作流程：
 *  1. 词法分析：识别 let/point/line/circle/intersect 等关键字
 *  2. 语法分析：构建 GeoConstruct 序列（拓扑排序）
 *  3. 约束图构建：每个 GeoConstruct 转化为 GeoNode + ConstraintEdge
 *  4. 类型推导：自动为每个节点分配 TypeRegion
 *  5. 返回可运行的约束图句柄
 *
 * @param dsl_source    DSL 源代码字符串
 * @param graph_out     输出：构建的约束图
 * @param err_msg       错误信息（失败时）
 * @return 成功返回 0，失败返回错误码
 */
int lv00_dsl_parse_cindyscript(
    const char *dsl_source,
    ConstraintGraph **graph_out,
    char **err_msg
);

/**
 * @brief 随机化定理验证器 —— 借鉴 CindyJS 的 RTC
 *
 * 对几何构造图中的目标不变量执行随机化数值验证。
 * 生成 N 个随机实例（对每个自由变量随机取值），
 * 在数值误差 epsilon 内检查不变量是否保持。
 *
 * @param graph          约束图
 * @param invariant_ids  要验证的不变量（命题）ID 数组
 * @param invariant_count 不变量数量
 * @param sample_count   随机采样次数（建议 1000-10000）
 * @param epsilon        数值误差容限
 * @param out_report     输出：验证报告
 * @return 验证结果
 */
RTCResult lv00_randomized_theorem_check(
    const ConstraintGraph *graph,
    const int *invariant_ids,
    int invariant_count,
    int sample_count,
    double epsilon,
    RTCReport *out_report
);
```

### 2.6 数值随机验证 vs 符号精确证明：两种范式的互补策略

CindyJS 的数值+随机化验证范式与 Lv-00 的符号精确证明范式并非竞争关系，而是**互补**关系：

| 维度 | CindyJS RTC | Lv-00 符号证明 |
|:---|:---|:---|
| **理论基础** | Schwartz-Zippel 引理（概率正确） | 代数几何 / Hilbert 零点定理（完全正确） |
| **验证速度** | 毫秒级（1000 次采样） | 数秒至数分钟（代数求解） |
| **假阳性率** | 极低但非零（~epsilon^d，d=多项式次数） | 零 |
| **适用阶段** | 探索猜想、交互反馈 | 定理确认、形式化输出 |
| **适用用户** | 学生探索、教师演示 | 研究者验证、形式化发布 |
| **计算机要求** | 浏览器内即可运行 | 可能需要求解器后端 |

**Lv-00 中的互补工作流**：

```
用户操作：拖动点 P，观察"内切圆是否存在"
  ↓
[RTC 快速验证 — < 50ms]
  ├─ 生成 1000 个随机三角形实例
  ├─ 检查每个实例：三内角平分线是否交于一点
  ├─ 1000/1000 通过 → 绿色高亮："极有可能成立"
  └─ 触发用户好奇心："为什么成立？我想验证"
  ↓
[符号证明 — 用户按下"证明"按钮]
  ├─ 编码角平分线约束为代数方程
  ├─ 使用面积法/Groebner 基证明交点唯一
  ├─ 输出证明步骤（可检查的形式化证明）
  └─ 标记为"已证明"
```

---

## 3. 实现方案

### 3.1 第一阶段：Lv-00 几何 DSL 核心语法（P3-1）

- [ ] 设计 Lv-00 几何 DSL 的完整语法（BNF 定义）
  - 自由点声明：`let A = point(x, y)`
  - 构造表达式：`line(A, B)`, `circle(A, r)`, `intersect(a, b, ±)`, `midpoint(A, B)`, `perpendicular(l, A)`
  - 断言：`assert equal(expr1, expr2)`, `assert on_circle(P, c)`
  - 渲染：`render(obj, style="...")`
- [ ] 实现 DSL 词法分析器（识别关键字 + 标识符 + 数值 + 运算符）
- [ ] 实现 DSL 语法分析器（递归下降解析器）
- [ ] 实现 DSL → GeoConstruct 中间表示转换
- [ ] 编写 10+ 个典型几何构造的 DSL 示例

### 3.2 第二阶段：随机化定理验证引擎（P3-2）

- [ ] 实现自由变量的随机实例生成器
  - 均匀分布在边界框内（默认 [-100, 100] × [-100, 100]）
  - 支持非退化条件过滤（三点不共线、四点不全等距等）
- [ ] 实现约束图的数值求值引擎
  - 按拓扑排序遍历节点
  - 每个 GeoConstruct 提供数值求值回调
  - 支持浮点误差传播管理
- [ ] 实现 RTC 统计引擎
  - 计数满足/不满足的采样
  - 计算置信区间
  - 输出可读报告
- [ ] 实现 RTC 与 UI 的集成
  - 颜色编码反馈（绿/黄/红）
  - 实时更新（每次鼠标拖动触发）

### 3.3 第三阶段：物理仿真引擎（P3-3）

- [ ] 实现 GeoNode 的物理状态扩展
  - 位置、速度、质量、阻尼
- [ ] 实现约束力的计算模型
  - 距离约束 → 弹簧力（F = -k * (d - d_target)）
  - 共线约束 → 投影力
  - 角度约束 → 扭矩
- [ ] 实现 Verlet 积分迭代求解器
  - 固定时间步长 dt
  - 速度 Verlet 或蛙跳积分
- [ ] 实现物理引擎与符号引擎的切换
  - 探索模式：物理引擎（连续平滑变形）
  - 证明模式：符号引擎（精确求解）
  - 切换时保留状态（物理结果作为符号求解的初始猜测）

### 3.4 第四阶段：WebGL 渲染管道（P3-4）

- [ ] 设计几何元素 → WebGL 着色器的编译管道
  - 点 → GL_POINTS + 顶点着色器（大小、颜色）
  - 线段 → GL_LINES + 几何着色器（宽度、虚线样式）
  - 圆 → 细分三角形条带 + 片段着色器
  - 文本标签 → 纹理映射四边形
- [ ] 实现增量渲染（仅重绘变动的元素）
- [ ] 实现抗锯齿和亚像素精度渲染
- [ ] 集成到 Lv-00 可视化前端

### 3.5 第五阶段：DSL 编辑器与实时反馈（P3-5）

- [ ] 实现基于 CodeMirror/ACE 的 DSL 编辑器组件
- [ ] 实现实时解析（每次按键触发增量解析）
- [ ] 实现实时 RTC（每次构造变动触发验证）
- [ ] 实现错误定位（语法错误 → 行号高亮，语义错误 → 节点高亮）

---

## 4. 设计决策与权衡

### 4.1 DSL 设计：声明型 vs 指令型

CindyScript 是声明型的——语句描述的是几何关系，而非计算步骤。用户写 `C = meet(c1, c2)` 而不是 `C = solve(circle_intersection(A1, r1, A2, r2))`。Lv-00 的 DSL 应保持这种声明风格，原因：
- 声明型 DSL 的语法树天然映射到约束图
- 构造的依赖关系从语法树直接提取（无需数据流分析）
- 用户意图更清晰（"交"vs"计算交点"）

### 4.2 RTC 的数值精度边界

RTC 使用浮点运算，面临数值精度问题：
- 两个极近的平行线可能被误判为相交
- 极小的面积值在阈值附近震荡
- 高次多项式的数值不稳定

缓解策略：
- 使用自适应 epsilon（基于构造的数值敏感性调整）
- 对接近退化的配置发出警告而非判定
- RTC 仅用作探索辅助，符号证明始终是最终仲裁者

### 4.3 物理引擎 vs 代数求解器的边界

物理仿真对于连续变形是理想的，但对于精确几何关系判定则不适用：
- 物理仿真（Verlet 积分）有漂移误差，长时间运行后点可能偏离约束表面
- 代数求解保证约束精确满足

建议的使用边界：
- **物理引擎**：仅用于 UI 交互（拖拽变形、动画）
- **代数求解器**：用于所有需要精确判断的场景（命题验证、证明生成）
- 两者之间保持单向数据流：物理引擎的最终状态 → 代数求解器作为初始猜测

### 4.4 CindyScript 语法 vs 新语法设计

是直接采用 CindyScript 语法还是设计全新的 Lv-00 语法？

直接采用的优势：用户熟悉，文档可参考，现有 Cinderella 教育资源可直接使用。
新设计的优势：可更好集成 Lv-00 特有的概念（如精化类型、Ghost 节点）。

建议方案：**CindyScript 语法 + Lv-00 扩展**。核心几何构造语法兼容 CindyJS，额外增加：
- `ghost A = midpoint(B, C)` — Ghost 辅助构造
- `prove @theorem_name` — 触发证明
- `refine TYPE{CONSTRAINT}` — 精化类型声明

---

## 5. 参考资源

- CindyJS 官方仓库：github.com/CindyJS/CindyJS
- Cinderella 官网及文档：cinderella.de
- CindyScript 参考手册：cinderella.de/tiki-index.php?page=CindyScript
- Richter-Gebert & Kortenkamp,《The Interactive Geometry Software Cinderella》(1999), Springer
- Richter-Gebert,《Realization Spaces of Polytopes》(1996) —— 随机化定理验证的数学基础
- Schwartz-Zippel 引理在几何证明中的应用：Tulone et al., "Randomized theorem proving in geometry"
- CindyJS WebGL 渲染管道：github.com/CindyJS/CindyJS/wiki/Rendering-Pipeline

---

## 6. 总结

CindyJS 和 CindyScript 为 Lv-00 提供了三个层面上的借鉴价值。在语言层面，CindyScript 的"一切都是几何元素"设计哲学启发 Lv-00 DSL 从"C 语言宏集合"升级为"一等几何编程语言"，使变量类型天然对应几何范畴、运算天然对应几何构造。在验证层面，随机化定理验证（RTC）为 Lv-00 提供了一条"快速探索→迭代验证→符号证明"的渐进式证明工作流，以数值概率验证的即时性弥补符号证明的高延迟。在交互层面，Cinderella 的"现实引擎"物理仿真解决了连续变形中的分支切换问题，使 Lv-00 的交互几何探索获得平滑的用户体验。这三项借鉴共同构成 Lv-00 在"交互几何编程语言与 Web 渲染"方向上的设计基础。
