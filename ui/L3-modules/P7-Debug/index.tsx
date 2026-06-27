import React, { useState } from 'react';

/* ── data ── */

interface ObjDef {
  id: string;
  label: string;
  type: string;
  props: { key: string; value: string }[];
}

const OBJECTS: ObjDef[] = [
  {
    id: 'ptA', label: 'Point A', type: 'Point',
    props: [
      { key: '坐标 Coordinates', value: '(100, 200)' },
      { key: '颜色 Color', value: '#4fc3f7' },
      { key: '约束 Constraints', value: 'free' },
    ],
  },
  {
    id: 'ptB', label: 'Point B', type: 'Point',
    props: [
      { key: '坐标 Coordinates', value: '(400, 150)' },
      { key: '颜色 Color', value: '#81c784' },
      { key: '约束 Constraints', value: 'free' },
    ],
  },
  {
    id: 'segAB', label: 'Segment AB', type: 'Segment',
    props: [
      { key: '起点 Start', value: 'A (100, 200)' },
      { key: '终点 End', value: 'B (400, 150)' },
      { key: '长度 Length', value: '316.23' },
      { key: '样式 Style', value: 'solid' },
    ],
  },
  {
    id: 'circ', label: 'Circle(A, |AB|)', type: 'Circle',
    props: [
      { key: '圆心 Center', value: 'A (100, 200)' },
      { key: '半径 Radius', value: '316.23 (|AB|)' },
      { key: '约束 Constraint', value: 'r = |AB|' },
    ],
  },
];

interface LogEntry {
  id: number;
  level: 'info' | 'warn' | 'error';
  text: string;
}

/* ── DebugPanel ── */

export const DebugPanel: React.FC = () => {
  const [selectedObjId, setSelectedObjId] = useState(OBJECTS[0].id);
  const [measurements, setMeasurements] = useState<Record<string, string>>({});
  const [logs, setLogs] = useState<LogEntry[]>([
    { id: 1, level: 'info', text: '[init] Lv-00 geometry engine loaded 加载完成' },
    { id: 2, level: 'info', text: '[graph] 4 objects registered 已注册 4 个对象' },
    { id: 3, level: 'warn', text: '[constraint] circle radius is symbolic \u2014 will recompute on drag 圆的半径为符号量' },
  ]);

  const selectedObj = OBJECTS.find((o) => o.id === selectedObjId)!;

  const measure = (type: string) => {
    let val = '';
    switch (type) {
      case 'Distance': val = '316.23 px'; break;
      case 'Angle': val = '67.38\u00B0'; break;
      case 'Area': val = '49607.8 px\u00B2'; break;
      case 'Perimeter': val = '632.46 px'; break;
    }
    setMeasurements((prev) => ({ ...prev, [type]: val }));
    const newLog: LogEntry = {
      id: logs.length + 1,
      level: 'info',
      text: `[measure] ${type}: ${val}`,
    };
    setLogs((prev) => [...prev, newLog]);
  };

  const MEASURE_BUTTONS = ['Distance', 'Angle', 'Area', 'Perimeter'];
  const MEASURE_LABELS: Record<string, string> = {
    Distance: '距离 Dist', Angle: '角度 Angle', Area: '面积 Area', Perimeter: '周长 Perim',
  };

  const section = (title: string, children: React.ReactNode) => (
    <div style={{ marginBottom: 10 }}>
      <div style={{
        fontSize: 'var(--font-size-xs, 11px)', fontWeight: 600,
        color: 'var(--color-module-debug, #e57373)', marginBottom: 4,
        textTransform: 'uppercase', letterSpacing: '0.5px',
      }}>
        {title}
      </div>
      {children}
    </div>
  );

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', gap: 8, overflowY: 'auto' }}>
      {/* Object Inspector */}
      {section('\u5BF9\u8C61\u68C0\u67E5\u5668 Object Inspector', (
        <div style={{
          padding: '6px 8px', borderRadius: 4,
          background: 'var(--color-bg-primary)',
          border: '1px solid var(--color-border-secondary)',
        }}>
          {/* dropdown */}
          <select
            value={selectedObjId}
            onChange={(e) => setSelectedObjId(e.target.value)}
            style={{
              width: '100%', padding: '4px 6px',
              background: 'var(--color-bg-secondary)',
              color: 'var(--color-text-primary)',
              border: '1px solid var(--color-border-secondary)',
              borderRadius: 4,
              fontSize: 'var(--font-size-sm, 12px)',
              fontFamily: 'var(--font-mono)',
              cursor: 'pointer',
              marginBottom: 6,
            }}
          >
            {OBJECTS.map((o) => (
              <option key={o.id} value={o.id}>{o.label} ({o.type})</option>
            ))}
          </select>

          {/* properties */}
          <div style={{ display: 'flex', flexDirection: 'column', gap: 2 }}>
            {selectedObj.props.map((p) => (
              <div key={p.key} style={{
                display: 'flex', justifyContent: 'space-between',
                fontSize: 'var(--font-size-xs, 11px)',
              }}>
                <span style={{ color: 'var(--color-text-tertiary, #555)' }}>{p.key}</span>
                <span style={{ fontFamily: 'var(--font-mono)', color: 'var(--color-text-primary)' }}>{p.value}</span>
              </div>
            ))}
          </div>
        </div>
      ))}

      {/* Measurements */}
      {section('\u6D4B\u91CF Measurements', (
        <div style={{ display: 'flex', flexWrap: 'wrap', gap: 4 }}>
          {MEASURE_BUTTONS.map((m) => (
            <button
              key={m}
              className="btn btn-small"
              onClick={() => measure(m)}
              style={{ fontSize: 'var(--font-size-xs, 11px)' }}
            >
              {MEASURE_LABELS[m]}
            </button>
          ))}
        </div>
      ))}

      {/* measurement results */}
      {Object.keys(measurements).length > 0 && (
        <div style={{
          padding: '6px 8px', borderRadius: 4,
          background: 'var(--color-bg-primary)',
          border: '1px solid var(--color-border-secondary)',
          display: 'flex', flexDirection: 'column', gap: 2,
        }}>
          {Object.entries(measurements).map(([k, v]) => (
            <div key={k} style={{
              display: 'flex', justifyContent: 'space-between',
              fontSize: 'var(--font-size-xs, 11px)',
            }}>
              <span style={{ color: 'var(--color-text-tertiary, #555)' }}>{k}</span>
              <span style={{ fontFamily: 'var(--font-mono)', color: 'var(--color-text-primary)' }}>{v}</span>
            </div>
          ))}
        </div>
      )}

      {/* Console */}
      {section('\u63A7\u5236\u53F0 Console', (
        <div style={{
          padding: '6px 8px', borderRadius: 4,
          background: '#010409',
          border: '1px solid var(--color-border-secondary)',
          fontFamily: 'var(--font-mono)', fontSize: 'var(--font-size-xs, 11px)',
          maxHeight: 120, overflowY: 'auto',
          lineHeight: 1.6,
        }}>
          {logs.map((l) => (
            <div key={l.id} style={{ color: l.level === 'warn' ? '#ffb74d' : l.level === 'error' ? '#e57373' : 'var(--color-text-secondary)' }}>
              {l.text}
            </div>
          ))}
        </div>
      ))}
    </div>
  );
};
