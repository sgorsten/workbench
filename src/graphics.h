#pragma once
#include "rhi.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace gfx
{
    struct context
    {
        context();
        ~context();

        const std::vector<rhi::backend_info> & get_backends();
        void poll_events();
    };

    struct binary_view 
    { 
        size_t size; const void * data; 
        binary_view(size_t size, const void * data) : size{size}, data{data} {}
        template<class T> binary_view(const T & obj) : binary_view{sizeof(T), &obj} { static_assert(std::is_trivially_copyable_v<T>, "binary_view supports only trivially_copyable types"); }
        template<class T> binary_view(const std::vector<T> & vec) : binary_view{vec.size()*sizeof(T), vec.data()} { static_assert(std::is_trivially_copyable_v<T>, "binary_view supports only trivially_copyable types"); }
    };

    class static_buffer
    {
        rhi::ptr<rhi::buffer> buffer;
        size_t size;
    public:
        static_buffer(rhi::ptr<rhi::device> dev, rhi::buffer_usage usage, binary_view contents) : buffer{dev->create_buffer({contents.size, usage, false}, contents.data)}, size(contents.size) {}

        operator rhi::buffer_range() const { return {buffer, 0, size}; }
    };

    class dynamic_buffer
    {
        rhi::ptr<rhi::buffer> buffer;
        char * mapped;
        size_t size, used;
    public:
        dynamic_buffer(rhi::ptr<rhi::device> dev, rhi::buffer_usage usage, size_t size) : buffer{dev->create_buffer({size, usage, true}, nullptr)}, mapped{buffer->get_mapped_memory()}, size{size}, used{0} {}

        void reset() { used = 0; }
        rhi::buffer_range write(binary_view contents)
        {
            const rhi::buffer_range range {buffer, (used+255)/256*256, contents.size};
            used = range.offset + range.size;
            if(used > size) throw std::runtime_error("out of memory");
            memcpy(mapped + range.offset, contents.data, static_cast<size_t>(range.size));       
            return range;
        }
    };

    class window
    {
        struct ignore { template<class... T> operator std::function<void(T...)>() const { return [](T...) {}; } };
        rhi::ptr<rhi::device> dev;
        rhi::ptr<rhi::window> rhi_window;
        GLFWwindow * glfw_window;
    public:
        window(rhi::ptr<rhi::device> dev, const int2 & dimensions, const std::string & title);
        ~window();

        // Observers
        bool should_close() const { return !!glfwWindowShouldClose(glfw_window); }
        bool get_key(int key) const { return glfwGetKey(glfw_window, key) != GLFW_RELEASE; }
        bool get_mouse_button(int button) const { return glfwGetMouseButton(glfw_window, button) != GLFW_RELEASE; }
        double2 get_cursor_pos() const { double2 pos; glfwGetCursorPos(glfw_window, &pos.x, &pos.y); return pos; }
        int2 get_window_size() const { int2 size; glfwGetWindowSize(glfw_window, &size.x, &size.y); return size; }
        int2 get_framebuffer_size() const { int2 size; glfwGetFramebufferSize(glfw_window, &size.x, &size.y); return size; }
        float get_aspect() const { auto size = get_window_size(); return (float)size.x/size.y; }

        // Mutators
        void set_pos(const int2 & pos) { glfwSetWindowPos(glfw_window, pos.x, pos.y); }
        rhi::window & get_rhi_window() { return *rhi_window; }

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
