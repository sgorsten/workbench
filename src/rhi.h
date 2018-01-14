#pragma once
#include "shader.h"
#include "geometry.h"
#include <functional>

struct GLFWwindow;

namespace rhi
{
    // Simple handle type used to refer to device objects
    template<class T> struct handle 
    { 
        int id;                                             // A handle is simply a strongly typed int
        handle() : id{0} {}                                 // Handles automatically default to zero, and are considered 'null'
        explicit handle(int id) : id{id} {}                 // Handles cannot be implicitly constructed from int
        explicit operator bool () const { return id != 0; } // A handle evaluates to true if it is not null
    };
    template<class T> bool operator == (const handle<T> & a, const handle<T> & b) { return a.id == b.id; } // Handles of the same type can be compared for equality
    template<class T> bool operator != (const handle<T> & a, const handle<T> & b) { return a.id != b.id; }

    // Object types
    using descriptor_set_layout = handle<struct descriptor_set_layout_tag>;
    using pipeline_layout = handle<struct pipeline_layout_tag>;
    using descriptor_pool = handle<struct descriptor_pool_tag>;
    using descriptor_set = handle<struct descriptor_set_tag>;
    using input_layout = handle<struct input_layout_tag>;
    using shader = handle<struct shader_tag>;
    using pipeline = handle<struct pipeline_tag>;
    using buffer = handle<struct buffer_tag>;
    using window = handle<struct window_tag>;

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
    struct pipeline_desc
    {
        pipeline_layout layout;             // descriptors
        input_layout input;                 // input state
        std::vector<shader> stages;         // programmable stages
        primitive_topology topology;        // rasterizer state
        compare_op depth_test;
    };

    // Buffer creation info
    enum class buffer_usage { vertex, index, uniform, storage };
    struct buffer_desc { size_t size; buffer_usage usage; bool dynamic; };

    struct device_info
    {
        std::string name;
        coord_system ndc_coords;
        linalg::z_range z_range;
    };

    struct buffer_range { buffer buffer; size_t offset, size; };

    struct device
    {
        //////////
        // info //
        //////////

        virtual auto get_info() const -> device_info = 0;

        /////////////
        // buffers //
        /////////////

        virtual auto create_buffer(const buffer_desc & desc, const void * initial_data) -> std::tuple<buffer, char *> = 0;
        virtual void destroy_buffer(buffer buffer) = 0;

        /////////////////
        // descriptors //
        /////////////////

        virtual auto create_descriptor_pool() -> descriptor_pool = 0;
        virtual void destroy_descriptor_pool(descriptor_pool pool) = 0;

        virtual auto create_descriptor_set_layout(const std::vector<descriptor_binding> & bindings) -> descriptor_set_layout = 0;
        virtual void destroy_descriptor_set_layout(descriptor_set_layout layout) = 0;

        virtual void reset_descriptor_pool(descriptor_pool pool) = 0;
        virtual auto alloc_descriptor_set(descriptor_pool pool, descriptor_set_layout layout) -> descriptor_set = 0;
        virtual void write_descriptor(descriptor_set set, int binding, buffer_range range) = 0;

        ///////////////
        // pipelines //
        ///////////////

        virtual auto create_pipeline_layout(const std::vector<descriptor_set_layout> & sets) -> pipeline_layout = 0;
        virtual void destroy_pipeline_layout(pipeline_layout layout) = 0;

        virtual auto create_input_layout(const std::vector<vertex_binding_desc> & bindings) -> input_layout = 0;
        virtual void destroy_input_layout(input_layout layout) = 0;

        virtual auto create_shader(const shader_module & module) -> shader = 0;
        virtual void destroy_shader(shader shader) = 0;

        virtual auto create_pipeline(const pipeline_desc & desc) -> pipeline = 0;
        virtual void destroy_pipeline(pipeline pipeline) = 0;

        /////////////
        // windows //
        /////////////

        virtual auto create_window(const int2 & dimensions, std::string_view title) -> std::tuple<window, GLFWwindow *> = 0;
        virtual void destroy_window(window window) = 0;

        ///////////////
        // rendering //
        ///////////////

        virtual void begin_render_pass(window window) = 0;
        virtual void bind_pipeline(pipeline pipe) = 0;
        virtual void bind_descriptor_set(pipeline_layout layout, int set_index, descriptor_set set) = 0;
        virtual void bind_vertex_buffer(int index, buffer_range range) = 0;
        virtual void bind_index_buffer(buffer_range range) = 0;
        virtual void draw(int first_vertex, int vertex_count) = 0;
        virtual void draw_indexed(int first_index, int index_count) = 0;
        virtual void end_render_pass() = 0;
        virtual void present(window window) = 0;
        virtual void wait_idle() = 0;
    };
}

std::shared_ptr<rhi::device> create_vulkan_device(std::function<void(const char *)> debug_callback);
std::shared_ptr<rhi::device> create_opengl_device(std::function<void(const char *)> debug_callback);
std::shared_ptr<rhi::device> create_d3d11_device(std::function<void(const char *)> debug_callback);
