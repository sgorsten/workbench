// This module will eventually be responsible for logically stateless named resources that can be shared between many objects.
#pragma once
#include "mesh.h"
#include "graphics.h"

struct mesh_asset
{
    std::string name;
    mesh cmesh;
    gfx::simple_mesh gmesh;

    std::optional<ray_mesh_hit> raycast(const ray & r) const;
};

struct texture_asset
{
    std::string name;
    bool linear;
    rhi::ptr<rhi::image> gtex;
};

struct material_asset
{
    std::string name;
    std::vector<std::string> texture_names;
    rhi::ptr<const rhi::pipeline> pipe;
};
