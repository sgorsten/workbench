#pragma once
#include "../rhi.h"
#include <atomic>

namespace rhi
{
    // Used for objects whose ownership is shared by their references
    template<class T> struct delete_when_unreferenced : T
    {
        std::atomic_uint32_t ref_count {0};
        void add_ref() override { ++ref_count; }
        void release() override { if(--ref_count == 0) delete this; }
        using T::T;
    };

    // Used for objects which are reference counted but whose lifetime is controlled by another object
    template<class T> struct counted : T
    {
        std::atomic_uint32_t ref_count {0};
        void add_ref() override { ++ref_count; }
        void release() override { --ref_count; }
        using T::T;
    };

    std::vector<backend_info> & global_backend_list();
    template<class Device> void register_backend(const char * name) { global_backend_list().push_back({name, [](std::function<void(const char *)> debug_callback) { return ptr<device>(new delete_when_unreferenced<Device>{debug_callback}); }}); }
    template<class Device> struct autoregister_backend { autoregister_backend(const char * name) { register_backend<Device>(name); } };

    enum attachment_type { color, depth_stencil };
    attachment_type get_attachment_type(image_format format);

    /////////////////////////////////////////////////////////////////////////////////////////
    // Emulation layer for backends which do not have a native concept of a descriptor set //
    /////////////////////////////////////////////////////////////////////////////////////////

    struct emulated_descriptor_set_layout : descriptor_set_layout
    {
        std::vector<descriptor_binding> bindings;
        std::unordered_map<int, size_t> buffer_offsets;
        std::unordered_map<int, size_t> image_offsets;
        size_t num_buffers=0, num_images=0;

        emulated_descriptor_set_layout(const std::vector<descriptor_binding> & bindings);
    };

    struct emulated_pipeline_layout : pipeline_layout
    {
        struct set { ptr<emulated_descriptor_set_layout> layout; size_t buffer_offset, image_offset; };
        std::vector<set> sets;
        size_t num_buffers=0, num_images=0;

        emulated_pipeline_layout(const std::vector<descriptor_set_layout *> & sets);
        size_t get_flat_buffer_binding(int set, int binding) const;
        size_t get_flat_image_binding(int set, int binding) const;        
    };

    struct buffer_binding { ptr<buffer> buffer; size_t offset, size; };
    struct image_binding { ptr<sampler> sampler; ptr<image> image; };
    struct emulated_descriptor_set : descriptor_set
    {
        ptr<emulated_descriptor_set_layout> layout;
        buffer_binding * buffer_bindings;
        image_binding * image_bindings;

        void write(int binding, buffer_range range) override;
        void write(int binding, sampler & sampler, image & image) override;
    };

    struct emulated_descriptor_pool : descriptor_pool
    {
        std::vector<buffer_binding> buffer_bindings;
        std::vector<image_binding> image_bindings;
        std::vector<counted<emulated_descriptor_set>> sets;
        size_t used_buffer_bindings;
        size_t used_image_bindings;
        size_t used_sets;

        emulated_descriptor_pool() : buffer_bindings{1024}, image_bindings{1024}, sets{1024}, used_buffer_bindings{0}, used_image_bindings{0}, used_sets{0}
        {

        }

        void reset() override;
        ptr<descriptor_set> alloc(descriptor_set_layout & layout) override;
    };

    template<class BindBufferFunction, class BindImageFunction>
    void bind_descriptor_set(pipeline_layout & layout, int set_index, descriptor_set & set, BindBufferFunction bind_buffer, BindImageFunction bind_image)
    {
        const auto & p = static_cast<emulated_pipeline_layout &>(layout);
        const auto & s = static_cast<emulated_descriptor_set &>(set);
        if(s.layout != p.sets[set_index].layout) throw std::logic_error("descriptor_set_layout mismatch");
        for(size_t i=0; i<s.layout->num_buffers; ++i)
        {
            if(!s.buffer_bindings[i].buffer) continue;
            bind_buffer(p.sets[set_index].buffer_offset + i, *s.buffer_bindings[i].buffer, s.buffer_bindings[i].offset, s.buffer_bindings[i].size);
        }
        for(size_t i=0; i<s.layout->num_images; ++i)
        {
            if(!s.image_bindings[i].image) continue;
            bind_image(p.sets[set_index].image_offset + i, *s.image_bindings[i].sampler, *s.image_bindings[i].image);
        }
    }

    /////////////////////////////////////////////////////////////////////////////////////////
    // Emulation layer for backends which do not have a native concept of a command buffer //
    /////////////////////////////////////////////////////////////////////////////////////////

    struct generate_mipmaps_command { ptr<image> im; };
    struct begin_render_pass_command { render_pass_desc pass; ptr<framebuffer> framebuffer; };
    struct set_scissor_rect_command { int x0, y0, x1, y1; };
    struct bind_pipeline_command { ptr<pipeline> pipe; };
    struct bind_descriptor_set_command { ptr<pipeline_layout> layout; int set_index; ptr<descriptor_set> set; };
    struct bind_vertex_buffer_command { int index; buffer_range range; };
    struct bind_index_buffer_command { buffer_range range; };
    struct draw_command { int first_vertex, vertex_count; };
    struct draw_indexed_command { int first_index, index_count; };
    struct end_render_pass_command {};

    struct emulated_command_buffer : command_buffer
    { 
        using command = std::variant<generate_mipmaps_command, begin_render_pass_command, set_scissor_rect_command, bind_pipeline_command, bind_descriptor_set_command, bind_vertex_buffer_command, bind_index_buffer_command, draw_command, draw_indexed_command, end_render_pass_command>;
        std::vector<command> commands; 
    
        void generate_mipmaps(image & image) override { commands.push_back(generate_mipmaps_command{&image}); }
        void begin_render_pass(const render_pass_desc & pass, framebuffer & framebuffer) override { commands.push_back(begin_render_pass_command{pass, &framebuffer}); }
        void set_scissor_rect(int x0, int y0, int x1, int y1) override { commands.push_back(set_scissor_rect_command{x0, y0, x1, y1}); }
        void bind_pipeline(pipeline & pipe) override { commands.push_back(bind_pipeline_command{&pipe}); }
        void bind_descriptor_set(pipeline_layout & layout, int set_index, descriptor_set & set) override { commands.push_back(bind_descriptor_set_command{&layout, set_index, &set}); }
        void bind_vertex_buffer(int index, buffer_range range) override { commands.push_back(bind_vertex_buffer_command{index, range}); }
        void bind_index_buffer(buffer_range range) override { commands.push_back(bind_index_buffer_command{range}); }
        void draw(int first_vertex, int vertex_count) override { commands.push_back(draw_command{first_vertex, vertex_count}); }
        void draw_indexed(int first_index, int index_count) override { commands.push_back(draw_indexed_command{first_index, index_count}); }
        void end_render_pass() override { commands.push_back(end_render_pass_command{}); }

        template<class ExecuteCommandFunction> void execute(ExecuteCommandFunction execute_command) const { for(auto & command : commands) std::visit(execute_command, command); }
    };
}