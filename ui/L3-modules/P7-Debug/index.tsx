import React, { useState, useEffect, useMemo } from 'react';
import { useGeometryStore, GeoObject } from '../../L5-core/store/geometryStore';

/* ── log entry ── */

interface LogEntry {
  id: number;
  level: 'info' | 'warn' | 'error';
  text: string;
}

/* ── helpers to build property list from GeoObject ── */

function buildProperties(obj: GeoObject, allObjects: GeoObject[]): { key: string; value: string }[] {
  const props: { key: string; value: string }[] = [];
  props.push({ key: 'ID', value: obj.id });
  props.push({ key: '类型 Type', value: obj.type });
  props.push({ key: '标签 Label', value: obj.label });
  props.push({ key: '颜色 Color', value: obj.color });
  props.push({ key: '可见 Visible', value: String(obj.visible) });
  props.push({ key: '创建时间 Created', value: new Date(obj.createdAt).toLocaleTimeString() });

  if (obj.type === 'point' && obj.x !== undefined && obj.y !== undefined) {
    props.push({ key: '坐标 Coordinates', value: `(${obj.x.toFixed(1)}, ${obj.y.toFixed(1)})` });
  }

  if (obj.startId) {
    const startPt = allObjects.find((o) => o.id === obj.startId);
    props.push({ key: '起点 Start', value: startPt ? `${startPt.label} (${startPt.id})` : obj.startId });
  }

  if (obj.endId) {
    const endPt = allObjects.find((o) => o.id === obj.endId);
    props.push({ key: '终点 End', value: endPt ? `${endPt.label} (${endPt.id})` : obj.endId });
  }

  if (obj.centerId) {
    const center = allObjects.find((o) => o.id === obj.centerId);
    props.push({ key: '圆心 Center', value: center ? `${center.label} (${center.id})` : obj.centerId });
  }

  if (obj.radiusPointId) {
    const rp = allObjects.find((o) => o.id === obj.radiusPointId);
    props.push({ key: '半径点 R.Point', value: rp ? `${rp.label} (${rp.id})` : obj.radiusPointId });
  }

  if (obj.vertexIds && obj.vertexIds.length > 0) {
    const labels = obj.vertexIds.map((vid) => allObjects.find((o) => o.id === vid)?.label ?? vid).join(', ');
    props.push({ key: '顶点 Vertices', value: labels });
  }

  if (obj.length !== undefined) {
    props.push({ key: '长度 Length', value: `${obj.length.toFixed(2)} px` });
  }

  if (obj.radius !== undefined) {
    props.push({ key: '半径 Radius', value: `${obj.radius.toFixed(2)} px` });
  }

  if (obj.angle !== undefined) {
    props.push({ key: '角度 Angle', value: `${obj.angle.toFixed(2)}\u00B0` });
  }

  if (obj.area !== undefined) {
    props.push({ key: '面积 Area', value: `${obj.area.toFixed(2)} px\u00B2` });
  }

  return props;
}

/* ── DebugPanel ── */

