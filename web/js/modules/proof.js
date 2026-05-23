/**
 * proof.js - PROOF 证明模块方法
 *
 * 实现证明模块的操作方法，包括创建命题、设置模式、
 * 统一性检查、导航（上一步/下一步/跳转）、导出等。
 *
 * 依赖：Lv00WebApp 构造函数、ui.js
 */
(function() {
    'use strict';

    // ================================================================
    // 公共辅助：HTML 转义
    // 将字符串中的 < > & " ' 转义为 HTML 实体，防止 XSS
    // fix: 提取为闭包内公共函数，避免多处复制粘贴相同转义逻辑
    // ================================================================
    var _escapeHtml = function(str) {
        if (str == null) return '';
        return String(str)
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;')
            .replace(/'/g, '&#39;');
    };

    // ================================================================
    // 创建命题
    // 基于当前图状态创建一个新的证明命题
    // ================================================================
    Lv00WebApp.prototype.proofCreateProposition = function() {
        if (!this.jsBackend) {
            this.appendLog('Create proposition: no backend', 'error');
            return;
        }

        // 创建命题对象
        this.proposition = {
            title: 'Proposition ' + Date.now(),
            hypothesis: [],
            conclusion: null,
            steps: [],
            currentStep: 0,
            status: 'in_progress'
        };

        // 更新 UI
        var stepEl = document.getElementById('proofStep');
        if (stepEl) stepEl.textContent = '0 / 0';
        var statusEl = document.getElementById('proofStatus');
        if (statusEl) statusEl.textContent = 'IN PROGRESS';

        this.appendLog('Proposition created / 命题已创建', 'info');
        this.showInfo('Proposition created / 命题已创建');
    };

    // ================================================================
    // 设置证明模式
    // 支持三种证明模式：
    //   - 'forward'（正向证明）：从已知条件出发，逐步推导出结论
    //   - 'contrapositive'（反证）：假设结论不成立，推导出与已知条件矛盾
    //   - 'reductio'（归谬）：假设命题不成立，推导出逻辑矛盾以证明原命题
    // ================================================================
    Lv00WebApp.prototype.proofSetPattern = function() {
        if (!this.proposition) {
            this.appendLog('Set pattern: 请先创建命题 / Create a proposition first', 'warn');
            this.showToast('请先创建命题', 'warning');
            return;
        }

        // 当前证明模式，默认正向
        var currentPattern = this.proposition.pattern || 'forward';

        // 构建模式选择 URI 编码，用于模态框展示
        var patterns = [
            { value: 'forward',        label: 'FORWARD / 正向证明',    desc: '从已知条件推导出结论' },
            { value: 'contrapositive', label: 'CONTRAPOSITIVE / 反证', desc: '假设结论不成立，推导矛盾' },
            { value: 'reductio',       label: 'REDUCTIO / 归谬',       desc: '假设命题不成立，推导逻辑矛盾' }
        ];

        // 使用模态框展示模式选择界面
        this._showModal('modalOverlay');

        var title = document.getElementById('modalTitle');
        if (title) {
            title.textContent = 'SET PROOF PATTERN / 设置证明模式';
        }

        var body = document.getElementById('modalBody');
        if (body) {
            while (body.firstChild) { body.removeChild(body.firstChild); }

            var self = this;

            for (var i = 0; i < patterns.length; i++) {
                (function(pattern) {
                    var row = document.createElement('div');
                    row.style.cssText = 'padding:8px 0;border-bottom:1px solid var(--color-border-secondary);cursor:pointer;';
                    if (currentPattern === pattern.value) {
                        row.style.cssText += 'background:var(--color-bg-primary);';
                    }

                    var labelDiv = document.createElement('div');
                    labelDiv.style.cssText = 'font-weight:bold;font-size:12px;color:var(--color-text-primary);';
                    labelDiv.textContent = pattern.label;

                    var descDiv = document.createElement('div');
                    descDiv.style.cssText = 'font-size:11px;color:var(--color-text-secondary);';
                    descDiv.textContent = pattern.desc;

                    row.appendChild(labelDiv);
                    row.appendChild(descDiv);

                    row.addEventListener('click', function() {
                        self.proposition.pattern = pattern.value;
                        self.appendLog('Proof pattern set to: ' + pattern.value + ' / 证明模式已设置为: ' + pattern.label, 'info');
                        self.showSuccess('证明模式已设置: ' + pattern.label);

                        // 关闭模态框
                        var overlay = document.getElementById('modalOverlay');
                        if (overlay) overlay.classList.remove('active');
                    });

                    body.appendChild(row);
                })(patterns[i]);
            }
        }
    };

    // ================================================================
    // 统一性检查
    // 基于约束图的约束关系，检查证明前提条件是否一致。
    // 检测冲突约束：如果存在两个约束对同一组节点给出了矛盾的关系，
    // 则说明前提不统一（不一致），证明可能存在问题。
    // ================================================================
    Lv00WebApp.prototype.proofUnifyCheck = function() {
        if (!this.graph) {
            this.appendLog('Unify check: 约束图未初始化 / No constraint graph', 'warn');
            this.showToast('约束图未初始化，无法进行统一性检查', 'warning');
            return;
        }

        var issues = [];
        var warnings = [];

        // 1. 检查约束图中的冲突约束（通过 .conflict 标志）
        if (this.graph.constraints) {
            for (var i = 0; i < this.graph.constraints.length; i++) {
                var c = this.graph.constraints[i];
                if (c.conflict) {
                    issues.push('约束 #' + i + ' 标记为冲突 (冲突约束类型: ' + (c.type !== undefined ? c.type : '未知') + ')');
                }
            }
        }

        // 2. 检查重复约束（相同类型、相同参数的约束）
        // fix: 以下为 O(n^2) 双重循环检测重复约束，约束数量较大时性能会下降。
        //      建议优化方向：利用 { type + arg1 + arg2 } 字符串键建立 Set/Map
        //      实现 O(n) 检测；或在约束添加时即做去重判断。
        if (this.graph.constraints && this.graph.constraints.length > 1) {
            for (var j = 0; j < this.graph.constraints.length; j++) {
                for (var k = j + 1; k < this.graph.constraints.length; k++) {
                    var c1 = this.graph.constraints[j];
                    var c2 = this.graph.constraints[k];
                    if (c1.type === c2.type &&
                        c1.arg1 === c2.arg1 &&
                        c1.arg2 === c2.arg2) {
                        warnings.push('发现重复约束: #' + j + ' 和 #' + k);
                    }
                }
            }
        }

        // 3. 检查节点引用的完整性（约束引用的节点是否存在）
        if (this.graph.constraints && this.graph.nodes) {
            for (var m = 0; m < this.graph.constraints.length; m++) {
                var constraint = this.graph.constraints[m];
                var refs = [constraint.arg1, constraint.arg2, constraint.arg3];
                for (var n = 0; n < refs.length; n++) {
                    if (refs[n] === undefined || refs[n] === null) continue;
                    var found = false;
                    for (var p = 0; p < this.graph.nodes.length; p++) {
                        if (this.graph.nodes[p].id === refs[n]) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        warnings.push('约束 #' + m + ' 引用了不存在的节点: ' + refs[n]);
                    }
                }
            }
        }

        // 汇总结果并展示
        var resultMsg = '';
        if (issues.length === 0 && warnings.length === 0) {
            resultMsg = '统一性检查通过 / Unification check passed -- 未发现问题';
            this.appendLog('Unify check: ' + resultMsg, 'info');
            this.showSuccess('统一性检查通过，前提条件当前一致');
        } else {
            if (issues.length > 0) {
                resultMsg += '### 问题 (Issues) ###\n' + issues.join('\n') + '\n';
            }
            if (warnings.length > 0) {
                resultMsg += '### 警告 (Warnings) ###\n' + warnings.join('\n') + '\n';
            }
            this.appendLog('Unify check 发现问题:\n' + resultMsg, 'warn');
            this.showToast('统一性检查发现问题: ' + issues.length + ' 个问题, ' + warnings.length + ' 个警告', 'warning');
        }

        // 更新证明状态
        if (this.proposition) {
            this.proposition.unifyStatus = (issues.length === 0) ? 'consistent' : 'inconsistent';
            var statusEl = document.getElementById('proofStatus');
            if (statusEl) {
                statusEl.textContent = this.proposition.unifyStatus === 'consistent' ? 'UNIFORM / 一致' : 'CONFLICT / 冲突';
            }
        }
    };

    // ================================================================
    // 导航到下一步
    // 在证明步骤中前进到下一步
    // ================================================================
    Lv00WebApp.prototype.proofNavNext = function() {
        if (!this.proposition) {
            this.appendLog('No proposition / 无命题', 'warn');
            return;
        }

        if (this.proposition.currentStep < this.proposition.steps.length) {
            this.proposition.currentStep++;
        } else {
            this.appendLog('Already at last step / 已在最后一步', 'info');
            return;
        }

        // 更新 UI
        var stepEl = document.getElementById('proofStep');
        if (stepEl) {
            stepEl.textContent = this.proposition.currentStep + ' / ' + this.proposition.steps.length;
        }

        this.appendLog('Proof step: ' + this.proposition.currentStep + ' / 证明步骤: ' + this.proposition.currentStep, 'info');
    };

    // ================================================================
    // 导航到上一步
    // 在证明步骤中回退到上一步
    // ================================================================
    Lv00WebApp.prototype.proofNavPrev = function() {
        if (!this.proposition) {
            this.appendLog('No proposition / 无命题', 'warn');
            return;
        }

        if (this.proposition.currentStep > 0) {
            this.proposition.currentStep--;
        } else {
            this.appendLog('Already at first step / 已在第一步', 'info');
            return;
        }

        // 更新 UI
        var stepEl = document.getElementById('proofStep');
        if (stepEl) {
            stepEl.textContent = this.proposition.currentStep + ' / ' + this.proposition.steps.length;
        }

        this.appendLog('Proof step: ' + this.proposition.currentStep + ' / 证明步骤: ' + this.proposition.currentStep, 'info');
    };

    // ================================================================
    // 跳转到指定步骤
    // @param {number} step - 目标步骤编号（从 0 开始）
    // 如果步骤超出范围，则截断到最近的有效步骤。
    // 跳转后自动更新 UI 和日志。
    // ================================================================
    Lv00WebApp.prototype.proofNavJump = function(step) {
        if (!this.proposition) {
            this.appendLog('Jump to step: 无命题 / No proposition', 'warn');
            this.showToast('请先创建命题', 'warning');
            return;
        }

        // 参数验证：step 必须为数字
        if (typeof step !== 'number' || isNaN(step) || step < 0) {
            this.appendLog('Jump to step: 无效的步骤编号 / Invalid step number: ' + step, 'warn');
            this.showToast('步骤编号无效，请输入非负整数', 'warning');
            return;
        }

        var totalSteps = this.proposition.steps.length;
        var targetStep;

        // 如果 totalSteps 为 0，则只能跳到步骤 0
        if (totalSteps === 0) {
            targetStep = 0;
        } else if (step >= totalSteps) {
            targetStep = totalSteps - 1;
            this.appendLog('Jump to step: 超出范围，已截止到步骤 ' + targetStep + ' / Clamped to step ' + targetStep, 'info');
        } else {
            targetStep = step;
        }

        // 执行跳转
        var prevStep = this.proposition.currentStep;
        this.proposition.currentStep = targetStep;

        // 更新 UI
        var stepEl = document.getElementById('proofStep');
        if (stepEl) {
            stepEl.textContent = this.proposition.currentStep + ' / ' + totalSteps;
        }

        this.appendLog('Proof jump: ' + prevStep + ' -> ' + targetStep + ' / ' + totalSteps +
                       ' / 证明跳转: ' + prevStep + ' -> ' + targetStep + ' / ' + totalSteps, 'info');
        this.showInfo('已跳转到步骤 ' + targetStep + ' / ' + totalSteps);
    };

    // ================================================================
    // 导出为 HTML
    // 生成包含完整证明信息的 HTML 文档，并以文件下载方式输出。
    // 文档包含：命题标题、证明模式、步骤列表、约束图摘要等。
    // ================================================================
    Lv00WebApp.prototype.proofExportHTML = function() {
        // 构建 HTML 文档内容
        var now = new Date();
        var timestamp = now.toISOString().replace(/T/, ' ').replace(/\..+/, '');

        var title = (this.proposition && this.proposition.title) ? this.proposition.title : 'Untitled Proof';
        var pattern = (this.proposition && this.proposition.pattern) ? this.proposition.pattern : 'forward';
        var patternLabels = { forward: '正向证明', contrapositive: '反证', reductio: '归谬' };
        var patternLabel = patternLabels[pattern] || pattern;

        var nodeCount = this.points ? this.points.length : 0;
        var edgeCount = this.segments ? this.segments.length : 0;
        var constraintCount = (this.graph && this.graph.constraints) ? this.graph.constraints.length : 0;

        var stepsHTML = '';
        if (this.proposition && this.proposition.steps && this.proposition.steps.length > 0) {
            stepsHTML += '<h3>Proof Steps / 证明步骤</h3>\n<ol>\n';
            for (var i = 0; i < this.proposition.steps.length; i++) {
                var stepText = _escapeHtml(this.proposition.steps[i]);  // fix: 使用公共 _escapeHtml 函数
                stepsHTML += '  <li>' + stepText + '</li>\n';
            }
            stepsHTML += '</ol>\n';
        } else {
            stepsHTML += '<p><em>No proof steps recorded / 未记录证明步骤</em></p>\n';
        }

        var pointsHTML = '';
        if (this.points && this.points.length > 0) {
            pointsHTML += '<h3>Nodes / 节点</h3>\n<ul>\n';
            for (var j = 0; j < this.points.length; j++) {
                pointsHTML += '  <li>n' + this.points[j].id + ': (' +
                    this.points[j].x.toFixed(4) + ', ' + this.points[j].y.toFixed(4) + ')</li>\n';
            }
            pointsHTML += '</ul>\n';
        }

        var constraintsHTML = '';
        if (this.graph && this.graph.constraints && this.graph.constraints.length > 0) {
            constraintsHTML += '<h3>Constraints / 约束</h3>\n<ul>\n';
            var typeNames = ['INCIDENCE', 'BETWEENNESS', 'INTERSECTION', 'CONTAINMENT', 'CONNECTION'];
            for (var k = 0; k < this.graph.constraints.length; k++) {
                var c = this.graph.constraints[k];
                var typeName = (c.type !== undefined && typeNames[c.type]) ? typeNames[c.type] : 'UNKNOWN';
                var argsStr = '';
                if (c.arg1 !== undefined) argsStr += ' arg1=' + c.arg1;
                if (c.arg2 !== undefined) argsStr += ' arg2=' + c.arg2;
                if (c.arg3 !== undefined) argsStr += ' arg3=' + c.arg3;
                constraintsHTML += '  <li>#' + k + ' [' + typeName + ']' + argsStr + '</li>\n';
            }
            constraintsHTML += '</ul>\n';
        }

        var html = '<!DOCTYPE html>\n<html lang="zh-CN">\n<head>\n' +
            '<meta charset="UTF-8">\n' +
            '<meta name="viewport" content="width=device-width, initial-scale=1.0">\n' +
            '<title>' + _escapeHtml(title) + ' - Lv-00 Proof Export</title>\n' +
            '<style>\n' +
            '  body { font-family: system-ui, -apple-system, sans-serif; max-width: 800px; margin: 40px auto; padding: 20px; color: #333; background: #fff; }\n' +
            '  h1 { border-bottom: 2px solid #333; padding-bottom: 10px; }\n' +
            '  h2 { color: #555; margin-top: 30px; }\n' +
            '  .meta { color: #888; font-size: 14px; margin-bottom: 20px; }\n' +
            '  .stats { background: #f5f5f5; padding: 10px 15px; border-radius: 4px; }\n' +
            '  ol li { margin: 6px 0; }\n' +
            '  ul li { margin: 4px 0; font-family: monospace; font-size: 13px; }\n' +
            '  footer { margin-top: 40px; padding-top: 15px; border-top: 1px solid #ddd; color: #999; font-size: 12px; }\n' +
            '</style>\n</head>\n<body>\n' +
            '<h1>Lv-00 Proof Export / 证明导出</h1>\n' +
            '<h2>' + _escapeHtml(title) + '</h2>\n' +
            '<div class="meta">\n' +
            '  <p><strong>Proof Pattern / 证明模式:</strong> ' + patternLabel + ' (' + pattern + ')</p>\n' +
            '  <p><strong>Generated / 生成时间:</strong> ' + timestamp + '</p>\n' +
            '  <p><strong>Engine / 引擎:</strong> Lv-00 v3.0.0</p>\n' +
            '</div>\n' +
            '<div class="stats">\n' +
            '  <p>Nodes / 节点: ' + nodeCount + ' | Segments / 线段: ' + edgeCount + ' | Constraints / 约束: ' + constraintCount + '</p>\n' +
            '</div>\n' +
            stepsHTML +
            pointsHTML +
            constraintsHTML +
            '<footer>\n' +
            '  <p>Exported by Lv-00 Symbolic Geometry Engine. / 由 Lv-00 符号几何引擎导出。</p>\n' +
            '</footer>\n</body>\n</html>';

        try {
            var blob = new Blob([html], { type: 'text/html;charset=utf-8' });
            var url = URL.createObjectURL(blob);
            var a = document.createElement('a');
            a.href = url;
            a.download = 'lv00_proof_' +
                now.toISOString().slice(0, 19).replace(/[T:]/g, '-') +
                '.html';
            document.body.appendChild(a);
            a.click();

            setTimeout(function() {
                document.body.removeChild(a);
                URL.revokeObjectURL(url);
            }, 100);

            this.appendLog('HTML proof exported: ' + a.download + ' / HTML证明已导出: ' + a.download, 'info');
            this.showSuccess('HTML 证明已导出: ' + a.download);
        } catch (e) {
            this.appendLog('HTML export failed: ' + e.message + ' / HTML导出失败', 'error');
            this.showError('HTML 导出失败: ' + e.message);
        }
    };

    // ================================================================
    // 导出为 LaTeX
    // 生成规范的 .tex 文件，包含证明的完整 LaTeX 源代码。
    // 文档结构：
    //   - 文档类 article
    //   - amsmath, amssymb, amsthm 宏包
    //   - 自定义 proof 环境
    //   - 命题声明
    //   - 证明步骤（基于当前命题数据）
    //   - 节点坐标表
    // ================================================================
    Lv00WebApp.prototype.proofExportLaTeX = function() {
        var title = (this.proposition && this.proposition.title) ? this.proposition.title : 'Untitled Proof';
        var pattern = (this.proposition && this.proposition.pattern) ? this.proposition.pattern : 'forward';
        var patternEnv = {
            forward: 'proof',
            contrapositive: 'proof',
            reductio: 'proof'
        };

        // 转义 LaTeX 特殊字符
        var escapeLaTeX = function(str) {
            return String(str)
                .replace(/\\/g, '\\textbackslash ')
                .replace(/[&%$#_{}]/g, function(ch) {
                    return '\\' + ch;
                })
                .replace(/~/g, '\\textasciitilde ')
                .replace(/\^/g, '\\textasciicircum ');
        };

        var tex = '% !TEX program = pdflatex\n';
        tex += '% Lv-00 Symbolic Geometry Engine -- Proof Export\n';
        tex += '% Generated: ' + new Date().toISOString() + '\n';
        tex += '% Proof Pattern: ' + pattern + '\n\n';

        tex += '\\documentclass[12pt,a4paper]{article}\n\n';
        tex += '% --- Packages ---\n';
        tex += '\\usepackage[UTF8]{ctex}\n';
        tex += '\\usepackage{amsmath,amssymb,amsthm}\n';
        tex += '\\usepackage{geometry}\n';
        tex += '\\geometry{margin=2.5cm}\n\n';

        tex += '% --- Theorem Environments ---\n';
        tex += '\\newtheorem{proposition}{Proposition}[section]\n';
        tex += '\\newtheorem{lemma}{Lemma}[section]\n\n';

        tex += '% --- Title ---\n';
        tex += '\\title{' + escapeLaTeX(title) + '}\n';
        tex += '\\author{Lv-00 Proof Engine}\n';
        tex += '\\date{\\today}\n\n';

        tex += '\\begin{document}\n';
        tex += '\\maketitle\n\n';

        tex += '% --- Proposition ---\n';
        tex += '\\begin{proposition}\n';
        tex += '  % 请在此处填写命题内容，或使用 proposition.steps 自动生成\n';
        tex += '\\end{proposition}\n\n';

        tex += '% --- Proof ---\n';
        tex += '\\begin{proof}\n';

        if (this.proposition && this.proposition.steps && this.proposition.steps.length > 0) {
            for (var i = 0; i < this.proposition.steps.length; i++) {
                tex += '  % Step ' + (i + 1) + '\n';
                tex += '  ' + escapeLaTeX(String(this.proposition.steps[i])) + '\n\n';
            }
        } else {
            tex += '  % 证明步骤待补充，请先在证明面板中添加步骤\n';
        }

        tex += '\\end{proof}\n\n';

        // 节点坐标表
        if (this.points && this.points.length > 0) {
            tex += '% --- Coordinate Table ---\n';
            tex += '\\begin{table}[h]\n';
            tex += '  \\centering\n';
            tex += '  \\begin{tabular}{|c|c|c|}\n';
            tex += '    \\hline\n';
            tex += '    Node ID & X & Y \\\\ \\hline\n';
            for (var j = 0; j < this.points.length; j++) {
                tex += '    $n_' + this.points[j].id + '$ & $' +
                    this.points[j].x.toFixed(4) + '$ & $' +
                    this.points[j].y.toFixed(4) + '$ \\\\ \\hline\n';
            }
            tex += '  \\end{tabular}\n';
            tex += '  \\caption{Node Coordinates / 节点坐标}\n';
            tex += '  \\label{tab:coordinates}\n';
            tex += '\\end{table}\n\n';
        }

        tex += '% --- End of Document ---\n';
        tex += '\\end{document}\n';

        try {
            var blob = new Blob([tex], { type: 'application/x-tex;charset=utf-8' });
            var url = URL.createObjectURL(blob);
            var a = document.createElement('a');
            a.href = url;
            a.download = 'lv00_proof_' +
                new Date().toISOString().slice(0, 19).replace(/[T:]/g, '-') +
                '.tex';
            document.body.appendChild(a);
            a.click();

            setTimeout(function() {
                document.body.removeChild(a);
                URL.revokeObjectURL(url);
            }, 100);

            this.appendLog('LaTeX proof exported: ' + a.download + ' / LaTeX证明已导出: ' + a.download, 'info');
            this.showSuccess('LaTeX 证明已导出: ' + a.download);
        } catch (e) {
            this.appendLog('LaTeX export failed: ' + e.message + ' / LaTeX导出失败', 'error');
            this.showError('LaTeX 导出失败: ' + e.message);
        }
    };

    // ================================================================
    // 导出 Coq 证明
    // 生成基于当前证明步骤的 Coq 代码并以模态框方式展示
    // （替代原有的 alert，提供更详细的信息展示）
    // ================================================================
    Lv00WebApp.prototype._proofExportCoq = function() {
        var self = this;
        // 构建 Coq 代码骨架
        var coqCode =
            '(* LV-00 Auto-generated Coq Proof *)\n' +
            '(* Generated: ' + new Date().toISOString() + ' *)\n\n' +
            'Lemma lv00_proof : True.\n' +
            'Proof.\n' +
            '  (* 请使用证明步骤替换此行 *)\n' +
            '  trivial.\n' +
            'Qed.\n';

        // 使用模态框展示 Coq 代码（比 alert 更详细、用户体验更好）
        this._showModal('modalOverlay');

        var title = document.getElementById('modalTitle');
        if (title) {
            title.textContent = 'Coq Proof Export / Coq 证明导出';
        }

        var body = document.getElementById('modalBody');
        if (body) {
            // 使用 DOM API 安全构建内容，避免 innerHTML XSS 风险
            while (body.firstChild) {
                body.removeChild(body.firstChild);
            }

            var infoPara = document.createElement('p');
            infoPara.style.cssText = 'font-size:11px;color:var(--color-text-secondary);margin-bottom:8px;';
            infoPara.textContent = 'Coq 证明代码已生成，请复制下方代码到您的 Coq 编辑器中使用：';
            body.appendChild(infoPara);

            var preBlock = document.createElement('pre');
            preBlock.style.cssText = 'background:var(--color-bg-primary);border:1px solid var(--color-border-secondary);border-radius:4px;padding:10px;font-size:11px;font-family:Consolas,monospace;color:var(--color-text-primary);white-space:pre-wrap;max-height:400px;overflow-y:auto;';
            preBlock.textContent = coqCode;
            body.appendChild(preBlock);

            var copyBtn = document.createElement('button');
            copyBtn.className = 'btn';
            copyBtn.textContent = '复制代码 / Copy Code';
            copyBtn.style.cssText = 'margin-top:8px;';
            copyBtn.addEventListener('click', function() {
                // fix: navigator.clipboard 在非 HTTPS 环境（如 http://localhost）下不可用。
                //      .catch 分支已做降级处理，但仍建议用户确保在 HTTPS 或 localhost 下使用。
                navigator.clipboard.writeText(coqCode).then(function() {
                    self.showToast('Coq 代码已复制到剪贴板', 'success');
                }).catch(function() {
                    self.showToast('复制失败，请手动选择复制', 'error');
                });
            });
            body.appendChild(copyBtn);
        }

        this.appendLog('Coq 证明代码已生成并通过模态框展示 / Coq proof code generated and displayed in modal', 'info');
        this.appendLog('Coq code:\n' + coqCode, 'info');
    };

    // ================================================================
    // 矛盾证明（Ex Falso Quodlibet）
    // 从矛盾中推导出任意命题
    // ================================================================
    Lv00WebApp.prototype._proofExFalso = function() {
        // 守卫：检查是否存在矛盾状态
        if (!this.graph) {
            this.appendLog('矛盾证明：约束图未初始化 / Ex Falso: no graph', 'warn');
            return;
        }

        // 检查冲突约束
        var hasConflict = false;
        if (this.graph.constraints) {
            for (var i = 0; i < this.graph.constraints.length; i++) {
                if (this.graph.constraints[i].conflict) {
                    hasConflict = true;
                    break;
                }
            }
        }

        if (hasConflict) {
            this.appendLog('矛盾证明已应用：检测到冲突约束 / Ex Falso: contradiction from conflicts', 'info');
            this.showInfo('Ex Falso quodlibet: 从矛盾可推出任意命题 / From contradiction, anything follows');
        } else {
            this.appendLog('矛盾证明：未检测到冲突，尝试基于约束图推导 / Ex Falso: no explicit conflict', 'warn');
            this.showWarning('Ex Falso: 未检测到显式矛盾 / No explicit contradiction detected');
        }
    };

})();
