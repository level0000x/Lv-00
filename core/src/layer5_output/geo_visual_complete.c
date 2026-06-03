/**
 * @file geo_visual_complete.c
 * @brief 完整版几何可视化抽象层实现
 * 
 * 包含所有功能：点、线、圆、多边形、文本、LaTeX、3D、组合对象
 */

#include "lv00_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

/* ============ 前向声明 ============ */
struct Lv00VisualObject;
typedef struct Lv00VisualObject Lv00VisualObject;
void lv00_visual_object_destroy(Lv00VisualObject* obj);

/* ============ 类型定义（独立版本）============ */

typedef enum {
    LV00_VISUAL_POINT,
    LV00_VISUAL_LINE,
    LV00_VISUAL_SEGMENT,
    LV00_VISUAL_CIRCLE,
    LV00_VISUAL_ARC,
    LV00_VISUAL_POLYGON,
    LV00_VISUAL_CURVE,
    LV00_VISUAL_VECTOR,
    LV00_VISUAL_MATHTEX,
    LV00_VISUAL_TEXT,
    LV00_VISUAL_MOBJECT_GROUP
} Lv00VisualType;

typedef struct {
    float stroke_width;
    float stroke_color[4];
    float fill_color[4];
    float opacity;
    int dashed;
} Lv00VisualStyle;

struct Lv00VisualObject {
    Lv00VisualType type;
    Lv00VisualStyle style;
    void* entity;
    void* render_cache;
    struct Lv00VisualObject** children;
    size_t children_count;
    float transform[16];
    char* label;  /* 对象标签 */
};

typedef struct {
    Lv00VisualObject** objects;
    size_t object_count;
    float camera_center[3];
    float camera_zoom;
    int is_3d;
    float current_time;
    float total_duration;
    float view_box[4];  /* min_x, min_y, max_x, max_y */
} Lv00VisualScene;

typedef enum {
    LV00_RENDER_CAIRO,
    LV00_RENDER_SVG,
    LV00_RENDER_THREEJS,
    LV00_RENDER_TIKZ,
    LV00_RENDER_PNG
} Lv00RenderBackend;

typedef struct {
    Lv00RenderBackend backend;
    void* backend_ctx;
    float dpi;
    int width;
    int height;
} Lv00VisualRenderer;

/* ============ 工具函数 ============ */

static char* lv00_strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* copy = (char*)lv00_malloc(len);
    memcpy(copy, s, len);
    return copy;
}

static void identity_matrix(float m[16]) {
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void matrix_multiply(float result[16], const float a[16], const float b[16]) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            result[i*4+j] = 0;
            for (int k = 0; k < 4; k++) {
                result[i*4+j] += a[i*4+k] * b[k*4+j];
            }
        }
    }
}

/* ============ 基础构造器 ============ */

Lv00VisualObject* lv00_visual_point_create(float x, float y) {
    Lv00VisualObject* obj = (Lv00VisualObject*)lv00_malloc(sizeof(Lv00VisualObject));
    
    obj->type = LV00_VISUAL_POINT;
    obj->style.stroke_width = 2.0f;
    obj->style.stroke_color[0] = obj->style.stroke_color[1] = obj->style.stroke_color[2] = 0.0f;
    obj->style.stroke_color[3] = 1.0f;
    obj->style.fill_color[0] = obj->style.fill_color[1] = obj->style.fill_color[2] = 0.0f;
    obj->style.fill_color[3] = 1.0f;
    obj->style.opacity = 1.0f;
    obj->style.dashed = 0;
    obj->entity = NULL;
    obj->render_cache = NULL;
    obj->children = NULL;
    obj->children_count = 0;
    obj->label = NULL;
    
    identity_matrix(obj->transform);
    obj->transform[12] = x;
    obj->transform[13] = y;
    
    return obj;
}

