# Mathport 参考文档：Lean 3 到 Lean 4 迁移工具

## 文档信息

- **文档版本**: 1.0
- **创建日期**: 2026-05-25
- **适用项目**: Lv-00 数学几何证明系统
- **参考来源**: leanprover-community/mathport

---

## 1. 项目概述

### 1.1 项目简介

Mathport 是由 Lean 官方团队开发的自动化迁移工具，专门用于将 Lean 3 编写的形式化数学证明代码迁移至 Lean 4。该工具在数学库 mathlib 从 Lean 3 向 Lean 4 的大规模迁移过程中发挥了核心作用，成功处理了超过 100 万行形式化代码的自动转换工作。

Mathport 通过自动化的语法分析、抽象语法树（AST）转换和代码生成，实现了形式化证明代码的版本升级自动化。

### 1.2 技术栈

| 组件 | 技术 | 用途 |
|------|------|------|
| 核心引擎 | Lean 4 | 主程序逻辑、AST 处理 |
| 解析器 | Lean 3 兼容层 | 解析 Lean 3 源代码 |
| 性能优化 | Rust | 高性能二进制数据处理 |
| 构建系统 | Lake | Lean 4 包管理 |
| 测试框架 | Lean 4 Test | 回归测试套件 |

Mathport 采用混合技术栈：核心转换逻辑使用 Lean 4 编写，充分利用其对元编程的内置支持；性能敏感的二进制数据处理模块使用 Rust 实现。

### 1.3 社区活跃度

- **GitHub 仓库**: https://github.com/leanprover-community/mathport
- **开发状态**: 活跃维护，随 Lean 版本更新持续迭代
- **主要贡献者**: Lean 官方团队成员及 mathlib 维护者
- **用户群体**: 数学形式化研究者、依赖 mathlib 的项目开发者
- **Issue 响应**: 通常在 1-2 周内获得维护者回应

### 1.4 许可证

Mathport 采用 **Apache License 2.0** 开源许可，允许商业使用、修改和分发，与 Lv-00 项目兼容。

---

## 2. 核心借鉴点

### 2.1 架构设计借鉴

Mathport 采用分层流水线架构处理代码迁移：

```
Lean 3 源代码
      |
      v
[解析层] Lean 3 解析器 → AST
      |
      v
[转换层] AST 转换器 → Lean 4 AST
      |
      v
[生成层] 代码生成器 → Lean 4 源代码
      |
      v
[修复层] 后处理器 → 可编译代码
      |
      v
  Lean 4 代码
```

### 2.2 Mathport 与 Lv-00 第 6 层对照表

| Mathport 特性 | 功能描述 | Lv-00 第 6 层映射 | 借鉴优先级 |
|---------------|----------|-------------------|------------|
| Synport | 语法级移植，处理 Lean 3 到 Lean 4 的语法转换 | 证明格式解析器 | 高 |
| Binport | 二进制数据移植，处理 .olean 文件 | 几何数据序列化 | 中 |
| Tactic 转换 | 自动将 Lean 3 tactic 转换为 Lean 4 等价物 | 证明策略标准化 | 高 |
| 错误报告系统 | 详细的迁移错误分类和定位 | 导出错误诊断 | 中 |
| AST 中间表示 | 语言无关的抽象语法树 | 证明中间表示 | 高 |
| 增量处理 | 仅处理变更文件 | 增量导出 | 低 |
| 批处理模式 | 大规模项目批量迁移 | 批量几何证明导出 | 中 |

### 2.3 关键技术借鉴

**抽象语法树（AST）中间表示**：Mathport 定义了语言无关的 AST 表示，使语法转换与代码生成解耦。Lv-00 可借鉴此设计，定义几何证明的中间表示格式，实现向多种目标格式（HTML/LaTeX/Coq）的转换。

**错误恢复机制**：面对无法自动转换的代码，Mathport 采用错误分类、降级处理、上下文保留和位置映射等策略，Lv-00 可借鉴实现健壮的导出错误处理。

---

## 3. Lv-00 映射方案

### 3.1 架构映射

基于 Mathport 的设计，Lv-00 第 6 层可采用以下架构：

