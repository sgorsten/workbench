#pragma once
#include "graphics.h"
#include "shader.h"

namespace pbr
{
    struct shaders
    {
        // Standard image operations
        rhi::shader_desc render_image_vertex_shader;
        rhi::shader_desc compute_brdf_integral_image_fragment_shader;

        // Standard cubemap operations
        rhi::shader_desc render_cubemap_vertex_shader;
        rhi::shader_desc copy_cubemap_from_spheremap_fragment_shader;
        rhi::shader_desc compute_irradiance_cubemap_fragment_shader;
        rhi::shader_desc compute_reflectance_cubemap_fragment_shader;

        static shaders compile(shader_compiler & compiler);
    };

    struct environment_map { rhi::ptr<rhi::image> environment_cubemap, irradiance_cubemap, reflectance_cubemap; };
    class device_objects
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
        rhi::ptr<rhi::image> create_cubemap_from_spheremap(gfx::transient_resource_pool & pool, int width, rhi::image & spheremap, const coord_system & preferred_coords);
        rhi::ptr<rhi::image> create_irradiance_cubemap(gfx::transient_resource_pool & pool, int width, rhi::image & cubemap);
        rhi::ptr<rhi::image> create_reflectance_cubemap(gfx::transient_resource_pool & pool, int width, rhi::image & cubemap);
    public:
        device_objects() = default;
        device_objects(rhi::ptr<rhi::device> dev, const shaders & standard);

        rhi::sampler & get_image_sampler() { return *image_sampler; }
        rhi::sampler & get_cubemap_sampler() { return *cubemap_sampler; }
        rhi::image & get_brdf_integral_image() { return *brdf_integral_image; }
    
        environment_map create_environment_map_from_cubemap(gfx::transient_resource_pool & pool, rhi::image & cubemap);
        environment_map create_environment_map_from_spheremap(gfx::transient_resource_pool & pool, rhi::image & spheremap, int width, const coord_system & preferred_coords);
    };

    // Descriptor set indices and uniform buffer layouts
    constexpr int scene_set_index = 0;
    constexpr int view_set_index = 1;
    constexpr int material_set_index = 2;
    constexpr int object_set_index = 3;

    struct scene_uniforms
    {
        struct point_light { alignas(16) float3 position, light; };
        point_light point_lights[4];
    };

    struct view_uniforms
    {
        alignas(16) float4x4 view_proj_matrix;
        alignas(16) float4x4 skybox_view_proj_matrix;
        alignas(16) float3 eye_position;
        alignas(16) float3 right_vector;
        alignas(16) float3 down_vector;
    };

    struct material_uniforms
    {
        alignas(16) float3 albedo_tint;
        alignas(4) float roughness, metalness;
    };

    struct object_uniforms
    {
        alignas(16) float4x4 model_matrix;
        alignas(16) float4x4 model_matrix_it;
        object_uniforms(float4x4 model_matrix) : model_matrix{model_matrix}, model_matrix_it{inverse(transpose(model_matrix))} {}
    };
}