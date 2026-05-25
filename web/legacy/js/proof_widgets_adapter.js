/**
 * @module web/js/proof_widgets_adapter
 * @description Lv-00 Web GUI 证明可视化组件适配器
 *
 *              借鉴 ProofWidgets4（proofwidgets.org）的证明面板组件设计，
 *              为 Lv-00 的 Web GUI（web-gui/）和传统 Web 前端（web/）
 *              提供统一的证明可视化组件。
 *
 *  借鉴项目： ProofWidgets4 (proofwidgets.org)
 *  核心借鉴点： 证明目标树渲染、步骤高亮、前提选择器
 *  分类：       P4 低优先级 / 前端证明可视化增强
 *  日期：       2026-05-24
 *
 *  基于现有组件：
 *    - web-gui/src/components/panels/ProofPanel.tsx（React/TypeScript）
 *    - web/js/modules/proof.js（vanilla JS 证明模块）
 *    - include/lv00/proof.h（C 层证明系统 API）
 *
 *  设计目标：
 *    1. 提供独立于框架的证明可视化核心逻辑
 *    2. 在 React ProofPanel 和 vanilla JS proof.js 之间共享
 *    3. 支持证明目标树渲染、步骤高亮、前提选择器三大子组件
 *    4. 基于 proof.h 中已定义的数据结构（ProofStep, ProofNavigator, BacktrackNode 等）
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
 * @typedef {Object} ProofWidgetsConfig
 * @property {boolean} enableGoalTree     - 是否启用证明目标树
 * @property {boolean} enableStepHighlight- 是否启用步骤高亮
 * @property {boolean} enablePremiseSelector - 是否启用前提选择器
 * @property {number}  maxTreeDepth       - 目标树最大渲染深度（默认 6）
 * @property {string}  colorScheme        - 颜色方案："trust"（信任颜色）或 "status"（状态颜色）
 * @property {boolean} showProofStrategy  - 是否显示证明策略注释
 * @property {boolean} showBacktrackTree  - 是否显示回溯搜索树
 * @property {boolean} autoScrollToActive - 自动滚动到当前活跃步骤
 */

/** @type {ProofWidgetsConfig} */
const DEFAULT_PROOF_WIDGETS_CONFIG = Object.freeze({
    enableGoalTree: true,
    enableStepHighlight: true,
    enablePremiseSelector: true,
    maxTreeDepth: 6,
    colorScheme: 'trust',
    showProofStrategy: true,
    showBacktrackTree: true,
    autoScrollToActive: true,
});

/**
 * 证明状态颜色映射——基于 proof.h 中的 ProofColor 枚举
 * @enum {string}
 */
const PROOF_COLORS = Object.freeze({
    GREEN:               '#51cf66',  // 全构造，无非常规依赖
    BLUE_UNEXPLORED:     '#4dabf7',  // 蓝色（未探索）
    BLUE_RESOURCE:       '#74c0fc',  // 蓝色（资源受限）
    BLUE_OUT_OF_RANGE:   '#a5d8ff',  // 蓝色（超出范围）
    GREEN_VERIFIED:      '#40c057',  // 绿色实框：已证不可构造
    YELLOW:              '#ffd43b',  // 黄色虚线框：条件性不可构造
    ORANGE_ORACLE:       '#ff922b',  // 浅橙色实心端口：依赖非构造性 oracle
    ORANGE_EX_FALSO:     '#f76707',  // 浅橙色虚线箭头：爆炸原理步骤
    AMBER:               '#fab005',  // 橙黄色：含数值假设
    DARK_ORANGE:         '#e8590c',  // 深橙色：非构造性依赖与数值假设叠加
});

/**
 * 证明步骤类型图标——基于 proof.h 中的 ProofStepType 枚举
 * @enum {string}
 */
const PROOF_STEP_ICONS = Object.freeze({
    ADD_NODE:       '\u2795',  // ➕ 添加节点
    ADD_CONSTRAINT: '\u26D4',  // ⛔ 添加约束
    REWRITE:        '\u27F3',  // ⟳ 重写步骤
    FUNCTION_APP:   '\u25B3',  // △ 函数应用
    PACK_FUNCTION:  '\u25A1',  // □ 打包函数块
    NORMALIZATION:  '\u2699',  // ⚙ 自动规范化
    UNIFY:          '\u2248',  // ≈ 合一检查
    EX_FALSO:       '\u22A5',  // ⊥ 爆炸原理
    ORACLE:         '\u2606',  // ☆ Oracle 依赖
});

/**
 * =========================================================================
 * 第二部分：证明目标树渲染器（Goal Tree Renderer）
 * =========================================================================
 */

/**
 * @class ProofGoalTreeRenderer
 * @description 证明目标树渲染器
 *
 *              借鉴 ProofWidgets4 的目标树设计：
 *              - 根节点 = 最终证明目标
 *              - 内部节点 = 子目标（通过反向链分解）
 *              - 叶子节点 = 已证/假设/待证的前提
 *              - 每个节点显示信任颜色和证明状态
 *
 *              与 ProofPanel.tsx 的集成方式：
 *              在 ProofPanel 中新增一个子面板区域，
 *              通过本类的 renderToDOM() 方法输出 DOM 元素。
 */
class ProofGoalTreeRenderer {
    /**
     * @param {ProofWidgetsConfig} [config] - 渲染配置
     */
    constructor(config = {}) {
        /** @type {ProofWidgetsConfig} */
        this.config = Object.assign({}, DEFAULT_PROOF_WIDGETS_CONFIG, config);

        /** @type {Object|null} 当前证明导航器数据 */
        this.navigatorData = null;

        /** @type {HTMLElement|null} 渲染目标 DOM 容器 */
        this.container = null;

        /** @type {Map<number, HTMLElement>} 节点 ID 到 DOM 元素的映射 */
        this.nodeElementMap = new Map();
    }

