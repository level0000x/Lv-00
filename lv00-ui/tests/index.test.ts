// ============================================================
// lv00-ui/tests — 集成测试套件
// 测试协议、Mock内核、SceneController、各模态数据投影
// 使用 vitest 运行: npx vitest run
// ============================================================

import { describe, it, expect, beforeEach } from 'vitest';

// ---- 直接内联测试（不依赖 vitest 导入路径） ----
// 运行: npx vitest run tests/

import {
  CommandType, makeAddNode, makeMoveNode, makeNormalize,
  coordRational, coordFromDouble, coordToString, coordToDouble,
  intToRGBA,
} from '@lv00/protocol';

import {
  graphCreate, addNode, moveNode, getNode, getAllNodes,
  execute, normalize, snapshot, restore, loadDemoGeometry,
} from '@lv00/mock-kernel';

import {
  SceneController, createSceneControllerWithMock,
} from '@lv00/scene-controller';

// ---- Mock 内核测试 ----

describe('Mock Kernel', () => {
  beforeEach(() => graphCreate());

  it('graphCreate 初始化空图', () => {
    expect(getAllNodes()).toHaveLength(0);
  });

  it('addNode 创建点', () => {
    const id = addNode({ name: 'A', coords: [coordRational(100), coordRational(200)] });
    expect(id).toBeGreaterThan(0);
    const node = getNode(id);
    expect(node).toBeDefined();
    expect(node!.name).toBe('A');
  });

  it('moveNode 移动点', () => {
    const id = addNode({ name: 'P', coords: [coordRational(0), coordRational(0)] });
    moveNode(id, [coordRational(50), coordRational(60)]);
    const node = getNode(id);
    expect(coordToDouble(node!.coords[0])).toBe(50);
  });

  it('normalize 合并重复坐标节点', () => {
    addNode({ name: 'A', coords: [coordRational(100), coordRational(100)] });
    addNode({ name: 'B', coords: [coordRational(100), coordRational(100)] });
    expect(getAllNodes()).toHaveLength(2);
    const pairs = normalize(false);
    expect(pairs.length).toBe(1);
    expect(getAllNodes()).toHaveLength(1);
  });

  it('snapshot/restore 快照恢复', () => {
    addNode({ name: 'X', coords: [coordRational(10), coordRational(20)] });
    const sid = snapshot();
    addNode({ name: 'Y', coords: [coordRational(30), coordRational(40)] });
    expect(getAllNodes()).toHaveLength(2);
    restore(sid);
    expect(getAllNodes()).toHaveLength(1);
    expect(getAllNodes()[0].name).toBe('X');
  });

  it('execute MOVE_NODE 命令', () => {
    const id = addNode({ name: 'M', coords: [coordRational(0), coordRational(0)] });
    const result = execute(makeMoveNode(id, coordRational(42)));
    expect(result.success).toBe(true);
    expect(coordToDouble(getNode(id)!.coords[0])).toBe(42);
  });

  it('execute NORMALIZE 命令', () => {
    addNode({ name: 'A', coords: [coordRational(1), coordRational(2)] });
    addNode({ name: 'B', coords: [coordRational(1), coordRational(2)] });
    const result = execute(makeNormalize(false));
    expect(result.success).toBe(true);
    expect(getAllNodes()).toHaveLength(1);
  });

  it('loadDemoGeometry 创建演示图形', () => {
    loadDemoGeometry();
    const nodes = getAllNodes();
    expect(nodes.length).toBeGreaterThanOrEqual(4);
    const A = nodes.find(n => n.name === 'A');
    expect(A).toBeDefined();
  });
});

// ---- SceneController 测试 ----

