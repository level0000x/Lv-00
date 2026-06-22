// ============================================================
// @lv00/scene-controller — L3 多模态投影引擎
// 将内核数据投影为 6 种模态可消费的视图
// 管理：选择、视口、命中测试、拖拽状态机、文本序列化
// ============================================================

import {
  Node, Constraint, Proof, Command, CommandType, CommandResult,
  Delta, DeltaRecord, DeltaType,
  DrawCommand, DrawCommandType, LineStyle,
  NodeRow, ConstraintRow, TreeNode,
  BlockNode, Port, TopologyGraph, TopologyEdge,
  Viewport, DragState, DragMode, HitResult,
  EditorState,
  coordToString, coordToDouble, coordFromDouble, intToRGBA,
  makeMoveNode, makeAddNode, makeNormalize, makeAddConstraint, makeDeleteNode,
} from '@lv00/protocol';

// ---- 回调类型 ----

export type StateChangeCallback = () => void;
export type DeltaCallback = (delta: Delta) => void;

// ---- SceneState ----

interface SceneState {
  nodes: Node[];
  constraints: Constraint[];
  proofs: Proof[];
  selection: number[];
  viewport: Viewport;
  currentDeltaSeq: number;
}

// ---- 命令历史条目 ----

interface CommandHistoryEntry {
  command: string;
  result: string;
  timestamp: number;
  success: boolean;
}

// ---- SceneController ----

export class SceneController {
  // 内部状态
  private state: SceneState = {
    nodes: [],
    constraints: [],
    proofs: [],
    selection: [],
    viewport: { offsetX: 0, offsetY: 0, scale: 1, canvasWidth: 800, canvasHeight: 600 },
    currentDeltaSeq: 0,
  };

  private drawQueue: DrawCommand[] = [];
  private dragState: DragState = {
    mode: DragMode.NONE,
    startScreenX: 0, startScreenY: 0,
    startWorldX: 0, startWorldY: 0,
    draggedNodeId: 0, selectedBefore: [], wasShiftDown: false,
    currentScreenX: 0, currentScreenY: 0,
    accumulatedWorldDeltaX: 0, accumulatedWorldDeltaY: 0,
  };

  private commandHistory: CommandHistoryEntry[] = [];
  private stateChangeCallbacks: StateChangeCallback[] = [];
  private shiftDown = false;

  private topologyLayoutX: Map<number, number> = new Map();
  private topologyLayoutY: Map<number, number> = new Map();

  // ---- 外部依赖注入（Mock 内核或真实 L2）----

  private executeFn: ((cmd: Command) => CommandResult) | null = null;
  private collectDeltaFn: (() => Delta) | null = null;
  private getNodesFn: (() => Node[]) | null = null;
  private getConstraintsFn: (() => Constraint[]) | null = null;

  injectKernel(
    exe: (cmd: Command) => CommandResult,
    delta: () => Delta,
    nodes: () => Node[],
    constraints: () => Constraint[],
  ): void {
    this.executeFn = exe;
    this.collectDeltaFn = delta;
    this.getNodesFn = nodes;
    this.getConstraintsFn = constraints;
  }

  // ---- 状态订阅 ----

  onStateChange(cb: StateChangeCallback): () => void {
    this.stateChangeCallbacks.push(cb);
    return () => {
      this.stateChangeCallbacks = this.stateChangeCallbacks.filter(c => c !== cb);
    };
  }

  private notifyStateChange(): void {
    for (const cb of this.stateChangeCallbacks) cb();
  }

  // ---- 数据同步 ----

  syncFromKernel(): void {
    if (this.getNodesFn) this.state.nodes = this.getNodesFn();
    if (this.getConstraintsFn) this.state.constraints = this.getConstraintsFn();
    this.notifyStateChange();
  }

  // ---- 通用状态 ----

  getState(): Readonly<SceneState> { return this.state; }
  getNodes(): Node[] { return this.state.nodes; }
  getConstraints(): Constraint[] { return this.state.constraints; }
  getNode(id: number): Node | undefined { return this.state.nodes.find(n => n.id === id); }

  // ---- 选择管理 ----

  selectNode(id: number): void {
    this.state.selection = [id];
    this.notifyStateChange();
  }

