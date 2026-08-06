const fs = require('fs');
const { Document, Packer, Paragraph, TextRun,
        Header, Footer, AlignmentType,
        PageNumber, PageBreak } = require('docx');

// ==================== 配置与共享辅助（来自 docx_helpers.js） ====================
const { FONT_ASCII, FONT_CJK, PAGE_WIDTH, PAGE_HEIGHT, MARGIN, CONTENT_WIDTH,
        heading, para, boldPara, makeTable: makeTableShared,
        makeNumberConfigs, makeBulletConfig, makeNumGroup } = require('./docx_helpers');

// 本生成器 makeTable 表宽为 CONTENT_WIDTH（9360）
function makeTable(headers, rows, colWidths) {
    return makeTableShared(headers, rows, colWidths, CONTENT_WIDTH);
}

// ==================== 文档内容 ====================
const numGroup = makeNumGroup();
function startNumGroup() { numGroup.start(); }
function numItem(text) { return numGroup.item(text); }

// 编号配置
const numberConfigs = makeNumberConfigs();

const children = [];

// ==================== 封面 ====================
children.push(new Paragraph({ spacing: { before: 3000 }, children: [] }));
children.push(new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { after: 400 },
    children: [new TextRun({ text: "Lv-00 \u9879\u76ee\u5168\u57df\u4f18\u5316\u4efb\u52a1\u6c47\u62a5", font: { ascii: FONT_ASCII, hAnsi: FONT_ASCII, eastAsia: FONT_CJK }, size: 52, bold: true })]
}));
children.push(new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { after: 200 },
    children: [new TextRun({ text: "\u529f\u80fd\u8865\u5168 \u00b7 \u4ee3\u7801\u8d28\u91cf \u00b7 \u5b89\u5168\u4fee\u590d \u00b7 \u6a21\u5757\u5316\u6807\u51c6\u5316", font: { ascii: FONT_ASCII, hAnsi: FONT_ASCII, eastAsia: FONT_CJK }, size: 28, color: "666666" })]
}));
children.push(new Paragraph({ spacing: { before: 800 }, children: [] }));
children.push(new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { after: 120 },
    children: [new TextRun({ text: "\u7248\u672c\uff1av3.3.0 \u2192 v3.4.0", font: { ascii: FONT_ASCII, hAnsi: FONT_ASCII, eastAsia: FONT_CJK }, size: 24 })]
}));
children.push(new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { after: 120 },
    children: [new TextRun({ text: "\u65e5\u671f\uff1a2026-05-25", font: { ascii: FONT_ASCII, hAnsi: FONT_ASCII, eastAsia: FONT_CJK }, size: 24 })]
}));
children.push(new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { after: 120 },
    children: [new TextRun({ text: "\u7528\u9014\uff1a\u7406\u8bba\u6570\u5b66\u7814\u7a76", font: { ascii: FONT_ASCII, hAnsi: FONT_ASCII, eastAsia: FONT_CJK }, size: 24 })]
}));
children.push(new Paragraph({ children: [new PageBreak()] }));

// ==================== 一、任务概述 ====================
children.push(heading("\u4efb\u52a1\u6982\u8ff0", 1));
children.push(para("\u672c\u6b21\u4efb\u52a1\u5bf9 Lv-00 \u9879\u76ee\u8fdb\u884c\u4e86\u5168\u9762\u7684\u4ee3\u7801\u8d28\u91cf\u5ba1\u67e5\u4e0e\u4f18\u5316\uff0c\u8986\u76d6 C \u6838\u5fc3\u5f15\u64ce\u3001Web \u524d\u7aef\u3001Python \u5b50\u7cfb\u7edf\u4e09\u5927\u5b50\u7cfb\u7edf\uff0c\u5171\u8ba1\u5ba1\u67e5\u7ea6 600+ \u4e2a\u6587\u4ef6\u3002\u4efb\u52a1\u76ee\u6807\u5305\u62ec\uff1a\u8865\u5168\u529f\u80fd\u7f3a\u5931\u3001\u4fee\u6b63\u4e0d\u4eba\u6027\u5316\u8bbe\u8ba1\u3001\u89c4\u8303\u4ee3\u7801\u98ce\u683c\u3001\u6a21\u5757\u5316\u6807\u51c6\u5316\u3001\u5b8c\u5584\u4e2d\u6587\u6ce8\u91ca\u3001\u6d88\u9664\u5b89\u5168\u98ce\u9669\u3002"));

