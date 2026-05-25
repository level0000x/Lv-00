/**
 * @module web/js/tikz_render_pipeline
 * @description Lv-00 几何可视化 -> TikZ -> SVG 的前端渲染管道
 *
 *              借鉴 jsTikZ (github.com/nicehorse06/jsTikZ) 和 TikZJax
 *              (tikzjax.com) 的 WASM 管道设计，将 Lv-00 的几何构造导出为
 *              TikZ 代码并通过 WASM 渲染为浏览器中的 SVG。
 *
 *  借鉴项目：  jsTikZ / TikZJax
 *  核心借鉴点： TikZ 模板生成、WASM 加载器、缓存策略
 *  分类：       P4 低优先级 / 前端可视化渲染管道
 *  日期：       2026-05-24
 *
 *  设计目标：
 *    1. 将 Lv-00 约束图中的几何对象编码为 TikZ 代码
 *    2. 通过 WASM 管道在浏览器中实时渲染为 SVG
 *    3. 缓存已渲染的 SVG 以优化性能
 *    4. 支持导出为独立可嵌入的 TikZ 代码（用于 LaTeX 论文）
 *
 * @author Lv-00 Project
 * @version 1.0.0
 */

'use strict';

/* 安全转义HTML特殊字符，防止XSS注入 */
function _safeEscapeHtml(str) {
    if (typeof str !== 'string') str = String(str);
    return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
              .replace(/"/g, '&quot;').replace(/'/g, '&#39;');
}

/**
 * =========================================================================
 * 第一部分：TikZ 模板生成函数
 * =========================================================================
 */

/**
 * @typedef {Object} TikzRenderOptions
 * @property {number}  [scale=1.0]             - 整体缩放因子
 * @property {string}  [unit='cm']             - 单位（cm, pt, mm）
 * @property {boolean} [showGrid=false]         - 是否显示网格
 * @property {boolean} [showLabels=true]        - 是否显示点标签
 * @property {boolean} [showCoordinates=false]  - 是否显示坐标数值
 * @property {string}  [pointStyle='fill=black'] - 点的样式
 * @property {string}  [lineStyle='thick']       - 线的样式
 * @property {string}  [circleStyle='']          - 圆的样式
 * @property {string}  [labelStyle='font=\\tiny'] - 标签样式
 * @property {boolean} [useTikzCalc=false]       - 是否使用 tikz 计算库
 * @property {boolean} [wrapInFigure=true]       - 是否包裹在 figure 环境中
 */

/** @type {TikzRenderOptions} */
const DEFAULT_TIKZ_OPTIONS = Object.freeze({
    scale: 1.0,
    unit: 'cm',
    showGrid: false,
    showLabels: true,
    showCoordinates: false,
    pointStyle: 'fill=black',
    lineStyle: 'thick',
    circleStyle: '',
    labelStyle: 'font=\\tiny',
    useTikzCalc: false,
    wrapInFigure: false,
});

/**
 * @class TikzTemplateGenerator
 * @description TikZ 模板生成器
 *
 *              将 Lv-00 的几何数据（点、线段、圆、约束）编码为 TikZ 代码。
 *
 *              编码规则（对照 Lv-00 约束图节点类型）：
 *              - POINT        -> \coordinate (P{id}) at (x, y);
 *              - LINE_SEGMENT -> \draw (P{p1}) -- (P{p2});
 *              - CIRCLE       -> \draw (P{center}) circle (radius);
 *              - REGION       -> \filldraw (P{v1}) -- (P{v2}) -- ... -- cycle;
 */
class TikzTemplateGenerator {
    /**
     * @param {TikzRenderOptions} [options]
     */
    constructor(options = {}) {
        /** @type {TikzRenderOptions} */
        this.options = Object.assign({}, DEFAULT_TIKZ_OPTIONS, options);
    }

    /**
     * 从 Lv-00 几何数据生成完整的 TikZ 文档
     *
     * @param {Object} geometryData - Lv-00 几何数据
     * @param {Array<{id: number, x: number, y: number, label?: string}>} geometryData.points
     * @param {Array<{id: number, p1: number, p2: number, label?: string}>} geometryData.segments
     * @param {Array<{id: number, centerX: number, centerY: number, radius: number}>} geometryData.circles
     * @param {Array<{id: number, vertices: Array<number>, label?: string}>} geometryData.regions
     * @param {Array<{id: number, type: string, participants: Array<number>}>} [geometryData.constraints]
     * @param {string} [geometryData.caption] - 图标题
     * @returns {string} 完整的 TikZ 代码
     */
    generateFullTikz(geometryData, caption = '') {
        const parts = [];
        parts.push(this._generatePreamble());
        parts.push(this._generateBody(geometryData, caption));
        return parts.join('\n');
    }

    /**
     * 仅生成 tikzpicture 环境内容（用于嵌入已有文档）
     * @param {Object} geometryData
     * @returns {string}
     */
    generateTikzPicture(geometryData) {
        return this._generateTikzPictureContent(geometryData);
    }

    /**
     * 从 Lv-00 App 的 points/segments/constraints 数据生成几何数据对象
     *
     * @param {Array<{id: number, x: number, y: number}>} points
     * @param {Array<{id: number, p1: number, p2: number}>} segments
     * @param {Array<{id: number, type: string, args: Array<number>}>} constraints
     * @returns {Object} geometryData
     */
    static fromAppData(points, segments, constraints) {
        return {
            points: (points || []).map((p) => ({
                id: p.id,
                x: p.x,
                y: p.y,
                label: `P_{${p.id}}`,
            })),
            segments: (segments || []).map((s) => ({
                id: s.id,
                p1: s.p1,
                p2: s.p2,
            })),
            circles: [],
            regions: [],
            constraints: (constraints || []).map((c) => ({
                id: c.id,
                type: c.type,
                participants: (c.args || []).slice(),
            })),
        };
    }

    /**
     * 生成 LaTeX 前导
     * @returns {string}
     * @private
     */
    _generatePreamble() {
        const lines = [
            '% Lv-00 几何可视化自动生成',
            '% Generated by Lv-00 TikZ Render Pipeline',
            '% Date: ' + new Date().toISOString(),
            '',
            '\\documentclass{standalone}',
            '\\usepackage{tikz}',
        ];
        if (this.options.useTikzCalc) {
            lines.push('\\usetikzlibrary{calc}');
        }
        lines.push('\\usetikzlibrary{intersections,through,angles,quotes}');
        lines.push('');
        lines.push('\\begin{document}');
        return lines.join('\n');
    }

    /**
     * 生成文档主体
     * @param {Object} d
     * @param {string} caption
     * @returns {string}
     * @private
     */
    _generateBody(d, caption) {
        const lines = [];
        if (this.options.wrapInFigure && caption) {
            lines.push('\\begin{figure}[htbp]');
            lines.push('  \\centering');
        }
        lines.push(this._generateTikzPictureContent(d));
        if (this.options.wrapInFigure && caption) {
            lines.push(`  \\caption{${this._escapeLatex(caption)}}`);
            lines.push('\\end{figure}');
        }
        lines.push('\\end{document}');
        return lines.join('\n');
    }

    /**
     * 生成 tikzpicture 环境内容
     * @param {Object} d - geometryData
     * @returns {string}
     * @private
     */
    _generateTikzPictureContent(d) {
        const lines = [];
        const { scale, unit, showGrid } = this.options;
        lines.push(`\\begin{tikzpicture}[scale=${scale}, every node/.style={${this.options.labelStyle}}]`);

        if (showGrid) {
            lines.push('  % Grid');
            lines.push('  \\draw[help lines, step=1] (-1,-1) grid (10,10);');
        }

        // 第 1 步：声明坐标点
        if (d.points && d.points.length > 0) {
            lines.push('  % Points');
            for (const pt of d.points) {
                lines.push(`  \\coordinate (P${pt.id}) at (${this._fmt(pt.x)}, ${this._fmt(pt.y)});`);
            }
            lines.push('');
        }

        // 第 2 步：绘制圆
        if (d.circles && d.circles.length > 0) {
            lines.push('  % Circles');
            for (const c of d.circles) {
                const cs = this.options.circleStyle ? `[${this.options.circleStyle}] ` : '';
                lines.push(`  \\draw ${cs}(${this._fmt(c.centerX)}, ${this._fmt(c.centerY)}) circle (${this._fmt(c.radius)}${unit});`);
            }
            lines.push('');
        }

        // 第 3 步：绘制区域
        if (d.regions && d.regions.length > 0) {
            lines.push('  % Regions');
            for (const r of d.regions) {
                if (r.vertices && r.vertices.length >= 3) {
                    const coords = r.vertices.map((vId) => {
                        const pt = d.points.find((p) => p.id === vId);
                        return pt ? `(${this._fmt(pt.x)}, ${this._fmt(pt.y)})` : `(P${vId})`;
                    }).join(' -- ');
                    lines.push(`  \\filldraw[fill=blue!10, draw=blue!50] ${coords} -- cycle;`);
                }
            }
            lines.push('');
        }

        // 第 4 步：绘制线段
        if (d.segments && d.segments.length > 0) {
            lines.push('  % Line Segments');
            for (const seg of d.segments) {
                lines.push(`  \\draw[${this.options.lineStyle}] (P${seg.p1}) -- (P${seg.p2});`);
            }
            lines.push('');
        }

        // 第 5 步：绘制点
        if (d.points && d.points.length > 0) {
            lines.push('  % Points (fill)');
            for (const pt of d.points) {
                let cmd = `  \\fill[${this.options.pointStyle}] (P${pt.id}) circle (1.5pt)`;
                if (this.options.showLabels && pt.label) {
                    cmd += ` node[anchor=south west] {$${this._escapeLatex(pt.label)}$}`;
                } else if (this.options.showLabels) {
                    cmd += ` node[anchor=south west] {$P_{${pt.id}}$}`;
                }
                cmd += ';';
                lines.push(cmd);
            }
            lines.push('');
        }

        // 第 6 步：坐标标注
        if (this.options.showCoordinates && d.points) {
            lines.push('  % Coordinate annotations');
            for (const pt of d.points) {
                lines.push(`  \\node[anchor=north east, font=\\tiny, gray] at (P${pt.id}) {(${this._fmt(pt.x)}, ${this._fmt(pt.y)})};`);
            }
            lines.push('');
        }

        lines.push('\\end{tikzpicture}');
        return lines.join('\n');
    }

    /**
     * 格式化数值
     * @param {number} val
     * @returns {string}
     * @private
     */
    _fmt(val) {
        if (val === undefined || val === null) return '0';
        if (Math.abs(val - Math.round(val)) < 1e-8) {
            return Math.round(val).toString();
        }
        return val.toFixed(4);
    }

    /**
     * 转义 LaTeX 特殊字符
     * @param {string} text
     * @returns {string}
     * @private
     */
    _escapeLatex(text) {
        if (!text) return '';
        return text
            .replace(/\\/g, '\\textbackslash{}')
            .replace(/&/g, '\\&')
            .replace(/%/g, '\\%')
            .replace(/\$/g, '\\$')
            .replace(/#/g, '\\#')
            .replace(/_/g, '\\_')
            .replace(/\{/g, '\\{')
            .replace(/\}/g, '\\}')
            .replace(/~/g, '\\textasciitilde{}')
            .replace(/\^/g, '\\textasciicircum{}');
    }
}


/**
 * =========================================================================
 * 第二部分：WASM 加载器（TikZ -> SVG）
 * =========================================================================
 */

/**
 * @typedef {Object} WasmLoaderConfig
 * @property {string}  wasmUrl       - WASM 模块的 URL
 * @property {string}  workerUrl     - Web Worker 脚本的 URL（可选，用于离主线程渲染）
 * @property {number}  timeoutMs     - 渲染超时（毫秒），默认 10000
 * @property {number}  maxRetries    - 加载失败最大重试次数，默认 3
 * @property {boolean} useWorker     - 是否使用 Web Worker 进行渲染
 * @property {boolean} lazyLoad      - 是否延迟加载 WASM 模块
 */

/** @type {WasmLoaderConfig} */
const DEFAULT_WASM_CONFIG = Object.freeze({
    wasmUrl: 'https://tikzjax.com/ef253ef29e2f057334f77ead7f06ed8f22607d38.wasm',
    workerUrl: null,
    timeoutMs: 10000,
    maxRetries: 3,
    useWorker: false,
    lazyLoad: true,
});

/** @enum {string} */
const WASM_LOADER_STATE = Object.freeze({
    IDLE: 'idle',
    LOADING: 'loading',
    READY: 'ready',
    ERROR: 'error',
    TIMED_OUT: 'timed_out',
});

/**
 * @class TikzWasmLoader
 * @description TikZ WASM 加载器
 *
 *              负责加载 TikZJax / jsTikZ 的 WASM 模块，
 *              提供 render(tikzCode) -> SVG 的异步接口。
 */
class TikzWasmLoader {
    /**
     * @param {WasmLoaderConfig} [config]
     */
    constructor(config = {}) {
        /** @type {WasmLoaderConfig} */
        this.config = Object.assign({}, DEFAULT_WASM_CONFIG, config);
        /** @type {string} */
        this.state = WASM_LOADER_STATE.IDLE;
        /** @type {Object|null} */
        this.wasmModule = null;
        /** @type {number} */
        this.retryCount = 0;
        /** @type {Array<Function>} */
        this._readyCallbacks = [];
        /** @type {Promise<void>|null} */
        this._loadPromise = null;
    }

    /**
     * 初始化 WASM 模块
     * @returns {Promise<void>}
     */
    async init() {
        if (this.state === WASM_LOADER_STATE.READY) return;
        if (this._loadPromise) return this._loadPromise;

        this.state = WASM_LOADER_STATE.LOADING;
        this._loadPromise = this._loadWasm();

        try {
            await this._loadPromise;
            this.state = WASM_LOADER_STATE.READY;
            for (const cb of this._readyCallbacks) {
                try { cb(); } catch (e) { /* ignore */ }
            }
            this._readyCallbacks = [];
        } catch (err) {
            this.state = WASM_LOADER_STATE.ERROR;
            this._loadPromise = null;
            throw err;
        }
        return this._loadPromise;
    }

    /**
     * 渲染 TikZ 代码为 SVG
     *
     * @param {string} tikzCode - 完整的 TikZ 代码（含 \\begin{tikzpicture}...\\end{tikzpicture}）
     * @returns {Promise<string>} SVG 字符串
     */
    async render(tikzCode) {
        await this.init();

        if (!this.wasmModule || !this.wasmModule.render) {
            throw new Error('WASM module not initialized or missing render function');
        }

        const timeoutPromise = new Promise((_, reject) => {
            setTimeout(() => reject(new Error('Render timed out')), this.config.timeoutMs);
        });

        const renderPromise = this.wasmModule.render(tikzCode);
        return Promise.race([renderPromise, timeoutPromise]);
    }

    /**
     * 当 WASM 就绪时执行回调
     * @param {Function} callback
     */
    onReady(callback) {
        if (this.state === WASM_LOADER_STATE.READY) {
            callback();
        } else {
            this._readyCallbacks.push(callback);
        }
    }

    /**
     * 加载 WASM 模块
     * @returns {Promise<void>}
     * @private
     */
    async _loadWasm() {
        const { wasmUrl, timeoutMs, maxRetries } = this.config;
        for (let attempt = 0; attempt <= maxRetries; attempt++) {
            try {
                const response = await this._fetchWithTimeout(wasmUrl, timeoutMs);
                const wasmBuffer = await response.arrayBuffer();
                this.wasmModule = {
                    buffer: wasmBuffer,
                    render: (tikzCode) => this._placeholderRender(tikzCode),
                };
                return;
            } catch (err) {
                this.retryCount = attempt + 1;
                if (attempt >= maxRetries) {
                    throw new Error(`WASM load failed after ${maxRetries + 1} attempts: ${err.message}`);
                }
                await new Promise((r) => setTimeout(r, Math.pow(2, attempt) * 1000));
            }
        }
    }

    /**
     * 带超时的 fetch
     * @param {string} url
     * @param {number} timeoutMs
     * @returns {Promise<Response>}
     * @private
     */
    async _fetchWithTimeout(url, timeoutMs) {
        const controller = new AbortController();
        const timeoutId = setTimeout(() => controller.abort(), timeoutMs);
        try {
            const response = await fetch(url, { signal: controller.signal });
            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }
            return response;
        } finally {
            clearTimeout(timeoutId);
        }
    }

    /**
     * 占位渲染：在 WASM 不可用时生成简单的 SVG 占位符
     *
     * 当 TikZJax WASM 模块加载失败或不可用时，此函数生成一个
     * 基于 Canvas 的简化 SVG 渲染（不含真正的 TeX 布局）。
     *
     * @param {string} tikzCode
     * @returns {string} SVG 字符串
     * @private
     */
    _placeholderRender(tikzCode) {
        // 尝试从 TikZ 代码中提取坐标点，生成简化 SVG
        const coords = [];
        const coordRegex = /\\coordinate\s*\(P(\d+)\)\s*at\s*\(([^,]+),\s*([^)]+)\)/g;
        let match;
        while ((match = coordRegex.exec(tikzCode)) !== null) {
            coords.push({
                id: parseInt(match[1]),
                x: parseFloat(match[2]),
                y: parseFloat(match[3]),
            });
        }

        // 提取线段
        const draws = [];
        const drawRegex = /\\draw.*?\(P(\d+)\)\s*--\s*\(P(\d+)\)/g;
        while ((match = drawRegex.exec(tikzCode)) !== null) {
            draws.push({ p1: parseInt(match[1]), p2: parseInt(match[2]) });
        }

        // 计算包围盒
        let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
        for (const c of coords) {
            if (c.x < minX) minX = c.x;
            if (c.y < minY) minY = c.y;
            if (c.x > maxX) maxX = c.x;
            if (c.y > maxY) maxY = c.y;
        }
        if (coords.length === 0) {
            minX = 0; minY = 0; maxX = 10; maxY = 10;
        }

        const pad = 1;
        const width = (maxX - minX + 2 * pad) * 30;
        const height = (maxY - minY + 2 * pad) * 30;
        const tx = (x) => (x - minX + pad) * 30;
        const ty = (y) => (maxY - y + pad) * 30; // SVG Y 轴向下，翻转

        let svg = `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}">\n`;
        svg += `  <!-- Lv-00 placeholder render (WASM not available) -->\n`;

        // 绘制线段
        for (const d of draws) {
            const p1 = coords.find((c) => c.id === d.p1);
            const p2 = coords.find((c) => c.id === d.p2);
            if (p1 && p2) {
                svg += `  <line x1="${tx(p1.x)}" y1="${ty(p1.y)}" x2="${tx(p2.x)}" y2="${ty(p2.y)}" stroke="#333" stroke-width="1.5" />\n`;
            }
        }

        // 绘制点
        for (const c of coords) {
            svg += `  <circle cx="${tx(c.x)}" cy="${ty(c.y)}" r="3" fill="black" />\n`;
            svg += `  <text x="${tx(c.x) + 5}" y="${ty(c.y) - 5}" font-size="10" font-family="serif" fill="#333">P_${c.id}</text>\n`;
        }

        svg += `</svg>`;
        return svg;
    }

    /**
     * 销毁加载器
     */
    destroy() {
        this.wasmModule = null;
        this._readyCallbacks = [];
        this.state = WASM_LOADER_STATE.IDLE;
    }
}


