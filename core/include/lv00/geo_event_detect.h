#ifndef LV00_GEO_EVENT_DETECT_H
#define LV00_GEO_EVENT_DETECT_H

#ifdef __cplusplus
extern "C" {
#endif

#define _USE_MATH_DEFINES
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ── Constants ── */
#define GEO_EVENT_MAX_EVENTS       128
#define GEO_EVENT_DEFAULT_TOL      1e-12
#define GEO_EVENT_MAX_ROOT_ITERS   100

/* ── Root-finding method ── */
typedef enum {
    LV00_ROOTFIND_BISECTION = 0,
    LV00_ROOTFIND_BRENT,
    LV00_ROOTFIND_ILLINOIS,
    LV00_ROOTFIND_NEWTON,
    LV00_ROOTFIND_SECANT
} Lv00RootMethod;

/* ── Event type ── */
typedef enum {
    LV00_EVENT_INTERSECTION = 0,
    LV00_EVENT_COLLISION,
    LV00_EVENT_BOUNDARY,
    LV00_EVENT_COPLANAR,
    LV00_EVENT_DEGENERATE,
    LV00_EVENT_CONTACT,
    LV00_EVENT_CROSSING,
    LV00_EVENT_THRESHOLD,
    LV00_EVENT_PERIODIC,
    LV00_EVENT_CUSTOM
} Lv00EventType;

/* ── Event result ── */
typedef enum {
    LV00_EVENT_RESULT_ERROR    = -1,
    LV00_EVENT_RESULT_NONE     = 0,
    LV00_EVENT_RESULT_DETECTED = 1,
    LV00_EVENT_RESULT_WARNING  = 2
} Lv00EventResult;

/* ── Forward decl ── */
typedef struct Lv00EventDetector Lv00EventDetector;

/* ── Event function signature ── */
typedef int (*Lv00EventFunc)(double t, const double *param, int dim,
                              double *g, Lv00EventDetector *detector);

/* ── Event callback ── */
typedef void (*Lv00EventCallback)(Lv00EventDetector *detector,
                                   int event_id, double t, const double *param);

/* ── Event entry ── */
typedef struct {
    int               event_id;
    Lv00EventType     type;
    Lv00EventFunc     func;
    bool              enabled;
    double            t_prev;
    double            t_span;
    int               direction;
    bool              terminal;
    Lv00EventCallback callback;
} Lv00EventEntry;

/* ── Event detector ── */
struct Lv00EventDetector {
    Lv00EventEntry  events[GEO_EVENT_MAX_EVENTS];
    int             num_events;
    Lv00RootMethod  root_method;
    double          root_tol;
    int             max_root_iters;
    double          t_prev;
    double          g_prev[GEO_EVENT_MAX_EVENTS];
    void           *user_data;
};

/* ── API ── */
Lv00EventDetector *geo_event_detector_create(void);
void geo_event_detector_destroy(Lv00EventDetector *detector);
int geo_event_register(Lv00EventDetector *detector, int event_id,
                       Lv00EventType type, Lv00EventFunc func, int direction,
                       bool terminal, Lv00EventCallback callback);
Lv00EventResult geo_event_detect(Lv00EventDetector *detector, double t_prev,
                                 const double *param_prev, double t_curr,
                                 const double *param_curr, int dim,
                                 int *event_id, double *t_event);
int geo_event_get_count(const Lv00EventDetector *detector);
const Lv00EventEntry *geo_event_get_entry(const Lv00EventDetector *detector, int idx);

/* ── Internal root-locating helpers ── */
int geo_event_check_sign(double g_prev, double g_curr, int direction);
int geo_event_root_locate(Lv00EventDetector *detector, int event_id,
                          const double *param_a, const double *param_b,
                          int dim, double a, double b, double ga, double gb,
                          double *root);
int geo_event_root_brent(Lv00EventDetector *detector, int event_id,
                         const double *param_a, const double *param_b,
                         int dim, double a, double b, double ga, double gb,
                         double tol, int max_iter, double *root);

#ifdef __cplusplus
}
#endif
#endif
