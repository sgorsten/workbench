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

size_t descriptor_emulator::get_flat_buffer_binding(pipeline_layout & layout, int set, int binding) const
{
    const auto & pipe_layout = static_cast<emulated_pipeline_layout &>(layout);
    const auto & set_layout = static_cast<emulated_descriptor_set_layout &>(*pipe_layout.sets[set].layout);
    auto it = set_layout.buffer_offsets.find(binding);
    if(it == set_layout.buffer_offsets.end()) throw std::logic_error("invalid binding");
    return pipe_layout.sets[set].buffer_offset + it->second;
}

size_t descriptor_emulator::get_flat_image_binding(pipeline_layout & layout, int set, int binding) const
{
    const auto & pipe_layout = static_cast<emulated_pipeline_layout &>(layout);
    const auto & set_layout = static_cast<emulated_descriptor_set_layout &>(*pipe_layout.sets[set].layout);
    auto it = set_layout.image_offsets.find(binding);
    if(it == set_layout.image_offsets.end()) throw std::logic_error("invalid binding");
    return pipe_layout.sets[set].image_offset + it->second;
}

ptr<descriptor_set_layout> descriptor_emulator::create_descriptor_set_layout(const std::vector<descriptor_binding> & bindings)
{
    ptr<emulated_descriptor_set_layout> layout {new delete_when_unreferenced<emulated_descriptor_set_layout>{}};
    layout->bindings = bindings;
    for(auto & b : bindings)
    {
        switch(b.type)
        {
        case rhi::descriptor_type::combined_image_sampler:
            layout->image_offsets[b.index] = layout->num_images;
            layout->num_images += b.count;
            break;
        case rhi::descriptor_type::uniform_buffer:
            layout->buffer_offsets[b.index] = layout->num_buffers;
            layout->num_buffers += b.count;
            break;
        }
    }
    return layout;
}

ptr<pipeline_layout> descriptor_emulator::create_pipeline_layout(const std::vector<descriptor_set_layout *> & sets)
{
    ptr<emulated_pipeline_layout> layout {new delete_when_unreferenced<emulated_pipeline_layout>{}};
    for(auto & s : sets)
    {
        layout->sets.push_back({s, layout->num_buffers, layout->num_images});
        layout->num_buffers += static_cast<emulated_descriptor_set_layout *>(s)->num_buffers;
        layout->num_images += static_cast<emulated_descriptor_set_layout *>(s)->num_images;
    }
    return layout;
}

ptr<descriptor_pool> descriptor_emulator::create_descriptor_pool()
{
    return new delete_when_unreferenced<emulated_descriptor_pool>{};
}

void descriptor_emulator::emulated_descriptor_pool::reset()
{
    buffer_bindings.clear();
    image_bindings.clear();
}

ptr<descriptor_set> descriptor_emulator::emulated_descriptor_pool::alloc(descriptor_set_layout & layout)
{
    ptr<emulated_descriptor_set> set {new delete_when_unreferenced<emulated_descriptor_set>{}};
    set->layout = static_cast<emulated_descriptor_set_layout *>(&layout);
    set->buffer_offset = buffer_bindings.size();
    set->image_offset = image_bindings.size();
    buffer_bindings.resize(set->buffer_offset + set->layout->num_buffers);
    image_bindings.resize(set->image_offset + set->layout->num_images);
    return set;
}

void descriptor_emulator::emulated_descriptor_set::write(int binding, buffer_range range)
{
    auto it = layout->buffer_offsets.find(binding);
    if(it == layout->buffer_offsets.end()) throw std::logic_error("invalid binding");
    pool->buffer_bindings[buffer_offset + it->second] = range;
}

void descriptor_emulator::emulated_descriptor_set::write(int binding, sampler & sampler, image & image)
{
    auto it = layout->image_offsets.find(binding);
    if(it == layout->image_offsets.end()) throw std::logic_error("invalid binding");
    pool->image_bindings[image_offset + it->second] = {&sampler, &image};
}
