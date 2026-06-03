"""
Lv-00 UI编程辅助系统 - 专为几何元语言可视化界面优化的LLM助手
================================================================

本文件是 Lv-00 项目模板和领域知识的权威来源（single source of truth）。

职责：
  1. 维护 Lv-00 C API 的签名文档（api_signatures）
  2. 维护面向 LLM 上下文注入的代码模式（code_patterns）
  3. 维护 UI 设计规范（ui_conventions）
  4. 维护常见编程任务的步骤指南（common_tasks）
  5. 提供提示词生成引擎（Lv00PromptEngine）

与 templates.py 的关系：
  - templates.py 中的部分模板与本文件 code_patterns 功能重叠，
    这些重叠模板已在 templates.py 中标记为 deprecated（保留数据以向后兼容）
  - 新代码和权威版本统一在本文件中维护

核心类：
  - Lv00Module: Lv-00 UI 相关模块的枚举定义
  - Lv00KnowledgeBase: 领域知识库，包含 API、代码模式、UI 规范
  - Lv00PromptEngine: 提示词生成引擎，基于知识库生成 AI 提示词

便捷函数：
  - get_lv00_helper(): 获取知识库和提示词引擎实例（带缓存）
  - invalidate_cache(): 手动使缓存失效
  - generate_binding_help(): 生成绑定代码帮助信息
  - generate_renderer_help(): 生成渲染器代码帮助信息
  - generate_task_help(): 生成任务帮助信息
"""

from __future__ import annotations

from typing import Any, Optional
from enum import Enum
import logging
import threading
import time

# 模块级日志
logger = logging.getLogger(__name__)


class Lv00Module(Enum):
    """Lv-00的UI相关模块"""
    SYMBOLIC_COORD = "symbolic_coord"  # 符号坐标
    CONSTRAINT_GRAPH = "constraint_graph"  # 约束图
    NORMALIZATION = "normalization"  # 归一化
    SOLVER = "solver"  # 求解器
    PROOF = "proof"  # 证明系统
    TYPE_SYSTEM = "type_system"  # 类型系统
    FUNC_BLOCK = "func_block"  # 函数块
    UI_CANVAS = "ui_canvas"  # Canvas渲染
    UI_EVENTS = "ui_events"  # 事件处理


