/**
 * ============================================================================
 *  mathlive_integration.js — MathLive 公式编辑器集成
 * ============================================================================
 *
 * 为 Lv-00 几何元语言 Web GUI 提供 MathLive 实时公式编辑器的封装层。
 *
 * MathLive 是一个成熟的 Web Component（<mathlive-mathfield>），支持：
 *   - LaTeX 实时输入与渲染
 *   - 虚拟数学键盘（触屏/桌面均可用）
 *   - MathJSON 结构化输出（基于 CortexJS/MathJSON 格式）
 *   - 自定义 LaTeX 宏（可用于注册 Lv-00 几何专用命令）
 *
 * 本模块提供：
 *   1. MathLive 编辑器初始化与配置封装
 *   2. mathlive_to_lv00_format() — 将 MathLive 输出的 MathJSON 转为
 *      Lv-00 内部几何构造格式（点/线段/圆/约束）
 *   3. lv00_to_mathlive_format()  — 将 Lv-00 内部格式转回 LaTeX，
 *      供 MathLive 渲染显示
 *   4. Lv-00 几何专用 LaTeX 宏注册（\point, \line, \circle, 等）
 *
 * 依赖（运行时通过 CDN 加载）：
 *   - MathLive core: https://unpkg.com/mathlive/dist/mathlive.min.js
 *   - MathLive CSS:  https://unpkg.com/mathlive/dist/mathlive-fonts.css
 *
 * 格式说明：
 *   - 本文件使用 ES module 格式（export/import）
 *   - 通过 <script type="module"> 引入，或动态 import()
 *
 * 版本：1.0.0
 * 作者：Lv-00 Team
 * 创建日期：2026-05-24
 * ============================================================================
 *
 * @module mathlive_integration
 */

// ============================================================================
//  类型定义（JSDoc，非运行时）
// ============================================================================

/**
 * @typedef {Object} MathLiveConfig
 * @property {string}  [containerId]      - 挂载 MathLive 编辑器的 DOM 元素 ID
 * @property {string}  [initialValue]     - 初始 LaTeX 字符串
 * @property {boolean} [virtualKeyboard]  - 是否启用虚拟键盘（默认 true）
 * @property {string}  [virtualKeyboardMode] - 虚拟键盘模式：'manual'|'onfocus'|'off'
 * @property {boolean} [smartFence]       - 是否启用智能括号匹配（默认 true）
 * @property {boolean} [smartMode]        - 是否启用智能模式（默认 true）
 * @property {string}  [locale]           - 界面语言（默认 'zh-CN'）
 * @property {Object}  [macros]           - 用户自定义 LaTeX 宏
 * @property {Object}  [onContentChange]  - 内容变化回调
 */

/**
 * @typedef {Object} Lv00GeometricNode
 * @property {string}       type       - 节点类型：'point'|'line'|'circle'|'constraint'|'equation'
 * @property {string}       [name]     - 节点名称标签
 * @property {Array<number>}[coords]   - 坐标 [x, y]（点/圆心）
 * @property {string}       [labelA]   - 端点 A 标签（线段）
 * @property {string}       [labelB]   - 端点 B 标签（线段）
 * @property {number}       [radius]   - 半径（圆）
 * @property {string}       [centerName] - 圆心名称（圆）
 * @property {string}       [kind]     - 约束类型（约束）
 * @property {Array<string>}[args]     - 约束参数标签列表（约束）
 * @property {string}       [latex]    - LaTeX 表示字符串
 * @property {string}       [raw]      - 原始输入文本（方程）
 */

/**
 * @typedef {Object} MathLiveIntegrationResult
 * @property {boolean}               success   - 转换是否成功
 * @property {Array<Lv00GeometricNode>} nodes  - Lv-00 几何节点列表
 * @property {Array<string>}         errors    - 错误消息列表
 * @property {Array<string>}         warnings  - 警告消息列表
 */

// ============================================================================
//  常量
// ============================================================================

/**
 * 已知数学函数名集合（不视为用户变量）
 * 用于在 MathJSON 解析中区分内置函数与用户自定义符号。
 * @type {Set<string>}
 */
const KNOWN_FUNCTIONS = new Set([
    'Add', 'Subtract', 'Multiply', 'Divide', 'Negate', 'Power', 'Sqrt', 'Root',
    'Abs', 'Exp', 'Ln', 'Log', 'Sin', 'Cos', 'Tan', 'Arcsin', 'Arccos', 'Arctan',
    'Sinh', 'Cosh', 'Tanh', 'Equal', 'NotEqual', 'Less', 'Greater',
    'LessEqual', 'GreaterEqual', 'And', 'Or', 'Not', 'Implies', 'Equivalent',
    'List', 'Tuple', 'Set', 'Sequence', 'Sum', 'Product', 'Integral',
    'Derivative', 'PartialDerivative', 'Limit', 'Floor', 'Ceil', 'Round',
    'Piecewise', 'Matrix', 'Subscript', 'Superscript', 'At', 'Range',
    'Lv00Point', 'Lv00Line', 'Lv00Circle', 'Lv00Intersection',
    'Lv00Constraint', 'Lv00Solve', 'Lv00Prove'
]);

