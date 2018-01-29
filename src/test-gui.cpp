#include "engine/shader.h"
#include "engine/sprite.h"
#include "engine/gui.h"

float srgb_to_linear(float srgb) { return srgb <= 0.04045f ? srgb/12.92f : std::pow((srgb+0.055f)/1.055f, 2.4f); }

void draw_tooltip(canvas & canvas, const font_face & face, const int2 & loc, std::string_view text)
{
    int w = face.get_text_width(text), h = face.line_height;

    canvas.begin_overlay();
    canvas.draw_partial_rounded_rect({loc.x+10, loc.y, loc.x+w+20, loc.y+h+10}, 8, top_right_corner|bottom_left_corner|bottom_right_corner, {float3(srgb_to_linear(0.5f)),1});
    canvas.draw_partial_rounded_rect({loc.x+11, loc.y+1, loc.x+w+19, loc.y+h+9}, 7, top_right_corner|bottom_left_corner|bottom_right_corner, {float3(srgb_to_linear(0.3f)),1});
    canvas.draw_shadowed_text(loc+int2(15,5), {1,1,1,1}, face, text);
    canvas.end_overlay();
}

struct node_type
{
    static const int corner_radius = 10;
    static const int title_height = 25;

    std::string caption;
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;

    int2 get_input_location(const rect<int> & r, size_t index) const { return {r.x0, r.y0 + title_height + 18 + 24 * (int)index}; }
    int2 get_output_location(const rect<int> & r, size_t index) const { return {r.x1, r.y0 + title_height + 18 + 24 * (int)index}; }

    void draw(canvas & canvas, const font_face & face, const rect<int> & r) const
    {
        canvas.draw_partial_rounded_rect({r.x0, r.y0, r.x1, r.y0+title_height}, corner_radius, top_left_corner|top_right_corner, {float3(srgb_to_linear(0.5f)),1});
        canvas.draw_partial_rounded_rect({r.x0, r.y0+title_height, r.x1, r.y1}, corner_radius, bottom_left_corner|bottom_right_corner, {float3(srgb_to_linear(0.3f)),0.8f});
        canvas.draw_shadowed_text({r.x0+8, r.y0+6}, {1,1,1,1}, face, caption);

        for(size_t i=0; i<inputs.size(); ++i)
        {
            const auto loc = get_input_location(r,i);
            canvas.draw_circle(loc, 8, {1,1,1,1});
            canvas.draw_circle(loc, 6, {float3(srgb_to_linear(0.2f)),1});
            canvas.draw_shadowed_text(loc + int2(12, -face.line_height/2), {1,1,1,1}, face, inputs[i]);
        }
        for(size_t i=0; i<outputs.size(); ++i)
        {
            const auto loc = get_output_location(r,i);
            canvas.draw_circle(loc, 8, {1,1,1,1});
            canvas.draw_circle(loc, 6, {float3(srgb_to_linear(0.2f)),1});
            canvas.draw_shadowed_text(loc + int2(-12 - face.get_text_width(outputs[i]), -face.line_height/2), {1,1,1,1}, face, outputs[i]);

            if(i == 1) draw_tooltip(canvas, face, loc, "Tooltip in an overlay");
        }
    }
};

struct node
{
    const node_type * type;
    rect<int> placement;

    int2 get_input_location(size_t index) const { return type->get_input_location(placement, index); }
    int2 get_output_location(size_t index) const { return type->get_output_location(placement, index); }
    void draw(canvas & canvas, font_face & face) const { type->draw(canvas, face, placement); }
};

struct edge
{
    const node * output_node;
    int output_index;
    const node * input_node;
    int input_index;
    bool curved;