class Lv00KnowledgeBase:
    """
    Lv-00领域知识库
    包含项目特定的API、模式、最佳实践
    """
    
    def __init__(self) -> None:
        self.api_signatures: dict[str, dict[str, str]] = self._init_api_signatures()
        self.code_patterns: dict[str, str] = self._init_code_patterns()
        self.ui_conventions: dict[str, str] = self._init_ui_conventions()
        self.common_tasks: list[dict[str, Any]] = self._init_common_tasks()
    
    def _init_api_signatures(self) -> dict[str, dict[str, str]]:
        """初始化Lv-00 C API签名"""
        return {
            # 图操作
            "graph_create": {
                "signature": "ConstraintGraph* graph_create(void)",
                "returns": "创建新的约束图，返回图指针",
                "notes": "使用 graph_destroy() 释放内存"
            },
            "graph_add_point": {
                "signature": "AddNodeResult graph_add_point(ConstraintGraph *g, SymbolicCoord **coords, int coord_count)",
                "returns": "AddNodeResult (ADD_NODE_OK, ADD_NODE_ERROR)",
                "notes": "coords数组会被内部复制"
            },
            "graph_add_line_segment": {
                "signature": "AddNodeResult graph_add_line_segment(ConstraintGraph *g, int p1, int p2)",
                "returns": "AddNodeResult，新节点的ID为 g->next_node_id - 1",
                "notes": "p1和p2必须是已有点的ID"
            },
            "graph_normalize": {
                "signature": "NormalizationResult* graph_normalize(ConstraintGraph *g, bool interactive)",
                "returns": "归一化结果结构体，包含 merged_count",
                "notes": "interactive=true时跨作用域合并需要确认"
            },
            
            # 符号坐标
            "symbolic_coord_create_rational": {
                "signature": "SymbolicCoord* symbolic_coord_create_rational(int64_t num, uint64_t den)",
                "returns": "符号坐标指针，创建失败返回NULL",
                "notes": "den不能为0，自动约分到最简形式"
            },
            "symbolic_coord_equal": {
                "signature": "bool symbolic_coord_equal(SymbolicCoord *a, SymbolicCoord *b)",
                "returns": "true如果相等，false否则",
                "notes": "精确符号比较，不是数值近似"
            },
            "symbolic_coord_serialize": {
                "signature": "char* symbolic_coord_serialize(SymbolicCoord *coord)",
                "returns": "字符串，需要调用 free() 释放",
                "notes": "格式如 '1/2' 或 'poly:[1,0,-2]'"
            },
            
            # 约束
            "graph_add_incidence": {
                "signature": "AddConstraintResult graph_add_incidence(ConstraintGraph *g, int point_id, int line_id)",
                "returns": "AddConstraintResult",
                "notes": "点必须在直线（包括延长线）上"
            },
            "graph_add_betweenness": {
                "signature": "AddConstraintResult graph_add_betweenness(ConstraintGraph *g, int p1, int p2, int p3)",
                "returns": "AddConstraintResult",
                "notes": "p2严格在p1和p3之间"
            },
            
            # 函数块
            "func_block_pack": {
                "signature": "PackResult func_block_pack(ConstraintGraph *g, int *internal_nodes, int internal_count, int *input_ports, int input_count, int *output_ports, int output_count, ConstraintHandle *cross_boundary, int cross_count, FuncBlock **out_block)",
                "returns": "PackResult",
                "notes": "打包前需检查跨边界约束冲突"
            },
            "func_block_instantiate": {
                "signature": "InstantiateResult func_block_instantiate(ConstraintGraph *g, FuncBlock *block, int *arg_map, int arg_count)",
                "returns": "InstantiateResult",
                "notes": "arg_map: input_port_id -> external_node_id"
            },
            
            # 合一检查
            "proof_unify": {
                "signature": "UnifyResult proof_unify(ConstraintGraph *construction, Proposition *prop, bool strict)",
                "returns": "UnifyResult (UNIFY_OK, UNIFY_MISMATCH, etc.)",
                "notes": "strict=true时执行完整合一检查"
            }
        }
    
    def _init_code_patterns(self) -> dict[str, str]:
        """
        初始化常用代码模式（面向 LLM 上下文注入）

        本方法是代码模式的权威来源。templates.py 中的同名功能模板
        已标记为 deprecated，新代码应统一使用本方法返回的模式。

        模式与 templates.py 的对应关系:
        - wasm_binding_basic       <-> templates.py: wasm_basic_binding (deprecated)
        - wasm_binding_with_string <-> templates.py: wasm_string_binding (deprecated)
        - wasm_binding_with_array  <-> templates.py: wasm_array_binding (deprecated)
        - canvas_renderer_class    <-> templates.py: canvas_basic_renderer (deprecated)
        - canvas_event_handlers    <-> templates.py: canvas_event_handler (deprecated)
        """
        return {
            # WebAssembly绑定模式
            "wasm_binding_basic": """
/* EMSCRIPTEN_KEEPALIVE 导出函数 */
EMSCRIPTEN_KEEPALIVE
int web_function_name(void* graph, int param1, int param2) {
    if (!graph) return -1;
    
    // 调用C API
    ResultType result = c_api_function((ConstraintGraph*)graph, param1, param2);
    
    // 处理结果
    if (result == EXPECTED_RESULT) {
        return 0;  // 成功
    }
    return -1;  // 失败
}
""",
            
            "wasm_binding_with_string": """
/* 返回字符串的绑定 - 调用者负责释放内存 */
EMSCRIPTEN_KEEPALIVE
char* web_get_info(void* graph) {
    if (!graph) return NULL;
    
    ConstraintGraph* g = (ConstraintGraph*)graph;
    
    // 构建字符串
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "Nodes: %d, Constraints: %d", 
             g->node_count, g->constraint_count);
    
    return strdup(buffer);  // 必须用strdup，JS端会用free释放
}
""",
            
            "wasm_binding_with_array": """
/* 返回结构体数组的绑定 */
typedef struct {
    int id;
    int type;
    double x;
    double y;
} WebPointInfo;

EMSCRIPTEN_KEEPALIVE
int web_get_points(void* graph, WebPointInfo* out_points, int max_points) {
    if (!graph || !out_points || max_points <= 0) return 0;
    
    ConstraintGraph* g = (ConstraintGraph*)graph;
    int count = 0;
    
    // 遍历节点
    for (int i = 0; i < g->node_count && count < max_points; i++) {
        GeomNode* node = g->nodes[i];
        if (node && node->type == GEOM_POINT) {
            out_points[count].id = node->id;
            out_points[count].type = node->type;
            // 转换为显示坐标
            out_points[count].x = symbolic_coord_to_double(node->symbolic_coords[0]);
            out_points[count].y = symbolic_coord_to_double(node->symbolic_coords[1]);
            count++;
        }
    }
    
    return count;
}
""",
            
            # JavaScript Canvas渲染模式
            "canvas_renderer_class": """
class GeometryRenderer {
    constructor(canvas) {
        this.canvas = canvas;
        this.ctx = canvas.getContext('2d');
        this.points = [];
        this.segments = [];
        this.selected = null;
        this.zoom = 1.0;
        this.offset = { x: 0, y: 0 };
    }
    
    // 世界坐标转屏幕坐标
    worldToScreen(wx, wy) {
        return {
            x: (wx * this.zoom) + this.offset.x,
            y: (wy * this.zoom) + this.offset.y
        };
    }
    
    // 屏幕坐标转世界坐标
    screenToWorld(sx, sy) {
        return {
            x: (sx - this.offset.x) / this.zoom,
            y: (sy - this.offset.y) / this.zoom
        };
    }
    
    clear() {
        this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
    }
    
    drawGrid() {
        // 绘制网格背景
        this.ctx.strokeStyle = '#1a1a1a';
        this.ctx.lineWidth = 1;
        
        const gridSize = 50 * this.zoom;
        const startX = this.offset.x % gridSize;
        const startY = this.offset.y % gridSize;
        
        for (let x = startX; x < this.canvas.width; x += gridSize) {
            this.ctx.beginPath();
            this.ctx.moveTo(x, 0);
            this.ctx.lineTo(x, this.canvas.height);
            this.ctx.stroke();
        }
        
        for (let y = startY; y < this.canvas.height; y += gridSize) {
            this.ctx.beginPath();
            this.ctx.moveTo(0, y);
            this.ctx.lineTo(this.canvas.width, y);
            this.ctx.stroke();
        }
    }
    
    drawPoint(id, x, y, selected = false) {
        const pos = this.worldToScreen(x, y);
        
        this.ctx.beginPath();
        this.ctx.arc(pos.x, pos.y, selected ? 8 : 6, 0, Math.PI * 2);
        this.ctx.fillStyle = selected ? '#4caf50' : '#888';
        this.ctx.fill();
        this.ctx.strokeStyle = '#333';
        this.ctx.lineWidth = 2;
        this.ctx.stroke();
        
        // 绘制标签
        this.ctx.fillStyle = '#aaa';
        this.ctx.font = '10px monospace';
        this.ctx.fillText('P' + id, pos.x + 10, pos.y + 4);
    }
    
    drawSegment(id, p1x, p1y, p2x, p2y, selected = false) {
        const s1 = this.worldToScreen(p1x, p1y);
        const s2 = this.worldToScreen(p2x, p2y);
        
        this.ctx.beginPath();
        this.ctx.moveTo(s1.x, s1.y);
        this.ctx.lineTo(s2.x, s2.y);
        this.ctx.strokeStyle = selected ? '#4caf50' : '#666';
        this.ctx.lineWidth = selected ? 3 : 2;
        this.ctx.stroke();
    }
    
    render() {
        this.clear();
        this.drawGrid();
        
        // 先绘制线段（在点下面）
        this.segments.forEach(seg => {
            const p1 = this.points.find(p => p.id === seg.p1);
            const p2 = this.points.find(p => p.id === seg.p2);
            if (p1 && p2) {
                this.drawSegment(seg.id, p1.x, p1.y, p2.x, p2.y);
            }
        });
        
        // 再绘制点（在最上层）
        this.points.forEach(p => {
            this.drawPoint(p.id, p.x, p.y, p.id === this.selected);
        });
    }
    
    resize() {
        const rect = this.canvas.parentElement.getBoundingClientRect();
        this.canvas.width = rect.width;
        this.canvas.height = rect.height;
        this.render();
    }
}
""",
            
            "canvas_event_handlers": """
class CanvasInteraction {
    constructor(canvas, renderer, api) {
        this.canvas = canvas;
        this.renderer = renderer;
        this.api = api;
        this.tool = 'select';
        this.pendingAction = null;
        
        this.setupEventListeners();
    }
    
    setupEventListeners() {
        // 鼠标按下
        this.canvas.addEventListener('mousedown', (e) => this.onMouseDown(e));
        
        // 鼠标移动
        this.canvas.addEventListener('mousemove', (e) => this.onMouseMove(e));
        
        // 鼠标松开
        this.canvas.addEventListener('mouseup', (e) => this.onMouseUp(e));
        
        // 右键菜单
        this.canvas.addEventListener('contextmenu', (e) => {
            e.preventDefault();
            this.onContextMenu(e);
        });
        
        // 滚轮缩放
        this.canvas.addEventListener('wheel', (e) => {
            e.preventDefault();
            this.onWheel(e);
        });
    }
    
    getWorldCoords(e) {
        const rect = this.canvas.getBoundingClientRect();
        const sx = e.clientX - rect.left;
        const sy = e.clientY - rect.top;
        return this.renderer.screenToWorld(sx, sy);
    }
    
    findNearestPoint(wx, wy, threshold = 20) {
        let nearest = null;
        let minDist = threshold;
        
        this.renderer.points.forEach(p => {
            const dist = Math.sqrt((p.x - wx) ** 2 + (p.y - wy) ** 2);
            if (dist < minDist) {
                minDist = dist;
                nearest = p;
            }
        });
        
        return nearest;
    }
    
    async onMouseDown(e) {
        const world = this.getWorldCoords(e);
        
        switch (this.tool) {
            case 'select':
                const point = this.findNearestPoint(world.x, world.y);
                if (point) {
                    this.renderer.selected = point.id;
                    this.renderer.render();
                }
                break;
                
            case 'point':
                // 添加新点
                await this.addPoint(world.x, world.y);
                break;
                
            case 'segment':
                // 开始绘制线段
                const start = this.findNearestPoint(world.x, world.y);
                if (start) {
                    this.pendingAction = { type: 'segment', start: start.id };
                }
                break;
                
            case 'pan':
                this.panStart = { x: e.clientX, y: e.clientY };
                break;
        }
    }
    
    async addPoint(x, y) {
        try {
            const pointId = await this.api.addPoint(x, y);
            this.renderer.points.push({ id: pointId, x, y });
            this.renderer.render();
            console.log(`Added point ${pointId} at (${x}, ${y})`);
        } catch (err) {
            console.error('Failed to add point:', err);
        }
    }
    
    async addSegment(p1, p2) {
        try {
            const segId = await this.api.addSegment(p1, p2);
            this.renderer.segments.push({ id: segId, p1, p2 });
            this.renderer.render();
            console.log(`Added segment ${segId}: P${p1} -> P${p2}`);
        } catch (err) {
            console.error('Failed to add segment:', err);
        }
    }
    
    onWheel(e) {
        const delta = e.deltaY > 0 ? 0.9 : 1.1;
        const world = this.getWorldCoords(e);
        
        // 以鼠标位置为中心缩放
        this.renderer.zoom *= delta;
        this.renderer.offset.x = e.clientX - world.x * this.renderer.zoom;
        this.renderer.offset.y = e.clientY - world.y * this.renderer.zoom;
        
        this.renderer.render();
    }
}
""",
            
            # 信任颜色系统
            "trust_colors": """
/* Lv-00 信任颜色系统 - UI实现 */
const TrustColors = {
    GREEN: '#4caf50',           // TRUST_GREEN - 全构造
    BLUE_UNEXPLORED: '#2196f3', // 未探索
    BLUE_RESOURCE: '#64b5f6',   // 资源受限
    BLUE_RANGE: '#90caf9',      // 超出范围
    GREEN_VERIFIED: '#66bb6a',  // 已证不可构造
    YELLOW: '#ffeb3b',          // 条件性不可构造
    LIGHT_ORANGE: '#ff9800',    // 非构造性oracle
    ORANGE_ORACLE: '#ffb74d',   // 爆炸原理
    AMBER: '#ffc107',           // 数值假设
    DARK_ORANGE: '#ff5722',     // 非构造性+数值假设
};

function getNodeColor(node) {
    switch (node.trustColor) {
        case 0: return TrustColors.GREEN;
        case 1: return TrustColors.BLUE_UNEXPLORED;
        case 2: return TrustColors.BLUE_RESOURCE;
        case 3: return TrustColors.BLUE_RANGE;
        case 4: return TrustColors.GREEN_VERIFIED;
        case 5: return TrustColors.YELLOW;
        case 6: return TrustColors.LIGHT_ORANGE;
        case 7: return TrustColors.ORANGE_ORACLE;
        case 8: return TrustColors.AMBER;
        case 9: return TrustColors.DARK_ORANGE;
        default: return '#9e9e9e';
    }
}
""",
            
            # 模块切换UI
            "module_tab_ui": """
/* Lv-00 模块标签切换逻辑 */
const modules = ['graph', 'block', 'proof', 'type', 'recurse', 'engine', 'debug'];

function switchModule(moduleName) {
    // 更新标签状态
    document.querySelectorAll('.module-tab').forEach(tab => {
        if (tab.dataset.module === moduleName) {
            tab.classList.add('active');
        } else {
            tab.classList.remove('active');
        }
    });
    
    // 更新面板显示
    document.querySelectorAll('.module-panel').forEach(panel => {
        const panelName = panel.id.replace('panel', '').toLowerCase();
        if (panelName === moduleName) {
            panel.classList.remove('panel-hidden');
        } else {
            panel.classList.add('panel-hidden');
        }
    });
    
    // 更新状态栏
    updateStatus(`MODULE: ${moduleName.toUpperCase()}`);
}

// 初始化标签点击事件
document.querySelectorAll('.module-tab').forEach(tab => {
    tab.addEventListener('click', () => {
        switchModule(tab.dataset.module);
    });
});
"""
        }
    
    def _init_ui_conventions(self) -> dict[str, str]:
        """初始化UI设计规范"""
        return {
            "color_scheme": """
Lv-00 UI 配色方案（深色主题）：
- 背景色: #0a0a0a (主背景)
- 面板色: #111111 (侧边栏)
- 边框色: #222222 (分隔线)
- 文字主色: #c8c8c8 (主要文字)
- 文字次色: #666666 (次要文字)
- 强调色: #4caf50 (成功/选中)
- 警告色: #ff9800 (警告/冲突)
- 错误色: #f44336 (错误)
- 按钮悬停: #1a1a1a
- 输入框背景: #0d0d0d
""",
            "z_index_layers": """
Lv-00 z-index 层级规范：
- 1: canvas grid/axes (网格/坐标轴)
- 5: selection rect (选择框)
- 10: toolbar, search bar (工具栏, 搜索栏)
- 50: context menu (右键菜单)
- 100: tooltip (提示框)
- 200: modal overlay (模态遮罩)
- 1000: modal content (模态内容)
""",
            "panel_layout": """
Lv-00 面板布局规范：
- 左侧边栏: 260px, min-width: 260px
- 右侧边栏: 260px, min-width: 260px
- 主区域: flex: 1, 自适应
- 面板内边距: 12px 14px
- 面板间距: 1px (border-bottom)
- 按钮高度: 约30px, padding: 6px 10px
""",
            "event_naming": """
Lv-00 事件命名规范：
- 按钮ID: btn{模块}{操作} (如 btnGraphAddPoint)
- 输入ID: input{模块}{字段} (如 inputPointX)
- 工具ID: tool{名称} (如 toolSelect)
- 标签ID: panel{模块} (如 panelGraph)
- 回调前缀: on{事件} (如 onMouseDown)
"""
        }
    
    def _init_common_tasks(self) -> list[dict[str, Any]]:
        """初始化常见编程任务模板"""
        return [
            {
                "task": "添加新的WebAssembly绑定",
                "description": "为C内核函数创建JavaScript可调用的绑定",
                "steps": [
                    "在lv00_web_bindings.c中编写EMSCRIPTEN_KEEPALIVE函数",
                    "处理参数类型转换",
                    "处理返回值",
                    "如果是字符串，用strdup分配内存",
                    "如果是数组，定义结构体并填充",
                    "在JavaScript端添加对应的ccall包装函数",
                    "添加错误处理和边界检查",
                    "编写测试代码验证绑定"
                ],
                "template_key": "wasm_binding_basic"
            },
            {
                "task": "实现Canvas几何渲染",
                "description": "为几何对象创建可视化渲染器",
                "steps": [
                    "创建Renderer类，初始化canvas和context",
                    "实现坐标系转换(worldToScreen/screenToWorld)",
                    "实现清屏和网格绘制",
                    "实现点/线段/区域的绘制方法",
                    "处理选中状态的高亮显示",
                    "处理缩放和平移变换",
                    "实现render()主循环",
                    "处理resize事件"
                ],
                "template_key": "canvas_renderer_class"
            },
            {
                "task": "添加Canvas交互功能",
                "description": "为画布添加鼠标/键盘交互",
                "steps": [
                    "创建Interaction类，保存renderer和api引用",
                    "实现事件监听器设置",
                    "实现坐标转换辅助方法",
                    "实现工具切换逻辑(select/point/segment/pan)",
                    "实现mousedown/mousemove/mouseup处理",
                    "实现右键菜单",
                    "实现滚轮缩放和拖拽平移",
                    "调用API执行相应操作"
                ],
                "template_key": "canvas_event_handlers"
            },
            {
                "task": "添加新的约束类型UI",
                "description": "在界面中添加新约束类型的操作按钮和处理逻辑",
                "steps": [
                    "在HTML中添加按钮(btn{id}Constraint)",
                    "添加按钮的点击事件监听器",
                    "实现约束参数的输入界面",
                    "收集参数并调用API",
                    "处理返回结果",
                    "更新UI状态",
                    "添加错误提示"
                ]
            },
            {
                "task": "实现证明导航器界面",
                "description": "创建证明步骤的浏览和导航UI",
                "steps": [
                    "创建证明步骤列表数据结构",
                    "实现步骤导航(prev/next/jump)",
                    "高亮当前步骤",
                    "显示步骤依赖关系",
                    "实现展开/折叠功能",
                    "添加导出功能(HTML/LaTeX/Coq)"
                ]
            }
        ]