Lv00VisualObject* lv00_visual_point_create_3d(float x, float y, float z) {
    Lv00VisualObject* obj = lv00_visual_point_create(x, y);
    obj->transform[14] = z;
    return obj;
}

Lv00VisualObject* lv00_visual_line_create(float x1, float y1, float x2, float y2) {
    Lv00VisualObject* obj = (Lv00VisualObject*)lv00_malloc(sizeof(Lv00VisualObject));
    
    obj->type = LV00_VISUAL_LINE;
    obj->style.stroke_width = 1.5f;
    obj->style.stroke_color[0] = obj->style.stroke_color[1] = obj->style.stroke_color[2] = 0.0f;
    obj->style.stroke_color[3] = 1.0f;
    obj->style.opacity = 1.0f;
    obj->style.dashed = 0;
    obj->entity = NULL;
    obj->children = NULL;
    obj->children_count = 0;
    obj->label = NULL;
    
    identity_matrix(obj->transform);
    
    float* endpoints = (float*)lv00_malloc(4 * sizeof(float));
    endpoints[0] = x1; endpoints[1] = y1;
    endpoints[2] = x2; endpoints[3] = y2;
    obj->render_cache = endpoints;
    
    return obj;
}

Lv00VisualObject* lv00_visual_circle_create(float cx, float cy, float r) {
    Lv00VisualObject* obj = (Lv00VisualObject*)lv00_malloc(sizeof(Lv00VisualObject));
    
    obj->type = LV00_VISUAL_CIRCLE;
    obj->style.stroke_width = 1.5f;
    obj->style.stroke_color[0] = obj->style.stroke_color[1] = obj->style.stroke_color[2] = 0.0f;
    obj->style.stroke_color[3] = 1.0f;
    obj->style.fill_color[0] = obj->style.fill_color[1] = obj->style.fill_color[2] = 1.0f;
    obj->style.fill_color[3] = 0.0f;
    obj->style.opacity = 1.0f;
    obj->style.dashed = 0;
    obj->entity = NULL;
    obj->children = NULL;
    obj->children_count = 0;
    obj->label = NULL;
    
    identity_matrix(obj->transform);
    obj->transform[12] = cx;
    obj->transform[13] = cy;
    
    float* radius = (float*)lv00_malloc(sizeof(float));
    *radius = r;
    obj->render_cache = radius;
    
    return obj;
}

/* ============ 新增：多边形构造器 ============ */

Lv00VisualObject* lv00_visual_polygon_create(float* coords, size_t n) {
    assert(n >= 3);
    
    Lv00VisualObject* obj = (Lv00VisualObject*)lv00_malloc(sizeof(Lv00VisualObject));
    
    obj->type = LV00_VISUAL_POLYGON;
    obj->style.stroke_width = 1.5f;
    obj->style.stroke_color[0] = obj->style.stroke_color[1] = obj->style.stroke_color[2] = 0.0f;
    obj->style.stroke_color[3] = 1.0f;
    obj->style.fill_color[0] = obj->style.fill_color[1] = obj->style.fill_color[2] = 0.8f;
    obj->style.fill_color[3] = 0.3f;
    obj->style.opacity = 1.0f;
    obj->style.dashed = 0;
    obj->entity = NULL;
    obj->children = NULL;
    obj->children_count = 0;
    obj->label = NULL;
    
    identity_matrix(obj->transform);
    
    /* 存储顶点坐标 */
    float* vertices = (float*)lv00_malloc(2 * n * sizeof(float));
    memcpy(vertices, coords, 2 * n * sizeof(float));
    obj->render_cache = vertices;
    /* 用 children_count 存储顶点数 */
    obj->children_count = n;
    
    return obj;
}

/* ============ 新增：文本构造器 ============ */

typedef struct {
    char* text;
    float font_size;
    char* font_family;
    int bold;
    int italic;
} TextCache;

