/**
 * @module components/panels/utils/proofSvgGenerator
 * @description 证明模块的 SVG 几何视图生成工具。
 *              为当前证明步骤生成 SVG 几何构造视图，
 *              以及生成自包含的交互式 HTML 证明文件。
 *
 *              SVG geometry view generator for the proof module.
 *              Generates SVG geometry construction views for proof steps,
 *              and self-contained interactive HTML proof files.
 */

import type { Point, Segment, Constraint } from '@/types';

// ================================================================
// 类型定义 / Type Definitions
// ================================================================

/** 证明步骤快照（包含几何状态） */
export interface ProofSnapshot {
  points: Point[];
  segments: Segment[];
  constraints: Constraint[];
  regions?: unknown[];
  ports?: unknown[];
  funcBlocks?: unknown[];
  timestamp?: number;
}

// ================================================================
// SVG 几何视图生成 / SVG Geometry View Generation
// ================================================================

/**
 * 为指定步骤生成 SVG 几何构造视图字符串。
 * 包含网格、线段、约束关系（虚线）、点（带标签）。
 *
 * @param step - 证明步骤快照
 * @param svgW - SVG 画布宽度（默认 280）
 * @param selectedPointId - 当前选中的点 ID（可选，高亮显示）
 * @returns SVG 字符串
 */
export function generateProofSvg(
  step: ProofSnapshot,
  svgW: number = 280,
  selectedPointId?: number | null,
): string {
  const pts = step.points;
  const segs = step.segments;

  if (pts.length === 0) return '';

  // ---- 计算画布范围 ----
  let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
  for (const p of pts) {
    if (p.x < minX) minX = p.x;
    if (p.y < minY) minY = p.y;
    if (p.x > maxX) maxX = p.x;
    if (p.y > maxY) maxY = p.y;
  }

  // 确保最小范围，避免退化情况
  if (minX === maxX) { minX -= 50; maxX += 50; }
  if (minY === maxY) { minY -= 50; maxY += 50; }

  // 添加边距
  const padX = (maxX - minX) * 0.15 || 10;
  const padY = (maxY - minY) * 0.15 || 10;
  const viewMinX = minX - padX;
  const viewMinY = minY - padY;
  const viewW = (maxX - minX) + 2 * padX;
  const viewH = (maxY - minY) + 2 * padY;

  // SVG 高度按比例计算
  const svgH = Math.max(160, (viewH / viewW) * svgW);

  // 世界坐标 -> SVG 坐标变换（Y 轴翻转）
  const xform = (wx: number) => ((wx - viewMinX) / viewW) * svgW;
  const yform = (wy: number) => svgH - ((wy - viewMinY) / viewH) * svgH;

  let svg = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${svgW} ${svgH}" width="${svgW}" height="${svgH}" class="proof-svg-view">\n`;
  svg += `  <rect width="${svgW}" height="${svgH}" fill="#0d1117" />\n`;

  // ---- 绘制网格 ----
  svg += `  <g stroke="#21262d" stroke-width="0.5">\n`;
  for (let gx = 0; gx <= svgW; gx += 20) {
    svg += `    <line x1="${gx}" y1="0" x2="${gx}" y2="${svgH}" />\n`;
  }
  for (let gy = 0; gy <= svgH; gy += 20) {
    svg += `    <line x1="0" y1="${gy}" x2="${svgW}" y2="${gy}" />\n`;
  }
  svg += `  </g>\n`;

  // ---- 绘制线段 ----
  for (const s of segs) {
    const p1 = pts.find((pp) => pp.id === s.p1);
    const p2 = pts.find((pp) => pp.id === s.p2);
    if (p1 && p2) {
      svg += `  <line x1="${xform(p1.x)}" y1="${yform(p1.y)}" x2="${xform(p2.x)}" y2="${yform(p2.y)}" stroke="#58a6ff" stroke-width="1.5" opacity="0.7" />\n`;
    }
  }

  // ---- 绘制约束（虚线箭头表明约束关系） ----
  const stepConstraints = step.constraints ?? [];
  for (const c of stepConstraints) {
    // 为不同类型的约束使用不同颜色
    let constraintColor = '#8b949e';
    switch (c.type) {
      case 'incidence': constraintColor = '#51cf66'; break;
      case 'betweenness': constraintColor = '#ffd43b'; break;
      case 'intersection': constraintColor = '#ff6b6b'; break;
      case 'containment': constraintColor = '#da77f2'; break;
      case 'connection': constraintColor = '#4dabf7'; break;
    }
    if (c.args.length >= 2) {
      const a1 = pts.find((pp) => pp.id === c.args[0]);
      const a2 = pts.find((pp) => pp.id === c.args[1]);
      if (a1 && a2) {
        svg += `  <line x1="${xform(a1.x)}" y1="${yform(a1.y)}" x2="${xform(a2.x)}" y2="${yform(a2.y)}" stroke="${constraintColor}" stroke-width="1" stroke-dasharray="4,3" opacity="0.5" />\n`;
      }
    }
  }

  // ---- 绘制点 ----
  for (const p of pts) {
    const cx = xform(p.x);
    const cy = yform(p.y);
    const isSelected = selectedPointId != null && selectedPointId === p.id;
    svg += `  <circle cx="${cx}" cy="${cy}" r="${isSelected ? 5 : 3.5}" fill="${isSelected ? '#ffd43b' : '#51cf66'}" stroke="${isSelected ? '#fff' : '#238636'}" stroke-width="1" />\n`;
    svg += `  <text x="${cx + 5}" y="${cy - 5}" fill="#e6edf3" font-size="8" font-family="monospace">p${p.id}</text>\n`;
  }

  svg += `</svg>`;
  return svg;
}

