const { Document, Packer, Paragraph, TextRun, Table, TableRow, HeadingLevel,
        AlignmentType, WidthType } = require('docx');
const fs = require('fs');
const path = require('path');

// 共享辅助（来自 tool/report_generators/docx_helpers.js）
const { font, borders, PAGE_WIDTH, PAGE_HEIGHT, MARGIN, makeCell, makeBulletConfig } =
    require('../tool/report_generators/docx_helpers');

function cell(text, opts = {}) {
    return makeCell(text, {
        width: opts.width || 2340,
        shading: opts.shade,
        margins: { top: 60, bottom: 60, left: 80, right: 80 }
    });
}

function h1(text) {
    return new Paragraph({ heading: HeadingLevel.HEADING_1, children: [new TextRun({ text, font: font(), size: 32, bold: true })] });
}
function h2(text) {
    return new Paragraph({ heading: HeadingLevel.HEADING_2, children: [new TextRun({ text, font: font(), size: 28, bold: true })] });
}
function h3(text) {
    return new Paragraph({ heading: HeadingLevel.HEADING_3, children: [new TextRun({ text, font: font(), size: 24, bold: true })] });
}
function p(text, opts = {}) {
    return new Paragraph({ children: [new TextRun({ text, font: font(), size: 21, ...opts })] });
}
function bullet(text) {
    return new Paragraph({ numbering: { reference: "bullets", level: 0 }, children: [new TextRun({ text, font: font(), size: 21 })] });
}