/**
 * Lv-00 几何专用 LaTeX 宏定义
 *
 * 这些宏被注册到 MathLive 中，使得用户可以直接输入几何命令：
 *   \point{A}{0}{0}     — 创建点 A 在坐标 (0, 0)
 *   \line{AB}{A}{B}     — 创建线段 AB 连接点 A 和 B
 *   \circle{O}{A}{3}    — 创建以 A 为圆心、半径为 3 的圆 O
 *   \intersection{X}{l1}{l2}    — 创建线段 l1 和 l2 的交点 X
 *   \constraint{perpendicular}{A}{B}{C}  — 创建 AB ⟂ BC 的垂直约束
 *   \triangle{ABC}{A}{B}{C}     — 创建三角形 ABC
 *   \midpoint{M}{A}{B}         — 创建 AB 的中点 M
 *
 * @type {Object<string, {args: number, def: string}>}
 */
const LV00_GEOMETRY_MACROS = {
    '\\point': {
        args: 3,
        def: '\\operatorname{point}\\left(#1,#2,#3\\right)'
    },
    '\\line': {
        args: 3,
        def: '\\operatorname{line}\\left(#1,#2,#3\\right)'
    },
    '\\circle': {
        args: 3,
        def: '\\operatorname{circle}\\left(#1,#2,#3\\right)'
    },
    '\\intersection': {
        args: 3,
        def: '\\operatorname{intersection}\\left(#1,#2,#3\\right)'
    },
    '\\constraint': {
        args: 3,
        def: '\\operatorname{constraint}\\left(#1,#2,#3\\right)'
    },
    '\\triangle': {
        args: 4,
        def: '\\operatorname{triangle}\\left(#1,#2,#3,#4\\right)'
    },
    '\\midpoint': {
        args: 3,
        def: '\\operatorname{midpoint}\\left(#1,#2,#3\\right)'
    }
};

// ============================================================================
//  MathLive 编辑器初始化和配置
// ============================================================================

/**
 * 加载 MathLive 核心库（从 CDN 动态加载）
 *
 * 如果 MathLive 尚未加载，会动态创建 <script> 标签并插入页面，
 * 返回一个在加载完成后 resolve 的 Promise。
 *
 * @param {string}  [cdnUrl] - MathLive JS 的 CDN URL
 *                             默认使用 unpkg CDN 最新稳定版
 * @returns {Promise<void>} 加载完成后 resolve
 *
 * @example
 * // 确保 MathLive 已加载后再挂载编辑器
 * await loadMathLive();
 * const mf = createMathField('formula-editor');
 */
export async function loadMathLive(cdnUrl) {
    const url = cdnUrl || 'https://unpkg.com/mathlive/dist/mathlive.min.js';

    // 已经加载过
    if (window.MathLive && window.MathLive.makeMathField) {
        return;
    }

    // 正在加载中，等待现有 Promise
    if (loadMathLive._pending) {
        return loadMathLive._pending;
    }

    loadMathLive._pending = new Promise((resolve, reject) => {
        const script = document.createElement('script');
        script.src = url;
        script.async = true;
        script.onload = () => {
            loadMathLive._pending = null;
            resolve();
        };
        script.onerror = () => {
            loadMathLive._pending = null;
            reject(new Error('MathLive 脚本加载失败: ' + url));
        };
        document.head.appendChild(script);
    });

    return loadMathLive._pending;
}

/**
 * 创建并挂载一个 MathLive mathfield 到指定 DOM 容器
 *
 * 使用 MathLive v0.x 的 makeMathField() API 创建一个可编辑的公式输入区域，
 * 挂载到 containerId 指定的 DOM 元素内。
 *
 * @param {string|HTMLElement} container   - 挂载目标（DOM 元素 ID 或元素本身）
 * @param {MathLiveConfig}     [config={}] - 配置选项
 * @returns {Object} MathLive mathfield 实例，包含：
 *   - $el            — 根 DOM 元素
 *   - $latex()       — 获取当前 LaTeX 字符串
 *   - $text()         — 获取纯文本
 *   - $selectedText() — 获取选中文本
 *   - $insert(s)      — 插入字符串
 *   - $selected(s)    — 替换选中内容
 *   - $perform(cmd)   — 执行编辑命令
 *   - $focus()        — 聚焦编辑器
 *   - $blur()         — 失去焦点
 *   - $config         — 当前配置对象
 *   - getValue(format)— 获取 MathJSON 或其他格式的值
 *   - setValue(val, format) — 设置内容
 *
 * @throws {Error} 如果 MathLive 未加载
 *
 * @example
 * // 基本用法
 * const mf = createMathField('formula-input', {
 *     initialValue: 'x^2 + y^2 = 9',
 *     virtualKeyboard: true,
 *     onContentChange: (latex) => console.log('LaTeX:', latex)
 * });
 *
 * @example
 * // 在自定义容器中挂载
 * const container = document.getElementById('math-area');
 * const mf = createMathField(container, { initialValue: '\\point{A}{0}{0}' });
 */
