#pragma once
#include "graphics.h"

struct render_image_vertex
{
    float2 position;
    float2 texcoord;
    static rhi::vertex_binding_desc get_binding(int index)
    {
        return {index, sizeof(render_image_vertex), {
            {0, rhi::attribute_format::float2, offsetof(render_image_vertex, position)},
            {1, rhi::attribute_format::float2, offsetof(render_image_vertex, texcoord)}
        }};
    }
};

struct render_cubemap_vertex 
{ 
    float2 position; 
    float3 direction;
    static rhi::vertex_binding_desc get_binding(int index)
    {
        return {index, sizeof(render_cubemap_vertex), {
            {0, rhi::attribute_format::float2, offsetof(render_cubemap_vertex, position)},
            {1, rhi::attribute_format::float3, offsetof(render_cubemap_vertex, direction)}
        }};
    }
};

struct standard_shaders
{
    // Standard image operations
    shader_module render_image_vertex_shader;
    shader_module compute_brdf_integral_image_fragment_shader;

    // Standard cubemap operations
    shader_module render_cubemap_vertex_shader;
    shader_module copy_cubemap_from_spheremap_fragment_shader;
    shader_module compute_irradiance_cubemap_fragment_shader;
    shader_module compute_reflectance_cubemap_fragment_shader;

    static standard_shaders compile(shader_compiler & compiler);
};

struct standard_device_objects
{
    std::shared_ptr<rhi::device> dev;
    rhi::sampler spheremap_sampler;
    rhi::sampler cubemap_sampler;

    rhi::buffer render_image_vertex_buffer;
    rhi::shader render_image_vertex_shader;
    rhi::shader compute_brdf_integral_image_fragment_shader;

    rhi::buffer render_cubemap_vertex_buffer;
    rhi::shader render_cubemap_vertex_shader;
    rhi::shader copy_cubemap_from_spheremap_fragment_shader;
    rhi::shader compute_irradiance_cubemap_fragment_shader;
    rhi::shader compute_reflectance_cubemap_fragment_shader;

    rhi::descriptor_set_layout op_set_layout;
    rhi::pipeline_layout op_pipeline_layout, empty_pipeline_layout;
    rhi::render_pass render_to_rg_float16_pass;
    rhi::render_pass render_to_rgba_float16_pass;
    rhi::pipeline compute_brdf_integral_image_pipeline;
    rhi::pipeline copy_cubemap_from_spheremap_pipeline;
    rhi::pipeline compute_irradiance_cubemap_pipeline;
    rhi::pipeline compute_reflectance_cubemap_pipeline;

    standard_device_objects(std::shared_ptr<rhi::device> dev, const standard_shaders & standard);
    ~standard_device_objects();

    rhi::pipeline create_image_pipeline(rhi::render_pass render_pass, rhi::pipeline_layout pipeline_layout, rhi::shader fragment_shader)
    {
        return dev->create_pipeline({render_pass, pipeline_layout, {render_image_vertex::get_binding(0)}, {render_image_vertex_shader, fragment_shader}, rhi::primitive_topology::triangles, rhi::front_face::clockwise, rhi::cull_mode::none, std::nullopt, false});
    }

    rhi::pipeline create_cubemap_pipeline(rhi::render_pass render_pass, rhi::pipeline_layout pipeline_layout, rhi::shader fragment_shader)
    {
        return dev->create_pipeline({render_pass, pipeline_layout, {render_cubemap_vertex::get_binding(0)}, {render_cubemap_vertex_shader, fragment_shader}, rhi::primitive_topology::triangles, rhi::front_face::clockwise, rhi::cull_mode::none, std::nullopt, false});
    }

    template<class F> void render_to_image(rhi::command_pool pool, rhi::image target_image, int mip, const int2 & dimensions, rhi::render_pass render_pass, bool generate_mips, F bind_pipeline)
    {
        std::vector<rhi::framebuffer> framebuffers;
        gfx::command_buffer cmd {*dev, dev->start_command_buffer(pool)};
        rhi::framebuffer fb = dev->create_framebuffer({dimensions, render_pass, {{target_image,mip,0}}});
        cmd.begin_render_pass(render_pass, fb, {{0,0,0,0},1,0});
        bind_pipeline(cmd);
        cmd.bind_vertex_buffer(0, {render_image_vertex_buffer, 0, sizeof(render_image_vertex)*6});
        cmd.draw(0, 6);
        cmd.end_render_pass();
        framebuffers.push_back(fb);
        if(generate_mips) dev->generate_mipmaps(cmd.cmd, target_image);
        dev->submit_and_wait(cmd.cmd, {});
        dev->wait_idle();
        for(auto fb : framebuffers) dev->destroy_framebuffer(fb);
    }