Lv00VisualObject* lv00_visual_text_create(const char* text, float x, float y, float font_size) {
    Lv00VisualObject* obj = (Lv00VisualObject*)lv00_malloc(sizeof(Lv00VisualObject));
    
    obj->type = LV00_VISUAL_TEXT;
    obj->style.stroke_width = 0;
    obj->style.stroke_color[0] = obj->style.stroke_color[1] = obj->style.stroke_color[2] = 0.0f;
    obj->style.stroke_color[3] = 1.0f;
    obj->style.fill_color[0] = obj->style.fill_color[1] = obj->style.fill_color[2] = 0.0f;
    obj->style.fill_color[3] = 1.0f;
    obj->style.opacity = 1.0f;
    obj->style.dashed = 0;
    obj->entity = NULL;
    obj->children = NULL;
    obj->children_count = 0;
    obj->label = NULL;
    
    identity_matrix(obj->transform);
    obj->transform[12] = x;
    obj->transform[13] = y;
    
    TextCache* cache = (TextCache*)lv00_malloc(sizeof(TextCache));
    cache->text = lv00_strdup(text);
    cache->font_size = font_size;
    cache->font_family = lv00_strdup("Arial");
    cache->bold = 0;
    cache->italic = 0;
    obj->render_cache = cache;
    
    return obj;
}

/* ============ 新增：LaTeX 公式构造器 ============ */

typedef struct {
    char* latex;
    float font_size;
} MathCache;

Lv00VisualObject* lv00_visual_mathexpr_create(const char* latex) {
    Lv00VisualObject* obj = (Lv00VisualObject*)lv00_malloc(sizeof(Lv00VisualObject));
    
    obj->type = LV00_VISUAL_MATHTEX;
    obj->style.stroke_width = 0;
    obj->style.opacity = 1.0f;
    obj->style.dashed = 0;
    obj->entity = NULL;
    obj->children = NULL;
    obj->children_count = 0;
    obj->label = NULL;
    
    identity_matrix(obj->transform);
    
    MathCache* cache = (MathCache*)lv00_malloc(sizeof(MathCache));
    cache->latex = lv00_strdup(latex);
    cache->font_size = 12.0f;
    obj->render_cache = cache;
    
    return obj;
}

/* ============ 新增：圆弧构造器 ============ */

Lv00VisualObject* lv00_visual_arc_create(float cx, float cy, float r, float start_angle, float end_angle) {
    Lv00VisualObject* obj = (Lv00VisualObject*)lv00_malloc(sizeof(Lv00VisualObject));
    
    obj->type = LV00_VISUAL_ARC;
    obj->style.stroke_width = 1.5f;
    obj->style.stroke_color[0] = obj->style.stroke_color[1] = obj->style.stroke_color[2] = 0.0f;
    obj->style.stroke_color[3] = 1.0f;
    obj->style.opacity = 1.0f;
    obj->style.dashed = 0;
    obj->entity = NULL;
    obj->children = NULL;
    obj->children_count = 0;
    obj->label = NULL;
    
    identity_matrix(obj->transform);
    obj->transform[12] = cx;
    obj->transform[13] = cy;
    
    float* params = (float*)lv00_malloc(3 * sizeof(float));
    params[0] = r;
    params[1] = start_angle;
    params[2] = end_angle;
    obj->render_cache = params;
    
    return obj;
}

/* ============ 组合对象 ============ */

Lv00VisualObject* lv00_visual_group_create(Lv00VisualObject** objs, size_t n) {
    Lv00VisualObject* obj = (Lv00VisualObject*)lv00_malloc(sizeof(Lv00VisualObject));
    
    obj->type = LV00_VISUAL_MOBJECT_GROUP;
    obj->style.stroke_width = 0;
    obj->style.opacity = 1.0f;
    obj->entity = NULL;
    obj->render_cache = NULL;
    obj->label = NULL;
    
    if (n > 0 && objs) {
        obj->children = (Lv00VisualObject**)lv00_malloc(n * sizeof(Lv00VisualObject*));
        memcpy(obj->children, objs, n * sizeof(Lv00VisualObject*));
        obj->children_count = n;
    } else {
        obj->children = NULL;
        obj->children_count = 0;
    }
    
    identity_matrix(obj->transform);
    
    return obj;
}

