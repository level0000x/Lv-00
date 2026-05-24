/**
 * @file spell_compiler.js
 * @brief 轰界法术编译器 (Hongjie Spell Compiler)
 * @description 从 creat-magic-main 项目移植的法术编译系统。
 *              支持四阶段编译流程：开模 -> 提纯 -> 灌注 -> 释放。
 *              包含节点库管理、法术编译、风险评估、雷达图评分等功能。
 *              提供节点查找、阶段管理、化合物检测等核心 API。
 *
 * @module spell_compiler
 * @version 1.0.0
 * @requires 严格 ES5 语法
 */

var SpellCompiler = (function() {
    'use strict';

    // ================================================================
    // 常量定义
    // ================================================================

    var STAGE_ORDER = ['model', 'purify', 'infuse', 'release'];
    var STAGE_LABELS = {
        model: '开模',
        purify: '提纯',
        infuse: '灌注',
        release: '释放'
    };
    var RADAR_LABELS = {
        power: '威力',
        stability: '稳定性',
        learnability: '学习难度',
        mana_efficiency: '魔力消耗',
        versatility: '泛用性',
        academic_value: '学术价值'
    };
    var BASE_SCORE = {
        power: 48,
        stability: 62,
        learnability: 60,
        mana_efficiency: 58,
        versatility: 55,
        academic_value: 45
    };
    var DIFFICULTY_LIMITS = {
        0: 18, 1: 44, 2: 60, 3: 90, 4: 112, 5: 140,
        6: 176, 7: 218, 8: 260, 9: 320, 10: 999
    };
    var DIFFICULTY_STEPS = {
        0: 18, 1: 20, 2: 24, 3: 28, 4: 32, 5: 36,
        6: 42, 7: 50, 8: 60, 9: 72, 10: 999
    };
    var RISK_TEXT = {
        thermal_spread: '热场外溢',
        edge_control: '刃形边界散逸',
        area_spill: '范围外溢',
        block_path: '阻挡己方路径',
        slip: '地面湿滑',
        conductive_context: '环境导电',
        drift: '路径偏移',
        terrain_damage: '地形破坏',
        cognitive_load: '认知负荷',
        overpressure: '过压失控',
        attention_parallel: '注意力并发不足',
        miss: '投射落点偏移',
        burst: '激发爆发',
        governance_review: '治理审查',
        high_voltage: '高压误伤',
        internal_boundary: '体内边界突破',
        domain_override: '领域统摄失控'
    };

    // ================================================================
    // 节点库
    // ================================================================

    var nodeLibrary = null;

    function loadNodeLibrary(jsonData) {
        nodeLibrary = jsonData;
        return nodeLibrary;
    }

    /**
     * 按 ID 查找节点
     * @param {string} nodeId - 节点 ID
     * @param {Object} [nodeMap] - 可选：预构建的节点索引表，传入后以 O(1) 查找
     * @returns {Object|null} 找到的节点对象或 null
     */
    function getNodeById(nodeId, nodeMap) {
        // 如果传入了预构建索引表，直接 O(1) 查找
        if (nodeMap) {
            return nodeMap[nodeId] || null;
        }
        if (!nodeLibrary || !nodeLibrary.nodes) return null;
        for (var i = 0; i < nodeLibrary.nodes.length; i++) {
            if (nodeLibrary.nodes[i].id === nodeId) {
                return nodeLibrary.nodes[i];
            }
        }
        return null;
    }

    /**
     * 构建以节点 ID 为键的索引表，将 O(n) 线性查找优化为 O(1) 散列查找
     * @returns {Object} 节点 ID -> 节点对象的映射表
     */
    function buildNodeMap() {
        var map = {};
        if (nodeLibrary && nodeLibrary.nodes) {
            for (var i = 0; i < nodeLibrary.nodes.length; i++) {
                var node = nodeLibrary.nodes[i];
                if (node && node.id) {
                    map[node.id] = node;
                }
            }
        }
        return map;
    }

    function getNodesByStage(stageId) {
        if (!nodeLibrary || !nodeLibrary.nodes) return [];
        return nodeLibrary.nodes.filter(function(node) {
            return node.stage === stageId;
        });
    }

    // ================================================================
    // 编译上下文
    // ================================================================

    function CompileContext() {
        this.mold = null;
        this.moldTags = [];
        this.element = null;
        this.elementKeys = [];
        this.system = null;
        this.compound = null;
        this.infusion = null;
        this.infusionTags = [];
    }

    // ================================================================
    // 编译主函数
    // ================================================================

    function compileSpell(stages, intent) {
        if (!nodeLibrary) {
            return { status: 'failed', error: '节点库未加载' };
        }

        // 构建节点 ID 索引表，将后续 O(n) 查找优化为 O(1)
        var nodeMap = buildNodeMap();

        var context = new CompileContext();
        var issues = [];
        var stageOutcomes = [];
        // 手动拷贝 BASE_SCORE，兼容 ES5 环境（避免使用 Object.assign）
        var score = {};
        for (var sk in BASE_SCORE) {
            if (BASE_SCORE.hasOwnProperty(sk)) {
                score[sk] = BASE_SCORE[sk];
            }
        }
        var riskTags = [];
        var compiledNodes = [];

        // 验证请求
        issues = issues.concat(validateRequest(stages));

        // 如果没有错误，编译各阶段
        var hasErrors = issues.some(function(issue) { return issue.severity === 'error'; });

        if (!hasErrors) {
            for (var i = 0; i < STAGE_ORDER.length; i++) {
                var stageId = STAGE_ORDER[i];
                var stage = findStage(stages, stageId);
                var outcome = compileStage(stageId, stage, context, nodeMap);
                stageOutcomes.push(outcome);

                // 收集节点信息和分数
                for (var j = 0; j < stage.nodes.length; j++) {
                    // 安全检查：跳过数组中可能为 null/undefined 的条目
                    var nodeEntry = stage.nodes[j];
                    if (!nodeEntry) continue;
                    var nodeId = nodeEntry.node_id;
                    var node = getNodeById(nodeId, nodeMap);
                    if (node) {
                        compiledNodes.push(node);
                        mergeScore(score, node.score_bias);
                        riskTags = riskTags.concat(node.risk_tags || []);
                    }
                }

                // 处理复合法术分数
                if (stageId === 'purify' && context.compound) {
                    mergeScore(score, context.compound.score_bias);
                    riskTags = riskTags.concat(context.compound.risk_tags || []);
                }
            }

            // 语义检查
            issues = issues.concat(semanticIssues(context));
        }

        // 评估法术等级
        var assessment = assessSpellLevel(compiledNodes, context.compound);

        // 确定状态
        var status = determineStatus(issues, score, hasErrors);

        // 生成法术名称和摘要
        var spellName = nameSpell(stageOutcomes, context, assessment);
        var summary = summarize(stageOutcomes, status, assessment);

        // 构建雷达图
        var radar = buildRadar(score);

        // 构建法术卡片
        var card = buildCard(spellName, summary, stageOutcomes, issues, riskTags, assessment);

        return {
            status: status,
            spell_name: spellName,
            summary: summary,
            spell_level: assessment,
            stage_outcomes: stageOutcomes,
            issues: issues,
            radar: radar,
            spell_card: card
        };
    }

    // ================================================================
    // 验证函数
    // ================================================================

    function validateRequest(stages) {
        var issues = [];
        var seenStages = {};

        // 检查重复阶段
        for (var i = 0; i < stages.length; i++) {
            var stageId = stages[i].stage;
            if (seenStages[stageId]) {
                issues.push(createIssue('stage.duplicate', 'error', stageId, null,
                    STAGE_LABELS[stageId] + '阶段重复。', '每个固定阶段只能出现一次。'));
            }
            seenStages[stageId] = true;
        }

        // 检查必需阶段
        for (var j = 0; j < STAGE_ORDER.length; j++) {
            var requiredStage = STAGE_ORDER[j];
            var stage = findStage(stages, requiredStage);

            if (!stage) {
                issues.push(createIssue('stage.missing', 'error', requiredStage, null,
                    '缺少' + STAGE_LABELS[requiredStage] + '阶段。', '补齐四个固定阶段后再编译。'));
            } else if (!stage.nodes || stage.nodes.length === 0) {
                issues.push(createIssue('stage.empty', 'error', requiredStage, null,
                    STAGE_LABELS[requiredStage] + '阶段没有节点。', '至少放入一个可产生阶段结果的节点。'));
            }
        }

        return issues;
    }

    function findStage(stages, stageId) {
        for (var i = 0; i < stages.length; i++) {
            if (stages[i].stage === stageId) {
                return stages[i];
            }
        }
        return null;
    }

    function createIssue(ruleId, severity, stage, nodeInstanceId, message, suggestion) {
        return {
            rule_id: ruleId,
            severity: severity,
            stage: stage,
            node_instance_id: nodeInstanceId,
            message: message,
            suggestion: suggestion
        };
    }

    // ================================================================
    // 阶段编译
    // ================================================================

    function compileStage(stageId, stage, context, nodeMap) {
        var definitions = [];
        for (var i = 0; i < stage.nodes.length; i++) {
            var node = getNodeById(stage.nodes[i].node_id, nodeMap);
            if (node) {
                definitions.push(node);
            }
        }

        var result = '';
        var tags = [];

        if (stageId === 'model') {
            context.moldTags = collectOutputs(definitions);
            context.mold = modelResult(context.moldTags);
            result = context.mold;
        } else if (stageId === 'purify') {
            context.elementKeys = collectFirstOutputs(definitions);
            context.compound = resolveCompound(context.elementKeys);
            context.system = context.elementKeys.length === 1 ? systemFromKey(context.elementKeys[0]) : null;
            context.element = context.compound ? context.compound.result : singleElementResult(context.elementKeys);
            result = context.element;
        } else if (stageId === 'infuse') {
            context.infusionTags = collectOutputs(definitions);
            context.infusion = infusionResult(context.mold, context.element, definitions);
            result = context.infusion;
        } else {
            var releaseNames = definitions.map(function(n) { return n.name; });
            result = releaseNames.join('、') + '完成';
        }

        // 收集所有标签
        for (var j = 0; j < definitions.length; j++) {
            tags = tags.concat(definitions[j].tags || []);
        }

        return {
            stage: stageId,
            label: STAGE_LABELS[stageId],
            result: result || '未形成结果',
            node_instance_ids: stage.nodes.map(function(n) { return n.instance_id; }),
            tags: uniqueArray(tags)
        };
    }

    function collectOutputs(nodes) {
        var outputs = [];
        for (var i = 0; i < nodes.length; i++) {
            if (nodes[i].outputs) {
                outputs = outputs.concat(nodes[i].outputs);
            }
        }
        return uniqueArray(outputs);
    }

    function collectFirstOutputs(nodes) {
        var outputs = [];
        for (var i = 0; i < nodes.length; i++) {
            if (nodes[i].outputs && nodes[i].outputs.length > 0) {
                outputs.push(nodes[i].outputs[0]);
            }
        }
        return outputs;
    }

    function uniqueArray(arr) {
        var seen = {};
        var result = [];
        for (var i = 0; i < arr.length; i++) {
            if (!seen[arr[i]]) {
                seen[arr[i]] = true;
                result.push(arr[i]);
            }
        }
        return result;
    }

    // ================================================================
    // 结果解析
    // ================================================================

    function modelResult(tags) {
        if (tags.indexOf('domain') >= 0) return '领域框架';
        if (tags.indexOf('internal') >= 0) return '体内边界';
        if (tags.indexOf('origin') >= 0) return '底层原理模型';
        if (tags.indexOf('essence') >= 0) return '本质框架';
        if (tags.indexOf('existing') >= 0) return '既有对象';
        if (tags.indexOf('large_area') >= 0) return '广域边界';
        if (tags.indexOf('adaptive') >= 0) return '自适应结构';
        if (tags.indexOf('focused') >= 0) return '聚焦模具';
        if (tags.indexOf('core') >= 0) return '核心应用模具';
        if (tags.indexOf('sphere') >= 0 && tags.indexOf('projectile') >= 0) return '球形弹体';
        if (tags.indexOf('blade') >= 0) return '刃形模具';
        if (tags.indexOf('area') >= 0) return '范围场域';
        if (tags.indexOf('line') >= 0) return '线形路径';
        if (tags.indexOf('wall') >= 0) return '墙体模具';
        return '复合模具';
    }

    function singleElementResult(keys) {
        var names = {
            fire: '火系法术',
            water: '水系法术',
            wind: '风系法术',
            earth: '土系法术',
            ether: '以太操作法术',
            chaos: '混沌系法术',
            vector: '引力系法术'
        };
        if (!keys || keys.length === 0) return '';
        if (keys.length === 1) return names[keys[0]] || keys[0];
        return '未登记复合属性';
    }

    function systemFromKey(key) {
        var systems = {
            fire: 'fire', water: 'water', wind: 'wind', earth: 'earth',
            ether: 'ether', chaos: 'ether', vector: 'ether'
        };
        return systems[key] || key;
    }

    function resolveCompound(keys) {
        if (!keys || keys.length < 2 || !nodeLibrary.compounds) return null;
        var primary = keys[0];
        var secondary = keys[1];
        var catalyst = keys.length > 2 ? keys[2] : null;

        for (var i = 0; i < nodeLibrary.compounds.length; i++) {
            var rule = nodeLibrary.compounds[i];
            if (rule.primary === primary && rule.secondary === secondary && rule.catalyst === catalyst) {
                return rule;
            }
        }
        return null;
    }

    function infusionResult(mold, element, nodes) {
        if (!mold || !element) return '';
        var technique = nodes.map(function(n) { return n.name; }).join('、');
        var hasMulti = nodes.some(function(n) {
            return n.tags && n.tags.indexOf('多重') >= 0;
        });
        if (hasMulti) {
            return '多重' + element + '灌注至' + mold;
        }
        return element + '经' + technique + '进入' + mold;
    }

    // ================================================================
    // 语义检查
    // ================================================================

    function semanticIssues(context) {
        var issues = [];
        if (!context.mold) {
            issues.push(createIssue('model.no_result', 'error', 'model', null,
                '开模阶段未形成模具。', '选择球形、刃形、范围、线形或墙体节点。'));
        }
        if (!context.element) {
            issues.push(createIssue('purify.no_result', 'error', 'purify', null,
                '提纯阶段未形成元素结果。', '至少选择一个元素提纯节点。'));
        }
        if (context.infusion && (!context.mold || !context.element)) {
            issues.push(createIssue('infuse.missing_input', 'error', 'infuse', null,
                '灌注缺少模具或元素输入。', '先让开模和提纯阶段形成结果。'));
        }
        if (!context.infusion) {
            issues.push(createIssue('release.missing_infusion', 'error', 'release', null,
                '释放缺少灌注完成的法术结构。', '让灌注阶段消费模具和元素并形成灌注结果。'));
        }
        return issues;
    }

    // ================================================================
    // 分数计算
    // ================================================================

    function mergeScore(score, bias) {
        if (!bias) return;
        for (var key in bias) {
            if (score.hasOwnProperty(key)) {
                score[key] += bias[key];
            }
        }
    }

    function clampScore(score) {
        var result = {};
        for (var key in score) {
            result[key] = Math.max(0, Math.min(100, score[key]));
        }
        return result;
    }

    // ================================================================
    // 法术等级评估
    // ================================================================

    function assessSpellLevel(nodes, compound) {
        var nodeTiers = nodes.map(function(n) { return n.tier || 0; });
        if (compound) {
            nodeTiers.push(compound.tier);
        }

        var baseTier = Math.max.apply(null, nodeTiers.concat([0]));
        var difficulty = nodes.reduce(function(sum, n) { return sum + (n.difficulty || 0); }, 0);
        if (compound) {
            difficulty += compound.difficulty || 0;
        }

        var limit = DIFFICULTY_LIMITS[baseTier] || 999;
        var rawBonus = 0;

        if (difficulty > limit) {
            var step = DIFFICULTY_STEPS[baseTier] || 1;
            rawBonus = 1 + Math.floor((difficulty - limit - 1) / step);
        }

        var rawTier = Math.min(10, baseTier + rawBonus);
        var tier = capTierByAnchor(baseTier, rawTier);
        var difficultyBonus = Math.max(0, tier - baseTier);

        var anchors = [];
        for (var i = 0; i < nodes.length; i++) {
            if (nodes[i].tier === baseTier && baseTier > 0) {
                anchors.push(nodes[i].name);
            }
        }
        if (compound && compound.tier === baseTier) {
            anchors.push(compound.result);
        }

        var reasons = [
            '最高节点等阶为 ' + baseTier + ' 阶。',
            '节点难度增幅合计 ' + difficulty + '，当前阶容量为 ' + limit + '。'
        ];
        if (difficultyBonus > 0) {
            reasons.push('难度溢出使法术上浮 ' + difficultyBonus + ' 阶。');
        }
        if (rawTier !== tier) {
            reasons.push('高阶需要本质、体内或领域锚点，已按最高锚点封顶。');
        }
        if (baseTier === 10) {
            reasons.push('领域锚点直接锁定十阶法术尝试。');
        }

        return {
            tier: tier,
            label: tier + '阶',
            base_tier: baseTier,
            difficulty: difficulty,
            difficulty_limit: limit,
            difficulty_bonus: difficultyBonus,
            anchor_nodes: uniqueArray(anchors),
            reasons: reasons
        };
    }

    function capTierByAnchor(baseTier, rawTier) {
        if (baseTier < 7) return Math.min(rawTier, 6);
        if (baseTier < 9) return Math.min(rawTier, 8);
        if (baseTier < 10) return Math.min(rawTier, 9);
        return rawTier;
    }

    // ================================================================
    // 状态确定
    // ================================================================

    function determineStatus(issues, score, hasErrors) {
        if (hasErrors) return 'failed';

        var hasUnsafe = issues.some(function(issue) { return issue.severity === 'unsafe'; });
        if (hasUnsafe) return 'unsafe';

        var clampedScore = clampScore(score);
        if (clampedScore.stability < 45 || clampedScore.mana_efficiency < 45) {
            return 'partial';
        }

        return 'compiled';
    }

    // ================================================================
    // 雷达图构建
    // ================================================================

    function buildRadar(score) {
        var radar = [];
        var clampedScore = clampScore(score);

        for (var key in RADAR_LABELS) {
            var value = clampedScore[key];
            // 对于学习难度和魔力消耗，值越低越好，需要反转
            if (key === 'learnability' || key === 'mana_efficiency') {
                value = 100 - value;
            }

            radar.push({
                key: key,
                label: RADAR_LABELS[key],
                value: value,
                direction: (key === 'learnability' || key === 'mana_efficiency') ? 'higher_worse' : 'higher_better',
                reason: scoreReason(key, value)
            });
        }

        return radar;
    }

    function scoreReason(key, value) {
        if (key === 'learnability' || key === 'mana_efficiency') {
            if (value >= 70) return '该维度负担较高，需要优化节点组合。';
            if (value <= 40) return '该维度负担较低。';
            return '该维度处于可接受区间。';
        }
        if (value >= 70) return '节点组合对该维度有明显加成。';
        if (value <= 40) return '节点组合在该维度存在明显负担。';
        return '该维度处于可用但仍需优化的区间。';
    }

    // ================================================================
    // 法术命名和摘要
    // ================================================================

    function nameSpell(outcomes, context, assessment) {
        var mold = getOutcomeResult(outcomes, 'model');
        var element = getOutcomeResult(outcomes, 'purify');
        var infusion = getOutcomeResult(outcomes, 'infuse');

        // 特殊命名规则
        if (element === '风系法术' && mold.indexOf('刃') >= 0 && infusion.indexOf('多重') >= 0) {
            return '多重风刃';
        }
        if (element === '泥沼系法术') return '泥沼术';
        if (element === '雷电系法术') return '雷电术';

        // 固定法术名称映射
        var fixedNames = {
            fire: { 1: '小火球', 2: '火球术', 3: '大火球', 4: '炎爆术' },
            water: { 1: '水弹', 2: '水箭', 3: '水冲', 4: '激流' },
            wind: { 1: '风刃', 2: '风切', 3: '龙卷', 4: '暴风' },
            earth: { 1: '土弹', 2: '石刺', 3: '岩盾', 4: '地震' }
        };

        if (context.system && fixedNames[context.system]) {
            var name = fixedNames[context.system][assessment.tier];
            if (name) return name;
        }

        if (element && element.indexOf('法术', element.length - '法术'.length) !== -1) {
            return element;
        }

        return '未命名法术';
    }

    function getOutcomeResult(outcomes, stageId) {
        for (var i = 0; i < outcomes.length; i++) {
            if (outcomes[i].stage === stageId) {
                return outcomes[i].result;
            }
        }
        return '';
    }

    function summarize(outcomes, status, assessment) {
        var chain = outcomes.map(function(o) { return o.result; }).join(' → ');
        var prefix = '该法术链路可执行';
        if (status === 'failed') prefix = '该法术链路无法执行';
        else if (status === 'unsafe') prefix = '该法术链路可形成结果，但高危';
        else if (status === 'partial') prefix = '该法术链路需要审查';

        return prefix + '，判定为' + assessment.label + '：' + chain + '。';
    }

    // ================================================================
    // 法术卡片构建
    // ================================================================

    function buildCard(spellName, summary, outcomes, issues, riskTags, assessment) {
        var uniqueRisks = uniqueArray(riskTags).map(function(tag) {
            return RISK_TEXT[tag] || tag;
        });

        var suggestions = [];
        if (issues.length === 0) {
            suggestions.push('当前方案可作为普通四阶段法术 MVP 示例。');
        } else {
            var seenSuggestions = {};
            for (var i = 0; i < issues.length; i++) {
                if (issues[i].suggestion && !seenSuggestions[issues[i].suggestion]) {
                    seenSuggestions[issues[i].suggestion] = true;
                    suggestions.push(issues[i].suggestion);
                }
            }
        }

        return {
            title: spellName,
            summary: summary,
            chain: ['意图确认'].concat(outcomes.map(function(o) { return o.result; }), ['代价与风险校验']),
            conditions: [
                '四阶段均需形成阶段结果。',
                '灌注必须同时消费开模结果和提纯结果。',
                '释放必须基于已完成的灌注结构。',
                '法术阶数由最高节点等阶与难度增幅共同评定。'
            ],
            costs: [
                '法术等阶：' + assessment.label,
                '基础锚点：' + assessment.base_tier + '阶',
                '节点难度增幅：' + assessment.difficulty + '/' + assessment.difficulty_limit
            ],
            risks: uniqueRisks,
            suggestions: suggestions
        };
    }

    // ================================================================
    // 导出
    // ================================================================

    return {
        // 常量
        STAGE_ORDER: STAGE_ORDER,
        STAGE_LABELS: STAGE_LABELS,
        RADAR_LABELS: RADAR_LABELS,
        RISK_TEXT: RISK_TEXT,

        // 核心函数
        loadNodeLibrary: loadNodeLibrary,
        getNodeById: getNodeById,
        getNodesByStage: getNodesByStage,
        compileSpell: compileSpell,

        // 工具函数
        createIssue: createIssue
    };

})();

// 全局导出
if (typeof window !== 'undefined') {
    window.SpellCompiler = SpellCompiler;
}
