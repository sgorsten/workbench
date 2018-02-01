#include "engine/pbr.h"
#include "engine/mesh.h"
#include "engine/sprite.h"
#include "engine/camera.h"

#include <chrono>
#include <iostream>

struct common_assets
{
    coord_system game_coords;
    shader_compiler compiler;
    standard_shaders standard;
    rhi::shader_desc vs, lit_fs, unlit_fs, skybox_vs, skybox_fs;
    image env_spheremap;
    mesh ground_mesh, box_mesh, sphere_mesh;

    sprite_sheet sheet;
    canvas_sprites sprites;
    font_face face;

    common_assets(loader & loader) : game_coords {coord_axis::right, coord_axis::forward, coord_axis::up}, 
        compiler{loader}, standard{standard_shaders::compile(compiler)},
        sprites{sheet}, face{sheet, loader.load_binary_file("arial.ttf"), 20, 0x20, 0x7E}
    {
        vs = compiler.compile_file(rhi::shader_stage::vertex, "static-mesh.vert");
        lit_fs = compiler.compile_file(rhi::shader_stage::fragment, "textured-pbr.frag");
        unlit_fs = compiler.compile_file(rhi::shader_stage::fragment, "colored-unlit.frag");
        skybox_vs = compiler.compile_file(rhi::shader_stage::vertex, "skybox.vert");
        skybox_fs = compiler.compile_file(rhi::shader_stage::fragment, "skybox.frag");

        env_spheremap = loader.load_image("monument-valley.hdr", true);

        ground_mesh = make_quad_mesh(game_coords(coord_axis::right)*8.0f, game_coords(coord_axis::forward)*8.0f);
        box_mesh = make_box_mesh({-0.3f,-0.3f,-0.3f}, {0.3f,0.3f,0.3f});
        sphere_mesh = make_sphere_mesh(32, 32, 0.5f);

        sheet.prepare_sheet();
    }
};

class device_session
{
    const common_assets & assets;
    rhi::ptr<rhi::device> dev;
    standard_device_objects standard;
    canvas_device_objects canvas_objects;
    rhi::device_info info;
    gfx::transient_resource_pool pools[3];
    int pool_index=0;

    gfx::simple_mesh ground, box, sphere;

    rhi::ptr<rhi::image> font_image;
    rhi::ptr<rhi::image> checkerboard;
    environment_map env;

    rhi::ptr<rhi::sampler> nearest;

    rhi::ptr<rhi::descriptor_set_layout> per_scene_layout, per_view_layout, skybox_material_layout, pbr_material_layout, static_object_layout;
    rhi::ptr<rhi::pipeline_layout> common_layout, object_layout, skybox_layout;
    rhi::ptr<rhi::pipeline> light_pipe, solid_pipe, skybox_pipe;

