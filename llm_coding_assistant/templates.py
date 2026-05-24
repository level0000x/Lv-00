"""
Lv-00 UI编程辅助系统 - 代码模板库
包含常用的代码模板和片段

本文件面向用户直接查询和代码生成，模板以结构化字典形式组织。
与 lv00_knowledge.py 的关系：
  - lv00_knowledge.py 是模板和知识的权威来源（single source of truth），
    其中的代码模式面向 LLM 上下文注入
  - templates.py 中的部分模板与 lv00_knowledge.py 存在功能重叠，
    这些重叠模板已标记为 deprecated，但保留数据以确保向后兼容

已废弃的模板（已迁移到 lv00_knowledge.py）：
  - wasm_basic_binding      -> Lv00KnowledgeBase.code_patterns["wasm_binding_basic"]
  - wasm_string_binding     -> Lv00KnowledgeBase.code_patterns["wasm_binding_with_string"]
  - wasm_array_binding      -> Lv00KnowledgeBase.code_patterns["wasm_binding_with_array"]
  - wasm_js_wrapper         -> Lv00KnowledgeBase.code_patterns["wasm_binding_basic"] (JS端)
  - canvas_basic_renderer   -> Lv00KnowledgeBase.code_patterns["canvas_renderer_class"]
  - canvas_event_handler    -> Lv00KnowledgeBase.code_patterns["canvas_event_handlers"]

独有模板（仅在本文件中维护）：
  - module_panel: 模块面板模板
  - modal_dialog: 模态对话框模板
"""

import logging

# 模块级日志
logger = logging.getLogger(__name__)

