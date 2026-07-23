# Lv-00 提交规范指南

> 本仓库遵循 [Conventional Commits](https://www.conventionalcommits.org/) 规范

---

## 提交消息格式

```
<type>(<scope>): <subject>

[optional body]

[optional footer(s)]
```

### 格式说明

| 部分 | 必需 | 说明 |
|------|------|------|
| `type` | ✅ | 提交类型（见下方列表） |
| `scope` | ❌ | 影响范围（模块名） |
| `subject` | ✅ | 简短描述（50字符以内） |
| `body` | ❌ | 详细描述（72字符换行） |
| `footer` | ❌ | 关联 issue、破坏性变更等 |

---

## 提交类型 (Type)

| 类型 | 用途 | 示例 |
|------|------|------|
| `feat` | 新功能 | `feat(constraint): add Groebner basis solver` |
| `fix` | 错误修复 | `fix(parser): handle empty expression` |
| `docs` | 文档更新 | `docs(api): add missing parameter docs` |
| `style` | 代码格式（不影响逻辑） | `style(core): fix indentation` |
| `refactor` | 代码重构 | `refactor(geometry): extract common utils` |
| `perf` | 性能优化 | `perf(solver): optimize AABB tree query` |
| `test` | 测试相关 | `test(axiom): add category theory tests` |
| `chore` | 构建/工具/维护 | `chore(ci): update GitHub Actions` |
| `build` | 构建系统变更 | `build(cmake): add Windows support` |
| `ci` | CI/CD 配置 | `ci: add code coverage workflow` |

---

## 作用域 (Scope)

作用域表示变更影响的模块或组件：

| 作用域 | 说明 |
|--------|------|
| `core` | 核心引擎 |
| `parser` | 解析器 (layer1) |
| `resource` | 资源管理 (layer2) |
| `geometry` | 几何引擎 (layer3) |
| `reasoning` | 推理引擎 (layer4) |
| `axiom` | 公理系统 |
| `constraint` | 约束求解 |
| `proof` | 证明系统 |
| `formal` | Lean4 形式化验证 |
| `python` | Python 绑定 |
| `web` | Web 前端 |
| `test` | 测试框架 |
| `doc` | 文档 |
| `repo` | 仓库维护 |

---

## 完整示例

### 简单提交
```
feat(constraint): implement polynomial GCD algorithm
```

### 带详细描述的提交
```
feat(geometry): add AABB tree spatial index

- Implement bounding box hierarchy for fast collision detection
- Support dynamic insertion and removal
- Add query methods: intersect, contain, nearest

Closes #234
```

### 破坏性变更
```
refactor(api): rename lv_init to lv_context_create

BREAKING CHANGE: lv_init() is removed. Use lv_context_create()
instead with the new configuration structure.

Migration guide:
  Before: lv_init(NULL);
  After:  lv_context_create(&config);
```

### 修复 bug
```
fix(solver): prevent division by zero in constraint solving

When all coefficients are zero, the solver would crash with
SIGFPE. Now returns lv_ERROR_DEGENERATE_CASE.

Fixes #456
```

---

## 提交最佳实践

### ✅ 应该做的

- 使用祈使语气（"add" 而非 "added" 或 "adds"）
- 首字母小写（`feat:` 而非 `Feat:`）
- 结尾不加句号
- 限制 subject 在 50 字符以内
- 在 body 中解释 **为什么** 而不是 **做了什么**（代码本身说明做了什么）
- 使用 body 详细说明设计决策
- 使用 footer 引用相关 issue

### ❌ 不应该做的

```
# 不好的示例
feat: update                    # 太模糊
feat: Added new feature         # 过去时，首字母大写
fix: fixed bug.                 # 结尾有句号
feat(geometry): implement the   # subject 太长
```

---

## 分支提交策略

| 分支 | 提交规范 |
|------|----------|
| `main` | 仅接受 PR 合并，提交必须规范 |
| `dev` | 功能开发分支，提交建议规范 |
| `exp` | 实验分支，可宽松但建议规范 |

---

## 工具支持

### 使用 commitlint 检查（可选）

```bash
# 安装 commitlint
npm install --save-dev @commitlint/config-conventional @commitlint/cli

# 配置 .commitlintrc.json
{
  "extends": ["@commitlint/config-conventional"]
}
```

### Git hook 自动检查

```bash
# 安装 husky
npx husky add .husky/commit-msg 'npx --no -- commitlint --edit ${1}'
```

---

## 参考

- [Conventional Commits 官方规范](https://www.conventionalcommits.org/)
- [Angular 提交规范](https://github.com/angular/angular/blob/main/CONTRIBUTING.md#-commit-message-format)
- [Semantic Versioning](https://semver.org/)

---

*最后更新：2026-05-28*
