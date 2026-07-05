import React from 'react';
import { SHORTCUTS } from '../../L5-core/hooks/useKeyboard';

/* ── section helper ── */

const Section: React.FC<{ title: string; children: React.ReactNode }> = ({ title, children }) => (
  <div style={{ marginBottom: 14 }}>
    <h4 style={{
      fontSize: 'var(--font-size-xs, 11px)', color: 'var(--color-text-primary)',
      marginTop: 0, marginBottom: 6, fontWeight: 600,
      textTransform: 'uppercase', letterSpacing: '0.5px',
    }}>
      {title}
    </h4>
    {children}
  </div>
);

const Cmd: React.FC<{ cmd: string; desc: string }> = ({ cmd, desc }) => (
  <div style={{ fontSize: 'var(--font-size-sm, 12px)', lineHeight: 1.6 }}>
    <span style={{ color: 'var(--color-accent, #4fc3f7)', fontFamily: 'var(--font-mono)' }}>{cmd}</span>
    <span style={{ color: 'var(--color-text-tertiary, #555)' }}> {'\u2014'} {desc}</span>
  </div>
);

/* ── HelpPanel ── */

export const HelpPanel: React.FC = () => (
  <div style={{ padding: 12, color: 'var(--color-text-secondary)', fontSize: 'var(--font-size-sm, 12px)', overflowY: 'auto', height: '100%' }}>
    <h3 style={{ color: 'var(--color-module-help, #ba68c8)', marginBottom: 8, fontSize: 'var(--font-size-md, 14px)' }}>
      帮助与快捷键 Help &amp; Shortcuts
    </h3>

    {/* keyboard shortcuts */}
    <Section title="键盘快捷键 Keyboard Shortcuts">
      <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
        {SHORTCUTS.map((s) => (
          <div key={s.label} style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '4px 0' }}>
            <span>{s.label}</span>
            <span className="kbd">{s.keys.join('+')}</span>
          </div>
        ))}
      </div>
    </Section>

    {/* terminal commands - matches actual M5-Terminal commands */}
    <Section title="终端命令 Terminal Commands">
      <div style={{ display: 'flex', flexDirection: 'column', gap: 2 }}>
        <Cmd cmd="help 帮助" desc="显示帮助 Show this help" />
        <Cmd cmd="clear 清屏" desc="清除终端 Clear terminal" />
        <Cmd cmd="list 列表" desc="列出所有对象 List all objects" />
        <Cmd cmd="stats 统计" desc="显示对象统计 Show statistics" />
        <Cmd cmd="demo 演示" desc="加载演示场景 Load demo scene" />
        <Cmd cmd="add point 添加点 &lt;name&gt; at (&lt;x&gt;, &lt;y&gt;)" desc="添加点 Add a point" />
        <Cmd cmd="add segment 添加线段 &lt;name&gt; between &lt;A&gt; and &lt;B&gt;" desc="添加线段 Add a segment" />
        <Cmd cmd="add circle 添加圆 &lt;name&gt; center &lt;A&gt; radius &lt;B&gt;" desc="添加圆 Add a circle" />
        <Cmd cmd="delete 删除 &lt;label&gt;" desc="删除对象 Delete an object" />
        <Cmd cmd="move 移动 &lt;label&gt; to (&lt;x&gt;, &lt;y&gt;)" desc="移动点 Move a point" />
        <Cmd cmd="select 选择 &lt;label&gt;, &lt;label&gt;" desc="选择对象 Select objects" />
        <Cmd cmd="midpoint 中点 &lt;A&gt;, &lt;B&gt;" desc="创建中点 Create midpoint" />
      </div>
    </Section>

    {/* about */}
    <Section title="关于 About">
      <div style={{
        padding: '8px 10px', borderRadius: 6,
        background: 'var(--color-bg-primary)',
        border: '1px solid var(--color-border-secondary)',
        display: 'flex', flexDirection: 'column', gap: 4,
        fontSize: 'var(--font-size-xs, 11px)',
      }}>
        <div><strong style={{ color: 'var(--color-text-primary)' }}>Lv-00 Geometry IDE</strong></div>
        <div style={{ color: 'var(--color-text-tertiary, #555)' }}>
          版本 Version: 0.1.0-alpha
        </div>
        <div style={{ color: 'var(--color-text-tertiary, #555)' }}>
          一款基于约束的交互式几何开发环境 An interactive constraint-based geometry development environment
        </div>
        <div style={{ color: 'var(--color-text-tertiary, #555)' }}>
          技术 Tech: React + TypeScript + Zustand + SVG Canvas
        </div>
      </div>
    </Section>

    {/* tips */}
    <Section title="使用提示 Tips">
      <div style={{ display: 'flex', flexDirection: 'column', gap: 6 }}>
        <Tip num={1} zh="在终端输入 `add point A at (100, 200)` 添加点 Use terminal to add points with coordinates" />
        <Tip num={2} zh="使用 `add segment` 和 `add circle` 命令构建几何构造 Use segment/circle commands to build constructions" />
        <Tip num={3} zh="输入 `demo` 加载预设演示场景，含点、线段、圆 Load a demo scene with points, segments, and circles" />
        <Tip num={4} zh="按 Ctrl+G 切换网格显示，Ctrl+A 切换坐标轴 Press Ctrl+G to toggle grid, Ctrl+A to toggle axes" />
        <Tip num={5} zh="在约束图中点击节点可以选中对应的几何对象 Click nodes in the constraint graph to select objects" />
      </div>
    </Section>
  </div>
);

const Tip: React.FC<{ num: number; zh: string }> = ({ num, zh }) => (
  <div style={{ display: 'flex', gap: 8, fontSize: 'var(--font-size-xs, 11px)', lineHeight: 1.5 }}>
    <span style={{
      width: 16, height: 16, borderRadius: '50%', flexShrink: 0,
      background: 'var(--color-module-help, #ba68c8)',
      color: '#000', fontWeight: 700, fontSize: 10,
      display: 'flex', alignItems: 'center', justifyContent: 'center',
    }}>
      {num}
    </span>
    <span style={{ color: 'var(--color-text-secondary)' }}>{zh}</span>
  </div>
);
