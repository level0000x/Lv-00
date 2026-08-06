/**
 * docx_helpers.js — Lv-00 docx 生成器共享辅助库
 *
 * 由 tool/report_generators/ 下三份 JS 生成器（generate_report.js、
 * gen_report_v3.3.0.js、generate_opt_report.js）与 doc/generate_version_doc.js
 * 中重复编写的辅助函数与样式常量合并而来，属于「共享函数、常量各文件保留差异」
 * 的最小合并方案：
 *   - 完全同构的辅助函数（heading/para/boldPara/bulletItem/makeCell/makeTable、
 *     编号配置生成、分页/空行等）统一在此实现；
 *   - 各生成器的差异（字体大小、页型、标题颜色、表格宽度口径等）保留在各文件。
 *
 * 依赖：docx（node_modules/docx，与各生成器原有依赖一致）。
 */

const {
  Paragraph, TextRun, Table, TableRow, TableCell,
  AlignmentType, LevelFormat, BorderStyle, WidthType, ShadingType, PageBreak
} = require('docx');

/* ============================================================
 * 共享样式常量
 * ============================================================ */

/** ASCII 字体（三份 JS 生成器一致：Arial） */
const FONT_ASCII = "Arial";
/** ASCII 字体别名（gen_report_v3.3.0 命名） */
const FONT_EN = "Arial";
/** 东亚字体（三份 JS 生成器一致：Microsoft YaHei） */
const FONT_CJK = "Microsoft YaHei";
/** 东亚字体别名（gen_report_v3.3.0 命名） */
const FONT_CN = "Microsoft YaHei";
/** 正文字号（11pt = 22 half-points，三份 JS 生成器一致） */
const FONT_SIZE_BODY = 22;

/** US Letter 页面常量（generate_report / generate_opt_report / generate_version_doc 一致） */
const PAGE_WIDTH = 12240;
const PAGE_HEIGHT = 15840;
const MARGIN = 1440;
const CONTENT_WIDTH = PAGE_WIDTH - 2 * MARGIN; // 9360

/** 表格边框（浅灰、仅四边）—— generate_report / generate_opt_report / generate_version_doc */
const border = { style: BorderStyle.SINGLE, size: 1, color: "CCCCCC" };
const borders = { top: border, bottom: border, left: border, right: border };

/** 表格边框（含内部线）—— gen_report_v3.3.0 */
const TABLE_BORDER = { style: BorderStyle.SINGLE, size: 1, color: "999999" };
const TABLE_BORDERS = {
  top: TABLE_BORDER,
  bottom: TABLE_BORDER,
  left: TABLE_BORDER,
  right: TABLE_BORDER,
  insideHorizontal: TABLE_BORDER,
  insideVertical: TABLE_BORDER,
};

/** 表头着色 —— gen_report_v3.3.0 */
const HEADER_SHADING = { type: ShadingType.SOLID, color: "1F4E79", fill: "1F4E79" };
/** 交替行着色 —— gen_report_v3.3.0 */
const ALT_ROW_SHADING = { type: ShadingType.SOLID, color: "F2F7FB", fill: "F2F7FB" };

/* ============================================================
 * 通用辅助函数
 * ============================================================ */

/** 返回标准中英文字体描述对象（ascii/hAnsi/eastAsia） */
function font() {
  return { ascii: FONT_ASCII, hAnsi: FONT_ASCII, eastAsia: FONT_CJK };
}

/**
 * 标题段落（generate_report / generate_opt_report 同构：
 * 字体取默认文档样式，级别由调用方传入 HeadingLevel）
 */
function heading(text, level) {
  return new Paragraph({
    heading: level,
    children: [new TextRun({ text, font: font() })]
  });
}

/** 正文段落（generate_report 的 para / generate_opt_report 的 body 同构） */
function para(text, opts = {}) {
  return new Paragraph({
    spacing: { after: 120 },
    ...opts,
    children: [new TextRun({ text, font: font(), size: FONT_SIZE_BODY })]
  });
}

/** body 为 para 的别名（generate_opt_report 使用 body 命名） */
const body = para;

/** 加粗正文段落（generate_report 的 boldPara / generate_opt_report 的 boldBody 同构） */
function boldPara(text) {
  return new Paragraph({
    spacing: { after: 120 },
    children: [new TextRun({ text, font: font(), size: FONT_SIZE_BODY, bold: true })]
  });
}

/** boldBody 为 boldPara 的别名（generate_opt_report 使用 boldBody 命名） */
const boldBody = boldPara;

/**
 * 编号项目段落（引用 numbering 配置中的 reference）
 * 与 generate_report / generate_opt_report 的 bulletItem 同构
 */
function bulletItem(text, ref) {
  return new Paragraph({
    numbering: { reference: ref, level: 0 },
    spacing: { after: 60 },
    children: [new TextRun({ text, font: font(), size: FONT_SIZE_BODY })]
  });
}

/**
 * 表格单元格
 * 兼容 generate_report 的 makeTableRow 内部单元与 generate_opt_report 的
 * makeCell（opts.bold / opts.shading / opts.width，DXA 宽度）；
 * text 为数组时渲染多段（generate_version_doc 的 cell 用法），
 * opts.margins 可覆盖默认单元格边距（generate_version_doc 使用 80/80）。
 */
