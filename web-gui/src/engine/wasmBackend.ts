/**
 * @module engine/wasmBackend
 * @description WASM backend implementation for the Lv-00 geometry engine.
 *              Bridges the IBackend interface to the C engine compiled to WebAssembly
 *              via Emscripten. Handles memory management, string marshaling, and
 *              JSON parsing of structured return values.
 *
 *              WASM 后端实现，将 IBackend 接口桥接到通过 Emscripten 编译为
 *              WebAssembly 的 C 引擎。处理内存管理、字符串编组和结构化返回值的 JSON 解析。
 */

import type { IBackend } from './backend';
import { RATIONAL_DENOMINATOR } from '@/utils/constants';

// ================================================================
// WASM Module Type / WASM 模块类型
// ================================================================

/**
 * Minimal type definition for the Emscripten-generated WASM module.
 * Only declares the functions we actually call from WasmBackend.
 *
 * Emscripten 生成的 WASM 模块的最低类型定义。
 * 仅声明 WasmBackend 实际调用的函数。
 */
export interface Lv00WasmModule {
  /** Allocate bytes on the WASM heap / 在 WASM 堆上分配字节 */
  _malloc(size: number): number;
  /** Free previously allocated WASM memory / 释放之前分配的 WASM 内存 */
  _free(ptr: number): void;
  /** Write a JS string to the WASM heap (null-terminated) / 将 JS 字符串写入 WASM 堆（以 null 结尾） */
  stringToUTF8(str: string, outPtr: number, maxBytesToWrite: number): number;
  /** Read a null-terminated string from the WASM heap / 从 WASM 堆读取以 null 结尾的字符串 */
  UTF8ToString(ptr: number): string;
  /** Get the byte length of a UTF-8 string on the WASM heap / 获取 WASM 堆上 UTF-8 字符串的字节长度 */
  lengthBytesUTF8(str: string): number;

  // ---- Graph Operations / 图操作 ----
  _web_graph_create(): number;
  _web_graph_destroy(graph: number): void;
  _web_graph_add_point(graph: number, x_num: number, x_den: number, y_num: number, y_den: number): number;
  _web_graph_add_line_segment(graph: number, p1: number, p2: number): number;
  _web_graph_add_region(graph: number, boundary_json: number): number;
  _web_graph_remove_node(graph: number, node_id: number): number;
  _web_graph_get_node_count(graph: number): number;
  _web_graph_get_constraint_count(graph: number): number;
  _web_graph_serialize_to_json(graph: number): number;
  _web_graph_deserialize_from_json(json: number): number;

  // ---- Constraint Operations / 约束操作 ----
  _web_graph_add_incidence(graph: number, point_id: number, segment_id: number): number;
  _web_graph_add_betweenness(graph: number, p1: number, p2: number, p3: number): number;
  _web_graph_add_intersection(graph: number, seg1: number, seg2: number, result_point_id: number): number;
  _web_graph_add_containment(graph: number, inner_id: number, outer_id: number): number;
  _web_graph_add_connection(graph: number, src: number, dst: number): number;

  // ---- Analysis Operations / 分析操作 ----
  _web_graph_normalize(graph: number): number;
  _web_find_merge_candidates(graph: number): number;
  _web_graph_detect_redundant(graph: number): number;
  _web_graph_detect_conflicts(graph: number): number;
  _web_count_degrees_of_freedom(graph: number): number;
  _web_graph_topological_sort(graph: number): number;
  _web_graph_hash(graph: number): number;

  // ---- Engine Operations / 引擎操作 ----
  _web_engine_create(): number;
  _web_engine_destroy(engine: number): void;
  _web_engine_solve(engine: number): number;
  _web_engine_rewrite_and_solve(engine: number, max_rewrite: number, max_solve: number): number;
  _web_engine_get_graph(engine: number): number;
  _web_engine_get_last_error(engine: number): number;

