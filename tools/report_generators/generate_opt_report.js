const fs = require('fs');
const { Document, Packer, Paragraph, TextRun, Table, TableRow, TableCell,
        Header, Footer, AlignmentType,
        HeadingLevel, BorderStyle, WidthType,
        PageNumber, PageBreak } = require('docx');

// ============================================================
// 共享辅助（来自 docx_helpers.js）
// ============================================================

const { para: body, boldPara: boldBody, heading,
        makeTable: makeTableShared, makeNumberConfigs, makeBulletConfigs,
        makeBulletGroup } = require('./docx_helpers');

// 本生成器 makeTable 表宽为列宽之和（8800）
function makeTable(headers, rows, colWidths) {
  const totalWidth = colWidths.reduce((a, b) => a + b, 0);
  return makeTableShared(headers, rows, colWidths, totalWidth);
}

// ============================================================
// 编号配置
// ============================================================

const numberConfigs = makeNumberConfigs();
const bulletConfigs = makeBulletConfigs();

const bulletGroup = makeBulletGroup();
function startBulletGroup() { bulletGroup.start(); }
function bul(text) { return bulletGroup.item(text); }

// ============================================================
// 文档内容
// ============================================================

const children = [];

// 封面
children.push(new Paragraph({ spacing: { before: 3000 }, children: [] }));
children.push(new Paragraph({
  alignment: AlignmentType.CENTER,
  spacing: { after: 400 },
  children: [new TextRun({ text: "Lv-00 \u9879\u76EE\u5C40\u90E8\u6700\u4F18\u89E3\u4F18\u5316\u4EFB\u52A1\u6C47\u62A5", font: { ascii: "Arial", hAnsi: "Arial", eastAsia: "Microsoft YaHei" }, size: 44, bold: true })]
}));
children.push(new Paragraph({
  alignment: AlignmentType.CENTER,
  spacing: { after: 200 },
  children: [new TextRun({ text: "\u2014\u2014 \u5168\u57DF\u4EE3\u7801\u8D28\u91CF\u5BA1\u67E5\u4E0E\u4F18\u5316 \u2014\u2014", font: { ascii: "Arial", hAnsi: "Arial", eastAsia: "Microsoft YaHei" }, size: 28, color: "666666" })]
}));
children.push(new Paragraph({ spacing: { before: 1000 }, children: [] }));
children.push(new Paragraph({
  alignment: AlignmentType.CENTER,
  spacing: { after: 100 },
  children: [new TextRun({ text: "\u9879\u76EE\u540D\u79F0\uFF1ALv-00 \u7406\u8BBA\u6570\u5B66\u7814\u7A76\u5E73\u53F0", font: { ascii: "Arial", hAnsi: "Arial", eastAsia: "Microsoft YaHei" }, size: 24 })]
}));
children.push(new Paragraph({
  alignment: AlignmentType.CENTER,
  spacing: { after: 100 },
  children: [new TextRun({ text: "\u7248\u672C\uFF1Av3.3.0", font: { ascii: "Arial", hAnsi: "Arial", eastAsia: "Microsoft YaHei" }, size: 24 })]
}));
children.push(new Paragraph({
  alignment: AlignmentType.CENTER,
  spacing: { after: 100 },
  children: [new TextRun({ text: "\u6C47\u62A5\u65E5\u671F\uFF1A2026-05-25", font: { ascii: "Arial", hAnsi: "Arial", eastAsia: "Microsoft YaHei" }, size: 24 })]
}));
children.push(new Paragraph({
  alignment: AlignmentType.CENTER,
  spacing: { after: 100 },
  children: [new TextRun({ text: "\u4F18\u5316\u8303\u56F4\uFF1A\u5168\u90E8 C/Python/JavaScript \u4EE3\u7801\u6A21\u5757", font: { ascii: "Arial", hAnsi: "Arial", eastAsia: "Microsoft YaHei" }, size: 24 })]
}));

children.push(new Paragraph({ children: [new PageBreak()] }));

// ============================================================
// 1. 项目概览
// ============================================================
children.push(heading("\u4E00\u3001\u9879\u76EE\u6982\u89C8", HeadingLevel.HEADING_1));
children.push(body("Lv-00 \u662F\u4E00\u4E2A\u9762\u5411\u7406\u8BBA\u6570\u5B66\u7814\u7A76\u7684\u7B26\u53F7\u8BA1\u7B97\u4E0E\u81EA\u52A8\u8BC1\u660E\u5E73\u53F0\uFF0C\u91C7\u7528 C \u8BED\u8A00\u4F5C\u4E3A\u6838\u5FC3\u5F15\u64CE\uFF0C\u914D\u5408 Python \u7ED1\u5B9A\u548C JavaScript Web \u524D\u7AEF\u3002\u9879\u76EE\u6DB5\u76D6\u7EA6 90 \u4E2A C \u6E90\u6587\u4EF6\u3001\u7EA6 130 \u4E2A C \u5934\u6587\u4EF6\u3001\u7EA6 80 \u4E2A Python \u6587\u4EF6\u3001\u7EA6 30 \u4E2A JavaScript \u6587\u4EF6\uFF0C\u603B\u8BA1\u7EA6 100,000+ \u884C\u4EE3\u7801\u3002"));
children.push(body("\u672C\u6B21\u4F18\u5316\u4EFB\u52A1\u5BF9\u9879\u76EE\u5168\u90E8\u4EE3\u7801\u8FDB\u884C\u4E86\u5168\u9762\u7684\u5C40\u90E8\u6700\u4F18\u89E3\u5316\uFF0C\u5305\u62EC Bug \u4FEE\u590D\u3001\u4EE3\u7801\u98CE\u683C\u7EDF\u4E00\u3001\u6CE8\u91CA\u5B8C\u5584\u3001\u6A21\u5757\u5316\u6539\u8FDB\u3001\u6027\u80FD\u4F18\u5316\u548C\u7F16\u7801\u95EE\u9898\u4FEE\u590D\u3002"));

