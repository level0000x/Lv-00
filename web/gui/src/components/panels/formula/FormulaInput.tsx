/**
 * @module components/panels/formula/FormulaInput
 * @description 公式输入区域子组件。
 *              包含公式文本输入框、语法模式选择、解析/渲染按钮、
 *              实时预览开关及预览内容、符号变量检测和求解入口。
 *
 * 功能特性：
 * - DSL 公式文本输入（双向绑定至 store）
 * - 语法模式选择（AUTO / DSL / LaTeX / Python）
 * - 实时预览（Live Preview）开关及逐条命令详情
 * - Mini SVG 预览画布
 * - 符号变量检测与求解入口
 * - 解析 / 渲染操作按钮
 */

import React, { useState, useCallback, useRef } from 'react';
import Panel from '../Panel';
import { parseFormula } from '@/utils/formulaParser';
import type { FormulaSyntax } from '@/types';

/** 实时预览结果类型 */
interface PreviewResult {
  commands: number;
  validCount: number;
  errors: string[];
  points: Array<{
    label: string;
    x: number;
    y: number;
    isSymbolic: boolean;
    exprX?: string;
    exprY?: string;
  }>;
  parsedCommands: Array<{
    raw: string;
    type: string;
    valid: boolean;
    error?: string;
    lineNum: number;
    description?: string;
  }>;
}

/**
 * FormulaInput 组件属性
 */
interface FormulaInputProps {
  /** 公式输入值（来自 store） */
  formulaInput: string;
  /** 语法模式（来自 store） */
  formulaSyntax: FormulaSyntax;
  /** 设置公式输入值的回调 */
  setFormulaInput: (value: string) => void;
  /** 设置语法模式的回调 */
  setFormulaSyntax: (value: FormulaSyntax) => void;
  /** 解析公式回调 */
  onParse: () => void;
  /** 渲染公式回调 */
  onRender: () => void;
  /** 求解回调 */
  onSolve: () => void;
}

/**
 * FormulaInput - 公式输入区域子组件
 *
 * 包含输入框、语法选择、实时预览、解析/渲染按钮等。
 */