```
Lv-00 内部证明表示
         |
         v
[解析层] 证明对象解析器 → 证明 AST
         |
         v
[转换层] 格式转换器 → 目标格式 AST
         |
         v
[生成层] 代码生成器 → 目标格式代码
         |
         v
[后处理] 格式化/优化 → 最终输出
         |
         v
  HTML / LaTeX / Coq / Lean
```

### 3.2 C 代码示例

#### 3.2.1 证明 AST 节点定义

```c
/* proof_ast.h - 证明抽象语法树定义 */
#ifndef PROOF_AST_H
#define PROOF_AST_H

#include <stddef.h>

typedef enum {
    AST_NODE_THEOREM,      /* 定理声明 */
    AST_NODE_LEMMA,        /* 引理 */
    AST_NODE_PROOF_STEP,   /* 证明步骤 */
    AST_NODE_TACTIC,       /* 策略应用 */
    AST_NODE_EXPRESSION,   /* 表达式 */
    AST_NODE_IDENTIFIER,   /* 标识符 */
    AST_NODE_LITERAL       /* 字面量 */
} AstNodeType;

typedef struct AstNode {
    AstNodeType type;
    char *name;                    /* 节点名称 */
    struct AstNode **children;     /* 子节点数组 */
    size_t child_count;            /* 子节点数量 */
    char *source_location;         /* 源位置信息 */
    void *annotation;              /* 附加元数据 */
} AstNode;

AstNode* ast_create_node(AstNodeType type, const char *name);
void ast_add_child(AstNode *parent, AstNode *child);
void ast_free(AstNode *node);

#endif /* PROOF_AST_H */
```

#### 3.2.2 证明解析器实现

```c
/* proof_parser.c - 证明解析器 */
#include "proof_ast.h"
#include "lv00_proof_engine.h"
#include <stdlib.h>
#include <string.h>

/* 将 Lv-00 内部证明转换为 AST */
AstNode* lv00_proof_to_ast(const ProofObject *proof) {
    if (proof == NULL) return NULL;
    
    AstNode *root = ast_create_node(AST_NODE_THEOREM, proof->theorem_name);
    
    /* 转换假设 */
    for (size_t i = 0; i < proof->hypothesis_count; i++) {
        AstNode *hyp = ast_create_node(AST_NODE_EXPRESSION, 
                                        proof->hypotheses[i].name);
        hyp->annotation = (void*)proof->hypotheses[i].type;
        ast_add_child(root, hyp);
    }
    
    /* 转换证明步骤 */
    ProofStep *step = proof->first_step;
    while (step != NULL) {
        AstNode *step_node = convert_proof_step(step);
        ast_add_child(root, step_node);
        step = step->next;
    }
    
    return root;
}

/* 转换单个证明步骤 */
static AstNode* convert_proof_step(const ProofStep *step) {
    AstNode *node = ast_create_node(AST_NODE_PROOF_STEP, NULL);
    
    /* 转换策略应用 */
    AstNode *tactic = ast_create_node(AST_NODE_TACTIC, step->tactic_name);
    for (size_t i = 0; i < step->argument_count; i++) {
        AstNode *arg = convert_expression(step->arguments[i]);
        ast_add_child(tactic, arg);
    }
    ast_add_child(node, tactic);
    
    /* 记录源位置用于错误报告 */
    node->source_location = strdup(step->location);
    
    return node;
}
```

#### 3.2.3 格式转换器接口

```c
/* format_converter.h - 格式转换器接口 */
#ifndef FORMAT_CONVERTER_H
#define FORMAT_CONVERTER_H

#include "proof_ast.h"

typedef enum {
    FORMAT_HTML,      /* HTML 可视化 */
    FORMAT_LATEX,     /* LaTeX 论文 */
    FORMAT_COQ,       /* Coq 证明助手 */
    FORMAT_LEAN4      /* Lean 4 代码 */
} OutputFormat;

typedef struct {
    OutputFormat format;
    const char *extension;
    union {
        struct { int include_css; } html;
        struct { const char *document_class; } latex;
        struct { int compatible_mode; } coq;
        struct { int use_mathlib4; } lean4;
    } config;
} ConverterConfig;

typedef struct {
    char *output_buffer;
    size_t buffer_size;
    int error_count;
    char **error_messages;
} ConversionResult;

ConversionResult* convert_proof(const AstNode *ast, 
                                 const ConverterConfig *config);
void free_conversion_result(ConversionResult *result);

#endif /* FORMAT_CONVERTER_H */
```

