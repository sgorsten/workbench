#pragma once
#include "../rhi.h"

template<class HANDLE, class TYPE> class object_set
{
    std::unordered_map<int, TYPE> objects;
    int next_id = 1;
public:
    const TYPE & operator[] (HANDLE h) const
    { 
        const auto it = objects.find(h.id);
        if(it == objects.end()) throw std::logic_error("invalid handle");
        return it->second;
    }

    TYPE & operator[] (HANDLE h) 
    { 
        const auto it = objects.find(h.id);
        if(it == objects.end()) throw std::logic_error("invalid handle");
        return it->second;
    }

    std::tuple<HANDLE, TYPE &> create() 
    { 
        const int id = next_id++;
        return {HANDLE{id}, objects[id]};
    }
};

struct emulated_descriptor_set_layout
{
    std::vector<rhi::descriptor_binding> bindings;
    std::unordered_map<int, size_t> offsets;
    size_t num_samplers=0, num_buffers=0;
};

struct emulated_pipeline_layout
{
    std::vector<rhi::descriptor_set_layout> sets;
    std::vector<size_t> sampler_offsets;
    std::vector<size_t> buffer_offsets;
    size_t num_samplers=0, num_buffers=0;
};

struct emulated_descriptor_pool
{
    std::vector<rhi::buffer_range> buffer_bindings;
    std::vector<rhi::descriptor_set> sets;
    size_t used_sets=0;
};

struct emulated_descriptor_set
{
    rhi::descriptor_pool pool;
    rhi::descriptor_set_layout layout;
    size_t buffer_offset;
};

class descriptor_set_emulator
{
    object_set<rhi::descriptor_set_layout, emulated_descriptor_set_layout> descriptor_set_layouts;
    object_set<rhi::pipeline_layout, emulated_pipeline_layout> pipeline_layouts;
    object_set<rhi::descriptor_pool, emulated_descriptor_pool> descriptor_pools;
    object_set<rhi::descriptor_set, emulated_descriptor_set> descriptor_sets;
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
        const auto & pipeline_layout = pipeline_layouts[layout];
        const auto & descriptor_set = descriptor_sets[set];
        if(descriptor_set.layout != pipeline_layout.sets[set_index]) throw std::logic_error("descriptor_set_layout mismatch");

        const auto & descriptor_pool = descriptor_pools[descriptor_set.pool];
        const auto & descriptor_set_layout = descriptor_set_layouts[descriptor_set.layout];
        // TODO: bind samplers
        for(size_t i=0; i<descriptor_set_layout.num_buffers; ++i) bind_buffer(pipeline_layout.buffer_offsets[set_index] + i, descriptor_pool.buffer_bindings[descriptor_set.buffer_offset + i]);
    }
};