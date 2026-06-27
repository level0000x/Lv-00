import React, { useState, useCallback, useRef, useEffect } from 'react';
import { useGeometryStore, GeoObject } from '../../L5-core/store/geometryStore';
import { Empty } from '../shared';

const TYPE_LABELS: Record<string, string> = {
  point: '点 Point',
  segment: '线段 Segment',
  line: '直线 Line',
  ray: '射线 Ray',
  circle: '圆 Circle',
  arc: '弧 Arc',
  polygon: '多边形 Polygon',
  angle: '角 Angle',
};

/* ---- Inline cell editor ---- */
const CellEditor: React.FC<{
  value: string;
  onCommit: (val: string) => void;
  onCancel: () => void;
}> = ({ value, onCommit, onCancel }) => {
  const ref = useRef<HTMLInputElement>(null);
  const [val, setVal] = useState(value);

  useEffect(() => {
    ref.current?.focus();
    ref.current?.select();
  }, []);

  return (
    <input
      ref={ref}
      value={val}
      onChange={(e) => setVal(e.target.value)}
      onBlur={() => onCommit(val)}
      onKeyDown={(e) => {
        if (e.key === 'Enter') onCommit(val);
        if (e.key === 'Escape') onCancel();
      }}
      style={{
        width: '100%',
        background: 'var(--color-bg-primary)',
        border: '1px solid var(--color-accent)',
        borderRadius: 3,
        color: 'var(--color-text-bright)',
        fontFamily: 'var(--font-mono)',
        fontSize: 12,
        padding: '2px 6px',
        outline: 'none',
      }}
    />
  );
};