children.push(heading("\u5ba1\u67e5\u8303\u56f4", 2));
children.push(makeTable(
    ["\u5b50\u7cfb\u7edf", "\u6587\u4ef6\u6570\u91cf", "\u5ba1\u67e5\u91cd\u70b9"],
    [
        ["C \u6838\u5fc3\u5f15\u64ce (src/)", "\u7ea6 120+ .c \u6587\u4ef6", "\u7ebf\u7a0b\u5b89\u5168\u3001\u5185\u5b58\u7ba1\u7406\u3001\u6574\u6570\u6ea2\u51fa\u3001\u6d4b\u8bd5\u65ad\u8a00"],
        ["\u5934\u6587\u4ef6 (include/lv00/)", "\u7ea6 150+ .h \u6587\u4ef6", "\u91cd\u590d\u5b9a\u4e49\u3001\u4f9d\u8d56\u5173\u7cfb\u3001API \u6587\u6863"],
        ["Web \u524d\u7aef (web/)", "\u7ea6 55 \u6587\u4ef6", "\u8fd0\u884c\u65f6\u5d29\u6e83\u3001\u5b89\u5168\u6027\u3001\u54cd\u5e94\u5f0f\u3001\u65e0\u969c\u788d"],
        ["Python \u5b50\u7cfb\u7edf", "\u7ea6 23 .py \u6587\u4ef6", "\u5b89\u5168\u6027\u3001\u5f02\u6b65\u8c03\u7528\u3001\u4f1a\u8bdd\u7ba1\u7406"],
        ["\u9884\u8bbe\u6a21\u5757 (src/preset/)", "\u7ea6 56 .c \u6587\u4ef6", "\u6ce8\u518c\u6a21\u5f0f\u7edf\u4e00\u3001\u5934\u6587\u4ef6\u5305\u542b\u4e00\u81f4\u6027"],
        ["\u6d4b\u8bd5\u6587\u4ef6 (tests/)", "\u7ea6 95 .c \u6587\u4ef6", "\u65ad\u8a00\u7f3a\u5931\u3001\u8fb9\u754c\u6d4b\u8bd5\u8986\u76d6"]
    ],
    [2000, 2000, 5360]
));

// ==================== 二、发现的问题总览 ====================
children.push(new Paragraph({ children: [new PageBreak()] }));
children.push(heading("\u53d1\u73b0\u7684\u95ee\u9898\u603b\u89c8", 1));

children.push(heading("\u95ee\u9898\u5206\u5e03\u7edf\u8ba1", 2));
children.push(makeTable(
    ["\u4e25\u91cd\u7a0b\u5ea6", "\u6570\u91cf", "\u5178\u578b\u95ee\u9898"],
    [
        ["P0 - \u963b\u65ad\u6027", "3", "constants.js \u672a\u52a0\u8f7d\u3001logger \u5b9a\u4e49\u987a\u5e8f\u9519\u8bef\u3001\u4e92\u65a5\u9501\u7ade\u6001"],
        ["P1 - \u9ad8\u4f18\u5148\u7ea7", "8", "\u7ebf\u7a0b\u5b89\u5168\u3001\u5b89\u5168\u6f0f\u6d1e\u3001\u6574\u6570\u6ea2\u51fa\u3001\u5934\u6587\u4ef6\u51b2\u7a81"],
        ["P2 - \u4e2d\u7b49\u4f18\u5148\u7ea7", "7", "\u54cd\u5e94\u5f0f\u8bbe\u8ba1\u3001\u65e0\u969c\u788d\u8bbf\u95ee\u3001\u6d4b\u8bd5\u65ad\u8a00\u3001\u4ee3\u7801\u98ce\u683c"],
        ["P3 - \u4f4e\u4f18\u5148\u7ea7", "5", "\u5e38\u91cf\u7edf\u4e00\u3001\u6ce8\u91ca\u5b8c\u5584\u3001\u547d\u540d\u89c4\u8303"]
    ],
    [2000, 1500, 5860]
));

// ==================== 三、已完成的修复 ====================
children.push(new Paragraph({ children: [new PageBreak()] }));
children.push(heading("\u5df2\u5b8c\u6210\u7684\u4fee\u590d", 1));

// --- P0 ---
children.push(heading("P0 \u963b\u65ad\u6027\u95ee\u9898\u4fee\u590d", 2));

children.push(boldPara("1. C \u6838\u5fc3\u4ee3\u7801\u5b89\u5168 Bug"));
startNumGroup();
children.push(numItem("func_block_preset.c\uff1a\u4fee\u590d Windows \u4e92\u65a5\u9501\u521d\u59cb\u5316\u7ade\u6001\u6761\u4ef6\uff0c\u5c06 bool \u6807\u5fd7\u6539\u4e3a LONG + InterlockedCompareExchange \u539f\u5b50\u64cd\u4f5c"));
children.push(numItem("func_block_compose.c\uff1a\u4fee\u590d bool \u51fd\u6570\u8fd4\u56de NULL \u7684\u672a\u5b9a\u4e49\u884c\u4e3a\uff0c\u6539\u4e3a return false"));
children.push(numItem("solver_core.c\uff1a\u4e3a\u53d8\u91cf\u6269\u5bb9\u548c\u7ea6\u675f\u5931\u8d25\u6807\u8bb0\u6570\u7ec4\u6269\u5bb9\u6dfb\u52a0\u6574\u6570\u6ea2\u51fa\u68c0\u67e5"));

