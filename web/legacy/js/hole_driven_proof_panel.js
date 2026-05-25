/**
 * @module web/js/hole_driven_proof_panel
 * @description Lv-00 Agda 风格 Hole-Driven 证明编辑面板
 *
 *              借鉴 Agda（wiki.portal.chalmers.se/agda）的 hole-driven
 *              交互式证明开发范式，为 Lv-00 的 Web 前端提供基于"洞"
 *              （typed hole）的证明构造编辑面板。
 *
 *  借鉴项目：  Agda (agda.readthedocs.io) — Emacs Mode / agda-mode
 *  核心借鉴点：  Typed-hole 编辑、FillSuggestion 菜单、实时类型检查、
 *              case-split 交互、逐步骤证明精化（proof refinement）
 *  分类：       P4 低优先级 / 前端证明编辑增强
 *  日期：       2026-05-24
 *
 *  设计目标：
 *    1. 用户在面板中声明几何命题（类型签名），系统插入 ?（hole）
 *    2. 点击 hole 弹出 FillSuggestion 菜单，提供构造/精化/引入/分情况等操作
 *    3. 每步填充后实时类型检查——绿色=已填充，黄色=待填充，红色=类型错误
 *    4. 与现有 web/js/proof_widgets_adapter.js 组件体系整合
 *    5. 纯 vanilla JS，零框架依赖，DOM API 直接操作
 *
 *  概念映射（Agda → Lv-00）：
 *    Agda typed hole    → Lv-00 GeometryHole（几何证明中的待填充位置）
 *    Agda case-split    → Lv-00 几何证明中的分情况证法（如锐角/直角/钝角）
 *    Agda auto          → Lv-00 proof_guided_fill() 自动填充建议
 *    Agda goal type     → Lv-00 几何命题签名（proposition signature）
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

'use strict';

/**
 * =========================================================================
 * 第一部分：常量与枚举定义
 * =========================================================================
 */

/**
 * 填充建议类型枚举 —— 对应 Agda-mode 的 Give/Refine/Auto/Case 操作
 * @enum {string}
 */
const FILL_SUGGESTION_TYPE = Object.freeze({
    /** 构造——创建一个几何实体（三角形、圆、线段等） */
    FILL_CONSTRUCTOR:  'FILL_CONSTRUCTOR',
    /** 精化——添加几何约束（垂直、平行、等长、共线等） */
    FILL_REFINE:       'FILL_REFINE',
    /** 引入——引入辅助参数或 lambda 表达式 */
    FILL_LAMBDA:       'FILL_LAMBDA',
    /** 分情况讨论——case split（锐角/直角/钝角；凸/非凸等） */
    FILL_CASE_SPLIT:   'FILL_CASE_SPLIT',
    /** 应用——应用已知定理或引理 */
    FILL_APPLY_THEOREM: 'FILL_APPLY_THEOREM',
    /** 展开——展开定义或公式 */
    FILL_UNFOLD:        'FILL_UNFOLD',
    /** 自动——调用 proof_guided_fill 自动填充 */
    FILL_AUTO:          'FILL_AUTO',
});

/**
 * Hole 状态枚举
 * @enum {string}
 */
const HOLE_STATUS = Object.freeze({
    /** 待填充 */
    PENDING:    'PENDING',
    /** 已填充 */
    FILLED:     'FILLED',
    /** 类型错误（填充内容类型不匹配） */
    TYPE_ERROR: 'TYPE_ERROR',
    /** 已证明（填充后通过完全验证） */
    PROVEN:     'PROVEN',
});

/**
 * 证明上下文中的命题状态
 * @enum {string}
 */
const PROP_STATUS = Object.freeze({
    PENDING:    'PENDING',     // 尚未开始证明
    IN_PROGRESS: 'IN_PROGRESS', // 正在构造中
    VERIFIED:   'VERIFIED',    // 已验证通过
    FAILED:     'FAILED',      // 验证失败
});

/**
 * 填充建议的视觉图标映射
 */
const FILL_ICONS = Object.freeze({
    FILL_CONSTRUCTOR:    '\u25B3',  // △ 构造
    FILL_REFINE:         '\u2699',  // ⚙ 约束
    FILL_LAMBDA:         '\u03BB',  // λ
    FILL_CASE_SPLIT:     '\u22C1',  // ⋁ 分情况
    FILL_APPLY_THEOREM:  '\u2605',  // ★ 应用定理
    FILL_UNFOLD:         '\u21C4',  // ⇄ 展开
    FILL_AUTO:           '\u2606',  // ☆ 自动
});

/**
 * 状态颜色映射
 */
const STATUS_COLORS = Object.freeze({
    FILLED:     '#40c057',  // 绿色 —— 已填充
    PENDING:    '#fab005',  // 黄色 —— 待填充
    TYPE_ERROR: '#f03e3e',  // 红色 —— 类型错误
    PROVEN:     '#51cf66',  // 亮绿 —— 已证明
});

/**
 * 状态 Emoji 指示器
 */
const STATUS_EMOJI = Object.freeze({
    FILLED:     '\u2705',  // 已填充
    PENDING:    '\uD83D\uDFE1',  // 待填充
    TYPE_ERROR: '\u274C',  // 类型错误
    PROVEN:     '\u2705',  // 已证明
});


/**
 * =========================================================================
 * 第二部分：数据结构定义
 * =========================================================================
 */

/**
 * @typedef {Object} GeometryProposition
 * @property {string}  id           - 命题唯一标识符
 * @property {string}  name         - 命题名称（如 "isosceles_triangle_area"）
 * @property {string}  signature    - 类型签名（如 "(a, b, c : Point) -> (area : Real)"）
 * @property {string}  statement    - 命题陈述（人类可读，如 "area = 0.5 * base * height"）
 * @property {string}  category     - 分类："triangle" | "circle" | "polygon" | ...
 * @property {Array<GeometryHole>} holes - 命题中的待填充洞列表
 * @property {Array<ProofStep>}     steps - 已完成的证明步骤
 * @property {string}  status       - 命题状态（对应 PROP_STATUS）
 */

/**
 * @typedef {Object} GeometryHole
 * @property {string}  id           - Hole 唯一标识符
 * @property {string}  holeLabel    - 用户可见标签（如 "?proof_body"）
 * @property {number}  position     - 在命题文本中的字节偏移
 * @property {string}  expectedType - 期望的填充类型（如 "Real", "Point", "Triangle"）
 * @property {string}  status       - 当前状态（对应 HOLE_STATUS）
 * @property {string}  filledContent- 已填充的内容（仅 FILLED/PROVEN 状态）
 * @property {string}  errorMessage - 类型检查错误信息（仅 TYPE_ERROR 状态）
 * @property {number}  depth        - 嵌套深度（顶层=0，每次 case-split +1）
 * @property {string}  parentHoleId - 父 Hole ID（用于嵌套 case-split 场景）
 */

/**
 * @typedef {Object} FillSuggestion
 * @property {string}        type        - 建议类型（对应 FILL_SUGGESTION_TYPE）
 * @property {string}        label       - 显示标签（中文 + 简短描述）
 * @property {string}        tooltip     - 详细提示文本
 * @property {string}        icon        - Unicode 图标
 * @property {string}        [snippet]   - 填充代码片段
 * @property {number}        [priority]  - 排序优先级（数值越小越靠前）
 * @property {Object}        [metadata]  - 额外元数据（如定理引用、参数列表）
 */