CODE_TEMPLATES = {
    # ============================================
    # WebAssembly 绑定模板
    # [已废弃] 已迁移到 lv00_knowledge.py Lv00KnowledgeBase.code_patterns
    # 保留数据以确保向后兼容，新代码请使用 lv00_knowledge.py 中的权威版本
    # ============================================

    "wasm_basic_binding": {
        "name": "基础WebAssembly绑定",
        "description": "为C函数创建简单的Web绑定",
        "language": "c",
        "tags": ["wasm", "binding", "deprecated"],
        "deprecated": True,
        "_deprecated": True,  # 标记为已废弃，get_template() 会打印警告
        "template": '''/* WebAssembly绑定模板 */
#include "lv00.h"
#include <emscripten.h>

/**
 * @brief {brief_description}
 * @param graph 约束图指针
{param_docs}
 * @return {return_description}
 */
EMSCRIPTEN_KEEPALIVE
{return_type} web_{function_name}(void* graph{params}) {{
    if (!graph) {{
        return {error_return};
    }}

    ConstraintGraph* g = (ConstraintGraph*)graph;

    // ---- 自动生成的桩实现 / Auto-generated stub ----
    // 替换 {implementation} 占位符为实际的 Lv-00 API 调用：
    //   graph_add_point(g, coords, 2);          // 添加点
    //   graph_add_line_segment(g, p1, p2);       // 添加线段
    //   graph_normalize(g);                      // 图规范化
    //   graph_get_node_count(g);                 // 查询节点数
    //   symbolic_coord_to_double(coord);         // 坐标转换
    {implementation}

    return {success_return};
}}
'''
    },

    "wasm_string_binding": {
        "name": "返回字符串的绑定",
        "description": "返回动态分配字符串的绑定",
        "language": "c",
        "tags": ["wasm", "binding", "string", "deprecated"],
        "deprecated": True,
        "_deprecated": True,
        "template": '''/* 返回字符串的绑定 - JS端负责free() */
EMSCRIPTEN_KEEPALIVE
char* web_{function_name}(void* graph) {{
    if (!graph) return NULL;

    ConstraintGraph* g = (ConstraintGraph*)graph;

    // 构建JSON格式的响应
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "id", 0);
    cJSON_AddStringToObject(root, "name", "{object_name}");

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    return strdup(json_str);  // 必须使用strdup
}}
'''
    },

    "wasm_array_binding": {
        "name": "返回结构体数组的绑定",
        "description": "返回数组的绑定，需要预分配内存",
        "language": "c",
        "tags": ["wasm", "binding", "array", "deprecated"],
        "deprecated": True,
        "_deprecated": True,
        "template": '''/* 返回数组的绑定 */
// 在绑定文件顶部定义结构体
typedef struct {{
    int id;
    int type;
    double x;
    double y;
}} WebPointInfo;

/**
 * @brief 获取所有点信息
 * @param graph 约束图
 * @param out_points 输出数组(预分配)
 * @param max_points 最大数量
 * @return 实际返回的数量
 */
EMSCRIPTEN_KEEPALIVE
int web_get_points(void* graph, WebPointInfo* out_points, int max_points) {{
    if (!graph || !out_points || max_points <= 0) return 0;

    ConstraintGraph* g = (ConstraintGraph*)graph;
    int count = 0;

    for (int i = 0; i < g->node_count && count < max_points; i++) {{
        GeomNode* node = g->nodes[i];
        if (!node) continue;

        // 根据节点类型填充
        if (node->type == GEOM_POINT) {{
            out_points[count].id = node->id;
            out_points[count].type = node->type;

            // 转换坐标到double
            if (node->symbolic_coords && node->coord_count >= 2) {{
                out_points[count].x = symbolic_coord_to_double(node->symbolic_coords[0]);
                out_points[count].y = symbolic_coord_to_double(node->symbolic_coords[1]);
            }}

            count++;
        }}
    }}

    return count;
}}
'''
    },

    # ============================================
    # JavaScript 绑定包装器
    # [已废弃] 已迁移到 lv00_knowledge.py，保留数据以确保向后兼容
    # ============================================

    "wasm_js_wrapper": {
        "name": "JavaScript绑定包装器",
        "description": "封装ccall调用的JS代码",
        "language": "javascript",
        "tags": ["wasm", "javascript", "wrapper", "deprecated"],
        "deprecated": True,
        "_deprecated": True,
        "template": '''/**
 * Lv-00 API 包装器
 * 自动处理WebAssembly模块加载和调用
 */
class Lv00API {{
    constructor() {{
        this.module = null;
        this.ready = false;
    }}

    async init() {{
        if (this.ready) return;

        // 加载WASM模块
        this.module = await loadLv00Wasm();
        this.ready = true;

        console.log('Lv-00 API initialized');
    }}

    // {description}
    async {function_name}({params}) {{
        if (!this.ready) await this.init();

        return new Promise((resolve, reject) => {{
            try {{
                const result = this.module.ccall(
                    'web_{function_name}',
                    '{return_type}',  // ccall格式: 'number', 'string', null
                    [{param_types}],  // 参数类型数组
                    [{param_names}]   // 参数值数组
                );
                resolve(result);
            }} catch (err) {{
                reject(err);
            }}
        }});
    }}

    // 便捷方法: 获取图统计
    async getStats() {{
        return {{
            nodeCount: await this._ccall('web_graph_get_node_count', 'number', []),
            constraintCount: await this._ccall('web_graph_get_constraint_count', 'number', [])
        }};
    }}

    // 内部: 通用ccall
    async _ccall(name, returnType, argTypes, args) {{
        if (!this.ready) await this.init();
        return this.module.ccall(name, returnType, argTypes, args);
    }}
}}

// 使用示例
const api = new Lv00API();
api.init().then(() => {{
    api.{function_name}({example_params}).then(result => {{
        console.log('Result:', result);
    }});
}});
'''
    },

    # ============================================
    # Canvas 渲染模板
    # [已废弃] 已迁移到 lv00_knowledge.py Lv00KnowledgeBase.code_patterns
    # 保留数据以确保向后兼容，新代码请使用 lv00_knowledge.py 中的权威版本
    # ============================================

    "canvas_basic_renderer": {
        "name": "基础Canvas渲染器",
        "description": "点线渲染的完整实现",
        "language": "javascript",
        "tags": ["canvas", "renderer", "deprecated"],
        "deprecated": True,
        "_deprecated": True,
        "template": '''/**
 * Lv-00 Canvas渲染器
 * 负责几何对象的可视化
 */
class GeometryRenderer {{
    constructor(canvas) {{
        this.canvas = canvas;
        this.ctx = canvas.getContext('2d');

        // 渲染状态
        this.points = [];
        this.segments = [];
        this.regions = [];
        this.labels = [];

        // 变换参数
        this.zoom = 1.0;
        this.offsetX = 0;
        this.offsetY = 0;

        // 选中状态
        this.selectedId = null;
        this.hoveredId = null;

        // 样式配置
        this.colors = {{
            background: '#080808',
            grid: '#1a1a1a',
            point: '#888888',
            pointSelected: '#4caf50',
            segment: '#666666',
            segmentSelected: '#4caf50',
            label: '#aaaaaa',
            trustColors: {{
                green: '#4caf50',
                blue: '#2196f3',
                yellow: '#ffeb3b',
                orange: '#ff9800',
                red: '#f44336'
            }}
        }};

        // 初始化
        this.resize();
        window.addEventListener('resize', () => this.resize());
    }}

    resize() {{
        const parent = this.canvas.parentElement;
        this.canvas.width = parent.clientWidth;
        this.canvas.height = parent.clientHeight;
        this.render();
    }}

    worldToScreen(wx, wy) {{
        return {{
            x: (wx * this.zoom) + this.offsetX,
            y: (wy * this.zoom) + this.offsetY
        }};
    }}

    screenToWorld(sx, sy) {{
        return {{
            x: (sx - this.offsetX) / this.zoom,
            y: (sy - this.offsetY) / this.zoom
        }};
    }}

    clear() {{
        this.ctx.fillStyle = this.colors.background;
        this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);
    }}

    drawGrid() {{
        const gridSize = 50 * this.zoom;
        const startX = this.offsetX % gridSize;
        const startY = this.offsetY % gridSize;

        this.ctx.strokeStyle = this.colors.grid;
        this.ctx.lineWidth = 1;

        // 垂直线
        for (let x = startX; x < this.canvas.width; x += gridSize) {{
            this.ctx.beginPath();
            this.ctx.moveTo(x, 0);
            this.ctx.lineTo(x, this.canvas.height);
            this.ctx.stroke();
        }}

        // 水平线
        for (let y = startY; y < this.canvas.height; y += gridSize) {{
            this.ctx.beginPath();
            this.ctx.moveTo(0, y);
            this.ctx.lineTo(this.canvas.width, y);
            this.ctx.stroke();
        }}

        // 坐标轴
        this.ctx.strokeStyle = '#333333';
        this.ctx.lineWidth = 2;

        const origin = this.worldToScreen(0, 0);
        if (origin.y >= 0 && origin.y <= this.canvas.height) {{
            this.ctx.beginPath();
            this.ctx.moveTo(0, origin.y);
            this.ctx.lineTo(this.canvas.width, origin.y);
            this.ctx.stroke();
        }}

        if (origin.x >= 0 && origin.x <= this.canvas.width) {{
            this.ctx.beginPath();
            this.ctx.moveTo(origin.x, 0);
            this.ctx.lineTo(origin.x, this.canvas.height);
            this.ctx.stroke();
        }}
    }}

    drawPoint(point) {{
        const pos = this.worldToScreen(point.x, point.y);
        const isSelected = point.id === this.selectedId;
        const isHovered = point.id === this.hoveredId;

        const radius = isSelected ? 8 : (isHovered ? 7 : 6);
        const color = point.trustColor !== undefined
            ? this.colors.trustColors[point.trustColor] || this.colors.point
            : (isSelected ? this.colors.pointSelected : this.colors.point);

        // 绘制点
        this.ctx.beginPath();
        this.ctx.arc(pos.x, pos.y, radius, 0, Math.PI * 2);
        this.ctx.fillStyle = color;
        this.ctx.fill();
        this.ctx.strokeStyle = '#222222';
        this.ctx.lineWidth = 2;
        this.ctx.stroke();

        // 绘制标签
        if (point.label || point.id !== undefined) {{
            this.ctx.fillStyle = this.colors.label;
            this.ctx.font = '10px Consolas, monospace';
            this.ctx.fillText(point.label || `P${{point.id}}`, pos.x + 12, pos.y + 4);
        }}
    }}

    drawSegment(seg) {{
        const p1 = this.points.find(p => p.id === seg.p1);
        const p2 = this.points.find(p => p.id === seg.p2);

        if (!p1 || !p2) return;

        const s1 = this.worldToScreen(p1.x, p1.y);
        const s2 = this.worldToScreen(p2.x, p2.y);

        const isSelected = seg.id === this.selectedId;

        this.ctx.beginPath();
        this.ctx.moveTo(s1.x, s1.y);
        this.ctx.lineTo(s2.x, s2.y);
        this.ctx.strokeStyle = isSelected ? this.colors.segmentSelected : this.colors.segment;
        this.ctx.lineWidth = isSelected ? 3 : 2;
        this.ctx.stroke();

        // 线段ID标签
        if (seg.label || seg.id !== undefined) {{
            const midX = (s1.x + s2.x) / 2;
            const midY = (s1.y + s2.y) / 2;
            this.ctx.fillStyle = '#666666';
            this.ctx.font = '9px Consolas, monospace';
            this.ctx.fillText(seg.label || `S${{seg.id}}`, midX + 5, midY - 5);
        }}
    }}

    render() {{
        this.clear();
        this.drawGrid();

        // 绘制线段（在点下面）
        this.segments.forEach(seg => this.drawSegment(seg));

        // 绘制区域
        this.regions.forEach(reg => this.drawRegion(reg));

        // 绘制点（在最上层）
        this.points.forEach(point => this.drawPoint(point));
    }}

    // 更新数据并重绘
    updateData(points, segments, regions) {{
        this.points = points || [];
        this.segments = segments || [];
        this.regions = regions || [];
        this.render();
    }}

    // 添加点
    addPoint(id, x, y, props = {{}}) {{
        this.points.push({{ id, x, y, ...props }});
        this.render();
    }}

    // 删除点
    removePoint(id) {{
        this.points = this.points.filter(p => p.id !== id);
        this.segments = this.segments.filter(s => s.p1 !== id && s.p2 !== id);
        this.render();
    }}

    // 缩放
    zoomAt(factor, cx, cy) {{
        const worldBefore = this.screenToWorld(cx, cy);
        this.zoom *= factor;
        const worldAfter = this.screenToWorld(cx, cy);

        this.offsetX += (worldAfter.x - worldBefore.x) * this.zoom;
        this.offsetY += (worldAfter.y - worldBefore.y) * this.zoom;

        this.render();
    }}

    // 平移
    pan(dx, dy) {{
        this.offsetX += dx;
        this.offsetY += dy;
        this.render();
    }}
}}

// 导出
window.GeometryRenderer = GeometryRenderer;
'''
    },

    "canvas_event_handler": {
        "name": "Canvas事件处理器",
        "description": "鼠标和键盘交互处理",
        "language": "javascript",
        "tags": ["canvas", "event", "interaction", "deprecated"],
        "deprecated": True,
        "_deprecated": True,
        "template": '''/**
 * Canvas交互处理器
 * 管理工具切换和事件响应
 */
class CanvasInteraction {{
    constructor(canvas, renderer, api) {{
        this.canvas = canvas;
        this.renderer = renderer;
        this.api = api;

        // 当前工具
        this.tool = 'select';  // select, point, segment, pan, region, probe
        this.tools = ['select', 'point', 'segment', 'pan', 'region', 'probe'];

        // 交互状态
        this.isDragging = false;
        this.isPanning = false;
        this.pendingSegment = null;
        this.dragStart = null;

        // 历史记录(用于撤销)
        this.history = [];
        this.historyIndex = -1;

        // 初始化
        this.bindEvents();
        this.updateCursor();
    }}

    bindEvents() {{
        // 鼠标事件
        this.canvas.addEventListener('mousedown', (e) => this.onMouseDown(e));
        this.canvas.addEventListener('mousemove', (e) => this.onMouseMove(e));
        this.canvas.addEventListener('mouseup', (e) => this.onMouseUp(e));
        this.canvas.addEventListener('mouseleave', (e) => this.onMouseLeave(e));

        // 右键菜单
        this.canvas.addEventListener('contextmenu', (e) => {{
            e.preventDefault();
            this.onContextMenu(e);
        }});

        // 滚轮缩放
        this.canvas.addEventListener('wheel', (e) => {{
            e.preventDefault();
            this.onWheel(e);
        }}, {{ passive: false }});

        // 键盘快捷键
        document.addEventListener('keydown', (e) => this.onKeyDown(e));

        // 工具栏按钮
        this.bindToolbar();
    }}

    bindToolbar() {{
        this.tools.forEach(tool => {{
            const btn = document.getElementById(`tool${{tool.charAt(0).toUpperCase() + tool.slice(1)}}`);
            if (btn) {{
                btn.addEventListener('click', () => this.setTool(tool));
            }}
        }});
    }}

    setTool(tool) {{
        if (!this.tools.includes(tool)) return;

        // 更新UI
        this.tools.forEach(t => {{
            const btn = document.getElementById(`tool${{t.charAt(0).toUpperCase() + t.slice(1)}}`);
            if (btn) btn.classList.toggle('active', t === tool);
        }});

        this.tool = tool;
        this.pendingSegment = null;
        this.updateCursor();

        console.log('Tool changed to:', tool);
    }}

    updateCursor() {{
        const cursors = {{
            select: 'default',
            point: 'crosshair',
            segment: 'crosshair',
            pan: 'grab',
            region: 'crosshair',
            probe: 'help'
        }};
        this.canvas.style.cursor = cursors[this.tool] || 'default';
    }}

    getWorldCoords(e) {{
        const rect = this.canvas.getBoundingClientRect();
        return this.renderer.screenToWorld(
            e.clientX - rect.left,
            e.clientY - rect.top
        );
    }}

    findNearestPoint(wx, wy, threshold = 0.3) {{
        let nearest = null;
        let minDist = threshold;

        for (const p of this.renderer.points) {{
            const dist = Math.sqrt((p.x - wx) ** 2 + (p.y - wy) ** 2);
            if (dist < minDist) {{
                minDist = dist;
                nearest = p;
            }}
        }}

        return nearest;
    }}

    async onMouseDown(e) {{
        const world = this.getWorldCoords(e);

        switch (this.tool) {{
            case 'select':
                const point = this.findNearestPoint(world.x, world.y);
                if (point) {{
                    this.renderer.selectedId = point.id;
                    this.isDragging = true;
                    this.dragStart = {{ x: world.x, y: world.y, pointId: point.id }};
                }} else {{
                    this.renderer.selectedId = null;
                }}
                this.renderer.render();
                break;

            case 'point':
                await this.addPointAt(world.x, world.y);
                break;

            case 'segment':
                const start = this.findNearestPoint(world.x, world.y);
                if (start) {{
                    this.pendingSegment = {{ start: start.id }};
                }}
                break;

            case 'pan':
                this.isPanning = true;
                this.lastPanPos = {{ x: e.clientX, y: e.clientY }};
                this.canvas.style.cursor = 'grabbing';
                break;
        }}
    }}

    async onMouseMove(e) {{
        const world = this.getWorldCoords(e);

        // 悬停高亮
        if (this.tool === 'select' || this.tool === 'probe') {{
            const point = this.findNearestPoint(world.x, world.y);
            this.renderer.hoveredId = point ? point.id : null;
            this.renderer.render();
        }}

        // 拖拽移动点
        if (this.isDragging && this.dragStart) {{
            await this.movePoint(this.dragStart.pointId, world.x, world.y);
        }}

        // 平移画布
        if (this.isPanning && this.lastPanPos) {{
            const dx = e.clientX - this.lastPanPos.x;
            const dy = e.clientY - this.lastPanPos.y;
            this.renderer.pan(dx, dy);
            this.lastPanPos = {{ x: e.clientX, y: e.clientY }};
        }}
    }}

    async onMouseUp(e) {{
        if (this.isDragging) {{
            this.isDragging = false;
            this.dragStart = null;
        }}

        if (this.isPanning) {{
            this.isPanning = false;
            this.updateCursor();
        }}

        // 完成线段绘制
        if (this.tool === 'segment' && this.pendingSegment) {{
            const world = this.getWorldCoords(e);
            const end = this.findNearestPoint(world.x, world.y);
            if (end && end.id !== this.pendingSegment.start) {{
                await this.addSegment(this.pendingSegment.start, end.id);
            }}
            this.pendingSegment = null;
        }}
    }}

    onMouseLeave(e) {{
        this.renderer.hoveredId = null;
        this.renderer.render();
    }}

    onWheel(e) {{
        const factor = e.deltaY > 0 ? 0.9 : 1.1;
        this.renderer.zoomAt(factor, e.clientX, e.clientY);
    }}

    onKeyDown(e) {{
        // 工具快捷键
        const keyMap = {{
            's': 'select',
            'p': 'point',
            'l': 'segment',
            'r': 'region',
            ' ': 'probe'  // 空格
        }};

        if (keyMap[e.key] && !e.ctrlKey && !e.metaKey) {{
            e.preventDefault();
            this.setTool(keyMap[e.key]);
        }}

        // 撤销/重做
        if (e.ctrlKey || e.metaKey) {{
            if (e.key === 'z') {{
                e.preventDefault();
                if (e.shiftKey) {{
                    this.redo();
                }} else {{
                    this.undo();
                }}
            }}
        }}

        // 删除选中
        if (e.key === 'Delete' || e.key === 'Backspace') {{
            if (this.renderer.selectedId !== null) {{
                this.deleteSelected();
            }}
        }}
    }}

    async onContextMenu(e) {{
        const world = this.getWorldCoords(e);
        const point = this.findNearestPoint(world.x, world.y);

        if (point) {{
            this.renderer.selectedId = point.id;
            this.renderer.render();

            // 显示右键菜单
            showContextMenu(e.clientX, e.clientY, [
                {{ label: 'Incidence / 关联', action: () => this.showIncidenceDialog(point.id) }},
                {{ label: 'Betweenness / 之间', action: () => this.showBetweennessDialog(point.id) }},
                {{ type: 'separator' }},
                {{ label: 'Delete / 删除', action: () => this.deletePoint(point.id), danger: true }}
            ]);
        }}
    }}

    // API操作
    async addPointAt(x, y) {{
        try {{
            const id = await this.api.addPoint(x, y);
            this.renderer.addPoint(id, x, y);
            this.addToHistory('add_point', {{ id, x, y }});
            showStatus(`Added point ${{id}} at (${{x.toFixed(2)}}, ${{y.toFixed(2)}})`);
        }} catch (err) {{
            showError('Failed to add point:', err);
        }}
    }}

    async addSegment(p1, p2) {{
        try {{
            const id = await this.api.addSegment(p1, p2);
            this.renderer.segments.push({{ id, p1, p2 }});
            this.renderer.render();
            this.addToHistory('add_segment', {{ id, p1, p2 }});
            showStatus(`Added segment ${{id}}: P${{p1}} -> P${{p2}}`);
        }} catch (err) {{
            showError('Failed to add segment:', err);
        }}
    }}

    async movePoint(id, x, y) {{
        try {{
            await this.api.movePoint(id, x, y);
            const point = this.renderer.points.find(p => p.id === id);
            if (point) {{
                point.x = x;
                point.y = y;
                this.renderer.render();
            }}
        }} catch (err) {{
            console.error('Failed to move point:', err);
        }}
    }}

    async deletePoint(id) {{
        try {{
            await this.api.deleteNode(id);
            this.renderer.removePoint(id);
            this.renderer.selectedId = null;
            this.addToHistory('delete_point', {{ id }});
            showStatus(`Deleted point ${{id}}`);
        }} catch (err) {{
            showError('Failed to delete point:', err);
        }}
    }}

    deleteSelected() {{
        if (this.renderer.selectedId !== null) {{
            this.deletePoint(this.renderer.selectedId);
        }}
    }}

    // 历史记录
    addToHistory(action, data) {{
        // 移除重做历史
        this.history = this.history.slice(0, this.historyIndex + 1);

        this.history.push({{ action, data, timestamp: Date.now() }});
        this.historyIndex = this.history.length - 1;
    }}

    undo() {{
        if (this.historyIndex < 0) return;

        const entry = this.history[this.historyIndex];
        // 撤销操作：根据 entry.action 类型逆操作
        // Undo: reverse the action based on entry.action type
        switch (entry.action) {{
            case 'add_point':
                // 撤销添加点 → 删除该点
                this.renderer.removePoint(entry.data.id);
                break;
            case 'add_segment':
                // 撤销添加线段 → 从 segments 数组中移除
                this.renderer.segments = this.renderer.segments.filter(
                    s => s.id !== entry.data.id
                );
                this.renderer.render();
                break;
            case 'delete_point':
                // 撤销删除点 → 重新添加该点
                this.renderer.points.push(entry.data);
                this.renderer.render();
                break;
        }}

        this.historyIndex--;
    }}

    redo() {{
        if (this.historyIndex >= this.history.length - 1) return;

        this.historyIndex++;
        const entry = this.history[this.historyIndex];
        // 重做操作：根据 entry.action 类型重新执行
        // Redo: re-apply the action based on entry.action type
        switch (entry.action) {{
            case 'add_point':
                // 重做添加点
                this.renderer.addPoint(entry.data.id, entry.data.x, entry.data.y, {{}});
                break;
            case 'add_segment':
                // 重做添加线段
                this.renderer.segments.push({{ id: entry.data.id, p1: entry.data.p1, p2: entry.data.p2 }});
                this.renderer.render();
                break;
            case 'delete_point':
                // 重做删除点
                this.renderer.removePoint(entry.data.id);
                break;
        }}
    }}
}}

// 导出
window.CanvasInteraction = CanvasInteraction;
'''
    },

    # ============================================
    # UI组件模板 (templates.py 独有)
    # ============================================

    "module_panel": {
        "name": "模块面板模板",
        "description": "标准的Lv-00模块面板结构",
        "language": "html",
        "tags": ["ui", "panel", "module"],
        "deprecated": False,
        "template": '''<!-- {module_name} 模块面板 -->
<div id="panel{module}" class="module-panel panel-hidden">
    <div class="panel">
        <div class="panel-title">{MODULE_TITLE} / {中文标题}</div>

        <!-- 操作按钮 -->
        <button class="btn btn-accent" id="btn{module}Action1">
            {ACTION_1} / {中文}
        </button>
        <button class="btn btn-accent" id="btn{module}Action2">
            {ACTION_2} / {中文}
        </button>

        <div class="panel-separator"></div>

        <!-- 参数输入 -->
        <div class="input-row">
            <label>{PARAM}</label>
            <input type="text" class="input-field" id="input{module}Param" placeholder="{placeholder}">
        </div>
    </div>

    <div class="panel">
        <div class="panel-title">{INFO} / 信息</div>
        <div class="info-box">
            <div class="info-row">
                <span class="info-label">{LABEL} / 标签</span>
                <span class="info-value" id="{module}Count">0</span>
            </div>
        </div>
    </div>
</div>

<script>
// {module} 模块逻辑
(function() {{
    // 操作1
    document.getElementById('btn{module}Action1')?.addEventListener('click', async function() {{
        const param = document.getElementById('input{module}Param')?.value;

        try {{
            const result = await window.lv00Api.{api_method}(param);
            console.log('{module} action1 result:', result);
            showStatus('{MODULE_TITLE}: 操作完成');
        }} catch (err) {{
            showError('{module} error:', err);
        }}
    }});

    // 操作2
    document.getElementById('btn{module}Action2')?.addEventListener('click', async function() {{
        try {{
            const result = await window.lv00Api.{api_method2}();
            document.getElementById('{module}Count').textContent = result;
        }} catch (err) {{
            showError('{module} error:', err);
        }}
    }});
}})();
</script>
'''
    },

    "modal_dialog": {
        "name": "模态对话框模板",
        "description": "Lv-00标准的模态对话框",
        "language": "html",
        "tags": ["ui", "modal", "dialog"],
        "deprecated": False,
        "template": '''<!-- 模态对话框模板 -->
<div class="modal-overlay" id="modal{template_name}" role="dialog" aria-modal="true">
    <div class="modal-dialog" style="min-width: {width}px;">
        <div class="modal-title">{TITLE} / {中文标题}</div>

        <div class="modal-body">
            <p style="margin-bottom:8px;">{description}</p>

            <!-- 对话框内容 -->
            <div id="{template_name}Content" style="max-height:300px;overflow-y:auto;">
                <!-- 动态内容 -->
            </div>
        </div>

        <div class="modal-actions">
            <button class="modal-btn" id="btn{template_name}Cancel">
                {CANCEL} / 取消
            </button>
            <button class="modal-btn modal-btn-primary" id="btn{template_name}Confirm">
                {CONFIRM} / 确认
            </button>
        </div>
    </div>
</div>

<script>
// 对话框控制逻辑
class {TemplateName}Dialog {{
    constructor() {{
        this.overlay = document.getElementById('modal{template_name}');
        this.content = document.getElementById('{template_name}Content');

        this.bindEvents();
    }}

    bindEvents() {{
        document.getElementById('btn{template_name}Cancel')?.addEventListener('click', () => this.hide());
        document.getElementById('btn{template_name}Confirm')?.addEventListener('click', () => this.confirm());

        // ESC键关闭
        this.overlay?.addEventListener('keydown', (e) => {{
            if (e.key === 'Escape') this.hide();
        }});
    }}

    show(data) {{
        this.currentData = data;
        this.renderContent(data);
        this.overlay.classList.add('active');
    }}

    hide() {{
        this.overlay.classList.remove('active');
        this.currentData = null;
    }}

    renderContent(data) {{
        // 子类实现
        this.content.innerHTML = '<p>Content here...</p>';
    }}

    confirm() {{
        // 子类实现
        this.hide();
    }}
}}

// 创建单例
window.{templateName}Dialog = new {TemplateName}Dialog();
</script>
'''
    }
}


