/**
 * ============================================================================
 *  Lv-00 编程魔法示例程序
 * ============================================================================
 *
 *  本文件展示如何使用 Lv-00 编程魔法系统
 *
 *  概念映射：
 *    符文 (Rune)        → 基于符号坐标的魔法构建块
 *    魔法阵 (MagicArray) → 基于约束图的结构
 *    咒语 (Spell)       → 基于函数块的施法流程
 *    元素 (Element)     → 轰界四元素体系
 *
 *  作者：Lv-00 Team
 *  版本：1.0.0
 * ============================================================================
 */

// 引入魔法模块（在浏览器中已通过 script 标签引入）
// var Lv00Magic = window.Lv00Magic;

// 创建魔法系统实例
var magic = Lv00Magic.createMagic();

console.log('========================================');
console.log('Lv-00 编程魔法系统示例');
console.log('========================================');
console.log('版本: ' + magic.version);
console.log('');

// ================================================================
// 示例 1: 基础符文系统
// ================================================================

console.log('【示例 1】基础符文系统');
console.log('----------------------------------------');

// 创建不同元素的符文
var fireRune = magic.createRationalRune(1, 1, Lv00Magic.Element.FIRE);
var waterRune = magic.createRationalRune(1, 2, Lv00Magic.Element.WATER);
var airRune = magic.createRationalRune(2, 1, Lv00Magic.Element.AIR);
var earthRune = magic.createRationalRune(3, 1, Lv00Magic.Element.EARTH);

console.log('创建的符文:');
console.log('  火符文: 值=' + magic.getRuneValue(fireRune) + ', 元素=' + magic.getElementName(fireRune.element));
console.log('  水符文: 值=' + magic.getRuneValue(waterRune) + ', 元素=' + magic.getElementName(waterRune.element));
console.log('  风符文: 值=' + magic.getRuneValue(airRune) + ', 元素=' + magic.getElementName(airRune.element));
console.log('  土符文: 值=' + magic.getRuneValue(earthRune) + ', 元素=' + magic.getElementName(earthRune.element));
console.log('');

// 设置符文强度
magic.setRunePower(fireRune, 5);
magic.setRunePower(waterRune, 3);
magic.setRunePower(airRune, 4);
magic.setRunePower(earthRune, 6);

console.log('符文强度:');
console.log('  火符文: ' + fireRune.power);
console.log('  水符文: ' + waterRune.power);
console.log('  风符文: ' + airRune.power);
console.log('  土符文: ' + earthRune.power);
console.log('');

// ================================================================
// 示例 2: 元素反应系统
// ================================================================

console.log('【示例 2】元素反应系统');
console.log('----------------------------------------');

var elements = [
    Lv00Magic.Element.FIRE,
    Lv00Magic.Element.WATER,
    Lv00Magic.Element.AIR,
    Lv00Magic.Element.EARTH
];

console.log('元素反应矩阵:');
for (var i = 0; i < elements.length; i++) {
    var row = '';
    for (var j = 0; j < elements.length; j++) {
        var reaction = magic.checkElementReaction(elements[i], elements[j]);
        var reactionName = magic.getReactionName(reaction);
        row += '  ' + magic.getElementName(elements[i]).charAt(0) + '-' +
               magic.getElementName(elements[j]).charAt(0) + ':' + reactionName;
    }
    console.log(row);
}
console.log('');

// ================================================================
// 示例 3: 魔法阵系统
// ================================================================

console.log('【示例 3】魔法阵系统');
console.log('----------------------------------------');

// 创建魔法阵
var array = magic.createArray();
console.log('创建魔法阵 #' + array.id);

// 添加符文到魔法阵
var runeId1 = magic.addRuneToArray(array, fireRune);
var runeId2 = magic.addRuneToArray(array, waterRune);
var runeId3 = magic.addRuneToArray(array, airRune);
console.log('添加符文: #' + runeId1 + ', #' + runeId2 + ', #' + runeId3);

