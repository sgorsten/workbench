#include "graphics.h"
#include "io.h"
#include "pbr.h"
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
        return {index, sizeof(mesh_vertex), {
            {0, rhi::attribute_format::float3, offsetof(mesh_vertex, position)},
            {1, rhi::attribute_format::float3, offsetof(mesh_vertex, color)},
            {2, rhi::attribute_format::float3, offsetof(mesh_vertex, normal)},
            {3, rhi::attribute_format::float2, offsetof(mesh_vertex, texcoord)},
        }};
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

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

struct image { int2 dimensions; rhi::image_format format; void * pixels; };
image load_image(const std::string & filename)
{
    int width, height;
    auto pixels = stbi_load(filename.c_str(), &width, &height, nullptr, 4);
    if(!pixels) throw std::runtime_error("stbi_load(\""+filename+"\", ...) failed");
    return {{width,height}, rhi::image_format::rgba_unorm8, pixels};
}
image load_image_hdr(const std::string & filename)
{
    int width, height;
    auto pixels = stbi_loadf(filename.c_str(), &width, &height, nullptr, 4);
    if(!pixels) throw std::runtime_error("stbi_loadf(\""+filename+"\", ...) failed");
    return {{width,height}, rhi::image_format::rgba_float32, pixels};
}

struct common_assets
{
    coord_system game_coords;
    standard_shaders standard;
    mesh basis_mesh, ground_mesh, box_mesh;
    shader_module vs, fs, fs_unlit;
    shader_module skybox_vs, skybox_fs_cubemap;
    shader_module cube_copy_fs;
    image env_spheremap;

    common_assets() : game_coords {coord_axis::right, coord_axis::forward, coord_axis::up}
    {
        env_spheremap = load_image_hdr("../../assets/monument-valley.hdr");

        shader_compiler compiler;
        standard = standard_shaders::compile(compiler);

        vs = compiler.compile(shader_stage::vertex, R"(#version 450
            layout(set=0,binding=0) uniform PerScene { vec3 light_pos; } per_scene;
            layout(set=0,binding=1) uniform PerView { mat4 view_proj_matrix, skybox_view_proj_matrix; } per_view;
            layout(set=1,binding=0) uniform PerObject { mat4 model_matrix; } per_object;
            layout(location=0) in vec3 v_position;
            layout(location=1) in vec3 v_color;
            layout(location=2) in vec3 v_normal;
            layout(location=3) in vec2 v_texcoord;
            layout(location=0) out vec3 position;
            layout(location=1) out vec3 color;
            layout(location=2) out vec3 normal;
            layout(location=3) out vec2 texcoord;
            void main()
            {
                position = (per_object.model_matrix * vec4(v_position,1)).xyz;
                color = v_color;
                normal = (per_object.model_matrix * vec4(v_normal,0)).xyz;
                texcoord = v_texcoord;
                gl_Position = per_view.view_proj_matrix * vec4(position,1);
            }
        )");
        fs = compiler.compile(shader_stage::fragment, R"(#version 450
            layout(set=0,binding=0) uniform PerScene { vec3 light_pos; } per_scene;
            layout(set=1,binding=1) uniform sampler2D albedo_tex;
            layout(location=0) in vec3 position;
            layout(location=1) in vec3 color;
            layout(location=2) in vec3 normal;
            layout(location=3) in vec2 texcoord;
            layout(location=0) out vec4 f_color;
            void main() 
            { 
                vec3 light_vec = normalize(per_scene.light_pos - position);
                vec3 normal_vec = normalize(normal);
                f_color = vec4(color*texture(albedo_tex, texcoord).rgb*max(dot(light_vec, normal_vec),0),1); 
            }
        )");
        fs_unlit = compiler.compile(shader_stage::fragment, R"(#version 450
            layout(location=0) in vec3 position;
            layout(location=1) in vec3 color;
            layout(location=0) out vec4 f_color;
            void main() 
            {
                f_color = vec4(color,1);
            }
        )");

        skybox_vs = compiler.compile(shader_stage::vertex, R"(#version 450
            layout(set=0,binding=0) uniform PerScene { vec3 light_pos; } per_scene;
            layout(set=0,binding=1) uniform PerView { mat4 view_proj_matrix, skybox_view_proj_matrix; } per_view;
            layout(location=0) in vec3 v_position;
            layout(location=1) in vec3 v_color;
            layout(location=2) in vec3 v_normal;
            layout(location=3) in vec2 v_texcoord;
            layout(location=0) out vec3 direction;
            void main()
            {
                direction = v_position;
                gl_Position = per_view.skybox_view_proj_matrix * vec4(v_position,1);
            }
        )");
        skybox_fs_cubemap = compiler.compile(shader_stage::fragment, R"(#version 450
            layout(set=1,binding=0) uniform samplerCube u_texture;
            layout(location=0) in vec3 direction;
            layout(location=0) out vec4 f_color;
            void main()
            {
                f_color = texture(u_texture, normalize(direction));
            }
        )");

        cube_copy_fs = compiler.compile(shader_stage::fragment, R"(#version 450
            layout(set=0,binding=0) uniform samplerCube u_texture;
            layout(location=0) in vec3 direction;
            layout(location=0) out vec4 f_color;
            void main() { f_color = texture(u_texture, direction); }
        )");

        //for(auto & desc : vs.descriptors) { std::cout << "layout(set=" << desc.set << ", binding=" << desc.binding << ") uniform " << desc.name << " : " << desc.type << std::endl; }
        //for(auto & v : vs.inputs) { std::cout << "layout(location=" << v.location << ") in " << v.name << " : " << v.type << std::endl; }
        //for(auto & v : vs.outputs) { std::cout << "layout(location=" << v.location << ") out " << v.name << " : " << v.type << std::endl; }

        basis_mesh = make_basis_mesh();
        ground_mesh = make_quad_mesh({0.5f,0.5f,0.5f}, game_coords(coord_axis::right)*8.0f, game_coords(coord_axis::forward)*8.0f);
        box_mesh = make_box_mesh({1,0,0}, {-0.3f,-0.3f,-0.3f}, {0.3f,0.3f,0.3f});
    }
};