# ============================================
# 常用代码片段
# ============================================

CODE_SNIPPETS = {
    "c_memory_management": '''
// Lv-00 内存管理规范

// 1. 创建后必须释放
SymbolicCoord* coord = symbolic_coord_create_rational(1, 2);
// ... 使用coord ...
symbolic_coord_destroy(coord);  // 必须释放

// 2. 图的创建和销毁
ConstraintGraph* graph = graph_create();
// ... 使用graph ...
graph_destroy(graph);

// 3. 字符串需要strdup后free
char* str = strdup("hello");
// ... 使用str ...
free(str);

// 4. 数组的分配
int* arr = malloc(n * sizeof(int));
// ... 使用arr ...
free(arr);
''',

    "c_error_handling": '''
// Lv-00 错误处理模式

// 检查空指针
if (!graph) return -1;
if (!coord) return false;

// 检查返回值
AddNodeResult result = graph_add_point(graph, coords, 2);
if (result != ADD_NODE_OK) {
    // 处理错误
    return -1;
}

// 使用错误码
typedef enum {
    ADD_NODE_OK = 0,
    ADD_NODE_ERROR = -1,
    ADD_NODE_NULL_PARAM = -2
} AddNodeResult;
''',

    "js_promise_await": '''
// JavaScript异步调用模式

// Promise风格
async function addPoint(x, y) {
    return new Promise((resolve, reject) => {
        try {
            const id = Module.ccall('web_graph_add_point', 'number',
                ['number', 'number', 'number', 'number', 'number'],
                [graphPtr, x, 1, y, 1]
            );
            resolve(id);
        } catch (err) {
            reject(err);
        }
    });
}

// 使用async/await
async function createTriangle() {
    try {
        const p1 = await addPoint(0, 0);
        const p2 = await addPoint(1, 0);
        const p3 = await addPoint(0.5, Math.sqrt(3)/2);
        const s1 = await addSegment(p1, p2);
        const s2 = await addSegment(p2, p3);
        const s3 = await addSegment(p3, p1);
        console.log('Triangle created!');
    } catch (err) {
        console.error('Failed to create triangle:', err);
    }
}
''',

    "css_dark_theme": '''
// Lv-00 深色主题变量

:root {
    --bg-primary: #0a0a0a;
    --bg-secondary: #111111;
    --bg-card: #141414;
    --border: #222222;
    --text-primary: #c8c8c8;
    --text-secondary: #666666;
    --accent: #4caf50;
    --accent-hover: #66bb6a;
    --warning: #ff9800;
    --error: #f44336;
}

// 使用方式
.panel {
    background: var(--bg-secondary);
    border: 1px solid var(--border);
    color: var(--text-primary);
}

.btn:hover {
    background: var(--accent);
    color: white;
}
''',

    "emscripten_build": '''
// CMakeLists.txt Emscripten配置

set(EMSCRIPTEN_FLAGS
    "-s EXPORTED_FUNCTIONS=['_web_graph_create','_web_graph_destroy',...]"
    "-s EXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
    "-s MODULARIZE=1"
    "-s EXPORT_NAME='createModule'"
    "-s ALLOW_MEMORY_GROWTH=1"
    "-s INITIAL_MEMORY=64MB"
    "-s MAXIMUM_MEMORY=256MB"
)

# 编译命令
# emcmake cmake -B build
# cmake --build build
'''
}


