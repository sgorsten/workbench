// This module consists of types that assist in the geometric transformation of objects between spaces
#pragma once
#include "core.h"

// vector    - a quantity of oriented length, having direction and magnitude, represented by its components in x, y, z order
// point     - a location in space, represented by the displacement vector from the origin
// bivector  - a quantity of oriented area, the exterior product of two vectors, represented by its components in yz, zx, xy order
// direction - an oriented one dimensional subspace, represented by a vector of unit length
// normal    - an oriented two dimensional subspace, represented by a bivector of unit area

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Transform types advertise traits, which allow many transformation and detransformation functions to be generated automatically //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum transform_trait_flags : int
{
    transform_is_linear_bit            = 1<<0, // Transform does not distinguish between vectors and points
    transform_is_composable_bit        = 1<<1, // Transforms a and b can be composed via mul(a,b)
    transform_is_invertible_bit        = 1<<2, // Transform t can be inverted via inverse(t)
    transform_preserves_scale_bit      = 1<<3, // Transform preserves the magnitude of physical measurements
    transform_preserves_handedness_bit = 1<<4, // Transform preserves the sign of physical measurements
};

template<class Transform> struct transform_traits : std::integral_constant<int, Transform::transform_traits> {};
template<class Transform> constexpr bool is_linear            = (transform_traits<Transform>::value & transform_is_linear_bit) != 0;
template<class Transform> constexpr bool is_composable        = (transform_traits<Transform>::value & transform_is_composable_bit) != 0;
template<class Transform> constexpr bool is_invertible        = (transform_traits<Transform>::value & transform_is_invertible_bit) != 0;
template<class Transform> constexpr bool preserves_scale      = (transform_traits<Transform>::value & transform_preserves_scale_bit) != 0;
template<class Transform> constexpr bool preserves_handedness = (transform_traits<Transform>::value & transform_preserves_handedness_bit) != 0;

// All transforms must provide a conversion of last resort: get_transform_matrix :: Transform -> float4x4
template<class Transform> float3 transform_vector(const Transform & transform, const float3 & vector)
{ 
    return transform_vector(get_transform_matrix(transform), vector); 
}
template<class Transform> float3 transform_point(const Transform & transform, const float3 & point) 
{ 
    if constexpr(is_linear<Transform>) return transform_vector(transform, point); // Linear transforms can treat points as vectors
    else return transform_point(get_transform_matrix(transform), point); // Otherwise fall back to using matrices
}
template<class Transform> float3 transform_bivector(const Transform & transform, const float3 & bivector)
{
    if constexpr(preserves_handedness<Transform>) return transform_vector(transform, bivector); // Transforms which preserve handedness can treat bivectors as vectors
    else return transform_bivector(get_transform_matrix(transform), bivector); // Otherwise fall back to using matrices (which requires an expensive inverse transpose)
}
template<class Transform> float3 transform_direction(const Transform & transform, const float3 & direction)
{
    if constexpr(preserves_scale<Transform>) return transform_vector(transform, direction); // Transforms which preserve scale can treat directions as vectors
    else return normalize(transform_vector(transform, direction)); // Otherwise they must be renormalized after transformation
}
template<class Transform> float3 transform_normal(const Transform & transform, const float3 & normal) 
{ 
    if constexpr(preserves_handedness<Transform>) return transform_direction(transform, normal); // Transforms which preserve handedness can treat normals as directions
    else if constexpr(preserves_scale<Transform>) return transform_bivector(transform, normal); // Transforms which preserve scale can treat normals as bivectors
    else return normalize(transform_bivector(transform, normal)); // Otherwise they must be renormalized after transformation
}
template<class Transform> float4 transform_quaternion(const Transform & transform, const float4 & quaternion)
{
    return {transform_bivector(transform, quaternion.xyz()), quaternion.w};
}