    /**
     * 设置证明导航器的数据源
     *
     * @param {Object} navData - 证明导航器数据（来自 proof.h 的 ProofNavigator）
     * @param {Array<Object>} navData.steps        - 证明步骤数组
     * @param {number}        navData.currentStep  - 当前步骤索引
     * @param {Object}        navData.targetProp   - 目标命题
     * @param {Array<Object>} navData.dependencies - 依赖树
     * @param {string}        [navData.strategyNote] - 策略注释
     */
    setNavigatorData(navData) {
        this.navigatorData = navData;
        if (this.container) {
            this.render();
        }
    }

    /**
     * 将目标树渲染到指定的 DOM 容器中
     *
     * @param {HTMLElement} container - 目标容器元素
     */
    renderToDOM(container) {
        this.container = container;
        this.render();
    }

    /**
     * 执行完整的渲染流程
     * @private
     */
    render() {
        if (!this.container || !this.navigatorData) {
            return;
        }

        this.container.innerHTML = '';
        this.nodeElementMap.clear();

        const treeRoot = this._buildGoalTree();
        if (treeRoot) {
            const rootEl = this._renderGoalNode(treeRoot, 0);
            this.container.appendChild(rootEl);
        }

        if (this.config.autoScrollToActive && this.navigatorData.currentStep >= 0) {
            const activeEl = this.nodeElementMap.get(this.navigatorData.currentStep);
            if (activeEl) {
                activeEl.scrollIntoView({ behavior: 'smooth', block: 'center' });
            }
        }
    }

    /**
     * 从证明步骤数组构建目标树结构
     *
     * 算法：反向遍历证明步骤，将每个步骤作为子目标节点，
     * 根据步骤之间的依赖关系（dependency_step_ids）建立父子关系。
     *
     * @returns {Object|null} 目标树的根节点
     * @private
     */
    _buildGoalTree() {
        const { steps, targetProp, dependencies } = this.navigatorData;
        if (!steps || steps.length === 0) {
            return {
                id: -1,
                label: targetProp ? (targetProp.name || 'Unnamed Goal') : 'No Goal',
                type: 'goal',
                color: 'BLUE_UNEXPLORED',
                status: 'pending',
                children: [],
            };
        }

        // 第 1 步：为每个步骤创建节点
        const nodeMap = new Map();
        for (let i = 0; i < steps.length; i++) {
            const step = steps[i];
            nodeMap.set(step.id, {
                id: step.id,
                label: this._getStepLabel(step),
                type: this._getStepNodeType(step),
                color: step.color || 'GREEN',
                status: step.is_completed ? 'completed' : (i === this.navigatorData.currentStep ? 'active' : 'pending'),
                stepIndex: i,
                stepData: step,
                children: [],
            });
        }

        // 第 2 步：根据依赖关系建立树结构
        // 依赖关系：step.dependency_step_ids → 子节点
        const rootChildren = [];
        const hasParent = new Set();

        for (let i = 0; i < steps.length; i++) {
            const step = steps[i];
            const node = nodeMap.get(step.id);
            if (!node) continue;

            if (step.dependency_step_ids && step.dependency_step_ids.length > 0) {
                for (const depId of step.dependency_step_ids) {
                    const parentNode = nodeMap.get(depId);
                    if (parentNode) {
                        parentNode.children.push(node);
                        hasParent.add(step.id);
                    }
                }
            }

            // 没有依赖的步骤 = 根节点的直接子节点
            if (!step.dependency_step_ids || step.dependency_step_ids.length === 0) {
                rootChildren.push(node);
            }
        }

        // 第 3 步：构建根节点
        return {
            id: -1,
            label: targetProp ? (targetProp.name || 'Target') : 'Proof',
            type: 'goal',
            color: 'GREEN',
            status: this.navigatorData.isComplete ? 'completed' : 'active',
            children: rootChildren,
            strategyNote: this.navigatorData.strategyNote || null,
        };
    }

    /**
     * 获取步骤节点的可读标签
     * @param {Object} step - 证明步骤
     * @returns {string}
     * @private
     */
    _getStepLabel(step) {
        const icon = PROOF_STEP_ICONS[step.type] || '\u25CF'; // ● 默认图标
        const typeName = (step.type || 'UNKNOWN').replace('PROOF_STEP_', '');
        let label = `${icon} ${typeName}`;

        if (step.node_id >= 0) {
            label += ` [node #${step.node_id}]`;
        }
        if (step.constraint_id >= 0) {
            label += ` [cstr #${step.constraint_id}]`;
        }
        if (step.note) {
            label += ` : ${step.note}`;
        }
        return label;
    }

    /**
     * 获取步骤在目标树中的节点类型
     * @param {Object} step - 证明步骤
     * @returns {string} 'goal' | 'lemma' | 'construction' | 'axiom' | 'oracle'
     * @private
     */
    _getStepNodeType(step) {
        switch (step.type) {
            case 'PROOF_STEP_ADD_NODE':
            case 'PROOF_STEP_ADD_CONSTRAINT':
                return 'construction';
            case 'PROOF_STEP_REWRITE':
            case 'PROOF_STEP_NORMALIZATION':
                return 'lemma';
            case 'PROOF_STEP_PACK_FUNCTION':
            case 'PROOF_STEP_FUNCTION_APP':
                return 'goal';
            case 'PROOF_STEP_UNIFY':
                return 'goal';
            case 'PROOF_STEP_ORACLE':
                return 'oracle';
            case 'PROOF_STEP_EX_FALSO':
                return 'axiom';
            default:
                return 'goal';
        }
    }

