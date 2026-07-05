import React, { useState, useCallback, useRef, useEffect } from 'react';
import { useGeometryStore, GeoObject } from '../../L5-core/store/geometryStore';
import { Empty } from '../shared';

const TYPE_LABELS: Record<string, string> = {
  point: 'Point', segment: 'Segment', line: 'Line', ray: 'Ray',
  circle: 'Circle', arc: 'Arc', polygon: 'Polygon', angle: 'Angle',
};

const CellEditor: React.FC<{ value: string; onCommit: (val: string) => void; onCancel: () => void }> = ({ value, onCommit, onCancel }) => {
  const ref = useRef<HTMLInputElement>(null);
  const [val, setVal] = useState(value);
  useEffect(() => { ref.current?.focus(); ref.current?.select(); }, []);
  return (
    <input ref={ref} value={val} onChange={(e) => setVal(e.target.value)}
      onBlur={() => onCommit(val)} onKeyDown={(e) => { if (e.key === 'Enter') onCommit(val); if (e.key === 'Escape') onCancel(); }}
      style={{ width: '100%', background: 'var(--color-bg-primary)', border: '1px solid var(--color-accent)', borderRadius: 3, color: 'var(--color-text-bright)', fontFamily: 'var(--font-mono)', fontSize: 12, padding: '2px 6px', outline: 'none' }} />
  );
};

export const TableView: React.FC = () => {
  const objects = useGeometryStore((s) => s.objects);
  const constraints = useGeometryStore((s) => s.constraints);
  const updateObject = useGeometryStore((s) => s.updateObject);
  const removeObject = useGeometryStore((s) => s.removeObject);
  const addObject = useGeometryStore((s) => s.addObject);
  const selectObjects = useGeometryStore((s) => s.selectObjects);
  const selectedIds = useGeometryStore((s) => s.selectedIds);
  const [editingCell, setEditingCell] = useState<{ id: string; field: string } | null>(null);
  const [hoverId, setHoverId] = useState<string | null>(null);

  const constraintCount = useCallback((objId: string): number => constraints.filter((c) => c.objIds.includes(objId)).length, [constraints]);
  const commitCell = (val: string) => {
    if (!editingCell) return;
    const { id, field } = editingCell;
    if (field === 'label') updateObject(id, { label: val });
    else if (field === 'x') { const n = parseFloat(val); if (!isNaN(n)) updateObject(id, { x: n }); }
    else if (field === 'y') { const n = parseFloat(val); if (!isNaN(n)) updateObject(id, { y: n }); }
    setEditingCell(null);
  };

  if (!objects.length) return <Empty msg="No data" icon={'\u{1F4CA}'} />;

  return (
    <div style={{ width: '100%', height: '100%', overflow: 'auto', display: 'flex', flexDirection: 'column' }}>
      <table style={{ width: '100%', borderCollapse: 'collapse', fontFamily: 'var(--font-mono)', fontSize: 12 }}>
        <thead>
          <tr style={{ position: 'sticky', top: 0, zIndex: 2, background: 'var(--color-bg-secondary)' }}>
            {['ID', 'Type', 'Label', 'X', 'Y', 'Props', 'Con'].map((h) => (
              <th key={h} style={{ padding: '8px 14px', textAlign: 'left', fontWeight: 600, borderBottom: '1px solid var(--color-border-primary)', color: 'var(--color-text-secondary)', fontSize: 11 }}>{h}</th>
            ))}
            <th style={{ width: 32 }} />
          </tr>
        </thead>
        <tbody>
          {objects.map((obj) => {
            const sel = selectedIds.includes(obj.id);
            const hovered = obj.id === hoverId;
            const cc = constraintCount(obj.id);
            let propDisplay = '';
            if (obj.type === 'segment' || obj.type === 'line') propDisplay = obj.length !== undefined ? 'L=' + obj.length.toFixed(1) : '';
            else if (obj.type === 'circle') propDisplay = obj.radius !== undefined ? 'R=' + obj.radius.toFixed(1) : '';
            else if (obj.type === 'polygon') propDisplay = obj.area !== undefined ? 'S=' + obj.area.toFixed(1) : '';
            const isEditing = (f: string) => editingCell && editingCell.id === obj.id && editingCell.field === f;
            const renderCell = (field: string, displayVal: string) => {
              if (isEditing(field)) return <CellEditor value={displayVal} onCommit={commitCell} onCancel={() => setEditingCell(null)} />;
              const editable = field === 'label' || (obj.type === 'point' && (field === 'x' || field === 'y'));
              return <span onDoubleClick={editable ? () => setEditingCell({ id: obj.id, field }) : undefined}
                style={{ cursor: editable ? 'text' : 'default', padding: '1px 0', display: 'inline-block', minWidth: 30,
                  color: editable ? 'var(--color-text-primary)' : 'var(--color-text-secondary)' }}>{displayVal}</span>;
            };
            return (
              <tr key={obj.id} onClick={() => selectObjects([obj.id])}
                onMouseEnter={() => setHoverId(obj.id)} onMouseLeave={() => setHoverId(null)}
                style={{ borderBottom: '1px solid var(--color-border-secondary)', background: sel ? 'rgba(0,188,212,0.08)' : 'transparent', cursor: 'pointer', color: 'var(--color-text-primary)' }}>
                <td style={{ padding: '7px 14px', color: 'var(--color-text-muted)', fontSize: 11 }}>{obj.id}</td>
                <td style={{ padding: '7px 14px' }}><span style={{ color: obj.color, fontWeight: 600 }}>{TYPE_LABELS[obj.type] ?? obj.type}</span></td>
                <td style={{ padding: '7px 14px' }}>{renderCell('label', obj.label)}</td>
                <td style={{ padding: '7px 14px' }}>{obj.type === 'point' ? renderCell('x', String(obj.x ?? '--')) : '--'}</td>
                <td style={{ padding: '7px 14px' }}>{obj.type === 'point' ? renderCell('y', String(obj.y ?? '--')) : '--'}</td>
                <td style={{ padding: '7px 14px', color: 'var(--color-text-secondary)', fontSize: 11 }}>{propDisplay || '--'}</td>
                <td style={{ padding: '7px 14px', textAlign: 'center', color: cc > 0 ? 'var(--color-accent)' : 'var(--color-text-muted)' }}>{cc}</td>
                <td style={{ padding: '7px 14px' }}>
                  {(hovered || sel) && <button onClick={(e) => { e.stopPropagation(); removeObject(obj.id); }}
                    style={{ background: 'transparent', border: 'none', color: 'var(--color-danger)', cursor: 'pointer', fontSize: 14, padding: 0 }}>x</button>}
                </td>
              </tr>
            );
          })}
        </tbody>
      </table>
      <div onClick={() => { const id = addObject({ type: 'point', label: '', x: 0, y: 0, visible: true }); selectObjects([id]); }}
        style={{ padding: '6px 14px', borderTop: '1px solid var(--color-border-secondary)', cursor: 'pointer', color: 'var(--color-text-muted)', fontSize: 12 }}>
        + Add Point
      </div>
    </div>
  );
};
