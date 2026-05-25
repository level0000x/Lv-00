/**
 * @file axiom_engine.js
 * @brief 魔法公理引擎 (Magic Axiom Engine)
 * @description 用公理系统来定义和验证魔法效果。提供基于公理的魔法规则引擎，
 *              包含元素枚举、能量阈值、元素反应矩阵、符文公理等核心数据结构。
 *              支持从公理推导定理，并用证明系统验证咒语的正确性。
 *
 * 核心概念：
 *   - 公理 (Axiom): 魔法的基础规则
 *   - 定理 (Theorem): 从公理推导出的效果
 *   - 证明 (Proof): 验证咒语的正确性
 *
 * @module axiom_engine
 * @version 1.0.0
 * @requires 严格 ES5 语法
 */

var MagicAxiomEngine = (function() {
    'use strict';

    // ================================================================
    // 元素枚举
    // ================================================================

    var Element = {
        FIRE: 'FIRE',
        WATER: 'WATER',
        AIR: 'AIR',
        EARTH: 'EARTH',
        ETHER: 'ETHER',
        NONE: 'NONE'
    };

    var ElementSymbol = {
        FIRE: '※',
        WATER: '○',
        AIR: '◇',
        EARTH: '□',
        ETHER: '☆',
        NONE: '?'
    };

    // ================================================================
    // 能量阈值
    // ================================================================

    var EnergyThreshold = {
        T1: 1,
        T2: 10,
        T3: 100,
        T4: 1000,
        T5: 10000,
        T6: 100000
    };

    // ================================================================
    // 元素反应
    // ================================================================

    var ElementReaction = {
        ENHANCE: 'ENHANCE',
        WEAKEN: 'WEAKEN',
        CONFLICT: 'CONFLICT',
        NONE: 'NONE'
    };

    var ReactionMatrix = {
        'FIRE+AIR': { type: ElementReaction.ENHANCE, factor: 1.2 },
        'FIRE+WATER': { type: ElementReaction.CONFLICT, factor: 0.5 },
        'FIRE+EARTH': { type: ElementReaction.ENHANCE, factor: 1.1 },
        'WATER+AIR': { type: ElementReaction.WEAKEN, factor: 0.8 },
        'WATER+EARTH': { type: ElementReaction.ENHANCE, factor: 1.3 },
        'AIR+EARTH': { type: ElementReaction.CONFLICT, factor: 0.6 }
    };

    // ================================================================
    // 符文公理
    // ================================================================

    function RuneAxiom(element, power) {
        this.type = 'RuneAxiom';
        this.element = element;
        this.power = power || 1;
        this.symbol = ElementSymbol[element];
    }

    RuneAxiom.prototype.evaluate = function() {
        return {
            element: this.element,
            power: this.power,
            symbol: this.symbol
        };
    };

    RuneAxiom.prototype.toString = function() {
        return this.symbol + '(' + this.power + ')';
    };

    // ================================================================
    // 元素反应公理
    // ================================================================

    function ElementReactionAxiom(rune1, rune2) {
        this.type = 'ElementReactionAxiom';
        this.rune1 = rune1;
        this.rune2 = rune2;
    }

    ElementReactionAxiom.prototype.evaluate = function() {
        var key = this.rune1.element + '+' + this.rune2.element;
        var reaction = ReactionMatrix[key] || { type: ElementReaction.NONE, factor: 1.0 };

        var basePower = this.rune1.power + this.rune2.power;

        var effectivePower;
        if (reaction.type === ElementReaction.ENHANCE) {
            effectivePower = basePower * reaction.factor;
        } else if (reaction.type === ElementReaction.WEAKEN) {
            effectivePower = basePower * reaction.factor;
        } else if (reaction.type === ElementReaction.CONFLICT) {
            effectivePower = basePower * (1 - reaction.factor);
        } else {
            effectivePower = basePower;
        }

        return {
            reaction: reaction.type,
            factor: reaction.factor,
            basePower: basePower,
            effectivePower: effectivePower
        };
    };

    // ================================================================
    // 开模公理
    // ================================================================

    function MoldingAxiom(runes) {
        this.type = 'MoldingAxiom';
        this.runes = runes || [];
    }

    MoldingAxiom.prototype.evaluate = function() {
        if (this.runes.length < 1 || this.runes.length > 9) {
            return { valid: false, error: '符文数量必须在 1-9 之间' };
        }

        var totalPower = 0;
        var elementCounts = {};

        for (var i = 0; i < this.runes.length; i++) {
            var rune = this.runes[i];
            totalPower += rune.power;
            elementCounts[rune.element] = (elementCounts[rune.element] || 0) + 1;
        }

        var stability = totalPower / this.runes.length;

        // 找出主元素
        var dominantElement = Element.NONE;
        var maxCount = 0;
        for (var el in elementCounts) {
            if (elementCounts[el] > maxCount) {
                maxCount = elementCounts[el];
                dominantElement = el;
            }
        }

        return {
            valid: true,
            runeCount: this.runes.length,
            totalPower: totalPower,
            stability: stability,
            dominantElement: dominantElement,
            elementCounts: elementCounts,
            structure: 'sphere'
        };
    };

    // ================================================================
    // 提纯公理
    // ================================================================

    function PurifyingAxiom(moldingResult, targetElement) {
        this.type = 'PurifyingAxiom';
        this.moldingResult = moldingResult;
        this.targetElement = targetElement;
    }

    PurifyingAxiom.prototype.evaluate = function() {
        var molding = this.moldingResult;

        if (!molding.valid) {
            return { valid: false, error: '开模阶段失败' };
        }

        var elementCount = molding.elementCounts[this.targetElement] || 0;
        var purity = elementCount / molding.runeCount;

        if (purity < 0.5 && elementCount === 0) {
            return {
                valid: false,
                error: '缺少目标元素 ' + this.targetElement
            };
        }

        return {
            valid: true,
            purity: purity,
            element: this.targetElement,
            hasMatchingElement: elementCount > 0
        };
    };

    // ================================================================
    // 灌注公理
    // ================================================================

    function InfusingAxiom(purifyingResult, threshold) {
        this.type = 'InfusingAxiom';
        this.purifyingResult = purifyingResult;
        this.threshold = threshold;
    }

    InfusingAxiom.prototype.evaluate = function() {
        var purifying = this.purifyingResult;

        if (!purifying.valid) {
            return { valid: false, error: '提纯阶段失败' };
        }

        var stability = this.purifyingResult.molding ? this.purifyingResult.molding.stability : 0.5;

        if (stability < 0.3) {
            return {
                valid: false,
                error: '稳定性不足，触发反噬',
                backlash: true
            };
        }

        var baseEnergy = EnergyThreshold[this.threshold] || 100;
        var energyMultiplier;

        if (stability >= 0.7) {
            energyMultiplier = 1.2;
        } else if (stability >= 0.5) {
            energyMultiplier = 1.0;
        } else {
            energyMultiplier = 0.8;
        }

        var effectiveEnergy = baseEnergy * energyMultiplier;

        return {
            valid: true,
            energy: effectiveEnergy,
            threshold: this.threshold,
            stability: stability,
            multiplier: energyMultiplier
        };
    };

    // ================================================================
    // 释放公理
    // ================================================================

    function ReleasingAxiom(infusingResult, range) {
        this.type = 'ReleasingAxiom';
        this.infusingResult = infusingResult;
        this.range = range || 10;
    }

    ReleasingAxiom.prototype.evaluate = function() {
        var infusing = this.infusingResult;

        if (!infusing.valid) {
            return { valid: false, error: '灌注阶段失败' };
        }

        var energy = infusing.energy;
        var purity = this.infusingResult.purifying ? this.infusingResult.purifying.purity : 1;

        var damage = Math.round(energy * purity / 10);
        var duration = Math.round(energy / 100);

        return {
            valid: true,
            damage: damage,
            range: this.range,
            duration: duration,
            energy: energy
        };
    };

    // ================================================================
    // 咒语定理
    // ================================================================

    function SpellTheorem(name, config) {
        this.name = name;
        this.config = config || {};
        this.axioms = [];
        this.proof = [];
        this.result = null;
        this.proven = false;
    }

    SpellTheorem.prototype.addAxiom = function(axiom) {
        this.axioms.push(axiom);
        return this;
    };

    SpellTheorem.prototype.addProof = function(step) {
        this.proof.push(step);
        return this;
    };

    SpellTheorem.prototype.prove = function() {
        if (this.axioms.length === 0) {
            this.result = { proven: false, error: '缺少公理' };
            return this.result;
        }

        // 执行证明
        var stepResults = [];

        for (var i = 0; i < this.axioms.length; i++) {
            var axiomResult = this.axioms[i].evaluate();
            stepResults.push({
                axiom: this.axioms[i],
                result: axiomResult
            });

            // 检查是否有错误
            if (axiomResult.error) {
                this.result = {
                    proven: false,
                    error: axiomResult.error,
                    backlash: axiomResult.backlash,
                    step: i + 1
                };
                return this.result;
            }
        }

        // 所有步骤都通过
        this.result = {
            proven: true,
            steps: stepResults,
            finalEffect: stepResults[stepResults.length - 1].result
        };
        this.proven = true;

        return this.result;
    };

    // ================================================================
    // 公理包
    // ================================================================

    function AxiomPack() {
        this.name = 'Lv00-Magic-Axiom-Pack';
        this.version = '1.0.0';
        this.theorems = {};
        this.registeredAxioms = {};
    }

    // 注册预定义咒语定理
    AxiomPack.prototype.registerTheorem = function(theorem) {
        this.theorems[theorem.name] = theorem;
    };

    // ================================================================
    // 辅助函数：安全获取公理的评估结果，避免脆弱的隐式依赖
    // 在调用 evaluate() 前检查公理是否存在且具有 evaluate 方法
    // ================================================================

    /**
     * 安全评估公理，防止因公理未正确添加而导致运行时错误
     * @param {Object} axiom - 需要评估的公理对象
     * @param {string} errorStage - 错误时提示的阶段名称
     * @returns {Object} 评估结果，失败时返回 valid: false
     */
    function safeEvaluateAxiom(axiom, errorStage) {
        if (axiom && typeof axiom.evaluate === 'function') {
            return axiom.evaluate();
        }
        return { valid: false, error: errorStage + '公理未正确添加，缺少 evaluate 方法' };
    }

    // ================================================================
    // 辅助函数：数据驱动的咒语定理构建器
    // 从配置对象统一构建五阶段的咒语定理，消除重复代码
    // ================================================================

    /**
     * 根据配置对象构建咒语定理
     * @param {Object} config - 咒语配置
     * @param {string} config.name - 咒语名称
     * @param {Array} config.runes - 符文公理数组
     * @param {string} config.element - 提纯目标元素
     * @param {string} config.threshold - 灌注能量阈值
     * @param {number} config.range - 释放范围
     * @param {Array<string>} config.proofs - 证明步骤文本数组
     * @returns {SpellTheorem} 构建完成的咒语定理
     */
    function buildSpellTheorem(config) {
        var theorem = new SpellTheorem(config.name);
        theorem.runes = config.runes;

        // 阶段1：开模公理
        theorem.addAxiom(new MoldingAxiom(theorem.runes));

        // 阶段2：提纯公理 — 安全评估前序公理结果
        theorem.addAxiom(new PurifyingAxiom(
            safeEvaluateAxiom(theorem.axioms[0], '开模'),
            config.element
        ));

        // 阶段3：灌注公理 — 安全评估前序公理结果
        theorem.addAxiom(new InfusingAxiom(
            safeEvaluateAxiom(theorem.axioms[1], '提纯'),
            config.threshold
        ));

        // 阶段4：释放公理 — 安全评估前序公理结果
        theorem.addAxiom(new ReleasingAxiom(
            safeEvaluateAxiom(theorem.axioms[2], '灌注'),
            config.range
        ));

        // 添加证明步骤
        for (var i = 0; i < config.proofs.length; i++) {
            theorem.addProof(config.proofs[i]);
        }

        return theorem;
    }

    // 创建火球术
    AxiomPack.prototype.createFireballTheorem = function() {
        return buildSpellTheorem({
            name: 'Fireball',
            runes: [
                new RuneAxiom(Element.FIRE, 1),
                new RuneAxiom(Element.FIRE, 1),
                new RuneAxiom(Element.FIRE, 1)
            ],
            element: Element.FIRE,
            threshold: 'T3',
            range: 50,
            proofs: [
                '1. Molding establishes structure: sphere',
                '2. Purifying confirms fire element',
                '3. Infusing at T3 threshold',
                '4. Releasing projects to target'
            ]
        });
    };

    // 创建冰锥术
    AxiomPack.prototype.createIceShardTheorem = function() {
        return buildSpellTheorem({
            name: 'IceShard',
            runes: [
                new RuneAxiom(Element.WATER, 1),
                new RuneAxiom(Element.AIR, 1),
                new RuneAxiom(Element.WATER, 3)
            ],
            element: Element.WATER,
            threshold: 'T2',
            range: 30,
            proofs: [
                '1. Water+Air compound molding',
                '2. Ice purification at -10°C',
                '3. Sharp shard formation',
                '4. High-velocity release'
            ]
        });
    };

    // 创建闪电术
    AxiomPack.prototype.createLightningTheorem = function() {
        return buildSpellTheorem({
            name: 'Lightning',
            runes: [
                new RuneAxiom(Element.AIR, 2),
                new RuneAxiom(Element.AIR, 1),
                new RuneAxiom(Element.AIR, 1)
            ],
            element: Element.AIR,
            threshold: 'T3',
            range: 60,
            proofs: [
                '1. Air concentration molding',
                '2. Electrical charge purification',
                '3. Lightning channel formation',
                '4. Target seeking release'
            ]
        });
    };

    // 创建土墙术
    AxiomPack.prototype.createEarthWallTheorem = function() {
        return buildSpellTheorem({
            name: 'EarthWall',
            runes: [
                new RuneAxiom(Element.EARTH, 1),
                new RuneAxiom(Element.EARTH, 1),
                new RuneAxiom(Element.EARTH, 1),
                new RuneAxiom(Element.EARTH, 1)
            ],
            element: Element.EARTH,
            threshold: 'T2',
            range: 20,
            proofs: [
                '1. Four-point earth molding',
                '2. Solid structure purification',
                '3. Barrier reinforcement',
                '4. Defensive release'
            ]
        });
    };

    // 创建治疗术
    AxiomPack.prototype.createHealTheorem = function() {
        return buildSpellTheorem({
            name: 'Heal',
            runes: [
                new RuneAxiom(Element.WATER, 1),
                new RuneAxiom(Element.WATER, 1),
                new RuneAxiom(Element.WATER, 1),
                new RuneAxiom(Element.WATER, 2)
            ],
            element: Element.WATER,
            threshold: 'T2',
            range: 10,
            proofs: [
                '1. Pure water concentration',
                '2. Life force purification',
                '3. Healing energy infusion',
                '4. Target restoration'
            ]
        });
    };

    // 初始化预定义咒语
    AxiomPack.prototype.init = function() {
        this.registerTheorem(this.createFireballTheorem());
        this.registerTheorem(this.createIceShardTheorem());
        this.registerTheorem(this.createLightningTheorem());
        this.registerTheorem(this.createEarthWallTheorem());
        this.registerTheorem(this.createHealTheorem());
    };

    // 获取咒语定理
    AxiomPack.prototype.getTheorem = function(name) {
        return this.theorems[name];
    };

    // 列出所有咒语
    AxiomPack.prototype.listTheorems = function() {
        return Object.keys(this.theorems);
    };

    // ================================================================
    // 自定义咒语创建
    // ================================================================

    AxiomPack.prototype.createCustomSpell = function(name, runes, element, threshold, range) {
        var theorem = new SpellTheorem(name);

        // 转换符文
        theorem.runes = runes.map(function(r) {
            return new RuneAxiom(r.element, r.power);
        });

        theorem.addAxiom(new MoldingAxiom(theorem.runes));
        theorem.addAxiom(new PurifyingAxiom(
            theorem.axioms[0].evaluate(),
            element
        ));
        theorem.addAxiom(new InfusingAxiom(
            theorem.axioms[1].evaluate(),
            threshold
        ));
        theorem.addAxiom(new ReleasingAxiom(
            theorem.axioms[2].evaluate(),
            range
        ));

        this.registerTheorem(theorem);
        return theorem;
    };

    // ================================================================
    // 领域系统
    // ================================================================

    function DomainAxiom(name, center, radius, rules) {
        this.name = name;
        this.center = center || { x: 0, y: 0 };
        this.radius = radius || 100;
        this.rules = rules || [];
        this.active = false;
    }

    DomainAxiom.prototype.contains = function(point) {
        var dx = point.x - this.center.x;
        var dy = point.y - this.center.y;
        var distance = Math.sqrt(dx * dx + dy * dy);
        return distance <= this.radius;
    };

    /**
     * 对目标应用领域内的所有规则
     * DomainAxiom 定义的领域通过 contains() 判断是否包含目标，
     * 再通过 applyRules() 对包含在内的目标逐一应用领域内注册的规则。
     * @param {Object} target - 需要应用规则的目标对象（通常包含坐标等属性）
     * @returns {Array} 所有规则应用后产生的效果数组，每项为 rule.apply(target) 的返回结果
     */
    DomainAxiom.prototype.applyRules = function(target) {
        var effects = [];
        for (var i = 0; i < this.rules.length; i++) {
            var rule = this.rules[i];
            effects.push(rule.apply(target));
        }
        return effects;
    };

    // ================================================================
    // 导出
    // ================================================================

    return {
        // 枚举
        Element: Element,
        ElementSymbol: ElementSymbol,
        EnergyThreshold: EnergyThreshold,
        ElementReaction: ElementReaction,

        // 类
        RuneAxiom: RuneAxiom,
        ElementReactionAxiom: ElementReactionAxiom,
        MoldingAxiom: MoldingAxiom,
        PurifyingAxiom: PurifyingAxiom,
        InfusingAxiom: InfusingAxiom,
        ReleasingAxiom: ReleasingAxiom,
        SpellTheorem: SpellTheorem,
        DomainAxiom: DomainAxiom,
        AxiomPack: AxiomPack,

        // 工厂函数
        createAxiomPack: function() {
            var pack = new AxiomPack();
            pack.init();
            return pack;
        },

        // 快捷方法
        createRune: function(element, power) {
            return new RuneAxiom(element, power);
        },

        proveSpell: function(name, axiomPack) {
            var theorem = axiomPack.getTheorem(name);
            if (!theorem) {
                return { proven: false, error: '咒语 ' + name + ' 不存在' };
            }
            return theorem.prove();
        }
    };

})();

if (typeof window !== 'undefined') {
    window.MagicAxiomEngine = MagicAxiomEngine;
}
