/**
 * @module components/panels/DebugPanel
 * @description 调试模块侧边栏面板 / Debug module sidebar panel.
 *              提供日志级别选择、调试计数器、报告和可滚动日志查看器。
 *              Provides log level selection, debug counters, reports,
 *              and a scrollable log viewer.
 */

import React, { useCallback, useRef, useEffect, useMemo } from 'react';
import Panel from './Panel';
import { useAppStore } from '@/stores';
import type { AppLogLevel } from '@/types'; // [安全修复 M-08] 重命名 LogLevel -> AppLogLevel

/**
 * DebugPanel - 调试模块侧边栏面板 / Debug module sidebar panel
 *
 * 区段 / Sections:
 * - DEBUG: 日志级别选择器、重置计数器、调试报告、计数器
 *           Log level selector, reset counters, debug report, counters
 * - LOG: 带颜色编码条目的可滚动日志查看器
 *        Scrollable log viewer with color-coded entries
 */
const DebugPanel: React.FC = () => {
  const minLogLevel = useAppStore((s) => s.minLogLevel);
  const setMinLogLevel = useAppStore((s) => s.setMinLogLevel);
  const logs = useAppStore((s) => s.logs);
  const clearLogs = useAppStore((s) => s.clearLogs);
  const addToast = useAppStore((s) => s.addToast);
  const logContentRef = useRef<HTMLDivElement>(null);

  // 新日志条目时自动滚动到底部 / Auto-scroll to bottom on new log entries
  useEffect(() => {
    if (logContentRef.current) {
      logContentRef.current.scrollTop = logContentRef.current.scrollHeight;
    }
  }, [logs]);

  const handleResetCounters = useCallback(() => {
    addToast('info', '计数器已重置 / Counters reset');
  }, [addToast]);

  /** 过滤后的日志列表（缓存） / Filtered log entries (memoized) */
  const filteredLogs = useMemo(
    () => logs.filter((log) => {
      const levels: AppLogLevel[] = ['debug', 'info', 'warn', 'error'];
      return levels.indexOf(log.level) >= levels.indexOf(minLogLevel);
    }),
    [logs, minLogLevel],
  );

  const handleDebugReport = useCallback(() => {
    addToast('info', `调试报告: ${logs.length} 条日志 / Debug report: ${logs.length} entries`);
  }, [logs.length, addToast]);

  const handleLogLevelChange = useCallback((e: React.ChangeEvent<HTMLSelectElement>) => {
    setMinLogLevel(e.target.value as AppLogLevel);
  }, [setMinLogLevel]);

  const handleShowCounters = useCallback(() => {
    addToast('info', `计数器: ${logs.length} entries`);
  }, [logs.length, addToast]);

  return (
    <>
      <Panel title="DEBUG / 调试" panelId="debug-ops">
        <div className="input-row">
          <label>LOG</label>
          <select
            className="select-field"
            value={minLogLevel}
            onChange={handleLogLevelChange}
          >
            <option value="debug">DEBUG</option>
            <option value="info">INFO</option>
            <option value="warn">WARN</option>
            <option value="error">ERROR</option>
          </select>
        </div>
        <button className="btn" onClick={handleResetCounters}>
          RESET COUNTERS / 重置计数器
        </button>
        <button className="btn" onClick={handleDebugReport}>
          DEBUG REPORT / 调试报告
        </button>
        <button className="btn" onClick={handleShowCounters}>
          COUNTERS / 计数器
        </button>
      </Panel>

      <Panel title="LOG / 日志" panelId="debug-log">
        <div className="log-panel">
          <div className="log-content" id="logContent" ref={logContentRef} role="log" aria-live="polite">
            {filteredLogs.length === 0 ? (
              <div className="log-empty-msg">暂无日志消息 / No log messages yet</div>
            ) : (
              filteredLogs.map((log, i) => (
                  <div key={i} className={`log-entry log-${log.level}`}>
                    <span className="log-time">
                      {new Date(log.timestamp).toLocaleTimeString()}
                    </span>
                    <span className="log-msg">{log.message}</span>
                  </div>
                ))
            )}
          </div>
          <div className="log-actions">
            <button className="log-action-btn" onClick={clearLogs}>
              CLEAR / 清空
            </button>
          </div>
        </div>
      </Panel>
    </>
  );
};

export default DebugPanel;
