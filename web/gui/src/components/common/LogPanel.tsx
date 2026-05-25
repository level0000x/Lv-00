/**
 * @module components/common/LogPanel
 * @description 通用日志面板组件 / Reusable log panel component
 *
 * 用于在面板中显示可滚动的日志列表，替代各面板中重复的日志区域代码。
 * 支持两种日志格式：纯字符串和带时间戳的结构化日志。
 *
 * Used to display a scrollable log list in panels,
 * replacing repetitive log area code across panels.
 * Supports two log formats: plain strings and structured entries with timestamps.
 */

import React, { useRef, useEffect } from 'react';

/**
 * 结构化日志条目（面板专用，与 types/index.ts 中的 LogEntry 不同）
 * Structured log entry (panel-specific, different from LogEntry in types/index.ts)
 *
 * @property time - 时间戳文本 / Timestamp text
 * @property msg - 日志消息 / Log message
 */
export interface LogPanelEntry {
  /** 时间戳 / Timestamp */
  time: string;
  /** 日志消息 / Log message */
  msg: string;
}

/**
 * LogPanel 组件的 props 接口 / LogPanel component props interface
 * @property entries - 日志内容（字符串数组或结构化条目数组）/ Log content (string array or structured entry array)
 * @property emptyText - 空状态提示文本 / Empty state placeholder text
 * @property maxHeight - 日志区域的最大高度 / Max height of the log area
 * @property className - 额外的 CSS 类名 / Additional CSS class name
 * @property onClear - 清空日志的回调 / Callback to clear logs
 * @property clearText - 清空按钮文本 / Clear button text
 */
interface LogPanelProps {
  /** 日志内容（字符串数组或结构化条目数组）/ Log content */
  entries: string[] | LogPanelEntry[];
  /** 空状态提示文本 / Empty state placeholder text */
  emptyText?: string;
  /** 日志区域的最大高度 / Max height of the log area */
  maxHeight?: string;
  /** 额外的 CSS 类名 / Additional CSS class name */
  className?: string;
  /** 清空日志的回调 / Callback to clear logs */
  onClear?: () => void;
  /** 清空按钮文本 / Clear button text */
  clearText?: string;
}

/**
 * 判断日志条目是否为结构化格式
 * 通过检查第一个元素是否为包含 time 字段的对象来判断。
 *
 * Type guard: check if log entries are in structured format.
 * Determines by checking if the first element is an object with a 'time' field.
 *
 * @param entries - 日志条目数组 / Log entries array
 * @returns 是否为结构化格式 / Whether entries are structured
 */
function isStructuredLog(entries: string[] | LogPanelEntry[]): entries is LogPanelEntry[] {
  if (entries.length === 0) return false;
  const first = entries[0];
  return typeof first === 'object' && first !== null && 'time' in first;
}

/**
 * LogPanel - 通用日志面板 / Reusable log panel
 *
 * 功能特性 / Features:
 * - 可滚动的日志列表 / Scrollable log list
 * - 新日志自动滚动到底部 / Auto-scroll to bottom on new logs
 * - 支持纯字符串和结构化（带时间戳）两种日志格式
 *   Supports both plain string and structured (with timestamp) log formats
 * - 空状态提示 / Empty state placeholder
 * - 可选的清空按钮 / Optional clear button
 * - 完整的 ARIA 属性支持 / Full ARIA attribute support
 *
 * @example
 * ```tsx
 * // 纯字符串日志 / Plain string logs
 * <LogPanel entries={['操作成功', '已保存']} />
 *
 * // 结构化日志 / Structured logs
 * <LogPanel
 *   entries={[
 *     { time: '12:00:01', msg: '操作成功' },
 *     { time: '12:00:02', msg: '已保存' },
 *   ]}
 *   onClear={() => setLogs([])}
 * />
 * ```
 */
const LogPanel: React.FC<LogPanelProps> = ({
  entries,
  emptyText = '暂无日志 / No logs',
  maxHeight = '200px',
  className = '',
  onClear,
  clearText = 'CLEAR / 清空',
}) => {
  /** 用于自动滚动到底部的锚点引用 / Anchor reference for auto-scrolling to bottom */
  const endRef = useRef<HTMLDivElement>(null);

  /**
   * 自动滚动到底部
   * 当日志条目数量变化时，将滚动容器滚动到最底部。
   * 使用 'instant' 行为避免平滑滚动造成的延迟感。
   *
   * Auto-scroll to bottom.
   * When the number of log entries changes, scrolls the container to the bottom.
   * Uses 'instant' behavior to avoid delay from smooth scrolling.
   */
  useEffect(() => {
    endRef.current?.scrollIntoView({ behavior: 'instant' as ScrollBehavior });
  }, [entries.length]);

  /** 当前日志是否为结构化格式 / Whether current logs are in structured format */
  const structured = isStructuredLog(entries);

  return (
    <div className={`log-panel-container ${className}`} role="log" aria-label="日志面板 / Log panel">
      {/* 清空按钮（仅在提供 onClear 回调时显示） */}
      {/* Clear button (only shown when onClear callback is provided) */}
      {onClear && (
        <button
          className="btn btn-sm"
          onClick={onClear}
          aria-label={clearText}
        >
          {clearText}
        </button>
      )}

      {/* 可滚动的日志区域 / Scrollable log area */}
      <div className="log-area" style={{ maxHeight, overflowY: 'auto' }}>
        {entries.length === 0 ? (
          /* 空状态提示 / Empty state placeholder */
          <div className="log-empty-msg">{emptyText}</div>
        ) : structured ? (
          /* 结构化日志（带时间戳）/ Structured logs (with timestamps) */
          entries.map((entry, idx) => (
            <div className="log-entry" key={idx}>
              <span className="log-time">[{entry.time}]</span>
              <span className="log-msg">{entry.msg}</span>
            </div>
          ))
        ) : (
          /* 纯字符串日志 / Plain string logs */
          entries.map((msg, idx) => (
            <div className="log-entry" key={idx}>
              <span className="log-msg">{msg}</span>
            </div>
          ))
        )}
        {/* 自动滚动锚点 / Auto-scroll anchor */}
        <div ref={endRef} />
      </div>
    </div>
  );
};

export default LogPanel;