    void draw(canvas & canvas) const
    {
        const auto p0 = float2(output_node->get_output_location(output_index));
        const auto p3 = float2(input_node->get_input_location(input_index));
        const auto p1 = float2((p0.x+p3.x)/2, p0.y), p2 = float2((p0.x+p3.x)/2, p3.y);
        canvas.draw_circle(output_node->get_output_location(output_index), 7, {1,1,1,1});
        canvas.draw_circle(input_node->get_input_location(input_index), 7, {1,1,1,1});
        if(curved) canvas.draw_bezier_curve(p0, p1, p2, p3, 3, {1,1,1,1});
        else canvas.draw_line(p0, p3, 3, {1,1,1,1});
    }
};

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
    canvas_sprites sprites{sheet};
    font_face face{sheet, loader.load_binary_file("arialbd.ttf"), 14, 0x20, 0x7E};
    font_face icons{sheet, loader.load_binary_file("fontawesome-webfont.ttf"), 14, 0xf000, 0xf295};
    sheet.prepare_sheet();

    // Set up graph state
    const node_type type = {"Graph Node", {"Input 1", "Input 2"}, {"Output 1", "Output 2", "Output 3"}};
    node nodes[] = {
        {&type, {50,50,300,250}},
        {&type, {650,150,900,350}}
    };
    const edge edges[] = {
        {&nodes[0], 0, &nodes[1], 0, false},
        {&nodes[0], 2, &nodes[1], 1, true}
    };

    // Obtain a device and create device objects
    gfx::context context;
    auto debug = [](const char * message) { std::cerr << message << std::endl; };
    auto dev = context.get_backends().back().create_device(debug);

    auto font_image = dev->create_image({rhi::image_shape::_2d, {sheet.sheet_image.dims(),1}, 1, rhi::image_format::r_unorm8, rhi::sampled_image_bit}, {sheet.sheet_image.data()});
    auto linear = dev->create_sampler({rhi::filter::linear, rhi::filter::linear, std::nullopt, rhi::address_mode::clamp_to_edge, rhi::address_mode::repeat});
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

    gui g;
    gwindow->on_scroll = [w=gwindow->get_glfw_window(), &g](double2 scroll) { g.on_scroll(w, scroll.x, scroll.y); };
    gwindow->on_mouse_button = [w=gwindow->get_glfw_window(), &g](int button, int action, int mods) { g.on_mouse_button(w, button, action, mods); };
    gwindow->on_key = [w=gwindow->get_glfw_window(), &g](int key, int scancode, int action, int mods) { g.on_key(w, key, action, mods); };
    gwindow->on_char = [w=gwindow->get_glfw_window(), &g](uint32_t ch, int mods) { g.on_char(w, ch); };
    int split = -300;
    size_t tab = 0;

    // Main loop
    while(!gwindow->should_close())
    {
        // Poll events
        g.begin_frame();
        context.poll_events();

        int2 move {0,0};
        if(gwindow->get_key(GLFW_KEY_W)) move.y -= 4;
        if(gwindow->get_key(GLFW_KEY_A)) move.x -= 4;
        if(gwindow->get_key(GLFW_KEY_S)) move.y += 4;
        if(gwindow->get_key(GLFW_KEY_D)) move.x += 4;
        nodes[1].placement.x0 += move.x;
        nodes[1].placement.y0 += move.y;
        nodes[1].placement.x1 += move.x;
        nodes[1].placement.y1 += move.y;

        // Reset resources
        pool_index = (pool_index+1)%3;
        auto & pool = pools[pool_index];
        pool.begin_frame(*dev);

        // Draw the UI
        rect<int> client_rect {{0,0}, gwindow->get_window_size()};
        canvas canvas {sprites, pool, gwindow->get_window_size()};

        // Handle the menu
        g.begin_window(gwindow->get_glfw_window(), &face, &icons, &canvas);
        g.begin_menu(0, client_rect.take_y0(20));
            g.begin_popup(1, "File");
                g.begin_popup(1, "New");
                    g.menu_item("Game...", GLFW_MOD_CONTROL|GLFW_MOD_SHIFT, GLFW_KEY_N, 0);
                    g.menu_item("Scene", GLFW_MOD_CONTROL, GLFW_KEY_N, 0);
                g.end_popup();
                g.menu_item("Open...", GLFW_MOD_CONTROL, GLFW_KEY_O, 0xf115);
                g.menu_item("Save", GLFW_MOD_CONTROL, GLFW_KEY_S, 0xf0c7);
                g.menu_item("Save As...", 0, 0, 0);
                g.menu_seperator();
                if(g.menu_item("Exit", GLFW_MOD_ALT, GLFW_KEY_F4, 0)) glfwSetWindowShouldClose(gwindow->get_glfw_window(), 1);
            g.end_popup();
            g.begin_popup(2, "Edit");
                g.menu_item("Undo", GLFW_MOD_CONTROL, GLFW_KEY_Z, 0xf0e2);
                g.menu_item("Redo", GLFW_MOD_CONTROL, GLFW_KEY_Y, 0xf01e);
                g.menu_seperator();
                g.menu_item("Cut", GLFW_MOD_CONTROL, GLFW_KEY_X, 0xf0c4);
                g.menu_item("Copy", GLFW_MOD_CONTROL, GLFW_KEY_C, 0xf0c5);
                g.menu_item("Paste", GLFW_MOD_CONTROL, GLFW_KEY_V, 0xf0ea);
                g.menu_seperator();
                g.menu_item("Select All", GLFW_MOD_CONTROL, GLFW_KEY_A, 0xf245);
            g.end_popup();
            g.begin_popup(3, "Help");
                g.menu_item("View Help", GLFW_MOD_CONTROL, GLFW_KEY_F1, 0xf059);
            g.end_popup();
        g.end_menu();
        g.end_window();

        auto [left_rect, right_rect] = hsplitter(g, 1, client_rect, split);

        // Draw nodes
        g.begin_scissor(left_rect);
        for(auto & e : edges) e.draw(canvas);
        for(auto & n : nodes) n.draw(canvas, face);
        g.draw_wire_rect(left_rect, {1,1,1,1});
        g.end_scissor();

        tabbed_container(g, right_rect, {"Nodes", "Variables", "Subgraphs"}, tab);

        // Set up descriptor set for UI global transform and font image
        auto & fb = gwindow->get_rhi_window().get_swapchain_framebuffer();
        const coord_system ui_coords {coord_axis::right, coord_axis::down, coord_axis::forward};

        auto dims = gwindow->get_window_size();
        const float4x4 ortho {{2.0f/dims.x,0,0,0}, {0,2.0f/dims.y,0,0}, {0,0,1,0}, {-1,-1,0,1}};
        auto set = pool.descriptors->alloc(*set_layout);
        set->write(0, pool.uniforms.upload(mul(make_transform_4x4(ui_coords, fb.get_ndc_coords()), ortho)));
        set->write(1, *linear, *font_image);

        // Encode our command canvas
        auto cmd = dev->create_command_buffer();
        rhi::render_pass_desc pass;
        pass.color_attachments = {{rhi::clear_color{0,0,0,1}, rhi::store{rhi::layout::present_source}}};
        pass.depth_attachment = {rhi::clear_depth{1.0f,0}, rhi::dont_care{}};
        cmd->begin_render_pass(pass, fb);
        cmd->bind_pipeline(*pipe);
        cmd->bind_descriptor_set(*pipe_layout, 0, *set);
        canvas.encode_commands(*cmd);
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
