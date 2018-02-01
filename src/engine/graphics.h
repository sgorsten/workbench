#pragma once
#include "rhi.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace gfx
{
    enum class cursor_type { arrow, hresize, vresize, ibeam };
    GLFWcursor * get_standard_cursor(cursor_type type);

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
        template<class T> binary_view(std::initializer_list<T> ilist) : binary_view{ilist.size()*sizeof(T), vec.data()} { static_assert(std::is_trivially_copyable_v<T>, "binary_view supports only trivially_copyable types"); }
        template<class T> binary_view(array_view<T> view) : binary_view{view.size()*sizeof(T), view.data()} { static_assert(std::is_trivially_copyable_v<T>, "binary_view supports only trivially_copyable types"); }
    };

    struct static_buffer
    {
        rhi::ptr<rhi::buffer> buffer;
        size_t size;

        static_buffer() : size{0} {}
        static_buffer(rhi::device & dev, rhi::buffer_flags flags, binary_view contents) : buffer{dev.create_buffer({contents.size, flags & ~rhi::mapped_memory_bit}, contents.data)}, size(contents.size) {}

        operator rhi::buffer_range() const { return {*buffer, 0, size}; }
    };

    struct simple_mesh
    {
        gfx::static_buffer vertex_buffer, index_buffer;

        simple_mesh() = default;
        simple_mesh(rhi::device & dev, binary_view vertices, binary_view indices) : vertex_buffer{dev, rhi::vertex_buffer_bit, vertices}, index_buffer{dev, rhi::index_buffer_bit, indices} {}

        void draw(rhi::command_buffer & cmd) const
        {
            cmd.bind_vertex_buffer(0, vertex_buffer);
            cmd.bind_index_buffer(index_buffer);
            cmd.draw_indexed(0, exactly(index_buffer.size/sizeof(int)));
        }
    };

    class dynamic_buffer
    {
        rhi::ptr<rhi::buffer> buffer;
        size_t size, alignment;
        char * mapped;
        
        size_t offset, used;
    public:
        dynamic_buffer(rhi::device & dev, rhi::buffer_flags flags, size_t size) : buffer{dev.create_buffer({size, flags | rhi::mapped_memory_bit}, nullptr)}, size{size}, alignment{buffer->get_offset_alignment()}, mapped{buffer->get_mapped_memory()}, offset{0}, used{0} {}

        void reset() { offset = used = 0; }

        void begin() { offset = used = round_up(used, alignment); }
        void write(binary_view contents)
        {
            if(used + contents.size > size) throw std::runtime_error("out of memory");
            memcpy(mapped + used, contents.data, contents.size);
            used += contents.size;
        }
        rhi::buffer_range end() { return {*buffer, offset, used-offset}; }

        rhi::buffer_range upload(binary_view contents) { begin(); write(contents); return end(); }
    };

    struct transient_resource_pool
    {
        rhi::ptr<rhi::descriptor_pool> descriptors;
        dynamic_buffer uniforms, vertices, indices;
        uint64_t last_submission_id=0;

        transient_resource_pool() = default;
        transient_resource_pool(rhi::device & dev) : 
            descriptors{dev.create_descriptor_pool()}, 
            uniforms{dev, rhi::uniform_buffer_bit, 1024*1024},
            vertices{dev, rhi::vertex_buffer_bit, 1024*1024},
            indices{dev, rhi::index_buffer_bit, 1024*1024} {}

        void begin_frame(rhi::device & dev)
        {
            dev.wait_until_complete(last_submission_id);
            descriptors->reset();
            uniforms.reset();
            vertices.reset();
            indices.reset();
        }

        void end_frame(rhi::device & dev) { last_submission_id = dev.get_last_submission_id(); }
    };

    // gfx::pipeline_layout wraps rhi::pipeline_layout, but remembers which set layouts were used to create it
    class pipeline_layout
    {
        std::vector<rhi::ptr<const rhi::descriptor_set_layout>> set_layouts;
        rhi::ptr<const rhi::pipeline_layout> pipe_layout;
    public:
        pipeline_layout(rhi::device & dev, const std::vector<const rhi::descriptor_set_layout *> & sets) : pipe_layout{dev.create_pipeline_layout(sets)}, set_layouts{sets.begin(), sets.end()} {}

        const rhi::pipeline_layout & get_rhi_pipeline_layout() const { return *pipe_layout; }
        const rhi::descriptor_set_layout & get_rhi_descriptor_set_layout(int set_index) const { return *set_layouts[set_index]; }
    };

    // gfx::pipeline wraps rhi::pipeline, but remembers which pipeline layout was used to create it
    class pipeline
    {
        const pipeline_layout & layout;
        rhi::ptr<const rhi::pipeline> pipe;        
    public:
        pipeline(rhi::device & dev, const pipeline_layout & layout, rhi::pipeline_desc desc) : layout{layout}
        {
            desc.layout = &layout.get_rhi_pipeline_layout();
            pipe = dev.create_pipeline(desc);
        }

        const rhi::pipeline & get_rhi_pipeline() const { return *pipe; }
        const rhi::pipeline_layout & get_rhi_pipeline_layout() const { return layout.get_rhi_pipeline_layout(); }
        const rhi::descriptor_set_layout & get_rhi_descriptor_set_layout(int set_index) const { return layout.get_rhi_descriptor_set_layout(set_index); }
    };

    // gfx::descriptor_set wraps rhi::descriptor_set, but remembers its pipeline and set index, and provides access to transient resources
    class descriptor_set
    {
        gfx::transient_resource_pool & pool;
        const gfx::pipeline & pipeline;
        int set_index;
        rhi::ptr<rhi::descriptor_set> set;
    public:
        descriptor_set(gfx::transient_resource_pool & pool, const gfx::pipeline & pipeline, int set_index) : 
            pool(pool), pipeline(pipeline), set_index(set_index), set{pool.descriptors->alloc(pipeline.get_rhi_descriptor_set_layout(set_index))} {}

        void write(int binding, rhi::buffer_range range) { set->write(binding, range); }
        void write(int binding, gfx::binary_view view) { set->write(binding, pool.uniforms.upload(view)); }
        void write(int binding, rhi::sampler & sampler, rhi::image & image) { set->write(binding, sampler, image); }

        void bind(rhi::command_buffer & cmd) const { cmd.bind_descriptor_set(pipeline.get_rhi_pipeline_layout(), set_index, *set); }
    };

    class window
    {
        struct ignore { template<class... T> operator std::function<void(T...)>() const { return [](T...) {}; } };
        rhi::ptr<rhi::window> rhi_window;
        GLFWwindow * glfw_window;
    public:
        window(rhi::device & dev, const int2 & dimensions, const std::string & title);
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
        void set_cursor(cursor_type type) { glfwSetCursor(glfw_window, get_standard_cursor(type)); }
        rhi::window & get_rhi_window() { return *rhi_window; }
        GLFWwindow * get_glfw_window() { return glfw_window; }

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

    template<class T> struct vertex_binder : rhi::vertex_binding_desc
    {
        vertex_binder(int binding_index) { index = binding_index; stride = sizeof(T); }
        vertex_binder attribute(int attribute_index, float2 T::*field) { attributes.push_back({attribute_index, rhi::attribute_format::float2, exactly(member_offset(field))}); return *this; }
        vertex_binder attribute(int attribute_index, float3 T::*field) { attributes.push_back({attribute_index, rhi::attribute_format::float3, exactly(member_offset(field))}); return *this; }
        vertex_binder attribute(int attribute_index, float4 T::*field) { attributes.push_back({attribute_index, rhi::attribute_format::float4, exactly(member_offset(field))}); return *this; }
    };
}