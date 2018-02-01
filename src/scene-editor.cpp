#include "engine/pbr.h"
#include "engine/mesh.h"
#include "engine/camera.h"
#include "engine/load.h"
#include "engine/gui.h"

#include <chrono>
#include <iostream>

///////////////////////
// Asset definitions //
///////////////////////

constexpr coord_system coords {coord_axis::right, coord_axis::forward, coord_axis::up};

struct texture_asset
{
    std::string name;
    rhi::ptr<rhi::image> gtex;
};

struct mesh_asset
{
    std::string name;
    mesh cmesh;
    gfx::simple_mesh gmesh;

    bool raycast(const ray & r) const
    {
        for(auto & tri : cmesh.triangles)
        {
            if(intersect_ray_triangle(r, cmesh.vertices[tri[0]].position, cmesh.vertices[tri[1]].position, cmesh.vertices[tri[2]].position))
            {
                return true;
            }
        }
        return false;
    }
};

struct asset_library
{
    std::vector<texture_asset *> textures;
    std::vector<mesh_asset *> meshes;
};

//////////////////////
// Scene definition //
//////////////////////

struct object
{
    std::string name;
    float3 position;
    float3 scale;
    mesh_asset * mesh;
    texture_asset * albedo;
    pbr_material_uniforms material;
    float3 light;

    float4x4 get_model_matrix() const { return mul(translation_matrix(position), scaling_matrix(scale)); }
    pbr_object_uniforms get_object_uniforms() const { return {get_model_matrix()}; }

    bool raycast(const ray & r) const
    {
        return mesh ? mesh->raycast({(r.origin-position)/scale, r.direction/scale}) : false;
    }
};

struct scene
{
    std::vector<object> objects;
};

//////////////////
// Editor logic //
//////////////////

struct gizmo
{
    rhi::ptr<const rhi::pipeline> pipe;
    const mesh_asset * arrows[3];
public:
    gizmo(const rhi::pipeline & pipe, const mesh_asset * arrow_x, const mesh_asset * arrow_y, const mesh_asset * arrow_z) : pipe{&pipe}, arrows{arrow_x, arrow_y, arrow_z}
    {
    
    }

    void draw(rhi::command_buffer & cmd, gfx::transient_resource_pool & pool, const float3 & position) const
    {
        cmd.clear_depth(1.0);
        cmd.bind_pipeline(*pipe);

        auto object_set = pool.alloc_descriptor_set(*pipe, pbr_per_object_set_index);
        object_set.write(0, pbr_object_uniforms{translation_matrix(position)});
        object_set.bind(cmd);

        const float3 colors[] {{1,0,0},{0,1,0},{0,0,1}};
        for(int i=0; i<3; ++i)
        {
            auto material_set = pool.alloc_descriptor_set(*pipe, pbr_per_material_set_index);
            material_set.write(0, pbr_material_uniforms{colors[i],0.8f,0.0f});        
            material_set.bind(cmd); 
            arrows[i]->gmesh.draw(cmd);
        }
    }
};

struct ranged_property { float & value; float min, max; };
class property_editor
{
    gui & g;
    asset_library & assets;
    rect<int> key_bounds, value_bounds;
    int next_id=0;

    void edit_property(rect<int> bounds, std::string & prop) { ::edit(g, next_id++, bounds, prop); }
    void edit_property(rect<int> bounds, float & prop) { ::edit(g, next_id++, bounds, prop); }
    void edit_property(rect<int> bounds, float3 & prop) { ::edit(g, next_id++, bounds, prop); }
    void edit_property(rect<int> bounds, ranged_property & prop) { ::hslider(g, next_id++, bounds, prop.min, prop.max, prop.value); }
    void edit_property(rect<int> bounds, mesh_asset * & prop) { combobox<mesh_asset *>(g, next_id++, bounds, assets.meshes, [](const mesh_asset * m) { return std::string_view{m->name}; }, prop); }
    void edit_property(rect<int> bounds, texture_asset * & prop) { icon_combobox<texture_asset *>(g, next_id++, bounds, assets.textures, 
        [](const texture_asset * t) { return std::string_view{t->name}; }, 
        [&](const texture_asset * t, const rect<int> & r) { g.draw_image(r, {1,1,1,1}, *t->gtex); }, prop);
    }
public:
    property_editor(gui & g, asset_library & assets, rect<int> bounds, int & split) : g{g}, assets{assets} { std::tie(key_bounds, value_bounds) = hsplitter(g, next_id++, bounds.shrink(4), split); }

