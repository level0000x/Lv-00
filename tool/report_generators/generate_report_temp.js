const {
  Document, Packer, Paragraph, TextRun, Table, TableRow, TableCell,
  HeadingLevel, AlignmentType, BorderStyle, WidthType, PageBreak,
  ShadingType, TableLayoutType, VerticalAlign, Header, Footer,
  PageNumber, NumberFormat
} = require("docx");
const fs = require("fs");
const path = require("path");

// ============================================================
// Color palette
// ============================================================
const COLORS = {
  primary: "1F4E79",      // deep blue
  secondary: "2E75B6",    // medium blue
  accent: "D6E4F0",       // light blue
  headerBg: "1F4E79",     // table header background
  headerText: "FFFFFF",   // table header text
  lightGray: "F2F2F2",    // alternating row
  borderColor: "B4C6E7",  // table border
  text: "333333",
  subtitle: "666666",
};

// ============================================================
// Helper: create a table cell
// ============================================================
function cell(text, opts = {}) {
  const {
    bold = false,
    color = COLORS.text,
    size = 20,           // half-points (20 = 10pt)
    font = "Microsoft YaHei",
    shading,
    alignment = AlignmentType.LEFT,
    verticalAlign = VerticalAlign.CENTER,
    columnSpan,
    rowSpan,
    width,
    borders,
  } = opts;

  const cellOpts = {
    children: [
      new Paragraph({
        alignment,
        spacing: { before: 40, after: 40 },
        children: [
          new TextRun({ text: String(text), bold, color, size, font }),
        ],
      }),
    ],
    verticalAlign,
    borders: borders || {
      top: { style: BorderStyle.SINGLE, size: 1, color: COLORS.borderColor },
      bottom: { style: BorderStyle.SINGLE, size: 1, color: COLORS.borderColor },
      left: { style: BorderStyle.SINGLE, size: 1, color: COLORS.borderColor },
      right: { style: BorderStyle.SINGLE, size: 1, color: COLORS.borderColor },
    },
  };

  if (shading) {
    cellOpts.shading = { type: ShadingType.SOLID, color: shading };
  }
  if (columnSpan) cellOpts.columnSpan = columnSpan;
  if (rowSpan) cellOpts.rowSpan = rowSpan;
  if (width) cellOpts.width = width;

  return new TableCell(cellOpts);
}

// ============================================================
// Helper: header cell
// ============================================================
function hCell(text, opts = {}) {
  return cell(text, {
    bold: true,
    color: COLORS.headerText,
    size: 20,
    shading: COLORS.headerBg,
    alignment: AlignmentType.CENTER,
    ...opts,
  });
}

// ============================================================
// Helper: create a problem table
// ============================================================
function createProblemTable(headers, rows) {
  const colCount = headers.length;
  const colWidth = Math.floor(9000 / colCount);

  const headerRow = new TableRow({
    tableHeader: true,
    children: headers.map(h => hCell(h, { width: { size: colWidth, type: WidthType.DXA } })),
  });

  const dataRows = rows.map((row, idx) => {
    const bg = idx % 2 === 1 ? COLORS.lightGray : undefined;
    return new TableRow({
      children: row.map((text, ci) =>
        cell(text, {
          shading: bg,
          alignment: ci === 0 ? AlignmentType.CENTER : AlignmentType.LEFT,
          size: 18,
          width: { size: colWidth, type: WidthType.DXA },
        })
      ),
    });
  });

  return new Table({
    width: { size: 9000, type: WidthType.DXA },
    layout: TableLayoutType.FIXED,
    rows: [headerRow, ...dataRows],
  });
}

// ============================================================
// Helper: create a "no-issue" table
// ============================================================
function createNoIssueTable(rows) {
  const headers = ["模块", "检查项", "结论"];
  return createProblemTable(headers, rows);
}

// ============================================================
// Helper: heading paragraph
// ============================================================
function heading(text, level = HeadingLevel.HEADING_1) {
  return new Paragraph({
    heading: level,
    spacing: { before: 240, after: 120 },
    children: [
      new TextRun({
        text,
        font: "Microsoft YaHei",
        color: COLORS.primary,
        bold: true,
        size: level === HeadingLevel.HEADING_1 ? 32 : level === HeadingLevel.HEADING_2 ? 28 : 24,
      }),
    ],
  });
}

// ============================================================
// Helper: body paragraph
// ============================================================
function body(text, opts = {}) {
  const { bold = false, spacing = { before: 80, after: 80 }, alignment = AlignmentType.LEFT } = opts;
  return new Paragraph({
    alignment,
    spacing,
    children: [
      new TextRun({
        text,
        font: "Microsoft YaHei",
        size: 22,
        color: COLORS.text,
        bold,
      }),
    ],
  });
}

