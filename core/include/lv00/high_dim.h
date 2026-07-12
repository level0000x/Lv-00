#ifndef LV00_HIGH_DIM_H
#define LV00_HIGH_DIM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "constraint_graph.h"

/* ── Constants ── */
#define HIGH_DIM_MAX_DIMENSIONS            16
#define HIGH_DIM_MAX_DEPTH                 32
#define HIGH_DIM_MAX_PROJECTION_PRESETS    64
#define HIGH_DIM_MAX_ACTIVE_VIEWS          16
#define HIGH_DIM_INITIAL_CAPACITY          8
#define HIGH_DIM_PROJECTION_NAME_MAX       64
#define HIGH_DIM_DEFAULT_FIDELITY_THRESHOLD 0.85

/* ── Forward decls ── */
struct SymbolicCoord;
typedef struct SymbolicCoord SymbolicCoord;

typedef struct HighDimTransform2D HighDimTransform2D;

/* ── Axis Mapping Type ── */
typedef enum {
    HIGH_DIM_MAP_NONE    = 0,
    HIGH_DIM_MAP_TO_X,
    HIGH_DIM_MAP_TO_Y,
    HIGH_DIM_MAP_FOLD,
    HIGH_DIM_MAP_DISCARD,
    HIGH_DIM_MAP_LINEAR,
    HIGH_DIM_MAP_LOG,
    HIGH_DIM_MAP_EXP,
    HIGH_DIM_MAP_PCA,
    HIGH_DIM_MAP_T_SNE
} HighDimMappingType;

/* ── Axis Mapping ── */
typedef struct {
    int                axis_index;
    HighDimMappingType mapping_type;
    double             scale;
    double             offset;
} HighDimAxisMapping;

/* ── Transform 2D ── */
struct HighDimTransform2D {
    double  m[3][3];
    int     type;
};

/* ── Projection Preset ── */
typedef struct HighDimProjectionPreset {
    char                name[HIGH_DIM_PROJECTION_NAME_MAX];
    int                 dimension_count;
    int                 mapping_count;
    HighDimAxisMapping  mappings[HIGH_DIM_MAX_DIMENSIONS];
    HighDimTransform2D  transform;
    bool                is_default;
} HighDimProjectionPreset;

/* ── Projected 2D Coord ── */
typedef struct HighDimProjectedCoord {
    double x;
    double y;
    char   folded_info[256];
    bool   is_valid;
} HighDimProjectedCoord;

/* ── Visibility Stats ── */
typedef struct HighDimVisibilityStats {
    double  fidelity_ratio;
    double  occlusion_rate;
    int     visible_elements;
    int     total_elements;
    int     visible_relations;
    int     total_relations;
    bool    is_below_threshold;
} HighDimVisibilityStats;

/* ── Abstract Block ── */
typedef struct HighDimAbstractBlock {
    int                     block_id;
    int                     dimension_count;
    char                    name[64];
    HighDimProjectionPreset presets[HIGH_DIM_MAX_PROJECTION_PRESETS];
    int                     preset_count;
    int                     current_preset_index;
    int                     mapping_count;
    HighDimAxisMapping      mappings[HIGH_DIM_MAX_DIMENSIONS];
    double                  fidelity_ratio;
} HighDimAbstractBlock;

/* ── High-Dim Manager ── */
typedef struct HighDimManager {
    HighDimAbstractBlock    *blocks;          /* inline array, not pointer array */
    int                      block_count;
    int                      block_capacity;
    int                      perspective_depth;
    int                      perspective_stack[HIGH_DIM_MAX_DEPTH];
} HighDimManager;

/* ── Manager API ── */
HighDimManager* high_dim_manager_create(void);
void high_dim_manager_destroy(HighDimManager *manager);
int  high_dim_manager_init(HighDimManager *manager);

/* ── Block API ── */
int  high_dim_register_block(HighDimManager *manager, int block_id, int dimension_count);
int  high_dim_unregister_block(HighDimManager *manager, int block_id);
HighDimAbstractBlock* high_dim_get_block(HighDimManager *manager, int block_id);

/* ── Preset API ── */
int  high_dim_add_projection_preset(HighDimManager *manager, int block_id,
                                     const HighDimProjectionPreset *preset);
int  high_dim_remove_projection_preset(HighDimManager *manager, int block_id, int preset_index);
int  high_dim_set_current_preset(HighDimManager *manager, int block_id, int preset_index);
const HighDimProjectionPreset* high_dim_get_current_preset(const HighDimManager *manager, int block_id);
int  high_dim_create_default_preset(int dimension_count, HighDimProjectionPreset *preset);

/* ── Projection API ── */
int  high_dim_project_coordinates(HighDimManager *manager, int block_id,
                                   const SymbolicCoord **high_dim_coords, int coord_count,
                                   HighDimProjectedCoord *projected);

/* ── Transform API ── */
int  high_dim_apply_transform(const HighDimProjectedCoord *coord,
                               const HighDimTransform2D *transform,
                               HighDimProjectedCoord *result);
int  high_dim_create_rotation_transform(double angle_rad, HighDimTransform2D *transform);
int  high_dim_create_scale_transform(double scale_x, double scale_y, HighDimTransform2D *transform);

/* ── Fidelity API ── */
int  high_dim_calculate_fidelity(HighDimManager *manager, int block_id,
                                  const ConstraintGraph *graph,
                                  HighDimVisibilityStats *stats);
int  high_dim_is_fidelity_below_threshold(const HighDimManager *manager, int block_id, double threshold);
int  high_dim_get_fidelity_warning(const HighDimManager *manager, int block_id,
                                    char *buffer, size_t buffer_size);
int  high_dim_compute_fidelity(HighDimManager *manager, int block_id,
                                const ConstraintGraph *graph,
                                HighDimVisibilityStats *stats);

/* ── Perspective API ── */
int  high_dim_enter_block_perspective(HighDimManager *manager, int block_id);
int  high_dim_exit_block_perspective(HighDimManager *manager);
int  high_dim_get_current_depth(const HighDimManager *manager);

/* ── Multi-view API ── */
int  high_dim_create_multi_projection_view(HighDimManager *manager, int block_id,
                                            const int *preset_indices, int preset_count,
                                            int *out_view_ids);
int  high_dim_destroy_multi_projection_view(HighDimManager *manager, int view_id);
int  high_dim_link_highlight(HighDimManager *manager, const int *view_ids, int view_count, int element_id);

/* ── Serialize API ── */
int  high_dim_preset_serialize_json(const HighDimProjectionPreset *preset,
                                     char *buffer, size_t buffer_size);
int  high_dim_preset_deserialize_json(const char *json, HighDimProjectionPreset *preset);

/* ── Folded Dimensions ── */
int  high_dim_get_folded_dimensions_info(const HighDimProjectionPreset *preset,
                                          char *buffer, size_t buffer_size);

/* ── 3D Projection ── */
int  high_dim_project_to_3d(const double *coord_4d, int dim_count,
                             double angle_xy, int coord_count, double *out_coords);

/* ── Internals ── */
int  high_dim_validate_mapping(int dimension_count, const HighDimAxisMapping *mappings, int mapping_count);
const char *high_dim_mapping_type_to_string(HighDimMappingType type);
HighDimMappingType high_dim_mapping_type_from_string(const char *str);
double symbolic_coord_to_double(const SymbolicCoord *coord);

#ifdef __cplusplus
}
#endif
#endif