const FormulaInput: React.FC<FormulaInputProps> = ({
  formulaInput,
  formulaSyntax,
  setFormulaInput,
  setFormulaSyntax,
  onParse,
  onRender,
  onSolve,
}) => {
  // ================================================================
  // 本地状态 —— 实时预览
  // ================================================================
  const [livePreviewEnabled, setLivePreviewEnabled] = useState<boolean>(false);
  const [previewResult, setPreviewResult] = useState<PreviewResult | null>(null);
  const [detectedVariables, setDetectedVariables] = useState<string[]>([]);
  const debounceRef = useRef<ReturnType<typeof setTimeout> | null>(null);

  // ================================================================
  // 辅助函数
  // ================================================================

  /** 检测公式中的符号变量 */
  const detectSymbolicVars = useCallback((text: string): string[] => {
    const varSet = new Set<string>();
    const pointRe = /point\s+(\w+)\s*\(\s*([^,]+)\s*,\s*([^)]+)\s*\)/gi;
    let m: RegExpExecArray | null;
    while ((m = pointRe.exec(text)) !== null) {
      const px = m[2]!.trim();
      const py = m[3]!.trim();
      [px, py].forEach((expr) => {
        const tokens = expr.match(/[a-zA-Z_]\w*/g);
        if (tokens) tokens.forEach((t) => { if (!['sin','cos','tan','sqrt','abs','PI','pi','E','e'].includes(t)) varSet.add(t); });
      });
    }
    return Array.from(varSet);
  }, []);

  /** 尝试将 DSL 点坐标中的单个字母变量解析为符号变量 */
  const parsePointWithVars = useCallback((raw: string): {
    label: string;
    x: number;
    y: number;
    isSymbolic: boolean;
    exprX?: string;
    exprY?: string;
  } | null => {
    const m = raw.match(/point\s+(\w+)\s*\(\s*([^,]+)\s*,\s*([^)]+)\s*\)/i);
    if (!m) return null;
    const label = m[1]!;
    const sx = m[2]!.trim();
    const sy = m[3]!.trim();
    const nx = Number(sx);
    const ny = Number(sy);
    if (!isNaN(nx) && !isNaN(ny)) {
      return { label, x: nx, y: ny, isSymbolic: false };
    }
    let xVal = 0, yVal = 0;
    try {
      xVal = 0;
      yVal = 0;
    } catch { /* ignore */ }
    return { label, x: xVal, y: yVal, isSymbolic: true, exprX: sx!, exprY: sy! };
  }, []);

  /** 构造 mini SVG 预览（200x150） */
  const buildMiniSvg = useCallback(
    (pts: Array<{ label: string; x: number; y: number }>): string => {
      if (pts.length === 0) return '';
      const W = 200, H = 150, PAD = 20;
      const xs = pts.map((p) => p.x);
      const ys = pts.map((p) => p.y);
      const minX = Math.min(...xs);
      const maxX = Math.max(...xs);
      const minY = Math.min(...ys);
      const maxY = Math.max(...ys);
      const rangeX = maxX - minX || 1;
      const rangeY = maxY - minY || 1;
      const scaleX = (W - 2 * PAD) / rangeX;
      const scaleY = (H - 2 * PAD) / rangeY;
      const scale = Math.min(scaleX, scaleY);
      const cx = (minX + maxX) / 2;
      const cy = (minY + maxY) / 2;
      const tx = (px: number) => W / 2 + (px - cx) * scale;
      const ty = (py: number) => H / 2 - (py - cy) * scale;
      let svg = `<svg xmlns="http://www.w3.org/2000/svg" width="${W}" height="${H}" style="background:#fafafa;border:1px solid #ccc;border-radius:4px;">`;
      svg += `<line x1="${PAD}" y1="${H/2}" x2="${W-PAD}" y2="${H/2}" stroke="#e0e0e0" stroke-width="0.5"/>`;
      svg += `<line x1="${W/2}" y1="${PAD}" x2="${W/2}" y2="${H-PAD}" stroke="#e0e0e0" stroke-width="0.5"/>`;
      pts.forEach((p) => {
        svg += `<circle cx="${tx(p.x)}" cy="${ty(p.y)}" r="3" fill="#4a90d9" stroke="#2c5f8a" stroke-width="0.5"/>`;
        svg += `<text x="${tx(p.x)+4}" y="${ty(p.y)-2}" font-size="8" fill="#333" font-family="monospace">${p.label}</text>`;
      });
      for (let i = 1; i < pts.length; i++) {
        svg += `<line x1="${tx(pts[i-1]!.x)}" y1="${ty(pts[i-1]!.y)}" x2="${tx(pts[i]!.x)}" y2="${ty(pts[i]!.y)}" stroke="#888" stroke-width="0.8"/>`;
      }
      svg += `</svg>`;
      return svg;
    },
    [],
  );

  /** 防抖实时预览 —— 解析公式并更新 previewResult */
  const runLivePreview = useCallback(
    (text: string) => {
      if (!text.trim()) {
        setPreviewResult(null);
        setDetectedVariables([]);
        return;
      }
      const result = parseFormula(text);
      const validCount = result.commands.filter((c) => c.type !== 'comment' && !c.error).length;
      const pts: Array<{ label: string; x: number; y: number; isSymbolic: boolean; exprX?: string; exprY?: string }> = [];

      const commandDescriptions: Record<string, (raw: string) => string> = {
        point: (raw) => {
          const m = raw.match(/point\s+(\w+)\s*\(\s*([^,]+)\s*,\s*([^)]+)\s*\)/i);
          return m ? `\u5C06\u521B\u5EFA\u70B9 (${m[2]!.trim()},${m[3]!.trim()})` : '\u5C06\u521B\u5EFA\u70B9';
        },
        segment: () => '\u5C06\u521B\u5EFA\u7EBF\u6BB5 / will create a segment',
        circle: () => '\u5C06\u521B\u5EFA\u5706 / will create a circle',
        midpoint: () => '\u5C06\u521B\u5EFA\u4E2D\u70B9 / will create a midpoint',
        perpendicular: () => '\u5C06\u521B\u5EFA\u5782\u7EBF / will create a perpendicular line',
        parallel: () => '\u5C06\u521B\u5EFA\u5E73\u884C\u7EBF / will create a parallel line',
        intersect: () => '\u5C06\u521B\u5EFA\u4EA4\u70B9 / will create an intersection',
        measure: (raw) => raw.includes('distance') ? '\u5C06\u6D4B\u91CF\u8DDD\u79BB / will measure distance' : '\u5C06\u6D4B\u91CF\u89D2\u5EA6 / will measure angle',
      };

      const parsedCommands: Array<{ raw: string; type: string; valid: boolean; error?: string; lineNum: number; description?: string }> = [];
      result.commands.forEach((cmd, idx) => {
        if (cmd.type === 'comment') return;
        if (cmd.type === 'point' && !cmd.error) {
          const parsed = parsePointWithVars(cmd.raw);
          if (parsed) pts.push(parsed);
        }
        const descFn = commandDescriptions[cmd.type];
        const desc = descFn ? descFn(cmd.raw) : undefined;
        parsedCommands.push({
          raw: cmd.raw,
          type: cmd.type,
          valid: !cmd.error,
          error: cmd.error,
          lineNum: idx + 1,
          description: desc,
        });
      });

      setPreviewResult({
        commands: result.commands.length,
        validCount,
        errors: result.errors,
        points: pts,
        parsedCommands,
      });
      const vars = detectSymbolicVars(text);
      setDetectedVariables(vars);
    },
    [parsePointWithVars, detectSymbolicVars],
  );

  /** 输入框 onChange 带防抖 */
  const handleFormulaChange = useCallback(
    (e: React.ChangeEvent<HTMLTextAreaElement>) => {
      const val = e.target.value;
      setFormulaInput(val);
      if (livePreviewEnabled) {
        if (debounceRef.current) clearTimeout(debounceRef.current);
        debounceRef.current = setTimeout(() => runLivePreview(val), 300);
      }
    },
    [setFormulaInput, livePreviewEnabled, runLivePreview],
  );

  /** 切换实时预览开关 —— 开启时立即预览 */
  const handleToggleLivePreview = useCallback(() => {
    setLivePreviewEnabled((prev) => {
      const next = !prev;
      if (next && formulaInput.trim()) {
        runLivePreview(formulaInput);
      } else if (!next) {
        setPreviewResult(null);
        setDetectedVariables([]);
      }
      return next;
    });
  }, [formulaInput, runLivePreview]);

  // ================================================================
  // 渲染
  // ================================================================
  return (
    <Panel title="INPUT / 输入" panelId="formula-input">
      {/* 语法模式选择器：控制后端解析器的语法解析策略 */}
      <div className="formula-syntax-row">
        <label>语法 / Syntax</label>
        <select
          className="select-field"
          value={formulaSyntax}
          onChange={(e) => setFormulaSyntax(e.target.value as FormulaSyntax)}
        >
          <option value="auto">AUTO / 自动</option>
          <option value="dsl">DSL / 术式</option>
          <option value="latex">LaTeX</option>
          <option value="python">Python</option>
        </select>
      </div>
      {/* 公式输入框：双向绑定至 uiStore.formulaInput，带防抖实时预览 */}
      <textarea
        className="input-field"
        id="formulaInput"
        placeholder={`输入几何 DSL 命令 / Enter geometry DSL...&#10;例如:&#10;point A(0, 0)&#10;point B(4, 0)&#10;segment AB&#10;midpoint M of A, B&#10;measure distance A, B`}
        aria-label="数学公式输入框"
        value={formulaInput}
        onChange={handleFormulaChange}
        rows={6}
      />

      {/* Live Preview 开关 */}
      <div className="formula-syntax-row" style={{ marginTop: 4 }}>
        <label>LIVE PREVIEW / 实时预览</label>
        <button
          className={`btn ${livePreviewEnabled ? 'btn-accent' : ''}`}
          onClick={handleToggleLivePreview}
          style={{ fontSize: '11px', padding: '2px 8px' }}
        >
          {livePreviewEnabled ? '\u{1F7E2} LIVE ON / \u5B9E\u65F6\u9884\u89C8\u5F00' : '\u{1F534} LIVE OFF / \u5B9E\u65F6\u9884\u89C8\u5173'}
        </button>
      </div>

      {/* 实时预览 —— 逐条命令详情 */}
      {livePreviewEnabled && previewResult && (
        <div
          style={{
            marginTop: 6,
            border: '1px solid #d0d0d0',
            borderRadius: 4,
            padding: '6px 8px',
            backgroundColor: '#fafafa',
            fontSize: '11px',
            fontFamily: 'Consolas, Monaco, "Courier New", monospace',
            maxHeight: 200,
            overflowY: 'auto',
          }}
        >
          <div style={{ fontWeight: 600, marginBottom: 4, color: '#333', fontSize: '12px' }}>
            {'\uD83D\uDCD0'} {'\u5B9E\u65F6\u9884\u89C8 / Live Preview:'}
          </div>
          {previewResult.parsedCommands.length === 0 ? (
            <div style={{ color: '#999', fontStyle: 'italic' }}>{'\u6682\u65E0\u547D\u4EE4 / No commands'}</div>
          ) : (
            previewResult.parsedCommands.map((cmd, i) => (
              <div
                key={i}
                style={{
                  color: cmd.valid ? '#2e7d32' : '#d32f2f',
                  lineHeight: 1.6,
                  whiteSpace: 'nowrap',
                  overflow: 'hidden',
                  textOverflow: 'ellipsis',
                }}
              >
                {cmd.valid ? (
                  <span>
                    {'  \u2713 '}
                    <span style={{ fontWeight: 600 }}>{cmd.type}</span>
                    {' '}
                    <span>{cmd.raw.replace(/^(\w+)\s+/, '')}</span>
                    {cmd.description && (
                      <span style={{ color: '#666', marginLeft: 4 }}>
                        {'\u2192'} {cmd.description}
                      </span>
                    )}
                  </span>
                ) : (
                  <span>
                    {'  \u26A0 line '}{cmd.lineNum}{': '}{cmd.error}
                  </span>
                )}
              </div>
            ))
          )}
          <div
            style={{
              marginTop: 4,
              paddingTop: 4,
              borderTop: '1px solid #e0e0e0',
              color: '#555',
              fontSize: '10px',
              display: 'flex',
              gap: 12,
            }}
          >
            <span>{'\u2713'} {previewResult.validCount} valid</span>
            <span>{'\u26A0'} {previewResult.errors.length} errors</span>
            <span>{'\uD83D\uDCCD'} {previewResult.points.length} points</span>
          </div>
        </div>
      )}

      {/* Mini SVG 预览画布 */}
      {livePreviewEnabled && previewResult && previewResult.points.length > 0 && (
        <div style={{ marginTop: 6, textAlign: 'center' }}>
          <div
            className="formula-svg-preview"
            dangerouslySetInnerHTML={{
              __html: buildMiniSvg(
                previewResult.points.map((p) => ({ label: p.label, x: p.x, y: p.y })),
              ),
            }}
          />
        </div>
      )}

      {/* 符号变量列表 */}
      {detectedVariables.length > 0 && (
        <div className="info-box" style={{ marginTop: 4, fontSize: '11px' }}>
          <div className="info-row">
            <span>Variables / 变量:</span>
            <span>{detectedVariables.join(', ')}</span>
          </div>
          <button
            className="btn"
            onClick={onSolve}
            style={{ fontSize: '11px', padding: '2px 8px', marginTop: 4 }}
          >
            SOLVE FOR / 求解
          </button>
        </div>
      )}
      {/* 操作按钮行：解析与渲染 */}
      <div className="formula-btn-row">
        <button className="btn btn-accent" onClick={onParse}>
          PARSE / 解析
        </button>
        <button className="btn btn-accent" onClick={onRender}>
          RENDER / 渲染
        </button>
      </div>
    </Panel>
  );
};

export default FormulaInput;
