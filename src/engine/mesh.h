#pragma once
#include "geometry.h"

constexpr float tau = 6.283185307179586476925286766559f;

struct mesh_vertex
{
    float3 position, normal; float2 texcoord; float3 tangent, bitangent;
};

struct mesh
{
    std::vector<mesh_vertex> vertices;
    std::vector<int3> triangles;

    void compute_normals();
    void compute_tangents();
};

mesh make_box_mesh(const float3 & a, const float3 & b);
mesh make_sphere_mesh(int slices, int stacks, float radius);
mesh make_quad_mesh(const float3 & tangent_s, const float3 & tangent_t);
mesh make_lathed_mesh(const float3 & axis, const float3 & arm1, const float3 & arm2, int slices, array_view<float2> points);
