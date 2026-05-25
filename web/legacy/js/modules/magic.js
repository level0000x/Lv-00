/**
 * @file magic.js
 * @brief MAGIC 魔法模块（整合轰界法术生成器）
 * @description 将轰界法术编译器（SpellCompiler）集成到 Lv-00 主界面，
 *              支持四阶段法术构造流程：开模 -> 提纯 -> 灌注 -> 释放。
 *              包含节点库管理、阶段选择、法术编译与评估、雷达图展示等功能。
 *              挂载到 Lv00WebApp.prototype 上，作为 MAGIC 面板的 UI 层。
 *
 * @module magic
 * @requires Lv00WebApp 构造函数（app.js）
 * @requires SpellCompiler（spell_compiler.js）轰界法术编译器
 * @since 3.0.0
 */

(function() {
    'use strict';

    // 模块状态
    var state = {
        nodeLibrary: null,
        stages: {
            model: { stage: 'model', nodes: [] },
            purify: { stage: 'purify', nodes: [] },
            infuse: { stage: 'infuse', nodes: [] },
            release: { stage: 'release', nodes: [] }
        },
        currentStage: 'model',
        compileResult: null,
        nodeCounter: 0
    };

    // ================================================================
    // 初始化
    // ================================================================

    function init() {
        // 加载节点库
        loadNodeLibrary();

        // 绑定事件
        bindEvents();

        console.log('[Magic Module v2] Initialized with Hongjie Spell Compiler');
    }

    function loadNodeLibrary() {
        // 尝试从本地加载节点库（带 10 秒超时）
        var controller = new AbortController();
        var timeoutId = setTimeout(function() { controller.abort(); }, 10000);
        fetch('data/magic_nodes.json', { signal: controller.signal })
            .then(function(response) {
                clearTimeout(timeoutId);
                if (!response.ok) throw new Error('Failed to load node library');
                return response.json();
            })
            .then(function(data) {
                state.nodeLibrary = data;
                if (typeof SpellCompiler !== 'undefined') {
                    SpellCompiler.loadNodeLibrary(data);
                }
                updateNodeLibraryUI();
                console.log('[Magic Module] Node library loaded:', data.nodes.length, 'nodes');
            })
            .catch(function(error) {
                clearTimeout(timeoutId);
                // 网络请求失败或 JSON 解析失败时，使用内嵌的回退节点库
                console.warn('[Magic Module] 节点库加载失败，使用回退数据:', error.message || error);
                loadFallbackLibrary();
                // 向用户提示加载失败（不影响功能使用）
                if (window.lv00App && typeof window.lv00App.appendLog === 'function') {
                    window.lv00App.appendLog('魔法节点库远程加载失败，已使用内置回退数据 / Magic node library fetch failed, using fallback', 'warn');
                }
            });
    }

    function loadFallbackLibrary() {
        // 内嵌的节点库数据（简化版）
        var fallbackData = {
            version: 'mvp-0.1-fallback',
            stages: [
                { id: 'model', name: '开模', purpose: 'Build the carrier form for the spell.' },
                { id: 'purify', name: '提纯', purpose: 'Choose the element or ether system.' },
                { id: 'infuse', name: '灌注', purpose: 'Inject the purified result into the form.' },
                { id: 'release', name: '释放', purpose: 'Execute the completed spell structure.' }
            ],
            nodes: [
                // 开模节点
                { id: 'model_sphere', stage: 'model', name: '球形', category: 'shape', tier: 1, difficulty: 8, outputs: ['sphere', 'projectile'], tags: ['弹体'], risk_tags: [], score_bias: { power: 4, stability: 8, learnability: 8, mana_efficiency: 4, versatility: 8, academic_value: 2 } },
                { id: 'model_blade', stage: 'model', name: '刃形', category: 'shape', tier: 2, difficulty: 14, outputs: ['blade'], tags: ['刃', '切割'], risk_tags: ['edge_control'], score_bias: { power: 8, stability: 2, learnability: 2, mana_efficiency: 4, versatility: 4, academic_value: 4 } },
                { id: 'model_wall', stage: 'model', name: '墙体', category: 'shape', tier: 2, difficulty: 14, outputs: ['wall'], tags: ['壁', '防御'], risk_tags: ['block_path'], score_bias: { power: 1, stability: 8, learnability: 3, mana_efficiency: 2, versatility: 5, academic_value: 3 } },
                { id: 'model_line', stage: 'model', name: '线形', category: 'shape', tier: 1, difficulty: 8, outputs: ['line'], tags: ['线形'], risk_tags: [], score_bias: { power: 5, stability: 5, learnability: 4, mana_efficiency: 4, versatility: 5, academic_value: 3 } },
                { id: 'model_area', stage: 'model', name: '范围', category: 'shape', tier: 3, difficulty: 20, outputs: ['area'], tags: ['场域'], risk_tags: ['area_spill'], score_bias: { power: 6, stability: -4, learnability: -2, mana_efficiency: -2, versatility: 6, academic_value: 5 } },
                // 提纯节点
                { id: 'purify_fire', stage: 'purify', name: '火', category: 'element', tier: 1, difficulty: 8, outputs: ['fire'], tags: ['火'], risk_tags: ['thermal_spread'], score_bias: { power: 10, stability: -2, learnability: 7, mana_efficiency: 3, versatility: 5, academic_value: 4 } },
                { id: 'purify_water', stage: 'purify', name: '水', category: 'element', tier: 1, difficulty: 8, outputs: ['water'], tags: ['水'], risk_tags: ['slip', 'conductive_context'], score_bias: { power: 3, stability: 5, learnability: 6, mana_efficiency: 5, versatility: 7, academic_value: 4 } },
                { id: 'purify_wind', stage: 'purify', name: '风', category: 'element', tier: 1, difficulty: 8, outputs: ['wind'], tags: ['风'], risk_tags: ['drift'], score_bias: { power: 6, stability: -3, learnability: 3, mana_efficiency: 6, versatility: 7, academic_value: 5 } },
                { id: 'purify_earth', stage: 'purify', name: '土', category: 'element', tier: 1, difficulty: 8, outputs: ['earth'], tags: ['土'], risk_tags: ['terrain_damage'], score_bias: { power: 6, stability: 8, learnability: 4, mana_efficiency: -2, versatility: 5, academic_value: 4 } },
                { id: 'purify_chaos', stage: 'purify', name: '以太无属性', category: 'element', tier: 1, difficulty: 14, outputs: ['chaos'], tags: ['以太', '无属性', '混沌'], risk_tags: ['cognitive_load'], score_bias: { power: 5, stability: -5, learnability: -7, mana_efficiency: -5, versatility: 10, academic_value: 9 } },
                // 灌注节点
                { id: 'infuse_standard', stage: 'infuse', name: '标准灌注', category: 'infusion', tier: 1, difficulty: 8, outputs: ['infusion'], tags: ['标准'], risk_tags: [], score_bias: { power: 3, stability: 8, learnability: 8, mana_efficiency: 6, versatility: 7, academic_value: 2 } },
                { id: 'infuse_maintain', stage: 'infuse', name: '维持灌注', category: 'infusion', tier: 2, difficulty: 14, outputs: ['infusion', 'maintain'], tags: ['维持'], risk_tags: ['attention_parallel'], score_bias: { power: 4, stability: 4, learnability: 2, mana_efficiency: -2, versatility: 6, academic_value: 4 } },
                { id: 'infuse_compress', stage: 'infuse', name: '压缩灌注', category: 'infusion', tier: 3, difficulty: 26, outputs: ['infusion', 'compressed'], tags: ['压缩'], risk_tags: ['overpressure'], score_bias: { power: 10, stability: -7, learnability: -4, mana_efficiency: -6, versatility: 3, academic_value: 5 } },
                { id: 'infuse_multi', stage: 'infuse', name: '多重灌注', category: 'infusion', tier: 3, difficulty: 28, outputs: ['infusion', 'multi'], tags: ['多重'], risk_tags: ['attention_parallel'], score_bias: { power: 8, stability: -9, learnability: -7, mana_efficiency: -8, versatility: 6, academic_value: 7 } },
                // 释放节点
                { id: 'release_projectile', stage: 'release', name: '投射释放', category: 'release', tier: 1, difficulty: 8, outputs: ['effect'], tags: ['弹体'], risk_tags: ['miss'], score_bias: { power: 5, stability: 5, learnability: 8, mana_efficiency: 5, versatility: 6, academic_value: 2 } },
                { id: 'release_maintain', stage: 'release', name: '维持释放', category: 'release', tier: 2, difficulty: 14, outputs: ['effect'], tags: ['维持'], risk_tags: ['attention_parallel'], score_bias: { power: 3, stability: 4, learnability: 2, mana_efficiency: -2, versatility: 6, academic_value: 4 } },
                { id: 'release_activate', stage: 'release', name: '激发释放', category: 'release', tier: 3, difficulty: 26, outputs: ['effect'], tags: ['激发'], risk_tags: ['burst'], score_bias: { power: 9, stability: -5, learnability: -4, mana_efficiency: -3, versatility: 4, academic_value: 5 } },
                { id: 'release_diffuse', stage: 'release', name: '扩散释放', category: 'release', tier: 4, difficulty: 32, outputs: ['effect'], tags: ['扩散'], risk_tags: ['area_spill'], score_bias: { power: 7, stability: -7, learnability: -4, mana_efficiency: -3, versatility: 6, academic_value: 6 } }
            ],
            compounds: [
                { primary: 'fire', secondary: 'wind', catalyst: 'water', result: '雷电系法术', form: '激发', tier: 5, difficulty: 42, risk_tags: ['high_voltage', 'governance_review'], score_bias: { power: 14, stability: -12, learnability: -10, mana_efficiency: -10, versatility: 5, academic_value: 10 } },
                { primary: 'earth', secondary: 'water', catalyst: null, result: '泥沼系法术', form: '混合', tier: 4, difficulty: 28, risk_tags: ['terrain_damage'], score_bias: { power: 5, stability: 4, learnability: -2, mana_efficiency: -1, versatility: 8, academic_value: 7 } },
                { primary: 'water', secondary: 'earth', catalyst: null, result: '泥沼系法术', form: '混合', tier: 4, difficulty: 28, risk_tags: ['terrain_damage'], score_bias: { power: 5, stability: 4, learnability: -2, mana_efficiency: -1, versatility: 8, academic_value: 7 } }
            ]
        };

        state.nodeLibrary = fallbackData;
        if (typeof SpellCompiler !== 'undefined') {
            SpellCompiler.loadNodeLibrary(fallbackData);
        }
        updateNodeLibraryUI();
    }

    // ================================================================
    // UI 更新
    // ================================================================

    // fix: 本函数采用全量 DOM 重建策略（innerHTML 赋值），每次调用都会销毁并
    //      重新创建所有节点 DOM 元素。节点库规模较大时（>100 节点），建议改为增量
    //      更新策略（如 Virtual DOM diff 或按需更新变化的 stage 区块），以减少
    //      repaint/reflow 开销。
    function updateNodeLibraryUI() {
        var container = document.getElementById('magicNodeLibrary');
        if (!container || !state.nodeLibrary) return;

        var html = '';
        var stages = ['model', 'purify', 'infuse', 'release'];
        var stageNames = { model: '开模', purify: '提纯', infuse: '灌注', release: '释放' };

        stages.forEach(function(stageId) {
            var nodes = state.nodeLibrary.nodes.filter(function(n) { return n.stage === stageId; });
            if (nodes.length === 0) return;

            html += '<div class="magic-node-stage">';
            html += '<div class="magic-node-stage-title">' + stageNames[stageId] + '</div>';
            html += '<div class="magic-node-list">';

            nodes.forEach(function(node) {
                html += '<div class="magic-node-item" data-node-id="' + _escapeHtml(node.id) + '" data-stage="' + stageId + '">';
                html += '<span class="magic-node-name">' + _escapeHtml(node.name) + '</span>';
                html += '<span class="magic-node-tier">T' + _escapeHtml(node.tier) + '</span>';
                html += '</div>';
            });

            html += '</div></div>';
        });

        // fix: 使用 innerHTML 拼接 HTML 时，若 node.name/node.tier/node.id 等字段
        //      来自外部数据源（如 JSON 文件或用户输入），存在 XSS 风险。
        //      建议对动态内容使用 textContent 赋值或进行 HTML 实体转义。
        // fix: result.spell_name、result.summary、issue.message 等字段来自编译输出，
        //      使用 innerHTML 拼接时存在 XSS 风险。建议对动态内容进行 HTML 实体转义。
        container.innerHTML = html;

        // 绑定节点点击事件
        var nodeItems = container.querySelectorAll('.magic-node-item');
        nodeItems.forEach(function(item) {
            item.addEventListener('click', function() {
                var nodeId = item.getAttribute('data-node-id');
                var stageId = item.getAttribute('data-stage');
                addNodeToStage(nodeId, stageId);
            });
        });
    }

    function updateStageUI() {
        var stages = ['model', 'purify', 'infuse', 'release'];
        var stageNames = { model: '开模', purify: '提纯', infuse: '灌注', release: '释放' };

        stages.forEach(function(stageId) {
            var container = document.getElementById('magicStage_' + stageId);
            if (!container) return;

            var stage = state.stages[stageId];
            var html = '';

            if (stage.nodes.length === 0) {
                html = '<div class="magic-stage-empty">点击左侧节点添加 / Click nodes to add</div>';
            } else {
                stage.nodes.forEach(function(nodeRef, index) {
                    var node = state.nodeLibrary ? state.nodeLibrary.nodes.find(function(n) {
                        return n.id === nodeRef.node_id;
                    }) : null;

                    if (node) {
                        html += '<div class="magic-stage-node">';
                        html += '<span class="magic-stage-node-name">' + _escapeHtml(node.name) + '</span>';
                        html += '<span class="magic-stage-node-tier">T' + _escapeHtml(node.tier) + '</span>';
                        html += '<button class="magic-stage-node-remove" data-stage="' + stageId + '" data-index="' + index + '">&times;</button>';
                        html += '</div>';
                    }
                });
            }

            // fix: 同上，innerHTML 拼接 node.name/node.tier 等字段存在 XSS 风险，
            //      建议对动态内容进行转义。
            container.innerHTML = html;
        });

        // 绑定删除按钮
        document.querySelectorAll('.magic-stage-node-remove').forEach(function(btn) {
            btn.addEventListener('click', function() {
                var stageId = btn.getAttribute('data-stage');
                var index = parseInt(btn.getAttribute('data-index'), 10);
                removeNodeFromStage(stageId, index);
            });
        });

        // 更新阶段标签
        updateStageTabs();
    }

    function updateStageTabs() {
        var tabs = document.querySelectorAll('.magic-stage-tab');
        tabs.forEach(function(tab) {
            var stageId = tab.getAttribute('data-stage');
            var nodeCount = state.stages[stageId].nodes.length;
            var badge = tab.querySelector('.magic-stage-badge');
            if (badge) {
                badge.textContent = nodeCount;
                badge.style.display = nodeCount > 0 ? 'inline-block' : 'none';
            }

            // 高亮当前阶段
            if (stageId === state.currentStage) {
                tab.classList.add('active');
            } else {
                tab.classList.remove('active');
            }
        });
    }

    function updateCompileResult(result) {
        var container = document.getElementById('magicCompileResult');
        if (!container) return;

        if (!result) {
            container.innerHTML = '<div class="panel-empty-state">点击编译查看结果 / Click compile to see result</div>';
            return;
        }

        var html = '<div class="magic-result-content">';

        // 状态
        var statusClass = 'magic-status-' + result.status;
        var statusText = {
            compiled: '✓ 编译成功',
            partial: '⚠ 部分编译',
            failed: '✗ 编译失败',
            unsafe: '☠ 高危法术'
        }[result.status] || result.status;

        html += '<div class="magic-result-status ' + statusClass + '">' + statusText + '</div>';

        // 法术名称和等级
        if (result.spell_name) {
            html += '<div class="magic-result-name">' + _escapeHtml(result.spell_name) + '</div>';
        }
        if (result.spell_level) {
            html += '<div class="magic-result-level">' + _escapeHtml(result.spell_level.label) + '</div>';
        }

        // 摘要
        if (result.summary) {
            html += '<div class="magic-result-summary">' + _escapeHtml(result.summary) + '</div>';
        }

        // 阶段结果
        if (result.stage_outcomes && result.stage_outcomes.length > 0) {
            html += '<div class="magic-result-stages">';
            html += '<div class="magic-result-section-title">阶段结果</div>';
            result.stage_outcomes.forEach(function(outcome) {
                html += '<div class="magic-result-stage">';
                html += '<span class="magic-result-stage-label">' + _escapeHtml(outcome.label) + '</span>';
                html += '<span class="magic-result-stage-result">' + _escapeHtml(outcome.result) + '</span>';
                html += '</div>';
            });
            html += '</div>';
        }

        // 雷达图
        if (result.radar && result.radar.length > 0) {
            html += '<div class="magic-result-radar">';
            html += '<div class="magic-result-section-title">六维雷达</div>';
            html += '<div class="magic-radar-container">';
            html += '<canvas id="magicRadarCanvas" width="200" height="200" aria-label="法术六维雷达图" role="img"></canvas>';
            html += '</div>';
            html += '<div class="magic-radar-legend">';
            result.radar.forEach(function(item) {
                html += '<div class="magic-radar-item">';
                html += '<span class="magic-radar-label">' + _escapeHtml(item.label) + '</span>';
                html += '<div class="magic-radar-bar"><div class="magic-radar-fill" style="width:' + Math.round(item.value) + '%"></div></div>';
                html += '<span class="magic-radar-value">' + Math.round(item.value) + '</span>';
                html += '</div>';
            });
            html += '</div>';
            html += '</div>';
        }

        // 问题列表
        if (result.issues && result.issues.length > 0) {
            html += '<div class="magic-result-issues">';
            html += '<div class="magic-result-section-title">问题与建议</div>';
            result.issues.forEach(function(issue) {
                var severityClass = 'magic-issue-' + issue.severity;
                html += '<div class="magic-issue ' + severityClass + '">';
                html += '<div class="magic-issue-message">' + _escapeHtml(issue.message) + '</div>';
                if (issue.suggestion) {
                    html += '<div class="magic-issue-suggestion">&rarr; ' + _escapeHtml(issue.suggestion) + '</div>';
                }
                html += '</div>';
            });
            html += '</div>';
        }

        // 法术卡片
        if (result.spell_card) {
            var card = result.spell_card;
            html += '<div class="magic-result-card">';
            html += '<div class="magic-result-section-title">法术卡片</div>';

            if (card.conditions && card.conditions.length > 0) {
                html += '<div class="magic-card-section">';
                html += '<div class="magic-card-section-title">施法条件</div>';
                card.conditions.forEach(function(cond) {
                    html += '<div class="magic-card-item">&bull; ' + _escapeHtml(cond) + '</div>';
                });
                html += '</div>';
            }

            if (card.costs && card.costs.length > 0) {
                html += '<div class="magic-card-section">';
                html += '<div class="magic-card-section-title">代价</div>';
                card.costs.forEach(function(cost) {
                    html += '<div class="magic-card-item">&bull; ' + _escapeHtml(cost) + '</div>';
                });
                html += '</div>';
            }

            if (card.risks && card.risks.length > 0) {
                html += '<div class="magic-card-section">';
                html += '<div class="magic-card-section-title">风险</div>';
                card.risks.forEach(function(risk) {
                    html += '<div class="magic-card-item magic-card-risk">&#9888; ' + _escapeHtml(risk) + '</div>';
                });
                html += '</div>';
            }

            if (card.suggestions && card.suggestions.length > 0) {
                html += '<div class="magic-card-section">';
                html += '<div class="magic-card-section-title">建议</div>';
                card.suggestions.forEach(function(sugg) {
                    html += '<div class="magic-card-item magic-card-suggestion">&rarr; ' + _escapeHtml(sugg) + '</div>';
                });
                html += '</div>';
            }

            html += '</div>';
        }

        html += '</div>';
        // fix: result.spell_name、result.summary、issue.message 等字段来自编译输出，
        //      使用 innerHTML 拼接时存在 XSS 风险。建议对动态内容进行 HTML 实体转义。
        container.innerHTML = html;

        // 绘制雷达图
        if (result.radar) {
            drawRadarChart(result.radar);
        }
    }

    function drawRadarChart(radarData) {
        var canvas = document.getElementById('magicRadarCanvas');
        if (!canvas) return;

        var ctx = canvas.getContext('2d');
        var centerX = canvas.width / 2;
        var centerY = canvas.height / 2;
        var radius = 80;

        ctx.clearRect(0, 0, canvas.width, canvas.height);

        // 绘制背景网格
        ctx.strokeStyle = 'rgba(155, 89, 182, 0.3)';
        ctx.lineWidth = 1;

        for (var i = 1; i <= 5; i++) {
            ctx.beginPath();
            ctx.arc(centerX, centerY, radius * i / 5, 0, Math.PI * 2);
            ctx.stroke();
        }

        // 绘制轴线
        var angleStep = (Math.PI * 2) / radarData.length;
        ctx.strokeStyle = 'rgba(155, 89, 182, 0.5)';

        for (var j = 0; j < radarData.length; j++) {
            var angle = j * angleStep - Math.PI / 2;
            ctx.beginPath();
            ctx.moveTo(centerX, centerY);
            ctx.lineTo(centerX + Math.cos(angle) * radius, centerY + Math.sin(angle) * radius);
            ctx.stroke();
        }

        // 绘制数据
        ctx.fillStyle = 'rgba(155, 89, 182, 0.3)';
        ctx.strokeStyle = '#9b59b6';
        ctx.lineWidth = 2;
        ctx.beginPath();

        for (var k = 0; k < radarData.length; k++) {
            var dataAngle = k * angleStep - Math.PI / 2;
            var dataRadius = (radarData[k].value / 100) * radius;
            var x = centerX + Math.cos(dataAngle) * dataRadius;
            var y = centerY + Math.sin(dataAngle) * dataRadius;

            if (k === 0) {
                ctx.moveTo(x, y);
            } else {
                ctx.lineTo(x, y);
            }
        }

        ctx.closePath();
        ctx.fill();
        ctx.stroke();
    }

    // ================================================================
    // 事件处理
    // ================================================================

    function bindEvents() {
        // 阶段标签切换
        document.querySelectorAll('.magic-stage-tab').forEach(function(tab) {
            tab.addEventListener('click', function() {
                state.currentStage = tab.getAttribute('data-stage');
                updateStageTabs();
            });
        });

        // 编译按钮
        var compileBtn = document.getElementById('btnMagicCompile');
        if (compileBtn) {
            compileBtn.addEventListener('click', onCompile);
        }

        // 清空按钮
        var clearBtn = document.getElementById('btnMagicClear');
        if (clearBtn) {
            clearBtn.addEventListener('click', onClear);
        }

        // 预设法术
        var presetList = document.getElementById('magicPresetList');
        if (presetList) {
            presetList.addEventListener('click', onPresetClick);
        }
    }

    function addNodeToStage(nodeId, stageId) {
        state.nodeCounter++;
        state.stages[stageId].nodes.push({
            instance_id: stageId + '-' + state.nodeCounter,
            node_id: nodeId
        });
        updateStageUI();
        showToast('已添加: ' + (getNodeName(nodeId) || nodeId));
    }

    function removeNodeFromStage(stageId, index) {
        var nodeId = state.stages[stageId].nodes[index].node_id;
        state.stages[stageId].nodes.splice(index, 1);
        updateStageUI();
        showToast('已移除: ' + (getNodeName(nodeId) || nodeId));
    }

    function getNodeName(nodeId) {
        if (!state.nodeLibrary) return nodeId;
        var node = state.nodeLibrary.nodes.find(function(n) { return n.id === nodeId; });
        return node ? node.name : nodeId;
    }

    function onCompile() {
        if (typeof SpellCompiler === 'undefined') {
            showToast('编译器未加载 / Compiler not loaded', 'error');
            return;
        }

        // 构建编译请求
        var stages = Object.values(state.stages).filter(function(s) {
            return s.nodes.length > 0;
        });

        if (stages.length < 4) {
            showToast('请完成四个阶段的构造 / Please complete all four stages', 'warning');
            // fix: 四个阶段不全时应立即返回，避免后续执行不完整的编译
            return;
        }

        var result = SpellCompiler.compileSpell(stages, '自定义法术');
        state.compileResult = result;
        updateCompileResult(result);

        // 显示状态提示
        var statusMessages = {
            compiled: '法术编译成功！',
            partial: '法术部分编译，需要审查',
            failed: '法术编译失败',
            unsafe: '警告：高危法术！'
        };
        showToast(statusMessages[result.status] || result.status,
            result.status === 'compiled' ? 'success' :
            result.status === 'unsafe' ? 'error' : 'warning');
    }

    function onClear() {
        state.stages = {
            model: { stage: 'model', nodes: [] },
            purify: { stage: 'purify', nodes: [] },
            infuse: { stage: 'infuse', nodes: [] },
            release: { stage: 'release', nodes: [] }
        };
        state.nodeCounter = 0;
        state.compileResult = null;
        updateStageUI();
        updateCompileResult(null);
        showToast('已清空 / Cleared');
    }

    function onPresetClick(e) {
        var li = e.target.closest('li');
        if (!li) return;

        var spellName = li.getAttribute('data-spell');
        if (!spellName) return;

        // 加载预设法术
        loadPresetSpell(spellName);
    }

    function loadPresetSpell(spellName) {
        var presets = {
            fireball: {
                intent: '远程伤害',
                stages: [
                    { stage: 'model', nodes: [{ instance_id: 'model-1', node_id: 'model_sphere' }] },
                    { stage: 'purify', nodes: [{ instance_id: 'purify-1', node_id: 'purify_fire' }] },
                    { stage: 'infuse', nodes: [{ instance_id: 'infuse-1', node_id: 'infuse_standard' }] },
                    { stage: 'release', nodes: [{ instance_id: 'release-1', node_id: 'release_projectile' }] }
                ]
            },
            lightning: {
                intent: '雷电伤害',
                stages: [
                    { stage: 'model', nodes: [{ instance_id: 'model-1', node_id: 'model_sphere' }] },
                    { stage: 'purify', nodes: [
                        { instance_id: 'purify-1', node_id: 'purify_fire' },
                        { instance_id: 'purify-2', node_id: 'purify_wind' },
                        { instance_id: 'purify-3', node_id: 'purify_water' }
                    ]},
                    { stage: 'infuse', nodes: [{ instance_id: 'infuse-1', node_id: 'infuse_compress' }] },
                    { stage: 'release', nodes: [{ instance_id: 'release-1', node_id: 'release_activate' }] }
                ]
            },
            mire: {
                intent: '控制地形',
                stages: [
                    { stage: 'model', nodes: [{ instance_id: 'model-1', node_id: 'model_area' }] },
                    { stage: 'purify', nodes: [
                        { instance_id: 'purify-1', node_id: 'purify_earth' },
                        { instance_id: 'purify-2', node_id: 'purify_water' }
                    ]},
                    { stage: 'infuse', nodes: [{ instance_id: 'infuse-1', node_id: 'infuse_maintain' }] },
                    { stage: 'release', nodes: [{ instance_id: 'release-1', node_id: 'release_diffuse' }] }
                ]
            },
            multi_wind_blade: {
                intent: '多重切割',
                stages: [
                    { stage: 'model', nodes: [{ instance_id: 'model-1', node_id: 'model_blade' }] },
                    { stage: 'purify', nodes: [{ instance_id: 'purify-1', node_id: 'purify_wind' }] },
                    { stage: 'infuse', nodes: [{ instance_id: 'infuse-1', node_id: 'infuse_multi' }] },
                    { stage: 'release', nodes: [{ instance_id: 'release-1', node_id: 'release_projectile' }] }
                ]
            }
        };

        var preset = presets[spellName];
        if (!preset) {
            showToast('预设法术不存在 / Preset not found', 'error');
            return;
        }

        // 重置并加载
        state.stages = {
            model: { stage: 'model', nodes: [] },
            purify: { stage: 'purify', nodes: [] },
            infuse: { stage: 'infuse', nodes: [] },
            release: { stage: 'release', nodes: [] }
        };
        state.nodeCounter = 0;

        preset.stages.forEach(function(stage) {
            state.stages[stage.stage].nodes = stage.nodes.map(function(n) {
                state.nodeCounter++;
                return {
                    instance_id: n.instance_id,
                    node_id: n.node_id
                };
            });
        });

        updateStageUI();

        // 自动编译
        if (typeof SpellCompiler !== 'undefined') {
            var result = SpellCompiler.compileSpell(preset.stages, preset.intent);
            state.compileResult = result;
            updateCompileResult(result);
        }

        showToast('已加载: ' + spellName);
    }

    // ================================================================
    // 工具函数
    // ================================================================

    /**
     * @brief 转义HTML特殊字符，防止XSS注入（引用 utils.js 中的全局版本）
     * fix: 统一使用 Lv00WebApp.prototype._escapeHtml，避免重复定义
     */
    var _escapeHtml = Lv00WebApp.prototype._escapeHtml;

    function showToast(message, type) {
        type = type || 'info';
        if (window.lv00App && window.lv00App.showToast) {
            window.lv00App.showToast(message, type);
        } else {
            console.log('[' + type + '] ' + message);
        }
    }

    // ================================================================
    // 导出
    // ================================================================

    window.MagicModule = {
        init: init,
        getState: function() { return state; },
        clear: onClear,
        compile: onCompile
    };

    // 初始化
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', init);
    } else {
        init();
    }

})();