export function createMathField(container, config) {
    if (!window.MathLive || !window.MathLive.makeMathField) {
        throw new Error(
            'MathLive 未加载。请先调用 loadMathLive() 或确保 mathlive.min.js 已通过 ' +
            '<script> 标签引入。'
        );
    }

    /** @type {MathLiveConfig} */
    const cfg = config || {};

    // 解析容器
    const containerEl = (typeof container === 'string')
        ? document.getElementById(container)
        : container;

    if (!containerEl) {
        throw new Error('MathLive 挂载容器未找到: ' + container);
    }

    // 构建 MathLive 配置
    const mlConfig = {
        // 初始 LaTeX 内容
        initialValue: cfg.initialValue || '',

        // 虚拟键盘
        virtualKeyboardMode: cfg.virtualKeyboard !== false
            ? (cfg.virtualKeyboardMode || 'onfocus')
            : 'off',

        // 智能括号
        smartFence: cfg.smartFence !== false,

        // 智能模式
        smartMode: cfg.smartMode !== false,

        // 自定义宏
        macros: Object.assign({}, LV00_GEOMETRY_MACROS, cfg.macros || {}),

        // 内容变化回调
        onContentDidChange: (mf) => {
            if (typeof cfg.onContentChange === 'function') {
                cfg.onContentChange(mf.$latex());
            }
        }
    };

    // 创建 mathfield
    const mf = window.MathLive.makeMathField(containerEl, mlConfig);

    // 存储引用
    containerEl._mathliveField = mf;

    return mf;
}

/**
 * 创建 MathLive 静态渲染元素（只读公式展示）
 *
 * 使用 MathLive 的 makeStaticMath() API 将 LaTeX 字符串渲染为只读的
 * 高质量数学公式展示元素。
 *
 * @param {string|HTMLElement} container - 挂载目标
 * @param {string}             latex     - LaTeX 公式字符串
 * @returns {HTMLElement} 渲染后的 DOM 元素
 *
 * @example
 * const el = createStaticMath('output-area', 'x^2 + y^2 = 9');
 * // el 包含渲染好的公式 DOM
 */
export function createStaticMath(container, latex) {
    if (!window.MathLive || !window.MathLive.makeStaticMath) {
        throw new Error('MathLive 未加载。');
    }

    const containerEl = (typeof container === 'string')
        ? document.getElementById(container)
        : container;

    const span = window.MathLive.makeStaticMath(latex);
    if (containerEl) {
        containerEl.appendChild(span);
    }
    return span;
}

// ============================================================================
//  注册 Lv-00 几何专用 LaTeX 宏
// ============================================================================

/**
 * 将 Lv-00 几何专用 LaTeX 宏注册到 MathLive 的宏字典中
 *
 * 此函数将 LV00_GEOMETRY_MACROS 中定义的所有宏（如 \point, \line,
 * \circle 等）逐个注入到 MathLive 的全局宏注册表。
 * 调用后，MathLive 编辑器将能够正确识别和渲染这些几何命令。
 *
 * 宏列表：
 *   \point{name}{x}{y}            — 定义几何点
 *   \line{name}{from}{to}         — 定义线段
 *   \circle{name}{center}{radius} — 定义圆
 *   \intersection{name}{l1}{l2}    — 两条线段的交点
 *   \constraint{kind}{a}{b}       — 几何约束
 *   \triangle{name}{A}{B}{C}      — 三角形
 *   \midpoint{name}{A}{B}         — 中点
 *
 * @param {Object} [extraMacros={}] - 额外的自定义宏（会合并到现有宏中）
 * @returns {Object} 已注册的宏字典
 *
 * @example
 * // 注册 Lv-00 默认宏
 * registerLv00Macros();
 *
 * // 注册额外宏
 * registerLv00Macros({
 *     '\\distance': { args: 3, def: '\\operatorname{dist}\\left(#1,#2,#3\\right)' }
 * });
 */
export function registerLv00Macros(extraMacros) {
    if (!window.MathLive) {
        console.warn('[mathlive_integration] MathLive 未加载，延迟注册宏。');
        return LV00_GEOMETRY_MACROS;
    }

    const allMacros = Object.assign({}, LV00_GEOMETRY_MACROS, extraMacros || {});

    // MathLive 的宏注册方式（取决于版本）：
    // v0.x 通过 MathLive.macros 或配置传递
    // v1.x 通过全局 register 或 MathfieldElement 静态属性
    try {
        // 尝试 v1.x API
        if (typeof window.MathLive.registerMacro === 'function') {
            for (const [name, info] of Object.entries(allMacros)) {
                window.MathLive.registerMacro(name, info);
            }
        } else if (window.MathLive.macros) {
            // 尝试 v0.x 全局字典
            Object.assign(window.MathLive.macros, allMacros);
        }
    } catch (e) {
        console.warn('[mathlive_integration] 无法注册宏到 MathLive:', e.message);
    }

    return allMacros;
}

// ============================================================================
//  自定义 LaTeX 宏的解析与生成
// ============================================================================

/**
 * 解析 Lv-00 几何 LaTeX 宏命令为 Lv00GeometricNode 对象
 *
 * 将形如 \point{A}{0}{0} 或 \line{AB}{A}{B} 的 LaTeX 宏调用
 * 解析为结构化的 Lv00GeometricNode 对象。
 *
 * @param {string} macroName - 宏名称（如 'point', 'line', 'circle'）
 * @param {Array<string|number>} args - 宏参数列表
 * @returns {Lv00GeometricNode|null} 解析后的节点，无法识别返回 null
 *
 * @example
 * const node = parseGeometryMacro('point', ['A', 0, 0]);
 * // => { type: 'point', name: 'A', coords: [0, 0], latex: '\\point{A}{0}{0}' }
 */