/**
 * =========================================================================
 * 第三部分：类型检查器（本地 JS 实现）
 * =========================================================================
 */

/**
 * @class GeometryTypeChecker
 * @description 本地几何类型检查器
 *
 *              在无法调用 lv00_js_backend.js 的 C/WASM 后端时，
 *              提供基本的命题类型一致性检查。
 *
 *              检查项：
 *              - 类型签名中的参数数量匹配
 *              - 子类型关系（Point <: GeometryEntity，Triangle <: Polygon）
 *              - 算术表达式返回值类型（Real）
 */
class GeometryTypeChecker {
    constructor() {
        /** @type {Map<string, string>} 已知实体到类型的映射 */
        this.entityTypeMap = new Map();

        /** @type {Map<string, Set<string>>} 子类型关系图 */
        this.subtypeGraph = new Map();
    }

    /**
     * 注册一个已知实体及其类型
     * @param {string} entityName - 实体名称
     * @param {string} typeName   - 类型名称
     */
    registerEntity(entityName, typeName) {
        this.entityTypeMap.set(entityName, typeName);
    }

    /**
     * 注册子类型关系
     * @param {string} subType   - 子类型
     * @param {string} superType - 父类型
     */
    registerSubtype(subType, superType) {
        if (!this.subtypeGraph.has(subType)) {
            this.subtypeGraph.set(subType, new Set());
        }
        this.subtypeGraph.get(subType).add(superType);
    }

    /**
     * 检查两个类型是否兼容（sub 是否可赋值给 super）
     * @param {string} subType   - 候选类型
     * @param {string} superType - 期望类型
     * @returns {boolean}
     */
    isCompatible(subType, superType) {
        if (subType === superType) return true;

        // 检查直接子类型关系
        const parents = this.subtypeGraph.get(subType);
        if (parents && parents.has(superType)) return true;

        // 可选的传递闭包检查（对小型类型层次）
        if (parents) {
            for (const parent of parents) {
                if (this.isCompatible(parent, superType)) return true;
            }
        }

        return false;
    }

    /**
     * 执行完整的类型检查
     *
     * @param {GeometryProposition} proposition - 几何命题
     * @returns {{ valid: boolean, errors: Array<{holeId: string, message: string}> }}
     */
    checkProposition(proposition) {
        const errors = [];

        if (!proposition || !proposition.holes) {
            return { valid: true, errors: [] };
        }

        for (const hole of proposition.holes) {
            if (hole.status === HOLE_STATUS.FILLED && hole.filledContent) {
                const result = this._checkHoleFill(hole);
                if (!result.valid) {
                    errors.push({
                        holeId: hole.id,
                        message: result.error,
                    });
                }
            }
        }

        return { valid: errors.length === 0, errors };
    }

    /**
     * 检查单个 Hole 的填充是否类型匹配
     * @param {GeometryHole} hole
     * @returns {{ valid: boolean, error: string }}
     * @private
     */
    _checkHoleFill(hole) {
        const content = hole.filledContent.trim();
        const expected = hole.expectedType;

        if (!content) {
            return { valid: false, error: 'Empty fill content / 填充内容为空' };
        }

        // 简单的启发式类型推断：
        // - 以 "triangle_" 开头 → Triangle 类型
        // - 以 "circle_" 开头 → Circle 类型
        // - 以 "point_" 开头 → Point 类型
        // - 以数字结尾 → Real 类型
        // - 包含 "assert_*" → Constraint 类型
        // - 包含 "lambda" → Function 类型

        let inferredType = null;

        if (content.startsWith('triangle_') || content.startsWith('isosceles_')) {
            inferredType = 'Triangle';
        } else if (content.startsWith('circle_') || content.startsWith('circumcircle_')) {
            inferredType = 'Circle';
        } else if (content.startsWith('point_') || content.startsWith('midpoint_')) {
            inferredType = 'Point';
        } else if (content.startsWith('assert_') || content.startsWith('constraint_')) {
            inferredType = 'Constraint';
        } else if (content.includes('lambda') || content.includes('=>') || content.includes('function')) {
            inferredType = 'Function';
        } else if (content.startsWith('line_') || content.startsWith('segment_')) {
            inferredType = 'Segment';
        } else if (content.startsWith('polygon_') || content.startsWith('quadrilateral_')) {
            inferredType = 'Polygon';
        } else if (/[\d.]+/.test(content) && !/[a-zA-Z_]/.test(content.replace(/[\d.+\-*/() ]/g, ''))) {
            inferredType = 'Real';
        } else {
            // 无法推断类型——保守地认为类型兼容
            inferredType = expected;
        }

        if (inferredType !== expected) {
            // 检查子类型关系
            if (!this.isCompatible(inferredType, expected)) {
                return {
                    valid: false,
                    error: `Type mismatch / 类型不匹配: expected ${expected}, got ${inferredType}`,
                };
            }
        }

        return { valid: true, error: '' };
    }
}


/**
 * =========================================================================
 * 第四部分：填充建议生成器
 * =========================================================================
 */

/**
 * @class FillSuggestionEngine
 * @description 填充建议生成引擎
 *
 *              基于当前 Hole 的期望类型和证明上下文，
 *              生成一组合理的 FillSuggestion 候选列表。
 *
 *              借鉴 Agda-mode 的 "Give" 和 "Refine" 操作策略：
 *              - 对几何命题优先推荐构造器（三角形构建/圆构建）
 *              - 对约束类型优先推荐 assert_* 精化操作
 *              - 对复杂命题提供 case-split 分情况讨论
 *              - 底部提供 "Auto" 自动填充（由 proof_guided_fill 驱动）
 */
class FillSuggestionEngine {
    constructor(typeChecker) {
        /** @type {GeometryTypeChecker} */
        this.typeChecker = typeChecker || new GeometryTypeChecker();

        /** @type {Array<Object>} 系统注册的已知定理 */
        this.knownTheorems = [];

        /** @type {Array<Object>} 系统注册的已知构造器 */
        this.knownConstructors = [];

        this._initDefaultKnowledge();
    }

