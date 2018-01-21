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
    rhi::ptr<rhi::device> dev;

    rhi::ptr<rhi::sampler> image_sampler;
    rhi::ptr<rhi::sampler> spheremap_sampler;
    rhi::ptr<rhi::sampler> cubemap_sampler;

    rhi::ptr<rhi::buffer> render_image_vertex_buffer;
    rhi::shader render_image_vertex_shader;
    rhi::shader compute_brdf_integral_image_fragment_shader;

    rhi::ptr<rhi::buffer> render_cubemap_vertex_buffer;
    rhi::shader render_cubemap_vertex_shader;
    rhi::shader copy_cubemap_from_spheremap_fragment_shader;
    rhi::shader compute_irradiance_cubemap_fragment_shader;
    rhi::shader compute_reflectance_cubemap_fragment_shader;

    rhi::descriptor_set_layout op_set_layout;
    rhi::pipeline_layout op_pipeline_layout, empty_pipeline_layout;
    rhi::pipeline compute_brdf_integral_image_pipeline;
    rhi::pipeline copy_cubemap_from_spheremap_pipeline;
    rhi::pipeline compute_irradiance_cubemap_pipeline;
    rhi::pipeline compute_reflectance_cubemap_pipeline;

    standard_device_objects(rhi::ptr<rhi::device> dev, const standard_shaders & standard);
    ~standard_device_objects();

    rhi::pipeline create_image_pipeline(rhi::pipeline_layout pipeline_layout, rhi::shader fragment_shader)
    {
        return dev->create_pipeline({pipeline_layout, {render_image_vertex::get_binding(0)}, {render_image_vertex_shader, fragment_shader}, rhi::primitive_topology::triangles, rhi::front_face::clockwise, rhi::cull_mode::none, std::nullopt, false});
    }

    rhi::pipeline create_cubemap_pipeline(rhi::pipeline_layout pipeline_layout, rhi::shader fragment_shader)
    {
        return dev->create_pipeline({pipeline_layout, {render_cubemap_vertex::get_binding(0)}, {render_cubemap_vertex_shader, fragment_shader}, rhi::primitive_topology::triangles, rhi::front_face::clockwise, rhi::cull_mode::none, std::nullopt, false});
    }

    template<class F> void render_to_image(rhi::image & target_image, int mip, const int2 & dimensions, bool generate_mips, F bind_pipeline)
    {
        auto fb = dev->create_framebuffer({dimensions, {{&target_image,mip,0}}});
        auto cmd = dev->start_command_buffer();
        rhi::render_pass_desc pass;
        pass.color_attachments = {{rhi::dont_care{}, rhi::store{rhi::layout::shader_read_only_optimal}}};
        cmd->begin_render_pass(pass, *fb);
        bind_pipeline(*cmd);
        cmd->bind_vertex_buffer(0, {render_image_vertex_buffer, 0, sizeof(render_image_vertex)*6});
        cmd->draw(0, 6);
        cmd->end_render_pass();
        if(generate_mips) cmd->generate_mipmaps(target_image);
        dev->wait_until_complete(dev->submit(*cmd));
    }

    rhi::ptr<rhi::image> create_brdf_integral_image(gfx::descriptor_pool & desc_pool, gfx::dynamic_buffer & uniform_buffer)
    {
        auto target = dev->create_image({rhi::image_shape::_2d, {512,512,1}, 1, rhi::image_format::rg_float16, rhi::sampled_image_bit|rhi::color_attachment_bit}, {});
        render_to_image(*target, 0, {512,512}, false, [&](rhi::command_buffer & cmd)
        {
            cmd.bind_pipeline(compute_brdf_integral_image_pipeline);
        });
        return target;
    }

    template<class F> void render_to_cubemap(rhi::image & target_cube_map, int mip, const int2 & dimensions, bool generate_mips, F bind_pipeline)
    {
        std::vector<rhi::ptr<rhi::framebuffer>> framebuffers;
        auto cmd = dev->start_command_buffer();
        for(int i=0; i<6; ++i)
        {
            auto fb = dev->create_framebuffer({dimensions, {{&target_cube_map,mip,i}}});
            rhi::render_pass_desc pass;
            pass.color_attachments = {{rhi::dont_care{}, rhi::store{rhi::layout::shader_read_only_optimal}}};
            cmd->begin_render_pass(pass, *fb);
            bind_pipeline(*cmd);
            cmd->bind_vertex_buffer(0, {render_cubemap_vertex_buffer, exactly(sizeof(render_cubemap_vertex)*6*i), sizeof(render_cubemap_vertex)*6});
            cmd->draw(0, 6);
            cmd->end_render_pass();
            framebuffers.push_back(fb);
        }
        if(generate_mips) cmd->generate_mipmaps(target_cube_map);
        dev->wait_until_complete(dev->submit(*cmd));
    }

    rhi::ptr<rhi::image> create_cubemap_from_spheremap(int width, gfx::descriptor_pool & desc_pool, gfx::dynamic_buffer & uniform_buffer, rhi::image & spheremap, const coord_system & preferred_coords)
    {
        auto target = dev->create_image({rhi::image_shape::cube, {width,width,1}, exactly(std::ceil(std::log2(width)+1)), rhi::image_format::rgba_float16, rhi::sampled_image_bit|rhi::color_attachment_bit}, {});
        auto set = desc_pool.alloc(op_set_layout);
        set.write(0, uniform_buffer, make_transform_4x4(preferred_coords, {coord_axis::right, coord_axis::down, coord_axis::forward}));
        set.write(1, *spheremap_sampler, spheremap);
        render_to_cubemap(*target, 0, {width,width}, true, [&](rhi::command_buffer & cmd)
        {
            cmd.bind_pipeline(copy_cubemap_from_spheremap_pipeline);
            cmd.bind_descriptor_set(op_pipeline_layout, 0, set.set);
        });
        return target;
    }

    rhi::ptr<rhi::image> create_irradiance_cubemap(int width, gfx::descriptor_pool & desc_pool, gfx::dynamic_buffer & uniform_buffer, rhi::image & cubemap)
    {
        auto target = dev->create_image({rhi::image_shape::cube, {width,width,1}, 1, rhi::image_format::rgba_float16, rhi::sampled_image_bit|rhi::color_attachment_bit}, {});
        struct {} empty;
        auto set = desc_pool.alloc(op_set_layout);
        set.write(0, uniform_buffer, empty);
        set.write(1, *cubemap_sampler, cubemap);
        render_to_cubemap(*target, 0, {width,width}, false, [&](rhi::command_buffer & cmd)
        {
            cmd.bind_pipeline(compute_irradiance_cubemap_pipeline);
            cmd.bind_descriptor_set(op_pipeline_layout, 0, set.set);  
        });
        return target;
    }

    rhi::ptr<rhi::image> create_reflectance_cubemap(int width, gfx::descriptor_pool & desc_pool, gfx::dynamic_buffer & uniform_buffer, rhi::image & cubemap)
    {
        auto target = dev->create_image({rhi::image_shape::cube, {width,width,1}, 5, rhi::image_format::rgba_float16, rhi::sampled_image_bit|rhi::color_attachment_bit}, {});
        for(int mip=0; mip<5; ++mip)
        {
            const float roughness = mip/4.0f;
            auto set = desc_pool.alloc(op_set_layout);
            set.write(0, uniform_buffer, roughness);
            set.write(1, *cubemap_sampler, cubemap);
            render_to_cubemap(*target, mip, {width,width}, false, [&](rhi::command_buffer & cmd)
            {
                cmd.bind_pipeline(compute_reflectance_cubemap_pipeline);
                cmd.bind_descriptor_set(op_pipeline_layout, 0, set.set);  
            });
            width /= 2;
        }
        return target;    
    }
};