class device_session
{
    std::shared_ptr<rhi::device> dev;
    standard_device_objects standard;
    rhi::device_info info;
    rhi::command_pool cmd_pool;
    gfx::descriptor_pool desc_pool;
    gfx::dynamic_buffer uniform_buffer;
    rhi::fence transient_resource_fence;

    gfx::static_buffer basis_vertex_buffer, ground_vertex_buffer, ground_index_buffer, box_vertex_buffer, box_index_buffer;

    rhi::descriptor_set_layout per_scene_view_layout, per_object_layout, skybox_per_object_layout;
    rhi::pipeline_layout pipe_layout, skybox_pipe_layout;

    rhi::render_pass pass;
    rhi::shader vs, fs, fs_unlit, skybox_vs, skybox_fs_cubemap;
    rhi::pipeline wire_pipe, solid_pipe, skybox_pipe_cubemap;

    rhi::sampler nearest, spheremap_sampler;
    rhi::image checkerboard;
    rhi::image env_spheremap;
    rhi::image env_cubemap;
    rhi::image env_cubemap2;

    std::unique_ptr<gfx::window> gwindow;
    double2 last_cursor;
public:
    device_session(const common_assets & assets, const std::string & name, std::shared_ptr<rhi::device> dev, const int2 & window_pos) :
        dev{dev}, standard{dev, assets.standard}, info{dev->get_info()}, 
        cmd_pool{dev->create_command_pool()},
        desc_pool{dev}, uniform_buffer{dev, rhi::buffer_usage::uniform, 1024*1024},
        transient_resource_fence{dev->create_fence(true)},
        basis_vertex_buffer{dev, rhi::buffer_usage::vertex, assets.basis_mesh.vertices},
        ground_vertex_buffer{dev, rhi::buffer_usage::vertex, assets.ground_mesh.vertices},
        box_vertex_buffer{dev, rhi::buffer_usage::vertex, assets.box_mesh.vertices},
        ground_index_buffer{dev, rhi::buffer_usage::index, assets.ground_mesh.triangles},
        box_index_buffer{dev, rhi::buffer_usage::index, assets.box_mesh.triangles}
    {
        per_scene_view_layout = dev->create_descriptor_set_layout({
            {0, rhi::descriptor_type::uniform_buffer, 1},
            {1, rhi::descriptor_type::uniform_buffer, 1}
        });
        per_object_layout = dev->create_descriptor_set_layout({
            {0, rhi::descriptor_type::uniform_buffer, 1},
            {1, rhi::descriptor_type::combined_image_sampler, 1}
        });
        skybox_per_object_layout = dev->create_descriptor_set_layout({
            {0, rhi::descriptor_type::combined_image_sampler, 1}
        });
        pipe_layout = dev->create_pipeline_layout({per_scene_view_layout, per_object_layout});
        skybox_pipe_layout = dev->create_pipeline_layout({per_scene_view_layout, skybox_per_object_layout});

        rhi::render_pass_desc pass_desc;
        pass_desc.color_attachments.push_back({rhi::image_format::rgba_unorm8, rhi::clear{}, rhi::store{rhi::layout::present_src}});
        pass_desc.depth_attachment = {rhi::image_format::depth_float32, rhi::clear{}, rhi::dont_care{}};
        pass = dev->create_render_pass(pass_desc);

        vs = dev->create_shader(assets.vs);
        fs = dev->create_shader(assets.fs);
        fs_unlit = dev->create_shader(assets.fs_unlit);
        skybox_vs = dev->create_shader(assets.skybox_vs);
        skybox_fs_cubemap = dev->create_shader(assets.skybox_fs_cubemap);

        wire_pipe = dev->create_pipeline({pass, pipe_layout, {mesh_vertex::get_binding(0)}, {vs,fs_unlit}, rhi::primitive_topology::lines, rhi::front_face::counter_clockwise, rhi::cull_mode::none, rhi::compare_op::less, true});
        solid_pipe = dev->create_pipeline({pass, pipe_layout, {mesh_vertex::get_binding(0)}, {vs,fs}, rhi::primitive_topology::triangles, rhi::front_face::counter_clockwise, rhi::cull_mode::none, rhi::compare_op::less, true});
        skybox_pipe_cubemap = dev->create_pipeline({pass, skybox_pipe_layout, {mesh_vertex::get_binding(0)}, {skybox_vs,skybox_fs_cubemap}, rhi::primitive_topology::triangles, rhi::front_face::clockwise, rhi::cull_mode::none, rhi::compare_op::always, false});

        nearest = dev->create_sampler({rhi::filter::nearest, rhi::filter::nearest, std::nullopt, rhi::address_mode::clamp_to_edge, rhi::address_mode::repeat});
        spheremap_sampler = dev->create_sampler({rhi::filter::linear, rhi::filter::linear, std::nullopt, rhi::address_mode::repeat, rhi::address_mode::clamp_to_edge});

        const byte4 w{255,255,255,255}, g{128,128,128,255}, grid[]{w,g,w,g,g,w,g,w,w,g,w,g,g,w,g,w};
        checkerboard = dev->create_image({rhi::image_shape::_2d, {4,4,1}, 1, rhi::image_format::rgba_unorm8, rhi::sampled_image_bit}, {grid});
        env_spheremap = dev->create_image({rhi::image_shape::_2d, {assets.env_spheremap.dimensions,1}, 1, assets.env_spheremap.format, rhi::sampled_image_bit}, {assets.env_spheremap.pixels});
        env_cubemap = standard.create_cubemap_from_spheremap(512, cmd_pool, desc_pool, uniform_buffer, env_spheremap, assets.game_coords);
        env_cubemap2 = standard.create_irradiance_cubemap(32, cmd_pool, desc_pool, uniform_buffer, env_cubemap);
        dev->wait_idle();

        std::ostringstream ss; ss << "Workbench 2018 Render Test (" << name << ")";
        gwindow = std::make_unique<gfx::window>(dev, pass, int2{512,512}, ss.str());
        gwindow->set_pos(window_pos); 
    }

