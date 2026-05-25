/**
 * @module components/streaming/StreamExport
 * @description Stream event export component with dropdown menu for format selection.
 *              Supports JSON, CSV, and Markdown export with scope filtering,
 *              clipboard copy, and file download.
 *
 *              流式事件导出组件，支持下拉菜单选择导出格式。
 *              支持 JSON、CSV、Markdown 导出，可按范围筛选，
 *              提供一键复制和文件下载功能。
 */

import React, { useState, useCallback, useRef, useEffect, useMemo } from 'react';
import type { StreamingEvent, EngineStreamEvent, EngineStreamCategory } from '@/types';

// ================================================================
// Types / 类型定义
// ================================================================

/** Export format / 导出格式 */
type ExportFormat = 'json' | 'csv' | 'markdown';

/** Export scope / 导出范围 */
type ExportScope = 'all' | 'filtered' | 'category';

interface StreamExportProps {
  /** All streaming events / 全部流式事件 */
  events: (StreamingEvent | EngineStreamEvent)[];
  /** Indices of filtered (visible) events / 过滤后可见事件的索引 */
  filteredIndices?: number[];
  /** Base file name for downloads (without extension) / 下载文件基础名（不含扩展名） */
  fileName?: string;
}

// ================================================================
// Constants / 常量
// ================================================================

/** Format labels / 格式标签 */
const FORMAT_LABELS: Record<ExportFormat, { en: string; zh: string; ext: string; mime: string }> = {
  json:     { en: 'JSON',     zh: 'JSON',     ext: '.json',  mime: 'application/json' },
  csv:      { en: 'CSV',      zh: 'CSV',      ext: '.csv',   mime: 'text/csv' },
  markdown: { en: 'Markdown', zh: 'Markdown', ext: '.md',    mime: 'text/markdown' },
};

/** Scope labels / 范围标签 */
const SCOPE_LABELS: Record<ExportScope, { en: string; zh: string }> = {
  all:      { en: 'All Events',      zh: '全部事件' },
  filtered: { en: 'Filtered Events', zh: '过滤后事件' },
  category: { en: 'By Category',     zh: '按类别导出' },
};

/** Category labels / 类别标签 */
const CATEGORY_LABELS: Record<string, { en: string; zh: string }> = {
  engine:      { en: 'Engine',      zh: '引擎' },
  normalize:   { en: 'Normalize',   zh: '归一化' },
  rewrite:     { en: 'Rewrite',     zh: '重写' },
  solve:       { en: 'Solve',       zh: '求解' },
  proof:       { en: 'Proof',       zh: '证明' },
  func_block:  { en: 'Func Block',  zh: '函数块' },
  conflict:    { en: 'Conflict',    zh: '冲突' },
  info:        { en: 'Info',        zh: '信息' },
};

/** All engine stream categories for filtering / 用于过滤的全部引擎流式类别 */
const ALL_CATEGORIES: EngineStreamCategory[] = [
  'engine', 'normalize', 'rewrite', 'solve', 'proof', 'func_block', 'conflict', 'info',
];

// ================================================================
// Type Guards / 类型守卫
// ================================================================

/** Check if an event is an EngineStreamEvent / 检查事件是否为 EngineStreamEvent */
function isEngineStreamEvent(event: StreamingEvent | EngineStreamEvent): event is EngineStreamEvent {
  return 'category' in event && 'timestamp_ms' in event;
}

// ================================================================
// Event Normalization / 事件标准化
// ================================================================

/**
 * 标准化事件行（导出用），将不同事件类型统一为扁平结构
 * Normalized event row for export, unifying different event types into a flat structure
 */
interface NormalizedEvent {
  /** 事件类型标识 */
  type: string;
  /** 步骤编号 */
  step: number;
  /** 事件描述 */
  description: string;
  /** ISO 8601 时间戳 */
  timestamp: string;
  /** 事件类别 */
  category: string;
  /** 关联节点 ID（-1 表示无关联） */
  nodeId: number;
  /** 关联约束 ID（-1 表示无关联） */
  constraintId: number;
}

