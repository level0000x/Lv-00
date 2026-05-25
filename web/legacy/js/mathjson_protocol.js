/**
 * ============================================================================
 *  mathjson_protocol.js — Lv-00 前后端 MathJSON 通信协议
 * ============================================================================
 *
 * CortexJS MathJSON 是结构化的数学中间表示格式。原始 MathJSON 使用
 * S-expression 风格数组表达数学对象，例如：
 *   ["Add", "x", 2]  表示  x + 2
 *   ["Equal", ["Power", "x", 2], 9]  表示  x^2 = 9
 *
 * 本模块在标准 MathJSON 基础上定义 Lv-00 的几何扩展协议，使 Web 前端
 * 和 Python/Rust/C 后端可以通过统一的 MathJSON 格式交换几何构造命令。
 *
 * Lv-00 几何扩展函数（自定义 MathJSON 字典条目）：
 *
 *   ["Lv00Point", name, x, y]
 *       — 创建几何点 name 在坐标 (x, y)
 *
 *   ["Lv00Line", name, from, to]
 *       — 创建线段 name，连接点 from 和 to
 *
 *   ["Lv00Circle", name, center, radius]
 *       — 创建圆 name，圆心为 center，半径为 radius
 *
 *   ["Lv00Intersection", name, line1, line2]
 *       — 计算线段 line1 和 line2 的交点 name
 *
 *   ["Lv00Constraint", kind, ...participants]
 *       — 创建几何约束
 *         kind: "incidence"|"betweenness"|"containment"|"connection"
 *
 *   ["Lv00Solve", target?]
 *       — 触发几何约束系统求解
 *
 *   ["Lv00Prove", statement]
 *       — 触发几何命题证明
 *
 * 支持的几何操作命令（高层封装）：
 *   add_point, add_line, add_circle, add_intersection,
 *   add_constraint, solve, prove
 *
 * 依赖：
 *   - 本模块为纯逻辑模块，无外部运行时依赖
 *   - 可与 mathlive_integration.js 配合使用：
 *       前端 MathLive MathJSON -> mathlive_to_lv00_format() ->
 *       lv00_command_to_mathjson() -> 发送到后端
 *
 * 格式说明：
 *   - 本文件使用 ES module 格式（export/import）
 *
 * 版本：1.0.0
 * 作者：Lv-00 Team
 * 创建日期：2026-05-24
 * ============================================================================
 *
 * @module mathjson_protocol
 */

// ============================================================================
//  类型定义（JSDoc，非运行时）
// ============================================================================

/**
 * @typedef {Object} Lv00Command
 * @property {string}          command  - 命令名称
 *                                        'add_point'|'add_line'|'add_circle'|
 *                                        'add_intersection'|'add_constraint'|
 *                                        'solve'|'prove'
 * @property {Object}          [params] - 命令参数字典
 * @property {string}          [params.name]       - 几何元素名称
 * @property {number}          [params.x]           - X 坐标（点）
 * @property {number}          [params.y]           - Y 坐标（点）
 * @property {string}          [params.from]        - 起点标签（线段）
 * @property {string}          [params.to]          - 终点标签（线段）
 * @property {string}          [params.center]      - 圆心标签（圆）
 * @property {number}          [params.radius]      - 半径（圆）
 * @property {string}          [params.line1]        - 线段 1 标签（交点）
 * @property {string}          [params.line2]        - 线段 2 标签（交点）
 * @property {string}          [params.constraintType] - 约束类型
 * @property {Array<string>}   [params.participants]   - 约束参与者标签列表
 * @property {string}          [params.target]          - 求解目标（solve）
 * @property {string}          [params.statement]       - 证明命题（prove）
 * @property {string}          [params.id]             - 命令唯一 ID（可选）
 */

/**
 * @typedef {Object} MathJSONEnvelope
 * @property {string}          protocol   - 协议标识 'lv00-mathjson/1.0'
 * @property {string}          [id]       - 消息唯一 ID
 * @property {number}          timestamp  - Unix 时间戳（毫秒）
 * @property {string}          type       - 消息类型 'command'|'response'|'event'
 * @property {Array|Object}    payload    - MathJSON 负载
 * @property {Object}          [meta]     - 元数据
 */

/**
 * @typedef {Object} ProtocolDecodeResult
 * @property {boolean}         success    - 解码是否成功
 * @property {Lv00Command|null} command   - 解码出的 Lv-00 命令
 * @property {Array<string>}   errors     - 错误消息列表
 * @property {Array<string>}   warnings   - 警告消息列表
 */

// ============================================================================
//  常量
// ============================================================================

/**
 * 协议版本号
 * @type {string}
 */
const PROTOCOL_VERSION = 'lv00-mathjson/1.0';

/**
 * 有效的几何命令名称集合
 * @type {Set<string>}
 */
const VALID_COMMANDS = new Set([
    'add_point',
    'add_line',
    'add_circle',
    'add_intersection',
    'add_constraint',
    'solve',
    'prove'
]);

