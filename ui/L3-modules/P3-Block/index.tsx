import React, { useState } from 'react';

/* ── block data model ── */

interface BlockDef {
  id: string;
  name: string;
  nameZh: string;
  icon: string;
  inputs: string[];
  outputs: string[];
  description: string;
}

const BLOCK_TEMPLATES: BlockDef[] = [
  {
    id: 'construct', name: 'Construct', nameZh: '构造', icon: '\u{1F527}',
    inputs: ['points', 'constraints'],
    outputs: ['graph'],
    description: '创建几何构造 Create geometric constructions',
  },
  {
    id: 'solve', name: 'Solve', nameZh: '求解', icon: '\u{1F4A1}',
    inputs: ['equations', 'bounds'],
    outputs: ['solution'],
    description: '求解约束方程 Solve constraint equations',
  },
  {
    id: 'verify', name: 'Verify', nameZh: '验证', icon: '\u2705',
    inputs: ['theorem', 'proof'],
    outputs: ['result'],
    description: '验证定理正确性 Verify theorem correctness',
  },
  {
    id: 'export', name: 'Export', nameZh: '导出', icon: '\u{1F4E4}',
    inputs: ['graph', 'format'],
    outputs: ['file'],
    description: '导出构造结果 Export construction results',
  },
  {
    id: 'optimize', name: 'Optimize', nameZh: '优化', icon: '\u26A1',
    inputs: ['graph', 'criteria'],
    outputs: ['optimized'],
    description: '优化构造流程 Optimize construction flow',
  },
];

const PortDot: React.FC<{ color: string; label: string }> = ({ color, label }) => (
  <span style={{ display: 'inline-flex', alignItems: 'center', gap: 3, fontSize: 'var(--font-size-xs, 11px)' }}>
    <span style={{ width: 7, height: 7, borderRadius: '50%', background: color, flexShrink: 0 }} />
    <span style={{ color: 'var(--color-text-secondary)' }}>{label}</span>
  </span>
);

/* ── BlockPanel ── */

export const BlockPanel: React.FC = () => {
  const [blocks, setBlocks] = useState<BlockDef[]>(BLOCK_TEMPLATES);

  const addBlock = () => {
    const template = BLOCK_TEMPLATES[blocks.length % BLOCK_TEMPLATES.length];
    const newBlock: BlockDef = {
      ...template,
      id: `${template.id}-${Date.now()}`,
    };
    setBlocks((prev) => [...prev, newBlock]);
  };

  const removeBlock = (id: string) =>
    setBlocks((prev) => prev.filter((b) => b.id !== id));

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', gap: 8 }}>
      {/* header */}
      <div style={{ display: 'flex', gap: 8, justifyContent: 'space-between', alignItems: 'center' }}>
        <span style={{ fontSize: 'var(--font-size-sm, 12px)', color: 'var(--color-text-secondary)' }}>
          函数块 Function Blocks
        </span>
        <button className="btn btn-small" onClick={addBlock}>
          + 添加 Add Block
        </button>
      </div>

      {/* block list */}
      <div style={{ flex: 1, overflowY: 'auto', display: 'flex', flexDirection: 'column', gap: 6 }}>
        {blocks.map((block) => (
          <div
            key={block.id}
            style={{
              padding: '8px 10px',
              borderRadius: 6,
              border: '1px solid var(--color-border-secondary)',
              background: 'var(--color-bg-primary)',
            }}
          >
            {/* block header */}
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: 6 }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                <span>{block.icon}</span>
                <span style={{ fontWeight: 600, fontSize: 'var(--font-size-sm, 12px)', color: 'var(--color-text-primary)' }}>
                  {block.nameZh} {block.name}
                </span>
              </div>
              <button
                onClick={() => removeBlock(block.id)}
                title="移除 Remove"
                style={{
                  background: 'none', border: 'none', cursor: 'pointer',
                  color: 'var(--color-text-tertiary, #555)', fontSize: 'var(--font-size-sm, 12px)', padding: 0,
                }}
              >
                \u00D7
              </button>
            </div>

            {/* description */}
            <div style={{ fontSize: 'var(--font-size-xs, 11px)', color: 'var(--color-text-tertiary, #555)', marginBottom: 6 }}>
              {block.description}
            </div>

            {/* ports */}
            <div style={{ display: 'flex', gap: 12 }}>
              <div style={{ display: 'flex', flexDirection: 'column', gap: 2 }}>
                <span style={{ fontSize: 'var(--font-size-xs, 10px)', color: 'var(--color-text-tertiary, #555)', textTransform: 'uppercase' }}>
                  输入 Input
                </span>
                {block.inputs.map((p) => <PortDot key={p} color="#81c784" label={p} />)}
              </div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: 2 }}>
                <span style={{ fontSize: 'var(--font-size-xs, 10px)', color: 'var(--color-text-tertiary, #555)', textTransform: 'uppercase' }}>
                  输出 Output
                </span>
                {block.outputs.map((p) => <PortDot key={p} color="#ffb74d" label={p} />)}
              </div>
            </div>
          </div>
        ))}
      </div>
    </div>
  );
};
