#pragma once
#include "graphics.h"
#include "shader.h"

struct render_image_vertex
{
    float2 position;
    float2 texcoord;
    static rhi::vertex_binding_desc get_binding(int index)
    {
        return gfx::vertex_binder<render_image_vertex>(0)
            .attribute(0, &render_image_vertex::position)
            .attribute(1, &render_image_vertex::texcoord);
    }
};

struct render_cubemap_vertex 
{ 
    float2 position; 
    float3 direction;
    static rhi::vertex_binding_desc get_binding(int index)
    {
        return gfx::vertex_binder<render_cubemap_vertex>(0)
            .attribute(0, &render_cubemap_vertex::position)
            .attribute(1, &render_cubemap_vertex::direction);
    }
};

struct standard_shaders
{
    // Standard image operations
    rhi::shader_desc render_image_vertex_shader;
    rhi::shader_desc compute_brdf_integral_image_fragment_shader;

    // Standard cubemap operations
    rhi::shader_desc render_cubemap_vertex_shader;
    rhi::shader_desc copy_cubemap_from_spheremap_fragment_shader;
    rhi::shader_desc compute_irradiance_cubemap_fragment_shader;
    rhi::shader_desc compute_reflectance_cubemap_fragment_shader;

    static standard_shaders compile(shader_compiler & compiler);
};

struct environment_map { rhi::ptr<rhi::image> environment_cubemap, irradiance_cubemap, reflectance_cubemap; };
class standard_device_objects
{
    rhi::ptr<rhi::device> dev;
    rhi::ptr<rhi::buffer> render_image_vertex_buffer, render_cubemap_vertex_buffer;
    rhi::ptr<rhi::sampler> image_sampler, spheremap_sampler, cubemap_sampler;
    rhi::ptr<rhi::image> brdf_integral_image;
    rhi::ptr<rhi::descriptor_set_layout> op_set_layout;
    rhi::ptr<rhi::pipeline_layout> op_pipeline_layout;
    rhi::ptr<rhi::pipeline> copy_cubemap_from_spheremap_pipeline;
    rhi::ptr<rhi::pipeline> compute_irradiance_cubemap_pipeline;
    rhi::ptr<rhi::pipeline> compute_reflectance_cubemap_pipeline;

    template<class F> void render_to_image(rhi::image & target_image, int mip, const int2 & dimensions, bool generate_mips, F bind_pipeline);
    template<class F> void render_to_cubemap(rhi::image & target_cube_map, int mip, const int2 & dimensions, bool generate_mips, F bind_pipeline);
    rhi::ptr<rhi::image> create_cubemap_from_spheremap(int width, rhi::descriptor_pool & desc_pool, gfx::dynamic_buffer & uniform_buffer, rhi::image & spheremap, const coord_system & preferred_coords);
    rhi::ptr<rhi::image> create_irradiance_cubemap(int width, rhi::descriptor_pool & desc_pool, gfx::dynamic_buffer & uniform_buffer, rhi::image & cubemap);
    rhi::ptr<rhi::image> create_reflectance_cubemap(int width, rhi::descriptor_pool & desc_pool, gfx::dynamic_buffer & uniform_buffer, rhi::image & cubemap);
public:
    standard_device_objects(rhi::ptr<rhi::device> dev, const standard_shaders & standard);

    rhi::sampler & get_image_sampler() { return *image_sampler; }
    rhi::sampler & get_cubemap_sampler() { return *cubemap_sampler; }
    rhi::image & get_brdf_integral_image() { return *brdf_integral_image; }
    
    environment_map create_environment_map_from_cubemap(rhi::descriptor_pool & desc_pool, gfx::dynamic_buffer & uniform_buffer, rhi::image & cubemap);
    environment_map create_environment_map_from_spheremap(rhi::descriptor_pool & desc_pool, gfx::dynamic_buffer & uniform_buffer, rhi::image & spheremap, int width, const coord_system & preferred_coords);
};

constexpr int pbr_per_scene_set_index = 0;
constexpr int pbr_per_view_set_index = 1;
constexpr int pbr_per_object_set_index = 2;

struct pbr_per_scene_uniforms
{
    struct point_light { alignas(16) float3 position, light; };
    point_light point_lights[4];
};

struct pbr_per_view_uniforms
{
    alignas(16) float4x4 view_proj_matrix;
    alignas(16) float4x4 skybox_view_proj_matrix;
    alignas(16) float3 eye_position;
    alignas(16) float3 right_vector;
    alignas(16) float3 down_vector;
};