    std::unique_ptr<gfx::window> gwindow;
    double2 last_cursor;
public:
    device_session(common_assets & assets, const std::string & name, rhi::ptr<rhi::device> dev, const int2 & window_pos) : 
        assets{assets}, dev{dev}, standard{dev, assets.standard}, canvas_objects{*dev, assets.compiler, assets.sheet}, info{dev->get_info()}, pools{*dev, *dev, *dev}
    {
        // Buffers
        ground = {*dev, assets.ground_mesh.vertices, assets.ground_mesh.triangles};
        box = {*dev, assets.box_mesh.vertices, assets.box_mesh.triangles};
        sphere = {*dev, assets.sphere_mesh.vertices, assets.sphere_mesh.triangles};

        // Samplers
        nearest = dev->create_sampler({rhi::filter::nearest, rhi::filter::nearest, std::nullopt, rhi::address_mode::clamp_to_edge, rhi::address_mode::repeat});

        // Images
        const byte4 w{255,255,255,255}, g{128,128,128,255}, grid[]{w,g,w,g,g,w,g,w,w,g,w,g,g,w,g,w};
        checkerboard = dev->create_image({rhi::image_shape::_2d, {4,4,1}, 1, rhi::image_format::rgba_unorm8, rhi::sampled_image_bit}, {grid});
        font_image = dev->create_image({rhi::image_shape::_2d, {assets.sheet.sheet_image.dims(),1}, 1, rhi::image_format::r_unorm8, rhi::sampled_image_bit}, {assets.sheet.sheet_image.data()});
        auto env_spheremap = dev->create_image({rhi::image_shape::_2d, {assets.env_spheremap.dimensions,1}, 1, assets.env_spheremap.format, rhi::sampled_image_bit}, {assets.env_spheremap.get_pixels()});

        // Descriptor set layouts
        per_scene_layout = dev->create_descriptor_set_layout({
            {0, rhi::descriptor_type::uniform_buffer, 1},
            {1, rhi::descriptor_type::combined_image_sampler, 1},
            {2, rhi::descriptor_type::combined_image_sampler, 1},
            {3, rhi::descriptor_type::combined_image_sampler, 1}
        });
        per_view_layout = dev->create_descriptor_set_layout({
            {0, rhi::descriptor_type::uniform_buffer, 1}
        });
        skybox_material_layout = dev->create_descriptor_set_layout({
            {0, rhi::descriptor_type::combined_image_sampler, 1}
        });
        pbr_material_layout = dev->create_descriptor_set_layout({
            {0, rhi::descriptor_type::uniform_buffer, 1},
            {1, rhi::descriptor_type::combined_image_sampler, 1}
        });
        static_object_layout = dev->create_descriptor_set_layout({
            {0, rhi::descriptor_type::uniform_buffer, 1},
        });

        // Pipeline layouts
        common_layout = dev->create_pipeline_layout({per_scene_layout, per_view_layout});
        skybox_layout = dev->create_pipeline_layout({per_scene_layout, per_view_layout, skybox_material_layout});
        object_layout = dev->create_pipeline_layout({per_scene_layout, per_view_layout, pbr_material_layout, static_object_layout});

        const auto mesh_vertex_binding = gfx::vertex_binder<mesh_vertex>(0)
            .attribute(0, &mesh_vertex::position)
            .attribute(1, &mesh_vertex::normal)
            .attribute(2, &mesh_vertex::texcoord)
            .attribute(3, &mesh_vertex::tangent)
            .attribute(4, &mesh_vertex::bitangent);
        const auto ui_vertex_binding = gfx::vertex_binder<ui_vertex>(0)
            .attribute(0, &ui_vertex::position)
            .attribute(1, &ui_vertex::texcoord)
            .attribute(2, &ui_vertex::color);        

        // Shaders
        auto vs = dev->create_shader(assets.vs), lit_fs = dev->create_shader(assets.lit_fs), unlit_fs = dev->create_shader(assets.unlit_fs);
        auto skybox_vs = dev->create_shader(assets.skybox_vs), skybox_fs = dev->create_shader(assets.skybox_fs);

        // Blend states
        const rhi::blend_state opaque {false};
        const rhi::blend_state translucent {true, {rhi::blend_factor::source_alpha, rhi::blend_op::add, rhi::blend_factor::one_minus_source_alpha}, {rhi::blend_factor::source_alpha, rhi::blend_op::add, rhi::blend_factor::one_minus_source_alpha}};

        // Pipelines
        light_pipe = dev->create_pipeline({object_layout, {mesh_vertex_binding}, {vs,unlit_fs}, rhi::primitive_topology::triangles, rhi::front_face::counter_clockwise, rhi::cull_mode::none, rhi::compare_op::less, true, {opaque}});       
        solid_pipe = dev->create_pipeline({object_layout, {mesh_vertex_binding}, {vs,lit_fs}, rhi::primitive_topology::triangles, rhi::front_face::counter_clockwise, rhi::cull_mode::none, rhi::compare_op::less, true, {opaque}});       
        skybox_pipe = dev->create_pipeline({skybox_layout, {mesh_vertex_binding}, {skybox_vs,skybox_fs}, rhi::primitive_topology::triangles, rhi::front_face::clockwise, rhi::cull_mode::none, rhi::compare_op::always, false, {opaque}});

        // Do some initial work
        pools[pool_index].begin_frame(*dev);
        env = standard.create_environment_map_from_spheremap(pools[pool_index], *env_spheremap, 512, assets.game_coords);
        pools[pool_index].end_frame(*dev);

        // Window
        gwindow = std::make_unique<gfx::window>(*dev, int2{512,512}, to_string("Workbench 2018 Render Test (", name, ")"));
        gwindow->set_pos(window_pos); 
    }

