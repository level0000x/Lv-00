/**
 * @file ga_interface.h
 * @brief Geometric quantity embedding and extraction interface for PGA
 *
 * Provides functions to convert between standard 3D geometric representations
 * (points, vectors, planes, rays, rotations, translations) and their
 * Projective Geometric Algebra (PGA) multivector encodings.
 *
 * Convention for Cl(3,0,1):
 *   - Point:       trivector (e123 component)
 *   - Vector:      e1 + e2*e03 + e3*e03 (direction vector)
 *   - Plane:       normal vector (e1 + e2 + e3 components)
 *   - Line:        bivector (e12 + e13 + e23 components)
 *   - Rotor:       even-grade multivector (scalar + bivector)
 *   - Motor:       even-grade multivector (scalar + bivector + pseudoscalar)
 *
 * @version 1.1.0
 */

#ifndef lv_GA_INTERFACE_H
#define lv_GA_INTERFACE_H

#include "ga_multivector.h"
#include "lv_api_spec.h" /* lv_PUBLIC_API（K59） */

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Point Operations
 * ======================================================================== */

/**
 * @brief Embed a 3D point into PGA as a trivector
 *
 * In Cl(3,0,1), a point (x, y, z) is represented as:
 *   P = x*e023 + y*e013 + z*e012 + e123
 *
 * @param x  X coordinate
 * @param y  Y coordinate
 * @param z  Z coordinate
 * @return A new multivector representing the embedded point
 */
lv_PUBLIC_API lvMultiVector *ga_embed_point(double x, double y, double z);

/**
 * @brief Extract 3D coordinates from a PGA point
 *
 * @param mv    Point multivector (trivector)
 * @param out_x  Output X coordinate (must not be NULL)
 * @param out_y  Output Y coordinate (must not be NULL)
 * @param out_z  Output Z coordinate (must not be NULL)
 * @return 0 on success, -1 if mv is NULL or not a valid point
 */
lv_PUBLIC_API int ga_extract_point(const lvMultiVector *mv, double *out_x, double *out_y, double *out_z);

/* ========================================================================
 * Vector Operations
 * ======================================================================== */

/**
 * @brief Embed a 3D direction vector into PGA
 *
 * A direction vector (vx, vy, vz) is represented as:
 *   v = vx*e1 + vy*e2 + vz*e3
 *
 * @param vx  X component of direction
 * @param vy  Y component of direction
 * @param vz  Z component of direction
 * @return A new multivector representing the embedded vector
 */
lv_PUBLIC_API lvMultiVector *ga_embed_vector(double vx, double vy, double vz);

/**
 * @brief Extract a 3D direction vector from a PGA vector
 *
 * @param mv     Vector multivector
 * @param out_vx  Output X component (must not be NULL)
 * @param out_vy  Output Y component (must not be NULL)
 * @param out_vz  Output Z component (must not be NULL)
 * @return 0 on success, -1 if mv is NULL
 */
lv_PUBLIC_API int ga_extract_vector(const lvMultiVector *mv, double *out_vx, double *out_vy, double *out_vz);

/* ========================================================================
 * Plane Operations
 * ======================================================================== */

/**
 * @brief Embed a plane (defined by normal + distance) into PGA
 *
 * A plane with normal (nx, ny, nz) and distance d from origin:
 *   pi = nx*e1 + ny*e2 + nz*e3 + d*e0
 *
 * @param nx  Normal X component
 * @param ny  Normal Y component
 * @param nz  Normal Z component
 * @param d   Distance from origin
 * @return A new multivector representing the embedded plane
 */
lv_PUBLIC_API lvMultiVector *ga_embed_plane(double nx, double ny, double nz, double d);

/**
 * @brief Extract plane parameters from a PGA plane
 *
 * @param mv     Plane multivector
 * @param out_nx  Output normal X (must not be NULL)
 * @param out_ny  Output normal Y (must not be NULL)
 * @param out_nz  Output normal Z (must not be NULL)
 * @param out_d   Output distance (must not be NULL)
 * @return 0 on success, -1 if mv is NULL
 */
lv_PUBLIC_API int ga_extract_plane(const lvMultiVector *mv, double *out_nx, double *out_ny, double *out_nz,
                                   double *out_d);

/* ========================================================================
 * Ray Operations
 * ======================================================================== */

/**
 * @brief Embed a ray (origin + direction) into PGA
 *
 * A ray from point P in direction v is represented as the
 * outer product P ^ v (a bivector in PGA).
 *
 * @param origin   Origin point multivector
 * @param dir      Direction vector multivector
 * @return A new multivector representing the embedded ray (bivector)
 */