class Lv00PromptEngine:
    """
    Lv-00专用提示词引擎
    生成针对项目上下文优化的AI提示词
    """
    
    def __init__(self, knowledge_base: Lv00KnowledgeBase):
        self.kb = knowledge_base
    
    def generate_wasm_binding_prompt(self, c_function: str, description: str) -> str:
        """生成WebAssembly绑定的提示词"""
        api_info = self.kb.api_signatures.get(c_function, {})
        
        return f"""
为Lv-00项目生成WebAssembly绑定代码。

目标C函数: {c_function}
函数签名: {api_info.get('signature', '未知')}
返回值: {api_info.get('returns', '未知')}
注意事项: {api_info.get('notes', '无')}

功能描述: {description}

要求:
1. 使用EMSCRIPTEN_KEEPALIVE标记导出函数
2. 函数名前缀为'web_'
3. 处理空指针检查
4. 如果返回字符串，必须用strdup分配内存
5. 如果返回数组，需要定义结构体并返回长度
6. 包含适当的错误处理
7. 遵循Lv-00的代码风格(4空格缩进)
"""
    
    def generate_canvas_renderer_prompt(self, element_type: str, features: list[str]) -> str:
        """生成Canvas渲染器的提示词"""
        return f"""
为Lv-00几何画布生成渲染代码。

需要渲染的元素: {element_type}
功能要求: {', '.join(features)}

代码规范:
1. 使用ES6 class语法
2. 支持世界坐标到屏幕坐标的转换
3. 支持缩放(zoom)和平移(offset)
4. 实现选中高亮
5. 遵循Lv-00的UI配色方案
6. 使用requestAnimationFrame进行高效渲染

参考代码模式:
{self.kb.code_patterns.get('canvas_renderer_class', '')}

请生成完整的渲染代码实现。
"""
    
    def generate_ui_component_prompt(self, component_type: str, module: str) -> str:
        """生成UI组件的提示词"""
        return f"""
为Lv-00 {module.upper()} 模块生成{component_type}组件。

模块: {module}
组件类型: {component_type}

设计规范:
{self.kb.ui_conventions.get('color_scheme', '')}
{self.kb.ui_conventions.get('panel_layout', '')}

命名规范:
{self.kb.ui_conventions.get('event_naming', '')}

要求:
1. 遵循深色主题配色
2. 使用panel/panel-title/btn等标准类名
3. 中英双语标签
4. 适当的交互反馈(hover/active状态)
"""
    
    def generate_coding_task_prompt(self, task_description: str) -> str:
        """
        生成编程任务的提示词
        使用三级匹配算法查找最相关的任务模板：
          1. 完全匹配：任务描述与模板名称完全一致
          2. 前缀匹配：任务描述以模板名称开头
          3. 子串匹配：任务描述包含模板名称中的某个关键词
        """
        desc_lower = task_description.lower().strip()

        # 三级匹配：完全匹配 > 前缀匹配 > 子串匹配
        matched_task = None
        best_match_type = -1  # 匹配等级：0=完全, 1=前缀, 2=子串
        best_match_score = 0  # 同等级内按匹配长度排序，越长越精确

        for task in self.kb.common_tasks:
            task_name = task['task'].lower()
            # 将任务名称拆分为关键词列表
            keywords = task_name.split()

            # 第一级：完全匹配
            if desc_lower == task_name and best_match_type > 0:
                matched_task = task
                best_match_type = 0
                best_match_score = len(task_name)
                break  # 完全匹配直接使用，无需继续查找

            # 第二级：前缀匹配（描述以任务名称开头）
            if best_match_type >= 1 and desc_lower.startswith(task_name):
                score = len(task_name)
                if best_match_type > 1 or score > best_match_score:
                    matched_task = task
                    best_match_type = 1
                    best_match_score = score

            # 第三级：子串匹配（描述包含任务名称中的关键词）
            if best_match_type >= 2:
                for keyword in keywords:
                    if keyword in desc_lower:
                        score = len(keyword)
                        if best_match_type > 2 or score > best_match_score:
                            matched_task = task
                            best_match_type = 2
                            best_match_score = score
                        break  # 该任务已匹配到一个关键词，跳过剩余关键词
        
        base_prompt = f"""
Lv-00 UI编程任务:

{task_description}

项目上下文:
- Lv-00是一个几何元语言系统(C语言实现)
- UI使用HTML/CSS/JavaScript，基于Canvas 2D
- Web层通过WebAssembly与C内核通信
- 使用Emscripten进行编译
"""
        
        if matched_task:
            base_prompt += f"""
参考任务模板: {matched_task['task']}

步骤指南:
{chr(10).join(f"{i+1}. {step}" for i, step in enumerate(matched_task['steps']))}
"""
        
        base_prompt += """
请提供:
1. 详细的实现方案
2. 关键代码片段
3. 可能的陷阱和注意事项
4. 测试建议
"""
        
        return base_prompt


