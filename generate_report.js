const fs = require("fs");
const path = require("path");
const {
  Document, Packer, Paragraph, TextRun, Table, TableRow, TableCell,
  Header, Footer, AlignmentType, LevelFormat,
  HeadingLevel, BorderStyle, WidthType, ShadingType,
  PageNumber, PageBreak
} = require("docx");

// ============================================================
// Helper functions
// ============================================================

const FONT_CJK = "Microsoft YaHei";
const FONT_ASCII = "Arial";

function fontRun(text, opts = {}) {
  const { bold, size, color, italic } = opts;
  return new TextRun({
    text,
    bold: bold || false,
    size: size || 21, // 10.5pt = 21 half-points
    font: { ascii: FONT_ASCII, hAnsi: FONT_ASCII, eastAsia: FONT_CJK },
    color: color || "000000",
    italics: italic || false,
  });
}

function bodyPara(text, opts = {}) {
  const { indent, spacing, bold, alignment } = opts;
  return new Paragraph({
    alignment: alignment || AlignmentType.JUSTIFIED,
    spacing: spacing || { after: 120, line: 360 },
    indent: indent || { firstLine: 420 },
    children: [fontRun(text, { bold: bold || false })],
  });
}

function bulletItem(text, level = 0) {
  return new Paragraph({
    numbering: { reference: "bullet-list", level },
    spacing: { after: 80, line: 360 },
    children: [fontRun(text)],
  });
}

// ============================================================
// Table helpers
// ============================================================

const border = { style: BorderStyle.SINGLE, size: 1, color: "999999" };
const borders = { top: border, bottom: border, left: border, right: border };

function headerCell(text, width) {
  return new TableCell({
    borders,
    width: { size: width, type: WidthType.DXA },
    shading: { fill: "D5E8F0", type: ShadingType.CLEAR },
    margins: { top: 80, bottom: 80, left: 120, right: 120 },
    verticalAlign: "center",
    children: [
      new Paragraph({
        alignment: AlignmentType.CENTER,
        spacing: { after: 0 },
        children: [fontRun(text, { bold: true, size: 20 })],
      }),
    ],
  });
}

function dataCell(text, width, opts = {}) {
  const { alignment } = opts;
  return new TableCell({
    borders,
    width: { size: width, type: WidthType.DXA },
    margins: { top: 60, bottom: 60, left: 120, right: 120 },
    verticalAlign: "center",
    children: [
      new Paragraph({
        alignment: alignment || AlignmentType.LEFT,
        spacing: { after: 0 },
        children: [fontRun(text, { size: 19 })],
      }),
    ],
  });
}

// ============================================================
// Build document content
// ============================================================

const children = [];

// ---- Title ----
children.push(
  new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { before: 600, after: 200 },
    children: [
      new TextRun({
        text: "Lv-00 \u529F\u80FD\u8865\u5168\u4E0E\u4EE3\u7801\u8D28\u91CF\u5168\u9762\u4F18\u5316\u4EFB\u52A1\u6C47\u62A5",
        bold: true,
        size: 36, // 18pt
        font: { ascii: FONT_ASCII, hAnsi: FONT_ASCII, eastAsia: FONT_CJK },
      }),
    ],
  })
);

children.push(
  new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { after: 400 },
    children: [
      new TextRun({
        text: "v11.0",
        bold: true,
        size: 28,
        font: { ascii: FONT_ASCII, hAnsi: FONT_ASCII, eastAsia: FONT_CJK },
        color: "444444",
      }),
    ],
  })
);

// ---- \u6982\u8FF0 ----
children.push(
  new Paragraph({
    heading: HeadingLevel.HEADING_1,
    spacing: { before: 360, after: 200 },
    children: [fontRun("\u6982\u8FF0", { bold: true, size: 28 })],
  })
);

children.push(
  bodyPara(
    "\u672C\u6B21\u4EFB\u52A1\u5BF9 Lv-00 \u7406\u8BBA\u6570\u5B66\u7814\u7A76\u5E73\u53F0\u8FDB\u884C\u4E86\u5168\u9762\u7684\u4EE3\u7801\u8D28\u91CF\u5BA1\u67E5\u4E0E\u4FEE\u590D\u3002\u9879\u76EE\u5305\u542B 16 \u4E2A\u6838\u5FC3 C \u6E90\u7801\u6587\u4EF6\u300136 \u4E2A\u5934\u6587\u4EF6\u300113 \u4E2A Python \u7ED1\u5B9A\u6587\u4EF6\u3001\u4EE5\u53CA\u591A\u4E2A\u8F85\u52A9\u6A21\u5757\u3002\u901A\u8FC7\u7CFB\u7EDF\u6027\u5206\u6790\uFF0C\u8BC6\u522B\u5E76\u4FEE\u590D\u4E86 6 \u7C7B\u4E25\u91CD Bug\u30015 \u7C7B\u5B89\u5168\u98CE\u9669\u30014 \u7C7B\u4E00\u81F4\u6027\u95EE\u9898\u30013 \u7C7B\u4EE3\u7801\u91CD\u590D\u95EE\u9898\uFF0C\u5E76\u5B8C\u5584\u4E86\u4E2D\u6587\u6CE8\u91CA\u548C\u4EE3\u7801\u98CE\u683C\u3002"
  )
);

