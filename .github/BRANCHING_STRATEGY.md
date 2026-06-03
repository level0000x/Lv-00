# Lv-00 项目分支管理规范

## 概述

本文档定义 Lv-00 几何元语言内核项目的 Git 分支管理策略，确保代码质量、版本控制和协作效率。

## 分支类型

### 1. 主分支 (main)

- **用途**: 生产就绪代码，始终保持稳定
- **保护规则**:
  - 禁止直接推送，必须通过 Pull Request 合并
  - 要求至少 2 名审查者批准
  - 要求 CI 检查通过
  - 要求分支为最新状态
- **命名**: `main`

### 2. 开发分支 (develop)

- **用途**: 日常开发集成，包含最新开发特性
- **保护规则**:
  - 禁止直接推送，必须通过 Pull Request 合并
  - 要求至少 1 名审查者批准
  - 要求 CI 检查通过
- **命名**: `develop`
- **合并来源**: 功能分支 (`feature/*`)、修复分支 (`fix/*`)

### 3. 功能分支 (feature/*)

- **用途**: 开发新功能
- **命名规则**: `feature/<简短描述>` 或 `feature/<issue-id>-<描述>`
- **示例**:
  - `feature/constraint-graph-optimization`
  - `feature/#123-groebner-solver`
- **基于**: `develop`
- **合并目标**: `develop`
- **生命周期**: 功能完成后删除

### 4. 修复分支 (fix/*)

- **用途**: 修复 bug
- **命名规则**: `fix/<简短描述>` 或 `fix/<issue-id>-<描述>`
- **示例**:
  - `fix/memory-leak-in-solver`
  - `fix/#456-null-pointer-deref`
- **基于**: `develop`（普通 bug）或 `main`（紧急修复）
- **合并目标**: `develop` 和/或 `main`
- **生命周期**: 修复完成后删除

### 5. 文档分支 (docs/*)

- **用途**: 文档更新
- **命名规则**: `docs/<简短描述>`
- **示例**:
  - `docs/api-reference-update`
  - `docs/contributing-guide`
- **基于**: `develop`
- **合并目标**: `develop`

### 6. 重构分支 (refactor/*)

- **用途**: 代码重构，不添加新功能
- **命名规则**: `refactor/<简短描述>`
- **示例**:
  - `refactor/symbolic-coord-module`
  - `refactor/constraint-graph-cleanup`
- **基于**: `develop`
- **合并目标**: `develop`

### 7. 稳定分支 (stable)

- **用途**: 长期稳定版本，用于重要里程碑
- **命名**: `stable`
- **保护规则**:
  - 禁止直接推送
  - 仅允许从 `main` 分支合并
  - 要求 3 名审查者批准
- **创建时机**: 每个主要版本发布时

### 8. 正式发行分支 (release/*)

- **用途**: 版本发布准备，进行最后的测试和文档更新
- **命名规则**: `release/v<主版本>.<次版本>.<补丁版本>`
- **示例**:
  - `release/v3.5.0`
  - `release/v3.4.2`
- **基于**: `develop`（新功能版本）或 `main`（补丁版本）
- **合并目标**: `main` 和 `develop`
- **生命周期**: 版本发布后删除

### 9. 热修复分支 (hotfix/*)

- **用途**: 紧急修复生产环境问题
- **命名规则**: `hotfix/<描述>`
- **示例**:
  - `hotfix/critical-solver-crash`
- **基于**: `main`
- **合并目标**: `main` 和 `develop`
- **生命周期**: 修复发布后删除

## 工作流程

### 功能开发流程

```
1. 从 develop 创建功能分支
   git checkout develop
   git pull origin develop
   git checkout -b feature/my-feature

2. 开发并提交更改
   git add .
   git commit -m "feat(scope): description"

3. 推送到远程
   git push -u origin feature/my-feature

4. 创建 Pull Request 到 develop
   - 填写 PR 模板
   - 关联相关 Issue
   - 请求审查

5. 审查通过后合并
   - 使用 Squash Merge 保持历史整洁
   - 删除功能分支
```

### 发布流程

```
1. 从 develop 创建 release 分支
   git checkout develop
   git pull origin develop
   git checkout -b release/v3.5.0

2. 进行发布准备
   - 更新版本号
   - 更新 CHANGELOG.md
   - 进行回归测试

3. 创建 PR 到 main 和 develop

4. 审查通过后合并到 main
   - 打上版本标签: git tag -a v3.5.0 -m "Release v3.5.0"
   - 推送标签: git push origin v3.5.0

5. 合并回 develop

6. 删除 release 分支
```

### 热修复流程

```
1. 从 main 创建 hotfix 分支
   git checkout main
   git pull origin main
   git checkout -b hotfix/critical-fix

2. 修复问题并提交

3. 创建 PR 到 main 和 develop

4. 审查通过后合并
   - 更新补丁版本号
   - 打上标签

5. 删除 hotfix 分支
```

## 提交信息规范

遵循 [Conventional Commits](https://www.conventionalcommits.org/) 规范：

```
<type>(<scope>): <subject>

<body>

<footer>
```

### 类型 (type)

- `feat`: 新功能
- `fix`: 修复
- `docs`: 文档变更
- `style`: 代码格式（不影响功能）
- `refactor`: 重构
- `perf`: 性能优化
- `test`: 测试相关
- `chore`: 构建/工具变更
- `ci`: CI/CD 变更

### 范围 (scope)

- `solver`: 求解器模块
- `proof`: 证明系统
- `graph`: 约束图
- `stream`: 流式输出
- `gui`: Web GUI
- `docs`: 文档
- `build`: 构建系统

### 示例

```
feat(solver): 新增 Groebner 基增量求解

实现增量求解算法，仅重解脏变量子图，
提高大规模约束系统的求解效率。

Closes #123
```

## 分支保护规则

### main 分支

- [x] 禁止强制推送
- [x] 禁止删除
- [x] 要求 PR 审查（最少 2 人）
- [x] 要求状态检查通过
- [x] 要求分支为最新
- [x] 要求线性历史

### develop 分支

- [x] 禁止强制推送
- [x] 禁止删除
- [x] 要求 PR 审查（最少 1 人）
- [x] 要求状态检查通过

### release/* 分支

- [x] 禁止强制推送
- [x] 要求 PR 审查（最少 2 人）
- [x] 要求状态检查通过

## 清理策略

- 功能分支合并后 7 天自动删除
- 修复分支合并后 7 天自动删除
- Release 分支发布后保留 30 天
- 定期清理超过 90 天未更新的分支

## 相关文档

- [CONTRIBUTING.md](../CONTRIBUTING.md) - 贡献指南
- [COMMIT_CONVENTION.md](../COMMIT_CONVENTION.md) - 提交规范
- [SECURITY.md](../SECURITY.md) - 安全策略
