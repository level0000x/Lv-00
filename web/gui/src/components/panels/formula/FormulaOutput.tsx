/**
 * @module components/panels/formula/FormulaOutput
 * @description 公式输出区域子组件。
 *              展示解析/渲染/求解的结果文本，以及操作日志。
 *
 * 功能特性：
 * - 解析结果展示
 * - 渲染结果展示（含摘要卡片）
 * - 求解结果展示
 * - 操作日志列表
 */

import React from 'react';
import Panel from '../Panel';

/**
 * FormulaOutput 组件属性
 */
interface FormulaOutputProps {
  /** 输出文本内容 */
  outputText: string;
  /** 日志条目列表 */
  logEntries: Array<{ time: string; msg: string; id: string }>;
}

/**
 * FormulaOutput - 公式输出区域子组件
 *
 * 包含 OUTPUT 面板（结果展示）和 LOG 面板（操作日志）。
 */
const FormulaOutput: React.FC<FormulaOutputProps> = ({
  outputText,
  logEntries,
}) => {
  return (
    <>
      {/* OUTPUT 区域：解析/渲染结果展示 */}
      <Panel title="OUTPUT / 输出" panelId="formula-output">
        <div className="formula-output-area" id="formulaOutput">
          {outputText ? (
            <div className="formula-code-block">
              <pre style={{ margin: 0, whiteSpace: 'pre-wrap', fontSize: '11px' }}>
                {outputText}
              </pre>
            </div>
          ) : (
            <div className="formula-output-placeholder">
              等待输入公式... / Waiting for formula input...
            </div>
          )}
        </div>
      </Panel>

      {/* LOG 区域：公式操作日志 */}
      <Panel title="LOG / 日志" panelId="formula-log">
        <div className="formula-log-area" id="formulaLog">
          {logEntries.length === 0 ? (
            <div className="log-empty-msg">暂无日志 / No logs</div>
          ) : (
            logEntries.map((entry) => (
              <div key={entry.id} className="formula-log-entry">
                <span className="formula-log-time">{entry.time}</span>
                <span className="formula-log-msg">{entry.msg}</span>
              </div>
            ))
          )}
        </div>
      </Panel>
    </>
  );
};

export default FormulaOutput;