// ================================================================
// HTML 导出 / HTML Export
// ================================================================

/**
 * 为每个步骤生成 SVG 数据（用于 HTML 导出）。
 * 返回 SVG 字符串数组，每个元素对应一个步骤。
 *
 * @param proofSteps - 证明步骤快照数组
 * @returns SVG 字符串数组
 */
function generateStepSvgs(proofSteps: ProofSnapshot[]): string[] {
  const stepsSvgData: string[] = [];

  for (let i = 0; i < proofSteps.length; i++) {
    const step = proofSteps[i];
    if (!step || step.points.length === 0) {
      stepsSvgData.push('');
      continue;
    }

    const pts = step.points;
    const segs = step.segments;

    // 计算画布范围
    let sminX = Infinity, sminY = Infinity, smaxX = -Infinity, smaxY = -Infinity;
    for (const p of pts) {
      if (p.x < sminX) sminX = p.x;
      if (p.y < sminY) sminY = p.y;
      if (p.x > smaxX) smaxX = p.x;
      if (p.y > smaxY) smaxY = p.y;
    }
    if (sminX === smaxX) { sminX -= 50; smaxX += 50; }
    if (sminY === smaxY) { sminY -= 50; smaxY += 50; }

    const spadX = (smaxX - sminX) * 0.15 || 10;
    const spadY = (smaxY - sminY) * 0.15 || 10;
    const svMinX = sminX - spadX;
    const svMinY = sminY - spadY;
    const svW = (smaxX - sminX) + 2 * spadX;
    const svH = (smaxY - sminY) + 2 * spadY;

    const svgw = 400;
    const svgh = Math.max(200, (svH / svW) * svgw);
    const sxf = (wx: number) => ((wx - svMinX) / svW) * svgw;
    const syf = (wy: number) => svgh - ((wy - svMinY) / svH) * svgh;

    let ssvg = '';
    // 绘制线段
    for (const s of segs) {
      const p1 = pts.find((pp) => pp.id === s.p1);
      const p2 = pts.find((pp) => pp.id === s.p2);
      if (p1 && p2) {
        ssvg += `<line x1="${sxf(p1.x)}" y1="${syf(p1.y)}" x2="${sxf(p2.x)}" y2="${syf(p2.y)}" stroke="#58a6ff" stroke-width="1.5" opacity="0.7"/>\n`;
      }
    }
    // 绘制点
    for (const p of pts) {
      ssvg += `<circle cx="${sxf(p.x)}" cy="${syf(p.y)}" r="4" fill="#51cf66" stroke="#238636" stroke-width="1"/>\n`;
      ssvg += `<text x="${sxf(p.x) + 6}" y="${syf(p.y) - 6}" fill="#e6edf3" font-size="10" font-family="monospace">p${p.id}</text>\n`;
    }

    stepsSvgData.push(
      `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${svgw} ${svgh}" width="100%" style="max-width:400px;height:auto;background:#0d1117;border:1px solid #30363d;border-radius:4px">\n${ssvg}</svg>`
    );
  }

  return stepsSvgData;
}

/**
 * 生成自包含的交互式 HTML 证明文件。
 * 包含所有步骤的 SVG、导航控件、键盘快捷键和统计信息。
 *
 * @param proofSteps - 证明步骤快照数组
 * @param currentStepIndex - 当前步骤索引
 * @param pointsCount - 当前点数
 * @param segmentsCount - 当前线段数
 * @param constraintsCount - 当前约束数
 * @returns HTML 字符串
 */
