# Axiom Packages -- Lv-00 公理包系统

## 概述 / Overview

Lv-00 公理包系统（Axiom Package System）是一个**可插拔（pluggable）、可版本化（versioned）的公理体系管理框架**。它允许用户按需加载不同的数学公理系统（如欧氏几何、双曲几何、集合论、群论等），并在这些公理基础上进行形式化推理和定理证明。

### 设计灵感 / Design Inspiration

本系统的设计借鉴了两个成熟项目：

| 项目 | 借鉴点 | 说明 |
|------|--------|------|
| **GeoCoq** | 公理分层（axiom layering） | 几何公理按逻辑依赖分层：incidence -> order -> congruence -> continuity -> parallels |
| **LeanGeo** | 命名约定 `namespace.axiom_name` | 每个公理通过命名空间唯一标识，如 `euclidean.I1`、`euclidean.Playfair` |
| **GAP** | 包管理（package management） | 模块化的数学包系统，支持依赖解析和版本管理 |

## 核心概念 / Core Concepts

### 1. 公理包命名约定 / Naming Convention

采用 `namespace.axiom_name` 格式（继承自 LeanGeo）：

```
euclidean.I1          -- 欧氏几何第一关联公理
euclidean.B1          -- 欧氏几何第一顺序公理
euclidean.C1          -- 欧氏几何第一全等公理
euclidean.Playfair    -- 欧氏几何平行公理（Playfair 形式）
hyperbolic.Lobachevsky -- 双曲几何平行公理（Lobachevsky 形式）
zfc.extensionality    -- ZFC 外延公理
group.associativity   -- 群论结合律
```

### 2. 分层哲学 / Layering Philosophy

借鉴 GeoCoq 的分层架构，公理按**逻辑依赖关系**组织为层级。每个上层只能依赖下层已定义的公理：

```
Layer 0: Incidence (关联)     -- I1, I2, I3
    |
Layer 1: Order (顺序)         -- B1, B2, B3, B4
    |
Layer 2: Congruence (全等)    -- C1, C2, C3
    |
Layer 3: Continuity (连续性)   -- Dedekind
    |
Layer 4: Parallels (平行)      -- Playfair (欧氏) / Lobachevsky (双曲)
```

**分层的好处**：
- **独立性追踪（Independence Tracking）**：可以精确知道哪些定理仅依赖前 N 层公理
- **最小化依赖**：推理时只需加载实际需要的层，减少搜索空间
- **几何变体切换**：替换第 4 层即可在欧氏几何和双曲几何之间切换

### 3. 依赖管理 / Dependency Management

每个公理包在 `manifest.json` 中声明其依赖关系：

```json
{
  "name": "euclidean_plane",
  "layers": [
    { "name": "incidence",   "depends_on": [] },
    { "name": "order",       "depends_on": ["incidence"] },
    { "name": "congruence",  "depends_on": ["incidence", "order"] },
    { "name": "continuity",  "depends_on": ["incidence", "order"] },
    { "name": "parallels",   "depends_on": ["incidence"] }
  ]
}
```

系统在加载时会自动进行**依赖解析（dependency resolution）**，确保所有依赖的公理层都已加载。

### 4. 包版本化 / Package Versioning

采用语义化版本（Semantic Versioning, SemVer）：`MAJOR.MINOR.PATCH`

- **MAJOR**：不兼容的公理变更（如修改公理内容）
- **MINOR**：向后兼容的新增（如新增派生规则）
- **PATCH**：文档修正、元数据更新

### 5. 完整性校验 / Hash-based Integrity

每个公理包的内容通过 SHA-256 哈希进行完整性校验。`manifest.json` 中的 `content_hash_algorithm` 字段指定使用的哈希算法。

## 加载与使用 / Loading and Usage

### 通过 Lv-00 API 加载

```c
// 加载欧氏几何公理包
lv_load_package("euclidean_plane");

// 加载时指定版本
lv_load_package_version("euclidean_plane", "1.0.0");

// 仅加载特定层
lv_load_layer("euclidean_plane", "incidence");
lv_load_layer("euclidean_plane", "order");
```

### 查询已加载公理

```c
// 列出所有已加载的公理
lv_list_axioms();

// 检查特定公理是否已加载
lv_axiom_loaded("euclidean.Playfair");
```

## 包注册表 / Package Registry

所有可用公理包的元数据保存在 `INDEX.json` 中。该文件作为包的**中央注册表**，包含每个包的版本、分类、依赖和描述信息。

要添加新包，请：
1. 创建包的目录和 `manifest.json`
2. 在 `INDEX.json` 中注册该包
3. 提供公理定义文件（`.lvz` 格式）

## 目录结构 / Directory Structure

```
axiom_packages/
  README.md                    -- 本文件
  INDEX.json                   -- 包注册表
  CHANGELOG.md                 -- 变更日志
  package_template.json        -- 新包创建模板
  euclidean/                   -- 欧氏几何公理包
    README.md                  -- 欧氏几何分层公理文档
    manifest.json              -- 欧氏几何包清单
  hyperbolic/                  -- 双曲几何公理包
    manifest.json              -- 双曲几何包清单
  *.lvz                        -- 公理定义文件（Lv-00 专用格式）
```

## 现有包列表 / Available Packages

| 包名 | 分类 | 说明 |
|------|------|------|
| `euclidean_plane` | geometry | Tarski 风格欧氏平面几何（5 层分层公理） |
| `hyperbolic_geometry` | geometry | 双曲几何（复用欧氏 0-3 层，替换平行公理） |
| `elliptic_geometry` | geometry | 椭圆（球面）几何 |
| `zfc_set_theory` | foundations | Zermelo-Fraenkel 集合论（含选择公理） |
| `group_theory` | algebra | 群论公理 |
| `ring_theory` | algebra | 环论公理（依赖群论） |
| `field_theory` | algebra | 域论公理（依赖环论） |
| `real_analysis` | analysis | 实分析（完备性公理） |
| `category_theory` | foundations | 范畴论公理 |
| `intuitionistic_logic` | logic | 直觉主义一阶逻辑 |
| `classical_propositional_logic` | logic | 经典命题逻辑 |

更多包定义请参见 `INDEX.json`。
