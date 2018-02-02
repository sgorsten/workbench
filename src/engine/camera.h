#pragma once
#include "geometry.h"
#include "grid.h"

// A viewpoint in space from which the scene will be viewed
struct camera
{
    coord_system coords;
    float3 position;
    float pitch, yaw;

    pure_rotation get_orientation() const { return mul(pure_rotation(coords.cross(coord_axis::forward, coord_axis::right), yaw), pure_rotation(coords.cross(coord_axis::forward, coord_axis::down), pitch)); }
    float3 get_direction(coord_axis axis) const { return transform_direction(get_orientation(), coords(axis)); }

    rigid_transform get_pose() const { return {get_orientation(), position}; }
    float4x4 get_view_matrix() const { return get_inverse_transform_matrix(get_pose()); }
    float4x4 get_skybox_view_matrix() const { return get_inverse_transform_matrix(get_orientation()); }

    float4x4 get_proj_matrix(float aspect, const coord_system & ndc_coords, linalg::z_range z_range) const { return mul(linalg::perspective_matrix(1.0f, aspect, 0.1f, 100.0f, linalg::pos_z, z_range), get_transform_matrix(coord_transform{coords, ndc_coords})); }
    float4x4 get_view_proj_matrix(float aspect, const coord_system & ndc_coords, linalg::z_range z_range) const { return mul(get_proj_matrix(aspect, ndc_coords, z_range), get_view_matrix()); }
    float4x4 get_skybox_view_proj_matrix(float aspect, const coord_system & ndc_coords, linalg::z_range z_range) const { return mul(get_proj_matrix(aspect, ndc_coords, z_range), get_skybox_view_matrix()); }

    void move(coord_axis direction, float distance) { position += get_direction(direction) * distance; }

    ray camera::get_ray_from_pixel(const int2 & pixel, const rect<int> & viewport) const
    {
        const coord_system pixel_coords {coord_axis::right, coord_axis::down, coord_axis::forward};
        const float4x4 inv_view_proj = inverse(get_view_proj_matrix(viewport.aspect_ratio(), pixel_coords, linalg::zero_to_one));
        const float2 p = float2(pixel - viewport.corner00())/float2(viewport.dims()) * 2.0f - 1.0f;       
        const float4 p0 = mul(inv_view_proj, float4(p,0,1)), p1 = mul(inv_view_proj, float4(p,1,1));
        return {position, p1.xyz()*p0.w - p0.xyz()*p1.w};
    }
};