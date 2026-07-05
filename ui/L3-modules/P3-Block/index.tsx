import React, { useMemo } from 'react';
import { useGeometryStore, GeoObject } from '../../L5-core/store/geometryStore';

/* ── block data model ── */

interface BlockDef {
  id: string;
  name: string;
  nameZh: string;
  icon: string;
  inputs: string[];
  outputs: string[];
  description: string;
}

const PortDot: React.FC<{ color: string; label: string }> = ({ color, label }) => (
  <span style={{ display: 'inline-flex', alignItems: 'center', gap: 3, fontSize: 'var(--font-size-xs, 11px)' }}>
    <span style={{ width: 7, height: 7, borderRadius: '50%', background: color, flexShrink: 0 }} />
    <span style={{ color: 'var(--color-text-secondary)' }}>{label}</span>
  </span>
);

/* ── helpers to derive blocks from store objects ── */

function makeBlockFromObject(obj: GeoObject, allObjects: GeoObject[]): BlockDef {
  switch (obj.type) {
    case 'segment': {
      const start = allObjects.find((o) => o.id === obj.startId);
      const end = allObjects.find((o) => o.id === obj.endId);
      return {
        id: obj.id,
        name: `Segment(${start?.label ?? '?'}\u2192${end?.label ?? '?'})`,
        nameZh: `线段 ${obj.label}`,
        icon: '\u{1F4CF}',
        inputs: [
          start ? `\u8D77\u70B9 Start: ${start.label} (${start.x}, ${start.y})` : '起点 Start: ?',
          end ? `\u7EC8\u70B9 End: ${end.label} (${end.x}, ${end.y})` : '终点 End: ?',
        ],
        outputs: [
          obj.length ? `\u957F\u5EA6 Length: ${obj.length.toFixed(1)}px` : '长度 Length: ?',
        ],
        description: `\u7EBF\u6BB5\u8FDE\u63A5 ${start?.label ?? '?'} \u2192 ${end?.label ?? '?'} Segment connecting ${start?.label ?? '?'} to ${end?.label ?? '?'}`,
      };
    }
    case 'circle': {
      const center = allObjects.find((o) => o.id === obj.centerId);
      const rp = allObjects.find((o) => o.id === obj.radiusPointId);
      return {
        id: obj.id,
        name: `Circle(${center?.label ?? '?'})`,
        nameZh: `圆 ${obj.label}`,
        icon: '\u{1F534}',
        inputs: [
          center ? `\u5706\u5FC3 Center: ${center.label} (${center.x}, ${center.y})` : '圆心 Center: ?',
          rp ? `\u534A\u5F84\u70B9 R.Point: ${rp.label}` : '半径点 R.Point: ?',
        ],
        outputs: [
          obj.radius ? `\u534A\u5F84 Radius: ${obj.radius.toFixed(1)}px` : '半径 Radius: ?',
          obj.radius ? `\u9762\u79EF Area: ${(Math.PI * obj.radius * obj.radius).toFixed(1)}px\u00B2` : '面积 Area: ?',
          obj.radius ? `\u5468\u957F Circum: ${(2 * Math.PI * obj.radius).toFixed(1)}px` : '周长 Circum: ?',
        ],
        description: `\u4EE5 ${center?.label ?? '?'} \u4E3A\u5706\u5FC3\u7684\u5706 Circle centered at ${center?.label ?? '?'}`,
      };
    }
    case 'line': {
      const p1 = allObjects.find((o) => o.id === obj.startId);
      const p2 = allObjects.find((o) => o.id === obj.endId);
      return {
        id: obj.id,
        name: `Line(${p1?.label ?? '?'}\u2192${p2?.label ?? '?'})`,
        nameZh: `直线 ${obj.label}`,
        icon: '\u{1F4D8}',
        inputs: [
          p1 ? `\u70B9 Point 1: ${p1.label}` : '点 Point 1: ?',
          p2 ? `\u70B9 Point 2: ${p2.label}` : '点 Point 2: ?',
        ],
        outputs: [
          obj.length ? `\u53C2\u8003\u957F Ref.Len: ${obj.length.toFixed(1)}px` : '参考长 Ref.Len: ?',
        ],
        description: `\u76F4\u7EBF\u901A\u8FC7 ${p1?.label ?? '?'} \u548C ${p2?.label ?? '?'} Line through ${p1?.label ?? '?'} and ${p2?.label ?? '?'}`,
      };
    }
    case 'ray': {
      const p1 = allObjects.find((o) => o.id === obj.startId);
      const p2 = allObjects.find((o) => o.id === obj.endId);
      return {
        id: obj.id,
        name: `Ray(${p1?.label ?? '?'}\u2192)`,
        nameZh: `射线 ${obj.label}`,
        icon: '\u{1F535}',
        inputs: [
          p1 ? `\u8D77\u70B9 Origin: ${p1.label}` : '起点 Origin: ?',
          p2 ? `\u65B9\u5411\u70B9 Dir: ${p2.label}` : '方向点 Dir: ?',
        ],
        outputs: [],
        description: `\u5C04\u7EBF\u4ECE ${p1?.label ?? '?'} \u7ECF\u8FC7 ${p2?.label ?? '?'}`,
      };
    }
    case 'polygon': {
      const vertexLabels = (obj.vertexIds ?? [])
        .map((vid) => allObjects.find((o) => o.id === vid)?.label ?? '?')
        .join(', ');
      return {
        id: obj.id,
        name: `Polygon(${vertexLabels})`,
        nameZh: `多边形 ${obj.label}`,
        icon: '\u{1F7E2}',
        inputs: [`\u9876\u70B9 Vertices: ${vertexLabels}`],
        outputs: [
          obj.area ? `\u9762\u79EF Area: ${obj.area.toFixed(1)}px\u00B2` : '面积 Area: ?',
        ],
        description: `\u591A\u8FB9\u5F62 Polygon with ${obj.vertexIds?.length ?? 0} vertices`,
      };
    }
    case 'arc': {
      return {
        id: obj.id,
        name: `Arc ${obj.label}`,
        nameZh: `弧 ${obj.label}`,
        icon: '\u{1F535}',
        inputs: ['参数 Parameters'],
        outputs: [obj.angle ? `\u89D2\u5EA6 Angle: ${obj.angle.toFixed(1)}\u00B0` : '角度 Angle: ?'],
        description: `\u5706\u5F27 Arc`,
      };
    }
    case 'angle': {
      return {
        id: obj.id,
        name: `Angle ${obj.label}`,
        nameZh: `角 ${obj.label}`,
        icon: '\u{1F4D0}',
        inputs: ['参数 Parameters'],
        outputs: [obj.angle ? `\u89D2\u5EA6 Angle: ${obj.angle.toFixed(1)}\u00B0` : '角度 Angle: ?'],
        description: `\u89D2\u5EA6\u5EA6\u91CF Angle measurement`,
      };
    }
    default:
      return {
        id: obj.id,
        name: `${obj.type} ${obj.label}`,
        nameZh: `${obj.type} ${obj.label}`,
        icon: '\u{1F4E6}',
        inputs: [],
        outputs: [],
        description: `${obj.type} object`,
      };
  }
}