const td: React.CSSProperties = { padding: '7px 14px' };

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

  // Count constraints per object
  const constraintCount = useCallback((objId: string): number => {
    return constraints.filter((c) => c.objIds.includes(objId)).length;
  }, [constraints]);

  // Handle cell double-click to edit
  const handleCellDblClick = (objId: string, field: string) => {
    setEditingCell({ id: objId, field });
  };

  // Commit cell edit
  const commitCell = (val: string) => {
    if (!editingCell) return;
    const { id, field } = editingCell;
    if (field === 'label') {
      updateObject(id, { label: val });
    } else if (field === 'x') {
      const num = parseFloat(val);
      if (!isNaN(num)) updateObject(id, { x: num });
    } else if (field === 'y') {
      const num = parseFloat(val);
      if (!isNaN(num)) updateObject(id, { y: num });
    }
    setEditingCell(null);
  };

  // Add point
  const addPoint = () => {
    const id = addObject({ type: 'point', label: '', x: 0, y: 0, visible: true });
    selectObjects([id]);
  };

  // Delete object
  const deleteObj = (objId: string) => {
    removeObject(objId);
  };

  // Click row to select
  const handleRowClick = (objId: string) => {
    selectObjects([objId]);
  };

  if (!objects.length) return <Empty msg="无数据 No data to display" icon={'📊'} />;

  const isEditing = (objId: string, field: string) =>
    editingCell && editingCell.id === objId && editingCell.field === field;

  return (
    <div style={{ width: '100%', height: '100%', overflow: 'auto', display: 'flex', flexDirection: 'column' }}>
      <table style={{ width: '100%', borderCollapse: 'collapse', fontFamily: 'var(--font-mono)', fontSize: 12 }}>
        <thead>
          <tr style={{ position: 'sticky', top: 0, zIndex: 2, background: 'var(--color-bg-secondary)' }}>
            {['ID', '类型 Type', '名称 Label', 'X', 'Y', '属性 Props', '约束 Constraints'].map((h) => (
              <th key={h} style={{
                padding: '8px 14px', textAlign: 'left', fontWeight: 600,
                borderBottom: '1px solid var(--color-border-primary)',
                color: 'var(--color-text-secondary)', fontSize: 11, textTransform: 'uppercase',
              }}>{h}</th>
            ))}
            <th style={{ width: 32 }} />
          </tr>
        </thead>
        <tbody>
          {objects.map((obj) => {
            const sel = selectedIds.includes(obj.id);
            const hovered = obj.id === hoverId;
            const cc = constraintCount(obj.id);

            // Computed property display
            let propDisplay = '';
            if (obj.type === 'segment' || obj.type === 'line' || obj.type === 'ray') {
              propDisplay = obj.length !== undefined ? `L=${obj.length.toFixed(1)}` : '';
            } else if (obj.type === 'circle') {
              propDisplay = obj.radius !== undefined ? `R=${obj.radius.toFixed(1)}` : '';
            } else if (obj.type === 'polygon') {
              propDisplay = obj.area !== undefined ? `S=${obj.area.toFixed(1)}` : '';
            } else if (obj.type === 'angle') {
              propDisplay = obj.angle !== undefined ? `${obj.angle.toFixed(1)}deg` : '';
            }

            const renderEditableCell = (field: 'label' | 'x' | 'y', displayVal: string) => {
              if (isEditing(obj.id, field)) {
                return (
                  <CellEditor
                    value={displayVal}
                    onCommit={commitCell}
                    onCancel={() => setEditingCell(null)}
                  />
                );
              }
              const isEditable = field === 'label' || (obj.type === 'point' && (field === 'x' || field === 'y'));
              return (
                <span
                  onDoubleClick={isEditable ? () => handleCellDblClick(obj.id, field) : undefined}
                  style={{
                    cursor: isEditable ? 'text' : 'default',
                    padding: '1px 0',
                    display: 'inline-block',
                    minWidth: 30,
                    color: isEditable ? 'var(--color-text-primary)' : 'var(--color-text-secondary)',
                  }}
                  title={isEditable ? '双击编辑 Double-click to edit' : ''}
                >
                  {displayVal}
                </span>
              );
            };

            return (
              <tr
                key={obj.id}
                onClick={() => handleRowClick(obj.id)}
                onMouseEnter={() => setHoverId(obj.id)}
                onMouseLeave={() => setHoverId(null)}
                style={{
                  borderBottom: '1px solid var(--color-border-secondary)',
                  background: sel ? 'rgba(0,188,212,0.08)' : 'transparent',
                  cursor: 'pointer',
                  transition: 'background 0.12s',
                  color: 'var(--color-text-primary)',
                }}
              >
                <td style={{ ...td, color: 'var(--color-text-muted)', fontSize: 11 }}>{obj.id}</td>
                <td style={td}>
                  <span style={{ color: obj.color, fontWeight: 600 }}>
                    {TYPE_LABELS[obj.type] ?? obj.type}
                  </span>
                </td>
                <td style={td}>{renderEditableCell('label', obj.label)}</td>
                <td style={td}>
                  {obj.type === 'point'
                    ? renderEditableCell('x', String(obj.x ?? '--'))
                    : '--'}
                </td>
                <td style={td}>
                  {obj.type === 'point'
                    ? renderEditableCell('y', String(obj.y ?? '--'))
                    : '--'}
                </td>
                <td style={{ ...td, color: 'var(--color-text-secondary)', fontSize: 11 }}>
                  {propDisplay || '--'}
                </td>
                <td style={{ ...td, textAlign: 'center', color: cc > 0 ? 'var(--color-accent)' : 'var(--color-text-muted)' }}>
                  {cc}
                </td>
                <td style={td}>
                  {(hovered || sel) && (
                    <button
                      onClick={(e) => { e.stopPropagation(); deleteObj(obj.id); }}
                      title="删除 Delete"
                      style={{
                        background: 'transparent',
                        border: 'none',
                        color: 'var(--color-danger)',
                        cursor: 'pointer',
                        fontSize: 14,
                        padding: 0,
                        lineHeight: 1,
                        opacity: 0.8,
                      }}
                    >
                      x
                    </button>
                  )}
                </td>
              </tr>
            );
          })}
        </tbody>
      </table>

      {/* Add point button */}
      <div
        onClick={addPoint}
        style={{
          padding: '6px 14px',
          borderTop: '1px solid var(--color-border-secondary)',
          cursor: 'pointer',
          color: 'var(--color-text-muted)',
          fontSize: 12,
          display: 'flex',
          alignItems: 'center',
          gap: 6,
          transition: 'color 0.15s, background 0.15s',
          userSelect: 'none',
        }}
        onMouseEnter={(e) => {
          (e.currentTarget as HTMLElement).style.color = 'var(--color-accent)';
          (e.currentTarget as HTMLElement).style.background = 'var(--color-bg-hover-subtle)';
        }}
        onMouseLeave={(e) => {
          (e.currentTarget as HTMLElement).style.color = 'var(--color-text-muted)';
          (e.currentTarget as HTMLElement).style.background = 'transparent';
        }}
      >
        <span style={{ fontSize: 16, lineHeight: 1 }}>+</span>
        <span>添加点 Add Point</span>
      </div>
    </div>
  );
};
