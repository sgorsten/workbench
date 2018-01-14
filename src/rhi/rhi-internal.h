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
        objects.erase(h.id);
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
    int get_flat_buffer_binding(rhi::pipeline_layout layout, int set, int binding) const;

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