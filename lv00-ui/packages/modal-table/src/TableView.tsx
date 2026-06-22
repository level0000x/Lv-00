// ============================================================
// @lv00/modal-table — M3 属性表格面板 (TableView)
// AG Grid 高性能表格 + 内联编辑 + 选中联动
// ============================================================

import React, { useMemo, useCallback, useEffect, useState, useRef } from 'react';
import { SceneController } from '@lv00/scene-controller';
import { NodeRow } from '@lv00/protocol';

// AG Grid 类型声明（运行时由 ag-grid-react 提供）
declare const agGrid: any;

interface TableViewProps {
  controller: SceneController;
}

const TRUST_COLORS: Record<string, string> = {
  GREEN: '#22C55E', LIGHT_GREEN: '#86EFAC', YELLOW: '#EAB308',
  ORANGE: '#FB923C', DARK_ORANGE: '#EA580C', RED: '#EF4444',
  GRAY: '#9CA3AF', BLUE: '#3B82F6', PURPLE: '#A855F7', CYAN: '#06B6D4',
};

function intToHex(color: number): string {
  const r = (color >> 16) & 0xFF;
  const g = (color >> 8) & 0xFF;
  const b = color & 0xFF;
  return `#${r.toString(16).padStart(2, '0')}${g.toString(16).padStart(2, '0')}${b.toString(16).padStart(2, '0')}`;
}

// 降级：纯 React 表格实现（无 AG Grid 依赖时使用）
export const TableView: React.FC<TableViewProps> = ({ controller }) => {
  const [rowData, setRowData] = useState<NodeRow[]>([]);
  const [selectedId, setSelectedId] = useState<number | null>(null);
  const [editCell, setEditCell] = useState<{ id: number; field: string } | null>(null);
  const editInputRef = useRef<HTMLInputElement>(null);

  // --- 数据同步 ---
  const refreshData = useCallback(() => {
    setRowData(controller.getNodesAsTable());
    const sel = controller.getSelection();
    if (sel.length > 0) setSelectedId(sel[0]);
  }, [controller]);

  useEffect(() => {
    refreshData();
    return controller.onStateChange(refreshData);
  }, [controller, refreshData]);

  // --- 行点击 → 选中联动 ---
  const handleRowClick = (id: number) => {
    controller.selectNode(id);
    setSelectedId(id);
  };

  // --- 内联编辑 ---
  const handleDoubleClick = (id: number, field: string) => {
    setEditCell({ id, field });
    setTimeout(() => editInputRef.current?.focus(), 50);
  };

  const handleEditCommit = () => {
    if (!editCell) return;
    const val = editInputRef.current?.value ?? '';
    controller.updateNodeCell(editCell.id, editCell.field, val);
    setEditCell(null);
  };

  const handleEditKeyDown = (e: React.KeyboardEvent) => {
    if (e.key === 'Enter') handleEditCommit();
    if (e.key === 'Escape') setEditCell(null);
  };

  if (rowData.length === 0) {
    return <div style={{ padding: 20, color: '#666' }}>无几何数据。请使用终端或文本编辑器添加节点。</div>;
  }

  return (
    <div style={{
      width: '100%', height: '100%', overflow: 'auto',
      background: '#0a0a0a', color: '#c8c8c8', fontSize: 13,
    }}>
      <table style={{ width: '100%', borderCollapse: 'collapse' }}>
        <thead>
          <tr style={{ background: '#161b22', position: 'sticky', top: 0, zIndex: 2 }}>
            {['ID', '名称', '类型', 'X', 'Y', '状态', '约束数'].map(h => (
              <th key={h} style={thStyle}>{h}</th>
            ))}
          </tr>
        </thead>
        <tbody>
          {rowData.map(row => {
            const isSel = row.id === selectedId;
            return (
              <tr
                key={row.id}
                onClick={() => handleRowClick(row.id)}
                style={{
                  ...trStyle,
                  background: isSel ? '#1a3a5c' : 'transparent',
                  cursor: 'pointer',
                }}
                onMouseEnter={e => { if (!isSel) e.currentTarget.style.background = '#111'; }}
                onMouseLeave={e => { if (!isSel) e.currentTarget.style.background = 'transparent'; }}
              >
                <td style={tdMono}>{row.id}</td>
                <td style={tdStyle}>{row.name}</td>
                <td style={tdStyle}>
                  <span style={{ color: row.type === 'point' ? '#4caf50' : '#ff9800' }}>
                    {row.type}
                  </span>
                </td>
                <td style={tdMono} onDoubleClick={() => handleDoubleClick(row.id, 'coordX')}>
                  {editCell?.id === row.id && editCell?.field === 'coordX' ? (
                    <input ref={editInputRef} defaultValue={row.coordX}
                      onBlur={handleEditCommit} onKeyDown={handleEditKeyDown}
                      style={editInputStyle} />
                  ) : row.coordX}
                </td>
                <td style={tdMono} onDoubleClick={() => handleDoubleClick(row.id, 'coordY')}>
                  {editCell?.id === row.id && editCell?.field === 'coordY' ? (
                    <input ref={editInputRef} defaultValue={row.coordY}
                      onBlur={handleEditCommit} onKeyDown={handleEditKeyDown}
                      style={editInputStyle} />
                  ) : row.coordY}
                </td>
                <td style={tdStyle}>
                  <span style={{
                    display: 'inline-block', width: 14, height: 14, borderRadius: '50%',
                    background: intToHex(row.color), verticalAlign: 'middle',
                  }} />
                </td>
                <td style={{ ...tdStyle, textAlign: 'center' }}>
                  {row.constraint_count}
                </td>
              </tr>
            );
          })}
        </tbody>
      </table>
    </div>
  );
};

const thStyle: React.CSSProperties = {
  padding: '8px 12px', textAlign: 'left', fontWeight: 600,
  borderBottom: '1px solid #30363d', color: '#8b949e', fontSize: 12,
  textTransform: 'uppercase',
};

const trStyle: React.CSSProperties = {
  borderBottom: '1px solid #1a1a1a',
  transition: 'background 0.15s',
};

const tdStyle: React.CSSProperties = { padding: '6px 12px' };
const tdMono: React.CSSProperties = { padding: '6px 12px', fontFamily: 'monospace' };

const editInputStyle: React.CSSProperties = {
  background: '#222', color: '#c8c8c8', border: '1px solid #4caf50',
  padding: '2px 8px', borderRadius: 4, width: 100, fontFamily: 'monospace', fontSize: 13,
};
