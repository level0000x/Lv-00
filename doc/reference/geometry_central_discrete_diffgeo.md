# Geometry Central 离散微分几何计算借鉴设计

> **借鉴项目**：Geometry Central（github.com/nmwsharp/geometry-central）
> **核心借鉴点**：SurfaceMesh+MeshData容器系统、离散微分几何算子（Laplace-Beltrami/曲率/法向量）、内蕴Delaunay三角剖分
> **分类**：P3 中优先级 / 离散几何计算基础
> **日期**：2026-05-24

---

## 1. 概述

Geometry Central 是由 Nicholas Sharp、Keenan Crane 等人在卡内基梅隆大学开发的 C++ 离散微分几何计算库。其核心创新在于通过 **SurfaceMesh + MeshData 容器系统**将网格数据结构与附着在网格元素上的计算数据（标量、向量、张量）进行严格分离，同时提供了一套完整的**离散微分几何算子**（Laplace-Beltrami、主曲率、平均曲率、高斯曲率、法向量等），并内置了**内蕴 Delaunay 三角剖分**以保证离散 Laplace 算子的正定性。这三个特性对 Lv-00 的几何约束类型系统和离散几何计算基础设施有关键的借鉴价值。

Geometry Central 的 SurfaceMesh 数据结构通过"半边"（halfedge）表示网格拓扑，MeshData 容器则将标量/向量/复数等数据类型映射到网格的顶点（vertex）、面（face）、边（edge）、角（corner）等元素上。这种"数据与拓扑分离"的架构精确对应 Lv-00 几何类型系统（`geometry_types.h`）中将数据类型（标量、向量、约束条件）与几何元素（点、边、三角形）关联的需求。与 Polyscope 的 Structure+Quantity 主要服务于可视化的目标不同，Geometry Central 的 SurfaceMesh+MeshData 面向的是**计算**——数据结构经过精心设计以支持高效的离散微分几何运算。

对 Lv-00 而言，离散微分几何算子（法向量、曲率、Laplace-Beltrami）在几何定理证明中可能成为关键的约束条件。例如，三角形内角和定理可以通过离散曲率的约束来表达，等周不等式可以转化为 Laplace-Beltrami 特征值的约束。Geometry Central 的计算实现为 Lv-00 提供了将这些连续几何概念"离散化"为符号坐标可以处理的代数约束的工程方案。

---

## 2. 核心借鉴 → Lv-00 映射

### 2.1 SurfaceMesh + MeshData 容器系统

Geometry Central 的核心架构将网格拓扑和数据严格分离：

```
SurfaceMesh（网格拓扑）              MeshData<T>（附着数据）
  ├─ 顶点 Vertex                   ←    ├─ VertexData<double>  顶点标量
  │   └─ outgoingHalfedge()             ├─ VertexData<Vector3>  顶点向量（法向量）
  ├─ 半边 Halfedge                ←    ├─ FaceData<double>    面标量（面积）
  │   └─ next() / twin() / vertex()     ├─ FaceData<Vector3>    面法向量
  ├─ 边 Edge                      ←    ├─ EdgeData<double>    边长/二面角
  │   └─ halfedge() / twoVertices()     ├─ EdgeData<Complex>   边复数（共形参数化）
  ├─ 面 Face                      ←    ├─ CornerData<double>  角标量（角度）
  │   └─ halfedge()                     └─ CornerData<Vector2> 角UV坐标（纹理）
  ├─ 角 Corner                    ←
  │   └─ halfedge() / face() / vertex()
  └─ 边界 BoundaryLoop
      └─ isInterior()
```

**关键特性**：
- `MeshData<T>` 是模板类，可存储任意类型的数据（标量、向量、复数、自定义结构体）
- `MeshData` 自动随网格的修改（增删顶点、翻转边）维护数据一致性
- 转换操作：`VertexData → FaceData`（顶点→面平均）、`FaceData → VertexData`（面→顶点点积）
- `MeshData` 支持算术运算：`faceDataA + faceDataB` 自动逐面求和

