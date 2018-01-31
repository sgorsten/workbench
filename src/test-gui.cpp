#include "engine/shader.h"
#include "engine/sprite.h"
#include "engine/gui.h"

float srgb_to_linear(float srgb) { return srgb <= 0.04045f ? srgb/12.92f : std::pow((srgb+0.055f)/1.055f, 2.4f); }

void draw_tooltip(gui & g, const int2 & loc, std::string_view text)
{
    int w = g.get_style().def_font.get_text_width(text), h = g.get_style().def_font.line_height;

    g.begin_overlay();
    g.draw_partial_rounded_rect({loc.x+10, loc.y, loc.x+w+20, loc.y+h+10}, 8, top_right_corner|bottom_left_corner|bottom_right_corner, {float3(srgb_to_linear(0.5f)),1});
    g.draw_partial_rounded_rect({loc.x+11, loc.y+1, loc.x+w+19, loc.y+h+9}, 7, top_right_corner|bottom_left_corner|bottom_right_corner, {float3(srgb_to_linear(0.3f)),1});
    g.draw_shadowed_text(loc+int2(15,5), {1,1,1,1}, text);
    g.end_overlay();
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

    void on_gui(gui & g, int id, rect<int> & placement) const
    {
        int2 p = placement.corner00();
        if(g.draggable_widget(id, placement.dims(), p))
        {
            int2 move = p - placement.corner00();
            placement = placement.adjusted(move.x, move.y, move.x, move.y);
        }

        const auto & font = g.get_style().def_font;
        g.draw_partial_rounded_rect({placement.x0, placement.y0, placement.x1, placement.y0 + title_height}, corner_radius, top_left_corner|top_right_corner, {float3(srgb_to_linear(0.5f)),1});
        g.draw_partial_rounded_rect({placement.x0, placement.y0 + title_height, placement.x1, placement.y1}, corner_radius, bottom_left_corner|bottom_right_corner, {float3(srgb_to_linear(0.3f)),0.8f});
        g.draw_shadowed_text({placement.x0+8, placement.y0+6}, {1,1,1,1}, caption);

        for(size_t i=0; i<inputs.size(); ++i)
        {
            const auto loc = get_input_location(placement,i);
            g.draw_circle(loc, 8, {1,1,1,1});
            g.draw_circle(loc, 6, {float3(srgb_to_linear(0.2f)),1});
            g.draw_shadowed_text(loc + int2(12, -font.line_height/2), {1,1,1,1}, inputs[i]);
            if(g.is_cursor_over({loc-8,loc+8})) draw_tooltip(g, loc, "Tooltip for "+inputs[i]);
        }
        for(size_t i=0; i<outputs.size(); ++i)
        {
            const auto loc = get_output_location(placement,i);
            g.draw_circle(loc, 8, {1,1,1,1});
            g.draw_circle(loc, 6, {float3(srgb_to_linear(0.2f)),1});
            g.draw_shadowed_text(loc + int2(-12 - font.get_text_width(outputs[i]), -font.line_height/2), {1,1,1,1}, outputs[i]);
            if(g.is_cursor_over({loc-8,loc+8})) draw_tooltip(g, loc, "Tooltip for "+outputs[i]);
        }
    }
};

struct node
{
    const node_type * type;
    rect<int> placement;

    int2 get_input_location(size_t index) const { return type->get_input_location(placement, index); }
    int2 get_output_location(size_t index) const { return type->get_output_location(placement, index); }
    void on_gui(gui & g, int id) { type->on_gui(g, id, placement); }
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
    canvas_device_objects device_objects {*dev, compiler, sheet};
    auto gwindow = std::make_unique<gfx::window>(*dev, int2{1280,720}, to_string("Workbench 2018 - GUI Test"));
    
    // Create transient resources
    gfx::transient_resource_pool pools[3] {*dev, *dev, *dev};
    int pool_index=0;

    gui_state gs;
    gwindow->on_scroll = [w=gwindow->get_glfw_window(), &gs](double2 scroll) { gs.on_scroll(w, scroll.x, scroll.y); };
    gwindow->on_mouse_button = [w=gwindow->get_glfw_window(), &gs](int button, int action, int mods) { gs.on_mouse_button(w, button, action, mods); };
    gwindow->on_key = [w=gwindow->get_glfw_window(), &gs](int key, int scancode, int action, int mods) { gs.on_key(w, key, action, mods); };
    gwindow->on_char = [w=gwindow->get_glfw_window(), &gs](uint32_t ch, int mods) { gs.on_char(w, ch); };

    // Main loop
    while(!gwindow->should_close())
    {
        // Poll events
        gs.begin_frame();
        context.poll_events();

        // Reset resources
        pool_index = (pool_index+1)%3;
        auto & pool = pools[pool_index];
        pool.begin_frame(*dev);

        // Draw the UI
        rect<int> client_rect {{0,0}, gwindow->get_window_size()};
        canvas canvas {sprites, device_objects, pool};

        // Handle the menu
        const gui_style style {face, icons};
        gui g {gs, canvas, style, gwindow->get_glfw_window()};

        // Draw nodes
        for(auto & e : edges) e.draw(canvas);
        for(size_t i=0; i<countof(nodes); ++i) nodes[i].on_gui(g, exactly(i));
                
        // Encode our command buffer
        auto cmd = dev->create_command_buffer();
        rhi::render_pass_desc pass;
        pass.color_attachments = {{rhi::clear_color{0,0,0,1}, rhi::store{rhi::layout::present_source}}};
        pass.depth_attachment = {rhi::clear_depth{1.0f,0}, rhi::dont_care{}};
        cmd->begin_render_pass(pass, gwindow->get_rhi_window().get_swapchain_framebuffer());
        canvas.encode_commands(*cmd, *gwindow);
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