# ============================================
# 模块级缓存，避免便捷函数重复创建实例
# 缓存有效期1小时，超时自动失效
# 使用 threading.Lock 保证多线程环境下的安全访问
# ============================================
_cached_helper: Optional[tuple[Lv00KnowledgeBase, Lv00PromptEngine]] = None
# 缓存创建时间戳，用于判断是否过期（Unix 纪元秒数）
_cache_created_at: float = 0.0
# 缓存有效期（秒）：1小时
_CACHE_TTL_SECONDS: int = 3600
# 缓存访问锁，保护 _cached_helper 和 _cache_created_at 的并发读写
_cache_lock: threading.Lock = threading.Lock()


def invalidate_cache() -> None:
    """
    手动使缓存失效
    在知识库数据更新后调用此函数，确保下次获取辅助系统时重新创建实例。
    线程安全：使用 _cache_lock 保护缓存变量的并发访问。
    """
    global _cached_helper, _cache_created_at
    with _cache_lock:
        _cached_helper = None
        _cache_created_at = 0.0
    logger.info("Lv-00 辅助系统缓存已手动失效")


def _is_cache_valid() -> bool:
    """检查缓存是否仍然有效（未过期），线程安全"""
    with _cache_lock:
        return _is_cache_valid_unlocked()


def _is_cache_valid_unlocked() -> bool:
    """检查缓存是否仍然有效（未过期），不加锁版本。

    仅在已持有 _cache_lock 的上下文中调用，避免重复获取锁导致死锁。
    """
    if _cached_helper is None:
        return False
    # 检查缓存是否超过有效期
    if time.time() - _cache_created_at > _CACHE_TTL_SECONDS:
        logger.info("Lv-00 辅助系统缓存已过期（超过 %d 秒），将重新创建", _CACHE_TTL_SECONDS)
        return False
    return True


