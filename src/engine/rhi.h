 #pragma once
#include "geometry.h" // For coord_system

struct GLFWwindow;

namespace rhi
{
    ///////////////////////////////////////
    // Forward declarations of API types //
    ///////////////////////////////////////

    struct object;
    struct device;
    struct buffer;
    struct sampler;
    struct image;
    struct framebuffer;
    struct window;
    struct descriptor_set_layout;
    struct pipeline_layout;
    struct shader;
    struct pipeline;
    struct descriptor_pool;
    struct descriptor_set;
    struct command_buffer;

    using buffer_flags = int;
    using image_flags = int;

    enum class client_api : int;
    enum class shader_stage : int;
    enum class layout : int;
    enum class image_format : int;
    enum class image_shape : int;
    enum class filter : int;
    enum class address_mode : int;
    enum class descriptor_type : int;
    enum class attribute_format : int;
    enum class primitive_topology : int;
    enum class front_face : int;
    enum class cull_mode : int;
    enum class compare_op : int;
    enum class stencil_op : int;
    enum class blend_op : int;
    enum class blend_factor : int;

    /////////////////////////////////////////////////////
    // Reference counted smart pointer for API objects //
    /////////////////////////////////////////////////////

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

    /////////////////////////
    // Description structs //
    /////////////////////////
    
    struct buffer_desc 
    { 
        size_t size; 
        buffer_flags flags;
    };

    struct image_desc
    {
        image_shape shape;
        int3 dimensions;
        int mip_levels;
        image_format format;
        image_flags flags;
        // Not yet supported: multisampling, arrays
    };

    struct sampler_desc
    {
        filter mag_filter, min_filter;
        std::optional<filter> mip_filter;
        address_mode wrap_s, wrap_t, wrap_r;
    };

    struct framebuffer_attachment_desc { ptr<image> image; int mip, layer; };
    struct framebuffer_desc
    {
        int2 dimensions;
        std::vector<framebuffer_attachment_desc> color_attachments;
        std::optional<framebuffer_attachment_desc> depth_attachment;
    };

    struct descriptor_binding { int index; descriptor_type type; int count; };

    struct shader_desc { shader_stage stage; std::vector<uint32_t> spirv; };

    struct vertex_attribute_desc { int index; attribute_format type; int offset; };
    struct vertex_binding_desc { int index, stride; std::vector<vertex_attribute_desc> attributes; }; // TODO: per_vertex/per_instance
    struct blend_equation { blend_factor source_factor; blend_op op; blend_factor dest_factor; };
    struct blend_state { bool write_mask; bool enable; blend_equation color, alpha; };
    struct depth_state
    {
        compare_op test;
        bool write_mask;
    };
    struct stencil_face
    {
        compare_op test {};
        stencil_op stencil_fail_op {};
        stencil_op stencil_pass_depth_fail_op {};
        stencil_op stencil_pass_depth_pass_op {};
    };
    struct stencil_state
    {
        stencil_face front, back;
        uint8_t read_mask = 0xFF;
        uint8_t write_mask = 0xFF;
    };
    struct pipeline_desc
    {
        ptr<const pipeline_layout> layout;      // descriptors
        std::vector<vertex_binding_desc> input; // input state
        std::vector<ptr<const shader>> stages;  // programmable stages
        primitive_topology topology;            // rasterizer state
        front_face front_face;       
        cull_mode cull_mode;
        std::optional<depth_state> depth;       // If non-null, parameters for depth test, if null, no depth test or writes are performed
        std::optional<stencil_state> stencil;   // If non-null, parameters for stencil test, if null, no stencil test or writes are performed
        std::vector<blend_state> blend;         // blending state
    };
   
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
    
    struct device_info { linalg::z_range z_range; bool inverted_framebuffers; };
    using debug_callback = std::function<void(const char *)>;

    struct client_info
    {
        std::string name;
        client_api api;
        std::function<rhi::ptr<rhi::device>(debug_callback debug_callback)> create_device;
    };