lv_PUBLIC_API lvMultiVector *ga_embed_ray(const lvMultiVector *origin, const lvMultiVector *dir);

/**
 * @brief Extract ray parameters from a PGA ray
 *
 * @param mv       Ray multivector (bivector)
 * @param out_origin  Output origin point ([take] caller owns, free with ga_mv_destroy)
 * @param out_dir     Output direction vector ([take] caller owns, free with ga_mv_destroy)
 * @return 0 on success, -1 if mv is NULL or not a valid ray
 */
lv_PUBLIC_API int ga_extract_ray(const lvMultiVector *mv, lvMultiVector **out_origin, lvMultiVector **out_dir);

/* ========================================================================
 * Rotor Operations (Rotations)
 * ======================================================================== */

/**
 * @brief Embed a rotation (axis + angle) into PGA as a rotor
 *
 * A rotation by angle theta around unit axis (ax, ay, az):
 *   R = cos(theta/2) + sin(theta/2) * (ax*e23 + ay*e13 + az*e12)
 *
 * @param ax     Rotation axis X component (should be normalized)
 * @param ay     Rotation axis Y component
 * @param az     Rotation axis Z component
 * @param angle  Rotation angle in radians
 * @return A new multivector representing the rotor
 */
lv_PUBLIC_API lvMultiVector *ga_embed_rotation(double ax, double ay, double az, double angle);

/**
 * @brief Extract rotation parameters from a PGA rotor
 *
 * @param rotor    Rotor multivector (even-grade)
 * @param out_ax   Output axis X (must not be NULL)
 * @param out_ay   Output axis Y (must not be NULL)
 * @param out_az   Output axis Z (must not be NULL)
 * @param out_angle Output angle in radians (must not be NULL)
 * @return 0 on success, -1 if rotor is NULL or not a valid rotor
 */
lv_PUBLIC_API int ga_extract_rotation(const lvMultiVector *rotor, double *out_ax, double *out_ay, double *out_az,
                                      double *out_angle);

/* ========================================================================
 * Motor Operations (Translations)
 * ======================================================================== */

/**
 * @brief Embed a translation into PGA as a motor
 *
 * A translation by vector (tx, ty, tz):
 *   T = 1 + 0.5*(tx*e01 + ty*e02 + tz*e03)
 *
 * @param tx  Translation X component
 * @param ty  Translation Y component
 * @param tz  Translation Z component
 * @return A new multivector representing the translation motor
 */
lv_PUBLIC_API lvMultiVector *ga_embed_translation(double tx, double ty, double tz);

/* ========================================================================
 * Geometric Construction Functions
 * ======================================================================== */

/**
 * @brief Compute the line through two points in PGA
 *
 * The line through points P and Q is: L = P ^ Q (outer product).
 * The result is a bivector.
 *
 * @param p1  First point multivector
 * @param p2  Second point multivector
 * @return A new multivector representing the line (bivector)
 */
lv_PUBLIC_API lvMultiVector *ga_line_from_two_points(const lvMultiVector *p1, const lvMultiVector *p2);

/**
 * @brief Check if three points are collinear
 *
 * Three points P, Q, R are collinear iff P ^ Q ^ R = 0.
 *
 * @param p1  First point
 * @param p2  Second point
 * @param p3  Third point
 * @return true if collinear, false otherwise
 */
lv_PUBLIC_API bool ga_three_points_collinear(const lvMultiVector *p1, const lvMultiVector *p2, const lvMultiVector *p3);

/**
 * @brief Check if four points are coplanar
 *
 * Four points P, Q, R, S are coplanar iff P ^ Q ^ R ^ S = 0.
 *
 * @param p1  First point
 * @param p2  Second point
 * @param p3  Third point
 * @param p4  Fourth point
 * @return true if coplanar, false otherwise
 */
lv_PUBLIC_API bool ga_four_points_coplanar(const lvMultiVector *p1, const lvMultiVector *p2, const lvMultiVector *p3,
                                           const lvMultiVector *p4);

/**
 * @brief Compute the plane through three points in PGA
 *
 * The plane through points P, Q, R is: pi = P ^ Q ^ R (outer product).
 * The result is a trivector.
 *
 * @param p1  First point
 * @param p2  Second point
 * @param p3  Third point
 * @return A new multivector representing the plane (trivector)
 */
lv_PUBLIC_API lvMultiVector *ga_plane_from_three_points(const lvMultiVector *p1, const lvMultiVector *p2,
                                                        const lvMultiVector *p3);

#ifdef __cplusplus
}
#endif

#endif /* lv_GA_INTERFACE_H */