    /**
     * 递归渲染目标树节点
     * @param {Object} node   - 树节点
     * @param {number} depth  - 当前深度
     * @returns {HTMLElement}
     * @private
     */
    _renderGoalNode(node, depth) {
        if (depth > this.config.maxTreeDepth) {
            const el = document.createElement('div');
            el.className = 'pw-goal-node pw-truncated';
            el.textContent = '...';
            return el;
        }

        const el = document.createElement('div');
        el.className = 'pw-goal-node';
        el.dataset.nodeId = node.id;
        el.dataset.depth = depth;

        const color = PROOF_COLORS[node.color] || PROOF_COLORS.GREEN;

        // 节点头部
        const header = document.createElement('div');
        header.className = 'pw-goal-header';

        // 颜色指示器
        const indicator = document.createElement('span');
        indicator.className = 'pw-goal-indicator';
        indicator.style.backgroundColor = color;
        indicator.style.width = '10px';
        indicator.style.height = '10px';
        indicator.style.borderRadius = '2px';
        indicator.style.display = 'inline-block';
        indicator.style.marginRight = '6px';
        indicator.style.flexShrink = '0';
        header.appendChild(indicator);

        // 标签
        const label = document.createElement('span');
        label.className = 'pw-goal-label';
        label.textContent = node.label;
        label.style.color = 'var(--text, #e0e0e0)';
        label.style.fontSize = '12px';
        header.appendChild(label);

        // 状态标记
        if (node.status === 'active') {
            const activeBadge = document.createElement('span');
            activeBadge.className = 'pw-goal-active-badge';
            activeBadge.textContent = 'CURRENT';
            activeBadge.style.color = '#51cf66';
            activeBadge.style.fontSize = '9px';
            activeBadge.style.marginLeft = '6px';
            activeBadge.style.fontWeight = 'bold';
            header.appendChild(activeBadge);
        }

        el.appendChild(header);

        // 策略注释（仅根节点显示）
        if (node.strategyNote && depth === 0) {
            const strategyEl = document.createElement('div');
            strategyEl.className = 'pw-goal-strategy';
            strategyEl.textContent = `Strategy: ${node.strategyNote}`;
            strategyEl.style.fontSize = '10px';
            strategyEl.style.color = '#868e96';
            strategyEl.style.marginTop = '4px';
            strategyEl.style.fontStyle = 'italic';
            strategyEl.style.paddingLeft = '16px';
            el.appendChild(strategyEl);
        }

        // 子节点
        if (node.children && node.children.length > 0) {
            const childrenContainer = document.createElement('div');
            childrenContainer.className = 'pw-goal-children';
            childrenContainer.style.marginLeft = '20px';
            childrenContainer.style.borderLeft = `1px solid ${color}44`;
            childrenContainer.style.paddingLeft = '10px';
            childrenContainer.style.marginTop = '4px';

            for (const child of node.children) {
                const childEl = this._renderGoalNode(child, depth + 1);
                childEl.style.marginTop = '4px';
                childrenContainer.appendChild(childEl);
            }
            el.appendChild(childrenContainer);
        }

        // 缓存节点
        if (node.id >= 0) {
            this.nodeElementMap.set(node.id, el);
        }

        return el;
    }
}


/**
 * =========================================================================
 * 第三部分：步骤高亮器（Step Highlighter）
 * =========================================================================
 */

/**
 * @class ProofStepHighlighter
 * @description 证明步骤高亮管理器
 *
 *              借鉴 ProofWidgets4 的步骤高亮设计：
 *              - 当前活跃步骤以加粗边框和辉光效果突出
 *              - 已完成的步骤以淡色背景显示
 *              - 待执行的步骤以灰色虚线边框显示
 *              - 支持步骤点击跳转（回调到证明导航器）
 */
class ProofStepHighlighter {
    /**
     * @param {Object} [options]
     * @param {Function} [options.onStepClick] - 步骤被点击时的回调 (stepIndex: number) => void
     */
    constructor(options = {}) {
        /** @type {Function|null} */
        this.onStepClick = options.onStepClick || null;

        /** @type {Array<Object>} 步骤数据 */
        this.steps = [];

        /** @type {number} 当前活跃步骤索引 */
        this.activeStepIndex = -1;

        /** @type {Set<number>} 已完成的步骤索引集合 */
        this.completedSteps = new Set();

        /** @type {Set<number>} 断点步骤索引集合 */
        this.breakpointSteps = new Set();

        /** @type {HTMLElement|null} */
        this.container = null;
    }

    /**
     * 加载证明步骤数据
     *
     * @param {Array<Object>} steps           - 证明步骤数组
     * @param {number}        activeIndex     - 当前活跃步骤索引
     * @param {Array<number>} [breakpoints]   - 断点步骤索引列表
     */
    loadSteps(steps, activeIndex, breakpoints = []) {
        this.steps = steps;
        this.activeStepIndex = activeIndex;

        // 活跃步骤之前的所有步骤视为已完成
        this.completedSteps.clear();
        for (let i = 0; i < activeIndex; i++) {
            this.completedSteps.add(i);
        }

        this.breakpointSteps.clear();
        for (const bp of breakpoints) {
            this.breakpointSteps.add(bp);
        }

        if (this.container) {
            this.render();
        }
    }

