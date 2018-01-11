#pragma once
#include <functional>
#include <string_view>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "linalg.h"
using namespace linalg::aliases;

namespace glfw
{
    class window;

    class context
    {
        friend class window;
    public:
        // Noncopyable and immovable
        context();
        context(const context &) = delete;
        context(context &&) = delete;
        context & operator = (const context &) = delete;
        context & operator = (context &&) = delete;
        ~context();

        void poll_events() { glfwPollEvents(); }
    };

    class window
    {
        struct ignore { template<class... T> operator std::function<void(T...)>() const { return [](T...) {}; } };
        GLFWwindow * w;
    public:
        // Noncopyable and immovable
        window(GLFWwindow * window);
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
        float get_aspect() const { auto size = get_window_size(); return (float)size.x/size.y; }

        // Mutators
        void set_pos(const int2 & pos) { glfwSetWindowPos(w, pos.x, pos.y); }

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