### 2.2 Lv-00 geometry_types.h → SurfaceMesh+MeshData 映射

Lv-00 的 `geometry_types.h` 中已经定义了基本的几何类型系统，可以借鉴 Geometry Central 的设计进一步扩展为 MeshData 风格的数据关联：

| Geometry Central 概念 | Lv-00 geometry_types.h 映射 | 说明 |
|:---|:---|:---|
| SurfaceMesh | `ConstraintGraph` 的子图（三角剖分/多边形网格） | Lv-00 中的离散几何域 |
| Vertex | `GEOM_NODE_POINT` | 网格顶点 |
| Edge | `GEOM_NODE_SEGMENT` | 网格边 |
| Face | `GEOM_NODE_TRIANGLE` / `GEOM_NODE_POLYGON` | 网格面 |
| Halfedge | `ConstraintEdge` + `orientation` | 有向边（用于半边遍历） |
| VertexData\<T\> | `ConstraintVertexAttribute<T>` | 顶点关联的约束数据 |
| FaceData\<T\> | `ConstraintFaceAttribute<T>` | 面关联的约束数据 |
| EdgeData\<T\> | `ConstraintEdgeAttribute<T>` | 边关联的约束数据 |

```c
/**
 * @brief Lv-00 几何网格数据属性 —— 借鉴 Geometry Central MeshData<T>
 *
 * 将约束条件（长度等式、角度等式、面积等式）与几何元素
 * （顶点、边、面、角）关联，建立离散几何约束的代数表示。
 *
 * 与 Geometry Central 的 MeshData<T> 直接对应：
 *   GC: VertexData<double> → Lv-00: vertex_attributes.type = ATTR_SCALAR
 *   GC: VertexData<Vector3> → Lv-00: vertex_attributes.type = ATTR_VECTOR3
 *   GC: FaceData<double>   → Lv-00: face_attributes.type   = ATTR_SCALAR
 */

/** 属性值类型 */
typedef enum {
    ATTR_SCALAR,             /**< 标量（double）：长度、面积、角度 */
    ATTR_VECTOR2,            /**< 2D 向量：平面位移、梯度 */
    ATTR_VECTOR3,            /**< 3D 向量：法向量、3D 位移 */
    ATTR_COMPLEX,            /**< 复数：共形参数化 */
    ATTR_CONSTRAINT,         /**< 约束表达式：等式/不等式约束 */
    ATTR_INDEX               /**< 索引：引用其他节点 */
} AttributeValueType;

/** 属性值联合体 */
typedef union {
    double scalar;           /**< ATTR_SCALAR */
    struct { double x, y; } vector2;  /**< ATTR_VECTOR2 */
    struct { double x, y, z; } vector3; /**< ATTR_VECTOR3 */
    struct { double re, im; } complex; /**< ATTR_COMPLEX */
    void *constraint_expr;   /**< ATTR_CONSTRAINT（约束表达式指针） */
    int index;               /**< ATTR_INDEX */
} AttributeValue;

/** 顶点属性（Geometry Central: VertexData<T>） */
typedef struct {
    int vertex_id;           /**< 关联的顶点节点 ID */
    AttributeValueType type; /**< 属性类型 */
    AttributeValue value;    /**< 属性值 */
    char *label;             /**< 属性标签（如 "GaussianCurvature"） */
    bool is_constraint;      /**< 是否参与约束求解 */
} ConstraintVertexAttribute;

/** 面属性（Geometry Central: FaceData<T>） */
typedef struct {
    int face_id;             /**< 关联的面节点 ID */
    AttributeValueType type;
    AttributeValue value;
    char *label;
    bool is_constraint;
} ConstraintFaceAttribute;

/** 边属性（Geometry Central: EdgeData<T>） */
typedef struct {
    int edge_id;             /**< 关联的边节点 ID */
    AttributeValueType type;
    AttributeValue value;
    char *label;
    bool is_constraint;
} ConstraintEdgeAttribute;

/** 角属性（Geometry Central: CornerData<T>） */
typedef struct {
    int corner_id;           /**< 关联的角节点 ID（面+顶点组合） */
    AttributeValueType type;
    AttributeValue value;
    char *label;
    bool is_constraint;
} ConstraintCornerAttribute;
```