  // ---- Formula Operations / 公式操作 ----
  _web_formula_parse(input: number, syntax: number): number;
  _web_formula_render(ast_ptr: number, format: number): number;
  _web_formula_to_graph(ast_ptr: number, graph: number): number;
  _web_graph_to_formula(graph: number): number;
  _web_formula_node_destroy(ast_ptr: number): void;
  _web_formula_detect_syntax(input: number): number;

  // ---- Query Operations / 查询操作 ----
  _web_get_all_points(graph: number): number;
  _web_get_all_segments(graph: number): number;
  _web_get_all_constraints(graph: number): number;

  // ---- Coordinate Operations / 坐标操作 ----
  _web_coord_create_rational(num: number, den: number): number;
  _web_coord_destroy(coord: number): void;
  _web_coord_serialize(coord: number): number;

  // ---- Utility / 工具 ----
  _web_free_string(str: number): void;
  _web_get_version(): number;
}

// ================================================================
// WasmBackend Implementation / WASM 后端实现
// ================================================================

/**
 * WASM backend that delegates all geometry operations to the C engine
 * compiled to WebAssembly via Emscripten.
 *
 * WASM 后端，将所有几何操作委托给通过 Emscripten 编译为 WebAssembly 的 C 引擎。
 *
 * Memory management strategy:
 * - Strings passed to C are allocated on the WASM heap via _malloc + stringToUTF8,
 *   and freed after the C function returns.
 * - Strings returned from C (char*) are read via UTF8ToString and freed via _web_free_string.
 * - Graph handles are opaque pointers stored as JavaScript numbers.
 *
 * 内存管理策略：
 * - 传递给 C 的字符串通过 _malloc + stringToUTF8 在 WASM 堆上分配，
 *   在 C 函数返回后释放。
 * - 从 C 返回的字符串 (char*) 通过 UTF8ToString 读取，通过 _web_free_string 释放。
 * - 图句柄是不透明指针，存储为 JavaScript 数字。
 */
export class WasmBackend implements IBackend {
  readonly type = 'wasm' as const;

  /** The Emscripten WASM module instance / Emscripten WASM 模块实例 */
  private mod: Lv00WasmModule;

  constructor(mod: Lv00WasmModule) {
    this.mod = mod;
  }

  // ---- Memory helpers / 内存辅助函数 ----

  /**
   * Write a JS string to the WASM heap and return the pointer.
   * Caller MUST call freeWasmString() when done.
   *
   * 将 JS 字符串写入 WASM 堆并返回指针。
   * 调用方完成后必须调用 freeWasmString()。
   */
  private allocWasmString(str: string): number {
    const len = this.mod.lengthBytesUTF8(str) + 1; /* +1 for null terminator */
    const ptr = this.mod._malloc(len);
    if (ptr === 0) {
      throw new Error('[WasmBackend] Failed to allocate WASM memory for string');
    }
    this.mod.stringToUTF8(str, ptr, len);
    return ptr;
  }

  /**
   * Free a previously allocated WASM string pointer.
   *
   * 释放之前分配的 WASM 字符串指针。
   */
  private freeWasmString(ptr: number): void {
    if (ptr !== 0) {
      this.mod._free(ptr);
    }
  }

  /**
   * Read a C string (char*) from the WASM heap and free it.
   * Returns an empty string if the pointer is null/0.
   *
   * 从 WASM 堆读取 C 字符串 (char*) 并释放。
   * 如果指针为 null/0 则返回空字符串。
   */
  private readAndFreeCString(ptr: number): string {
    if (ptr === 0) return '';
    const str = this.mod.UTF8ToString(ptr);
    this.mod._web_free_string(ptr);
    return str;
  }

  /**
   * Read a C string from WASM heap, parse as JSON, and free.
   * Returns null if the pointer is null/0 or JSON parsing fails.
   *
   * 从 WASM 堆读取 C 字符串，解析为 JSON，并释放。
   * 如果指针为 null/0 或 JSON 解析失败则返回 null。
   */
  private readJsonAndFree<T>(ptr: number): T | null {
    if (ptr === 0) return null;
    const str = this.mod.UTF8ToString(ptr);
    this.mod._web_free_string(ptr);
    try {
      return JSON.parse(str) as T;
    } catch {
      console.warn('[WasmBackend] Failed to parse JSON from C:', str);
      return null;
    }
  }

