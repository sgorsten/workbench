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

void draw_mesh(const mesh & m)
{
    for(GLuint i : {0,1}) glEnableVertexAttribArray(i);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(mesh_vertex), &m.vertices[0].position);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(mesh_vertex), &m.vertices[0].color);
    glDrawElements(GL_QUADS, m.quads.size()*4, GL_UNSIGNED_INT, m.quads.data());
    glDrawElements(GL_TRIANGLES, m.triangles.size()*3, GL_UNSIGNED_INT, m.triangles.data());
    glDrawElements(GL_LINES, m.lines.size()*2, GL_UNSIGNED_INT, m.lines.data());
    for(GLuint i : {0,1}) glDisableVertexAttribArray(i);
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

GLuint compile_shader(GLenum type, const char * source)
{
    auto shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint status, length;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if(status == GL_FALSE)
    {
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> buffer(length);
        glGetShaderInfoLog(shader, buffer.size(), &length, buffer.data());
        throw std::runtime_error(buffer.data());
    }
    return shader;
}

GLuint link_program(std::initializer_list<GLuint> shaders)
{
    auto program = glCreateProgram();
    for(auto shader : shaders) glAttachShader(program, shader);
    glLinkProgram(program);

    GLint status, length;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if(status == GL_FALSE)
    {
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> buffer(length);
        glGetProgramInfoLog(program, buffer.size(), &length, buffer.data());
        throw std::runtime_error(buffer.data());
    }
    return program;
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
    glfw::window window {context, {1280,720}, "workbench"};
    window.make_context_current();

    std::cout << "GL_VERSION = " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GL_SHADING_LANGUAGE_VERSION = " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
    std::cout << "GL_VENDOR = " << glGetString(GL_VENDOR) << std::endl;
    std::cout << "GL_RENDERER = " << glGetString(GL_RENDERER) << std::endl;

    auto vs = compile_shader(GL_VERTEX_SHADER, R"(#version 450
layout(location=0) uniform mat4 u_transform;
layout(location=0) in vec3 v_position;
layout(location=1) in vec3 v_color;
layout(location=0) out vec3 color;
void main()
{
    gl_Position = u_transform * vec4(v_position,1);
    color = v_color;
})");
    auto fs = compile_shader(GL_FRAGMENT_SHADER, R"(#version 450
layout(location=0) in vec3 color;
layout(location=0) out vec4 f_color;
void main() { f_color = vec4(color,1); }
)");
    auto prog = link_program({vs,fs});

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

        glUseProgram(prog);
        gl::uniform(0, view_proj_matrix);;
        draw_mesh(basis_mesh);

        const auto model_matrix = translation_matrix(game_coords(coord_axis::down)*0.1f);
        gl::uniform(0, mul(view_proj_matrix, model_matrix));
        draw_mesh(ground_mesh);

        window.swap_buffers();

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