// If a transform is not invertible, we can always fall back to inverting its 4x4 matrix representation
template<class Transform> float4x4 get_inverse_transform_matrix(const Transform & transform) 
{ 
    if constexpr(is_invertible<Transform>) return get_transform_matrix(inverse(transform));
    else return inverse(get_transform_matrix(transform)); 
}
template<class Transform> float3 detransform_vector(const Transform & transform, const float3 & vector)
{ 
    if constexpr(is_invertible<Transform>) return transform_vector(inverse(transform), vector);
    else return transform_vector(get_inverse_transform_matrix(transform), vector);
}
template<class Transform> float3 detransform_point(const Transform & transform, const float3 & point)
{ 
    if constexpr(is_linear<Transform>) return detransform_vector(transform, point);
    if constexpr(is_invertible<Transform>) return transform_point(inverse(transform), point);
    else return transform_point(get_inverse_transform_matrix(transform), point);
}
template<class Transform> float3 detransform_bivector(const Transform & transform, const float3 & bivector)
{ 
    if constexpr(preserves_handedness<Transform>) return detransform_vector(transform, bivector);
    else if constexpr(is_invertible<Transform>) return transform_bivector(inverse(transform), bivector);
    else return transform_bivector(get_inverse_transform_matrix(transform), bivector);
}
template<class Transform> float3 detransform_direction(const Transform & transform, const float3 & direction)
{ 
    if constexpr(preserves_scale<Transform>) return detransform_vector(transform, direction);
    else if constexpr(is_invertible<Transform>) return transform_direction(inverse(transform), direction);
    else return normalize(detransform_direction(transform, direction));
}
template<class Transform> float3 detransform_normal(const Transform & transform, const float3 & normal) 
{ 
    if constexpr(preserves_handedness<Transform>) return detransform_direction(transform, normal);
    else if constexpr(preserves_scale<Transform>) return detransform_bivector(transform, normal);
    else if constexpr(is_invertible<Transform>) return transform_normal(inverse(transform), normal);
    else return normalize(detransform_bivector(transform, normal));
}
template<class Transform> float4 detransform_quaternion(const Transform & transform, const float4 & quaternion)
{
    return {detransform_bivector(transform, quaternion.xyz()), quaternion.w};
}

///////////////////////////////////////////////////////////////////////////////
// 4x4 matrices provide a composable and invertible transform of last resort //
///////////////////////////////////////////////////////////////////////////////

template<> struct transform_traits<float4x4> : std::integral_constant<int, transform_is_composable_bit | transform_is_invertible_bit> {};
inline float3 transform_vector  (const float4x4 & t, const float3 & vector) { return mul(t, float4{vector,0}).xyz(); }
inline float3 transform_point   (const float4x4 & t, const float3 & point) { auto r=mul(t, float4{point,1}); return r.xyz()/r.w; }
inline float3 transform_bivector(const float4x4 & t, const float3 & bivector) { return mul(inverse(transpose(t)), float4{bivector,0}).xyz(); }

/////////////////////////////////////////////////////////////////////////////
// 3x3 matrices are useful when representing purely linear transformations //
/////////////////////////////////////////////////////////////////////////////

template<> struct transform_traits<float3x3> : std::integral_constant<int, transform_is_linear_bit | transform_is_composable_bit | transform_is_invertible_bit> {};
inline float4x4 get_transform_matrix(const float3x3 & t) { return {{t.x,0},{t.y,0},{t.z,0},{0,0,0,1}}; }
inline float3 transform_vector  (const float3x3 & t, const float3 & v) { return mul(t, v); }
inline float3 transform_bivector(const float3x3 & t, const float3 & b) { return mul(inverse(transpose(t)), b); }
inline float3 transform_point   (const float3x3 & t, const float3 & p) { return transform_vector(t, p); }

////////////////////////////////////////////////////////////////
// scaling factors represent distinct scaling along each axis //
////////////////////////////////////////////////////////////////

struct scaling_factors
{
    static constexpr int transform_traits = transform_is_linear_bit | transform_is_composable_bit | transform_is_invertible_bit;

    float3 factors; // The scaling along each axis, stored as the components of a vector

    scaling_factors()                       : factors{1,1,1} {}
    scaling_factors(float uniform_factor)   : factors{uniform_factor} {}
    scaling_factors(const float3 & factors) : factors{factors} {}
};

inline float4x4 get_transform_matrix (const scaling_factors & t)                            { return scaling_matrix(t.factors); }
inline float3 transform_vector       (const scaling_factors & t, const float3 & v)          { return v * t.factors; }
inline float3 transform_bivector     (const scaling_factors & t, const float3 & b)          { return b * product(t.factors)/t.factors; }
inline float3 transform_normal       (const scaling_factors & t, const float3 & b)          { return normalize(b / t.factors); }
inline scaling_factors mul           (const scaling_factors & a, const scaling_factors & b) { return {a.factors * b.factors}; }
inline scaling_factors inverse       (const scaling_factors & t)                            { return {1.0f/t.factors}; }

