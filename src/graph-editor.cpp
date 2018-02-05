#include "engine/shader.h"
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
    std::string caption;
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;
};

struct editor_state;

struct node;

struct edge
{
    const node * other;
    size_t pin;

    edge() : other(), pin() {}
    edge(const node * other, size_t pin) : other(other), pin(pin) {}
};

struct node
{
    const node_type * type;
    int2 placement;

    std::vector<edge> input_edges;

    node(const node_type * type, const int2 & placement) : type(type), placement(placement), input_edges(type->inputs.size()) {}
};

const int corner_radius = 10;
const int title_height = 25;
static int get_node_width(const gui & g, const node & n) 
{ 
    int l=0, r=0;
    for(auto & in : n.type->inputs) l = std::max(l, g.get_style().def_font.get_text_width(in));
    for(auto & out : n.type->outputs) r = std::max(r, g.get_style().def_font.get_text_width(out));
    return std::max(l + r + 50, g.get_style().def_font.get_text_width(n.type->caption) + 16);
}
static int get_in_pins(const node & n) { return static_cast<int>(n.type->inputs.size()); }
static int get_out_pins(const node & n) { return static_cast<int>(n.type->outputs.size()); }
static int get_node_body_height(const node & n) { return std::max(get_in_pins(n), get_out_pins(n)) * 24 + 12; }
static int get_node_height(const node & n) { return title_height + get_node_body_height(n); }
static rect<int> get_node_rect(const gui & g, const node & n) { return {n.placement.x, n.placement.y, n.placement.x+get_node_width(g,n), n.placement.y+get_node_height(n)}; }
static int2 get_input_location (const gui & g, const node & n, size_t index) { auto r = get_node_rect(g,n); return {r.x0, r.y0 + title_height + 18 + 24 * (int)index + std::max(get_out_pins(n) - get_in_pins(n), 0) * 12}; }
static int2 get_output_location(const gui & g, const node & n, size_t index) { auto r = get_node_rect(g,n); return {r.x1, r.y0 + title_height + 18 + 24 * (int)index + std::max(get_in_pins(n) - get_out_pins(n), 0) * 12}; }
static rect<int> get_input_rect (const gui & g, const node & n, size_t index) { auto loc = get_input_location(g,n,index); return {loc.x-8, loc.y-8, loc.x+8, loc.y+8}; }
static rect<int> get_output_rect(const gui & g, const node & n, size_t index) { auto loc = get_output_location(g,n,index); return {loc.x-8, loc.y-8, loc.x+8, loc.y+8}; }

const node_type types[] = {
    {"Add", {"A", "B"}, {"A + B"}},
    {"Subtract", {"A", "B"}, {"A - B"}},
    {"Multiply", {"A", "B"}, {"A * B"}},
    {"Divide", {"A", "B"}, {"A / B"}},
    {"Make Float2", {"X", "Y"}, {"(X, Y)"}},
    {"Make Float3", {"X", "Y", "Z"}, {"(X, Y, Z)"}},
    {"Make Float4", {"X", "Y", "Z", "W"}, {"(X, Y, Z, W)"}},
    {"Break Float2", {"(X, Y)"}, {"X", "Y"}},
    {"Break Float3", {"(X, Y, Z)"}, {"X", "Y", "Z"}},
    {"Break Float4", {"(X, Y, Z, W)"}, {"X", "Y", "Z", "W"}},
    {"Normalize Vector", {"V"}, {"V / |V|"}},
};

bool is_subsequence(const std::string & seq, const std::string & sub)
{
    auto it = begin(seq);
    for(char ch : sub)
    {
        bool match = false;
        for(; it != end(seq); ++it)
        {
            if(toupper(ch) == toupper(*it))
            {
                match = true;
                break;
            }
        }
        if(!match) return false;
    }
    return true;
}

struct graph
{
    std::vector<node *> nodes;

    node * link_input_node, * link_output_node;
    size_t link_input_pin, link_output_pin;

    int2 popup_loc;
    std::string node_filter;
    int node_scroll;

    graph() { reset_link(); }

    void reset_link()
    {
        link_input_node = link_output_node = nullptr;
    }

    void draw_wire(gui & g, const float2 & p0, const float2 & p1) const
    {
        g.draw_bezier_curve(p0, float2(p0.x + abs(p1.x - p0.x)*0.7f, p0.y), float2(p1.x - abs(p1.x - p0.x)*0.7f, p1.y), p1, 2, {1,1,1,1});    
    }