/**
 * 有效的几何约束类型集合
 * 与 lv00_js_backend.js 中 Constraint.type 枚举对应：
 *   CT_INCIDENCE     = 0  (点在线段上)
 *   CT_BETWEENNESS   = 1  (介于关系)
 *   CT_INTERSECTION  = 2  (交点)
 *   CT_CONTAINMENT   = 3  (包含关系)
 *   CT_CONNECTION    = 4  (连接关系)
 * @type {Set<string>}
 */
const VALID_CONSTRAINT_TYPES = new Set([
    'incidence',
    'betweenness',
    'intersection',
    'containment',
    'connection'
]);

// ============================================================================
//  MathJSON 字典注册：Lv-00 几何专用函数
// ============================================================================

/**
 * Lv-00 几何扩展函数的 MathJSON 字典描述
 *
 * 每个条目描述了函数名、参数签名和语义，可被 MathLive/CortexJS
 * 引擎用于验证和自动补全。
 *
 * @type {Object<string, {signature: string, domain: string, description: string}>}
 */
const LV00_MATHJSON_DICTIONARY = {
    'Lv00Point': {
        signature: 'Lv00Point(name: string, x: number, y: number)',
        domain: 'Geometry',
        description: '创建一个名为 name 的几何点，坐标为 (x, y)'
    },
    'Lv00Line': {
        signature: 'Lv00Line(name: string, from: string, to: string)',
        domain: 'Geometry',
        description: '创建一条名为 name 的线段，连接点 from 和点 to'
    },
    'Lv00Circle': {
        signature: 'Lv00Circle(name: string, center: string, radius: number)',
        domain: 'Geometry',
        description: '创建一个名为 name 的圆，圆心为 center，半径为 radius'
    },
    'Lv00Intersection': {
        signature: 'Lv00Intersection(name: string, line1: string, line2: string)',
        domain: 'Geometry',
        description: '计算线段 line1 和 line2 的交点，命名为 name'
    },
    'Lv00Constraint': {
        signature: 'Lv00Constraint(kind: string, ...participants: string)',
        domain: 'Geometry',
        description: '创建几何约束：kind 为类型（incidence/betweenness/intersection/containment/connection），participants 为参与元素的标签列表'
    },
    'Lv00Solve': {
        signature: 'Lv00Solve(target?: string)',
        domain: 'Geometry',
        description: '触发几何约束系统求解，可选指定求解目标'
    },
    'Lv00Prove': {
        signature: 'Lv00Prove(statement: expression)',
        domain: 'Geometry',
        description: '触发几何命题证明，statement 为待证命题的 MathJSON 表达式'
    }
};

// ============================================================================
//  内部辅助函数
// ============================================================================

/**
 * 生成唯一消息 ID
 *
 * 使用时间戳 + 随机数生成一个全局唯一的消息标识符，
 * 用于请求-响应匹配和日志追踪。
 *
 * @returns {string} 格式为 'lv00-{timestamp}-{random}' 的唯一 ID
 */
function generateMessageId() {
    const ts = Date.now().toString(36);
    const rand = Math.floor(Math.random() * 0xffffffff).toString(36);
    return `lv00-${ts}-${rand}`;
}

/**
 * 验证命令参数
 *
 * @param {Lv00Command} command - Lv-00 命令对象
 * @returns {{valid: boolean, errors: Array<string>}} 验证结果
 */
function validateCommand(command) {
    /** @type {Array<string>} */
    const errors = [];

    if (!command || typeof command !== 'object') {
        errors.push('命令对象为空或类型无效');
        return { valid: false, errors };
    }

    if (!command.command) {
        errors.push('缺少 command 字段');
        return { valid: false, errors };
    }

    if (!VALID_COMMANDS.has(command.command)) {
        errors.push(`未知的命令类型: '${command.command}'。有效类型: ${[...VALID_COMMANDS].join(', ')}`);
        return { valid: false, errors };
    }

    const params = command.params || {};

    switch (command.command) {
        case 'add_point':
            if (!params.name || typeof params.name !== 'string') {
                errors.push('add_point 需要字符串参数 name');
            }
            if (params.x === undefined || typeof params.x !== 'number') {
                errors.push('add_point 需要数字参数 x');
            }
            if (params.y === undefined || typeof params.y !== 'number') {
                errors.push('add_point 需要数字参数 y');
            }
            break;

        case 'add_line':
            if (!params.name || typeof params.name !== 'string') {
                errors.push('add_line 需要字符串参数 name');
            }
            if (!params.from || typeof params.from !== 'string') {
                errors.push('add_line 需要字符串参数 from');
            }
            if (!params.to || typeof params.to !== 'string') {
                errors.push('add_line 需要字符串参数 to');
            }
            break;

        case 'add_circle':
            if (!params.name || typeof params.name !== 'string') {
                errors.push('add_circle 需要字符串参数 name');
            }
            if (!params.center || typeof params.center !== 'string') {
                errors.push('add_circle 需要字符串参数 center');
            }
            if (params.radius === undefined || typeof params.radius !== 'number') {
                errors.push('add_circle 需要数字参数 radius');
            }
            break;

        case 'add_intersection':
            if (!params.name || typeof params.name !== 'string') {
                errors.push('add_intersection 需要字符串参数 name');
            }
            if (!params.line1 || typeof params.line1 !== 'string') {
                errors.push('add_intersection 需要字符串参数 line1');
            }
            if (!params.line2 || typeof params.line2 !== 'string') {
                errors.push('add_intersection 需要字符串参数 line2');
            }
            break;

        case 'add_constraint':
            if (!params.constraintType ||
                !VALID_CONSTRAINT_TYPES.has(params.constraintType)) {
                errors.push(`add_constraint 需要有效的 constraintType: ${[...VALID_CONSTRAINT_TYPES].join(', ')}`);
            }
            if (!params.participants || !Array.isArray(params.participants) ||
                params.participants.length === 0) {
                errors.push('add_constraint 需要非空的 participants 数组');
            }
            break;

        case 'solve':
            // solve 命令参数都是可选的
            break;

        case 'prove':
            if (!params.statement || typeof params.statement !== 'string') {
                errors.push('prove 需要字符串参数 statement');
            }
            break;
    }

    return { valid: errors.length === 0, errors };
}

