/**
 * @module components/panels/TypePanel
 * @description 类型系统模块侧边栏面板 / Type system module sidebar panel.
 *
 *              提供几何类型定义、类型检查、结构统一化、子类型判定等功能。
 *              Provides geometry type definition, type checking, structure
 *              unification, and subtype determination features.
 *              所有功能使用纯 JS 实现，不依赖 WASM 后端。
 *              All features use pure JS implementation, no WASM backend dependency.
 *              内置 Point、Segment、Triangle、Square、Circle、Polygon 等基础类型。
 *              Built-in types include Point, Segment, Triangle, Square, Circle, Polygon, etc.
 */

import React, { useState, useCallback, useMemo } from 'react';
import Panel from './Panel';
import { useAppStore } from '@/stores';

// ================================================================
// 类型定义 / Type Definitions
// ================================================================

/** 自定义几何类型接口 */
interface GeoType {
  /** 类型名称 */
  name: string;
  /** 所需点数（-1 表示不限） */
  requiredPoints: number;
  /** 所需线段数（-1 表示不限） */
  requiredSegments: number;
  /** 所需约束类型列表 */
  requiredConstraintTypes: string[];
  /** 描述 */
  description: string;
  /** 是否为内置类型 */
  builtin?: boolean;
}

/** 类型检查结果 */
interface TypeCheckResult {
  typeName: string;
  passed: boolean;
  details: string;
}

/** 统一化结果 */
interface UnifyResult {
  isomorphic: boolean;
  differences: string[];
}

/** 子类型检查结果 */
interface SubtypeResult {
  isSubtype: boolean;
  details: string;
}

// ================================================================
// 内置类型 / Built-in Types
// ================================================================

const BUILTIN_TYPES: GeoType[] = [
  {
    name: 'Point',
    requiredPoints: 1,
    requiredSegments: 0,
    requiredConstraintTypes: [],
    description: '单个点 / A single point',
    builtin: true,
  },
  {
    name: 'Segment',
    requiredPoints: 2,
    requiredSegments: 1,
    requiredConstraintTypes: [],
    description: '两个点和一条线段 / Two points and one segment',
    builtin: true,
  },
  {
    name: 'Triangle',
    requiredPoints: 3,
    requiredSegments: 3,
    requiredConstraintTypes: [],
    description: '三个点和三条线段 / Three points and three segments',
    builtin: true,
  },
  {
    name: 'Square',
    requiredPoints: 4,
    requiredSegments: 4,
    requiredConstraintTypes: [],
    description: '四个点和四条线段 / Four points and four segments',
    builtin: true,
  },
  {
    name: 'Circle',
    requiredPoints: -1,
    requiredSegments: -1,
    requiredConstraintTypes: [],
    description: '圆形（特殊类型，需要约束定义） / Circle (special type)',
    builtin: true,
  },
  {
    name: 'Polygon',
    requiredPoints: -1,
    requiredSegments: -1,
    requiredConstraintTypes: [],
    description: '多边形（>= 3 个点） / Polygon (>= 3 points)',
    builtin: true,
  },
];

/**
 * TypePanel - 类型系统模块侧边栏面板
 *
 * 面板分区:
 * - TYPE OPERATIONS: 创建、检查、统一化、子类型判定
 * - CREATE TYPE: 类型创建表单
 * - RESULTS: 检查/统一化/子类型结果展示
 * - INFO: 来自 Store 的实时类型相关统计数据
 */