function makeSceneBlock(objects: GeoObject[]): BlockDef {
  const points = objects.filter((o) => o.type === 'point');
  const nonPoints = objects.filter((o) => o.type !== 'point');

  const pointLabels = points.map((p) => `${p.label}(${p.x}, ${p.y})`).join(', ') || '(none 无)';

  const measurements: string[] = [];
  for (const o of nonPoints) {
    if (o.type === 'segment' && o.length) measurements.push(`${o.label}: L=${o.length.toFixed(1)}`);
    if (o.type === 'circle' && o.radius) measurements.push(`${o.label}: R=${o.radius.toFixed(1)}`);
    if (o.type === 'polygon' && o.area) measurements.push(`${o.label}: A=${o.area.toFixed(1)}`);
  }

  return {
    id: '__scene__',
    name: 'Scene',
    nameZh: '场景概览',
    icon: '\u{1F3AF}',
    inputs: [
      `\u70B9 Points (${points.length}): ${pointLabels.length > 60 ? pointLabels.slice(0, 60) + '...' : pointLabels}`,
    ],
    outputs: measurements.length > 0
      ? measurements.slice(0, 4)
      : ['(no measurements 无测量值)'],
    description: `\u5168\u573A\u666F\u5EFA\u6A21 Full scene model \u2014 ${objects.length} objects, ${points.length} points, ${nonPoints.length} constructions`,
  };
}

