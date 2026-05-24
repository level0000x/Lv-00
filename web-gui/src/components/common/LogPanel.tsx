/**
 * @module components/common/LogPanel
 * @description 通用日志面板组件 / Reusable log panel component
 *
 * 用于在面板中显示可滚动的日志列表，替代各面板中重复的日志区域代码。
 * 支持两种日志格式：纯字符串和带时间戳的结构化日志。
 */

import React, { useRef, useEffect } from 'react';

/** 结构化日志条目（面板专用，与 types/index.ts 中的 LogEntry 不同） */
export interface LogPanelEntry {
  /** 时间戳 / Timestamp */
  time: string;
  /** 日志消息 / Log message */
  msg: string;
}

interface LogPanelProps {
  /** 日志内容（字符串数组或结构化条目数组） / Log content */
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
 * 通用日志面板
 *
 * @description 渲染一个可滚动的日志列表，自动滚动到底部。
 *              支持纯字符串和结构化（带时间戳）两种日志格式。
 *
 * @param props - 组件属性
 * @returns 日志面板 JSX
 */
const LogPanel: React.FC<LogPanelProps> = ({
  entries,
  emptyText = '暂无日志 / No logs',
  maxHeight = '200px',
  className = '',
  onClear,
  clearText = 'CLEAR / 清空',
}) => {
  const endRef = useRef<HTMLDivElement>(null);

  // 自动滚动到底部 / Auto-scroll to bottom
  useEffect(() => {
    endRef.current?.scrollIntoView({ behavior: 'instant' as ScrollBehavior });
  }, [entries.length]);

  /** 判断是否为结构化日志 / Check if entries are structured */
  const isStructured = (e: string[] | LogPanelEntry[]): e is LogPanelEntry[] =>
    e.length > 0 && typeof e[0] === 'object' && e[0] !== null && 'time' in e[0];

  return (
    <div className={`log-panel-container ${className}`}>
      {onClear && (
        <button className="btn btn-sm" onClick={onClear} aria-label={clearText}>
          {clearText}
        </button>
      )}
      <div className="log-area" style={{ maxHeight, overflowY: 'auto' }}>
        {entries.length === 0 ? (
          <div className="log-empty-msg">{emptyText}</div>
        ) : isStructured(entries) ? (
          entries.map((entry, idx) => (
            <div className="log-entry" key={idx}>
              <span className="log-time">[{entry.time}]</span>
              <span className="log-msg">{entry.msg}</span>
            </div>
          ))
        ) : (
          entries.map((msg, idx) => (
            <div className="log-entry" key={idx}>
              <span className="log-msg">{msg}</span>
            </div>
          ))
        )}
        <div ref={endRef} />
      </div>
    </div>
  );
};

export default LogPanel;