children.push(boldPara("2. Web \u524d\u7aef\u963b\u65ad\u6027\u95ee\u9898"));
startNumGroup();
children.push(numItem("index.html\uff1a\u6dfb\u52a0 constants.js \u7684 <script> \u6807\u7b7e\uff0c\u4fee\u590d Lv00Const \u547d\u540d\u7a7a\u95f4\u672a\u5b9a\u4e49\u5bfc\u81f4\u7684\u8fd0\u884c\u65f6\u5d29\u6e83"));
children.push(numItem("index.html\uff1a\u4e3a\u52a0\u8f7d\u906e\u7f69\u6dfb\u52a0\u56db\u6b65\u52a0\u8f7d\u8fdb\u5ea6\u63d0\u793a\uff0c\u63d0\u5347\u7528\u6237\u4f53\u9a8c"));
children.push(numItem("app.js\uff1a\u4e3a\u6784\u9020\u51fd\u6570\u4e2d\u7684 DOM \u64cd\u4f5c\u6dfb\u52a0 null \u68c0\u67e5\u548c\u5bb9\u9519\u5904\u7406"));

children.push(boldPara("3. Python \u5b50\u7cfb\u7edf\u963b\u65ad\u6027\u95ee\u9898"));
startNumGroup();
children.push(numItem("api_server.py\uff1a\u5c06 logger \u5b9a\u4e49\u548c logging.basicConfig \u4ece\u7b2c 148 \u884c\u79fb\u5230\u7b2c 90 \u884c\uff0c\u4fee\u590d\u5b9a\u4e49\u524d\u4f7f\u7528\u7684 NameError"));

// --- P1 ---
children.push(heading("P1 \u9ad8\u4f18\u5148\u7ea7\u95ee\u9898\u4fee\u590d", 2));

children.push(boldPara("1. \u5934\u6587\u4ef6\u91cd\u590d\u5b9a\u4e49"));
startNumGroup();
children.push(numItem("status_codes.h\uff1a\u4e3a LV00_OK \u5b8f\u6dfb\u52a0 #ifndef \u4fdd\u62a4\uff0c\u907f\u514d\u4e0e error_codes.h \u4e2d\u7684\u679a\u4e3e\u503c\u51b2\u7a81"));

children.push(boldPara("2. \u7ebf\u7a0b\u5b89\u5168\u4fee\u590d"));
startNumGroup();
children.push(numItem("debug.c\uff1a\u6dfb\u52a0\u8be6\u7ec6\u7684\u7ebf\u7a0b\u5b89\u5168\u7b56\u7565\u6ce8\u91ca\uff0c\u4fee\u590d debug_log_init() \u4e2d\u7684\u7ade\u6001\u6761\u4ef6\uff0c\u5c06 g_initialized \u6539\u4e3a volatile bool"));
children.push(numItem("debug.c\uff1a\u4fee\u590d debug_log_shutdown() \u4e2d\u7684 TOCTOU \u7ade\u6001\uff0c\u5c06\u68c0\u67e5\u79fb\u5165\u9501\u5185"));
children.push(numItem("formula_renderer.c\uff1a\u4e3a\u7f13\u51b2\u533a\u6c60\u6dfb\u52a0\u4e92\u65a5\u9501\u4fdd\u62a4\uff08Windows CRITICAL_SECTION + InterlockedCompareExchange / POSIX pthread_mutex\uff09"));

children.push(boldPara("3. Web \u524d\u7aef\u5b89\u5168\u6027\u4e0e\u9519\u8bef\u5904\u7406"));
startNumGroup();
children.push(numItem("formula_renderer.js\uff1a\u4fee\u590d _ensureElement \u4e2d el \u53d8\u91cf\u88ab\u8986\u76d6\u5bfc\u81f4\u9519\u8bef\u6d88\u606f\u663e\u793a null \u7684\u95ee\u9898"));
children.push(numItem("magic.js\uff1a\u4e3a fetch \u6dfb\u52a0 .catch() \u9519\u8bef\u5904\u7406\uff0c\u5c06\u8b66\u544a\u5199\u5165\u7528\u6237\u754c\u9762\u65e5\u5fd7"));
children.push(numItem("index.html\uff1a\u4e3a Canvas \u5143\u7d20\u6dfb\u52a0 aria-label \u548c role=\"img\""));

