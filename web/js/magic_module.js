/**
 * @file magic_module.js
 * @brief 编程魔法模块 (Magic Programming Module)
 * @description 基于 Lv-00 核心系统的编程魔法模拟器。
 *              包含魔法元素系统、符文系统、魔法阵系统、咒语系统、
 *              咒语书系统、咏唱系统和领域系统共 7 个子模块。
 *              魔法效果通过几何约束图和函数块系统实现，
 *              将数学概念映射为可交互的魔法体验。
 *
 * @module Lv00Magic
 * @version 1.0.0
 * @author Lv-00 Team
 * @since 2026-05-22
 * @requires 严格 ES5 语法（无 class, const, let, arrow functions）
 */

var Lv00Magic = (function() {
    'use strict';

    // ================================================================
    // 魔法元素枚举
    // ================================================================

    var MagicElement = {
        NONE: 0,
        FIRE: 1,      // 火元素 - 释放与热效应
        WATER: 2,     // 水元素 - 流动与相态调节
        AIR: 3,       // 风元素 - 运动与压差
        EARTH: 4,     // 土元素 - 结构与承载
        ETHER: 5      // 以太 - 第五元素
    };

    var ElementName = {
        0: '无属性',
        1: '火',
        2: '水',
        3: '风',
        4: '土',
        5: '以太'
    };

    // 元素反应矩阵
    var ElementReaction = {
        NONE: 0,
        ENHANCE: 1,    // 协同增强
        WEAKEN: 2,     // 相互削弱
        CONFLICT: 3    // 对立冲突
    };

    var ReactionName = {
        0: '无反应',
        1: '增强',
        2: '削弱',
        3: '冲突'
    };

    // 元素反应矩阵
    var _elementReactionMatrix = [
        [0, 0, 0, 0, 0, 0],           // NONE
        [0, 0, 3, 1, 1, 0],           // FIRE
        [0, 3, 0, 2, 1, 0],           // WATER
        [0, 1, 2, 0, 3, 0],          // AIR
        [0, 1, 1, 3, 0, 0],          // EARTH
        [0, 0, 0, 0, 0, 0]           // ETHER
    ];

    // ================================================================
    // 施法阶段枚举
    // ================================================================

    var SpellStage = {
        MOLDING: 0,    // 开模
        PURIFYING: 1,  // 提纯
        INFUSING: 2,   // 灌注
        RELEASING: 3   // 释放
    };

    var StageName = {
        0: '开模',
        1: '提纯',
        2: '灌注',
        3: '释放'
    };

    // ================================================================
    // 施法状态枚举
    // ================================================================

    var SpellStatus = {
        IDLE: 0,       // 空闲
        CASTING: 1,    // 施法中
        SUCCESS: 2,    // 成功
        FAILED: 3,     // 失败
        BACKLASH: 4   // 反噬
    };

    var StatusName = {
        0: '空闲',
        1: '施法中',
        2: '成功',
        3: '失败',
        4: '反噬'
    };

    // ================================================================
    // 纯度等级
    // ================================================================

    var PurityLevel = {
        RAW: 0,         // 原始混合 < 30%
        COARSE: 1,     // 粗提纯 30-60%
        STANDARD: 2,   // 标准纯 60-85%
        HIGH: 3,       // 高纯 85-95%
        ULTRA: 4,      // 极纯 95-99%
        THEORETICAL: 5 // 理论纯 > 99%
    };

    var PurityValue = [0.15, 0.45, 0.725, 0.9, 0.97, 0.995];
    var PurityName = {
        0: '原始',
        1: '粗提纯',
        2: '标准',
        3: '高纯',
        4: '极纯',
        5: '理论纯'
    };

    // ================================================================
    // 能量阈值
    // ================================================================

    var EnergyThreshold = {
        T1: 0,   // 微效 1-10 E_u
        T2: 1,   // 弱效 10-100 E_u
        T3: 2,   // 中效 100-1k E_u
        T4: 3,   // 强效 1k-10k E_u
        T5: 4,   // 极效 10k-100k E_u
        T6: 5    // 超限 > 100k E_u
    };

    var ThresholdEnergy = [1, 10, 100, 1000, 10000, 100000];
    var ThresholdName = {
        0: 'T1 微效',
        1: 'T2 弱效',
        2: 'T3 中效',
        3: 'T4 强效',
        4: 'T5 极效',
        5: 'T6 超限'
    };

    // ================================================================
    // 魔法阵约束类型
    // ================================================================

    var ArrayConstraintType = {
        CONNECTION: 0,     // 连接：能量流动路径
        ENHANCEMENT: 1,   // 增强：元素协同
        CONFLICT: 2,      // 冲突：元素对立
        INTERSECTION: 3,  // 相交：能量汇聚
        CONTAINMENT: 4,   // 包含：区域包围
        BOUNDARY: 5,      // 边界：结界边缘
        CHANNEL: 6,       // 通道：能量传输线
        FOCUS: 7         // 焦点：能量集中点
    };

    var ArrayConstraintName = {
        0: '连接',
        1: '增强',
        2: '冲突',
        3: '相交',
        4: '包含',
        5: '边界',
        6: '通道',
        7: '焦点'
    };

    // ================================================================
    // 限制级别
    // ================================================================

    var RestrictionLevel = {
        NONE: 0,         // 无限制
        LIMITED: 1,    // 限制级
        CONTROLLED: 2,  // 管制级
        FORBIDDEN: 3,   // 禁术级
        ABSOLUTE: 4     // 绝对禁术
    };

    var RestrictionName = {
        0: '无限制',
        1: '限制级',
        2: '管制级',
        3: '禁术级',
        4: '绝对禁术'
    };

    // ================================================================
    // 构造函数
    // ================================================================

    function Magic() {
        this.version = "1.0.0-magic";
        this.runeCount = 0;
        this.arrayCount = 0;
        this.spellCount = 0;
    }

    // ================================================================
    // 工具函数
    // ================================================================

    Magic.prototype._gcd = function(a, b) {
        if (typeof a !== 'number' || typeof b !== 'number') return 0;
        if (isNaN(a) || isNaN(b)) return 0;
        a = Math.abs(a);
        b = Math.abs(b);
        while (b !== 0) {
            var r = a % b;
            a = b;
            b = r;
        }
        return a;
    };

    // ================================================================
    // 符文系统
    // ================================================================

    /**
     * 创建符文
     * @param {number} value - 符文值
     * @param {number} element - 元素类型
     * @returns {Object} 符文对象
     */
    Magic.prototype.createRune = function(value, element) {
        this.runeCount++;
        return {
            id: this.runeCount,
            value: value,
            element: element,
            power: 1,
            name: null,
            symbol: null
        };
    };

    /**
     * 创建有理数符文
     */
    Magic.prototype.createRationalRune = function(num, den, element) {
        var g = this._gcd(num, den);
        num = num / g;
        den = den / g;
        var rune = this.createRune(num / den, element);
        rune.coordType = 'RATIONAL';
        rune.num = num;
        rune.den = den;
        return rune;
    };

    /**
     * 创建代数符文
     */
    Magic.prototype.createAlgebraicRune = function(value, element) {
        var rune = this.createRune(value, element);
        rune.coordType = 'ALGEBRAIC';
        return rune;
    };

    /**
     * 获取符文值
     */
    Magic.prototype.getRuneValue = function(rune) {
        if (!rune) return 0;
        if (rune.coordType === 'RATIONAL') {
            return rune.num / rune.den;
        }
        return rune.value;
    };

    /**
     * 设置符文强度
     */
    Magic.prototype.setRunePower = function(rune, power) {
        if (!rune) return;
        rune.power = Math.max(1, Math.min(10, power));
    };

    // ================================================================
    // 符文序列
    // ================================================================

    Magic.prototype.createRuneSequence = function() {
        return {
            runes: [],
            add: function(rune) {
                this.runes.push(rune);
            },
            get: function(index) {
                return this.runes[index];
            },
            length: function() {
                return this.runes.length;
            },
            clear: function() {
                this.runes = [];
            }
        };
    };

    // ================================================================
    // 元素系统
    // ================================================================

    /**
     * 检查元素反应
     */
    Magic.prototype.checkElementReaction = function(e1, e2) {
        if (e1 < 0 || e1 > 5 || e2 < 0 || e2 > 5) {
            return ElementReaction.NONE;
        }
        return _elementReactionMatrix[e1][e2];
    };

    /**
     * 获取元素名称
     */
    Magic.prototype.getElementName = function(element) {
        return ElementName[element] || '未知';
    };

    /**
     * 获取反应名称
     */
    Magic.prototype.getReactionName = function(reaction) {
        return ReactionName[reaction] || '未知';
    };

    /**
     * 统计魔法阵中的元素
     */
    Magic.prototype.countElements = function(array, element) {
        if (!array || !array.runes) return 0;
        var count = 0;
        for (var i = 0; i < array.runes.length; i++) {
            if (array.runes[i].element === element) {
                count++;
            }
        }
        return count;
    };

    // ================================================================
    // 魔法阵系统
    // ================================================================

    /**
     * 创建魔法阵
     */
    Magic.prototype.createArray = function() {
        this.arrayCount++;
        return {
            id: this.arrayCount,
            runes: [],
            constraints: [],
            nextRuneId: 0,
            nextConstraintId: 0
        };
    };

    /**
     * 添加符文到魔法阵
     */
    Magic.prototype.addRuneToArray = function(array, rune) {
        if (!array || !rune) return -1;
        var runeId = array.nextRuneId++;
        var runeCopy = {
            id: runeId,
            value: rune.value,
            element: rune.element,
            power: rune.power,
            coordType: rune.coordType,
            num: rune.num,
            den: rune.den
        };
        array.runes.push(runeCopy);
        return runeId;
    };

    /**
     * 添加约束到魔法阵
     */
    Magic.prototype.addConstraintToArray = function(array, type, rune1Id, rune2Id) {
        if (!array) return -1;
        var constraintId = array.nextConstraintId++;
        array.constraints.push({
            id: constraintId,
            type: type,
            rune1: rune1Id,
            rune2: rune2Id
        });
        return constraintId;
    };

    /**
     * 计算魔法阵稳定性
     */
    Magic.prototype.calculateArrayStability = function(array) {
        if (!array || array.runes.length === 0) return 0;

        var stability = 1.0;
        var conflicts = 0;

        for (var i = 0; i < array.constraints.length; i++) {
            if (array.constraints[i].type === ArrayConstraintType.CONFLICT) {
                conflicts++;
            }
        }

        // 每有一个冲突约束，稳定性降低
        stability -= conflicts * 0.1;

        // 符文数量过少也不稳定
        if (array.runes.length < 3) {
            stability *= 0.5;
        }

        return Math.max(0, stability);
    };

    /**
     * 检查魔法阵元素平衡
     */
    Magic.prototype.checkArrayBalance = function(array) {
        if (!array) return false;

        var elementCounts = {0: 0, 1: 0, 2: 0, 3: 0, 4: 0, 5: 0};
        for (var i = 0; i < array.runes.length; i++) {
            var el = array.runes[i].element;
            if (elementCounts[el] !== undefined) {
                elementCounts[el]++;
            }
        }

        // 计算方差
        var total = array.runes.length;
        var mean = total / 5.0;
        var variance = 0;

        for (var e = 1; e <= 5; e++) {
            var diff = elementCounts[e] - mean;
            variance += diff * diff;
        }
        variance /= 5;

        return variance < mean * 2.0;
    };

    /**
     * 获取魔法阵信息
     */
    Magic.prototype.getArrayInfo = function(array) {
        if (!array) return null;

        var elementCounts = {1: 0, 2: 0, 3: 0, 4: 0, 5: 0};
        var totalPower = 0;

        for (var i = 0; i < array.runes.length; i++) {
            var el = array.runes[i].element;
            if (elementCounts[el] !== undefined) {
                elementCounts[el]++;
            }
            totalPower += array.runes[i].power;
        }

        return {
            id: array.id,
            runeCount: array.runes.length,
            constraintCount: array.constraints.length,
            stability: this.calculateArrayStability(array),
            balanced: this.checkArrayBalance(array),
            elementCounts: elementCounts,
            totalPower: totalPower
        };
    };

    // ================================================================
    // 咒语系统
    // ================================================================

    /**
     * 创建咒语
     */
    Magic.prototype.createSpell = function(name) {
        this.spellCount++;
        return {
            id: this.spellCount,
            name: name || '未命名咒语',
            description: '',
            difficulty: 1,
            inputCount: 0,
            outputCount: 1,

            // 施法阶段配置
            molding: null,          // 开模符文序列
            purifyingElement: MagicElement.FIRE,
            purifyingPurity: 0.8,
            infusingThreshold: EnergyThreshold.T2,
            releasingRange: 10,
            releasingDamage: 10,

            // 当前状态
            currentStage: SpellStage.MOLDING,
            status: SpellStatus.IDLE
        };
    };

    /**
     * 配置咒语开模阶段
     */
    Magic.prototype.configureSpellMolding = function(spell, sequence) {
        if (!spell) return false;
        spell.molding = {
            runes: sequence.runes.slice()
        };
        return true;
    };

    /**
     * 配置咒语提纯阶段
     */
    Magic.prototype.configureSpellPurifying = function(spell, element, purity) {
        if (!spell) return false;
        spell.purifyingElement = element;
        spell.purifyingPurity = Math.max(0, Math.min(1, purity));
        return true;
    };

    /**
     * 配置咒语灌注阶段
     */
    Magic.prototype.configureSpellInfusing = function(spell, thresholdLevel) {
        if (!spell) return false;
        spell.infusingThreshold = thresholdLevel;
        return true;
    };

    /**
     * 配置咒语释放阶段
     */
    Magic.prototype.configureSpellReleasing = function(spell, range, damage) {
        if (!spell) return false;
        spell.releasingRange = range;
        spell.releasingDamage = damage;
        return true;
    };

    /**
     * 执行咒语
     */
    Magic.prototype.castSpell = function(spell, array) {
        if (!spell || !array) {
            return {
                status: SpellStatus.FAILED,
                stage: SpellStage.MOLDING,
                message: '参数错误'
            };
        }

        spell.currentStage = SpellStage.MOLDING;
        spell.status = SpellStatus.CASTING;

        // 开模阶段
        if (!spell.molding || spell.molding.runes.length === 0) {
            spell.status = SpellStatus.FAILED;
            return {
                status: spell.status,
                stage: spell.currentStage,
                message: '开模失败：缺少符文配置'
            };
        }

        spell.currentStage = SpellStage.PURIFYING;

        // 提纯阶段 - 检查元素匹配
        var hasMatchingElement = false;
        for (var i = 0; i < array.runes.length; i++) {
            if (array.runes[i].element === spell.purifyingElement) {
                hasMatchingElement = true;
                break;
            }
        }

        if (!hasMatchingElement && spell.purifyingPurity > 0.5) {
            spell.status = SpellStatus.FAILED;
            return {
                status: spell.status,
                stage: spell.currentStage,
                message: '提纯失败：缺少匹配元素 "' + this.getElementName(spell.purifyingElement) + '"'
            };
        }

        spell.currentStage = SpellStage.INFUSING;

        // 灌注阶段 - 检查稳定性
        var stability = this.calculateArrayStability(array);
        if (stability < 0.3) {
            spell.status = SpellStatus.BACKLASH;
            return {
                status: spell.status,
                stage: spell.currentStage,
                message: '反噬：魔法阵不稳定 (稳定性: ' + (stability * 100).toFixed(1) + '%)'
            };
        }

        spell.currentStage = SpellStage.RELEASING;

        // 释放阶段 - 计算效果
        var effect = this.calculateSpellEffect(spell, array);

        spell.status = SpellStatus.SUCCESS;
        return {
            status: spell.status,
            stage: spell.currentStage,
            message: '施法成功',
            effect: effect
        };
    };

    /**
     * 计算咒语效果
     */
    Magic.prototype.calculateSpellEffect = function(spell, array) {
        if (!spell || !array) return null;

        // 基础效果 = 符文强度之和
        var baseEffect = 0;
        for (var i = 0; i < array.runes.length; i++) {
            baseEffect += array.runes[i].power;
        }

        // 元素加成
        var elementBonus = this.countElements(array, spell.purifyingElement) * 0.1;

        // 纯度加成
        var purityBonus = spell.purifyingPurity;

        // 能量阈值
        var energy = ThresholdEnergy[spell.infusingThreshold];

        // 最终效果
        var finalEffect = baseEffect * (1 + elementBonus) * purityBonus * (energy / 100);

        return {
            baseEffect: baseEffect,
            elementBonus: elementBonus,
            purityBonus: purityBonus,
            energy: energy,
            finalEffect: Math.round(finalEffect),
            range: spell.releasingRange,
            damage: spell.releasingDamage
        };
    };

    /**
     * 获取咒语信息
     */
    Magic.prototype.getSpellInfo = function(spell) {
        if (!spell) return null;

        return {
            id: spell.id,
            name: spell.name,
            description: spell.description,
            difficulty: spell.difficulty,
            difficultyName: this.getDifficultyName(spell.difficulty),
            element: this.getElementName(spell.purifyingElement),
            purity: (spell.purifyingPurity * 100).toFixed(1) + '%',
            threshold: ThresholdName[spell.infusingThreshold],
            currentStage: StageName[spell.currentStage],
            status: StatusName[spell.status]
        };
    };

    /**
     * 获取难度名称
     */
    Magic.prototype.getDifficultyName = function(level) {
        if (level <= 2) return '简单';
        if (level <= 4) return '普通';
        if (level <= 6) return '困难';
        if (level <= 8) return '极难';
        return '禁忌';
    };

    // ================================================================
    // 咒语书系统
    // ================================================================

    Magic.prototype.createSpellBook = function() {
        return {
            spells: [],
            add: function(spell) {
                this.spells.push(spell);
            },
            remove: function(name) {
                for (var i = 0; i < this.spells.length; i++) {
                    if (this.spells[i].name === name) {
                        this.spells.splice(i, 1);
                        return true;
                    }
                }
                return false;
            },
            get: function(name) {
                for (var i = 0; i < this.spells.length; i++) {
                    if (this.spells[i].name === name) {
                        return this.spells[i];
                    }
                }
                return null;
            },
            list: function() {
                var names = [];
                for (var i = 0; i < this.spells.length; i++) {
                    names.push(this.spells[i].name);
                }
                return names;
            },
            count: function() {
                return this.spells.length;
            }
        };
    };

    // ================================================================
    // 咏唱系统
    // ================================================================

    Magic.prototype.IncantationLength = {
        INSTANT: 0,    // 瞬发 0词
        SHORT: 1,      // 短咏 1-3词
        STANDARD: 2,   // 标准咏 4-7词
        LONG: 3,       // 长咏 8-15词
        RITUAL: 4      // 仪式咏 >15词
    };

    Magic.prototype.IncantationName = {
        0: '瞬发',
        1: '短咏',
        2: '标准咏',
        3: '长咏',
        4: '仪式咏'
    };

    /**
     * 优化咏唱配置
     */
    Magic.prototype.optimizeIncantation = function(goal) {
        var profile = {
            length: this.IncantationLength.STANDARD,
            precision: 0.8,
            speed: 0.8,
            stealth: 0.5
        };

        if (goal === 'speed') {
            profile.length = this.IncantationLength.SHORT;
            profile.speed = 0.95;
            profile.precision = 0.5;
            profile.stealth = 0.9;
        } else if (goal === 'precision') {
            profile.length = this.IncantationLength.LONG;
            profile.speed = 0.4;
            profile.precision = 0.95;
            profile.stealth = 0.3;
        } else if (goal === 'stealth') {
            profile.length = this.IncantationLength.SHORT;
            profile.speed = 0.8;
            profile.precision = 0.6;
            profile.stealth = 0.95;
        }

        return profile;
    };

    /**
     * 计算咏唱威力
     */
    Magic.prototype.calculateIncantationPower = function(profile) {
        var power = profile.precision * 0.4 + profile.speed * 0.3 + profile.stealth * 0.3;

        switch (profile.length) {
            case this.IncantationLength.INSTANT:
                power *= 0.5;
                break;
            case this.IncantationLength.SHORT:
                power *= 0.7;
                break;
            case this.IncantationLength.STANDARD:
                power *= 1.0;
                break;
            case this.IncantationLength.LONG:
                power *= 1.2;
                break;
            case this.IncantationLength.RITUAL:
                power *= 1.5;
                break;
        }

        return power;
    };

    // ================================================================
    // 领域系统
    // ================================================================

    Magic.prototype.createDomain = function(name, range) {
        return {
            name: name || '未命名领域',
            range: range || 50,
            center: null,
            active: false,
            strength: 0,
            rules: []
        };
    };

    Magic.prototype.activateDomain = function(domain, centerX, centerY) {
        if (!domain) return false;
        domain.center = { x: centerX, y: centerY };
        domain.active = true;
        domain.strength = 1.0;
        return true;
    };

    Magic.prototype.deactivateDomain = function(domain) {
        if (!domain) return false;
        domain.active = false;
        domain.strength = 0;
        return true;
    };

    // ================================================================
    // 预定义咒语生成器
    // ================================================================

    /**
     * 创建火球术
     */
    Magic.prototype.createFireballSpell = function() {
        var spell = this.createSpell('火球术');
        spell.description = '基础火系攻击法术，发射一个燃烧的火球';
        spell.difficulty = 2;
        spell.purifyingElement = MagicElement.FIRE;
        spell.purifyingPurity = 0.8;
        spell.infusingThreshold = EnergyThreshold.T3;
        spell.releasingRange = 50;
        spell.releasingDamage = 50;

        // 配置开模符文
        var seq = this.createRuneSequence();
        seq.add(this.createRationalRune(1, 1, MagicElement.FIRE));
        seq.add(this.createRationalRune(1, 1, MagicElement.FIRE));
        seq.add(this.createAlgebraicRune(3.14, MagicElement.NONE));
        this.configureSpellMolding(spell, seq);

        return spell;
    };

    /**
     * 创建冰锥术
     */
    Magic.prototype.createIceShardSpell = function() {
        var spell = this.createSpell('冰锥术');
        spell.description = '水风复合攻击法术，射出尖锐的冰锥';
        spell.difficulty = 3;
        spell.purifyingElement = MagicElement.WATER;
        spell.purifyingPurity = 0.85;
        spell.infusingThreshold = EnergyThreshold.T2;
        spell.releasingRange = 30;
        spell.releasingDamage = 30;

        // 配置开模符文
        var seq = this.createRuneSequence();
        seq.add(this.createRationalRune(1, 1, MagicElement.WATER));
        seq.add(this.createRationalRune(1, 2, MagicElement.AIR));
        seq.add(this.createRationalRune(3, 1, MagicElement.WATER));
        this.configureSpellMolding(spell, seq);

        return spell;
    };

    /**
     * 创建闪电术
     */
    Magic.prototype.createLightningSpell = function() {
        var spell = this.createSpell('闪电术');
        spell.description = '风系攻击法术，召唤闪电打击目标';
        spell.difficulty = 3;
        spell.purifyingElement = MagicElement.AIR;
        spell.purifyingPurity = 0.9;
        spell.infusingThreshold = EnergyThreshold.T3;
        spell.releasingRange = 60;
        spell.releasingDamage = 40;

        // 配置开模符文
        var seq = this.createRuneSequence();
        seq.add(this.createRationalRune(2, 1, MagicElement.AIR));
        seq.add(this.createRationalRune(1, 1, MagicElement.AIR));
        seq.add(this.createRationalRune(1, 1, MagicElement.AIR));
        this.configureSpellMolding(spell, seq);

        return spell;
    };

    /**
     * 创建土墙术
     */
    Magic.prototype.createEarthWallSpell = function() {
        var spell = this.createSpell('土墙术');
        spell.description = '土系防御法术，召唤土墙阻挡攻击';
        spell.difficulty = 2;
        spell.purifyingElement = MagicElement.EARTH;
        spell.purifyingPurity = 0.75;
        spell.infusingThreshold = EnergyThreshold.T2;
        spell.releasingRange = 20;
        spell.releasingDamage = 0;

        // 配置开模符文
        var seq = this.createRuneSequence();
        seq.add(this.createRationalRune(1, 1, MagicElement.EARTH));
        seq.add(this.createRationalRune(1, 1, MagicElement.EARTH));
        seq.add(this.createRationalRune(1, 1, MagicElement.EARTH));
        seq.add(this.createRationalRune(1, 1, MagicElement.EARTH));
        this.configureSpellMolding(spell, seq);

        return spell;
    };

    /**
     * 创建治疗术
     */
    Magic.prototype.createHealSpell = function() {
        var spell = this.createSpell('治疗术');
        spell.description = '水系辅助法术，恢复目标的生命力';
        spell.difficulty = 3;
        spell.purifyingElement = MagicElement.WATER;
        spell.purifyingPurity = 0.95;
        spell.infusingThreshold = EnergyThreshold.T2;
        spell.releasingRange = 10;
        spell.releasingDamage = -30;  // 负数表示治疗

        // 配置开模符文
        var seq = this.createRuneSequence();
        seq.add(this.createRationalRune(1, 1, MagicElement.WATER));
        seq.add(this.createRationalRune(1, 1, MagicElement.WATER));
        seq.add(this.createRationalRune(1, 1, MagicElement.WATER));
        seq.add(this.createRationalRune(2, 1, MagicElement.WATER));
        this.configureSpellMolding(spell, seq);

        return spell;
    };

    // ================================================================
    // 预设咒语书
    // ================================================================

    Magic.prototype.createStarterSpellBook = function() {
        var book = this.createSpellBook();
        book.add(this.createFireballSpell());
        book.add(this.createIceShardSpell());
        book.add(this.createLightningSpell());
        book.add(this.createEarthWallSpell());
        book.add(this.createHealSpell());
        return book;
    };

    // ================================================================
    // 导出公开 API
    // ================================================================

    return {
        Magic: Magic,

        // 枚举导出
        Element: MagicElement,
        Reaction: ElementReaction,
        Stage: SpellStage,
        Status: SpellStatus,
        Purity: PurityLevel,
        Threshold: EnergyThreshold,
        ArrayConstraint: ArrayConstraintType,
        Restriction: RestrictionLevel,
        IncantationLength: {
            INSTANT: 0,
            SHORT: 1,
            STANDARD: 2,
            LONG: 3,
            RITUAL: 4
        },

        // 名称映射
        ElementName: ElementName,
        ReactionName: ReactionName,
        StageName: StageName,
        StatusName: StatusName,
        PurityName: PurityName,
        ThresholdName: ThresholdName,
        RestrictionName: RestrictionName,
        IncantationName: {
            0: '瞬发',
            1: '短咏',
            2: '标准咏',
            3: '长咏',
            4: '仪式咏'
        },

        // 创建实例
        createMagic: function() {
            return new Magic();
        }
    };

})();

// 导出到全局
if (typeof window !== 'undefined') {
    window.Lv00Magic = Lv00Magic;
}
