#include "engine/pbr.h"
#include "engine/mesh.h"
#include "engine/camera.h"
#include "engine/load.h"
#include "engine/gui.h"

#include <chrono>
#include <iostream>

//////////////////////
// Scene definition //
//////////////////////

constexpr coord_system coords {coord_axis::right, coord_axis::forward, coord_axis::up};

struct object
{
    std::string name;
    float3 position;
    float roughness;
    float metalness;
};

struct scene
{
    std::vector<object> objects;
};

//////////////////
// Editor logic //
//////////////////

struct editor
{
    scene & cur_scene;
    std::shared_ptr<gfx::window> win;
    camera cam;
    object * selection = 0;
    double2 last_cursor;

    rect<int> viewport_rect;
    int sidebar_split = -300;
    int object_list_split = 250;
    int property_split = 100;
    size_t tab = 0;

    editor(scene & cur_scene, std::shared_ptr<gfx::window> win) : cur_scene{cur_scene}, win{win}, cam{coords}
    {    
        cam.pitch += 0.8f;
        cam.move(coord_axis::back, 10.0f);
    }

    void menu_gui(gui & g, const int id, rect<int> bounds)
    {
        g.begin_menu(id, bounds);
        g.begin_popup(1, "File");
            g.begin_popup(1, "New");
                g.menu_item("Game...", GLFW_MOD_CONTROL|GLFW_MOD_SHIFT, GLFW_KEY_N, 0);
                g.menu_item("Scene", GLFW_MOD_CONTROL, GLFW_KEY_N, 0);
            g.end_popup();
            g.menu_item("Open...", GLFW_MOD_CONTROL, GLFW_KEY_O, 0xf115);
            g.menu_item("Save", GLFW_MOD_CONTROL, GLFW_KEY_S, 0xf0c7);
            g.menu_item("Save As...", 0, 0, 0);
            g.menu_seperator();
            if(g.menu_item("Exit", GLFW_MOD_ALT, GLFW_KEY_F4, 0)) glfwSetWindowShouldClose(win->get_glfw_window(), 1);
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
    }

    void viewport_gui(gui & g, int id, rect<int> bounds, float timestep)
    {
        viewport_rect = bounds;

        const double2 cursor = win->get_cursor_pos();
        if(g.focusable_widget(id, bounds))
        {
            if(win->get_mouse_button(GLFW_MOUSE_BUTTON_LEFT))
            {
                cam.yaw += static_cast<float>(cursor.x - last_cursor.x) * 0.01f;
                cam.pitch = std::min(std::max(cam.pitch + static_cast<float>(cursor.y - last_cursor.y) * 0.01f, -1.5f), +1.5f);
            }

            if(win->get_key(GLFW_KEY_W)) cam.move(coord_axis::forward, timestep * 10);
            if(win->get_key(GLFW_KEY_A)) cam.move(coord_axis::left, timestep * 10);
            if(win->get_key(GLFW_KEY_S)) cam.move(coord_axis::back, timestep * 10);
            if(win->get_key(GLFW_KEY_D)) cam.move(coord_axis::right, timestep * 10);
        }
        last_cursor = cursor;

        g.draw_wire_rect(bounds, 1, g.is_focused(id) ? float4{1,1,1,1} : float4{0.5f,0.5f,0.5f,1});
    }

    void object_list_gui(gui & g, const int id, rect<int> bounds)
    {
        size_t tab=0;
        bounds = tabbed_container(g, bounds, {"Object List"}, tab);
        for(auto & obj : cur_scene.objects)
        {
            auto r = bounds.take_y0(g.get_style().def_font.line_height);
            if(g.is_cursor_over(r)) 
            {
                g.draw_rect(r, g.get_style().selection_background);
                if(g.is_mouse_clicked()) 
                {
                    selection = &obj; // TODO: Multiselect, etc.
                    g.clear_focus();
                    g.consume_click();
                }
            }

            g.draw_shadowed_text(r.corner00(), selection == &obj ? g.get_style().active_text : g.get_style().passive_text, obj.name);
            bounds.y0 += 4;
        }
    }