    void new_node_popup(gui & g, int id)
    {
        if(g.is_right_mouse_down())
        {
            g.begin_group(id);
            g.set_focus(id);
            popup_loc = int2(g.get_cursor());
            node_filter = "";
        }

        if(g.is_focused(id) || g.is_group_focused(id))
        {
            int w = 0;
            int n = 0;
            for(auto & type : types)
            {
                w = std::max(w, g.get_style().def_font.get_text_width(type.caption));
                if(is_subsequence(type.caption, node_filter)) ++n;
            }

            auto loc = popup_loc;
            const rect<int> overlay = {loc.x, loc.y, loc.x + w + 30, loc.y + 200}; 
            g.begin_group(id);
            g.begin_overlay();      

            g.draw_rect(overlay, {0.7f,0.7f,0.7f,1});
            g.draw_rect({overlay.x0+1, overlay.y0+1, overlay.x1-1, overlay.y1-1}, {0.3f,0.3f,0.3f,1});
            edit(g, 1, {overlay.x0+4, overlay.y0+4, overlay.x1-4, overlay.y0 + g.get_style().def_font.line_height + 8}, node_filter);

            auto c = vscroll_panel(g, 2, {overlay.x0 + 1, overlay.y0 + g.get_style().def_font.line_height + 12, overlay.x1 - 1, overlay.y1 - 1}, (g.get_style().def_font.line_height+4) * n - 4, node_scroll);
            g.begin_scissor(c);
            int y = c.y0 - node_scroll;
            for(auto & type : types)
            {
                if(!is_subsequence(type.caption, node_filter)) continue;

                rect<int> r = {c.x0, y, c.x1, y + g.get_style().def_font.line_height};
                if(g.clickable_widget(r))
                {
                    nodes.push_back(new node{&type, popup_loc});
                    g.clear_focus();
                }
                if(g.is_cursor_over(r)) g.draw_rect(r, {0.7f,0.7f,0.3f,1});
                g.draw_shadowed_text({r.x0+4, r.y0}, {1,1,1,1}, type.caption);                
                y = r.y1 + 4;
            }
            g.end_scissor();

            g.end_overlay();
            g.end_group();
            if(overlay.contains(g.get_cursor())) g.consume_click();
            else if(g.is_mouse_clicked()) g.clear_focus();
        }
    }