/* ── BlockPanel ── */

export const BlockPanel: React.FC = () => {
  const objects = useGeometryStore((s) => s.objects);

  const blocks: BlockDef[] = useMemo(() => {
    if (objects.length === 0) return [];
    const nonPointBlocks = objects
      .filter((o) => o.type !== 'point')
      .map((o) => makeBlockFromObject(o, objects));
    const sceneBlock = makeSceneBlock(objects);
    return [...nonPointBlocks, sceneBlock];
  }, [objects]);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', gap: 8 }}>
      {/* header */}
      <div style={{ display: 'flex', gap: 8, justifyContent: 'space-between', alignItems: 'center' }}>
        <span style={{ fontSize: 'var(--font-size-sm, 12px)', color: 'var(--color-text-secondary)' }}>
          函数块 Function Blocks
        </span>
        <span style={{ fontSize: 'var(--font-size-xs, 11px)', color: 'var(--color-text-tertiary, #555)', fontFamily: 'var(--font-mono)' }}>
          {blocks.length} 块 blocks
        </span>
      </div>

      {/* block list */}
      <div style={{ flex: 1, overflowY: 'auto', display: 'flex', flexDirection: 'column', gap: 6 }}>
        {blocks.length === 0 && (
          <div style={{
            padding: '20px 10px', borderRadius: 6,
            border: '1px dashed var(--color-border-secondary)',
            textAlign: 'center',
            color: 'var(--color-text-tertiary, #555)',
            fontSize: 'var(--font-size-xs, 11px)',
          }}>
            暂无构造对象 No construction objects. Use terminal commands to add objects.
          </div>
        )}
        {blocks.map((block) => (
          <div
            key={block.id}
            style={{
              padding: '8px 10px',
              borderRadius: 6,
              border: block.id === '__scene__'
                ? '1px solid var(--color-module-block, #90caf9)'
                : '1px solid var(--color-border-secondary)',
              background: block.id === '__scene__'
                ? 'rgba(144, 202, 249, 0.08)'
                : 'var(--color-bg-primary)',
            }}
          >
            {/* block header */}
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: 6 }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                <span>{block.icon}</span>
                <span style={{ fontWeight: 600, fontSize: 'var(--font-size-sm, 12px)', color: 'var(--color-text-primary)' }}>
                  {block.nameZh} {block.name}
                </span>
              </div>
            </div>

            {/* description */}
            <div style={{ fontSize: 'var(--font-size-xs, 11px)', color: 'var(--color-text-tertiary, #555)', marginBottom: 6 }}>
              {block.description}
            </div>

            {/* ports */}
            <div style={{ display: 'flex', gap: 12 }}>
              <div style={{ display: 'flex', flexDirection: 'column', gap: 2 }}>
                <span style={{ fontSize: 'var(--font-size-xs, 10px)', color: 'var(--color-text-tertiary, #555)', textTransform: 'uppercase' }}>
                  输入 Input
                </span>
                {block.inputs.map((p) => <PortDot key={p} color="#81c784" label={p} />)}
              </div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: 2 }}>
                <span style={{ fontSize: 'var(--font-size-xs, 10px)', color: 'var(--color-text-tertiary, #555)', textTransform: 'uppercase' }}>
                  输出 Output
                </span>
                {block.outputs.map((p) => <PortDot key={p} color="#ffb74d" label={p} />)}
              </div>
            </div>
          </div>
        ))}
      </div>
    </div>
  );
};