// ============================================================================
//  命令 <-> MathJSON 编解码
// ============================================================================

/**
 * 将 Lv-00 几何构造命令编码为 MathJSON 格式
 *
 * 这是前端向后端发送命令的核心编码函数。每个 Lv-00 命令被转换为
 * 标准的 MathJSON 函数调用表达式。
 *
 * 编码映射：
 *   add_point       -> ["Lv00Point", name, x, y]
 *   add_line        -> ["Lv00Line", name, from, to]
 *   add_circle      -> ["Lv00Circle", name, center, radius]
 *   add_intersection -> ["Lv00Intersection", name, line1, line2]
 *   add_constraint  -> ["Lv00Constraint", kind, ...participants]
 *   solve           -> ["Lv00Solve", target?]
 *   prove           -> ["Lv00Prove", statement]
 *
 * @param {Lv00Command} command    - Lv-00 几何构造命令
 * @param {Object}      [options={}] - 编码选项
 * @param {boolean}     [options.wrapInEnvelope] - 是否包装为 MathJSONEnvelope
 * @param {string}      [options.id]             - 强制指定消息 ID（默认自动生成）
 * @returns {Array|MathJSONEnvelope} MathJSON 编码结果
 *         未包装时返回纯 MathJSON 数组；包装时返回 MathJSONEnvelope 对象
 *
 * @example
 * // 编码一个添加点的命令
 * const mathjson = lv00_command_to_mathjson({
 *     command: 'add_point',
 *     params: { name: 'A', x: 0, y: 0 }
 * });
 * // => ["Lv00Point", "A", 0, 0]
 *
 * @example
 * // 包装为协议信封发送到后端
 * const envelope = lv00_command_to_mathjson({
 *     command: 'add_circle',
 *     params: { name: 'O', center: 'A', radius: 3 }
 * }, { wrapInEnvelope: true });
 * // => { protocol: 'lv00-mathjson/1.0', type: 'command', ... }
 */
export function lv00_command_to_mathjson(command, options) {
    const opts = options || {};
    const params = (command && command.params) || {};
    let mathjson = null;

    switch (command.command) {
        case 'add_point':
            mathjson = [
                'Lv00Point',
                params.name || 'P',
                params.x !== undefined ? params.x : 0,
                params.y !== undefined ? params.y : 0
            ];
            break;

        case 'add_line':
            mathjson = [
                'Lv00Line',
                params.name || 'l',
                params.from || 'A',
                params.to || 'B'
            ];
            break;

        case 'add_circle':
            mathjson = [
                'Lv00Circle',
                params.name || 'C',
                params.center || 'O',
                params.radius !== undefined ? params.radius : 1
            ];
            break;

        case 'add_intersection':
            mathjson = [
                'Lv00Intersection',
                params.name || 'X',
                params.line1 || 'l1',
                params.line2 || 'l2'
            ];
            break;

        case 'add_constraint':
            mathjson = [
                'Lv00Constraint',
                params.constraintType || 'incidence'
            ];
            if (params.participants && Array.isArray(params.participants)) {
                for (const p of params.participants) {
                    mathjson.push(p);
                }
            }
            break;

        case 'solve':
            mathjson = ['Lv00Solve'];
            if (params.target) {
                mathjson.push(params.target);
            }
            break;

        case 'prove':
            mathjson = [
                'Lv00Prove',
                params.statement || ''
            ];
            break;

        default:
            throw new Error(`未知命令类型: ${command.command}`);
    }

    // 根据选项决定是否包装为协议信封
    if (opts.wrapInEnvelope) {
        return {
            protocol: PROTOCOL_VERSION,
            id: opts.id || generateMessageId(),
            timestamp: Date.now(),
            type: 'command',
            payload: mathjson,
            meta: {
                command: command.command,
                client: 'lv00-web-gui'
            }
        };
    }

    return mathjson;
}