    bool update(camera & cam, float timestep)
    {
        const double2 cursor = gwindow->get_cursor_pos();
        if(gwindow->get_mouse_button(GLFW_MOUSE_BUTTON_LEFT))
        {
            cam.yaw += static_cast<float>(cursor.x - last_cursor.x) * 0.01f;
            cam.pitch = std::min(std::max(cam.pitch + static_cast<float>(cursor.y - last_cursor.y) * 0.01f, -1.5f), +1.5f);
        }
        last_cursor = cursor;

        const float cam_speed = timestep * 10;
        if(gwindow->get_key(GLFW_KEY_W)) cam.move(coord_axis::forward, cam_speed);
        if(gwindow->get_key(GLFW_KEY_A)) cam.move(coord_axis::left, cam_speed);
        if(gwindow->get_key(GLFW_KEY_S)) cam.move(coord_axis::back, cam_speed);
        if(gwindow->get_key(GLFW_KEY_D)) cam.move(coord_axis::right, cam_speed);

        return !gwindow->should_close();
    }

    void render_frame(const camera & cam)
    {       
        // Reset resources
        pool_index = (pool_index+1)%3;
        auto & pool = pools[pool_index];
        pool.begin_frame(*dev);

        // Set up per scene uniforms
        struct pbr_scene_uniforms per_scene_uniforms {};
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
        const auto proj_matrix = mul(linalg::perspective_matrix(1.0f, gwindow->get_aspect(), 0.1f, 100.0f, linalg::pos_z, info.z_range), make_transform_4x4(cam.coords, fb.get_ndc_coords()));

        pbr_view_uniforms per_view_uniforms;
        per_view_uniforms.view_proj_matrix = mul(proj_matrix, cam.get_view_matrix());
        per_view_uniforms.skybox_view_proj_matrix = mul(proj_matrix, cam.get_skybox_view_matrix());
        per_view_uniforms.eye_position = cam.position;
        per_view_uniforms.right_vector = cam.get_direction(coord_axis::right);
        per_view_uniforms.down_vector = cam.get_direction(coord_axis::down);

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
        auto skybox_set = pool.descriptors->alloc(*skybox_material_layout);
        skybox_set->write(0, standard.get_cubemap_sampler(), *env.environment_cubemap);
        cmd->bind_descriptor_set(*skybox_layout, pbr_per_material_set_index, *skybox_set);
        box.draw(*cmd);

        // Draw lights
        cmd->bind_pipeline(*light_pipe);
        auto material_set = pool.descriptors->alloc(*pbr_material_layout);
        material_set->write(0, pool.uniforms.upload(pbr_material_uniforms{{0.5f,0.5f,0.5f},0.5f,0}));
        material_set->write(1, *nearest, *checkerboard);
        cmd->bind_descriptor_set(*object_layout, pbr_per_material_set_index, *material_set);
        for(auto & p : per_scene_uniforms.point_lights)
        {
            auto object_set = pool.descriptors->alloc(*static_object_layout);
            object_set->write(0, pool.uniforms.upload(pbr_object_uniforms{mul(translation_matrix(p.position), scaling_matrix(float3{0.5f}))}));
            cmd->bind_descriptor_set(*object_layout, pbr_per_object_set_index, *object_set);

            sphere.draw(*cmd);
        }

        // Draw the ground
        cmd->bind_pipeline(*solid_pipe);
        auto object_set = pool.descriptors->alloc(*static_object_layout);
        object_set->write(0, pool.uniforms.upload(pbr_object_uniforms{translation_matrix(cam.coords(coord_axis::down)*0.5f)}));
        cmd->bind_descriptor_set(*object_layout, pbr_per_object_set_index, *object_set);
        ground.draw(*cmd);

        // Draw a bunch of spheres
        for(int i=0; i<6; ++i)
        {
            for(int j=0; j<6; ++j)
            {
                material_set = pool.descriptors->alloc(*pbr_material_layout);
                material_set->write(0, pool.uniforms.upload(pbr_material_uniforms{{1,1,1},(j+0.5f)/6,(i+0.5f)/6}));
                material_set->write(1, *nearest, *checkerboard);
                cmd->bind_descriptor_set(*object_layout, pbr_per_material_set_index, *material_set);

                object_set = pool.descriptors->alloc(*static_object_layout);
                object_set->write(0, pool.uniforms.upload(pbr_object_uniforms{translation_matrix(cam.coords(coord_axis::right)*(i*2-5.f) + cam.coords(coord_axis::forward)*(j*2-5.f))}));
                cmd->bind_descriptor_set(*object_layout, pbr_per_object_set_index, *object_set);

                sphere.draw(*cmd);
            }
        }

        // Draw the UI
        canvas canvas {assets.sprites, canvas_objects, pool};
        canvas.set_target(0, {{0,0},gwindow->get_window_size()}, nullptr);
        canvas.draw_rounded_rect({10,10,140,30+assets.face.line_height}, 6, {0,0,0,0.8f});
        canvas.draw_shadowed_text({20,20}, {1,1,1,1}, assets.face, "This is a test");
        canvas.encode_commands(*cmd, *gwindow);

        cmd->end_render_pass();
        dev->acquire_and_submit_and_present(*cmd, gwindow->get_rhi_window());

        pool.end_frame(*dev);
    }
};