    /**
     * 初始化默认的几何知识和定理库
     * @private
     */
    _initDefaultKnowledge() {
        // 已知几何构造器
        this.knownConstructors = [
            { name: 'triangle_create',       returns: 'Triangle', args: ['Point', 'Point', 'Point'], desc: 'Create a triangle from 3 points / 由三点创建三角形' },
            { name: 'right_triangle_create', returns: 'Triangle', args: ['Point', 'Point', 'Point'], desc: 'Create a right triangle / 创建直角三角形' },
            { name: 'isosceles_triangle_create', returns: 'Triangle', args: ['Point', 'Point', 'Point'], desc: 'Create an isosceles triangle / 创建等腰三角形' },
            { name: 'equilateral_triangle_create', returns: 'Triangle', args: ['Point', 'Point'], desc: 'Create an equilateral triangle / 创建等边三角形' },
            { name: 'circle_create',         returns: 'Circle',   args: ['Point', 'Real'],    desc: 'Create a circle / 创建圆' },
            { name: 'segment_create',        returns: 'Segment',  args: ['Point', 'Point'],    desc: 'Create a line segment / 创建线段' },
            { name: 'polygon_create',        returns: 'Polygon',  args: ['List<Point>'],       desc: 'Create a polygon / 创建多边形' },
            { name: 'midpoint',              returns: 'Point',    args: ['Point', 'Point'],    desc: 'Compute midpoint / 计算中点' },
            { name: 'perpendicular_line',    returns: 'Line',     args: ['Point', 'Line'],     desc: 'Create perpendicular line / 创建垂线' },
            { name: 'parallel_line',         returns: 'Line',     args: ['Point', 'Line'],     desc: 'Create parallel line / 创建平行线' },
        ];

        // 已知约束精化
        this.knownTheorems = [
            { name: 'assert_right_angle',    type: 'Constraint', desc: 'Assert right angle / 断言直角' },
            { name: 'assert_parallel',       type: 'Constraint', desc: 'Assert parallel lines / 断言平行' },
            { name: 'assert_perpendicular',  type: 'Constraint', desc: 'Assert perpendicular / 断言垂直' },
            { name: 'assert_equal_length',   type: 'Constraint', desc: 'Assert equal length / 断言等长' },
            { name: 'assert_collinear',      type: 'Constraint', desc: 'Assert collinearity / 断言共线' },
            { name: 'assert_concyclic',      type: 'Constraint', desc: 'Assert concyclic / 断言共圆' },
            { name: 'pythagorean_theorem',   type: 'Theorem',    desc: 'Pythagorean theorem / 勾股定理' },
        ];
    }

    /**
     * 为指定的 Hole 生成填充建议列表
     *
     * @param {GeometryHole}        hole          - 当前洞
     * @param {GeometryProposition} [proposition] - 所属命题（用于上下文感知）
     * @returns {Array<FillSuggestion>} 建议列表（按 priority 排序）
     */
    generate(hole, proposition) {
        const suggestions = [];
        const expectedType = hole.expectedType || '?';

        // 1. 构造器建议 —— 对实体类型（Triangle, Circle, Point 等）推荐构造器
        for (const ctor of this.knownConstructors) {
            if (ctor.returns === expectedType ||
                this.typeChecker.isCompatible(ctor.returns, expectedType)) {
                const args = ctor.args.map((a, i) => `arg${i + 1}`).join(', ');
                suggestions.push({
                    type:       FILL_SUGGESTION_TYPE.FILL_CONSTRUCTOR,
                    label:      ctor.name,
                    tooltip:    ctor.desc,
                    icon:       FILL_ICONS.FILL_CONSTRUCTOR,
                    snippet:    `${ctor.name}(${args})`,
                    priority:   10,
                    metadata:   { constructor: ctor, args: ctor.args },
                });
            }
        }

        // 2. 精化建议 —— 对约束类型推荐约束断言
        for (const theorem of this.knownTheorems) {
            if (theorem.type === 'Constraint' ||
                (expectedType === 'Constraint' || expectedType === 'Theorem')) {
                suggestions.push({
                    type:       FILL_SUGGESTION_TYPE.FILL_REFINE,
                    label:      theorem.name,
                    tooltip:    theorem.desc,
                    icon:       FILL_ICONS.FILL_REFINE,
                    snippet:    `${theorem.name}(...)`,
                    priority:   20,
                    metadata:   { theorem },
                });
            }
        }

        // 3. Lambda 引入建议 —— 对函数类型推荐
        if (expectedType === 'Function' || expectedType.includes('->')) {
            suggestions.push({
                type:       FILL_SUGGESTION_TYPE.FILL_LAMBDA,
                label:      'Introduce parameter / 引入参数',
                tooltip:    'Wrap current expression in a lambda / 将当前表达式包装为 lambda',
                icon:       FILL_ICONS.FILL_LAMBDA,
                snippet:    '(x) => ...',
                priority:   15,
                metadata:   {},
            });
        }

        // 4. Case-split 建议 —— 对几何命题提供分情况讨论
        if (expectedType === 'Triangle' || expectedType === 'Polygon' ||
            expectedType === 'Proof'   || hole.depth < 3) {
            const caseTypes = this._inferCaseSplits(expectedType, proposition);
            for (const cs of caseTypes) {
                suggestions.push({
                    type:       FILL_SUGGESTION_TYPE.FILL_CASE_SPLIT,
                    label:      cs.label,
                    tooltip:    cs.tooltip,
                    icon:       FILL_ICONS.FILL_CASE_SPLIT,
                    snippet:    cs.snippet,
                    priority:   25,
                    metadata:   { cases: cs.cases },
                });
            }
        }

        // 5. 应用定理建议
        for (const theorem of this.knownTheorems) {
            if (theorem.type === 'Theorem') {
                suggestions.push({
                    type:       FILL_SUGGESTION_TYPE.FILL_APPLY_THEOREM,
                    label:      theorem.name,
                    tooltip:    theorem.desc,
                    icon:       FILL_ICONS.FILL_APPLY_THEOREM,
                    snippet:    `apply ${theorem.name}`,
                    priority:   30,
                    metadata:   { theorem },
                });
            }
        }

        // 6. 自动填充建议 —— 总是出现在列表底部
        suggestions.push({
            type:       FILL_SUGGESTION_TYPE.FILL_AUTO,
            label:      'Auto Fill (guided) / 自动填充',
            tooltip:    'Invoke proof_guided_fill to automatically fill this hole / 调用 proof_guided_fill 自动填充',
            icon:       FILL_ICONS.FILL_AUTO,
            snippet:    '?{auto}',
            priority:   99,
            metadata:   {},
        });

        // 按优先级排序
        suggestions.sort((a, b) => (a.priority || 50) - (b.priority || 50));

        return suggestions;
    }

    /**
     * 根据命题上下文推断可能的 case-split 策略
     *
     * @param {string} expectedType
     * @param {GeometryProposition} [proposition]
     * @returns {Array<{label: string, tooltip: string, snippet: string, cases: Array<string>}>}
     * @private
     */
    _inferCaseSplits(expectedType, proposition) {
        const category = proposition ? proposition.category : '';

        if (category === 'triangle' || expectedType === 'Triangle') {
            return [
                {
                    label:   'Case by angle / 按角类型分情况',
                    tooltip: 'Split into acute, right, obtuse triangle / 分为锐角、直角、钝角',
                    snippet: 'case {\n  acute => ...\n  right => ...\n  obtuse => ...\n}',
                    cases:   ['acute', 'right', 'obtuse'],
                },
                {
                    label:   'Case by side / 按边类型分情况',
                    tooltip: 'Split into equilateral, isosceles, scalene / 分为等边、等腰、不等边',
                    snippet: 'case {\n  equilateral => ...\n  isosceles => ...\n  scalene => ...\n}',
                    cases:   ['equilateral', 'isosceles', 'scalene'],
                },
            ];
        }

        if (category === 'polygon' || expectedType === 'Polygon') {
            return [
                {
                    label:   'Case by convexity / 按凸性分情况',
                    tooltip: 'Split into convex and concave polygon / 分为凸多边形和凹多边形',
                    snippet: 'case {\n  convex => ...\n  concave => ...\n}',
                    cases:   ['convex', 'concave'],
                },
            ];
        }

        return [
            {
                label:   'General case split / 通用分情况',
                tooltip: 'Split the proof into multiple cases / 将证明分为多种情况',
                snippet: 'case {\n  case1 => ...\n  case2 => ...\n}',
                cases:   ['case1', 'case2'],
            },
        ];
    }
}