/** Normalize a streaming event into a flat row / 将流式事件标准化为扁平行 */
function normalizeEvent(event: StreamingEvent | EngineStreamEvent): NormalizedEvent {
  const isEngine = isEngineStreamEvent(event);

  const ts = isEngine ? event.timestamp_ms : (event.timestamp ?? Date.now());
  const timestamp = new Date(ts).toISOString();

  if (isEngine) {
    return {
      type: event.type,
      step: event.step,
      description: event.description,
      timestamp,
      category: event.category,
      nodeId: event.node_id,
      constraintId: event.constraint_id,
    };
  }

  // Legacy StreamingEvent
  return {
    type: String(event.type),
    step: event.stepNumber,
    description: event.description,
    timestamp,
    category: 'info',
    nodeId: event.nodeId ?? -1,
    constraintId: -1,
  };
}

// ================================================================
// Export Formatters / 导出格式化器
// ================================================================

/**
 * Format events as JSON string / 将事件格式化为 JSON 字符串
 */
function formatJSON(events: NormalizedEvent[]): string {
  return JSON.stringify(events, null, 2);
}

/**
 * Format events as CSV string / 将事件格式化为 CSV 字符串
 */
function formatCSV(events: NormalizedEvent[]): string {
  const header = 'type,step,description,timestamp,category,node_id,constraint_id';
  const rows = events.map((e) => {
    // Escape double quotes and wrap description in quotes
    const desc = `"${e.description.replace(/"/g, '""')}"`;
    return `${e.type},${e.step},${desc},${e.timestamp},${e.category},${e.nodeId},${e.constraintId}`;
  });
  return [header, ...rows].join('\n');
}

/**
 * Format events as Markdown timeline / 将事件格式化为 Markdown 时间线
 */
function formatMarkdown(events: NormalizedEvent[]): string {
  const lines: string[] = [
    '# Stream Events / 流式事件',
    '',
    `> Exported at ${new Date().toLocaleString()} | 共 ${events.length} 条事件`,
    '',
    '---',
    '',
  ];

  // Group by category
  const grouped = new Map<string, NormalizedEvent[]>();
  for (const e of events) {
    const cat = e.category || 'info';
    if (!grouped.has(cat)) grouped.set(cat, []);
    grouped.get(cat)!.push(e);
  }

  for (const [category, catEvents] of grouped) {
    const catLabel = CATEGORY_LABELS[category] ?? { en: category, zh: category };
    lines.push(`## ${catLabel.en} / ${catLabel.zh}`);
    lines.push('');

    for (const e of catEvents) {
      const timeShort = e.timestamp.replace('T', ' ').slice(0, 19);
      lines.push(`- **[${timeShort}]** Step ${e.step} | ${e.description}`);
      if (e.nodeId >= 0) {
        lines.push(`  - Node ID: ${e.nodeId}`);
      }
      if (e.constraintId >= 0) {
        lines.push(`  - Constraint ID: ${e.constraintId}`);
      }
    }

    lines.push('');
  }

  return lines.join('\n');
}

/** Get the formatter for a given export format / 获取指定导出格式的格式化器 */
function getFormatter(format: ExportFormat): (events: NormalizedEvent[]) => string {
  switch (format) {
    case 'json':     return formatJSON;
    case 'csv':      return formatCSV;
    case 'markdown': return formatMarkdown;
  }
}

// ================================================================
// Component / 组件
// ================================================================

