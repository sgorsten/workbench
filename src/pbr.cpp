#include "pbr.h"
#include <string>

standard_shaders standard_shaders::compile(shader_compiler & compiler)
{
    standard_shaders standard;
    standard.render_image_vertex_shader = compiler.compile_file(rhi::shader_stage::vertex, "render-image.vert");
    standard.compute_brdf_integral_image_fragment_shader = compiler.compile_file(rhi::shader_stage::fragment, "compute-brdf-integral-image.frag");
    standard.render_cubemap_vertex_shader = compiler.compile_file(rhi::shader_stage::vertex, "render-cubemap.vert");
    standard.copy_cubemap_from_spheremap_fragment_shader = compiler.compile_file(rhi::shader_stage::fragment, "copy-cubemap-from-spheremap.frag");
    standard.compute_irradiance_cubemap_fragment_shader = compiler.compile_file(rhi::shader_stage::fragment, "compute-irradiance-cubemap.frag");
    standard.compute_reflectance_cubemap_fragment_shader = compiler.compile_file(rhi::shader_stage::fragment, "compute-reflectance-cubemap.frag");
    return standard;
}

struct render_image_vertex { float2 position; float2 texcoord; };
struct render_cubemap_vertex { float2 position; float3 direction; };

template<class F> void standard_device_objects::render_to_image(rhi::image & target_image, int mip, const int2 & dimensions, bool generate_mips, F bind_pipeline)
{
    auto fb = dev->create_framebuffer({dimensions, {{&target_image,mip,0}}});
    auto cmd = dev->create_command_buffer();
    rhi::render_pass_desc pass;
    pass.color_attachments = {{rhi::dont_care{}, rhi::store{rhi::layout::shader_read_only_optimal}}};
    cmd->begin_render_pass(pass, *fb);
    bind_pipeline(*cmd);
    cmd->bind_vertex_buffer(0, {*render_image_vertex_buffer, 0, sizeof(render_image_vertex)*6});
    cmd->draw(0, 6);
    cmd->end_render_pass();
    if(generate_mips) cmd->generate_mipmaps(target_image);
    dev->wait_until_complete(dev->submit(*cmd));
}

template<class F> void standard_device_objects::render_to_cubemap(rhi::image & target_cube_map, int mip, const int2 & dimensions, bool generate_mips, F bind_pipeline)
{
    auto cmd = dev->create_command_buffer();
    for(int i=0; i<6; ++i)
    {
        auto fb = dev->create_framebuffer({dimensions, {{&target_cube_map,mip,i}}});
        rhi::render_pass_desc pass;
        pass.color_attachments = {{rhi::dont_care{}, rhi::store{rhi::layout::shader_read_only_optimal}}};
        cmd->begin_render_pass(pass, *fb);
        bind_pipeline(*cmd);
        cmd->bind_vertex_buffer(0, {*render_cubemap_vertex_buffer, exactly(sizeof(render_cubemap_vertex)*6*i), sizeof(render_cubemap_vertex)*6});
        cmd->draw(0, 6);
        cmd->end_render_pass();
    }
    if(generate_mips) cmd->generate_mipmaps(target_cube_map);
    dev->wait_until_complete(dev->submit(*cmd));
}

