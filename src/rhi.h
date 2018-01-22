#pragma once
#include "shader.h"
#include "geometry.h"
#include <functional>

struct GLFWwindow;

namespace rhi
{
    template<class T> class ptr
    {
        T * p {nullptr};
    public:
        ptr() = default;
        ptr(T * p) : p{p} { if(p) p->add_ref(); }
        ptr(const ptr & r) : ptr(r.p) {}
        ptr(ptr && r) noexcept : p{r.p} { r.p = nullptr; }
        ptr & operator = (const ptr & r) { return *this = ptr(r); }
        ptr & operator = (ptr && r) { std::swap(p, r.p); return *this; }
        ~ptr() { if(p) p->release(); }

        template<class U> ptr(const ptr<U> & r) : ptr(static_cast<U *>(r)) {}
        template<class U> ptr & operator = (const ptr<U> & r) { return *this = static_cast<U *>(r); }

        operator T * () const { return p; }
        T & operator * () const { return *p; }
        T * operator -> () const { return p; }
    };

    struct object
    {
        virtual void add_ref() = 0;
        virtual void release() = 0;
    };

    struct buffer : object { virtual char * get_mapped_memory() = 0; };
    struct sampler : object {};
    struct image : object {};
    struct framebuffer : object { virtual coord_system get_ndc_coords() const = 0; };
    struct window : object
    {
        virtual GLFWwindow * get_glfw_window() = 0;
        virtual framebuffer & get_swapchain_framebuffer() = 0;
    };

    struct descriptor_set_layout : object {};
    struct pipeline_layout : object {};
    struct shader : object {};
    struct pipeline : object {};

    struct buffer_range { buffer * buffer; size_t offset, size; };
    struct descriptor_set : object 
    {
        virtual void write(int binding, buffer_range range) = 0;
        virtual void write(int binding, sampler & sampler, image & image) = 0;    
    };
    struct descriptor_pool : object 
    {
        virtual void reset() = 0;
        virtual ptr<descriptor_set> alloc(descriptor_set_layout & layout) = 0;
    };

    // Render pass creation info
    enum class layout { attachment_optimal, shader_read_only_optimal, present_source };
    struct dont_care {};
    struct clear_color { float r,g,b,a; };
    struct clear_depth { float depth; uint8_t stencil; };
    struct load { layout initial_layout; };
    struct store { layout final_layout; };
    struct color_attachment_desc { std::variant<dont_care, clear_color, load> load_op; std::variant<dont_care, store> store_op; };
    struct depth_attachment_desc { std::variant<dont_care, clear_depth, load> load_op; std::variant<dont_care, store> store_op; };
    struct render_pass_desc 
    {
        std::vector<color_attachment_desc> color_attachments;
        std::optional<depth_attachment_desc> depth_attachment;
    };
    struct command_buffer : object
    {
        virtual void generate_mipmaps(image & image) = 0;
        virtual void begin_render_pass(const render_pass_desc & desc, framebuffer & framebuffer) = 0;
        virtual void bind_pipeline(pipeline & pipe) = 0;
        virtual void bind_descriptor_set(pipeline_layout & layout, int set_index, descriptor_set & set) = 0;
        virtual void bind_vertex_buffer(int index, buffer_range range) = 0;
        virtual void bind_index_buffer(buffer_range range) = 0;
        virtual void draw(int first_vertex, int vertex_count) = 0;
        virtual void draw_indexed(int first_index, int index_count) = 0;
        virtual void end_render_pass() = 0;
    };

    // Buffer creation info
    enum class buffer_usage { vertex, index, uniform, storage };
    struct buffer_desc { size_t size; buffer_usage usage; bool dynamic; };

    // Image creation info
    enum class image_format 
    {
        rgba_unorm8,
        rgba_srgb8,
        rgba_norm8,
        rgba_uint8,
        rgba_int8,
        rgba_unorm16,
        rgba_norm16, 
        rgba_uint16,
        rgba_int16,  
        rgba_float16,
        rgba_uint32,
        rgba_int32,  
        rgba_float32,
        rgb_uint32, 
        rgb_int32,
        rgb_float32,
        rg_unorm8,
        rg_norm8,
        rg_uint8,
        rg_int8,
        rg_unorm16,
        rg_norm16,
        rg_uint16,
        rg_int16,
        rg_float16,
        rg_uint32,
        rg_int32,
        rg_float32,
        r_unorm8,
        r_norm8,
        r_uint8,
        r_int8,
        r_unorm16,
        r_norm16,
        r_uint16,
        r_int16,
        r_float16,
        r_uint32,    
        r_int32,
        r_float32,
        depth_unorm16,
        depth_unorm24_stencil8,
        depth_float32,
        depth_float32_stencil8,
    };
    enum class image_shape { _1d, _2d, _3d, cube };
    enum image_flag
    {
        sampled_image_bit    = 1<<0, // Image can be bound to a sampler
        color_attachment_bit = 1<<1, // Image can be bound to a framebuffer as a color attachment
        depth_attachment_bit = 1<<2, // Image can be bound to a framebuffer as the depth/stencil attachment
    };
    using image_flags = int;
    struct image_desc
    {
        image_shape shape;
        int3 dimensions;
        int mip_levels;
        image_format format;
        image_flags flags;
        // Not yet supported: multisampling, arrays
    };