### 2.3 离散微分几何算子

Geometry Central 提供了一套完整的离散微分几何算子，这些算子在 Lv-00 中可以转化为几何不变量约束：

| Geometry Central 算子 | 数学定义 | Lv-00 几何约束类型 | 证明应用场景 |
|:---|:---|:---|:---|
| `vertexPosition` | 顶点坐标 | `CONSTRAINT_COORDINATE_EQ` | 点坐标等式约束 |
| `vertexNormalEquallyWeighted` | 均匀加权顶点法向量 | `CONSTRAINT_NORMAL_EQ` | 法向量方向约束（平行/垂直面） |
| `vertexMeanCurvature` | 离散平均曲率 `H = (1/2)||\Delta p||` | `CONSTRAINT_MEAN_CURVATURE_EQ` | 极小曲面约束（H=0） |
| `vertexGaussianCurvature` | 离散高斯曲率 `K = (2π - Σθ_i)/A` | `CONSTRAINT_GAUSS_CURVATURE_EQ` | 局部平坦性约束（K=0） |
| `vertexPrincipalCurvatures` | 主曲率方向与大小 | `CONSTRAINT_PRINCIPAL_CURV_EQ` | 曲面挠向约束 |
| `faceNormal` | 面法向量 | `CONSTRAINT_FACE_NORMAL_EQ` | 面方向约束（平行六面体） |
| `faceArea` | 面面积 | `CONSTRAINT_AREA_EQ` | 面积等式（等积变形） |
| `edgeLength` | 边长 | `CONSTRAINT_LENGTH_EQ` | 边长等式（等边三角形/正方形） |
| `edgeDihedralAngle` | 二面角 | `CONSTRAINT_DIHEDRAL_EQ` | 二面角等式（正多面体约束） |
| `cornerAngle` | 角角度 | `CONSTRAINT_ANGLE_EQ` | 角度等式（三角形内角和） |
| `laplaceBeltrami` | 离散 Laplace-Beltrami | `CONSTRAINT_LB_EQ` | Laplace 特征值约束 |
| `geodesicDistance` | 测地距离（热方法） | `CONSTRAINT_GEODESIC_EQ` | 最短路径约束 |

### 2.4 离散 Laplace-Beltrami 算子作为几何不变量约束

Laplace-Beltrami 算子是最重要的离散微分几何算子之一，在几何定理证明中有广泛应用。以下是 Lv-00 中将离散 LB 算子声明为约束的代码示例：

