#pragma once
#include "core.h"

// A value type representing an abstract direction vector in 3D space, independent of any coordinate system
enum class coord_axis { forward, back, left, right, up, down };
constexpr float dot(coord_axis a, coord_axis b) { return a == b ? 1 : (static_cast<int>(a) ^ static_cast<int>(b)) == 1 ? -1 : 0.0f; }

// A concrete 3D coordinate system with defined x, y, and z axes
struct coord_system
{
    coord_axis x_axis, y_axis, z_axis;
    constexpr float3 operator ()(coord_axis axis) const { return {dot(x_axis, axis), dot(y_axis, axis), dot(z_axis, axis)}; }
    constexpr float3 cross(coord_axis a, coord_axis b) const { return linalg::cross((*this)(a), (*this)(b)); }
    constexpr bool is_orthogonal() const { return dot(x_axis, y_axis) == 0 && dot(y_axis, z_axis) == 0 && dot(z_axis, x_axis) == 0; }
    constexpr bool is_left_handed() const { return dot(cross(coord_axis::forward, coord_axis::up), (*this)(coord_axis::left)) == 1; }
    constexpr bool is_right_handed() const { return dot(cross(coord_axis::forward, coord_axis::up), (*this)(coord_axis::right)) == 1; }
};
constexpr float3x3 make_transform(const coord_system & from, const coord_system & to) { return {to(from.x_axis), to(from.y_axis), to(from.z_axis)}; }
constexpr float4x4 make_transform_4x4(const coord_system & from, const coord_system & to) { return {{to(from.x_axis),0}, {to(from.y_axis),0}, {to(from.z_axis),0}, {0,0,0,1}}; }

// A proper rigid transformation represented as a rotation followed by a translation
struct rigid_transform
{
    float4 rotation {0,0,0,1};  // The rotation component stored as a quaternion of approximately unit length
    float3 translation {0,0,0}; // The translation component stored as a vector

    rigid_transform inverse() const { return {qconj(rotation), qrot(qconj(rotation), -translation)}; }
    float4x4 matrix() const { return pose_matrix(rotation, translation); }

    float3 transform_vector(const float3 & v) const { return qrot(rotation, v); }
    float3 transform_point(const float3 & p) const { return translation + transform_vector(p); }
    float3 detransform_point(const float3 & p) const { return detransform_vector(p - translation); }
    float3 detransform_vector(const float3 & v) const { return qrot(qconj(rotation), v); }
};
inline rigid_transform slerp(const rigid_transform & a, const rigid_transform & b, float t) { return {slerp(a.rotation, b.rotation, t), lerp(a.translation, b.translation, t)}; }
inline rigid_transform nlerp(const rigid_transform & a, const rigid_transform & b, float t) { return {nlerp(a.rotation, b.rotation, t), lerp(a.translation, b.translation, t)}; }

// Convert from a normalized right-down-forward direction vector to right-down texcoords, with the forward vector centered at 0.5,0.5
inline float2 compute_sphere_texcoords(float3 direction) { return float2{std::atan2(direction.x, direction.z)*0.1591549f, std::asin(direction.y)*0.3183099f}+0.5f; }

// Shape transformations
struct ray { float3 origin, direction; };
inline ray transform(const rigid_transform & t, const ray & r) { return {t.transform_point(r.origin), t.transform_vector(r.direction)}; }
inline ray detransform(const rigid_transform & t, const ray & r) { return {t.detransform_point(r.origin), t.detransform_vector(r.direction)}; }
struct ray_triangle_hit { float t; float2 uv; };
struct ray_mesh_hit { float t; size_t triangle; float2 uv; };

// Shape intersection routines
std::optional<float> intersect_ray_plane(const ray & ray, const float4 & plane);
std::optional<ray_triangle_hit> intersect_ray_triangle(const ray & ray, const float3 & v0, const float3 & v1, const float3 & v2);