/**
 * =========================================================================
 * 第五部分：Hole-Driven 证明面板主类
 * =========================================================================
 */

/**
 * @class HoleDrivenProofPanel
 * @description Agda 风格的 Hole-Driven 证明编辑面板
 *
 *              使用方式：
 *              ```javascript
 *              const panel = new HoleDrivenProofPanel({
 *                  container: document.getElementById('hole-driven-proof-panel'),
 *                  enableTypeCheck: true,
 *                  enableAutoSuggest: true,
 *              });
 *
 *              panel.loadProposition({
 *                  name: 'isosceles_triangle_area',
 *                  signature: '(a, b, c : Point) -> Real',
 *                  statement: 'area = 0.5 * base * height',
 *                  category: 'triangle',
 *              });
 *              ```
 */
class HoleDrivenProofPanel {
    /**
     * @param {Object} options
     * @param {HTMLElement} [options.container]            - DOM 容器
     * @param {boolean}     [options.enableTypeCheck=true]  - 启用实时类型检查
     * @param {boolean}     [options.enableAutoSuggest=true]- 启用自动建议
     * @param {boolean}     [options.showLineNumbers=true]  - 显示行号
     * @param {Function}    [options.onHoleFill]            - Hole 填充回调 (holeId, content) => void
     * @param {Function}    [options.onPropositionChange]   - 命题变更回调 (proposition) => void
     * @param {Object}      [options.externalBackend]       - 外部后端接口（lv00_js_backend 桥接）
     */
    constructor(options = {}) {
        /** @type {HTMLElement|null} */
        this.container = options.container || null;

        /** @type {boolean} */
        this.enableTypeCheck = options.enableTypeCheck !== false;

        /** @type {boolean} */
        this.enableAutoSuggest = options.enableAutoSuggest !== false;

        /** @type {boolean} */
        this.showLineNumbers = options.showLineNumbers !== false;

        /** @type {Function|null} */
        this.onHoleFill = options.onHoleFill || null;

        /** @type {Function|null} */
        this.onPropositionChange = options.onPropositionChange || null;

        /** @type {Object|null} */
        this.externalBackend = options.externalBackend || null;

        /** @type {GeometryTypeChecker} */
        this.typeChecker = new GeometryTypeChecker();
        this._initTypeSystem();

        /** @type {FillSuggestionEngine} */
        this.suggestionEngine = new FillSuggestionEngine(this.typeChecker);

        /** @type {GeometryProposition|null} 当前加载的命题 */
        this.proposition = null;

        /** @type {number} 内部计数器（用于生成唯一 ID） */
        this._idCounter = 0;

        /** @type {Map<string, HTMLElement>} Hole ID → 对应 DOM 元素 */
        this._holeElementMap = new Map();

        /** @type {HTMLElement|null} 当前打开的浮层菜单 */
        this._activePopover = null;

        /** @type {boolean} */
        this._rendered = false;

        // 如果提供了容器，立即构建布局
        if (this.container) {
            this._buildLayout();
        }
    }

    /**
     * 初始化几何类型系统
     * @private
     */
    _initTypeSystem() {
        // 注册子类型关系
        this.typeChecker.registerSubtype('Triangle', 'Polygon');
        this.typeChecker.registerSubtype('Quadrilateral', 'Polygon');
        this.typeChecker.registerSubtype('RightTriangle', 'Triangle');
        this.typeChecker.registerSubtype('IsoscelesTriangle', 'Triangle');
        this.typeChecker.registerSubtype('EquilateralTriangle', 'Triangle');
        this.typeChecker.registerSubtype('Circle', 'GeometryEntity');
        this.typeChecker.registerSubtype('Line', 'GeometryEntity');
        this.typeChecker.registerSubtype('Segment', 'GeometryEntity');
        this.typeChecker.registerSubtype('Point', 'GeometryEntity');
        this.typeChecker.registerSubtype('Polygon', 'GeometryEntity');
    }

    /**
     * 生成唯一 ID
     * @returns {string}
     * @private
     */
    _makeId() {
        return 'hole_' + (++this._idCounter);
    }

    /**
     * 构建面板 DOM 布局
     * @private
     */
    _buildLayout() {
        if (!this.container || this._rendered) return;

        this.container.innerHTML = '';

        // ── 标题栏 ──
        const titleBar = this._el('div', 'hdp-title-bar', {
            padding: '8px 12px',
            background: 'var(--panel-header, #2d2d2d)',
            borderBottom: '1px solid var(--segment, #555)',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
        });
        const title = this._el('span', 'hdp-title', {
            fontWeight: 'bold',
            fontSize: '13px',
            color: 'var(--text, #e0e0e0)',
        }, 'HOLE-DRIVEN PROOF / 洞驱动证明');
        titleBar.appendChild(title);

        // 状态指示灯
        this._statusLed = this._el('span', 'hdp-status-led', {
            width: '10px', height: '10px', borderRadius: '50%',
            display: 'inline-block', marginLeft: '8px',
            background: STATUS_COLORS.PENDING,
        });
        title.appendChild(this._statusLed);

        // 工具栏按钮
        const toolbar = this._el('div', 'hdp-toolbar', { display: 'flex', gap: '4px' });
        const checkBtn = this._el('button', 'hdp-btn-check', {
            padding: '4px 8px', fontSize: '10px', cursor: 'pointer',
            background: 'var(--canvas-bg, #1a1a2e)', color: 'var(--text, #e0e0e0)',
            border: '1px solid var(--segment, #555)', borderRadius: '3px',
        }, 'Type Check / 类型检查');
        checkBtn.addEventListener('click', () => this._runTypeCheck());
        toolbar.appendChild(checkBtn);

        const resetBtn = this._el('button', 'hdp-btn-reset', {
            padding: '4px 8px', fontSize: '10px', cursor: 'pointer',
            background: 'var(--canvas-bg, #1a1a2e)', color: 'var(--text, #e0e0e0)',
            border: '1px solid var(--segment, #555)', borderRadius: '3px',
        }, 'Reset / 重置');
        resetBtn.addEventListener('click', () => this._resetProposition());
        toolbar.appendChild(resetBtn);

        titleBar.appendChild(toolbar);
        this.container.appendChild(titleBar);

        // ── 命题声明区 ──
        this._sigSection = this._el('div', 'hdp-signature-section', {
            padding: '10px 12px',
            background: 'var(--canvas-bg, #1a1a2e)',
            borderBottom: '1px solid var(--segment, #555)',
        });
        this._sigDisplay = this._el('div', 'hdp-signature-display', {
            fontFamily: 'monospace',
            fontSize: '12px',
            color: 'var(--text, #e0e0e0)',
            whiteSpace: 'pre-wrap',
            wordBreak: 'break-word',
        });
        this._sigSection.appendChild(this._sigDisplay);
        this.container.appendChild(this._sigSection);

        // ── 证明主体（可编辑的行区域）──
        this._proofBody = this._el('div', 'hdp-proof-body', {
            padding: '8px 4px 8px 12px',
            background: '#121220',
            minHeight: '200px',
            maxHeight: '500px',
            overflowY: 'auto',
            fontFamily: 'monospace',
            fontSize: '12px',
            lineHeight: '1.7',
        });
        this.container.appendChild(this._proofBody);

        // ── 底部状态栏 ──
        this._statusBar = this._el('div', 'hdp-status-bar', {
            padding: '4px 12px',
            fontSize: '10px',
            color: '#868e96',
            borderTop: '1px solid var(--segment, #555)',
            background: 'var(--canvas-bg, #1a1a2e)',
            display: 'flex',
            gap: '16px',
        });
        this._statusMessage = this._el('span', 'hdp-status-message', {},
            'No proposition loaded / 未加载命题');
        this._statusBar.appendChild(this._statusMessage);
        this.container.appendChild(this._statusBar);

        // ── 浮层菜单容器（绝对定位，全局）──
        if (!document.getElementById('hdp-popover-layer')) {
            const layer = this._el('div', 'hdp-popover-layer', {
                position: 'fixed',
                top: '0', left: '0',
                width: '100vw', height: '100vh',
                pointerEvents: 'none',
                zIndex: '10000',
            });
            this._popoverLayer = layer;
            document.body.appendChild(layer);
        } else {
            this._popoverLayer = document.getElementById('hdp-popover-layer');
        }

        this._rendered = true;
    }

