#include "engine/shader.h"
#include "engine/sprite.h"
#include <iostream>

int main(int argc, const char * argv[]) try
{
    // Load assets
    loader loader;
    loader.register_root(get_program_binary_path() + "../../assets");
    loader.register_root("C:/windows/fonts");

    shader_compiler compiler{loader};
    auto ui_vs = compiler.compile_file(rhi::shader_stage::vertex, "ui.vert");
    auto ui_fs = compiler.compile_file(rhi::shader_stage::fragment, "ui.frag");      
    
    sprite_sheet sheet;
    gui_sprites sprites{sheet};
    font_face face{sheet, loader.load_binary_file("arial.ttf"), 20};
    sheet.prepare_sheet();

    // Obtain a device and create device objects
    gfx::context context;
    auto debug = [](const char * message) { std::cerr << message << std::endl; };
    auto dev = context.get_backends().back().create_device(debug);

    auto font_image = dev->create_image({rhi::image_shape::_2d, {sheet.img.dimensions,1}, 1, sheet.img.format, rhi::sampled_image_bit}, {sheet.img.get_pixels()});
    auto nearest = dev->create_sampler({rhi::filter::nearest, rhi::filter::nearest, std::nullopt, rhi::address_mode::clamp_to_edge, rhi::address_mode::repeat});
    auto set_layout = dev->create_descriptor_set_layout({
        {0, rhi::descriptor_type::uniform_buffer, 1},
        {1, rhi::descriptor_type::combined_image_sampler, 1}
    });
    auto pipe_layout = dev->create_pipeline_layout({set_layout});
    const auto ui_vertex_binding = gfx::vertex_binder<ui_vertex>(0)
        .attribute(0, &ui_vertex::position)
        .attribute(1, &ui_vertex::texcoord)
        .attribute(2, &ui_vertex::color);
    auto ui_vss = dev->create_shader(ui_vs), ui_fss = dev->create_shader(ui_fs);
    const rhi::blend_state translucent {true, {rhi::blend_factor::source_alpha, rhi::blend_op::add, rhi::blend_factor::one_minus_source_alpha}, {rhi::blend_factor::source_alpha, rhi::blend_op::add, rhi::blend_factor::one_minus_source_alpha}};
    auto pipe = dev->create_pipeline({pipe_layout, {ui_vertex_binding}, {ui_vss,ui_fss}, rhi::primitive_topology::triangles, rhi::front_face::counter_clockwise, rhi::cull_mode::none, rhi::compare_op::always, false, {translucent}});
    auto gwindow = std::make_unique<gfx::window>(*dev, int2{1280,720}, to_string("Workbench 2018 - GUI Test"));

    // Create transient resources
    gfx::transient_resource_pool pools[3] {*dev, *dev, *dev};
    int pool_index=0;

    // Main loop
    while(!gwindow->should_close())
    {
        // Poll events
        context.poll_events();

        // Reset resources
        pool_index = (pool_index+1)%3;
        auto & pool = pools[pool_index];
        pool.begin_frame(*dev);

        // Draw the UI
        gui_context gui = gui_context(sprites, pool, gwindow->get_window_size());
        gui.draw_rounded_rect({32,32,512,512}, 8, {1,1,1,1});
        gui.draw_rounded_rect({34,34,510,510}, 6, {0,0,0.3f,1});
        gui.draw_shadowed_text(face, {1,1,1,1}, 56, 56, "This is a test");

        // Set up descriptor set for UI global transform and font image
        auto & fb = gwindow->get_rhi_window().get_swapchain_framebuffer();
        const coord_system ui_coords {coord_axis::right, coord_axis::down, coord_axis::forward};
        auto set = pool.descriptors->alloc(*set_layout);
        set->write(0, pool.uniforms.upload(make_transform_4x4(ui_coords, fb.get_ndc_coords())));
        set->write(1, *nearest, *font_image);

        // Encode our command buffer
        auto cmd = dev->create_command_buffer();
        rhi::render_pass_desc pass;
        pass.color_attachments = {{rhi::clear_color{0.2f,0.2f,0.2f,1}, rhi::store{rhi::layout::present_source}}};
        pass.depth_attachment = {rhi::clear_depth{1.0f,0}, rhi::dont_care{}};
        cmd->begin_render_pass(pass, fb);
        cmd->bind_pipeline(*pipe);
        cmd->bind_descriptor_set(*pipe_layout, 0, *set);
        gui.draw(*cmd);
        cmd->end_render_pass();

        // Submit and end frame
        dev->acquire_and_submit_and_present(*cmd, gwindow->get_rhi_window());
        pool.end_frame(*dev);
    }
    return EXIT_SUCCESS;
}
catch(const std::exception & e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
