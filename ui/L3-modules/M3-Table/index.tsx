import React from 'react';
import { TableRow } from '../types';
import { Empty } from '../shared';

interface TableViewProps {
  rows: TableRow[];
  selectedId?: number | null;
  onRowClick?: (id: number) => void;
}

const Columns = ['ID', 'Name', 'Type', 'X', 'Y', 'Constraints'];

export const TableView: React.FC<TableViewProps> = ({ rows, selectedId, onRowClick }) => {
  if (!rows.length) return <Empty msg="No data to display" icon={'\uD83D\uDCCA'} />;

  return (
    <div style={{ width: '100%', height: '100%', overflow: 'auto' }}>
      <table style={{
        width: '100%', borderCollapse: 'collapse',
        fontFamily: 'var(--font-mono)', fontSize: 12,
      }}>
        <thead>
          <tr style={{ position: 'sticky', top: 0, zIndex: 2, background: 'var(--color-bg-secondary)' }}>
            {Columns.map((h) => (
              <th key={h} style={{
                padding: '8px 14px', textAlign: 'left', fontWeight: 600,
                borderBottom: '1px solid var(--color-border-primary)',
                color: 'var(--color-text-secondary)', fontSize: 11,
                textTransform: 'uppercase',
              }}>
                {h}
              </th>
            ))}
          </tr>
        </thead>
        <tbody>
          {rows.map((r) => {
            const sel = r.id === selectedId;
            return (
              <tr
                key={r.id}
                onClick={() => onRowClick?.(r.id)}
                style={{
                  borderBottom: '1px solid var(--color-border-secondary)',
                  background: sel ? 'rgba(var(--color-accent-rgb), 0.08)' : 'transparent',
                  cursor: 'pointer',
                  transition: 'background 0.12s',
                  color: 'var(--color-text-primary)',
                }}
                onMouseEnter={(e) => {
                  if (!sel) (e.currentTarget as HTMLElement).style.background = 'var(--color-bg-hover-subtle)';
                }}
                onMouseLeave={(e) => {
                  if (!sel) (e.currentTarget as HTMLElement).style.background = 'transparent';
                }}
              >
                <td style={td}>{r.id}</td>
                <td style={td}>{r.name}</td>
                <td style={td}>
                  <span style={{ color: 'var(--color-success)', fontWeight: 600 }}>{r.type}</span>
                </td>
                <td style={td}>{r.coordX}</td>
                <td style={td}>{r.coordY}</td>
                <td style={{ ...td, textAlign: 'center', color: 'var(--color-text-secondary)' }}>
                  {r.constraintCount}
                </td>
              </tr>
            );
          })}
        </tbody>
      </table>
    </div>
  );
};

const td: React.CSSProperties = { padding: '7px 14px' };