standard_device_objects::standard_device_objects(rhi::ptr<rhi::device> dev, const standard_shaders & standard) : dev{dev}
{
    image_sampler = dev->create_sampler({rhi::filter::linear, rhi::filter::linear, std::nullopt, rhi::address_mode::clamp_to_edge, rhi::address_mode::clamp_to_edge});
    spheremap_sampler = dev->create_sampler({rhi::filter::linear, rhi::filter::linear, std::nullopt, rhi::address_mode::repeat, rhi::address_mode::clamp_to_edge});
    cubemap_sampler = dev->create_sampler({rhi::filter::linear, rhi::filter::linear, rhi::filter::linear, rhi::address_mode::clamp_to_edge, rhi::address_mode::clamp_to_edge, rhi::address_mode::clamp_to_edge});

    const float y = dev->get_info().inverted_framebuffers ? -1.0f : 1.0f;
    const render_image_vertex image_vertices[] {{{-1,-y},{0,0}}, {{+1,-y},{1,0}}, {{+1,+y},{1,1}}, {{-1,-y},{0,0}}, {{+1,+y},{1,1}}, {{-1,+y},{0,1}}};
    const render_cubemap_vertex cubemap_vertices[]
    {
        {{-1,-y},{+1,+1,+1}}, {{+1,-y},{+1,+1,-1}}, {{+1,+y},{+1,-1,-1}}, {{-1,-y},{+1,+1,+1}}, {{+1,+y},{+1,-1,-1}}, {{-1,+y},{+1,-1,+1}}, // standard positive x face
        {{-1,-y},{-1,+1,-1}}, {{+1,-y},{-1,+1,+1}}, {{+1,+y},{-1,-1,+1}}, {{-1,-y},{-1,+1,-1}}, {{+1,+y},{-1,-1,+1}}, {{-1,+y},{-1,-1,-1}}, // standard negative x face
        {{-1,-y},{-1,+1,-1}}, {{+1,-y},{+1,+1,-1}}, {{+1,+y},{+1,+1,+1}}, {{-1,-y},{-1,+1,-1}}, {{+1,+y},{+1,+1,+1}}, {{-1,+y},{-1,+1,+1}}, // standard positive y face
        {{-1,-y},{-1,-1,+1}}, {{+1,-y},{+1,-1,+1}}, {{+1,+y},{+1,-1,-1}}, {{-1,-y},{-1,-1,+1}}, {{+1,+y},{+1,-1,-1}}, {{-1,+y},{-1,-1,-1}}, // standard negative y face
        {{-1,-y},{-1,+1,+1}}, {{+1,-y},{+1,+1,+1}}, {{+1,+y},{+1,-1,+1}}, {{-1,-y},{-1,+1,+1}}, {{+1,+y},{+1,-1,+1}}, {{-1,+y},{-1,-1,+1}}, // standard positive z face
        {{-1,-y},{+1,+1,-1}}, {{+1,-y},{-1,+1,-1}}, {{+1,+y},{-1,-1,-1}}, {{-1,-y},{+1,+1,-1}}, {{+1,+y},{-1,-1,-1}}, {{-1,+y},{+1,-1,-1}}, // standard negative z face
    };
    render_image_vertex_buffer = dev->create_buffer({sizeof(image_vertices), rhi::buffer_usage::vertex, false}, image_vertices);
    render_cubemap_vertex_buffer = dev->create_buffer({sizeof(cubemap_vertices), rhi::buffer_usage::vertex, false}, cubemap_vertices);

    auto render_image_vertex_shader = dev->create_shader(standard.render_image_vertex_shader);
    auto render_cubemap_vertex_shader = dev->create_shader(standard.render_cubemap_vertex_shader);
    auto compute_brdf_integral_image_fragment_shader = dev->create_shader(standard.compute_brdf_integral_image_fragment_shader);
    auto copy_cubemap_from_spheremap_fragment_shader = dev->create_shader(standard.copy_cubemap_from_spheremap_fragment_shader);
    auto compute_irradiance_cubemap_fragment_shader = dev->create_shader(standard.compute_irradiance_cubemap_fragment_shader);
    auto compute_reflectance_cubemap_fragment_shader = dev->create_shader(standard.compute_reflectance_cubemap_fragment_shader);

    op_set_layout = dev->create_descriptor_set_layout({{0, rhi::descriptor_type::combined_image_sampler, 1}, {1, rhi::descriptor_type::uniform_buffer, 1}});
    op_pipeline_layout = dev->create_pipeline_layout({op_set_layout});
    auto empty_pipeline_layout = dev->create_pipeline_layout({});
    
    const auto image_vertex_binding = gfx::vertex_binder<render_image_vertex>(0).attribute(0, &render_image_vertex::position).attribute(1, &render_image_vertex::texcoord);
    const auto cubemap_vertex_binding = gfx::vertex_binder<render_cubemap_vertex>(0).attribute(0, &render_cubemap_vertex::position).attribute(1, &render_cubemap_vertex::direction);
    const rhi::blend_state opaque {false};
    auto compute_brdf_integral_image_pipeline = dev->create_pipeline({empty_pipeline_layout, {image_vertex_binding}, {render_image_vertex_shader, compute_brdf_integral_image_fragment_shader}, rhi::primitive_topology::triangles, rhi::front_face::clockwise, rhi::cull_mode::none, std::nullopt, false, {opaque}});
    copy_cubemap_from_spheremap_pipeline = dev->create_pipeline({op_pipeline_layout, {cubemap_vertex_binding}, {render_cubemap_vertex_shader, copy_cubemap_from_spheremap_fragment_shader}, rhi::primitive_topology::triangles, rhi::front_face::clockwise, rhi::cull_mode::none, std::nullopt, false, {opaque}});
    compute_irradiance_cubemap_pipeline = dev->create_pipeline({op_pipeline_layout, {cubemap_vertex_binding}, {render_cubemap_vertex_shader, compute_irradiance_cubemap_fragment_shader}, rhi::primitive_topology::triangles, rhi::front_face::clockwise, rhi::cull_mode::none, std::nullopt, false, {opaque}});
    compute_reflectance_cubemap_pipeline = dev->create_pipeline({op_pipeline_layout, {cubemap_vertex_binding}, {render_cubemap_vertex_shader, compute_reflectance_cubemap_fragment_shader}, rhi::primitive_topology::triangles, rhi::front_face::clockwise, rhi::cull_mode::none, std::nullopt, false, {opaque}});

    brdf_integral_image = dev->create_image({rhi::image_shape::_2d, {512,512,1}, 1, rhi::image_format::rg_float16, rhi::sampled_image_bit|rhi::color_attachment_bit}, {});
    render_to_image(*brdf_integral_image, 0, {512,512}, false, [&](rhi::command_buffer & cmd) { cmd.bind_pipeline(*compute_brdf_integral_image_pipeline); });
}