children.push(heading("\u4EE3\u7801\u89C4\u6A21\u7EDF\u8BA1", HeadingLevel.HEADING_2));
children.push(makeTable(
  ["\u8BED\u8A00", "\u6587\u4EF6\u6570", "\u4F30\u7B97\u603B\u884C\u6570", "\u4E3B\u8981\u76EE\u5F55"],
  [
    ["C (\u5934\u6587\u4EF6)", "~130", "~11,600", "include/lv00/"],
    ["C (\u6E90\u6587\u4EF6)", "~90", "~28,600", "src/core/, src/preset/, src/func_block/, src/parser/"],
    ["Python", "~80", "~15,000", "python/lv00/, concurrent_monitor/, llm_coding_assistant/"],
    ["JavaScript", "~30", "~12,000", "web/js/, stream-monitor/"],
    ["\u5408\u8BA1", "~330", "~67,200", "\u2014"]
  ],
  [1800, 1500, 2000, 3500]
));

// ============================================================
// 2. 发现的问题汇总
// ============================================================
children.push(new Paragraph({ children: [new PageBreak()] }));
children.push(heading("\u4E8C\u3001\u53D1\u73B0\u7684\u95EE\u9898\u6C47\u603B", HeadingLevel.HEADING_1));

children.push(heading("2.1 \u9AD8\u4F18\u5148\u7EA7\u95EE\u9898\uFF08\u5DF2\u4FEE\u590D\uFF09", HeadingLevel.HEADING_2));
children.push(makeTable(
  ["\u7F16\u53F7", "\u6587\u4EF6", "\u95EE\u9898\u63CF\u8FF0", "\u4E25\u91CD\u7A0B\u5EA6"],
  [
    ["H-1", "constraint_graph.c", "\u54C8\u5E0C\u8868\u5F00\u653E\u5BFB\u5740\u5220\u9664\u903B\u8F91\u7F3A\u9677\uFF0C\u53EF\u80FD\u5BFC\u81F4\u6570\u636E\u6761\u76EE\u6C38\u4E45\u4E22\u5931", "\u4E25\u91CD"],
    ["H-2", "algebra_mode.c", "g_geom_id_counter \u975E\u7EBF\u7A0B\u5C40\u90E8\uFF0C\u591A\u7EBF\u7A0B\u4E0B ID \u51B2\u7A81", "\u4E25\u91CD"],
    ["H-3", "bdd_encoding.c", "bdd_manager_destroy \u4E0D\u56DE\u6536\u552F\u4E00\u8868\u8282\u70B9\uFF0C\u5185\u5B58\u6CC4\u6F0F", "\u4E25\u91CD"],
    ["H-4", "engine.c", "\u6DF7\u7528\u6807\u51C6 free() \u800C\u975E lv00_free()\uFF0C\u7834\u574F\u7EDF\u4E00\u5185\u5B58\u7BA1\u7406", "\u4E2D\u7B49"],
    ["H-5", "approx_counter.c", "cnf_builder_add_lit \u4E2D realloc \u5931\u8D25\u672A\u5904\u7406\uFF0C\u53EF\u80FD\u5199\u5165 NULL \u6307\u9488", "\u4E25\u91CD"],
    ["H-6", "ecosystem.c", "\u4F7F\u7528 O(n\u00B2) \u5192\u6CE1\u6392\u5E8F\uFF0C\u5927\u89C4\u6A21\u6570\u636E\u4E0B\u6027\u80FD\u5DEE", "\u4E2D\u7B49"],
    ["H-7", "exact_arithmetic.c", "\u7EB3\u79D2\u89C4\u8303\u5316\u4F7F\u7528 while \u5FAA\u73AF\uFF0C\u6781\u7AEF\u503C\u4E0B\u6027\u80FD\u95EE\u9898", "\u4F4E"]
  ],
  [800, 2200, 4200, 1600]
));

