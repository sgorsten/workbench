#pragma once
#include <functional>
#include <string_view>
#include <GLFW/glfw3.h>
#include "linalg.h"
using namespace linalg::aliases;

namespace glfw
{
    class window;

    class context
    {
        friend class window;
        GLFWwindow * hidden_window;
    public:
        // Noncopyable and immovable
        context();
        context(const context &) = delete;
        context(context &&) = delete;
        context & operator = (const context &) = delete;
        context & operator = (context &&) = delete;
        ~context();

        void make_shared_context_current() { glfwMakeContextCurrent(hidden_window); }
        void poll_events() { glfwPollEvents(); }
    };

    class window
    {
        struct ignore { template<class... T> operator std::function<void(T...)>() const { return [](T...) {}; } };
        GLFWwindow * w;
    public:
        // Noncopyable and immovable
        window(context & ctx, int2 dimensions, std::string_view title);
        window(const window &) = delete;
        window(window &&) = delete;
        window & operator = (const window &) = delete;
        window & operator = (window &&) = delete;
        ~window();

        // Observers
        bool should_close() const { return !!glfwWindowShouldClose(w); }
        bool get_key(int key) const { return glfwGetKey(w, key) != GLFW_RELEASE; }
        bool get_mouse_button(int button) const { return glfwGetMouseButton(w, button) != GLFW_RELEASE; }
        double2 get_cursor_pos() const { double2 pos; glfwGetCursorPos(w, &pos.x, &pos.y); return pos; }
        int2 get_window_size() const { int2 size; glfwGetWindowSize(w, &size.x, &size.y); return size; }
        int2 get_framebuffer_size() const { int2 size; glfwGetFramebufferSize(w, &size.x, &size.y); return size; }

        // Mutators
        void make_context_current() { glfwMakeContextCurrent(w); }
        void swap_buffers() { glfwSwapBuffers(w); }

        // Event handling callbacks
        std::function<void(int2 pos)> on_window_pos = ignore{};
        std::function<void(int2 size)> on_window_size = ignore{};
        std::function<void()> on_window_close = ignore{};
        std::function<void()> on_window_refresh = ignore{};
        std::function<void(bool focused)> on_window_focus = ignore{};
        std::function<void(bool iconified)> on_window_iconify = ignore{};
        std::function<void(int2 size)> on_framebuffer_size = ignore{};
        std::function<void(int button, int action, int mods)> on_mouse_button = ignore{};
        std::function<void(double2 pos)> on_cursor_pos = ignore{};
        std::function<void(bool entered)> on_cursor_enter = ignore{};
        std::function<void(double2 offset)> on_scroll = ignore{};
        std::function<void(int key, int scancode, int action, int mods)> on_key = ignore{};
        std::function<void(unsigned int codepoint, int mods)> on_char = ignore{};
    };
}

namespace gl
{
    inline void vertex(const float2 & v) { glVertex2fv(&v[0]); } inline void vertex(const double2 & v) { glVertex2dv(&v[0]); } 
    inline void vertex(const float3 & v) { glVertex3fv(&v[0]); } inline void vertex(const double3 & v) { glVertex3dv(&v[0]); } 
    inline void vertex(const float4 & v) { glVertex4fv(&v[0]); } inline void vertex(const double4 & v) { glVertex4dv(&v[0]); } 
    inline void vertex(const int2 & v) { glVertex2iv(&v[0]); } inline void vertex(const short2 & v) { glVertex2sv(&v[0]); }
    inline void vertex(const int3 & v) { glVertex3iv(&v[0]); } inline void vertex(const short3 & v) { glVertex3sv(&v[0]); }
    inline void vertex(const int4 & v) { glVertex4iv(&v[0]); } inline void vertex(const short4 & v) { glVertex4sv(&v[0]); }    

    inline void color(const float3 & v) { glColor3fv(&v[0]); } inline void color(const double3 & v) { glColor3dv(&v[0]); } 
    inline void color(const float4 & v) { glColor4fv(&v[0]); } inline void color(const double4 & v) { glColor4dv(&v[0]); } 
    inline void color(const int3 & v) { glColor3iv(&v[0]); } inline void color(const short3 & v) { glColor3sv(&v[0]); }
    inline void color(const int4 & v) { glColor4iv(&v[0]); } inline void color(const short4 & v) { glColor4sv(&v[0]); }
    inline void color(const uint3 & v) { glColor3uiv(&v[0]); } inline void color(const ushort3 & v) { glColor3usv(&v[0]); }
    inline void color(const uint4 & v) { glColor4uiv(&v[0]); } inline void color(const ushort4 & v) { glColor4usv(&v[0]); }  
    inline void color(const byte3 & v) { glColor3ubv(&v[0]); } inline void color(const linalg::vec<signed char,3> & v) { glColor3bv(&v[0]); }
    inline void color(const byte4 & v) { glColor4ubv(&v[0]); } inline void color(const linalg::vec<signed char,4> & v) { glColor4bv(&v[0]); }

    inline void load_matrix(const float4x4 & m) { glLoadMatrixf(&m[0][0]); }
    inline void load_matrix(const double4x4 & m) { glLoadMatrixd(&m[0][0]); }
}