children.push(boldPara("4. Python \u5b50\u7cfb\u7edf\u5b89\u5168\u6027"));
startNumGroup();
children.push(numItem("api_server.py\uff1a\u4e3a\u6ce8\u518c\u7aef\u70b9\u6dfb\u52a0\u57fa\u4e8e\u5185\u5b58\u7684 IP \u901f\u7387\u9650\u5236\uff08\u6bcf\u5206\u949f\u6700\u591a 5 \u6b21\uff09"));
children.push(numItem("api_server.py\uff1a\u6539\u5584 JWT_SECRET_KEY \u672a\u8bbe\u7f6e\u65f6\u7684\u9519\u8bef\u63d0\u793a\uff0c\u63d0\u4f9b\u591a\u5e73\u53f0\u8bbe\u7f6e\u6307\u5f15"));
children.push(numItem("ai_engine.py\uff1a\u4e3a OpenAI \u6d41\u5f0f\u8c03\u7528\u6dfb\u52a0 asyncio.to_thread \u5305\u88c5\uff0c\u907f\u514d\u963b\u585e\u4e8b\u4ef6\u5faa\u73af"));
children.push(numItem("ai_engine.py\uff1a\u5b9e\u73b0\u4f1a\u8bdd\u5386\u53f2 LRU \u6e05\u7406\u903b\u8f91\uff0c\u6bcf 10 \u5206\u949f\u6e05\u7406\u8d85\u8fc7 30 \u5206\u949f\u672a\u8bbf\u95ee\u7684\u4f1a\u8bdd"));
children.push(numItem("dashboard.py\uff1a\u5c06 print() \u8bed\u53e5\u66ff\u6362\u4e3a logger.info()"));

// --- P2 ---
children.push(heading("P2 \u4e2d\u7b49\u4f18\u5148\u7ea7\u95ee\u9898\u4fee\u590d", 2));

children.push(boldPara("1. C \u6d4b\u8bd5\u65ad\u8a00\u7f3a\u5931"));
startNumGroup();
children.push(numItem("test_solver.c\uff1a\u4e3a test_degrees_of_freedom \u6dfb\u52a0 assert(dof2 == 3) \u65ad\u8a00"));
children.push(numItem("test_solver.c\uff1a\u4e3a test_conflict_detection \u6dfb\u52a0 assert(has_conflict == false) \u65ad\u8a00"));
children.push(numItem("test_proof.c\uff1a\u6269\u5c55\u547d\u9898\u751f\u547d\u5468\u671f\u6d4b\u8bd5\uff0c\u6dfb\u52a0\u521d\u59cb\u72b6\u6001\u3001\u72b6\u6001\u53d8\u5316\u3001\u8d44\u6e90\u91ca\u653e\u9a8c\u8bc1"));
children.push(numItem("test_basic.c\uff1a\u65b0\u589e test_rational_boundary \u51fd\u6570\uff0c\u8986\u76d6\u96f6\u503c\u8fd0\u7b97\u3001\u8d1f\u6570\u8fd0\u7b97\u3001\u5206\u6bcd\u4e3a\u96f6\u9519\u8bef\u5904\u7406"));

children.push(boldPara("2. \u4ee3\u7801\u98ce\u683c\u7edf\u4e00"));
startNumGroup();
children.push(numItem("constraint_graph.c\uff1a\u6dfb\u52a0\u9519\u8bef\u7cfb\u7edf\u8fc1\u79fb\u8bf4\u660e\u6ce8\u91ca\uff08\u5df2\u5b8c\u6210\u4ece\u53cc\u8f68\u9519\u8bef\u7cfb\u7edf\u5230\u7edf\u4e00\u9519\u8bef\u7cfb\u7edf\u7684\u8fc1\u79fb\uff09"));
children.push(numItem("func_block.c\uff1a\u4fdd\u7559\u624b\u52a8\u6d41\u5f0f\u4e0a\u4e0b\u6587\u58f0\u660e\u5e76\u6dfb\u52a0\u8be6\u7ec6\u6ce8\u91ca\u8bf4\u660e\u539f\u56e0"));
children.push(numItem("axiom_pkg.c\uff1a\u5c06\u624b\u52a8\u58f0\u660e\u6539\u4e3a\u4f7f\u7528 LV00_DECLARE_STREAM_CTX \u5b8f"));

children.push(boldPara("3. \u5185\u5b58\u7ba1\u7406\u6ce8\u91ca\u8865\u5145"));
startNumGroup();
children.push(numItem("memory_pool.c\uff1a\u6dfb\u52a0\u5faa\u73af\u4f9d\u8d56\u8bf4\u660e\u6ce8\u91ca\uff0c\u89e3\u91ca\u4e3a\u4f55\u4f7f\u7528\u6807\u51c6 malloc/free"));
children.push(numItem("geometry_transform.c\uff1a\u6dfb\u52a0\u5185\u5b58\u7ba1\u7406\u7b56\u7565\u8bf4\u660e\uff08\u907f\u514d\u5185\u5b58\u6c60\u788e\u7247\u5316\uff09"));
children.push(numItem("expr_canonical.c\uff1a\u6dfb\u52a0\u5185\u5b58\u7ba1\u7406\u7b56\u7565\u8bf4\u660e\uff08\u907f\u514d\u5185\u5b58\u6c60\u538b\u529b\uff09"));

