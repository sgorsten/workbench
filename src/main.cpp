#include "io.h"
#include <cstdlib>
#include <chrono>
#include <vector>

// A value type representing an abstract direction vector in 3D space, independent of any coordinate system
enum class coord_axis { forward, back, left, right, up, down };
constexpr float dot(coord_axis a, coord_axis b) { return a == b ? 1.0f : (static_cast<int>(a) ^ static_cast<int>(b)) == 1 ? -1.0f : 0.0f; }

// A concrete 3D coordinate system with defined x, y, and z axes
struct coord_system
{
    coord_axis x_axis, y_axis, z_axis;
    constexpr float3 operator ()(coord_axis axis) const { return {dot(x_axis, axis), dot(y_axis, axis), dot(z_axis, axis)}; }
    constexpr float3 cross(coord_axis a, coord_axis b) const { return linalg::cross((*this)(a), (*this)(b)); }
    constexpr bool is_orthogonal() const { return dot(x_axis, y_axis) == 0 && dot(y_axis, z_axis) == 0 && dot(z_axis, x_axis) == 0; }
    constexpr bool is_left_handed() const { return dot(cross(coord_axis::forward, coord_axis::up), (*this)(coord_axis::left)) == 1; }
    constexpr bool is_right_handed() const { return dot(cross(coord_axis::forward, coord_axis::up), (*this)(coord_axis::right)) == 1; }
};
constexpr float3x3 make_transform(const coord_system & from, const coord_system & to) { return {to(from.x_axis), to(from.y_axis), to(from.z_axis)}; }
constexpr float4x4 make_transform_4x4(const coord_system & from, const coord_system & to) { return {{to(from.x_axis),0}, {to(from.y_axis),0}, {to(from.z_axis),0}, {0,0,0,1}}; }

// A proper rigid transformation represented as a rotation followed by a translation
struct rigid_transform
{
    float4 rotation {0,0,0,1};  // The rotation component stored as a quaternion of approximately unit length
    float3 translation {0,0,0}; // The translation component stored as a vector

    rigid_transform inverse() const { return {qconj(rotation), qrot(qconj(rotation), -translation)}; }
    float4x4 matrix() const { return linalg::pose_matrix(rotation, translation); }
};
rigid_transform slerp(const rigid_transform & a, const rigid_transform & b, float t) { return {slerp(a.rotation, b.rotation, t), lerp(a.translation, b.translation, t)}; }
rigid_transform nlerp(const rigid_transform & a, const rigid_transform & b, float t) { return {nlerp(a.rotation, b.rotation, t), lerp(a.translation, b.translation, t)}; }

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

#include <iostream>

int main()
{
    int ortho=0, left=0, right=0;
    for(int i=0; i<6; ++i) for(int j=0; j<6; ++j) for(int k=0; k<6; ++k)
    {
        coord_system coords {static_cast<coord_axis>(i), static_cast<coord_axis>(j), static_cast<coord_axis>(k)};
        if(coords.is_orthogonal()) ++ortho;
        if(coords.is_left_handed()) ++left;
        if(coords.is_right_handed()) ++right;
    }
    std::cout << "There are " << ortho << " valid orthogonal bases, of which " << left << " are left-handed and " << right << " are right-handed" << std::endl;

    constexpr coord_system game_coords {coord_axis::right, coord_axis::forward, coord_axis::up};

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
        glClear(GL_COLOR_BUFFER_BIT);

        constexpr coord_system opengl_coords {coord_axis::right, coord_axis::up, coord_axis::back};
        gl::load_matrix(cam.get_view_proj_matrix((float)fb_size.x/fb_size.y, opengl_coords));

        // Render basis
        glBegin(GL_LINES);
        glColor3f(1,0,0); glVertex3f(0,0,0); glVertex3f(1,0,0);
        glColor3f(0,1,0); glVertex3f(0,0,0); glVertex3f(0,1,0);
        glColor3f(0,0,1); glVertex3f(0,0,0); glVertex3f(0,0,1);
        glEnd();

        window.swap_buffers();

        // Poll events
        context.poll_events();
    }
    return EXIT_SUCCESS;
}