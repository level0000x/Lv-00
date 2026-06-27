import React, { useState, useCallback } from 'react';
import { useGeometryStore, GeoObject, GeoObjectType } from '../../L5-core/store/geometryStore';
import { Empty } from '../shared';

/* ---- Type group config ---- */
interface TypeGroup {
  type: GeoObjectType;
  label: string;
  icon: string;
}

const TYPE_GROUPS: TypeGroup[] = [
  { type: 'point', label: '点 Points', icon: '●' },
  { type: 'segment', label: '线段 Segments', icon: '━' },
  { type: 'circle', label: '圆 Circles', icon: '○' },
  { type: 'line', label: '直线 Lines', icon: '╱' },
  { type: 'ray', label: '射线 Rays', icon: '→' },
  { type: 'arc', label: '弧 Arcs', icon: '◠' },
  { type: 'polygon', label: '多边形 Polygons', icon: '△' },
  { type: 'angle', label: '角 Angles', icon: '∠' },
];

export const TreeView: React.FC = () => {
  const objects = useGeometryStore((s) => s.objects);
  const selectObjects = useGeometryStore((s) => s.selectObjects);
  const removeObject = useGeometryStore((s) => s.removeObject);
  const addObject = useGeometryStore((s) => s.addObject);
  const selectedIds = useGeometryStore((s) => s.selectedIds);
  const [expandedGroups, setExpandedGroups] = useState<Set<string>>(
    () => new Set(TYPE_GROUPS.map((g) => g.type))
  );

  const toggleGroup = (type: string) => {
    setExpandedGroups((prev) => {
      const next = new Set(prev);
      if (next.has(type)) next.delete(type);
      else next.add(type);
      return next;
    });
  };

  const expandAll = () => {
    setExpandedGroups(new Set(TYPE_GROUPS.map((g) => g.type)));
  };

  const collapseAll = () => {
    setExpandedGroups(new Set());
  };

  const handleNodeClick = (objId: string) => {
    selectObjects([objId]);
  };

  const handleAddObject = (type: GeoObjectType) => {
    if (type === 'point') {
      addObject({ type: 'point', label: '', x: 200, y: 200, visible: true });
    }
  };

  const handleDeleteObject = (objId: string) => {
    removeObject(objId);
  };

  if (!objects.length) return <Empty msg="无几何对象 No geometry objects" icon={'🌿'} />;

  // Format object subtitle
  const formatSubtitle = (obj: GeoObject): string => {
    if (obj.type === 'point') {
      return `(${obj.x?.toFixed(0) ?? '?'}, ${obj.y?.toFixed(0) ?? '?'})`;
    }
    if (obj.type === 'segment' || obj.type === 'line' || obj.type === 'ray') {
      return obj.length !== undefined ? `L = ${obj.length.toFixed(1)}` : '';
    }
    if (obj.type === 'circle') {
      return obj.radius !== undefined ? `R = ${obj.radius.toFixed(1)}` : '';
    }
    if (obj.type === 'polygon') {
      return obj.area !== undefined ? `S = ${obj.area.toFixed(1)}` : '';
    }
    if (obj.type === 'angle') {
      return obj.angle !== undefined ? `${obj.angle.toFixed(1)} deg` : '';
    }
    return '';
  };

  // Format properties for expanded child nodes
  const formatProperties = (obj: GeoObject): { key: string; value: string }[] => {
    const props: { key: string; value: string }[] = [];
    if (obj.type === 'point') {
      props.push({ key: 'X', value: String(obj.x ?? '?') });
      props.push({ key: 'Y', value: String(obj.y ?? '?') });
    } else if (obj.type === 'segment' || obj.type === 'line' || obj.type === 'ray') {
      props.push({ key: 'Start', value: obj.startId ?? '?' });
      props.push({ key: 'End', value: obj.endId ?? '?' });
      if (obj.length !== undefined) props.push({ key: 'Length', value: obj.length.toFixed(1) });
    } else if (obj.type === 'circle') {
      props.push({ key: 'Center', value: obj.centerId ?? '?' });
      props.push({ key: 'Radius Pt', value: obj.radiusPointId ?? '?' });
      if (obj.radius !== undefined) props.push({ key: 'Radius', value: obj.radius.toFixed(1) });
    }
    return props;
  };

  return (
    <div style={{ width: '100%', height: '100%', display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
      {/* Toolbar */}
      <div style={{
        padding: '4px 8px',
        borderBottom: '1px solid var(--color-border-secondary)',
        display: 'flex',
        gap: 4,
        flexShrink: 0,
      }}>
        <button
          className="btn btn-small"
          onClick={() => handleAddObject('point')}
        >
          + 添加点 Add Point
        </button>
        <button
          className="btn btn-small"
          onClick={collapseAll}
        >
          全部折叠 Collapse
        </button>
        <button
          className="btn btn-small"
          onClick={expandAll}
        >
          全部展开 Expand
        </button>
      </div>

      {/* Tree content */}
      <div style={{ flex: 1, overflow: 'auto', padding: '4px 0' }}>
        {TYPE_GROUPS.map((group) => {
          const groupObjects = objects.filter((o) => o.type === group.type);
          if (groupObjects.length === 0) return null;

          const isExpanded = expandedGroups.has(group.type);

          return (
            <div key={group.type}>
              {/* Group header */}
              <div
                onClick={() => toggleGroup(group.type)}
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  cursor: 'pointer',
                  padding: '5px 8px',
                  fontWeight: 600,
                  fontSize: 12,
                  color: 'var(--color-text-secondary)',
                  gap: 6,
                  userSelect: 'none',
                }}
                onMouseEnter={(e) => { (e.currentTarget as HTMLElement).style.background = 'var(--color-bg-hover-subtle)'; }}
                onMouseLeave={(e) => { (e.currentTarget as HTMLElement).style.background = 'transparent'; }}
              >
                <span style={{ fontSize: 10, width: 12 }}>{isExpanded ? '▼' : '▶'}</span>
                <span>{group.icon}</span>
                <span>{group.label}</span>
                <span style={{ fontSize: 10, color: 'var(--color-text-muted)' }}>({groupObjects.length})</span>
              </div>

              {/* Group children */}
              {isExpanded && groupObjects.map((obj) => {
                const isSelected = selectedIds.includes(obj.id);
                const props = formatProperties(obj);

                return (
                  <div key={obj.id}>
                    {/* Object node */}
                    <div
                      onClick={() => handleNodeClick(obj.id)}
                      style={{
                        display: 'flex',
                        alignItems: 'center',
                        cursor: 'pointer',
                        padding: '3px 8px 3px 28px',
                        fontSize: 12,
                        color: 'var(--color-text-primary)',
                        gap: 6,
                        background: isSelected ? 'rgba(0,188,212,0.08)' : 'transparent',
                      }}
                      onMouseEnter={(e) => {
                        if (!isSelected) (e.currentTarget as HTMLElement).style.background = 'var(--color-bg-hover-subtle)';
                      }}
                      onMouseLeave={(e) => {
                        if (!isSelected) (e.currentTarget as HTMLElement).style.background = 'transparent';
                      }}
                    >
                      <span style={{ width: 8, height: 8, borderRadius: '50%', background: obj.color, flexShrink: 0 }} />
                      <span style={{ fontWeight: 500 }}>{obj.label}</span>
                      <span style={{ color: 'var(--color-text-muted)', fontSize: 11 }}>{formatSubtitle(obj)}</span>
                      <span style={{ marginLeft: 'auto', fontSize: 11, color: 'var(--color-text-muted)' }}>{obj.id}</span>
                    </div>

                    {/* Properties as child nodes */}
                    {isSelected && props.length > 0 && (
                      <div style={{ paddingLeft: 44 }}>
                        {props.map((p) => (
                          <div key={p.key} style={{
                            display: 'flex',
                            gap: 8,
                            padding: '1px 8px',
                            fontSize: 11,
                            color: 'var(--color-text-muted)',
                          }}>
                            <span style={{ width: 60 }}>{p.key}:</span>
                            <span style={{ fontFamily: 'var(--font-mono)' }}>{p.value}</span>
                          </div>
                        ))}
                      </div>
                    )}

                    {/* Delete button when selected */}
                    {isSelected && (
                      <div style={{ paddingLeft: 44, paddingBottom: 2 }}>
                        <button
                          onClick={(e) => { e.stopPropagation(); handleDeleteObject(obj.id); }}
                          style={{
                            background: 'none',
                            border: 'none',
                            color: 'var(--color-danger)',
                            cursor: 'pointer',
                            fontSize: 11,
                            padding: '1px 4px',
                          }}
                        >
                          删除 Delete
                        </button>
                      </div>
                    )}
                  </div>
                );
              })}
            </div>
          );
        })}
      </div>
    </div>
  );
};