const StreamExport: React.FC<StreamExportProps> = ({
  events,
  filteredIndices,
  fileName = 'stream-events',
}) => {
  const [menuOpen, setMenuOpen] = useState(false);
  const [scope, setScope] = useState<ExportScope>('all');
  const [selectedCategories, setSelectedCategories] = useState<Set<EngineStreamCategory>>(
    new Set(ALL_CATEGORIES),
  );
  const [toastMessage, setToastMessage] = useState<string | null>(null);
  const menuRef = useRef<HTMLDivElement>(null);
  const toastTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);

  // Close dropdown when clicking outside / 点击外部关闭下拉菜单
  useEffect(() => {
    if (!menuOpen) return;
    const handleClickOutside = (e: MouseEvent) => {
      if (menuRef.current && !menuRef.current.contains(e.target as Node)) {
        setMenuOpen(false);
      }
    };
    document.addEventListener('mousedown', handleClickOutside);
    return () => document.removeEventListener('mousedown', handleClickOutside);
  }, [menuOpen]);

  // Clear toast timer on unmount / 卸载时清除 toast 定时器
  useEffect(() => {
    return () => {
      if (toastTimerRef.current) clearTimeout(toastTimerRef.current);
    };
  }, []);

  /** Show a temporary toast message / 显示临时提示消息 */
  const showToast = useCallback((msg: string) => {
    setToastMessage(msg);
    if (toastTimerRef.current) clearTimeout(toastTimerRef.current);
    toastTimerRef.current = setTimeout(() => setToastMessage(null), 2000);
  }, []);

  /** Toggle a category in the selected set / 切换类别选中状态 */
  const toggleCategory = useCallback((cat: EngineStreamCategory) => {
    setSelectedCategories((prev) => {
      const next = new Set(prev);
      if (next.has(cat)) {
        next.delete(cat);
      } else {
        next.add(cat);
      }
      return next;
    });
  }, []);

  /** Get events based on current scope / 根据当前范围获取事件 */
  const getScopedEvents = useCallback((): NormalizedEvent[] => {
    let source: (StreamingEvent | EngineStreamEvent)[];

    switch (scope) {
      case 'filtered':
        if (filteredIndices && filteredIndices.length > 0) {
          source = filteredIndices.map((i) => events[i]!).filter(Boolean);
        } else {
          source = events;
        }
        break;
      case 'category':
        source = events.filter((e) => {
          if (isEngineStreamEvent(e)) {
            return selectedCategories.has(e.category);
          }
          // Legacy StreamingEvent: include if 'info' category is selected
          return selectedCategories.has('info');
        });
        break;
      case 'all':
      default:
        source = events;
        break;
    }

    return source.map(normalizeEvent);
  }, [events, filteredIndices, scope, selectedCategories]);

  /** Copy export content to clipboard / 复制导出内容到剪贴板 */
  const handleCopy = useCallback(async (format: ExportFormat) => {
    const scopedEvents = getScopedEvents();
    if (scopedEvents.length === 0) {
      showToast('No events to export / 无事件可导出');
      return;
    }

    const formatter = getFormatter(format);
    const content = formatter(scopedEvents);

    try {
      await navigator.clipboard.writeText(content);
      showToast(`Copied ${scopedEvents.length} events (${FORMAT_LABELS[format].zh}) / 已复制 ${scopedEvents.length} 条事件`);
    } catch {
      showToast('Copy failed / 复制失败');
    }

    setMenuOpen(false);
  }, [getScopedEvents, showToast]);

  /** Download export content as a file / 下载导出内容为文件 */
  const handleDownload = useCallback((format: ExportFormat) => {
    const scopedEvents = getScopedEvents();
    if (scopedEvents.length === 0) {
      showToast('No events to export / 无事件可导出');
      return;
    }

    const formatter = getFormatter(format);
    const content = formatter(scopedEvents);
    const { ext, mime } = FORMAT_LABELS[format];

    const blob = new Blob([content], { type: `${mime};charset=utf-8` });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `${fileName}-${Date.now()}${ext}`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);

    showToast(`Downloaded ${scopedEvents.length} events / 已下载 ${scopedEvents.length} 条事件`);
    setMenuOpen(false);
  }, [getScopedEvents, fileName, showToast]);

  /** Memoized scoped event count for display / 用于显示的范围事件计数 */
  const scopedCount = useMemo(() => getScopedEvents().length, [getScopedEvents]);

  return (
    <div className="stream-export" ref={menuRef}>
      {/* Export button / 导出按钮 */}
      <button
        className="stream-export-btn"
        onClick={() => setMenuOpen((prev) => !prev)}
        title="Export / 导出"
      >
        <span className="stream-export-btn-icon">{'\u2913'}</span>
        <span className="stream-export-btn-label">EXPORT / 导出</span>
      </button>

      {/* Dropdown menu / 下拉菜单 */}
      {menuOpen && (
        <div className="stream-export-menu">
          {/* Scope selector / 范围选择 */}
          <div className="stream-export-section">
            <div className="stream-export-section-title">
              SCOPE / 范围
            </div>
            <div className="stream-export-scope-options">
              {(Object.keys(SCOPE_LABELS) as ExportScope[]).map((s) => (
                <button
                  key={s}
                  className={`stream-export-scope-btn ${scope === s ? 'active' : ''}`}
                  onClick={() => setScope(s)}
                >
                  {SCOPE_LABELS[s].zh}
                  <span className="stream-export-scope-en">{SCOPE_LABELS[s].en}</span>
                </button>
              ))}
            </div>
          </div>

          {/* Category filter (visible when scope is 'category') / 类别过滤（范围为"按类别"时可见） */}
          {scope === 'category' && (
            <div className="stream-export-section">
              <div className="stream-export-section-title">
                CATEGORIES / 类别
              </div>
              <div className="stream-export-category-grid">
                {ALL_CATEGORIES.map((cat) => {
                  const isSelected = selectedCategories.has(cat);
                  const label = CATEGORY_LABELS[cat] ?? { en: cat, zh: cat };
                  return (
                    <button
                      key={cat}
                      className={`stream-export-category-btn ${isSelected ? 'active' : ''}`}
                      onClick={() => toggleCategory(cat)}
                    >
                      <span className="stream-export-category-check">
                        {isSelected ? '\u2713' : '\u25CB'}
                      </span>
                      <span className="stream-export-category-zh">{label.zh}</span>
                      <span className="stream-export-category-en">{label.en}</span>
                    </button>
                  );
                })}
              </div>
            </div>
          )}

          {/* Event count / 事件计数 */}
          <div className="stream-export-count">
            {scopedCount} / {events.length} events / 条事件
          </div>

          {/* Format & action buttons / 格式与操作按钮 */}
          <div className="stream-export-section">
            <div className="stream-export-section-title">
              FORMAT / 格式
            </div>
            <div className="stream-export-format-grid">
              {(Object.keys(FORMAT_LABELS) as ExportFormat[]).map((format) => (
                <div key={format} className="stream-export-format-cell">
                  <span className="stream-export-format-name">
                    {FORMAT_LABELS[format].zh}
                  </span>
                  <div className="stream-export-format-actions">
                    <button
                      className="stream-export-action-btn copy"
                      onClick={() => handleCopy(format)}
                      title={`Copy as ${FORMAT_LABELS[format].en} / 复制为${FORMAT_LABELS[format].zh}`}
                    >
                      {'\uD83D\uDCCB'} Copy / 复制
                    </button>
                    <button
                      className="stream-export-action-btn download"
                      onClick={() => handleDownload(format)}
                      title={`Download as ${FORMAT_LABELS[format].en} / 下载为${FORMAT_LABELS[format].zh}`}
                    >
                      {'\u2B07'} Download / 下载
                    </button>
                  </div>
                </div>
              ))}
            </div>
          </div>
        </div>
      )}

      {/* Toast notification / 提示通知 */}
      {toastMessage && (
        <div className="stream-export-toast">
          {toastMessage}
        </div>
      )}
    </div>
  );
};

export default StreamExport;