    /**
     * 渲染到指定容器
     * @param {HTMLElement} container
     */
    renderToDOM(container) {
        this.container = container;
        this.render();
    }

    /**
     * 渲染步骤列表
     * @private
     */
    render() {
        if (!this.container) return;
        this.container.innerHTML = '';

        for (let i = 0; i < this.steps.length; i++) {
            const step = this.steps[i];
            const stepEl = this._createStepElement(step, i);
            this.container.appendChild(stepEl);
        }
    }

    /**
     * 创建单个步骤的 DOM 元素
     * @param {Object} step     - 步骤数据
     * @param {number} index    - 步骤索引
     * @returns {HTMLElement}
     * @private
     */
    _createStepElement(step, index) {
        const el = document.createElement('div');
        el.className = 'pw-step-item';
        el.dataset.stepIndex = index;

        const isActive = index === this.activeStepIndex;
        const isCompleted = this.completedSteps.has(index);
        const isBreakpoint = this.breakpointSteps.has(index);

        // 样式
        const baseStyle = {
            padding: '6px 10px',
            marginBottom: '4px',
            borderRadius: '4px',
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '8px',
            transition: 'all 0.2s ease',
            fontSize: '11px',
            fontFamily: 'monospace',
        };

        if (isActive) {
            Object.assign(baseStyle, {
                background: '#51cf6622',
                border: '2px solid #51cf66',
                boxShadow: '0 0 8px #51cf6644',
                fontWeight: 'bold',
            });
        } else if (isCompleted) {
            Object.assign(baseStyle, {
                background: '#2b2b2b',
                border: '1px solid #444',
                opacity: '0.7',
            });
        } else {
            Object.assign(baseStyle, {
                background: '#1a1a2e',
                border: '1px dashed #555',
                opacity: '0.6',
            });
        }

        if (isBreakpoint) {
            baseStyle.borderLeft = '3px solid #ffd43b';
        }

        Object.assign(el.style, baseStyle);

        // 步骤编号
        const numSpan = document.createElement('span');
        numSpan.className = 'pw-step-num';
        numSpan.textContent = `#${index + 1}`;
        numSpan.style.color = '#868e96';
        numSpan.style.minWidth = '28px';
        numSpan.style.flexShrink = '0';
        el.appendChild(numSpan);

        // 步骤类型图标
        const iconSpan = document.createElement('span');
        iconSpan.className = 'pw-step-icon';
        iconSpan.textContent = PROOF_STEP_ICONS[step.type] || '\u25CF';
        iconSpan.style.flexShrink = '0';
        el.appendChild(iconSpan);

        // 步骤描述
        const descSpan = document.createElement('span');
        descSpan.className = 'pw-step-desc';
        descSpan.textContent = this._getStepDescription(step);
        descSpan.style.flex = '1';
        descSpan.style.overflow = 'hidden';
        descSpan.style.textOverflow = 'ellipsis';
        descSpan.style.whiteSpace = 'nowrap';
        el.appendChild(descSpan);

        // 信任颜色指示器
        const colorDot = document.createElement('span');
        colorDot.className = 'pw-step-color';
        colorDot.style.width = '8px';
        colorDot.style.height = '8px';
        colorDot.style.borderRadius = '50%';
        colorDot.style.backgroundColor = PROOF_COLORS[step.color] || PROOF_COLORS.GREEN;
        colorDot.style.flexShrink = '0';
        el.appendChild(colorDot);

        // 点击事件
        el.addEventListener('click', () => {
            if (this.onStepClick) {
                this.onStepClick(index);
            }
        });

        // 悬停效果
        el.addEventListener('mouseenter', () => {
            if (!isActive) {
                el.style.opacity = '1';
                el.style.borderColor = '#4dabf7';
            }
        });
        el.addEventListener('mouseleave', () => {
            if (!isActive) {
                el.style.opacity = isCompleted ? '0.7' : '0.6';
                el.style.borderColor = isCompleted ? '#444' : '#555';
            }
        });

        return el;
    }

    /**
     * 获取步骤的人类可读描述
     * @param {Object} step
     * @returns {string}
     * @private
     */
    _getStepDescription(step) {
        const typeName = (step.type || '').replace('PROOF_STEP_', '').replace(/_/g, ' ');
        let desc = typeName;

        if (step.node_id >= 0) {
            desc += ` node#${step.node_id}`;
        }
        if (step.func_block_id >= 0) {
            desc += ` block#${step.func_block_id}`;
        }
        if (step.note) {
            desc += ` — ${step.note}`;
        }
        return desc;
    }

    /**
     * 高亮指定步骤（外部调用以响应用户操作或程序化导航）
     * @param {number} stepIndex - 要激活的步骤索引
     */
    highlightStep(stepIndex) {
        if (stepIndex < 0 || stepIndex >= this.steps.length) return;
        this.activeStepIndex = stepIndex;
        if (this.container) {
            this.render();
        }
    }
}


/**
 * =========================================================================
 * 第四部分：前提选择器（Premise Selector）
 * =========================================================================
 */

/**
 * @class ProofPremiseSelector
 * @description 前提选择器组件
 *
 *              借鉴 ProofWidgets4 的前提选择器设计：
 *              - 列出当前证明上下文中所有可用的前提（公理、已证引理、假设）
 *              - 支持前提搜索过滤
 *              - 支持前提的信任颜色标注
 *              - 点击前提可将其应用于当前证明步骤
 */