/**
 * 将 MathJSON 解码为 Lv-00 几何构造命令
 *
 * 这是后端响应或前端接收消息时的核心解码函数。
 * 支持两种输入格式：
 *   1. MathJSONEnvelope 对象（含 protocol / type / payload 字段）
 *   2. 纯 MathJSON 数组
 *
 * @param {Array|MathJSONEnvelope|string} json - MathJSON 数据
 *        可以是 MathJSON 数组、MathJSONEnvelope 对象或 JSON 字符串
 * @param {Object} [options={}] - 解码选项
 * @param {boolean}[options.strict] - 严格模式：拒绝未知命令（默认 false）
 * @returns {ProtocolDecodeResult} 解码结果
 *
 * @example
 * // 解码纯 MathJSON 数组
 * const result = mathjson_to_lv00_command(["Lv00Point", "A", 0, 0]);
 * // => { success: true, command: { command: 'add_point', params: { name: 'A', x: 0, y: 0 } }, ... }
 *
 * @example
 * // 解码协议信封
 * const result = mathjson_to_lv00_command({
 *     protocol: 'lv00-mathjson/1.0',
 *     type: 'command',
 *     payload: ["Lv00Line", "AB", "A", "B"]
 * });
 * // => { success: true, command: { command: 'add_line', params: { name: 'AB', from: 'A', to: 'B' } }, ... }
 *
 * @example
 * // 解码 JSON 字符串
 * const result = mathjson_to_lv00_command('["Lv00Circle","O","A",3]');
 */
export function mathjson_to_lv00_command(json, options) {
    const opts = options || {};
    /** @type {Array<string>} */
    const errors = [];
    /** @type {Array<string>} */
    const warnings = [];

    if (!json) {
        errors.push('输入为空');
        return { success: false, command: null, errors, warnings };
    }

    // 步骤 1：解析输入格式
    let payload;

    // 字符串：尝试 JSON 解析
    if (typeof json === 'string') {
        try {
            payload = JSON.parse(json);
        } catch (e) {
            errors.push('无法解析 JSON 字符串: ' + e.message);
            return { success: false, command: null, errors, warnings };
        }
    } else {
        payload = json;
    }

    // 步骤 2：解包协议信封
    if (payload && typeof payload === 'object' && !Array.isArray(payload)) {
        if (payload.protocol) {
            // 检查协议版本
            if (!payload.protocol.startsWith('lv00-mathjson')) {
                warnings.push('协议版本可能不兼容: ' + payload.protocol);
            }
        }

        if (payload.type && payload.type !== 'command') {
            // 非命令类型（response/event），尝试提取 payload
            if (payload.type === 'response') {
                warnings.push('收到 response 类型消息，非 command');
            }
        }

        if (Array.isArray(payload.payload)) {
            payload = payload.payload;
        } else if (payload.mathjson) {
            payload = payload.mathjson;
        } else if (payload.command) {
            // 可能是 Lv00Command 对象直接传入
            return {
                success: true,
                command: payload,
                errors: errors,
                warnings: warnings
            };
        } else {
            errors.push('无法从对象中提取 MathJSON payload');
            return { success: false, command: null, errors, warnings };
        }
    }

    // 步骤 3：验证 MathJSON 数组格式
    if (!Array.isArray(payload) || payload.length === 0) {
        errors.push('MathJSON 格式无效：应为非空数组');
        return { success: false, command: null, errors, warnings };
    }

    // 步骤 4：解码 MathJSON 函数调用
    const head = payload[0];
    const args = payload.slice(1);

    /** @type {Lv00Command} */
    let command = null;

    switch (head) {
        case 'Lv00Point': {
            if (args.length < 3) {
                errors.push('Lv00Point 参数不足：需要 (name, x, y)，实际 ' + args.length);
                break;
            }
            command = {
                command: 'add_point',
                params: {
                    name: String(args[0]),
                    x: typeof args[1] === 'number' ? args[1] : parseFloat(args[1]) || 0,
                    y: typeof args[2] === 'number' ? args[2] : parseFloat(args[2]) || 0
                }
            };
            break;
        }

        case 'Lv00Line': {
            if (args.length < 3) {
                errors.push('Lv00Line 参数不足：需要 (name, from, to)，实际 ' + args.length);
                break;
            }
            command = {
                command: 'add_line',
                params: {
                    name: String(args[0]),
                    from: String(args[1]),
                    to: String(args[2])
                }
            };
            break;
        }

        case 'Lv00Circle': {
            if (args.length < 3) {
                errors.push('Lv00Circle 参数不足：需要 (name, center, radius)，实际 ' + args.length);
                break;
            }
            command = {
                command: 'add_circle',
                params: {
                    name: String(args[0]),
                    center: String(args[1]),
                    radius: typeof args[2] === 'number' ? args[2] : parseFloat(args[2]) || 1
                }
            };
            break;
        }

        case 'Lv00Intersection': {
            if (args.length < 3) {
                errors.push('Lv00Intersection 参数不足：需要 (name, line1, line2)，实际 ' + args.length);
                break;
            }
            command = {
                command: 'add_intersection',
                params: {
                    name: String(args[0]),
                    line1: String(args[1]),
                    line2: String(args[2])
                }
            };
            break;
        }

        case 'Lv00Constraint': {
            if (args.length < 2) {
                errors.push('Lv00Constraint 参数不足：需要 (kind, ...participants)，实际 ' + args.length);
                break;
            }
            const constraintType = String(args[0]);
            if (!VALID_CONSTRAINT_TYPES.has(constraintType)) {
                errors.push(`未知约束类型 '${constraintType}'。有效值: ${[...VALID_CONSTRAINT_TYPES].join(', ')}`);
                break;
            }
            const participants = args.slice(1).map(String);
            command = {
                command: 'add_constraint',
                params: {
                    constraintType: constraintType,
                    participants: participants
                }
            };
            break;
        }

        case 'Lv00Solve': {
            command = {
                command: 'solve',
                params: {}
            };
            if (args.length > 0) {
                command.params.target = String(args[0]);
            }
            break;
        }

        case 'Lv00Prove': {
            command = {
                command: 'prove',
                params: {
                    statement: args.length > 0 ? String(args[0]) : ''
                }
            };
            break;
        }

        default: {
            const msg = `未知的 Lv-00 MathJSON 函数: '${head}'`;
            if (opts.strict) {
                errors.push(msg);
            } else {
                warnings.push(msg + '（非严格模式下跳过）');
            }
            break;
        }
    }

    return {
        success: errors.length === 0 && command !== null,
        command: command,
        errors: errors,
        warnings: warnings
    };
}

