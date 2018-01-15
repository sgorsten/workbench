#include "rhi-internal.h"
using namespace rhi;

std::vector<backend_info> & rhi::global_backend_list()
{
    static std::vector<backend_info> backends;
    return backends;
}

attachment_type rhi::get_attachment_type(image_format format)
{
    switch(format)
    {
    #define X(FORMAT, SIZE, TYPE, VK, DX, GLI, GLF, GLT) case FORMAT: return TYPE;
    #include "rhi-format.inl"
    #undef X
    default: fail_fast();
    }
}

size_t rhi::get_pixel_size(image_format format)
{
    switch(format)
    {
    #define X(FORMAT, SIZE, TYPE, VK, DX, GLI, GLF, GLT) case FORMAT: return SIZE;
    #include "rhi-format.inl"
    #undef X
    default: fail_fast();
    }
}

size_t descriptor_emulator::get_flat_buffer_binding(rhi::pipeline_layout layout, int set, int binding) const
{
    const auto & pipe_layout = objects[layout];
    const auto & set_layout = objects[pipe_layout.sets[set].layout];
    auto it = set_layout.buffer_offsets.find(binding);
    if(it == set_layout.buffer_offsets.end()) throw std::logic_error("invalid binding");
    return pipe_layout.sets[set].buffer_offset + it->second;
}

size_t descriptor_emulator::get_flat_image_binding(rhi::pipeline_layout layout, int set, int binding) const
{
    const auto & pipe_layout = objects[layout];
    const auto & set_layout = objects[pipe_layout.sets[set].layout];
    auto it = set_layout.image_offsets.find(binding);
    if(it == set_layout.image_offsets.end()) throw std::logic_error("invalid binding");
    return pipe_layout.sets[set].image_offset + it->second;
}


rhi::descriptor_set_layout descriptor_emulator::create_descriptor_set_layout(const std::vector<rhi::descriptor_binding> & bindings)
{
    auto [handle, layout] = objects.create<rhi::descriptor_set_layout>();
    layout.bindings = bindings;
    for(auto & b : bindings)
    {
        switch(b.type)
        {
        case rhi::descriptor_type::combined_image_sampler:
            layout.image_offsets[b.index] = layout.num_images;
            layout.num_images += b.count;
            break;
        case rhi::descriptor_type::uniform_buffer:
            layout.buffer_offsets[b.index] = layout.num_buffers;
            layout.num_buffers += b.count;
            break;
        }
    }
    return handle;
}

rhi::pipeline_layout descriptor_emulator::create_pipeline_layout(const std::vector<rhi::descriptor_set_layout> & sets)
{
    auto [handle, layout] = objects.create<rhi::pipeline_layout>();
    for(auto & s : sets)
    {
        layout.sets.push_back({s, layout.num_buffers, layout.num_images});
        layout.num_buffers += objects[s].num_buffers;
        layout.num_images += objects[s].num_images;
    }
    return handle;
}

rhi::descriptor_pool descriptor_emulator::create_descriptor_pool()
{
    return std::get<rhi::descriptor_pool>(objects.create<rhi::descriptor_pool>());
}

void descriptor_emulator::reset_descriptor_pool(rhi::descriptor_pool pool)
{
    objects[pool].buffer_bindings.clear();
    objects[pool].used_sets = 0;
}

rhi::descriptor_set descriptor_emulator::alloc_descriptor_set(rhi::descriptor_pool pool, rhi::descriptor_set_layout layout)
{
    auto & dpool = objects[pool];
    if(dpool.used_sets == dpool.sets.size())
    {
        auto [handle, set] = objects.create<rhi::descriptor_set>();
        set.pool = pool;
        dpool.sets.push_back(handle);
    }

    auto handle = dpool.sets[dpool.used_sets++];
    auto & dset = objects[handle];
    const auto & dlayout = objects[layout];
    dset.layout = layout;
    dset.buffer_offset = dpool.buffer_bindings.size();
    dset.image_offset = dpool.image_bindings.size();
    dpool.buffer_bindings.resize(dset.buffer_offset + dlayout.num_buffers);
    dpool.image_bindings.resize(dset.image_offset + dlayout.num_images);
    return handle;
}

void descriptor_emulator::write_descriptor(rhi::descriptor_set set, int binding, rhi::buffer_range range)
{
    auto & dset = objects[set];
    auto & dpool = objects[dset.pool];
    const auto & dlayout = objects[dset.layout];
    auto it = dlayout.buffer_offsets.find(binding);
    if(it == dlayout.buffer_offsets.end()) throw std::logic_error("invalid binding");
    dpool.buffer_bindings[dset.buffer_offset + it->second] = range;
}

void descriptor_emulator::write_descriptor(rhi::descriptor_set set, int binding, rhi::sampler sampler, rhi::image image)
{
    auto & dset = objects[set];
    auto & dpool = objects[dset.pool];
    const auto & dlayout = objects[dset.layout];
    auto it = dlayout.image_offsets.find(binding);
    if(it == dlayout.image_offsets.end()) throw std::logic_error("invalid binding");
    dpool.image_bindings[dset.image_offset + it->second] = {sampler, image};
}
