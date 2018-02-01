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
    #define RHI_IMAGE_FORMAT(FORMAT, SIZE, TYPE, VK, DX, GLI, GLF, GLT) case FORMAT: return TYPE;
    #include "rhi-tables.inl"
    default: fail_fast();
    }
}

size_t rhi::get_pixel_size(image_format format)
{
    switch(format)
    {
    #define RHI_IMAGE_FORMAT(FORMAT, SIZE, TYPE, VK, DX, GLI, GLF, GLT) case FORMAT: return SIZE;
    #include "rhi-tables.inl"
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

emulated_pipeline_layout::emulated_pipeline_layout(const std::vector<const descriptor_set_layout *> & sets)
{
    for(auto & s : sets)
    {
        auto & layout = static_cast<const emulated_descriptor_set_layout &>(*s);
        this->sets.push_back({&layout, num_buffers, num_images});
        num_buffers += layout.num_buffers;
        num_images += layout.num_images;
    }
}

void emulated_descriptor_pool::reset()
{
    for(auto & set : sets) if(set.ref_count != 0) throw std::logic_error("rhi::descriptor_pool::reset called with descriptor sets outstanding");
    for(auto & binding : buffer_bindings) binding = {};
    for(auto & binding : image_bindings) binding = {};
    used_buffer_bindings = used_image_bindings = used_sets = 0;
}

ptr<descriptor_set> emulated_descriptor_pool::alloc(const descriptor_set_layout & layout)
{
    auto & set_layout = static_cast<const emulated_descriptor_set_layout &>(layout);
    const size_t needed_buffer_bindings = used_buffer_bindings + set_layout.num_buffers;
    const size_t needed_image_bindings = used_image_bindings + set_layout.num_images;
    if(needed_buffer_bindings > buffer_bindings.size()) throw std::logic_error("out of buffer bindings");
    if(needed_image_bindings > image_bindings.size()) throw std::logic_error("out of image bindings");
    if(used_sets == sets.size()) throw std::logic_error("out of descriptor sets");

    auto & set = sets[used_sets++];
    set.layout = &set_layout;
    set.buffer_bindings = buffer_bindings.data() + used_buffer_bindings;
    set.image_bindings = image_bindings.data() + used_image_bindings;
    used_buffer_bindings = needed_buffer_bindings;
    used_image_bindings = needed_image_bindings;
    return &set;
}

void emulated_descriptor_set::write(int binding, buffer_range range)
{
    auto it = layout->buffer_offsets.find(binding);
    if(it == layout->buffer_offsets.end()) throw std::logic_error("invalid binding");
    buffer_bindings[it->second] = {&range.buffer, range.offset, range.size};
}

void emulated_descriptor_set::write(int binding, sampler & sampler, image & image)
{
    auto it = layout->image_offsets.find(binding);
    if(it == layout->image_offsets.end()) throw std::logic_error("invalid binding");
    image_bindings[it->second] = {&sampler, &image};
}