// ============================================================================
//  命令批量编解码
// ============================================================================

/**
 * 将多个 Lv-00 命令批量编码为 MathJSON Sequence
 *
 * 使用 MathJSON 的 Sequence 容器将多条命令打包为单条消息，
 * 方便一次性发送给后端。
 *
 * @param {Array<Lv00Command>} commands  - Lv-00 命令列表
 * @param {Object}            [options={}] - 编码选项（同 lv00_command_to_mathjson）
 * @returns {Array|MathJSONEnvelope} MathJSON Sequence
 *
 * @example
 * const seq = lv00_commands_to_mathjson([
 *     { command: 'add_point', params: { name: 'A', x: 0, y: 0 } },
 *     { command: 'add_point', params: { name: 'B', x: 3, y: 4 } },
 *     { command: 'add_line',  params: { name: 'AB', from: 'A', to: 'B' } }
 * ]);
 * // => ["Sequence", ["Lv00Point","A",0,0], ["Lv00Point","B",3,4], ["Lv00Line","AB","A","B"]]
 */
export function lv00_commands_to_mathjson(commands, options) {
    if (!Array.isArray(commands) || commands.length === 0) {
        return [];
    }

    const encoded = commands.map(cmd => lv00_command_to_mathjson(cmd, { wrapInEnvelope: false }));

    const opts = options || {};
    if (opts.wrapInEnvelope) {
        return {
            protocol: PROTOCOL_VERSION,
            id: opts.id || generateMessageId(),
            timestamp: Date.now(),
            type: 'command',
            payload: ['Sequence'].concat(encoded),
            meta: {
                command: 'batch',
                count: commands.length,
                client: 'lv00-web-gui'
            }
        };
    }

    return ['Sequence'].concat(encoded);
}

/**
 * 从 MathJSON Sequence 中批量解码 Lv-00 命令
 *
 * 与 lv00_commands_to_mathjson 配对使用，解析 Sequence 容器中
 * 的多条命令。
 *
 * @param {Array|MathJSONEnvelope|string} json - MathJSON 数据
 * @param {Object} [options={}] - 解码选项（同 mathjson_to_lv00_command）
 * @returns {Array<ProtocolDecodeResult>} 每条命令的解码结果列表
 *
 * @example
 * const results = mathjson_to_lv00_commands(sequenceJSON);
 * for (const r of results) {
 *     if (r.success) executeCommand(r.command);
 * }
 */