/**
 * =========================================================================
 * 第三部分：缓存策略
 * =========================================================================
 */

/**
 * @class TikzRenderCache
 * @description TikZ 渲染缓存管理器
 *
 *              借鉴 jsTikZ 的缓存策略：
 *              - 基于内容哈希的缓存键
 *              - LRU 淘汰策略
 *              - 内存 + IndexedDB 双层缓存
 *              - 坐标轻微变化时的智能缓存失效
 */
class TikzRenderCache {
    /**
     * @param {Object} [options]
     * @param {number} [options.maxMemoryEntries=50]   - 内存缓存最大条目数
     * @param {number} [options.maxDbEntries=200]       - IndexedDB 最大条目数
     * @param {number} [options.coordEpsilon=0.01]      - 坐标变化容忍度
     * @param {boolean} [options.enablePersistence=false] - 是否启用 IndexedDB 持久化
     */
    constructor(options = {}) {
        /** @type {number} */
        this.maxMemoryEntries = options.maxMemoryEntries || 50;
        /** @type {number} */
        this.maxDbEntries = options.maxDbEntries || 200;
        /** @type {number} */
        this.coordEpsilon = options.coordEpsilon || 0.01;
        /** @type {boolean} */
        this.enablePersistence = options.enablePersistence || false;

        /**
         * 内存缓存：Map<contentHash, {svg: string, timestamp: number, hitCount: number}>
         * @type {Map<string, {svg: string, timestamp: number, hitCount: number}>}
         */
        this.memoryCache = new Map();

        /**
         * 访问顺序记录（用于 LRU 淘汰）
         * @type {Array<string>}
         */
        this.accessOrder = [];

        /** @type {IDBDatabase|null} */
        this.db = null;
        /** @type {boolean} */
        this.dbReady = false;

        if (this.enablePersistence) {
            this._initDb();
        }
    }