export const DebugPanel: React.FC = () => {
  const objects = useGeometryStore((s) => s.objects);
  const constraints = useGeometryStore((s) => s.constraints);
  const [selectedObjId, setSelectedObjId] = useState<string>('');
  const [logs, setLogs] = useState<LogEntry[]>([
    { id: 0, level: 'info', text: '[init] Lv-00 geometry engine loaded 加载完成' },
  ]);
  const [activeTab, setActiveTab] = useState<'inspector' | 'measurements' | 'console'>('inspector');

  // Auto-select first object if none selected
  useEffect(() => {
    if (objects.length > 0 && !selectedObjId) {
      setSelectedObjId(objects[0].id);
    }
    // If selected object was removed, reset
    if (selectedObjId && !objects.find((o) => o.id === selectedObjId)) {
      setSelectedObjId(objects.length > 0 ? objects[0].id : '');
    }
  }, [objects, selectedObjId]);

  // Log store changes
  const prevObjectsCount = React.useRef(objects.length);
  useEffect(() => {
    const prev = prevObjectsCount.current;
    const curr = objects.length;
    if (curr > prev) {
      setLogs((l) => [...l, {
        id: Date.now(),
        level: 'info',
        text: `[store] added ${curr - prev} object(s) \u2014 total: ${curr}`,
      }]);
    } else if (curr < prev) {
      setLogs((l) => [...l, {
        id: Date.now(),
        level: 'warn',
        text: `[store] removed ${prev - curr} object(s) \u2014 total: ${curr}`,
      }]);
    }
    prevObjectsCount.current = curr;
  }, [objects.length]);

  const selectedObj = useMemo(() => {
    return objects.find((o) => o.id === selectedObjId);
  }, [objects, selectedObjId]);

  const selectedObjProps = useMemo(() => {
    if (!selectedObj) return [];
    return buildProperties(selectedObj, objects);
  }, [selectedObj, objects]);

  // Computed measurements from store
  const measurementsData = useMemo(() => {
    const data: { key: string; value: string }[] = [];
    for (const o of objects) {
      if (o.type === 'segment' && o.length) {
        data.push({ key: `长度 ${o.label}`, value: `${o.length.toFixed(2)} px` });
      }
      if (o.type === 'circle' && o.radius) {
        data.push({ key: `半径 ${o.label}`, value: `${o.radius.toFixed(2)} px` });
        data.push({ key: `面积 ${o.label}`, value: `${(Math.PI * o.radius * o.radius).toFixed(2)} px\u00B2` });
        data.push({ key: `周长 ${o.label}`, value: `${(2 * Math.PI * o.radius).toFixed(2)} px` });
      }
      if (o.type === 'polygon' && o.area) {
        data.push({ key: `面积 ${o.label}`, value: `${o.area.toFixed(2)} px\u00B2` });
      }
      if (o.type === 'line' && o.length) {
        data.push({ key: `参考长 ${o.label}`, value: `${o.length.toFixed(2)} px` });
      }
    }
    return data;
  }, [objects]);

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
      {/* Tab bar */}
      <div style={{ display: 'flex', gap: 2 }}>
        {(['inspector', 'measurements', 'console'] as const).map((tab) => (
          <button
            key={tab}
            onClick={() => setActiveTab(tab)}
            className="btn btn-small"
            style={{
              flex: 1,
              fontSize: 'var(--font-size-xs, 11px)',
              background: activeTab === tab ? 'var(--color-module-debug, #e57373)' : undefined,
              color: activeTab === tab ? '#000' : undefined,
            }}
          >
            {tab === 'inspector' ? '检查器 Inspector' : tab === 'measurements' ? '测量 Measurements' : '控制台 Console'}
          </button>
        ))}
      </div>

      {/* Inspector Tab */}
      {activeTab === 'inspector' && section('对象检查器 Object Inspector', (
        <div style={{
          padding: '6px 8px', borderRadius: 4,
          background: 'var(--color-bg-primary)',
          border: '1px solid var(--color-border-secondary)',
        }}>
          {objects.length === 0 ? (
            <div style={{ color: 'var(--color-text-tertiary, #555)', fontSize: 'var(--font-size-xs, 11px)' }}>
              暂无对象 No objects. Add objects via Terminal.
            </div>
          ) : (
            <>
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
                {objects.map((o) => (
                  <option key={o.id} value={o.id}>{o.label} ({o.type})</option>
                ))}
              </select>

              {/* properties */}
              <div style={{ display: 'flex', flexDirection: 'column', gap: 2 }}>
                {selectedObjProps.map((p) => (
                  <div key={p.key} style={{
                    display: 'flex', justifyContent: 'space-between',
                    fontSize: 'var(--font-size-xs, 11px)',
                  }}>
                    <span style={{ color: 'var(--color-text-tertiary, #555)' }}>{p.key}</span>
                    <span style={{ fontFamily: 'var(--font-mono)', color: 'var(--color-text-primary)' }}>{p.value}</span>
                  </div>
                ))}
              </div>
            </>
          )}
        </div>
      ))}

      {/* Measurements Tab */}
      {activeTab === 'measurements' && section('测量 Measurements', (
        <div style={{
          padding: '6px 8px', borderRadius: 4,
          background: 'var(--color-bg-primary)',
          border: '1px solid var(--color-border-secondary)',
          display: 'flex', flexDirection: 'column', gap: 2,
        }}>
          {measurementsData.length === 0 ? (
            <div style={{ color: 'var(--color-text-tertiary, #555)', fontSize: 'var(--font-size-xs, 11px)' }}>
              暂无可计算的测量值 No computed measurements. Add segments/circles/polygons.
            </div>
          ) : (
            measurementsData.map((m) => (
              <div key={m.key} style={{
                display: 'flex', justifyContent: 'space-between',
                fontSize: 'var(--font-size-xs, 11px)',
              }}>
                <span style={{ color: 'var(--color-text-tertiary, #555)' }}>{m.key}</span>
                <span style={{ fontFamily: 'var(--font-mono)', color: 'var(--color-text-primary)' }}>{m.value}</span>
              </div>
            ))
          )}
          <div style={{ marginTop: 4, fontSize: 'var(--font-size-xs, 10px)', color: 'var(--color-text-tertiary, #555)' }}>
            约束数 Constraints: {constraints.length}
          </div>
        </div>
      ))}

      {/* Console Tab */}
      {activeTab === 'console' && section('控制台 Console', (
        <div style={{
          padding: '6px 8px', borderRadius: 4,
          background: '#010409',
          border: '1px solid var(--color-border-secondary)',
          fontFamily: 'var(--font-mono)', fontSize: 'var(--font-size-xs, 11px)',
          maxHeight: 300, overflowY: 'auto',
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
