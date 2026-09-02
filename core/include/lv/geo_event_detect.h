#ifndef lv_GEO_EVENT_DETECT_H
#define lv_GEO_EVENT_DETECT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */
#include "lv/lv_platform.h"
#include "lv/lv_utils.h" /* GEO_EVENT_DEFAULT_TOL 语义别名 = lv_EPSILON_DOUBLE */
#include <stdbool.h>
#include <stdint.h>

/* ── Constants ── */
#define GEO_EVENT_MAX_EVENTS 128
#define GEO_EVENT_DEFAULT_TOL lv_EPSILON_DOUBLE /* 事件检测默认容差（语义别名 = lv_EPSILON_DOUBLE，1e-12） */
#define GEO_EVENT_MAX_ROOT_ITERS 100

/* ── Root-finding method ── */
typedef enum {
    lv_ROOTFIND_BISECTION = 0,
    lv_ROOTFIND_BRENT,
    lv_ROOTFIND_ILLINOIS,
    lv_ROOTFIND_NEWTON,
    lv_ROOTFIND_SECANT
} lvRootMethod;

/* ── Event type ── */
typedef enum {
    lv_EVENT_INTERSECTION = 0,
    lv_EVENT_COLLISION,
    lv_EVENT_BOUNDARY,
    lv_EVENT_COPLANAR,
    lv_EVENT_DEGENERATE,
    lv_EVENT_CONTACT,
    lv_EVENT_CROSSING,
    lv_EVENT_THRESHOLD,
    lv_EVENT_PERIODIC,
    lv_EVENT_CUSTOM
} lvEventType;

/* ── Event result ── */
typedef enum {
    lv_EVENT_RESULT_ERROR = -1,
    lv_EVENT_RESULT_NONE = 0,
    lv_EVENT_RESULT_DETECTED = 1,
    lv_EVENT_RESULT_WARNING = 2
} lvEventResult;

/* ── Forward decl ── */
typedef struct lvEventDetector lvEventDetector;

/* ── Event function signature ── */
typedef int (*lvEventFunc)(double t, const double *param, int dim, double *g, lvEventDetector *detector);

/* ── Event callback ── */
typedef void (*lvEventCallback)(lvEventDetector *detector, int event_id, double t, const double *param);

/* ── Event entry ── */
typedef struct {
    int event_id;
    lvEventType type;
    lvEventFunc func;
    bool enabled;
    double t_prev;
    double t_span;
    int direction;
    bool terminal;
    lvEventCallback callback;
} lvEventEntry;

/* ── Event detector ── */
struct lvEventDetector {
    lvEventEntry events[GEO_EVENT_MAX_EVENTS];
    int num_events;
    lvRootMethod root_method;
    double root_tol;
    int max_root_iters;
    double t_prev;
    double g_prev[GEO_EVENT_MAX_EVENTS];
    void *user_data;
};

/* ── API ── */
lvEventDetector *geo_event_detector_create(void);
lv_PUBLIC_API void geo_event_detector_destroy(lvEventDetector *detector);
int geo_event_register(lvEventDetector *detector, int event_id, lvEventType type, lvEventFunc func, int direction,
                       bool terminal, lvEventCallback callback);
lvEventResult geo_event_detect(lvEventDetector *detector, double t_prev, const double *param_prev, double t_curr,
                               const double *param_curr, int dim, int *event_id, double *t_event);
lv_PUBLIC_API int geo_event_get_count(const lvEventDetector *detector);
lv_PUBLIC_API const lvEventEntry *geo_event_get_entry(const lvEventDetector *detector, int idx);

/* ── Internal root-locating helpers ── */
lv_PUBLIC_API int geo_event_check_sign(double g_prev, double g_curr, int direction);
int geo_event_root_locate(lvEventDetector *detector, int event_id, const double *param_a, const double *param_b,
                          int dim, double a, double b, double ga, double gb, double *root);
int geo_event_root_brent(lvEventDetector *detector, int event_id, const double *param_a, const double *param_b, int dim,
                         double a, double b, double ga, double gb, double tol, int max_iter, double *root);

#ifdef __cplusplus
}
#endif
#endif