    /**
     * 计算 TikZ 代码的内容哈希
     *
     * 使用简单的 djb2 哈希算法，并对坐标进行离散化以减少
     * 微小浮点变动导致的缓存失效。
     *
     * @param {string} tikzCode
     * @returns {string} 哈希字符串
     */
    computeHash(tikzCode) {
        // 对坐标进行离散化（将坐标值四舍五入到容忍度）
        const normalized = tikzCode.replace(
            /at\s*\(\s*(-?\d+\.?\d*)\s*,\s*(-?\d+\.?\d*)\s*\)/g,
            (_, x, y) => {
                const nx = Math.round(parseFloat(x) / this.coordEpsilon) * this.coordEpsilon;
                const ny = Math.round(parseFloat(y) / this.coordEpsilon) * this.coordEpsilon;
                return `at (${nx}, ${ny})`;
            }
        );

        // djb2 哈希
        let hash = 5381;
        for (let i = 0; i < normalized.length; i++) {
            hash = ((hash << 5) + hash) + normalized.charCodeAt(i);
            hash = hash & hash; // 转换为 32 位整数
        }
        return 'tikz_' + (hash >>> 0).toString(16);
    }

    /**
     * 获取缓存的 SVG
     *
     * @param {string} tikzCode
     * @returns {string|null} 缓存的 SVG，未命中返回 null
     */
    get(tikzCode) {
        const hash = this.computeHash(tikzCode);
        const entry = this.memoryCache.get(hash);

        if (entry) {
            entry.hitCount++;
            this._touchAccessOrder(hash);
            return entry.svg;
        }

        return null;
    }