    template<class T> void edit(const char * label, T & field)
    {
        const int widget_height = g.get_style().def_font.line_height + 2;
        g.draw_shadowed_text(key_bounds.take_y0(widget_height).corner00() + int2{0,1}, g.get_style().passive_text, label);
        key_bounds.y0 += 4; 
        edit_property(value_bounds.take_y0(widget_height), field);
        value_bounds.y0 += 4;              
    }
};

struct editor
{
    asset_library & assets;
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

    editor(asset_library & assets, scene & cur_scene, std::shared_ptr<gfx::window> win) : assets{assets}, cur_scene{cur_scene}, win{win}, cam{coords}
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
        size_t tab=0;
        viewport_rect = tabbed_container(g, bounds, {"Scene View"}, tab);

        // First handle click selections
        if(g.clickable_widget(viewport_rect))
        {
            selection = nullptr;
            auto ray = cam.get_ray_from_pixel(g.get_cursor(), viewport_rect);
            for(auto & object : cur_scene.objects)
            {
                if(object.raycast(ray))
                {
                    selection = &object;
                }
            }
            g.set_focus(id);
            g.consume_click();
        }
        if(g.is_right_mouse_clicked() && g.is_cursor_over(viewport_rect))
        {
            g.set_focus(id);
        }

        const double2 cursor = win->get_cursor_pos();
        if(g.is_focused(id))
        {
            if(win->get_mouse_button(GLFW_MOUSE_BUTTON_RIGHT))
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
    }

    void object_list_gui(gui & g, const int id, rect<int> bounds)
    {
        size_t tab=0;
        bounds = tabbed_container(g, bounds, {"Object List"}, tab);
        g.begin_scissor(bounds);

        const int line_height = g.get_style().def_font.line_height;

        static int scr=0;
        if(g.is_cursor_over(bounds)) scr -= g.get_scroll().y;
        bounds = vscroll_panel(g, id, bounds, (line_height+4)*exact_cast<int>(cur_scene.objects.size())-4, scr);
        bounds.y0 -= scr;
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
        g.end_scissor();
    }