// ============================================================
// \u4E00\u3001\u4E25\u91CD Bug \u4FEE\u590D
// ============================================================
children.push(
  new Paragraph({
    heading: HeadingLevel.HEADING_1,
    spacing: { before: 360, after: 200 },
    children: [fontRun("\u4E00\u3001\u4E25\u91CD Bug \u4FEE\u590D", { bold: true, size: 28 })],
  })
);

// 1.1
children.push(
  new Paragraph({
    heading: HeadingLevel.HEADING_2,
    spacing: { before: 240, after: 160 },
    children: [fontRun("1.1 engine.c use-after-free\uFF08\u4E25\u91CD\uFF09", { bold: true, size: 24 })],
  })
);
children.push(bulletItem("\u4F4D\u7F6E\uFF1Asrc/core/engine.c \u7B2C 1327-1332 \u884C"));
children.push(bulletItem("\u95EE\u9898\uFF1AENGINE_CIRCUIT_ACTION_DOWNGRADE \u5206\u652F\u4E2D\uFF0Csymbolic_coord_destroy(overflow_coord) \u5148\u91CA\u653E\u4E86\u5BF9\u8C61\u5185\u5B58\uFF0C\u968F\u540E\u53C8\u901A\u8FC7 overflow_coord->type \u8BBF\u95EE\u5DF2\u91CA\u653E\u5185\u5B58"));
children.push(bulletItem("\u4FEE\u590D\uFF1A\u91C7\u7528\u6570\u636E\u4EA4\u6362\u7B56\u7565\uFF0C\u5148\u5C06 overflow_coord \u7684\u65E7\u6570\u636E\u8F6C\u79FB\u5230 new_coord\uFF0C\u518D\u5C06\u65B0\u6570\u636E\u5199\u5165 overflow_coord\uFF0C\u6700\u540E\u901A\u8FC7 symbolic_coord_destroy(new_coord) \u5B89\u5168\u91CA\u653E"));

// 1.2
children.push(
  new Paragraph({
    heading: HeadingLevel.HEADING_2,
    spacing: { before: 240, after: 160 },
    children: [fontRun("1.2 proof.c \u65E0\u9650\u9012\u5F52\u98CE\u9669\uFF08\u4E25\u91CD\uFF09", { bold: true, size: 24 })],
  })
);
children.push(bulletItem("\u4F4D\u7F6E\uFF1Asrc/core/proof.c \u7B2C 127-134 \u884C"));
children.push(bulletItem("\u95EE\u9898\uFF1A\u8FED\u4EE3\u9500\u6BC1\u547D\u9898\u65F6\uFF0C\u6808\u6269\u5BB9\u5931\u8D25\u56DE\u9000\u5230\u8C03\u7528 proposition_destroy()\uFF0C\u8BE5\u51FD\u6570\u5185\u90E8\u53C8\u8C03\u7528\u81EA\u8EAB\uFF0C\u5F62\u6210\u65E0\u9650\u9012\u5F52"));
children.push(bulletItem("\u4FEE\u590D\uFF1A\u6808\u6269\u5BB9\u5931\u8D25\u65F6\u6539\u4E3A\u4EC5\u901A\u8FC7 lv00_free() \u91CA\u653E\u5269\u4F59\u5B50\u547D\u9898\u7684\u7ED3\u6784\u4F53\u5916\u58F3\uFF0C\u6DFB\u52A0\u8B66\u544A\u65E5\u5FD7\uFF0C\u907F\u514D\u9012\u5F52\u5BFC\u81F4\u6808\u6EA2\u51FA"));

// 1.3
children.push(
  new Paragraph({
    heading: HeadingLevel.HEADING_2,
    spacing: { before: 240, after: 160 },
    children: [fontRun("1.3 LV00_SAFE_FREE \u5B8F\u7C7B\u578B\u9519\u8BEF\uFF08\u4E25\u91CD\uFF09", { bold: true, size: 24 })],
  })
);
children.push(bulletItem("\u4F4D\u7F6E\uFF1Ainclude/lv00/lv00_utils.h \u7B2C 412 \u884C"));
children.push(bulletItem("\u95EE\u9898\uFF1Alv00_free \u7B7E\u540D\u4E3A void lv00_free(void **ptr)\uFF0C\u4F46\u5B8F\u4E2D\u76F4\u63A5\u4F20 ptr \u800C\u975E &ptr\uFF0C\u7C7B\u578B\u4E0D\u5339\u914D"));
children.push(bulletItem("\u4FEE\u590D\uFF1A\u6539\u4E3A lv00_free((void **)&(ptr))"));

