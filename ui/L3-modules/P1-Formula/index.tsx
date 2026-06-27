import React, { useState, useCallback } from 'react';
import { useGeometryStore, GeoObject } from '../../L5-core/store/geometryStore';

/* ── Expression display driven by GeometryStore ── */
const COLORS = ['#4fc3f7', '#81c784', '#ffb74d', '#e57373', '#ba68c8', '#4dd0e1'];

const TYPE_COLOR: Record<string, string> = {
  point: '#4fc3f7',
  segment: '#81c784',
  circle: '#ffd54f',
  line: '#90caf9',
};

const TYPE_LABELS: Record<string, string> = {
  point: '点',
  segment: '段',
  circle: '圆',
  line: '线',
};

/* ── ExpressionList ── */
const ExpressionList: React.FC<{
  objects: GeoObject[];
  onToggle: (id: string) => void;
  onDelete: (id: string) => void;
}> = ({ objects, onToggle, onDelete }) => (
  <div style={{ display: 'flex', flexDirection: 'column', gap: 2 }}>
    {objects.map((obj) => {
      const color = obj.color || TYPE_COLOR[obj.type] || '#4fc3f7';
      const typeTag = TYPE_LABELS[obj.type] || obj.type;
      let detail = '';
      if (obj.type === 'point') detail = `(${obj.x?.toFixed(0) ?? '?'}, ${obj.y?.toFixed(0) ?? '?'})`;
      else if (obj.type === 'segment' || obj.type === 'line') {
        detail = obj.length !== undefined ? `L=${obj.length.toFixed(1)}` : '';
      } else if (obj.type === 'circle') {
        detail = obj.radius !== undefined ? `R=${obj.radius.toFixed(1)}` : '';
      }

      return (
        <div
          key={obj.id}
          style={{
            display: 'flex',
            alignItems: 'center',
            gap: 6,
            padding: '4px 6px',
            borderRadius: 4,
            background: obj.visible ? 'transparent' : 'var(--color-bg-secondary, #1a1a2e)',
            opacity: obj.visible ? 1 : 0.5,
          }}
        >
          {/* visibility toggle */}
          <button
            onClick={() => onToggle(obj.id)}
            title={obj.visible ? '隐藏 Hide' : '显示 Show'}
            style={{
              background: 'none',
              border: 'none',
              cursor: 'pointer',
              color: obj.visible ? color : 'var(--color-text-tertiary, #555)',
              fontSize: 12,
              width: 18,
              height: 18,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
            }}
          >
            {obj.visible ? '\u25CF' : '\u25CB'}
          </button>

          {/* color swatch */}
          <span style={{
            width: 8,
            height: 8,
            borderRadius: '50%',
            background: color,
            flexShrink: 0,
          }} />

          {/* text display */}
          <span style={{
            flex: 1,
            color: 'var(--color-text-primary)',
            fontFamily: 'var(--font-mono)',
            fontSize: 12,
          }}>
            <span style={{ color: 'var(--color-text-muted)', fontSize: 10 }}>[{typeTag}]</span>{' '}
            {obj.label}{' '}
            <span style={{ color: 'var(--color-text-muted)', fontSize: 11 }}>{detail}</span>
          </span>

          {/* delete */}
          <button
            onClick={() => onDelete(obj.id)}
            title="删除 Delete"
            style={{
              background: 'none',
              border: 'none',
              cursor: 'pointer',
              color: 'var(--color-text-tertiary, #555)',
              fontSize: 12,
              padding: 0,
            }}
          >
            \u00D7
          </button>
        </div>
      );
    })}
  </div>
);

