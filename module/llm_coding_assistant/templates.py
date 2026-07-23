"""
Lv-00 UI编程辅助系统 - 代码模板库
包含常用的代码模板和片段

本文件面向用户直接查询和代码生成，模板以结构化字典形式组织。
与 lv_knowledge.py 的关系：
  - lv_knowledge.py 是模板和知识的权威来源（single source of truth），
    其中的代码模式面向 LLM 上下文注入
  - 本文件仅保留 templates.py 独有的 UI 组件模板

已废弃的模板（已迁移到 lv_knowledge.py，不再在本文件中保留数据）：
  - wasm_basic_binding      -> lvKnowledgeBase.code_patterns["wasm_binding_basic"]
  - wasm_string_binding     -> lvKnowledgeBase.code_patterns["wasm_binding_with_string"]
  - wasm_array_binding      -> lvKnowledgeBase.code_patterns["wasm_binding_with_array"]
  - wasm_js_wrapper         -> lvKnowledgeBase.code_patterns["wasm_binding_basic"] (JS端)
  - canvas_basic_renderer   -> lvKnowledgeBase.code_patterns["canvas_renderer_class"]
  - canvas_event_handler    -> lvKnowledgeBase.code_patterns["canvas_event_handlers"]

独有模板（仅在本文件中维护）：
  - module_panel: 模块面板模板
  - modal_dialog: 模态对话框模板
"""
from __future__ import annotations

import logging
import warnings

# 模块级日志
logger = logging.getLogger(__name__)

# 已废弃的模板名称集合，用于 get_template() 中检测并发出警告
_DEPRECATED_TEMPLATE_NAMES = frozenset({
    "wasm_basic_binding",
    "wasm_string_binding",
    "wasm_array_binding",
    "wasm_js_wrapper",
    "canvas_basic_renderer",
    "canvas_event_handler",
})

CODE_TEMPLATES = {
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
            const result = await window.lvApi.{api_method}(param);
            console.log('{module} action1 result:', result);
            showStatus('{MODULE_TITLE}: 操作完成');
        }} catch (err) {{
            showError('{module} error:', err);
        }}
    }});

    // 操作2
    document.getElementById('btn{module}Action2')?.addEventListener('click', async function() {{
        try {{
            const result = await window.lvApi.{api_method2}();
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

    对于已废弃的模板（已迁移到 lv_knowledge.py），返回空字符串并记录
    deprecation warning。

    Args:
        name: 模板名称（对应 CODE_TEMPLATES 的键）

    Returns:
        模板字符串；若模板不存在或已废弃则返回空字符串
    """
    # 检查是否为已废弃的模板名称
    if name in _DEPRECATED_TEMPLATE_NAMES:
        logger.warning(
            "模板 '%s' 已废弃（已迁移到 lv_knowledge.py），"
            "请使用 lvKnowledgeBase.code_patterns 中的权威版本",
            name,
        )
        warnings.warn(
            f"模板 '{name}' 已废弃，请使用 lv_knowledge.py 中的权威版本",
            DeprecationWarning,
            stacklevel=2,
        )
        return ""

    entry = CODE_TEMPLATES.get(name, {})
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
