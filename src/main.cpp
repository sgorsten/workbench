#include "graphics.h"
#include "io.h"
#include "pbr.h"
#include "load.h"
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>
#include <chrono>
#include <iostream>

// A viewpoint in space from which the scene will be viewed
struct camera
{
    coord_system coords;
    float3 position;
    float pitch, yaw;

    float4 get_orientation() const { return qmul(rotation_quat(coords.cross(coord_axis::forward, coord_axis::right), yaw), rotation_quat(coords.cross(coord_axis::forward, coord_axis::down), pitch)); }
    float3 get_direction(coord_axis axis) const { return qrot(get_orientation(), coords(axis)); }

    rigid_transform get_pose() const { return {get_orientation(), position}; }
    float4x4 get_view_matrix() const { return get_pose().inverse().matrix(); }
    float4x4 get_skybox_view_matrix() const { return rotation_matrix(qconj(get_orientation())); }

    void move(coord_axis direction, float distance) { position += get_direction(direction) * distance; }
};

struct mesh_vertex
{
    float3 position, color, normal;
    float2 texcoord;
    static rhi::vertex_binding_desc get_binding(int index)
    {
        return gfx::vertex_binder<mesh_vertex>(index)
            .attribute(0, &mesh_vertex::position)
            .attribute(1, &mesh_vertex::color)
            .attribute(2, &mesh_vertex::normal)
            .attribute(3, &mesh_vertex::texcoord);
    }
};

struct mesh
{
    std::vector<mesh_vertex> vertices;
    std::vector<int2> lines;
    std::vector<int3> triangles;
};

mesh make_basis_mesh()
{
    mesh m;
    m.vertices = {{{0,0,0},{1,0,0}}, {{1,0,0},{1,0,0}}, {{0,0,0},{0,1,0}}, {{0,1,0},{0,1,0}}, {{0,0,0},{0,0,1}}, {{0,0,1},{0,0,1}}};
    m.lines = {{0,1},{2,3},{4,5}};
    return m;
}

mesh make_box_mesh(const float3 & color, const float3 & a, const float3 & b)
{
    mesh m;
    m.vertices = {
        {{a.x,a.y,a.z}, color, {-1,0,0}, {0,0}}, {{a.x,a.y,b.z}, color, {-1,0,0}, {0,1}}, {{a.x,b.y,b.z}, color, {-1,0,0}, {1,1}}, {{a.x,b.y,a.z}, color, {-1,0,0}, {1,0}},
        {{b.x,b.y,a.z}, color, {+1,0,0}, {0,0}}, {{b.x,b.y,b.z}, color, {+1,0,0}, {0,1}}, {{b.x,a.y,b.z}, color, {+1,0,0}, {1,1}}, {{b.x,a.y,a.z}, color, {+1,0,0}, {1,0}},
        {{a.x,a.y,a.z}, color, {0,-1,0}, {0,0}}, {{b.x,a.y,a.z}, color, {0,-1,0}, {0,1}}, {{b.x,a.y,b.z}, color, {0,-1,0}, {1,1}}, {{a.x,a.y,b.z}, color, {0,-1,0}, {1,0}},
        {{a.x,b.y,b.z}, color, {0,+1,0}, {0,0}}, {{b.x,b.y,b.z}, color, {0,+1,0}, {0,1}}, {{b.x,b.y,a.z}, color, {0,+1,0}, {1,1}}, {{a.x,b.y,a.z}, color, {0,+1,0}, {1,0}},
        {{a.x,a.y,a.z}, color, {0,0,-1}, {0,0}}, {{a.x,b.y,a.z}, color, {0,0,-1}, {0,1}}, {{b.x,b.y,a.z}, color, {0,0,-1}, {1,1}}, {{b.x,a.y,a.z}, color, {0,0,-1}, {1,0}},
        {{b.x,a.y,b.z}, color, {0,0,+1}, {0,0}}, {{b.x,b.y,b.z}, color, {0,0,+1}, {0,1}}, {{a.x,b.y,b.z}, color, {0,0,+1}, {1,1}}, {{a.x,a.y,b.z}, color, {0,0,+1}, {1,0}},
    };
    m.triangles = {{0,1,2},{0,2,3},{4,5,6},{4,6,7},{8,9,10},{8,10,11},{12,13,14},{12,14,15},{16,17,18},{16,18,19},{20,21,22},{20,22,23}};
    return m;
}

