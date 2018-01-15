#include "graphics.h"
#include "io.h"
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
    float4x4 get_proj_matrix(float aspect) const { return linalg::perspective_matrix(1.0f, aspect, 0.5f, 100.0f); }
    float4x4 get_view_proj_matrix(float aspect, const coord_system & ndc_coords) const { return mul(get_proj_matrix(aspect), make_transform_4x4(coords, ndc_coords), get_view_matrix()); }

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

struct common_assets
{
    coord_system game_coords;
    mesh basis_mesh, ground_mesh, box_mesh;
    shader_module vs, fs, fs_unlit;

    common_assets() : game_coords {coord_axis::right, coord_axis::forward, coord_axis::up}
    {
        shader_compiler compiler;
        vs = compiler.compile(shader_stage::vertex, R"(#version 450
            layout(set=0,binding=0) uniform PerScene { vec3 light_pos; } per_scene;
            layout(set=0,binding=1) uniform PerView { mat4 view_proj_matrix; } per_view;
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
                gl_Position = (per_view.view_proj_matrix * per_object.model_matrix) * vec4(v_position,1);
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

        for(auto & desc : vs.descriptors) { std::cout << "layout(set=" << desc.set << ", binding=" << desc.binding << ") uniform " << desc.name << " : " << desc.type << std::endl; }
        for(auto & v : vs.inputs) { std::cout << "layout(location=" << v.location << ") in " << v.name << " : " << v.type << std::endl; }
        for(auto & v : vs.outputs) { std::cout << "layout(location=" << v.location << ") out " << v.name << " : " << v.type << std::endl; }

        basis_mesh = make_basis_mesh();
        ground_mesh = make_quad_mesh({0.5f,0.5f,0.5f}, game_coords(coord_axis::right)*8.0f, game_coords(coord_axis::forward)*8.0f);
        box_mesh = make_box_mesh({1,0,0}, {-0.3f,-0.3f,-0.3f}, {0.3f,0.3f,0.3f});
    }
};

class device_session
{
    std::shared_ptr<rhi::device> dev;
    rhi::device_info info;
    gfx::descriptor_pool desc_pool;
    gfx::dynamic_buffer uniform_buffer;
    gfx::static_buffer basis_vertex_buffer, ground_vertex_buffer, ground_index_buffer, box_vertex_buffer, box_index_buffer;

    rhi::descriptor_set_layout per_scene_view_layout, per_object_layout;
    rhi::pipeline_layout pipe_layout;

    rhi::render_pass pass;
    rhi::shader vs, fs, fs_unlit;
    rhi::pipeline wire_pipe, solid_pipe;

    rhi::sampler nearest;
    rhi::image checkerboard;

    rhi::window rwindow;
    std::unique_ptr<glfw::window> gwindow;
public:
    device_session(const common_assets & assets, std::shared_ptr<rhi::device> dev, const int2 & window_pos) : 
        dev{dev}, info{dev->get_info()}, desc_pool{dev}, uniform_buffer{dev, rhi::buffer_usage::uniform, 16*1024},
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
        pipe_layout = dev->create_pipeline_layout({per_scene_view_layout, per_object_layout});

        pass = dev->create_render_pass({});
        vs = dev->create_shader(assets.vs);
        fs = dev->create_shader(assets.fs);
        fs_unlit = dev->create_shader(assets.fs_unlit);

        wire_pipe = dev->create_pipeline({pass, pipe_layout, {mesh_vertex::get_binding(0)}, {vs,fs_unlit}, rhi::primitive_topology::lines, rhi::compare_op::less});
        solid_pipe = dev->create_pipeline({pass, pipe_layout, {mesh_vertex::get_binding(0)}, {vs,fs}, rhi::primitive_topology::triangles, rhi::compare_op::less});

        nearest = dev->create_sampler({rhi::filter::nearest, rhi::filter::nearest, std::nullopt, rhi::address_mode::clamp_to_edge, rhi::address_mode::repeat});

        const byte4 w{255,255,255,255}, g{128,128,128,255}, grid[]{w,g,w,g,g,w,g,w,w,g,w,g,g,w,g,w};
        checkerboard = dev->create_image({rhi::image_shape::_2d, {4,4,1}, 1, rhi::image_format::r8g8b8a8_unorm, rhi::sampled_image_bit}, {grid});

