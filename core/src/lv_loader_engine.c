/**
 * @file lv_loader_engine.c
 * @brief .lv 装载入图实现（F24/I5 P0-②：从 lv_loader.c 拆分）
 *
 * @details lv_loader.c 只产 AST（解析/验证，L1）；本文件（L0 编排层）
 *          实现「装载入图」——把 LvParseResult 应用到 lvEngine（L3/L4），
 *          解除 lv_layer1_parser 对 L3/L4 的链接依赖（设计 L1150-②）。
 *
 *          迁移自 lv_loader.c 的装载侧：名称映射表（loader_names）、
 *          声明处理（decl_* / process_declaration）、
 *          lv_apply_parse_result（三遍处理 + 约束添加）。
 *
 * @version 1.0.0
 */

#include "lv/lv_loader.h" /* lv_apply_parse_result / LvParseResult / LvSemaContext */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/lv_ast.h"
#include "lv/lv_parser.h"
#include "lv/lv_sema.h"
#include "lv/engine.h"          /* lvEngine / engine_get_main_graph（L4） */
#include "lv/lv.h"              /* lv_add_line_segment（L0 便利头） */
#include "lv/constraint_graph.h" /* graph_add_constraint_with_id（L3） */
#include "lv/lv_registry.h"     /* lv_REGISTRY_STATIC 名称映射表 */
#include "lv/lv_internal.h"     /* lv_loader_reset 声明 */
#include "lv/lv_utils.h"
#include "lv/lv_str_utils.h"

/* F24/I5：本文件属 L0 编排层，允许依赖 L1-L10 全部层（lv_ALLOW_LAYER 省略，
 * L0 便利层 CAN_DEPEND 允许所有）。 */

/**
 * @brief 名称映射表（通用注册表设施：key = 实体名称，value = boxed int 引擎节点 ID）。
 *
 * 将 AST 中声明的实体名称与引擎内部的节点 ID 关联，
 * 用于在后续约束和证明语句中引用已声明的实体。
 * 文件级单例（lv_once 惰性初始化，线程安全）；strcmp 查重、内部 name 拷贝、
 * 动态扩容与清空均由 lv_registry 承担。
 *
 * 语义保持：重复添加同名实体时首次映射生效（原线性查找"首个匹配优先"），
 * 容量上限 LV_MAX_NAMED_ENTITIES 与原实现一致（满时静默忽略）。
 */
lv_REGISTRY_STATIC(loader_names, 32);

/** @brief 名称映射表最大容量 */
#define LV_MAX_NAMED_ENTITIES 256

/** @brief 装箱引擎节点 ID（注册表 value） */
static void *loader_box_id(int node_id) {
    int *p = (int *) lv_malloc(sizeof(int));
    if (p) {
        *p = node_id;
    }
    return p;
}

/** @brief boxed 节点 ID 的注册表 destroy 回调适配器（void(*)(void*) 形态） */
static void loader_box_destroy(void *value) {
    lv_free((void **) &value);
}

/**
 * @brief 清空名称映射表
 *
 * 释放所有条目（destroy 回调 + 内部 name），保留注册表结构可继续使用。
 */
static void loader_names_clear(void) {
    loader_names_ensure();
    lv_registry_clear(&g_loader_names);
}

/**
 * @brief 重置加载器名称映射表（测试进程内隔离用）
 *
 * 清空 static 全局名称映射表 g_loader_names（名称 → 引擎节点 ID）。
 * lv_loader.c 无独立 ID 计数器（引擎节点 ID 由引擎生成，loader 仅记录），
 * 本函数为名称表这一 static 全局状态提供显式重置能力，供测试进程内
 * 隔离使用。正常加载路径（lv_apply_parse_result 每次调用已内部清空）
 * 行为完全不变。
 */
void lv_loader_reset(void) {
    loader_names_clear();
}

/**
 * @brief 向名称映射表添加条目
 *
 * @param name    实体名称
 * @param node_id 引擎节点 ID
 */