// 添加约束
magic.addConstraintToArray(array, Lv00Magic.ArrayConstraint.CONNECTION, runeId1, runeId2);
magic.addConstraintToArray(array, Lv00Magic.ArrayConstraint.ENHANCEMENT, runeId2, runeId3);
console.log('添加约束: 连接符文1-2, 增强符文2-3');

// 获取魔法阵信息
var arrayInfo = magic.getArrayInfo(array);
console.log('\n魔法阵信息:');
console.log('  符文数量: ' + arrayInfo.runeCount);
console.log('  约束数量: ' + arrayInfo.constraintCount);
console.log('  稳定性: ' + (arrayInfo.stability * 100).toFixed(1) + '%');
console.log('  元素平衡: ' + (arrayInfo.balanced ? '是' : '否'));
console.log('  元素分布: 火=' + arrayInfo.elementCounts[1] +
            ', 水=' + arrayInfo.elementCounts[2] +
            ', 风=' + arrayInfo.elementCounts[3] +
            ', 土=' + arrayInfo.elementCounts[4]);
console.log('');

// ================================================================
// 示例 4: 预定义咒语
// ================================================================

console.log('【示例 4】预定义咒语');
console.log('----------------------------------------');

// 创建预设咒语书
var spellBook = magic.createStarterSpellBook();
var spellNames = spellBook.list();

console.log('咒语书包含 ' + spellBook.count() + ' 个咒语:');
for (var k = 0; k < spellNames.length; k++) {
    var spell = spellBook.get(spellNames[k]);
    var spellInfo = magic.getSpellInfo(spell);
    console.log('\n  [' + spellInfo.name + ']');
    console.log('    描述: ' + spellInfo.description);
    console.log('    难度: ' + spellInfo.difficulty + ' (' + spellInfo.difficultyName + ')');
    console.log('    元素: ' + spellInfo.element);
    console.log('    纯度: ' + spellInfo.purity);
    console.log('    阈值: ' + spellInfo.threshold);
}
console.log('');

// ================================================================
// 示例 5: 执行施法
// ================================================================

console.log('【示例 5】执行施法');
console.log('----------------------------------------');

// 获取火球术
var fireball = spellBook.get('火球术');
console.log('施放: ' + fireball.name);
console.log('');

// 创建施法用魔法阵（纯火元素）
var fireArray = magic.createArray();
magic.addRuneToArray(fireArray, magic.createRationalRune(1, 1, Lv00Magic.Element.FIRE));
magic.addRuneToArray(fireArray, magic.createRationalRune(1, 1, Lv00Magic.Element.FIRE));
magic.addRuneToArray(fireArray, magic.createRationalRune(1, 1, Lv00Magic.Element.FIRE));
magic.setRunePower(fireArray.runes[0], 3);
magic.setRunePower(fireArray.runes[1], 4);
magic.setRunePower(fireArray.runes[2], 5);

// 添加约束
magic.addConstraintToArray(fireArray, Lv00Magic.ArrayConstraint.CONNECTION, 0, 1);
magic.addConstraintToArray(fireArray, Lv00Magic.ArrayConstraint.CONNECTION, 1, 2);

console.log('准备施法:');
console.log('  符文数量: ' + fireArray.runes.length);
console.log('  总强度: ' + magic.getArrayInfo(fireArray).totalPower);
console.log('  稳定性: ' + (magic.calculateArrayStability(fireArray) * 100).toFixed(1) + '%');
console.log('');

// 执行施法
var result = magic.castSpell(fireball, fireArray);

console.log('施法结果:');
console.log('  状态: ' + Lv00Magic.StatusName[result.status]);
console.log('  阶段: ' + Lv00Magic.StageName[result.stage]);
console.log('  消息: ' + result.message);

if (result.effect) {
    console.log('\n  效果详情:');
    console.log('    基础效果: ' + result.effect.baseEffect);
    console.log('    元素加成: +' + (result.effect.elementBonus * 100).toFixed(1) + '%');
    console.log('    纯度加成: ×' + result.effect.purityBonus.toFixed(2));
    console.log('    能量等级: ' + result.effect.energy + ' E_u');
    console.log('    最终效果: ' + result.effect.finalEffect);
    console.log('    射程: ' + result.effect.range);
    console.log('    伤害: ' + result.effect.damage);
}
console.log('');