int main(int argc, const char * argv[]) try
{
    // Run tests, if requested
    if(argc > 1 && strcmp("--test", argv[1]) == 0)
    {
        doctest::Context dt_context;
        dt_context.applyCommandLine(argc-1, argv+1);
        const int dt_return = dt_context.run();
        if(dt_context.shouldExit()) return dt_return;
    }

    // Register asset paths
    std::cout << "Running from " << get_program_binary_path() << std::endl;
    loader loader;
    loader.register_root(get_program_binary_path() + "../../assets");
    loader.register_root("C:/windows/fonts");
    
    // Loader assets and initialize state
    common_assets assets{loader};
    camera cam {assets.game_coords};
    cam.pitch += 0.8f;
    cam.move(coord_axis::back, 10.0f);
    
    // Create a session for each device
    gfx::context context;
    auto debug = [](const char * message) { std::cerr << message << std::endl; };
    int2 pos{100,100};
    std::vector<std::unique_ptr<device_session>> sessions;
    for(auto & backend : context.get_backends())
    {
        std::cout << "Initializing " << backend.name << " backend:\n";
        sessions.push_back(std::make_unique<device_session>(assets, backend.name, backend.create_device(debug), pos));
        std::cout << backend.name << " has been initialized." << std::endl;
        pos.x += 600;
    }

    // Main loop
    auto t0 = std::chrono::high_resolution_clock::now();
    bool running = true;
    while(running)
    {
        // Render frame
        for(auto & s : sessions) s->render_frame(cam);

        // Poll events
        context.poll_events();

        // Compute timestep
        const auto t1 = std::chrono::high_resolution_clock::now();
        const auto timestep = std::chrono::duration<float>(t1-t0).count();
        t0 = t1;

        // Handle input
        for(auto & s : sessions) if(!s->update(cam, timestep)) running = false;
    }
    return EXIT_SUCCESS;
}
catch(const std::exception & e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