    void property_list_gui(gui & g, const int id, rect<int> bounds)
    {
        size_t tab=0;
        bounds = tabbed_container(g, bounds, {"Object Properties"}, tab);
        if(!selection) return;

        const int widget_height = g.get_style().def_font.line_height + 2, vspacing = 4;

        g.begin_group(id);
        int child_id = 0;
        auto [key_bounds, value_bounds] = hsplitter(g, child_id++, bounds.shrink(4), property_split);

        g.draw_shadowed_text(key_bounds.take_y0(widget_height).corner00() + int2{0,1}, g.get_style().passive_text, "Name");
        edit(g, child_id++, value_bounds.take_y0(widget_height), selection->name);
        key_bounds.y0 += vspacing; value_bounds.y0 += vspacing;

        g.draw_shadowed_text(key_bounds.take_y0(widget_height).corner00() + int2{0,1}, g.get_style().passive_text, "Position");
        edit(g, child_id++, value_bounds.take_y0(widget_height), selection->position);
        key_bounds.y0 += vspacing; value_bounds.y0 += vspacing;

        g.draw_shadowed_text(key_bounds.take_y0(widget_height).corner00() + int2{0,1}, g.get_style().passive_text, "Roughness");
        edit(g, child_id++, value_bounds.take_y0(widget_height), selection->roughness);
        key_bounds.y0 += vspacing; value_bounds.y0 += vspacing;

        g.draw_shadowed_text(key_bounds.take_y0(widget_height).corner00() + int2{0,1}, g.get_style().passive_text, "Metalness");
        edit(g, child_id++, value_bounds.take_y0(widget_height), selection->metalness);
        key_bounds.y0 += vspacing; value_bounds.y0 += vspacing;

        g.end_group();
    }

    void on_gui(gui & g, float timestep)
    {
        rect<int> client_rect {{0,0},win->get_window_size()};
        menu_gui(g, 0, client_rect.take_y0(20));
        const auto [viewport_rect, sidebar_rect] = hsplitter(g, 1, client_rect, sidebar_split);
        const auto [list_rect, props_rect] = vsplitter(g, 2, sidebar_rect, object_list_split);
        viewport_gui(g, 3, viewport_rect, timestep);
        object_list_gui(g, 4, list_rect);
        property_list_gui(g, 5, props_rect);
    }
};

