#include "pbr.h"
#include <string>

standard_shaders standard_shaders::compile(shader_compiler & compiler)
{
    standard_shaders standard;
    standard.render_image_vertex_shader = compiler.compile_file(shader_stage::vertex, "../../assets/render-image.vert");
    standard.compute_brdf_integral_image_fragment_shader = compiler.compile_file(shader_stage::fragment, "../../assets/compute-brdf-integral-image.frag");
    standard.render_cubemap_vertex_shader = compiler.compile_file(shader_stage::vertex, "../../assets/render-cubemap.vert");
    standard.copy_cubemap_from_spheremap_fragment_shader = compiler.compile_file(shader_stage::fragment, "../../assets/copy-cubemap-from-spheremap.frag");
    standard.compute_irradiance_cubemap_fragment_shader = compiler.compile_file(shader_stage::fragment, "../../assets/compute-irradiance-cubemap.frag");
    standard.compute_reflectance_cubemap_fragment_shader = compiler.compile_file(shader_stage::fragment, "../../assets/compute-reflectance-cubemap.frag");
    return standard;
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

    render_image_vertex_shader = dev->create_shader(standard.render_image_vertex_shader);
    auto compute_brdf_integral_image_fragment_shader = dev->create_shader(standard.compute_brdf_integral_image_fragment_shader);

    render_cubemap_vertex_shader = dev->create_shader(standard.render_cubemap_vertex_shader);
    auto copy_cubemap_from_spheremap_fragment_shader = dev->create_shader(standard.copy_cubemap_from_spheremap_fragment_shader);
    auto compute_irradiance_cubemap_fragment_shader = dev->create_shader(standard.compute_irradiance_cubemap_fragment_shader);
    auto compute_reflectance_cubemap_fragment_shader = dev->create_shader(standard.compute_reflectance_cubemap_fragment_shader);

    op_set_layout = dev->create_descriptor_set_layout({
        {0, rhi::descriptor_type::uniform_buffer, 1},
        {1, rhi::descriptor_type::combined_image_sampler, 1}
    });
    op_pipeline_layout = dev->create_pipeline_layout({op_set_layout});
    empty_pipeline_layout = dev->create_pipeline_layout({});

    compute_brdf_integral_image_pipeline = create_image_pipeline(*empty_pipeline_layout, *compute_brdf_integral_image_fragment_shader);
    copy_cubemap_from_spheremap_pipeline = create_cubemap_pipeline(*op_pipeline_layout, *copy_cubemap_from_spheremap_fragment_shader);
    compute_irradiance_cubemap_pipeline = create_cubemap_pipeline(*op_pipeline_layout, *compute_irradiance_cubemap_fragment_shader);
    compute_reflectance_cubemap_pipeline = create_cubemap_pipeline(*op_pipeline_layout, *compute_reflectance_cubemap_fragment_shader);    
}

standard_device_objects::~standard_device_objects()
{

}
