# Lv-00 参考设计：Three.js WebGL/WebGPU 3D 渲染管道与场景图架构

> **版本**: 1.0.0
> **日期**: 2026-05-24
> **参考**: [three.js](https://github.com/mrdoob/three.js) —— JavaScript WebGL/WebGPU 3D 渲染库，MIT 许可，110k+ stars
> **目标**: 借鉴 Three.js 的场景图架构、高性能缓冲区几何体、数学库和射线拾取机制，指导 Lv-00 Web GUI 的 GeomNode 树形可视化、几何体批量渲染和交互选择系统的设计

---

## 目录

1. [项目概述](#1-项目概述)
2. [核心借鉴要点与对照表](#2-核心借鉴要点与对照表)
3. [Lv-00 映射方案与代码示例](#3-lv-00-映射方案与代码示例)
4. [实现路线图](#4-实现路线图)
5. [附录](#5-附录)

---

## 1. 项目概述

### 1.1 Three.js 是什么

Three.js 是由 Ricardo Cabello（mrdoob）于 2010 年发起的开源 JavaScript 3D 渲染库，目前是 Web 3D 领域的事实标准，在 GitHub 上拥有超过 110,000 颗星。Three.js 的核心理念是：**为 Web 提供一个轻量级、易用且功能完备的 3D 渲染引擎**，在浏览器中封装 WebGL 和 WebGPU 的底层复杂性。

Three.js 的关键设计决策：

1. **场景图（Scene Graph）架构**：所有 3D 对象组织在一个树形结构中。`Scene` 是根节点，`Object3D` 是树中每个节点的基类。子节点继承父节点的变换（位置、旋转、缩放），形成层级变换链。这种设计使得复杂的 3D 场景可以通过组合简单的节点来构建。

2. **双后端渲染器**：Three.js 提供 `WebGLRenderer`（成熟稳定，覆盖最广）和 `WebGPURenderer`（新一代，更高性能）两个渲染后端。两者共享相同的场景图 API，用户可以无痛切换。WebGPURenderer 额外支持计算着色器（Compute Shader），可在 GPU 上执行通用并行计算。

3. **BufferGeometry 高性能顶点存储**：Three.js 使用类型化数组（`Float32Array`、`Uint32Array`）存储顶点数据（位置、法线、UV、索引），直接映射到 GPU 缓冲区，避免每帧的 JavaScript 对象分配和 GC 压力。这是 Three.js 性能优于早期 `Geometry` 类（已废弃）的关键设计。

4. **模块化 ESM 架构**：Three.js 完全采用 ECMAScript Modules（`import`/`export`）组织代码。核心在 `three` 包中，扩展（如 OrbitControls、GLTFLoader、PostProcessing）放在 `three/examples/jsm/` 中。用户只需导入需要的模块，Tree-shaking 自动剔除未使用代码。

5. **数学库体系**：`Matrix4`（4x4 变换矩阵）、`Quaternion`（四元数旋转）、`Vector3`/`Vector2`（向量）、`Euler`（欧拉角）、`Raycaster`（射线）构成完整的几何计算基础设施。

```
// Three.js 最小示例：创建旋转的立方体
import * as THREE from 'three';

// 1. 场景（场景图的根）
const scene = new THREE.Scene();

// 2. 相机
const camera = new THREE.PerspectiveCamera(75, width/height, 0.1, 1000);
camera.position.z = 5;

// 3. 渲染器（WebGL 后端）
const renderer = new THREE.WebGLRenderer({ antialias: true });
renderer.setSize(width, height);
document.body.appendChild(renderer.domElement);

// 4. 几何体 + 材质 → 网格（场景图节点）
const geometry = new THREE.BoxGeometry(1, 1, 1);
const material = new THREE.MeshStandardMaterial({ color: 0x00ff00 });
const cube = new THREE.Mesh(geometry, material);
scene.add(cube);

// 5. 渲染循环
function animate() {
    requestAnimationFrame(animate);
    cube.rotation.x += 0.01;
    cube.rotation.y += 0.01;
    renderer.render(scene, camera);
}
animate();
```

### 1.2 为什么借鉴 Three.js

Lv-00 的 Web GUI 层需要对几何对象进行 3D 可视化渲染和交互操作。当前 Web GUI 在以下方面存在空白：

- **几何对象可视化**：从 DSL 解析出的几何构造（点、线、圆、多边形、3D 体）需要在浏览器中渲染，但目前缺乏统一的渲染管线。
- **层级变换链**：GeomNode 之间的几何关系（如"C 是 A、B 的中点"）天然形成依赖树，但尚未映射到可视化变换层级。
- **交互拾取**：用户点击几何对象的交互选择需要高效的射线-几何体相交检测。
- **性能**：复杂几何场景（如有理曲线、曲面细分）可能包含大量顶点数据，需要批量渲染优化。
- **计算加速**：WebGL/WebGPU 的计算着色器可用于加速几何约束求解。

Three.js 作为 Web 3D 的事实标准，其架构设计直接适用于 Lv-00 的 Web 前端需求。

---

## 2. 核心借鉴要点与对照表

### 2.1 Scene Graph 场景树架构

Three.js 的场景图是所有 3D 对象的组织结构基础。每个 `Object3D` 节点包含 `position`、`rotation`（四元数）、`scale` 属性和 `children` 数组。渲染时，引擎从根 `Scene` 出发深度遍历，将每个节点自身的局部变换与其父节点的世界变换相乘，形成世界矩阵。

| Three.js 概念 | 职责 | Lv-00 对应 | 关键差异 |
|--------------|------|-----------|---------|
| `Scene` | 场景根节点，持有所有可见对象、灯光、雾 | `GeomNodeTree` 根节点 | Lv-00 的根是"画布"本身 |
| `Object3D` | 所有 3D 对象的基类，提供 `position`/`rotation`/`scale`/`children` | `GeomNode` 基类 | Lv-00 的节点还携带构造约束 |
| `Object3D.matrixWorld` | 世界空间变换矩阵（自动从父-子链计算） | `GeomNode.world_transform` | Lv-00 的变换受约束图驱动 |
| `Object3D.children` | 子节点数组 | `GeomNode.children` | 结构一致 |
| `Scene.add(obj)` | 将对象添加到场景 | `geom_node_add_child(parent, child)` | 调用形式不同 |
| `traverse(callback)` | 深度优先遍历所有后代节点 | `geom_node_traverse(node, callback)` | 语义相同 |

Lv-00 的场景图与 Three.js 的关键区别在于：Three.js 的变换由用户显式设置（如 `cube.position.x = 1`），而 Lv-00 的变换由**约束图**推导——例如"点 C 是 A、B 的中点"意味着 C 的位置由 A、B 计算得出，而非直接设置。这要求 Lv-00 的场景图节点需要感知其"构造约束"和"自由参数"。

### 2.2 BufferGeometry 高性能顶点数据存储

Three.js 的 `BufferGeometry` 将几何数据（顶点位置、索引、法线、UV 坐标等）存储在 `BufferAttribute` 中，每个 `BufferAttribute` 底层是一个 `Float32Array` 或 `Uint32Array` 或 `Uint16Array` 等类型化数组。这些数据直接通过 `gl.bufferData()` 上传到 GPU，避免每帧的中间转换。

| Three.js 概念 | 职责 | Lv-00 对应 |
|--------------|------|-----------|
| `BufferGeometry` | 几何数据容器 | `GeomVertexBuffer` |
| `BufferAttribute` | 单属性数组（position、normal、uv） | `VertexAttribute` |
| `Float32Array` | 底层存储（直接映射 GPU） | `float[]` / `vec3[]` |
| `geometry.index` | 索引数组（三角剖分） | `index_buffer` |
| `geometry.setAttribute(name, attr)` | 动态添加属性 | `vertex_buffer_set_attr(buf, name, data)` |
| `geometry.computeBoundingSphere()` | 计算包围球（用于视锥体剔除） | `compute_bounding_box(buf)` |

Lv-00 需要批量渲染大量几何原语，例如：
- 曲线上的采样点（数百个）
- 曲面的三角剖分（数千到数万个三角形）
- 约束图中的所有几何对象叠加渲染

每次渲染时逐个对象调用 WebGL draw call 是不可行的。借鉴 BufferGeometry，Lv-00 应将同类型的几何原语合并到单个缓冲区中一次提交。

### 2.3 Matrix4 / Quaternion 数学库

Three.js 的数学库是所有几何变换的基石。关键类包括：

| 类 | 功能 | Lv-00 对应实现 |
|---|------|--------------|
| `Matrix4` (4x4) | 仿射变换（平移+旋转+缩放+投影） | `mat4` (Web C 端) |
| `Quaternion` | 旋转表示（无万向锁） | `quat` |
| `Vector3` | 三维向量 | `vec3` |
| `Euler` | 欧拉角（与四元数互转） | `euler_to_quat()` |
| `Matrix4.compose()` | 从 position+quaternion+scale 构建矩阵 | `mat4_compose()` |
| `Matrix4.decompose()` | 从矩阵提取 position+quaternion+scale | `mat4_decompose()` |
| `Matrix4.multiplyMatrices(a, b)` | 矩阵乘法 | `mat4_mul(out, a, b)` |
| `Matrix4.invert()` | 逆矩阵 | `mat4_inv()` |

Three.js 的数学库在 Web 端用纯 JavaScript 实现，利用 JIT 优化达到可接受的性能。Lv-00 的数学运算在 C 内核中完成（利用 Eigen 的 SIMD 优化），Web 前端通过 WASM 调用获取结果，或使用 `gl-matrix` 库纯 JS 实现轻量操作。

### 2.4 Raycaster 射线拾取

Three.js 的 `Raycaster` 提供基于射线的 3D 对象拾取机制。核心流程：
1. 从屏幕坐标和相机矩阵生成射线（Ray）
2. 对场景中每个可见对象，检测射线与其包围盒/包围球是否相交（快速剔除）
3. 对通过包围盒测试的对象，执行精确的射线-三角面相交测试
4. 返回所有交点，按距离排序

| Raycaster 步骤 | Three.js 实现 | Lv-00 对应 |
|---------------|--------------|-----------|
| 射线生成 | `raycaster.setFromCamera(mouse, camera)` | `ray_from_screen(x, y, camera)` |
| 包围盒快速剔除 | `geometry.boundingSphere.intersectsRay(ray)` | `bounding_box_ray_test(box, ray)` |
| 精确三角测试 | Moller-Trumbore 算法 | 与 Three.js 相同算法 |
| 结果排序 | 按 `distance` 升序 | 统一接口 |
| 回调/事件 | `intersectObjects(objects)` 返回数组 | `geom_pick(ray)` 返回 `GeomNode*` |

Lv-00 的射线拾取还有一个额外需求：不仅需要检测射线与渲染三角形的相交，还需要检测与**隐式几何对象**的相交——例如"以 A 为圆心、AB 为半径的圆"需要射线-圆相交测试。这要求拾取系统能分发到不同类型的几何体检测器。

### 2.5 WebGPURenderer 计算着色器

Three.js 的 `WebGPURenderer` 是下一代渲染后端，其关键新增能力是**计算着色器（Compute Shader）**——利用 GPU 大规模并行计算能力执行通用数值计算。

对于 Lv-00，计算着色器可在 Web 端加速：
- 几何约束求解（如大规模线性系统的 Gauss-Seidel 迭代）
- 曲线/曲面采样（并行计算大量采样点）
- 符号坐标的数值近似（多项式求根）

但 WebGPU 在浏览器中的支持率仍需关注——截至 2026 年，Chrome、Edge 已完整支持，Firefox 和 Safari 仍为实验性。因此 Lv-00 应设计双路径：WebGPU 可用时走计算着色器加速，不可用时回退到 CPU/wasm。

### 2.6 模块化 ESM 架构

Three.js 的代码组织遵循"核心精简 + 扩展可选"原则：

```
three/
  ├── build/three.module.js    ← ESM 入口
  ├── src/
  │   ├── Three.js             ← 命名空间导出
  │   ├── math/                ← 数学库（Vector3、Matrix4、Quaternion...）
  │   ├── core/                ← 核心（Object3D、BufferGeometry、Raycaster...）
  │   ├── renderers/           ← 渲染器（WebGL、WebGPU、CSS...）
  │   ├── materials/           ← 材质系统
  │   ├── lights/              ← 光源
  │   └── cameras/             ← 相机
  └── examples/jsm/            ← 可选扩展（控件、加载器、后处理...）
```

Lv-00 Web GUI 可借鉴此结构：

```
lv00-web/
  ├── src/
  │   ├── index.js             ← 主入口
  │   ├── core/                ← 核心模块
  │   │   ├── GeomNode.js      ← 场景图节点
  │   │   ├── GeomScene.js     ← 场景管理
  │   │   └── GeomCamera.js    ← 相机控制
  │   ├── renderer/            ← 渲染器
  │   │   ├── GeomRenderer.js  ← 渲染器基类/工厂
  │   │   ├── WebGLBackend.js  ← WebGL 实现
  │   │   └── WebGPUBackend.js ← WebGPU 实现（可选）
  │   ├── math/                ← 数学库
  │   │   ├── vec3.js
  │   │   ├── mat4.js
  │   │   └── quat.js
  │   ├── interaction/         ← 交互
  │   │   ├── OrbitControl.js  ← 轨道旋转/缩放
  │   │   ├── RayPicker.js     ← 射线拾取
  │   │   └── Gizmo.js         ← 变换操作手柄
  │   └── wasm/                ← WASM 桥接
  │       └── lv00_bridge.js   ← C 内核调用封装
  └── examples/                ← 示例
```

### 2.7 核心借鉴要点总对照表

| 序号 | 借鉴点 | Three.js 来源 | Lv-00 目标模块 | 借鉴深度 | 优先级 |
|------|--------|-------------|---------------|---------|--------|
| 1 | Scene Graph 场景树 | `Object3D` + `Scene` | `GeomNode` 树形可视化组织 | 架构级 | P3 |
| 2 | BufferGeometry 批量渲染 | `BufferGeometry` + `BufferAttribute` | 几何体坐标数据批量渲染 | 架构级 | P3 |
| 3 | Matrix4/Quaternion 数学库 | `math/` 子包 | 几何变换 Web 端实现 | 实现级 | P2 |
| 4 | Raycaster 交互拾取 | `Raycaster` 类 | 几何对象点击选择 | 实现级 | P4 |
| 5 | WebGPU 计算着色器 | `WebGPURenderer` | Web 端计算着色器几何加速 | 架构级 | P5 |
| 6 | ESM 模块化组织 | `src/` + `examples/jsm/` | Web GUI 模块化设计 | 架构级 | P3 |

---

## 3. Lv-00 映射方案与代码示例

### 3.1 GeomNode 场景图节点设计

借鉴 Three.js 的 `Object3D`，设计 GeomNode 作为 Lv-00 可视化场景树的节点基类。每个 GeomNode 对应 DSL 中的一个几何对象（点、线、圆、多边形等），并维护与约束图节点的双向映射。

```c
/**
 * @file include/lv00/geom_render_types.h
 * @brief GeomNode 场景图节点类型定义
 *
 * 借鉴 Three.js Object3D 的设计：
 * - 层级变换链（局部 + 世界矩阵）
 * - 父子节点关系
 * - 可见性控制
 * - 包围盒（用于视锥体剔除和射线拾取）
 */

#ifndef GEOM_RENDER_TYPES_H
#define GEOM_RENDER_TYPES_H

#include <stdbool.h>
#include <stdint.h>

/* ── 三维向量（对应 Three.js Vector3） ── */
typedef struct {
    double x, y, z;
} vec3;

/* ── 四元数（对应 Three.js Quaternion） ── */
typedef struct {
    double x, y, z, w;
} quat;

/* ── 4x4 矩阵（对应 Three.js Matrix4） ── */
typedef struct {
    double m[16];  /* 列主序存储，与 OpenGL / Three.js 一致 */
} mat4;

/* ── 几何体类型枚举 ── */
typedef enum {
    GEOM_TYPE_POINT,        /* 点 */
    GEOM_TYPE_LINE,         /* 线段 */
    GEOM_TYPE_RAY,          /* 射线 */
    GEOM_TYPE_CIRCLE,       /* 圆 */
    GEOM_TYPE_POLYGON,      /* 多边形 */
    GEOM_TYPE_POLYLINE,     /* 折线 */
    GEOM_TYPE_CURVE,        /* 参数曲线 */
    GEOM_TYPE_SURFACE,      /* 曲面 */
    GEOM_TYPE_TEXT,         /* 文本标签 */
    GEOM_TYPE_GROUP         /* 空组节点（仅变换） */
} GeomType;

/* ── 前向声明 ── */
typedef struct GeomNode GeomNode;
typedef struct GeomVertexBuffer GeomVertexBuffer;

/**
 * @brief 场景图节点 —— 借鉴 Three.js Object3D
 *
 * 每个 GeomNode 对应 DSL 中一个几何对象。节点通过 children
 * 指针形成树形结构，子节点的世界变换 = 父节点世界变换 × 子节点局部变换。
 *
 * 与 Three.js 的关键差异：
 * - 节点携带 constraint_id 反向映射到约束图
 * - 局部变换可由约束图自动推导（受约束控制的点），也可由用户拖拽设置（自由点）
 */
struct GeomNode {
    /* ── 标识 ── */
    uint64_t id;                   /* 节点唯一 ID */
    GeomType type;                 /* 几何类型 */
    const char *label;             /* 显示标签（可为 NULL） */

    /* ── 约束图反向映射 ── */
    int constraint_id;             /* 对应约束图中节点的 ID（-1 表示无映射） */
    bool is_constraint_driven;     /* true: 变换由约束图推算；false: 变换由用户设置 */

    /* ── 变换属性（借鉴 Object3D.position/rotation/scale） ── */
    vec3 position;                 /* 局部平移 */
    quat rotation;                 /* 局部旋转（四元数） */
    vec3 scale;                    /* 局部缩放 */
    mat4 local_matrix;             /* 局部变换矩阵（缓存） */
    mat4 world_matrix;             /* 世界变换矩阵（缓存） */
    bool matrix_needs_update;      /* 矩阵需要重新计算 */

    /* ── 包围盒（用于视锥体剔除和射线拾取） ── */
    vec3 bbox_min, bbox_max;       /* 轴对齐包围盒（AABB） */
    bool bbox_valid;               /* 包围盒是否有效 */

    /* ── 可见性（借鉴 Object3D.visible） ── */
    bool visible;                  /* 是否渲染 */
    bool frozen;                   /* 是否禁止交互 */

    /* ── 顶点数据（借鉴 BufferGeometry） ── */
    GeomVertexBuffer *vertices;    /* 渲染用的顶点缓冲区 */

    /* ── 树形结构（借鉴 Object3D.children + parent） ── */
    GeomNode *parent;              /* 父节点 */
    GeomNode *first_child;         /* 第一个子节点 */
    GeomNode *next_sibling;        /* 下一个兄弟节点 */
    GeomNode *prev_sibling;        /* 上一个兄弟节点 */
    int child_count;               /* 子节点数量 */

    /* ── 用户数据（扩展点） ── */
    void *user_data;               /* 任意用户数据 */
};
```

**JavaScript 端（Web GUI）对应类型：**

```javascript
/**
 * @file lv00-web/src/core/GeomNode.js
 * @brief GeomNode —— 借鉴 Three.js Object3D 的 JS 封装
 *
 * 在 Web GUI 层封装 WASM 传来的 GeomNode 数据，提供
 * 与 Three.js 风格一致的 API。
 */

export class GeomNode {
    /**
     * @param {Object} data - 从 C 内核传来的节点数据
     * @param {number} data.id
     * @param {string} data.type - 'point' | 'line' | 'circle' | 'polygon' | ...
     * @param {string} [data.label]
     */
    constructor(data) {
        this.id = data.id;
        this.type = data.type;
        this.label = data.label ?? '';

        // 变换属性（借鉴 THREE.Object3D）
        this.position = { x: 0, y: 0, z: 0 };  // THREE.Vector3 风格
        this.rotation = { x: 0, y: 0, z: 0, w: 1 };  // 四元数
        this.scale    = { x: 1, y: 1, z: 1 };

        // 层级结构（借鉴 THREE.Object3D.children）
        this.parent   = null;
        this.children = [];  // GeomNode[]

        // 可见性（借鉴 THREE.Object3D.visible）
        this.visible = true;

        // Three.js 渲染对象（惰性创建）
        this._mesh     = null;  // THREE.Mesh | THREE.Line | THREE.Points
        this._geometry = null;  // THREE.BufferGeometry

        // 约束图反向引用
        this.constraintId = data.constraintId ?? -1;
    }

    /* ── 借鉴 THREE.Object3D.add() ── */
    add(child) {
        if (child.parent) child.parent.remove(child);
        child.parent = this;
        this.children.push(child);
        return this;
    }

    /* ── 借鉴 THREE.Object3D.remove() ── */
    remove(child) {
        const idx = this.children.indexOf(child);
        if (idx >= 0) {
            this.children.splice(idx, 1);
            child.parent = null;
        }
        return this;
    }

    /* ── 借鉴 THREE.Object3D.traverse() ── */
    traverse(callback) {
        callback(this);
        for (const child of this.children) {
            child.traverse(callback);
        }
    }

    /* ── 借鉴 THREE.Object3D.updateMatrixWorld() ── */
    updateMatrixWorld() {
        // 从 C 层获取世界矩阵（通过 WASM 调用）
        // wasm_geom_node_get_world_matrix(this.id, this._worldMatrixArray);
        if (this.parent) {
            this.parent.updateMatrixWorld();
        }
    }

    /* ── 创建 Three.js Mesh（惰性） ── */
    getMesh(renderer) {
        if (this._mesh) return this._mesh;
        // 根据类型创建对应的 Three.js 对象
        // ...
        return this._mesh;
    }
}
```

### 3.2 GeomVertexBuffer 批量渲染设计

借鉴 Three.js BufferGeometry，将同类几何原语合并为单次 GPU 提交：

```c
/**
 * @brief 顶点缓冲区 —— 借鉴 Three.js BufferGeometry + BufferAttribute
 *
 * 将所有同类型几何体（如全部点、全部线段）的顶点数据
 * 合并到一个连续缓冲区中，一次 draw call 完成批量渲染。
 */

/* ── 单个属性数组（对应 BufferAttribute） ── */
typedef struct {
    const char *name;        /* 属性名: "position", "color", "normal" */
    int component_count;     /* 每顶点分量数: 3 for vec3, 4 for vec4 */
    int vertex_count;        /* 顶点总数 */
    int capacity;            /* 已分配容量（顶点数） */
    float *data;             /* 实际数据（连续存储） */
    bool dirty;              /* 数据是否需要重新上传 GPU */
} VertexAttribute;

/* ── 顶点缓冲区（对应 BufferGeometry） ── */
struct GeomVertexBuffer {
    GeomType geom_type;            /* 几何体类型（决定绘制模式） */
    int attr_count;                /* 属性数量 */
    VertexAttribute *attrs;        /* 属性数组（至少包含 "position"） */

    /* 索引缓冲区（用于三角形和多边形） */
    bool has_index;                /* 是否使用索引绘制 */
    int index_count;               /* 索引数量 */
    uint32_t *indices;             /* 索引数据 */
    bool index_dirty;

    /* 绘制参数 */
    int draw_mode;                 /* POINTS / LINES / TRIANGLES / LINE_STRIP */
    int instance_count;            /* 实例化渲染数量（0 = 非实例化） */

    /* 分段信息（支持按对象着色/选择） */
    int segment_count;             /* 不同几何体的分段数量 */
    int *segment_offsets;          /* 每个分段的起始顶点/索引 */
    int *segment_lengths;          /* 每个分段的顶点/索引数 */
};

/**
 * @brief 将单个几何体的顶点追加到全局缓冲区（批量渲染核心）
 *
 * 借鉴 Three.js BufferGeometry 的合并策略：所有同类型几何体的
 * 顶点数据合并到一个大的 Float32Array 中，仅一次 `gl.bufferData()`
 * 上传，一次 `gl.drawArrays()`/`gl.drawElements()` 调用完成渲染。
 *
 * @param buf       全局顶点缓冲区
 * @param vertices  待追加的顶点坐标（xyz 交错）
 * @param count     顶点数量
 * @param indices   待追加的索引（TRIANGLES 模式下必需）
 * @param idx_count 索引数量
 * @return 该几何体在缓冲区中的起始偏移（用于后续单独操作）
 */
int vertex_buffer_append(GeomVertexBuffer *buf,
                         const float *vertices, int count,
                         const uint32_t *indices, int idx_count);
```

**JavaScript 端批量渲染流程：**

```javascript
/**
 * @file lv00-web/src/renderer/GeomRenderer.js
 * @brief 批量渲染器 —— 借鉴 Three.js BufferGeometry 合并策略
 */

export class GeomRenderer {
    constructor(canvas) {
        // 为每种几何类型维护独立的合并 BufferGeometry
        this.batchedGeometries = {
            point:    new THREE.BufferGeometry(),
            line:     new THREE.BufferGeometry(),
            triangle: new THREE.BufferGeometry(),
        };

        // 使用 InstancedMesh 提高性能（每个几何体一个实例）
        // 借鉴 THREE.InstancedMesh + 自定义 attribute
        this.pointInstanced = null;     // 点 → THREE.Points
        this.lineSegments = null;       // 线段 → THREE.LineSegments
        this.triangleMesh = null;       // 三角形 → THREE.Mesh

        this.renderer = new THREE.WebGLRenderer({ canvas, antialias: true });
        this.scene = new THREE.Scene();
    }

    /**
     * 从 WASM 获取顶点数据并批量更新 GPU 缓冲区
     *
     * 借鉴 Three.js 的 BufferGeometry.setAttribute +
     * BufferAttribute.needsUpdate 模式。
     */
    updateFromWasm(wasmBuffer) {
        // 1. 从 WASM 线性内存中读取合并后的顶点数组
        const pointData = wasmBuffer.pointVertices;   // Float32Array
        const lineData  = wasmBuffer.lineVertices;    // Float32Array
        const triData   = wasmBuffer.triVertices;     // Float32Array

        // 2. 批量上传（借鉴 BufferGeometry.setAttribute）
        if (pointData.length > 0) {
            const attr = new THREE.BufferAttribute(pointData, 3);
            this.batchedGeometries.point.setAttribute('position', attr);
            this.batchedGeometries.point.setDrawRange(0, pointData.length / 3);
        }

        if (lineData.length > 0) {
            const attr = new THREE.BufferAttribute(lineData, 3);
            this.batchedGeometries.line.setAttribute('position', attr);
            this.batchedGeometries.line.setDrawRange(0, lineData.length / 3);
        }

        if (triData.length > 0) {
            const attr = new THREE.BufferAttribute(triData, 3);
            this.batchedGeometries.triangle.setAttribute('position', attr);
            // 同时上传索引缓冲区
            // this.batchedGeometries.triangle.setIndex(triIndices);
        }
    }

    render(camera) {
        this.renderer.render(this.scene, camera);
    }
}
```

### 3.3 RayPicker 射线拾取设计

借鉴 Three.js Raycaster 的完整流程，适配 Lv-00 的混合几何体场景：

```javascript
/**
 * @file lv00-web/src/interaction/RayPicker.js
 * @brief 射线拾取器 —— 借鉴 Three.js Raycaster
 *
 * 适配 Lv-00 的几何场景：不仅检测渲染三角形，
 * 还需要通过 WASM 调用 C 端的精确几何相交测试。
 */

import * as THREE from 'three';

export class RayPicker {
    constructor(geomScene, camera) {
        this.scene = geomScene;
        this.camera = camera;
        this.raycaster = new THREE.Raycaster();

        // 拾取阈值（像素）
        this.pixelThreshold = 5.0;
    }

    /**
     * 主拾取入口 —— 借鉴 THREE.Raycaster.intersectObjects()
     *
     * 分为两层：
     * 1. GPU 层：射线与渲染三角形相交（适用于曲面/多边形）
     * 2. C 层：通过 WASM 调用精确几何相交（适用于隐式定义的点/线/圆）
     */
    pick(mouseX, mouseY) {
        // ── 步骤 1：生成射线（借鉴 raycaster.setFromCamera） ──
        const ndc = {
            x:  (mouseX / window.innerWidth)  * 2 - 1,
            y: -(mouseY / window.innerHeight) * 2 + 1,
        };
        this.raycaster.setFromCamera(ndc, this.camera);

        // ── 步骤 2：GPU 层粗检测（借鉴 intersectObjects） ──
        // 获取场景中所有 Three.js Mesh
        const meshes = this.scene.getAllMeshes();
        const gpuHits = this.raycaster.intersectObjects(meshes, false);

        // ── 步骤 3：C 层精确检测（Lv-00 特有） ──
        // 对于每个 GPU 命中，反向查找对应的 GeomNode
        // 对于隐式几何体（点、线、圆），通过 WASM 调用精确测试
        const candidates = [];
        for (const hit of gpuHits) {
            const geomNode = hit.object.userData.geomNode;
            if (geomNode) candidates.push(geomNode);
        }

        // 追加隐式几何体候选（未被 GPU 层覆盖的）
        // wasm_pick_implicit_geoms(rayOrigin, rayDir, candidates);

        // ── 步骤 4：排序返回最近命中 ──
        candidates.sort((a, b) => a.distance - b.distance);
        return candidates.length > 0 ? candidates[0] : null;
    }
}
```

**C 端精确几何相交测试（WASM 导出）：**

```c
/**
 * @brief 射线-几何体精确相交测试（Lv-00 C 端）
 *
 * Three.js Raycaster 只能检测射线与三角形的相交。
 * 对于 Lv-00 的隐式几何定义（如"以 A 为圆心、AB 为半径的圆"），
 * 需要 C 端的符号精度相交测试。
 */

/**
 * @brief 检测射线与约束图节点的几何相交
 * @param ray_origin  射线起点（世界坐标）
 * @param ray_dir     射线方向（单位向量）
 * @param graph       约束图
 * @param node_id     被检测的约束图节点 ID
 * @param out_distance 输出：相交距离（若相交）
 * @return true 如果相交
 */
bool geom_ray_intersect(const vec3 *ray_origin,
                         const vec3 *ray_dir,
                         const ConstraintGraph *graph,
                         int node_id,
                         double *out_distance);

/*
 * 内部分发逻辑：
 *
 * switch (node_type) {
 * case POINT:
 *     → 计算射线到点的最近距离，与拾取阈值比较
 * case LINE_SEGMENT:
 *     → 射线与线段的最近点距离
 * case CIRCLE:
 *     → 射线与圆的相交（二次方程求解）
 * case POLYGON:
 *     → 射线与多边形平面的交点 → 点在多边形内的测试
 * case CURVE:
 *     → 数值采样 + 分段线性逼近 → 最小距离
 * }
 */
```

### 3.4 矩阵变换链 Web 端实现

```javascript
/**
 * @file lv00-web/src/math/mat4.js
 * @brief 4x4 矩阵运算 —— 借鉴 Three.js Matrix4
 *
 * 在 Web 端实现轻量级矩阵运算，用于：
 * - 相机投影/视图矩阵
 * - 节点局部→世界变换
 * - OrbitControl 旋转/平移/缩放计算
 *
 * 列主序存储（与 OpenGL / Three.js 一致）。
 */

export class Mat4 {
    constructor() {
        this.elements = new Float32Array([
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1,
        ]);
    }

    /* 借鉴 THREE.Matrix4.multiplyMatrices() */
    multiplyMatrices(a, b) {
        const ae = a.elements, be = b.elements, te = this.elements;
        for (let col = 0; col < 4; col++) {
            for (let row = 0; row < 4; row++) {
                te[col * 4 + row] =
                    ae[row]      * be[col * 4] +
                    ae[row + 4]  * be[col * 4 + 1] +
                    ae[row + 8]  * be[col * 4 + 2] +
                    ae[row + 12] * be[col * 4 + 3];
            }
        }
        return this;
    }

    /* 借鉴 THREE.Matrix4.compose() */
    compose(position, quaternion, scale) {
        const { x: px, y: py, z: pz } = position;
        const { x: qx, y: qy, z: qz, w: qw } = quaternion;
        const { x: sx, y: sy, z: sz } = scale;

        // 从四元数构建旋转矩阵
        const xx = qx * qx, yy = qy * qy, zz = qz * qz;
        const xy = qx * qy, xz = qx * qz, yz = qy * qz;
        const wx = qw * qx, wy = qw * qy, wz = qw * qz;

        const te = this.elements;
        te[0]  = (1 - 2 * (yy + zz)) * sx;
        te[1]  = (2 * (xy + wz)) * sx;
        te[2]  = (2 * (xz - wy)) * sx;
        te[3]  = 0;
        te[4]  = (2 * (xy - wz)) * sy;
        te[5]  = (1 - 2 * (xx + zz)) * sy;
        te[6]  = (2 * (yz + wx)) * sy;
        te[7]  = 0;
        te[8]  = (2 * (xz + wy)) * sz;
        te[9]  = (2 * (yz - wx)) * sz;
        te[10] = (1 - 2 * (xx + yy)) * sz;
        te[11] = 0;
        te[12] = px;
        te[13] = py;
        te[14] = pz;
        te[15] = 1;
        return this;
    }

    /* 借鉴 THREE.Matrix4.invert() */
    invert() {
        // 高斯-约当消元法求逆
        // ... (标准 4x4 求逆实现)
        return this;
    }
}
```

### 3.5 模块化 Web GUI 组织

```javascript
/**
 * @file lv00-web/src/index.js
 * @brief Lv-00 Web GUI 主入口 —— 借鉴 Three.js 模块化组织
 *
 * 设计原则：
 * 1. 核心精简：lv00-web 核心仅包含场景图、渲染器、数学库
 * 2. 扩展可选：交互控件、后处理、导出器作为独立模块
 * 3. Tree-shaking 友好：用户只需 import 需要的模块
 */

// ── 核心（始终需要）──
export { GeomScene }    from './core/GeomScene.js';
export { GeomNode }     from './core/GeomNode.js';
export { GeomCamera }   from './core/GeomCamera.js';

// ── 渲染器 ──
export { GeomRenderer } from './renderer/GeomRenderer.js';

// ── 数学库（可选，也可直接用 gl-matrix） ──
export { Vec3 }         from './math/vec3.js';
export { Mat4 }         from './math/mat4.js';
export { Quat }         from './math/quat.js';

// ── 交互（可选，按需引入） ──
// export { OrbitControl } from './interaction/OrbitControl.js';
// export { RayPicker }    from './interaction/RayPicker.js';
// export { Gizmo }        from './interaction/Gizmo.js';

// ── WASM 桥接 ──
// export { Lv00Bridge }   from './wasm/lv00_bridge.js';
```

---

## 4. 实现路线图

### 4.1 第一阶段：GeomNode 场景图基础 + 数学库（P3）

| 任务 | 文件 | 说明 |
|------|------|------|
| 定义 `GeomNode` 结构体和 `GeomVertexBuffer` | `include/lv00/geom_render_types.h` | C 端场景图节点 + 顶点缓冲区类型定义 |
| 实现 `vec3`、`mat4`、`quat` 数学函数 | `include/lv00/geom_math.h` | C 端矩阵/向量/四元数运算 |
| WASM 绑定：场景树遍历 API | `src/wasm/geom_render_bridge.c` | C 端函数导出为 WASM，供 JS 调用 |
| JS 端 GeomNode 类和 GeomScene 类 | `lv00-web/src/core/GeomNode.js`、`GeomScene.js` | 借鉴 Three.js Object3D API |
| JS 端 Mat4/Vec3/Quat 数学类 | `lv00-web/src/math/` | 轻量纯 JS 实现 |

**预估规模**：约 600 行 C + 400 行 JS

### 4.2 第二阶段：批量渲染 + 基础可视化（P3）

| 任务 | 说明 |
|------|------|
| C 端：`vertex_buffer_append()` 合并逻辑 | 将约束图节点的几何体顶点合并到全局缓冲区 |
| WASM 绑定：顶点数据批量获取 | 单次 WASM 调用返回全部顶点数据 |
| JS 端：`GeomRenderer` 批量渲染类 | 借鉴 BufferGeometry 合并策略，一次 draw call 渲染所有同类型几何体 |
| 基础几何体渲染器（点/线/三角形） | 使用 Three.js 的 Points / LineSegments / Mesh |
| 相机设置（正交/透视可切换） | 借鉴 Three.js PerspectiveCamera / OrthographicCamera |

**预估规模**：约 400 行 C + 500 行 JS

### 4.3 第三阶段：交互拾取 + OrbitControl（P4）

| 任务 | 说明 |
|------|------|
| C 端：`geom_ray_intersect()` 精确相交 | 射线-点/线/圆/多边形相交测试 |
| JS 端：`RayPicker` 拾取器 | 双层检测（GPU 粗筛 + C 端精确） |
| JS 端：`OrbitControl` 相机控制 | 鼠标旋转/平移/缩放 |
| 选中高亮反馈 | 拾取命中后的视觉高亮效果 |
| 拖拽变换 Gizmo | 借鉴 Three.js TransformControls |

**预估规模**：约 300 行 C + 400 行 JS

### 4.4 第四阶段：WebGPU 加速 + 计算着色器（P5）

| 任务 | 说明 |
|------|------|
| WebGPU 后端渲染器 | 借鉴 Three.js WebGPURenderer |
| 计算着色器加速约束求解 | GPU 并行执行几何约束迭代 |
| 特性检测与降级策略 | WebGPU 不可用时回退到 WebGL |
| 性能基准测试与对比 | 对比纯 CPU、WebGL、WebGPU 三路径 |

**预估规模**：约 600 行 JS（WGSL 着色器 + 管线设置）

---

## 5. 附录

### 附录 A：Three.js 核心类与 Lv-00 映射速查

| Three.js 类 | 所属模块 | Lv-00 C 端 | Lv-00 JS 端 |
|-------------|---------|-----------|------------|
| `Object3D` | `core/` | `GeomNode` | `GeomNode` |
| `Scene` | `scenes/` | `GeomNode`（type=GROUP 为根） | `GeomScene` |
| `BufferGeometry` | `core/` | `GeomVertexBuffer` | `THREE.BufferGeometry`（复用） |
| `BufferAttribute` | `core/` | `VertexAttribute` | `THREE.BufferAttribute`（复用） |
| `Matrix4` | `math/` | `mat4` | `Mat4` |
| `Quaternion` | `math/` | `quat` | `Quat` 或 `THREE.Quaternion` |
| `Vector3` | `math/` | `vec3` | `Vec3` 或 `THREE.Vector3` |
| `Raycaster` | `core/` | `geom_ray_intersect()` | `RayPicker` |
| `WebGLRenderer` | `renderers/` | —（浏览器端） | `THREE.WebGLRenderer`（复用） |
| `WebGPURenderer` | `renderers/` | —（浏览器端） | 自行实现或复用 Three.js |
| `PerspectiveCamera` | `cameras/` | —（浏览器端） | `THREE.PerspectiveCamera`（复用） |
| `Mesh` | `objects/` | —（浏览器端） | `THREE.Mesh`（复用） |
| `Points` | `objects/` | —（浏览器端） | `THREE.Points`（复用） |
| `LineSegments` | `objects/` | —（浏览器端） | `THREE.LineSegments`（复用） |

### 附录 B：WebGPU 浏览器支持现状（截至 2026-05）

| 浏览器 | WebGPU 支持 | 计算着色器 | 备注 |
|--------|-----------|-----------|------|
| Chrome 121+ | 完整支持 | 支持 | 桌面 + Android |
| Edge 121+ | 完整支持 | 支持 | 基于 Chromium |
| Firefox Nightly | 实验性（`dom.webgpu.enabled`） | 部分 | 需手动开启 |
| Safari 17+ | 实验性 | 部分 | macOS + iOS |

推荐策略：优先检测 `navigator.gpu`，可用时初始化 WebGPU 路径；不可用时回退到 `THREE.WebGLRenderer`。

### 附录 C：与 Lv-00 现有模块的关系

| 现有模块 | 与本文档的关系 |
|---------|-------------|
| `constraint_graph.h` | GeomNode 的 `constraint_id` 指向约束图节点，是可视化层与数据层的桥梁 |
| `symbolic_coord.h` | 约束图中的符号坐标在渲染时需数值化，生成顶点缓冲区数据 |
| `geometry_types.h` | GeomType 枚举需与此文件的几何类型定义保持一致 |
| `solver.h` | 计算着色器加速的求解器可替代/补充 CPU 端求解器 |
| `preset_basic_geometry.h` | 预置几何函数块的可视化为 GeomNode 的构造来源 |

---

> **文档结束**
> 本文档详述了 Three.js 的场景图架构、BufferGeometry 批量渲染、Matrix4/Quaternion 数学库、Raycaster 交互拾取、WebGPU 计算着色器和 ESM 模块化架构六个核心借鉴点，并提供了完整的 C/JS 代码示例和四阶段实现路线图。核心结论：(1) 借鉴 Object3D 设计 GeomNode 作为 Lv-00 场景图节点，映射约束图关系；(2) 借鉴 BufferGeometry 合并同类几何体为单次 GPU 提交，实现批量渲染；(3) 双层拾取架构（GPU 粗筛 + C 端精确相交）适配 Lv-00 的混合几何体场景；(4) WebGPU 计算着色器作为远期加速路径。