// 1.4
children.push(
  new Paragraph({
    heading: HeadingLevel.HEADING_2,
    spacing: { before: 240, after: 160 },
    children: [fontRun("1.4 func_block_preset_ops.h ODR \u8FDD\u89C4\uFF08\u4E25\u91CD\uFF09", { bold: true, size: 24 })],
  })
);
children.push(bulletItem("\u4F4D\u7F6E\uFF1Ainclude/lv00/func_block_preset_ops.h \u7B2C 288 \u884C"));
children.push(bulletItem("\u95EE\u9898\uFF1A\u5934\u6587\u4EF6\u4E2D\u5B9A\u4E49 static \u51FD\u6570 preset_compose\uFF0C\u6BCF\u4E2A\u7F16\u8BD1\u5355\u5143\u751F\u6210\u72EC\u7ACB\u526F\u672C"));
children.push(bulletItem("\u4FEE\u590D\uFF1A\u79FB\u9664 static \u5173\u952E\u5B57\uFF0C\u6539\u4E3A\u666E\u901A\u51FD\u6570\u58F0\u660E"));

// 1.5
children.push(
  new Paragraph({
    heading: HeadingLevel.HEADING_2,
    spacing: { before: 240, after: 160 },
    children: [fontRun("1.5 symbolic_coord.h \u7F13\u51B2\u533A\u6EA2\u51FA\u98CE\u9669\uFF08\u4E25\u91CD\uFF09", { bold: true, size: 24 })],
  })
);
children.push(bulletItem("\u4F4D\u7F6E\uFF1Ainclude/lv00/symbolic_coord.h \u7B2C 90\u300196 \u884C"));
children.push(bulletItem("\u95EE\u9898\uFF1Abase_name \u4EC5 8 \u5B57\u8282\u3001name \u4EC5 32 \u5B57\u8282\uFF0C\u957F\u5E38\u91CF\u540D\u4F1A\u5BFC\u81F4\u6EA2\u51FA"));
children.push(bulletItem("\u4FEE\u590D\uFF1Abase_name \u6269\u5927\u4E3A 16 \u5B57\u8282\uFF0Cname \u6269\u5927\u4E3A 64 \u5B57\u8282"));

// 1.6
children.push(
  new Paragraph({
    heading: HeadingLevel.HEADING_2,
    spacing: { before: 240, after: 160 },
    children: [fontRun("1.6 constraint_graph.c \u54C8\u5E0C\u8868\u65E0\u9650\u5FAA\u73AF\uFF08\u4E2D\u7B49\uFF09", { bold: true, size: 24 })],
  })
);
children.push(bulletItem("\u4F4D\u7F6E\uFF1Asrc/core/constraint_graph.c \u7B2C 419-422 \u884C"));
children.push(bulletItem("\u95EE\u9898\uFF1A\u5F00\u653E\u5BFB\u5740\u5220\u9664\u7684\u91CD\u65B0\u63D2\u5165\u5FAA\u73AF\u7F3A\u5C11\u7EC8\u6B62\u6761\u4EF6"));
children.push(bulletItem("\u4FEE\u590D\uFF1A\u6DFB\u52A0 j != idx \u6761\u4EF6\u9632\u6B62\u54C8\u5E0C\u8868\u6EE1\u65F6\u65E0\u9650\u5FAA\u73AF"));

// ============================================================
// \u4E8C\u3001\u5B89\u5168\u98CE\u9669\u4FEE\u590D
// ============================================================
children.push(
  new Paragraph({
    heading: HeadingLevel.HEADING_1,
    spacing: { before: 360, after: 200 },
    children: [fontRun("\u4E8C\u3001\u5B89\u5168\u98CE\u9669\u4FEE\u590D", { bold: true, size: 28 })],
  })
);

// 2.1
children.push(
  new Paragraph({
    heading: HeadingLevel.HEADING_2,
    spacing: { before: 240, after: 160 },
    children: [fontRun("2.1 CORS \u901A\u914D\u7B26\uFF08\u9AD8\uFF09", { bold: true, size: 24 })],
  })
);
children.push(bulletItem("\u4F4D\u7F6E\uFF1Aconcurrent_monitor/web/routes.py"));
children.push(bulletItem("\u4FEE\u590D\uFF1A\u5C06 Access-Control-Allow-Origin \u4ECE \"*\" \u6539\u4E3A\u4ECE\u73AF\u5883\u53D8\u91CF CORS_ORIGINS \u8BFB\u53D6\uFF0C\u9ED8\u8BA4\u4EC5\u5141\u8BB8 localhost"));