int main(int argc, const char * argv[]) try
{
    // Register asset paths
    loader loader;
    loader.register_root(get_program_binary_path() + "../../assets");
    loader.register_root("C:/windows/fonts");
    
    sprite_sheet sheet;
    canvas_sprites sprites{sheet};
    font_face face{sheet, loader.load_binary_file("arialbd.ttf"), 14, 0x20, 0x7E};
    font_face icons{sheet, loader.load_binary_file("fontawesome-webfont.ttf"), 14, 0xf000, 0xf295};
    sheet.prepare_sheet();

    shader_compiler compiler{loader};
    auto standard_sh = standard_shaders::compile(compiler);
    auto vs = compiler.compile_file(rhi::shader_stage::vertex, "static-mesh.vert");
    auto lit_fs = compiler.compile_file(rhi::shader_stage::fragment, "textured-pbr.frag");
    auto unlit_fs = compiler.compile_file(rhi::shader_stage::fragment, "colored-unlit.frag");
    auto skybox_vs = compiler.compile_file(rhi::shader_stage::vertex, "skybox.vert");
    auto skybox_fs = compiler.compile_file(rhi::shader_stage::fragment, "skybox.frag");

    auto env_spheremap_img = loader.load_image("monument-valley.hdr");
    auto ground_mesh = make_quad_mesh({0.5f,0.5f,0.5f}, coords(coord_axis::right)*8.0f, coords(coord_axis::forward)*8.0f);
    auto box_mesh = make_box_mesh({1,0,0}, {-0.3f,-0.3f,-0.3f}, {0.3f,0.3f,0.3f});
    auto sphere_mesh = make_sphere_mesh(32, 32, 0.5f);

    scene scene;
    for(int i=0; i<3; ++i)
    {
        for(int j=0; j<3; ++j)
        {
            scene.objects.push_back({to_string("Sphere ", static_cast<char>('A'+i*3+j)), coords(coord_axis::right)*(i*2-2.f) + coords(coord_axis::forward)*(j*2-2.f), (j+0.5f)/3, (i+0.5f)/3});
        }
    }
    
    // Create a session for each device
    gfx::context context;
    auto debug = [](const char * message) { std::cerr << message << std::endl; };
    auto dev = context.get_backends().back().create_device(debug);

    standard_device_objects standard = {dev, standard_sh};
    canvas_device_objects canvas_objects {*dev, compiler, sheet};
    gfx::simple_mesh ground = {*dev, ground_mesh.vertices, ground_mesh.triangles};
    gfx::simple_mesh box = {*dev, box_mesh.vertices, box_mesh.triangles};
    gfx::simple_mesh sphere = {*dev, sphere_mesh.vertices, sphere_mesh.triangles};

    // Samplers
    auto nearest = dev->create_sampler({rhi::filter::nearest, rhi::filter::nearest, std::nullopt, rhi::address_mode::clamp_to_edge, rhi::address_mode::repeat});

    // Images
    const byte4 w{255,255,255,255}, g{128,128,128,255}, grid[]{w,g,w,g,g,w,g,w,w,g,w,g,g,w,g,w};
    auto checkerboard = dev->create_image({rhi::image_shape::_2d, {4,4,1}, 1, rhi::image_format::rgba_unorm8, rhi::sampled_image_bit}, {grid});
    auto env_spheremap = dev->create_image({rhi::image_shape::_2d, {env_spheremap_img.dimensions,1}, 1, env_spheremap_img.format, rhi::sampled_image_bit}, {env_spheremap_img.get_pixels()});

    // Descriptor set layouts
    auto per_scene_layout = dev->create_descriptor_set_layout({
        {0, rhi::descriptor_type::uniform_buffer, 1},
        {1, rhi::descriptor_type::combined_image_sampler, 1},
        {2, rhi::descriptor_type::combined_image_sampler, 1},
        {3, rhi::descriptor_type::combined_image_sampler, 1}
    });
    auto per_view_layout = dev->create_descriptor_set_layout({
        {0, rhi::descriptor_type::uniform_buffer, 1}
    });
    auto per_object_layout = dev->create_descriptor_set_layout({
        {0, rhi::descriptor_type::uniform_buffer, 1},
        {1, rhi::descriptor_type::combined_image_sampler, 1}
    });
    auto skybox_per_object_layout = dev->create_descriptor_set_layout({
        {0, rhi::descriptor_type::combined_image_sampler, 1}
    });

    // Pipeline layouts
    auto common_layout = dev->create_pipeline_layout({per_scene_layout, per_view_layout});
    auto object_layout = dev->create_pipeline_layout({per_scene_layout, per_view_layout, per_object_layout});
    auto skybox_layout = dev->create_pipeline_layout({per_scene_layout, per_view_layout, skybox_per_object_layout});

    const auto mesh_vertex_binding = gfx::vertex_binder<mesh_vertex>(0)
        .attribute(0, &mesh_vertex::position)
        .attribute(1, &mesh_vertex::color)
        .attribute(2, &mesh_vertex::normal)
        .attribute(3, &mesh_vertex::texcoord);   

    // Shaders
    auto vss = dev->create_shader(vs), lit_fss = dev->create_shader(lit_fs), unlit_fss = dev->create_shader(unlit_fs);
    auto skybox_vss = dev->create_shader(skybox_vs), skybox_fss = dev->create_shader(skybox_fs);

    // Blend states
    const rhi::blend_state opaque {false};
    const rhi::blend_state translucent {true, {rhi::blend_factor::source_alpha, rhi::blend_op::add, rhi::blend_factor::one_minus_source_alpha}, {rhi::blend_factor::source_alpha, rhi::blend_op::add, rhi::blend_factor::one_minus_source_alpha}};

    // Pipelines
    auto light_pipe = dev->create_pipeline({object_layout, {mesh_vertex_binding}, {vss,unlit_fss}, rhi::primitive_topology::triangles, rhi::front_face::counter_clockwise, rhi::cull_mode::none, rhi::compare_op::less, true, {opaque}});       
    auto solid_pipe = dev->create_pipeline({object_layout, {mesh_vertex_binding}, {vss,lit_fss}, rhi::primitive_topology::triangles, rhi::front_face::counter_clockwise, rhi::cull_mode::none, rhi::compare_op::less, true, {opaque}});       
    auto skybox_pipe = dev->create_pipeline({skybox_layout, {mesh_vertex_binding}, {skybox_vss,skybox_fss}, rhi::primitive_topology::triangles, rhi::front_face::clockwise, rhi::cull_mode::none, rhi::compare_op::always, false, {opaque}});

    // Create transient resources
    gfx::transient_resource_pool pools[3] {*dev, *dev, *dev};
    int pool_index=0;

    // Do some initial work
    pools[pool_index].begin_frame(*dev);
    auto env = standard.create_environment_map_from_spheremap(pools[pool_index], *env_spheremap, 512, coords);
    pools[pool_index].end_frame(*dev);

    // Window
    gui_state gs;
    auto gwindow = std::make_shared<gfx::window>(*dev, int2{1280,720}, to_string("Workbench 2018 - Scene Editor"));
    gwindow->on_scroll = [w=gwindow->get_glfw_window(), &gs](double2 scroll) { gs.on_scroll(w, scroll.x, scroll.y); };
    gwindow->on_mouse_button = [w=gwindow->get_glfw_window(), &gs](int button, int action, int mods) { gs.on_mouse_button(w, button, action, mods); };
    gwindow->on_key = [w=gwindow->get_glfw_window(), &gs](int key, int scancode, int action, int mods) { gs.on_key(w, key, action, mods); };
    gwindow->on_char = [w=gwindow->get_glfw_window(), &gs](uint32_t ch, int mods) { gs.on_char(w, ch); };

    editor editor{scene, gwindow};

    // Main loop
    double2 last_cursor;
    auto t0 = std::chrono::high_resolution_clock::now();
    while(!gwindow->should_close())
    {
        // Poll events
        gs.begin_frame();
        context.poll_events();

        // Compute timestep
        const auto t1 = std::chrono::high_resolution_clock::now();
        const auto timestep = std::chrono::duration<float>(t1-t0).count();
        t0 = t1;

        // Reset resources
        pool_index = (pool_index+1)%3;
        auto & pool = pools[pool_index];
        pool.begin_frame(*dev);

        // Draw the UI
        rect<int> client_rect {{0,0}, gwindow->get_window_size()};
        canvas canvas {sprites, canvas_objects, pool};

        // Handle the menu
        const gui_style style {face, icons};
        gui g {gs, canvas, style, gwindow->get_glfw_window()};
        editor.on_gui(g, timestep);

        // Set up per scene uniforms
        struct pbr_per_scene_uniforms per_scene_uniforms {};
        per_scene_uniforms.point_lights[0] = {{-3, -3, 8}, {23.47f, 21.31f, 20.79f}};
        per_scene_uniforms.point_lights[1] = {{ 3, -3, 8}, {23.47f, 21.31f, 20.79f}};
        per_scene_uniforms.point_lights[2] = {{ 3,  3, 8}, {23.47f, 21.31f, 20.79f}};
        per_scene_uniforms.point_lights[3] = {{-3,  3, 8}, {23.47f, 21.31f, 20.79f}};

        auto per_scene_set = pool.descriptors->alloc(*per_scene_layout);
        per_scene_set->write(0, pool.uniforms.upload(per_scene_uniforms));
        per_scene_set->write(1, standard.get_image_sampler(), standard.get_brdf_integral_image());
        per_scene_set->write(2, standard.get_cubemap_sampler(), *env.irradiance_cubemap);
        per_scene_set->write(3, standard.get_cubemap_sampler(), *env.reflectance_cubemap);

        auto cmd = dev->create_command_buffer();

        // Set up per-view uniforms for a specific framebuffer
        auto & fb = gwindow->get_rhi_window().get_swapchain_framebuffer();
        const auto proj_matrix = mul(linalg::perspective_matrix(1.0f, gwindow->get_aspect(), 0.1f, 100.0f, linalg::pos_z, dev->get_info().z_range), make_transform_4x4(coords, fb.get_ndc_coords()));

        pbr_per_view_uniforms per_view_uniforms;
        per_view_uniforms.view_proj_matrix = mul(proj_matrix, editor.cam.get_view_matrix());
        per_view_uniforms.skybox_view_proj_matrix = mul(proj_matrix, editor.cam.get_skybox_view_matrix());
        per_view_uniforms.eye_position = editor.cam.position;
        per_view_uniforms.right_vector = editor.cam.get_direction(coord_axis::right);
        per_view_uniforms.down_vector = editor.cam.get_direction(coord_axis::down);

        auto per_view_set = pool.descriptors->alloc(*per_view_layout);
        per_view_set->write(0, pool.uniforms.upload(per_view_uniforms));

        // Draw objects to our primary framebuffer
        rhi::render_pass_desc pass;
        pass.color_attachments = {{rhi::dont_care{}, rhi::store{rhi::layout::present_source}}};
        pass.depth_attachment = {rhi::clear_depth{1.0f,0}, rhi::dont_care{}};
        cmd->begin_render_pass(pass, fb);

        // Bind common descriptors
        cmd->bind_descriptor_set(*common_layout, pbr_per_scene_set_index, *per_scene_set);
        cmd->bind_descriptor_set(*common_layout, pbr_per_view_set_index, *per_view_set);

        // Draw skybox
        cmd->bind_pipeline(*skybox_pipe);
        auto skybox_set = pool.descriptors->alloc(*skybox_per_object_layout);
        skybox_set->write(0, standard.get_cubemap_sampler(), *env.environment_cubemap);
        cmd->bind_descriptor_set(*skybox_layout, pbr_per_object_set_index, *skybox_set);
        box.draw(*cmd);
        
        // Draw lights
        cmd->bind_pipeline(*light_pipe);
        for(auto & p : per_scene_uniforms.point_lights)
        {
            auto set = pool.descriptors->alloc(*per_object_layout);
            set->write(0, pool.uniforms.upload(mul(translation_matrix(p.position), scaling_matrix(float3{0.5f}))));
            set->write(1, *nearest, *checkerboard);
            cmd->bind_descriptor_set(*object_layout, pbr_per_object_set_index, *set);
            sphere.draw(*cmd);
        }

        // Draw the ground
        cmd->bind_pipeline(*solid_pipe);
        struct { float4x4 model_matrix; float roughness, metalness; } per_object;
        per_object.model_matrix = translation_matrix(editor.cam.coords(coord_axis::down)*0.5f);
        per_object.roughness = 0.5f;
        per_object.metalness = 0.0f;
        auto ground_set = pool.descriptors->alloc(*per_object_layout);
        ground_set->write(0, pool.uniforms.upload(per_object));
        ground_set->write(1, *nearest, *checkerboard);
        cmd->bind_descriptor_set(*object_layout, pbr_per_object_set_index, *ground_set);
        ground.draw(*cmd);

        // Draw a bunch of spheres
        for(auto & object : scene.objects)
        {
            per_object.model_matrix = translation_matrix(object.position);
            per_object.roughness = object.roughness;
            per_object.metalness = object.metalness;
            auto sphere_set = pool.descriptors->alloc(*per_object_layout);
            sphere_set->write(0, pool.uniforms.upload(per_object));
            sphere_set->write(1, *nearest, *checkerboard);
            cmd->bind_descriptor_set(*object_layout, pbr_per_object_set_index, *sphere_set);
            sphere.draw(*cmd);
        }

        canvas.encode_commands(*cmd, *gwindow);

        // Submit and end frame
        cmd->end_render_pass();
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