static void loader_names_add(const char *name, int node_id) {
    loader_names_ensure();
    if (lv_registry_count(&g_loader_names) >= LV_MAX_NAMED_ENTITIES)
        return;

    void *boxed = loader_box_id(node_id);
    if (!boxed)
        return;
    if (!lv_registry_put_ex(&g_loader_names, name, boxed, loader_box_destroy)) {
        /* 名称重复：保留首次映射（与原线性查找"首个匹配优先"语义一致） */
        lv_free((void **) &boxed);
    }
}

/**
 * @brief 在名称映射表中查找名称
 *
 * 委托注册表按名称查找（strcmp 由 lv_registry 承担）。
 *
 * @param name 实体名称（允许为 NULL，返回 -1）
 * @return 引擎节点 ID，未找到或 name 为 NULL 返回 -1
 */
static int loader_names_lookup(const char *name) {
    if (!name)
        return -1;
    loader_names_ensure();
    void *boxed = lv_registry_get(&g_loader_names, name);
    return boxed ? *(int *) boxed : -1;
}

static bool loader_expr_int(const LvAstNode *node, long long *out) {
    if (node && node->type == LV_AST_INTEGER_LITERAL) {
        *out = node->data.literal.integer_value;
        return true;
    }
    return false;
}

/** 坐标数值节点 → (num, den) 精确有理数对。
 *  支持 INTEGER / RATIONAL / DECIMAL / unary(-)（S1 坐标字面量与既有 {x,y} 共用）。 */
static bool loader_expr_coord(const LvAstNode *node, long long *num, unsigned long *den) {
    if (!node)
        return false;
    if (node->type == LV_AST_INTEGER_LITERAL) {
        *num = node->data.literal.integer_value;
        *den = 1;
        return true;
    }
    if (node->type == LV_AST_RATIONAL_LITERAL) {
        *num = node->data.literal.rational_value.num;
        *den = (unsigned long) node->data.literal.rational_value.den;
        if (*den == 0)
            return false;
        return true;
    }
    /* LV_AST_DECIMAL_LITERAL：批次 D 起 parser 不再产出（十进制文本已在解析期
     * 精确转为 RATIONAL 节点，见 lv_parser.c decimal_text_to_rational）；
     * 坐标值要求精确有理数——外部 API 若直接构造 decimal 节点于坐标位置，
     * 返回 false（加载报错）而非静默 double 近似。 */
    if (node->type == LV_AST_DECIMAL_LITERAL) {
        return false;
    }
    if (node->type == LV_AST_UNARY_OP) {
        /* 负数：-(N) → num 取负 */
        const char *op = node->data.unary.op;
        if (op && op[0] == '-') {
            long long n2;
            unsigned long d2;
            if (loader_expr_coord(node->data.unary.operand, &n2, &d2)) {
                *num = -n2;
                *den = d2;
                return true;
            }
        }
        return false;
    }
    return false;
}

/**
 * @brief 提取结构化坐标：Point A := {x: N, y: M} / Point A = (1, 2)（S1）
 *
 * STRUCT_LITERAL → child 链表为 FIELD 节点，每个 FIELD 有 name 和 value。
 * 支持字段名 "x"/"y"（及别名 "X"/"Y"）；值支持整数/有理数/小数/负数。
 *
 * @param value AST 声明值节点
 * @param out_x / out_y 输出坐标分子
 * @param out_xd / out_yd 输出坐标分母（新，S1 小数坐标）
 * @param has_x / has_y 输出是否找到对应字段
 */