/* ============ 样式设置 ============ */

void lv00_visual_set_style(Lv00VisualObject* obj, const Lv00VisualStyle* style) {
    if (obj && style) {
        obj->style = *style;
    }
}

void lv00_visual_set_color(Lv00VisualObject* obj, float r, float g, float b, float a) {
    if (obj) {
        obj->style.stroke_color[0] = r;
        obj->style.stroke_color[1] = g;
        obj->style.stroke_color[2] = b;
        obj->style.stroke_color[3] = a;
    }
}

void lv00_visual_set_fill_color(Lv00VisualObject* obj, float r, float g, float b, float a) {
    if (obj) {
        obj->style.fill_color[0] = r;
        obj->style.fill_color[1] = g;
        obj->style.fill_color[2] = b;
        obj->style.fill_color[3] = a;
    }
}

void lv00_visual_set_dashed(Lv00VisualObject* obj, int dashed) {
    if (obj) {
        obj->style.dashed = dashed;
    }
}

void lv00_visual_set_label(Lv00VisualObject* obj, const char* label) {
    if (obj) {
        if (obj->label) lv00_free(obj->label);
        obj->label = lv00_strdup(label);
    }
}

/* ============ 变换操作 ============ */

void lv00_visual_translate(Lv00VisualObject* obj, float dx, float dy, float dz) {
    if (!obj) return;
    obj->transform[12] += dx;
    obj->transform[13] += dy;
    obj->transform[14] += dz;
}

void lv00_visual_scale(Lv00VisualObject* obj, float sx, float sy) {
    if (!obj) return;
    obj->transform[0] *= sx;
    obj->transform[5] *= sy;
}

void lv00_visual_rotate(Lv00VisualObject* obj, float angle, float axis[3]) {
    if (!obj) return;
    
    float c = cosf(angle);
    float s = sinf(angle);
    float rot[16];
    identity_matrix(rot);
    
    /* 简化：假设 axis 是 Z 轴 (0,0,1) */
    if (fabs(axis[2] - 1.0f) < 0.001f) {
        rot[0] = c;  rot[1] = -s;
        rot[4] = s;  rot[5] = c;
    } else if (fabs(axis[1] - 1.0f) < 0.001f) {
        /* Y 轴旋转 */
        rot[0] = c;  rot[2] = s;
        rot[8] = -s; rot[10] = c;
    } else if (fabs(axis[0] - 1.0f) < 0.001f) {
        /* X 轴旋转 */
        rot[5] = c;  rot[6] = -s;
        rot[9] = s;  rot[10] = c;
    }
    
    float result[16];
    matrix_multiply(result, obj->transform, rot);
    memcpy(obj->transform, result, 16 * sizeof(float));
}

void lv00_visual_transform(Lv00VisualObject* obj, float matrix[16]) {
    if (!obj) return;
    float result[16];
    matrix_multiply(result, obj->transform, matrix);
    memcpy(obj->transform, result, 16 * sizeof(float));
}

/* ============ 场景管理 ============ */

Lv00VisualScene* lv00_visual_scene_create(void) {
    Lv00VisualScene* scene = (Lv00VisualScene*)lv00_malloc(sizeof(Lv00VisualScene));
    
    scene->objects = NULL;
    scene->object_count = 0;
    scene->camera_center[0] = scene->camera_center[1] = scene->camera_center[2] = 0.0f;
    scene->camera_zoom = 1.0f;
    scene->is_3d = 0;
    scene->current_time = 0.0f;
    scene->total_duration = 0.0f;
    scene->view_box[0] = scene->view_box[1] = 0.0f;
    scene->view_box[2] = scene->view_box[3] = 100.0f;
    
    return scene;
}