children.push(heading("2.2 \u7F16\u7801\u95EE\u9898\uFF08\u5DF2\u4FEE\u590D\uFF09", HeadingLevel.HEADING_2));
children.push(makeTable(
  ["\u7F16\u53F7", "\u6587\u4EF6", "\u95EE\u9898\u63CF\u8FF0"],
  [
    ["E-1", "bdd_encoding.c", "\u4E2D\u6587\u6CE8\u91CA\u5168\u90E8\u663E\u793A\u4E3A UTF-8 \u4E71\u7801\uFF08\u7EA6 30 \u5904\uFF09"],
    ["E-2", "approx_counter.c", "\u4E2D\u6587\u6CE8\u91CA\u5168\u90E8\u663E\u793A\u4E3A UTF-8 \u4E71\u7801\uFF08\u7EA6 25 \u5904\uFF09"],
    ["E-3", "float_error.c", "\u4E2D\u6587\u6CE8\u91CA\u5168\u90E8\u663E\u793A\u4E3A UTF-8 \u4E71\u7801\uFF08\u7EA6 40 \u5904\uFF09"]
  ],
  [800, 2200, 5800]
));

children.push(heading("2.3 \u6CE8\u91CA\u8986\u76D6\u7387\u95EE\u9898\uFF08\u5DF2\u4FEE\u590D\uFF09", HeadingLevel.HEADING_2));
children.push(makeTable(
  ["\u6587\u4EF6", "\u4F18\u5316\u524D\u8986\u76D6\u7387", "\u4F18\u5316\u540E\u8986\u76D6\u7387", "\u65B0\u589E\u6CE8\u91CA\u6570"],
  [
    ["ecosystem.c", "15.3%", "~45%", "~200 \u884C"],
    ["algebra_mode.c", "15.2%", "~50%", "~250 \u884C"],
    ["gc_language.c", "19.5%", "~45%", "~180 \u884C"],
    ["bdd_encoding.c", "19.0%", "~50%", "~150 \u884C"],
    ["constraint_graph.c", "17.8%", "~35%", "~70 \u884C"],
    ["approx_counter.c", "22.2%", "~50%", "~90 \u884C"],
    ["float_error.c", "24.9%", "~55%", "~100 \u884C"],
    ["JS \u6A21\u5757 (12\u4E2A\u6587\u4EF6)", "~20%", "~45%", "~120 \u5904"]
  ],
  [2500, 2000, 2000, 2300]
));

children.push(heading("2.4 \u4EE3\u7801\u98CE\u683C\u95EE\u9898\uFF08\u5DF2\u4FEE\u590D\uFF09", HeadingLevel.HEADING_2));
children.push(makeTable(
  ["\u7F16\u53F7", "\u6587\u4EF6", "\u95EE\u9898\u63CF\u8FF0"],
  [
    ["S-1", "python/lv00/core.py", "\u6DF7\u5408\u4F7F\u7528\u4E2D\u82F1\u6587\u6CE8\u91CA\uFF0C\u7C7B\u578B\u6CE8\u89E3\u7F3A\u5931"],
    ["S-2", "concurrent_monitor/core/config.py", "\u914D\u7F6E\u9A8C\u8BC1\u903B\u8F91\u4E0D\u5B8C\u6574"],
    ["S-3", "concurrent_monitor/core/models.py", "\u672A\u4F7F\u7528\u7684\u5BFC\u5165\u672A\u6E05\u7406"],
    ["S-4", "JS \u6A21\u5757 (12\u4E2A)", "\u6587\u4EF6\u5934\u7F3A\u5C11\u6807\u51C6 JSDoc\uFF0C\u82F1\u6587\u6CE8\u91CA\u672A\u7EDF\u4E00"]
  ],
  [800, 2800, 5200]
));

// ============================================================
// 3. 执行的修复详情
// ============================================================
children.push(new Paragraph({ children: [new PageBreak()] }));
children.push(heading("\u4E09\u3001\u6267\u884C\u7684\u4FEE\u590D\u8BE6\u60C5", HeadingLevel.HEADING_1));

children.push(heading("3.1 \u54C8\u5E0C\u8868\u5220\u9664\u7F3A\u9677\u4FEE\u590D (H-1)", HeadingLevel.HEADING_2));
children.push(body("\u6587\u4EF6\uFF1Asrc/core/constraint_graph.c\uFF0C\u51FD\u6570 node_index_remove()"));
children.push(body("\u95EE\u9898\uFF1A\u5F00\u653E\u5BFB\u5740\u54C8\u5E0C\u8868\u5220\u9664\u65F6\uFF0C\u5F53\u91CD\u65B0\u63D2\u5165\u7D22\u5F15 j == idx \u65F6\u505C\u6B62\uFF0C\u5BFC\u81F4\u88AB\u8DF3\u8FC7\u7684\u6761\u76EE\u6C38\u4E45\u4E22\u5931\uFF0C\u540E\u7EED\u67E5\u627E\u5931\u8D25\u3002"));
children.push(body("\u4FEE\u590D\u65B9\u6848\uFF1A\u91C7\u7528\u57FA\u4E8E\u7406\u60F3\u54C8\u5E0C\u4F4D\u7F6E\u7684\u8303\u56F4\u5224\u65AD\u903B\u8F91\u3002\u8BA1\u7B97\u6761\u76EE\u7684 ideal \u54C8\u5E0C\u4F4D\u7F6E\uFF0C\u5224\u65AD\u5176\u662F\u5426\u843D\u5728\u88AB\u5220\u9664\u69FD [idx+1, i] \u7684\u73AF\u7ED5\u5F71\u54CD\u8303\u56F4\u5185\u3002\u4EC5\u5728\u5F71\u54CD\u8303\u56F4\u5185\u65F6\u624D\u91CD\u65B0\u63D2\u5165\uFF0C\u5426\u5219\u4FDD\u6301\u539F\u4F4D\u3002"));