class ProofPremiseSelector {
    /**
     * @param {Object} [options]
     * @param {Function} [options.onPremiseSelect] - 前提被选择时的回调 (premise: Object) => void
     * @param {Function} [options.onPremiseFilter] - 前提过滤回调 (premise: Object, query: string) => boolean
     */
    constructor(options = {}) {
        /** @type {Function|null} */
        this.onPremiseSelect = options.onPremiseSelect || null;

        /** @type {Function|null} */
        this.onPremiseFilter = options.onPremiseFilter || null;

        /** @type {Array<Object>} 所有可用前提 */
        this.premises = [];

        /** @type {Array<Object>} 当前过滤后的前提列表 */
        this.filteredPremises = [];

        /** @type {HTMLElement|null} */
        this.container = null;

        /** @type {HTMLElement|null} */
        this.searchInput = null;

        /** @type {string} */
        this.currentQuery = '';

        /** @type {Set<string>} 已选择的前提 ID 集合 */
        this.selectedPremiseIds = new Set();
    }

    /**
     * 前提数据结构
     * @typedef {Object} Premise
     * @property {string}  id           - 前提唯一标识符
     * @property {string}  name         - 前提名称（如 "pythagorean_theorem"）
     * @property {string}  description  - 人类可读描述
     * @property {string}  type         - 前提类型："axiom" | "lemma" | "assumption" | "theorem"
     * @property {string}  color        - 信任颜色（对应 ProofColor 枚举）
     * @property {string}  source       - 来源："axiom_package" | "user_proved" | "external"
     * @property {boolean} applicable   - 是否适用于当前证明步骤
     * @property {Array<string>} tags   - 分类标签
     */

    /**
     * 加载可用前提列表
     *
     * 前提来源：
     * 1. 当前加载的公理包中的公理（来自 axiom_packages/）
     * 2. 已证的引理（来自 proof.h 的 ProofNavigator 中等价命题表）
     * 3. 当前证明步骤中已建立的前提
     *
     * @param {Array<Premise>} premises - 前提列表
     */
    loadPremises(premises) {
        this.premises = premises;
        this._applyFilter();
        if (this.container) {
            this.render();
        }
    }

    /**
     * 渲染到指定容器
     * @param {HTMLElement} container
     */
    renderToDOM(container) {
        this.container = container;
        this.render();
    }

    /**
     * 完整渲染
     * @private
     */
    render() {
        if (!this.container) return;
        this.container.innerHTML = '';

        // 搜索输入框
        const searchWrapper = document.createElement('div');
        searchWrapper.className = 'pw-premise-search';
        searchWrapper.style.marginBottom = '8px';

        this.searchInput = document.createElement('input');
        this.searchInput.type = 'text';
        this.searchInput.placeholder = 'Filter premises... / 过滤前提...';
        this.searchInput.value = this.currentQuery;
        Object.assign(this.searchInput.style, {
            width: '100%',
            padding: '6px 8px',
            background: 'var(--canvas-bg, #1a1a2e)',
            color: 'var(--text, #e0e0e0)',
            border: '1px solid var(--segment, #555)',
            borderRadius: '4px',
            fontSize: '11px',
            fontFamily: 'inherit',
            boxSizing: 'border-box',
        });

        this.searchInput.addEventListener('input', (e) => {
            this.currentQuery = e.target.value;
            this._applyFilter();
            this._renderPremiseList();
        });

        searchWrapper.appendChild(this.searchInput);
        this.container.appendChild(searchWrapper);

        // 前提列表容器
        this.listContainer = document.createElement('div');
        this.listContainer.className = 'pw-premise-list';
        this.listContainer.style.maxHeight = '300px';
        this.listContainer.style.overflowY = 'auto';
        this.container.appendChild(this.listContainer);

        this._renderPremiseList();
    }

    /**
     * 渲染过滤后的前提列表
     * @private
     */
    _renderPremiseList() {
        if (!this.listContainer) return;
        this.listContainer.innerHTML = '';

        if (this.filteredPremises.length === 0) {
            const emptyEl = document.createElement('div');
            emptyEl.textContent = this.currentQuery
                ? 'No matching premises / 无匹配前提'
                : 'No premises available / 无可用前提';
            emptyEl.style.cssText = 'color:#868e96;font-size:11px;padding:12px;text-align:center;';
            this.listContainer.appendChild(emptyEl);
            return;
        }

        for (const premise of this.filteredPremises) {
            const itemEl = this._createPremiseElement(premise);
            this.listContainer.appendChild(itemEl);
        }
    }