// ================================================================
// 示例 6: 施法失败案例
// ================================================================

console.log('【示例 6】施法失败案例');
console.log('----------------------------------------');

// 创建水系魔法阵
var waterArray = magic.createArray();
magic.addRuneToArray(waterArray, magic.createRationalRune(1, 1, Lv00Magic.Element.WATER));
magic.addRuneToArray(waterArray, magic.createRationalRune(1, 1, Lv00Magic.Element.WATER));

// 添加冲突约束
magic.addConstraintToArray(waterArray, Lv00Magic.ArrayConstraint.CONFLICT, 0, 1);

console.log('尝试用火球术（需要火元素）施放到水系魔法阵:');
result = magic.castSpell(fireball, waterArray);
console.log('  状态: ' + Lv00Magic.StatusName[result.status]);
console.log('  消息: ' + result.message);
console.log('');

// ================================================================
// 示例 7: 不稳定魔法阵（反噬）
// ================================================================

console.log('【示例 7】不稳定魔法阵（反噬）');
console.log('----------------------------------------');

// 创建不稳定的魔法阵
var unstableArray = magic.createArray();
magic.addRuneToArray(unstableArray, magic.createRationalRune(1, 1, Lv00Magic.Element.FIRE));
magic.addRuneToArray(unstableArray, magic.createRationalRune(1, 1, Lv00Magic.Element.WATER));

// 添加冲突约束（火-水）
magic.addConstraintToArray(unstableArray, Lv00Magic.ArrayConstraint.CONFLICT, 0, 1);

console.log('不稳定魔法阵:');
console.log('  符文: 火 + 水');
console.log('  约束: 冲突（火-水）');
console.log('  稳定性: ' + (magic.calculateArrayStability(unstableArray) * 100).toFixed(1) + '%');
console.log('');

result = magic.castSpell(fireball, unstableArray);
console.log('施法结果:');
console.log('  状态: ' + Lv00Magic.StatusName[result.status]);
console.log('  消息: ' + result.message);
console.log('');

// ================================================================
// 示例 8: 咏唱系统
// ================================================================

console.log('【示例 8】咏唱系统');
console.log('----------------------------------------');

var goals = ['speed', 'precision', 'stealth'];
for (var g = 0; g < goals.length; g++) {
    var profile = magic.optimizeIncantation(goals[g]);
    var power = magic.calculateIncantationPower(profile);

    console.log('\n  优化目标: ' + goals[g]);
    console.log('    咏唱类型: ' + Lv00Magic.IncantationName[profile.length]);
    console.log('    精度: ' + (profile.precision * 100).toFixed(0) + '%');
    console.log('    速度: ' + (profile.speed * 100).toFixed(0) + '%');
    console.log('    隐蔽性: ' + (profile.stealth * 100).toFixed(0) + '%');
    console.log('    综合威力: ' + power.toFixed(2));
}
console.log('');

// ================================================================
// 示例 9: 创建自定义咒语
// ================================================================

console.log('【示例 9】创建自定义咒语');
console.log('----------------------------------------');

// 创建自定义咒语
var customSpell = magic.createSpell('流星火雨');
customSpell.description = '高级火系范围攻击法术，召唤火流星雨';
customSpell.difficulty = 6;
magic.configureSpellPurifying(customSpell, Lv00Magic.Element.FIRE, 0.9);
magic.configureSpellInfusing(customSpell, Lv00Magic.Threshold.T4);
magic.configureSpellReleasing(customSpell, 100, 80);

// 配置开模符文
var meteorSequence = magic.createRuneSequence();
for (var m = 0; m < 6; m++) {
    meteorSequence.add(magic.createRationalRune(2, 1, Lv00Magic.Element.FIRE));
}
magic.configureSpellMolding(customSpell, meteorSequence);