  // ---- Graph Lifecycle / 图生命周期 ----

  graphCreate(): number {
    return this.mod._web_graph_create();
  }

  graphDestroy(graphHandle: number): void {
    this.mod._web_graph_destroy(graphHandle);
  }

  // ---- Point Operations / 点操作 ----

  /**
   * Add a point with floating-point coordinates.
   * Converts float to rational (num/den) with denominator = 1000000 for precision.
   *
   * 添加浮点坐标的点。
   * 将浮点数转换为有理数 (num/den)，分母为 1000000 以保证精度。
   */
  graphAddPoint(graphHandle: number, x: number, y: number): number {
    const den = RATIONAL_DENOMINATOR;
    const xNum = Math.round(x * den);
    const yNum = Math.round(y * den);
    return this.mod._web_graph_add_point(graphHandle, xNum, den, yNum, den);
  }

  graphRemovePoint(graphHandle: number, pointId: number): boolean {
    return this.mod._web_graph_remove_node(graphHandle, pointId) === 0;
  }

  // ---- Segment Operations / 线段操作 ----

  graphAddLineSegment(graphHandle: number, p1: number, p2: number): number {
    return this.mod._web_graph_add_line_segment(graphHandle, p1, p2);
  }

  graphRemoveLineSegment(graphHandle: number, segmentId: number): boolean {
    return this.mod._web_graph_remove_node(graphHandle, segmentId) === 0;
  }

  // ---- Constraint Operations / 约束操作 ----

  graphAddIncidence(graphHandle: number, pointId: number, segmentId: number): number {
    return this.mod._web_graph_add_incidence(graphHandle, pointId, segmentId);
  }

  graphAddBetweenness(graphHandle: number, a: number, b: number, c: number): number {
    return this.mod._web_graph_add_betweenness(graphHandle, a, b, c);
  }

  graphAddIntersection(graphHandle: number, seg1: number, seg2: number): number {
    /* 相交约束需要交点 ID，此处先创建一个临时点作为交点占位 */
    const intersectionPointId = this.graphAddPoint(graphHandle, 0, 0);
    if (intersectionPointId < 0) return -1;
    return this.mod._web_graph_add_intersection(
      graphHandle, seg1, seg2, intersectionPointId
    );
  }

  graphAddContainment(graphHandle: number, inner: number, outer: number): number {
    return this.mod._web_graph_add_containment(graphHandle, inner, outer);
  }

  graphAddConnection(graphHandle: number, elem1: number, elem2: number): number {
    return this.mod._web_graph_add_connection(graphHandle, elem1, elem2);
  }

  // ---- Analysis Operations / 分析操作 ----

  graphNormalize(graphHandle: number): boolean {
    const mergedCount = this.mod._web_graph_normalize(graphHandle);
    return mergedCount >= 0;
  }

  graphFindMergeCandidates(graphHandle: number): Array<{ a: number; b: number }> {
    const jsonPtr = this.mod._web_find_merge_candidates(graphHandle);
    const result = this.readJsonAndFree<Array<{ a: number; b: number }>>(jsonPtr);
    return result ?? [];
  }

  graphDetectRedundant(graphHandle: number): number[] {
    const jsonPtr = this.mod._web_graph_detect_redundant(graphHandle);
    const result = this.readJsonAndFree<number[]>(jsonPtr);
    return result ?? [];
  }

  graphDetectConflicts(graphHandle: number): Array<{ c1: number; c2: number }> {
    const jsonPtr = this.mod._web_graph_detect_conflicts(graphHandle);
    const raw = this.readJsonAndFree<number[][]>(jsonPtr);
    if (!raw) return [];

    /* 将冲突组数组转换为 {c1, c2} 对数组 */
    const conflicts: Array<{ c1: number; c2: number }> = [];
    for (const group of raw) {
      for (let i = 0; i < group.length - 1; i++) {
        for (let j = i + 1; j < group.length; j++) {
          conflicts.push({ c1: group[i] ?? 0, c2: group[j] ?? 0 });
        }
      }
    }
    return conflicts;
  }