mesh make_sphere_mesh(int slices, int stacks, float radius)
{
    const auto make_vertex = [slices, stacks, radius](int i, int j)
    {
        const float tau = 6.2831853f, longitude = i*tau/slices, latitude = (j-(stacks*0.5f))*tau/2/stacks;
        const float3 normal {cos(longitude)*cos(latitude), sin(latitude), sin(longitude)*cos(latitude)}; // Poles at +/-y
        return mesh_vertex{normal*radius, {1,1,1}, normal, {(float)i/slices, (float)j/stacks}};
    };

    mesh m;
    for(int i=0; i<slices; ++i)
    {
        for(int j=0; j<stacks; ++j)
        {
            int base = exactly(m.vertices.size());
            m.vertices.push_back(make_vertex(i,j));
            m.vertices.push_back(make_vertex(i,j+1));
            m.vertices.push_back(make_vertex(i+1,j+1));
            m.vertices.push_back(make_vertex(i+1,j));
            m.triangles.push_back(base+int3(0,1,2));
            m.triangles.push_back(base+int3(0,2,3));
        }
    }
    return m;
}

mesh make_quad_mesh(const float3 & color, const float3 & tangent_s, const float3 & tangent_t)
{
    const auto normal = normalize(cross(tangent_s, tangent_t));
    mesh m;
    m.vertices =
    {
        {-tangent_s-tangent_t, color, normal, {0,0}},
        {+tangent_s-tangent_t, color, normal, {1,0}},
        {+tangent_s+tangent_t, color, normal, {1,1}},
        {-tangent_s+tangent_t, color, normal, {0,1}}
    };
    m.triangles = {{0,1,2},{0,2,3}};
    return m;
}

#define STB_TRUETYPE_IMPLEMENTATION  // force following include to generate implementation
#include "stb_truetype.h"

struct common_assets
{
    coord_system game_coords;
    standard_shaders standard;
    mesh basis_mesh, ground_mesh, box_mesh, sphere_mesh;
    rhi::shader_desc vs, fs, fs_unlit, ui_vs, ui_fs;
    rhi::shader_desc skybox_vs, skybox_fs_cubemap;
    image env_spheremap;
    std::vector<std::byte> font_buffer;

    common_assets(loader & loader) : game_coords {coord_axis::right, coord_axis::forward, coord_axis::up}
    {
        env_spheremap = loader.load_image("monument-valley.hdr");

        shader_compiler compiler{loader};
        standard = standard_shaders::compile(compiler);

        vs = compiler.compile_file(rhi::shader_stage::vertex, "static-mesh.vert");
        fs = compiler.compile_file(rhi::shader_stage::fragment, "textured-pbr.frag");
        fs_unlit = compiler.compile_file(rhi::shader_stage::fragment, "colored-unlit.frag");

        skybox_vs = compiler.compile_file(rhi::shader_stage::vertex, "skybox.vert");
        skybox_fs_cubemap = compiler.compile_file(rhi::shader_stage::fragment, "skybox.frag");

        ui_vs = compiler.compile_file(rhi::shader_stage::vertex, "ui.vert");
        ui_fs = compiler.compile_file(rhi::shader_stage::fragment, "ui.frag");

        basis_mesh = make_basis_mesh();
        ground_mesh = make_quad_mesh({0.5f,0.5f,0.5f}, game_coords(coord_axis::right)*8.0f, game_coords(coord_axis::forward)*8.0f);
        box_mesh = make_box_mesh({1,0,0}, {-0.3f,-0.3f,-0.3f}, {0.3f,0.3f,0.3f});
        sphere_mesh = make_sphere_mesh(32, 32, 0.5f);

        font_buffer = loader.load_binary_file("arialbd.ttf");
    }
};

class device_session
{
    rhi::ptr<rhi::device> dev;
    standard_device_objects standard;
    rhi::device_info info;
    rhi::ptr<rhi::descriptor_pool> desc_pool;
    gfx::dynamic_buffer uniform_buffer;
    uint64_t transient_resource_fence=0;

    gfx::static_buffer basis_vertex_buffer, ground_vertex_buffer, ground_index_buffer, box_vertex_buffer, box_index_buffer, sphere_vertex_buffer, sphere_index_buffer;
    gfx::static_buffer font_buffer;