export function mathjson_to_lv00_commands(json, options) {
    /** @type {Array<ProtocolDecodeResult>} */
    const results = [];

    if (!json) return results;

    // 步骤 1: 解析和解包
    let payload;
    if (typeof json === 'string') {
        try {
            payload = JSON.parse(json);
        } catch (e) {
            results.push({
                success: false, command: null,
                errors: ['JSON 解析失败: ' + e.message], warnings: []
            });
            return results;
        }
    } else {
        payload = json;
    }

    // 解包信封
    if (payload && typeof payload === 'object' && !Array.isArray(payload)) {
        payload = payload.payload || payload.mathjson || payload;
    }

    // 步骤 2: 检查是否为 Sequence
    if (!Array.isArray(payload)) {
        results.push({
            success: false, command: null,
            errors: ['输入不是有效的 MathJSON 数组'], warnings: []
        });
        return results;
    }

    if (payload[0] === 'Sequence' || payload[0] === 'List' || payload[0] === 'Tuple') {
        const items = payload.slice(1);
        for (const item of items) {
            if (Array.isArray(item)) {
                results.push(mathjson_to_lv00_command(item, options));
            }
        }
    } else {
        // 不是容器，当作单条命令
        results.push(mathjson_to_lv00_command(payload, options));
    }

    return results;
}

// ============================================================================
//  协议信封工具
// ============================================================================

/**
 * 封包：将 MathJSON 负载包装为协议信封
 *
 * @param {Array|Object} payload  - MathJSON 负载
 * @param {string}       type     - 消息类型 'command'|'response'|'event'
 * @param {Object}       [meta={}] - 元数据
 * @param {string}       [id]      - 强制指定消息 ID
 * @returns {MathJSONEnvelope} 协议信封对象
 *
 * @example
 * const envelope = wrapEnvelope(
 *     ["Lv00Point", "A", 0, 0],
 *     'command',
 *     { client: 'lv00-web-gui' }
 * );
 */
export function wrapEnvelope(payload, type, meta, id) {
    return {
        protocol: PROTOCOL_VERSION,
        id: id || generateMessageId(),
        timestamp: Date.now(),
        type: type || 'command',
        payload: payload,
        meta: Object.assign({}, meta || {})
    };
}

/**
 * 解包：从协议信封中提取 MathJSON 负载
 *
 * @param {MathJSONEnvelope} envelope - 协议信封
 * @returns {{payload: Array|Object, type: string, id: string, meta: Object}|null}
 *          解包后的内容，格式无效返回 null
 */
export function unwrapEnvelope(envelope) {
    if (!envelope || typeof envelope !== 'object') return null;
    if (!envelope.protocol || !envelope.payload) return null;

    return {
        payload: envelope.payload,
        type: envelope.type || 'unknown',
        id: envelope.id || '',
        meta: envelope.meta || {}
    };
}

// ============================================================================
//  响应构建
// ============================================================================

/**
 * 构建成功响应信封
 *
 * @param {Array|Object} result   - 执行结果（MathJSON 格式）
 * @param {string}       [replyTo] - 回应哪条消息的 ID
 * @param {Object}       [meta={}] - 附加元数据
 * @returns {MathJSONEnvelope} 响应信封
 *
 * @example
 * const response = buildSuccessResponse(
 *     ["Lv00Point", "A", 3, 4],
 *     requestEnvelope.id
 * );
 */
export function buildSuccessResponse(result, replyTo, meta) {
    return {
        protocol: PROTOCOL_VERSION,
        id: generateMessageId(),
        timestamp: Date.now(),
        type: 'response',
        payload: result,
        meta: Object.assign({ status: 'success' }, meta || {}, replyTo ? { replyTo } : {})
    };
}

/**
 * 构建错误响应信封
 *
 * @param {string}       errorMessage - 错误消息
 * @param {string}       [code]       - 错误码
 * @param {string}       [replyTo]    - 回应哪条消息的 ID
 * @param {Object}       [meta={}]    - 附加元数据
 * @returns {MathJSONEnvelope} 响应信封
 *
 * @example
 * const response = buildErrorResponse(
 *     '命令执行失败：点 B 未定义',
 *     'REFERENCE_ERROR',
 *     requestEnvelope.id
 * );
 */
export function buildErrorResponse(errorMessage, code, replyTo, meta) {
    return {
        protocol: PROTOCOL_VERSION,
        id: generateMessageId(),
        timestamp: Date.now(),
        type: 'response',
        payload: {
            error: errorMessage,
            code: code || 'UNKNOWN_ERROR'
        },
        meta: Object.assign({ status: 'error' }, meta || {}, replyTo ? { replyTo } : {})
    };
}

// ============================================================================
//  WebSocket / HTTP 传输适配器
// ============================================================================

/**
 * 通过 HTTP POST 发送 MathJSON 命令到后端
 *
 * @param {string}       endpoint  - 后端 API 端点 URL
 * @param {Lv00Command|Array<Lv00Command>} command - 单条或批量命令
 * @param {Object}       [options={}] - 请求选项
 * @param {AbortSignal}  [options.signal]     - fetch AbortSignal
 * @param {number}       [options.timeoutMs]  - 超时毫秒数
 * @returns {Promise<MathJSONEnvelope>} 后端响应信封
 *
 * @example
 * const response = await sendMathJSONCommand('/api/lv00/execute', {
 *     command: 'add_point',
 *     params: { name: 'A', x: 0, y: 0 }
 * });
 */