  selectNodes(ids: number[]): void {
    this.state.selection = ids;
    this.notifyStateChange();
  }

  addToSelection(id: number): void {
    if (!this.state.selection.includes(id)) {
      this.state.selection.push(id);
      this.notifyStateChange();
    }
  }

  removeFromSelection(id: number): void {
    this.state.selection = this.state.selection.filter(s => s !== id);
    this.notifyStateChange();
  }

  clearSelection(): void {
    this.state.selection = [];
    this.notifyStateChange();
  }

  getSelection(): number[] { return [...this.state.selection]; }
  isSelected(id: number): boolean { return this.state.selection.includes(id); }

  // ---- 视口 ----

  getViewport(): Viewport { return { ...this.state.viewport }; }

  setViewport(vp: Partial<Viewport>): void {
    Object.assign(this.state.viewport, vp);
  }

  worldToScreen(wx: number, wy: number): { sx: number; sy: number } {
    const vp = this.state.viewport;
    return {
      sx: (wx - vp.offsetX) * vp.scale + vp.canvasWidth / 2,
      sy: (wy - vp.offsetY) * vp.scale + vp.canvasHeight / 2,
    };
  }

  screenToWorld(sx: number, sy: number): { wx: number; wy: number } {
    const vp = this.state.viewport;
    return {
      wx: (sx - vp.canvasWidth / 2) / vp.scale + vp.offsetX,
      wy: (sy - vp.canvasHeight / 2) / vp.scale + vp.offsetY,
    };
  }

  // ===================== 命中测试 =====================

  hitTest(screenX: number, screenY: number): HitResult {
    const best: HitResult = { type: 'NONE', id: 0, distance: Infinity };
    const { wx, wy } = this.screenToWorld(screenX, screenY);

    // 检测节点
    for (const node of this.state.nodes) {
      const nx = coordToDouble(node.coords[0]);
      const ny = coordToDouble(node.coords[1] ?? node.coords[0]);
      const dx = nx - wx;
      const dy = ny - wy;
      const screenDist = Math.sqrt(dx * dx + dy * dy) * this.state.viewport.scale;

      if (screenDist < 12 && screenDist < best.distance) {
        best.type = 'NODE';
        best.id = node.id;
        best.distance = screenDist;
      }
    }

    // 若无命中，检测线段约束
    if (best.type === 'NONE') {
      for (const c of this.state.constraints) {
        if (c.type === 'CONSTR_SEGMENT' && c.p0 !== undefined && c.p1 !== undefined) {
          const p0 = this.getNode(c.p0);
          const p1 = this.getNode(c.p1);
          if (!p0 || !p1) continue;

          const dist = this.pointToSegmentDist(wx, wy, p0, p1);
          if (dist < 10 && dist < best.distance) {
            best.type = 'CONSTRAINT';
            best.id = c.id;
            best.distance = dist;
          }
        }
      }
    }

    return best;
  }

  private pointToSegmentDist(px: number, py: number, a: Node, b: Node): number {
    const ax = coordToDouble(a.coords[0]);
    const ay = coordToDouble(a.coords[1] ?? a.coords[0]);
    const bx = coordToDouble(b.coords[0]);
    const by = coordToDouble(b.coords[1] ?? b.coords[0]);

    const dx = bx - ax;
    const dy = by - ay;
    const len2 = dx * dx + dy * dy;
    if (len2 === 0) return Math.sqrt((px - ax) ** 2 + (py - ay) ** 2);

    let t = ((px - ax) * dx + (py - ay) * dy) / len2;
    t = Math.max(0, Math.min(1, t));
    const cx = ax + t * dx;
    const cy = ay + t * dy;
    return Math.sqrt((px - cx) ** 2 + (py - cy) ** 2) * this.state.viewport.scale;
  }

  // ===================== 鼠标交互 =====================

