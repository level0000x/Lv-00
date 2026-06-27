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

    {/* terminal commands */}
    <Section title="终端命令 Terminal Commands">
      <div style={{ display: 'flex', flexDirection: 'column', gap: 2 }}>
        <Cmd cmd="help 帮助" desc="显示帮助 Show this help" />
        <Cmd cmd="clear 清屏" desc="清除终端 Clear terminal" />
        <Cmd cmd="normalize 归一化" desc="归一化图 Normalize graph" />
        <Cmd cmd="add point 添加点 &lt;name&gt; at (&lt;x&gt;,&lt;y&gt;)" desc="添加点 Add a point" />
        <Cmd cmd="solve 求解" desc="求解约束 Solve constraints" />
        <Cmd cmd="prove 证明" desc="验证定理 Verify theorem" />
        <Cmd cmd="stats 统计" desc="显示统计 Show statistics" />
        <Cmd cmd="inspect 检查 &lt;object&gt;" desc="检查对象 Inspect an object" />
        <Cmd cmd="export 导出 &lt;format&gt;" desc="导出 Export to format" />
        <Cmd cmd="undo 撤销" desc="撤销操作 Undo last action" />
        <Cmd cmd="redo 重做" desc="重做操作 Redo last action" />
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
        <Tip num={1} zh="在画布上拖拽节点可以实时更新约束图 Drag nodes on the canvas to update the constraint graph in real-time" />
        <Tip num={2} zh="使用公式面板输入 DSL 命令来定义几何对象 Use the formula panel to input DSL commands for defining geometric objects" />
        <Tip num={3} zh="证明面板支持 Lean 4 风格的策略式证明 The proof panel supports Lean 4-style tactic proofs" />
        <Tip num={4} zh="按 Ctrl+G 切换网格显示，Ctrl+A 切换坐标轴 Press Ctrl+G to toggle grid, Ctrl+A to toggle axes" />
        <Tip num={5} zh="调试面板可以检查任意几何对象的属性 The debug panel can inspect properties of any geometric object" />
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