//////////////////////////////////////////////////////////////////
// pure rotations show up frequently in spatial transformations //
//////////////////////////////////////////////////////////////////

struct pure_rotation
{
    static constexpr int transform_traits = transform_is_linear_bit | transform_is_composable_bit | transform_is_invertible_bit | transform_preserves_scale_bit | transform_preserves_handedness_bit;

    float4 quaternion; // May contain either unit-length quaternion representing the desired rotation

    pure_rotation()                                 : quaternion{0,0,0,1} {}
    pure_rotation(const float4 & quaternion)        : quaternion{quaternion} {}
    pure_rotation(const float3 & axis, float angle) : quaternion{rotation_quat(axis, angle)} {}
    pure_rotation(const float3x3 & rotation_matrix) : quaternion{rotation_quat(rotation_matrix)} {}
};

inline float4x4 get_transform_matrix (const pure_rotation & t)                          { return rotation_matrix(t.quaternion); }
inline float3 transform_vector       (const pure_rotation & t, const float3 & v)        { return qrot(t.quaternion, v); }
inline pure_rotation mul             (const pure_rotation & a, const pure_rotation & b) { return {qmul(a.quaternion, b.quaternion)}; }
inline pure_rotation inverse         (const pure_rotation & t)                          { return {qconj(t.quaternion)}; }

///////////////////////////////////////////////////////////////////////////////////////////////////
// general transform with per-axis scaling followed by a pure rotation followed by a translation //
///////////////////////////////////////////////////////////////////////////////////////////////////

struct scaled_transform
{
    static constexpr int transform_traits = 0;

    scaling_factors scaling;
    pure_rotation rotation;
    float3 translation;

    scaled_transform()                                                                                              = default;
    scaled_transform(const pure_rotation & rotation)                                                                : rotation{rotation} {}
    scaled_transform(const float3 & translation)                                                                    : translation{translation} {}
    scaled_transform(const pure_rotation & rotation, const float3 & translation)                                    : rotation{rotation}, translation{translation} {}
    scaled_transform(const scaling_factors & scaling)                                                               : scaling{scaling} {}
    scaled_transform(const scaling_factors & scaling, const pure_rotation & rotation)                               : scaling{scaling}, rotation{rotation} {}
    scaled_transform(const scaling_factors & scaling, const float3 & translation)                                   : scaling{scaling}, translation{translation} {}
    scaled_transform(const scaling_factors & scaling, const pure_rotation & rotation, const float3 & translation)   : scaling{scaling}, rotation{rotation}, translation{translation} {}
};

inline float4x4 get_transform_matrix (const scaled_transform & t)                   { return mul(pose_matrix(t.rotation.quaternion, t.translation), get_transform_matrix(t.scaling)); }
inline float3 transform_vector       (const scaled_transform & t, const float3 & v) { return transform_vector(t.rotation, transform_vector(t.scaling, v)); }
inline float3 transform_point        (const scaled_transform & t, const float3 & p) { return transform_vector(t, p) + t.translation; }
inline float3 transform_bivector     (const scaled_transform & t, const float3 & b) { return transform_bivector(t.rotation, transform_bivector(t.scaling, b)); }
inline float3 transform_normal       (const scaled_transform & t, const float3 & n) { return transform_normal(t.rotation, transform_normal(t.scaling, n)); }
inline float3 detransform_vector     (const scaled_transform & t, const float3 & v) { return detransform_vector(t.scaling, detransform_vector(t.rotation, v)); }
inline float3 detransform_point      (const scaled_transform & t, const float3 & p) { return detransform_vector(t, p - t.translation); }
inline float3 detransform_bivector   (const scaled_transform & t, const float3 & b) { return detransform_bivector(t.scaling, detransform_bivector(t.rotation, b)); }
inline float3 detransform_normal     (const scaled_transform & t, const float3 & n) { return detransform_normal(t.scaling, detransform_normal(t.rotation, n)); }

///////////////////////////////////////////////////////////////////////////////
// The group of direct isometries is an exceptionally well behaved transform //
///////////////////////////////////////////////////////////////////////////////