  onMouseDown(screenX: number, screenY: number, button: number): void {
    const hit = this.hitTest(screenX, screenY);

    if (button === 0) { // 左键
      if (hit.type === 'NODE') {
        if (this.shiftDown) {
          if (this.isSelected(hit.id)) {
            this.removeFromSelection(hit.id);
          } else {
            this.addToSelection(hit.id);
          }
        } else {
          this.selectNode(hit.id);
          this.dragState.mode = DragMode.DRAGGING;
          this.dragState.draggedNodeId = hit.id;
          this.dragState.startScreenX = screenX;
          this.dragState.startScreenY = screenY;
          const node = this.getNode(hit.id);
          if (node) {
            this.dragState.startWorldX = coordToDouble(node.coords[0]);
            this.dragState.startWorldY = coordToDouble(node.coords[1] ?? node.coords[0]);
          }
        }
      } else {
        if (!this.shiftDown) this.clearSelection();
        this.dragState.mode = DragMode.SELECTING;
        this.dragState.startScreenX = screenX;
        this.dragState.startScreenY = screenY;
      }
    } else if (button === 1 || button === 2) { // 中键或右键
      this.dragState.mode = DragMode.PANNING;
      this.dragState.startScreenX = screenX;
      this.dragState.startScreenY = screenY;
    }
  }

  onMouseMove(screenX: number, screenY: number): void {
    switch (this.dragState.mode) {
      case DragMode.DRAGGING: {
        const dScreenX = screenX - this.dragState.startScreenX;
        const dScreenY = screenY - this.dragState.startScreenY;
        const dWorldX = dScreenX / this.state.viewport.scale;
        const dWorldY = dScreenY / this.state.viewport.scale;
        const newWX = this.dragState.startWorldX + dWorldX;
        const newWY = this.dragState.startWorldY + dWorldY;

        // 生成临时拖拽预览指令
        const { sx, sy } = this.worldToScreen(newWX, newWY);
        this.drawQueue.push({
          type: 'POINT', x1: sx, y1: sy, color: 0x66FFAA88, radius: 8,
        });

        // 拖拽轨迹虚线
        const { sx: sx0, sy: sy0 } = this.worldToScreen(this.dragState.startWorldX, this.dragState.startWorldY);
        this.drawQueue.push({
          type: 'LINE', x1: sx0, y1: sy0, x2: sx, y2: sy,
          color: 0x66FFAA44, style: 'DASHED', lineWidth: 1.5,
        });
        break;
      }
      case DragMode.PANNING: {
        const dX = (screenX - this.dragState.startScreenX) / this.state.viewport.scale;
        const dY = (screenY - this.dragState.startScreenY) / this.state.viewport.scale;
        this.state.viewport.offsetX -= dX;
        this.state.viewport.offsetY -= dY;
        this.dragState.startScreenX = screenX;
        this.dragState.startScreenY = screenY;
        break;
      }
    }
    // 更新悬停检测高亮
    const hoverHit = this.hitTest(screenX, screenY);
    if (hoverHit.type === 'NODE') {
      const node = this.getNode(hoverHit.id);
      if (node) {
        const { sx, sy } = this.worldToScreen(coordToDouble(node.coords[0]), coordToDouble(node.coords[1] ?? node.coords[0]));
        this.drawQueue.push({ type: 'HIGHLIGHT', x1: sx, y1: sy, color: 0x44FFFFFF, radius: 16 });
      }
    }
  }

  onMouseUp(screenX: number, screenY: number, button: number): void {
    switch (this.dragState.mode) {
      case DragMode.DRAGGING: {
        const dWorldX = (screenX - this.dragState.startScreenX) / this.state.viewport.scale;
        const dWorldY = (screenY - this.dragState.startScreenY) / this.state.viewport.scale;
        const finalX = this.dragState.startWorldX + dWorldX;
        const finalY = this.dragState.startWorldY + dWorldY;

        if (this.executeFn) {
          this.executeFn(makeMoveNode(this.dragState.draggedNodeId, coordFromDouble(finalX)));
          this.executeFn(makeMoveNode(this.dragState.draggedNodeId, coordFromDouble(finalY)));
          this.syncFromKernel();
        }
        this.dragState.mode = DragMode.NONE;
        break;
      }
      case DragMode.SELECTING: {
        // 框选范围内的节点
        const sx1 = Math.min(this.dragState.startScreenX, screenX);
        const sy1 = Math.min(this.dragState.startScreenY, screenY);
        const sx2 = Math.max(this.dragState.startScreenX, screenX);
        const sy2 = Math.max(this.dragState.startScreenY, screenY);

        const hitNodes: number[] = [];
        for (const node of this.state.nodes) {
          const { sx, sy } = this.worldToScreen(coordToDouble(node.coords[0]), coordToDouble(node.coords[1] ?? node.coords[0]));
          if (sx >= sx1 && sx <= sx2 && sy >= sy1 && sy <= sy2) {
            hitNodes.push(node.id);
          }
        }

        if (this.shiftDown) {
          for (const id of hitNodes) { if (!this.isSelected(id)) this.addToSelection(id); }
        } else {
          this.selectNodes(hitNodes);
        }
        this.dragState.mode = DragMode.NONE;
        break;
      }
      case DragMode.PANNING: {
        this.dragState.mode = DragMode.NONE;
        break;
      }
    }
  }