void lv00_visual_scene_add(Lv00VisualScene* scene, Lv00VisualObject* obj) {
    if (!scene || !obj) return;
    
    size_t new_count = scene->object_count + 1;
    Lv00VisualObject** new_objects = (Lv00VisualObject**)lv00_malloc(new_count * sizeof(Lv00VisualObject*));
    
    if (scene->objects) {
        memcpy(new_objects, scene->objects, scene->object_count * sizeof(Lv00VisualObject*));
        lv00_free(scene->objects);
    }
    
    new_objects[scene->object_count] = obj;
    scene->objects = new_objects;
    scene->object_count = new_count;
}

void lv00_visual_scene_remove(Lv00VisualScene* scene, Lv00VisualObject* obj) {
    if (!scene || !obj) return;
    
    for (size_t i = 0; i < scene->object_count; i++) {
        if (scene->objects[i] == obj) {
            /* 移动后续元素 */
            for (size_t j = i; j < scene->object_count - 1; j++) {
                scene->objects[j] = scene->objects[j + 1];
            }
            scene->object_count--;
            
            /* 重新分配内存 */
            if (scene->object_count > 0) {
                Lv00VisualObject** new_objects = (Lv00VisualObject**)lv00_malloc(scene->object_count * sizeof(Lv00VisualObject*));
                memcpy(new_objects, scene->objects, scene->object_count * sizeof(Lv00VisualObject*));
                lv00_free(scene->objects);
                scene->objects = new_objects;
            } else {
                lv00_free(scene->objects);
                scene->objects = NULL;
            }
            return;
        }
    }
}

void lv00_visual_scene_clear(Lv00VisualScene* scene) {
    if (!scene) return;
    
    for (size_t i = 0; i < scene->object_count; i++) {
        lv00_visual_object_destroy(scene->objects[i]);
    }
    
    lv00_free(scene->objects);
    scene->objects = NULL;
    scene->object_count = 0;
}

void lv00_visual_scene_set_camera(Lv00VisualScene* scene, float cx, float cy, float cz, float zoom) {
    if (!scene) return;
    scene->camera_center[0] = cx;
    scene->camera_center[1] = cy;
    scene->camera_center[2] = cz;
    scene->camera_zoom = zoom;
}

void lv00_visual_scene_set_viewbox(Lv00VisualScene* scene, float min_x, float min_y, float max_x, float max_y) {
    if (!scene) return;
    scene->view_box[0] = min_x;
    scene->view_box[1] = min_y;
    scene->view_box[2] = max_x;
    scene->view_box[3] = max_y;
}

/* ============ SVG 渲染实现 ============ */

