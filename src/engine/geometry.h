#pragma once
#include "core.h"
#include "linalg.h"
using namespace linalg::aliases;

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
    float4x4 matrix() const { return linalg::pose_matrix(rotation, translation); }
};
inline rigid_transform slerp(const rigid_transform & a, const rigid_transform & b, float t) { return {slerp(a.rotation, b.rotation, t), lerp(a.translation, b.translation, t)}; }
inline rigid_transform nlerp(const rigid_transform & a, const rigid_transform & b, float t) { return {nlerp(a.rotation, b.rotation, t), lerp(a.translation, b.translation, t)}; }

// Convert from a normalized right-down-forward direction vector to right-down texcoords, with the forward vector centered at 0.5,0.5
inline float2 compute_sphere_texcoords(float3 direction) { return float2{std::atan2(direction.x, direction.z)*0.1591549f, std::asin(direction.y)*0.3183099f}+0.5f; }