export function parseGeometryMacro(macroName, args) {
    if (!macroName || !args) return null;

    switch (macroName.toLowerCase()) {
        case 'point': {
            const name = String(args[0] || 'P');
            const x = parseFloat(args[1]) || 0;
            const y = parseFloat(args[2]) || 0;
            return {
                type: 'point',
                name: name,
                coords: [x, y],
                latex: `\\point{${name}}{${x}}{${y}}`
            };
        }

        case 'line': {
            const name = String(args[0] || 'l');
            const labelA = String(args[1] || 'A');
            const labelB = String(args[2] || 'B');
            return {
                type: 'line',
                name: name,
                labelA: labelA,
                labelB: labelB,
                latex: `\\line{${name}}{${labelA}}{${labelB}}`
            };
        }

        case 'circle': {
            const name = String(args[0] || 'C');
            const centerName = String(args[1] || 'O');
            const radius = parseFloat(args[2]) || 1;
            return {
                type: 'circle',
                name: name,
                centerName: centerName,
                radius: radius,
                latex: `\\circle{${name}}{${centerName}}{${radius}}`
            };
        }

        case 'intersection': {
            const name = String(args[0] || 'X');
            const line1 = String(args[1] || 'l1');
            const line2 = String(args[2] || 'l2');
            return {
                type: 'constraint',
                kind: 'intersection',
                name: name,
                args: [line1, line2],
                latex: `\\intersection{${name}}{${line1}}{${line2}}`
            };
        }

        case 'constraint': {
            const kind = String(args[0] || 'unknown');
            const a = String(args[1] || '');
            const b = String(args[2] || '');
            return {
                type: 'constraint',
                kind: kind,
                args: [a, b],
                latex: `\\constraint{${kind}}{${a}}{${b}}`
            };
        }

        case 'triangle': {
            const name = String(args[0] || 'T');
            const A = String(args[1] || 'A');
            const B = String(args[2] || 'B');
            const C = String(args[3] || 'C');
            return {
                type: 'constraint',
                kind: 'triangle',
                name: name,
                args: [A, B, C],
                latex: `\\triangle{${name}}{${A}}{${B}}{${C}}`
            };
        }

        case 'midpoint': {
            const name = String(args[0] || 'M');
            const A = String(args[1] || 'A');
            const B = String(args[2] || 'B');
            return {
                type: 'constraint',
                kind: 'midpoint',
                name: name,
                args: [A, B],
                latex: `\\midpoint{${name}}{${A}}{${B}}`
            };
        }

        default:
            return null;
    }
}

/**
 * 将 Lv00GeometricNode 转换为 LaTeX 宏调用字符串
 *
 * @param {Lv00GeometricNode} node - Lv-00 几何节点
 * @returns {string} LaTeX 宏调用字符串
 *
 * @example
 * toGeometryLatex({ type: 'point', name: 'A', coords: [0, 0] })
 * // => '\\point{A}{0}{0}'
 */
export function toGeometryLatex(node) {
    if (!node || !node.type) return '';

    switch (node.type) {
        case 'point':
            return `\\point{${node.name || 'P'}}{${(node.coords || [0, 0])[0]}}{${(node.coords || [0, 0])[1]}}`;

        case 'line':
            return `\\line{${node.name || 'l'}}{${node.labelA || 'A'}}{${node.labelB || 'B'}}`;

        case 'circle':
            return `\\circle{${node.name || 'C'}}{${node.centerName || 'O'}}{${node.radius || 1}}`;

        case 'constraint': {
            const kind = node.kind || 'unknown';
            const args = node.args || [];
            if (kind === 'intersection') {
                return `\\intersection{${node.name || 'X'}}{${args[0] || ''}}{${args[1] || ''}}`;
            }
            if (kind === 'triangle') {
                return `\\triangle{${node.name || 'T'}}{${args[0] || 'A'}}{${args[1] || 'B'}}{${args[2] || 'C'}}`;
            }
            if (kind === 'midpoint') {
                return `\\midpoint{${node.name || 'M'}}{${args[0] || 'A'}}{${args[1] || 'B'}}`;
            }
            return `\\constraint{${kind}}{${args[0] || ''}}{${args[1] || ''}}`;
        }

        case 'equation':
            return node.latex || node.raw || '';

        default:
            return node.latex || '';
    }
}

// ============================================================================
//  MathJSON 解析辅助
// ============================================================================

/**
 * 将 MathJSON 数值节点解析为 JavaScript 数字
 *
 * 支持 MathJSON 数值表示：
 *   - 直接数字：3.14, 42
 *   - 负数表示：["Negate", 5]
 *   - 分数表示：["Divide", 1, 2]
 *
 * @param {*} node - MathJSON 数值节点
 * @returns {number} 解析后的数字，无法解析返回 0
 */
function parseMathJSONNumber(node) {
    if (typeof node === 'number') return node;
    if (typeof node === 'string') {
        const n = parseFloat(node);
        return isNaN(n) ? 0 : n;
    }
    if (Array.isArray(node) && node.length > 0) {
        const head = node[0];
        if (head === 'Negate' && node.length >= 2) {
            return -parseMathJSONNumber(node[1]);
        }
        if (head === 'Divide' && node.length >= 3) {
            const num = parseMathJSONNumber(node[1]);
            const den = parseMathJSONNumber(node[2]);
            return den !== 0 ? num / den : 0;
        }
    }
    return 0;
}

/**
 * 将 MathJSON 节点展平为可读字符串
 *
 * 递归遍历 MathJSON 表达式树，生成人类可读的字符串表示。
 * 用于错误消息、日志输出等场景。
 *
 * @param {*} node - MathJSON 节点
 * @returns {string} 可读字符串
 */