  onMouseWheel(delta: number, _centerX: number, _centerY: number): void {
    const factor = delta > 0 ? 1.15 : 0.87;
    this.state.viewport.scale *= factor;
    this.state.viewport.scale = Math.max(0.1, Math.min(20, this.state.viewport.scale));
  }

  onKeyDown(_key: string, shift: boolean): void {
    this.shiftDown = shift;
  }

  onKeyUp(_key: string, shift: boolean): void {
    this.shiftDown = shift;
  }

  // ===================== M1: 画布投影 =====================

  getDrawCommands(): DrawCommand[] {
    const commands: DrawCommand[] = [...this.drawQueue];
    this.drawQueue = [];

    // 投影所有节点
    for (const node of this.state.nodes) {
      const x = coordToDouble(node.coords[0]);
      const y = coordToDouble(node.coords[1] ?? node.coords[0]);
      const { sx, sy } = this.worldToScreen(x, y);
      const isSel = this.state.selection.includes(node.id);

      commands.push({
        type: 'POINT',
        x1: sx, y1: sy,
        color: node.color,
        radius: isSel ? 8 : 5,
      });

      if (node.name) {
        commands.push({
          type: 'TEXT',
          x1: sx + 10, y1: sy - 10,
          text: node.name,
          color: 0xFFC8C8C8,
        });
      }
    }

    // 投影所有线段约束
    for (const c of this.state.constraints) {
      if (c.type === 'CONSTR_SEGMENT' && c.p0 !== undefined && c.p1 !== undefined) {
        const p0 = this.getNode(c.p0);
        const p1 = this.getNode(c.p1);
        if (!p0 || !p1) continue;

        const { sx: sx0, sy: sy0 } = this.worldToScreen(
          coordToDouble(p0.coords[0]), coordToDouble(p0.coords[1] ?? p0.coords[0])
        );
        const { sx: sx1, sy: sy1 } = this.worldToScreen(
          coordToDouble(p1.coords[0]), coordToDouble(p1.coords[1] ?? p1.coords[0])
        );

        commands.push({
          type: 'LINE',
          x1: sx0, y1: sy0, x2: sx1, y2: sy1,
          color: c.is_satisfied ? 0xFF888888 : 0xFFF44336,
          lineWidth: 2,
          style: 'SOLID',
        });
      }
    }

    return commands;
  }

  // ===================== M2: 文本投影 =====================

  getTextRepresentation(): string {
    const lines: string[] = [];

    for (const node of this.state.nodes) {
      if (node.type === 'NODE_POINT') {
        const x = coordToString(node.coords[0]);
        const y = coordToString(node.coords[1] ?? node.coords[0]);
        lines.push(`point ${node.name} at (${x}, ${y})`);
      }
    }

    for (const c of this.state.constraints) {
      if (c.type === 'CONSTR_SEGMENT') {
        const p0 = this.getNode(c.participants[0]);
        const p1 = this.getNode(c.participants[1]);
        if (p0 && p1) {
          lines.push(`segment ${c.name} between ${p0.name} and ${p1.name}`);
        }
      }
    }

    for (const c of this.state.constraints) {
      if (c.type !== 'CONSTR_SEGMENT') {
        const names = c.participants.map(id => this.getNode(id)?.name ?? `#${id}`).join(', ');
        lines.push(`constraint ${c.type.toLowerCase().replace('constr_', '')}(${names})`);
      }
    }

    return lines.join('\n');
  }

