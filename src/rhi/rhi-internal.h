#pragma once
#include "../rhi.h"

template<class Handle, class Type> class object_set
{
    std::unordered_map<int, Type> objects;
    int next_id = 1;
public:
    const Type & operator[] (Handle h) const
    { 
        const auto it = objects.find(h.id);
        if(it == objects.end()) throw std::logic_error("invalid handle");
        return it->second;
    }

    Type & operator[] (Handle h) 
    { 
        const auto it = objects.find(h.id);
        if(it == objects.end()) throw std::logic_error("invalid handle");
        return it->second;
    }

    std::tuple<Handle, Type &> create() 
    { 
        const int id = next_id++;
        return {Handle{id}, objects[id]};
    }

    void destroy(Handle h)
    {
        auto it = objects.find(h.id);
        if(it == objects.end()) throw std::logic_error("invalid handle");
        objects.erase(it);
    }
};

template<template<class> class Traits, class... Classes> class heterogeneous_object_set
{
    template<class T> using set_t = object_set<T, typename Traits<T>::type>;
    std::tuple<set_t<Classes>...> objects;
public:
    template<class T> auto & operator[] (T handle) const { return std::get<set_t<T>>(objects)[handle]; }
    template<class T> auto & operator[] (T handle) { return std::get<set_t<T>>(objects)[handle]; }
    template<class T> auto create() { return std::get<set_t<T>>(objects).create(); }    
    template<class T> void destroy(T handle) { return std::get<set_t<T>>(objects).destroy(handle); }
};

class descriptor_emulator
{
    struct descriptor_pool
    {
        std::vector<rhi::buffer_range> buffer_bindings;
        std::vector<rhi::descriptor_set> sets;
        size_t used_sets=0;
    };

    struct descriptor_set_layout
    {
        std::vector<rhi::descriptor_binding> bindings;
        std::unordered_map<int, size_t> offsets;
        size_t num_samplers=0, num_buffers=0;
    };

    struct descriptor_set
    {
        rhi::descriptor_pool pool;
        rhi::descriptor_set_layout layout;
        size_t buffer_offset;
    };

    struct pipeline_layout
    {
        std::vector<rhi::descriptor_set_layout> sets;
        std::vector<size_t> sampler_offsets;
        std::vector<size_t> buffer_offsets;
        size_t num_samplers=0, num_buffers=0;
    };

    template<class T> struct traits;
    template<> struct traits<rhi::descriptor_pool> { using type = descriptor_pool; };
    template<> struct traits<rhi::descriptor_set_layout> { using type = descriptor_set_layout; };      
    template<> struct traits<rhi::descriptor_set> { using type = descriptor_set; };
    template<> struct traits<rhi::pipeline_layout> { using type = pipeline_layout; };
    heterogeneous_object_set<traits, rhi::descriptor_pool, rhi::descriptor_set_layout, rhi::descriptor_set, rhi::pipeline_layout> objects;
public:
    size_t get_flat_buffer_binding(rhi::pipeline_layout layout, int set, int binding) const;

    rhi::descriptor_set_layout create_descriptor_set_layout(const std::vector<rhi::descriptor_binding> & bindings);
    rhi::pipeline_layout create_pipeline_layout(const std::vector<rhi::descriptor_set_layout> & sets);
    rhi::descriptor_pool create_descriptor_pool();

    void reset_descriptor_pool(rhi::descriptor_pool pool);
    rhi::descriptor_set alloc_descriptor_set(rhi::descriptor_pool pool, rhi::descriptor_set_layout layout);
    void write_descriptor(rhi::descriptor_set set, int binding, rhi::buffer_range range);

    template<class BindBufferFunction>
    void bind_descriptor_set(rhi::pipeline_layout layout, int set_index, rhi::descriptor_set set, BindBufferFunction bind_buffer) const
    {
        const auto & pipeline_layout = objects[layout];
        const auto & descriptor_set = objects[set];
        if(descriptor_set.layout != pipeline_layout.sets[set_index]) throw std::logic_error("descriptor_set_layout mismatch");

        const auto & descriptor_pool = objects[descriptor_set.pool];
        const auto & descriptor_set_layout = objects[descriptor_set.layout];
        // TODO: bind samplers
        for(size_t i=0; i<descriptor_set_layout.num_buffers; ++i) bind_buffer(pipeline_layout.buffer_offsets[set_index] + i, descriptor_pool.buffer_bindings[descriptor_set.buffer_offset + i]);
    }

    // TODO: Check for dependencies before wiping out
    void destroy(rhi::descriptor_set_layout layout) { objects.destroy(layout); }
    void destroy(rhi::pipeline_layout layout) { objects.destroy(layout); }
    void destroy(rhi::descriptor_pool pool) { objects.destroy(pool); }
};

struct begin_render_pass_command { rhi::render_pass pass; rhi::framebuffer framebuffer; };
struct bind_pipeline_command { rhi::pipeline pipe; };
struct bind_descriptor_set_command { rhi::pipeline_layout layout; int set_index; rhi::descriptor_set set; };
struct bind_vertex_buffer_command { int index; rhi::buffer_range range; };
struct bind_index_buffer_command { rhi::buffer_range range; };
struct draw_command { int first_vertex, vertex_count; };
struct draw_indexed_command { int first_index, index_count; };
struct end_render_pass_command {};

class command_emulator
{
    using command = std::variant<begin_render_pass_command, bind_pipeline_command, bind_descriptor_set_command, bind_vertex_buffer_command, bind_index_buffer_command, draw_command, draw_indexed_command, end_render_pass_command>;
    struct command_buffer { std::vector<command> commands; };
    object_set<rhi::command_buffer, command_buffer> buffers;
public:
    rhi::command_buffer create_command_buffer() { return std::get<rhi::command_buffer>(buffers.create()); }
    void destroy_command_buffer(rhi::command_buffer cmd) { buffers.destroy(cmd); }

    void begin_render_pass(rhi::command_buffer cmd, rhi::render_pass pass, rhi::framebuffer framebuffer) { buffers[cmd].commands.push_back(begin_render_pass_command{pass, framebuffer}); }
    void bind_pipeline(rhi::command_buffer cmd, rhi::pipeline pipe) { buffers[cmd].commands.push_back(bind_pipeline_command{pipe}); }
    void bind_descriptor_set(rhi::command_buffer cmd, rhi::pipeline_layout layout, int set_index, rhi::descriptor_set set) { buffers[cmd].commands.push_back(bind_descriptor_set_command{layout, set_index, set}); }
    void bind_vertex_buffer(rhi::command_buffer cmd, int index, rhi::buffer_range range) { buffers[cmd].commands.push_back(bind_vertex_buffer_command{index, range}); }
    void bind_index_buffer(rhi::command_buffer cmd, rhi::buffer_range range) { buffers[cmd].commands.push_back(bind_index_buffer_command{range}); }
    void draw(rhi::command_buffer cmd, int first_vertex, int vertex_count) { buffers[cmd].commands.push_back(draw_command{first_vertex, vertex_count}); }
    void draw_indexed(rhi::command_buffer cmd, int first_index, int index_count) { buffers[cmd].commands.push_back(draw_indexed_command{first_index, index_count}); }
    void end_render_pass(rhi::command_buffer cmd) { buffers[cmd].commands.push_back(end_render_pass_command{}); }

    template<class ExecuteCommandFunction> void execute(rhi::command_buffer cmd, ExecuteCommandFunction execute_command) const { for(auto & command : buffers[cmd].commands) std::visit(execute_command, command); }
};