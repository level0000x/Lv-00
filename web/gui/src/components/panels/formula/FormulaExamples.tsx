/**
 * @module components/panels/formula/FormulaExamples
 * @description 示例公式列表子组件。
 *              展示预设的 DSL 公式示例，点击可快速加载到输入框。
 *
 * 功能特性：
 * - 预设示例列表（等边三角形、圆的方程、勾股定理等）
 * - 点击加载示例到输入框
 * - 键盘可访问（Enter/Space 触发）
 */

import React from 'react';
import Panel from '../Panel';

/**
 * 预设的公式示例列表，使用真实 DSL 语法。
 * 每个示例的 id 对应 DSL 中的常见几何命题。
 * 导出供 FormulaPanel 等组件使用。
 */
export const FORMULA_EXAMPLES: Array<{ id: string; label: string; code: string }> = [
  {
    id: 'equilateral_triangle',
    label: '等边三角形 / Equilateral Triangle',
    code: `// 等边三角形
point A(0, 0)
point B(4, 0)
point C(2, 3.46)
segment AB
segment BC
segment CA
measure distance A, B
measure angle A, B, C`,
  },
  {
    id: 'circle_equation',
    label: '圆的方程 / Circle Equation',
    code: `// 以原点为圆心、半径为 3 的圆
point O(0, 0)
point R(3, 0)
circle center(O) radius(R)
measure distance O, R`,
  },
  {
    id: 'pythagorean',
    label: '勾股定理 / Pythagorean',
    code: `// 直角三角形 3-4-5
point A(0, 0)
point B(4, 0)
point C(0, 3)
segment AB
segment BC
segment CA
measure distance A, B
measure distance B, C
measure distance C, A
measure angle B, A, C`,
  },
  {
    id: 'midpoint',
    label: '中垂线 / Midpoint',
    code: `// 中点与中垂线
point A(0, 0)
point B(6, 0)
midpoint M of A, B
segment AB
measure distance A, M
measure distance M, B`,
  },
  {
    id: 'line_equation',
    label: '直线方程 / Line Equation',
    code: `// 直线上的点
point A(1, 1)
point B(5, 3)
segment AB
measure distance A, B`,
  },
  {
    id: 'triangle_area',
    label: '三角形面积 / Triangle Area',
    code: `// 三角形（用底和高估算面积）
point A(0, 0)
point B(6, 0)
point C(3, 4)
segment AB
segment BC
segment CA
measure distance A, B
measure distance A, C`,
  },
  {
    id: 'distance',
    label: '两点距离 / Distance',
    code: `// 计算两点之间的距离
point P(1, 2)
point Q(4, 6)
segment PQ
measure distance P, Q`,
  },
  {
    id: 'intersection',
    label: '交点 / Intersection',
    code: `// 两条线段的交点
point A(0, 0)
point B(4, 4)
point C(0, 4)
point D(4, 0)
segment AB
segment CD
intersect segment AB with CD`,
  },
];

/**
 * FormulaExamples 组件属性
 */
interface FormulaExamplesProps {
  /** 点击示例时的回调，传入示例 id */
  onExampleClick: (exampleId: string) => void;
}

/**
 * FormulaExamples - 示例公式列表子组件
 *
 * 展示预设的 DSL 公式示例，点击可快速加载到输入框。
 */
const FormulaExamples: React.FC<FormulaExamplesProps> = ({ onExampleClick }) => {
  return (
    <Panel title="EXAMPLES / 示例" panelId="formula-examples">
      <ul className="examples-list" id="formulaExamplesList">
        {FORMULA_EXAMPLES.map((ex) => (
          <li
            key={ex.id}
            data-example={ex.id}
            role="button"
            tabIndex={0}
            onClick={() => onExampleClick(ex.id)}
            onKeyDown={(e) => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); onExampleClick(ex.id); } }}
          >
            {ex.label}
          </li>
        ))}
      </ul>
    </Panel>
  );
};

export default FormulaExamples;