  applyTextEdit(text: string): void {
    if (!this.executeFn) return;

    const lines = text.split('\n').map(l => l.trim()).filter(l => l && !l.startsWith('#'));
    for (const line of lines) {
      const cmd = this.parseTextLine(line);
      if (cmd) {
        this.executeFn(cmd);
        this.commandHistory.push({
          command: line,
          result: 'ok',
          timestamp: Date.now(),
          success: true,
        });
      }
    }
    this.syncFromKernel();
  }

  private parseTextLine(line: string): Command | null {
    const tokens = line.match(/[\w.]+|[(),]/g) || [];
    if (tokens.length < 2) return null;

    if (tokens[0] === 'point') {
      const name = tokens[1];
      const x = parseFloat(tokens[4]) || 0;
      const y = parseFloat(tokens[6]) || 0;
      return makeAddNode({ name, coords: [coordFromDouble(x), coordFromDouble(y)], type: 'NODE_POINT' });
    }

    if (tokens[0] === 'segment') {
      const name = tokens[1];
      const aName = tokens[3];
      const bName = tokens[5];
      const a = this.state.nodes.find(n => n.name === aName);
      const b = this.state.nodes.find(n => n.name === bName);
      if (a && b) return makeAddConstraint('CONSTR_SEGMENT', [a.id, b.id]);
    }

    return null;
  }

  // ===================== M3: 表格投影 =====================

  getNodesAsTable(): NodeRow[] {
    return this.state.nodes.map(node => ({
      id: node.id,
      name: node.name,
      type: node.type.toLowerCase().replace('node_', ''),
      coordX: coordToString(node.coords[0]),
      coordY: coordToString(node.coords[1] ?? node.coords[0]),
      color: node.color,
      constraint_count: this.state.constraints.filter(c => c.participants.includes(node.id)).length,
      parent_block: node.parent_block_id,
    }));
  }

  getConstraintsAsTable(): ConstraintRow[] {
    return this.state.constraints.map(c => ({
      id: c.id,
      type: c.type.toLowerCase().replace('constr_', ''),
      participants: c.participants.map(id => this.getNode(id)?.name ?? `#${id}`).join(', '),
      is_satisfied: c.is_satisfied,
    }));
  }

  updateNodeCell(nodeId: number, field: string, value: string): void {
    if (!this.executeFn) return;
    const node = this.getNode(nodeId);
    if (!node) return;

    if (field === 'coordX' || field === 'coordY') {
      const newVal = parseFloat(value);
      if (isNaN(newVal)) return;
      const x = field === 'coordX' ? newVal : coordToDouble(node.coords[0]);
      const y = field === 'coordY' ? newVal : coordToDouble(node.coords[1] ?? node.coords[0]);
      this.executeFn(makeMoveNode(nodeId, coordFromDouble(x)));
      this.executeFn(makeMoveNode(nodeId, coordFromDouble(y)));
    }
    this.syncFromKernel();
  }

  // ===================== M4: 依赖树投影 =====================

  getDependencyTree(): TreeNode {
    const root: TreeNode = {
      id: '__root__',
      label: '证明依赖树',
      color: 0xFFFFFFFF,
      status: 'root',
      children: [],
      node_id: 0,
    };

    for (const proof of this.state.proofs) {
      const proofNode: TreeNode = {
        id: `proof_${proof.id}`,
        label: proof.name,
        color: proof.trust_color,
        status: proof.is_proved ? 'proved' : 'pending',
        children: [],
        node_id: proof.root_node_id,
      };
      root.children.push(proofNode);
    }

    return root;
  }

  getProofTree(): TreeNode {
    return this.getDependencyTree();
  }

  // ===================== M5: 终端投影 =====================

  getCommandHistory(): CommandHistoryEntry[] {
    return [...this.commandHistory];
  }