    ////////////////
    // Device API //
    ////////////////

    struct object
    {
        virtual void add_ref() const = 0;
        virtual void release() const = 0;
    };

    struct device : object
    {
        virtual auto get_info() const -> device_info = 0;

        virtual ptr<buffer> create_buffer(const buffer_desc & desc, const void * initial_data) = 0;
        virtual ptr<sampler> create_sampler(const sampler_desc & desc) = 0;
        virtual ptr<image> create_image(const image_desc & desc, std::vector<const void *> initial_data) = 0; // one ptr for non-cube, six ptrs in +x,-x,+y,-y,+z,-z order for cube
        virtual ptr<framebuffer> create_framebuffer(const framebuffer_desc & desc) = 0;
        virtual ptr<window> create_window(const int2 & dimensions, std::string_view title) = 0;

        virtual ptr<descriptor_set_layout> create_descriptor_set_layout(const std::vector<descriptor_binding> & bindings) = 0;
        virtual ptr<pipeline_layout> create_pipeline_layout(const std::vector<const descriptor_set_layout *> & sets) = 0;
        virtual ptr<shader> create_shader(const shader_desc & desc) = 0;
        virtual ptr<pipeline> create_pipeline(const pipeline_desc & desc) = 0;

        virtual ptr<descriptor_pool> create_descriptor_pool() = 0;
        virtual ptr<command_buffer> create_command_buffer() = 0;

        virtual uint64_t submit(command_buffer & cmd) = 0;
        virtual uint64_t acquire_and_submit_and_present(command_buffer & cmd, window & window) = 0; // Submit commands to execute when the next frame is available, followed by a present
        virtual uint64_t get_last_submission_id() = 0;
        virtual void wait_until_complete(uint64_t submission_id) = 0;
    };

    struct buffer : object 
    { 
        virtual size_t get_offset_alignment() = 0;
        virtual char * get_mapped_memory() = 0; 
    };
    struct sampler : object {};
    struct image : object {};
    struct framebuffer : object { virtual coord_system get_ndc_coords() const = 0; };
    struct window : object
    {
        virtual GLFWwindow * get_glfw_window() = 0;
        virtual framebuffer & get_swapchain_framebuffer() = 0;
    };

    struct descriptor_set_layout : object {};
    struct pipeline_layout : object 
    {
        virtual int get_descriptor_set_count() const = 0;
        virtual const descriptor_set_layout & get_descriptor_set_layout(int index) const = 0;
    };
    struct shader : object {};
    struct pipeline : object 
    {
        virtual const pipeline_layout & get_layout() const = 0;
    };

    struct buffer_range { buffer & buffer; size_t offset, size; };
    struct descriptor_set : object 
    {
        virtual void write(int binding, buffer_range range) = 0;
        virtual void write(int binding, sampler & sampler, image & image) = 0;    
    };
    struct descriptor_pool : object 
    {
        virtual void reset() = 0;
        virtual ptr<descriptor_set> alloc(const descriptor_set_layout & layout) = 0;
    };    

    struct command_buffer : object
    {
        virtual void generate_mipmaps(image & image) = 0;
        virtual void begin_render_pass(const render_pass_desc & desc, framebuffer & framebuffer) = 0;
        // TODO: virtual void clear_color(int index, clear_color color) = 0;
        virtual void clear_depth(float depth) = 0;
        virtual void clear_stencil(uint8_t stencil) = 0;
        virtual void set_viewport_rect(int x0, int y0, int x1, int y1) = 0;
        virtual void set_scissor_rect(int x0, int y0, int x1, int y1) = 0;
        virtual void set_stencil_ref(uint8_t ref) = 0;
        virtual void bind_pipeline(const pipeline & pipe) = 0;
        virtual void bind_descriptor_set(const pipeline_layout & layout, int set_index, const descriptor_set & set) = 0;
        virtual void bind_vertex_buffer(int index, buffer_range range) = 0;
        virtual void bind_index_buffer(buffer_range range) = 0;
        virtual void draw(int first_vertex, int vertex_count) = 0;
        virtual void draw_indexed(int first_index, int index_count) = 0;
        virtual void end_render_pass() = 0;
    };