const TypePanel: React.FC = () => {
  const addToast = useAppStore((s) => s.addToast);
  const appendLog = useAppStore((s) => s.appendLog);

  // ================================================================
  // 从 Store 读取真实数据
  // ================================================================
  const points = useAppStore((s) => s.points);
  const segments = useAppStore((s) => s.segments);
  const constraints = useAppStore((s) => s.constraints);

  // ================================================================
  // 本地状态 / Local State
  // ================================================================
  /** 用户自定义类型列表 */
  const [customTypes, setCustomTypes] = useState<GeoType[]>([]);

  /** 创建类型表单状态 */
  const [showCreateForm, setShowCreateForm] = useState(false);
  const [typeName, setTypeName] = useState('');
  const [requiredPoints, setRequiredPoints] = useState('3');
  const [requiredSegments, setRequiredSegments] = useState('3');
  const [requiredConstraints, setRequiredConstraints] = useState('');

  /** 结果展示状态 */
  const [showResults, setShowResults] = useState(false);
  const [resultsText, setResultsText] = useState('');
  const [resultsColor, setResultsColor] = useState<string>('var(--text, #e0e0e0)');

  /** 子类型检查：选择两个类型 */
  const [subtypeA, setSubtypeA] = useState('');
  const [subtypeB, setSubtypeB] = useState('');
  const [showSubtypeForm, setShowSubtypeForm] = useState(false);

  // ================================================================
  // 所有类型（内置 + 自定义）/ All Types (builtin + custom)
  // ================================================================
  const allTypes = useMemo(() => [...BUILTIN_TYPES, ...customTypes], [customTypes]);

  // ================================================================
  // CREATE TYPE / 创建类型
  // ================================================================

  const handleCreateType = useCallback(() => {
    const name = typeName.trim();
    if (!name) {
      addToast('error', '请输入类型名称 / Please enter a type name');
      return;
    }

    // 检查名称是否已存在
    if (allTypes.some((t) => t.name.toLowerCase() === name.toLowerCase())) {
      addToast('error', `类型 "${name}" 已存在 / Type "${name}" already exists`);
      return;
    }

    const rp = parseInt(requiredPoints, 10);
    const rs = parseInt(requiredSegments, 10);
    if (isNaN(rp) || isNaN(rs)) {
      addToast('error', '点数和线段数必须为整数 / Points and segments must be integers');
      return;
    }

    // 解析约束类型
    const constraintTypes = requiredConstraints
      .split(',')
      .map((s) => s.trim())
      .filter((s) => s.length > 0);

    const newType: GeoType = {
      name,
      requiredPoints: rp,
      requiredSegments: rs,
      requiredConstraintTypes: constraintTypes,
      description: `自定义类型: ${rp} 点, ${rs} 线段 / Custom: ${rp} pts, ${rs} segs`,
    };

    setCustomTypes((prev) => [...prev, newType]);
    setTypeName('');
    setRequiredPoints('3');
    setRequiredSegments('3');
    setRequiredConstraints('');
    setShowCreateForm(false);

    appendLog(`类型创建: "${name}" (${rp} 点, ${rs} 线段, ${constraintTypes.length} 约束类型)`, 'info');
    addToast('success', `类型 "${name}" 已创建 / Type "${name}" created`);
  }, [typeName, requiredPoints, requiredSegments, requiredConstraints, allTypes, addToast, appendLog]);

  // ================================================================
  // TYPE CHECK / 类型检查
  // ================================================================

  /** 检查当前几何图是否匹配指定类型 */
  const checkType = useCallback((geoType: GeoType): TypeCheckResult => {
    const details: string[] = [];
    let passed = true;

    // 检查点数
    if (geoType.requiredPoints >= 0) {
      if (points.length === geoType.requiredPoints) {
        details.push(`[PASS] 点数: ${points.length} = ${geoType.requiredPoints}`);
      } else {
        details.push(`[FAIL] 点数: ${points.length} != ${geoType.requiredPoints}`);
        passed = false;
      }
    } else {
      // -1 表示不限，但 Polygon 至少需要 3 个点
      if (geoType.name === 'Polygon' && points.length < 3) {
        details.push(`[FAIL] 多边形至少需要 3 个点，当前 ${points.length}`);
        passed = false;
      } else {
        details.push(`[PASS] 点数: ${points.length} (无限制)`);
      }
    }

    // 检查线段数
    if (geoType.requiredSegments >= 0) {
      if (segments.length === geoType.requiredSegments) {
        details.push(`[PASS] 线段数: ${segments.length} = ${geoType.requiredSegments}`);
      } else {
        details.push(`[FAIL] 线段数: ${segments.length} != ${geoType.requiredSegments}`);
        passed = false;
      }
    } else {
      if (geoType.name === 'Polygon' && segments.length < 3) {
        details.push(`[FAIL] 多边形至少需要 3 条线段，当前 ${segments.length}`);
        passed = false;
      } else {
        details.push(`[PASS] 线段数: ${segments.length} (无限制)`);
      }
    }

    // 检查约束类型
    if (geoType.requiredConstraintTypes.length > 0) {
      const currentConstraintTypes = new Set(constraints.map((c) => c.type as string));
      for (const reqType of geoType.requiredConstraintTypes) {
        if (currentConstraintTypes.has(reqType)) {
          details.push(`[PASS] 约束类型 "${reqType}" 存在`);
        } else {
          details.push(`[FAIL] 缺少约束类型 "${reqType}"`);
          passed = false;
        }
      }
    } else {
      details.push(`[PASS] 无额外约束要求`);
    }

    return {
      typeName: geoType.name,
      passed,
      details: details.join('\n'),
    };
  }, [points, segments, constraints]);

  const handleTypeCheck = useCallback(() => {
    if (points.length === 0 && segments.length === 0) {
      addToast('warning', '画布为空，无法进行类型检查 / Canvas is empty');
      return;
    }

    const results: TypeCheckResult[] = allTypes.map((t) => checkType(t));
    const passedTypes = results.filter((r) => r.passed);
    const failedTypes = results.filter((r) => !r.passed);

    let text = `=== 类型检查结果 / Type Check Results ===\n`;
    text += `当前几何: ${points.length} 点, ${segments.length} 线段, ${constraints.length} 约束\n\n`;

    if (passedTypes.length > 0) {
      text += `--- 匹配的类型 / Matched Types ---\n`;
      for (const r of passedTypes) {
        text += `\n[${r.typeName}] PASS\n${r.details}\n`;
      }
    }

    if (failedTypes.length > 0) {
      text += `\n--- 不匹配的类型 / Non-matched Types ---\n`;
      for (const r of failedTypes) {
        text += `\n[${r.typeName}] FAIL\n${r.details}\n`;
      }
    }

    text += `\n总计: ${passedTypes.length}/${results.length} 类型匹配`;
    text += `\nTotal: ${passedTypes.length}/${results.length} types matched`;

    setResultsText(text);
    setResultsColor(passedTypes.length > 0 ? '#51cf66' : '#ff6b6b');
    setShowResults(true);

    appendLog(`类型检查: ${passedTypes.length}/${results.length} 类型匹配`, 'info');
    addToast('info', `${passedTypes.length}/${results.length} 类型匹配 / ${passedTypes.length}/${results.length} types matched`);
  }, [points, segments, constraints, allTypes, checkType, addToast, appendLog]);

  // ================================================================
  // UNIFY / 统一化
  // ================================================================

  const handleUnify = useCallback(() => {
    // 将当前几何构造的拓扑结构签名与自身比较
    // 在实际场景中，这会比较两个不同的构造
    // 这里我们比较当前构造与内置类型的拓扑结构

    const topology = {
      pointCount: points.length,
      segmentCount: segments.length,
      constraintTypes: [...new Set(constraints.map((c) => c.type as string))].sort(),
      constraintCount: constraints.length,
    };

    // 计算邻接矩阵的度数序列（拓扑不变量）
    const degreeMap = new Map<number, number>();
    for (const p of points) {
      degreeMap.set(p.id, 0);
    }
    for (const s of segments) {
      degreeMap.set(s.p1, (degreeMap.get(s.p1) || 0) + 1);
      degreeMap.set(s.p2, (degreeMap.get(s.p2) || 0) + 1);
    }
    const degreeSequence = [...degreeMap.values()].sort((a, b) => b - a);

    let text = `=== 统一化分析 / Unification Analysis ===\n\n`;
    text += `当前构造拓扑 / Current topology:\n`;
    text += `  点数 / Points: ${topology.pointCount}\n`;
    text += `  线段数 / Segments: ${topology.segmentCount}\n`;
    text += `  约束数 / Constraints: ${topology.constraintCount}\n`;
    text += `  约束类型 / Constraint types: ${topology.constraintTypes.join(', ') || '(无/none)'}\n`;
    text += `  度数序列 / Degree sequence: [${degreeSequence.join(', ')}]\n\n`;

    // 与内置类型比较
    text += `--- 与内置类型比较 / Compare with built-in types ---\n\n`;

    for (const bt of BUILTIN_TYPES) {
      const result: UnifyResult = { isomorphic: true, differences: [] };

      if (bt.requiredPoints >= 0 && bt.requiredPoints !== topology.pointCount) {
        result.isomorphic = false;
        result.differences.push(`点数不同: ${topology.pointCount} vs ${bt.requiredPoints}`);
      }
      if (bt.requiredSegments >= 0 && bt.requiredSegments !== topology.segmentCount) {
        result.isomorphic = false;
        result.differences.push(`线段数不同: ${topology.segmentCount} vs ${bt.requiredSegments}`);
      }

      const status = result.isomorphic ? 'ISOMORPHIC / 同构' : `NOT ISOMORPHIC / 不同构`;
      text += `[${bt.name}] ${status}\n`;
      if (result.differences.length > 0) {
        for (const d of result.differences) {
          text += `  - ${d}\n`;
        }
      }
      text += '\n';
    }

    // 自定义类型比较
    for (const ct of customTypes) {
      const result: UnifyResult = { isomorphic: true, differences: [] };

      if (ct.requiredPoints >= 0 && ct.requiredPoints !== topology.pointCount) {
        result.isomorphic = false;
        result.differences.push(`点数不同: ${topology.pointCount} vs ${ct.requiredPoints}`);
      }
      if (ct.requiredSegments >= 0 && ct.requiredSegments !== topology.segmentCount) {
        result.isomorphic = false;
        result.differences.push(`线段数不同: ${topology.segmentCount} vs ${ct.requiredSegments}`);
      }
      for (const reqCt of ct.requiredConstraintTypes) {
        if (!topology.constraintTypes.includes(reqCt)) {
          result.isomorphic = false;
          result.differences.push(`缺少约束类型: ${reqCt}`);
        }
      }

      const status = result.isomorphic ? 'ISOMORPHIC / 同构' : `NOT ISOMORPHIC / 不同构`;
      text += `[${ct.name}] ${status}\n`;
      if (result.differences.length > 0) {
        for (const d of result.differences) {
          text += `  - ${d}\n`;
        }
      }
      text += '\n';
    }

    setResultsText(text);
    setResultsColor('var(--text, #e0e0e0)');
    setShowResults(true);

    appendLog(`统一化分析: ${topology.pointCount} 点, ${topology.segmentCount} 线段`, 'info');
    addToast('info', '统一化分析完成 / Unification analysis complete');
  }, [points, segments, constraints, customTypes]);

  // ================================================================
  // SUBTYPE / 子类型判定
  // ================================================================

  const handleSubtype = useCallback(() => {
    if (!subtypeA || !subtypeB) {
      addToast('error', '请选择两个类型 / Please select two types');
      return;
    }

    const typeA = allTypes.find((t) => t.name === subtypeA);
    const typeB = allTypes.find((t) => t.name === subtypeB);
    if (!typeA || !typeB) {
      addToast('error', '类型不存在 / Type not found');
      return;
    }

    // 检查 A 是否是 B 的子类型
    // 子类型条件：A 的要求 >= B 的要求（A 更严格）
    const result: SubtypeResult = { isSubtype: true, details: '' };

    const details: string[] = [];
    details.push(`检查: "${typeA.name}" 是否是 "${typeB.name}" 的子类型`);
    details.push(`Check: Is "${typeA.name}" a subtype of "${typeB.name}"?\n`);

    // 点数检查：子类型需要 >= 父类型的点数
    if (typeB.requiredPoints >= 0) {
      if (typeA.requiredPoints >= 0 && typeA.requiredPoints >= typeB.requiredPoints) {
        details.push(`[PASS] 点数: ${typeA.requiredPoints} >= ${typeB.requiredPoints}`);
      } else if (typeA.requiredPoints < 0) {
        details.push(`[PASS] 点数: 无限制 >= ${typeB.requiredPoints}`);
      } else {
        details.push(`[FAIL] 点数: ${typeA.requiredPoints} < ${typeB.requiredPoints}`);
        result.isSubtype = false;
      }
    }

    // 线段数检查
    if (typeB.requiredSegments >= 0) {
      if (typeA.requiredSegments >= 0 && typeA.requiredSegments >= typeB.requiredSegments) {
        details.push(`[PASS] 线段数: ${typeA.requiredSegments} >= ${typeB.requiredSegments}`);
      } else if (typeA.requiredSegments < 0) {
        details.push(`[PASS] 线段数: 无限制 >= ${typeB.requiredSegments}`);
      } else {
        details.push(`[FAIL] 线段数: ${typeA.requiredSegments} < ${typeB.requiredSegments}`);
        result.isSubtype = false;
      }
    }

    // 约束类型检查：子类型需要包含父类型的所有约束类型
    for (const reqCt of typeB.requiredConstraintTypes) {
      if (typeA.requiredConstraintTypes.includes(reqCt)) {
        details.push(`[PASS] 约束 "${reqCt}" 已包含`);
      } else {
        details.push(`[FAIL] 缺少约束 "${reqCt}"`);
        result.isSubtype = false;
      }
    }

    // 额外约束（子类型特有）
    const extraConstraints = typeA.requiredConstraintTypes.filter(
      (c) => !typeB.requiredConstraintTypes.includes(c),
    );
    if (extraConstraints.length > 0) {
      details.push(`\n子类型额外约束 / Extra subtype constraints: ${extraConstraints.join(', ')}`);
    }

    const conclusion = result.isSubtype
      ? `"${typeA.name}" 是 "${typeB.name}" 的子类型 / Subtype confirmed`
      : `"${typeA.name}" 不是 "${typeB.name}" 的子类型 / Not a subtype`;
    details.push(`\n结论 / Conclusion: ${conclusion}`);

    result.details = details.join('\n');

    setResultsText(`=== 子类型判定 / Subtype Check ===\n\n${result.details}`);
    setResultsColor(result.isSubtype ? '#51cf66' : '#ff6b6b');
    setShowResults(true);

    appendLog(`子类型判定: "${typeA.name}" ${result.isSubtype ? '是' : '不是'} "${typeB.name}" 的子类型`, 'info');
    addToast(
      result.isSubtype ? 'success' : 'info',
      conclusion,
    );
  }, [subtypeA, subtypeB, allTypes, addToast, appendLog]);

  // ================================================================
  // 删除自定义类型 / Delete Custom Type
  // ================================================================

  const handleDeleteType = useCallback((name: string) => {
    setCustomTypes((prev) => prev.filter((t) => t.name !== name));
    addToast('info', `类型 "${name}" 已删除 / Type "${name}" deleted`);
  }, [addToast]);

  return (
    <>
      <Panel title="TYPE OPERATIONS / 类型操作" panelId="type-ops">
        {/* 创建类型 */}
        <button className="btn btn-accent" onClick={() => setShowCreateForm(!showCreateForm)}>
          {showCreateForm ? 'CLOSE FORM / 关闭表单' : 'CREATE TYPE / 创建类型'}
        </button>

        {/* 类型检查 */}
        <button className="btn" onClick={handleTypeCheck}>
          TYPE CHECK / 类型检查
        </button>

        {/* 统一化 */}
        <button className="btn" onClick={handleUnify}>
          UNIFY / 统一化
        </button>

        {/* 子类型判定 */}
        <button className="btn" onClick={() => setShowSubtypeForm(!showSubtypeForm)}>
          {showSubtypeForm ? 'CLOSE SUBTYPE / 关闭子类型' : 'SUBTYPE / 子类型'}
        </button>
      </Panel>

      {/* 创建类型表单 */}
      {showCreateForm && (
        <Panel title="CREATE TYPE / 创建类型" panelId="type-create">
          <div className="panel-form">
            <label className="panel-form-label">类型名称 / Type Name</label>
            <input
              className="panel-form-input"
              value={typeName}
              onChange={(e) => setTypeName(e.target.value)}
              placeholder="例: EquilateralTriangle"
            />
            <label className="panel-form-label">所需点数 / Required Points (-1 = 不限)</label>
            <input
              className="panel-form-input"
              type="number"
              value={requiredPoints}
              onChange={(e) => setRequiredPoints(e.target.value)}
              placeholder="3"
            />
            <label className="panel-form-label">所需线段数 / Required Segments (-1 = 不限)</label>
            <input
              className="panel-form-input"
              type="number"
              value={requiredSegments}
              onChange={(e) => setRequiredSegments(e.target.value)}
              placeholder="3"
            />
            <label className="panel-form-label">所需约束类型 / Required Constraint Types (逗号分隔)</label>
            <input
              className="panel-form-input"
              value={requiredConstraints}
              onChange={(e) => setRequiredConstraints(e.target.value)}
              placeholder="incidence, betweenness"
            />
            <button className="btn btn-accent" onClick={handleCreateType}>
              CONFIRM / 确认创建
            </button>
          </div>
        </Panel>
      )}

      {/* 子类型检查表单 */}
      {showSubtypeForm && (
        <Panel title="SUBTYPE CHECK / 子类型检查" panelId="type-subtype">
          <div className="panel-form">
            <label className="panel-form-label">子类型 / Subtype (A)</label>
            <select
              className="panel-form-input"
              value={subtypeA}
              onChange={(e) => setSubtypeA(e.target.value)}
            >
              <option value="">-- 选择类型 / Select --</option>
              {allTypes.map((t) => (
                <option key={t.name} value={t.name}>
                  {t.name} {t.builtin ? '(内置)' : '(自定义)'}
                </option>
              ))}
            </select>
            <label className="panel-form-label">父类型 / Parent Type (B)</label>
            <select
              className="panel-form-input"
              value={subtypeB}
              onChange={(e) => setSubtypeB(e.target.value)}
            >
              <option value="">-- 选择类型 / Select --</option>
              {allTypes.map((t) => (
                <option key={t.name} value={t.name}>
                  {t.name} {t.builtin ? '(内置)' : '(自定义)'}
                </option>
              ))}
            </select>
            <button className="btn btn-accent" onClick={handleSubtype}>
              CHECK / 检查
            </button>
          </div>
        </Panel>
      )}

      {/* 结果展示面板 */}
      {showResults && (
        <Panel title="RESULTS / 结果" panelId="type-results">
          <pre className="panel-result-box" style={{ color: resultsColor }}>
            {resultsText}
          </pre>
          <button className="btn" onClick={() => setShowResults(false)} style={{ marginTop: '4px', width: '100%' }}>
            CLOSE / 关闭
          </button>
        </Panel>
      )}

      {/* 已定义类型列表 */}
      {customTypes.length > 0 && (
        <Panel title="CUSTOM TYPES / 自定义类型" panelId="type-custom">
          {customTypes.map((t) => (
            <div key={t.name} style={{
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center',
              padding: '2px 0',
              fontSize: '11px',
            }}>
              <span style={{ color: 'var(--text, #e0e0e0)' }}>
                {t.name} ({t.requiredPoints}P/{t.requiredSegments}S)
              </span>
              <button
                className="btn"
                onClick={() => handleDeleteType(t.name)}
                style={{ padding: '1px 6px', fontSize: '10px' }}
              >
                DEL
              </button>
            </div>
          ))}
        </Panel>
      )}

      <Panel title="INFO / 信息" panelId="type-info">
        <div className="info-box">
          {/* 当前几何统计 */}
          <div className="info-row">
            <span className="info-label">POINTS / 点</span>
            <span className="info-value">{points.length}</span>
          </div>
          <div className="info-row">
            <span className="info-label">SEGMENTS / 线段</span>
            <span className="info-value">{segments.length}</span>
          </div>
          <div className="info-row">
            <span className="info-label">CONSTRAINTS / 约束</span>
            <span className="info-value">{constraints.length}</span>
          </div>
          {/* 类型统计 */}
          <div className="info-row">
            <span className="info-label">BUILTIN / 内置类型</span>
            <span className="info-value">{BUILTIN_TYPES.length}</span>
          </div>
          <div className="info-row">
            <span className="info-label">CUSTOM / 自定义类型</span>
            <span className="info-value">{customTypes.length}</span>
          </div>
          <div className="info-row">
            <span className="info-label">TOTAL / 总类型数</span>
            <span className="info-value">{allTypes.length}</span>
          </div>
          {/* 拓扑信息 */}
          <div className="info-row">
            <span className="info-label">CONSTRAINT TYPES / 约束类型</span>
            <span className="info-value">
              {[...new Set(constraints.map((c) => c.type))].join(', ') || '--'}
            </span>
          </div>
          <div className="info-row">
            <span className="info-label">STATUS / 状态</span>
            <span className="info-value" style={{ color: '#51cf66' }}>
              READY
            </span>
          </div>
        </div>
      </Panel>
    </>
  );
};

export default TypePanel;