def get_lv00_helper() -> tuple[Lv00KnowledgeBase, Lv00PromptEngine]:
    """
    获取Lv-00编程辅助系统的核心组件（带缓存）

    缓存有效期为1小时，超时后自动重新创建实例。
    也可通过 invalidate_cache() 手动使缓存失效。
    线程安全：使用 _cache_lock 保护缓存变量的并发读写，
    防止多线程同时创建实例或读取到不一致的状态。

    Returns:
        (Lv00KnowledgeBase, Lv00PromptEngine) 元组
    """
    global _cached_helper, _cache_created_at
    # 先在锁外做快速检查（无锁读），避免每次调用都获取锁
    if _is_cache_valid():
        return _cached_helper

    # 缓存无效，需要创建新实例，获取锁以保证只有一个线程执行创建
    with _cache_lock:
        # 双重检查：获取锁后再次验证，防止等待锁期间其他线程已完成创建
        if _is_cache_valid_unlocked():
            return _cached_helper
        try:
            kb = Lv00KnowledgeBase()
            pe = Lv00PromptEngine(kb)
            _cached_helper = (kb, pe)
            _cache_created_at = time.time()
            logger.info("Lv-00 辅助系统实例已创建/刷新")
        except Exception as e:
            logger.error("初始化 Lv-00 辅助系统失败: %s", e)
            raise
    return _cached_helper