function mathJSONToDisplayString(node) {
    if (typeof node === 'number') return String(node);
    if (typeof node === 'string') return node;
    if (!Array.isArray(node) || node.length === 0) return String(node);

    const head = node[0];
    const args = node.slice(1);

    if (head === 'Add') return args.map(mathJSONToDisplayString).join(' + ');
    if (head === 'Subtract') return args.map(mathJSONToDisplayString).join(' - ');
    if (head === 'Multiply') return args.map(mathJSONToDisplayString).join(' * ');
    if (head === 'Divide' && args.length >= 2) {
        return `(${mathJSONToDisplayString(args[0])})/(${mathJSONToDisplayString(args[1])})`;
    }
    if (head === 'Negate' && args.length >= 1) {
        return `-${mathJSONToDisplayString(args[0])}`;
    }
    if (head === 'Power' && args.length >= 2) {
        return `${mathJSONToDisplayString(args[0])}^${mathJSONToDisplayString(args[1])}`;
    }
    if (head === 'Sqrt') {
        return `sqrt(${args.map(mathJSONToDisplayString).join(', ')})`;
    }
    if (head === 'Equal') {
        return `${mathJSONToDisplayString(args[0])} = ${mathJSONToDisplayString(args[1])}`;
    }

    // 通用函数形式
    return `${head}(${args.map(mathJSONToDisplayString).join(', ')})`;
}

/**
 * 展开 MathJSON 表达式，识别 Lv-00 几何语义
 *
 * 遍历 MathJSON AST，将顶层的 Lv-00 几何宏调用（Lv00Point, Lv00Line,
 * Lv00Circle 等）提取为 Lv00GeometricNode 对象，同时保留非几何表达式
 * 作为 equation 节点。
 *
 * @param {Array} mathJSON - MathJSON 表达式数组
 * @returns {MathLiveIntegrationResult} 转换结果
 */
function extractGeometryFromMathJSON(mathJSON) {
    /** @type {Array<Lv00GeometricNode>} */
    const nodes = [];
    /** @type {Array<string>} */
    const errors = [];
    /** @type {Array<string>} */
    const warnings = [];

    if (!Array.isArray(mathJSON) || mathJSON.length === 0) {
        errors.push('MathJSON 表达式为空或格式无效');
        return { success: false, nodes, errors, warnings };
    }

    const head = mathJSON[0];
    const args = mathJSON.slice(1);

    // 识别 Lv-00 几何函数
    switch (head) {
        case 'Lv00Point': {
            if (args.length < 3) {
                errors.push('Lv00Point 需要至少 3 个参数 (name, x, y)');
                break;
            }
            const name = String(args[0]);
            const x = parseMathJSONNumber(args[1]);
            const y = parseMathJSONNumber(args[2]);
            nodes.push({
                type: 'point',
                name: name,
                coords: [x, y],
                latex: `\\point{${name}}{${x}}{${y}}`
            });
            break;
        }

        case 'Lv00Line': {
            if (args.length < 3) {
                errors.push('Lv00Line 需要至少 3 个参数 (name, from, to)');
                break;
            }
            const name = String(args[0]);
            const labelA = String(args[1]);
            const labelB = String(args[2]);
            nodes.push({
                type: 'line',
                name: name,
                labelA: labelA,
                labelB: labelB,
                latex: `\\line{${name}}{${labelA}}{${labelB}}`
            });
            break;
        }

        case 'Lv00Circle': {
            if (args.length < 3) {
                errors.push('Lv00Circle 需要至少 3 个参数 (name, center, radius)');
                break;
            }
            const name = String(args[0]);
            const centerName = String(args[1]);
            const radius = parseMathJSONNumber(args[2]);
            nodes.push({
                type: 'circle',
                name: name,
                centerName: centerName,
                radius: radius,
                latex: `\\circle{${name}}{${centerName}}{${radius}}`
            });
            break;
        }

        case 'Lv00Intersection': {
            if (args.length < 3) {
                errors.push('Lv00Intersection 需要至少 3 个参数 (name, l1, l2)');
                break;
            }
            const name = String(args[0]);
            const l1 = String(args[1]);
            const l2 = String(args[2]);
            nodes.push({
                type: 'constraint',
                kind: 'intersection',
                name: name,
                args: [l1, l2],
                latex: `\\intersection{${name}}{${l1}}{${l2}}`
            });
            break;
        }

        case 'Lv00Constraint': {
            if (args.length < 2) {
                errors.push('Lv00Constraint 需要至少 2 个参数 (kind, ...participants)');
                break;
            }
            const kind = String(args[0]);
            const participants = args.slice(1).map(String);
            nodes.push({
                type: 'constraint',
                kind: kind,
                args: participants,
                latex: `\\constraint{${kind}}{${participants.join('}{')}}`
            });
            break;
        }

        case 'Lv00Solve': {
            warnings.push('Lv00Solve 为后端求解指令，非几何节点');
            break;
        }

        case 'Lv00Prove': {
            const statement = args.length > 0 ? mathJSONToDisplayString(args[0]) : '';
            warnings.push('Lv00Prove 为证明指令: ' + (statement || '(空)'));
            break;
        }

        case 'Sequence':
        case 'List':
        case 'Tuple': {
            // 顶层容器：递归处理每个子项
            for (const item of args) {
                if (Array.isArray(item)) {
                    const subResult = extractGeometryFromMathJSON(item);
                    for (const n of subResult.nodes) nodes.push(n);
                    for (const e of subResult.errors) errors.push(e);
                    for (const w of subResult.warnings) warnings.push(w);
                }
            }
            break;
        }

        default: {
            // 非几何表达式：视为方程
            const latex = mathJSONToDisplayString(mathJSON);
            nodes.push({
                type: 'equation',
                raw: latex,
                latex: latex
            });
            break;
        }
    }

    return {
        success: errors.length === 0,
        nodes: nodes,
        errors: errors,
        warnings: warnings
    };
}

