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
        const dWorldY = (screenY - this.dragState.startScreenY) /