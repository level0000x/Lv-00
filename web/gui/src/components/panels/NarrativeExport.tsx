/**
 * @module components/panels/NarrativeExport
 * @description Penrose 风格的自动化几何叙述生成器。
 *              根据当前几何构造，自动生成叙述性证明/故事和带标注的 SVG 可视化。
 *
 *              Penrose-style automated geometric narrative generator.
 *              Given the current geometric construction, automatically generates
 *              a narrative proof/story and an annotated SVG visualization.
 *
 * 主要功能 / Key Features:
 * - 自动检测几何构造中的模式（三角形、平行四边形、圆等）
 * - 生成叙述性证明步骤，支持中文和英文
 * - 支持多种叙述风格：简洁（concise）、详细（detailed）、教学（pedagogical）
 * - 生成带标注的 SVG 可视化图，可下载或复制
 * - 提供设置面板，可控制是否显示约束和测量信息
 * - 核心逻辑已提取到 utils/narrativeGenerator.ts 和 utils/narrativeSvg.ts
 *
 * 使用示例 / Usage:
 *   // 作为侧边栏面板使用
 *   <NarrativeExport />
 *
 *   // 用户点击"生成叙述"按钮后，自动分析当前几何构造并输出证明步骤
 */

import React, { useState, useCallback, useMemo, useRef } from 'react';
import Panel from './Panel';
import { useAppStore } from '@/stores';

// ---- 提取的工具模块 / Extracted utility modules ----
import {
  detectPattern,
  generateNarrative,
  PATTERN_ICONS,
} from './utils/narrativeGenerator';
import type {
  Narrative,
  NarrativeSettings,
  NarrativeStyle,
  NarrativeLanguage,
} from './utils/narrativeGenerator';
import { generateNarrativeSVG } from './utils/narrativeSvg';

// ================================================================
// Component / 组件
// ================================================================

