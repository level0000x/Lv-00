import React, { useState, useCallback, useRef, useEffect } from 'react';
import { TableRow, trustColorToCSS } from '../types';
import { Empty } from '../shared';

/* ---- Modified row type for editing ---- */
export interface ModifiedRow {
  id: number;
  name: string;
  coordX: string;
  coordY: string;
  nodeType: string;
  constraintCount: number;
  colorRGBA: number;
  trustColor: string;
  status: string;
  parentBlockId: number;
}

interface TableViewProps {
  rows: TableRow[];
  selectedId?: number | null;
  onRowClick?: (id: number) => void;
  onRowsChange?: (rows: ModifiedRow[]) => void;
}

const Columns = ['ID', '名称 Name', '类型 Type', 'X', 'Y', '约束数 Cnt'];

/* ---- Inline cell editor ---- */
const CellEditor: React.FC<{
  value: string;
  onCommit: (val: string) => void;
  onCancel: () => void;
  style?: React.CSSProperties;
}> = ({ value, onCommit, onCancel, style }) => {
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
        if (e.key === 'Enter') { onCommit(val); }
        if (e.key === 'Escape') { onCancel(); }
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
        boxShadow: '0 0 0 2px rgba(var(--color-accent-rgb), 0.15)',
        ...style,
      }}
    />
  );
};

export const TableView: React.FC<TableViewProps> = ({
  rows: initialRows,
  selectedId,
  onRowClick,
  onRowsChange,
}) => {
  /* ---- Local editable rows ---- */
  const [localRows, setLocalRows] = useState<ModifiedRow[]>(() =>
    initialRows.map((r) => ({
      id: r.id, name: r.name, coordX: r.coordX, coordY: r.coordY,
      nodeType: r.nodeType, constraintCount: r.constraintCount,
      colorRGBA: r.colorRGBA, trustColor: r.trustColor as string,
      status: r.status, parentBlockId: r.parentBlockId,
    }))
  );

  /* Sync from props when they change externally */
  useEffect(() => {
    setLocalRows(initialRows.map((r) => ({
      id: r.id, name: r.name, coordX: r.coordX, coordY: r.coordY,
      nodeType: r.nodeType, constraintCount: r.constraintCount,
      colorRGBA: r.colorRGBA, trustColor: r.trustColor as string,
      status: r.status, parentBlockId: r.parentBlockId,
    })));
  }, [initialRows]);

  /* ---- Cell editing state ---- */
  const [editingCell, setEditingCell] = useState<{ rowId: number; field: string } | null>(null);
  const [hoverRowId, setHoverRowId] = useState<number | null>(null);

  const notifyChange = useCallback((next: ModifiedRow[]) => {
    setLocalRows(next);
    onRowsChange?.(next);
  }, [onRowsChange]);

  /* ---- Double-click to edit ---- */
  const handleCellDblClick = (rowId: number, field: string) => {
    setEditingCell({ rowId, field });
  };

  /* ---- Commit cell edit ---- */
  const commitCell = (val: string) => {
    if (!editingCell) return;
    const { rowId, field } = editingCell;
    const next = localRows.map((r) => {
      if (r.id !== rowId) return r;
      if (field === 'name') return { ...r, name: val };
      if (field === 'coordX') return { ...r, coordX: val };
      if (field === 'coordY') return { ...r, coordY: val };
      return r;
    });
    notifyChange(next);
    setEditingCell(null);
  };

  /* ---- Add row ---- */
  const addRow = () => {
    const maxId = localRows.reduce((m, r) => Math.max(m, r.id), 0);
    const newRow: ModifiedRow = {
      id: maxId + 1,
      name: `New_${maxId + 1}`,
      coordX: '0',
      coordY: '0',
      nodeType: 'Point',
      constraintCount: 0,
      colorRGBA: 0,
      trustColor: 'GREY',
      status: 'free',
      parentBlockId: 1,
    };
    notifyChange([...localRows, newRow]);
  };

  /* ---- Delete row ---- */
  const deleteRow = (rowId: number) => {
    notifyChange(localRows.filter((r) => r.id !== rowId));
  };

  if (!localRows.length) return <Empty msg="无数据 No data to display" icon={'📊'} />;

  return (
    <div style={{ width: '100%', height: '100%', overflow: 'auto', display: 'flex', flexDirection: 'column' }}>
      <table style={{ width: '100%', borderCollapse: 'collapse', fontFamily: 'var(--font-mono)', fontSize: 12 }}>
        <thead>
          <tr style={{ position: 'sticky', top: 0, zIndex: 2, background: 'var(--color-bg-secondary)' }}>
            {Columns.map((h) => (
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
          {localRows.map((r) => {
            const sel = r.id === selectedId;
            const hovered = r.id === hoverRowId;
            const col = trustColorToCSS(r.trustColor as any);
            const isEditing = (field: string) =>
              editingCell && editingCell.rowId === r.id && editingCell.field === field;

            const renderCell = (field: 'name' | 'coordX' | 'coordY', displayVal: string) => {
              if (isEditing(field)) {
                return (
                  <CellEditor
                    value={displayVal}
                    onCommit={commitCell}
                    onCancel={() => setEditingCell(null)}
                  />
                );
              }
              return (
                <span
                  onDoubleClick={() => handleCellDblClick(r.id, field)}
                  style={{ cursor: 'text', padding: '1px 0', display: 'inline-block', minWidth: 30 }}
                >
                  {displayVal}
                </span>
              );
            };

            return (
              <tr
                key={r.id}
                onClick={() => onRowClick?.(r.id)}
                onMouseEnter={() => setHoverRowId(r.id)}
                onMouseLeave={() => setHoverRowId(null)}
                style={{
                  borderBottom: '1px solid var(--color-border-secondary)',
                  background: sel ? 'rgba(var(--color-accent-rgb), 0.08)' : 'transparent',
                  cursor: 'pointer',
                  transition: 'background 0.12s',
                  color: 'var(--color-text-primary)',
                }}
              >
                <td style={td}>{r.id}</td>
                <td style={td}>{renderCell('name', r.name)}</td>
                <td style={td}><span style={{ color: col, fontWeight: 600 }}>{r.nodeType}</span></td>
                <td style={td}>{renderCell('coordX', r.coordX)}</td>
                <td style={td}>{renderCell('coordY', r.coordY)}</td>
                <td style={{ ...td, textAlign: 'center', color: 'var(--color-text-secondary)' }}>{r.constraintCount}</td>
                <td style={td}>
                  {(hovered || sel) && (
                    <button
                      onClick={(e) => { e.stopPropagation(); deleteRow(r.id); }}
                      title="删除行 Delete row"
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
                      ×
                    </button>
                  )}
                </td>
              </tr>
            );
          })}
        </tbody>
      </table>
      {/* Add row button */}
      <div
        onClick={addRow}
        style={{
          padding: '6px 14px',
          borderTop: '1px solid var(--color-border-secondary)',
          cursor: 'pointer',
          color: 'var(--color-text-muted)',
          fontSize: 12,
          display: 'flex',
          alignItems: 'center',
          gap: 6,
          transition: 'color var(--transition-fast), background var(--transition-fast)',
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
        <span>添加行 Add Row</span>
      </div>
    </div>
  );
};

const td: React.CSSProperties = { padding: '7px 14px' };