    void on_gui(gui & g)
    {
        const int ID_NEW_WIRE = 1, ID_POPUP_MENU = 2, ID_DRAG_GRAPH = 3;
        int id = 4;

        new_node_popup(g, ID_POPUP_MENU);
        
        // Draw wires
        for(auto & n : nodes)
        {
            for(size_t i=0; i<n->input_edges.size(); ++i)
            {
                if(!n->input_edges[i].other) continue;
                const auto p0 = float2(get_output_location(g, *n->input_edges[i].other, n->input_edges[i].pin)), p3 = float2(get_input_location(g, *n, i));
                draw_wire(g, p0, p3);
            }
        }

        // Clickable + draggable nodes
        for(auto & n : nodes)
        {
            // Input pin interactions
            for(size_t i=0; i<n->type->inputs.size(); ++i)
            {
                if(g.check_press(ID_NEW_WIRE, get_input_rect(g,*n,i)))
                {
                    n->input_edges[i].other = nullptr;
                    link_input_node = n;
                    link_input_pin = i;
                    g.consume_click();
                }

                if(g.is_cursor_over(get_input_rect(g,*n,i)) && link_output_node && g.check_release(ID_NEW_WIRE))
                {
                    n->input_edges[i] = {link_output_node, link_output_pin};
                    reset_link();
                }
            }
            
            // Output pin interactions
            for(size_t i=0; i<n->type->outputs.size(); ++i)
            {
                if(g.check_press(ID_NEW_WIRE, get_output_rect(g,*n,i)))
                {
                    if(g.is_alt_held()) for(auto & other : nodes) for(auto & edge : other->input_edges) if(edge.other == n) edge.other = nullptr;
                    link_output_node = n;
                    link_output_pin = i;
                    g.consume_click();
                }

                if(g.is_cursor_over(get_output_rect(g,*n,i)) && link_input_node && g.check_release(ID_NEW_WIRE))
                {
                    link_input_node->input_edges[link_input_pin] = {n, i};
                    reset_link();
                }
            }

            // Drag the body of the node
            {
                auto r = get_node_rect(g,*n);
                auto p = r.corner00();
                if(g.draggable_widget(id, r.dims(), p)) n->placement = p;
            }

            // Draw the node
            auto r = get_node_rect(g,*n);
            g.draw_partial_rounded_rect({r.x0, r.y0, r.x1, r.y0+title_height}, corner_radius, top_left_corner|top_right_corner, {0.5f,0.5f,0.5f,0.85f});
            g.draw_partial_rounded_rect({r.x0, r.y0+title_height, r.x1, r.y1}, corner_radius, bottom_left_corner|bottom_right_corner, {0.3f,0.3f,0.3f,0.85f});
            g.begin_scissor({r.x0, r.y0, r.x1, r.y0+title_height});
            g.draw_shadowed_text({r.x0+8, r.y0+6}, {1,1,1,1}, n->type->caption);
            g.end_scissor();

            for(size_t i=0; i<n->type->inputs.size(); ++i)
            {
                const auto loc = get_input_location(g,*n,i);
                g.draw_circle(loc, 8, {1,1,1,1});
                g.draw_circle(loc, 6, {0.2f,0.2f,0.2f,1});
                g.draw_shadowed_text(loc + int2(12, -g.get_style().def_font.line_height/2), {1,1,1,1}, n->type->inputs[i]);

                if(g.is_cursor_over({loc.x-8, loc.y-8, loc.x+8, loc.y+8}))
                {
                    draw_tooltip(g, loc, "This is an input");
                }
            }
            for(size_t i=0; i<n->type->outputs.size(); ++i)
            {
                const auto loc = get_output_location(g,*n,i);
                g.draw_circle(loc, 8, {1,1,1,1});
                g.draw_circle(loc, 6, {0.2f,0.2f,0.2f,1});
                g.draw_shadowed_text(loc + int2(-12 - g.get_style().def_font.get_text_width(n->type->outputs[i]), -g.get_style().def_font.line_height/2), {1,1,1,1}, n->type->outputs[i]);

                if(g.is_cursor_over({loc.x-8, loc.y-8, loc.x+8, loc.y+8}))
                {
                    draw_tooltip(g, loc, "This is an output");
                }
            }
            ++id;
        }

        // Fill in pins with connected wires
        for(auto & n : nodes)
        {
            for(size_t i=0; i<n->input_edges.size(); ++i)
            {
                if(!n->input_edges[i].other) continue;
                g.draw_circle(get_output_location(g, *n->input_edges[i].other, n->input_edges[i].pin), 7, {1,1,1,1});
                g.draw_circle(get_input_location(g, *n, i), 7, {1,1,1,1});
            }
        }

        // If the user is currently dragging a wire between two pins, draw it
        if(g.is_pressed(ID_NEW_WIRE))
        {
            int2 p0 = g.get_cursor(), p1 = p0;
            if(link_output_node)
            {
                auto loc = get_output_location(g, *link_output_node, link_output_pin);
                g.draw_circle(loc, 7, {1,1,1,1});
                p0 = loc;
            }
            if(link_input_node)
            {
                auto loc = get_input_location(g, *link_input_node, link_input_pin);
                g.draw_circle(loc, 7, {1,1,1,1});
                p1 = loc;
            }
            draw_wire(g, float2(p0), float2(p1));

            if(g.check_release(ID_NEW_WIRE)) reset_link();
        }
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
    graph gr;
    gr.nodes = {
        new node{&types[0], {50,50}},
        new node{&types[1], {650,150}}
    };
    gr.nodes[1]->input_edges[1] = edge(gr.nodes[0], 0);


    // Obtain a device and create device objects
    gfx::context context;
    auto dev = context.create_device({rhi::client_api::opengl}, [](const char * message) { std::cerr << message << std::endl; });
    canvas_device_objects device_objects {*dev, compiler, sheet};
    auto gwindow = std::make_unique<gfx::window>(*dev, int2{1280,720}, to_string("Workbench 2018 - Graph Editor"));
    
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

        // Handle the GUI
        canvas canvas {sprites, device_objects, pool};
        const gui_style style {face, icons};
        gui g {gs, canvas, style, gwindow->get_glfw_window()};
        gr.on_gui(g);
                
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