    // Sampler creation info
    enum class filter { nearest, linear };
    enum class address_mode { repeat, mirrored_repeat, clamp_to_edge, mirror_clamp_to_edge, clamp_to_border };
    struct sampler_desc
    {
        filter mag_filter, min_filter;
        std::optional<filter> mip_filter;
        address_mode wrap_s, wrap_t, wrap_r;
    };

    // Framebuffer creation info
    struct framebuffer_attachment_desc { ptr<image> image; int mip, layer; };
    struct framebuffer_desc
    {
        int2 dimensions;
        std::vector<framebuffer_attachment_desc> color_attachments;
        std::optional<framebuffer_attachment_desc> depth_attachment;
    };

    // Descriptor set layout creation info
    enum class descriptor_type { combined_image_sampler, uniform_buffer };
    struct descriptor_binding { int index; descriptor_type type; int count; };

    // Input layout creation info
    enum class attribute_format { float1, float2, float3, float4 };
    struct vertex_attribute_desc { int index; attribute_format type; int offset; };
    struct vertex_binding_desc { int index, stride; std::vector<vertex_attribute_desc> attributes; }; // TODO: per_vertex/per_instance

    // Pipeline creation info
    enum class primitive_topology { points, lines, triangles };
    enum class compare_op { never, less, equal, less_or_equal, greater, not_equal, greater_or_equal, always };
    enum class front_face { counter_clockwise, clockwise };
    enum class cull_mode { none, back, front };
    struct pipeline_desc
    {
        ptr<pipeline_layout> layout;            // descriptors
        std::vector<vertex_binding_desc> input; // input state
        std::vector<ptr<shader>> stages;        // programmable stages
        primitive_topology topology;            // rasterizer state
        front_face front_face;       
        cull_mode cull_mode;
        std::optional<compare_op> depth_test;   // depth state
        bool depth_write;
    };

    struct device_info { linalg::z_range z_range; bool inverted_framebuffers; };

    struct device : object
    {
        virtual auto get_info() const -> device_info = 0;

        virtual ptr<buffer> create_buffer(const buffer_desc & desc, const void * initial_data) = 0;
        virtual ptr<sampler> create_sampler(const sampler_desc & desc) = 0;
        virtual ptr<image> create_image(const image_desc & desc, std::vector<const void *> initial_data) = 0; // one ptr for non-cube, six ptrs in +x,-x,+y,-y,+z,-z order for cube
        virtual ptr<framebuffer> create_framebuffer(const framebuffer_desc & desc) = 0;
        virtual ptr<window> create_window(const int2 & dimensions, std::string_view title) = 0;

        virtual ptr<descriptor_set_layout> create_descriptor_set_layout(const std::vector<descriptor_binding> & bindings) = 0;
        virtual ptr<pipeline_layout> create_pipeline_layout(const std::vector<descriptor_set_layout *> & sets) = 0;
        virtual ptr<shader> create_shader(const shader_module & module) = 0;
        virtual ptr<pipeline> create_pipeline(const pipeline_desc & desc) = 0;

        virtual ptr<descriptor_pool> create_descriptor_pool() = 0;
        virtual ptr<command_buffer> create_command_buffer() = 0;

        virtual uint64_t submit(command_buffer & cmd) = 0;
        virtual uint64_t acquire_and_submit_and_present(command_buffer & cmd, window & window) = 0; // Submit commands to execute when the next frame is available, followed by a present
        virtual void wait_until_complete(uint64_t submit_id) = 0;
    };

    struct backend_info
    {
        std::string name;
        std::function<rhi::ptr<rhi::device>(std::function<void(const char *)> debug_callback)> create_device;
    };

    size_t get_pixel_size(image_format format);
}