    void property_list_gui(gui & g, const int id, rect<int> bounds)
    {
        size_t tab=0;
        bounds = tabbed_container(g, bounds, {"Object Properties"}, tab).shrink(4);
        if(!selection) return;
        
        g.begin_group(id);
        property_editor p {g, assets, bounds, property_split};
        p.edit("Name", selection->name);
        p.edit("Position", selection->position);
        p.edit("Scale", selection->scale);
        p.edit("Mesh", selection->mesh);
        p.edit("Albedo", selection->albedo);
        p.edit("Albedo Tint", selection->material.albedo_tint);
        p.edit("Roughness", ranged_property{selection->material.roughness,0,1});
        p.edit("Metalness", ranged_property{selection->material.metalness,0,1});
        p.edit("Light", selection->light);
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

struct pipelines
{
    rhi::ptr<const rhi::pipeline_layout> common_layout;
    rhi::ptr<const rhi::pipeline> light_pipe;
    rhi::ptr<const rhi::pipeline> skybox_pipe;
    rhi::ptr<const rhi::pipeline> colored_pbr_pipe;
    rhi::ptr<const rhi::pipeline> textured_pbr_pipe;
};

pipelines create_pipelines(rhi::device & dev, shader_compiler & compiler)
{
    // Descriptor set layouts
    auto per_scene_layout = dev.create_descriptor_set_layout({
        {0, rhi::descriptor_type::uniform_buffer, 1},
        {1, rhi::descriptor_type::combined_image_sampler, 1},
        {2, rhi::descriptor_type::combined_image_sampler, 1},
        {3, rhi::descriptor_type::combined_image_sampler, 1}
    });
    auto per_view_layout = dev.create_descriptor_set_layout({
        {0, rhi::descriptor_type::uniform_buffer, 1}
    });
    auto skybox_material_layout = dev.create_descriptor_set_layout({
        {0, rhi::descriptor_type::combined_image_sampler, 1}
    });
    auto colored_pbr_layout = dev.create_descriptor_set_layout({
        {0, rhi::descriptor_type::uniform_buffer, 1},
    });
    auto textured_pbr_layout = dev.create_descriptor_set_layout({
        {0, rhi::descriptor_type::uniform_buffer, 1},
        {1, rhi::descriptor_type::combined_image_sampler, 1}
    });
    auto static_object_layout = dev.create_descriptor_set_layout({
        {0, rhi::descriptor_type::uniform_buffer, 1},
    });

    // Pipeline layouts
    auto common_layout = dev.create_pipeline_layout({per_scene_layout, per_view_layout});
    auto skybox_layout = dev.create_pipeline_layout({per_scene_layout, per_view_layout, skybox_material_layout});
    auto colored_pipe_layout = dev.create_pipeline_layout({per_scene_layout, per_view_layout, colored_pbr_layout, static_object_layout});
    auto textured_pipe_layout = dev.create_pipeline_layout({per_scene_layout, per_view_layout, textured_pbr_layout, static_object_layout});
    
    // Vertex input state
    const auto mesh_vertex_binding = gfx::vertex_binder<mesh_vertex>(0)
        .attribute(0, &mesh_vertex::position)
        .attribute(1, &mesh_vertex::normal)
        .attribute(2, &mesh_vertex::texcoord);

    // Shaders
    auto vs = compiler.compile_file(rhi::shader_stage::vertex, "static-mesh.vert");
    auto colored_fs = compiler.compile_file(rhi::shader_stage::fragment, "colored-pbr.frag");
    auto textured_fs = compiler.compile_file(rhi::shader_stage::fragment, "textured-pbr.frag");
    auto unlit_fs = compiler.compile_file(rhi::shader_stage::fragment, "colored-unlit.frag");
    auto skybox_vs = compiler.compile_file(rhi::shader_stage::vertex, "skybox.vert");
    auto skybox_fs = compiler.compile_file(rhi::shader_stage::fragment, "skybox.frag");

    auto vss = dev.create_shader(vs), unlit_fss = dev.create_shader(unlit_fs), colored_fss = dev.create_shader(colored_fs), textured_fss = dev.create_shader(textured_fs);
    auto skybox_vss = dev.create_shader(skybox_vs), skybox_fss = dev.create_shader(skybox_fs);

    // Blend states
    const rhi::blend_state opaque {false};
    const rhi::blend_state translucent {true, {rhi::blend_factor::source_alpha, rhi::blend_op::add, rhi::blend_factor::one_minus_source_alpha}, {rhi::blend_factor::source_alpha, rhi::blend_op::add, rhi::blend_factor::one_minus_source_alpha}};

    // Pipelines
    auto light_pipe = dev.create_pipeline({colored_pipe_layout, {mesh_vertex_binding}, {vss,unlit_fss}, rhi::primitive_topology::triangles, rhi::front_face::counter_clockwise, rhi::cull_mode::none, rhi::compare_op::less, true, {opaque}});       
    auto skybox_pipe = dev.create_pipeline({skybox_layout, {mesh_vertex_binding}, {skybox_vss,skybox_fss}, rhi::primitive_topology::triangles, rhi::front_face::clockwise, rhi::cull_mode::none, rhi::compare_op::always, false, {opaque}});
    auto colored_pbr_pipe = dev.create_pipeline({colored_pipe_layout, {mesh_vertex_binding}, {vss,colored_fss}, rhi::primitive_topology::triangles, rhi::front_face::counter_clockwise, rhi::cull_mode::none, rhi::compare_op::less, true, {opaque}});       
    auto textured_pbr_pipe = dev.create_pipeline({textured_pipe_layout, {mesh_vertex_binding}, {vss,textured_fss}, rhi::primitive_topology::triangles, rhi::front_face::counter_clockwise, rhi::cull_mode::none, rhi::compare_op::less, true, {opaque}});       

    return {common_layout, light_pipe, skybox_pipe, colored_pbr_pipe, textured_pbr_pipe};
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
    auto env_spheremap_img = loader.load_image("monument-valley.hdr");

    const float2 arrow_points[] {{0, 0.05f}, {1, 0.05f}, {1, 0.10f}, {1.1f, 0.05f}, {1.15f, 0.025f}, {1.2f, 0}};
    auto arrow_x = new mesh_asset{"arrow_x", make_lathed_mesh({1,0,0}, {0,1,0}, {0,0,1}, 12, arrow_points)};
    auto arrow_y = new mesh_asset{"arrow_y", make_lathed_mesh({0,1,0}, {0,0,1}, {1,0,0}, 12, arrow_points)};
    auto arrow_z = new mesh_asset{"arrow_z", make_lathed_mesh({0,0,1}, {1,0,0}, {0,1,0}, 12, arrow_points)};
    auto box = new mesh_asset{"box", make_box_mesh({-0.3f,-0.3f,-0.3f}, {0.3f,0.3f,0.3f})};
    auto sphere = new mesh_asset{"sphere", make_sphere_mesh(32, 32, 0.5f)};
    auto plane = new mesh_asset{"plane", make_quad_mesh(coords(coord_axis::right)*8.0f, coords(coord_axis::forward)*8.0f)};

    auto checker = new texture_asset{"checker.png"};
    auto marble = new texture_asset{"marble.png"};
    auto scratched = new texture_asset{"scratched.png"};
    auto normal = new texture_asset{"normal.png"};

    asset_library assets;
    assets.meshes = {arrow_x, arrow_y, arrow_z, box, sphere, plane};
    assets.textures = {checker, marble, scratched, normal};

    scene scene;
    scene.objects.push_back({"Light A", {-3, -3, 8}, {0.5f,0.5f,0.5f}, sphere, checker, {{1,1,1}, 0.5f, 0.0f}, {23.47f, 21.31f, 20.79f}});
    scene.objects.push_back({"Light B", { 3, -3, 8}, {0.5f,0.5f,0.5f}, sphere, checker, {{1,1,1}, 0.5f, 0.0f}, {23.47f, 21.31f, 20.79f}});
    scene.objects.push_back({"Light C", { 3,  3, 8}, {0.5f,0.5f,0.5f}, sphere, checker, {{1,1,1}, 0.5f, 0.0f}, {23.47f, 21.31f, 20.79f}});
    scene.objects.push_back({"Light D", {-3,  3, 8}, {0.5f,0.5f,0.5f}, sphere, checker, {{1,1,1}, 0.5f, 0.0f}, {23.47f, 21.31f, 20.79f}});
    scene.objects.push_back({"Ground", coords(coord_axis::down)*0.5f, {1,1,1}, plane, marble, {{0.5f,0.5f,0.5f}, 0.5f, 0.0f}});
    for(int i=0; i<3; ++i) for(int j=0; j<3; ++j)
    {
        scene.objects.push_back({to_string("Sphere ", static_cast<char>('A'+i*3+j)), coords(coord_axis::right)*(i*2-2.f) + coords(coord_axis::forward)*(j*2-2.f), {1,1,1}, sphere, checker, {{1,1,1}, (j+0.5f)/3, (i+0.5f)/3}});
    }
    
    // Create our device and load our device objects
    gfx::context context;
    auto debug = [](const char * message) { std::cerr << message << std::endl; };
    auto dev = context.get_backends().back().create_device(debug);

    standard_device_objects standard = {dev, standard_sh};
    canvas_device_objects canvas_objects {*dev, compiler, sheet};
    for(auto m : assets.meshes) m->gmesh = {*dev, m->cmesh.vertices, m->cmesh.triangles};
    for(auto t : assets.textures)
    {
        auto im = loader.load_image(t->name);
        t->gtex = dev->create_image({rhi::image_shape::_2d, {im.dimensions,1}, 1, im.format, rhi::sampled_image_bit}, {im.get_pixels()});
    }

    // Samplers
    auto linear = dev->create_sampler({rhi::filter::linear, rhi::filter::linear, std::nullopt, rhi::address_mode::clamp_to_edge, rhi::address_mode::repeat});

    // Images
    auto env_spheremap = dev->create_image({rhi::image_shape::_2d, {env_spheremap_img.dimensions,1}, 1, env_spheremap_img.format, rhi::sampled_image_bit}, {env_spheremap_img.get_pixels()});

    auto pipelines = create_pipelines(*dev, compiler);

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

    gizmo gizmo{*pipelines.colored_pbr_pipe, arrow_x, arrow_y, arrow_z};
    editor editor{assets, scene, gwindow};

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
        canvas canvas {sprites, canvas_objects, pool};
        const gui_style style {face, icons};
        gui g {gs, canvas, style, gwindow->get_glfw_window()};
        editor.on_gui(g, timestep);

        auto & fb = gwindow->get_rhi_window().get_swapchain_framebuffer();
        auto vp = editor.viewport_rect;

        // Set up per scene uniforms
        struct pbr_scene_uniforms per_scene_uniforms {};
        int num_lights = 0;
        for(auto & obj : scene.objects)
        {
            if(obj.light == float3{0,0,0}) continue;
            per_scene_uniforms.point_lights[num_lights++] = {obj.position, obj.light};
            if(num_lights == 4) break;
        }

        auto per_scene_set = pool.alloc_descriptor_set(*pipelines.common_layout, pbr_per_scene_set_index);
        per_scene_set.write(0, per_scene_uniforms);
        per_scene_set.write(1, standard.get_image_sampler(), standard.get_brdf_integral_image());
        per_scene_set.write(2, standard.get_cubemap_sampler(), *env.irradiance_cubemap);
        per_scene_set.write(3, standard.get_cubemap_sampler(), *env.reflectance_cubemap);

        // Set up per-view uniforms for a specific framebuffer
        pbr_view_uniforms per_view_uniforms;
        per_view_uniforms.view_proj_matrix = editor.cam.get_view_proj_matrix(vp.aspect_ratio(), fb.get_ndc_coords(), dev->get_info().z_range);
        per_view_uniforms.skybox_view_proj_matrix = editor.cam.get_skybox_view_proj_matrix(vp.aspect_ratio(), fb.get_ndc_coords(), dev->get_info().z_range);
        per_view_uniforms.eye_position = editor.cam.position;
        per_view_uniforms.right_vector = editor.cam.get_direction(coord_axis::right);
        per_view_uniforms.down_vector = editor.cam.get_direction(coord_axis::down);

        auto per_view_set = pool.alloc_descriptor_set(*pipelines.common_layout, pbr_per_view_set_index);
        per_view_set.write(0, per_view_uniforms);

        // Draw objects to our primary framebuffer
        auto cmd = dev->create_command_buffer();
        rhi::render_pass_desc pass;
        pass.color_attachments = {{rhi::clear_color{0.05f,0.05f,0.05f,1.0f}, rhi::store{rhi::layout::present_source}}};
        pass.depth_attachment = {rhi::clear_depth{1.0f,0}, rhi::dont_care{}};
        cmd->begin_render_pass(pass, fb);
        cmd->set_viewport_rect(vp.x0, vp.y0, vp.x1, vp.y1);

        // Bind common descriptors
        per_scene_set.bind(*cmd);
        per_view_set.bind(*cmd);

        // Draw skybox
        cmd->bind_pipeline(*pipelines.skybox_pipe);
        auto skybox_set = pool.alloc_descriptor_set(*pipelines.skybox_pipe, pbr_per_material_set_index);
        skybox_set.write(0, standard.get_cubemap_sampler(), *env.environment_cubemap);
        skybox_set.bind(*cmd);
        box->gmesh.draw(*cmd);

        // Draw our objects
        for(auto & object : scene.objects)
        {
            if(!object.mesh || !object.albedo) continue;
            bool is_light = object.light != float3{0,0,0};
            auto & pipe = is_light ? pipelines.light_pipe : pipelines.textured_pbr_pipe;

            cmd->bind_pipeline(*pipe);

            auto material_set = pool.alloc_descriptor_set(*pipe, pbr_per_material_set_index);
            material_set.write(0, object.material);
            if(!is_light) material_set.write(1, *linear, *object.albedo->gtex);
            material_set.bind(*cmd);

            auto object_set = pool.alloc_descriptor_set(*pipe, pbr_per_object_set_index);
            object_set.write(0, object.get_object_uniforms());
            object_set.bind(*cmd);

            object.mesh->gmesh.draw(*cmd);
        }

        // Draw our gizmo
        if(editor.selection)
        {
            gizmo.draw(*cmd, pool, editor.selection->position);
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