// 2.2
children.push(
  new Paragraph({
    heading: HeadingLevel.HEADING_2,
    spacing: { before: 240, after: 160 },
    children: [fontRun("2.2 ai_engine.py \u5168\u5C40\u72B6\u6001\u4FEE\u6539\uFF08\u4E2D\uFF09", { bold: true, size: 24 })],
  })
);
children.push(bulletItem("\u4F4D\u7F6E\uFF1Allm_coding_assistant/core/ai_engine.py"));
children.push(bulletItem("\u4FEE\u590D\uFF1A\u79FB\u9664\u5BF9 dashscope.api_key \u5168\u5C40\u53D8\u91CF\u7684\u4FEE\u6539\uFF0C\u6539\u4E3A\u901A\u8FC7 api_key \u53C2\u6570\u4F20\u9012"));

// 2.3
children.push(
  new Paragraph({
    heading: HeadingLevel.HEADING_2,
    spacing: { before: 240, after: 160 },
    children: [fontRun("2.3 debug.c \u591A\u7EBF\u7A0B\u5B89\u5168\uFF08\u4E2D\uFF09", { bold: true, size: 24 })],
  })
);
children.push(bulletItem("\u4F4D\u7F6E\uFF1Asrc/core/debug.c ref_count_get \u51FD\u6570"));
children.push(bulletItem("\u4FEE\u590D\uFF1A\u6DFB\u52A0\u52A0\u9501\u8BFB\u53D6\uFF0C\u4FDD\u8BC1\u4E0E ref_count_inc/dec \u7684\u4E92\u65A5\u4E00\u81F4\u6027"));

// 2.4
children.push(
  new Paragraph({
    heading: HeadingLevel.HEADING_2,
    spacing: { before: 240, after: 160 },
    children: [fontRun("2.4 solver.c \u5185\u5B58\u5206\u914D\u5668\u6DF7\u7528\uFF08\u4E2D\uFF09", { bold: true, size: 24 })],
  })
);
children.push(bulletItem("\u4F4D\u7F6E\uFF1Asrc/core/solver.c \u591A\u5904"));
children.push(bulletItem("\u4FEE\u590D\uFF1A\u5C06 16 \u5904\u9519\u8BEF\u4F7F\u7528 lv00_malloc \u7684 poly.coeffs \u5206\u914D\u7EDF\u4E00\u6539\u4E3A malloc\uFF0C\u4E0E mpz_poly_clear() \u4E2D\u7684 free() \u914D\u5BF9"));

// 2.5
children.push(
  new Paragraph({
    heading: HeadingLevel.HEADING_2,
    spacing: { before: 240, after: 160 },
    children: [fontRun("2.5 code_analyzer.py \u6CE8\u91CA\u68C0\u6D4B\u8BEF\u5224\uFF08\u4F4E\uFF09", { bold: true, size: 24 })],
  })
);
children.push(bulletItem("\u4F4D\u7F6E\uFF1Allm_coding_assistant/core/code_analyzer.py"));
children.push(bulletItem("\u4FEE\u590D\uFF1A\u6DFB\u52A0 _is_python_comment() \u65B9\u6CD5\u6392\u9664\u5B57\u7B26\u4E32\u4E2D\u7684 # \u53F7\uFF1B\u6269\u5C55 TODO \u68C0\u6D4B\u4E3A TODO/FIXME/HACK/XXX"));

// ============================================================
// \u4E09\u3001\u4E00\u81F4\u6027\u95EE\u9898\u4FEE\u590D
// ============================================================
children.push(
  new Paragraph({
    heading: HeadingLevel.HEADING_1,
    spacing: { before: 360, after: 200 },
    children: [fontRun("\u4E09\u3001\u4E00\u81F4\u6027\u95EE\u9898\u4FEE\u590D", { bold: true, size: 28 })],
  })
);

// 3.1
children.push(
  new Paragraph({
    heading: HeadingLevel.HEADING_2,
    spacing: { before: 240, after: 160 },
    children: [fontRun("3.1 \u7248\u672C\u53F7\u7EDF\u4E00", { bold: true, size: 24 })],
  })
);
children.push(bodyPara("\u5C06\u6240\u6709\u6A21\u5757\u7248\u672C\u53F7\u7EDF\u4E00\u4E3A 3.0.2\uFF1A", { indent: { firstLine: 0 } }));
children.push(bulletItem("lv00.h\uFF1A3.0.1 -> 3.0.2"));
children.push(bulletItem("func_block_preset.h\uFF1A4.0.0 -> 3.0.2"));
children.push(bulletItem("high_dim.h\uFF1A3.0.0 -> 3.0.2"));
children.push(bulletItem("CMakeLists.txt\uFF1A3.1.0 -> 3.0.2"));
children.push(bulletItem("concurrent_monitor/__init__.py\uFF1A3.1.0 -> 3.0.2"));

// 3.2
children.push(
  new Paragraph({
    heading: HeadingLevel.HEADING_2,
    spacing: { before: 240, after: 160 },
    children: [fontRun("3.2 Python \u7248\u672C\u8981\u6C42\u7EDF\u4E00", { bold: true, size: 24 })],
  })
);
children.push(bodyPara("\u5C06\u6240\u6709 Python \u7248\u672C\u8981\u6C42\u7EDF\u4E00\u4E3A >= 3.10\uFF1A", { indent: { firstLine: 0 } }));
children.push(bulletItem("llm_coding_assistant/start.py\uFF1A>=3.8 -> >=3.10"));
children.push(bulletItem("CHANGELOG.md\uFF1APython: 3.8+ -> Python: 3.10+"));