```c
/**
 * @brief 离散 Laplace-Beltrami 算子作为 Lv-00 几何不变量约束
 *
 * 借鉴 Geometry Central 的 laplaceBeltrami() 实现，
 * 将离散 LB 算子转化为符号坐标的代数约束。
 *
 * 连续形式：Δf = div(∇f) = (1/√|g|) ∂_i(√|g| g^{ij} ∂_j f)
 *
 * 离散形式（cotangent 权重）：
 *   (Δf)_i = (1/2A_i) * Σ_{j∈N(i)} (cot α_{ij} + cot β_{ij}) * (f_j - f_i)
 *
 * 其中：
 *   A_i  = 顶点 i 的 Voronoi 区域面积
 *   N(i) = 顶点 i 的一环邻域
 *   α_{ij}, β_{ij} = 边 ij 的对角
 */

/**
 * @brief 声明离散 Laplace-Beltrami 等式约束
 *
 * 声明对给定网格的顶点标量函数 f 的离散 Laplace-Beltrami 运算
 * 结果等于 g 的约束。
 *
 * 用途示例：
 *   - 调和函数约束：Δf = 0（f 是调和函数）
 *   - 特征值约束：Δf = λf（f 是 Laplace 特征函数）
 *   - 热方程稳态：Δf = c（稳态温度分布）
 *   - 极小曲面约束：Δx = 0（坐标函数是调和的 → 极小曲面）
 *
 * @param mesh        目标三角形网格
 * @param f_attrs     顶点标量函数 f 的属性（长度 = vertex_count）
 * @param g_attrs     目标顶点标量函数 g 的属性（长度 = vertex_count，可为 NULL 表示 0）
 * @param eigenvalue  特征值 λ（当 g = λf 时使用，否则为 1.0）
 * @return 新创建的约束 ID，失败返回 -1
 */
int constraint_declare_laplace_beltrami(
    ConstraintGraph *mesh,
    ConstraintVertexAttribute *f_attrs,
    ConstraintVertexAttribute *g_attrs,
    double eigenvalue
);

/**
 * @brief 声明极小曲面约束（LB 算子的特例）
 *
 * 极小曲面约束：坐标函数 x, y, z 各自是调和函数，即 Δx = Δy = Δz = 0
 *
 * @param mesh  目标三角形网格
 * @return 新创建的约束组 ID（包含三个独立的 LB 等式约束）
 */
int constraint_declare_minimal_surface(ConstraintGraph *mesh);

/**
 * @brief 声明曲率等式约束（通过 LB 算子定义）
 *
 * 平均曲率 H 通过 LB 算子定义：
 *   H * n = Δp （其中 p 是位置向量，n 是法向量）
 *
 * 声明 H = target_value 的等式约束。
 *
 * @param mesh         目标三角形网格
 * @param target_value 目标平均曲率值（0 = 极小曲面）
 * @return 新创建的约束 ID
 */
int constraint_declare_mean_curvature_eq(
    ConstraintGraph *mesh,
    double target_value
);
```

### 2.5 离散曲率算子的代数化

将 Geometry Central 的离散曲率运算从浮点数转化为 Lv-00 的符号代数约束：

```c
/**
 * @brief 离散高斯曲率的符号代数约束
 *
 * Geometry Central 实现（浮点）：
 *   K_i = (2π - Σ_{j} θ_{ij}) / A_i
 *
 * Lv-00 符号约束版本：
 *   K_i * A_i = 2π - Σ_{j} θ_{ij}
 *   即：K_i * A_i - 2π + Σ_{j} θ_{ij} = 0
 *
 * 其中 θ_{ij} 是顶点 i 处第 j 个角的角角度。
 * 角角度本身由边长通过余弦定理表达为代数等式：
 *   cos(θ_{ij}) = (a² + b² - c²) / (2ab)
 *
 * 这导致一个包含三角函数的非线性代数方程组。
 *
 * 简化策略：在 Lv-00 中，角度作为独立符号变量处理，
 * 通过余弦定理将其与边长关联，避免在约束系统中引入三角函数。
 */

/**
 * @brief 从余弦定理生成角度约束
 *
 * 对三角形 ABC，边 AB=c, BC=a, CA=b，角 C 在顶点 C 处：
 *   cos(C) = (a² + b² - c²) / (2ab)
 *
 * 在 Lv-00 中编码为：
 *   (a² + b² - c²) - 2ab * cos_C = 0   （等式约束）
 *   cos_C ∈ [-1, 1]                      （有界约束）
 *
 * @param tri  三角形节点
 * @return 约束组 ID
 */
int constraint_declare_cosine_law(TriangleNode *tri);
```

---

## 3. 实现方案

### 3.1 第一阶段：MeshData 属性系统（P3-1）

- [ ] 在 `geometry_types.h` 中增加属性类型基础设施
  - `AttributeValueType` 枚举（SCALAR/VECTOR2/VECTOR3/COMPLEX/CONSTRAINT/INDEX）
  - `AttributeValue` 联合体
  - `ConstraintVertexAttribute`、`ConstraintFaceAttribute`、`ConstraintEdgeAttribute`、`ConstraintCornerAttribute` 结构体
