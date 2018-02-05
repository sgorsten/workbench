#include "mesh.h"
#include "core.h"

void mesh::compute_normals()
{
    for(auto & v : vertices) v.normal = float3();
    for(auto & t : triangles)
    {
        auto & v0 = vertices[t[0]], & v1 = vertices[t[1]], & v2 = vertices[t[2]];
        const float3 n = cross(v1.position - v0.position, v2.position - v0.position);
        v0.normal += n; v1.normal += n; v2.normal += n;
    }
    for(auto & v : vertices) v.normal = normalize(v.normal);
}

void mesh::compute_tangents()
{
    for(auto & v : vertices) v.tangent = v.bitangent = float3();
    for(auto & t : triangles)
    {
        auto & v0 = vertices[t[0]], & v1 = vertices[t[1]], & v2 = vertices[t[2]];
        const float3 e1 = v1.position - v0.position, e2 = v2.position - v0.position;
        const float2 d1 = v1.texcoord - v0.texcoord, d2 = v2.texcoord - v0.texcoord;
        const float3 dpds = float3(d2.y * e1.x - d1.y * e2.x, d2.y * e1.y - d1.y * e2.y, d2.y * e1.z - d1.y * e2.z) / cross(d1, d2);
        const float3 dpdt = float3(d1.x * e2.x - d2.x * e1.x, d1.x * e2.y - d2.x * e1.y, d1.x * e2.z - d2.x * e1.z) / cross(d1, d2);
        v0.tangent += dpds; v1.tangent += dpds; v2.tangent += dpds;
        v0.bitangent += dpdt; v1.bitangent += dpdt; v2.bitangent += dpdt;
    }
    for(auto & v : vertices)
    {
        v.tangent = normalize(v.tangent);
        v.bitangent = normalize(v.bitangent);
    }
}

mesh make_box_mesh(const float3 & a, const float3 & b)
{
    mesh m;
    m.vertices = {
        {{a.x,a.y,a.z}, {-1,0,0}, {0,0}}, {{a.x,a.y,b.z}, {-1,0,0}, {0,1}}, {{a.x,b.y,b.z}, {-1,0,0}, {1,1}}, {{a.x,b.y,a.z}, {-1,0,0}, {1,0}},
        {{b.x,b.y,a.z}, {+1,0,0}, {0,0}}, {{b.x,b.y,b.z}, {+1,0,0}, {0,1}}, {{b.x,a.y,b.z}, {+1,0,0}, {1,1}}, {{b.x,a.y,a.z}, {+1,0,0}, {1,0}},
        {{a.x,a.y,a.z}, {0,-1,0}, {0,0}}, {{b.x,a.y,a.z}, {0,-1,0}, {0,1}}, {{b.x,a.y,b.z}, {0,-1,0}, {1,1}}, {{a.x,a.y,b.z}, {0,-1,0}, {1,0}},
        {{a.x,b.y,b.z}, {0,+1,0}, {0,0}}, {{b.x,b.y,b.z}, {0,+1,0}, {0,1}}, {{b.x,b.y,a.z}, {0,+1,0}, {1,1}}, {{a.x,b.y,a.z}, {0,+1,0}, {1,0}},
        {{a.x,a.y,a.z}, {0,0,-1}, {0,0}}, {{a.x,b.y,a.z}, {0,0,-1}, {0,1}}, {{b.x,b.y,a.z}, {0,0,-1}, {1,1}}, {{b.x,a.y,a.z}, {0,0,-1}, {1,0}},
        {{b.x,a.y,b.z}, {0,0,+1}, {0,0}}, {{b.x,b.y,b.z}, {0,0,+1}, {0,1}}, {{a.x,b.y,b.z}, {0,0,+1}, {1,1}}, {{a.x,a.y,b.z}, {0,0,+1}, {1,0}},
    };
    m.triangles = {{0,1,2},{0,2,3},{4,5,6},{4,6,7},{8,9,10},{8,10,11},{12,13,14},{12,14,15},{16,17,18},{16,18,19},{20,21,22},{20,22,23}};
    m.compute_tangents();
    return m;
}

mesh make_sphere_mesh(int slices, int stacks, float radius)
{
    mesh m;
    for(int i=0; i<=slices; ++i)
    {
        for(int j=0; j<=stacks; ++j)
        {
            const float tau = 6.2831853f, longitude = (i%slices)*tau/slices, latitude = (j-(stacks*0.5f))*tau/2/stacks;
            const float3 normal {cos(longitude)*cos(latitude), sin(latitude), sin(longitude)*cos(latitude)}; // Poles at +/-y
            m.vertices.push_back({normal*radius, normal, {(float)i/slices, (float)j/stacks}});

            if(i>0 && j>0)
            {
                int i0 = (i-1)*(stacks+1)+j-1;
                int i1 = (i-1)*(stacks+1)+j-0;
                int i2 = (i-0)*(stacks+1)+j-0;
                int i3 = (i-0)*(stacks+1)+j-1;
                m.triangles.push_back(int3(i0,i1,i2));
                m.triangles.push_back(int3(i0,i2,i3));
            }
        }
    }
    m.compute_tangents();
    for(int j=0; j<=stacks; ++j)
    {
        auto & v0 = m.vertices[j], & v1 = m.vertices[slices*(stacks+1)+j];
        v0.tangent = v1.tangent = normalize(v0.tangent + v1.tangent);
        v0.bitangent = v1.bitangent = normalize(v0.bitangent + v1.bitangent);
    }
    return m;
}

mesh make_quad_mesh(const float3 & tangent_s, const float3 & tangent_t)
{
    const auto normal = normalize(cross(tangent_s, tangent_t));
    mesh m;
    m.vertices =
    {
        {-tangent_s-tangent_t, normal, {0,0}},
        {+tangent_s-tangent_t, normal, {1,0}},
        {+tangent_s+tangent_t, normal, {1,1}},
        {-tangent_s+tangent_t, normal, {0,1}}
    };
    m.triangles = {{0,1,2},{0,2,3}};
    m.compute_tangents();
    return m;
}

mesh make_lathed_mesh(const float3 & axis, const float3 & arm1, const float3 & arm2, int slices, array_view<float2> points)
{
    mesh mesh;
    for(int i=0; i<slices; ++i)
    {
        const float angle = i*tau/slices;
        const float3x2 mat {axis, arm1 * std::cos(angle) + arm2 * std::sin(angle)};
        for(auto & p : points) mesh.vertices.push_back({mul(mat,p), {1,1,1}});
        for(size_t j = 1; j < points.size(); ++j)
        {
            if(points[j-1] == points[j]) continue;
            const int ii = (i+1)%slices;
            const int i0 = i*points.size() + (j-1);
            const int i1 = ii*points.size() + (j-1);
            const int i2 = ii*points.size() + (j-0);
            const int i3 = i*points.size() + (j-0);
            mesh.triangles.push_back({i0,i1,i2});
            mesh.triangles.push_back({i0,i2,i3});
        }
    }
    mesh.compute_normals();
    return mesh;
}