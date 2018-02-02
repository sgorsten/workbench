#pragma once
#include "transform.h"

/////////////////
// Shape types //
/////////////////

// Shape transformations
struct ray { float3 origin, direction; };

template<class Transform> ray transform(const Transform & t, const ray & ray) { return {transform_point(t, ray.origin), transform_vector(t, ray.direction)}; }
template<class Transform> ray detransform(const Transform & t, const ray & ray) { return {detransform_point(t, ray.origin), detransform_vector(t, ray.direction)}; }

struct ray_triangle_hit { float t; float2 uv; };
struct ray_mesh_hit { float t; size_t triangle; float2 uv; };



// Shape intersection routines
std::optional<float> intersect_ray_plane(const ray & ray, const float4 & plane);
std::optional<ray_triangle_hit> intersect_ray_triangle(const ray & ray, const float3 & v0, const float3 & v1, const float3 & v2);


// Convert from a normalized right-down-forward direction vector to right-down texcoords, with the forward vector centered at 0.5,0.5
inline float2 compute_sphere_texcoords(float3 direction) { return float2{std::atan2(direction.x, direction.z)*0.1591549f, std::asin(direction.y)*0.3183099f}+0.5f; }