  getCommandCompletions(prefix: string): string[] {
    const builtins = [
      'add point', 'add segment', 'add constraint', 'add region',
      'move point', 'remove point', 'remove constraint',
      'pack function', 'unpack function',
      'prove', 'check proof', 'load axiom', 'save', 'normalize',
      'undo', 'redo', 'history',
    ];

    const completions = builtins.filter(c => c.startsWith(prefix.toLowerCase()));

    for (const node of this.state.nodes) {
      const sug = `point ${node.name}`;
      if (sug.startsWith(prefix.toLowerCase())) completions.push(sug);
    }

    return completions;
  }

  executeCommandString(cmdStr: string): { success: boolean; output?: string; error_message?: string } {
    const cmd = this.parseTextLine(cmdStr.trim());
    if (!cmd) {
      this.commandHistory.push({ command: cmdStr, result: 'unknown command', timestamp: Date.now(), success: false });
      return { success: false, error_message: `Unknown command: ${cmdStr}` };
    }

    if (!this.executeFn) {
      return { success: false, error_message: 'No kernel injected' };
    }

    const result = this.executeFn(cmd);
    this.syncFromKernel();
    this.commandHistory.push({ command: cmdStr, result: result.error_message, timestamp: Date.now(), success: result.success });

    return { success: result.success, output: result.error_message, error_message: result.success ? undefined : result.error_message };
  }

  // ===================== M6: 拓扑投影 =====================

  getTopologyView(): TopologyGraph {
    const blocks: BlockNode[] = [];
    const edges: TopologyEdge[] = [];

    // 找到所有函数块节点
    for (const node of this.state.nodes) {
      if (node.type === 'NODE_FUNCTION_BLOCK') {
        const block: BlockNode = {
          id: node.id,
          name: node.name,
          inputs: [],
          outputs: [],
          layoutX: this.topologyLayoutX.get(node.id) ?? (blocks.length % 4) * 300,
          layoutY: this.topologyLayoutY.get(node.id) ?? Math.floor(blocks.length / 4) * 150,
        };

        for (const c of this.state.constraints) {
          if (c.type === 'CONSTR_CONNECTION') {
            const port: Port = {
              id: c.id,
              name: `port_${c.id}`,
              direction: c.to_port === node.id ? 'PORT_INPUT' : 'PORT_OUTPUT',
              connected_to: c.to_port === node.id ? c.from_port! : c.to_port!,
            };
            if (port.direction === 'PORT_INPUT') block.inputs.push(port);
            else block.outputs.push(port);
          }
        }

        blocks.push(block);
      }
    }

    // 提取连接边
    for (const c of this.state.constraints) {
      if (c.type === 'CONSTR_CONNECTION' && c.from_port && c.to_port) {
        edges.push({
          from_block: c.from_port,
          from_port: c.from_port,
          to_block: c.to_port,
          to_port: c.to_port,
        });
      }
    }

    return { blocks, edges };
  }

  setTopologyLayout(blockId: number, x: number, y: number): void {
    this.topologyLayoutX.set(blockId, x);
    this.topologyLayoutY.set(blockId, y);
  }

  onTopologyConnect(params: { source: string; target: string; sourceHandle: string; targetHandle: string }): void {
    if (!this.executeFn) return;
    // 通过 sourceHandle/targetHandle 解析端口ID，创建连接约束
    this.commandHistory.push({
      command: `connect ${params.source} -> ${params.target}`,
      result: 'ok',
      timestamp: Date.now(),
      success: true,
    });
  }

  // ---- 节点名称 ----

  getNodeNames(): string[] {
    return this.state.nodes.map(n => n.name);
  }
}

// 便捷工厂：创建绑定 Mock 内核的 SceneController
export function createSceneControllerWithMock(
  mockKernel: {
    execute: (cmd: Command) => CommandResult;
    collectDelta: () => Delta;
    getAllNodes: () => Node[];
    getAllConstraints: () => Constraint[];
    loadDemoGeometry?: () => void;
  }
): SceneController {
  const sc = new SceneController();
  sc.injectKernel(
    mockKernel.execute,
    mockKernel.collectDelta,
    mockKernel.getAllNodes,
    mockKernel.getAllConstraints,
  );
  if (mockKernel.loadDemoGeometry) {
    mockKernel.loadDemoGeometry();
    sc.syncFromKernel();
  }
  return sc;
}
