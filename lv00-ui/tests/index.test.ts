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
      collectDelta: () => ({ records: [], fr