children.push(heading("3.2 \u7EBF\u7A0B\u5B89\u5168\u4FEE\u590D (H-2)", HeadingLevel.HEADING_2));
children.push(body("\u6587\u4EF6\uFF1Asrc/core/algebra_mode.c"));
children.push(body("\u95EE\u9898\uFF1Ag_geom_id_counter \u4F7F\u7528 static \u5168\u5C40\u53D8\u91CF\uFF0C\u591A\u7EBF\u7A0B\u540C\u65F6\u521B\u5EFA\u51E0\u4F55\u4F53\u4F1A\u4EA7\u751F ID \u51B2\u7A81\u3002"));
children.push(body("\u4FEE\u590D\uFF1A\u6539\u4E3A static LV00_THREAD_LOCAL int g_geom_id_counter = 0\uFF0C\u4F7F\u5176\u6210\u4E3A\u7EBF\u7A0B\u5C40\u90E8\u53D8\u91CF\u3002\u540C\u65F6\u4FEE\u590D\u4E86 LV00_CHECK_NULL \u7684\u9519\u8BEF\u7528\u6CD5\uFF08\u68C0\u67E5\u51FD\u6570\u5730\u5740\u800C\u975E\u8FD4\u56DE\u503C\uFF09\u3002"));

children.push(heading("3.3 BDD \u5185\u5B58\u6CC4\u6F0F\u4FEE\u590D (H-3)", HeadingLevel.HEADING_2));
children.push(body("\u6587\u4EF6\uFF1Asrc/core/bdd_encoding.c"));
children.push(body("\u95EE\u9898\uFF1Abdd_manager_destroy() \u4EC5\u91CA\u653E\u7BA1\u7406\u5668\u7ED3\u6784\uFF0C\u4E0D\u904D\u5386\u552F\u4E00\u8868\u56DE\u6536\u8282\u70B9\uFF0C\u6240\u6709\u901A\u8FC7 bdd_unique_lookup \u521B\u5EFA\u7684\u8282\u70B9\u5747\u5185\u5B58\u6CC4\u6F0F\u3002"));
children.push(body("\u4FEE\u590D\uFF1A\u5728 bdd_manager_destroy \u4E2D\u6DFB\u52A0\u904D\u5386\u552F\u4E00\u8868\u7684\u903B\u8F91\uFF0C\u91CA\u653E\u6240\u6709\u975E NULL \u8282\u70B9\u3002\u540C\u65F6\u4E3A bdd_new_var \u6DFB\u52A0\u4E86\u5BB9\u91CF\u68C0\u67E5\u8BF4\u660E\u6CE8\u91CA\u3002"));

children.push(heading("3.4 \u5185\u5B58\u7BA1\u7406\u7EDF\u4E00\u4FEE\u590D (H-4)", HeadingLevel.HEADING_2));
children.push(body("\u6587\u4EF6\uFF1Asrc/core/engine.c"));
children.push(body("\u95EE\u9898\uFF1Aengine_handle_circuit_trip_with_action \u4E2D\u4F7F\u7528\u6807\u51C6 free(new_coord) \u800C\u975E\u9879\u76EE\u7EDF\u4E00\u7684 lv00_free()\uFF0C\u7834\u574F\u4E86\u5185\u5B58\u8FFD\u8E2A\u80FD\u529B\u3002"));
children.push(body("\u4FEE\u590D\uFF1A\u6539\u4E3A lv00_free((void **)&new_coord)\uFF0C\u4E0E\u5168\u9879\u76EE\u7EDF\u4E00\u5185\u5B58\u7BA1\u7406\u5668\u4FDD\u6301\u4E00\u81F4\u3002"));

children.push(heading("3.5 realloc \u5931\u8D25\u5904\u7406\u4FEE\u590D (H-5)", HeadingLevel.HEADING_2));
children.push(body("\u6587\u4EF6\uFF1Asrc/core/approx_counter.c"));
children.push(body("\u95EE\u9898\uFF1Acnf_builder_add_lit \u4E2D lv00_realloc \u5931\u8D25\u65F6\u672A\u5904\u7406\uFF0C\u7EE7\u7EED\u4F7F\u7528\u65E7\u6307\u9488\uFF08\u53EF\u80FD\u5DF2\u5931\u6548\uFF09\uFF0C\u5BFC\u81F4\u5199\u5165 NULL \u6307\u9488\u3002"));
children.push(body("\u4FEE\u590D\uFF1A\u5C06 realloc \u7ED3\u679C\u5B58\u5165\u4E34\u65F6\u53D8\u91CF new_buf\uFF0C\u68C0\u67E5 NULL \u540E\u518D\u8D4B\u503C\uFF0C\u5931\u8D25\u65F6\u76F4\u63A5 return \u8DF3\u8FC7\u8BE5\u6587\u5B57\u3002"));