export function generateHtmlProof(
  proofSteps: ProofSnapshot[],
  currentStepIndex: number,
  pointsCount: number,
  segmentsCount: number,
  constraintsCount: number,
): string {
  if (proofSteps.length === 0) return '';

  const totalSteps = proofSteps.length;
  const stepsSvgData = generateStepSvgs(proofSteps);
  const timestamp = new Date().toISOString().slice(0, 19).replace(/[:-]/g, '');

  // 构建步骤数据（用于 JS 中的信息表）
  const stepsData = proofSteps.map((step, i) => ({
    idx: i,
    points: step?.points?.length ?? 0,
    segments: step?.segments?.length ?? 0,
    constraints: step?.constraints?.length ?? 0,
  }));

  const html = `<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Lv-00 Proof Export / 证明导出 - ${timestamp}</title>
<style>
  :root {
    --bg: #0d1117; --bg2: #161b22; --bg3: #21262d;
    --text: #e6edf3; --text2: #8b949e; --text3: #484f58;
    --accent: #58a6ff; --green: #51cf66; --red: #ff6b6b;
    --border: #30363d; --yellow: #ffd43b;
  }
  * { margin: 0; padding: 0; box-sizing: border-box; }
  body { background: var(--bg); color: var(--text); font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif; padding: 20px; }
  h1 { font-size: 20px; margin-bottom: 8px; color: var(--accent); }
  .meta { font-size: 12px; color: var(--text2); margin-bottom: 20px; }
  .controls { display: flex; align-items: center; gap: 8px; margin-bottom: 16px; flex-wrap: wrap; }
  .controls button { padding: 6px 14px; border: 1px solid var(--border); background: var(--bg2); color: var(--text); cursor: pointer; border-radius: 4px; font-size: 13px; font-family: inherit; transition: background 0.15s; }
  .controls button:hover { background: var(--bg3); }
  .controls button:disabled { opacity: 0.3; cursor: not-allowed; }
  .controls button.active { border-color: var(--accent); color: var(--accent); }
  .step-info { font-size: 13px; color: var(--text2); min-width: 80px; text-align: center; }
  .jump-input { width: 60px; padding: 5px 8px; border: 1px solid var(--border); background: var(--bg2); color: var(--text); font-size: 13px; border-radius: 4px; text-align: center; font-family: monospace; outline: none; }
  .jump-input:focus { border-color: var(--accent); }
  .main-layout { display: flex; gap: 16px; flex-wrap: wrap; }
  .svg-panel { flex: 1; min-width: 300px; }
  .info-panel { flex: 1; min-width: 250px; }
  .info-panel pre { background: var(--bg2); border: 1px solid var(--border); border-radius: 4px; padding: 12px; font-size: 12px; line-height: 1.5; overflow-x: auto; white-space: pre-wrap; word-break: break-word; color: var(--text2); }
  .info-table { width: 100%; border-collapse: collapse; font-size: 12px; }
  .info-table td { padding: 4px 8px; border-bottom: 1px solid var(--border); }
  .info-table .label { color: var(--text2); }
  .info-table .value { color: var(--text); font-weight: 600; text-align: right; }
  .shortcuts { font-size: 11px; color: var(--text3); margin-top: 12px; line-height: 1.6; }
  .shortcuts kbd { background: var(--bg3); border: 1px solid var(--border); border-radius: 3px; padding: 1px 5px; font-family: monospace; font-size: 10px; color: var(--text2); }
  .legend { display: flex; gap: 12px; flex-wrap: wrap; margin-top: 8px; font-size: 11px; color: var(--text2); }
  .legend span { display: flex; align-items: center; gap: 4px; }
  .legend-dot { width: 10px; height: 10px; border-radius: 2px; display: inline-block; }
</style>
</head>
<body>
<h1>Lv-00 Geometry Proof / 几何证明导出</h1>
<div class="meta">Generated: ${new Date().toLocaleString()} | Steps: ${totalSteps} | Points: ${pointsCount} | Segments: ${segmentsCount} | Constraints: ${constraintsCount}</div>

<div class="controls">
  <button onclick="goToStep(0)" id="btnFirst" title="First Step / 第一步">&#x23EE; First</button>
  <button onclick="prevStep()" id="btnPrev" title="Previous Step / 上一步">&#x25C0; Prev</button>
  <span class="step-info">Step <span id="stepNum">${totalSteps}</span> / ${totalSteps}</span>
  <button onclick="nextStep()" id="btnNext" title="Next Step / 下一步">Next &#x25B6;</button>
  <button onclick="goToStep(${totalSteps - 1})" id="btnLast" title="Last Step / 最后一步">Last &#x23ED;</button>
  <input type="number" class="jump-input" id="jumpInput" min="1" max="${totalSteps}" placeholder="Go to..."
    onkeydown="if(event.key==='Enter')goToStep(parseInt(this.value)-1)" />
  <button onclick="toggleAuto()" id="btnAuto" class="active">Auto SVG</button>
</div>

<div class="main-layout">
  <div class="svg-panel" id="svgPanel"></div>
  <div class="info-panel">
    <table class="info-table" id="infoTable"></table>
    <div class="legend" style="margin-top:12px">
      <span><span class="legend-dot" style="background:#51cf66"></span> Point / 点</span>
      <span><span class="legend-dot" style="background:#58a6ff"></span> Segment / 线段</span>
      <span style="color:#ff6b6b">---</span> Constraint / 约束
    </div>
  </div>
</div>

<div class="shortcuts">
  <strong>Keyboard Shortcuts / 快捷键:</strong><br/>
  <kbd>Left Arrow</kbd> Prev step / 上一步 &nbsp;
  <kbd>Right Arrow</kbd> Next step / 下一步 &nbsp;
  <kbd>Home</kbd> First step / 第一步 &nbsp;
  <kbd>End</kbd> Last step / 最后一步<br/>
  <kbd>Space</kbd> Toggle auto-play / 切换自动播放 &nbsp;
  <kbd>G</kbd> Focus jump input / 跳转步骤
</div>

<script>
var currentStep = ${currentStepIndex};
var totalSteps = ${totalSteps};
var autoPlayInterval = null;
var showAutoSvg = true;

var stepsData = ${JSON.stringify(stepsData)};
var svgs = ${JSON.stringify(stepsSvgData)};

function updateStep(newStep) {
  if (newStep < 0 || newStep >= totalSteps) return;
  currentStep = newStep;
  document.getElementById('stepNum').textContent = newStep + 1;
  document.getElementById('btnPrev').disabled = (currentStep <= 0);
  document.getElementById('btnNext').disabled = (currentStep >= totalSteps - 1);
  document.getElementById('btnFirst').disabled = (currentStep <= 0);
  document.getElementById('btnLast').disabled = (currentStep >= totalSteps - 1);
  document.getElementById('jumpInput').value = '';

  var svgPanel = document.getElementById('svgPanel');
  if (showAutoSvg && svgs[newStep]) {
    svgPanel.innerHTML = svgs[newStep];
  }

  var sd = stepsData[newStep];
  var info = '<tr><td class="label">Step / 步骤</td><td class="value">' + (newStep + 1) + ' / ' + totalSteps + '</td></tr>';
  if (sd) {
    info += '<tr><td class="label">Points / 点</td><td class="value" style="color:#51cf66">' + sd.points + '</td></tr>';
    info += '<tr><td class="label">Segments / 线段</td><td class="value" style="color:#58a6ff">' + sd.segments + '</td></tr>';
    info += '<tr><td class="label">Constraints / 约束</td><td class="value" style="color:#ffd43b">' + sd.constraints + '</td></tr>';
  }
  document.getElementById('infoTable').innerHTML = info;
}

function prevStep() { updateStep(currentStep - 1); }
function nextStep() { updateStep(currentStep + 1); }
function goToStep(step) {
  if (isNaN(step) || step < 0 || step >= totalSteps) { alert('Please enter a valid step (1-' + totalSteps + ').'); return; }
  updateStep(step);
}
function toggleAuto() {
  showAutoSvg = !showAutoSvg;
  var btn = document.getElementById('btnAuto');
  if (showAutoSvg) {
    btn.classList.add('active');
    updateStep(currentStep);
  } else {
    btn.classList.remove('active');
    document.getElementById('svgPanel').innerHTML = '';
  }
}

document.addEventListener('keydown', function(e) {
  if (e.target.tagName === 'INPUT') return;
  switch(e.key) {
    case 'ArrowLeft': e.preventDefault(); prevStep(); break;
    case 'ArrowRight': e.preventDefault(); nextStep(); break;
    case 'Home': e.preventDefault(); goToStep(0); break;
    case 'End': e.preventDefault(); goToStep(totalSteps - 1); break;
    case ' ': e.preventDefault(); toggleAuto(); break;
    case 'g': case 'G': e.preventDefault(); var inp = document.getElementById('jumpInput'); inp.focus(); inp.select(); break;
  }
});

updateStep(currentStep);
</script>
</body>
</html>`;

  return html;
}