    ~device_session()
    {
        dev->wait_for_fence(transient_resource_fence);
        gwindow.reset();
        for(auto pipeline : {skybox_pipe_cubemap, solid_pipe, wire_pipe}) dev->destroy_pipeline(pipeline);
        for(auto shader : {skybox_vs, vs, skybox_fs_cubemap, fs, fs_unlit}) dev->destroy_shader(shader);
        for(auto layout : {pipe_layout, skybox_pipe_layout}) dev->destroy_pipeline_layout(layout);
        for(auto layout : {per_scene_view_layout, per_object_layout, skybox_per_object_layout}) dev->destroy_descriptor_set_layout(layout);
        for(auto image : {checkerboard, env_spheremap, env_cubemap, env_cubemap2}) dev->destroy_image(image);
        for(auto sampler : {nearest, spheremap_sampler}) dev->destroy_sampler(sampler);
        dev->destroy_render_pass(pass);
        dev->destroy_command_pool(cmd_pool);
        dev->destroy_fence(transient_resource_fence);
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
        dev->wait_for_fence(transient_resource_fence);
        dev->reset_command_pool(cmd_pool);
        desc_pool.reset();
        uniform_buffer.reset();

        // Set up per scene and per view uniforms
        auto fb = dev->get_swapchain_framebuffer(gwindow->get_rhi_window());
        auto ndc_coords = dev->get_ndc_coords(fb);
        const auto proj_matrix = linalg::perspective_matrix(1.0f, gwindow->get_aspect(), 0.1f, 100.0f, linalg::pos_z, info.z_range);
        struct { float4x4 view_proj_matrix, skybox_view_proj_matrix; } per_view_uniforms;
        per_view_uniforms.view_proj_matrix = mul(proj_matrix, make_transform_4x4(cam.coords, ndc_coords), cam.get_view_matrix());
        per_view_uniforms.skybox_view_proj_matrix = mul(proj_matrix, make_transform_4x4(cam.coords, ndc_coords), cam.get_skybox_view_matrix());
        auto per_scene_view_set = desc_pool.alloc(per_scene_view_layout)
            .write(0, uniform_buffer.write(cam.coords(coord_axis::up)*2.0f))
            .write(1, uniform_buffer.write(per_view_uniforms));

        gfx::command_buffer cmd {*dev, dev->start_command_buffer(cmd_pool)};

        // Draw objects to our primary framebuffer
        cmd.begin_render_pass(pass, dev->get_swapchain_framebuffer(gwindow->get_rhi_window()), {{0,0,0,1},1.0f,0});

        // Draw skybox
        cmd.bind_pipeline(skybox_pipe_cubemap);
        cmd.bind_descriptor_set(skybox_pipe_layout, 0, per_scene_view_set);
        cmd.bind_descriptor_set(skybox_pipe_layout, 1, desc_pool.alloc(skybox_per_object_layout).write(0, spheremap_sampler, env_cubemap2));
        cmd.bind_vertex_buffer(0, box_vertex_buffer);
        cmd.bind_index_buffer(box_index_buffer);
        cmd.draw_indexed(0, 36);

        // Draw basis
        cmd.bind_pipeline(wire_pipe);
        cmd.bind_descriptor_set(pipe_layout, 1, desc_pool.alloc(per_object_layout).write(0, uniform_buffer, float4x4{linalg::identity})
                                                                                  .write(1, nearest, checkerboard));
        cmd.bind_vertex_buffer(0, basis_vertex_buffer);
        cmd.draw(0, 6);

        // Draw the ground
        cmd.bind_pipeline(solid_pipe);
        cmd.bind_descriptor_set(pipe_layout, 1, desc_pool.alloc(per_object_layout).write(0, uniform_buffer, translation_matrix(cam.coords(coord_axis::down)*0.3f))
                                                                                  .write(1, nearest, checkerboard));
        cmd.bind_vertex_buffer(0, ground_vertex_buffer);
        cmd.bind_index_buffer(ground_index_buffer);
        cmd.draw_indexed(0, 6);

        // Draw a bunch of boxes
        cmd.bind_vertex_buffer(0, box_vertex_buffer);
        cmd.bind_index_buffer(box_index_buffer);
        for(int i=0; i<6; ++i)
        {
            for(int j=0; j<6; ++j)
            {
                const float3 position = cam.coords(coord_axis::right)*(i*2-5.f) + cam.coords(coord_axis::forward)*(j*2-5.f);
                cmd.bind_descriptor_set(pipe_layout, 1, desc_pool.alloc(per_object_layout).write(0, uniform_buffer, translation_matrix(position))
                                                                                          .write(1, nearest, checkerboard));
                cmd.draw_indexed(0, 36);
            }
        }
        cmd.end_render_pass();
        dev->acquire_and_submit_and_present(cmd.cmd, gwindow->get_rhi_window(), transient_resource_fence);
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

    // Launch the workbench
    common_assets assets;
    camera cam {assets.game_coords};
    cam.pitch += 0.8f;
    cam.move(coord_axis::back, 10.0f);
    
    // Create the devices
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
