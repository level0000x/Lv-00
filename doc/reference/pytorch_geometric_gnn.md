# PyTorch Geometric 几何深度学习库参考文档

> **项目**: PyTorch Geometric (PyG)  
> **链接**: [github.com/pyg-team/pytorch_geometric](https://github.com/pyg-team/pytorch_geometric)  
> **语言**: Python (主) + C++/CUDA (底层)  
> **许可**: MIT  
> **Stars**: 21k+  
> **创建日期**: 2026-05-27  
> **适用层级**: Lv-00 第 4 层（多策略自动推理层）+ 第 7 层（AI 辅助推理层）

---

## 一、项目概述

PyTorch Geometric (PyG) 是基于 PyTorch 的几何深度学习库，由 TU Dortmund 大学 Matthias Fey 团队开发。它提供了图神经网络 (GNN)、点云处理、网格分析等几何深度学习算法的统一实现框架。

### 1.1 核心定位

PyG 的独特价值在于：

- **消息传递抽象**：统一的 `MessagePassing` 接口定义 GNN 层
- **高效数据结构**：稀疏张量存储图数据，支持大规模图
- **丰富预训练模型**：GCN、GAT、PointNet++ 等 50+ 模型
- **GPU 加速**：CUDA 优化的邻居采样和聚合操作

### 1.2 架构组成

PyG 采用分层架构：

| 层级 | 模块 | 功能描述 |
|------|------|---------|
| **引擎层** | `pyg-lib` | C++/CUDA 高效算子 |
| **存储层** | `torch_geometric.data` | 图数据存储与加载 |
| **算子层** | `torch_geometric.nn` | GNN 层与算子 |
| **模型层** | `torch_geometric.models` | 预训练模型 |
| **采样层** | `torch_geometric.sampler` | 邻居采样策略 |

---

## 二、核心借鉴点

### 2.1 消息传递抽象

PyG 的核心创新是统一的 `MessagePassing` 接口：

```python
from torch_geometric.nn import MessagePassing

class EdgeConv(MessagePassing):
    def __init__(self, in_channels, out_channels):
        super().__init__(aggr="max")  # 聚合方式：max/mean/add
        
    def forward(self, x, edge_index):
        # x: [N, in_channels] 节点特征
        # edge_index: [2, E] 边连接
        return self.propagate(edge_index, x=x)
    
    def message(self, x_j, x_i):
        # x_j: 源节点特征 [E, in_channels]
        # x_i: 目标节点特征 [E, in_channels]
        edge_features = torch.cat([x_i, x_j - x_i], dim=-1)
        return self.mlp(edge_features)
    
    def update(self, aggr_out):
        # 更新节点特征
        return aggr_out
```

### 2.2 图数据存储结构

PyG 使用高效的稀疏存储格式：

```python
from torch_geometric.data import Data

# 图数据结构
data = Data(
    x=torch.randn(100, 16),           # 节点特征 [N, F]
    edge_index=torch.tensor([[0,1,2], [1,2,3]]),  # 边连接 [2, E]
    edge_attr=torch.randn(3, 8),      # 边特征 [E, D]
    y=torch.tensor([0, 1, 0]),        # 标签
    pos=torch.randn(100, 3),          # 3D 位置（点云）
)

# 批处理
from torch_geometric.data import DataLoader
loader = DataLoader([data1, data2, data3], batch_size=2, shuffle=True)
```

### 2.3 点云处理算子

PyG 提供丰富的点云处理算子：

```python
from torch_geometric.nn import PointNetConv, fps, radius_graph

# 最远点采样 (FPS)
indices = fps(pos, ratio=0.5)  # 采样 50% 点

# 半径图构建
edge_index = radius_graph(pos, r=0.5)  # 距离 < 0.5 的点连接

# PointNet 卷积
conv = PointNetConv(local_nn=torch.nn.Linear(32, 16))
out = conv(x, pos, edge_index)
```

### 2.4 GNN 模型实现

PyG 内置 50+ GNN 模型：

| 模型 | 论文 | 特点 | 适用场景 |
|------|------|------|---------|
| **GCNConv** | Kipf 2017 | 图卷积网络 | 图分类 |
| **GATConv** | Veličković 2018 | 图注意力网络 | 节点分类 |
| **SAGEConv** | Hamilton 2017 | GraphSAGE | 大规模图 |
| **EdgeConv** | Wang 2018 | 边卷积 | 点云分割 |
| **PointNetConv** | Qi 2017 | PointNet++ | 点云处理 |

### 2.5 异构图支持

PyG 支持异构图（多类型节点/边）：

```python
from torch_geometric.data import HeteroData

data = HeteroData()
data['paper'].x = torch.randn(100, 16)     # 论文节点
data['author'].x = torch.randn(50, 16)     # 作者节点
data['paper', 'cites', 'paper'].edge_index = ...  # 论文引用边
data['author', 'writes', 'paper'].edge_index = ...  # 作者写作边

# 异构图神经网络
from torch_geometric.nn import HGTConv
conv = HGTConv(in_channels, out_channels, metadata=data.metadata())
```

---

## 三、Lv-00 映射方案

### 3.1 消息传递抽象映射

将 PyG 的消息传递抽象映射到 Lv-00 的约束传播系统：

```c
// Lv-00 消息传递接口
typedef struct Lv00MessagePassing {
    Lv00AggregationType aggr;        // MAX/MEAN/ADD
    Lv00MessageFunc message_func;    // 消息函数
    Lv00UpdateFunc update_func;      // 更新函数
    
    // 状态
    Lv00Tensor *node_features;       // 节点特征
    Lv00Tensor *edge_index;          // 边连接
} Lv00MessagePassing;

// 消息传递执行
Lv00Tensor *lv00_mp_propagate(
    Lv00MessagePassing *mp,
    Lv00Tensor *x,
    Lv00Tensor *edge_index
);

// 消息函数
Lv00Tensor *lv00_mp_message(
    Lv00MessagePassing *mp,
    Lv00Tensor *x_j,  // 源节点特征
    Lv00Tensor *x_i   // 目标节点特征
);

// 聚合函数
Lv00Tensor *lv00_mp_aggregate(
    Lv00MessagePassing *mp,
    Lv00Tensor *messages,
    Lv00Tensor *edge_index
);
```

### 3.2 图数据存储映射

```c
// Lv-00 图数据结构
typedef struct Lv00GraphData {
    Lv00Tensor *node_features;       // [N, F] 节点特征
    Lv00Tensor *edge_index;          // [2, E] 边连接
    Lv00Tensor *edge_features;       // [E, D] 边特征
    Lv00Tensor *node_positions;      // [N, 3] 3D 位置
    Lv00Tensor *labels;              // [N] 标签
    
    // 元数据
    int node_count;
    int edge_count;
    int feature_dim;
} Lv00GraphData;

// 图数据批处理
typedef struct Lv00GraphBatch {
    Lv00GraphData **graphs;
    int batch_size;
    Lv00Tensor *batch_index;         // 批索引
} Lv00GraphBatch;

// 图数据加载器
Lv00GraphBatch *lv00_graph_loader_next(Lv00GraphLoader *loader);
```

### 3.3 点云处理映射

```c
// Lv-00 点云处理算子
typedef struct Lv00PointCloudOps {
    // 最远点采样
    int *lv00_fps(Lv00Tensor *pos, float ratio);
    
    // 半径图构建
    Lv00Tensor *lv00_radius_graph(Lv00Tensor *pos, float radius);
    
    // KNN 图构建
    Lv00Tensor *lv00_knn_graph(Lv00Tensor *pos, int k);
    
    // 点云卷积
    Lv00Tensor *lv00_pointnet_conv(
        Lv00Tensor *x,
        Lv00Tensor *pos,
        Lv00Tensor *edge_index
    );
} Lv00PointCloudOps;
```

### 3.4 GNN 模型映射

```c
// Lv-00 GNN 模型结构
typedef struct Lv00GNNModel {
    Lv00MessagePassing **layers;
    int layer_count;
    Lv00Tensor *hidden_features;
    Lv00Tensor *output_features;
    
    // 模型类型
    Lv00GNNModelType type;           // GCN/GAT/SAGE/EDGE
} Lv00GNNModel;

// 模型前向传播
Lv00Tensor *lv00_gnn_forward(
    Lv00GNNModel *model,
    Lv00GraphData *data
);

// 模型训练
Lv00TrainResult lv00_gnn_train(
    Lv00GNNModel *model,
    Lv00GraphLoader *loader,
    int epochs
);
```

---

## 四、实现路线图

### 4.1 分阶段实施表

| 阶段 | 目标 | 交付物 | 工作量 | 依赖 |
|------|------|--------|--------|------|
| **P1: 图数据结构** | 实现稀疏图存储 | `include/lv00/graph_data.h`（~250行） | 3 天 | 无 |
| **P2: 消息传递接口** | 实现消息传递抽象 | `include/lv00/message_passing.h`（~300行） | 4 天 | P1 |
| **P3: 点云算子** | 实现 FPS/半径图/KNN | `include/lv00/pointcloud_ops.h`（~350行） | 5 天 | P2 |
| **P4: GNN 模型** | 实现 GCN/GAT 模型 | `include/lv00/gnn_model.h`（~400行） | 6 天 | P3 |
| **P5: AI 推理集成** | 与证明系统集成 | `include/lv00/ai_prover.h`（~300行） | 4 天 | P4 |

### 4.2 技术选型建议

| PyG 特性 | Lv-00 实现建议 | 理由 |
|---------|---------------|------|
| PyTorch 后端 | 使用 ONNX Runtime 或自定义张量库 | 避免 Python 依赖 |
| CUDA 加速 | 使用 CUDA C 或 OpenCL | 跨平台支持 |
| 稀疏张量 | 使用 CSR/CSC 格式 | 与几何数据一致 |
| 消息传递 | C 函数指针 + 回调 | 灵活性高 |

### 4.3 性能基准

| 操作 | PyG 性能 | Lv-00 目标 | 测试方法 |
|------|---------|-----------|---------|
| GCN 层前向 | ~5ms | ~10ms | 10K 节点图 |
| FPS 采样 | ~2ms | ~5ms | 10K 点云 |
| 半径图构建 | ~10ms | ~20ms | 10K 点云 |
| 模型训练 | ~100ms/epoch | ~200ms/epoch | 100 epoch |

---

## 五、附录

### 5.1 PyG GNN 层完整列表

| 层名 | 论文 | 特点 |
|------|------|------|
| GCNConv | Kipf 2017 | 图卷积 |
| GATConv | Veličković 2018 | 图注意力 |
| GATv2Conv | Brody 2022 | 动态注意力 |
| SAGEConv | Hamilton 2017 | 采样聚合 |
| EdgeConv | Wang 2018 | 边卷积 |
| PointNetConv | Qi 2017 | PointNet++ |
| TransformerConv | Shi 2020 | 图 Transformer |
| GINConv | Xu 2019 | 图同构网络 |
| PNAConv | Corso 2020 | 多尺度聚合 |
| HGTConv | Hu 2020 | 异构图 Transformer |

### 5.2 PyG 数据集列表

| 数据集 | 类型 | 规模 | 用途 |
|--------|------|------|------|
| Cora | 引用图 | 2708 节点 | 节点分类 |
| CiteSeer | 引用图 | 3327 节点 | 节点分类 |
| PubMed | 引用图 | 19717 节点 | 节点分类 |
| FAUST | 点云 | 6890 点 | 形状匹配 |
| ModelNet40 | 点云 | 40 类 | 形状分类 |
| ShapeNet | 点云 | 16 类 | 形状分割 |

### 5.3 参考文献

1. PyG Documentation: [pytorch-geometric.readthedocs.io](https://pytorch-geometric.readthedocs.io)
2. PyG GitHub: [github.com/pyg-team/pytorch_geometric](https://github.com/pyg-team/pytorch_geometric)
3. "Fast Geometric Deep Learning with Continuous B-Spline Kernels" - Fey et al., 2018
4. "PyG 2.0: Scalable Graph Neural Networks" - Fey et al., 2025

---

> **文档结束**  
> 本文档约 430 行，覆盖 PyTorch Geometric 的消息传递抽象、图数据存储、点云处理、GNN 模型等核心特性，为 Lv-00 第 4 层和第 7 层提供直接参考。