    rhi::ptr<rhi::descriptor_set_layout> per_scene_layout, per_view_layout, per_object_layout, skybox_per_object_layout;
    rhi::ptr<rhi::pipeline_layout> common_layout, pipe_layout, skybox_pipe_layout;
    rhi::ptr<rhi::pipeline> wire_pipe, solid_pipe, skybox_pipe_cubemap, ui_pipe;

    rhi::ptr<rhi::sampler> nearest;

    rhi::ptr<rhi::image> font_image;

    rhi::ptr<rhi::image> checkerboard;
    environment_map env;

    std::unique_ptr<gfx::window> gwindow;
    double2 last_cursor;
public:
    device_session(const common_assets & assets, const std::string & name, rhi::ptr<rhi::device> dev, const int2 & window_pos) :
        dev{dev}, standard{dev, assets.standard}, info{dev->get_info()},
        uniform_buffer{*dev, rhi::buffer_usage::uniform, 1024*1024},
        basis_vertex_buffer{*dev, rhi::buffer_usage::vertex, assets.basis_mesh.vertices},
        ground_vertex_buffer{*dev, rhi::buffer_usage::vertex, assets.ground_mesh.vertices},
        box_vertex_buffer{*dev, rhi::buffer_usage::vertex, assets.box_mesh.vertices},
        sphere_vertex_buffer{*dev, rhi::buffer_usage::vertex, assets.sphere_mesh.vertices},
        ground_index_buffer{*dev, rhi::buffer_usage::index, assets.ground_mesh.triangles},
        box_index_buffer{*dev, rhi::buffer_usage::index, assets.box_mesh.triangles},
        sphere_index_buffer{*dev, rhi::buffer_usage::index, assets.sphere_mesh.triangles}
    {
        desc_pool = dev->create_descriptor_pool();

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
        common_layout = dev->create_pipeline_layout({per_scene_layout, per_view_layout});
        pipe_layout = dev->create_pipeline_layout({per_scene_layout, per_view_layout, per_object_layout});
        skybox_pipe_layout = dev->create_pipeline_layout({per_scene_layout, per_view_layout, skybox_per_object_layout});

        auto vs = dev->create_shader(assets.vs);
        auto fs = dev->create_shader(assets.fs);
        auto fs_unlit = dev->create_shader(assets.fs_unlit);
        auto skybox_vs = dev->create_shader(assets.skybox_vs);
        auto skybox_fs_cubemap = dev->create_shader(assets.skybox_fs_cubemap);

        auto ui_vs = dev->create_shader(assets.ui_vs);
        auto ui_fs = dev->create_shader(assets.ui_fs);

        const rhi::blend_state opaque {false};
        const rhi::blend_state translucent {true, {rhi::blend_factor::source_alpha, rhi::blend_op::add, rhi::blend_factor::one_minus_source_alpha}, {rhi::blend_factor::source_alpha, rhi::blend_op::add, rhi::blend_factor::one_minus_source_alpha}};

        wire_pipe = dev->create_pipeline({pipe_layout, {mesh_vertex::get_binding(0)}, {vs,fs_unlit}, rhi::primitive_topology::lines, rhi::front_face::counter_clockwise, rhi::cull_mode::none, rhi::compare_op::less, true, {opaque}});
        solid_pipe = dev->create_pipeline({pipe_layout, {mesh_vertex::get_binding(0)}, {vs,fs}, rhi::primitive_topology::triangles, rhi::front_face::counter_clockwise, rhi::cull_mode::none, rhi::compare_op::less, true, {opaque}});
        skybox_pipe_cubemap = dev->create_pipeline({skybox_pipe_layout, {mesh_vertex::get_binding(0)}, {skybox_vs,skybox_fs_cubemap}, rhi::primitive_topology::triangles, rhi::front_face::clockwise, rhi::cull_mode::none, rhi::compare_op::always, false, {opaque}});
        ui_pipe = dev->create_pipeline({pipe_layout, {render_image_vertex::get_binding(0)}, {ui_vs,ui_fs}, rhi::primitive_topology::triangles, rhi::front_face::counter_clockwise, rhi::cull_mode::none, rhi::compare_op::always, false, {translucent}});

        nearest = dev->create_sampler({rhi::filter::nearest, rhi::filter::nearest, std::nullopt, rhi::address_mode::clamp_to_edge, rhi::address_mode::repeat});

        {
            stbtt_fontinfo font;
            auto font_data = reinterpret_cast<const uint8_t *>(assets.font_buffer.data());
            stbtt_InitFont(&font, font_data, stbtt_GetFontOffsetForIndex(font_data, 0));

            int width, height;
            auto bitmap = stbtt_GetCodepointBitmap(&font, 0, stbtt_ScaleForPixelHeight(&font, 64), 'A', &width, &height, 0,0);
            font_image = dev->create_image({rhi::image_shape::_2d, {width,height,1}, 1, rhi::image_format::r_unorm8, rhi::sampled_image_bit}, {bitmap});

            render_image_vertex verts[]
            {
                {{-0.5f,-0.5f}, {0,0}},
                {{+0.5f,-0.5f}, {1,0}},
                {{+0.5f,+0.5f}, {1,1}},
                {{-0.5f,-0.5f}, {0,0}},
                {{+0.5f,+0.5f}, {1,1}},
                {{-0.5f,+0.5f}, {0,1}}
            };
            font_buffer = gfx::static_buffer{*dev, rhi::buffer_usage::vertex, verts};
        }

        const byte4 w{255,255,255,255}, g{128,128,128,255}, grid[]{w,g,w,g,g,w,g,w,w,g,w,g,g,w,g,w};
        checkerboard = dev->create_image({rhi::image_shape::_2d, {4,4,1}, 1, rhi::image_format::rgba_unorm8, rhi::sampled_image_bit}, {grid});

        auto env_spheremap = dev->create_image({rhi::image_shape::_2d, {assets.env_spheremap.dimensions,1}, 1, assets.env_spheremap.format, rhi::sampled_image_bit}, {assets.env_spheremap.pixels});
        env = standard.create_environment_map_from_spheremap(*desc_pool, uniform_buffer, *env_spheremap, 512, assets.game_coords);

        gwindow = std::make_unique<gfx::window>(*dev, int2{512,512}, to_string("Workbench 2018 Render Test (", name, ")"));
        gwindow->set_pos(window_pos); 
    }