#### 3.2.4 HTML 转换器实现

```c
/* html_converter.c - HTML 格式转换器 */
#include "format_converter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ConversionResult* convert_to_html(const AstNode *ast, 
                                   const ConverterConfig *config) {
    ConversionResult *result = calloc(1, sizeof(ConversionResult));
    result->output_buffer = malloc(4096);
    result->buffer_size = 4096;
    result->output_buffer[0] = '\0';
    
    append_html_header(&result->output_buffer, &result->buffer_size);
    ast_to_html(ast, &result->output_buffer, &result->buffer_size);
    append_html_footer(&result->output_buffer, &result->buffer_size);
    
    return result;
}

static void ast_to_html(const AstNode *node, char **buffer, size_t *size) {
    if (node == NULL) return;
    
    switch (node->type) {
        case AST_NODE_THEOREM:
            appendf(buffer, size, "<div class=\"theorem\">\n");
            appendf(buffer, size, "  <h3>定理: %s</h3>\n", node->name);
            for (size_t i = 0; i < node->child_count; i++) {
                ast_to_html(node->children[i], buffer, size);
            }
            appendf(buffer, size, "</div>\n");
            break;
            
        case AST_NODE_PROOF_STEP:
            appendf(buffer, size, "  <div class=\"proof-step\">\n");
            for (size_t i = 0; i < node->child_count; i++) {
                ast_to_html(node->children[i], buffer, size);
            }
            appendf(buffer, size, "  </div>\n");
            break;
            
        case AST_NODE_TACTIC:
            appendf(buffer, size, "    <span class=\"tactic\">%s</span>\n", 
                    node->name);
            for (size_t i = 0; i < node->child_count; i++) {
                appendf(buffer, size, "    <span class=\"argument\">");
                ast_to_html(node->children[i], buffer, size);
                appendf(buffer, size, "</span>\n");
            }
            break;
            
        case AST_NODE_EXPRESSION:
            appendf(buffer, size, "<code>%s</code>", node->name);
            break;
            
        default:
            break;
    }
}
```

#### 3.2.5 LaTeX 转换器实现

```c
/* latex_converter.c - LaTeX 格式转换器 */
#include "format_converter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* LATEX_PREAMBLE = 
    "\\documentclass{article}\n"
    "\\usepackage{amsmath}\n"
    "\\usepackage{amsthm}\n"
    "\\newtheorem{theorem}{定理}\n"
    "\\begin{document}\n";

static const char* LATEX_POSTAMBLE = "\\end{document}\n";

ConversionResult* convert_to_latex(const AstNode *ast,
                                    const ConverterConfig *config) {
    ConversionResult *result = calloc(1, sizeof(ConversionResult));
    result->output_buffer = malloc(8192);
    result->buffer_size = 8192;
    
    strncat(result->output_buffer, LATEX_PREAMBLE, 
            result->buffer_size - strlen(result->output_buffer) - 1);
    ast_to_latex(ast, &result->output_buffer, &result->buffer_size);
    strncat(result->output_buffer, LATEX_POSTAMBLE,
            result->buffer_size - strlen(result->output_buffer) - 1);
    
    return result;
}

static void ast_to_latex(const AstNode *node, char **buffer, size_t *size) {
    if (node == NULL) return;
    
    switch (node->type) {
        case AST_NODE_THEOREM:
            appendf(buffer, size, "\\begin{theorem}\n");
            if (node->name) {
                appendf(buffer, size, "[%s]\n", node->name);
            }
            for (size_t i = 0; i < node->child_count; i++) {
                ast_to_latex(node->children[i], buffer, size);
            }
            appendf(buffer, size, "\\end{theorem}\n");
            break;
            
        case AST_NODE_PROOF_STEP:
            appendf(buffer, size, "\\begin{proof}\n");
            for (size_t i = 0; i < node->child_count; i++) {
                ast_to_latex(node->children[i], buffer, size);
            }
            appendf(buffer, size, "\\end{proof}\n");
            break;
            
        case AST_NODE_TACTIC:
            appendf(buffer, size, "应用策略 \\texttt{%s}", node->name);
            if (node->child_count > 0) {
                appendf(buffer, size, " 参数: ");
                for (size_t i = 0; i < node->child_count; i++) {
                    ast_to_latex(node->children[i], buffer, size);
                    if (i < node->child_count - 1) {
                        appendf(buffer, size, ", ");
                    }
                }
            }
            appendf(buffer, size, "\\n");
            break;
            
        case AST_NODE_EXPRESSION:
            appendf(buffer, size, "$%s$", node->name);
            break;
            
        default:
            break;
    }
}
```

