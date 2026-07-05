import React, { useState } from 'react';
import { useGeometryStore, GeoObject, GeoObjectType } from '../../L5-core/store/geometryStore';
import { Empty } from '../shared';

interface TypeGroup { type: GeoObjectType; label: string; icon: string; }
const TYPE_GROUPS: TypeGroup[] = [
  { type: 'point', label: 'Points', icon: '\u25CF' },
  { type: 'segment', label: 'Segments', icon: '\u2501' },
  { type: 'circle', label: 'Circles', icon: '\u25CB' },
  { type: 'line', label: 'Lines', icon: '\u2571' },
  { type: 'ray', label: 'Rays', icon: '\u2192' },
  { type: 'arc', label: 'Arcs', icon: '\u25E0' },
  { type: 'polygon', label: 'Polygons', icon: '\u25B3' },
  { type: 'angle', label: 'Angles', icon: '\u2220' },
];

export const TreeView: React.FC = () => {
  const objects = useGeometryStore((s) => s.objects);
  const selectObjects = useGeometryStore((s) => s.selectObjects);
  const removeObject = useGeometryStore((s) => s.removeObject);
  const addObject = useGeometryStore((s) => s.addObject);
  const selectedIds = useGeometryStore((s) => s.selectedIds);
  const [expandedGroups, setExpandedGroups] = useState<Set<string>>(() => new Set(TYPE_GROUPS.map((g) => g.type)));

  const toggleGroup = (type: string) => {
    setExpandedGroups((prev) => { const next = new Set(prev); if (next.has(type)) next.delete(type); else next.add(type); return next; });
  };

  const formatSubtitle = (obj: GeoObject): string => {
    if (obj.type === 'point') return '(' + (obj.x?.toFixed(0) ?? '?') + ', ' + (obj.y?.toFixed(0) ?? '?') + ')';
    if ((obj.type === 'segment' || obj.type === 'line') && obj.length !== undefined) return 'L=' + obj.length.toFixed(1);
    if (obj.type === 'circle' && obj.radius !== undefined) return 'R=' + obj.radius.toFixed(1);
    if (obj.type === 'polygon' && obj.area !== undefined) return 'S=' + obj.area.toFixed(1);
    return '';
  };

  const formatProperties = (obj: GeoObject): { key: string; value: string }[] => {
    const props: { key: string; value: string }[] = [];
    if (obj.type === 'point') { props.push({ key: 'X', value: String(obj.x ?? '?') }); props.push({ key: 'Y', value: String(obj.y ?? '?') }); }
    else if (obj.type === 'segment' || obj.type === 'line') {
      props.push({ key: 'Start', value: obj.startId ?? '?' }); props.push({ key: 'End', value: obj.endId ?? '?' });
      if (obj.length !== undefined) props.push({ key: 'Length', value: obj.length.toFixed(1) });
    } else if (obj.type === 'circle') {
      props.push({ key: 'Center', value: obj.centerId ?? '?' }); props.push({ key: 'RadiusPt', value: obj.radiusPointId ?? '?' });
      if (obj.radius !== undefined) props.push({ key: 'Radius', value: obj.radius.toFixed(1) });
    }
    return props;
  };

  if (!objects.length) return <Empty msg="No geometry objects" icon={'\u{1F33F}'} />;

  return (
    <div style={{ width: '100%', height: '100%', display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
      <div style={{ padding: '4px 8px', borderBottom: '1px solid var(--color-border-secondary)', display: 'flex', gap: 4, flexShrink: 0 }}>
        <button className="btn btn-small" onClick={() => { addObject({ type: 'point', label: '', x: 200, y: 200, visible: true }); }}>+ Point</button>
        <button className="btn btn-small" onClick={() => setExpandedGroups(new Set())}>Collapse</button>
        <button className="btn btn-small" onClick={() => setExpandedGroups(new Set(TYPE_GROUPS.map((g) => g.type)))}>Expand</button>
      </div>
      <div style={{ flex: 1, overflow: 'auto', padding: '4px 0' }}>
        {TYPE_GROUPS.map((group) => {
          const groupObjects = objects.filter((o) => o.type === group.type);
          if (groupObjects.length === 0) return null;
          const isExpanded = expandedGroups.has(group.type);
          return (
            <div key={group.type}>
              <div onClick={() => toggleGroup(group.type)}
                style={{ display: 'flex', alignItems: 'center', cursor: 'pointer', padding: '5px 8px', fontWeight: 600, fontSize: 12, color: 'var(--color-text-secondary)', gap: 6, userSelect: 'none' }}>
                <span style={{ fontSize: 10, width: 12 }}>{isExpanded ? '\u25BC' : '\u25B6'}</span>
                <span>{group.icon}</span>
                <span>{group.label}</span>
                <span style={{ fontSize: 10, color: 'var(--color-text-muted)' }}>({groupObjects.length})</span>
              </div>
              {isExpanded && groupObjects.map((obj) => {
                const isSelected = selectedIds.includes(obj.id);
                const props = formatProperties(obj);
                return (
                  <div key={obj.id}>
                    <div onClick={() => selectObjects([obj.id])}
                      style={{ display: 'flex', alignItems: 'center', cursor: 'pointer', padding: '3px 8px 3px 28px', fontSize: 12, color: 'var(--color-text-primary)', gap: 6, background: isSelected ? 'rgba(0,188,212,0.08)' : 'transparent' }}>
                      <span style={{ width: 8, height: 8, borderRadius: '50%', background: obj.color, flexShrink: 0 }} />
                      <span style={{ fontWeight: 500 }}>{obj.label}</span>
                      <span style={{ color: 'var(--color-text-muted)', fontSize: 11 }}>{formatSubtitle(obj)}</span>
                      <span style={{ marginLeft: 'auto', fontSize: 11, color: 'var(--color-text-muted)' }}>{obj.id}</span>
                    </div>
                    {isSelected && props.length > 0 && (
                      <div style={{ paddingLeft: 44 }}>
                        {props.map((p) => (
                          <div key={p.key} style={{ display: 'flex', gap: 8, padding: '1px 8px', fontSize: 11, color: 'var(--color-text-muted)' }}>
                            <span style={{ width: 60 }}>{p.key}:</span>
                            <span style={{ fontFamily: 'var(--font-mono)' }}>{p.value}</span>
                          </div>
                        ))}
                      </div>
                    )}
                    {isSelected && (
                      <div style={{ paddingLeft: 44, paddingBottom: 2 }}>
                        <button onClick={(e) => { e.stopPropagation(); removeObject(obj.id); }}
                          style={{ background: 'none', border: 'none', color: 'var(--color-danger)', cursor: 'pointer', fontSize: 11 }}>Delete</button>
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