static void loader_extract_struct_coords(const LvAstNode *value,
                                         long long *out_x, long long *out_y,
                                         unsigned long *out_xd, unsigned long *out_yd,
                                         bool *has_x, bool *has_y) {
    *has_x = *has_y = false;
    if (!value || value->type != LV_AST_STRUCT_LITERAL)
        return;
    for (const LvAstNode *f = value->child; f; f = f->next) {
        if (f->type != LV_AST_STRUCT_FIELD || !f->data.field.name)
            continue;
        const char *fn = f->data.field.name;
        if ((fn[0] == 'x' || fn[0] == 'X') && !*has_x) {
            *has_x = loader_expr_coord(f->data.field.value, out_x, out_xd);
        } else if ((fn[0] == 'y' || fn[0] == 'Y') && !*has_y) {
            *has_y = loader_expr_coord(f->data.field.value, out_y, out_yd);
        }
    }
}

/**
 * @brief 提取几何表达式坐标：Point A := point(x, y)
 *
 * GEOMETRY_EXPR 且 func_name == "point"，参数链表为整数列表。
 * 取前两个整数作为坐标。
 *
 * @param value AST 声明值节点
 * @param out_x / out_y 输出坐标
 * @return 成功提取两个坐标返回 true
 */
static bool loader_extract_point_expr(const LvAstNode *value,
                                      long long *out_x, long long *out_y) {
    if (!value || value->type != LV_AST_GEOMETRY_EXPR)
        return false;
    if (!value->data.call.func_name || !lv_str_eq(value->data.call.func_name, "point"))
        return false;
    const LvAstNode *args = value->data.call.args;
    long long x, y;
    if (!args || !loader_expr_int(args, &x))
        return false;
    args = args->next;
    if (!args || !loader_expr_int(args, &y))
        return false;
    *out_x = x;
    *out_y = y;
    return true;
}

/* ================================================================
 * 处理 Declaration 节点，向引擎添加几何实体
 * ================================================================ */

/** @brief 实体声明处理器函数指针类型 */
typedef void (*EntityDeclHandler)(lvEngine *engine, const char *name);

/** 注册为自由点（默认坐标 (0,1,0,1) 即 (0,0)；带声明值时使用真实坐标） */

static void decl_register_point(lvEngine *engine, const char *name) {
    int id = lv_add_point(engine, 0, 1, 0, 1);
    if (id >= 0)
        loader_names_add(name, id);
}

/**
 * @brief 注册带坐标的点（声明值版本）
 *
 * 支持三种声明值形态：
 *   - Point A := {x: 3, y: 4};   （结构字面量）
 *   - Point A = (1.5, -2);       （S1 坐标字面量 → 同一结构字面量路径）
 *   - Point A := point(3, 4);    （几何表达式）
 * 坐标用 lv_add_point 的精确有理数接口写入（整数/小数/负数均转 num/den）。
 *
 * @param engine 引擎
 * @param name   实体名
 * @param value  声明值 AST（可为 NULL，NULL 时按默认坐标注册）
 */
static void decl_register_point_with_value(lvEngine *engine, const char *name,
                                           const LvAstNode *value) {
    long long x = 0, y = 0;
    unsigned long xd = 1, yd = 1;
    bool has_x = false, has_y = false;
    bool ok = false;

    if (value) {
        /* 形态 1: 结构字面量 {x, y}（含 S1 坐标字面量转换结果） */
        loader_extract_struct_coords(value, &x, &y, &xd, &yd, &has_x, &has_y);
        if (has_x && has_y) {
            ok = true;
        } else {
            /* 形态 2: point(x, y) 几何表达式 */
            long long px = 0, py = 0;
            if (loader_extract_point_expr(value, &px, &py)) {
                x = px; y = py; ok = true;
            }
        }
    }

    int id;
    if (ok && xd > 0 && yd > 0) {
        id = lv_add_point(engine, (int64_t) x, (uint64_t) xd, (int64_t) y, (uint64_t) yd);
    } else {
        id = lv_add_point(engine, 0, 1, 0, 1);
    }
    if (id >= 0)
        loader_names_add(name, id);
}

/** 暂存名称（-1 表示尚未关联端点；未支持类型同样走暂存而非报错，保留原 default 折叠语义） */
static void decl_stash(lvEngine *engine, const char *name) {
    (void) engine;
    loader_names_add(name, -1);
}

