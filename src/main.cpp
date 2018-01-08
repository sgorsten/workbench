#include "io.h"
#include "geometry.h"

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

void draw_vertex(const mesh_vertex & v) { gl::color(v.color); gl::vertex(v.position); }
void draw_mesh(const mesh & m)
{
    glBegin(GL_QUADS); for(auto & quad : m.quads) for(auto index : quad) draw_vertex(m.vertices[index]); glEnd();
    glBegin(GL_TRIANGLES); for(auto & triangle : m.triangles) for(auto index : triangle) draw_vertex(m.vertices[index]); glEnd();
    glBegin(GL_LINES); for(auto & line : m.lines) for(auto index : line) draw_vertex(m.vertices[index]); glEnd();
}

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

int main(int argc, const char * argv[])
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
    glfw::window window {context, {1280,720}, "workbench"};

    double2 last_cursor;
    auto t0 = std::chrono::high_resolution_clock::now();
    while(!window.should_close())
    {
        // Compute timestep
        const auto t1 = std::chrono::high_resolution_clock::now();
        const auto timestep = std::chrono::duration<float>(t1-t0).count();
        t0 = t1;

        // Handle input
        const double2 cursor = window.get_cursor_pos();
        if(window.get_mouse_button(GLFW_MOUSE_BUTTON_LEFT))
        {
            cam.yaw += static_cast<float>(cursor.x - last_cursor.x) * 0.01f;
            cam.pitch = std::min(std::max(cam.pitch + static_cast<float>(cursor.y - last_cursor.y) * 0.01f, -1.5f), +1.5f);
        }
        last_cursor = cursor;

        const float cam_speed = timestep * 10;
        if(window.get_key(GLFW_KEY_W)) cam.move(coord_axis::forward, cam_speed);
        if(window.get_key(GLFW_KEY_A)) cam.move(coord_axis::left, cam_speed);
        if(window.get_key(GLFW_KEY_S)) cam.move(coord_axis::back, cam_speed);
        if(window.get_key(GLFW_KEY_D)) cam.move(coord_axis::right, cam_speed);

        // Render frame
        const int2 fb_size = window.get_framebuffer_size();
        window.make_context_current();
        glViewport(0, 0, fb_size.x, fb_size.y);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);

        
        constexpr coord_system opengl_coords {coord_axis::right, coord_axis::up, coord_axis::back};
        const auto view_proj_matrix = cam.get_view_proj_matrix((float)fb_size.x/fb_size.y, opengl_coords);
        gl::load_matrix(view_proj_matrix);
        draw_mesh(basis_mesh);

        const auto model_matrix = translation_matrix(game_coords(coord_axis::down)*0.1f);
        gl::load_matrix(mul(view_proj_matrix, model_matrix));
        draw_mesh(ground_mesh);

        window.swap_buffers();

        // Poll events
        context.poll_events();
    }
    return EXIT_SUCCESS;
}