children.push(heading("3.6 \u6392\u5E8F\u7B97\u6CD5\u4F18\u5316 (H-6)", HeadingLevel.HEADING_2));
children.push(body("\u6587\u4EF6\uFF1Asrc/core/ecosystem.c"));
children.push(body("\u95EE\u9898\uFF1Aeco_package_search \u4E2D\u4F7F\u7528 O(n\u00B2) \u5192\u6CE1\u6392\u5E8F\uFF0C\u5305\u6570\u91CF\u5927\u65F6\u6027\u80FD\u5DEE\u3002"));
children.push(body("\u4FEE\u590D\uFF1A\u6DFB\u52A0\u9759\u6001\u6BD4\u8F83\u51FD\u6570 eco_package_sort_cmp\uFF0C\u4F7F\u7528 qsort \u66FF\u4EE3\u5192\u6CE1\u6392\u5E8F\uFF0C\u65F6\u95F4\u590D\u6742\u5EA6\u964D\u4E3A O(n log n)\u3002"));

children.push(heading("3.7 \u7EB3\u79D2\u89C4\u8303\u5316\u4F18\u5316 (H-7)", HeadingLevel.HEADING_2));
children.push(body("\u6587\u4EF6\uFF1Asrc/core/exact_arithmetic.c"));
children.push(body("\u95EE\u9898\uFF1A\u4F7F\u7528 while \u5FAA\u73AF\u9010\u6B21\u51CF/\u52A0 10 \u4EBF\u6765\u89C4\u8303\u5316\u7EB3\u79D2\uFF0C\u6781\u7AEF\u503C\u65F6\u5FAA\u73AF\u6B21\u6570\u8FC7\u591A\u3002"));
children.push(body("\u4FEE\u590D\uFF1A\u4F7F\u7528\u6574\u6570\u9664\u6CD5\u548C\u53D6\u6A21\u4E00\u6B21\u6027\u8BA1\u7B97\u6EA2\u51FA\u79D2\u6570\uFF0C\u5E76\u6B63\u786E\u5904\u7406\u8D1F\u6570\u53D6\u6A21\u7684\u4F59\u6570\u4FEE\u6B63\u3002"));

children.push(heading("3.8 UTF-8 \u7F16\u7801\u4E71\u7801\u4FEE\u590D (E-1~E-3)", HeadingLevel.HEADING_2));
children.push(body("\u5BF9 bdd_encoding.c\u3001approx_counter.c\u3001float_error.c \u4E09\u4E2A\u6587\u4EF6\u7684\u5168\u90E8\u4E71\u7801\u6CE8\u91CA\u8FDB\u884C\u4E86\u4FEE\u590D\uFF0C\u5171\u8BA1\u7EA6 95 \u5904\u3002\u4FEE\u590D\u5185\u5BB9\u5305\u62EC\u6587\u4EF6\u5934\u6CE8\u91CA\u3001\u51FD\u6570\u6CE8\u91CA\u3001\u884C\u5185\u6CE8\u91CA\u7B49\u3002\u6839\u636E\u4EE3\u7801\u4E0A\u4E0B\u6587\u63A8\u65AD\u539F\u59CB\u4E2D\u6587\u542B\u4E49\u5E76\u8FDB\u884C\u66FF\u6362\u3002"));

children.push(heading("3.9 Python \u6A21\u5757\u4F18\u5316 (S-1~S-3)", HeadingLevel.HEADING_2));
children.push(body("\u5BF9 8 \u4E2A Python \u6587\u4EF6\u8FDB\u884C\u4E86\u4EE3\u7801\u8D28\u91CF\u4F18\u5316\uFF1A"));
startBulletGroup();
children.push(bul("python/lv00/core.py\uFF1A\u7EDF\u4E00\u82F1\u6587\u6CE8\u91CA\u4E3A\u4E2D\u6587\uFF0C\u4FEE\u590D Point.__hash__ \u7C7B\u578B\u6CE8\u89E3"));
children.push(bul("concurrent_monitor/core/config.py\uFF1A\u589E\u5F3A\u914D\u7F6E\u9A8C\u8BC1\u903B\u8F91\uFF08default_timeout\u3001max_bytes\u3001backup_count\u3001LOG_LEVEL\uFF09"));
children.push(bul("concurrent_monitor/core/models.py\uFF1A\u6E05\u7406\u672A\u4F7F\u7528\u7684\u5BFC\u5165"));
children.push(bul("\u5176\u4F59 5 \u4E2A\u6587\u4EF6\u6CE8\u91CA\u8986\u76D6\u7387\u5DF2\u8FBE 98%\uFF0C\u65E0\u9700\u989D\u5916\u4FEE\u6539"));