def generate_binding_help(function_name: str, description: str) -> str:
    """生成绑定代码的帮助信息"""
    kb, pe = get_lv00_helper()
    return pe.generate_wasm_binding_prompt(function_name, description)


def generate_renderer_help(element: str, features: list[str]) -> str:
    """生成渲染器代码的帮助信息"""
    kb, pe = get_lv00_helper()
    return pe.generate_canvas_renderer_prompt(element, features)


def generate_task_help(task: str) -> str:
    """生成任务帮助信息"""
    kb, pe = get_lv00_helper()
    return pe.generate_coding_task_prompt(task)


# ============================================
# 概念解释数据（从 main.py 提取）
# ============================================

CONCEPT_EXPLANATIONS: dict[str, str] = {
    "normalization": """
【图归一化 (Graph Normalization)】

归一化是Lv-00保证幂等性的核心机制。

工作流程:
1. 点合并: 合并坐标相同的点
   - 使用坐标哈希分组
   - 精确coord_equal()判等
   - 处理作用域冲突

2. 线段/区域合并: 合并端点相同的几何体

3. 稳定化: 拓扑排序固定顺序

关键API:
- graph_normalize(g, interactive)
  - interactive=true 时跨作用域合并需确认
  - 返回 NormalizationResult

注意事项:
- 归一化后图再次归一化不会变化(幂等性)
- 合并日志记录用于证明导航器回放
""",

    "unification": """
【合一检查 (Unification)】

合一检查是证明系统的核心。

执行流程:
1. 对构造图和命题图各自归一化
2. 展开命题中的模板为正则形式
3. 三层匹配:
   - 端口类型匹配
   - 约束类型匹配
   - 符号坐标精确匹配

关键API:
- proof_unify(construction, proposition, strict)

返回值:
- UNIFY_OK: 合一成功
- UNIFY_MISMATCH: 匹配失败
- UNIFY_INCOMPLETE: 部分匹配

严格边界:
- 不调用求解器判定语义等价
- 仅比较结构
""",

    "proof": """
【证明系统 (Proof System)】

命题结构:
- 输入/输出端口 (声明期望的证物)
- 虚线框几何模式 (等待填充)
- 前置/后置条件 (可选)

证明步骤:
1. 创建命题 (proposition_create)
2. 设置模式图 (proposition_set_pattern)
3. 执行构造
4. 合一检查 (proof_unify)
5. 成功则命题得证

信任颜色:
- 绿色: 全构造
- 蓝色: 待完成
- 黄色: 条件性不可构造
- 橙色: 非构造性依赖

关键API:
- proof_create_proposition()
- proof_unify()
- proof_step_forward()
- proof_step_backward()
""",

    "func_block": """
【函数块 (Function Block)】

函数块封装内部约束子图为可复用单元。

生命周期:
1. 打包 (Pack): 将子图封装为函数块
2. 例化 (Instantiate): 创建函数块实例
3. beta-归约: 应用参数到形式参数

打包要求:
- 必须处理跨边界约束冲突
- 端口标记 (namespace_depth, parent_block_id)
- 变量捕获消解

确定性:
- VERIFIED: 静态分析确认唯一解
- PARTIALLY_VERIFIED: 未发现冲突
- NON_DETERMINISTIC: 出现多解

组合子:
- Compose: f*g 组合
- Product: f*xg 乘积

关键API:
- func_block_pack()
- func_block_instantiate()
- func_block_compose()
""",

    "trust": """
【信任颜色系统】

Lv-00使用颜色编码构造的可靠性:

TRUST_GREEN (0)
   全构造，无任何非常规依赖

TRUST_BLUE (1-3)
   - 未探索 (UNEXPLORED)
   - 资源受限 (RESOURCE)
   - 超出范围 (OUT_OF_RANGE)

TRUST_GREEN_VERIFIED (4)
   已证不可构造

TRUST_YELLOW (5)
   条件性不可构造

TRUST_ORANGE (6-7)
   - 非构造性oracle
   - 爆炸原理 (ex falso)

TRUST_AMBER (8)
   含数值假设

TRUST_DARK_ORANGE (9)
   非构造性+数值假设
"""
}