// ============================================================
// \u56DB\u3001\u4EE3\u7801\u8D28\u91CF\u6539\u8FDB
// ============================================================
children.push(
  new Paragraph({
    heading: HeadingLevel.HEADING_1,
    spacing: { before: 360, after: 200 },
    children: [fontRun("\u56DB\u3001\u4EE3\u7801\u8D28\u91CF\u6539\u8FDB", { bold: true, size: 28 })],
  })
);

// 4.1
children.push(
  new Paragraph({
    heading: HeadingLevel.HEADING_2,
    spacing: { before: 240, after: 160 },
    children: [fontRun("4.1 \u6B7B\u4EE3\u7801\u6E05\u7406", { bold: true, size: 24 })],
  })
);
children.push(bulletItem("engine.c \u7B2C 580-584 \u884C\uFF1A\u79FB\u9664 if/else \u4E24\u4E2A\u5B8C\u5168\u76F8\u540C\u7684\u5206\u652F"));
children.push(bulletItem("engine.c \u7B2C 824 \u884C\uFF1A\u4FEE\u590D\u6CE8\u91CA\u8BED\u6CD5\u9519\u8BEF\uFF08\u591A\u4F59\u7684 \u3011 \u5B57\u7B26\uFF09"));

// 4.2
children.push(
  new Paragraph({
    heading: HeadingLevel.HEADING_2,
    spacing: { before: 240, after: 160 },
    children: [fontRun("4.2 \u6027\u80FD\u4F18\u5316", { bold: true, size: 24 })],
  })
);
children.push(bulletItem("solver.c\uFF1A5 \u5904\u5FAA\u73AF\u4E2D\u91CD\u590D\u7684 strlen(prefix) \u8C03\u7528\u7F13\u5B58\u5230\u5C40\u90E8\u53D8\u91CF"));

// 4.3
children.push(
  new Paragraph({
    heading: HeadingLevel.HEADING_2,
    spacing: { before: 240, after: 160 },
    children: [fontRun("4.3 \u91CD\u590D\u5B9A\u4E49\u6CBB\u7406", { bold: true, size: 24 })],
  })
);
children.push(bulletItem("debug.h \u65E5\u5FD7\u7EA7\u522B\u679A\u4E3E\uFF1A\u6DFB\u52A0\u6CE8\u91CA\u8BF4\u660E\u4E0E lv00_internal.h \u7684\u5173\u7CFB"));
children.push(bulletItem("module.h MAX_MODULE_DEPTH\uFF1A\u6DFB\u52A0\u6CE8\u91CA\u8BF4\u660E\u6743\u5A01\u5B9A\u4E49\u5728 symbolic_coord.h"));
children.push(bulletItem("proof.c deep_copy \u51FD\u6570\uFF1A\u6DFB\u52A0\u5757\u6CE8\u91CA\u8BF4\u660E\u4FDD\u7559\u72EC\u7ACB\u5B9E\u73B0\u7684\u539F\u56E0"));

// 4.4
children.push(
  new Paragraph({
    heading: HeadingLevel.HEADING_2,
    spacing: { before: 240, after: 160 },
    children: [fontRun("4.4 \u6784\u5EFA\u914D\u7F6E\u4F18\u5316", { bold: true, size: 24 })],
  })
);
children.push(bulletItem("CMakeLists.txt\uFF1AGLOB_RECURSE \u6DFB\u52A0 CONFIGURE_DEPENDS \u9009\u9879"));
children.push(bulletItem("package.json\uFF1A\u8865\u5145 name\u3001version\u3001scripts \u5B57\u6BB5"));

// ============================================================
// \u4E94\u3001\u6CE8\u91CA\u4E0E\u98CE\u683C\u5B8C\u5584
// ============================================================
children.push(
  new Paragraph({
    heading: HeadingLevel.HEADING_1,
    spacing: { before: 360, after: 200 },
    children: [fontRun("\u4E94\u3001\u6CE8\u91CA\u4E0E\u98CE\u683C\u5B8C\u5584", { bold: true, size: 28 })],
  })
);

// 5.1
children.push(
  new Paragraph({
    heading: HeadingLevel.HEADING_2,
    spacing: { before: 240, after: 160 },
    children: [fontRun("5.1 \u4E2D\u6587\u6CE8\u91CA\u8865\u5145", { bold: true, size: 24 })],
  })
);
children.push(bulletItem("_ctypes_binding.py\uFF1A20+ \u5904\u5E38\u91CF\u5206\u7EC4\u6CE8\u91CA\u5347\u7EA7\u4E3A ===== \u683C\u5F0F\uFF0C9 \u4E2A\u51FD\u6570\u7B7E\u540D\u533A\u57DF\u6DFB\u52A0\u4E2D\u6587\u5206\u7EC4\u6CE8\u91CA"));
children.push(bulletItem("solver.c\uFF1Astrlen \u7F13\u5B58\u5904\u6DFB\u52A0\u6027\u80FD\u4F18\u5316\u8BF4\u660E\u6CE8\u91CA"));