children.push(boldPara("4. Web \u524d\u7aef UI \u4eba\u6027\u5316"));
startNumGroup();
children.push(numItem("main.css\uff1a\u4fee\u590d\u4e3b\u5e03\u5c40\u9ad8\u5ea6\u786c\u7f16\u7801\uff0c\u6539\u7528 flexbox \u81ea\u52a8\u586b\u5145\u5269\u4f59\u7a7a\u95f4"));
children.push(numItem("main.css\uff1a\u4e3a\u53f3\u4fa7\u8fb9\u680f\u6dfb\u52a0 768px/480px \u65ad\u70b9\u9002\u914d\uff0c\u5c0f\u5c4f\u6298\u53e0\u4e3a\u56fa\u5b9a\u5b9a\u4f4d\u9762\u677f"));
children.push(numItem("main.css\uff1a\u65b0\u589e\u6807\u7b7e\u9875\u4e0b\u62c9\u83dc\u5355\u6837\u5f0f\uff0c\u7a84\u5c4f\u66ff\u4ee3\u6a2a\u5411\u6eda\u52a8"));
children.push(numItem("index.html\uff1a\u4e3a\u5de5\u5177\u680f\u6309\u94ae\u6dfb\u52a0 aria-label\uff0c\u4e3a\u6807\u7b7e\u9875\u6dfb\u52a0 aria-controls \u548c\u952e\u76d8\u5bfc\u822a"));
children.push(numItem("index.html\uff1a\u65b0\u589e\u4fa7\u8fb9\u680f\u5207\u6362\u6309\u94ae\u548c\u906e\u7f69\u5c42\uff0c\u652f\u6301 ESC \u5173\u95ed"));

// --- P3 ---
children.push(heading("P3 \u4f4e\u4f18\u5148\u7ea7\u6539\u8fdb", 2));

children.push(boldPara("1. \u9884\u8bbe\u6a21\u5757\u6807\u51c6\u5316"));
startNumGroup();
children.push(numItem("6 \u4e2a\u9884\u8bbe\u6587\u4ef6\u6dfb\u52a0 #include \"preset_common.h\"\uff0c\u7edf\u4e00\u5b89\u5168\u5b8f\u4fdd\u62a4"));
children.push(numItem("preset_set_theory.c\uff1a\u5c06 REGISTER_SET \u5b8f\u6539\u4e3a\u59d4\u6258\u7ed9 PRESET_REGISTER_EX\uff0c\u7edf\u4e00\u6ce8\u518c\u6a21\u5f0f"));

children.push(boldPara("2. \u5e38\u91cf\u7edf\u4e00"));
startNumGroup();
children.push(numItem("func_block_serialize.c\uff1a\u79fb\u9664\u672c\u5730 SERIALIZE_BUFFER_INITIAL_SIZE \u5b9a\u4e49\uff0c\u7edf\u4e00\u4f7f\u7528 LV00_SERIALIZE_BUFFER_INITIAL_SIZE"));
children.push(numItem("func_block_preset.c\uff1a\u5c06\u901a\u7528 BUFFER_SIZE \u6539\u540d\u4e3a PRESET_SERIALIZE_BUFFER_SIZE\uff0c\u503c\u7edf\u4e00\u4e3a 1024"));

children.push(boldPara("3. \u7f16\u8bd1\u65f6\u68c0\u67e5\u589e\u5f3a"));
startNumGroup();
children.push(numItem("stream.h\uff1a\u5c06 STREAM_EVENT_TYPE_COUNT \u7684 _Static_assert \u4e2d\u786c\u7f16\u7801\u6570\u5b57\u66ff\u6362\u4e3a\u679a\u4e3e\u672b\u5c3e\u503c + 1\uff0c\u786e\u4fdd\u65b0\u589e\u4e8b\u4ef6\u7c7b\u578b\u65f6\u81ea\u52a8\u68c0\u6d4b"));

// ==================== 四、修改文件清单 ====================
children.push(new Paragraph({ children: [new PageBreak()] }));
children.push(heading("\u4fee\u6539\u6587\u4ef6\u6e05\u5355", 1));