    ~device_session()
    {
        dev->wait_until_complete(transient_resource_fence);
        gwindow.reset();
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
        dev->wait_until_complete(transient_resource_fence);
        desc_pool->reset();
        uniform_buffer.reset();

        // Set up per scene uniforms
        struct pbr_per_scene_uniforms per_scene_uniforms {};
        per_scene_uniforms.point_lights[0] = {{-3, -3, 8}, {23.47f, 21.31f, 20.79f}};
        per_scene_uniforms.point_lights[1] = {{ 3, -3, 8}, {23.47f, 21.31f, 20.79f}};
        per_scene_uniforms.point_lights[2] = {{ 3,  3, 8}, {23.47f, 21.31f, 20.79f}};
        per_scene_uniforms.point_lights[3] = {{-3,  3, 8}, {23.47f, 21.31f, 20.79f}};

        auto per_scene_set = desc_pool->alloc(*per_scene_layout);
        per_scene_set->write(0, uniform_buffer.write(per_scene_uniforms));
        per_scene_set->write(1, standard.get_image_sampler(), standard.get_brdf_integral_image());
        per_scene_set->write(2, standard.get_cubemap_sampler(), *env.irradiance_cubemap);
        per_scene_set->write(3, standard.get_cubemap_sampler(), *env.reflectance_cubemap);

        auto cmd = dev->create_command_buffer();

        // Set up per-view uniforms for a specific framebuffer
        auto & fb = gwindow->get_rhi_window().get_swapchain_framebuffer();
        const auto proj_matrix = mul(linalg::perspective_matrix(1.0f, gwindow->get_aspect(), 0.1f, 100.0f, linalg::pos_z, info.z_range), make_transform_4x4(cam.coords, fb.get_ndc_coords()));

        pbr_per_view_uniforms per_view_uniforms;
        per_view_uniforms.view_proj_matrix = mul(proj_matrix, cam.get_view_matrix());
        per_view_uniforms.skybox_view_proj_matrix = mul(proj_matrix, cam.get_skybox_view_matrix());
        per_view_uniforms.eye_position = cam.position;
        per_view_uniforms.right_vector = cam.get_direction(coord_axis::right);
        per_view_uniforms.down_vector = cam.get_direction(coord_axis::down);

        auto per_view_set = desc_pool->alloc(*per_view_layout);
        per_view_set->write(0, uniform_buffer.write(per_view_uniforms));

        // Draw objects to our primary framebuffer
        rhi::render_pass_desc pass;
        pass.color_attachments = {{rhi::dont_care{}, rhi::store{rhi::layout::present_source}}};
        pass.depth_attachment = {rhi::clear_depth{1.0f,0}, rhi::dont_care{}};
        cmd->begin_render_pass(pass, fb);

        // Bind common descriptors
        cmd->bind_descriptor_set(*common_layout, pbr_per_scene_set_index, *per_scene_set);
        cmd->bind_descriptor_set(*common_layout, pbr_per_view_set_index, *per_view_set);

        // Draw skybox
        cmd->bind_pipeline(*skybox_pipe_cubemap);
        auto skybox_set = desc_pool->alloc(*skybox_per_object_layout);
        skybox_set->write(0, standard.get_cubemap_sampler(), *env.environment_cubemap);
        cmd->bind_descriptor_set(*skybox_pipe_layout, pbr_per_object_set_index, *skybox_set);
        cmd->bind_vertex_buffer(0, box_vertex_buffer);
        cmd->bind_index_buffer(box_index_buffer);
        cmd->draw_indexed(0, 36);

        // Draw basis
        cmd->bind_pipeline(*wire_pipe);
        auto basis_set = desc_pool->alloc(*per_object_layout);
        basis_set->write(0, uniform_buffer.write(float4x4{linalg::identity}));
        basis_set->write(1, *nearest, *checkerboard);
        cmd->bind_descriptor_set(*pipe_layout, pbr_per_object_set_index, *basis_set);
        cmd->bind_vertex_buffer(0, basis_vertex_buffer);
        cmd->draw(0, 6);

        // Draw the ground
        cmd->bind_pipeline(*solid_pipe);

        struct { float4x4 model_matrix; float roughness, metalness; } per_object;
        per_object.model_matrix = translation_matrix(cam.coords(coord_axis::down)*0.5f);
        per_object.roughness = 0.5f;
        per_object.metalness = 0.0f;
        auto ground_set = desc_pool->alloc(*per_object_layout);
        ground_set->write(0, uniform_buffer.write(per_object));
        ground_set->write(1, *nearest, *checkerboard);
        cmd->bind_descriptor_set(*pipe_layout, pbr_per_object_set_index, *ground_set);
        cmd->bind_vertex_buffer(0, ground_vertex_buffer);
        cmd->bind_index_buffer(ground_index_buffer);
        cmd->draw_indexed(0, 6);

        // Draw a bunch of spheres
        cmd->bind_vertex_buffer(0, sphere_vertex_buffer);
        cmd->bind_index_buffer(sphere_index_buffer);
        for(int i=0; i<6; ++i)
        {
            for(int j=0; j<6; ++j)
            {
                per_object.model_matrix = translation_matrix(cam.coords(coord_axis::right)*(i*2-5.f) + cam.coords(coord_axis::forward)*(j*2-5.f));
                per_object.roughness = (j+0.5f)/6;
                per_object.metalness = (i+0.5f)/6;
                auto sphere_set = desc_pool->alloc(*per_object_layout);
                sphere_set->write(0, uniform_buffer.write(per_object));
                sphere_set->write(1, *nearest, *checkerboard);
                cmd->bind_descriptor_set(*pipe_layout, pbr_per_object_set_index, *sphere_set);                      
                cmd->draw_indexed(0, 32*32*6);
            }
        }

        // Draw the UI
        const coord_system ui_coords {coord_axis::right, coord_axis::down, coord_axis::forward};
        auto ui_set = desc_pool->alloc(*per_object_layout);
        ui_set->write(0, uniform_buffer.write(make_transform_4x4(ui_coords, fb.get_ndc_coords())));
        ui_set->write(1, *nearest, *font_image);
        cmd->bind_pipeline(*ui_pipe);
        cmd->bind_descriptor_set(*pipe_layout, pbr_per_object_set_index, *ui_set);
        cmd->bind_vertex_buffer(0, font_buffer);
        cmd->draw(0, 6);

        cmd->end_render_pass();
        transient_resource_fence = dev->acquire_and_submit_and_present(*cmd, gwindow->get_rhi_window());
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