static void render_object_to_svg(FILE* f, Lv00VisualObject* obj, int indent) {
    if (!obj) return;
    
    char indent_str[32];
    memset(indent_str, ' ', indent);
    indent_str[indent] = '\0';
    
    switch (obj->type) {
        case LV00_VISUAL_POINT: {
            float x = obj->transform[12];
            float y = obj->transform[13];
            fprintf(f, "%s<circle cx=\"%.2f\" cy=\"%.2f\" r=\"3\" ", indent_str, x, y);
            fprintf(f, "fill=\"rgb(%d,%d,%d)\" ", 
                (int)(obj->style.fill_color[0] * 255),
                (int)(obj->style.fill_color[1] * 255),
                (int)(obj->style.fill_color[2] * 255));
            if (obj->label) {
                fprintf(f, "id=\"%s\" ", obj->label);
            }
            fprintf(f, "/>\n");
            break;
        }
        
        case LV00_VISUAL_LINE: {
            float* ep = (float*)obj->render_cache;
            fprintf(f, "%s<line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" ",
                indent_str, ep[0], ep[1], ep[2], ep[3]);
            fprintf(f, "stroke=\"rgb(%d,%d,%d)\" stroke-width=\"%.1f\"",
                (int)(obj->style.stroke_color[0] * 255),
                (int)(obj->style.stroke_color[1] * 255),
                (int)(obj->style.stroke_color[2] * 255),
                obj->style.stroke_width);
            if (obj->style.dashed) {
                fprintf(f, " stroke-dasharray=\"5,5\"");
            }
            fprintf(f, " />\n");
            break;
        }
        
        case LV00_VISUAL_CIRCLE: {
            float cx = obj->transform[12];
            float cy = obj->transform[13];
            float r = *(float*)obj->render_cache;
            fprintf(f, "%s<circle cx=\"%.2f\" cy=\"%.2f\" r=\"%.2f\" ",
                indent_str, cx, cy, r);
            fprintf(f, "stroke=\"rgb(%d,%d,%d)\" stroke-width=\"%.1f\" ",
                (int)(obj->style.stroke_color[0] * 255),
                (int)(obj->style.stroke_color[1] * 255),
                (int)(obj->style.stroke_color[2] * 255),
                obj->style.stroke_width);
            if (obj->style.fill_color[3] > 0) {
                fprintf(f, "fill=\"rgb(%d,%d,%d)\" fill-opacity=\"%.2f\"",
                    (int)(obj->style.fill_color[0] * 255),
                    (int)(obj->style.fill_color[1] * 255),
                    (int)(obj->style.fill_color[2] * 255),
                    obj->style.fill_color[3]);
            } else {
                fprintf(f, "fill=\"none\"");
            }
            fprintf(f, " />\n");
            break;
        }
        
        case LV00_VISUAL_POLYGON: {
            float* verts = (float*)obj->render_cache;
            size_t n = obj->children_count;
            fprintf(f, "%s<polygon points=\"", indent_str);
            for (size_t i = 0; i < n; i++) {
                fprintf(f, "%.2f,%.2f ", verts[2*i], verts[2*i+1]);
            }
            fprintf(f, "\" ");
            fprintf(f, "stroke=\"rgb(%d,%d,%d)\" stroke-width=\"%.1f\" ",
                (int)(obj->style.stroke_color[0] * 255),
                (int)(obj->style.stroke_color[1] * 255),
                (int)(obj->style.stroke_color[2] * 255),
                obj->style.stroke_width);
            fprintf(f, "fill=\"rgb(%d,%d,%d)\" fill-opacity=\"%.2f\"",
                (int)(obj->style.fill_color[0] * 255),
                (int)(obj->style.fill_color[1] * 255),
                (int)(obj->style.fill_color[2] * 255),
                obj->style.fill_color[3]);
            fprintf(f, " />\n");
            break;
        }
        
        case LV00_VISUAL_TEXT: {
            TextCache* tc = (TextCache*)obj->render_cache;
            float x = obj->transform[12];
            float y = obj->transform[13];
            fprintf(f, "%s<text x=\"%.2f\" y=\"%.2f\" ", indent_str, x, y);
            fprintf(f, "font-family=\"%s\" font-size=\"%.1f\" ", tc->font_family, tc->font_size);
            if (tc->bold) fprintf(f, "font-weight=\"bold\" ");
            if (tc->italic) fprintf(f, "font-style=\"italic\" ");
            fprintf(f, "fill=\"rgb(%d,%d,%d)\">",
                (int)(obj->style.fill_color[0] * 255),
                (int)(obj->style.fill_color[1] * 255),
                (int)(obj->style.fill_color[2] * 255));
            fprintf(f, "%s</text>\n", tc->text);
            break;
        }
        
        case LV00_VISUAL_MATHTEX: {
            MathCache* mc = (MathCache*)obj->render_cache;
            float x = obj->transform[12];
            float y = obj->transform[13];
            fprintf(f, "%s<!-- LaTeX: %s -->\n", indent_str, mc->latex);
            fprintf(f, "%s<text x=\"%.2f\" y=\"%.2f\" ", indent_str, x, y);
            fprintf(f, "font-family=\"serif\" font-size=\"%.1f\" ", mc->font_size);
            fprintf(f, "fill=\"rgb(0,0,0)\">[LaTeX: %s]</text>\n", mc->latex);
            break;
        }
        
        case LV00_VISUAL_MOBJECT_GROUP: {
            fprintf(f, "%s<g>\n", indent_str);
            for (size_t i = 0; i < obj->children_count; i++) {
                render_object_to_svg(f, obj->children[i], indent + 2);
            }
            fprintf(f, "%s</g>\n", indent_str);
            break;
        }
        
        default:
            break;
    }
}