export async function sendMathJSONCommand(endpoint, command, options) {
    const opts = options || {};

    // 编码命令为协议信封
    let envelope;
    if (Array.isArray(command)) {
        envelope = lv00_commands_to_mathjson(command, { wrapInEnvelope: true });
    } else {
        envelope = lv00_command_to_mathjson(command, { wrapInEnvelope: true });
    }

    // 构建 fetch 请求
    const fetchOptions = {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
            'Accept': 'application/json'
        },
        body: JSON.stringify(envelope),
        signal: opts.signal || undefined
    };

    // 超时处理
    let timeoutId;
    if (opts.timeoutMs && opts.timeoutMs > 0) {
        const controller = new AbortController();
        if (!opts.signal) {
            fetchOptions.signal = controller.signal;
        }
        timeoutId = setTimeout(() => controller.abort(), opts.timeoutMs);
    }

    try {
        const response = await fetch(endpoint, fetchOptions);

        if (!response.ok) {
            throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();
        return data;
    } finally {
        if (timeoutId) clearTimeout(timeoutId);
    }
}

/**
 * 通过 WebSocket 发送 MathJSON 命令
 *
 * @param {WebSocket}    ws       - 已连接的 WebSocket 实例
 * @param {Lv00Command|Array<Lv00Command>} command - 单条或批量命令
 * @returns {string} 已发送消息的 ID（用于匹配响应）
 *
 * @example
 * const ws = new WebSocket('ws://localhost:8080/lv00');
 * ws.onopen = () => {
 *     sendMathJSONCommandWS(ws, {
 *         command: 'add_point',
 *         params: { name: 'A', x: 0, y: 0 }
 *     });
 * };
 */
export function sendMathJSONCommandWS(ws, command) {
    if (!ws || ws.readyState !== WebSocket.OPEN) {
        throw new Error('WebSocket 未连接或未就绪');
    }

    let envelope;
    if (Array.isArray(command)) {
        envelope = lv00_commands_to_mathjson(command, { wrapInEnvelope: true });
    } else {
        envelope = lv00_command_to_mathjson(command, { wrapInEnvelope: true });
    }

    const message = JSON.stringify(envelope);
    ws.send(message);
    return envelope.id;
}

// ============================================================================
//  使用示例（文档内嵌，非运行时）
// ============================================================================