children.push(makeTable(
    ["\u6587\u4ef6\u8def\u5f84", "\u4fee\u6539\u7c7b\u578b", "\u4f18\u5148\u7ea7"],
    [
        ["src/func_block/func_block_preset.c", "\u4e92\u65a5\u9501\u7ade\u6001\u4fee\u590d", "P0"],
        ["src/func_block/func_block_compose.c", "bool \u8fd4\u56de\u503c\u4fee\u590d", "P0"],
        ["src/core/solver_core.c", "\u6574\u6570\u6ea2\u51fa\u68c0\u67e5", "P0"],
        ["web/index.html", "constants.js \u52a0\u8f7d + \u65e0\u969c\u788d + \u54cd\u5e94\u5f0f", "P0/P2"],
        ["web/js/app.js", "DOM \u5bb9\u9519 + \u52a0\u8f7d\u6b65\u9aa4", "P0"],
        ["web/js/formula_renderer.js", "\u9519\u8bef\u6d88\u606f\u4fee\u590d", "P1"],
        ["web/js/modules/magic.js", "fetch \u9519\u8bef\u5904\u7406", "P1"],
        ["web/css/main.css", "\u54cd\u5e94\u5f0f + flexbox \u5e03\u5c40", "P2"],
        ["include/lv00/status_codes.h", "LV00_OK \u91cd\u590d\u5b9a\u4e49\u4fee\u590d", "P1"],
        ["src/core/debug.c", "\u7ebf\u7a0b\u5b89\u5168\u4fee\u590d", "P1"],
        ["src/parser/formula_renderer.c", "\u7f13\u51b2\u533a\u6c60\u7ebf\u7a0b\u5b89\u5168", "P1"],
        ["src/core/memory_pool.c", "\u5faa\u73af\u4f9d\u8d56\u6ce8\u91ca", "P2"],
        ["src/core/geometry_transform.c", "\u5185\u5b58\u7ba1\u7406\u6ce8\u91ca", "P2"],
        ["src/core/expr_canonical.c", "\u5185\u5b58\u7ba1\u7406\u6ce8\u91ca", "P2"],
        ["src/core/constraint_graph.c", "\u9519\u8bef\u7cfb\u7edf\u8fc1\u79fb\u8bf4\u660e", "P2"],
        ["src/func_block/func_block.c", "\u6d41\u5f0f\u4e0a\u4e0b\u6587\u6ce8\u91ca", "P2"],
        ["src/axiom/axiom_pkg.c", "\u6d41\u5f0f\u4e0a\u4e0b\u6587\u7edf\u4e00", "P2"],
        ["include/lv00/stream.h", "\u7f16\u8bd1\u65f6\u68c0\u67e5\u589e\u5f3a", "P3"],
        ["src/func_block/func_block_serialize.c", "\u5e38\u91cf\u7edf\u4e00", "P3"],
        ["src/func_block/func_block_preset.c", "\u5e38\u91cf\u91cd\u547d\u540d", "P3"],
        ["src/preset/preset_set_theory.c", "\u6ce8\u518c\u6a21\u5f0f\u7edf\u4e00", "P3"],
        ["src/preset/ \u4e0b 6 \u4e2a\u6587\u4ef6", "\u6dfb\u52a0 preset_common.h", "P3"],
        ["tests/test_solver.c", "\u65ad\u8a00\u8865\u5145", "P2"],
        ["tests/test_proof.c", "\u65ad\u8a00\u8865\u5145", "P2"],
        ["tests/test_basic.c", "\u8fb9\u754c\u6d4b\u8bd5\u65b0\u589e", "P2"],
        ["llm_coding_assistant/api_server.py", "logger + \u901f\u7387\u9650\u5236", "P0/P1"],
        ["llm_coding_assistant/core/ai_engine.py", "\u5f02\u6b65 + \u4f1a\u8bdd\u6e05\u7406", "P1"],
        ["concurrent_monitor/web/dashboard.py", "print \u66ff\u6362\u4e3a logger", "P1"],
        ["include/lv00/axiom_pkg.h", "\u51fd\u6570\u58f0\u660e\u66f4\u65b0", "P2"]
    ],
    [4000, 3360, 2000]
));

// ==================== 五、遗留问题与建议 ====================
children.push(new Paragraph({ children: [new PageBreak()] }));
children.push(heading("\u9057\u7559\u95ee\u9898\u4e0e\u540e\u7eed\u5efa\u8bae", 1));

children.push(heading("\u67b6\u6784\u7ea7\u95ee\u9898\uff08\u5efa\u8bae\u4e0b\u4e00\u5927\u7248\u672c\u5904\u7406\uff09", 2));

