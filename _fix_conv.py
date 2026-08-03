import io

def load(p, enc):
    with io.open(p, "r", encoding=enc, newline="") as f:
        return f.read()

def save(p, s, enc):
    with io.open(p, "w", encoding=enc, newline="") as f:
        f.write(s)

# 1. representation_converter.c: guard legacy stub implementations
p1 = r"c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\core\src\layer2_resource\representation_converter.c"
s1 = load(p1, "utf-8")
old_start = "/* ============ Legacy 直接转换 API（桩实现） ============ */"
assert s1.count(old_start) == 1, "start anchor not unique"
s1 = s1.replace(old_start, "#if !defined(LV_HAS_LAYER6_CONVERTER)\n" + old_start)
old_tail = '''    int off = snprintf(buf, 4096, "block_from_node_%s { inputs: 0 outputs: 0 nodes: %d }", fb->name ? fb->name : "null",
                       fb->internal_node_count);
    if (off < 0 || off >= 4096) {
        lv_free((void **) &buf);
        return make_error_result("buffer overflow");
    }
    return make_success_result(buf);
}
'''
assert s1.count(old_tail) == 1, "tail anchor not unique"
s1 = s1.replace(old_tail, old_tail + "#endif /* !LV_HAS_LAYER6_CONVERTER */\n")
save(p1, s1, "utf-8")
print("rep_converter OK")

# 2. block_to_text.c: define LV_HAS_LAYER6_CONVERTER (keep BOM)
p2 = r"c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\core\src\layer6_visual\converter\block_to_text.c"
s2 = load(p2, "utf-8-sig")
old_inc = '#include "lv/representation_converter.h"'
assert s2.count(old_inc) == 1, "include anchor not unique"
new_inc = old_inc + '\n\n/* layer6 新实现接管：屏蔽 layer2 旧桩的同名直接转换 API */\n#define LV_HAS_LAYER6_CONVERTER'
s2 = s2.replace(old_inc, new_inc)
save(p2, s2, "utf-8-sig")
print("block_to_text OK")

# 3. CMakeLists.txt: propagate macro to layer2 target
p3 = r"c:\Users\xingg\Desktop\知识体系化Wiki\Lv-00\CMakeLists.txt"
s3 = load(p3, "utf-8")
old_setup = "add_library(lv_layer2_resource OBJECT ${lv_LAYER2_SOURCES} ${lv_HEADERS})\nlv_setup_layer(lv_layer2_resource 2)"
assert s3.count(old_setup) == 1, "cmake anchor not unique"
new_setup = old_setup + "\n# layer6_visual/converter 已实现同名直接转换 API（block_to_text/node/geometry），\n# 用宏守卫排除 layer2 旧桩，避免重复符号及 FuncBlock* 语义误读\ntarget_compile_definitions(lv_layer2_resource PRIVATE LV_HAS_LAYER6_CONVERTER)"
s3 = s3.replace(old_setup, new_setup)
save(p3, s3, "utf-8")
print("CMakeLists OK")