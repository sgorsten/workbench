#pragma once
#include "rhi.h"

template<class HANDLE, class TYPE> class object_set
{
    std::unordered_map<int, TYPE> objects;
    int next_id = 1;
public:
    std::tuple<HANDLE, TYPE &> create() 
    { 
        const int id = next_id++;
        return {HANDLE{id}, objects[id]};
    }

    TYPE & operator[] (HANDLE h) 
    { 
        const auto it = objects.find(h.id);
        if(it == objects.end()) throw std::logic_error("invalid handle");
        return it->second;
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
};

struct emulated_descriptor_set
{
    rhi::descriptor_pool pool;
    rhi::descriptor_set_layout layout;
    size_t buffer_offset;
};

struct descriptor_set_emulator
{
    object_set<rhi::descriptor_set_layout, emulated_descriptor_set_layout> descriptor_set_layouts;
    object_set<rhi::pipeline_layout, emulated_pipeline_layout> pipeline_layouts;
    object_set<rhi::descriptor_pool, emulated_descriptor_pool> descriptor_pools;
    object_set<rhi::descriptor_set, emulated_descriptor_set> descriptor_sets;

    rhi::descriptor_set_layout create_descriptor_set_layout(const std::vector<rhi::descriptor_binding> & bindings)
    {
        auto [handle, layout] = descriptor_set_layouts.create();
        layout.bindings = bindings;
        for(auto & b : bindings)
        {
            switch(b.type)
            {
            case rhi::descriptor_type::combined_image_sampler:
                layout.offsets[b.index] = layout.num_samplers;
                layout.num_samplers += b.count;
                break;
            case rhi::descriptor_type::uniform_buffer:
                layout.offsets[b.index] = layout.num_buffers;
                layout.num_buffers += b.count;
                break;
            }
        }
        return handle;
    }

    rhi::pipeline_layout create_pipeline_layout(const std::vector<rhi::descriptor_set_layout> & sets)
    {
        auto [handle, layout] = pipeline_layouts.create();
        layout.sets = sets;
        for(auto & s : sets)
        {
            layout.sampler_offsets.push_back(layout.num_samplers);
            layout.buffer_offsets.push_back(layout.num_buffers);
            layout.num_samplers += descriptor_set_layouts[s].num_samplers;
            layout.num_buffers += descriptor_set_layouts[s].num_buffers;
        }
        return handle;
    }

    rhi::descriptor_pool create_descriptor_pool()
    {
        auto [handle, pool] = descriptor_pools.create();
        return handle;
    }

    void reset_descriptor_pool(rhi::descriptor_pool pool)
    {
        descriptor_pools[pool].buffer_bindings.clear();
        // TODO: Wipe out allocated descriptor sets
    }

    rhi::descriptor_set alloc_descriptor_set(rhi::descriptor_pool pool, rhi::descriptor_set_layout layout)
    {
        auto & dpool = descriptor_pools[pool];
        auto & dlayout = descriptor_set_layouts[layout];
        auto [handle, set] = descriptor_sets.create();
        set.pool = pool;
        set.layout = layout;
        set.buffer_offset = dpool.buffer_bindings.size();
        dpool.buffer_bindings.resize(set.buffer_offset + dlayout.num_buffers);
        return handle;
    }

    void write_descriptor(rhi::descriptor_set set, int binding, rhi::buffer_range range)
    {
        auto & dset = descriptor_sets[set];
        auto & dpool = descriptor_pools[dset.pool];
        auto & dlayout = descriptor_set_layouts[dset.layout];
        // TODO: Check that this is actually a buffer binding
        dpool.buffer_bindings[dset.buffer_offset + dlayout.offsets[binding]] = range;
    }

    template<class BindBufferFunction>
    void bind_descriptor_set(rhi::pipeline_layout layout, int set_index, rhi::descriptor_set set, BindBufferFunction bind_buffer)
    {
        auto & pipeline_layout = pipeline_layouts[layout];
        auto & descriptor_set = descriptor_sets[set];
        if(descriptor_set.layout != pipeline_layout.sets[set_index]) throw std::logic_error("descriptor_set_layout mismatch");

        auto & descriptor_pool = descriptor_pools[descriptor_set.pool];
        auto & descriptor_set_layout = descriptor_set_layouts[descriptor_set.layout];
        // TODO: bind samplers
        for(size_t i=0; i<descriptor_set_layout.num_buffers; ++i) bind_buffer(pipeline_layout.buffer_offsets[set_index] + i, descriptor_pool.buffer_bindings[descriptor_set.buffer_offset + i]);
    }
};