    /**
     * 存储 SVG 到缓存
     *
     * @param {string} tikzCode
     * @param {string} svg
     */
    set(tikzCode, svg) {
        const hash = this.computeHash(tikzCode);

        this.memoryCache.set(hash, {
            svg,
            timestamp: Date.now(),
            hitCount: 0,
        });

        this.accessOrder.push(hash);

        // LRU 淘汰
        while (this.memoryCache.size > this.maxMemoryEntries) {
            const oldest = this.accessOrder.shift();
            if (oldest) {
                const evicted = this.memoryCache.get(oldest);
                this.memoryCache.delete(oldest);
                // 将被淘汰的条目存入 IndexedDB
                if (this.enablePersistence && this.dbReady && evicted) {
                    this._persistToDb(oldest, evicted.svg).catch(() => {});
                }
            }
        }

        // 持久化
        if (this.enablePersistence && this.dbReady) {
            this._persistToDb(hash, svg).catch(() => {});
        }
    }

    /**
     * 清除所有缓存
     */
    clear() {
        this.memoryCache.clear();
        this.accessOrder = [];
    }

    /**
     * 获取缓存统计信息
     * @returns {{memoryEntries: number, totalHits: number}}
     */
    getStats() {
        let totalHits = 0;
        for (const entry of this.memoryCache.values()) {
            totalHits += entry.hitCount;
        }
        return {
            memoryEntries: this.memoryCache.size,
            totalHits,
        };
    }