#### 3.2.6 Coq 导出转换器

```c
/* coq_converter.c - Coq 证明助手导出 */
#include "format_converter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Lv-00 策略到 Coq tactic 的映射表 */
static struct {
    const char *lv00_name;
    const char *coq_name;
} tactic_mapping[] = {
    {"intro", "intros"},
    {"apply", "apply"},
    {"rewrite", "rewrite"},
    {"simpl", "simpl"},
    {"reflexivity", "reflexivity"},
    {"induction", "induction"},
    {"cases", "destruct"},
    {"split", "split"},
    {"left", "left"},
    {"right", "right"},
    {NULL, NULL}
};

static const char* find_coq_tactic(const char *lv00_tactic) {
    for (int i = 0; tactic_mapping[i].lv00_name != NULL; i++) {
        if (strcmp(tactic_mapping[i].lv00_name, lv00_tactic) == 0) {
            return tactic_mapping[i].coq_name;
        }
    }
    return lv00_tactic;
}

ConversionResult* convert_to_coq(const AstNode *ast,
                                  const ConverterConfig *config) {
    ConversionResult *result = calloc(1, sizeof(ConversionResult));
    result->output_buffer = malloc(4096);
    result->buffer_size = 4096;
    result->error_messages = malloc(10 * sizeof(char*));
    
    appendf(&result->output_buffer, &result->buffer_size,
            "(* 由 Lv-00 自动生成的 Coq 代码 *)\n");
    
    ast_to_coq(ast, &result->output_buffer, &result->buffer_size, result);
    
    return result;
}

static void ast_to_coq(const AstNode *node, char **buffer, size_t *size,
                       ConversionResult *result) {
    if (node == NULL) return;
    
    switch (node->type) {
        case AST_NODE_THEOREM:
            appendf(buffer, size, "Theorem %s : ", node->name);
            for (size_t i = 0; i < node->child_count; i++) {
                if (node->children[i]->type == AST_NODE_EXPRESSION) {
                    appendf(buffer, size, "%s -> ", node->children[i]->name);
                }
            }
            appendf(buffer, size, "conclusion.\nProof.\n");
            for (size_t i = 0; i < node->child_count; i++) {
                if (node->children[i]->type == AST_NODE_PROOF_STEP) {
                    ast_to_coq(node->children[i], buffer, size, result);
                }
            }
            appendf(buffer, size, "Qed.\n\n");
            break;
            
        case AST_NODE_TACTIC: {
            const char *coq_tac = find_coq_tactic(node->name);
            appendf(buffer, size, "  %s", coq_tac);
            
            if (strcmp(coq_tac, node->name) == 0) {
                result->error_messages[result->error_count++] =
                    strdup_printf("警告: 策略 '%s' 未映射", node->name);
            }
            
            for (size_t i = 0; i < node->child_count; i++) {
                appendf(buffer, size, " ");
                ast_to_coq(node->children[i], buffer, size, result);
            }
            appendf(buffer, size, ".\n");
            break;
        }
            
        case AST_NODE_EXPRESSION:
            appendf(buffer, size, "(%s)", node->name);
            break;
            
        default:
            break;
    }
}
```

### 3.3 错误处理与报告

```c
/* error_reporter.h - 错误报告系统 */
#ifndef ERROR_REPORTER_H
#define ERROR_REPORTER_H

typedef enum {
    ERR_SYNTAX,       /* 语法错误 */
    ERR_SEMANTIC,     /* 语义错误 */
    ERR_UNSUPPORTED,  /* 不支持的特性 */
    ERR_CONVERSION,   /* 转换错误 */
    ERR_IO            /* IO 错误 */
} ErrorType;

typedef struct {
    ErrorType type;
    char *location;      /* 源位置 */
    char *message;       /* 错误描述 */
    char *suggestion;    /* 修复建议 */
    int severity;        /* 严重级别 1-5 */
} ConversionError;

typedef struct {
    ConversionError *errors;
    size_t count;
    size_t capacity;
} ErrorReport;

void error_report_init(ErrorReport *report);
void error_report_add(ErrorReport *report, ErrorType type,
                      const char *location, const char *message,
                      const char *suggestion, int severity);
void error_report_print(const ErrorReport *report, FILE *output);
void error_report_free(ErrorReport *report);

#endif /* ERROR_REPORTER_H */
```

