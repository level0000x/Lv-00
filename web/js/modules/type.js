/**
 * @file type.js
 * @brief TYPE 类型模块方法
 * @description 实现 Lv-00 类型系统的前端操作方法，挂载到 Lv00WebApp.prototype 上。
 *              包括创建各种几何类型（点、线段、区域、函数、乘积类型）、
 *              类型等价检查、类型推断、类型层级检查等功能。
 *              所有类型操作通过后端 API（Lv00JSBackend）执行，
 *              前端负责参数收集、状态更新和日志输出。
 *
 * @module type
 * @requires Lv00WebApp 构造函数（app.js）
 * @requires ui.js（appendLog, showSuccess 等方法）
 * @since 3.0.0
 */
(function() {
    'use strict';

    // ================================================================
    // 公共辅助：类型创建
    // fix: 从五个重复的 typeCreate* 函数中提取公共模式，
    //      统一处理后端调用、错误提示和计数更新。
    // @param {object} self - Lv00WebApp 实例
    // @param {string} errorMsg - 无类型系统时的错误日志消息
    // @param {string} successMsg - 创建成功时的日志消息
    // @param {string} backendMethod - 后端方法名
    // ================================================================
    var _doTypeCreate = function(self, errorMsg, successMsg, backendMethod) {
        if (!self.jsBackend || !self.typeSystem) {
            self.appendLog(errorMsg, 'error');
            return;
        }
        var result = self._callBackend(backendMethod, [self.typeSystem]);
        if (result != null) {
            self.appendLog(successMsg, 'info');
            self._updateTypeCount();
        }
    };

    // ================================================================
    // 创建点类型
    // ================================================================
    Lv00WebApp.prototype.typeCreatePoint = function() {
        _doTypeCreate(this,
            'Create point type: no type system / 创建点类型：无类型系统',
            'Point type created / 点类型已创建',
            'typeSystemCreatePointType');
    };

    // ================================================================
    // 创建线段类型
    // ================================================================
    Lv00WebApp.prototype.typeCreateSegment = function() {
        _doTypeCreate(this,
            'Create segment type: no type system',
            'Segment type created / 线段类型已创建',
            'typeSystemCreateSegmentType');
    };

    // ================================================================
    // 创建区域类型
    // ================================================================
    Lv00WebApp.prototype.typeCreateRegion = function() {
        _doTypeCreate(this,
            'Create region type: no type system',
            'Region type created / 区域类型已创建',
            'typeSystemCreateRegionType');
    };

    // ================================================================
    // 创建函数类型
    // ================================================================
    Lv00WebApp.prototype.typeCreateFunction = function() {
        _doTypeCreate(this,
            'Create function type: no type system',
            'Function type created / 函数类型已创建',
            'typeSystemCreateFunctionType');
    };

    // ================================================================
    // 创建乘积类型
    // ================================================================
    Lv00WebApp.prototype.typeCreateProduct = function() {
        _doTypeCreate(this,
            'Create product type: no type system',
            'Product type created / 乘积类型已创建',
            'typeSystemCreateProductType');
    };

    // ================================================================
    // 类型等价检查
    // 检查两个类型是否等价
    // ================================================================
    Lv00WebApp.prototype.typeCheckEquiv = function() {
        if (!this.jsBackend || !this.typeSystem) {
            this.appendLog('Type equiv check: no type system / 类型等价检查：无类型系统', 'error');
            return;
        }

        // 使用选中的两个点进行类型等价检查
        if (this.selectedPoints && this.selectedPoints.length >= 2) {
            var id1 = this.selectedPoints[0].id;
            var id2 = this.selectedPoints[1].id;
            var result = this._callBackend('typeSystemCheckEquiv', [this.typeSystem, id1, id2]);
            if (result !== null && result !== undefined) {
                var equivStr = result ? 'EQUIVALENT / 等价' : 'NOT EQUIVALENT / 不等价';
                this.appendLog('Type equiv n' + id1 + ' <-> n' + id2 + ': ' + equivStr, 'info');
                this.showInfo(equivStr);
            }
        } else {
            this.appendLog('Type equiv: 请选择两个节点 / Select two nodes', 'warn');
        }
    };

    // ================================================================
    // 类型推断
    // 推断选中节点的类型
    // ================================================================
    Lv00WebApp.prototype.typeInferNode = function() {
        if (!this.jsBackend || !this.typeSystem) {
            this.appendLog('Type infer: no type system / 类型推断：无类型系统', 'error');
            return;
        }

        var nodeId = this.selectedPoint ? this.selectedPoint.id : -1;
        if (nodeId < 0) {
            this.appendLog('Type infer: 请选择一个节点 / Select a node', 'warn');
            return;
        }

        var result = this._callBackend('typeSystemInferNode', [this.typeSystem, nodeId]);
        if (result !== null && result !== undefined) {
            this.appendLog('Type of n' + nodeId + ': ' + JSON.stringify(result) + ' / 节点 n' + nodeId + ' 的类型', 'info');
            this.showInfo('Type: ' + JSON.stringify(result));
        }
    };

    // ================================================================
    // 类型层级检查
    // 检查类型层次结构是否符合理论要求。
    // 优先调用后端 API（/api/type/level），
    // 后端不可用时执行客户端类型层级验证，
    // 包括：循环基类检测、字段兼容性、子类型关系传递性检查。
    // ================================================================
    Lv00WebApp.prototype.typeCheckLevel = function() {
        if (!this.jsBackend || !this.typeSystem) {
            this.showToast('类型系统未初始化，无法进行层级检查', 'error');
            this.appendLog('Type level check: no type system / 类型层级检查：无类型系统', 'error');
            return;
        }

        // 优先通过后端 API 进行层级检查
        var result = this._callBackend('typeSystemCheckLevel', [this.typeSystem]);
        if (result !== null && result !== undefined) {
            this.appendLog('Type level (backend): ' + JSON.stringify(result) +
                ' / 类型层级（后端）', 'info');
            this.showToast('层级检查完成: ' + (result.valid ? '通过' : '发现问题'), result.valid ? 'success' : 'warn');
            return;
        }

        // --- 后端不可用，执行客户端层级验证 ---
        var issues = [];
        var warnings = [];
        var typeMap = this.typeSystem.types || {};
        var typeList = Object.keys(typeMap);

        if (typeList.length === 0) {
            this.showToast('类型系统为空，无层级可检查', 'info');
            this.appendLog('Type level: no types registered / 类型层级：无已注册类型', 'info');
            return;
        }

        // 1) 循环基类检测：从每个类型出发沿 parent 链溯源，检查是否有环
        var visitedInPath = {};
        var cyclicTypes = [];

        /**
         * 遍历 parent 链检测环
         * @param {string} typeName - 当前类型名
         * @param {object} pathSet  - 当前路径上的类型集合
         * @returns {boolean} 是否存在环
         */
        var detectCycle = function(typeName, pathSet) {
            if (pathSet[typeName]) {
                cyclicTypes.push(typeName);
                return true;
            }
            var t = typeMap[typeName];
            if (!t || !t.parent) return false;
            pathSet[typeName] = true;
            var hasCycle = detectCycle(t.parent, pathSet);
            delete pathSet[typeName];
            return hasCycle;
        };

        for (var i = 0; i < typeList.length; i++) {
            var tn = typeList[i];
            var pathSet = {};
            detectCycle(tn, pathSet);
        }

        if (cyclicTypes.length > 0) {
            issues.push('检测到循环基类引用: ' + cyclicTypes.join(', ') +
                ' / Cyclic parent reference detected: ' + cyclicTypes.join(', '));
        }

        // 2) 子类型字段兼容性检查：子类型应包含父类型的全部字段
        for (var j = 0; j < typeList.length; j++) {
            var childName = typeList[j];
            var childType = typeMap[childName];
            if (!childType || !childType.parent) continue;

            var parentType = typeMap[childType.parent];
            if (!parentType) {
                warnings.push('父类型不存在: ' + childType.parent + ' <- ' + childName +
                    ' / Parent type not found: ' + childType.parent + ' <- ' + childName);
                continue;
            }

            // 检查子类型是否包含父类型的字段
            if (childType.fields && parentType.fields) {
                var childFieldNames = Object.keys(childType.fields);
                var parentFieldNames = Object.keys(parentType.fields);
                for (var pf = 0; pf < parentFieldNames.length; pf++) {
                    var parentField = parentFieldNames[pf];
                    if (childFieldNames.indexOf(parentField) === -1) {
                        warnings.push('子类型缺少父类型字段: ' + childName +
                            ' 缺少 "' + parentField + '" (来自父类型 ' + parentType.name + ')' +
                            ' / Missing parent field');
                    }
                }
            }
        }

        // 3) 类型深度统计：计算每个类型在继承树中的深度
        var maxDepth = 0;
        var depthMap = {};
        var computeDepth = function(typeName) {
            if (depthMap.hasOwnProperty(typeName)) return depthMap[typeName];
            var t = typeMap[typeName];
            if (!t || !t.parent) {
                depthMap[typeName] = 0;
                return 0;
            }
            var parentDepth = computeDepth(t.parent);
            var d = parentDepth + 1;
            depthMap[typeName] = d;
            if (d > maxDepth) maxDepth = d;
            return d;
        };

        for (var k = 0; k < typeList.length; k++) {
            computeDepth(typeList[k]);
        }

        // 汇总结果
        var summary = {
            totalTypes: typeList.length,
            maxDepth: maxDepth,
            issues: issues.length,
            warnings: warnings.length,
            isCyclic: cyclicTypes.length > 0
        };

        var levelText = '类型层级检查: 共 ' + summary.totalTypes +
            ' 个类型, 最大深度 ' + summary.maxDepth;

        if (issues.length > 0) {
            for (var ii = 0; ii < issues.length; ii++) {
                this.appendLog('Type level ISSUE: ' + issues[ii], 'error');
            }
            this.showToast(levelText + ', ' + issues.length + ' 个问题', 'error');
        } else if (warnings.length > 0) {
            for (var wi = 0; wi < warnings.length; wi++) {
                this.appendLog('Type level WARN: ' + warnings[wi], 'warn');
            }
            this.showToast(levelText + ', ' + warnings.length + ' 个警告', 'warn');
        } else {
            this.showToast(levelText + ', 结构正常', 'success');
        }

        this.appendLog('Type level check: ' + JSON.stringify(summary) +
            ' / 类型层级检查（客户端）: ' + levelText, 'info');
    };

    // ================================================================
    // 更新类型计数显示
    // @private
    // ================================================================
    Lv00WebApp.prototype._updateTypeCount = function() {
        var count = 0;
        if (this.typeSystem && this.typeSystem.types) {
            count = Object.keys(this.typeSystem.types).length;
        }
        var countEl = document.getElementById('typeCount');
        if (countEl) {
            countEl.textContent = count;
        }
    };

})();