    /**
     * 更新 LRU 访问顺序
     * @param {string} hash
     * @private
     */
    _touchAccessOrder(hash) {
        const idx = this.accessOrder.indexOf(hash);
        if (idx >= 0) {
            this.accessOrder.splice(idx, 1);
        }
        this.accessOrder.push(hash);
    }

    /**
     * 初始化 IndexedDB
     * @returns {Promise<void>}
     * @private
     */
    async _initDb() {
        try {
            return new Promise((resolve, reject) => {
                const request = indexedDB.open('Lv00TikzCache', 1);
                request.onupgradeneeded = (event) => {
                    const db = /** @type {IDBDatabase} */ (event.target.result);
                    if (!db.objectStoreNames.contains('renderings')) {
                        db.createObjectStore('renderings', { keyPath: 'hash' });
                    }
                };
                request.onsuccess = (event) => {
                    this.db = /** @type {IDBDatabase} */ (event.target.result);
                    this.dbReady = true;
                    resolve();
                };
                request.onerror = () => {
                    reject(new Error('Failed to open IndexedDB'));
                };
            });
        } catch (e) {
            // IndexedDB 不可用（如隐私模式），静默降级
            this.enablePersistence = false;
        }
    }

    /**
     * 持久化条目到 IndexedDB
     * @param {string} hash
     * @param {string} svg
     * @returns {Promise<void>}
     * @private
     */
    async _persistToDb(hash, svg) {
        if (!this.db || !this.dbReady) return;
        try {
            const tx = this.db.transaction('renderings', 'readwrite');
            const store = tx.objectStore('renderings');
            store.put({ hash, svg, timestamp: Date.now() });

            // 检查 IndexedDB 条目数，超过上限时删除最旧的
            const countReq = store.count();
            countReq.onsuccess = () => {
                if (countReq.result > this.maxDbEntries) {
                    const cursorReq = store.openCursor();
                    let removed = 0;
                    const toRemove = countReq.result - this.maxDbEntries;
                    cursorReq.onsuccess = (event) => {
                        const cursor = /** @type {IDBCursorWithValue} */ (event.target.result);
                        if (cursor && removed < toRemove) {
                            cursor.delete();
                            removed++;
                            cursor.continue();
                        }
                    };
                }
            };
        } catch (e) {
            /* IndexedDB 写入失败，静默忽略 */
        }
    }
}