// ============================================================================
//  主转换函数：MathJSON <-> Lv-00 格式
// ============================================================================

/**
 * 将 MathLive 输出的 MathJSON 转换为 Lv-00 内部几何构造格式
 *
 * 这是本模块的核心转换函数。MathLive 编辑器通过 getValue('math-json')
 * 可以获取到当前公式的 MathJSON 表示，此函数将其翻译为 Lv-00 Web GUI
 * 可以消费的结构化几何节点列表。
 *
 * 转换逻辑：
 *   1. 如果 MathJSON 中包含 Lv-00 几何宏调用（Lv00Point 等），
 *      直接提取为几何节点。
 *   2. 如果 MathJSON 中是普通代数表达式，尝试识别几何语义：
 *      - 点坐标赋值（x = ..., y = ...）
 *      - 方程（x^2 + y^2 = ...）
 *      - 距离/中点公式
 *   3. 无法识别的表达式作为方程节点保留。
 *
 * @param {*}      mathliveJSON - MathLive getValue('math-json') 的输出
 *                                可以是 MathJSON 数组或包含 MathJSON 的对象
 * @param {Object} [options={}] - 转换选项
 * @param {boolean}[options.strict] - 严格模式：仅识别显式 Lv-00 几何宏（默认 false）
 * @returns {MathLiveIntegrationResult} 转换结果
 *
 * @example
 * const mf = createMathField('input');
 * const json = mf.getValue('math-json');
 *
 * const result = mathlive_to_lv00_format(json);
 * if (result.success) {
 *     for (const node of result.nodes) {
 *         console.log(node.type, node.latex);
 *     }
 * }
 */
export function mathlive_to_lv00_format(mathliveJSON, options) {
    const opts = options || {};
    /** @type {Array<Lv00GeometricNode>} */
    const nodes = [];
    /** @type {Array<string>} */
    const errors = [];
    /** @type {Array<string>} */
    const warnings = [];

    if (!mathliveJSON) {
        errors.push('输入为空');
        return { success: false, nodes, errors, warnings };
    }

    // 如果输入是对象且包含 mathjson 字段，解包
    let rawJSON = mathliveJSON;
    if (typeof rawJSON === 'object' && !Array.isArray(rawJSON)) {
        if (rawJSON.mathjson) rawJSON = rawJSON.mathjson;
        else if (rawJSON.value) rawJSON = rawJSON.value;
    }

    // 字符串：尝试解析为 JSON
    if (typeof rawJSON === 'string') {
        try {
            rawJSON = JSON.parse(rawJSON);
        } catch (e) {
            errors.push('无法解析输入的 JSON 字符串: ' + e.message);
            return { success: false, nodes, errors, warnings };
        }
    }

    // 顶级数组直接处理
    if (Array.isArray(rawJSON)) {
        const result = extractGeometryFromMathJSON(rawJSON);
        for (const n of result.nodes) nodes.push(n);
        for (const e of result.errors) errors.push(e);
        for (const w of result.warnings) warnings.push(w);
    } else if (typeof rawJSON === 'object') {
        // 对象形式：尝试遍历所有顶层键
        let found = false;
        for (const key of Object.keys(rawJSON)) {
            const val = rawJSON[key];
            if (Array.isArray(val)) {
                found = true;
                const result = extractGeometryFromMathJSON(val);
                for (const n of result.nodes) nodes.push(n);
                for (const e of result.errors) errors.push(e);
                for (const w of result.warnings) warnings.push(w);
            }
        }
        if (!found) {
            errors.push('输入对象中未找到可用 MathJSON 数组');
        }
    } else {
        errors.push('不支持的输入类型: ' + typeof rawJSON);
    }

    // 严格模式下，过滤非显式几何节点
    if (opts.strict) {
        for (let i = nodes.length - 1; i >= 0; i--) {
            if (nodes[i].type === 'equation') {
                warnings.push('严格模式：移除未识别为几何命令的表达式: ' + nodes[i].raw);
                nodes.splice(i, 1);
            }
        }
    }

    return {
        success: errors.length === 0,
        nodes: nodes,
        errors: errors,
        warnings: warnings
    };
}

