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
        std::shared_ptr<rhi::device> dev;
        rhi::buffer buffer;
        size_t size;
    public:
        static_buffer(std::shared_ptr<rhi::device> dev, rhi::buffer_usage usage, binary_view contents) : dev{dev}, buffer{dev->create_buffer({contents.size, usage, false}, contents.data)}, size(contents.size) {}
        ~static_buffer() { dev->destroy_buffer(buffer); }

        operator rhi::buffer_range() const { return {buffer, 0, size}; }
    };

    class dynamic_buffer
    {
        std::shared_ptr<rhi::device> dev;
        rhi::buffer buffer;
        char * mapped;
        size_t size, used;
    public:
        dynamic_buffer(std::shared_ptr<rhi::device> dev, rhi::buffer_usage usage, size_t size) : dev{dev}, buffer{dev->create_buffer({size, usage, true}, nullptr)}, mapped{dev->get_mapped_memory(buffer)}, size{size}, used{0} {}
        ~dynamic_buffer() { dev->destroy_buffer(buffer); }

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

    struct descriptor_set
    {
        rhi::device & dev;
        rhi::descriptor_set set;

        descriptor_set write(int binding, rhi::buffer_range range) { dev.write_descriptor(set, binding, range); return *this; }
        descriptor_set write(int binding, dynamic_buffer & buffer, binary_view data) { return write(binding, buffer.write(data)); }
        descriptor_set write(int binding, rhi::sampler sampler, rhi::image image) { dev.write_descriptor(set, binding, sampler, image); return *this; }
    };

    struct descriptor_pool
    {
        std::shared_ptr<rhi::device> dev;
        rhi::descriptor_pool pool;
    public:
        descriptor_pool(std::shared_ptr<rhi::device> dev) : dev{dev}, pool{dev->create_descriptor_pool()} {}
        ~descriptor_pool() { dev->destroy_descriptor_pool(pool); }

        void reset() { dev->reset_descriptor_pool(pool); }
        descriptor_set alloc(rhi::descriptor_set_layout layout) { return{*dev, dev->alloc_descriptor_set(pool, layout)}; }
    };

    struct command_buffer
    {
        rhi::device & dev;
        rhi::command_buffer cmd;

        command_buffer(rhi::device & dev) : dev{dev}, cmd{dev.start_command_buffer()} {}
        void begin_render_pass(const rhi::render_pass_desc & desc, rhi::framebuffer framebuffer) { dev.begin_render_pass(cmd, desc, framebuffer); }
        void bind_pipeline(rhi::pipeline pipe) { dev.bind_pipeline(cmd, pipe); }
        void bind_descriptor_set(rhi::pipeline_layout layout, int set_index, rhi::descriptor_set set) { dev.bind_descriptor_set(cmd, layout, set_index, set); }
        void bind_descriptor_set(rhi::pipeline_layout layout, int set_index, descriptor_set set) { dev.bind_descriptor_set(cmd, layout, set_index, set.set); }
        void bind_vertex_buffer(int index, rhi::buffer_range range) { dev.bind_vertex_buffer(cmd, index, range); }
        void bind_index_buffer(rhi::buffer_range range) { dev.bind_index_buffer(cmd, range); }
        void draw(int first_vertex, int vertex_count) { dev.draw(cmd, first_vertex, vertex_count); }
        void draw_indexed(int first_index, int index_count)  { dev.draw_indexed(cmd, first_index, index_count); }
        void end_render_pass() { dev.end_render_pass(cmd); }
        void generate_mipmaps(rhi::image image) { dev.generate_mipmaps(cmd, image); }
        uint64_t submit() { return dev.submit(cmd); }
    };

    class window
    {
        struct ignore { template<class... T> operator std::function<void(T...)>() const { return [](T...) {}; } };
        std::shared_ptr<rhi::device> dev;
        rhi::window rhi_window;
        GLFWwindow * glfw_window;
    public:
        window(std::shared_ptr<rhi::device> dev, const int2 & dimensions, const std::string & title);
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
        rhi::window get_rhi_window() { return rhi_window; }

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
