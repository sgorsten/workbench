#include "rhi.h"
#include "geometry.h"
#include "shader.h"

#define DOCTEST_CONFIG_NO_SHORT_MACRO_NAMES
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
    float3 position, color;
};

struct mesh
{
    std::vector<mesh_vertex> vertices;
    std::vector<int2> lines;
    std::vector<int3> triangles;
    std::vector<int4> quads;
};

mesh make_basis_mesh()
{
    mesh m;
    m.vertices = {{{0,0,0},{1,0,0}}, {{1,0,0},{1,0,0}}, {{0,0,0},{0,1,0}}, {{0,1,0},{0,1,0}}, {{0,0,0},{0,0,1}}, {{0,0,1},{0,0,1}}};
    m.lines = {{0,1},{2,3},{4,5}};
    return m;
}

mesh make_quad_mesh(const float3 & color, const float3 & tangent_s, const float3 & tangent_t)
{
    mesh m;
    m.vertices =
    {
        {-tangent_s-tangent_t, color},
        {+tangent_s-tangent_t, color},
        {+tangent_s+tangent_t, color},
        {-tangent_s+tangent_t, color}
    };
    m.quads = {{0,1,2,3}};
    return m;
}

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
    constexpr coord_system game_coords {coord_axis::right, coord_axis::forward, coord_axis::up};
    const auto basis_mesh = make_basis_mesh();
    const auto ground_mesh = make_quad_mesh({0.5f,0.5f,0.5f}, game_coords(coord_axis::right)*5.0f, game_coords(coord_axis::forward)*5.0f);

    camera cam {game_coords};
    cam.pitch += 0.8f;
    cam.move(coord_axis::back, 10.0f);
    
    glfw::context context;
    shader_compiler compiler;

    auto dev = create_opengl_device();
    auto window = dev->create_window({1280,720}, "workbench");

    auto mesh_vertex_format = dev->create_vertex_format({
        {0, sizeof(mesh_vertex), {
            {0, rhi::attribute_format::float3, offsetof(mesh_vertex, position)},
            {1, rhi::attribute_format::float3, offsetof(mesh_vertex, color)},
        }}
    });

    auto vs = dev->create_shader(compiler.compile(shader_stage::vertex, R"(#version 450
layout(binding=0) uniform PerObject { mat4 u_transform; };
layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_color;
layout(location=0) out vec3 color;
void main()
{
    gl_Position = u_transform * vec4(v_position,1);
    color = v_color;
})"));

    auto fs = dev->create_shader(compiler.compile(shader_stage::fragment, R"(#version 450
layout(location=0) in vec3 color;
layout(location=0) out vec4 f_color;
void main() { f_color = vec4(color,1); }
)"));

    auto wire_pipe = dev->create_pipeline({mesh_vertex_format, {vs,fs}, rhi::primitive_topology::lines, rhi::compare_op::less});
    auto solid_pipe = dev->create_pipeline({mesh_vertex_format, {vs,fs}, rhi::primitive_topology::triangles, rhi::compare_op::less});

    auto basis_vertex_buffer = dev->create_static_buffer(basis_mesh.vertices);
    auto ground_vertex_buffer = dev->create_static_buffer(ground_mesh.vertices);

    dynamic_buffer uniform_buffer {*dev, 1024};    

    double2 last_cursor;
    auto t0 = std::chrono::high_resolution_clock::now();
    while(!window->should_close())
    {
        // Compute timestep
        const auto t1 = std::chrono::high_resolution_clock::now();
        const auto timestep = std::chrono::duration<float>(t1-t0).count();
        t0 = t1;

        // Handle input
        const double2 cursor = window->get_cursor_pos();
        if(window->get_mouse_button(GLFW_MOUSE_BUTTON_LEFT))
        {
            cam.yaw += static_cast<float>(cursor.x - last_cursor.x) * 0.01f;
            cam.pitch = std::min(std::max(cam.pitch + static_cast<float>(cursor.y - last_cursor.y) * 0.01f, -1.5f), +1.5f);
        }
        last_cursor = cursor;

        const float cam_speed = timestep * 10;
        if(window->get_key(GLFW_KEY_W)) cam.move(coord_axis::forward, cam_speed);
        if(window->get_key(GLFW_KEY_A)) cam.move(coord_axis::left, cam_speed);
        if(window->get_key(GLFW_KEY_S)) cam.move(coord_axis::back, cam_speed);
        if(window->get_key(GLFW_KEY_D)) cam.move(coord_axis::right, cam_speed);

        // Render frame
        constexpr coord_system opengl_coords {coord_axis::right, coord_axis::up, coord_axis::back};
        const auto view_proj_matrix = cam.get_view_proj_matrix(window->get_aspect(), opengl_coords);
        uniform_buffer.reset();

        dev->begin_render_pass(*window);

        dev->bind_pipeline(*wire_pipe);
        dev->bind_uniform_buffer(0, uniform_buffer.write(view_proj_matrix));       
        dev->bind_vertex_buffer(0, basis_vertex_buffer);
        dev->draw(0, 6);
        
        dev->bind_pipeline(*solid_pipe);
        dev->bind_uniform_buffer(0, uniform_buffer.write(mul(view_proj_matrix, translation_matrix(game_coords(coord_axis::down)*0.1f))));
        dev->bind_vertex_buffer(0, ground_vertex_buffer);
        dev->draw(0, 3);

        dev->end_render_pass();

        dev->present(*window);

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