- [ ] 实现属性容器（借鉴 Geometry Central 的 `MeshData<T>`）
  - 顶点属性容器 `VertexAttributeContainer`
  - 面属性容器 `FaceAttributeContainer`
  - 边属性容器 `EdgeAttributeContainer`
  - 角属性容器 `CornerAttributeContainer`
- [ ] 实现属性算术运算（逐元素加/减/乘/除）
- [ ] 实现属性类型转换（VertexData → FaceData 平均、FaceData → VertexData 点积）
- [ ] 编写属性系统的单元测试

### 3.2 第二阶段：离散微分几何算子（P3-2）

- [ ] 实现基本几何量计算（以符号坐标为基础）
  - 边长：`edge_length(i, j) = sqrt((x_i-x_j)² + (y_i-y_j)² + (z_i-z_j)²)`
  - 面积：Heron 公式 `A = sqrt(s(s-a)(s-b)(s-c))`
  - 角角度：余弦定理 `cos θ = (a²+b²-c²)/(2ab)`
- [ ] 实现离散法向量计算
  - 面法向量：`n = (v1-v0) × (v2-v0)` 归一化
  - 顶点法向量：均匀加权或面积加权平均
- [ ] 实现离散曲率计算
  - 高斯曲率：`K_i = (2π - Σθ)/A`
  - 平均曲率：通过 `H*n = Δp` 计算的 LB 方法
  - 主曲率：通过形状算子特征分解
- [ ] 实现离散 Laplace-Beltrami 算子
  - Cotangent 权重计算：`cot α = cos α / sin α = (a²+b²-c²)/(4A)`
  - 质量矩阵（lumped mass）构建
  - 稀疏 Laplace 矩阵组装
- [ ] 编写微分几何算子的精度验证测试

### 3.3 第三阶段：内蕴 Delaunay 翻转（P3-3）

- [ ] 实现内蕴 Delaunay 条件判断
  - 对内蕴 Delaunay 条件：`α + β ≤ π`（其中 α、β 是对角）
  - 翻转后计算新边长：通过 Ptolemy/Alexandrov 公式
- [ ] 实现边翻转（Edge Flip）操作
  - 拓扑翻转：更新半边连接关系
  - 几何翻转：通过公式计算新边长
  - 属性维护：翻转后属性重新映射
- [ ] 实现全局内蕴 Delaunay 剖分
  - 迭代边翻转直至所有边满足 Delaunay 条件
  - 收敛性保证
- [ ] 实现基于内蕴 Delaunay 的 Laplace 算子
  - 保证正定性（所有 cot 权重 ≥ 0）
  - 提高数值稳定性
- [ ] 编写内蕴 Delaunay 翻转的单元测试

### 3.4 第四阶段：约束类型集成（P3-4）

- [ ] 在 `constraint_system.h` 中注册新的离散几何约束类型
  - `CONSTRAINT_LAPLACE_BELTRAMI_EQ`：LB 等式约束
  - `CONSTRAINT_GAUSS_CURVATURE_EQ`：高斯曲率等式约束
  - `CONSTRAINT_MEAN_CURVATURE_EQ`：平均曲率等式约束
  - `CONSTRAINT_NORMAL_EQ`：法向量等式约束
  - `CONSTRAINT_AREA_EQ`：面积等式约束
  - `CONSTRAINT_DIHEDRAL_EQ`：二面角等式约束
  - `CONSTRAINT_GEODESIC_EQ`：测地距离等式约束
- [ ] 实现每种约束类型的代数编码
  - 约束 → 多项式/代数方程转换
  - 约束 → SMT-LIB 编码
- [ ] 实现约束求解器与离散几何算子的集成
- [ ] 编写端到端的几何定理证明测试用例

---

## 4. 设计决策与权衡

### 4.1 符号 vs 浮点：连续微分几何的离散化策略

Geometry Central 的所有算子基于浮点数计算，Lv-00 需要将这些连续概念离散化为符号代数约束。这是一项非平凡的工作：

**直接浮点模仿**（不推荐）：在 Lv-00 中实现浮点版本的算子作为数值验证器。优点是实现简单，缺点是不提供符号证明。