/** @brief 实体类型 → 声明处理器 VTable（按枚举索引；显式列出全部类型，未支持类型走暂存） */
static const EntityDeclHandler kEntityDeclHandlers[] = {
    [LV_ENTITY_POINT]       = decl_register_point,
    [LV_ENTITY_LINE]        = decl_stash,
    [LV_ENTITY_CIRCLE]      = decl_stash,
    [LV_ENTITY_SEGMENT]     = decl_stash,
    [LV_ENTITY_RAY]         = decl_stash,
    [LV_ENTITY_ANGLE]       = decl_stash,
    [LV_ENTITY_TRIANGLE]    = decl_stash,
    [LV_ENTITY_POLYGON]     = decl_stash,
    [LV_ENTITY_SCALAR]      = decl_stash,
    [LV_ENTITY_BOOL]        = decl_stash,
    [LV_ENTITY_PROPOSITION] = decl_stash,
    [LV_ENTITY_PROOF]       = decl_stash,
};

/**
 * @brief 处理 AST 声明节点，向引擎添加几何实体
 *
 * 解析 Declaration 节点的实体类型和名称列表，根据实体类型
 * 执行对应的引擎添加操作（如添加点、预留线段/直线名称等）。
 * 名称列表为逗号分隔的字符串。
 *
 * 带声明值（":= Expr"）时：
 *   - Point A := {x: N, y: M}; / Point A := point(N, M);  → 真实坐标
 *   - Line L := line(a, b);    → 创建线段（第二遍端点已知后）
 *   - 其他类型 → 暂存名称（与无值一致）
 *
 * @param engine 引擎指针
 * @param node   AST 声明节点
 */
static void process_declaration(lvEngine *engine, LvAstNode *node) {
    LvEntityType etype = (LvEntityType) node->data.decl.entity_type;
    const char *names = node->data.decl.names;
    if (!names || !engine)
        return;

    /* 复制 names 用于拆分 */
    char buf[1024];
    lv_strlcpy(buf, names, sizeof(buf));

    /* VTable 调度（越界/未登记类型走暂存，对应原 default 分支） */
    EntityDeclHandler handler = decl_stash;
    if ((unsigned)etype < (unsigned)lv_ARRAY_SIZE(kEntityDeclHandlers))
        handler = kEntityDeclHandlers[etype];

    /* 拆分逗号分隔的名称列表 */
    char *save;
    char *tok = lv_strtok_r(buf, ",", &save);
    while (tok) {
        /* 带声明值且为 Point 时走坐标注册；声明值仅为单个名称服务 */
        if (node->data.decl.value && etype == LV_ENTITY_POINT) {
            decl_register_point_with_value(engine, tok, node->data.decl.value);
        } else {
            handler(engine, tok);
        }
        tok = lv_strtok_r(NULL, ",", &save);
    }
}

/* ================================================================
 * 公共 API
 * ================================================================ */

/**
 * @brief 加载并解析一个 .lv 文件
 *
 * 完整的文件加载管线：读取文件 → 创建词法分析器 → 创建解析器 →
 * 解析为 AST → 释放临时资源 → 返回解析结果。
 *
 * @param filepath .lv 文件路径
 * @return 解析结果结构体，包含 AST 和可能的错误信息
 */

/**
 * @brief 将解析结果应用到引擎
 *
 * 三遍处理策略：
 * 1. 第一遍：处理声明，向引擎添加几何对象（点等）
 * 2. 第二遍：处理需要端点已知的实体（线段/直线：Line L := line(a, b);
 *    / Segment S := segment(a, b);，端点在第一遍注册后此处创建线段）
 * 3. 第三遍：处理约束和证明语句
 *
 * @param engine 引擎指针
 * @param result 解析结果
 * @param sema   语义分析上下文（当前保留供将来扩展）
 * @return 应用成功返回 true，失败返回 false
 */