# ============================================
# 代码生成指导文本（从 main.py 提取）
# ============================================

CODE_GUIDANCE: dict[str, str] = {
    "binding": """
【生成WebAssembly绑定代码】

请提供以下信息:
1. C函数名 (例如: graph_add_circle)
2. 函数描述
3. 参数列表

我将生成:
- C绑定代码 (EMSCRIPTEN_KEEPALIVE)
- JavaScript包装器
- 使用示例

提示: 使用 'api graph' 查看可用API
""",
    "renderer": """
【生成Canvas渲染器代码】

提供:
1. 渲染元素 (point/segment/region/mixed)
2. 特殊功能 (选中高亮/缩放/拖拽)

我将生成:
- GeometryRenderer类
- 坐标系转换
- 渲染方法
- 事件绑定
""",
    "interaction": """
【生成交互处理器代码】

提供:
1. 交互模式 (select/construct/analyze)
2. 工具列表
3. 特殊操作

我将生成:
- CanvasInteraction类
- 事件处理器
- 工具切换逻辑
- 撤销/重做支持
""",
    "panel": """
【生成UI面板代码】

提供:
1. 模块名称 (graph/block/proof/type/recurse/engine/debug)
2. 面板功能
3. 按钮列表

我将生成:
- HTML面板结构
- CSS样式
- JavaScript事件绑定
- API调用逻辑
""",
}


# 导出所有公共接口
__all__ = [
    'Lv00Module',
    'Lv00KnowledgeBase',
    'Lv00PromptEngine',
    'get_lv00_helper',
    'invalidate_cache',
    'generate_binding_help',
    'generate_renderer_help',
    'generate_task_help',
    'CONCEPT_EXPLANATIONS',
    'CODE_GUIDANCE',
]