# ============================================
# API快速参考
# ============================================

API_QUICKREF = {
    "Graph Operations": {
        "graph_create": "创建约束图",
        "graph_destroy": "销毁约束图",
        "graph_add_point": "添加点",
        "graph_add_line_segment": "添加线段",
        "graph_add_region": "添加区域",
        "graph_normalize": "归一化",
        "graph_get_node_count": "获取节点数",
        "graph_get_constraint_count": "获取约束数"
    },

    "Constraints": {
        "graph_add_incidence": "添加关联约束",
        "graph_add_betweenness": "添加之间约束",
        "graph_add_intersection": "添加相交约束",
        "graph_add_containment": "添加包含约束"
    },

    "Function Blocks": {
        "func_block_pack": "打包函数块",
        "func_block_instantiate": "例化函数块",
        "func_block_compose": "组合函数块"
    },

    "Proof": {
        "proof_create_proposition": "创建命题",
        "proof_unify": "合一检查",
        "proof_step_forward": "证明前进一步",
        "proof_step_backward": "证明后退一步"
    },

    "Coordinates": {
        "symbolic_coord_create_rational": "创建有理数坐标",
        "symbolic_coord_equal": "坐标相等判断",
        "symbolic_coord_serialize": "序列化坐标",
        "symbolic_coord_to_double": "转换为双精度"
    }
}


