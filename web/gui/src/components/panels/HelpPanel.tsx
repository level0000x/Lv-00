/**
 * @module components/panels/HelpPanel
 * @description 帮助面板，包含键盘快捷键、使用指南和关于信息。
 *              Help panel with keyboard shortcuts, usage guide, and about information.
 */

import React from 'react';
import Panel from './Panel';
import { APP_VERSION } from '@/utils/constants';

/**
 * HelpPanel - 帮助与文档侧边栏面板 / Help and documentation sidebar panel
 *
 * 区段 / Sections:
 * - KEYBOARD SHORTCUTS: 所有键盘快捷键的快速参考
 *                        Quick reference for all keyboard shortcuts
 * - ABOUT: 应用版本和描述 / Application version and description
 */
const HelpPanel: React.FC = React.memo(() => {
  return (
    <>
      <Panel title="KEYBOARD SHORTCUTS / 快捷键" panelId="help-shortcuts">
        <div className="help-panel-content">
          <h4>TOOLS / 工具</h4>
          <ul>
            <li><code>V</code> - SELECT / 选择工具</li>
            <li><code>P</code> - ADD POINT / 添加点</li>
            <li><code>L</code> - ADD SEGMENT / 添加线段</li>
            <li><code>H</code> - PAN VIEW / 平移视图</li>
            <li><code>R</code> - REGION / 区域工具</li>
            <li><code>?</code> - PROBE / 探测工具</li>
          </ul>

          <h4>VIEW / 视图</h4>
          <ul>
            <li><code>+</code> / <code>=</code> - ZOOM IN / 放大</li>
            <li><code>-</code> - ZOOM OUT / 缩小</li>
            <li><code>Ctrl+0</code> - RESET VIEW / 重置视图</li>
            <li>Scroll / 滚轮 - SMOOTH ZOOM / 平滑缩放</li>
          </ul>

          <h4>EDIT / 编辑</h4>
          <ul>
            <li><code>Ctrl+Z</code> - UNDO / 撤销</li>
            <li><code>Ctrl+Y</code> - REDO / 重做</li>
            <li><code>Delete</code> - DELETE SELECTED / 删除选中</li>
            <li><code>Escape</code> - CANCEL / 取消</li>
          </ul>

          <h4>GENERAL / 通用</h4>
          <ul>
            <li><code>Ctrl+F</code> - SEARCH / 搜索</li>
          </ul>
        </div>
      </Panel>

      <Panel title="ABOUT / 关于" panelId="help-about">
        <div className="help-panel-content">
          <p>
            <strong>Lv-00</strong> -- Symbolic Geometry Engine v{APP_VERSION}
          </p>
          <p>
            A geometric metalanguage system for symbolic geometry computation.
            / 几何元语言系统，用于符号化几何计算。
          </p>
        </div>
      </Panel>
    </>
  );
});

export default HelpPanel;