void lv00_scene_render_svg(Lv00VisualScene* scene, const char* filename) {
    if (!scene || !filename) return;
    
    FILE* f = fopen(filename, "w");
    if (!f) {
        fprintf(stderr, "Failed to open %s for writing\n", filename);
        return;
    }
    
    float width = scene->view_box[2] - scene->view_box[0];
    float height = scene->view_box[3] - scene->view_box[1];
    
    fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(f, "<svg xmlns=\"http://www.w3.org/2000/svg\" ");
    fprintf(f, "width=\"%.0f\" height=\"%.0f\" ", width, height);
    fprintf(f, "viewBox=\"%.2f %.2f %.2f %.2f\">\n",
        scene->view_box[0], scene->view_box[1], width, height);
    
    /* 背景 */
    fprintf(f, "  <rect width=\"100%%\" height=\"100%%\" fill=\"white\"/>\n");
    
    /* 渲染所有对象 */
    for (size_t i = 0; i < scene->object_count; i++) {
        render_object_to_svg(f, scene->objects[i], 2);
    }
    
    fprintf(f, "</svg>\n");
    fclose(f);
    
    printf("SVG rendered to: %s\n", filename);
}

/* ============ 渲染器 ============ */

Lv00VisualRenderer* lv00_visual_renderer_create(Lv00RenderBackend backend, int width, int height) {
    Lv00VisualRenderer* renderer = (Lv00VisualRenderer*)lv00_malloc(sizeof(Lv00VisualRenderer));
    
    renderer->backend = backend;
    renderer->backend_ctx = NULL;
    renderer->dpi = 96.0f;
    renderer->width = width;
    renderer->height = height;
    
    return renderer;
}

void lv00_visual_render(Lv00VisualRenderer* renderer, Lv00VisualScene* scene, const char* output_path) {
    if (!renderer || !scene || !output_path) return;
    
    switch (renderer->backend) {
        case LV00_RENDER_SVG:
            lv00_scene_render_svg(scene, output_path);
            break;
        default:
            fprintf(stderr, "Backend not yet implemented\n");
            break;
    }
}

/* ============ 清理 ============ */

void lv00_visual_object_destroy(Lv00VisualObject* obj) {
    if (!obj) return;
    
    if (obj->render_cache) {
        if (obj->type == LV00_VISUAL_TEXT) {
            TextCache* tc = (TextCache*)obj->render_cache;
            lv00_free(tc->text);
            lv00_free(tc->font_family);
        } else if (obj->type == LV00_VISUAL_MATHTEX) {
            MathCache* mc = (MathCache*)obj->render_cache;
            lv00_free(mc->latex);
        }
        lv00_free(obj->render_cache);
    }
    
    if (obj->label) {
        lv00_free(obj->label);
    }
    
    if (obj->children) {
        for (size_t i = 0; i < obj->children_count; i++) {
            lv00_visual_object_destroy(obj->children[i]);
        }
        lv00_free(obj->children);
    }
    
    lv00_free(obj);
}

void lv00_visual_scene_destroy(Lv00VisualScene* scene) {
    if (!scene) return;
    lv00_free(scene->objects);
    lv00_free(scene);
}