  graphDegreesOfFreedom(graphHandle: number): number {
    return this.mod._web_count_degrees_of_freedom(graphHandle);
  }

  graphTopologicalSort(graphHandle: number): number[] {
    const jsonPtr = this.mod._web_graph_topological_sort(graphHandle);
    const result = this.readJsonAndFree<number[]>(jsonPtr);
    return result ?? [];
  }

  graphHash(graphHandle: number): string {
    const jsonPtr = this.mod._web_graph_hash(graphHandle);
    return this.readAndFreeCString(jsonPtr);
  }

  // ---- Coordinate Operations / 坐标操作 ----

  coordCreateRational(num: number, den: number): number {
    return this.mod._web_coord_create_rational(num, den);
  }

  // ================================================================
  //  Extended WASM-only operations / 扩展 WASM 专有操作
  // ================================================================

  /**
   * Serialize the graph to a JSON string.
   * / 将图序列化为 JSON 字符串。
   */
  graphSerializeToJson(graphHandle: number): string {
    const jsonPtr = this.mod._web_graph_serialize_to_json(graphHandle);
    return this.readAndFreeCString(jsonPtr);
  }

  /**
   * Deserialize a graph from a JSON string.
   * / 从 JSON 字符串反序列化图。
   */
  graphDeserializeFromJson(json: string): number {
    const ptr = this.allocWasmString(json);
    const graphPtr = this.mod._web_graph_deserialize_from_json(ptr);
    this.freeWasmString(ptr);
    return graphPtr;
  }

  /**
   * Add a region with boundary segment IDs.
   * / 添加由边界线段围成的区域。
   */
  graphAddRegion(graphHandle: number, boundarySegmentIds: number[]): number {
    const jsonStr = JSON.stringify(boundarySegmentIds);
    const ptr = this.allocWasmString(jsonStr);
    const regionId = this.mod._web_graph_add_region(graphHandle, ptr);
    this.freeWasmString(ptr);
    return regionId;
  }

  /**
   * Get all points as structured data.
   * / 获取所有点的结构化数据。
   */
  getAllPoints(graphHandle: number): Array<{ id: number; x: number; y: number; type: number }> {
    const jsonPtr = this.mod._web_get_all_points(graphHandle);
    const result = this.readJsonAndFree<Array<{ id: number; x: number; y: number; type: number }>>(jsonPtr);
    return result ?? [];
  }

  /**
   * Get all segments as structured data.
   * / 获取所有线段的结构化数据。
   */
  getAllSegments(graphHandle: number): Array<{ id: number; p1: number; p2: number }> {
    const jsonPtr = this.mod._web_get_all_segments(graphHandle);
    const result = this.readJsonAndFree<Array<{ id: number; p1: number; p2: number }>>(jsonPtr);
    return result ?? [];
  }

  /**
   * Get all constraints as structured data.
   * / 获取所有约束的结构化数据。
   */
  getAllConstraints(graphHandle: number): Array<{ id: number; type: string; args: number[] }> {
    const jsonPtr = this.mod._web_get_all_constraints(graphHandle);
    const result = this.readJsonAndFree<Array<{ id: number; type: string; args: number[] }>>(jsonPtr);
    return result ?? [];
  }

  /**
   * Parse a formula string and return the AST handle.
   * / 解析公式字符串并返回 AST 句柄。
   */
  formulaParse(input: string, syntax: number): { success: boolean; ast: number; error: string | null } {
    const ptr = this.allocWasmString(input);
    const jsonPtr = this.mod._web_formula_parse(ptr, syntax);
    this.freeWasmString(ptr);
    const result = this.readJsonAndFree<{ success: boolean; ast: number; error: string | null }>(jsonPtr);
    return result ?? { success: false, ast: 0, error: 'Failed to parse formula' };
  }