    /**
     * 创建单个前提元素的 DOM
     * @param {Premise} premise
     * @returns {HTMLElement}
     * @private
     */
    _createPremiseElement(premise) {
        const el = document.createElement('div');
        el.className = 'pw-premise-item';
        el.dataset.premiseId = premise.id;

        const isSelected = this.selectedPremiseIds.has(premise.id);
        const color = PROOF_COLORS[premise.color] || PROOF_COLORS.GREEN;

        Object.assign(el.style, {
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            padding: '6px 8px',
            marginBottom: '2px',
            borderRadius: '4px',
            cursor: 'pointer',
            fontSize: '11px',
            border: isSelected ? `2px solid ${color}` : '1px solid transparent',
            background: isSelected ? `${color}22` : 'transparent',
            transition: 'all 0.15s ease',
        });

        // 选中复选框
        const checkbox = document.createElement('input');
        checkbox.type = 'checkbox';
        checkbox.checked = isSelected;
        checkbox.style.flexShrink = '0';
        checkbox.style.cursor = 'pointer';
        checkbox.addEventListener('change', (e) => {
            e.stopPropagation();
            if (e.target.checked) {
                this.selectedPremiseIds.add(premise.id);
            } else {
                this.selectedPremiseIds.delete(premise.id);
            }
            this._updatePremiseStyle(el, premise);
        });
        el.appendChild(checkbox);

        // 类型图标
        const typeIcons = {
            axiom: '\u25C7',       // ◇
            lemma: '\u25B3',       // △
            assumption: '\u25CB',  // ○
            theorem: '\u2605',     // ★
        };
        const typeIcon = document.createElement('span');
        typeIcon.textContent = typeIcons[premise.type] || '\u25CF';
        typeIcon.style.flexShrink = '0';
        typeIcon.style.fontSize = '10px';
        el.appendChild(typeIcon);

        // 前提名称和描述
        const textSpan = document.createElement('span');
        textSpan.style.flex = '1';
        textSpan.style.overflow = 'hidden';

        const nameEl = document.createElement('div');
        nameEl.textContent = premise.name;
        nameEl.style.fontWeight = 'bold';
        nameEl.style.fontSize = '11px';
        nameEl.style.color = 'var(--text, #e0e0e0)';
        textSpan.appendChild(nameEl);

        if (premise.description) {
            const descEl = document.createElement('div');
            descEl.textContent = premise.description;
            descEl.style.fontSize = '9px';
            descEl.style.color = '#868e96';
            descEl.style.marginTop = '1px';
            descEl.style.overflow = 'hidden';
            descEl.style.textOverflow = 'ellipsis';
            descEl.style.whiteSpace = 'nowrap';
            textSpan.appendChild(descEl);
        }

        el.appendChild(textSpan);

        // 信任颜色指示器
        const colorDot = document.createElement('span');
        colorDot.style.width = '6px';
        colorDot.style.height = '6px';
        colorDot.style.borderRadius = '50%';
        colorDot.style.backgroundColor = color;
        colorDot.style.flexShrink = '0';
        el.appendChild(colorDot);

        // 适用性标记
        if (premise.applicable !== undefined && !premise.applicable) {
            const notApplicable = document.createElement('span');
            notApplicable.textContent = 'N/A';
            notApplicable.style.fontSize = '8px';
            notApplicable.style.color = '#868e96';
            notApplicable.style.flexShrink = '0';
            el.appendChild(notApplicable);
            el.style.opacity = '0.4';
        }

        // 点击选择
        el.addEventListener('click', (e) => {
            if (e.target !== checkbox) {
                if (this.onPremiseSelect) {
                    this.onPremiseSelect(premise);
                }
            }
        });

        // 悬停效果
        el.addEventListener('mouseenter', () => {
            el.style.background = `${color}11`;
        });
        el.addEventListener('mouseleave', () => {
            el.style.background = isSelected ? `${color}22` : 'transparent';
        });

        return el;
    }

    /**
     * 更新前提元素的样式
     * @param {HTMLElement} el
     * @param {Premise} premise
     * @private
     */
    _updatePremiseStyle(el, premise) {
        const isSelected = this.selectedPremiseIds.has(premise.id);
        const color = PROOF_COLORS[premise.color] || PROOF_COLORS.GREEN;
        el.style.border = isSelected ? `2px solid ${color}` : '1px solid transparent';
        el.style.background = isSelected ? `${color}22` : 'transparent';
    }

    /**
     * 应用搜索过滤
     * @private
     */
    _applyFilter() {
        if (!this.currentQuery) {
            this.filteredPremises = [...this.premises];
            return;
        }

        const query = this.currentQuery.toLowerCase();
        this.filteredPremises = this.premises.filter((p) => {
            // 使用自定义过滤器（如果提供）
            if (this.onPremiseFilter) {
                return this.onPremiseFilter(p, query);
            }
            // 默认搜索：名称、描述、标签
            return (p.name && p.name.toLowerCase().includes(query))
                || (p.description && p.description.toLowerCase().includes(query))
                || (p.tags && p.tags.some((t) => t.toLowerCase().includes(query)));
        });
    }

    /**
     * 获取当前选中的所有前提
     * @returns {Array<Premise>}
     */
    getSelectedPremises() {
        return this.premises.filter((p) => this.selectedPremiseIds.has(p.id));
    }

    /**
     * 清除所有选择
     */
    clearSelection() {
        this.selectedPremiseIds.clear();
        if (this.container) {
            this.render();
        }
    }
}


/**
 * =========================================================================
 * 第五部分：证明面板适配器（整合三大组件）
 * =========================================================================
 */

/**
 * @class ProofWidgetsAdapter
 * @description 证明面板适配器——将 GoalTree、StepHighlighter、PremiseSelector
 *              三个子组件整合为统一的证明可视化面板。
 *
 *              此适配器作为 web-gui/ ProofPanel.tsx 和 web/ modules/proof.js
 *              之间的桥接层，提供框架无关的核心逻辑。
 *
 *              使用方式（React ProofPanel）：
 *              ```tsx
 *              const adapter = new ProofWidgetsAdapter();
 *              adapter.attachToContainer(containerRef.current);
 *              adapter.setProofData(navData, premises);
 *              ```
 *
 *              使用方式（vanilla JS proof.js）：
 *              ```javascript
 *              const adapter = new ProofWidgetsAdapter();
 *              adapter.attachToContainer(document.getElementById('proof-panel'));
 *              adapter.setProofData(navData, premises);
 *              ```
 */
