#include "engine/pbr.h"
#include "engine/mesh.h"
#include "engine/camera.h"
#include "engine/load.h"

#include <chrono>
#include <iostream>

constexpr coord_system coords {coord_axis::right, coord_axis::forward, coord_axis::up};
int main(int argc, const char * argv[]) try
{
    // Register asset paths
    loader loader;
    loader.register_root(get_program_binary_path() + "../../assets");

    shader_compiler compiler{loader};
    auto standard_sh = standard_shaders::compile(compiler);
    auto vs = compiler.compile_file(rhi::shader_stage::vertex, "static-mesh.vert");
    auto lit_fs = compiler.compile_file(rhi::shader_stage::fragment, "textured-pbr.frag");
    auto unlit_fs = compiler.compile_file(rhi::shader_stage::fragment, "colored-unlit.frag");
    auto skybox_vs = compiler.compile_file(rhi::shader_stage::vertex, "skybox.vert");
    auto skybox_fs = compiler.compile_file(rhi::shader_stage::fragment, "skybox.frag");

    auto env_spheremap_img = loader.load_image("monument-valley.hdr", true);
    auto ground_mesh = make_quad_mesh(coords(coord_axis::right)*8.0f, coords(coord_axis::forward)*8.0f);
    auto box_mesh = make_box_mesh({-0.3f,-0.3f,-0.3f}, {0.3f,0.3f,0.3f});
    auto sphere_mesh = make_sphere_mesh(32, 32, 0.5f);

    camera cam {coords};
    cam.pitch += 0.8f;
    cam.move(coord_axis::back, 10.0f);
    
    // Create a session for each device
    gfx::context context;
    auto debug = [](const char * message) { std::cerr << message << std::endl; };
    auto dev = context.get_backends().back().create_device(debug);

    standard_device_objects standard = {dev, standard_sh};
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
    auto skybox_material_layout = dev->create_descriptor_set_layout({
        {0, rhi::descriptor_type::combined_image_sampler, 1}
    });
    auto colored_pbr_layout = dev->create_descriptor_set_layout({
        {0, rhi::descriptor_type::uniform_buffer, 1},
    });
    auto textured_pbr_layout = dev->create_descriptor_set_layout({
        {0, rhi::descriptor_type::uniform_buffer, 1},
        {1, rhi::descriptor_type::combined_image_sampler, 1}
    });
    auto static_object_layout = dev->create_descriptor_set_layout({
        {0, rhi::descriptor_type::uniform_buffer, 1},
    });

    // Pipeline layouts
    auto common_layout = dev->create_pipeline_layout({per_scene_layout, per_view_layout});
    auto skybox_layout = dev->create_pipeline_layout({per_scene_layout, per_view_layout, skybox_material_layout});
    auto colored_object_layout = dev->create_pipeline_layout({per_scene_layout, per_view_layout, colored_pbr_layout, static_object_layout});
    auto textured_object_layout = dev->create_pipeline_layout({per_scene_layout, per_view_layout, textured_pbr_layout, static_object_layout});

    const auto mesh_vertex_binding = gfx::vertex_binder<mesh_vertex>(0)
        .attribute(0, &mesh_vertex::position)
        .attribute(1, &mesh_vertex::normal)
        .attribute(2, &mesh_vertex::texcoord)
        .attribute(3, &mesh_vertex::tangent)
        .attribute(4, &mesh_vertex::bitangent);

    // Shaders
    auto vss = dev->create_shader(vs), lit_fss = dev->create_shader(lit_fs), unlit_fss = dev->create_shader(unlit_fs);
    auto skybox_vss = dev->create_shader(skybox_vs), skybox_fss = dev->create_shader(skybox_fs);

    // Blend states
    const rhi::blend_state opaque {false};
    const rhi::blend_state translucent {true, {rhi::blend_factor::source_alpha, rhi::blend_op::add, rhi::blend_factor::one_minus_source_alpha}, {rhi::blend_factor::source_alpha, rhi::blend_op::add, rhi::blend_factor::one_minus_source_alpha}};

    // Pipelines
    auto light_pipe = dev->create_pipeline({textured_object_layout, {mesh_vertex_binding}, {vss,unlit_fss}, rhi::primitive_topology::triangles, rhi::front_face::counter_clockwise, rhi::cull_mode::none, rhi::compare_op::less, true, {opaque}});       
    auto solid_pipe = dev->create_pipeline({textured_object_layout, {mesh_vertex_binding}, {vss,lit_fss}, rhi::primitive_topology::triangles, rhi::front_face::counter_clockwise, rhi::cull_mode::none, rhi::compare_op::less, true, {opaque}});       
    auto skybox_pipe = dev->create_pipeline({skybox_layout, {mesh_vertex_binding}, {skybox_vss,skybox_fss}, rhi::primitive_topology::triangles, rhi::front_face::clockwise, rhi::cull_mode::none, rhi::compare_op::always, false, {opaque}});

    // Create transient resources
    gfx::transient_resource_pool pools[3] {*dev, *dev, *dev};
    int pool_index=0;

    // Do some initial work
    pools[pool_index].begin_frame(*dev);
    auto env = standard.create_environment_map_from_spheremap(pools[pool_index], *env_spheremap, 512, coords);
    pools[pool_index].end_frame(*dev);

    // Window
    auto gwindow = std::make_unique<gfx::window>(*dev, int2{1280,720}, to_string("Workbench 2018 - PBR Test"));

    // Main loop
    double2 last_cursor;
    auto t0 = std::chrono::high_resolution_clock::now();
    while(!gwindow->should_close())
    {
        // Poll events
        context.poll_events();

        // Compute timestep
        const auto t1 = std::chrono::high_resolution_clock::now();
        const auto timestep = std::chrono::duration<float>(t1-t0).count();
        t0 = t1;

        // Handle input
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

        auto per_scene_set = pool.alloc_descriptor_set(*common_layout, pbr_per_scene_set_index);
        per_scene_set.write(0, pool.uniforms.upload(per_scene_uniforms));
        per_scene_set.write(1, standard.get_image_sampler(), standard.get_brdf_integral_image());
        per_scene_set.write(2, standard.get_cubemap_sampler(), *env.irradiance_cubemap);
        per_scene_set.write(3, standard.get_cubemap_sampler(), *env.reflectance_cubemap);

        auto cmd = dev->create_command_buffer();

        // Set up per-view uniforms for a specific framebuffer
        auto & fb = gwindow->get_rhi_window().get_swapchain_framebuffer();
        const auto proj_matrix = mul(linalg::perspective_matrix(1.0f, gwindow->get_aspect(), 0.1f, 100.0f, linalg::pos_z, dev->get_info().z_range), make_transform_4x4(cam.coords, fb.get_ndc_coords()));

        pbr_view_uniforms per_view_uniforms;
        per_view_uniforms.view_proj_matrix = mul(proj_matrix, cam.get_view_matrix());
        per_view_uniforms.skybox_view_proj_matrix = mul(proj_matrix, cam.get_skybox_view_matrix());
        per_view_uniforms.eye_position = cam.position;
        per_view_uniforms.right_vector = cam.get_direction(coord_axis::right);
        per_view_uniforms.down_vector = cam.get_direction(coord_axis::down);

        auto per_view_set = pool.alloc_descriptor_set(*common_layout, pbr_per_view_set_index);
        per_view_set.write(0, pool.uniforms.upload(per_view_uniforms));

        // Draw objects to our primary framebuffer
        rhi::render_pass_desc pass;
        pass.color_attachments = {{rhi::dont_care{}, rhi::store{rhi::layout::present_source}}};
        pass.depth_attachment = {rhi::clear_depth{1.0f,0}, rhi::dont_care{}};
        cmd->begin_render_pass(pass, fb);

        // Bind common descriptors
        per_scene_set.bind(*cmd);
        per_view_set.bind(*cmd);

        // Draw skybox
        cmd->bind_pipeline(*skybox_pipe);
        auto skybox_set = pool.alloc_descriptor_set(*skybox_pipe, pbr_per_material_set_index);
        skybox_set.write(0, standard.get_cubemap_sampler(), *env.environment_cubemap);
        skybox_set.bind(*cmd);
        box.draw(*cmd);
        
        // Draw lights
        cmd->bind_pipeline(*light_pipe);
        auto material_set = pool.alloc_descriptor_set(*light_pipe, pbr_per_material_set_index);
        material_set.write(0, pbr_material_uniforms{{0.5f,0.5f,0.5f},0.5f,0});
        material_set.write(1, *nearest, *checkerboard);
        material_set.bind(*cmd);
        for(auto & p : per_scene_uniforms.point_lights)
        {
            auto object_set = pool.alloc_descriptor_set(*light_pipe, pbr_per_object_set_index);
            object_set.write(0, pbr_object_uniforms{mul(translation_matrix(p.position), scaling_matrix(float3{0.5f}))});
            object_set.bind(*cmd);
            sphere.draw(*cmd);
        }

        // Draw the ground
        cmd->bind_pipeline(*solid_pipe);
        auto object_set = pool.alloc_descriptor_set(*solid_pipe, pbr_per_object_set_index);
        object_set.write(0, pbr_object_uniforms{translation_matrix(cam.coords(coord_axis::down)*0.5f)});
        object_set.bind(*cmd);
        ground.draw(*cmd);

        // Draw a bunch of spheres
        for(int i=0; i<6; ++i)
        {
            for(int j=0; j<6; ++j)
            {
                auto material_set = pool.alloc_descriptor_set(*solid_pipe, pbr_per_material_set_index);
                material_set.write(0, pbr_material_uniforms{{1,1,1},(j+0.5f)/6,(i+0.5f)/6});
                material_set.write(1, *nearest, *checkerboard);
                material_set.bind(*cmd);

                auto object_set = pool.alloc_descriptor_set(*solid_pipe, pbr_per_object_set_index);
                object_set.write(0, pbr_object_uniforms{translation_matrix(cam.coords(coord_axis::right)*(i*2-5.f) + cam.coords(coord_axis::forward)*(j*2-5.f))});
                object_set.bind(*cmd);
                sphere.draw(*cmd);
            }
        }

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