bool lv_apply_parse_result(lvEngine *engine, const LvParseResult *result, LvSemaContext *sema) {
    if (!engine || !result || !result->ast)
        return false;

    loader_names_clear();

    /* 使用 sema 进行语义验证 */
    if (sema) {
        if (lv_sema_error_count(sema) > 0) {
            return false;
        }
    }

    LvAstNode *ast = result->ast;
    if (ast->type != LV_AST_PROGRAM)
        return false;

    /* 第一遍：处理声明（添加几何对象到引擎） */
    for (LvAstNode *stmt = ast->child; stmt; stmt = stmt->next) {
        if (stmt->type == LV_AST_DECLARATION) {
            process_declaration(engine, stmt);
        }
    }

    /* 第二遍：处理声明之后的线段/直线（如果端点已知） */
    for (LvAstNode *stmt = ast->child; stmt; stmt = stmt->next) {
        if (stmt->type != LV_AST_DECLARATION)
            continue;
        LvEntityType etype = (LvEntityType) stmt->data.decl.entity_type;
        if (etype != LV_ENTITY_SEGMENT && etype != LV_ENTITY_LINE)
            continue;
        if (!stmt->data.decl.value)
            continue; /* 无构造值，无法创建 */

        /* 支持 Line L := line(a, b); / Segment S := line(a, b); / segment(a, b);
         * 构造值形态：GEOMETRY_EXPR，func_name ∈ {line, segment}，参数为两个标识符 */
        const LvAstNode *v = stmt->data.decl.value;
        if (v->type != LV_AST_GEOMETRY_EXPR)
            continue;
        const char *fn = v->data.call.func_name;
        if (!fn || !(lv_str_eq(fn, "line") || lv_str_eq(fn, "segment")))
            continue;
        const LvAstNode *a0 = v->data.call.args;
        const LvAstNode *a1 = a0 ? a0->next : NULL;
        if (!a0 || !a1 || a0->type != LV_AST_IDENTIFIER_EXPR || a1->type != LV_AST_IDENTIFIER_EXPR)
            continue;

        int p1 = loader_names_lookup(a0->data.ident.name);
        int p2 = loader_names_lookup(a1->data.ident.name);
        if (p1 < 0 || p2 < 0)
            continue; /* 端点未注册，跳过 */

        int seg_id = lv_add_line_segment(engine, p1, p2);
        if (seg_id >= 0) {
            /* 名称列表拆分（声明值只作用于首个名称，此处为单名声明场景） */
            char buf[1024];
            lv_strlcpy(buf, stmt->data.decl.names, sizeof(buf));
            char *save;
            char *tok = lv_strtok_r(buf, ",", &save);
            if (tok)
                loader_names_add(tok, seg_id);
        }
    }

    /* 第三遍：处理 Constraint 和 Prove 语句 */
    for (LvAstNode *stmt = ast->child; stmt; stmt = stmt->next) {
        if (stmt->type == LV_AST_CONSTRAINT_STMT) {
            LvAstNode *expr = stmt->data.stmt.expr;
            if (!expr)
                continue;

            /* ── 形态 1: dist(a, b) = N; —— 距离约束
             *   AST: COMPARE(op==, left={MEASURE|FUNCTION_CALL}{dist/distance, args=[a,b]}, right=INT N)
             *   dist 作为普通标识符解析为 FUNCTION_CALL；distance 关键字解析为 MEASURE。 */
            if (expr->type == LV_AST_COMPARE && expr->data.compare.left &&
                (expr->data.compare.left->type == LV_AST_MEASURE ||
                 expr->data.compare.left->type == LV_AST_FUNCTION_CALL)) {
                const LvAstNode *call = expr->data.compare.left;
                const char *fn = call->data.call.func_name;
                if (fn && (lv_str_eq(fn, "dist") || lv_str_eq(fn, "distance"))) {
                    const LvAstNode *a0 = call->data.call.args;
                    const LvAstNode *a1 = a0 ? a0->next : NULL;
                    if (a0 && a1 && a0->type == LV_AST_IDENTIFIER_EXPR &&
                        a1->type == LV_AST_IDENTIFIER_EXPR) {
                        int id0 = loader_names_lookup(a0->data.ident.name);
                        int id1 = loader_names_lookup(a1->data.ident.name);
                        long long dist_val = 0;
                        loader_expr_int(expr->data.compare.right, &dist_val);
                        if (id0 >= 0 && id1 >= 0 && engine_get_main_graph(engine)) {
                            /* 用 INCIDENCE 承载距离约束对，numeric_value 存距离值 */
                            int participants[2] = {id0, id1};
                            int cid = engine_get_main_graph(engine)->next_constraint_id;
                            Constraint *c = graph_add_constraint_with_id(
                                engine_get_main_graph(engine), cid, INCIDENCE, participants, 2);
                            if (c) {
                                c->numeric_value = (double) dist_val;
                                c->satisfaction = 1.0;
                            }
                        }
                    }
                }
                continue;
            }

            /* ── 形态 2: 关系调用 collinear/perpendicular/parallel/... —— 关系约束
             *   AST: RELATION{func_name, args=[a,b,c,...]} */
            if (expr->type == LV_AST_RELATION) {
                const char *fname = expr->data.call.func_name;
                int arg_ids[16];
                int arg_count = 0;
                for (const LvAstNode *a = expr->data.call.args; a && arg_count < 16; a = a->next) {
                    if (a->type == LV_AST_IDENTIFIER_EXPR) {
                        int id = loader_names_lookup(a->data.ident.name);
                        if (id >= 0)
                            arg_ids[arg_count++] = id;
                    }
                }

                if (fname && engine_get_main_graph(engine)) {
                    if (lv_str_eq(fname, "collinear") && arg_count >= 3) {
                        /* 共线：依次用 betweenness 连接首点与其余各点 */
                        for (int i = 1; i + 1 < arg_count; i++) {
                            int p[3] = {arg_ids[0], arg_ids[i], arg_ids[i + 1]};
                            int cid = engine_get_main_graph(engine)->next_constraint_id;
                            graph_add_constraint_with_id(engine_get_main_graph(engine), cid,
                                                         BETWEENNESS, p, 3);
                        }
                    } else if (lv_str_eq(fname, "perpendicular") && arg_count >= 2) {
                        int p[2] = {arg_ids[0], arg_ids[1]};
                        int cid = engine_get_main_graph(engine)->next_constraint_id;
                        graph_add_constraint_with_id(engine_get_main_graph(engine), cid,
                                                     ANGLE, p, 2);
                    } else if (lv_str_eq(fname, "containment") && arg_count >= 2) {
                        int p[2] = {arg_ids[0], arg_ids[1]};
                        int cid = engine_get_main_graph(engine)->next_constraint_id;
                        graph_add_constraint_with_id(engine_get_main_graph(engine), cid,
                                                     CONTAINMENT, p, 2);
                    } else if (lv_str_eq(fname, "connection") && arg_count >= 2) {
                        int p[2] = {arg_ids[0], arg_ids[1]};
                        int cid = engine_get_main_graph(engine)->next_constraint_id;
                        graph_add_constraint_with_id(engine_get_main_graph(engine), cid,
                                                     CONNECTION, p, 2);
                    }
                }
                continue;
            }

            /* ── 形态 3: 其他已命名约束（保留原样，语义由引擎后续处理） */
        } else if (stmt->type == LV_AST_PROVE_STMT) {
            /* Prove 语句：加载阶段不改变引擎状态（证明目标不影响图结构）。
             * 验证语义由 lv_verify_proofs / lv_load_file_verified 完成
             * （见下方「微自举 B —— 证明验证器」）。 */
        }
    }

    return true;
}
