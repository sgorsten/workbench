#pragma once
#include "shader.h"
#include "io.h"
#include "geometry.h"

namespace rhi
{
    // Object types
    struct buffer {};
    struct vertex_format {};
    struct shader {};
    struct pipeline {};   

    enum class buffer_usage { vertex, index, uniform, storage };
    struct buffer_desc { size_t size; buffer_usage usage; bool dynamic; };

    enum class attribute_format { float1, float2, float3, float4 };
    struct vertex_attribute_desc { int index; attribute_format type; int offset; };
    struct vertex_binding_desc { int index, stride; std::vector<vertex_attribute_desc> attributes; }; // TODO: per_vertex/per_instance

    enum class primitive_topology { points, lines, triangles };
    enum class compare_op { never, less, equal, less_or_equal, greater, not_equal, greater_or_equal, always };
    struct pipeline_desc
    {
        const vertex_format * format;       // input state
        std::vector<shader *> stages;       // programmable stages
        primitive_topology topology;        // rasterizer state
        compare_op depth_test;
    };

    struct buffer_range { buffer * buffer; size_t offset, size; };
    struct mapped_buffer_range : buffer_range { char * memory; };

    struct device_info
    {
        std::string name;
        coord_system ndc_coords;
        linalg::z_range z_range;
    };

    struct device
    {
        virtual device_info         get_info() const = 0;

        virtual glfw::window *      create_window(const int2 & dimensions, std::string_view title) = 0;
        virtual buffer *            create_buffer(const buffer_desc & desc, const void * initial_data) = 0;
        virtual shader *            create_shader(const shader_module & module) = 0;
        virtual vertex_format *     create_vertex_format(const std::vector<vertex_binding_desc> & bindings) = 0;
        virtual pipeline *          create_pipeline(const pipeline_desc & desc) = 0;

        virtual char *              get_mapped_memory(buffer * buffer) = 0;

        virtual void                begin_render_pass(glfw::window & window) = 0;
        virtual void                bind_pipeline(pipeline & pipe) = 0;
        virtual void                bind_uniform_buffer(int index, buffer_range range) = 0;
        virtual void                bind_vertex_buffer(int index, buffer_range range) = 0;
        virtual void                bind_index_buffer(buffer_range range) = 0;
        virtual void                draw(int first_vertex, int vertex_count) = 0;
        virtual void                draw_indexed(int first_index, int index_count) = 0;
        virtual void                end_render_pass() = 0;

        virtual void                present(glfw::window & window) = 0;
    };
}

rhi::device * create_opengl_device(std::function<void(const char *)> debug_callback);
rhi::device * create_d3d11_device(std::function<void(const char *)> debug_callback);

#include <type_traits>
struct binary_view 
{ 
    size_t size; const void * data; 
    binary_view(size_t size, const void * data) : size{size}, data{data} {}
    template<class T> binary_view(const T & obj) : binary_view{sizeof(T), &obj} { static_assert(std::is_trivially_copyable_v<T>, "binary_view supports only trivially_copyable types"); }
    template<class T> binary_view(const std::vector<T> & vec) : binary_view{vec.size()*sizeof(T), vec.data()} { static_assert(std::is_trivially_copyable_v<T>, "binary_view supports only trivially_copyable types"); }
    // TODO: std::array, C array, etc.
};

inline rhi::buffer_range make_static_buffer(rhi::device & dev, rhi::buffer_usage usage, binary_view contents)
{
    return {dev.create_buffer({contents.size, usage, false}, contents.data), 0, contents.size};
}

class dynamic_buffer
{
    rhi::buffer * buffer;
    char * mapped;
    size_t used;
public:
    dynamic_buffer(rhi::device & dev, rhi::buffer_usage usage, size_t size)
    {
        buffer = dev.create_buffer({size, usage, true}, nullptr);
        mapped = dev.get_mapped_memory(buffer);
    }

    void reset()
    {
        used = 0;
    }

    rhi::buffer_range write(binary_view contents)
    {
        // TODO: Check for buffer exhaustion
        const rhi::buffer_range range {buffer, (used+255)/256*256, contents.size};
        memcpy(mapped + range.offset, contents.data, static_cast<size_t>(range.size));
        used = range.offset + range.size;
        return range;
    }
};