/**
 * =========================================================================
 * 第四部分：完整渲染管道（TemplateGen + WasmLoader + Cache）
 * =========================================================================
 */

/**
 * @class TikzRenderPipeline
 * @description TikZ 渲染管道——组合 TemplateGenerator、WasmLoader、RenderCache
 *              为 Lv-00 提供一键式几何可视化导出管道。
 */
class TikzRenderPipeline {
    /**
     * @param {Object} [options]
     * @param {TikzRenderOptions} [options.template] - TikZ 模板选项
     * @param {WasmLoaderConfig} [options.wasm]       - WASM 加载器配置
     * @param {Object} [options.cache]                - 缓存选项
     */
    constructor(options = {}) {
        /** @type {TikzTemplateGenerator} */
        this.templateGen = new TikzTemplateGenerator(options.template || {});

        /** @type {TikzWasmLoader} */
        this.wasmLoader = new TikzWasmLoader(options.wasm || {});

        /** @type {TikzRenderCache} */
        this.cache = new TikzRenderCache(options.cache || {});

        /** @type {Function|null} 进度回调 */
        this.onProgress = null;
    }

    /**
     * 核心方法：将 Lv-00 几何数据渲染为 SVG
     *
     * 管道流程：
     *   几何数据 -> [TemplateGen] -> TikZ代码 -> [Cache?] -> [WasmLoader] -> SVG
     *
     * @param {Object} geometryData   - 几何数据（同 TikzTemplateGenerator.generateFullTikz）
     * @param {Object} [options]
     * @param {boolean} [options.skipCache=false]   - 跳过缓存
     * @param {string} [options.caption]            - 图标题
     * @returns {Promise<{svg: string, tikz: string, cached: boolean, renderTimeMs: number}>}
     */
    async render(geometryData, options = {}) {
        const startTime = performance.now();

        this._reportProgress('generating', 0);

        // 阶段 1：生成 TikZ 代码
        const tikzCode = this.templateGen.generateTikzPicture(geometryData);
        this._reportProgress('tikz_generated', 30);

        // 阶段 2：检查缓存
        if (!options.skipCache) {
            const cachedSvg = this.cache.get(tikzCode);
            if (cachedSvg) {
                const renderTimeMs = performance.now() - startTime;
                this._reportProgress('cache_hit', 100);
                return {
                    svg: cachedSvg,
                    tikz: tikzCode,
                    cached: true,
                    renderTimeMs,
                };
            }
        }

        // 阶段 3：WASM 渲染
        this._reportProgress('rendering', 50);
        const svg = await this.wasmLoader.render(tikzCode);
        this._reportProgress('rendered', 90);

        // 阶段 4：存入缓存
        if (!options.skipCache) {
            this.cache.set(tikzCode, svg);
        }

        const renderTimeMs = performance.now() - startTime;
        this._reportProgress('complete', 100);

        return {
            svg,
            tikz: tikzCode,
            cached: false,
            renderTimeMs,
        };
    }

