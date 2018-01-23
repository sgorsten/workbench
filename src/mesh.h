#pragma once
#include "geometry.h"

struct mesh_vertex
{
    float3 position, color, normal; float2 texcoord;
};

struct mesh
{
    std::vector<mesh_vertex> vertices;
    std::vector<int2> lines;
    std::vector<int3> triangles;
};

mesh make_basis_mesh();
mesh make_box_mesh(const float3 & color, const float3 & a, const float3 & b);
mesh make_sphere_mesh(int slices, int stacks, float radius);
mesh make_quad_mesh(const float3 & color, const float3 & tangent_s, const float3 & tangent_t);