    /**
     * 加载一个新的几何命题
     *
     * @param {Object} propData
     * @param {string} propData.name        - 命题名称
     * @param {string} propData.signature   - 类型签名
     * @param {string} propData.statement   - 命题陈述
     * @param {string} [propData.category]  - 分类
     */
    loadProposition(propData) {
        if (!this._rendered && this.container) {
            this._buildLayout();
        }
        if (!this._rendered) return;

        // 解析命题，识别 hole 位置
        const holes = this._parseHoles(propData);

        /** @type {GeometryProposition} */
        this.proposition = {
            id:        'prop_' + (++this._idCounter),
            name:      propData.name || 'unnamed',
            signature: propData.signature || '',
            statement: propData.statement || '',
            category:  propData.category || 'geometry',
            holes:     holes,
            steps:     [],
            status:    PROP_STATUS.IN_PROGRESS,
        };

        this._renderProposition();
        this._updateStatusLed();
        this._setStatus('Proposition loaded: ' + propData.name + ' / 命题已加载');
    }

    /**
     * 解析命题文本，自动识别 hole 位置并创建 GeometryHole 对象
     *
     * 解析规则：
     *   - '?' 标记为待填充 hole
     *   - '?{label}' 为带标签的 hole
     *   - 每个 hole 关联到其所在的语句上下文
     *
     * @param {Object} propData
     * @returns {Array<GeometryHole>}
     * @private
     */
    _parseHoles(propData) {
        const holes = [];
        const statement = propData.statement || '';
        const signature = propData.signature || '';

        // 在 statement 中搜索 '?' 标记
        let pos = 0;
        while (pos < statement.length) {
            const qIdx = statement.indexOf('?', pos);
            if (qIdx === -1) break;

            // 提取 hole 标签
            let labelEnd = qIdx + 1;
            let label = '?';
            if (qIdx + 1 < statement.length && statement[qIdx + 1] === '{') {
                const closeBrace = statement.indexOf('}', qIdx + 2);
                if (closeBrace !== -1) {
                    label = '?' + statement.substring(qIdx + 1, closeBrace + 1);
                    labelEnd = closeBrace + 1;
                }
            }

            holes.push({
                id:            this._makeId(),
                holeLabel:     label,
                position:      qIdx,
                expectedType:  this._inferExpectedType(signature, statement, qIdx),
                status:        HOLE_STATUS.PENDING,
                filledContent: null,
                errorMessage:  null,
                depth:         0,
                parentHoleId:  null,
            });

            pos = labelEnd;
        }

        // 如果没有显式的 hole，根据签名添加至少一个 hole
        if (holes.length === 0) {
            const returnType = this._extractReturnType(signature);
            holes.push({
                id:            this._makeId(),
                holeLabel:     '?proof_body',
                position:      statement.length,
                expectedType:  returnType || '?',
                status:        HOLE_STATUS.PENDING,
                filledContent: null,
                errorMessage:  null,
                depth:         0,
                parentHoleId:  null,
            });
        }

        return holes;
    }

    /**
     * 从类型签名中提取返回类型
     * @param {string} signature
     * @returns {string}
     * @private
     */
    _extractReturnType(signature) {
        const arrowIdx = signature.lastIndexOf('->');
        if (arrowIdx === -1) return signature.trim();
        return signature.substring(arrowIdx + 2).trim();
    }

    /**
     * 根据上下文推断 hole 期望的类型
     * @param {string} signature
     * @param {string} statement
     * @param {number} position
     * @returns {string}
     * @private
     */
    _inferExpectedType(signature, statement, position) {
        // 简单启发式：如果在 'area'/'length'/'distance' 附近 → Real
        const before = statement.substring(Math.max(0, position - 30), position).toLowerCase();
        if (before.includes('area') || before.includes('length') ||
            before.includes('distance') || before.includes('=')) {
            return 'Real';
        }
        // 如果在 'triangle' 附近 → Triangle
        if (before.includes('triangle') || before.includes('tri_')) {
            return 'Triangle';
        }
        // 如果在 'circle' 附近 → Circle
        if (before.includes('circle')) {
            return 'Circle';
        }
        // 默认返回签名的返回类型
        return this._extractReturnType(signature);
    }

    /**
     * 渲染当前命题的完整视图
     * @private
     */
    _renderProposition() {
        if (!this.proposition) return;

        // 渲染命题签名
        this._sigDisplay.innerHTML = '';
        const sigLabel = this._el('span', 'hdp-sig-label', {
            color: '#4dabf7', fontWeight: 'bold',
        }, 'prove ');
        this._sigDisplay.appendChild(sigLabel);

        const sigName = this._el('span', 'hdp-sig-name', {
            color: '#e0e0e0', fontWeight: 'bold',
        }, this.proposition.name);
        this._sigDisplay.appendChild(sigName);

        const sigType = this._el('span', 'hdp-sig-type', {
            color: '#868e96',
        }, ' : ' + this.proposition.signature);
        this._sigDisplay.appendChild(sigType);

        // 渲染证明主体（带有可点击的 hole）
        this._renderProofBody();
    }