children.push(boldPara("1. \u56db\u5957\u72ec\u7acb\u6ce8\u518c\u8868\u5e76\u5b58"));
children.push(para("\u9879\u76ee\u4e2d\u5b58\u5728\u56db\u4e2a\u72ec\u7acb\u7684\u9884\u8bbe\u51fd\u6570\u5757\u6ce8\u518c\u8868\uff08func_block_registry.c\u3001func_block_preset.c\u3001preset_blocks.c\u3001preset_manager.c\uff09\uff0c\u5404\u81ea\u7ef4\u62a4\u72ec\u7acb\u7684 InternalPresetEntry \u5b9a\u4e49\u3001\u521d\u59cb\u5316\u903b\u8f91\u548c\u67e5\u627e\u51fd\u6570\u3002\u5efa\u8bae\u5408\u5e76\u4e3a\u5355\u4e00\u6ce8\u518c\u8868\u5b9e\u73b0\uff0c\u4fdd\u7559\u529f\u80fd\u6700\u5b8c\u6574\u7684 preset_blocks.c \u7248\u672c\u3002"));

children.push(boldPara("2. \u8d85\u5927\u6587\u4ef6\u62c6\u5206"));
children.push(para("\u591a\u4e2a\u6838\u5fc3\u6587\u4ef6\u8d85\u8fc7 4000 \u884c\uff1asolver.c (7415\u884c)\u3001interop.c (6710\u884c)\u3001symbolic_coord.c (5141\u884c)\u3001proof.c (5012\u884c)\u3001constraint_graph.c (4118\u884c)\u3001rewrite.c (4082\u884c)\u3001module.c (4003\u884c)\u3002\u5efa\u8bae\u6309\u529f\u80fd\u57df\u62c6\u5206\u4e3a\u591a\u4e2a\u5b50\u6a21\u5757\u3002"));

children.push(boldPara("3. \u4e24\u5957\u7c7b\u578b/\u7c7b\u522b\u7cfb\u7edf\u5e76\u5b58"));
children.push(para("PresetParamType vs PresetType\u3001PresetCategory vs PresetExtendedCategory \u4e24\u5957\u7c7b\u578b\u7cfb\u7edf\u5e76\u5b58\uff0c\u5bb9\u6613\u9020\u6210 API \u6df7\u6dc6\u3002\u5efa\u8bae\u7edf\u4e00\u4e3a\u5355\u4e00\u7c7b\u578b\u7cfb\u7edf\u3002"));

children.push(boldPara("4. preset_common.h \u4f9d\u8d56\u5185\u90e8\u5934\u6587\u4ef6"));
children.push(para("preset_common.h \u4f9d\u8d56 lv00_internal.h \u7834\u574f\u4e86\u5c01\u88c5\u6027\u3002\u5efa\u8bae\u5c06\u65e5\u5fd7\u51fd\u6570\u58f0\u660e\u63d0\u53d6\u5230\u72ec\u7acb\u7684\u516c\u5171\u5934\u6587\u4ef6\u4e2d\u3002"));

children.push(heading("\u529f\u80fd\u589e\u5f3a\u5efa\u8bae", 2));
startNumGroup();
children.push(numItem("\u4e3a Web \u524d\u7aef\u6dfb\u52a0\u5355\u5143\u6d4b\u8bd5\u6846\u67b6\uff08\u5f53\u524d package.json \u4e2d test \u811a\u672c\u4e3a\u7a7a\u64cd\u4f5c\uff09"));
children.push(numItem("\u4e3a llm_coding_assistant \u6dfb\u52a0\u96c6\u6210\u6d4b\u8bd5\uff08\u8ba4\u8bc1\u6d41\u7a0b\u3001WebSocket \u8fde\u63a5\u3001\u901f\u7387\u9650\u5236\uff09"));
children.push(numItem("\u6dfb\u52a0 TikZ \u5bfc\u51fa\u3001Groebner \u57fa\u5f15\u64ce\u3001BDD \u7f16\u7801\u7b49\u9ad8\u7ea7\u529f\u80fd\u7684\u4e13\u9879\u6d4b\u8bd5"));
children.push(numItem("\u8003\u8651\u4e3a api_server.py \u6dfb\u52a0\u6301\u4e45\u5316\u7528\u6237\u5b58\u50a8\uff08\u5f53\u524d\u4e3a\u5185\u5b58\u5b57\u5178\uff09"));