// ============================================================
// Helper: bullet paragraph
// ============================================================
function bullet(text) {
  return new Paragraph({
    bullet: { level: 0 },
    spacing: { before: 40, after: 40 },
    children: [
      new TextRun({ text, font: "Microsoft YaHei", size: 22, color: COLORS.text }),
    ],
  });
}

// ============================================================
// Helper: numbered paragraph
// ============================================================
function numbered(text, level = 0) {
  return new Paragraph({
    numbering: { reference: "default-numbering", level },
    spacing: { before: 40, after: 40 },
    children: [
      new TextRun({ text, font: "Microsoft YaHei", size: 22, color: COLORS.text }),
    ],
  });
}

// ============================================================
// Helper: empty paragraph spacer
// ============================================================
function spacer(size = 200) {
  return new Paragraph({ spacing: { before: size, after: 0 }, children: [] });
}

// ============================================================
// Build Document
// ============================================================

// --- Cover page ---
const coverChildren = [
  spacer(2400),
  new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { after: 200 },
    children: [
      new TextRun({
        text: "Lv-00 功能补全与代码质量全面优化任务汇报",
        font: "Microsoft YaHei",
        size: 48,
        bold: true,
        color: COLORS.primary,
      }),
    ],
  }),
  spacer(200),
  new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { after: 100 },
    children: [
      new TextRun({
        text: "v3.3.1 — 安全修复、性能优化、代码标准化",
        font: "Microsoft YaHei",
        size: 28,
        color: COLORS.secondary,
      }),
    ],
  }),
  spacer(600),
  new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { after: 80 },
    children: [
      new TextRun({ text: "日期：2026-05-25", font: "Microsoft YaHei", size: 24, color: COLORS.subtitle }),
    ],
  }),
  new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { after: 80 },
    children: [
      new TextRun({ text: "项目：Lv-00 几何元语言系统", font: "Microsoft YaHei", size: 24, color: COLORS.subtitle }),
    ],
  }),
  new Paragraph({
    alignment: AlignmentType.CENTER,
    children: [
      new TextRun({ text: "文档版本：v3.3.1", font: "Microsoft YaHei", size: 24, color: COLORS.subtitle }),
    ],
  }),
];

// --- Section 1: 任务概述 ---
const section1 = [
  heading("一、任务概述"),
  body("本次任务的目标是对 Lv-00 几何元语言系统进行全面的代码审查与质量优化。具体工作包括："),
  bullet("全面审查项目代码，识别功能缺失与不人性化设计"),
  bullet("修复安全漏洞（XSS 注入、整数溢出、缓冲区越界等）"),
  bullet("优化性能（正则缓存、惰性初始化、异步非阻塞调用等）"),
  bullet("标准化代码风格（统一空值检查、命名空间封装、Doxygen 注释等）"),
  bullet("完善中文注释与模块文档"),
  bullet("统一版本号，更新 README 和 CHANGELOG"),
  spacer(100),
  body("本次审查覆盖了 C 核心引擎、前端 JavaScript、Python 模块、构建系统及文档等多个维度，共发现并修复 24 个问题（P0 级 4 个、P1 级 10 个、P2 级 10 个），确认 7 项无问题。"),
];

// --- Section 2: 审查范围 ---
const section2 = [
  heading("二、审查范围"),
  body("本次审查涵盖以下模块："),
  bullet("C 核心引擎（约 120 个 .c 文件，约 160 个 .h 头文件）"),
  bullet("前端 JavaScript（约 25 个 JS 文件，约 5 个 CSS 文件）"),
  bullet("Python 模块（concurrent_monitor、llm_coding_assistant）"),
  bullet("构建系统（CMakeLists.txt、CI/CD 配置）"),
  bullet("文档（README.md、CHANGELOG.md 等）"),
];

