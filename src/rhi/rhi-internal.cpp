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

emulated_descriptor_set_layout::emulated_descriptor_set_layout(const std::vector<descriptor_binding> & bindings) : bindings{bindings}
{
    for(auto & b : bindings)
    {
        switch(b.type)
        {
        case rhi::descriptor_type::combined_image_sampler:
            image_offsets[b.index] = num_images;
            num_images += b.count;
            break;
        case rhi::descriptor_type::uniform_buffer:
            buffer_offsets[b.index] = num_buffers;
            num_buffers += b.count;
            break;
        }
    }
}

size_t emulated_pipeline_layout::get_flat_buffer_binding(int set, int binding) const
{
    auto it = sets[set].layout->buffer_offsets.find(binding);
    if(it == sets[set].layout->buffer_offsets.end()) throw std::logic_error("invalid binding");
    return sets[set].buffer_offset + it->second;
}

size_t emulated_pipeline_layout::get_flat_image_binding(int set, int binding) const
{
    auto it = sets[set].layout->image_offsets.find(binding);
    if(it == sets[set].layout->image_offsets.end()) throw std::logic_error("invalid binding");
    return sets[set].image_offset + it->second;
}

emulated_pipeline_layout::emulated_pipeline_layout(const std::vector<descriptor_set_layout *> & sets)
{
    for(auto & s : sets)
    {
        auto & layout = static_cast<emulated_descriptor_set_layout &>(*s);
        this->sets.push_back({&layout, num_buffers, num_images});
        num_buffers += layout.num_buffers;
        num_images += layout.num_images;
    }
}

void emulated_descriptor_pool::reset()
{
    buffer_bindings.clear();
    image_bindings.clear();
}

ptr<descriptor_set> emulated_descriptor_pool::alloc(descriptor_set_layout & layout)
{
    ptr<emulated_descriptor_set> set {new delete_when_unreferenced<emulated_descriptor_set>{}};
    set->pool = this;
    set->layout = static_cast<emulated_descriptor_set_layout *>(&layout);
    set->buffer_offset = buffer_bindings.size();
    set->image_offset = image_bindings.size();
    buffer_bindings.resize(set->buffer_offset + set->layout->num_buffers);
    image_bindings.resize(set->image_offset + set->layout->num_images);
    return set;
}

void emulated_descriptor_set::write(int binding, buffer_range range)
{
    auto it = layout->buffer_offsets.find(binding);
    if(it == layout->buffer_offsets.end()) throw std::logic_error("invalid binding");
    pool->buffer_bindings[buffer_offset + it->second] = range;
}

void emulated_descriptor_set::write(int binding, sampler & sampler, image & image)
{
    auto it = layout->image_offsets.find(binding);
    if(it == layout->image_offsets.end()) throw std::logic_error("invalid binding");
    pool->image_bindings[image_offset + it->second] = {&sampler, &image};
}