struct rigid_transform
{
    static constexpr int transform_traits = transform_is_composable_bit | transform_is_invertible_bit | transform_preserves_scale_bit | transform_preserves_handedness_bit;

    pure_rotation rotation;
    float3 translation;

    rigid_transform()                                                           = default;
    rigid_transform(const pure_rotation & rotation)                             : rotation{rotation} {}
    rigid_transform(const float3 & translation)                                 : translation{translation} {}
    rigid_transform(const pure_rotation & rotation, const float3 & translation) : rotation{rotation}, translation{translation} {}
};

inline float4x4 get_transform_matrix (const rigid_transform & t)                            { return pose_matrix(t.rotation.quaternion, t.translation); }
inline float3 transform_vector       (const rigid_transform & t, const float3 & v)          { return transform_vector(t.rotation, v); }
inline float3 transform_point        (const rigid_transform & t, const float3 & p)          { return transform_vector(t, p) + t.translation; }
inline float3 detransform_vector     (const rigid_transform & t, const float3 & v)          { return detransform_vector(t.rotation, v); }
inline float3 detransform_point      (const rigid_transform & t, const float3 & p)          { return detransform_vector(t, p - t.translation); }
inline rigid_transform mul           (const rigid_transform & a, const rigid_transform & b) { return {mul(a.rotation, b.rotation), transform_point(a, b.translation)}; }
inline rigid_transform inverse       (const rigid_transform & t)                            { return {inverse(t.rotation), detransform_vector(t, -t.translation)}; }

///////////////////////////////////////////////////////////////////////////////
// coordinate system transforms are capable of transforming other transforms //
///////////////////////////////////////////////////////////////////////////////

struct coord_transform
{
    static constexpr int transform_traits = transform_is_linear_bit | transform_is_composable_bit | transform_is_invertible_bit | transform_preserves_scale_bit;

    float3x3 matrix; // One of the forty eight orthonormal 3x3 matrices where all entries are -1, 0, or 1
    float det;       // Determinant of matrix, -1 in case of a handedness flip, 1 otherwise

    coord_transform()                                                   : matrix{{1,0,0},{0,1,0},{0,0,1}}, det{1} {}
    coord_transform(const coord_system & from, const coord_system & to) : matrix{to(from.x_axis), to(from.y_axis), to(from.z_axis)}, det{determinant(matrix)} {}
    coord_transform(const float3x3 & matrix, float det)                 : matrix{matrix}, det{det} {}
};

inline float4x4 get_transform_matrix (const coord_transform & t)                            { return get_transform_matrix(t.matrix); }
inline float3 transform_vector       (const coord_transform & t, const float3 & v)          { return mul(t.matrix, v); }
inline float3 transform_bivector     (const coord_transform & t, const float3 & b)          { return mul(t.matrix, b) * t.det; }
inline coord_transform mul           (const coord_transform & a, const coord_transform & b) { return {mul(a.matrix, b.matrix), a.det * b.det}; }
inline coord_transform inverse       (const coord_transform & t)                            { return {transpose(t.matrix), t.det}; }

inline float4x4 transform_transform         (const coord_transform & t, const float4x4 & m)         { return mul(get_transform_matrix(t), m, transpose(get_transform_matrix(t))); }
inline float3x3 transform_transform         (const coord_transform & t, const float3x3 & m)         { return mul(t.matrix, m, transpose(t.matrix)); }
inline scaling_factors transform_transform  (const coord_transform & t, const scaling_factors & s)  { return diagonal(transform_transform(t, {{s.factors.x,0,0},{0,s.factors.y,0},{0,0,s.factors.z}})); }
inline pure_rotation transform_transform    (const coord_transform & t, const pure_rotation & r)    { return {transform_quaternion(t, r.quaternion)}; }
inline scaled_transform transform_transform (const coord_transform & t, const scaled_transform & s) { return {transform_transform(t,s.scaling), transform_transform(t,s.rotation), transform_vector(t,s.translation)}; }
inline rigid_transform transform_transform  (const coord_transform & t, const rigid_transform & r)  { return {transform_transform(t,r.rotation), transform_vector(t,r.translation)}; }

template<class Transform> Transform detransform_transform(const coord_transform & t, const Transform & r) { return transform_transform(inverse(t), r); }
