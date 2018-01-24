#include "engine/pbr.h"
#include "engine/mesh.h"
#include "engine/sprite.h"
#include "engine/camera.h"

#include <chrono>
#include <iostream>

constexpr coord_system coords {coord_axis::right, coord_axis::forward, coord_axis::up};
class device_session
{
    rhi::ptr<rhi::device> dev;
    gfx::transient_resource_pool pools[3];
    int pool_index=0;

    standard_device_objects standard;
    gfx::simple_mesh ground, box, sphere;
    rhi::ptr<rhi::image> checkerboard;
    environment_map env;

    rhi::ptr<rhi::sampler> nearest;
    rhi::ptr<rhi::descriptor_set_layout> per_scene_layout, per_view_layout, per_object_layout, skybox_per_object_layout;
    rhi::ptr<rhi::pipeline_layout> common_layout, object_layout, skybox_layout;
    rhi::ptr<rhi::pipeline> light_pipe, solid_pipe, skybox_pipe;

    std::unique_ptr<gfx::window> gwindow;
    double2 last_cursor;
public:
    device_session(loader & loader, rhi::ptr<rhi::device> dev) : dev{dev}, pools{*dev, *dev, *dev}
    {
        shader_compiler compiler{loader};
        auto standard_sh = standard_shaders::compile(compiler);
        auto vs = compiler.compile_file(rhi::shader_stage::vertex, "static-mesh.vert");
        auto lit_fs = compiler.compile_file(rhi::shader_stage::fragment, "textured-pbr.frag");
        auto unlit_fs = compiler.compile_file(rhi::shader_stage::fragment, "colored-unlit.frag");
        auto skybox_vs = compiler.compile_file(rhi::shader_stage::vertex, "skybox.vert");
        auto skybox_fs = compiler.compile_file(rhi::shader_stage::fragment, "skybox.frag");
        auto ui_vs = compiler.compile_file(rhi::shader_stage::vertex, "ui.vert");
        auto ui_fs = compiler.compile_file(rhi::shader_stage::fragment, "ui.frag");

        standard = {dev, standard_sh};

        auto env_spheremap_img = loader.load_image("monument-valley.hdr");

        auto ground_mesh = make_quad_mesh({0.5f,0.5f,0.5f}, coords(coord_axis::right)*8.0f, coords(coord_axis::forward)*8.0f);
        auto box_mesh = make_box_mesh({1,0,0}, {-0.3f,-0.3f,-0.3f}, {0.3f,0.3f,0.3f});
        auto sphere_mesh = make_sphere_mesh(32, 32, 0.5f);

        // Buffers
        ground = {*dev, ground_mesh.vertices, ground_mesh.triangles};
        box = {*dev, box_mesh.vertices, box_mesh.triangles};
        sphere = {*dev, sphere_mesh.vertices, sphere_mesh.triangles};

        // Samplers
        nearest = dev->create_sampler({rhi::filter::nearest, rhi::filter::nearest, std::nullopt, rhi::address_mode::clamp_to_edge, rhi::address_mode::repeat});

        // Images
        const byte4 w{255,255,255,255}, g{128,128,128,255}, grid[]{w,g,w,g,g,w,g,w,w,g,w,g,g,w,g,w};
        checkerboard = dev->create_image({rhi::image_shape::_2d, {4,4,1}, 1, rhi::image_format::rgba_unorm8, rhi::sampled_image_bit}, {grid});
        auto env_spheremap = dev->create_image({rhi::image_shape::_2d, {env_spheremap_img.dimensions,1}, 1, env_spheremap_img.format, rhi::sampled_image_bit}, {env_spheremap_img.get_pixels()});

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
        per_object_layout = dev->create_descriptor_set_layout({
            {0, rhi::descriptor_type::uniform_buffer, 1},
            {1, rhi::descriptor_type::combined_image_sampler, 1}
        });
        skybox_per_object_layout = dev->create_descriptor_set_layout({
            {0, rhi::descriptor_type::combined_image_sampler, 1}
        });

        // Pipeline layouts
        common_layout = dev->create_pipeline_layout({per_scene_layout, per_view_layout});
        object_layout = dev->create_pipeline_layout({per_scene_layout, per_view_layout, per_object_layout});
        skybox_layout = dev->create_pipeline_layout({per_scene_layout, per_view_layout, skybox_per_object_layout});

        const auto mesh_vertex_binding = gfx::vertex_binder<mesh_vertex>(0)
            .attribute(0, &mesh_vertex::position)
            .attribute(1, &mesh_vertex::color)
            .attribute(2, &mesh_vertex::normal)
            .attribute(3, &mesh_vertex::texcoord);
        const auto ui_vertex_binding = gfx::vertex_binder<ui_vertex>(0)
            .attribute(0, &ui_vertex::position)
            .attribute(1, &ui_vertex::texcoord)
            .attribute(2, &ui_vertex::color);        

        // Shaders
        auto vss = dev->create_shader(vs), lit_fss = dev->create_shader(lit_fs), unlit_fss = dev->create_shader(unlit_fs);
        auto skybox_vss = dev->create_shader(skybox_vs), skybox_fss = dev->create_shader(skybox_fs);

        // Blend states
        const rhi::blend_state opaque {false};
        const rhi::blend_state translucent {true, {rhi::blend_factor::source_alpha, rhi::blend_op::add, rhi::blend_factor::one_minus_source_alpha}, {rhi::blend_factor::source_alpha, rhi::blend_op::add, rhi::blend_factor::one_minus_source_alpha}};

        // Pipelines
        light_pipe = dev->create_pipeline({object_layout, {mesh_vertex_binding}, {vss,unlit_fss}, rhi::primitive_topology::triangles, rhi::front_face::counter_clockwise, rhi::cull_mode::none, rhi::compare_op::less, true, {opaque}});       
        solid_pipe = dev->create_pipeline({object_layout, {mesh_vertex_binding}, {vss,lit_fss}, rhi::primitive_topology::triangles, rhi::front_face::counter_clockwise, rhi::cull_mode::none, rhi::compare_op::less, true, {opaque}});       
        skybox_pipe = dev->create_pipeline({skybox_layout, {mesh_vertex_binding}, {skybox_vss,skybox_fss}, rhi::primitive_topology::triangles, rhi::front_face::clockwise, rhi::cull_mode::none, rhi::compare_op::always, false, {opaque}});

        // Do some initial work
        pools[pool_index].begin_frame(*dev);
        env = standard.create_environment_map_from_spheremap(pools[pool_index], *env_spheremap, 512, coords);
        pools[pool_index].end_frame(*dev);

        // Window
        gwindow = std::make_unique<gfx::window>(*dev, int2{1280,720}, to_string("Workbench 2018 - PBR Test"));
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
        const auto proj_matrix = mul(linalg::perspective_matrix(1.0f, gwindow->get_aspect(), 0.1f, 100.0f, linalg::pos_z, dev->get_info().z_range), make_transform_4x4(cam.coords, fb.get_ndc_coords()));

        pbr_per_view_uniforms per_view_uniforms;
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
        per_object.model_matrix = translation_matrix(cam.coords(coord_axis::down)*0.5f);
        per_object.roughness = 0.5f;
        per_object.metalness = 0.0f;
        auto ground_set = pool.descriptors->alloc(*per_object_layout);
        ground_set->write(0, pool.uniforms.upload(per_object));
        ground_set->write(1, *nearest, *checkerboard);
        cmd->bind_descriptor_set(*object_layout, pbr_per_object_set_index, *ground_set);
        ground.draw(*cmd);

        // Draw a bunch of spheres
        for(int i=0; i<6; ++i)
        {
            for(int j=0; j<6; ++j)
            {
                per_object.model_matrix = translation_matrix(cam.coords(coord_axis::right)*(i*2-5.f) + cam.coords(coord_axis::forward)*(j*2-5.f));
                per_object.roughness = (j+0.5f)/6;
                per_object.metalness = (i+0.5f)/6;
                auto sphere_set = pool.descriptors->alloc(*per_object_layout);
                sphere_set->write(0, pool.uniforms.upload(per_object));
                sphere_set->write(1, *nearest, *checkerboard);
                cmd->bind_descriptor_set(*object_layout, pbr_per_object_set_index, *sphere_set);
                sphere.draw(*cmd);
            }
        }

        cmd->end_render_pass();
        dev->acquire_and_submit_and_present(*cmd, gwindow->get_rhi_window());
        pool.end_frame(*dev);
    }
};

int main(int argc, const char * argv[]) try
{
    // Register asset paths
    loader loader;
    loader.register_root(get_program_binary_path() + "../../assets");
    
    // Loader assets and initialize state
    camera cam {coords};
    cam.pitch += 0.8f;
    cam.move(coord_axis::back, 10.0f);
    
    // Create a session for each device
    gfx::context context;
    auto debug = [](const char * message) { std::cerr << message << std::endl; };
    auto dev = context.get_backends().back().create_device(debug);
    device_session session {loader, dev};

    // Main loop
    auto t0 = std::chrono::high_resolution_clock::now();
    bool running = true;
    while(running)
    {
        // Render frame
        session.render_frame(cam);

        // Poll events
        context.poll_events();

        // Compute timestep
        const auto t1 = std::chrono::high_resolution_clock::now();
        const auto timestep = std::chrono::duration<float>(t1-t0).count();
        t0 = t1;

        // Handle input
        if(!session.update(cam, timestep)) break;
    }
    return EXIT_SUCCESS;
}
catch(const std::exception & e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