**完全符号化**（目标）：将每个离散算子表达为符号变量的代数表达式。挑战在于：
- `sqrt()` 引入代数数（非有理数），超出有理数精确表示范围
- `cot θ` 涉及三角函数，在严格代数意义下是超越的
- 角度和三角函数需要引入辅助符号变量

**混合策略**（实用方案）：

```
对于边长/面积等有理数友好的量：
  直接符号化 → 有理数代数约束

对于角度/曲率等涉及无量纲比的量：
  引入辅助符号变量 angle_i_j → 通过余弦定理与边长关联：
    cos(angle_i_j) - (a²+b²-c²)/(2ab) = 0

对于 Laplace-Beltrami 等涉及 cot 权重的量：
  使用 (a²+b²-c²)/(4A) 替代 cot α（纯代数表达式，无三角函数）
```

**关键公式替换表**：

| 涉及的量 | 浮点公式 | Lv-00 符号替代 |
|:---|:---|:---|
| `cot α` | `cos α / sin α` | `(b² + c² - a²) / (4 * area)` |
| `sin α` | `sin(arccos(...))` | `2 * area / (b * c)` |
| `area` | `0.5 * base * height` | `(1/4) * sqrt(4a²b² - (a²+b²-c²)²)` |
| `cos α` | `cos(...)` | `(b² + c² - a²) / (2 * b * c)` |

**面积公式的代数化**：`area` 使用 Heron 公式时引入平方根。策略是将 `area²` 作为中间符号变量：
```
let s = (a + b + c) / 2
let area_sq = s(s-a)(s-b)(s-c)
// area_sq 是纯有理数表达式，无需 sqrt
// 在需要 area 的约束中，使用 area_sq 形式的等式
```

### 4.2 半边数据结构的取舍

Geometry Central 使用完整的半边数据结构（halfedge）表示网格拓扑。Lv-00 的 `ConstraintGraph` 基于节点-边的通用图模型，不直接支持半边语义。两种适配策略：

**策略 A：完全移植半边结构**（完备但侵入式）
- 在 `ConstraintGraph` 内部增加半边层
- 优点：完整支持所有离散几何运算
- 缺点：增加 `ConstraintGraph` 的复杂度，对非网格几何不友好

**策略 B：适配层包装**（隔离但有限）
- 为网格子图创建 `HalfedgeAdapter`，通过遍历算法模拟半边操作
- 优点：不侵入 `ConstraintGraph` 核心
- 缺点：性能开销，部分高级操作可能缺失

推荐策略 B 用于 P3 阶段，策略 A 作为未来 P2 的升级路径。

### 4.3 与已有 Catlab GAT 借鉴的关系

Lv-00 已有 `catlab_gat_compilation.md` 参考文档，Catlab 的泛化代数理论（GAT）可描述网格拓扑的公理。Geometry Central 的离散几何算子可以与 Catlab 的代数表达相结合：

| Catlab GAT | Geometry Central | 结合点 |
|:---|:---|:---|
| `HalfEdgeGraph` 理论 | SurfaceMesh 半边数据结构 | GAT 定义网格拓扑公理，GC 实现具体存储 |
| `MeshData` 函子 | MeshData\<T\> 容器 | GAT 定义数据关联的类型安全性，GC 实现运行时容器 |
| `DiscreteExteriorCalculus` | Laplace-Beltrami + Hodge 星 | DEC 提供高级代数框架，GC 提供基础算子实现 |

---

## 5. 补充：Geometry Central 的数值精度保证

### 5.1 内蕴 Delaunay 翻转的正定性保证

Geometry Central 的一个关键贡献是内蕴 Delaunay 三角剖分保证了离散 Laplace 算子的正定性。在标准的 Euclidean Delaunay 剖分中，所有 cotangent 权重都必须是正的（`cot α + cot β ≥ 0`），但这在包含钝角的网格中可能不成立。

