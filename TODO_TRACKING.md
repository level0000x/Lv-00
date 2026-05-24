# Lv-00 TODO/FIXME 追踪表

> 自动生成于 2026-05-24 | 当前版本: v3.3.0

## 高优先级（功能缺失 / 未实现）

| ID | 文件 | 行号 | 标签 | 描述 | 建议方案 |
|----|------|------|------|------|----------|
| T-001 | `src/core/proof.c` | 4681 | TODO | SLEDGE_ASYNC 模式暂未实现——需要线程池支持 | 待线程池基础设施就绪后实现 |
| T-002 | `src/core/proof.c` | 4694 | TODO | SLEDGE_ASYNC 模式暂未实现 | 同上 |
| T-003 | `src/core/proof.c` | 5029 | TODO | VERIFY_UNDECIDED: 需要完整项语法树支持 | 需实现项语法树（Term AST） |
| T-004 | `src/core/sparse_linear_algebra.c` | 614 | TODO | 集成 SuiteSparse CHOLMOD | 可选：添加外部库依赖 |
| T-005 | `src/core/sparse_linear_algebra.c` | 684 | TODO | 集成 SuiteSparse UMFPACK | 可选：添加外部库依赖 |
| T-006 | `src/core/sparse_linear_algebra.c` | 750 | TODO | 集成 SuiteSparse SPQR | 可选：添加外部库依赖 |

## 中优先级（功能占位 / 待完善）

| ID | 文件 | 行号 | 标签 | 描述 | 建议方案 |
|----|------|------|------|------|----------|
| T-007 | `src/core/geometry_compress.c` | 119 | TODO | Huffman-Tree 压缩优化 | v3.4 计划完善 |
| T-008 | `src/core/geometry_compress.c` | 175 | TODO | 字典压缩模式 | v3.4 计划完善 |
| T-009 | `src/core/geometry_compress.c` | 320 | TODO | 流式压缩接口 | 需与 stream 模块协调 |
| T-010 | `src/core/geometry_compress.c` | 1074 | TODO | 解压完整性校验 | 添加 CRC/哈希校验 |
| T-011 | `src/core/geometry_compress.c` | 1078 | TODO | 流式解压接口 | 需与 stream 模块协调 |
| T-012 | `src/core/geometry_compress.c` | 1094 | TODO | 增量解压 | 需设计增量协议 |
| T-013 | `src/core/geometry_compress.c` | 1147 | TODO | 压缩率统计报告 | 添加性能统计 API |
| T-014 | `src/core/rewrite.c` | 4521 | TODO(P2) | 逆向替换需实现真正的逆向替换 | 重写引擎 P2 规划 |
| T-015 | `src/core/rewrite.c` | 4751 | TODO(P2) | 重构 DFS 以正确回传路径 | 重写引擎 P2 规划 |
| T-016 | `src/core/stream.c` | 1526 | TODO | 超长描述需考虑堆分配缓冲区 | 添加动态缓冲区分配 |

## 低优先级（Web 前端 / 工具）

| ID | 文件 | 行号 | 标签 | 描述 | 建议方案 |
|----|------|------|------|------|----------|
| T-017 | `web-gui/src/utils/idGenerator.ts` | 40 | TODO(v3.3) | 将 60+ 处 generateId() 调用迁移到 generateUniqueId() | 批量重构 |

## 统计

- **总计**: 17 条活跃 TODO/FIXME（已排除已解决的 18 条 solver.c 条目和文档中的引用）
- **高优先级**: 6 条
- **中优先级**: 10 条
- **低优先级**: 1 条

## 处理策略

1. 每个条目绑定到具体版本里程碑（T-001~006 绑定 v3.4.0）
2. SuiteSparse 相关条目（T-004~006）作为可选增强，不阻塞版本发布
3. geometry_compress 条目（T-007~013）在压缩模块重写时一并处理
4. Web 前端条目（T-017）在下一次前端重构时批量迁移