class ProofWidgetsAdapter {
    /**
     * @param {ProofWidgetsConfig} [config]
     */
    constructor(config = {}) {
        /** @type {ProofWidgetsConfig} */
        this.config = Object.assign({}, DEFAULT_PROOF_WIDGETS_CONFIG, config);

        /** @type {ProofGoalTreeRenderer} */
        this.goalTree = new ProofGoalTreeRenderer(this.config);

        /** @type {ProofStepHighlighter} */
        this.stepHighlighter = new ProofStepHighlighter({
            onStepClick: (index) => this._handleStepClick(index),
        });

        /** @type {ProofPremiseSelector} */
        this.premiseSelector = new ProofPremiseSelector({
            onPremiseSelect: (premise) => this._handlePremiseSelect(premise),
        });

        /** @type {HTMLElement|null} 主容器 */
        this.rootContainer = null;

        /** @type {Object|null} 缓存的最新证明数据 */
        this._cachedProofData = null;

        /** @type {Array<Object>} 缓存的最新前提数据 */
        this._cachedPremises = [];

        /** @type {Function|null} */
        this.onStepNavigationRequest = null;

        /** @type {Function|null} */
        this.onPremiseApplicationRequest = null;
    }

    /**
     * 将整个证明面板附加到 DOM 容器中
     *
     * 容器内布局：
     * ┌──────────────────────────┐
     * │  GOAL TREE / 目标树      │
     * │                          │
     * ├──────────────────────────┤
     * │  STEPS / 证明步骤         │
     * │  [step slider/nav]       │
     * ├──────────────────────────┤
     * │  PREMISES / 前提          │
     * │  [search] [premise list] │
     * └──────────────────────────┘
     *
     * @param {HTMLElement} container
     */
    attachToContainer(container) {
        this.rootContainer = container;
        this._buildLayout();

        // 如果有缓存的数据，立即渲染
        if (this._cachedProofData) {
            const { steps, currentStep, targetProp, breakpoints } = this._cachedProofData;
            this.goalTree.setNavigatorData({ steps, currentStep, targetProp, dependencies: [] });
            this.stepHighlighter.loadSteps(steps, currentStep, breakpoints || []);
        }
        if (this._cachedPremises.length > 0) {
            this.premiseSelector.loadPremises(this._cachedPremises);
        }
    }

    /**
     * 构建布局结构
     * @private
     */
    _buildLayout() {
        if (!this.rootContainer) return;
        this.rootContainer.innerHTML = '';

        // 目标树区域
        if (this.config.enableGoalTree) {
            const goalSection = document.createElement('div');
            goalSection.className = 'pw-section pw-goal-section';
            const goalTitle = document.createElement('div');
            goalTitle.className = 'pw-section-title';
            goalTitle.textContent = 'GOAL TREE / 目标树';
            goalTitle.style.cssText = 'font-weight:bold;font-size:11px;color:#868e96;margin-bottom:6px;text-transform:uppercase;';
            goalSection.appendChild(goalTitle);

            const goalContent = document.createElement('div');
            goalContent.className = 'pw-goal-content';
            goalContent.id = 'pw-goal-tree-container';
            goalSection.appendChild(goalContent);
            this.rootContainer.appendChild(goalSection);

            this.goalTree.renderToDOM(goalContent);
        }

        // 步骤高亮区域
        if (this.config.enableStepHighlight) {
            const stepSection = document.createElement('div');
            stepSection.className = 'pw-section pw-step-section';
            const stepTitle = document.createElement('div');
            stepTitle.className = 'pw-section-title';
            stepTitle.textContent = 'PROOF STEPS / 证明步骤';
            stepTitle.style.cssText = 'font-weight:bold;font-size:11px;color:#868e96;margin-bottom:6px;text-transform:uppercase;';
            stepSection.appendChild(stepTitle);

            const stepContent = document.createElement('div');
            stepContent.className = 'pw-step-content';
            stepContent.id = 'pw-step-highlighter-container';
            stepContent.style.maxHeight = '250px';
            stepContent.style.overflowY = 'auto';
            stepSection.appendChild(stepContent);
            this.rootContainer.appendChild(stepSection);

            this.stepHighlighter.renderToDOM(stepContent);
        }

        // 前提选择器区域
        if (this.config.enablePremiseSelector) {
            const premiseSection = document.createElement('div');
            premiseSection.className = 'pw-section pw-premise-section';
            const premiseTitle = document.createElement('div');
            premiseTitle.className = 'pw-section-title';
            premiseTitle.textContent = 'PREMISES / 可用前提';
            premiseTitle.style.cssText = 'font-weight:bold;font-size:11px;color:#868e96;margin-bottom:6px;text-transform:uppercase;';
            premiseSection.appendChild(premiseTitle);

            const premiseContent = document.createElement('div');
            premiseContent.className = 'pw-premise-content';
            premiseContent.id = 'pw-premise-selector-container';
            premiseSection.appendChild(premiseContent);
            this.rootContainer.appendChild(premiseSection);

            this.premiseSelector.renderToDOM(premiseContent);
        }
    }