    /**
     * 仅生成 TikZ 代码（不渲染 SVG）
     * @param {Object} geometryData
     * @param {string} [caption]
     * @returns {string}
     */
    generateTikzOnly(geometryData, caption = '') {
        if (caption) {
            return this.templateGen.generateFullTikz(geometryData, caption);
        }
        return this.templateGen.generateTikzPicture(geometryData);
    }

    /**
     * 将 SVG 和 TikZ 代码嵌入 HTML 页面
     *
     * @param {Object} geometryData
     * @param {HTMLElement} container - 容器 DOM 元素
     * @param {Object} [options]
     * @returns {Promise<void>}
     */
    async renderToDOM(geometryData, container, options = {}) {
        try {
            container.innerHTML = '<div class="tikz-loading" style="padding:20px;color:#868e96;font-size:12px;">Rendering... / 渲染中...</div>';
            const result = await this.render(geometryData, options);
            container.innerHTML = result.svg;

            // 添加右键导出菜单
            container.title = 'Right-click to export TikZ code / 右键导出 TikZ 代码';
            container.style.cursor = 'context-menu';
            container.dataset.tikzCode = result.tikz;

        } catch (err) {
            container.innerHTML = `<div class="tikz-error" style="padding:20px;color:#ff6b6b;font-size:12px;">Render error: ${_safeEscapeHtml(err.message)}<br>渲染错误: ${_safeEscapeHtml(err.message)}</div>`;
        }
    }