rhi::ptr<rhi::image> standard_device_objects::create_cubemap_from_spheremap(int width, rhi::descriptor_pool & desc_pool, gfx::dynamic_buffer & uniform_buffer, rhi::image & spheremap, const coord_system & preferred_coords)
{
    auto target = dev->create_image({rhi::image_shape::cube, {width,width,1}, exactly(std::ceil(std::log2(width)+1)), rhi::image_format::rgba_float16, rhi::sampled_image_bit|rhi::color_attachment_bit}, {});
    auto set = desc_pool.alloc(*op_set_layout);
    set->write(0, *spheremap_sampler, spheremap);
    set->write(1, uniform_buffer.write(make_transform_4x4(preferred_coords, {coord_axis::right, coord_axis::down, coord_axis::forward})));
    render_to_cubemap(*target, 0, {width,width}, true, [&](rhi::command_buffer & cmd)
    {
        cmd.bind_pipeline(*copy_cubemap_from_spheremap_pipeline);
        cmd.bind_descriptor_set(*op_pipeline_layout, 0, *set);
    });
    return target;
}

rhi::ptr<rhi::image> standard_device_objects::create_irradiance_cubemap(int width, rhi::descriptor_pool & desc_pool, gfx::dynamic_buffer & uniform_buffer, rhi::image & cubemap)
{
    auto target = dev->create_image({rhi::image_shape::cube, {width,width,1}, 1, rhi::image_format::rgba_float16, rhi::sampled_image_bit|rhi::color_attachment_bit}, {});
    auto set = desc_pool.alloc(*op_set_layout);
    set->write(0, *cubemap_sampler, cubemap);
    render_to_cubemap(*target, 0, {width,width}, false, [&](rhi::command_buffer & cmd)
    {
        cmd.bind_pipeline(*compute_irradiance_cubemap_pipeline);
        cmd.bind_descriptor_set(*op_pipeline_layout, 0, *set);  
    });
    return target;
}

rhi::ptr<rhi::image> standard_device_objects::create_reflectance_cubemap(int width, rhi::descriptor_pool & desc_pool, gfx::dynamic_buffer & uniform_buffer, rhi::image & cubemap)
{
    auto target = dev->create_image({rhi::image_shape::cube, {width,width,1}, 5, rhi::image_format::rgba_float16, rhi::sampled_image_bit|rhi::color_attachment_bit}, {});
    for(int mip=0; mip<5; ++mip)
    {
        auto set = desc_pool.alloc(*op_set_layout);
        set->write(0, *cubemap_sampler, cubemap);
        set->write(1, uniform_buffer.write(mip/4.0f));
        render_to_cubemap(*target, mip, {width,width}, false, [&](rhi::command_buffer & cmd)
        {
            cmd.bind_pipeline(*compute_reflectance_cubemap_pipeline);
            cmd.bind_descriptor_set(*op_pipeline_layout, 0, *set);  
        });
        width /= 2;
    }
    return target;    
}

environment_map standard_device_objects::create_environment_map_from_cubemap(rhi::descriptor_pool & desc_pool, gfx::dynamic_buffer & uniform_buffer, rhi::image & cubemap)
{
    return {&cubemap, create_irradiance_cubemap(32, desc_pool, uniform_buffer, cubemap), create_reflectance_cubemap(128, desc_pool, uniform_buffer, cubemap)};
}

environment_map standard_device_objects::create_environment_map_from_spheremap(rhi::descriptor_pool & desc_pool, gfx::dynamic_buffer & uniform_buffer, rhi::image & spheremap, int width, const coord_system & preferred_coords)
{
    auto cubemap = create_cubemap_from_spheremap(width, desc_pool, uniform_buffer, spheremap, preferred_coords);
    return create_environment_map_from_cubemap(desc_pool, uniform_buffer, *cubemap);
}