    /**
     * 渲染证明主体内容，将 statement 文本中的 ? 替换为可点击的 Hole 元素
     * @private
     */
    _renderProofBody() {
        if (!this.proposition || !this._proofBody) return;

        this._proofBody.innerHTML = '';
        this._holeElementMap.clear();

        const statement = this.proposition.statement;

        // 如果有显式 hole，按位置插值渲染
        if (this.proposition.holes.length > 0) {
            let lastPos = 0;
            // 按位置排序
            const sortedHoles = [...this.proposition.holes].sort(
                (a, b) => a.position - b.position
            );

            for (const hole of sortedHoles) {
                // 渲染 hole 前面的文本
                if (hole.position > lastPos) {
                    const textNode = document.createTextNode(
                        statement.substring(lastPos, hole.position)
                    );
                    this._proofBody.appendChild(textNode);
                }
                // 渲染 hole 元素
                const holeEl = this._createHoleElement(hole);
                this._proofBody.appendChild(holeEl);
                this._holeElementMap.set(hole.id, holeEl);

                // 更新 lastPos：跳过 hole 标记的字符数
                const labelLen = hole.holeLabel.length;
                lastPos = hole.position + labelLen;
            }

            // 渲染剩余文本
            if (lastPos < statement.length) {
                const textNode = document.createTextNode(
                    statement.substring(lastPos)
                );
                this._proofBody.appendChild(textNode);
            }
        } else {
            // 无显式 hole 时，创建默认 hole
            const textNode = document.createTextNode(statement + ' ');
            this._proofBody.appendChild(textNode);

            const hole = this.proposition.holes[0];
            if (hole) {
                const holeEl = this._createHoleElement(hole);
                this._proofBody.appendChild(holeEl);
                this._holeElementMap.set(hole.id, holeEl);
            }
        }
    }

    /**
     * 创建单个 Hole 的 DOM 元素
     *
     * @param {GeometryHole} hole
     * @returns {HTMLElement}
     * @private
     */
    _createHoleElement(hole) {
        const el = this._el('span', 'hdp-hole', {
            display: 'inline-block',
            padding: '1px 6px',
            borderRadius: '3px',
            cursor: 'pointer',
            fontFamily: 'monospace',
            fontSize: '11px',
            fontWeight: 'bold',
            transition: 'all 0.2s ease',
            position: 'relative',
            userSelect: 'none',
        });

        // 根据状态设置样式
        this._applyHoleStyle(el, hole);

        // 内容
        if (hole.status === HOLE_STATUS.FILLED || hole.status === HOLE_STATUS.PROVEN) {
            el.textContent = hole.filledContent || hole.holeLabel;
        } else if (hole.status === HOLE_STATUS.TYPE_ERROR) {
            el.textContent = hole.holeLabel + ' [ERROR]';
        } else {
            el.textContent = hole.holeLabel;
        }

        el.dataset.holeId = hole.id;

        // 点击事件 —— 弹出 FillSuggestion 菜单
        el.addEventListener('click', (e) => {
            e.stopPropagation();
            this._showFillSuggestionMenu(hole, el);
        });

        // 悬停效果
        el.addEventListener('mouseenter', () => {
            el.style.boxShadow = '0 0 6px ' + STATUS_COLORS[hole.status] + '88';
            el.style.transform = 'scale(1.05)';
        });
        el.addEventListener('mouseleave', () => {
            el.style.boxShadow = 'none';
            el.style.transform = 'scale(1)';
        });

        // 错误提示 tooltip
        if (hole.status === HOLE_STATUS.TYPE_ERROR && hole.errorMessage) {
            el.title = hole.errorMessage;
        }

        return el;
    }

    /**
     * 根据 Hole 状态应用样式
     * @param {HTMLElement} el
     * @param {GeometryHole} hole
     * @private
     */
    _applyHoleStyle(el, hole) {
        const color = STATUS_COLORS[hole.status] || STATUS_COLORS.PENDING;

        switch (hole.status) {
        case HOLE_STATUS.PENDING:
            el.style.background = 'rgba(250, 176, 5, 0.15)';
            el.style.border = '1px dashed #fab005';
            el.style.color = '#fab005';
            break;
        case HOLE_STATUS.FILLED:
        case HOLE_STATUS.PROVEN:
            el.style.background = 'rgba(64, 192, 87, 0.12)';
            el.style.border = '1px solid #40c057';
            el.style.color = '#40c057';
            break;
        case HOLE_STATUS.TYPE_ERROR:
            el.style.background = 'rgba(240, 62, 62, 0.15)';
            el.style.border = '1px dashed #f03e3e';
            el.style.color = '#ff6b6b';
            break;
        }
    }

    /**
     * 显示填充建议浮层菜单
     *
     * @param {GeometryHole} hole
     * @param {HTMLElement}  anchorEl - 锚点 DOM 元素
     * @private
     */
    _showFillSuggestionMenu(hole, anchorEl) {
        // 关闭之前的浮层
        this._closePopover();

        // 生成建议
        const suggestions = this.suggestionEngine.generate(hole, this.proposition);

        // 创建浮层
        const popover = this._el('div', 'hdp-popover', {
            position: 'absolute',
            background: '#2d2d2d',
            border: '1px solid #4dabf7',
            borderRadius: '6px',
            boxShadow: '0 4px 20px rgba(0,0,0,0.5)',
            minWidth: '280px',
            maxWidth: '400px',
            maxHeight: '400px',
            overflowY: 'auto',
            zIndex: '10001',
            pointerEvents: 'auto',
            fontSize: '11px',
        });

        // 标题
        const header = this._el('div', 'hdp-popover-header', {
            padding: '8px 12px',
            borderBottom: '1px solid #4dabf7',
            color: '#4dabf7',
            fontWeight: 'bold',
            fontSize: '11px',
        }, 'Fill Hole: ' + hole.holeLabel + ' / 填充洞');

        // 期望类型提示
        const typeHint = this._el('div', 'hdp-popover-expected-type', {
            padding: '6px 12px',
            color: '#868e96',
            fontSize: '10px',
            fontStyle: 'italic',
            borderBottom: '1px solid #444',
        }, 'Expected type / 期望类型: ' + (hole.expectedType || '?'));
        header.appendChild(typeHint);

        popover.appendChild(header);

        // 建议列表
        for (const suggestion of suggestions) {
            const item = this._createSuggestionItem(suggestion, hole, popover);
            popover.appendChild(item);
        }

        // 手动输入区域
        const manualDiv = this._el('div', 'hdp-popover-manual', {
            padding: '8px',
            borderTop: '1px solid #444',
            display: 'flex',
            gap: '4px',
        });
        const manualInput = this._el('input', 'hdp-popover-input', {
            flex: '1',
            padding: '4px 6px',
            fontSize: '11px',
            fontFamily: 'monospace',
            background: '#1a1a2e',
            color: '#e0e0e0',
            border: '1px solid #555',
            borderRadius: '3px',
        });
        manualInput.placeholder = 'Manual entry / 手动输入...';
        const manualBtn = this._el('button', 'hdp-popover-manual-btn', {
            padding: '4px 8px',
            fontSize: '10px',
            cursor: 'pointer',
            background: '#40c057',
            color: '#fff',
            border: 'none',
            borderRadius: '3px',
            fontWeight: 'bold',
        }, 'Fill / 填充');
        manualBtn.addEventListener('click', (e) => {
            e.stopPropagation();
            const content = manualInput.value.trim();
            if (content) {
                this._fillHole(hole.id, content);
                this._closePopover();
            }
        });
        // Enter 键提交
        manualInput.addEventListener('keydown', (e) => {
            if (e.key === 'Enter') {
                e.stopPropagation();
                const content = manualInput.value.trim();
                if (content) {
                    this._fillHole(hole.id, content);
                    this._closePopover();
                }
            }
        });
        manualDiv.appendChild(manualInput);
        manualDiv.appendChild(manualBtn);
        popover.appendChild(manualDiv);

        // 定位浮层
        this._positionPopover(popover, anchorEl);

        // 添加到浮层容器
        this._popoverLayer.appendChild(popover);
        this._activePopover = popover;

        // 点击外部关闭
        setTimeout(() => {
            document.addEventListener('click', this._externalClickHandler = (evt) => {
                if (!popover.contains(evt.target) && evt.target !== anchorEl) {
                    this._closePopover();
                }
            });
        }, 0);
    }