        std::ostringstream ss; ss << "Workbench 2018 Render Test (" << info.name << ")";
        rwindow = dev->create_window(pass, {512,512}, ss.str());
        gwindow = std::make_unique<glfw::window>(dev->get_glfw_window(rwindow));
        gwindow->set_pos(window_pos);
    }

    ~device_session()
    {
        gwindow.reset();
        dev->destroy_pipeline(solid_pipe);
        dev->destroy_pipeline(wire_pipe);
        dev->destroy_window(rwindow);
        dev->destroy_render_pass(pass);
        dev->destroy_shader(fs_unlit);
        dev->destroy_shader(fs);
        dev->destroy_shader(vs);       
        dev->destroy_pipeline_layout(pipe_layout);
        dev->destroy_descriptor_set_layout(per_object_layout);
        dev->destroy_descriptor_set_layout(per_scene_view_layout);

        dev->destroy_image(checkerboard);
        dev->destroy_sampler(nearest);
    }

    glfw::window & get_window() { return *gwindow; }

    void render_frame(const camera & cam)
    {
        // Reset resources
        desc_pool.reset();
        uniform_buffer.reset();        

        // Set up per scene and per view uniforms
        const auto proj_matrix = linalg::perspective_matrix(1.0f, gwindow->get_aspect(), 0.5f, 100.0f, linalg::pos_z, info.z_range);
        const auto view_proj_matrix = mul(proj_matrix, make_transform_4x4(cam.coords, info.ndc_coords), cam.get_view_matrix());
        auto per_scene_view_set = desc_pool.alloc(per_scene_view_layout)
            .write(0, uniform_buffer.write(cam.coords(coord_axis::up)*2.0f))
            .write(1, uniform_buffer.write(view_proj_matrix));

        // Draw objects to our framebuffer
        gfx::command_buffer cmd {*dev, dev->start_command_buffer()};
        cmd.begin_render_pass(pass, dev->get_swapchain_framebuffer(rwindow));
        cmd.bind_pipeline(wire_pipe);
        cmd.bind_descriptor_set(pipe_layout, 0, per_scene_view_set);
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
        dev->present(cmd.cmd, rwindow);
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
    glfw::context context;
    auto debug = [](const char * message) { std::cerr << message << std::endl; };
    device_session session {assets, create_opengl_device(debug), {100,100}};
    device_session session2 {assets, create_d3d11_device(debug), {700,100}};
    device_session session3 {assets, create_vulkan_device(debug), {1300,100}};

    double2 last_cursor;
    auto t0 = std::chrono::high_resolution_clock::now();
    while(!session.get_window().should_close())
    {
        // Compute timestep
        const auto t1 = std::chrono::high_resolution_clock::now();
        const auto timestep = std::chrono::duration<float>(t1-t0).count();
        t0 = t1;

        // Handle input
        const double2 cursor = session.get_window().get_cursor_pos();
        if(session.get_window().get_mouse_button(GLFW_MOUSE_BUTTON_LEFT))
        {
            cam.yaw += static_cast<float>(cursor.x - last_cursor.x) * 0.01f;
            cam.pitch = std::min(std::max(cam.pitch + static_cast<float>(cursor.y - last_cursor.y) * 0.01f, -1.5f), +1.5f);
        }
        last_cursor = cursor;

        const float cam_speed = timestep * 10;
        if(session.get_window().get_key(GLFW_KEY_W)) cam.move(coord_axis::forward, cam_speed);
        if(session.get_window().get_key(GLFW_KEY_A)) cam.move(coord_axis::left, cam_speed);
        if(session.get_window().get_key(GLFW_KEY_S)) cam.move(coord_axis::back, cam_speed);
        if(session.get_window().get_key(GLFW_KEY_D)) cam.move(coord_axis::right, cam_speed);

        session.render_frame(cam);
        session2.render_frame(cam);
        session3.render_frame(cam);

        // Poll events
        context.poll_events();
    }
    return EXIT_SUCCESS;
}
catch(const std::exception & e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