/**
 * =========================================================================
 *  使用示例 / USAGE EXAMPLES
 * =========================================================================
 *
 * 以下示例展示本模块的主要使用方式。
 *
 * -----------------------------------------------------------------------
 *  示例 1：前端构造几何命令并编码
 * -----------------------------------------------------------------------
 *
 *   import { lv00_command_to_mathjson, sendMathJSONCommand } from './mathjson_protocol.js';
 *
 *   // 构建一条添加点的命令
 *   const cmd = {
 *       command: 'add_point',
 *       params: { name: 'A', x: 0, y: 0 }
 *   };
 *
 *   // 编码为 MathJSON
 *   const mathjson = lv00_command_to_mathjson(cmd);
 *   // => ["Lv00Point", "A", 0, 0]
 *
 *   // 发送到后端
 *   const response = await sendMathJSONCommand('/api/lv00/execute', cmd);
 *   console.log('后端响应:', response);
 *
 * -----------------------------------------------------------------------
 *  示例 2：批量命令（三角形构造）
 * -----------------------------------------------------------------------
 *
 *   import { lv00_commands_to_mathjson } from './mathjson_protocol.js';
 *
 *   const triangleCommands = [
 *       { command: 'add_point', params: { name: 'A', x: 0, y: 0 } },
 *       { command: 'add_point', params: { name: 'B', x: 3, y: 0 } },
 *       { command: 'add_point', params: { name: 'C', x: 1.5, y: 2.598 } },
 *       { command: 'add_line',  params: { name: 'AB', from: 'A', to: 'B' } },
 *       { command: 'add_line',  params: { name: 'BC', from: 'B', to: 'C' } },
 *       { command: 'add_line',  params: { name: 'CA', from: 'C', to: 'A' } }
 *   ];
 *
 *   const seq = lv00_commands_to_mathjson(triangleCommands);
 *   // => ["Sequence", ["Lv00Point","A",0,0], ["Lv00Point","B",3,0], ...]
 *
 * -----------------------------------------------------------------------
 *  示例 3：解码后端响应的 MathJSON
 * -----------------------------------------------------------------------
 *
 *   import { mathjson_to_lv00_command } from './mathjson_protocol.js';
 *
 *   // 假设从 WebSocket 收到后端消息
 *   const serverMessage = {
 *       protocol: 'lv00-mathjson/1.0',
 *       type: 'response',
 *       payload: ["Lv00Point", "M", 1.5, 1.299]
 *   };
 *
 *   const result = mathjson_to_lv00_command(serverMessage);
 *   if (result.success) {
 *       console.log('解码命令:', result.command);
 *       // => { command: 'add_point', params: { name: 'M', x: 1.5, y: 1.299 } }
 *   }
 *
 * -----------------------------------------------------------------------
 *  示例 4：添加几何约束
 * -----------------------------------------------------------------------
 *
 *   import { lv00_command_to_mathjson } from './mathjson_protocol.js';
 *
 *   // "点 P2 在线段 l1 上" 的关联约束
 *   const incCmd = {
 *       command: 'add_constraint',
 *       params: {
 *           constraintType: 'incidence',
 *           participants: ['P2', 'l1']
 *       }
 *   };
 *   const incJSON = lv00_command_to_mathjson(incCmd);
 *   // => ["Lv00Constraint", "incidence", "P2", "l1"]
 *
 *   // "点 P1 在 P0 和 P2 之间" 的介于约束
 *   const btwnCmd = {
 *       command: 'add_constraint',
 *       params: {
 *           constraintType: 'betweenness',
 *           participants: ['P0', 'P1', 'P2']
 *       }
 *   };
 *   const btwnJSON = lv00_command_to_mathjson(btwnCmd);
 *   // => ["Lv00Constraint", "betweenness", "P0", "P1", "P2"]
 *
 * -----------------------------------------------------------------------
 *  示例 5：结合 MathLive 集成模块使用
 * -----------------------------------------------------------------------
 *
 *   import { mathlive_to_lv00_format } from './mathlive_integration.js';
 *   import { lv00_commands_to_mathjson, sendMathJSONCommand } from './mathjson_protocol.js';
 *
 *   // 从 MathLive mathfield 获取 MathJSON
 *   const mathliveJSON = mathfield.getValue('math-json');
 *
 *   // 转换为 Lv-00 几何节点
 *   const integrateResult = mathlive_to_lv00_format(mathliveJSON);
 *
 *   // 将几何节点转为后端命令列表
 *   const commands = integrateResult.nodes.map(node => {
 *       if (node.type === 'point') {
 *           return { command: 'add_point', params: { name: node.name, x: node.coords[0], y: node.coords[1] } };
 *       }
 *       if (node.type === 'line') {
 *           return { command: 'add_line', params: { name: node.name, from: node.labelA, to: node.labelB } };
 *       }
 *       if (node.type === 'circle') {
 *           return { command: 'add_circle', params: { name: node.name, center: node.centerName, radius: node.radius } };
 *       }
 *       return null;
 *   }).filter(Boolean);
 *
 *   // 批量发送
 *   const envelope = lv00_commands_to_mathjson(commands, { wrapInEnvelope: true });
 *   const response = await sendMathJSONCommand('/api/lv00/execute', commands);
 *
 * -----------------------------------------------------------------------
 *  示例 6：协议信封的编解码
 * -----------------------------------------------------------------------
 *
 *   import { wrapEnvelope, unwrapEnvelope } from './mathjson_protocol.js';
 *
 *   // 创建请求信封
 *   const request = wrapEnvelope(
 *       ["Lv00Solve", "triangle"],
 *       'command',
 *       { priority: 'high' }
 *   );
 *
 *   // 创建成功响应
 *   const response = buildSuccessResponse(
 *       { solved: true, nodes: [...] },
 *       request.id
 *   );
 *
 *   // 解包响应
 *   const { payload, type } = unwrapEnvelope(response);
 *   console.log(`类型: ${type}, 负载:`, payload);
 */

// ============================================================================
//  命名导出
// ============================================================================

export {
    // 协议常量
    PROTOCOL_VERSION,
    VALID_COMMANDS,
    VALID_CONSTRAINT_TYPES,
    LV00_MATHJSON_DICTIONARY,

    // 核心编解码
    lv00_command_to_mathjson,
    mathjson_to_lv00_command,

    // 批量编解码
    lv00_commands_to_mathjson,
    mathjson_to_lv00_commands,

    // 信封工具
    wrapEnvelope,
    unwrapEnvelope,

    // 响应构建
    buildSuccessResponse,
    buildErrorResponse,

    // 传输适配器
    sendMathJSONCommand,
    sendMathJSONCommandWS,

    // 工具函数
    generateMessageId,
    validateCommand
};

// ============================================================================
//  默认导出
// ============================================================================

export default {
    // 协议常量
    PROTOCOL_VERSION,
    VALID_COMMANDS,
    VALID_CONSTRAINT_TYPES,
    LV00_MATHJSON_DICTIONARY,

    // 核心编解码
    lv00_command_to_mathjson,
    mathjson_to_lv00_command,

    // 批量编解码
    lv00_commands_to_mathjson,
    mathjson_to_lv00_commands,

    // 信封工具
    wrapEnvelope,
    unwrapEnvelope,

    // 响应构建
    buildSuccessResponse,
    buildErrorResponse,

    // 传输适配器
    sendMathJSONCommand,
    sendMathJSONCommandWS,

    // 工具
    generateMessageId,
    validateCommand
};