/**
 * 将 Lv-00 公式/几何节点转换回 MathLive 可渲染的 LaTeX 字符串
 *
 * 支持多种输入格式：
 *   - 单个 Lv00GeometricNode 对象
 *   - Lv00GeometricNode 数组
 *   - 包含 AST 的 Lv-00 公式对象
 *   - 原始的 LaTeX 字符串
 *   - 图转换结果对象（graph_to_formula.js 的输出）
 *
 * @param {*}      lv00Formula - Lv-00 公式数据
 * @param {Object} [options={}] - 转换选项
 * @param {boolean}[options.wrapInAlign]  - 用 \begin{align}...\end{align} 包裹多公式
 * @param {boolean}[options.useGeometryMacros] - 使用 Lv-00 几何宏而非纯 LaTeX
 * @returns {string} MathLive 可渲染的 LaTeX 字符串
 *
 * @example
 * // 从 Lv-00 图转换结果生成 LaTeX
 * const latex = lv00_to_mathlive_format(graphConversionResult);
 * mf.setValue(latex);
 *
 * @example
 * // 从几何节点数组生成 LaTeX
 * const latex = lv00_to_mathlive_format([
 *     { type: 'point', name: 'A', coords: [0, 0] },
 *     { type: 'point', name: 'B', coords: [3, 4] },
 *     { type: 'line', name: 'AB', labelA: 'A', labelB: 'B' }
 * ]);
 * // => '\\point{A}{0}{0}\n\\point{B}{3}{4}\n\\line{AB}{A}{B}'
 */
export function lv00_to_mathlive_format(lv00Formula, options) {
    const opts = options || {};

    if (!lv00Formula) return '';

    // 1. 字符串：直接返回（可能已经是 LaTeX）
    if (typeof lv00Formula === 'string') {
        return lv00Formula;
    }

    // 2. 数组：每个元素递归处理
    if (Array.isArray(lv00Formula)) {
        const parts = lv00Formula.map(item => lv00_to_mathlive_format(item, opts))
            .filter(s => s && s.length > 0);
        if (opts.wrapInAlign && parts.length > 1) {
            return '\\begin{aligned}\n' +
                parts.map(p => '  ' + p + ' \\\\').join('\n') +
                '\n\\end{aligned}';
        }
        return parts.join('\n');
    }

    // 3. Lv00GeometricNode 对象
    if (lv00Formula.type &&
        ['point', 'line', 'circle', 'constraint', 'equation'].indexOf(lv00Formula.type) >= 0) {
        if (opts.useGeometryMacros) {
            return toGeometryLatex(lv00Formula);
        }
        // 回退到节点自带的 latex 字符串
        if (lv00Formula.latex) return lv00Formula.latex;
        return toGeometryLatex(lv00Formula);
    }

    // 4. Lv-00 公式对象（包含 AST）
    if (lv00Formula.ast || lv00Formula.type === 'compound') {
        const ast = lv00Formula.ast || lv00Formula;
        return astToLatexString(ast);
    }

    // 5. 图转换结果对象（graph_to_formula.js 输出）
    if (lv00Formula.fullLatex) {
        return lv00Formula.fullLatex;
    }
    if (lv00Formula.points || lv00Formula.segments || lv00Formula.equations) {
        const latexLines = [];

        // 点
        if (lv00Formula.points) {
            for (const pt of lv00Formula.points) {
                const ptLatex = pt.latex || `P_{${pt.id || '?'}} = (x_${pt.id || '?'}, y_${pt.id || '?'})`;
                latexLines.push(ptLatex);
            }
        }

        // 线段
        if (lv00Formula.segments) {
            for (const seg of lv00Formula.segments) {
                const segLatex = seg.latex || seg.endpoints || `l_{${seg.id || '?'}}`;
                latexLines.push(segLatex);
            }
        }

        // 方程
        if (lv00Formula.equations) {
            for (const eq of lv00Formula.equations) {
                latexLines.push(eq.latex || eq.description || '');
            }
        }

        // 约束
        if (lv00Formula.constraints) {
            for (const c of lv00Formula.constraints) {
                latexLines.push(c.latex || `\\text{constraint}_${c.type || '?'}`);
            }
        }

        if (opts.wrapInAlign && latexLines.length > 1) {
            return '\\begin{aligned}\n' +
                latexLines.map(l => '  ' + l + ' \\\\').join('\n') +
                '\n\\end{aligned}';
        }
        return latexLines.filter(Boolean).join('\n');
    }

    // 6. 兜底：尝试 JSON 序列化
    try {
        return JSON.stringify(lv00Formula);
    } catch (e) {
        return String(lv00Formula);
    }
}

/**
 * 将 Lv-00 公式 AST 递归转换为 LaTeX 字符串
 *
 * @param {Object} ast - Lv-00 公式 AST 节点
 * @returns {string} LaTeX 字符串
 */
function astToLatexString(ast) {
    if (!ast) return '';

    // 复合语句
    if (ast.type === 'compound' && ast.statements) {
        return ast.statements.map(astToLatexString)
            .filter(Boolean)
            .join('\n');
    }

    // 点定义
    if (ast.type === 'point') {
        const name = ast.name || 'P';
        const x = ast.args ? (ast.args[0] && ast.args[0].value) || 0 : (ast.x || 0);
        const y = ast.args ? (ast.args[1] && ast.args[1].value) || 0 : (ast.y || 0);
        return `\\point{${name}}{${x}}{${y}}`;
    }

    // 线段定义
    if (ast.type === 'segment' || ast.type === 'line') {
        const name = ast.name || 'l';
        const from = ast.from ? (ast.from.name || ast.from) : (ast.args ? ast.args[0].name || ast.args[0] : 'A');
        const to = ast.to ? (ast.to.name || ast.to) : (ast.args ? ast.args[1].name || ast.args[1] : 'B');
        return `\\line{${name}}{${from}}{${to}}`;
    }

    // 圆定义
    if (ast.type === 'circle') {
        const name = ast.name || 'C';
        const center = ast.center ? (ast.center.name || ast.center) : 'O';
        const radius = ast.radius || 1;
        return `\\circle{${name}}{${center}}{${radius}}`;
    }

    // 约束
    if (ast.type === 'constraint') {
        const kind = ast.kind || '';
        const args = ast.args || [];
        if (kind === 'perpendicular' && args.length >= 3) {
            const a = args[0].name || args[0];
            const b = args[1].name || args[1];
            const c = args[2].name || args[2];
            return `${a}${b} \\perp ${b}${c}`;
        }
        if (kind === 'parallel' && args.length >= 2) {
            return `${args[0].name || args[0]} \\parallel ${args[1].name || args[1]}`;
        }
        if (kind === 'midpoint' && args.length >= 2) {
            const midName = ast.name || 'M';
            return `\\midpoint{${midName}}{${args[0].name || args[0]}}{${args[1].name || args[1]}}`;
        }
        return `\\constraint{${kind}}{${args.map(a => a.name || a).join('}{')}}`;
    }

    // 方程
    if (ast.type === 'equation') {
        return ast.raw || ast.expr || '';
    }

    // 兜底
    return String(ast.raw || ast.expr || '');
}