内蕴 Delaunay 翻转通过修改网格拓扑（不改变顶点位置）来修复：
- 翻转违反 Delaunay 条件的边（`α + β > π`）
- 翻转后新边长通过 Ptolemy 公式计算：`e' = sqrt(ac + bd)`（其中 a, b, c, d 是四边形的四条边）
- 迭代执行直到所有边满足 Delaunay 条件

**Lv-00 中的意义**：如果离散 Laplace 算子的 cot 权重始终非负，则 Lv-00 编码的 Laplace 等式约束具有更好的数值稳定性（无负数消去问题）。内蕴 Delaunay 翻转可以在将网格发送给约束求解器之前作为预处理步骤执行。

### 5.2 测地距离的热方法

Geometry Central 实现了基于热方程（Heat Method）的测地距离计算：

```
1. 求解热方程： (M - t*L) * u_t = u_0  （t 是小时间步长）
2. 计算归一化梯度： X = -∇u_t / |∇u_t|
3. 求解 Poisson 方程： L * φ = ∇·X
4. φ 即为测地距离（精确到加性常数）

关键词：M = 质量矩阵，L = Laplace 矩阵
```

在 Lv-00 中，测地距离约束（`CONSTRAINT_GEODESIC_EQ`）可以利用热方法的代数编码：

```c
/**
 * @brief 测地距离约束 —— 通过离散热方法编码
 *
 * 声明两个网格顶点之间的测地距离等于目标值。
 * 编码为热方法三个步骤的代数约束：
 *   1. (M - t*L) * u_t = u_0     // 热核初始扩散
 *   2. X_i = -grad(u_t)_i         // 梯度归一化
 *   3. L * φ = div(X)             // Poisson 重建
 *   4. φ(source) = 0, φ(target) = d_target  // 边界条件
 *
 * @param mesh        目标网格
 * @param source_vid  源顶点 ID
 * @param target_vid  目标顶点 ID
 * @param distance    目标测地距离
 * @return 约束组 ID
 */
int constraint_declare_geodesic_distance(
    ConstraintGraph *mesh,
    int source_vid,
    int target_vid,
    double distance
);
```

### 5.3 矢量/余切 Laplace 与方向场

Geometry Central 支持矢量值 Laplace-Beltrami 算子（用于方向场平滑）和连接 Laplace 算子（用于平行传输）。这些高级算子在 Lv-00 中的潜在应用：

- **方向场平滑**：矢量 Laplace 用于平滑约束梯度的方向场
- **平行传输**：连接 Laplace 用于在弯曲网格表面上平行移动切向量
- **Hodge 分解**：将任意向量场分解为梯度部分 + 旋度部分 + 调和部分

---

## 6. 参考资源

- Geometry Central 官方仓库：https://github.com/nmwsharp/geometry-central
- Geometry Central 官方文档：https://geometry-central.net/
- Keenan Crane, "Discrete Differential Geometry: An Applied Introduction", 2022
- Nicholas Sharp, "Geometry Central: A Modern Library for 3D Geometry Processing", 2019
- Geometry Central 源码关键文件：
  - `include/geometrycentral/surface/surface_mesh.h` —— SurfaceMesh 数据结构
  - `include/geometrycentral/surface/mesh_data.hpp` —— MeshData\<T\> 模板容器
  - `include/geometrycentral/surface/vertex_position_geometry.h` —— 几何嵌入
  - `include/geometrycentral/surface/intrinsic_geometry_interface.h` —— 内蕴Delaunay接口
  - `include/geometrycentral/surface/direction_fields.h` —— 方向场和矢量Laplace
- 与 Lv-00 相关的已有借鉴文档：
  - `polyscope_structure_quantity_viz.md` —— Polyscope 的 Structure+Quantity 可视化（与 GC 的 SurfaceMesh+MeshData 形成"可视化/计算"双视角）
  - `catlab_gat_compilation.md` —— 泛化代数理论对网格拓扑的公理化描述
  - `libigl_header_only_api.md` —— libigl 的离散几何处理 API 参考
  - `jsxgraph_interactive_geometry.md` —— 2D 交互式几何中角度/长度的精确计算参考
