/**
 * @file test_geo_visual.c
 * @brief 几何可视化渲染模块测试
 *
 * 测试 geo_visual_complete.c 的 SVG / TikZ 渲染输出正确性。
 * 涵盖对象创建、样式设置、场景管理、渲染输出和资源释放。
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv/geo_visual.h"

#define TEST_PASS_STATEMENT tests_passed++
#define TEST_FAIL_STATEMENT tests_failed++
#include "test_helpers.h"

static int tests_passed = 0;
static int tests_failed = 0;

/* 辅助：检查文件是否包含指定字符串 */
static bool file_contains(const char *path, const char *needle) {
    FILE *f = fopen(path, "r");
    if (!f)
        return false;
    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    if (n == 0)
        return false;
    buf[n] = '\0';
    return strstr(buf, needle) != NULL;
}

/* 辅助：检查文件是否以指定前缀开头 */
static bool file_starts_with(const char *path, const char *prefix) {
    FILE *f = fopen(path, "r");
    if (!f)
        return false;
    char buf[256];
    size_t n = fread(buf, 1, strlen(prefix), f);
    fclose(f);
    buf[n] = '\0';
    return strcmp(buf, prefix) == 0;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== geo_visual 渲染模块测试 ===\n\n");
    const char *tmp_svg = "test_geo_visual_output.svg";
    const char *tmp_tikz = "test_geo_visual_output.tex";
    const char *tmp_cairo = "test_geo_visual_output_cairo.c";
    const char *tmp_threejs = "test_geo_visual_output_threejs.html";

    /* ========== 组 1：对象创建 ========== */
    printf("[组 1] 对象创建\n");
    {
        TEST("point_create: 创建点");
        lvVisualObject *pt = lv_visual_point_create(100.0f, 200.0f);
        if (pt) {
            PASS();
            lv_visual_object_destroy(pt);
        } else
            FAIL("");

        TEST("point_create: 检查类型");
        pt = lv_visual_point_create(0, 0);
        if (pt && pt->type == lv_VISUAL_POINT)
            PASS();
        else {
            FAIL("");
            if (pt)
                lv_visual_object_destroy(pt);
        }

        TEST("line_create: 创建线段");
        lvVisualObject *line = lv_visual_line_create(0, 0, 100, 100);
        if (line && line->type == lv_VISUAL_SEGMENT)
            PASS();
        else
            FAIL("");
        lv_visual_object_destroy(line);

        TEST("circle_create: 创建圆");
        lvVisualObject *circ = lv_visual_circle_create(50, 50, 30);
        if (circ && circ->type == lv_VISUAL_CIRCLE)
            PASS();
        else
            FAIL("");
        lv_visual_object_destroy(circ);

        TEST("group_create: 创建组合对象");
        lvVisualObject *children[2];
        children[0] = lv_visual_point_create(0, 0);
        children[1] = lv_visual_point_create(10, 10);
        lvVisualObject *grp = lv_visual_group_create(children, 2);
        if (grp && grp->type == lv_VISUAL_MOBJECT_GROUP && grp->children_count == 2)
            PASS();
        else
            FAIL("");
        lv_visual_object_destroy(grp);

        TEST("group_create: NULL输入应返回NULL");
        lvVisualObject *null_grp = lv_visual_group_create(NULL, 0);
        if (null_grp == NULL)
            PASS();
        else {
            FAIL("");
            lv_visual_object_destroy(null_grp);
        }
    }

    /* ========== 组 2：样式设置 ========== */
    printf("[组 2] 样式设置\n");
    {
        lvVisualObject *pt = lv_visual_point_create(0, 0);

        TEST("set_color: 设置红色");
        lv_visual_set_color(pt, 1.0f, 0.0f, 0.0f, 1.0f);
        if (pt->style.stroke_color[0] == 1.0f && pt->style.stroke_color[1] == 0.0f && pt->style.stroke_color[2] == 0.0f)
            PASS();
        else
            FAIL("");

        TEST("set_dashed: 设置虚线");
        lv_visual_set_dashed(pt, 1);
        if (pt->style.dashed == 1)
            PASS();
        else
            FAIL("");

        TEST("set_style: 完整样式");
        lvVisualStyle s = {{0.5f, 0.5f, 0.5f, 0.8f}, {0, 0, 0, 0}, 2.0f, 0.9f, 1};
        lv_visual_set_style(pt, &s);
        if (pt->style.stroke_width == 2.0f && pt->style.opacity == 0.9f)
            PASS();
        else
            FAIL("");

        lv_visual_object_destroy(pt);
    }

    /* ========== 组 3：空间变换 ========== */
    printf("[组 3] 空间变换\n");
    {
        lvVisualObject *pt = lv_visual_point_create(10, 20);

        TEST("translate: 平移 (5,10,0)");
        lv_visual_translate(pt, 5, 10, 0);
        if (pt->transform[12] == 5.0f && pt->transform[13] == 10.0f)
            PASS();
        else
            FAIL("");

        lvVisualObject *pt2 = lv_visual_point_create(10, 10);
        TEST("scale: 缩放 2x");
        lv_visual_scale(pt2, 2.0f, 2.0f);
        if (pt2->transform[0] == 2.0f && pt2->transform[5] == 2.0f)
            PASS();
        else
            FAIL("");

        lv_visual_object_destroy(pt);
        lv_visual_object_destroy(pt2);
    }

    /* ========== 组 4：场景管理 ========== */
    printf("[组 4] 场景管理\n");
    {
        TEST("scene_create: 创建空场景");
        lvVisualScene *scene = lv_visual_scene_create();
        if (scene && scene->object_count == 0)
            PASS();
        else {
            FAIL("");
            if (scene)
                lv_visual_scene_destroy(scene);
        }

        TEST("scene_add: 添加对象");
        lvVisualObject *pt = lv_visual_point_create(10, 20);
        lv_visual_scene_add(scene, pt);
        if (scene->object_count == 1)
            PASS();
        else
            FAIL("");

        TEST("scene_set_camera: 设置相机");
        lv_visual_scene_set_camera(scene, 50, 50, 0, 2.0f);
        if (scene->camera_zoom == 2.0f)
            PASS();
        else
            FAIL("");

        lv_visual_scene_destroy(scene);
    }

    /* ========== 组 5：SVG 渲染输出 ========== */
    printf("[组 5] SVG 渲染输出\n");
    {
        lvVisualScene *scene = lv_visual_scene_create();
        lvVisualRenderer *r = lv_visual_renderer_create(lv_RENDER_SVG, 400, 300);

        TEST("svg: 空场景渲染");
        lv_visual_render(r, scene, tmp_svg);
        if (file_contains(tmp_svg, "<svg") && file_contains(tmp_svg, "</svg>"))
            PASS();
        else
            FAIL("");

        TEST("svg: 点元素渲染");
        lvVisualObject *pt = lv_visual_point_create(100, 150);
        lv_visual_set_color(pt, 1.0f, 0.0f, 0.0f, 1.0f);
        lv_visual_scene_add(scene, pt);
        lv_visual_render(r, scene, tmp_svg);
        if (file_contains(tmp_svg, "<circle") && file_contains(tmp_svg, "100.00") && file_contains(tmp_svg, "150.00"))
            PASS();
        else
            FAIL("");

        TEST("svg: 线段元素渲染");
        lvVisualObject *line = lv_visual_line_create(0, 0, 200, 200);
        lv_visual_set_color(line, 0.0f, 0.0f, 1.0f, 1.0f);
        lv_visual_scene_add(scene, line);
        lv_visual_render(r, scene, tmp_svg);
        if (file_contains(tmp_svg, "<line") && file_contains(tmp_svg, "x1="))
            PASS();
        else
            FAIL("");

        TEST("svg: 圆元素渲染");
        lvVisualObject *circ = lv_visual_circle_create(200, 100, 50);
        lv_visual_set_color(circ, 0.0f, 0.5f, 0.0f, 1.0f);
        lv_visual_scene_add(scene, circ);
        lv_visual_render(r, scene, tmp_svg);
        if (file_contains(tmp_svg, "<circle") && file_contains(tmp_svg, "50.00"))
            PASS();
        else
            FAIL("");

        TEST("svg: XML声明存在");
        if (file_starts_with(tmp_svg, "<?xml"))
            PASS();
        else
            FAIL("");

        TEST("svg: xmlns声明存在");
        if (file_contains(tmp_svg, "http://www.w3.org/2000/svg"))
            PASS();
        else
            FAIL("");

        lv_visual_scene_destroy(scene);
        lv_visual_renderer_destroy(r);
    }

    /* ========== 组 6：TikZ 渲染输出 ========== */
    printf("[组 6] TikZ 渲染输出\n");
    {
        lvVisualScene *scene = lv_visual_scene_create();
        lvVisualRenderer *r = lv_visual_renderer_create(lv_RENDER_TIKZ, 400, 300);

        TEST("tikz: 空场景渲染");
        lv_visual_render(r, scene, tmp_tikz);
        if (file_contains(tmp_tikz, "tikzpicture") && file_contains(tmp_tikz, "begin"))
            PASS();
        else
            FAIL("");

        TEST("tikz: 点渲染输出 \\fill");
        lvVisualObject *pt = lv_visual_point_create(50, 100);
        lv_visual_set_color(pt, 1.0f, 0.0f, 0.0f, 1.0f);
        lv_visual_scene_add(scene, pt);
        lv_visual_render(r, scene, tmp_tikz);
        if (file_contains(tmp_tikz, "\\fill") && file_contains(tmp_tikz, "50.00") && file_contains(tmp_tikz, "100.00"))
            PASS();
        else
            FAIL("");

        TEST("tikz: 线段渲染输出 \\draw");
        lvVisualObject *line = lv_visual_line_create(0, 0, 200, 200);
        lv_visual_set_color(line, 0.0f, 0.0f, 1.0f, 1.0f);
        lv_visual_scene_add(scene, line);
        lv_visual_render(r, scene, tmp_tikz);
        if (file_contains(tmp_tikz, "\\draw") && file_contains(tmp_tikz, "--"))
            PASS();
        else
            FAIL("");

        TEST("tikz: 圆渲染输出 circle");
        lvVisualObject *circ = lv_visual_circle_create(100, 100, 30);
        lv_visual_set_color(circ, 0.0f, 0.5f, 0.0f, 1.0f);
        lv_visual_scene_add(scene, circ);
        lv_visual_render(r, scene, tmp_tikz);
        if (file_contains(tmp_tikz, "circle") && file_contains(tmp_tikz, "30.00"))
            PASS();
        else
            FAIL("");

        TEST("tikz: 颜色 rgb 格式存在");
        if (file_contains(tmp_tikz, "rgb,1:red"))
            PASS();
        else
            FAIL("");

        lv_visual_scene_destroy(scene);
        lv_visual_renderer_destroy(r);
    }

    /* ========== 组 7：渲染器创建与资源释放 ========== */
    printf("[组 7] 渲染器\n");
    {
        TEST("renderer_create: SVG后端默认尺寸");
        lvVisualRenderer *r = lv_visual_renderer_create(lv_RENDER_SVG, 0, 0);
        if (r && r->width == 800 && r->height == 600)
            PASS();
        else {
            FAIL("");
            if (r)
                lv_visual_renderer_destroy(r);
        }

        TEST("renderer_create: 自定义尺寸");
        lvVisualRenderer *r2 = lv_visual_renderer_create(lv_RENDER_TIKZ, 1024, 768);
        if (r2 && r2->width == 1024 && r2->height == 768 && r2->backend == lv_RENDER_TIKZ)
            PASS();
        else
            FAIL("");
        lv_visual_renderer_destroy(r2);

        lvVisualScene *scene = lv_visual_scene_create();
        TEST("render: NULL参数安全");
        lv_visual_render(NULL, scene, "x.svg"); /* 不应崩溃 */
        lv_visual_render(r, NULL, "x.svg");
        lv_visual_render(r, scene, NULL);
        PASS();
        lv_visual_scene_destroy(scene);

        lv_visual_renderer_destroy(r);
    }

    /* ========== 组 8：组合对象递归渲染 ========== */
    printf("[组 8] 组合对象递归渲染\n");
    {
        lvVisualObject *c1 = lv_visual_point_create(0, 0);
        lvVisualObject *c2 = lv_visual_point_create(100, 100);
        lvVisualObject *c3 = lv_visual_line_create(0, 0, 100, 100);
        lvVisualObject *children[3] = {c1, c2, c3};
        lvVisualObject *grp = lv_visual_group_create(children, 3);

        lvVisualScene *scene = lv_visual_scene_create();
        lv_visual_scene_add(scene, grp);
        lvVisualRenderer *r = lv_visual_renderer_create(lv_RENDER_SVG, 400, 300);

        TEST("group: 递归渲染输出子对象");
        lv_visual_render(r, scene, tmp_svg);
        /* 组内有2个circle和1个line */
        int circle_count = 0, line_count = 0;
        FILE *f = fopen(tmp_svg, "r");
        if (f) {
            char buf[4096];
            size_t n = fread(buf, 1, sizeof(buf) - 1, f);
            fclose(f);
            buf[n] = '\0';
            char *p = buf;
            while ((p = strstr(p, "<circle")) != NULL) {
                circle_count++;
                p++;
            }
            p = buf;
            while ((p = strstr(p, "<line")) != NULL) {
                line_count++;
                p++;
            }
        }
        if (circle_count >= 2 && line_count >= 1)
            PASS();
        else
            FAIL("");

        lv_visual_scene_destroy(scene);
        lv_visual_renderer_destroy(r);
    }

    /* ========== 组 9：虚线样式 ========== */
    printf("[组 9] 虚线样式\n");
    {
        lvVisualScene *scene = lv_visual_scene_create();
        lvVisualObject *line = lv_visual_line_create(0, 0, 100, 100);
        lv_visual_set_dashed(line, 1);
        lv_visual_scene_add(scene, line);
        lvVisualRenderer *r = lv_visual_renderer_create(lv_RENDER_SVG, 200, 200);

        TEST("svg: 虚线 stroke-dasharray");
        lv_visual_render(r, scene, tmp_svg);
        if (file_contains(tmp_svg, "stroke-dasharray"))
            PASS();
        else
            FAIL("");

        lvVisualRenderer *r2 = lv_visual_renderer_create(lv_RENDER_TIKZ, 200, 200);
        TEST("tikz: 虚线 dashed 选项");
        lv_visual_render(r2, scene, tmp_tikz);
        if (file_contains(tmp_tikz, "dashed"))
            PASS();
        else
            FAIL("");

        lv_visual_scene_destroy(scene);
        lv_visual_renderer_destroy(r);
        lv_visual_renderer_destroy(r2);
    }

    /* ========== 组 10：Cairo 脚本渲染输出 ========== */
    printf("[组 10] Cairo 脚本渲染输出\n");
    {
        lvVisualScene *scene = lv_visual_scene_create();
        lvVisualRenderer *r = lv_visual_renderer_create(lv_RENDER_CAIRO, 400, 300);

        TEST("cairo: 空场景渲染 (main function)");
        lv_visual_render(r, scene, tmp_cairo);
        if (file_starts_with(tmp_cairo, "/* Generated") && file_contains(tmp_cairo, "int main(void)"))
            PASS();
        else
            FAIL("");

        lvVisualObject *pt = lv_visual_point_create(100, 150);
        lv_visual_set_color(pt, 1.0f, 0.0f, 0.0f, 1.0f);
        lv_visual_scene_add(scene, pt);

        TEST("cairo: cairo头文件引用");
        if (file_contains(tmp_cairo, "#include <cairo.h>"))
            PASS();
        else
            FAIL("");

        TEST("cairo: 点渲染为 cairo_arc");
        lv_visual_render(r, scene, tmp_cairo);
        if (file_contains(tmp_cairo, "cairo_arc") && file_contains(tmp_cairo, "cairo_fill"))
            PASS();
        else
            FAIL("");

        lvVisualObject *line = lv_visual_line_create(0, 0, 200, 200);
        lv_visual_scene_add(scene, line);
        lvVisualObject *circ = lv_visual_circle_create(200, 100, 50);
        lv_visual_scene_add(scene, circ);

        TEST("cairo: 线段渲染为 move_to / line_to / stroke");
        lv_visual_render(r, scene, tmp_cairo);
        if (file_contains(tmp_cairo, "cairo_move_to") && file_contains(tmp_cairo, "cairo_line_to") &&
            file_contains(tmp_cairo, "cairo_stroke"))
            PASS();
        else
            FAIL("");

        TEST("cairo: 圆渲染含 cairo_arc + stroke_preserve");
        if (file_contains(tmp_cairo, "cairo_stroke_preserve"))
            PASS();
        else
            FAIL("");

        TEST("cairo: 输出为 cairo_surface_write_to_png");
        if (file_contains(tmp_cairo, "cairo_surface_write_to_png"))
            PASS();
        else
            FAIL("");

        lv_visual_scene_destroy(scene);
        lv_visual_renderer_destroy(r);
    }

    /* ========== 组 11：Three.js HTML 渲染输出 ========== */
    printf("[组 11] Three.js 渲染输出\n");
    {
        lvVisualScene *scene = lv_visual_scene_create();
        lvVisualRenderer *r = lv_visual_renderer_create(lv_RENDER_THREEJS, 400, 300);

        TEST("threejs: 空场景渲染 (HTML DOCTYPE)");
        lv_visual_render(r, scene, tmp_threejs);
        if (file_contains(tmp_threejs, "<!DOCTYPE html>") && file_contains(tmp_threejs, "</html>"))
            PASS();
        else
            FAIL("");

        TEST("threejs: Three.js CDN 引用");
        if (file_contains(tmp_threejs, "three.module.js") && file_contains(tmp_threejs, "OrbitControls"))
            PASS();
        else
            FAIL("");

        TEST("threejs: WebGLRenderer 创建");
        if (file_contains(tmp_threejs, "WebGLRenderer"))
            PASS();
        else
            FAIL("");

        lvVisualObject *pt = lv_visual_point_create(100, 150);
        lv_visual_set_color(pt, 0.0f, 1.0f, 0.0f, 1.0f);
        lv_visual_scene_add(scene, pt);

        TEST("threejs: 点渲染为 SphereGeometry");
        lv_visual_render(r, scene, tmp_threejs);
        if (file_contains(tmp_threejs, "SphereGeometry") && file_contains(tmp_threejs, "THREE.Mesh"))
            PASS();
        else
            FAIL("");

        lvVisualObject *line = lv_visual_line_create(0, 0, 200, 200);
        lv_visual_set_color(line, 0.0f, 0.0f, 1.0f, 1.0f);
        lv_visual_scene_add(scene, line);
        lvVisualObject *circ = lv_visual_circle_create(200, 100, 50);
        lv_visual_scene_add(scene, circ);

        TEST("threejs: 线段渲染为 PlaneGeometry");
        lv_visual_render(r, scene, tmp_threejs);
        if (file_contains(tmp_threejs, "PlaneGeometry"))
            PASS();
        else
            FAIL("");

        TEST("threejs: 圆形渲染为 RingGeometry");
        if (file_contains(tmp_threejs, "RingGeometry"))
            PASS();
        else
            FAIL("");

        TEST("threejs: 场景含 OrbitControls");
        if (file_contains(tmp_threejs, "controls.update"))
            PASS();
        else
            FAIL("");

        lv_visual_scene_destroy(scene);
        lv_visual_renderer_destroy(r);
    }

    /* 清理临时文件 */
    remove(tmp_svg);
    remove(tmp_tikz);
    remove(tmp_cairo);
    remove(tmp_threejs);

    printf("\n=== 结果: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
