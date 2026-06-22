#ifndef LV00_GEOMETRY_TRANSFORM_H
#define LV00_GEOMETRY_TRANSFORM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <gmp.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Transform type enum ── */
typedef enum {
    TRANSFORM_IDENTITY    = 0,
    TRANSFORM_TRANSLATION,
    TRANSFORM_ROTATION,
    TRANSFORM_SCALE,
    TRANSFORM_SHEAR,
    TRANSFORM_REFLECTION,
    TRANSFORM_SCALING,
    TRANSFORM_AFFINE,
    TRANSFORM_PROJECTIVE,
    TRANSFORM_GLUING,
    TRANSFORM_COMPOSITE
} Lv00TransformType;

/* ── 2x3 affine matrix (rational) ── */
typedef struct {
    mpq_t a;
    mpq_t b;
    mpq_t tx;
    mpq_t c;
    mpq_t d;
    mpq_t ty;
} Lv00AffineMatrix;

/* ── Translation params ── */
typedef struct {
    mpq_t dx;
    mpq_t dy;
} Lv00TranslationParams;

/* ── Rotation params ── */
typedef struct {
    mpq_t cx;
    mpq_t cy;
    mpq_t cos_a;
    mpq_t sin_a;
    mpq_t cos_theta;
    mpq_t sin_theta;
    double angle;
    double angle_cos;
    double angle_sin;
    bool   is_special_angle;
    int    angle_numerator;
    int    angle_denominator;
} Lv00RotationParams;

/* ── Scale params ── */
typedef struct {
    mpq_t sx;
    mpq_t sy;
} Lv00ScaleParams;

/* ── Scaling params ── */
typedef struct {
    mpq_t sx;
    mpq_t sy;
    mpq_t cx;
    mpq_t cy;
    mpq_t scale;
} Lv00ScalingParams;

/* ── Reflection params ── */
typedef struct {
    mpq_t ax;
    mpq_t ay;
    mpq_t bx;
    mpq_t by;
    mpq_t line_a;
    mpq_t line_b;
    mpq_t line_c;
} Lv00ReflectionParams;

/* ── Transform params union ── */
typedef union {
    Lv00TranslationParams translation;
    Lv00RotationParams    rotation;
    Lv00ScaleParams       scale;
    Lv00ScalingParams     scaling;
    Lv00ReflectionParams  reflection;
} Lv00TransformParamsUnion;

typedef struct {
    Lv00TransformParamsUnion params;
} Lv00TransformParams;

/* ── Main transform struct ── */
typedef struct Lv00Transform {
    Lv00TransformType   type;
    Lv00AffineMatrix    matrix;
    bool                matrix_valid;
    Lv00TransformParams params;
    bool                is_isometry;
    bool                is_orientation_preserving;
    int                 ref_count;
} Lv00Transform;

/* ── Transform sequence ── */
typedef struct {
    Lv00Transform **transforms;
    int             count;
    int             capacity;
    bool            composite_valid;
} Lv00TransformSequence;

/* ── Transform group ── */
#define GROUP_MAX_GENERATORS 16
typedef struct {
    char            *group_name;
    Lv00Transform  **generators;
    int              generator_count;
    int              order;
    bool             is_abelian;
} Lv00TransformGroup;

/* ── Transform matrix (output type, uses mpq_t) ── */
typedef Lv00AffineMatrix Lv00TransformMatrix;

/* ── API ── */
Lv00Transform *lv00_transform_identity(void);
Lv00Transform *lv00_transform_translation(const mpq_t dx, const mpq_t dy);
Lv00Transform *lv00_transform_rotation(const mpq_t cx, const mpq_t cy,
                                        int angle_num, int angle_denom);
Lv00Transform *lv00_transform_rotation_arbitrary(const mpq_t cx, const mpq_t cy,
                                                  const mpq_t cos_a, const mpq_t sin_a);
Lv00Transform *lv00_transform_rotation_double(double cx, double cy, double angle_rad);
Lv00Transform *lv00_transform_scale(const mpq_t sx, const mpq_t sy);
Lv00Transform *lv00_transform_reflection(const mpq_t ax, const mpq_t ay,
                                          const mpq_t bx, const mpq_t by);
Lv00Transform *lv00_transform_reflection_line(const mpq_t a, const mpq_t b, const mpq_t c);
void lv00_transform_destroy(Lv00Transform *t);
void lv00_transform_ref(Lv00Transform *t);
void lv00_transform_unref(Lv00Transform *t);
bool lv00_transform_apply_point(const Lv00Transform *t, mpq_t x, mpq_t y);
void lv00_transform_apply_mpq(const Lv00Transform *t, const mpq_t src_x, const mpq_t src_y,
                                mpq_t dst_x, mpq_t dst_y);
void lv00_transform_apply_double(const Lv00Transform *t, double src_x, double src_y,
                                   double *dst_x, double *dst_y);
Lv00Transform *lv00_transform_compose(const Lv00Transform *a, const Lv00Transform *b);
bool lv00_transform_get_matrix(Lv00Transform *t, Lv00TransformMatrix *matrix);
const char *lv00_transform_type_name(Lv00TransformType type);
bool lv00_reflect_point(const mpq_t ax, const mpq_t ay,
                         const mpq_t bx, const mpq_t by,
                         const mpq_t px, const mpq_t py,
                         mpq_t rx, mpq_t ry);

/* ── Sequence API ── */
Lv00TransformSequence *lv00_transform_sequence_create(void);
void lv00_transform_sequence_destroy(Lv00TransformSequence *seq);
bool lv00_transform_sequence_add(Lv00TransformSequence *seq, Lv00Transform *t);
Lv00Transform *lv00_transform_sequence_compose_all(const Lv00TransformSequence *seq);

/* ── Group API ── */
Lv00TransformGroup *lv00_transform_group_create(const char *name);
void lv00_transform_group_destroy(Lv00TransformGroup *group);
bool lv00_transform_group_add_generator(Lv00TransformGroup *group, Lv00Transform *generator);
Lv00TransformGroup *lv00_transform_group_create_preset(const char *type);

/* ── Double convenience API ── */
void lv00_transform_identity_double(double out[16]);
void lv00_transform_translate_double(double out[16], double x, double y, double z);
void lv00_transform_rotate_double(double out[16], double angle_rad, double x, double y, double z);
void lv00_transform_scale_double(double out[16], double sx, double sy, double sz);
void lv00_transform_apply_double4x4(const double t[16], const double *in, double *out, size_t count);

#ifdef __cplusplus
}
#endif
#endif