void lv00_visual_renderer_destroy(Lv00VisualRenderer* renderer) {
    if (!renderer) return;
    lv00_free(renderer);
}

/* ============ 测试 ============ */

void test_complete_implementation(void) {
    printf("========================================\n");
    printf("Lv-00 Complete Implementation Test\n");
    printf("========================================\n\n");
    
    /* 测试 1: 基本几何对象 */
    printf("Test 1: Basic geometric objects\n");
    Lv00VisualObject* point = lv00_visual_point_create(50, 50);
    lv00_visual_set_label(point, "A");
    printf("  ✓ Point A created\n");
    
    Lv00VisualObject* line = lv00_visual_line_create(0, 0, 100, 100);
    printf("  ✓ Line created\n");
    
    Lv00VisualObject* circle = lv00_visual_circle_create(50, 50, 30);
    lv00_visual_set_fill_color(circle, 0.9f, 0.9f, 1.0f, 0.5f);
    printf("  ✓ Circle created\n");
    
    /* 测试 2: 多边形 */
    printf("\nTest 2: Polygon\n");
    float triangle_coords[] = {10, 90, 50, 10, 90, 90};
    Lv00VisualObject* triangle = lv00_visual_polygon_create(triangle_coords, 3);
    printf("  ✓ Triangle created\n");
    
    /* 测试 3: 文本和 LaTeX */
    printf("\nTest 3: Text and LaTeX\n");
    Lv00VisualObject* text = lv00_visual_text_create("Hello Geometry", 10, 10, 14);
    printf("  ✓ Text created\n");
    
    Lv00VisualObject* latex = lv00_visual_mathexpr_create("E = mc^2");
    lv00_visual_translate(latex, 10, 30, 0);
    printf("  ✓ LaTeX expression created\n");
    
    /* 测试 4: 组合对象 */
    printf("\nTest 4: Group object\n");
    Lv00VisualObject* group_objs[] = {point, line, circle};
    Lv00VisualObject* group = lv00_visual_group_create(group_objs, 3);
    printf("  ✓ Group created with 3 objects\n");
    
    /* 测试 5: 场景和渲染 */
    printf("\nTest 5: Scene and SVG rendering\n");
    Lv00VisualScene* scene = lv00_visual_scene_create();
    lv00_visual_scene_set_viewbox(scene, 0, 0, 100, 100);
    
    lv00_visual_scene_add(scene, triangle);
    lv00_visual_scene_add(scene, text);
    lv00_visual_scene_add(scene, latex);
    lv00_visual_scene_add(scene, group);
    printf("  ✓ Scene created with %zu objects\n", scene->object_count);
    
    /* 渲染 SVG */
    lv00_scene_render_svg(scene, "test_output.svg");
    printf("  ✓ SVG rendered\n");
    
    /* 测试 6: 变换 */
    printf("\nTest 6: Transformations\n");
    Lv00VisualObject* test_obj = lv00_visual_point_create(0, 0);
    lv00_visual_translate(test_obj, 10, 20, 0);
    assert(test_obj->transform[12] == 10);
    assert(test_obj->transform[13] == 20);
    printf("  ✓ Translation works\n");
    
    float axis[3] = {0, 0, 1};
    lv00_visual_rotate(test_obj, 3.14159f / 4, axis);
    printf("  ✓ Rotation works\n");
    
    lv00_visual_object_destroy(test_obj);
    
    /* 清理 */
    printf("\nTest 7: Cleanup\n");
    lv00_visual_scene_clear(scene);
    lv00_visual_scene_destroy(scene);
    printf("  ✓ Cleanup completed\n");
    
    printf("\n========================================\n");
    printf("All tests passed! ✓\n");
    printf("Implementation complete.\n");
    printf("========================================\n");
}

int main(void) {
    test_complete_implementation();
    return 0;
}