// ============================================================================
//  便捷方法
// ============================================================================

/**
 * 从 MathLive mathfield 获取当前公式并转换为 Lv-00 格式
 *
 * 便捷方法：组合 getValue('math-json') 和 mathlive_to_lv00_format()。
 *
 * @param {Object}  mathfield   - MathLive mathfield 实例
 * @param {Object}  [options={}] - 传递给 mathlive_to_lv00_format 的选项
 * @returns {MathLiveIntegrationResult} 转换结果
 */
export function getLv00FormatFromMathField(mathfield, options) {
    if (!mathfield) {
        return { success: false, nodes: [], errors: ['mathfield 为空'], warnings: [] };
    }
    const json = mathfield.getValue('math-json');
    return mathlive_to_lv00_format(json, options);
}

/**
 * 将 Lv-00 格式内容设置到 MathLive mathfield 中
 *
 * 便捷方法：组合 lv00_to_mathlive_format() 和 setValue()。
 *
 * @param {Object} mathfield     - MathLive mathfield 实例
 * @param {*}      lv00Content   - Lv-00 公式数据
 * @param {Object} [options={}]  - 传递给 lv00_to_mathlive_format 的选项
 */
export function setLv00FormatToMathField(mathfield, lv00Content, options) {
    if (!mathfield) return;
    const latex = lv00_to_mathlive_format(lv00Content, options);
    mathfield.setValue(latex);
}

/**
 * 将 MathLive mathfield 的 LaTeX 内容同步到 Lv-00 公式输入框
 *
 * @param {Object}  mathfield       - MathLive mathfield 实例
 * @param {string}  textareaId      - Lv-00 公式输入 textarea 的 DOM ID
 * @param {Object}  [options={}]    - 选项
 * @param {boolean} [options.triggerInput] - 是否触发 input 事件（默认 true）
 */
export function syncMathFieldToTextarea(mathfield, textareaId, options) {
    const opts = options || {};
    const triggerInput = opts.triggerInput !== false;

    const textarea = document.getElementById(textareaId);
    if (!textarea) {
        console.warn('[mathlive_integration] 公式输入框未找到: #' + textareaId);
        return;
    }

    const latex = mathfield ? mathfield.$latex() : '';
    textarea.value = latex;

    if (triggerInput) {
        textarea.dispatchEvent(new Event('input', { bubbles: true }));
    }
}

/**
 * MathLive 集成模块初始化入口
 *
 * 执行以下初始化步骤：
 *   1. 加载 MathLive 核心库
 *   2. 注册 Lv-00 几何专用 LaTeX 宏
 *   3. 根据配置创建 mathfield 实例
 *
 * @param {MathLiveConfig} config - 配置选项
 * @returns {Promise<Object>} { mathfield, latex, loaded }
 *
 * @example
 * const { mathfield } = await initMathLiveIntegration({
 *     containerId: 'formula-input',
 *     initialValue: '\\point{A}{0}{0}',
 *     virtualKeyboard: true,
 *     onContentChange: (latex) => {
 *         console.log('当前 LaTeX:', latex);
 *     }
 * });
 */
export async function initMathLiveIntegration(config) {
    const cfg = config || {};

    // 步骤 1：加载 MathLive
    await loadMathLive(cfg.cdnUrl);

    // 步骤 2：注册 Lv-00 几何宏
    registerLv00Macros(cfg.extraMacros);

    // 步骤 3：创建 mathfield
    const container = cfg.containerId || cfg.container;
    if (container) {
        const mf = createMathField(container, cfg);
        return {
            mathfield: mf,
            latex: mf.$latex(),
            loaded: true
        };
    }

    return {
        mathfield: null,
        latex: '',
        loaded: true
    };
}

// ============================================================================
//  默认导出：模块级公开 API
// ============================================================================

export default {
    // 初始化
    loadMathLive,
    createMathField,
    createStaticMath,
    initMathLiveIntegration,

    // 宏注册
    registerLv00Macros,
    LV00_GEOMETRY_MACROS,

    // 格式转换
    mathlive_to_lv00_format,
    lv00_to_mathlive_format,
    parseGeometryMacro,
    toGeometryLatex,

    // 便捷方法
    getLv00FormatFromMathField,
    setLv00FormatToMathField,
    syncMathFieldToTextarea
};