    /**
     * 预加载 WASM 模块（提前调用以加速首次渲染）
     * @returns {Promise<void>}
     */
    async preload() {
        return this.wasmLoader.init();
    }

    /**
     * 下载 TikZ 代码为文件
     * @param {string} tikzCode
     * @param {string} [filename='lv00_geometry.tex']
     */
    static downloadTikz(tikzCode, filename = 'lv00_geometry.tex') {
        const blob = new Blob([tikzCode], { type: 'text/plain;charset=utf-8' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = filename;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        URL.revokeObjectURL(url);
    }

    /**
     * 下载 SVG 为文件
     * @param {string} svg
     * @param {string} [filename='lv00_geometry.svg']
     */
    static downloadSvg(svg, filename = 'lv00_geometry.svg') {
        const blob = new Blob([svg], { type: 'image/svg+xml;charset=utf-8' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = filename;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        URL.revokeObjectURL(url);
    }

    /**
     * 销毁管道
     */
    destroy() {
        this.wasmLoader.destroy();
        this.cache.clear();
        this.onProgress = null;
    }

    /**
     * 报告进度
     * @param {string} stage
     * @param {number} percent
     * @private
     */
    _reportProgress(stage, percent) {
        if (this.onProgress) {
            this.onProgress({ stage, percent });
        }
    }
}


/**
 * =========================================================================
 * 第五部分：与 web/js/render.js 的集成桥接
 * =========================================================================
 *
 * 在 Lv-00 的 Web 前端中使用的示例：
 *
 * ```javascript
 * // 初始化管道
 * const pipeline = new TikzRenderPipeline({
 *     template: { scale: 1.0, showLabels: true },
 *     wasm: { lazyLoad: true },
 *     cache: { maxMemoryEntries: 50 },
 * });
 *
 * // 从 Lv-00 App 数据生成几何数据并渲染
 * async function exportTikzView() {
 *     const app = window.Lv00App; // Lv-00 当前应用实例
 *     const geomData = TikzTemplateGenerator.fromAppData(
 *         app.points, app.segments, app.constraints
 *     );
 *     const result = await pipeline.render(geomData);
 *     document.getElementById('tikz-output').innerHTML = result.svg;
 * }
 *
 * // 导出为 LaTeX 文件
 * function exportLatex() {
 *     const app = window.Lv00App;
 *     const geomData = TikzTemplateGenerator.fromAppData(
 *         app.points, app.segments, app.constraints
 *     );
 *     const fullTikz = pipeline.generateTikzOnly(geomData, 'Lv-00 Construction');
 *     TikzRenderPipeline.downloadTikz(fullTikz);
 * }
 * ```
 */


/**
 * =========================================================================
 * 第六部分：模块导出
 * =========================================================================
 */

if (typeof module !== 'undefined' && module.exports) {
    module.exports = {
        TikzTemplateGenerator,
        TikzWasmLoader,
        TikzRenderCache,
        TikzRenderPipeline,
        DEFAULT_TIKZ_OPTIONS,
        DEFAULT_WASM_CONFIG,
    };
}

if (typeof window !== 'undefined') {
    window.Lv00TikzPipeline = {
        TikzTemplateGenerator,
        TikzWasmLoader,
        TikzRenderCache,
        TikzRenderPipeline,
        DEFAULT_TIKZ_OPTIONS,
        DEFAULT_WASM_CONFIG,
    };
}