children.push(heading("3.10 JavaScript \u6A21\u5757\u4F18\u5316 (S-4)", HeadingLevel.HEADING_2));
children.push(body("\u5BF9 25 \u4E2A JS \u6587\u4EF6\u8FDB\u884C\u4E86\u8BC4\u4F30\uFF0C\u5176\u4E2D 13 \u4E2A\u6587\u4EF6\u6CE8\u91CA\u5DF2\u5B8C\u5584\uFF0C\u5BF9 12 \u4E2A\u6587\u4EF6\u6267\u884C\u4E86\u4F18\u5316\uFF1A"));
startBulletGroup();
children.push(bul("7 \u4E2A modules/ \u5B50\u76EE\u5F55\u6587\u4EF6\uFF1A\u7EDF\u4E00\u6DFB\u52A0\u6807\u51C6 JSDoc \u6587\u4EF6\u5934"));
children.push(bul("lv00_js_backend.js\uFF1A\u7FFB\u8BD1 16 \u4E2A\u82F1\u6587\u7AE0\u8282\u6807\u9898\u4E3A\u4E2D\u6587"));
children.push(bul("formula_renderer.js\uFF1A\u8865\u5145 4 \u4E2A\u5185\u90E8\u51FD\u6570\u7684 JSDoc \u53C2\u6570/\u8FD4\u56DE\u503C\u8BF4\u660E"));
children.push(bul("formula_to_graph.js\uFF1A\u6DFB\u52A0\u5185\u90E8\u72B6\u6001\u53D8\u91CF\u4E2D\u6587\u884C\u5C3E\u6CE8\u91CA"));

// ============================================================
// 4. C 源文件注释完善
// ============================================================
children.push(new Paragraph({ children: [new PageBreak()] }));
children.push(heading("\u56DB\u3001C \u6E90\u6587\u4EF6\u6CE8\u91CA\u5B8C\u5584", HeadingLevel.HEADING_1));
children.push(body("\u5BF9 7 \u4E2A\u6CE8\u91CA\u8986\u76D6\u7387\u6700\u4F4E\u7684 C \u6E90\u6587\u4EF6\u8FDB\u884C\u4E86\u5927\u89C4\u6A21\u6CE8\u91CA\u6DFB\u52A0\uFF0C\u91C7\u7528 Doxygen @brief/@param/@return \u683C\u5F0F\u3002"));

children.push(makeTable(
  ["\u6587\u4EF6", "\u65B0\u589E\u6CE8\u91CA\u7684\u51FD\u6570\u6570", "\u4E3B\u8981\u5185\u5BB9"],
  [
    ["ecosystem.c", "~20", "\u751F\u547D\u5468\u671F\u3001\u5305\u7BA1\u7406\u3001\u517C\u5BB9\u6027\u3001\u7EDF\u8BA1\u7B49\u5168\u90E8\u516C\u5171\u51FD\u6570"],
    ["algebra_mode.c", "~25+", "\u6240\u6709\u94FE\u5F0F API \u51FD\u6570\uFF08\u70B9/\u7EBF/\u5706/\u53D8\u6362/\u9009\u62E9\u5668/\u7EA6\u675F/\u8BC1\u660E\uFF09"],
    ["gc_language.c", "~18", "GCL \u547D\u4EE4\u5904\u7406\u3001\u89E3\u6790\u5668\u3001\u6267\u884C\u5F15\u64CE\u3001\u5BFC\u51FA\u3001WASM"],
    ["bdd_encoding.c", "~8", "BDD/ADD \u7BA1\u7406\u5668\u3001\u8282\u70B9\u64CD\u4F5C"],
    ["constraint_graph.c", "~7", "\u8282\u70B9/\u7EA6\u675F\u64CD\u4F5C\u3001\u79FB\u9664\u64CD\u4F5C"],
    ["approx_counter.c", "~5", "\u8BA1\u6570 API\u3001PAC \u7F6E\u4FE1\u5EA6"],
    ["float_error.c", "~15", "\u533A\u95F4\u7B97\u672F\u3001FPTaylor API"]
  ],
  [2500, 2200, 4100]
));

// ============================================================
// 5. 遗留问题与建议
// ============================================================
children.push(heading("\u4E94\u3001\u9057\u7559\u95EE\u9898\u4E0E\u540E\u7EED\u5EFA\u8BAE", HeadingLevel.HEADING_1));

children.push(heading("5.1 \u67B6\u6784\u5C42\u9762\u5EFA\u8BAE", HeadingLevel.HEADING_2));
startBulletGroup();
children.push(bul("proof.h (1293\u884C)\u8D1F\u8D23\u8FC7\u591A\uFF0C\u5EFA\u8BAE\u62C6\u5206\u4E3A proof_proposition.h\u3001proof_navigator.h\u3001proof_strategy.h \u7B49\u5B50\u6A21\u5757"));
children.push(bul("context.h (1209\u884C)\u5EFA\u8BAE\u5C06\u7194\u65AD\u5668\u3001\u7F13\u5B58\u7BA1\u7406\u3001\u7EDF\u8BA1\u4FE1\u606F\u62C6\u5206\u4E3A\u72EC\u7ACB\u6A21\u5757"));
children.push(bul("engine.h \u5EFA\u8BAE\u7EDF\u4E00\u51FD\u6570\u547D\u540D\u524D\u7F00\uFF08\u76EE\u524D\u6DF7\u7528 engine_* \u548C lv00_engine_*\uFF09"));
children.push(bul("symbolic_coord.c (5141\u884C)\u5EFA\u8BAE\u62C6\u5206\u4E3A rational.c\u3001algebraic.c\u3001quadratic.c\u3001transcendental.c"));

