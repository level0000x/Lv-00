# Lv-00 参考设计：Manim 数学动画引擎

> **版本**: 1.0.0
> **日期**: 2026-05-24
> **参考**: [Manim](https://github.com/3b1b/manim) —— 3Blue1Brown 的声明式数学动画引擎
> **目标**: 借鉴 Manim 的"声明式数学对象 -> 动画编排"API，将 Lv-00 的证明展示转化为"证明步骤 -> 动画叙事"的编排方式，映射到 Web GUI 的 `ProofPanel` 和 `NarrativeExport`

---

## 目录

1. [项目概述](#1-项目概述)
2. [核心借鉴要点](#2-核心借鉴要点)
3. [Lv-00 映射方案](#3-lv-00-映射方案)
4. [实现路线图](#4-实现路线图)

---

## 1. 项目概述

### 1.1 Manim 是什么

Manim（Mathematical Animation Engine）是 Grant Sanderson（3Blue1Brown 频道作者）创建的声明式数学动画引擎。它允许用户通过 Python 代码描述数学对象和动画场景，自动生成高质量的数学讲解视频。Manim 的核心范式是**"声明式数学对象 + 动画编排链"**：

```python
# Manim 示例：展示勾股定理的视觉证明
from manim import *

class PythagoreanProof(Scene):
    def construct(self):
        # 1. 声明数学对象
        triangle = Polygon(
            [-2, -1, 0], [2, -1, 0], [0, 1.5, 0],
            color=WHITE
        )
        right_angle = RightAngle(
            triangle.get_vertices()[1],
            triangle.get_vertices()[0],
            triangle.get_vertices()[2],
        )
        squares = VGroup(
            Square(side_length=3).next_to(triangle, LEFT, buff=0),
            Square(side_length=4).next_to(triangle, RIGHT, buff=0),
            Square(side_length=5).next_to(triangle, DOWN, buff=0),
        )

        # 2. 动画编排（逐步展示证明）
        self.play(Create(triangle))
        self.wait(0.5)
        self.play(Create(right_angle))
        self.wait(0.3)
        for square in squares:
            self.play(Create(square), run_time=1)
        self.wait(0.5)
        self.play(squares.animate.set_opacity(0.5))
        self.wait(1)
```

Manim 的关键机制：

1. **声明式数学对象（Mobject）**：每个几何/数学对象是 Python 对象，携带位置、颜色、样式
2. **动画编排链（Animation）**：`Create`、`Transform`、`FadeIn/Out`、`Write` 等以声明式链式调用
3. **场景（Scene）**：一个场景 = 一组 Mobject + 一系列 Animation 的时间编排
4. **阶段间自然过渡**：从一个数学状态到下一个的动画过渡

### 1.2 为什么借鉴 Manim

Lv-00 的 `ProofPanel`（`web-gui/src/components/panels/ProofPanel.tsx`）和 `NarrativeExport`（`NarrativeExport.tsx`）已经实现了证明步骤的导航和文本叙述生成。但它们缺乏 Manim 式**视觉动画编排**能力。当前 Lv-00 的证明展示是静态的——用户看到的是证明步骤的文本列表，缺少视觉上的"逐步展示"动画。借鉴 Manim 意味着：

1. 每个证明步骤对应一个动画动作（`Create` 几何对象、`Highlight` 关键交点、`Transform` 辅助线等）
2. 场景编排器将 `ProofNavigator` 的步骤序列转化为动画时间线
3. 证明的"叙事性"从纯文本提升为视觉叙事（动画+文字+高亮）
4. 证明导出不只是 LaTeX/HTML，还包括动画视频或交互式 Web 动画

---

## 2. 核心借鉴要点

### 2.1 数学对象声明与动画编排的分离

| Manim 概念 | Lv-00 对应概念 | 映射说明 |
|-------------|---------------|---------|
| Mobject 声明（`Square()`, `Circle()`） | `GeomNode` + `ConstraintGraph` | 几何对象已在约束图中声明 |
| Mobject 属性（`color`, `opacity`, `position`） | `GeomNode` 的 `trust` 颜色 + 坐标 | 信任颜色本身就是视觉效果 |
| Animation（`Create`, `FadeIn`, `Transform`） | `ProofAnimation` 枚举 | 证明步骤映射为动画动作 |
| Scene 时间编排 | `AnimationTimeline` | 步骤序列转化为时间线 |
| `self.play(anim)` | `animation_player_play(anim)` | 播放单个动画 |
| `self.wait(seconds)` | `timeline_add_pause(duration)` | 暂停以让观众消化 |
| `VGroup`（对象组） | `FuncBlock` 或 `Region` | 逻辑上相关的对象组 |
| `Transform(obj1, obj2)` | 从步骤 N 的状态过渡到步骤 N+1 | 对象在证明过程中的变换 |

### 2.2 "证明步骤 -> 动画叙事"的映射表

| Lv-00 证明步骤类型 | Manim 式动画动作 | 视觉叙事效果 |
|-------------------|-----------------|-------------|
| `PROOF_STEP_ADD_NODE` | `Create(GeomNode)` | 几何对象从无到有出现 |
| `PROOF_STEP_ADD_CONSTRAINT` | `Indicate(Constraint)` | 约束以虚线/箭头闪烁标注 |
| `PROOF_STEP_REWRITE` | `Transform(old, new)` | 旧构造平滑变形为新构造 |
| `PROOF_STEP_FUNCTION_APP` | `ApplyFuncBlock(func, args)` | 函数块以"盒子加工"动画展示 |
| `PROOF_STEP_PACK_FUNCTION` | `SurroundRectangle(nodes)` + `FadeOutInto(fb)` | 包围选中节点，收入函数块 |
| `PROOF_STEP_NORMALIZATION` | `Flash(merged_nodes)` | 被合并的重复节点闪烁后消失 |
| `PROOF_STEP_UNIFY` | `MatchPattern(pattern, construction)` | 模式图以半透明叠加展示匹配 |
| `PROOF_STEP_EX_FALSO` | `ExplosionAnimation` + `CrossMark` | 矛盾以视觉冲击展示 |
| `PROOF_STEP_ORACLE` | `ExternalCallout(oracle_name)` | 外部求解器调用以标注显示 |

### 2.3 证明颜色系统的动画增强

| Lv-00 `ProofColor` | Manim 式视觉呈现 | 动画效果 |
|-------------------|-----------------|---------|
| `GREEN`（全构造） | 饱满绿色，实线边框 | `Create` 以绿色光晕出现 |
| `YELLOW`（条件性不可构造） | 黄色虚线边框 | `Create` 以虚线闪烁出现 |
| `ORANGE_ORACLE` | 橙色端口标记 + 感叹号图标 | 播放时 oracle 标注从外飞入 |
| `AMBER`（数值假设） | 琥珀色半透明填充 | 播放时假设文本浮动提示 |
| `DARK_ORANGE`（叠加） | 深橙色虚线 + 红点警告 | 播放时多次警告动画叠加 |

---

## 3. Lv-00 映射方案

### 3.1 证明动画类型定义

```c
/**
 * @brief 证明动画类型 —— Manim Animation 的 Lv-00 等价枚举
 *
 * 每个动画类型对应一种视觉叙事动作，将证明步骤从
 * 纯数据操作转化为观众可理解的视觉动画。
 */
typedef enum {
    PROOF_ANIM_CREATE,          /**< 创建：对象从无到有（Manim Create） */
    PROOF_ANIM_HIGHLIGHT,       /**< 高亮：闪烁/发光标注关键对象（Manim Indicate） */
    PROOF_ANIM_CONNECT,         /**< 连线：两点间画线（Manim Create + Line） */
    PROOF_ANIM_TRANSFORM,       /**< 变换：一个对象平滑变形（Manim Transform） */
    PROOF_ANIM_FADE_IN,         /**< 淡入：对象渐显（Manim FadeIn） */
    PROOF_ANIM_FADE_OUT,        /**< 淡出：对象渐隐（Manim FadeOut） */
    PROOF_ANIM_APPLY_FUNCTION,  /**< 应用函数块（Manim ApplyFunction） */
    PROOF_ANIM_PACK,            /**< 打包：选中节点被包围并入函数块 */
    PROOF_ANIM_MERGE,           /**< 合并：多个重复节点融合为一个 */
    PROOF_ANIM_MATCH_PATTERN,   /**< 匹配模式：半透明模式图叠加展示 */
    PROOF_ANIM_EXPLOSION,       /**< 爆炸：矛盾冲击视觉效果 */
    PROOF_ANIM_ORACLE_CALLBACK, /**< Oracle 回调标注 */
    PROOF_ANIM_PAUSE,           /**< 暂停（Manim wait） */
    PROOF_ANIM_TEXT_OVERLAY,    /**< 文字叠加：显示引理/注释文本 */
    PROOF_ANIM_ARROW,           /**< 箭头：指向关键交点/共线关系 */
    PROOF_ANIM_MEASURE,         /**< 测量标注：显示角度/距离数值 */
} ProofAnimationType;
```

### 3.2 动画时间线数据结构

```c
/**
 * @brief 单个证明动画帧
 *
 * 描述一个最小动画动作，包含目标对象、动画类型、
 * 持续时间和颜色信息。
 */
typedef struct ProofAnimation {
    int id;                       /**< 动画 ID */
    ProofAnimationType type;      /**< 动画类型 */
    int target_node_id;           /**< 目标几何节点 ID（可为 -1） */
    int target_constraint_id;     /**< 目标约束 ID（可为 -1） */
    int *related_node_ids;        /**< 关联节点 ID 数组 */
    int related_count;            /**< 关联节点数量 */
    double duration;              /**< 动画持续时间（秒） */
    double start_offset;          /**< 在时间线中的起始偏移（秒） */
    ProofColor color;             /**< 动画的颜色（从证明步骤继承） */
    char *narration_text;         /**< 旁白文本（英文） */
    char *narration_text_zh;      /**< 旁白文本（中文） */
    int proof_step_id;            /**< 对应的证明步骤 ID */
    bool is_parallel;             /**< 是否可与前一动画并行播放 */
} ProofAnimation;

/**
 * @brief 动画时间线 —— Manim Scene 的 Lv-00 等价
 *
 * 将 ProofNavigator 的步骤序列转化为可播放的动画时间线。
 * 时间线支持顺序动画、并行动画组、章节分割和播放速度控制。
 */
typedef struct AnimationTimeline {
    ProofAnimation **animations;  /**< 动画序列 */
    int animation_count;          /**< 动画数量 */
    double total_duration;        /**< 总时长（秒） */

    /* 场景分割 */
    char **chapter_titles;        /**< 章节标题 */
    double *chapter_offsets;      /**< 各章节起始时间偏移 */
    int chapter_count;            /**< 章节数量 */

    /* 证明元数据 */
    char *title;                  /**< 整个动画的标题 */
    char *description;            /**< 证明描述 */
    ProofColor final_color;       /**< 最终信任颜色 */

    /* 样式配置 */
    struct {
        char *background_color;   /**< 背景色（如 "#1a1a2e"） */
        double playback_speed;    /**< 播放速度倍率（默认 1.0） */
        bool auto_advance;        /**< 是否自动推进 */
        bool show_trust_colors;   /**< 是否显示信任颜色编码 */
    } config;
} AnimationTimeline;
```

### 3.3 从 ProofNavigator 到 AnimationTimeline 的编译

```c
/**
 * @brief 将证明导航器的步骤序列编译为动画时间线
 *
 * 这是"证明步骤 -> 动画叙事"的核心编译器。
 *
 * 编译过程：
 *  1. 遍历 ProofNavigator.steps[] 中的每个 ProofStep
 *  2. 根据 ProofStep.type 选择对应的 ProofAnimationType
 *  3. 从 ProofStep.node_id / constraint_id 等字段提取动画目标
 *  4. 从 ProofStep.dependency_step_ids 提取并行化信息
 *     （无依赖的步骤可以并行播放）
 *  5. 根据 ProofColor 设置动画的颜色/样式
 *  6. 生成旁白文本（narration_text）
 *  7. 为复合步骤（如 ADD_NODE + ADD_CONSTRAINT）生成多帧动画
 *  8. 检测自然章节边界（如"命题声明"->"辅助构造"->"证明结论"）
 *
 * @param[in]  navigator      证明导航器（含完整步骤序列）
 * @param[in]  language       叙述语言
 * @param[out] out_timeline   输出时间线
 * @return 成功返回 true
 */
bool animation_timeline_compile(const ProofNavigator *navigator,
                                 NarrativeLanguage language,
                                 AnimationTimeline **out_timeline);

/**
 * @brief 销毁动画时间线并释放所有资源
 */
void animation_timeline_destroy(AnimationTimeline *timeline);

/**
 * @brief 将动画时间线序列化为 JSON（用于 Web GUI 传输）
 *
 * JSON 格式的 AnimationTimeline 可以直接被 Web 前端的
 * 动画播放器（基于 requestAnimationFrame）加载和播放。
 */
char *animation_timeline_serialize_json(const AnimationTimeline *timeline);
```

### 3.4 动画播放器 API（Web GUI 前端）

```c
/**
 * @brief 证明动画播放器状态 —— 与 Web GUI ProofPanel 中的播放控件对应
 *
 * 播放器管理动画时间线的播放/暂停/跳转/速度控制。
 * Web GUI 端：React 组件 AnimationPlayer 封装以下逻辑。
 */
typedef struct AnimationPlayer {
    const AnimationTimeline *timeline;  /**< 当前加载的时间线 */
    int current_animation_index;        /**< 当前播放到的动画索引 */
    double elapsed_time;                /**< 已播放时间（秒） */
    double playback_speed;              /**< 播放速度倍率 */
    bool is_playing;                    /**< 是否正在播放 */
    bool is_looping;                    /**< 是否循环播放（调试模式） */
    bool is_reversed;                   /**< 是否反向播放 */

    /* 回调 */
    void (*on_animation_start)(int anim_index);  /**< 每个动画开始时回调 */
    void (*on_animation_end)(int anim_index);    /**< 每个动画结束时回调 */
    void (*on_chapter_change)(int chapter_index); /**< 章节切换时回调 */
    void (*on_timeline_end)(void);               /**< 时间线结束回调 */
} AnimationPlayer;
```

### 3.5 映射到 Web GUI 的 ProofPanel 和 NarrativeExport

| 现有组件 | 在动画叙事中的角色 | 新增能力 |
|---------|------------------|---------|
| `ProofPanel.tsx` | 动画播放器容器 + 步骤导航栏 | 增加播放/暂停/快进/后退按钮 + 时间线进度条 |
| `NarrativeExport.tsx` | 叙述文本生成 + SVG 导出 | 增加动画时间线导出（JSON -> 可播放格式） |
| `web/js/modules/proof.js` | 证明步骤的状态管理 | 扩展为支持动画帧管理 |
| `render.js`（渲染器） | 画布渲染 | 增加动画过渡（`requestAnimationFrame` 驱动的补间动画） |
| Canvas SVG 图层 | 静态几何图示 | 增加动画图层：高亮圈、箭头、淡入淡出 |
| 旁白文本面板 | 当前不存在 | 新增：时间线播放时同步展示旁白字幕 |
| `app.js`（主应用） | 全局状态 | 新增 `AnimationPlayer` 状态管理 |

### 3.6 证明动画与现有 ProofPanel 的整合

ProofPanel 现有的"证明步骤导航"功能是动画的基础。通过整合动画能力，ProofPanel 从一个"静态步骤列表"升级为"动态证明回放"：

```
现有 ProofPanel:
  [步骤1] -> [步骤2] -> [步骤3] -> ...
  用户手动点击"上一步/下一步"按钮查看每个步骤的静态快照

升级后 ProofPanel（Manim 风格）:
  [PLAY | PAUSE | ffwd | rwd]  [===进度条===]  [1x 速度]
  ┌──────────────────────────────────────────────────┐
  │  动画画布（Canvas + SVG 图层）                     │
  │    - 几何对象逐步创建/高亮/变换                    │
  │    - 信任颜色实时渲染（GREEN/YELLOW/AMBER/...）   │
  │    - 约束关系以虚线/箭头渐进展示                    │
  ├──────────────────────────────────────────────────┤
  │  旁白字幕: "我们首先构造三角形 ABC..."             │
  ├──────────────────────────────────────────────────┤
  │  步骤 3/12 | 章节: 辅助构造 | 颜色: GREEN          │
  └──────────────────────────────────────────────────┘
```

### 3.7 旁白文本的自动生成

借鉴 Manim 中 Grant Sanderson 的讲解风格，为每个证明步骤自动生成自然语言旁白。旁白可中英双语输出，通过模板 + 节点标签填充生成：

```
// 旁白模板示例
// -------------------------------
// ADD_NODE:
//   en: "We construct point {label}."
//   zh: "我们构造点{label}。"
//
// ADD_CONSTRAINT:
//   en: "{labelA} lies on {labelB}."
//   zh: "{labelA}位于{labelB}上。"
//
// UNIFY:
//   en: "The construction matches the proposition pattern."
//   zh: "构造与命题模式匹配。"
//
// EX_FALSO:
//   en: "This leads to a contradiction!"
//   zh: "这导致了矛盾！"
//
// PACK_FUNCTION:
//   en: "We encapsulate this construction as a reusable function block."
//   zh: "我们将此构造封装为一个可复用的函数块。"
```

---

## 4. 实现路线图

### 4.1 第一阶段：动画核心数据结构（P3）

| 任务 | 文件 | 说明 |
|------|------|------|
| 定义 `ProofAnimation`、`ProofAnimationType`、`AnimationTimeline` | `include/lv00/proof_animation.h`（新文件） | 动画系统的核心数据结构 |
| 实现 `animation_timeline_create/destroy` | `src/proof_animation.c`（新文件） | 时间线的创建与销毁 |
| 实现 `animation_timeline_compile()` | `src/proof_animation.c` | 从 ProofNavigator 编译时间线 |
| 实现 `animation_timeline_serialize_json()` | `src/proof_animation.c` | JSON 序列化供 Web GUI 使用 |

**预估规模**：约 300 行 C 代码

### 4.2 第二阶段：Web GUI 动画播放器（P3-P4）

| 任务 | 文件 | 说明 |
|------|------|------|
| 实现 `AnimationPlayer` React 组件 | `web-gui/src/components/animations/AnimationPlayer.tsx`（新文件） | 基于 `requestAnimationFrame` 的动画播放引擎 |
| 实现动画补间（Tween）系统 | `web-gui/src/utils/animationTweens.ts`（新文件） | 几何对象的位置/透明度/颜色的平滑过渡 |
| 扩展 `ProofPanel.tsx` 集成动画播放器 | `web-gui/src/components/panels/ProofPanel.tsx` | 在 ProofPanel 中嵌入 AnimationPlayer |
| 实现旁白字幕组件 | `web-gui/src/components/animations/NarrationSubtitle.tsx`（新文件） | 时间线播放时同步展示字幕 |
| 扩展画布渲染以支持动画图层 | `web/js/render.js` | 增加动画图层：高亮圈、箭头、淡入淡出效果 |

**预估规模**：约 500 行 TypeScript/JavaScript 代码

### 4.3 第三阶段：导出版本（P4）

| 任务 | 说明 |
|------|------|
| 动画时间线导出为 MP4/GIF | 通过服务端渲染 + FFmpeg 将时间线渲染为视频 |
| 动画时间线导出为可嵌入 HTML | 生成独立 HTML 文件，内嵌动画播放器和所有数据 |
| 交互式 Web 演示模式 | 全屏、自动播放、"演讲者视图"（Speaker Notes 显示旁白） |
| 与 Lv-00 已有的 `proof_export_html()` 集成 | HTML 导出不仅包含静态证明，还包含动画回放 |

---

## 附录 A：Manim 与 Lv-00 概念对照速查

| Manim | Lv-00 | 关键差异 |
|--------|-------|---------|
| Python 脚本描述场景 | C 头文件 + Web TypeScript | Lv-00 前端用 TypeScript/React 驱动动画 |
| `Mobject`（数学对象） | `GeomNode` + `SymbolicCoord` | Lv-00 对象天然携带代数信息 |
| `Scene.construct()` | `animation_timeline_compile()` | Lv-00 的"场景"从证明步骤自动编译 |
| `self.play(Create(obj))` | `animation_player_play(PROOF_ANIM_CREATE)` | Lv-00 动画由类型枚举而非类驱动 |
| `self.wait(seconds)` | `PROOF_ANIM_PAUSE` | 等价 |
| `Transform(a, b)` | `PROOF_ANIM_TRANSFORM` + 补间系统 | Lv-00 的变换基于坐标插值 |
| LaTeX 渲染 | `formula_renderer.js`（MathJax/KaTeX） | 已支持 |
| 视频编码（FFmpeg） | HTML5 Canvas + 可选的 FFmpeg 后处理 | Web 端优先 Canvas 实时渲染 |
| `VGroup` | `Region` 或 `FuncBlock` 数据联合体 | 概念等价 |

---

## 附录 B：完整动画示例——三角形中线共点证明

```
场景: 证明三角形 ABC 的三条中线共点于重心 G

动画时间线（共 12 帧，总时长 ~45 秒）:
─────────────────────────────────────────────────

章节 1: 命题声明（0-6s）
  Frame 01 [0.0s, CREATE]:   三角形 ABC 以绿色出现
  Frame 02 [1.5s, PAUSE]:    暂停 1.0s
  Frame 03 [2.5s, TEXT]:     叠加命题文本 "证明：三条中线交于一点"
  Frame 04 [4.0s, PAUSE]:    暂停 2.0s

章节 2: 辅助构造（6-18s）
  Frame 05 [6.0s, CREATE]:   中点 M_AB 出现在 AB 上
  Frame 06 [8.0s, CREATE]:   中点 M_BC 出现在 BC 上
  Frame 07 [10.0s, CREATE]:  中点 M_CA 出现在 CA 上
  Frame 08 [12.0s, CONNECT]: 画出中线 med_A (A -> M_BC)，绿色
  Frame 09 [14.0s, CONNECT]: 画出中线 med_B (B -> M_CA)，绿色
  Frame 10 [16.0s, CONNECT]: 画出中线 med_C (C -> M_AB)，绿色

章节 3: 证明结论（18-30s）
  Frame 11 [18.0s, HIGHLIGHT]: 重心 G 以金色高亮标记
  Frame 12 [20.0s, TEXT]:      叠加 "三条中线共点于重心 G"
  Frame 13 [22.0s, HIGHLIGHT]: 三条中线同时闪烁 — 强调共点
  Frame 14 [25.0s, PAUSE]:     暂停 5.0s（允许观众消化）

章节 4: 信任颜色与总结（30-45s）
  Frame 15 [30.0s, FADE_IN]:   整个证明画布边框变为 GREEN
  Frame 16 [32.0s, TEXT]:      "信任颜色: GREEN — 完全构造性"
  Frame 17 [35.0s, TEXT]:      "面积法证明 | 无 Oracle 依赖 | 无数值假设"
  Frame 18 [38.0s, PAUSE]:     结束暂停

旁白（中文）：
  步骤 01: "我们考虑任意三角形 ABC。"
  步骤 05: "构造边 AB 的中点，记为 M_AB。"
  步骤 08: "连接 A 与 M_BC，得到中线 med_A。"
  步骤 11: "三条中线的公共交点即为重心 G。"
  步骤 16: "该证明完全构造性，无任何非常规依赖，信任颜色为绿色。"
```

---

> **文档结束**
> 本文档详述了 Manim 的"声明式数学对象 -> 动画编排"API 如何应用于 Lv-00 的证明展示——将 `ProofNavigator` 的步骤序列自动编译为 `AnimationTimeline`，并在 Web GUI 的 `ProofPanel` 和 `NarrativeExport` 中以"逐步动画+旁白字幕+信任颜色"的 Manim 风格呈现。核心结论：通过将"证明步骤"映射为"动画帧"，并引入动画播放器、补间系统和章节分割，Lv-00 的证明展示从静态文本列表升级为沉浸式数学叙事体验，使观众能够像观看 3Blue1Brown 视频一样理解几何证明。