// 5.2
children.push(
  new Paragraph({
    heading: HeadingLevel.HEADING_2,
    spacing: { before: 240, after: 160 },
    children: [fontRun("5.2 \u7C7B\u578B\u6CE8\u89E3\u8865\u5145", { bold: true, size: 24 })],
  })
);
children.push(bulletItem("core.py\uFF1APoint.__iter__\u3001Graph.__iter__\u3001Graph.__contains__ \u6DFB\u52A0\u8FD4\u56DE\u7C7B\u578B\u6CE8\u89E3"));

// 5.3
children.push(
  new Paragraph({
    heading: HeadingLevel.HEADING_2,
    spacing: { before: 240, after: 160 },
    children: [fontRun("5.3 \u6D4B\u8BD5\u8D28\u91CF\u6539\u8FDB", { bold: true, size: 24 })],
  })
);
children.push(bulletItem("test_graph.py\uFF1A\u5C06\u5BBD\u677E\u65AD\u8A00\uFF08assert g is not None\uFF09\u6539\u4E3A\u7CBE\u786E\u65AD\u8A00\uFF08assert isinstance(g, Graph)\uFF09"));

// ============================================================
// \u516D\u3001\u4FEE\u6539\u6587\u4EF6\u6E05\u5355
// ============================================================
children.push(
  new Paragraph({
    heading: HeadingLevel.HEADING_1,
    spacing: { before: 360, after: 200 },
    children: [fontRun("\u516D\u3001\u4FEE\u6539\u6587\u4EF6\u6E05\u5355", { bold: true, size: 28 })],
  })
);

const tableData = [
  ["src/core/engine.c", "Bug\u4FEE\u590D", "use-after-free\u3001\u6B7B\u4EE3\u7801\u3001\u6CE8\u91CA\u8BED\u6CD5"],
  ["src/core/proof.c", "Bug\u4FEE\u590D", "\u65E0\u9650\u9012\u5F52\u98CE\u9669\u3001deep_copy \u6CE8\u91CA"],
  ["src/core/solver.c", "\u5B89\u5168\u4FEE\u590D", "\u5185\u5B58\u5206\u914D\u5668\u7EDF\u4E00\u3001strlen \u7F13\u5B58"],
  ["src/core/debug.c", "\u5B89\u5168\u4FEE\u590D", "ref_count_get \u52A0\u9501"],
  ["src/core/constraint_graph.c", "Bug\u4FEE\u590D", "\u54C8\u5E0C\u8868\u65E0\u9650\u5FAA\u73AF"],
  ["include/lv00/lv00_utils.h", "Bug\u4FEE\u590D", "LV00_SAFE_FREE \u5B8F"],
  ["include/lv00/func_block_preset_ops.h", "Bug\u4FEE\u590D", "static \u51FD\u6570 ODR"],
  ["include/lv00/symbolic_coord.h", "\u5B89\u5168\u4FEE\u590D", "\u7F13\u51B2\u533A\u6269\u5927"],
  ["include/lv00/lv00.h", "\u4E00\u81F4\u6027", "\u7248\u672C\u53F7\u7EDF\u4E00"],
  ["include/lv00/func_block_preset.h", "\u4E00\u81F4\u6027", "\u7248\u672C\u53F7\u7EDF\u4E00"],
  ["include/lv00/high_dim.h", "\u4E00\u81F4\u6027", "\u7248\u672C\u53F7\u7EDF\u4E00"],
  ["include/lv00/debug.h", "\u4EE3\u7801\u8D28\u91CF", "\u65E5\u5FD7\u7EA7\u522B\u6CE8\u91CA"],
  ["include/lv00/module.h", "\u4EE3\u7801\u8D28\u91CF", "MAX_MODULE_DEPTH \u6CE8\u91CA"],
  ["CMakeLists.txt", "\u6784\u5EFA", "CONFIGURE_DEPENDS\u3001\u7248\u672C\u53F7"],
  ["package.json", "\u6784\u5EFA", "\u8865\u5145\u7F3A\u5931\u5B57\u6BB5"],
  ["python/lv00/_ctypes_binding.py", "\u6CE8\u91CA", "\u4E2D\u6587\u5206\u7EC4\u6CE8\u91CA"],
  ["python/lv00/core.py", "\u7C7B\u578B\u6CE8\u89E3", "\u8FD4\u56DE\u7C7B\u578B\u6CE8\u89E3"],
  ["python/tests/test_graph.py", "\u6D4B\u8BD5", "\u7CBE\u786E\u65AD\u8A00"],
  ["concurrent_monitor/__init__.py", "\u4E00\u81F4\u6027", "\u7248\u672C\u53F7\u7EDF\u4E00"],
  ["concurrent_monitor/web/routes.py", "\u5B89\u5168", "CORS \u9650\u5236"],
  ["llm_coding_assistant/start.py", "\u4E00\u81F4\u6027", "Python \u7248\u672C"],
  ["llm_coding_assistant/core/ai_engine.py", "\u5B89\u5168", "dashscope \u5168\u5C40\u72B6\u6001"],
  ["llm_coding_assistant/core/code_analyzer.py", "\u8D28\u91CF", "\u6CE8\u91CA\u68C0\u6D4B\u3001TODO \u6269\u5C55"],
  ["CHANGELOG.md", "\u4E00\u81F4\u6027", "Python \u7248\u672C"],
];

