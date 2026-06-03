/**
 * @module components/panels/formula/FormulaSyntaxGuide
 * @description DSL 语法速查子组件。
 *              提供可折叠的 DSL 语法参考指南。
 *
 * 功能特性：
 * - 可折叠的语法参考面板
 * - 涵盖所有 DSL 命令语法
 * - 中英双语说明
 */

import React from 'react';
import Panel from '../Panel';

/**
 * FormulaSyntaxGuide - DSL 语法速查子组件
 *
 * 展示可折叠的 DSL 语法参考指南。
 */
const FormulaSyntaxGuide: React.FC = () => {
  return (
    <Panel title="SYNTAX GUIDE / 语法参考" panelId="formula-syntax-guide">
      <details style={{ fontSize: '11px' }}>
        <summary style={{ cursor: 'pointer', fontWeight: 600, color: '#4a90d9', userSelect: 'none' }}>
          {'\u25B6 \u70B9\u51FB\u5C55\u5F00 DSL \u8BED\u6CD5\u53C2\u8003 / Click to expand DSL syntax reference'}
        </summary>
        <div style={{
          marginTop: 6,
          padding: '6px 8px',
          backgroundColor: '#f5f5f5',
          borderRadius: 4,
          border: '1px solid #e0e0e0',
          fontFamily: 'Consolas, Monaco, "Courier New", monospace',
          lineHeight: 1.8,
          maxHeight: 260,
          overflowY: 'auto',
        }}>
          <div style={{ marginBottom: 2, color: '#333' }}>
            <code style={{ color: '#4a90d9' }}>point NAME(x, y)</code>
            <span style={{ color: '#999' }}> - {'\u521B\u5EFA\u4E00\u4E2A\u70B9 / create a point'}</span>
          </div>
          <div style={{ marginBottom: 2, color: '#333' }}>
            <code style={{ color: '#4a90d9' }}>segment NAME</code>{' '}
            <span style={{ color: '#999' }}>{'\u6216 / or'}</span>{' '}
            <code style={{ color: '#4a90d9' }}>segment(A, B)</code>
            <span style={{ color: '#999' }}> - {'\u521B\u5EFA\u4E00\u6761\u7EBF\u6BB5 / create a segment'}</span>
          </div>
          <div style={{ marginBottom: 2, color: '#333' }}>
            <code style={{ color: '#4a90d9' }}>circle center(A) radius(r)</code>
            <span style={{ color: '#999' }}> - {'\u521B\u5EFA\u4E00\u4E2A\u5706 / create a circle'}</span>
          </div>
          <div style={{ marginBottom: 2, color: '#333' }}>
            <code style={{ color: '#4a90d9' }}>midpoint M of A, B</code>
            <span style={{ color: '#999' }}> - {'\u521B\u5EFA\u4E2D\u70B9 / create midpoint'}</span>
          </div>
          <div style={{ marginBottom: 2, color: '#333' }}>
            <code style={{ color: '#4a90d9' }}>perpendicular from A to segment BC</code>
            <span style={{ color: '#999' }}> - {'\u521B\u5EFA\u5782\u7EBF / perpendicular line'}</span>
          </div>
          <div style={{ marginBottom: 2, color: '#333' }}>
            <code style={{ color: '#4a90d9' }}>parallel to AB through C</code>
            <span style={{ color: '#999' }}> - {'\u521B\u5EFA\u5E73\u884C\u7EBF / parallel line'}</span>
          </div>
          <div style={{ marginBottom: 2, color: '#333' }}>
            <code style={{ color: '#4a90d9' }}>intersect segment AB with CD</code>
            <span style={{ color: '#999' }}> - {'\u6C42\u4EA4\u70B9 / find intersection'}</span>
          </div>
          <div style={{ marginBottom: 2, color: '#333' }}>
            <code style={{ color: '#4a90d9' }}>measure distance A, B</code>
            <span style={{ color: '#999' }}> - {'\u6D4B\u91CF\u8DDD\u79BB / measure distance'}</span>
          </div>
          <div style={{ marginBottom: 2, color: '#333' }}>
            <code style={{ color: '#4a90d9' }}>measure angle A, B, C</code>
            <span style={{ color: '#999' }}> - {'\u6D4B\u91CF\u89D2\u5EA6 / measure angle'}</span>
          </div>
        </div>
      </details>
    </Panel>
  );
};

export default FormulaSyntaxGuide;
