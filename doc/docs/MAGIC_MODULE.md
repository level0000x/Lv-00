# Lv-00 编程魔法模块 - 开发者文档

## 概述

Lv-00 编程魔法模块是一个基于轰界世界观和 Lv-00 核心系统的魔法模拟器。它将 Lv-00 的几何元语言系统映射为魔法概念，实现了"用代码施展魔法"的独特体验。

## 系统架构

### 核心概念映射

| Lv-00 系统 | 魔法概念 | 说明 |
|-----------|----------|------|
| 符号坐标 | 符文 (Rune) | 魔法的基本构建块 |
| 约束图 | 魔法阵 (MagicArray) | 符文之间的关系和约束 |
| 函数块 | 咒语 (Spell) | 封装好的施法流程 |
| 类型系统 | 元素系统 | 轰界四元素体系 |
| 递归系统 | 领域 (Domain) | 高级魔法结构 |

## 文件结构

```
Lv-00/
├── include/
│   └── lv00/
│       └── magic.h          # C 语言头文件
├── src/
│   └── magic.c              # C 语言实现
├── web/
│   ├── js/
│   │   └── magic_module.js  # JavaScript 模块
│   ├── examples/
│   │   └── magic_examples.js # 示例程序
│   └── magic_test.html      # 交互式测试页面
└── docs/
    ├── magic_system.md      # 魔法体系基础
    └── magic_system_detailed.md # 详细魔法体系
```

## API 参考

### JavaScript 模块 (magic_module.js)

#### 创建实例

```javascript
var magic = Lv00Magic.createMagic();
```

#### 符文系统

```javascript
// 创建符文
var rune = magic.createRationalRune(1, 1, Lv00Magic.Element.FIRE);

// 设置强度
magic.setRunePower(rune, 5);

// 获取值
var value = magic.getRuneValue(rune);
```

#### 魔法阵系统

```javascript
// 创建魔法阵
var array = magic.createArray();

// 添加符文
magic.addRuneToArray(array, rune);

// 添加约束
magic.addConstraintToArray(array, Lv00Magic.ArrayConstraint.CONNECTION, 0, 1);

// 计算稳定性
var stability = magic.calculateArrayStability(array);
```

#### 咒语系统

```javascript
// 创建咒语
var spell = magic.createSpell('火球术');
spell.difficulty = 2;
magic.configureSpellPurifying(spell, Lv00Magic.Element.FIRE, 0.9);
magic.configureSpellInfusing(spell, Lv00Magic.Threshold.T3);

// 执行施法
var result = magic.castSpell(spell, array);
```

#### 咒语书系统

```javascript
// 创建咒语书
var spellBook = magic.createSpellBook();

// 添加咒语
spellBook.add(spell);

// 获取咒语
var fireball = spellBook.get('火球术');
```

### 预定义咒语

```javascript
// 创建预设咒语书
var spellBook = magic.createStarterSpellBook();

// 可用咒语：
// - 火球术 (Fireball)
// - 冰锥术 (Ice Shard)
// - 闪电术 (Lightning)
// - 土墙术 (Earth Wall)
// - 治疗术 (Heal)
```

## 元素系统

### 轰界四元素

| 元素 | 符号 | 特性 |
|------|------|------|
| 火 | 🔥 | 释放与热效应 |
| 水 | 💧 | 流动与相态调节 |
| 风 | 💨 | 运动与压差 |
| 土 | 🪨 | 结构与承载 |
| 以太 | ✨ | 第五元素 |

### 元素反应矩阵

| 组合 | 反应 | 效果 |
|------|------|------|
| 火+风 | 增强 | 燃烧更剧烈 |
| 火+水 | 冲突 | 相互抵消 |
| 火+土 | 增强 | 熔岩/金属 |
| 水+风 | 削弱 | 稀释/消散 |
| 水+土 | 增强 | 泥沼/冰 |
| 风+土 | 冲突 | 扬尘/风暴 |

## 施法流程

### 四阶段模型

```
开模 (Molding) → 提纯 (Purifying) → 灌注 (Infusing) → 释放 (Releasing)
```

1. **开模**：定义法术的基本形态
2. **提纯**：纯化元素属性
3. **灌注**：注入能量达到阈值
4. **释放**：投射到目标

## 示例代码

### 完整示例：施放火球术

```javascript
// 1. 创建魔法系统实例
var magic = Lv00Magic.createMagic();

// 2. 创建咒语书
var spellBook = magic.createStarterSpellBook();

// 3. 获取火球术
var fireball = spellBook.get('火球术');

// 4. 准备魔法阵
var fireArray = magic.createArray();
magic.addRuneToArray(fireArray, magic.createRationalRune(1, 1, Lv00Magic.Element.FIRE));
magic.addRuneToArray(fireArray, magic.createRationalRune(1, 1, Lv00Magic.Element.FIRE));
magic.addRuneToArray(fireArray, magic.createRationalRune(1, 1, Lv00Magic.Element.FIRE));

// 5. 添加约束
magic.addConstraintToArray(fireArray, Lv00Magic.ArrayConstraint.CONNECTION, 0, 1);
magic.addConstraintToArray(fireArray, Lv00Magic.ArrayConstraint.CONNECTION, 1, 2);

// 6. 施放
var result = magic.castSpell(fireball, fireArray);

console.log(result.status); // SUCCESS
console.log(result.effect.finalEffect); // 效果值
```

## 使用方法

### Web 界面

打开 `web/magic_test.html` 文件即可使用交互式界面：

```bash
# 在浏览器中打开
file:///path/to/Lv-00/web/magic_test.html
```

### Node.js 环境

```javascript
// 加载模块
var Lv00Magic = require('./web/js/magic_module.js');

// 使用
var magic = Lv00Magic.createMagic();
```

## 扩展指南

### 添加新咒语

```javascript
Magic.prototype.createCustomSpell = function(name, element, purity, threshold) {
    var spell = this.createSpell(name);
    spell.purifyingElement = element;
    spell.purifyingPurity = purity;
    spell.infusingThreshold = threshold;

    // 配置开模符文
    var seq = this.createRuneSequence();
    seq.add(this.createRationalRune(1, 1, element));
    this.configureSpellMolding(spell, seq);

    return spell;
};
```

### 添加新元素

```javascript
// 在 MagicElement 枚举中添加新元素
var MagicElement = {
    FIRE: 1,
    WATER: 2,
    // ... 新增元素
    CUSTOM: 6  // 自定义元素
};

// 在元素反应矩阵中添加反应
_elementReactionMatrix[5][6] = ElementReaction.ENHANCE;
```

## 版本历史

| 版本 | 日期 | 描述 |
|------|------|------|
| 1.0.0 | 2026-05-22 | 初始版本，实现基础功能 |

## 许可

本模块作为 Lv-00 项目的一部分，采用相同的许可协议。

## 参考资料

- 轰界世界观手册
- Lv-00 核心系统文档
- 四元素理论
- 施法流程规范
