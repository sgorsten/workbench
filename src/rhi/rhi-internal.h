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

    std::vector<backend_info> & global_backend_list();
    template<class Device> void register_backend(const char * name) { global_backend_list().push_back({name, [](std::function<void(const char *)> debug_callback) { return ptr<device>(new delete_when_unreferenced<Device>{debug_callback}); }}); }
    template<class Device> struct autoregister_backend { autoregister_backend(const char * name) { register_backend<Device>(name); } };

    enum attachment_type { color, depth_stencil };
    attachment_type get_attachment_type(image_format format);

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
        struct sampled_image { ptr<sampler> sampler; ptr<image> image; };

        struct emulated_descriptor_pool
        {
            std::vector<buffer_range> buffer_bindings;
            std::vector<sampled_image> image_bindings;
            std::vector<descriptor_set> sets;
            size_t used_sets=0;
        };

        struct emulated_descriptor_set_layout : descriptor_set_layout
        {
            std::vector<descriptor_binding> bindings;
            std::unordered_map<int, size_t> buffer_offsets;
            std::unordered_map<int, size_t> image_offsets;
            size_t num_buffers=0, num_images=0;
        };

        struct emulated_descriptor_set
        {
            descriptor_pool pool;
            ptr<emulated_descriptor_set_layout> layout;
            size_t buffer_offset, image_offset;
        };

        struct emulated_pipeline_layout : pipeline_layout
        {
            struct set { ptr<descriptor_set_layout> layout; size_t buffer_offset, image_offset; };
            std::vector<set> sets;
            size_t num_buffers=0, num_images=0;
        };

        template<class T> struct traits;
        template<> struct traits<descriptor_pool> { using type = emulated_descriptor_pool; };
        template<> struct traits<descriptor_set> { using type = emulated_descriptor_set; };
        heterogeneous_object_set<traits, descriptor_pool, descriptor_set> objects;
    public:
        size_t get_flat_buffer_binding(pipeline_layout & layout, int set, int binding) const;
        size_t get_flat_image_binding(pipeline_layout & layout, int set, int binding) const;

        ptr<descriptor_set_layout> create_descriptor_set_layout(const std::vector<descriptor_binding> & bindings);
        ptr<pipeline_layout> create_pipeline_layout(const std::vector<descriptor_set_layout *> & sets);
        descriptor_pool create_descriptor_pool();

        void reset_descriptor_pool(descriptor_pool pool);
        descriptor_set alloc_descriptor_set(descriptor_pool pool, descriptor_set_layout & layout);
        void write_descriptor(descriptor_set set, int binding, buffer_range range);
        void write_descriptor(descriptor_set set, int binding, sampler & sampler, image & image);

        template<class BindBufferFunction, class BindImageFunction>
        void bind_descriptor_set(pipeline_layout & layout, int set_index, descriptor_set set, BindBufferFunction bind_buffer, BindImageFunction bind_image) const
        {
            const auto & pipeline_layout = objects[layout];
            const auto & descriptor_set = objects[set];
            if(descriptor_set.layout != pipeline_layout.sets[set_index].layout) throw std::logic_error("descriptor_set_layout mismatch");

            const auto & descriptor_pool = objects[descriptor_set.pool];
            const auto & descriptor_set_layout = objects[descriptor_set.layout];
            for(size_t i=0; i<descriptor_set_layout.num_buffers; ++i) bind_buffer(pipeline_layout.sets[set_index].buffer_offset + i, descriptor_pool.buffer_bindings[descriptor_set.buffer_offset + i]);
            for(size_t i=0; i<descriptor_set_layout.num_images; ++i) 
            {
                auto & binding = descriptor_pool.image_bindings[descriptor_set.image_offset + i];
                bind_image(pipeline_layout.sets[set_index].image_offset + i, binding.sampler, binding.image);
            }
        }

        // TODO: Check for dependencies before wiping out
        void destroy(descriptor_pool pool) { objects.destroy(pool); }
    };

    struct generate_mipmaps_command { ptr<image> im; };
    struct begin_render_pass_command { render_pass_desc pass; ptr<framebuffer> framebuffer; };
    struct bind_pipeline_command { ptr<pipeline> pipe; };
    struct bind_descriptor_set_command { ptr<pipeline_layout> layout; int set_index; descriptor_set set; };
    struct bind_vertex_buffer_command { int index; buffer_range range; };
    struct bind_index_buffer_command { buffer_range range; };
    struct draw_command { int first_vertex, vertex_count; };
    struct draw_indexed_command { int first_index, index_count; };
    struct end_render_pass_command {};

    class command_emulator
    {
        struct emulated_command_buffer : command_buffer
        { 
            using command = std::variant<generate_mipmaps_command, begin_render_pass_command, bind_pipeline_command, bind_descriptor_set_command, bind_vertex_buffer_command, bind_index_buffer_command, draw_command, draw_indexed_command, end_render_pass_command>;
            std::vector<command> commands; 
    
            void generate_mipmaps(image & image) { commands.push_back(generate_mipmaps_command{&image}); }
            void begin_render_pass(const render_pass_desc & pass, framebuffer & framebuffer) { commands.push_back(begin_render_pass_command{pass, &framebuffer}); }
            void bind_pipeline(pipeline & pipe) { commands.push_back(bind_pipeline_command{&pipe}); }
            void bind_descriptor_set(pipeline_layout & layout, int set_index, descriptor_set set) { commands.push_back(bind_descriptor_set_command{&layout, set_index, set}); }
            void bind_vertex_buffer(int index, buffer_range range) { commands.push_back(bind_vertex_buffer_command{index, range}); }
            void bind_index_buffer(buffer_range range) { commands.push_back(bind_index_buffer_command{range}); }
            void draw(int first_vertex, int vertex_count) { commands.push_back(draw_command{first_vertex, vertex_count}); }
            void draw_indexed(int first_index, int index_count) { commands.push_back(draw_indexed_command{first_index, index_count}); }
            void end_render_pass() { commands.push_back(end_render_pass_command{}); }
        };
    public:
        ptr<command_buffer> start_command_buffer() { return new delete_when_unreferenced<emulated_command_buffer>{}; }

        template<class ExecuteCommandFunction> void execute(command_buffer & cmd, ExecuteCommandFunction execute_command) 
        { 
            auto & buf = static_cast<emulated_command_buffer &>(cmd);
            for(auto & command : buf.commands) std::visit(execute_command, command);
            buffers.destroy(cmd);
        }
    };
}