  /**
   * Render a formula AST to a string in the specified format.
   * / 将公式 AST 渲染为指定格式的字符串。
   */
  formulaRender(astHandle: number, format: number): string {
    const jsonPtr = this.mod._web_formula_render(astHandle, format);
    return this.readAndFreeCString(jsonPtr);
  }

  /**
   * Convert a formula AST to graph operations.
   * / 将公式 AST 转换为图操作。
   */
  formulaToGraph(astHandle: number, graphHandle: number): {
    success: boolean;
    nodeIds: number[];
    constraintIds: number[];
    error: string | null;
  } {
    const jsonPtr = this.mod._web_formula_to_graph(astHandle, graphHandle);
    const result = this.readJsonAndFree<{
      success: boolean;
      node_ids: number[];
      constraint_ids: number[];
      error: string | null;
    }>(jsonPtr);
    if (!result) {
      return { success: false, nodeIds: [], constraintIds: [], error: 'Conversion failed' };
    }
    return {
      success: result.success,
      nodeIds: result.node_ids ?? [],
      constraintIds: result.constraint_ids ?? [],
      error: result.error,
    };
  }

  /**
   * Convert a graph to formula representations.
   * / 将图转换为公式表示。
   */
  graphToFormula(graphHandle: number): {
    success: boolean;
    latex: string | null;
    python: string | null;
    dsl: string | null;
    error: string | null;
  } {
    const jsonPtr = this.mod._web_graph_to_formula(graphHandle);
    const result = this.readJsonAndFree<{
      success: boolean;
      latex: string | null;
      python: string | null;
      dsl: string | null;
      error: string | null;
    }>(jsonPtr);
    if (!result) {
      return { success: false, latex: null, python: null, dsl: null, error: 'Conversion failed' };
    }
    return result;
  }

  /**
   * Destroy a formula AST node.
   * / 销毁公式 AST 节点。
   */
  formulaNodeDestroy(astHandle: number): void {
    this.mod._web_formula_node_destroy(astHandle);
  }

  /**
   * Detect the syntax type of a formula string.
   * / 检测公式字符串的语法类型。
   */
  formulaDetectSyntax(input: string): number {
    const ptr = this.allocWasmString(input);
    const result = this.mod._web_formula_detect_syntax(ptr);
    this.freeWasmString(ptr);
    return result;
  }

  // ---- Engine Operations / 引擎操作 ----

  /**
   * Create an engine instance.
   * / 创建引擎实例。
   */
  engineCreate(): number {
    return this.mod._web_engine_create();
  }

  /**
   * Destroy an engine instance.
   * / 销毁引擎实例。
   */
  engineDestroy(engineHandle: number): void {
    this.mod._web_engine_destroy(engineHandle);
  }

  /**
   * Run the full solve pipeline on the engine.
   * / 在引擎上执行完整求解流水线。
   */
  engineSolve(engineHandle: number): number {
    return this.mod._web_engine_solve(engineHandle);
  }

  /**
   * Run the rewrite-and-solve workflow.
   * / 执行重写-求解工作流。
   */
  engineRewriteAndSolve(engineHandle: number, maxRewrite: number, maxSolve: number): number {
    return this.mod._web_engine_rewrite_and_solve(engineHandle, maxRewrite, maxSolve);
  }

  /**
   * Get the main graph from an engine instance.
   * / 从引擎实例获取主图。
   */
  engineGetGraph(engineHandle: number): number {
    return this.mod._web_engine_get_graph(engineHandle);
  }

  /**
   * Get the last error message from the engine.
   * / 获取引擎最近一次错误消息。
   */
  engineGetLastError(engineHandle: number): string {
    const ptr = this.mod._web_engine_get_last_error(engineHandle);
    return this.readAndFreeCString(ptr);
  }

  /**
   * Get the library version string.
   * / 获取库版本字符串。
   */
  getVersion(): string {
    const ptr = this.mod._web_get_version();
    return this.mod.UTF8ToString(ptr);
  }
}
