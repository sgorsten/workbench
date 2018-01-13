#include "rhi-internal.h"

int descriptor_set_emulator::get_flat_buffer_binding(rhi::pipeline_layout layout, int set, int binding)
{
    auto & pipe_layout = pipeline_layouts[layout];
    auto & set_layout = descriptor_set_layouts[pipe_layout.sets[set]];
    return pipe_layout.buffer_offsets[set] + set_layout.offsets[binding];
}

rhi::descriptor_set_layout descriptor_set_emulator::create_descriptor_set_layout(const std::vector<rhi::descriptor_binding> & bindings)
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

rhi::pipeline_layout descriptor_set_emulator::create_pipeline_layout(const std::vector<rhi::descriptor_set_layout> & sets)
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

rhi::descriptor_pool descriptor_set_emulator::create_descriptor_pool()
{
    auto [handle, pool] = descriptor_pools.create();
    return handle;
}

void descriptor_set_emulator::reset_descriptor_pool(rhi::descriptor_pool pool)
{
    descriptor_pools[pool].buffer_bindings.clear();
    descriptor_pools[pool].used_sets = 0;
}

rhi::descriptor_set descriptor_set_emulator::alloc_descriptor_set(rhi::descriptor_pool pool, rhi::descriptor_set_layout layout)
{
    auto & dpool = descriptor_pools[pool];
    if(dpool.used_sets == dpool.sets.size())
    {
        auto [handle, set] = descriptor_sets.create();
        set.pool = pool;
        dpool.sets.push_back(handle);
    }

    auto handle = dpool.sets[dpool.used_sets++];
    auto & dset = descriptor_sets[handle];
    auto & dlayout = descriptor_set_layouts[layout];
    dset.layout = layout;
    dset.buffer_offset = dpool.buffer_bindings.size();
    dpool.buffer_bindings.resize(dset.buffer_offset + dlayout.num_buffers);
    return handle;
}

void descriptor_set_emulator::write_descriptor(rhi::descriptor_set set, int binding, rhi::buffer_range range)
{
    auto & dset = descriptor_sets[set];
    auto & dpool = descriptor_pools[dset.pool];
    auto & dlayout = descriptor_set_layouts[dset.layout];
    // TODO: Check that this is actually a buffer binding
    dpool.buffer_bindings[dset.buffer_offset + dlayout.offsets[binding]] = range;
}