const COL1 = 4200;
const COL2 = 1600;
const COL3 = 3560;

const tableRows = [
  new TableRow({
    cantSplit: true,
    children: [
      headerCell("\u6587\u4EF6", COL1),
      headerCell("\u4FEE\u6539\u7C7B\u578B", COL2),
      headerCell("\u4FEE\u6539\u5185\u5BB9", COL3),
    ],
  }),
];

for (const row of tableData) {
  tableRows.push(
    new TableRow({
      cantSplit: true,
      children: [
        dataCell(row[0], COL1),
        dataCell(row[1], COL2, { alignment: AlignmentType.CENTER }),
        dataCell(row[2], COL3),
      ],
    })
  );
}

children.push(
  new Table({
    width: { size: COL1 + COL2 + COL3, type: WidthType.DXA },
    columnWidths: [COL1, COL2, COL3],
    rows: tableRows,
  })
);

// ============================================================
// \u4E03\u3001\u9057\u7559\u95EE\u9898\u4E0E\u5EFA\u8BAE
// ============================================================
children.push(
  new Paragraph({
    heading: HeadingLevel.HEADING_1,
    spacing: { before: 360, after: 200 },
    children: [fontRun("\u4E03\u3001\u9057\u7559\u95EE\u9898\u4E0E\u5EFA\u8BAE", { bold: true, size: 28 })],
  })
);

// 7.1
children.push(
  new Paragraph({
    heading: HeadingLevel.HEADING_2,
    spacing: { before: 240, after: 160 },
    children: [fontRun("7.1 \u67B6\u6784\u5C42\u9762\uFF08\u5EFA\u8BAE\u540E\u7EED\u8FED\u4EE3\u5904\u7406\uFF09", { bold: true, size: 24 })],
  })
);
children.push(bulletItem("deep_copy_port/geom_node \u5728 engine.c \u548C proof.c \u4E2D\u4ECD\u6709\u91CD\u590D\uFF0C\u5EFA\u8BAE\u5C06 engine.c \u7248\u672C\u63D0\u5347\u4E3A\u516C\u5171 API"));
children.push(bulletItem("\u8BCD\u6CD5\u5206\u6790\u5668\u5728 module.c \u548C axiom_pkg.c \u4E2D\u91CD\u590D\uFF0C\u5EFA\u8BAE\u5171\u4EAB lexer_shared.h \u57FA\u7840\u8BBE\u65BD"));
children.push(bulletItem("Python \u5C42\u5B58\u5728\u4E24\u5957 DSL\uFF08dsl.py \u548C py_euclid_style.py\uFF09\uFF0C\u5EFA\u8BAE\u5408\u5E76\u6216\u660E\u786E\u5206\u5DE5"));
children.push(bulletItem("\u591A\u4E2A\u8D85\u5927\u6587\u4EF6\uFF08core.py 1463\u884C\u3001dsl.py 1733\u884C\u3001preset_func_blocks.py 2086\u884C\uFF09\u5EFA\u8BAE\u62C6\u5206"));

// 7.2
children.push(
  new Paragraph({
    heading: HeadingLevel.HEADING_2,
    spacing: { before: 240, after: 160 },
    children: [fontRun("7.2 \u529F\u80FD\u5C42\u9762", { bold: true, size: 24 })],
  })
);
children.push(bulletItem("VF2 \u5339\u914D\u7F3A\u5C11\u8D85\u65F6\u673A\u5236"));
children.push(bulletItem("\u56FE\u5FEB\u7167\u7F3A\u5C11\u589E\u91CF\u652F\u6301"));
children.push(bulletItem("\u8BC1\u660E\u7CFB\u7EDF\u7F3A\u5C11\u81EA\u52A8\u5316\u9A8C\u8BC1\u673A\u5236"));
children.push(bulletItem("\u9AD8\u7EF4\u5757\u67E5\u627E\u4F7F\u7528\u7EBF\u6027\u641C\u7D22\uFF0C\u5927\u91CF\u5757\u65F6\u6027\u80FD\u5DEE"));