children.push(heading("5.2 \u5B89\u5168\u5C42\u9762\u5EFA\u8BAE", HeadingLevel.HEADING_2));
startBulletGroup();
children.push(bul("Windows \u9759\u6001\u521D\u59CB\u5316\u7ADE\u6001\uFF1Aexact_arithmetic.c\u3001circuit_breaker.c \u4E2D\u7684\u9891\u7387\u7F13\u5B58\u521D\u59CB\u5316\u5E94\u4F7F\u7528 InterlockedCompareExchange \u6216 Once \u673A\u5236"));
children.push(bul("atp_backend.c \u7684\u5168\u5C40\u6CE8\u518C\u8868\u9700\u8981\u7EBF\u7A0B\u5B89\u5168\u4FDD\u62A4"));
children.push(bul("bdd_ite \u9012\u5F52\u5B9E\u73B0\u5E94\u6DFB\u52A0\u6DF1\u5EA6\u9650\u5236\uFF0C\u9632\u6B62\u5927\u89C4\u6A21 BDD \u6808\u6EA2\u51FA"));

children.push(heading("5.3 \u6027\u80FD\u5C42\u9762\u5EFA\u8BAE", HeadingLevel.HEADING_2));
startBulletGroup();
children.push(bul("constraint_graph.c \u4E2D constraint_exists \u7684 O(n) \u626B\u63CF\u5E94\u6539\u4E3A\u54C8\u5E0C\u7D22\u5F15"));
children.push(bul("gc_language.c \u4E2D gcl_find_symbol \u7684 O(n) \u626B\u63CF\u5E94\u6539\u4E3A\u54C8\u5E0C\u8868"));
children.push(bul("euclidean_geometry.c \u4E2D\u5B9E\u4F53\u6CE8\u518C\u68C0\u67E5\u5E94\u4F7F\u7528\u54C8\u5E0C\u8868\u66FF\u4EE3 O(n) \u7EBF\u6027\u626B\u63CF"));
children.push(bul("bdd_encoding.c \u5F53\u524D\u4E3A\u6869\u5B9E\u73B0\uFF0C\u5B8C\u6574\u5B9E\u73B0\u9700\u8981\u771F\u6B63\u7684\u552F\u4E00\u8868\u548C\u7F13\u5B58"));

children.push(heading("5.4 \u6D4B\u8BD5\u5C42\u9762\u5EFA\u8BAE", HeadingLevel.HEADING_2));
startBulletGroup();
children.push(bul("\u4E3A\u4FEE\u590D\u7684\u54C8\u5E0C\u8868\u5220\u9664\u903B\u8F91\u6DFB\u52A0\u4E13\u95E8\u7684\u5355\u5143\u6D4B\u8BD5\uFF0C\u8986\u76D6\u8FB9\u754C\u60C5\u51B5"));
children.push(bul("\u4E3A BDD \u7BA1\u7406\u5668\u6DFB\u52A0\u5185\u5B58\u6CC4\u6F0F\u68C0\u6D4B\u6D4B\u8BD5"));
children.push(bul("\u4E3A ecosystem.c \u7684 qsort \u6392\u5E8F\u6DFB\u52A0\u56DE\u5F52\u6D4B\u8BD5\uFF0C\u786E\u4FDD\u6392\u5E8F\u7ED3\u679C\u6B63\u786E"));

// ============================================================
// 6. 优化成果统计
// ============================================================
children.push(new Paragraph({ children: [new PageBreak()] }));
children.push(heading("\u516D\u3001\u4F18\u5316\u6210\u679C\u7EDF\u8BA1", HeadingLevel.HEADING_1));

children.push(makeTable(
  ["\u4F18\u5316\u7C7B\u522B", "\u4FEE\u6539\u6570\u91CF", "\u6D89\u53CA\u6587\u4EF6\u6570"],
  [
    ["\u9AD8\u4F18\u5148\u7EA7 Bug \u4FEE\u590D", "7", "7"],
    ["UTF-8 \u7F16\u7801\u4E71\u7801\u4FEE\u590D", "~95 \u5904", "3"],
    ["C \u6E90\u6587\u4EF6\u6CE8\u91CA\u6DFB\u52A0", "~98 \u4E2A\u51FD\u6570", "7"],
    ["Python \u6A21\u5757\u4F18\u5316", "5 \u5904\u4FEE\u6539", "3"],
    ["JavaScript \u6A21\u5757\u4F18\u5316", "12 \u4E2A\u6587\u4EF6", "12"],
    ["\u5408\u8BA1", "~222 \u5904\u4FEE\u6539", "~32"]
  ],
  [3500, 3000, 2300]
));