function makeCell(text, opts = {}) {
  const { bold = false, shading, width, margins } = opts;
  const cellOpts = {
    borders,
    margins: margins || { top: 60, bottom: 60, left: 100, right: 100 },
    children: Array.isArray(text)
      ? text.map(t => new Paragraph({ children: [new TextRun({ text: t, font: font(), size: 20 })] }))
      : [new Paragraph({ children: [new TextRun({ text, bold, font: font(), size: 20 })] })]
  };
  if (shading)
    cellOpts.shading = { fill: shading, type: ShadingType.CLEAR };
  if (width)
    cellOpts.width = { size: width, type: WidthType.DXA };
  return new TableCell(cellOpts);
}

/**
 * 表格行（generate_report 的 makeTableRow：cells 为 [text, width] 对数组，
 * isHeader 时加 D5E8F0 表头着色与加粗）
 */
function makeTableRow(cells, isHeader = false) {
  return new TableRow({
    cantSplit: true,
    children: cells.map(([text, width]) => makeCell(text, {
      width,
      bold: isHeader,
      shading: isHeader ? "D5E8F0" : undefined
    }))
  });
}

/**
 * 数据表（generate_report / generate_opt_report 的 makeTable 同构）
 * @param tableWidth 表宽（DXA）：generate_report 传 CONTENT_WIDTH(9360)，
 *                   generate_opt_report 传 colWidths 之和（8800）
 */
function makeTable(headers, rows, colWidths, tableWidth) {
  const headerRow = new TableRow({
    cantSplit: true,
    children: headers.map((h, i) => makeCell(h, { bold: true, shading: "D5E8F0", width: colWidths[i] }))
  });
  const dataRows = rows.map(row => new TableRow({
    cantSplit: true,
    children: row.map((cell, i) => makeCell(cell, { width: colWidths[i] }))
  }));
  return new Table({
    width: { size: tableWidth, type: WidthType.DXA },
    columnWidths: colWidths,
    rows: [headerRow, ...dataRows]
  });
}

/** 分页段落（gen_report_v3.3.0 / generate_version_doc 同构） */
function pageBreakPara() {
  return new Paragraph({ children: [new PageBreak()] });
}

/** 空行（gen_report_v3.3.0 的 emptyLine） */
function emptyLine() {
  return new Paragraph({ spacing: { before: 100, after: 100 }, children: [] });
}

/* ============================================================
 * 编号配置生成
 * ============================================================ */

/** 十进制编号配置组（numbers-i，generate_report / generate_opt_report 同构） */
function makeNumberConfigs(count = 30) {
  const configs = [];
  for (let i = 0; i < count; i++) {
    configs.push({
      reference: `numbers-${i}`,
      levels: [{ level: 0, format: LevelFormat.DECIMAL, text: "%1.", alignment: AlignmentType.LEFT,
        style: { paragraph: { indent: { left: 720, hanging: 360 } } } }]
    });
  }
  return configs;
}

/** 项目符号编号配置组（bullets-i，generate_opt_report 同构） */
function makeBulletConfigs(count = 30) {
  const configs = [];
  for (let i = 0; i < count; i++) {
    configs.push({
      reference: `bullets-${i}`,
      levels: [{ level: 0, format: LevelFormat.BULLET, text: "\u2022", alignment: AlignmentType.LEFT,
        style: { paragraph: { indent: { left: 720, hanging: 360 } } } }]
    });
  }
  return configs;
}

/** 单个 bullets 引用配置（generate_report / generate_version_doc 的 numbering 配置） */
function makeBulletConfig() {
  return {
    reference: "bullets",
    levels: [{ level: 0, format: LevelFormat.BULLET, text: "\u2022", alignment: AlignmentType.LEFT,
      style: { paragraph: { indent: { left: 720, hanging: 360 } } } }]
  };
}

/* ============================================================
 * 编号组（文档正文中连续编号）
 * ============================================================ */

/** 十进制编号组：start() 开启新组，item(text) 追加编号项 */
function makeNumGroup() {
  const counter = { value: 0 };
  return {
    start() {
      counter.value++;
    },
    item(text) {
      return new Paragraph({
        numbering: { reference: `numbers-${counter.value}`, level: 0 },
        spacing: { after: 60 },
        children: [new TextRun({ text, font: font(), size: FONT_SIZE_BODY })]
      });
    }
  };
}

/** 项目符号组：start() 开启新组，item(text) 追加项目项（generate_opt_report 同构） */
function makeBulletGroup() {
  const counter = { value: 0 };
  return {
    start() {
      counter.value++;
    },
    item(text) {
      return bulletItem(text, `bullets-${counter.value}`);
    }
  };
}

module.exports = {
  // 常量
  FONT_ASCII, FONT_EN, FONT_CJK, FONT_CN, FONT_SIZE_BODY,
  PAGE_WIDTH, PAGE_HEIGHT, MARGIN, CONTENT_WIDTH,
  border, borders, TABLE_BORDER, TABLE_BORDERS, HEADER_SHADING, ALT_ROW_SHADING,
  // 函数
  font, heading, para, body, boldPara, boldBody, bulletItem,
  makeCell, makeTableRow, makeTable,
  makeNumberConfigs, makeBulletConfigs, makeBulletConfig,
  makeNumGroup, makeBulletGroup,
  pageBreakPara, emptyLine
};