def get_template(name: str) -> str:
    """
    获取代码模板

    如果模板已标记为 deprecated，会通过 logger 打印一条警告日志，
    提示调用者使用 lv00_knowledge.py 中的权威版本。

    Args:
        name: 模板名称（对应 CODE_TEMPLATES 的键）

    Returns:
        模板字符串；若模板不存在则返回空字符串
    """
    entry = CODE_TEMPLATES.get(name, {})
    if entry.get("_deprecated"):
        logger.warning(
            "模板 '%s' 已废弃（已迁移到 lv00_knowledge.py），"
            "请使用 Lv00KnowledgeBase.code_patterns 中的权威版本",
            name,
        )
    return entry.get("template", "")


def get_snippet(name: str) -> str:
    """
    获取代码片段

    Args:
        name: 片段名称（对应 CODE_SNIPPETS 的键）

    Returns:
        代码片段字符串；若不存在则返回空字符串
    """
    return CODE_SNIPPETS.get(name, "")


def get_api_reference(module: str | None = None) -> dict:
    """
    获取API参考

    Args:
        module: 模块名称；若为 None 则返回全部 API 参考

    Returns:
        指定模块的 API 参考字典，或完整的 API_QUICKREF 字典
    """
    if module:
        return API_QUICKREF.get(module, {})
    return API_QUICKREF


# 导出
__all__ = [
    'CODE_TEMPLATES',
    'CODE_SNIPPETS',
    'API_QUICKREF',
    'get_template',
    'get_snippet',
    'get_api_reference'
]