// --- Section 3: 发现的问题汇总 ---
const section3 = [
  heading("三、发现的问题汇总"),

  // 3.1 P0
  heading("3.1 P0 级问题（已修复）", HeadingLevel.HEADING_2),
  body("P0 级问题为安全关键或功能阻断性问题，必须立即修复。"),
  createProblemTable(
    ["编号", "模块", "问题", "修复方案", "状态"],
    [
      ["P0-1", "func_block_serialize.c", "parse_quoted_string 不处理转义引号", "两遍扫描转义处理", "已修复"],
      ["P0-2", "func_block_serialize.c", "parse_int 无溢出检查", "添加 INT_MAX 溢出保护", "已修复"],
      ["P0-3", "modules/magic.js", "innerHTML 拼接外部数据导致 XSS", "添加 _escapeHtml 转义", "已修复"],
      ["P0-4", "前端多文件", "版本号不一致（3.2.0/3.0.0-js/3.3.0）", "统一为 v3.3.0", "已修复"],
    ]
  ),
  spacer(200),

  // 3.2 P1
  heading("3.2 P1 级问题（已修复）", HeadingLevel.HEADING_2),
  body("P1 级问题为影响稳定性或可维护性的重要问题。"),
  createProblemTable(
    ["编号", "模块", "问题", "修复方案", "状态"],
    [
      ["P1-1", "ai_engine.py", "DashScope 流式调用阻塞事件循环", "asyncio.to_thread 包装", "已修复"],
      ["P1-2", "ai_engine.py", "会话历史无 LRU 清理导致内存泄漏", "复用 _evict_oldest_sessions", "已修复"],
      ["P1-3", "lv00_knowledge.py", "缓存非线程安全", "双重检查锁定", "已修复"],
      ["P1-4", "ai_engine.py", "模块文档过于简略", "扩展为完整文档", "已修复"],
      ["P1-5", "app.js", "bindBtn 全局函数污染", "挂载到 Lv00WebApp 命名空间", "已修复"],
      ["P1-6", "debug.js", "debugReport 双重日志输出", "移除冗余调用", "已修复"],
      ["P1-7", "graph.js", "空值检查风格不一致", "统一为严格检查", "已修复"],
      ["P1-8", "engine.c", "注释语法缺陷", "修正为 Doxygen 格式", "已修复"],
      ["P1-9", "proof.c", "桩实现缺少文档", "添加完整 Doxygen 注释", "已修复"],
      ["P1-10", "README/CHANGELOG", "版本号和特性列表过时", "更新至 v3.3.0", "已修复"],
    ]
  ),
  spacer(200),

  // 3.3 P2
  heading("3.3 P2 级问题（已修复）", HeadingLevel.HEADING_2),
  body("P2 级问题为代码质量与规范层面的改进项。"),
  createProblemTable(
    ["编号", "模块", "问题", "修复方案", "状态"],
    [
      ["P2-1", "lv00.pc.in", "GMP 重复链接", "移除 Libs.private 中的 -lgmp", "已修复"],
      ["P2-2", "streaming.js", "硬编码颜色值", "CSS 变量 + _readCssColor", "已修复"],
      ["P2-3", "variables.css", "缺少流式输出颜色变量", "新增 33 个 CSS 变量", "已修复"],
      ["P2-4", "formula_parser.js", "detectSyntax 每次重编译正则", "惰性初始化缓存", "已修复"],
      ["P2-5", "index.html", "脚本同步加载阻塞渲染", "添加 defer 属性", "已修复"],
      ["P2-6", "formula_renderer.js", "CDN 加载无超时机制", "15秒超时 + 错误回调", "已修复"],
      ["P2-7", "func_block_preset.c", "generate_index 类别覆盖不足", "扩展至 25 个类别", "已修复"],
      ["P2-8", "preset_topology.c", "sizeof 计数与常量不同步", "使用 TOPOLOGY_PRESET_COUNT", "已修复"],
      ["P2-9", "preset_topology.c", "缺少 PRESET_CHECK_NULL 宏", "统一使用宏", "已修复"],
      ["P2-10", "func_block_instantiate.c", "约束复制失败静默跳过", "添加不完整标志和日志", "已修复"],
    ]
  ),
  spacer(200),

  // 3.4 已确认无问题
  heading("3.4 已确认无问题项", HeadingLevel.HEADING_2),
  body("以下检查项经审查确认不存在问题，或已有正确的防护措施："),
  createNoIssueTable([
    ["memory_pool.c", "ensure_stats_init 竞态条件", "已使用 InterlockedCompareExchange 修复"],
    ["preset_manager.c", "lock_library_init_once 竞态条件", "Windows 用 ICE，POSIX 用 pthread_once"],
    ["debug.c", "rotate_logs 竞态条件", "调用链均在锁内，添加注释说明"],
    ["engine.py", "asyncio.get_event_loop deprecated", "已使用 get_running_loop"],
    ["CMakeLists.txt", "LV00_EXCLUDE_BROKEN_PRESETS", "已在无条件位置定义"],
    ["CMakeLists.txt", "共享库线程链接", "已链接 Threads::Threads"],
    ["func_block_compose.c", "return NULL 类型不匹配", "实际为 return false，非 BUG"],
  ]),
];

// --- Section 4: 修改文件清单 ---
const modifiedFiles = [
  "src/core/func_block_serialize.c",
  "src/core/func_block_preset.c",
  "src/core/func_block_instantiate.c",
  "src/core/preset_topology.c",
  "src/core/engine.c",
  "src/core/proof.c",
  "src/python/ai_engine.py",
  "src/python/lv00_knowledge.py",
  "web/modules/magic.js",
  "web/js/app.js",
  "web/js/debug.js",
  "web/js/graph.js",
  "web/js/streaming.js",
  "web/js/formula_parser.js",
  "web/js/formula_renderer.js",
  "web/css/variables.css",
  "web/index.html",
  "pkg-config/lv00.pc.in",
  "README.md",
  "CHANGELOG.md",
];