---

## 4. 实现路线图

### 4.1 阶段划分

| 阶段 | 时间范围 | 目标 | 关键交付物 |
|------|----------|------|------------|
| 短期 | 1-2 个月 | 基础框架 | AST 定义、HTML 导出 |
| 中期 | 3-6 个月 | 多格式支持 | LaTeX、Coq 导出 |
| 长期 | 6-12 个月 | 高级特性 | Lean 4 导出、增量更新 |

### 4.2 短期目标（1-2 个月）

**核心任务**：
1. 设计并实现证明 AST 数据结构
2. 实现 Lv-00 内部证明到 AST 的解析器
3. 开发 HTML 导出转换器，支持基本可视化
4. 建立错误报告框架

**验收标准**：
- 能够导出简单几何定理为 HTML
- 错误报告包含位置信息和修复建议
- 单元测试覆盖率 > 80%

### 4.3 中期目标（3-6 个月）

**核心任务**：
1. 实现 LaTeX 导出转换器
2. 实现 Coq 导出转换器，支持策略映射
3. 开发配置系统，支持自定义模板
4. 优化性能，支持大规模证明导出

**验收标准**：
- LaTeX 输出可直接编译为 PDF
- Coq 输出可在 Coq 8.x 中验证
- 支持 1000+ 行证明的批量导出

### 4.4 长期目标（6-12 个月）

**核心任务**：
1. 实现 Lean 4 导出转换器
2. 开发增量导出机制
3. 实现证明差异比较工具
4. 集成到 Lv-00 可视化引擎

**验收标准**：
- Lean 4 输出可在 Lean 4 中编译
- 增量导出时间 < 完整导出的 10%
- 可视化引擎集成完成

---

## 5. 附录

### 5.1 关键 API 列表

```c
/* ast.h - AST 操作 API */
AstNode* ast_create_node(AstNodeType type, const char *name);
void ast_destroy_node(AstNode *node);
void ast_add_child(AstNode *parent, AstNode *child);
AstNode* ast_clone(const AstNode *node);
void ast_traverse(AstNode *node, void (*callback)(AstNode*));

/* converter.h - 转换器 API */
Converter* converter_create(OutputFormat format);
void converter_destroy(Converter *converter);
void converter_set_option(Converter *converter, const char *key, 
                          const char *value);
ConversionResult* converter_run(Converter *converter, const AstNode *ast);

/* export_manager.h - 导出管理 API */
ExportManager* export_manager_create(void);
void export_manager_destroy(ExportManager *manager);
int export_manager_register_converter(ExportManager *manager, 
                                       OutputFormat format,
                                       ConverterFactory factory);
ConversionResult* export_manager_export(ExportManager *manager,
                                         const ProofObject *proof,
                                         OutputFormat format);
```

### 5.2 参考文献

1. **Mathport 官方仓库**
   - URL: https://github.com/leanprover-community/mathport
   - 描述: Lean 3 到 Lean 4 的自动迁移工具源代码

2. **Lean 4 官方文档**
   - URL: https://lean-lang.org/lean4/doc/
   - 描述: Lean 4 编程语言和定理证明器文档

3. **Mathlib4 迁移指南**
   - URL: https://github.com/leanprover-community/mathlib4/wiki/Porting-Guide
   - 描述: mathlib 从 Lean 3 到 Lean 4 的迁移经验总结

4. **The Lean 4 Theorem Prover (论文)**
   - 作者: Leonardo de Moura, Sebastian Ullrich
   - 会议: CADE 2021
   - 描述: Lean 4 的设计理念和实现细节

5. **Coq 参考手册**
   - URL: https://coq.inria.fr/doc/
   - 描述: Coq 证明助手的官方文档

---

## 文档修订历史

| 版本 | 日期 | 修订内容 | 作者 |
|------|------|----------|------|
| 1.0 | 2026-05-25 | 初始版本 | Lv-00 团队 |

---

*本文档基于 Mathport 开源项目分析编写，仅供 Lv-00 项目内部参考使用。*