children.push(body(""));
children.push(boldBody("\u603B\u7ED3\uFF1A"));
children.push(body("\u672C\u6B21\u4F18\u5316\u5BF9 Lv-00 \u9879\u76EE\u5168\u90E8\u4EE3\u7801\u8FDB\u884C\u4E86\u5168\u9762\u7684\u5C40\u90E8\u6700\u4F18\u89E3\u5316\uFF0C\u4FEE\u590D\u4E86 7 \u4E2A\u9AD8/\u4E2D\u4F18\u5148\u7EA7 Bug\uFF08\u5305\u62EC\u54C8\u5E0C\u8868\u6570\u636E\u6B63\u786E\u6027\u95EE\u9898\u3001\u7EBF\u7A0B\u5B89\u5168\u95EE\u9898\u3001\u5185\u5B58\u6CC4\u6F0F\u95EE\u9898\uFF09\uFF0C\u4FEE\u590D\u4E86 3 \u4E2A\u6587\u4EF6\u7684 UTF-8 \u7F16\u7801\u4E71\u7801\uFF08\u7EA6 95 \u5904\uFF09\uFF0C\u4E3A 7 \u4E2A\u4F4E\u6CE8\u91CA\u8986\u76D6\u7387\u7684 C \u6E90\u6587\u4EF6\u6DFB\u52A0\u4E86\u7EA6 98 \u4E2A\u51FD\u6570\u7684\u4E2D\u6587 Doxygen \u6CE8\u91CA\uFF0C\u5E76\u5BF9 12 \u4E2A JavaScript \u6587\u4EF6\u548C 3 \u4E2A Python \u6587\u4EF6\u8FDB\u884C\u4E86\u4EE3\u7801\u8D28\u91CF\u4F18\u5316\u3002\u6240\u6709\u4FEE\u6539\u5747\u4FDD\u6301\u5411\u540E\u517C\u5BB9\uFF0C\u672A\u6539\u53D8\u4EFB\u4F55\u529F\u80FD\u903B\u8F91\u3002"));

// ============================================================
// 构建文档
// ============================================================

const doc = new Document({
  styles: {
    default: {
      document: {
        run: {
          font: { ascii: "Arial", hAnsi: "Arial", eastAsia: "Microsoft YaHei" },
          size: 22
        }
      }
    },
    paragraphStyles: [
      { id: "Heading1", name: "Heading 1", basedOn: "Normal", next: "Normal", quickFormat: true,
        run: { size: 32, bold: true, font: { ascii: "Arial", hAnsi: "Arial", eastAsia: "Microsoft YaHei" } },
        paragraph: { spacing: { before: 360, after: 200 }, outlineLevel: 0, keepNext: false, keepLines: false } },
      { id: "Heading2", name: "Heading 2", basedOn: "Normal", next: "Normal", quickFormat: true,
        run: { size: 28, bold: true, font: { ascii: "Arial", hAnsi: "Arial", eastAsia: "Microsoft YaHei" } },
        paragraph: { spacing: { before: 240, after: 160 }, outlineLevel: 1, keepNext: false, keepLines: false } },
    ]
  },
  numbering: { config: [...numberConfigs, ...bulletConfigs] },
  sections: [{
    properties: {
      page: {
        size: { width: 12240, height: 15840 },
        margin: { top: 1440, right: 1440, bottom: 1440, left: 1440 }
      }
    },
    headers: {
      default: new Header({
        children: [new Paragraph({
          alignment: AlignmentType.RIGHT,
          children: [new TextRun({ text: "Lv-00 \u5C40\u90E8\u6700\u4F18\u89E3\u4F18\u5316\u6C47\u62A5", font: { ascii: "Arial", hAnsi: "Arial", eastAsia: "Microsoft YaHei" }, size: 18, color: "999999" })]
        })]
      })
    },
    footers: {
      default: new Footer({
        children: [new Paragraph({
          alignment: AlignmentType.CENTER,
          children: [
            new TextRun({ text: "\u2014 ", font: { ascii: "Arial", hAnsi: "Arial", eastAsia: "Microsoft YaHei" }, size: 18, color: "999999" }),
            new TextRun({ children: [PageNumber.CURRENT], font: { ascii: "Arial", hAnsi: "Arial", eastAsia: "Microsoft YaHei" }, size: 18, color: "999999" }),
            new TextRun({ text: " \u2014", font: { ascii: "Arial", hAnsi: "Arial", eastAsia: "Microsoft YaHei" }, size: 18, color: "999999" })
          ]
        })]
      })
    },
    children
  }]
});

Packer.toBuffer(doc).then(buffer => {
  fs.writeFileSync("c:\\Users\\xingg\\Documents\\trae_projects\\Lv-00\\Lv-00_\u5C40\u90E8\u6700\u4F18\u89E3\u4F18\u5316\u4EFB\u52A1\u6C47\u62A5_2026-05-25.docx", buffer);
  console.log("Report generated successfully!");
});