    /**
     * 设置证明数据并触发渲染
     *
     * @param {Object} proofData - 证明导航器数据
     * @param {Array<Object>} proofData.steps        - 证明步骤数组
     * @param {number}        proofData.currentStep  - 当前步骤索引
     * @param {Object}        proofData.targetProp   - 目标命题
     * @param {Array<Object>} [proofData.dependencies] - 依赖树
     * @param {Array<number>} [proofData.breakpoints]  - 断点索引列表
     * @param {string}        [proofData.strategyNote] - 策略注释
     * @param {boolean}       [proofData.isComplete]   - 证明是否完成
     * @param {Array<Object>} premises               - 可用前提列表
     */
    setProofData(proofData, premises = []) {
        this._cachedProofData = proofData;
        this._cachedPremises = premises;

        if (!this.rootContainer) return;

        const { steps, currentStep, targetProp, dependencies, breakpoints, strategyNote, isComplete } = proofData;

        // 更新目标树
        if (this.config.enableGoalTree) {
            this.goalTree.setNavigatorData({
                steps: steps || [],
                currentStep: currentStep || 0,
                targetProp: targetProp || null,
                dependencies: dependencies || [],
                strategyNote: strategyNote || null,
                isComplete: isComplete || false,
            });
        }

        // 更新步骤高亮器
        if (this.config.enableStepHighlight) {
            this.stepHighlighter.loadSteps(steps || [], currentStep || 0, breakpoints || []);
        }

        // 更新前提选择器
        if (this.config.enablePremiseSelector) {
            this.premiseSelector.loadPremises(premises);
        }
    }

    /**
     * 导航到指定证明步骤
     *
     * @param {number} stepIndex - 目标步骤索引
     */
    navigateToStep(stepIndex) {
        this.stepHighlighter.highlightStep(stepIndex);
    }

    /**
     * 注册步骤点击回调（用于外部控制器响应步骤导航）
     *
     * @param {Function} callback - (stepIndex: number) => void
     */
    onStepNavigation(callback) {
        this.onStepNavigationRequest = callback;
    }

    /**
     * 注册前提应用回调（用于外部控制器处理前提选择后的操作）
     *
     * @param {Function} callback - (premise: Object) => void
     */
    onPremiseApplication(callback) {
        this.onPremiseApplicationRequest = callback;
    }

    /**
     * 内部步骤点击处理
     * @param {number} stepIndex
     * @private
     */
    _handleStepClick(stepIndex) {
        if (this.onStepNavigationRequest) {
            this.onStepNavigationRequest(stepIndex);
        }
    }

    /**
     * 内部前提选择处理
     * @param {Object} premise
     * @private
     */
    _handlePremiseSelect(premise) {
        if (this.onPremiseApplicationRequest) {
            this.onPremiseApplicationRequest(premise);
        }
    }

    /**
     * 销毁适配器，清理所有 DOM 引用和事件监听器
     */
    destroy() {
        this.rootContainer = null;
        this._cachedProofData = null;
        this._cachedPremises = [];
        this.onStepNavigationRequest = null;
        this.onPremiseApplicationRequest = null;
    }
}


/**
 * =========================================================================
 * 第六部分：React ProofPanel 增强集成指南
 * =========================================================================
 *
 * 在 web-gui/src/components/panels/ProofPanel.tsx 中的集成方式：
 *
 * ```tsx
 * import { useEffect, useRef, useCallback } from 'react';
 *
 * // 在 ProofPanel 组件中：
 * const adapterRef = useRef<ProofWidgetsAdapter | null>(null);
 * const widgetsContainerRef = useRef<HTMLDivElement>(null);
 *
 * useEffect(() => {
 *   const adapter = new ProofWidgetsAdapter({
 *     enableGoalTree: true,
 *     enableStepHighlight: true,
 *     enablePremiseSelector: true,
 *     colorScheme: 'trust',
 *   });
 *
 *   adapter.onStepNavigation((stepIndex) => {
 *     // 调用现有的步骤导航逻辑
 *     handleGotoStep(stepIndex);
 *   });
 *
 *   adapter.onPremiseApplication((premise) => {
 *     // 调用现有的前提应用逻辑
 *     applyPremiseToCurrentStep(premise);
 *   });
 *
 *   if (widgetsContainerRef.current) {
 *     adapter.attachToContainer(widgetsContainerRef.current);
 *   }
 *
 *   adapterRef.current = adapter;
 *
 *   return () => adapter.destroy();
 * }, []);
 *
 * // 当 proofSteps 或 undoStack 变化时更新数据：
 * useEffect(() => {
 *   if (adapterRef.current) {
 *     adapterRef.current.setProofData({
 *       steps: convertToProofWidgetSteps(proofSteps),
 *       currentStep: currentStepIndex,
 *       targetProp: { name: 'Current Construction' },
 *       breakpoints: breakpointIndices,
 *     }, availablePremises);
 *   }
 * }, [proofSteps, currentStepIndex]);
 *
 * // 在 JSX 中：
 * return (
 *   <>
 *     <Panel title="PROOF / 证明" panelId="proof-ops">
 *       {/* 保留现有的按钮和控件 * /}
 *       ...
 *     </Panel>
 *     <div ref={widgetsContainerRef} className="proof-widgets-container" />
 *   </>
 * );
 * ```
 */


/**
 * =========================================================================
 * 第七部分：模块导出
 * =========================================================================
 */

// 在模块系统中导出（CommonJS / ES Module 兼容）
if (typeof module !== 'undefined' && module.exports) {
    module.exports = {
        ProofGoalTreeRenderer,
        ProofStepHighlighter,
        ProofPremiseSelector,
        ProofWidgetsAdapter,
        PROOF_COLORS,
        PROOF_STEP_ICONS,
        DEFAULT_PROOF_WIDGETS_CONFIG,
    };
}

// 在浏览器全局作用域中注册（与 web/ 下的其他 JS 文件一致）
if (typeof window !== 'undefined') {
    window.Lv00ProofWidgets = {
        ProofGoalTreeRenderer,
        ProofStepHighlighter,
        ProofPremiseSelector,
        ProofWidgetsAdapter,
        PROOF_COLORS,
        PROOF_STEP_ICONS,
        DEFAULT_PROOF_WIDGETS_CONFIG,
    };
}