    /**
     * 创建单个填充建议条目的 DOM 元素
     *
     * @param {FillSuggestion} suggestion
     * @param {GeometryHole}   hole
     * @param {HTMLElement}    popover
     * @returns {HTMLElement}
     * @private
     */
    _createSuggestionItem(suggestion, hole, popover) {
        const item = this._el('div', 'hdp-suggestion-item', {
            padding: '8px 12px',
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'flex-start',
            gap: '8px',
            borderBottom: '1px solid #333',
            transition: 'background 0.15s ease',
        });

        // 图标
        const iconEl = this._el('span', 'hdp-suggestion-icon', {
            fontSize: '14px',
            flexShrink: '0',
            width: '20px',
            textAlign: 'center',
        }, suggestion.icon || '\u25CF');
        item.appendChild(iconEl);

        // 文本
        const textDiv = this._el('div', 'hdp-suggestion-text', { flex: '1' });
        const labelEl = this._el('div', 'hdp-suggestion-label', {
            fontWeight: 'bold',
            color: '#e0e0e0',
            fontSize: '11px',
        }, suggestion.label);
        textDiv.appendChild(labelEl);

        const typeLabel = this._el('span', 'hdp-suggestion-type', {
            fontSize: '9px',
            color: '#4dabf7',
            marginLeft: '6px',
            background: 'rgba(77, 171, 247, 0.15)',
            padding: '1px 4px',
            borderRadius: '2px',
        }, suggestion.type.replace('FILL_', ''));
        labelEl.appendChild(typeLabel);

        if (suggestion.tooltip) {
            const tooltipEl = this._el('div', 'hdp-suggestion-tooltip', {
                fontSize: '9px',
                color: '#868e96',
                marginTop: '2px',
            }, suggestion.tooltip);
            textDiv.appendChild(tooltipEl);
        }

        item.appendChild(textDiv);

        // 悬停效果
        item.addEventListener('mouseenter', () => {
            item.style.background = 'rgba(77, 171, 247, 0.1)';
        });
        item.addEventListener('mouseleave', () => {
            item.style.background = 'transparent';
        });

        // 点击选择
        item.addEventListener('click', (e) => {
            e.stopPropagation();
            const content = suggestion.snippet || suggestion.label;
            this._fillHole(hole.id, content);
            this._closePopover();
        });

        return item;
    }

    /**
     * 填充指定 Hole
     *
     * @param {string} holeId  - Hole 标识符
     * @param {string} content - 填充内容
     * @private
     */
    _fillHole(holeId, content) {
        if (!this.proposition) return;

        const hole = this.proposition.holes.find(h => h.id === holeId);
        if (!hole) return;

        hole.filledContent = content;
        hole.status = HOLE_STATUS.FILLED;

        // 如果启用了类型检查，立即检查
        if (this.enableTypeCheck) {
            this._checkSingleHole(hole);
        }

        // 重新渲染
        this._renderProofBody();
        this._updateStatusLed();

        // 触发回调
        if (this.onHoleFill) {
            this.onHoleFill(holeId, content);
        }
        if (this.onPropositionChange) {
            this.onPropositionChange(this.proposition);
        }

        const statusIcon = hole.status === HOLE_STATUS.TYPE_ERROR
            ? STATUS_EMOJI.TYPE_ERROR : STATUS_EMOJI.FILLED;
        this._setStatus(`Hole ${hole.holeLabel} filled / 已填充: ${content} ${statusIcon}`);
    }

    /**
     * 检查单个 Hole 的类型
     * @param {GeometryHole} hole
     * @private
     */
    _checkSingleHole(hole) {
        const result = this.typeChecker._checkHoleFill(hole);
        if (!result.valid) {
            hole.status = HOLE_STATUS.TYPE_ERROR;
            hole.errorMessage = result.error;
        } else {
            hole.status = HOLE_STATUS.FILLED;
            hole.errorMessage = null;
        }
    }

    /**
     * 运行完整的类型检查
     * @private
     */
    _runTypeCheck() {
        if (!this.proposition) return;

        const result = this.typeChecker.checkProposition(this.proposition);

        // 更新每个 hole 的状态
        for (const hole of this.proposition.holes) {
            if (hole.status === HOLE_STATUS.FILLED) {
                const holeErr = result.errors.find(e => e.holeId === hole.id);
                if (holeErr) {
                    hole.status = HOLE_STATUS.TYPE_ERROR;
                    hole.errorMessage = holeErr.message;
                }
            }
        }

        this._renderProofBody();
        this._updateStatusLed();

        if (result.valid) {
            this._setStatus('Type check passed / 类型检查通过');
            this.proposition.status = PROP_STATUS.VERIFIED;
        } else {
            this._setStatus('Type check failed: ' + result.errors.length + ' error(s) / 类型检查失败');
        }

        if (this.onPropositionChange) {
            this.onPropositionChange(this.proposition);
        }
    }

    /**
     * 重置当前命题
     * @private
     */
    _resetProposition() {
        if (!this.proposition) return;

        for (const hole of this.proposition.holes) {
            hole.status = HOLE_STATUS.PENDING;
            hole.filledContent = null;
            hole.errorMessage = null;
        }
        this.proposition.status = PROP_STATUS.IN_PROGRESS;
        this.proposition.steps = [];

        this._renderProofBody();
        this._updateStatusLed();
        this._setStatus('Proposition reset / 命题已重置');
    }

    /**
     * 更新状态指示灯
     * @private
     */
    _updateStatusLed() {
        if (!this._statusLed || !this.proposition) return;

        const holes = this.proposition.holes;
        if (holes.length === 0) {
            this._statusLed.style.background = STATUS_COLORS.PENDING;
            return;
        }

        let hasError = false;
        let hasPending = false;
        let allFilled = true;

        for (const hole of holes) {
            if (hole.status === HOLE_STATUS.TYPE_ERROR) hasError = true;
            if (hole.status === HOLE_STATUS.PENDING) hasPending = true;
            if (hole.status !== HOLE_STATUS.FILLED && hole.status !== HOLE_STATUS.PROVEN) {
                allFilled = false;
            }
        }

        if (hasError) {
            this._statusLed.style.background = STATUS_COLORS.TYPE_ERROR;
        } else if (hasPending) {
            this._statusLed.style.background = STATUS_COLORS.PENDING;
        } else if (allFilled) {
            this._statusLed.style.background = STATUS_COLORS.PROVEN;
        }
    }

    /**
     * 定位浮层在下锚点元素附近
     * @param {HTMLElement} popover
     * @param {HTMLElement} anchorEl
     * @private
     */
    _positionPopover(popover, anchorEl) {
        const rect = anchorEl.getBoundingClientRect();
        const pw = 300; // 估计宽度
        const ph = 350; // 估计高度

        let top = rect.bottom + 6;
        let left = rect.left;

        // 如果下方空间不够，显示在上方
        if (top + ph > window.innerHeight) {
            top = rect.top - ph - 6;
        }
        // 确保不出左边界
        if (left + pw > window.innerWidth) {
            left = window.innerWidth - pw - 10;
        }
        if (left < 10) left = 10;

        popover.style.top = top + 'px';
        popover.style.left = left + 'px';
    }