const doc = new Document({
    styles: {
        default: {
            document: {
                run: { font: { ascii: "Arial", hAnsi: "Arial", eastAsia: "Microsoft YaHei" }, size: 21 }
            }
        },
        paragraphStyles: [
            { id: "Heading1", name: "Heading 1", basedOn: "Normal", next: "Normal", quickFormat: true,
              run: { size: 32, bold: true, font: { ascii: "Arial", hAnsi: "Arial", eastAsia: "Microsoft YaHei" } },
              paragraph: { spacing: { before: 240, after: 120 }, outlineLevel: 0, keepNext: false, keepLines: false } },
            { id: "Heading2", name: "Heading 2", basedOn: "Normal", next: "Normal", quickFormat: true,
              run: { size: 28, bold: true, font: { ascii: "Arial", hAnsi: "Arial", eastAsia: "Microsoft YaHei" } },
              paragraph: { spacing: { before: 180, after: 90 }, outlineLevel: 1, keepNext: false, keepLines: false } },
            { id: "Heading3", name: "Heading 3", basedOn: "Normal", next: "Normal", quickFormat: true,
              run: { size: 24, bold: true, font: { ascii: "Arial", hAnsi: "Arial", eastAsia: "Microsoft YaHei" } },
              paragraph: { spacing: { before: 120, after: 60 }, outlineLevel: 2, keepNext: false, keepLines: false } },
        ]
    },
    numbering: {
        config: [makeBulletConfig()]
    },
    sections: [{
        properties: {
            page: {
                size: { width: PAGE_WIDTH, height: PAGE_HEIGHT },
                margin: { top: MARGIN, right: MARGIN, bottom: MARGIN, left: MARGIN }
            }
        },
        children: [
            new Paragraph({
                alignment: AlignmentType.CENTER,
                children: [new TextRun({ text: "Lv-00 v5.0.0 \u7248\u672c\u6587\u6863", font: { ascii: "Arial", hAnsi: "Arial", eastAsia: "Microsoft YaHei" }, size: 44, bold: true })]
            }),
            new Paragraph({
                alignment: AlignmentType.CENTER,
                spacing: { after: 240 },
                children: [new TextRun({ text: "\u53d1\u5e03\u65e5\u671f: 2026-06-04    \u7248\u672c\u53f7: 5.0.0    \u67b6\u6784\u7248\u672c: \u5341\u5c42\u5355\u5411\u4f9d\u8d56\u67b6\u6784 v5.0    \u8bb8\u53ef\u8bc1: MIT",
                    font: { ascii: "Arial", hAnsi: "Arial", eastAsia: "Microsoft YaHei" }, size: 20, color: "666666" })]
            }),

            h1("1. \u9879\u76ee\u6982\u8ff0"),
            p("Lv-00 \u662f\u4e00\u95e8\u4ee5\u51e0\u4f55\u4e3a\u552f\u4e00\u8f7d\u4f53\u7684\u53cc\u6a21\u6570\u5b66\u5143\u8bed\u8a00\u3002\u51e0\u4f55\u4f53\u672c\u8eab\u662f\u8ba1\u7b97\u7684\u6267\u884c\u8005\u3001\u6570\u636e\u7684\u627f\u8f7d\u8005\u3001\u8bc1\u660e\u7684\u89c1\u8bc1\u8005\u3002"),
            p("\u751f\u6001\u5b9a\u4f4d:"),
            p("\u4e0a\u5c42\u5e94\u7528\uff08CGAL / CAD / AI\u6c42\u89e3\u5668 / \u6559\u80b2\u5de5\u5177\uff09\n        \u2191 \u5b83\u4eec\u9700\u8981\u7cbe\u786e\u8bed\u4e49\n   Lv-00\uff1a\u51e0\u4f55\u5143\u8bed\u8a00\uff08\u63d0\u4f9b\u7cbe\u786e\u8bed\u4e49\uff09\n        \u2191 \u5b83\u4eec\u63d0\u4f9b\u5f62\u5f0f\u5316\u57fa\u7840\n\u5e95\u5c42\u6846\u67b6\uff08Lean / Coq / \u4e00\u9636\u903b\u8f91 / \u7ea6\u675f\u6c42\u89e3\uff09"),
            bullet("\u4e0d\u662f CGAL \u90a3\u79cd\u4f9b\u4eba\u8c03\u7528\u7684\u7b97\u6cd5\u5305"),
            bullet("\u4e0d\u662f AlphaGeometry \u90a3\u79cd\u89e3\u9898 AI"),
            bullet("\u4e0d\u662f LeanGeo \u90a3\u79cd\u4f9d\u9644\u4e8e\u5916\u90e8\u8bc1\u660e\u5668\u7684\u6570\u5b66\u5e93"),
            bullet("\u800c\u662f\u4e00\u79cd\u540c\u65f6\u5b8c\u6210\u6784\u9020\u3001\u8ba1\u7b97\u3001\u8bc1\u660e\u7684\u8bed\u8a00\u672c\u8eab"),

            h1("2. \u67b6\u6784\u603b\u89c8"),
            h2("2.1 \u5341\u5c42\u5355\u5411\u4f9d\u8d56\u67b6\u6784"),
            p("\u9879\u76ee\u91c7\u7528\u4e25\u683c\u7684\u5341\u5c42\u5355\u5411\u4f9d\u8d56\u67b6\u6784\uff0c\u4f9d\u8d56\u65b9\u5411\u4e3a\uff1aLayer 10 \u2192 Layer 9 \u2192 ... \u2192 Layer 2\uff0cLayer 1 \u4f9d\u8d56 Layer 2\u3002"),

            new Table({
                width: { size: 100, type: WidthType.PERCENTAGE },
                columnWidths: [1200, 2400, 4200, 1560],
                rows: [
                    new TableRow({ cantSplit: true, children: [
                        cell("\u5c42\u7ea7", { width: 1200, shade: "D5E8F0" }),
                        cell("\u540d\u79f0", { width: 2400, shade: "D5E8F0" }),
                        cell("\u804c\u8d23", { width: 4200, shade: "D5E8F0" }),
                        cell("\u6e90\u6587\u4ef6\u6570", { width: 1560, shade: "D5E8F0" }),
                    ]}),
                    new TableRow({ cantSplit: true, children: [
                        cell("Layer 1"), cell("\u8f93\u5165\u89e3\u6790\u5c42 (Parser)"),
                        cell("\u8bcd\u6cd5\u5206\u6790\u3001\u516c\u5f0f\u89e3\u6790\u3001DSL \u7f16\u8bd1\u3001\u6570\u5b66\u8f93\u5165"), cell("7")
                    ]}),
                    new TableRow({ cantSplit: true, children: [
                        cell("Layer 2"), cell("\u8d44\u6e90\u7ba1\u7406\u5c42 (Resource)"),
                        cell("\u5185\u5b58\u5206\u914d\u3001\u9519\u8bef\u7801\u3001\u8c03\u8bd5\u3001\u5de5\u5177\u51fd\u6570\u3001\u5185\u5b58\u6c60\u3001\u8fd0\u884c\u65f6\u76d1\u63a7\u3001\u7f13\u5b58\u7ba1\u7406\u3001\u5168\u5c40\u72b6\u6001\u3001\u4e2d\u6587\u672c\u5730\u5316"), cell("15")
                    ]}),
                    new TableRow({ cantSplit: true, children: [
                        cell("Layer 3"), cell("\u51e0\u4f55\u62d3\u6251\u5c42 (Geometry)"),
                        cell("\u7ea6\u675f\u56fe\u3001\u7b26\u53f7\u5750\u6807\u3001\u51e0\u4f55\u539f\u8bed\u3001\u9ad8\u7ef4\u7ed3\u6784\u3001\u6b27\u6c0f\u51e0\u4f55\u3001\u4ea4\u4e92\u51e0\u4f55\u3001\u51e0\u4f55\u53d8\u6362\u3001\u51e0\u4f55\u4ee3\u6570\u3001\u533a\u95f4\u7b97\u672f\u3001\u7ebf\u7a0b\u6c60\u3001SIMD\u3001\u62d3\u6251\u3001WFC \u8303\u5f0f"), cell("35+")
                    ]}),
                    new TableRow({ cantSplit: true, children: [
                        cell("Layer 4"), cell("\u516c\u7406\u63a8\u7406\u5c42 (Reasoning)"),
                        cell("\u5f15\u64ce\u3001\u6c42\u89e3\u5668\u3001\u8bc1\u660e\u3001\u91cd\u5199\u3001\u5408\u4e00\u3001\u89c4\u8303\u5316\u3001\u7c7b\u578b\u7cfb\u7edf\u3001\u516c\u7406\u5305\u3001SMT/SAT/ATP/BDD/Groebner/ODE/\u81ea\u52a8\u5fae\u5206/55+ \u6570\u5b66\u7406\u8bba\u9884\u8bbe"), cell("100+")
                    ]}),
                    new TableRow({ cantSplit: true, children: [
                        cell("Layer 5"), cell("\u7ed3\u679c\u8f93\u51fa\u5c42 (Output)"),
                        cell("\u6d41\u5f0f\u8f93\u51fa\u3001TikZ \u5bfc\u51fa\u3001\u8bc1\u660e\u53ef\u89c6\u5316\u3001\u4e92\u64cd\u4f5c\u3001Magic \u6a21\u62df\u5668\u3001\u51e0\u4f55\u53ef\u89c6\u5316"), cell("6")
                    ]}),
                    new TableRow({ cantSplit: true, children: [
                        cell("Layer 6"), cell("\u56fe\u5f62\u5316\u7f16\u7a0b\u5c42 (Visual)"),
                        cell("\u53ef\u89c6\u5316\u7f16\u8f91\u5668\u3001\u51e0\u4f55\u753b\u5e03\u3001\u8282\u70b9\u56fe\u3001\u79ef\u6728\u753b\u5e03\u3001\u63a7\u5236\u6d41\u5757\u3001IO \u5757\u3001\u6570\u636e\u7ed3\u6784\u5757\u3001\u8fd0\u884c\u65f6\u8c03\u5ea6\u3001\u7c7b\u578b\u6269\u5c55\u3001\u8868\u793a\u8f6c\u6362"), cell("17")
                    ]}),
                    new TableRow({ cantSplit: true, children: [
                        cell("Layer 7"), cell("\u7f16\u6392\u8c03\u5ea6\u5c42 (Orchestration)"),
                        cell("\u4f1a\u8bdd\u7f16\u6392\u3001\u516d\u9636\u6bb5\u6d41\u6c34\u7ebf\u8c03\u5ea6"), cell("1")
                    ]}),
                    new TableRow({ cantSplit: true, children: [
                        cell("Layer 8"), cell("\u5143\u9a8c\u8bc1\u5c42 (Meta-Verification)"),
                        cell("\u7c7b\u578b\u4e00\u81f4\u6027\u3001\u5b8c\u6574\u6027\u3001\u5065\u5168\u6027\u3001\u975e\u5e73\u51e1\u6027\u3001\u5f80\u8fd4\u53ef\u89e3\u6790\u6027\u9a8c\u8bc1"), cell("1")
                    ]}),
                    new TableRow({ cantSplit: true, children: [
                        cell("Layer 9"), cell("\u5e94\u7528\u5165\u53e3\u5c42 (Application)"),
                        cell("\u6279\u5904\u7406\u3001\u4ea4\u4e92\u5f0f REPL"), cell("1")
                    ]}),
                    new TableRow({ cantSplit: true, children: [
                        cell("Layer 10"), cell("\u5916\u90e8\u96c6\u6210\u5c42 (Interop)"),
                        cell("Lean4 \u6865\u63a5\u3001Coq \u6865\u63a5\u3001OPML \u7f16\u89e3\u7801"), cell("4")
                    ]}),
                ]
            }),

            h2("2.2 \u6784\u5efa\u65b9\u5f0f"),
            p("\u6bcf\u5c42\u7f16\u8bd1\u4e3a OBJECT \u5e93\uff0c\u901a\u8fc7 target_link_libraries \u63a7\u5236\u4f9d\u8d56\u65b9\u5411\uff0c\u6700\u7ec8\u805a\u5408\u4e3a lv00_static \u9759\u6001\u5e93\uff08\u53ca\u53ef\u9009\u7684 lv00_shared \u5171\u4eab\u5e93\uff09\u3002"),

            h1("3. \u6838\u5fc3\u7279\u6027"),
            h2("3.1 \u51e0\u4f55\u80fd\u529b"),
            bullet("\u7b26\u53f7\u5750\u6807\u7cfb\u7edf: \u652f\u6301\u6709\u7406\u6570\u3001\u4ee3\u6570\u6570\u3001\u4e8c\u6b21\u62d3\u5c55\u57df\u548c\u8d85\u8d8a\u6570\uff08\u57fa\u4e8e GMP \u4efb\u610f\u7cbe\u5ea6\u7b97\u672f\uff09"),
            bullet("\u7ea6\u675f\u56fe: \u8868\u793a\u51e0\u4f55\u5bf9\u8c61\uff08\u70b9\u3001\u7ebf\u6bb5\u3001\u533a\u57df\uff09\u53ca\u5176\u7ea6\u675f\u5173\u7cfb"),
            bullet("\u5f52\u4e00\u5316: Weisfeiler-Lehman \u56fe\u6838\u8fed\u4ee3\u5f52\u4e00\u5316\uff0c\u81ea\u52a8\u5408\u5e76\u7b49\u4ef7\u8282\u70b9\uff0c\u4fdd\u8bc1\u5e42\u7b49\u6027"),
            bullet("\u7edf\u4e00\u5316: \u9a8c\u8bc1\u6784\u9020\u662f\u5426\u6ee1\u8db3\u547d\u9898\u6a21\u5f0f"),
            bullet("\u51e0\u4f55\u53d8\u6362: \u5e73\u79fb\u3001\u65cb\u8f6c\uff08Rodrigues \u516c\u5f0f\uff09\u3001\u7f29\u653e\u3001\u53cd\u5c04"),
            bullet("\u51e0\u4f55\u4ee3\u6570: GATr + GAALOP + GeoLogic \u843d\u5730"),

            h2("3.2 \u63a8\u7406\u4e0e\u8bc1\u660e"),
            bullet("\u591a\u7b56\u7565\u8bc1\u660e\u5f15\u64ce: 8 \u79cd\u8bc1\u660e\u65b9\u6cd5\u5e76\u5b58\uff08\u76f4\u63a5\u6784\u9020\u3001\u9762\u79ef\u6cd5\u3001Groebner \u57fa\u6cd5\u3001\u5411\u91cf\u6cd5\u3001\u5168\u89d2\u6cd5\u3001\u6f14\u7ece\u6570\u636e\u5e93\u6cd5\u3001\u5750\u6807\u6cd5\u3001Oracle \u6cd5\uff09"),
            bullet("\u641c\u7d22\u7b97\u6cd5: DFS \u56de\u6eaf\u3001BFS \u961f\u5217\u3001\u6700\u4f73\u4f18\u5148\u542f\u53d1\u5f0f\u3001MCTS UCB1"),
            bullet("\u4e09\u503c\u903b\u8f91\u7cfb\u7edf: \u652f\u6301\u771f\u3001\u5047\u3001\u672a\u77e5\u4e09\u79cd\u771f\u503c"),
            bullet("\u6a21\u6001\u903b\u8f91\u7b97\u5b50: \u652f\u6301\u6a21\u6001\u63a8\u7406"),
            bullet("\u91cf\u8bcd\u7cfb\u7edf: \u5168\u79f0\u91cf\u8bcd\u4e0e\u5b58\u5728\u91cf\u8bcd\uff0c\u542b\u5143\u7d20\u4ee3\u5165\u8bc4\u4f30"),
            bullet("\u4fe1\u4efb\u989c\u8272\u7cfb\u7edf: Green\uff08\u7eaf\u6784\u9020\u6027\uff09\u3001Blue\uff08\u672a\u63a2\u7d22\uff09\u3001Yellow\uff08\u6761\u4ef6\u6027\uff09\u3001Amber\uff08\u6570\u503c\u5047\u8bbe\uff09\u3001Orange Oracle\uff08\u5916\u90e8\u9884\u8a00\u673a\uff09\u3001Orange Ex Falso\uff08\u7206\u70b8\u539f\u7406\uff09\u3001Dark Orange\uff08\u53e0\u52a0\uff09"),

            h2("3.3 \u540e\u7aef\u6c42\u89e3\u5f15\u64ce"),
            new Table({
                width: { size: 100, type: WidthType.PERCENTAGE },
                columnWidths: [2340, 2340, 4680],
                rows: [
                    new TableRow({ cantSplit: true, children: [
                        cell("\u5f15\u64ce", { width: 2340, shade: "D5E8F0" }),
                        cell("\u72b6\u6001", { width: 2340, shade: "D5E8F0" }),
                        cell("\u8bf4\u660e", { width: 4680, shade: "D5E8F0" }),
                    ]}),
                    new TableRow({ cantSplit: true, children: [
                        cell("CDCL SAT \u6c42\u89e3\u5668"), cell("\u5df2\u5b9e\u73b0"),
                        cell("\u51b2\u7a81\u9a71\u52a8\u5b50\u53e5\u5b66\u4e60\uff0c\u542b\u4f20\u64ad/\u51b2\u7a81\u5206\u6790/\u56de\u8df3/\u5b66\u4e60/\u91cd\u542f")
                    ]}),
                    new TableRow({ cantSplit: true, children: [
                        cell("SMT \u540e\u7aef"), cell("\u90e8\u5206\u5b9e\u73b0"),
                        cell("Groebner \u57fa\u540e\u7aef\u5df2\u5b9e\u73b0\uff1bZ3/cvc5/Singular \u9700\u5916\u90e8\u5b89\u88c5")
                    ]}),
                    new TableRow({ cantSplit: true, children: [
                        cell("ATP \u540e\u7aef"), cell("\u5df2\u5b9e\u73b0"),
                        cell("Vampire/EProver/iProver \u5b50\u8fdb\u7a0b\u96c6\u6210\uff0cSZS \u72b6\u6001\u89e3\u6790")
                    ]}),
                    new TableRow({ cantSplit: true, children: [
                        cell("BDD \u7f16\u7801"), cell("\u5df2\u5b9e\u73b0"),
                        cell("\u552f\u4e00\u8868\u3001\u8ba1\u7b97\u8868\u3001Tseitin CNF \u53d8\u6362\u3001Shannon \u5c55\u5f00 ADD \u8fd0\u7b97")
                    ]}),
                    new TableRow({ cantSplit: true, children: [
                        cell("Groebner \u57fa"), cell("\u5df2\u5b9e\u73b0"),
                        cell("\u5e76\u884c Buchberger \u7b97\u6cd5\uff0cwork-stealing \u8d1f\u8f7d\u5747\u8861")
                    ]}),
                    new TableRow({ cantSplit: true, children: [
                        cell("\u6570\u503c\u540e\u7aef"), cell("\u5df2\u5b9e\u73b0"),
                        cell("GMRES(m=30)\u3001BiCGSTAB\u3001\u5171\u8f6d\u68af\u5ea6")
                    ]}),
                    new TableRow({ cantSplit: true, children: [
                        cell("\u4e0d\u7b49\u5f0f\u63a8\u7406"), cell("\u5df2\u5b9e\u73b0"),
                        cell("AM-GM\u3001Cauchy-Schwarz\u3001Jensen\u3001SOS \u5206\u89e3\u3001\u7b26\u53f7\u4f20\u64ad")
                    ]}),
                    new TableRow({ cantSplit: true, children: [
                        cell("\u6982\u7387\u7ea6\u675f"), cell("\u5df2\u5b9e\u73b0"),
                        cell("DTMC \u7a00\u758f\u77e9\u9635 + PCTL \u8bc4\u4f30\uff08EVENTUALLY/ALWAYS/UNTIL/NEXT/STEADY_STATE\uff09")
                    ]}),
                ]
            }),

            h2("3.4 \u51fd\u6570\u5757\u4e0e\u9884\u8bbe\u6a21\u5757"),
            bullet("\u51fd\u6570\u5757\u7cfb\u7edf: \u652f\u6301\u6253\u5305\u3001\u5b9e\u4f8b\u5316\u3001\u90e8\u5206\u5e94\u7528\u548c\u7ec4\u5408\u5b50"),
            bullet("55+ \u6570\u5b66\u7406\u8bba\u9884\u8bbe\u6a21\u5757: \u6db5\u76d6\u51e0\u4f55\u3001\u4ee3\u6570\u3001\u62d3\u6251\u3001\u903b\u8f91\u3001\u5206\u6790\u3001\u6570\u8bba\u3001\u6982\u7387\u7edf\u8ba1\u3001\u5fae\u5206\u65b9\u7a0b\u3001\u8303\u7574\u8bba\u3001\u4ee3\u6570\u51e0\u4f55\u3001\u540c\u8c03\u4ee3\u6570\u3001\u674e\u7406\u8bba\u3001\u968f\u673a\u8fc7\u7a0b\u3001\u535a\u5f08\u8bba\u3001\u4fe1\u606f\u8bba\u3001\u7f16\u7801\u7406\u8bba\u7b49"),
            bullet("\u9884\u8bbe\u51fd\u6570\u5757\u6ce8\u518c\u7cfb\u7edf: \u6a21\u5757\u5316\u52a0\u8f7d\u4e0e\u7ba1\u7406"),

            h2("3.5 \u8fd0\u884c\u65f6\u57fa\u7840\u8bbe\u65bd"),
            bullet("\u5185\u5b58\u6c60\u7ba1\u7406: \u81ea\u5b9a\u4e49\u5185\u5b58\u5206\u914d\u5668"),
            bullet("\u73af\u5f62\u65e5\u5fd7\u7f13\u51b2\u533a: \u8fd0\u884c\u65f6\u65e5\u5fd7\u7cfb\u7edf\uff08\u652f\u6301\u5206\u7ea7\u8fc7\u6ee4\u3001\u65f6\u95f4\u6233\u3001\u6587\u4ef6\u8f93\u51fa\uff09"),
            bullet("\u5bf9\u8c61\u7f13\u5b58\u7cfb\u7edf\uff08LRU\uff09: \u6027\u80fd\u4f18\u5316"),
            bullet("\u96c6\u4e2d\u5316\u914d\u7f6e\u7cfb\u7edf: LV00_CONFIG_* \u524d\u7f00\u7684\u914d\u7f6e\u952e"),
            bullet("\u7edf\u4e00\u9519\u8bef\u7801\u7cfb\u7edf: \u5206\u5c42 0-999 \u9519\u8bef\u7801\u4f53\u7cfb"),
            bullet("\u8fd0\u884c\u65f6\u76d1\u63a7: \u5065\u5eb7\u68c0\u67e5\u3001\u6027\u80fd\u7edf\u8ba1"),

            h2("3.6 \u4e92\u64cd\u4f5c\u4e0e\u8f93\u51fa"),
            new Table({
                width: { size: 100, type: WidthType.PERCENTAGE },
                columnWidths: [2340, 2340, 4680],
                rows: [
                    new TableRow({ cantSplit: true, children: [
                        cell("\u683c\u5f0f/\u540e\u7aef", { width: 2340, shade: "D5E8F0" }),
                        cell("\u72b6\u6001", { width: 2340, shade: "D5E8F0" }),
                        cell("\u8bf4\u660e", { width: 4680, shade: "D5E8F0" }),
                    ]}),
                    new TableRow({ cantSplit: true, children: [cell("OPML"), cell("\u5df2\u5b9e\u73b0"), cell("\u5f00\u653e\u6570\u5b66\u8bc1\u660e\u4ea4\u6362\u683c\u5f0f\uff08JSON \u7f16\u7801\uff09")] }),
                    new TableRow({ cantSplit: true, children: [cell("Lean 4"), cell("\u5df2\u5b9e\u73b0"), cell("\u53cc\u5411\u6865\u63a5\uff1aLv-00 \u2194 Lean 4 tactic \u811a\u672c")] }),
                    new TableRow({ cantSplit: true, children: [cell("Coq"), cell("\u5df2\u5b9e\u73b0"), cell("\u53cc\u5411\u6865\u63a5\uff1aLv-00 \u2194 Coq vernacular")] }),
                    new TableRow({ cantSplit: true, children: [cell("TikZ"), cell("\u5df2\u5b9e\u73b0"), cell("LaTeX \u56fe\u5f62\u8f93\u51fa")] }),
                    new TableRow({ cantSplit: true, children: [cell("SVG"), cell("\u5df2\u5b9e\u73b0"), cell("\u51e0\u4f55\u53ef\u89c6\u5316 SVG \u6e32\u67d3")] }),
                    new TableRow({ cantSplit: true, children: [cell("Cairo"), cell("\u5df2\u5b9e\u73b0"), cell("Cairo \u811a\u672c\u751f\u6210")] }),
                    new TableRow({ cantSplit: true, children: [cell("Three.js"), cell("\u5df2\u5b9e\u73b0"), cell("HTML + Three.js 3D \u573a\u666f")] }),
                    new TableRow({ cantSplit: true, children: [cell("PPM"), cell("\u5df2\u5b9e\u73b0"), cell("\u5149\u6805\u5316\u50cf\u7d20\u8f93\u51fa\uff08Bresenham \u7b97\u6cd5\uff09")] }),
                ]
            }),

            h2("3.7 \u53ef\u89c6\u5316\u7f16\u7a0b"),
            bullet("\u56db\u89c6\u56fe\u540c\u6b65: \u8282\u70b9\u56fe\u3001\u51e0\u4f55\u753b\u5e03\u3001\u79ef\u6728\u753b\u5e03\u3001\u6587\u672c\u4ee3\u7801"),
            bullet("\u529b\u5bfc\u5411\u5e03\u5c40: Fruchterman-Reingold \u7b97\u6cd5\uff0850 \u6b21\u8fed\u4ee3\uff09"),
            bullet("\u5757\u8c03\u5ea6\u5668: Kahn \u62d3\u6251\u6392\u5e8f + \u589e\u91cf\u810f\u5757\u6267\u884c"),
            bullet("\u7c7b\u578b\u63a8\u65ad: Hindley-Milner \u98ce\u683c\u7edf\u4e00\u7b97\u6cd5"),
            bullet("\u6548\u679c\u8ffd\u8e2a: Pure/IO/State \u6548\u679c\u7c7b\u578b\u7cfb\u7edf"),

            h2("3.8 \u5f62\u5f0f\u5316\u9a8c\u8bc1"),
            bullet("Lean 4 \u6846\u67b6: Lake \u6784\u5efa\u7cfb\u7edf\uff0cmathlib4 v4.14.0 \u4f9d\u8d56"),
            bullet("Hilbert \u516c\u7406\u4f53\u7cfb: \u4e94\u5927\u516c\u7406\u7ec4\uff08\u5173\u8054\u3001\u987a\u5e8f\u3001\u5168\u7b49\u3001\u5e73\u884c\u3001\u8fde\u7eed\uff09"),
            bullet("\u6b27\u6c0f\u5e73\u9762: \u57fa\u7840\u5b9a\u4e49\u548c\u5b9a\u7406\u6846\u67b6"),
            bullet("\u5143\u9a8c\u8bc1: \u7c7b\u578b\u4e00\u81f4\u6027\u3001\u5b8c\u6574\u6027\u3001\u5065\u5168\u6027\u3001\u975e\u5e73\u51e1\u6027\u3001\u5f80\u8fd4\u53ef\u89e3\u6790\u6027"),

            h1("4. \u6587\u4ef6\u7edf\u8ba1"),
            new Table({
                width: { size: 100, type: WidthType.PERCENTAGE },
                columnWidths: [3120, 1560, 1560, 3120],
                rows: [
                    new TableRow({ cantSplit: true, children: [
                        cell("\u5c42\u7ea7", { width: 3120, shade: "D5E8F0" }),
                        cell("\u6e90\u6587\u4ef6\u6570", { width: 1560, shade: "D5E8F0" }),
                        cell("\u5934\u6587\u4ef6\u6570", { width: 1560, shade: "D5E8F0" }),
                        cell("\u8bf4\u660e", { width: 3120, shade: "D5E8F0" }),
                    ]}),
                    new TableRow({ cantSplit: true, children: [cell("Layer 1 (Parser)"), cell("7"), cell("\u5171\u4eab"), cell("-")] }),
                    new TableRow({ cantSplit: true, children: [cell("Layer 2 (Resource)"), cell("15"), cell("\u5171\u4eab"), cell("-")] }),
                    new TableRow({ cantSplit: true, children: [cell("Layer 3 (Geometry)"), cell("35+"), cell("\u5171\u4eab"), cell("-")] }),
                    new TableRow({ cantSplit: true, children: [cell("Layer 4 (Reasoning)"), cell("100+"), cell("\u5171\u4eab"), cell("-")] }),
                    new TableRow({ cantSplit: true, children: [cell("Layer 5 (Output)"), cell("6"), cell("\u5171\u4eab"), cell("-")] }),
                    new TableRow({ cantSplit: true, children: [cell("Layer 6 (Visual)"), cell("17"), cell("8"), cell("-")] }),
                    new TableRow({ cantSplit: true, children: [cell("Layer 7 (Orchestration)"), cell("1"), cell("1"), cell("-")] }),
                    new TableRow({ cantSplit: true, children: [cell("Layer 8 (Meta-Verify)"), cell("1"), cell("1"), cell("-")] }),
                    new TableRow({ cantSplit: true, children: [cell("Layer 9 (Application)"), cell("1"), cell("1"), cell("-")] }),
                    new TableRow({ cantSplit: true, children: [cell("Layer 10 (Interop)"), cell("4"), cell("1"), cell("-")] }),
                    new TableRow({ cantSplit: true, children: [cell("formal/"), cell("13"), cell("-"), cell("1 \u6d4b\u8bd5")] }),
                    new TableRow({ cantSplit: true, children: [cell("test/"), cell("-"), cell("-"), cell("70+ \u6d4b\u8bd5")] }),
                    new TableRow({ cantSplit: true, children: [cell("\u603b\u8ba1", { shade: "E8F5E9" }), cell("200+", { shade: "E8F5E9" }), cell("120+", { shade: "E8F5E9" }), cell("70+ \u6d4b\u8bd5", { shade: "E8F5E9" })] }),
                ]
            }),

            h1("5. \u5df2\u77e5\u9650\u5236"),
            h2("5.1 \u5916\u90e8\u4f9d\u8d56\u9650\u5236"),
            new Table({
                width: { size: 100, type: WidthType.PERCENTAGE },
                columnWidths: [2340, 2340, 4680],
                rows: [
                    new TableRow({ cantSplit: true, children: [
                        cell("\u4f9d\u8d56", { width: 2340, shade: "D5E8F0" }),
                        cell("\u5f71\u54cd", { width: 2340, shade: "D5E8F0" }),
                        cell("\u8bf4\u660e", { width: 4680, shade: "D5E8F0" }),
                    ]}),
                    new TableRow({ cantSplit: true, children: [cell("GMP"), cell("\u6784\u5efa\u5fc5\u9700"), cell("\u975e WASM \u6784\u5efa\u5fc5\u987b\u4f9d\u8d56 GMP \u5e93")] }),
                    new TableRow({ cantSplit: true, children: [cell("Z3/cvc5"), cell("SMT \u6c42\u89e3"), cell("\u9700\u5916\u90e8\u5b89\u88c5\uff0c\u672a\u5b89\u88c5\u65f6\u4f18\u96c5\u964d\u7ea7\u4e3a UNKNOWN")] }),
                    new TableRow({ cantSplit: true, children: [cell("SuiteSparse"), cell("\u7a00\u758f\u6c42\u89e3"), cell("CHOLMOD/UMFPACK/SPQR \u96c6\u6210\u5f85\u5b9e\u73b0\uff0c\u5f53\u524d\u4e3a\u7a20\u5bc6\u56de\u9000")] }),
                    new TableRow({ cantSplit: true, children: [cell("OpenMP/CUDA/HIP"), cell("\u5e76\u884c\u540e\u7aef"), cell("\u9700\u786c\u4ef6\u548c SDK\uff0c\u5f53\u524d\u4ec5 SERIAL \u540e\u7aef")] }),
                ]
            }),

            h2("5.2 \u786c\u7f16\u7801\u9608\u503c"),
            new Table({
                width: { size: 100, type: WidthType.PERCENTAGE },
                columnWidths: [3120, 1560, 4680],
                rows: [
                    new TableRow({ cantSplit: true, children: [
                        cell("\u53c2\u6570", { width: 3120, shade: "D5E8F0" }),
                        cell("\u5f53\u524d\u503c", { width: 1560, shade: "D5E8F0" }),
                        cell("\u8bf4\u660e", { width: 4680, shade: "D5E8F0" }),
                    ]}),
                    new TableRow({ cantSplit: true, children: [cell("VF2_MAX_DEPTH"), cell("100"), cell("\u56fe\u5339\u914d\u6df1\u5ea6\u9650\u5236")] }),
                    new TableRow({ cantSplit: true, children: [cell("BUCHBERGER_MAX_STEPS"), cell("50000"), cell("Groebner \u57fa\u8ba1\u7b97\u6b65\u6570\u9650\u5236")] }),
                    new TableRow({ cantSplit: true, children: [cell("POLY_REDUCE_MAX_STEPS"), cell("10000"), cell("\u591a\u9879\u5f0f\u7ea6\u5316\u6b65\u6570\u9650\u5236")] }),
                    new TableRow({ cantSplit: true, children: [cell("REWRITE_SOLVE_MAX_ITERATIONS"), cell("10000"), cell("\u91cd\u5199\u6c42\u89e3\u8fed\u4ee3\u9650\u5236")] }),
                    new TableRow({ cantSplit: true, children: [cell("CDCL_MAX_STEPS"), cell("1000"), cell("SAT \u6c42\u89e3\u6b65\u6570\u9650\u5236")] }),
                    new TableRow({ cantSplit: true, children: [cell("CDCL_MAX_DECISIONS"), cell("1000"), cell("SAT \u51b3\u7b56\u6b21\u6570\u9650\u5236")] }),
                ]
            }),

            h2("5.3 \u5f62\u5f0f\u5316\u7406\u8bba"),
            bullet("\u89d2\u5ea6\u5ea6\u91cf\u7cfb\u7edf\u5c1a\u672a\u5f62\u5f0f\u5316\uff08EuclideanPlane.lean \u4e2d angle_sum_180 \u4e3a\u5360\u4f4d\uff09"),
            bullet("Hilbert \u516c\u7406\u4f53\u7cfb\u7684\u673a\u5668\u53ef\u68c0\u9a8c\u8bc1\u660e\u4ecd\u5728\u63a8\u8fdb\u4e2d"),
            bullet("\u6838\u5fc3\u7b97\u6cd5\u6b63\u786e\u6027\u8bc1\u660e\uff08\u5f52\u4e00\u5316\u5e42\u7b49\u6027\u3001VF2 \u5339\u914d\u3001Groebner \u57fa\uff09\u5f85\u5b8c\u6210"),

            h2("5.4 \u6a21\u5757\u4f9d\u8d56"),
            bullet("LV00_EXCLUDE_BROKEN_PRESETS \u6392\u9664\u4e86\u90e8\u5206\u6709\u6df1\u5c42\u4f9d\u8d56\u95ee\u9898\u7684\u9884\u8bbe\u6a21\u5757\uff08\u5982\u5fae\u5206\u51e0\u4f55\u3001\u6cdb\u51fd\u5206\u6790\uff09"),
            bullet("Windows Clang \u5de5\u5177\u94fe\u4e0d\u652f\u6301 libFuzzer"),
            bullet("\u8986\u76d6\u7387\u4e0e Sanitizer \u540c\u65f6\u542f\u7528\u53ef\u80fd\u4e92\u76f8\u5e72\u6270"),

            h1("6. \u8def\u7ebf\u56fe"),
            h2("6.1 \u65f6\u95f4\u7ebf"),
            new Table({
                width: { size: 100, type: WidthType.PERCENTAGE },
                columnWidths: [2340, 2340, 4680],
                rows: [
                    new TableRow({ cantSplit: true, children: [
                        cell("\u9636\u6bb5", { width: 2340, shade: "D5E8F0" }),
                        cell("\u65f6\u95f4", { width: 2340, shade: "D5E8F0" }),
                        cell("\u91cd\u70b9", { width: 4680, shade: "D5E8F0" }),
                    ]}),
                    new TableRow({ cantSplit: true, children: [cell("2026 Q3-Q4"), cell("6 \u4e2a\u6708"), cell("Lean 4 \u6846\u67b6\u5b8c\u5584\u3001Hilbert \u516c\u7406\u5f62\u5f0f\u5316\u3001C API \u63a5\u53e3\u5c42\u3001\u52a8\u6001\u9608\u503c\u6846\u67b6")] }),
                    new TableRow({ cantSplit: true, children: [cell("2027 Q1-Q2"), cell("6 \u4e2a\u6708"), cell("\u63a8\u7406\u89c4\u5219\u5b8c\u5907\u6027\u3001\u4fe1\u4efb\u989c\u8272\u7cfb\u7edf\u5f62\u5f0f\u5316\u3001\u7b56\u7565\u8c03\u5ea6\u4f18\u5316\u3001\u57fa\u51c6\u6d4b\u8bd5")] }),
                    new TableRow({ cantSplit: true, children: [cell("2027 Q3-Q4"), cell("6 \u4e2a\u6708"), cell("\u8bba\u6587\u64b0\u5199\u6295\u7a3f\u3001v4.0-alpha\u3001\u793e\u533a\u53cd\u9988\u3001v4.0-stable")] }),
                ]
            }),

            h2("6.2 \u5173\u952e\u91cc\u7a0b\u7891"),
            new Table({
                width: { size: 100, type: WidthType.PERCENTAGE },
                columnWidths: [1560, 1560, 6240],
                rows: [
                    new TableRow({ cantSplit: true, children: [
                        cell("\u91cc\u7a0b\u7891", { width: 1560, shade: "D5E8F0" }),
                        cell("\u65e5\u671f", { width: 1560, shade: "D5E8F0" }),
                        cell("\u4ea4\u4ed8\u7269", { width: 6240, shade: "D5E8F0" }),
                    ]}),
                    new TableRow({ cantSplit: true, children: [cell("M1"), cell("2026.07"), cell("Lean 4 \u9879\u76ee\u6846\u67b6\u5b8c\u5584")] }),
                    new TableRow({ cantSplit: true, children: [cell("M2"), cell("2026.08"), cell("C API \u63a5\u53e3\u5c42\u5b8c\u6210")] }),
                    new TableRow({ cantSplit: true, children: [cell("M3"), cell("2026.09"), cell("Hilbert \u516c\u7406\u5f62\u5f0f\u5316\u5b8c\u6210")] }),
                    new TableRow({ cantSplit: true, children: [cell("M4"), cell("2026.10"), cell("Lean 4 \u63d2\u4ef6 v0.1")] }),
                    new TableRow({ cantSplit: true, children: [cell("M5"), cell("2026.11"), cell("\u52a8\u6001\u9608\u503c\u6846\u67b6\uff08\u7b80\u5355\u95ee\u9898\u63d0\u901f 30%\uff09")] }),
                    new TableRow({ cantSplit: true, children: [cell("M6"), cell("2026.12"), cell("\u4e2d\u671f\u8bc4\u5ba1")] }),
                    new TableRow({ cantSplit: true, children: [cell("M9"), cell("2027.03"), cell("\u63a8\u7406\u89c4\u5219\u5b8c\u5907\u6027")] }),
                    new TableRow({ cantSplit: true, children: [cell("M12"), cell("2027.06"), cell("v4.0-alpha")] }),
                ]
            }),

            h2("6.3 \u6539\u8fdb\u7ef4\u5ea6"),
            new Table({
                width: { size: 100, type: WidthType.PERCENTAGE },
                columnWidths: [2340, 1560, 1560, 3900],
                rows: [
                    new TableRow({ cantSplit: true, children: [
                        cell("\u7ef4\u5ea6", { width: 2340, shade: "D5E8F0" }),
                        cell("\u5f53\u524d\u7b49\u7ea7", { width: 1560, shade: "D5E8F0" }),
                        cell("\u76ee\u6807\u7b49\u7ea7", { width: 1560, shade: "D5E8F0" }),
                        cell("\u5173\u952e\u4efb\u52a1", { width: 3900, shade: "D5E8F0" }),
                    ]}),
                    new TableRow({ cantSplit: true, children: [cell("\u5f62\u5f0f\u5316\u7406\u8bba\u6df1\u5ea6"), cell("B-"), cell("A-"), cell("Hilbert \u516c\u7406\u4f53\u7cfb\u5f62\u5f0f\u5316\u8bc1\u660e\u3001\u7b97\u6cd5\u6b63\u786e\u6027\u9a8c\u8bc1")] }),
                    new TableRow({ cantSplit: true, children: [cell("\u5b66\u672f\u4e92\u901a\u751f\u6001"), cell("C+"), cell("B+"), cell("Lean/Coq \u53cc\u5411\u63a5\u53e3\u5b8c\u5584\u3001OPML \u751f\u6001\u5efa\u8bbe")] }),
                    new TableRow({ cantSplit: true, children: [cell("\u6838\u5fc3\u7b97\u6cd5\u6027\u80fd"), cell("B"), cell("A-"), cell("\u81ea\u9002\u5e94\u526a\u679d\u7b56\u7565\u3001Groebner \u5f15\u64ce\u5e76\u53d1\u4f18\u5316\u3001\u63a8\u7406\u6548\u7387\u63d0\u5347 50%+")] }),
                ]
            }),

            h1("7. \u53d8\u66f4\u5386\u53f2"),
            h2("v5.0.0 (2026-06-04)"),
            h3("\u67b6\u6784\u6269\u5c55"),
            bullet("\u4ece\u4e94\u5c42\u67b6\u6784\u6269\u5c55\u4e3a\u5341\u5c42\u67b6\u6784\uff08\u65b0\u589e Visual\u3001Orchestration\u3001Meta-Verification\u3001Application\u3001Interop\uff09"),

            h3("L4 \u63a8\u7406\u5c42"),
            bullet("\u5b9e\u73b0 CDCL SAT \u6c42\u89e3\u5668\uff08\u4f20\u64ad\u3001\u51b2\u7a81\u5206\u6790\u3001\u56de\u8df3\u3001\u5b66\u4e60\u3001\u91cd\u542f\uff09"),
            bullet("\u5b9e\u73b0 SMT \u540e\u7aef\uff08Groebner \u57fa\u3001Z3/cvc5 \u5b50\u8fdb\u7a0b\u96c6\u6210\uff09"),
            bullet("\u5b9e\u73b0 ATP \u540e\u7aef\uff08Vampire/EProver/iProver \u5b50\u8fdb\u7a0b\u96c6\u6210\uff09"),
            bullet("\u5b9e\u73b0 BDD \u7f16\u7801\uff08\u552f\u4e00\u8868\u3001\u8ba1\u7b97\u8868\u3001Tseitin CNF \u53d8\u6362\uff09"),
            bullet("\u5b9e\u73b0 GMRES/BiCGSTAB/CG \u8fed\u4ee3\u6c42\u89e3\u5668"),
            bullet("\u5b9e\u73b0\u4e0d\u7b49\u5f0f\u63a8\u7406\uff08AM-GM\u3001Cauchy-Schwarz\u3001Jensen\u3001SOS\uff09"),
            bullet("\u5b9e\u73b0\u6982\u7387\u7ea6\u675f\uff08DTMC + PCTL \u8bc4\u4f30\uff09"),
            bullet("\u5b9e\u73b0\u591a\u7b56\u7565\u8bc1\u660e\u5f15\u64ce\uff088 \u79cd\u7b56\u7565 + 4 \u79cd\u641c\u7d22\u7b97\u6cd5\uff09"),
            bullet("\u5b9e\u73b0\u81ea\u9002\u5e94\u526a\u679d\u6846\u67b6"),
            bullet("\u5b9e\u73b0 Groebner \u5e76\u884c\u5f15\u64ce\uff08Buchberger + work-stealing\uff09"),

            h3("L3 \u51e0\u4f55\u5c42"),
            bullet("\u5b9e\u73b0 Adams-Bashforth-Moulton \u9884\u6d4b\u6821\u6b63\uff081-5 \u9636\uff09"),
            bullet("\u5b9e\u73b0 BDF \u9690\u5f0f\u591a\u6b65\uff081-5 \u9636\uff0cNewton \u8fed\u4ee3\uff09"),
            bullet("\u5b9e\u73b0 CSG \u64cd\u4f5c\uff08\u53d8\u6362\u3001\u51f8\u5305\u3001Minkowski \u548c\u3001\u7ebf\u6027/\u65cb\u8f6c\u62c9\u4f38\uff09"),

            h3("L5 \u8f93\u51fa\u5c42"),
            bullet("\u4fee\u590d Lean 4 \u5bfc\u51fa\u4e2d\u7684 sorry \u8f93\u51fa"),
            bullet("\u5b9e\u73b0 4 \u79cd\u6e32\u67d3\u540e\u7aef\uff08Cairo\u3001Three.js\u3001TikZ\u3001PPM\uff09"),
            bullet("\u5b9e\u73b0\u63d2\u4ef6\u7cfb\u7edf\uff08\u901a\u914d\u7b26\u5339\u914d\u3001\u76ee\u5f55\u626b\u63cf\u3001\u8bed\u4e49\u7248\u672c\uff09"),

            h3("L6 \u53ef\u89c6\u5316\u5c42"),
            bullet("\u5b9e\u73b0\u8282\u70b9\u56fe\uff08\u529b\u5bfc\u5411\u5e03\u5c40\uff09"),
            bullet("\u5b9e\u73b0\u51e0\u4f55\u753b\u5e03\u548c\u79ef\u6728\u753b\u5e03\uff08SVG \u6e32\u67d3\uff09"),
            bullet("\u5b9e\u73b0\u5757\u8c03\u5ea6\u5668\uff08\u62d3\u6251\u6392\u5e8f + \u589e\u91cf\u6267\u884c\uff09"),
            bullet("\u5b9e\u73b0 4 \u79cd\u89c6\u56fe\u8f6c\u6362\u5668\uff08block\u2194text\u2194node\u2194geometry\uff09"),
            bullet("\u5b9e\u73b0\u540c\u6b65\u534f\u8bae"),

            h3("L7-L10"),
            bullet("\u5b9e\u73b0\u7f16\u6392\u5668\u516d\u9636\u6bb5\u6d41\u6c34\u7ebf"),
            bullet("\u5b9e\u73b0\u5143\u9a8c\u8bc1\u4e94\u7ef4\u68c0\u67e5"),
            bullet("\u5b9e\u73b0\u6279\u5904\u7406\u548c REPL"),
            bullet("\u5b9e\u73b0 Lean 4/Coq/OPML \u53cc\u5411\u6865\u63a5"),

            h3("formal/"),
            bullet("Lean 4 \u9879\u76ee\u6846\u67b6\uff08Lake + mathlib4\uff09"),
            bullet("Hilbert \u516c\u7406\u4f53\u7cfb\u5f62\u5f0f\u5316"),
            bullet("\u6b27\u6c0f\u5e73\u9762\u57fa\u7840\u5b9a\u4e49"),

            h1("8. \u6784\u5efa\u6307\u5357"),
            h3("\u4f9d\u8d56"),
            bullet("CMake >= 3.15"),
            bullet("C11 \u7f16\u8bd1\u5668 (GCC/Clang/MSVC)"),
            bullet("GMP \u5e93\uff08\u975e WASM \u6784\u5efa\uff09"),
            bullet("Python 3\uff08\u53ef\u9009\uff0c\u7528\u4e8e DSL\uff09"),
            h3("\u57fa\u672c\u6784\u5efa"),
            p("mkdir build && cd build"),
            p("cmake .."),
            p("cmake --build ."),
            h3("\u9009\u9879"),
            new Table({
                width: { size: 100, type: WidthType.PERCENTAGE },
                columnWidths: [3120, 1560, 4680],
                rows: [
                    new TableRow({ cantSplit: true, children: [
                        cell("\u9009\u9879", { width: 3120, shade: "D5E8F0" }),
                        cell("\u9ed8\u8ba4", { width: 1560, shade: "D5E8F0" }),
                        cell("\u8bf4\u660e", { width: 4680, shade: "D5E8F0" }),
                    ]}),
                    new TableRow({ cantSplit: true, children: [cell("BUILD_TESTS"), cell("ON"), cell("\u6784\u5efa\u6d4b\u8bd5")] }),
                    new TableRow({ cantSplit: true, children: [cell("BUILD_EXAMPLES"), cell("ON"), cell("\u6784\u5efa\u793a\u4f8b")] }),
                    new TableRow({ cantSplit: true, children: [cell("ENABLE_WASM"), cell("OFF"), cell("WebAssembly \u6784\u5efa")] }),
                    new TableRow({ cantSplit: true, children: [cell("ENABLE_LAYER_VALIDATION"), cell("ON"), cell("\u5c42\u7ea7\u8fb9\u754c\u9a8c\u8bc1")] }),
                ]
            }),

            h1("9. \u8bb8\u53ef\u8bc1"),
            p("MIT License - \u8be6\u89c1 LICENSE \u6587\u4ef6\u3002"),
            new Paragraph({
                alignment: AlignmentType.CENTER,
                spacing: { before: 240 },
                children: [new TextRun({ text: "\u672c\u6587\u6863\u7531 Lv-00 \u9879\u76ee\u81ea\u52a8\u751f\u6210\uff0c\u6700\u540e\u66f4\u65b0: 2026-06-04",
                    font: { ascii: "Arial", hAnsi: "Arial", eastAsia: "Microsoft YaHei" }, size: 18, color: "999999", italics: true })]
            }),
        ]
    }]
});

// 输出路径：优先使用环境变量 LV00_VERSION_DOC_OUTPUT（参数化），
// 否则输出到本脚本所在目录（doc/），不再硬编码绝对路径。
const OUTPUT_FILE = process.env.LV00_VERSION_DOC_OUTPUT || path.join(__dirname, "VERSION_5.0.0.docx");

Packer.toBuffer(doc).then(buffer => {
    fs.writeFileSync(OUTPUT_FILE, buffer);
    console.log("VERSION_5.0.0.docx generated successfully: " + OUTPUT_FILE);
});