    rhi::image create_brdf_integral_image(rhi::command_pool cmd_pool, gfx::descriptor_pool & desc_pool, gfx::dynamic_buffer & uniform_buffer)
    {
        auto target = dev->create_image({rhi::image_shape::_2d, {512,512,1}, 1, rhi::image_format::rg_float16, rhi::sampled_image_bit|rhi::color_attachment_bit}, {});
        render_to_image(cmd_pool, target, 0, {512,512}, render_to_rg_float16_pass, false, [&](gfx::command_buffer & cmd)
        {
            cmd.bind_pipeline(compute_brdf_integral_image_pipeline);
        });
        return target;
    }

    template<class F> void render_to_cubemap(rhi::command_pool pool, rhi::image target_cube_map, int mip, const int2 & dimensions, rhi::render_pass render_pass, bool generate_mips, F bind_pipeline)
    {
        std::vector<rhi::framebuffer> framebuffers;
        gfx::command_buffer cmd {*dev, dev->start_command_buffer(pool)};
        for(int i=0; i<6; ++i)
        {
            rhi::framebuffer fb = dev->create_framebuffer({dimensions, render_pass, {{target_cube_map,mip,i}}});
            cmd.begin_render_pass(render_pass, fb, {{0,0,0,0},1,0});
            bind_pipeline(cmd);
            cmd.bind_vertex_buffer(0, {render_cubemap_vertex_buffer, exactly(sizeof(render_cubemap_vertex)*6*i), sizeof(render_cubemap_vertex)*6});
            cmd.draw(0, 6);
            cmd.end_render_pass();
            framebuffers.push_back(fb);
        }
        if(generate_mips) dev->generate_mipmaps(cmd.cmd, target_cube_map);
        dev->submit_and_wait(cmd.cmd, {});
        dev->wait_idle();
        for(auto fb : framebuffers) dev->destroy_framebuffer(fb);
    }

    rhi::image create_cubemap_from_spheremap(int width, rhi::command_pool cmd_pool, gfx::descriptor_pool & desc_pool, gfx::dynamic_buffer & uniform_buffer, rhi::image spheremap, const coord_system & preferred_coords)
    {
        auto target = dev->create_image({rhi::image_shape::cube, {width,width,1}, exactly(std::ceil(std::log2(width)+1)), rhi::image_format::rgba_float16, rhi::sampled_image_bit|rhi::color_attachment_bit}, {});
        auto set = desc_pool.alloc(op_set_layout);
        set.write(0, uniform_buffer, make_transform_4x4(preferred_coords, {coord_axis::right, coord_axis::down, coord_axis::forward}));
        set.write(1, spheremap_sampler, spheremap);
        render_to_cubemap(cmd_pool, target, 0, {width,width}, render_to_rgba_float16_pass, true, [&](gfx::command_buffer & cmd)
        {
            cmd.bind_pipeline(copy_cubemap_from_spheremap_pipeline);
            cmd.bind_descriptor_set(op_pipeline_layout, 0, set);
        });
        return target;
    }

    rhi::image create_irradiance_cubemap(int width, rhi::command_pool cmd_pool, gfx::descriptor_pool & desc_pool, gfx::dynamic_buffer & uniform_buffer, rhi::image cubemap)
    {
        auto target = dev->create_image({rhi::image_shape::cube, {width,width,1}, 1, rhi::image_format::rgba_float16, rhi::sampled_image_bit|rhi::color_attachment_bit}, {});
        struct {} empty;
        auto set = desc_pool.alloc(op_set_layout);
        set.write(0, uniform_buffer, empty);
        set.write(1, cubemap_sampler, cubemap);
        render_to_cubemap(cmd_pool, target, 0, {width,width}, render_to_rgba_float16_pass, false, [&](gfx::command_buffer & cmd)
        {
            cmd.bind_pipeline(compute_irradiance_cubemap_pipeline);
            cmd.bind_descriptor_set(op_pipeline_layout, 0, set);  
        });
        return target;
    }

    rhi::image create_reflectance_cubemap(int width, rhi::command_pool cmd_pool, gfx::descriptor_pool & desc_pool, gfx::dynamic_buffer & uniform_buffer, rhi::image cubemap)
    {
        auto target = dev->create_image({rhi::image_shape::cube, {width,width,1}, 5, rhi::image_format::rgba_float16, rhi::sampled_image_bit|rhi::color_attachment_bit}, {});
        for(int mip=0; mip<5; ++mip)
        {
            const float roughness = mip/4.0f;
            auto set = desc_pool.alloc(op_set_layout);
            set.write(0, uniform_buffer, roughness);
            set.write(1, cubemap_sampler, cubemap);
            render_to_cubemap(cmd_pool, target, mip, {width,width}, render_to_rgba_float16_pass, false, [&](gfx::command_buffer & cmd)
            {
                cmd.bind_pipeline(compute_reflectance_cubemap_pipeline);
                cmd.bind_descriptor_set(op_pipeline_layout, 0, set);  
            });
            width /= 2;
        }
        return target;    
    }
};

extern const std::string preamble;
extern const std::string pbr_lighting;