// 添加到咒语书
spellBook.add(customSpell);

console.log('创建自定义咒语: ' + customSpell.name);
console.log('描述: ' + customSpell.description);
console.log('难度: ' + magic.getSpellInfo(customSpell).difficultyName);
console.log('');

// 创建强大的魔法阵
var meteorArray = magic.createArray();
for (var n = 0; n < 8; n++) {
    magic.addRuneToArray(meteorArray, magic.createRationalRune(3, 1, Lv00Magic.Element.FIRE));
    magic.setRunePower(meteorArray.runes[n], 5 + Math.floor(Math.random() * 3));
}

// 添加增强约束
for (var p = 0; p < meteorArray.runes.length - 1; p++) {
    magic.addConstraintToArray(meteorArray, Lv00Magic.ArrayConstraint.ENHANCEMENT, p, p + 1);
}

console.log('准备流星火雨:');
console.log('  符文数量: ' + meteorArray.runes.length);
console.log('  总强度: ' + magic.getArrayInfo(meteorArray).totalPower);
console.log('  稳定性: ' + (magic.calculateArrayStability(meteorArray) * 100).toFixed(1) + '%');
console.log('');

result = magic.castSpell(customSpell, meteorArray);
console.log('施法结果:');
console.log('  状态: ' + Lv00Magic.StatusName[result.status]);
if (result.effect) {
    console.log('  最终效果: ' + result.effect.finalEffect + ' (' + result.effect.damage + ' 伤害)');
}
console.log('');

// ================================================================
// 示例 10: 领域系统
// ================================================================

console.log('【示例 10】领域系统');
console.log('----------------------------------------');

// 创建火领域
var fireDomain = magic.createDomain('烈焰领域', 100);
console.log('创建领域: ' + fireDomain.name);
console.log('范围: ' + fireDomain.range + ' 单位');

// 激活领域
magic.activateDomain(fireDomain, 0, 0);
console.log('领域状态: ' + (fireDomain.active ? '激活' : '未激活'));
console.log('领域强度: ' + (fireDomain.strength * 100).toFixed(0) + '%');
console.log('');

// ================================================================
// 总结
// ================================================================

console.log('========================================');
console.log('示例程序执行完毕');
console.log('========================================');
console.log('');
console.log('本示例展示了 Lv-00 编程魔法系统的核心功能:');
console.log('  1. 符文系统 - 基于符号坐标的魔法构建块');
console.log('  2. 元素系统 - 轰界四元素反应矩阵');
console.log('  3. 魔法阵系统 - 基于约束图的结构');
console.log('  4. 咒语系统 - 基于函数块的施法流程');
console.log('  5. 咏唱系统 - 施法优化');
console.log('  6. 领域系统 - 高级魔法效果');
console.log('');
console.log('下一步:');
console.log('  - 集成到 Lv-00 Web 界面');
console.log('  - 添加更多咒语和复合元素');
console.log('  - 实现更复杂的魔法效果');
console.log('');

// ================================================================
// HTML 测试界面代码
// ================================================================

/*
在 HTML 中使用示例:

<!DOCTYPE html>
<html>
<head>
    <title>Lv-00 编程魔法</title>
    <script src="js/lv00_js_backend.js"></script>
    <script src="js/magic_module.js"></script>
</head>
<body>
    <h1>Lv-00 编程魔法系统</h1>
    <div id="output"></div>
    <script>
        // 运行示例
        var magic = Lv00Magic.createMagic();
        var spellBook = magic.createStarterSpellBook();

        // 施放火球术
        var fireball = spellBook.get('火球术');
        var fireArray = magic.createArray();
        magic.addRuneToArray(fireArray,
            magic.createRationalRune(1, 1, Lv00Magic.Element.FIRE));
        magic.addRuneToArray(fireArray,
            magic.createRationalRune(1, 1, Lv00Magic.Element.FIRE));

        var result = magic.castSpell(fireball, fireArray);
        console.log(result);
    </script>
</body>
</html>
*/
