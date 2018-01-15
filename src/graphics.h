#pragma once
#include "rhi.h"
#include <type_traits>

namespace gfx
{
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
        static_buffer(std::shared_ptr<rhi::device> dev, rhi::buffer_usage usage, binary_view contents) : dev{dev}, buffer{std::get<rhi::buffer>(dev->create_buffer({contents.size, usage, false}, contents.data))}, size(contents.size) {}
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
        dynamic_buffer(std::shared_ptr<rhi::device> dev, rhi::buffer_usage usage, size_t size) : dev{dev}, size{size} { std::tie(buffer, mapped) = dev->create_buffer({size, usage, true}, nullptr); }
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

        void begin_render_pass(rhi::render_pass pass, rhi::framebuffer framebuffer) { dev.begin_render_pass(cmd, pass, framebuffer); }
        void bind_pipeline(rhi::pipeline pipe) { dev.bind_pipeline(cmd, pipe); }
        void bind_descriptor_set(rhi::pipeline_layout layout, int set_index, rhi::descriptor_set set) { dev.bind_descriptor_set(cmd, layout, set_index, set); }
        void bind_descriptor_set(rhi::pipeline_layout layout, int set_index, descriptor_set set) { dev.bind_descriptor_set(cmd, layout, set_index, set.set); }
        void bind_vertex_buffer(int index, rhi::buffer_range range) { dev.bind_vertex_buffer(cmd, index, range); }
        void bind_index_buffer(rhi::buffer_range range) { dev.bind_index_buffer(cmd, range); }
        void draw(int first_vertex, int vertex_count) { dev.draw(cmd, first_vertex, vertex_count); }
        void draw_indexed(int first_index, int index_count)  { dev.draw_indexed(cmd, first_index, index_count); }
        void end_render_pass() { dev.end_render_pass(cmd); }
    };
}