// 7.3
children.push(
  new Paragraph({
    heading: HeadingLevel.HEADING_2,
    spacing: { before: 240, after: 160 },
    children: [fontRun("7.3 \u6D4B\u8BD5\u5C42\u9762", { bold: true, size: 24 })],
  })
);
children.push(bulletItem("\u6D4B\u8BD5\u8986\u76D6\u7387\u4E0D\u8DB3\uFF0C\u591A\u4E2A\u6A21\u5757\u7F3A\u5C11\u5355\u5143\u6D4B\u8BD5"));
children.push(bulletItem("test_streaming_e2e.py \u4E2D\u901F\u7387\u9650\u5236\u6D4B\u8BD5\u5B9E\u9645\u672A\u9A8C\u8BC1\u8017\u65F6"));
children.push(bulletItem("\u7F3A\u5C11\u8FB9\u754C\u60C5\u51B5\u548C\u9519\u8BEF\u8DEF\u5F84\u6D4B\u8BD5"));

// ============================================================
// Assemble Document
// ============================================================

const doc = new Document({
  styles: {
    default: {
      document: {
        run: {
          font: { ascii: FONT_ASCII, hAnsi: FONT_ASCII, eastAsia: FONT_CJK },
          size: 21, // 10.5pt
        },
      },
    },
    paragraphStyles: [
      {
        id: "Heading1",
        name: "Heading 1",
        basedOn: "Normal",
        next: "Normal",
        quickFormat: true,
        run: {
          size: 28,
          bold: true,
          font: { ascii: FONT_ASCII, hAnsi: FONT_ASCII, eastAsia: FONT_CJK },
          color: "1F2937",
        },
        paragraph: {
          spacing: { before: 360, after: 200 },
          outlineLevel: 0,
          keepNext: false,
          keepLines: false,
        },
      },
      {
        id: "Heading2",
        name: "Heading 2",
        basedOn: "Normal",
        next: "Normal",
        quickFormat: true,
        run: {
          size: 24,
          bold: true,
          font: { ascii: FONT_ASCII, hAnsi: FONT_ASCII, eastAsia: FONT_CJK },
          color: "374151",
        },
        paragraph: {
          spacing: { before: 240, after: 160 },
          outlineLevel: 1,
          keepNext: false,
          keepLines: false,
        },
      },
    ],
  },
  numbering: {
    config: [
      {
        reference: "bullet-list",
        levels: [
          {
            level: 0,
            format: LevelFormat.BULLET,
            text: "\u2022",
            alignment: AlignmentType.LEFT,
            style: {
              paragraph: { indent: { left: 720, hanging: 360 } },
            },
          },
          {
            level: 1,
            format: LevelFormat.BULLET,
            text: "\u25E6",
            alignment: AlignmentType.LEFT,
            style: {
              paragraph: { indent: { left: 1080, hanging: 360 } },
            },
          },
        ],
      },
    ],
  },
  sections: [
    {
      properties: {
        page: {
          size: {
            width: 11906, // A4
            height: 16838,
          },
          margin: {
            top: 1440,
            right: 1440,
            bottom: 1440,
            left: 1440,
          },
        },
      },
      headers: {
        default: new Header({
          children: [
            new Paragraph({
              alignment: AlignmentType.RIGHT,
              children: [
                fontRun("Lv-00 \u529F\u80FD\u8865\u5168\u4E0E\u4EE3\u7801\u8D28\u91CF\u5168\u9762\u4F18\u5316\u4EFB\u52A1\u6C47\u62A5 v11.0", {
                  size: 16,
                  color: "999999",
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
                fontRun("\u7B2C ", { size: 16, color: "999999" }),
                new TextRun({
                  children: [PageNumber.CURRENT],
                  font: { ascii: FONT_ASCII, hAnsi: FONT_ASCII, eastAsia: FONT_CJK },
                  size: 16,
                  color: "999999",
                }),
                fontRun(" \u9875", { size: 16, color: "999999" }),
              ],
            }),
          ],
        }),
      },
      children,
    },
  ],
});

// ============================================================
// Write file
// ============================================================

const OUTPUT_DIR = "c:\\Users\\xingg\\Documents\\trae_projects\\Lv-00\\docs\\reports";
const OUTPUT_FILE = path.join(
  OUTPUT_DIR,
  "Lv-00_\u529F\u80FD\u8865\u5168\u4E0E\u4EE3\u7801\u8D28\u91CF\u5168\u9762\u4F18\u5316\u4EFB\u52A1\u6C47\u62A5_v11.0.docx"
);

// Ensure directory exists
if (!fs.existsSync(OUTPUT_DIR)) {
  fs.mkdirSync(OUTPUT_DIR, { recursive: true });
}

Packer.toBuffer(doc).then((buffer) => {
  fs.writeFileSync(OUTPUT_FILE, buffer);
  console.log("Document generated successfully:", OUTPUT_FILE);
});