// ==================== 六、总结 ====================
children.push(heading("\u603b\u7ed3", 1));
children.push(para("\u672c\u6b21\u4f18\u5316\u5171\u4fee\u6539 29 \u4e2a\u6587\u4ef6\uff0c\u89e3\u51b3\u4e86 3 \u4e2a\u963b\u65ad\u6027\u95ee\u9898\u30018 \u4e2a\u9ad8\u4f18\u5148\u7ea7\u95ee\u9898\u30017 \u4e2a\u4e2d\u7b49\u4f18\u5148\u7ea7\u95ee\u9898\u548c 5 \u4e2a\u4f4e\u4f18\u5148\u7ea7\u95ee\u9898\u3002\u4fee\u590d\u8303\u56f4\u8986\u76d6 C \u6838\u5fc3\u5f15\u64ce\u3001Web \u524d\u7aef\u3001Python \u5b50\u7cfb\u7edf\u3001\u9884\u8bbe\u6a21\u5757\u548c\u6d4b\u8bd5\u6587\u4ef6\u4e94\u5927\u5b50\u7cfb\u7edf\u3002"));
children.push(para("\u9879\u76ee\u6574\u4f53\u4ee3\u7801\u8d28\u91cf\u8f83\u9ad8\uff0c\u5177\u5907\u826f\u597d\u7684\u5206\u5c42\u67b6\u6784\u548c\u7ebf\u7a0b\u5b89\u5168\u610f\u8bc6\u3002\u672c\u6b21\u4fee\u590d\u4e3b\u8981\u96c6\u4e2d\u5728\u5b89\u5168\u98ce\u9669\u6d88\u9664\u3001\u7ebf\u7a0b\u5b89\u5168\u52a0\u5f3a\u3001UI \u4eba\u6027\u5316\u6539\u8fdb\u548c\u4ee3\u7801\u89c4\u8303\u5316\u56db\u4e2a\u65b9\u9762\u3002\u67b6\u6784\u7ea7\u95ee\u9898\uff08\u5982\u56db\u5957\u6ce8\u518c\u8868\u5e76\u5b58\u3001\u8d85\u5927\u6587\u4ef6\u62c6\u5206\uff09\u5efa\u8bae\u5728\u4e0b\u4e00\u5927\u7248\u672c\u4e2d\u7edf\u4e00\u5904\u7406\u3002"));

// ==================== 构建文档 ====================
const doc = new Document({
    styles: {
        default: {
            document: {
                run: {
                    font: { ascii: FONT_ASCII, hAnsi: FONT_ASCII, eastAsia: FONT_CJK },
                    size: 22
                }
            }
        },
        paragraphStyles: [
            { id: "Heading1", name: "Heading 1", basedOn: "Normal", next: "Normal", quickFormat: true,
              run: { size: 36, bold: true, font: { ascii: FONT_ASCII, hAnsi: FONT_ASCII, eastAsia: FONT_CJK } },
              paragraph: { spacing: { before: 360, after: 200 }, outlineLevel: 0, keepNext: false, keepLines: false } },
            { id: "Heading2", name: "Heading 2", basedOn: "Normal", next: "Normal", quickFormat: true,
              run: { size: 28, bold: true, font: { ascii: FONT_ASCII, hAnsi: FONT_ASCII, eastAsia: FONT_CJK } },
              paragraph: { spacing: { before: 240, after: 160 }, outlineLevel: 1, keepNext: false, keepLines: false } },
        ]
    },
    numbering: {
        config: [
            makeBulletConfig(),
            ...numberConfigs
        ]
    },
    sections: [{
        properties: {
            page: {
                size: { width: PAGE_WIDTH, height: PAGE_HEIGHT },
                margin: { top: MARGIN, right: MARGIN, bottom: MARGIN, left: MARGIN }
            }
        },
        headers: {
            default: new Header({
                children: [new Paragraph({
                    alignment: AlignmentType.RIGHT,
                    children: [new TextRun({ text: "Lv-00 \u5168\u57df\u4f18\u5316\u4efb\u52a1\u6c47\u62a5", font: { ascii: FONT_ASCII, hAnsi: FONT_ASCII, eastAsia: FONT_CJK }, size: 18, color: "999999" })]
                })]
            })
        },
        footers: {
            default: new Footer({
                children: [new Paragraph({
                    alignment: AlignmentType.CENTER,
                    children: [
                        new TextRun({ text: "\u2014 ", font: { ascii: FONT_ASCII, hAnsi: FONT_ASCII, eastAsia: FONT_CJK }, size: 18, color: "999999" }),
                        new TextRun({ children: [PageNumber.CURRENT], font: { ascii: FONT_ASCII, hAnsi: FONT_ASCII, eastAsia: FONT_CJK }, size: 18, color: "999999" }),
                        new TextRun({ text: " \u2014", font: { ascii: FONT_ASCII, hAnsi: FONT_ASCII, eastAsia: FONT_CJK }, size: 18, color: "999999" })
                    ]
                })]
            })
        },
        children
    }]
});

const OUTPUT_PATH = "c:\\Users\\xingg\\Documents\\trae_projects\\Lv-00\\Lv-00_\u5168\u57df\u4f18\u5316\u4efb\u52a1\u6c47\u62a5_v3.4.0.docx";

Packer.toBuffer(doc).then(buffer => {
    fs.writeFileSync(OUTPUT_PATH, buffer);
    console.log("Report generated: " + OUTPUT_PATH);
}).catch(err => {
    console.error("Error:", err);
    process.exit(1);
});