    /**
     * 关闭当前打开的浮层
     * @private
     */
    _closePopover() {
        if (this._activePopover) {
            this._activePopover.remove();
            this._activePopover = null;
        }
        if (this._externalClickHandler) {
            document.removeEventListener('click', this._externalClickHandler);
            this._externalClickHandler = null;
        }
    }

    /**
     * 设置状态栏消息
     * @param {string} message
     * @private
     */
    _setStatus(message) {
        if (this._statusMessage) {
            this._statusMessage.textContent = message;
        }
    }

    /**
     * 获取当前命题对象（供外部读取）
     * @returns {GeometryProposition|null}
     */
    getProposition() {
        return this.proposition;
    }

    /**
     * 获取填充进展统计
     * @returns {{ total: number, filled: number, pending: number, errors: number }}
     */
    getProgress() {
        if (!this.proposition) {
            return { total: 0, filled: 0, pending: 0, errors: 0 };
        }
        const stats = { total: 0, filled: 0, pending: 0, errors: 0 };
        for (const hole of this.proposition.holes) {
            stats.total++;
            if (hole.status === HOLE_STATUS.FILLED || hole.status === HOLE_STATUS.PROVEN) stats.filled++;
            if (hole.status === HOLE_STATUS.PENDING) stats.pending++;
            if (hole.status === HOLE_STATUS.TYPE_ERROR) stats.errors++;
        }
        return stats;
    }

    /**
     * 销毁面板，清理 DOM 引用和事件监听器
     */
    destroy() {
        this._closePopover();
        this.container = null;
        this.proposition = null;
        this._holeElementMap.clear();
        this._rendered = false;
    }

    /* ================================================================
     * 内部 DOM 工具方法
     * ================================================================ */

    /**
     * 创建 DOM 元素
     * @param {string} tag
     * @param {string} [className]
     * @param {Object} [style]
     * @param {string} [textContent]
     * @returns {HTMLElement}
     * @private
     */
    _el(tag, className, style, textContent) {
        const el = document.createElement(tag);
        if (className) el.className = className;
        if (style) {
            for (const key of Object.keys(style)) {
                el.style[key] = style[key];
            }
        }
        if (textContent) el.textContent = textContent;
        return el;
    }
}


/**
 * =========================================================================
 * 第六部分：与现有 proof_widgets_adapter.js 的集成桥接
 * =========================================================================
 */

/**
 * @class ProofBridge
 * @description 将 HoleDrivenProofPanel 与 ProofWidgetsAdapter 连接起来
 *
 *              当用户在洞驱动面板中填充一个 hole 时，
 *              自动在证明步骤高亮器中添加对应的证明步骤。
 *
 *              使用方式：
 *              ```javascript
 *              const bridge = new ProofBridge(holePanel, proofAdapter);
 *              bridge.connect();
 *              ```
 */
class ProofBridge {
    /**
     * @param {HoleDrivenProofPanel} holePanel
     * @param {ProofWidgetsAdapter}  proofAdapter
     */
    constructor(holePanel, proofAdapter) {
        /** @type {HoleDrivenProofPanel} */
        this.holePanel = holePanel;

        /** @type {ProofWidgetsAdapter} */
        this.proofAdapter = proofAdapter;

        /** @type {boolean} */
        this.connected = false;

        /** @type {number} */
        this._stepCounter = 0;
    }

    /**
     * 激活桥接——注册事件监听器
     */
    connect() {
        if (this.connected) return;

        this.holePanel.onHoleFill = (holeId, content) => {
            this._onHoleFilled(holeId, content);
        };

        this.connected = true;
    }

    /**
     * 断开桥接
     */
    disconnect() {
        this.holePanel.onHoleFill = null;
        this.connected = false;
    }

    /**
     * Hole 填充时的内部处理
     * @param {string} holeId
     * @param {string} content
     * @private
     */
    _onHoleFilled(holeId, content) {
        this._stepCounter++;

        // 构造一个证明步骤，同步到 ProofWidgetsAdapter
        const step = {
            id:                 'bridge_step_' + this._stepCounter,
            type:               'PROOF_STEP_ADD_NODE',
            node_id:            this._stepCounter,
            constraint_id:      -1,
            func_block_id:      -1,
            dependency_step_ids: [],
            note:               'Hole ' + holeId + ' filled :: ' + content,
            color:              'GREEN',
            is_completed:        true,
            timestamp:           Date.now(),
            description:         'Bridge: fill hole "' + holeId + '" with "' + content + '"',
        };

        const prop = this.holePanel.getProposition();
        if (prop) {
            prop.steps.push(step);
        }

        // 通知 ProofWidgetsAdapter
        if (this.proofAdapter && this.proofAdapter.setProofData) {
            this.proofAdapter.setProofData({
                steps:        prop ? prop.steps : [step],
                currentStep:  this._stepCounter - 1,
                targetProp:   { name: prop ? prop.name : 'unnamed' },
                dependencies: [],
                strategyNote: 'Hole-driven bridge / 洞驱动桥接',
            }, []);
        }
    }
}


/**
 * =========================================================================
 * 第七部分：HTML 集成指南
 * =========================================================================
 *
 * 在 index.html 中添加以下容器：
 *
 * ```html
 * <div id="hole-driven-proof-panel"></div>
 * ```
 *
 * 然后在 JavaScript 中初始化：
 *
 * ```javascript
 * // 初始化洞驱动证明面板
 * const holePanel = new HoleDrivenProofPanel({
 *     container: document.getElementById('hole-driven-proof-panel'),
 *     enableTypeCheck: true,
 *     enableAutoSuggest: true,
 * });
 *
 * // 加载几何命题
 * holePanel.loadProposition({
 *     name: 'isosceles_triangle_area',
 *     signature: '(a, b, c : Point) -> Real',
 *     statement: 'prove isosceles_triangle_area(a,b,c): area = 0.5 * base * height ?',
 *     category: 'triangle',
 * });
 *
 * // 可选：连接到证明小组件适配器
 * const bridge = new ProofBridge(holePanel, proofWidgetsAdapter);
 * bridge.connect();
 * ```
 */


/**
 * =========================================================================
 * 第八部分：模块导出
 * =========================================================================
 */

if (typeof module !== 'undefined' && module.exports) {
    module.exports = {
        HoleDrivenProofPanel,
        GeometryTypeChecker,
        FillSuggestionEngine,
        ProofBridge,
        FILL_SUGGESTION_TYPE,
        HOLE_STATUS,
        PROP_STATUS,
        STATUS_COLORS,
        STATUS_EMOJI,
        FILL_ICONS,
    };
}

if (typeof window !== 'undefined') {
    window.Lv00HoleDrivenProof = {
        HoleDrivenProofPanel,
        GeometryTypeChecker,
        FillSuggestionEngine,
        ProofBridge,
        FILL_SUGGESTION_TYPE,
        HOLE_STATUS,
        PROP_STATUS,
        STATUS_COLORS,
        STATUS_EMOJI,
        FILL_ICONS,
    };
}
