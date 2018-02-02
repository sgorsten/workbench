#include "asset.h"

std::optional<ray_mesh_hit> mesh_asset::raycast(const ray & r) const
{
    std::optional<ray_mesh_hit> result;
    for(size_t i=0; i<cmesh.triangles.size(); ++i)
    {
        auto [i0, i1, i2] = cmesh.triangles[i];
        if(auto hit = intersect_ray_triangle(r, cmesh.vertices[i0].position, cmesh.vertices[i1].position, cmesh.vertices[i2].position); hit && (!result || hit->t < result->t))
        {
            result = {hit->t, i, hit->uv};
        }
    }
    return result;
}