    size_t get_pixel_size(image_format format);

    //////////////////////
    // Enumerated types //
    //////////////////////

    enum buffer_flag : buffer_flags
    {
        vertex_buffer_bit  = 1<<0, // Buffer can supply vertex attributes
        index_buffer_bit   = 1<<1, // Buffer can supply indices during indexed draw calls
        uniform_buffer_bit = 1<<2, // Buffer can supply the contents of uniform blocks
        storage_buffer_bit = 1<<3, // Buffer can supply the contents of buffer blocks
        mapped_memory_bit  = 1<<4, // Buffer is permanently mapped into the client's address space
    };

    enum image_flag : image_flags
    {
        sampled_image_bit    = 1<<0, // Image can be bound to a sampler
        color_attachment_bit = 1<<1, // Image can be bound to a framebuffer as a color attachment
        depth_attachment_bit = 1<<2, // Image can be bound to a framebuffer as the depth/stencil attachment
    };

    enum class client_api : int
    {
        vulkan, // Vulkan 1.0
        opengl, // OpenGL 4.5 Core
        d3d11   // Direct3D 11.1
    };

    enum class shader_stage : int
    {
        vertex,                
        tesselation_control,   
        tesselation_evaluation,
        geometry,              
        fragment,              
        compute,
    };

    enum class image_shape
    { 
        _1d,
        _2d,
        _3d,
        cube
    };
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
    enum class layout { attachment_optimal, shader_read_only_optimal, present_source };
    enum class filter { nearest, linear };
    enum class address_mode 
    { 
        repeat,
        mirrored_repeat,
        clamp_to_edge,
        mirror_clamp_to_edge,
        clamp_to_border,
    };
    enum class descriptor_type { combined_image_sampler, uniform_buffer };
    enum class attribute_format { float1, float2, float3, float4 };
    enum class primitive_topology 
    { 
        points, 
        lines, 
        triangles,
    };
    enum class front_face 
    { 
        counter_clockwise, 
        clockwise,
    };
    enum class cull_mode
    { 
        none,  // front face visible, back face visible
        back,  // front face visible, back face culled
        front, // front face culled, back face visible
    };
    enum class compare_op 
    { 
        never,            // false
        less,             // a < b
        equal,            // a == b
        less_or_equal,    // a <= b
        greater,          // a > b
        not_equal,        // a != b
        greater_or_equal, // a >= b
        always,           // true
    };
    enum class blend_op 
    { 
        add,              // src + dst
        subtract,         // src - dst
        reverse_subtract, // dst - src
        min,              // min(src, dst)
        max,              // max(src, dst)
    };
    enum class blend_factor
    {
        zero,                       // {   0,    0,    0,    0}
        one,                        // {   1,    1,    1,    1}
        constant_color,             // {  cr,   cg,   cb,   ca}
        one_minus_constant_color,   // {1-cr, 1-cg, 1-cb, 1-ca}
        source_color,               // {  sr,   sg,   sb,   sa}
        one_minus_source_color,     // {1-sr, 1-sg, 1-sb, 1-sa}
        dest_color,                 // {  dr,   dg,   db,   da}
        one_minus_dest_color,       // {1-dr, 1-dg, 1-db, 1-da}
        source_alpha,               // {  sa,   sa,   sa,   sa}
        one_minus_source_alpha,     // {1-sa, 1-sa, 1-sa, 1-sa}
        dest_alpha,                 // {  da,   da,   da,   da}
        one_minus_dest_alpha,       // {1-da, 1-da, 1-da, 1-da}
    };
    enum class stencil_op
    {
        keep,               
        zero,               
        replace,            
        invert,              
        increment_and_wrap, 
        increment_and_clamp,
        decrement_and_wrap, 
        decrement_and_clamp,
    };
}