const section4 = [
  heading("四、修改文件清单"),
  body("本次任务共修改了以下 20 个文件："),
  ...modifiedFiles.map(f => bullet(f)),
];

// --- Section 5: 遗留问题与建议 ---
const section5 = [
  heading("五、遗留问题与建议"),
  body("以下问题在本次任务中未处理，建议在后续迭代中优先改进："),
  numbered("大文件拆分：solver.c（7415 行）、symbolic_coord.c（5141 行）等超大文件建议按功能模块拆分，提升可维护性。"),
  numbered("三重注册表碎片化：当前存在多套注册表机制，建议统一为单一注册表，降低维护成本。"),
  numbered("双类型系统统一：PresetType 与 PresetParamType 存在语义重叠，建议合并为统一类型体系。"),
  numbered("预设文件样板模式提取：预设注册代码存在大量重复模式，建议通过宏或数据驱动方式统一注册流程。"),
  numbered("llm_coding_assistant 整包无测试：该 Python 模块缺少单元测试，建议补充测试覆盖。"),
  numbered("concurrent_monitor/web/ 无测试：Web 界面部分缺少自动化测试，建议引入前端测试框架。"),
  numbered("前端 web/ 和 web-gui/ 功能重叠：两个前端目录存在功能重复，建议合并或明确职责划分。"),
];

// --- Section 6: 验证结果 ---
const section6 = [
  heading("六、验证结果"),
  body("所有修改均通过以下验证流程："),
  bullet("代码审查：逐文件审查所有修改，确认修复方案正确且无引入新问题"),
  bullet("回归验证：确认修改不影响已有功能的正常运作"),
  bullet("风格一致性：确认代码风格、注释格式、命名规范与项目标准一致"),
  spacer(100),
  body("结论：本次 v3.3.1 迭代共修复 24 个问题（P0 级 4 个、P1 级 10 个、P2 级 10 个），确认 7 项无问题，所有修改均通过代码审查验证，无引入新问题。", { bold: true }),
];

// ============================================================
// Assemble document
// ============================================================
const doc = new Document({
  styles: {
    default: {
      document: {
        run: {
          font: "Microsoft YaHei",
          size: 22,
          color: COLORS.text,
        },
      },
    },
  },
  numbering: {
    config: [
      {
        reference: "default-numbering",
        levels: [
          {
            level: 0,
            format: NumberFormat.DECIMAL,
            text: "%1.",
            alignment: AlignmentType.LEFT,
            style: { paragraph: { indent: { left: 720, hanging: 360 } } },
          },
        ],
      },
    ],
  },
  sections: [
    // Cover page
    {
      properties: {
        page: {
          size: { width: 11906, height: 16838 }, // A4
          margin: { top: 1440, bottom: 1440, left: 1440, right: 1440 },
        },
      },
      children: coverChildren,
    },
    // Main content
    {
      properties: {
        page: {
          size: { width: 11906, height: 16838 }, // A4
          margin: { top: 1440, bottom: 1440, left: 1440, right: 1440 },
        },
      },
      headers: {
        default: new Header({
          children: [
            new Paragraph({
              alignment: AlignmentType.RIGHT,
              children: [
                new TextRun({
                  text: "Lv-00 功能补全与代码质量全面优化任务汇报 v3.3.1",
                  font: "Microsoft YaHei",
                  size: 16,
                  color: COLORS.subtitle,
                }),
              ],
            }),
          ],
        }),
      },
      footers: {
        default: new Footer({
          children: [
            new Paragraph({
              alignment: AlignmentType.CENTER,
              children: [
                new TextRun({ text: "第 ", font: "Microsoft YaHei", size: 16, color: COLORS.subtitle }),
                new TextRun({ children: [PageNumber.CURRENT], font: "Microsoft YaHei", size: 16, color: COLORS.subtitle }),
                new TextRun({ text: " 页", font: "Microsoft YaHei", size: 16, color: COLORS.subtitle }),
              ],
            }),
          ],
        }),
      },
      children: [
        ...section1,
        ...section2,
        ...section3,
        ...section4,
        ...section5,
        ...section6,
      ],
    },
  ],
});

// ============================================================
// Write file
// ============================================================
const outputPath = path.join(
  "c:", "Users", "xingg", "Documents", "trae_projects", "Lv-00", "docs", "reports",
  "Lv-00_功能补全与代码质量全面优化任务汇报_v3.3.1.docx"
);

Packer.toBuffer(doc).then((buffer) => {
  fs.writeFileSync(outputPath, buffer);
  console.log("Document generated successfully: " + outputPath);
}).catch((err) => {
  console.error("Error generating document:", err);
  process.exit(1);
});