/* ── FormulaPanel ── */
export const FormulaPanel: React.FC = () => {
  const objects = useGeometryStore((s) => s.objects);
  const updateObject = useGeometryStore((s) => s.updateObject);
  const removeObject = useGeometryStore((s) => s.removeObject);
  const addObject = useGeometryStore((s) => s.addObject);

  const [input, setInput] = useState('');
  const [error, setError] = useState<string | null>(null);
  const [lastResult, setLastResult] = useState<string | null>(null);

  // Find point by label
  const findPointByLabel = (label: string) => {
    return objects.find((o) => o.type === 'point' && o.label.toLowerCase() === label.toLowerCase());
  };

  // Evaluate DSL commands
  const evaluate = useCallback(() => {
    if (!input.trim()) return;
    setError(null);
    setLastResult(null);

    const lines = input.split('\n').map((l) => l.trim()).filter(Boolean);
    const results: string[] = [];

    for (const line of lines) {
      // Parse: point <label> at (<x>, <y>)
      const ptMatch = line.match(/^point\s+(\w+)\s+at\s*\(\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*\)$/i);
      if (ptMatch) {
        const label = ptMatch[1];
        const x = parseFloat(ptMatch[2]);
        const y = parseFloat(ptMatch[3]);
        addObject({ type: 'point', label, x, y, visible: true });
        results.push(`point ${label} = (${x}, ${y})`);
        continue;
      }

      // Parse: segment <label> between <A> and <B>
      const segMatch = line.match(/^segment\s+(\w+)\s+between\s+(\w+)\s+and\s+(\w+)$/i);
      if (segMatch) {
        const label = segMatch[1];
        const ptA = findPointByLabel(segMatch[2]);
        const ptB = findPointByLabel(segMatch[3]);
        if (!ptA || !ptB) {
          setError(`点未找到 Point not found: ${!ptA ? segMatch[2] : segMatch[3]}`);
          return;
        }
        addObject({ type: 'segment', label, startId: ptA.id, endId: ptB.id, visible: true });
        results.push(`segment ${label} = ${ptA.label}${ptB.label}`);
        continue;
      }

      // Parse: circle <label> center <A> through <B>
      const cirMatch = line.match(/^circle\s+(\w+)\s+center\s+(\w+)\s+through\s+(\w+)$/i);
      if (cirMatch) {
        const label = cirMatch[1];
        const center = findPointByLabel(cirMatch[2]);
        const rp = findPointByLabel(cirMatch[3]);
        if (!center || !rp) {
          setError(`点未找到 Point not found: ${!center ? cirMatch[2] : cirMatch[3]}`);
          return;
        }
        const r = useGeometryStore.getState().getDistance(center.id, rp.id);
        addObject({ type: 'circle', label, centerId: center.id, radiusPointId: rp.id, radius: r, visible: true });
        results.push(`circle ${label} center=${center.label} r=${r.toFixed(1)}`);
        continue;
      }

      // Parse: midpoint <A>, <B>
      const midMatch = line.match(/^midpoint\s+(\w+)\s*,\s*(\w+)$/i);
      if (midMatch) {
        const ptA = findPointByLabel(midMatch[1]);
        const ptB = findPointByLabel(midMatch[2]);
        if (!ptA || !ptB) {
          setError(`点未找到 Point not found`);
          return;
        }
        const mx = ((ptA.x ?? 0) + (ptB.x ?? 0)) / 2;
        const my = ((ptA.y ?? 0) + (ptB.y ?? 0)) / 2;
        addObject({ type: 'point', label: `M(${ptA.label}${ptB.label})`, x: mx, y: my, visible: true });
        results.push(`midpoint(${ptA.label},${ptB.label}) = (${mx.toFixed(0)}, ${my.toFixed(0)})`);
        continue;
      }

      setError(`语法错误 Syntax error: "${line}"`);
      return;
    }

    setLastResult(results.join('\n'));
    setInput('');
  }, [input, objects, addObject, findPointByLabel]);

  const toggleVisibility = (id: string) => {
    const obj = objects.find((o) => o.id === id);
    if (obj) updateObject(id, { visible: !obj.visible });
  };

  const deleteObj = (id: string) => {
    removeObject(id);
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', gap: 8 }}>
      {/* header row */}
      <div style={{ display: 'flex', gap: 8, justifyContent: 'space-between', alignItems: 'center' }}>
        <span style={{ fontSize: 'var(--font-size-sm, 12px)', color: 'var(--color-text-secondary)' }}>
          表达式列表 Expression List ({objects.length})
        </span>
      </div>

      {/* expression list from GeometryStore */}
      <div
        style={{
          flex: 1,
          overflowY: 'auto',
          padding: '4px 0',
          borderBottom: '1px solid var(--color-border-secondary)',
        }}
      >
        {objects.length > 0 ? (
          <ExpressionList
            objects={objects}
            onToggle={toggleVisibility}
            onDelete={deleteObj}
          />
        ) : (
          <div style={{ color: 'var(--color-text-muted)', fontSize: 12, padding: '8px 0' }}>
            无对象 No objects. Use commands below.
          </div>
        )}
      </div>

      {/* Result display */}
      {lastResult && (
        <div style={{
          padding: '4px 8px',
          background: 'rgba(0,200,83,0.08)',
          borderRadius: 4,
          fontSize: 11,
          color: 'var(--color-success)',
          fontFamily: 'var(--font-mono)',
          whiteSpace: 'pre-wrap',
          maxHeight: 60,
          overflow: 'auto',
        }}>
          {lastResult}
        </div>
      )}

      {/* Error display */}
      {error && (
        <div style={{
          padding: '4px 8px',
          background: 'rgba(248,81,73,0.08)',
          borderRadius: 4,
          fontSize: 11,
          color: 'var(--color-danger)',
          fontFamily: 'var(--font-mono)',
        }}>
          {error}
        </div>
      )}

      {/* command input */}
      <div style={{ display: 'flex', flexDirection: 'column', gap: 6 }}>
        <div style={{ display: 'flex', gap: 8, justifyContent: 'space-between', alignItems: 'center' }}>
          <span style={{ fontSize: 'var(--font-size-xs, 11px)', color: 'var(--color-text-secondary)' }}>
            命令输入 Command Input
          </span>
          <span style={{ fontSize: 'var(--font-size-xs, 11px)', color: 'var(--color-text-muted)' }}>
            Lv-00 DSL
          </span>
        </div>
        <textarea
          className="formula-editor"
          value={input}
          onChange={(e) => { setInput(e.target.value); setError(null); }}
          placeholder={"point A at (100, 200);\npoint B at (400, 150);\nsegment AB between A and B;\ncircle C1 center A through B;\nmidpoint A, B;"}
          spellCheck={false}
          rows={4}
        />
        <button className="btn btn-primary" onClick={evaluate}>
          求值 Evaluate
        </button>
      </div>
    </div>
  );
};