const NarrativeExport: React.FC = () => {
  const points = useAppStore((s) => s.points);
  const segments = useAppStore((s) => s.segments);
  const constraints = useAppStore((s) => s.constraints);
  const addToast = useAppStore((s) => s.addToast);

  const [narrative, setNarrative] = useState<Narrative | null>(null);
  const [svgString, setSvgString] = useState<string>('');
  const [settings, setSettings] = useState<NarrativeSettings>({
    style: 'detailed',
    language: 'zh',
    showConstraints: true,
    showMeasurements: true,
  });
  const [settingsOpen, setSettingsOpen] = useState(false);
  const [generating, setGenerating] = useState(false);
  const narrativeRef = useRef<HTMLDivElement>(null);

  // Detect pattern (reactive, always up-to-date)
  const pattern = useMemo(
    () => detectPattern(points, segments, constraints),
    [points, segments, constraints],
  );

  // Generate narrative
  const handleGenerateNarrative = useCallback(() => {
    setGenerating(true);
    // Use setTimeout to allow the "generating" UI flash to render
    setTimeout(() => {
      const narr = generateNarrative(points, segments, constraints, pattern, settings);
      setNarrative(narr);
      setGenerating(false);
    }, 150);
  }, [points, segments, constraints, pattern, settings]);

  // Generate SVG
  const handleGenerateSVG = useCallback(() => {
    const svg = generateNarrativeSVG(points, segments, constraints, pattern);
    setSvgString(svg);
    addToast('success', settings.language === 'zh' ? 'SVG 已生成' : 'SVG generated');
  }, [points, segments, constraints, pattern, settings.language, addToast]);

  // Copy narrative to clipboard
  const handleCopyNarrative = useCallback(async () => {
    if (!narrative) return;
    const text = settings.language === 'zh'
      ? narrative.steps.map((s) => `步骤${s.step}: ${s.textZh}`).join('\n')
      : narrative.steps.map((s) => `Step ${s.step}: ${s.text}`).join('\n');
    try {
      await navigator.clipboard.writeText(text);
      addToast('success', settings.language === 'zh' ? '叙述已复制到剪贴板' : 'Narrative copied to clipboard');
    } catch {
      addToast('error', settings.language === 'zh' ? '复制失败' : 'Copy failed');
    }
  }, [narrative, settings.language, addToast]);

  // Download SVG
  const handleDownloadSVG = useCallback(() => {
    if (!svgString) return;
    const blob = new Blob([svgString], { type: 'image/svg+xml' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `lv00-construction-${Date.now()}.svg`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
    addToast('success', settings.language === 'zh' ? 'SVG 已下载' : 'SVG downloaded');
  }, [svgString, settings.language, addToast]);

  // Update a setting
  const updateSetting = useCallback(
    <K extends keyof NarrativeSettings>(key: K, value: NarrativeSettings[K]) => {
      setSettings((prev) => ({ ...prev, [key]: value }));
      // Clear previous results when settings change
      setNarrative(null);
      setSvgString('');
    },
    [],
  );

  const icon = PATTERN_ICONS[pattern.type];

  return (
    <Panel
      title="NARRATIVE EXPORT / 叙述导出"
      panelId="panelNarrativeExport"
      icon={'\u2728'}
    >
      {/* Current pattern indicator */}
      <div className="info-box" style={{ marginBottom: 8, textAlign: 'center' }}>
        <div style={{ fontSize: 20, marginBottom: 2 }}>{icon}</div>
        <div style={{ fontSize: 11, color: 'var(--color-text-secondary)' }}>
          {pattern.labelZh}
        </div>
        <div style={{ fontSize: 10, color: 'var(--color-text-muted)', marginTop: 2 }}>
          {settings.language === 'zh' ? pattern.detailZh : pattern.detail}
        </div>
      </div>

      {/* Penrose-style Settings */}
      <details
        open={settingsOpen}
        onToggle={(e) => setSettingsOpen((e.target as HTMLDetailsElement).open)}
        style={{ marginBottom: 8, fontSize: 11 }}
      >
        <summary style={{
          cursor: 'pointer',
          color: 'var(--color-text-secondary)',
          padding: '4px 0',
          userSelect: 'none',
        }}>
          {'\u2699'} PENROSE SETTINGS / 设置
        </summary>
        <div style={{ padding: '8px 0 4px 0' }}>
          {/* Narrative Style */}
          <div className="input-row" style={{ marginBottom: 6 }}>
            <label style={{ minWidth: 60 }}>STYLE / 风格</label>
            <select
              className="select-field"
              value={settings.style}
              onChange={(e) => updateSetting('style', e.target.value as NarrativeStyle)}
              style={{ fontSize: 11, marginBottom: 0 }}
            >
              <option value="detailed">Detailed / 详细</option>
              <option value="concise">Concise / 简洁</option>
              <option value="educational">Educational / 教学</option>
            </select>
          </div>

          {/* Language */}
          <div className="input-row" style={{ marginBottom: 6 }}>
            <label style={{ minWidth: 60 }}>LANG / 语言</label>
            <select
              className="select-field"
              value={settings.language}
              onChange={(e) => updateSetting('language', e.target.value as NarrativeLanguage)}
              style={{ fontSize: 11, marginBottom: 0 }}
            >
              <option value="zh">Chinese / 中文</option>
              <option value="en">English / 英文</option>
            </select>
          </div>

          {/* Show constraints toggle */}
          <div className="input-row" style={{ marginBottom: 6 }}>
            <label style={{ minWidth: 60 }}>CONSTRAINTS</label>
            <select
              className="select-field"
              value={settings.showConstraints ? 'yes' : 'no'}
              onChange={(e) => updateSetting('showConstraints', e.target.value === 'yes')}
              style={{ fontSize: 11, marginBottom: 0 }}
            >
              <option value="yes">Show / 显示</option>
              <option value="no">Hide / 隐藏</option>
            </select>
          </div>

          {/* Show measurements toggle */}
          <div className="input-row">
            <label style={{ minWidth: 60 }}>MEASUREMENTS</label>
            <select
              className="select-field"
              value={settings.showMeasurements ? 'yes' : 'no'}
              onChange={(e) => updateSetting('showMeasurements', e.target.value === 'yes')}
              style={{ fontSize: 11, marginBottom: 0 }}
            >
              <option value="yes">Show / 显示</option>
              <option value="no">Hide / 隐藏</option>
            </select>
          </div>
        </div>
      </details>

      {/* Action Buttons */}
      <button
        className="btn btn-accent"
        onClick={handleGenerateNarrative}
        disabled={points.length === 0 || generating}
      >
        {generating ? '\u23F3 GENERATING... / 生成中...' : '\uD83D\uDCC4 GENERATE NARRATIVE / 生成叙述'}
      </button>

      <button
        className="btn"
        onClick={handleGenerateSVG}
        disabled={points.length === 0}
      >
        {'\uD83D\uDDBC'} EXPORT SVG / 导出SVG
      </button>

      {/* Narrative Display Area */}
      {narrative && (
        <div ref={narrativeRef} style={{ marginTop: 10 }}>
          {/* Pattern header */}
          <div className="info-box" style={{
            marginBottom: 8,
            borderLeft: '3px solid var(--color-accent)',
            padding: '8px 10px',
          }}>
            <div style={{ fontSize: 11, color: 'var(--color-text-secondary)', marginBottom: 2 }}>
              {settings.language === 'zh' ? '\uD83D\uDCD0 检测到：' : '\uD83D\uDCD0 Detected: '}
              {settings.language === 'zh' ? narrative.pattern.labelZh : narrative.pattern.label}
            </div>
            <div style={{ fontSize: 10, color: 'var(--color-text-muted)' }}>
              {settings.language === 'zh' ? narrative.summaryZh : narrative.summary}
            </div>
          </div>

          {/* Steps */}
          <div style={{
            maxHeight: 280,
            overflowY: 'auto',
            border: '1px solid var(--color-border-secondary)',
            borderRadius: 'var(--radius-sm)',
            padding: '6px 8px',
            background: 'var(--color-bg-tertiary)',
            marginBottom: 8,
          }}>
            {narrative.steps.map((s) => (
              <div key={s.step} style={{
                display: 'flex',
                gap: 8,
                padding: '3px 0',
                borderBottom: '1px solid var(--color-border-secondary)',
                fontSize: 11,
              }}>
                <span style={{
                  color: 'var(--color-accent)',
                  fontWeight: 'var(--font-weight-semibold)',
                  minWidth: 24,
                  flexShrink: 0,
                }}>
                  {settings.language === 'zh' ? `步骤${s.step}` : `Step ${s.step}`}
                </span>
                <span style={{ color: 'var(--color-text-primary)', lineHeight: 1.4 }}>
                  {settings.language === 'zh' ? s.textZh : s.text}
                </span>
              </div>
            ))}
          </div>

          {/* Statistics */}
          <div className="info-box">
            <div className="info-row">
              <span className="info-label">
                {settings.language === 'zh' ? '点' : 'Points'}
              </span>
              <span className="info-value">{narrative.stats.pointCount}</span>
            </div>
            <div className="info-row">
              <span className="info-label">
                {settings.language === 'zh' ? '线段' : 'Segments'}
              </span>
              <span className="info-value">{narrative.stats.segmentCount}</span>
            </div>
            <div className="info-row">
              <span className="info-label">
                {settings.language === 'zh' ? '约束' : 'Constraints'}
              </span>
              <span className="info-value">{narrative.stats.constraintCount}</span>
            </div>
            {narrative.stats.area !== undefined && (
              <div className="info-row">
                <span className="info-label">
                  {settings.language === 'zh' ? '面积' : 'Area'}
                </span>
                <span className="info-value">{narrative.stats.area.toFixed(2)}</span>
              </div>
            )}
            {narrative.stats.perimeter !== undefined && (
              <div className="info-row">
                <span className="info-label">
                  {settings.language === 'zh' ? '周长' : 'Perimeter'}
                </span>
                <span className="info-value">{narrative.stats.perimeter.toFixed(2)}</span>
              </div>
            )}
          </div>

          {/* Copy & Download buttons */}
          <div style={{ display: 'flex', gap: 6, marginTop: 8 }}>
            <button
              className="btn btn-small"
              onClick={handleCopyNarrative}
              style={{ flex: 1 }}
            >
              {'\uD83D\uDCCB'} {settings.language === 'zh' ? '复制叙述' : 'COPY NARRATIVE'}
            </button>
            <button
              className="btn btn-small"
              onClick={handleDownloadSVG}
              disabled={!svgString}
              style={{ flex: 1 }}
            >
              {'\u2B07'} {settings.language === 'zh' ? '下载SVG' : 'DOWNLOAD SVG'}
            </button>
          </div>
        </div>
      )}

      {/* Empty state */}
      {!narrative && points.length === 0 && (
        <div style={{
          textAlign: 'center',
          padding: '20px 10px',
          color: 'var(--color-text-muted)',
          fontSize: 12,
        }}>
          <div style={{ fontSize: 32, marginBottom: 8, opacity: 0.4 }}>{'\u221E'}</div>
          <div>{'\u2190'} Place points to begin / 放置点开始构造</div>
        </div>
      )}

      {/* No narrative generated yet but points exist */}
      {!narrative && points.length > 0 && (
        <div style={{
          textAlign: 'center',
          padding: '16px 10px',
          color: 'var(--color-text-muted)',
          fontSize: 12,
        }}>
          <div style={{ marginBottom: 4 }}>
            {settings.language === 'zh'
              ? `点击上方按钮生成叙述 (${points.length} 个点可用)`
              : `Click the button above to generate a narrative (${points.length} point(s) available)`}
          </div>
        </div>
      )}

      {/* SVG preview */}
      {svgString && (
        <div style={{ marginTop: 10 }}>
          <div style={{
            fontSize: 11,
            color: 'var(--color-text-secondary)',
            marginBottom: 4,
            fontWeight: 'var(--font-weight-semibold)',
          }}>
            {'\uD83D\uDDBC'} SVG PREVIEW / 预览
          </div>
          <div
            style={{
              border: '1px solid var(--color-border-secondary)',
              borderRadius: 'var(--radius-sm)',
              overflow: 'hidden',
              background: '#1a1a2e',
            }}
            dangerouslySetInnerHTML={{ __html: svgString }}
          />
        </div>
      )}
    </Panel>
  );
};

export default NarrativeExport;