describe('SceneController', () => {
  let scene: SceneController;

  beforeEach(() => {
    graphCreate();
    scene = createSceneControllerWithMock({
      execute,
      collectDelta: () => ({ records: [], from_seq: 0, to_seq: 0 }),
      getAllNodes,
      getAllConstraints: () => [],
      loadDemoGeometry,
    });
  });

  it('syncFromKernel 同步节点数据', () => {
    // createSceneControllerWithMock 已加载演示数据
    expect(scene.getNodes().length).toBeGreaterThanOrEqual(4);
  });

  it('selectNode / getSelection 选择管理', () => {
    loadDemoGeometry();
    scene.syncFromKernel();
    const nodes = scene.getNodes();
    expect(nodes.length).toBeGreaterThan(0);

    scene.selectNode(nodes[0].id);
    expect(scene.getSelection()).toEqual([nodes[0].id]);

    scene.clearSelection();
    expect(scene.getSelection()).toHaveLength(0);
  });

  it('getDrawCommands 生成绘制指令', () => {
    loadDemoGeometry();
    scene.syncFromKernel();
    const commands = scene.getDrawCommands();
    expect(commands.length).toBeGreaterThan(0);
    const points = commands.filter(c => c.type === 'POINT');
    expect(points.length).toBeGreaterThan(0);
  });

  it('getTextRepresentation 导出文本', () => {
    loadDemoGeometry();
    scene.syncFromKernel();
    const text = scene.getTextRepresentation();
    expect(text).toContain('point');
    expect(text).toContain('A');
  });

  it('getNodesAsTable 导出表格数据', () => {
    loadDemoGeometry();
    scene.syncFromKernel();
    const rows = scene.getNodesAsTable();
    expect(rows.length).toBeGreaterThan(0);
    expect(rows[0]).toHaveProperty('id');
    expect(rows[0]).toHaveProperty('coordX');
  });

  it('getCommandCompletions 命令补全', () => {
    loadDemoGeometry();
    scene.syncFromKernel();
    const comps = scene.getCommandCompletions('add');
    expect(comps.length).toBeGreaterThan(0);
    expect(comps).toContain('add point');
  });

  it('getDependencyTree 创建树', () => {
    const tree = scene.getDependencyTree();
    expect(tree.id).toBe('__root__');
  });

  it('getTopologyView 导出拓扑图', () => {
    const topo = scene.getTopologyView();
    expect(topo).toHaveProperty('blocks');
    expect(topo).toHaveProperty('edges');
  });

  it('视口变换 worldToScreen / screenToWorld 互逆', () => {
    const vp = scene.getViewport();
    const { sx, sy } = scene.worldToScreen(100, 200);
    const { wx, wy } = scene.screenToWorld(sx, sy);
    expect(Math.abs(wx - 100)).toBeLessThan(0.01);
    expect(Math.abs(wy - 200)).toBeLessThan(0.01);
  });
});

// ---- 协议工具函数测试 ----

describe('Protocol Utils', () => {
  it('coordFromDouble 生成有理坐标', () => {
    const c = coordFromDouble(3.14);
    expect(c.type).toBe('SYM_RATIONAL');
    expect(c.den).toBe(1000000);
    expect(Math.abs(c.num / c.den - 3.14)).toBeLessThan(0.00001);
  });

  it('coordToString 有理数', () => {
    expect(coordToString(coordRational(3, 4))).toBe('3/4');
    expect(coordToString(coordRational(5))).toBe('5');
  });

  it('coordToString 二次根式', () => {
    const s = coordToString({ type: 'SYM_QUADRATIC', a: 1, b: 2, n: 3 });
    expect(s).toBe('1+2√3');
  });

  it('makeAddNode 构造命令', () => {
    const cmd = makeAddNode({ name: 'P1', coords: [coordRational(1), coordRational(2)] });
    expect(cmd.type).toBe(CommandType.ADD_NODE);
    expect(cmd.blob).toBeDefined();
  });

  it('intToRGBA 颜色转换', () => {
    const rgba = intToRGBA(0xFF22C55E);
    expect(rgba).toContain('rgba');
    expect(rgba).toContain('34');
  });
});
